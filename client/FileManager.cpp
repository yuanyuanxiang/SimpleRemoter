// FileManager.cpp: implementation of the CFileManager class.
//
//////////////////////////////////////////////////////////////////////

#include "FileManager.h"
#include <shellapi.h>
#include "ZstdArchive.h"
#include "file_upload.h"

typedef struct {
    DWORD	dwSizeHigh;
    DWORD	dwSizeLow;
} FILESIZE;

struct SearchParam {
    CFileManager *pThis;
    char szPath[MAX_PATH];
    char szName[MAX_PATH];
};

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CFileManager::CFileManager(CClientSocket *pClient, int h, void* user):CManager(pClient)
{
    m_nTransferMode = TRANSFER_MODE_NORMAL;
    m_bSearching = false;
    m_hSearchThread = NULL;
    // 发送驱动器列表, 开始进行文件管理，建立新线程
    SendDriveList();
}

CFileManager::~CFileManager()
{
    m_bSearching = false;
    if (m_hSearchThread) {
        WaitForSingleObject(m_hSearchThread, 5000);
        SAFE_CLOSE_HANDLE(m_hSearchThread);
    }
    m_UploadList.clear();
}


// 展开环境变量 (如 %USERPROFILE%, %DESKTOP% 等)
static std::string ExpandPath(const char* path)
{
    char szExpanded[MAX_PATH];
    DWORD len = ExpandEnvironmentStringsA(path, szExpanded, MAX_PATH);
    if (len > 0 && len < MAX_PATH) {
        return szExpanded;
    }
    return path;  // 展开失败则返回原路径
}

std::string GetExtractDir(const std::string& archivePath)
{
    if (archivePath.size() >= 5) {
        std::string ext = archivePath.substr(archivePath.size() - 5);
        for (char& c : ext) c = tolower(c);
        if (ext == ".zsta") {
            return archivePath.substr(0, archivePath.size() - 5);
        }
    }
    return archivePath + "_extract";
}

std::string GetDirectory(const std::string& filePath)
{
    size_t pos = filePath.find_last_of("/\\");
    if (pos != std::string::npos) {
        return filePath.substr(0, pos);
    }
    return ".";
}

VOID CFileManager::OnReceive(PBYTE lpBuffer, ULONG nSize)
{
    switch (lpBuffer[0]) {
    case CMD_COMPRESS_FILES: {
        std::vector<std::string> paths = ParseMultiStringPath((char*)lpBuffer + 1, nSize - 1);
        // 展开所有路径中的环境变量
        for (size_t i = 0; i < paths.size(); i++) {
            paths[i] = ExpandPath(paths[i].c_str());
        }
        zsta::Error err = zsta::CZstdArchive::Compress(std::vector<std::string>(paths.begin() + 1, paths.end()), paths.at(0));
        if (err != zsta::Error::Success) {
            Mprintf("压缩失败: %s\n", zsta::CZstdArchive::GetErrorString(err));
        } else {
            std::string dir = GetDirectory(paths.at(0));
            SendFilesList((char*)dir.c_str());
        }
        break;
    }
    case CMD_UNCOMPRESS_FILES: {
        std::string dir;
        std::vector<std::string> paths = ParseMultiStringPath((char*)lpBuffer + 1, nSize - 1);
        for (size_t i = 0; i < paths.size(); i++) {
            std::string path = ExpandPath(paths[i].c_str());
            std::string destDir = GetExtractDir(path);
            zsta::Error err = zsta::CZstdArchive::Extract(path, destDir);
            if (err != zsta::Error::Success) {
                Mprintf("解压失败: %s\n", zsta::CZstdArchive::GetErrorString(err));
            } else {
                dir = GetDirectory(path);
            }
        }
        if (!dir.empty()) {
            SendFilesList((char*)dir.c_str());
        }
        break;
    }
    case COMMAND_LIST_FILES:// 获取文件列表
        SendFilesList((char *)lpBuffer + 1);
        break;
    case COMMAND_DELETE_FILE: {// 删除文件
        std::string path = ExpandPath((char *)lpBuffer + 1);
        DeleteFile(path.c_str());
        SendToken(TOKEN_DELETE_FINISH);
        break;
    }
    case COMMAND_DELETE_DIRECTORY: {// 删除目录
        std::string path = ExpandPath((char *)lpBuffer + 1);
        DeleteDirectory(path.c_str());
        SendToken(TOKEN_DELETE_FINISH);
        break;
    }
    case COMMAND_DOWN_FILES: // 上传文件
        // 注意：不展开环境变量，保持原始路径发送给服务端
        // 这样服务端的 Replace(m_Remote_Path, localPath) 能正确匹配
        UploadToRemote(lpBuffer + 1);
        break;
    case COMMAND_CONTINUE: // 上传文件
        SendFileData(lpBuffer + 1);
        break;
    case COMMAND_CREATE_FOLDER: {
        std::string path = ExpandPath((char *)lpBuffer + 1);
        CreateFolder((LPBYTE)path.c_str());
        break;
    }
    case COMMAND_RENAME_FILE: {
        // 数据格式: [OldName\0][NewName\0]
        char *pOldName = (char *)lpBuffer + 1;
        char *pNewName = pOldName + strlen(pOldName) + 1;
        std::string oldPath = ExpandPath(pOldName);
        std::string newPath = ExpandPath(pNewName);
        // 构造展开后的数据
        char szBuf[MAX_PATH * 2 + 2];
        strcpy(szBuf, oldPath.c_str());
        strcpy(szBuf + oldPath.length() + 1, newPath.c_str());
        Rename((LPBYTE)szBuf);
        break;
    }
    case COMMAND_STOP:
        StopTransfer();
        break;
    case COMMAND_SET_TRANSFER_MODE:
        SetTransferMode(lpBuffer + 1);
        break;
    case COMMAND_FILE_SIZE: {
        // 数据格式: [8字节大小][文件名\0]
        // 需要展开文件名部分
        BYTE bNewBuffer[MAX_PATH + 16];
        memcpy(bNewBuffer, lpBuffer + 1, 8);  // 复制大小
        std::string path = ExpandPath((char *)lpBuffer + 9);
        strcpy((char *)bNewBuffer + 8, path.c_str());
        CreateLocalRecvFile(bNewBuffer);
        break;
    }
    case COMMAND_FILE_DATA:
        WriteLocalRecvFile(lpBuffer + 1, nSize -1);
        break;
    case COMMAND_SEARCH_FILE: {
        // 停止上一次搜索
        m_bSearching = false;
        if (m_hSearchThread) {
            WaitForSingleObject(m_hSearchThread, 5000);
            SAFE_CLOSE_HANDLE(m_hSearchThread);
            m_hSearchThread = NULL;
        }
        // 数据格式: [SearchPath\0][SearchFileName\0]
        char *pSearchPath = (char *)lpBuffer + 1;
        char *pSearchName = pSearchPath + strlen(pSearchPath) + 1;
        // 复制参数，在线程中使用
        SearchParam *pParam = new SearchParam;
        pParam->pThis = this;
        lstrcpynA(pParam->szPath, pSearchPath, MAX_PATH);
        lstrcpynA(pParam->szName, pSearchName, MAX_PATH);
        m_bSearching = true;
        m_hSearchThread = __CreateThread(NULL, 0, SearchThreadProc, (LPVOID)pParam, 0, NULL);
        break;
    }
    case COMMAND_FILES_SEARCH_STOP:
        m_bSearching = false;
        break;
    case COMMAND_OPEN_FILE_SHOW: {
        std::string path = ExpandPath((char *)lpBuffer + 1);
        OpenFile(path.c_str(), SW_SHOW);
        break;
    }
    case COMMAND_OPEN_FILE_HIDE: {
        std::string path = ExpandPath((char *)lpBuffer + 1);
        OpenFile(path.c_str(), SW_HIDE);
        break;
    }
    default:
        break;
    }
}


bool CFileManager::MakeSureDirectoryPathExists(LPCTSTR pszDirPath)
{
    LPTSTR p, pszDirCopy;
    DWORD dwAttributes;

    // Make a copy of the string for editing.

    __try {
        pszDirCopy = (LPTSTR)malloc(sizeof(TCHAR) * (lstrlen(pszDirPath) + 1));

        if(pszDirCopy == NULL)
            return FALSE;

        lstrcpy(pszDirCopy, pszDirPath);

        p = pszDirCopy;

        //  If the second character in the path is "\", then this is a UNC
        //  path, and we should skip forward until we reach the 2nd \ in the path.

        if((*p == TEXT('\\')) && (*(p+1) == TEXT('\\'))) {
            p++;            // Skip over the first \ in the name.
            p++;            // Skip over the second \ in the name.

            //  Skip until we hit the first "\" (\\Server\).

            while(*p && *p != TEXT('\\')) {
                p = CharNext(p);
            }

            // Advance over it.

            if(*p) {
                p++;
            }

            //  Skip until we hit the second "\" (\\Server\Share\).

            while(*p && *p != TEXT('\\')) {
                p = CharNext(p);
            }

            // Advance over it also.

            if(*p) {
                p++;
            }
        } else if(*(p+1) == TEXT(':')) { // Not a UNC.  See if it's <drive>:
            p++;
            p++;

            // If it exists, skip over the root specifier

            if(*p && (*p == TEXT('\\'))) {
                p++;
            }
        }

        while(*p) {
            if(*p == TEXT('\\')) {
                *p = TEXT('\0');
                dwAttributes = GetFileAttributes(pszDirCopy);

                // Nothing exists with this name.  Try to make the directory name and error if unable to.
                if(dwAttributes == 0xffffffff) {
                    if(!CreateDirectory(pszDirCopy, NULL)) {
                        if(GetLastError() != ERROR_ALREADY_EXISTS) {
                            free(pszDirCopy);
                            return FALSE;
                        }
                    }
                } else {
                    if((dwAttributes & FILE_ATTRIBUTE_DIRECTORY) != FILE_ATTRIBUTE_DIRECTORY) {
                        // Something exists with this name, but it's not a directory... Error
                        free(pszDirCopy);
                        return FALSE;
                    }
                }

                *p = TEXT('\\');
            }

            p = CharNext(p);
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        free(pszDirCopy);
        return FALSE;
    }

    free(pszDirCopy);
    return TRUE;
}

bool CFileManager::OpenFile(LPCTSTR lpFile, INT nShowCmd)
{
    char	lpSubKey[500];
    HKEY	hKey;
    char	strTemp[MAX_PATH];
    LONG	nSize = sizeof(strTemp);
    char	*lpstrCat = NULL;
    memset(strTemp, 0, sizeof(strTemp));

    const char	*lpExt = strrchr(lpFile, '.');
    if (!lpExt)
        return false;

    if (RegOpenKeyEx(HKEY_CLASSES_ROOT, lpExt, 0L, KEY_ALL_ACCESS, &hKey) != ERROR_SUCCESS)
        return false;
    RegQueryValue(hKey, NULL, strTemp, &nSize);
    RegCloseKey(hKey);
    memset(lpSubKey, 0, sizeof(lpSubKey));
    wsprintf(lpSubKey, "%s\\shell\\open\\command", strTemp);

    if (RegOpenKeyEx(HKEY_CLASSES_ROOT, lpSubKey, 0L, KEY_ALL_ACCESS, &hKey) != ERROR_SUCCESS)
        return false;
    memset(strTemp, 0, sizeof(strTemp));
    nSize = sizeof(strTemp);
    RegQueryValue(hKey, NULL, strTemp, &nSize);
    RegCloseKey(hKey);

    lpstrCat = strstr(strTemp, "\"%1");
    if (lpstrCat == NULL)
        lpstrCat = strstr(strTemp, "%1");

    if (lpstrCat == NULL) {
        lstrcat(strTemp, " ");
        lstrcat(strTemp, lpFile);
    } else
        lstrcpy(lpstrCat, lpFile);

    STARTUPINFO si = {0};
    PROCESS_INFORMATION pi;
    si.cb = sizeof si;
    if (nShowCmd != SW_HIDE)
        si.lpDesktop = "WinSta0\\Default";

    CreateProcess(NULL, strTemp, NULL, NULL, false, 0, NULL, NULL, &si, &pi);
    SAFE_CLOSE_HANDLE(pi.hProcess);
    SAFE_CLOSE_HANDLE(pi.hThread);
    return true;
}

UINT CFileManager::SendDriveList()
{
    char	DriveString[256];
    // 前一个字节为令牌，后面的52字节为驱动器跟相关属性
    BYTE	DriveList[1024];
    char	FileSystem[MAX_PATH];
    char	*pDrive = NULL;
    DriveList[0] = TOKEN_DRIVE_LIST; // 驱动器列表
    GetLogicalDriveStrings(sizeof(DriveString), DriveString);
    pDrive = DriveString;

    unsigned __int64	HDAmount = 0;
    unsigned __int64	HDFreeSpace = 0;
    unsigned __int64	AmntMB = 0; // 总大小
    unsigned __int64	FreeMB = 0; // 剩余空间

    DWORD dwOffset = 1;
    for (; *pDrive != '\0'; pDrive += lstrlen(pDrive) + 1) {
        memset(FileSystem, 0, sizeof(FileSystem));
        // 得到文件系统信息及大小
        GetVolumeInformation(pDrive, NULL, 0, NULL, NULL, NULL, FileSystem, MAX_PATH);
        SHFILEINFO	sfi = {};
        SHGetFileInfo(pDrive, FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(SHFILEINFO), SHGFI_TYPENAME | SHGFI_USEFILEATTRIBUTES);

        int	nTypeNameLen = lstrlen(sfi.szTypeName) + 1;
        int	nFileSystemLen = lstrlen(FileSystem) + 1;

        // 计算磁盘大小
        if (pDrive[0] != 'A' && pDrive[0] != 'B' && GetDiskFreeSpaceEx(pDrive, (PULARGE_INTEGER)&HDFreeSpace, (PULARGE_INTEGER)&HDAmount, NULL)) {
            AmntMB = HDAmount / 1024 / 1024;
            FreeMB = HDFreeSpace / 1024 / 1024;
        } else {
            AmntMB = 0;
            FreeMB = 0;
        }
        // 开始赋值
        DriveList[dwOffset] = pDrive[0];
        DriveList[dwOffset + 1] = GetDriveType(pDrive);

        // 磁盘空间描述占去了8字节
        memcpy(DriveList + dwOffset + 2, &AmntMB, sizeof(unsigned long));
        memcpy(DriveList + dwOffset + 6, &FreeMB, sizeof(unsigned long));

        // 磁盘卷标名及磁盘类型
        memcpy(DriveList + dwOffset + 10, sfi.szTypeName, nTypeNameLen);
        memcpy(DriveList + dwOffset + 10 + nTypeNameLen, FileSystem, nFileSystemLen);

        dwOffset += 10 + nTypeNameLen + nFileSystemLen;
    }
    HttpMask mask(DEFAULT_HOST, m_ClientObject->GetClientIPHeader());
    return m_ClientObject->Send2Server((char*)DriveList, dwOffset, &mask);
}


UINT CFileManager::SendFilesList(LPCTSTR lpszDirectory)
{
    // 重置传输方式
    m_nTransferMode = TRANSFER_MODE_NORMAL;

    // 展开环境变量 (如 %USERPROFILE%, %DESKTOP% 等)
    char	szExpandedDir[MAX_PATH];
    ExpandEnvironmentStringsA(lpszDirectory, szExpandedDir, MAX_PATH);

    UINT	nRet = 0;
    char	strPath[MAX_PATH];
    char	*pszFileName = NULL;
    LPBYTE	lpList = NULL;
    HANDLE	hFile;
    DWORD	dwOffset = 0; // 位移指针
    int		nLen = 0;
    DWORD	nBufferSize =  1024 * 10; // 先分配10K的缓冲区
    WIN32_FIND_DATA	FindFileData;

    lpList = (BYTE *)LocalAlloc(LPTR, nBufferSize);
    if (lpList==NULL) {
        return 0;
    }

    wsprintf(strPath, "%s\\*.*", szExpandedDir);
    hFile = FindFirstFile(strPath, &FindFileData);

    if (hFile == INVALID_HANDLE_VALUE) {
        BYTE bToken = TOKEN_FILE_LIST;
        return Send(&bToken, 1);
    }

    *lpList = TOKEN_FILE_LIST;

    // 1 为数据包头部所占字节,最后赋值
    dwOffset = 1;
    /*
    文件属性	1
    文件名		strlen(filename) + 1 ('\0')
    文件大小	4
    */
    do {
        // 动态扩展缓冲区
        if (dwOffset > (nBufferSize - MAX_PATH * 2)) {
            nBufferSize += MAX_PATH * 2;
            lpList = (BYTE *)LocalReAlloc(lpList, nBufferSize, LMEM_ZEROINIT|LMEM_MOVEABLE);
            if (lpList == NULL)
                continue;
        }
        pszFileName = FindFileData.cFileName;
        if (strcmp(pszFileName, ".") == 0 || strcmp(pszFileName, "..") == 0)
            continue;
        // 文件属性 1 字节
        *(lpList + dwOffset) = FindFileData.dwFileAttributes &	FILE_ATTRIBUTE_DIRECTORY;
        dwOffset++;
        // 文件名 lstrlen(pszFileName) + 1 字节
        nLen = lstrlen(pszFileName);
        memcpy(lpList + dwOffset, pszFileName, nLen);
        dwOffset += nLen;
        *(lpList + dwOffset) = 0;
        dwOffset++;

        // 文件大小 8 字节
        memcpy(lpList + dwOffset, &FindFileData.nFileSizeHigh, sizeof(DWORD));
        memcpy(lpList + dwOffset + 4, &FindFileData.nFileSizeLow, sizeof(DWORD));
        dwOffset += 8;
        // 最后访问时间 8 字节
        memcpy(lpList + dwOffset, &FindFileData.ftLastWriteTime, sizeof(FILETIME));
        dwOffset += 8;
    } while(FindNextFile(hFile, &FindFileData));

    nRet = Send(lpList, dwOffset);

    LocalFree(lpList);
    FindClose(hFile);
    return nRet;
}


// 通配符匹配 (支持 * 和 ?)
static bool WildcardMatch(const char* pattern, const char* str)
{
    while (*pattern) {
        if (*pattern == '*') {
            pattern++;
            if (!*pattern) return true;
            while (*str) {
                if (WildcardMatch(pattern, str)) return true;
                str++;
            }
            return false;
        } else if (*pattern == '?') {
            if (!*str) return false;
            pattern++;
            str++;
        } else {
            if (tolower((unsigned char)*pattern) != tolower((unsigned char)*str)) return false;
            pattern++;
            str++;
        }
    }
    return *str == '\0';
}

#define SEARCH_MAX_DEPTH    32      // 最大递归深度
#define SEARCH_MAX_RESULTS  10000   // 最大搜索结果数

// 判断是否应跳过的系统/特殊目录
static bool ShouldSkipDirectory(LPCTSTR lpszName, DWORD dwAttributes)
{
    // 跳过隐藏+系统目录 (如 $Recycle.Bin, System Volume Information)
    if ((dwAttributes & FILE_ATTRIBUTE_HIDDEN) && (dwAttributes & FILE_ATTRIBUTE_SYSTEM))
        return true;
    // 跳过重解析点 (符号链接/junction 避免死循环)
    if (dwAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
        return true;
    return false;
}

void CFileManager::SearchFilesRecursive(LPCTSTR lpszDirectory, LPCTSTR lpszPattern,
                                        LPBYTE &lpList, DWORD &dwOffset, DWORD &nBufferSize, int nDepth, DWORD &nResultCount, DWORD &dwLastSendTime)
{
    if (!m_bSearching) return;
    if (nDepth > SEARCH_MAX_DEPTH) return;
    if (nResultCount >= SEARCH_MAX_RESULTS) {
        m_bSearching = false;
        return;
    }

    char strPath[MAX_PATH];
    WIN32_FIND_DATA FindFileData;

    wsprintf(strPath, "%s\\*.*", lpszDirectory);
    HANDLE hFile = FindFirstFile(strPath, &FindFileData);
    if (hFile == INVALID_HANDLE_VALUE) return;

    do {
        if (!m_bSearching) break;
        if (nResultCount >= SEARCH_MAX_RESULTS) break;

        if (strcmp(FindFileData.cFileName, ".") == 0 || strcmp(FindFileData.cFileName, "..") == 0)
            continue;

        BOOL bIsDir = FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;

        char szFullPath[MAX_PATH];
        wsprintf(szFullPath, "%s\\%s", lpszDirectory, FindFileData.cFileName);

        // 检查文件名是否匹配搜索模式
        if (WildcardMatch(lpszPattern, FindFileData.cFileName)) {
            int nFullPathLen = lstrlen(szFullPath);

            // 需要空间: 1(attr) + nFullPathLen+1(name) + 8(size) + 8(time)
            DWORD nNeeded = 1 + nFullPathLen + 1 + 16;

            // 缓冲区快满时先发送当前批次
            if (dwOffset + nNeeded > nBufferSize - MAX_PATH) {
                if (dwOffset > 1) {
                    Send(lpList, dwOffset);
                    dwLastSendTime = GetTickCount();
                }
                *lpList = TOKEN_SEARCH_FILE_LIST;
                dwOffset = 1;
            }

            // 动态扩展
            if (dwOffset + nNeeded > nBufferSize) {
                nBufferSize = dwOffset + nNeeded + MAX_PATH * 2;
                lpList = (BYTE *)LocalReAlloc(lpList, nBufferSize, LMEM_ZEROINIT | LMEM_MOVEABLE);
                if (lpList == NULL) break;
            }

            // 文件属性
            *(lpList + dwOffset) = FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
            dwOffset++;
            // 完整路径
            memcpy(lpList + dwOffset, szFullPath, nFullPathLen);
            dwOffset += nFullPathLen;
            *(lpList + dwOffset) = 0;
            dwOffset++;
            // 文件大小 8 字节
            memcpy(lpList + dwOffset, &FindFileData.nFileSizeHigh, sizeof(DWORD));
            memcpy(lpList + dwOffset + 4, &FindFileData.nFileSizeLow, sizeof(DWORD));
            dwOffset += 8;
            // 最后修改时间 8 字节
            memcpy(lpList + dwOffset, &FindFileData.ftLastWriteTime, sizeof(FILETIME));
            dwOffset += 8;

            nResultCount++;
        }

        // 超过2秒未发送，先把缓冲区内的数据发出去
        if (dwOffset > 1 && GetTickCount() - dwLastSendTime >= 2000) {
            Send(lpList, dwOffset);
            dwLastSendTime = GetTickCount();
            *lpList = TOKEN_SEARCH_FILE_LIST;
            dwOffset = 1;
        }

        // 递归搜索子目录 (跳过系统/隐藏/重解析点目录)
        if (bIsDir && !ShouldSkipDirectory(FindFileData.cFileName, FindFileData.dwFileAttributes)) {
            SearchFilesRecursive(szFullPath, lpszPattern, lpList, dwOffset, nBufferSize, nDepth + 1, nResultCount, dwLastSendTime);
        }
    } while (FindNextFile(hFile, &FindFileData));

    FindClose(hFile);
}

DWORD WINAPI CFileManager::SearchThreadProc(LPVOID lpParam)
{
    SearchParam *pParam = (SearchParam *)lpParam;
    pParam->pThis->SearchFiles(pParam->szPath, pParam->szName);
    delete pParam;
    return 0;
}

void CFileManager::SearchFiles(LPCTSTR lpszSearchPath, LPCTSTR lpszSearchName)
{
    // 展开环境变量
    char szExpandedPath[MAX_PATH];
    ExpandEnvironmentStringsA(lpszSearchPath, szExpandedPath, MAX_PATH);

    // 构建搜索模式 (如 *.txt 或 *keyword*)
    char szPattern[MAX_PATH];
    if (strchr(lpszSearchName, '*') == NULL && strchr(lpszSearchName, '?') == NULL) {
        wsprintf(szPattern, "*%s*", lpszSearchName);
    } else {
        lstrcpy(szPattern, lpszSearchName);
    }

    DWORD nBufferSize = 1024 * 64;
    LPBYTE lpList = (BYTE *)LocalAlloc(LPTR, nBufferSize);
    if (lpList == NULL) {
        SendToken(TOKEN_SEARCH_FILE_FINISH);
        return;
    }

    *lpList = TOKEN_SEARCH_FILE_LIST;
    DWORD dwOffset = 1;
    DWORD nResultCount = 0;
    DWORD dwLastSendTime = GetTickCount();

    DWORD dwStart = dwLastSendTime;
    Mprintf("[Search] Start: path=\"%s\" pattern=\"%s\"\n", szExpandedPath, szPattern);

    SearchFilesRecursive(szExpandedPath, szPattern, lpList, dwOffset, nBufferSize, 0, nResultCount, dwLastSendTime);

    DWORD dwElapsed = GetTickCount() - dwStart;
    Mprintf("[Search] Done: %d results, %d ms, stopped=%d\n", nResultCount, dwElapsed, !m_bSearching);

    // 发送剩余数据 (如果已停止则丢弃)
    if (m_bSearching && lpList && dwOffset > 1) {
        Send(lpList, dwOffset);
    }

    if (lpList) LocalFree(lpList);
    SendToken(TOKEN_SEARCH_FILE_FINISH);
    m_bSearching = false;
}

bool CFileManager::DeleteDirectory(LPCTSTR lpszDirectory)
{
    WIN32_FIND_DATA	wfd;
    char	lpszFilter[MAX_PATH];

    wsprintf(lpszFilter, "%s\\*.*", lpszDirectory);

    HANDLE hFind = FindFirstFile(lpszFilter, &wfd);
    if (hFind == INVALID_HANDLE_VALUE) // 如果没有找到或查找失败
        return FALSE;

    do {
        if (wfd.cFileName[0] != '.') {
            if (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                char strDirectory[MAX_PATH];
                wsprintf(strDirectory, "%s\\%s", lpszDirectory, wfd.cFileName);
                DeleteDirectory(strDirectory);
            } else {
                char strFile[MAX_PATH];
                wsprintf(strFile, "%s\\%s", lpszDirectory, wfd.cFileName);
                DeleteFile(strFile);
            }
        }
    } while (FindNextFile(hFind, &wfd));

    FindClose(hFind); // 关闭查找句柄

    if(!RemoveDirectory(lpszDirectory)) {
        return FALSE;
    }
    return true;
}

UINT CFileManager::SendFileSize(LPCTSTR lpszFileName)
{
    UINT	nRet = 0;
    DWORD	dwSizeHigh;
    DWORD	dwSizeLow;
    // 1 字节token, 8字节大小, 文件名称, '\0'
    HANDLE	hFile;
    // 保存当前正在操作的文件名（原始路径，可能含环境变量）
    memset(m_strCurrentProcessFileName, 0, sizeof(m_strCurrentProcessFileName));
    strcpy(m_strCurrentProcessFileName, lpszFileName);

    // 展开环境变量用于实际文件操作
    std::string strExpanded = ExpandPath(lpszFileName);
    hFile = CreateFile(strExpanded.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    if (hFile == INVALID_HANDLE_VALUE)
        return FALSE;
    dwSizeLow =	GetFileSize(hFile, &dwSizeHigh);
    SAFE_CLOSE_HANDLE(hFile);
    // 构造数据包，发送文件长度
    // 注意：发送原始路径（可能含环境变量），让服务端能正确 Replace
    int		nPacketSize = lstrlen(lpszFileName) + 10;
    BYTE	*bPacket = (BYTE *)LocalAlloc(LPTR, nPacketSize);
    if (bPacket==NULL) {
        return 0;
    }
    memset(bPacket, 0, nPacketSize);

    bPacket[0] = TOKEN_FILE_SIZE;
    FILESIZE *pFileSize = (FILESIZE *)(bPacket + 1);
    pFileSize->dwSizeHigh = dwSizeHigh;
    pFileSize->dwSizeLow = dwSizeLow;
    memcpy(bPacket + 9, lpszFileName, lstrlen(lpszFileName) + 1);

    nRet = Send(bPacket, nPacketSize);
    LocalFree(bPacket);
    return nRet;
}

UINT CFileManager::SendFileData(LPBYTE lpBuffer)
{
    UINT		nRet = 0;
    FILESIZE	*pFileSize;

    pFileSize = (FILESIZE *)lpBuffer;

    // 远程跳过，传送下一个
    if (pFileSize->dwSizeLow == -1) {
        UploadNext();
        return 0;
    }
    // 展开环境变量用于实际文件操作
    std::string strExpanded = ExpandPath(m_strCurrentProcessFileName);
    HANDLE	hFile;
    hFile = CreateFile(strExpanded.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    if (hFile == INVALID_HANDLE_VALUE)
        return -1;

    SetFilePointer(hFile, pFileSize->dwSizeLow, (long *)&(pFileSize->dwSizeHigh), FILE_BEGIN);

    int		nHeadLength = 9; // 1 + 4 + 4数据包头部大小
    DWORD	nNumberOfBytesToRead = MAX_SEND_BUFFER - nHeadLength;
    DWORD	nNumberOfBytesRead = 0;

    LPBYTE	lpPacket = (LPBYTE)LocalAlloc(LPTR, MAX_SEND_BUFFER);
    if (lpPacket == NULL)
        return -1;
    // Token,  大小，偏移，文件名，数据
    lpPacket[0] = TOKEN_FILE_DATA;
    memcpy(lpPacket + 1, pFileSize, sizeof(FILESIZE));
    ReadFile(hFile, lpPacket + nHeadLength, nNumberOfBytesToRead, &nNumberOfBytesRead, NULL);
    SAFE_CLOSE_HANDLE(hFile);

    if (nNumberOfBytesRead > 0) {
        int	nPacketSize = nNumberOfBytesRead + nHeadLength;
        nRet = Send(lpPacket, nPacketSize);
    } else {
        UploadNext();
    }

    LocalFree(lpPacket);

    return nRet;
}

// 传送下一个文件
void CFileManager::UploadNext()
{
    std::list <std::string>::iterator it = m_UploadList.begin();
    // 删除一个任务
    m_UploadList.erase(it);
    // 还有上传任务
    if(m_UploadList.empty()) {
        SendToken(TOKEN_TRANSFER_FINISH);
    } else {
        // 上传下一个
        it = m_UploadList.begin();
        SendFileSize((*it).c_str());
    }
}

int CFileManager::SendToken(BYTE bToken)
{
    return Send(&bToken, 1);
}

bool CFileManager::UploadToRemote(LPBYTE lpBuffer)
{
    if (lpBuffer[lstrlen((char *)lpBuffer) - 1] == '\\') {
        FixedUploadList((char *)lpBuffer);
        if (m_UploadList.empty()) {
            StopTransfer();
            return true;
        }
    } else {
        m_UploadList.push_back((char *)lpBuffer);
    }

    std::list <std::string>::iterator it = m_UploadList.begin();
    // 发送第一个文件
    SendFileSize((*it).c_str());

    return true;
}

bool CFileManager::FixedUploadList(LPCTSTR lpPathName)
{
    WIN32_FIND_DATA	wfd;
    char	lpszFilter[MAX_PATH];
    char	*lpszSlash = NULL;
    memset(lpszFilter, 0, sizeof(lpszFilter));

    if (lpPathName[lstrlen(lpPathName) - 1] != '\\')
        lpszSlash = "\\";
    else
        lpszSlash = "";

    // 展开环境变量用于 FindFirstFile
    std::string strExpandedPath = ExpandPath(lpPathName);
    wsprintf(lpszFilter, "%s%s*.*", strExpandedPath.c_str(), lpszSlash);

    HANDLE hFind = FindFirstFile(lpszFilter, &wfd);
    if (hFind == INVALID_HANDLE_VALUE) // 如果没有找到或查找失败
        return false;

    do {
        if (wfd.cFileName[0] != '.') {
            if (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                // 保持原始路径（可能含环境变量）+ 子目录名
                char strDirectory[MAX_PATH];
                wsprintf(strDirectory, "%s%s%s", lpPathName, lpszSlash, wfd.cFileName);
                FixedUploadList(strDirectory);
            } else {
                // 保持原始路径（可能含环境变量）+ 文件名
                char strFile[MAX_PATH];
                wsprintf(strFile, "%s%s%s", lpPathName, lpszSlash, wfd.cFileName);
                m_UploadList.push_back(strFile);
            }
        }
    } while (FindNextFile(hFind, &wfd));

    FindClose(hFind); // 关闭查找句柄
    return true;
}

void CFileManager::StopTransfer()
{
    if (!m_UploadList.empty())
        m_UploadList.clear();
    SendToken(TOKEN_TRANSFER_FINISH);
}

void CFileManager::CreateLocalRecvFile(LPBYTE lpBuffer)
{
    FILESIZE	*pFileSize = (FILESIZE *)lpBuffer;
    // 保存当前正在操作的文件名
    memset(m_strCurrentProcessFileName, 0, sizeof(m_strCurrentProcessFileName));
    strcpy(m_strCurrentProcessFileName, (char *)lpBuffer + 8);

    // 保存文件长度
    m_nCurrentProcessFileLength = (pFileSize->dwSizeHigh * (MAXDWORD + long long(1))) + pFileSize->dwSizeLow;

    // 创建多层目录
    MakeSureDirectoryPathExists(m_strCurrentProcessFileName);

    WIN32_FIND_DATA FindFileData;
    HANDLE hFind = FindFirstFile(m_strCurrentProcessFileName, &FindFileData);

    if (hFind != INVALID_HANDLE_VALUE
        && m_nTransferMode != TRANSFER_MODE_OVERWRITE_ALL
        && m_nTransferMode != TRANSFER_MODE_ADDITION_ALL
        && m_nTransferMode != TRANSFER_MODE_JUMP_ALL
       ) {
        SendToken(TOKEN_GET_TRANSFER_MODE);
    } else {
        GetFileData();
    }
    FindClose(hFind);
}

void CFileManager::GetFileData()
{
    int	nTransferMode;
    switch (m_nTransferMode) {
    case TRANSFER_MODE_OVERWRITE_ALL:
        nTransferMode = TRANSFER_MODE_OVERWRITE;
        break;
    case TRANSFER_MODE_ADDITION_ALL:
        nTransferMode = TRANSFER_MODE_ADDITION;
        break;
    case TRANSFER_MODE_JUMP_ALL:
        nTransferMode = TRANSFER_MODE_JUMP;
        break;
    default:
        nTransferMode = m_nTransferMode;
    }

    WIN32_FIND_DATA FindFileData;
    HANDLE hFind = FindFirstFile(m_strCurrentProcessFileName, &FindFileData);

    //  1字节Token,四字节偏移高四位，四字节偏移低四位
    BYTE	bToken[9];
    DWORD	dwCreationDisposition = CREATE_ALWAYS; // 文件打开方式
    memset(bToken, 0, sizeof(bToken));
    bToken[0] = TOKEN_DATA_CONTINUE;

    // 文件已经存在
    if (hFind != INVALID_HANDLE_VALUE) {
        // 提示点什么
        // 如果是续传
        if (nTransferMode == TRANSFER_MODE_ADDITION) {
            memcpy(bToken + 1, &FindFileData.nFileSizeHigh, 4);
            memcpy(bToken + 5, &FindFileData.nFileSizeLow, 4);
            dwCreationDisposition = OPEN_EXISTING;
        }
        // 覆盖
        else if (nTransferMode == TRANSFER_MODE_OVERWRITE) {
            // 偏移置0
            memset(bToken + 1, 0, 8);
            // 重新创建
            dwCreationDisposition = CREATE_ALWAYS;
        }
        // 传送下一个
        else if (nTransferMode == TRANSFER_MODE_JUMP) {
            DWORD dwOffset = -1;
            memcpy(bToken + 5, &dwOffset, 4);
            dwCreationDisposition = OPEN_EXISTING;
        }
    } else {
        // 偏移置0
        memset(bToken + 1, 0, 8);
        // 重新创建
        dwCreationDisposition = CREATE_ALWAYS;
    }
    FindClose(hFind);

    HANDLE	hFile =
        CreateFile
        (
            m_strCurrentProcessFileName,
            GENERIC_WRITE,
            FILE_SHARE_WRITE,
            NULL,
            dwCreationDisposition,
            FILE_ATTRIBUTE_NORMAL,
            0
        );
    // 需要错误处理
    if (hFile == INVALID_HANDLE_VALUE) {
        m_nCurrentProcessFileLength = 0;
        return;
    }
    SAFE_CLOSE_HANDLE(hFile);

    Send(bToken, sizeof(bToken));
}

void CFileManager::WriteLocalRecvFile(LPBYTE lpBuffer, UINT nSize)
{
    // 传输完毕
    BYTE	*pData;
    DWORD	dwBytesToWrite;
    DWORD	dwBytesWrite;
    int		nHeadLength = 9; // 1 + 4 + 4  数据包头部大小，为固定的9
    FILESIZE	*pFileSize;
    // 得到数据的偏移
    pData = lpBuffer + 8;

    pFileSize = (FILESIZE *)lpBuffer;

    // 得到数据在文件中的偏移
    LONG	dwOffsetHigh = pFileSize->dwSizeHigh;
    LONG	dwOffsetLow = pFileSize->dwSizeLow;

    dwBytesToWrite = nSize - 8;

    HANDLE	hFile =
        CreateFile
        (
            m_strCurrentProcessFileName,
            GENERIC_WRITE,
            FILE_SHARE_WRITE,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            0
        );

    SetFilePointer(hFile, dwOffsetLow, &dwOffsetHigh, FILE_BEGIN);

    int nRet = 0;
    // 写入文件
    nRet = WriteFile
           (
               hFile,
               pData,
               dwBytesToWrite,
               &dwBytesWrite,
               NULL
           );
    SAFE_CLOSE_HANDLE(hFile);
    // 为了比较，计数器递增
    BYTE	bToken[9];
    bToken[0] = TOKEN_DATA_CONTINUE;
    dwOffsetLow += dwBytesWrite;
    memcpy(bToken + 1, &dwOffsetHigh, sizeof(dwOffsetHigh));
    memcpy(bToken + 5, &dwOffsetLow, sizeof(dwOffsetLow));
    Send(bToken, sizeof(bToken));
}

void CFileManager::SetTransferMode(LPBYTE lpBuffer)
{
    memcpy(&m_nTransferMode, lpBuffer, sizeof(m_nTransferMode));
    GetFileData();
}

void CFileManager::CreateFolder(LPBYTE lpBuffer)
{
    MakeSureDirectoryPathExists((char *)lpBuffer);
    SendToken(TOKEN_CREATEFOLDER_FINISH);
}

void CFileManager::Rename(LPBYTE lpBuffer)
{
    LPCTSTR lpExistingFileName = (char *)lpBuffer;
    LPCTSTR lpNewFileName = lpExistingFileName + lstrlen(lpExistingFileName) + 1;
    ::MoveFile(lpExistingFileName, lpNewFileName);
    SendToken(TOKEN_RENAME_FINISH);
}
