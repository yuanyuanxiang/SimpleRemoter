
// 2015Remote.cpp : 定义应用程序的类行为。
//

#include "stdafx.h"
#include "2015Remote.h"
#include "SplashDlg.h"
#include "2015RemoteDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// dump相关
#include <io.h>
#include <direct.h>
#include <DbgHelp.h>
#include <intrin.h>  // for __cpuid, __cpuidex
#include "IOCPUDPServer.h"
#include "ServerServiceWrapper.h"
#include "common/SafeString.h"
#include "CrashReport.h"
#pragma comment(lib, "Dbghelp.lib")

// Check if CPU supports AVX2 instruction set
static BOOL IsAVX2Supported()
{
    int cpuInfo[4] = { 0 };
    __cpuid(cpuInfo, 0);
    int nIds = cpuInfo[0];

    if (nIds >= 7) {
        __cpuidex(cpuInfo, 7, 0);
        return (cpuInfo[1] & (1 << 5)) != 0;  // EBX bit 5 = AVX2
    }
    return FALSE;
}

BOOL ServerPair::StartServer(pfnNotifyProc NotifyProc, pfnOfflineProc OffProc, USHORT uPort)
{
    UINT ret1 = m_tcpServer->StartServer(NotifyProc, OffProc, uPort);
    if (ret1) THIS_APP->MessageBox(_L(_T("启动TCP服务失败: ")) + std::to_string(uPort).c_str()
                                       + _L(_T("。错误码: ")) + std::to_string(ret1).c_str(), _TR("提示"), MB_ICONINFORMATION);
    UINT ret2 = m_udpServer->StartServer(NotifyProc, OffProc, uPort);
    if (ret2) THIS_APP->MessageBox(_L(_T("启动UDP服务失败: ")) + std::to_string(uPort).c_str()
                                       + _L(_T("。错误码: ")) + std::to_string(ret2).c_str(), _TR("提示"), MB_ICONINFORMATION);
    return (ret1 == 0 || ret2 == 0);
}

CMy2015RemoteApp* GetThisApp()
{
    return ((CMy2015RemoteApp*)AfxGetApp());
}

config& GetThisCfg()
{
    config *cfg = GetThisApp()->GetCfg();
    return *cfg;
}

std::string GetMasterHash()
{
    static std::string hash(skCrypt(MASTER_HASH));
    return hash;
}

/**
* @brief 程序遇到未知BUG导致终止时调用此函数，不弹框
* 并且转储dump文件到dump目录.
*/
long WINAPI whenbuged(_EXCEPTION_POINTERS *excp)
{
    // 获取dump文件夹，若不存在，则创建之
    char dumpDir[_MAX_PATH];
    char dumpFile[_MAX_PATH + 64];

    if (!GetModuleFileNameA(NULL, dumpDir, _MAX_PATH)) {
        return EXCEPTION_EXECUTE_HANDLER;
    }

    char* p = strrchr(dumpDir, '\\');
    if (p) {
        strcpy_s(p + 1, _MAX_PATH - (p - dumpDir + 1), "dump");
    } else {
        strcpy_s(dumpDir, _MAX_PATH, "dump");
    }

    if (_access(dumpDir, 0) == -1)
        _mkdir(dumpDir);

    // 构建完整的dump文件路径
    char curTime[64];
    time_t TIME = time(0);
    struct tm localTime;
    localtime_s(&localTime, &TIME);
    strftime(curTime, sizeof(curTime), "\\YAMA_%Y-%m-%d %H%M%S.dmp", &localTime);
    sprintf_s(dumpFile, sizeof(dumpFile), "%s%s", dumpDir, curTime);

    HANDLE hFile = ::CreateFileA(dumpFile, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                                 FILE_ATTRIBUTE_NORMAL, NULL);
    if(INVALID_HANDLE_VALUE != hFile) {
        MINIDUMP_EXCEPTION_INFORMATION einfo = {::GetCurrentThreadId(), excp, FALSE};
        ::MiniDumpWriteDump(::GetCurrentProcess(), ::GetCurrentProcessId(),
                            hFile, MiniDumpWithFullMemory, &einfo, NULL, NULL);
        SAFE_CLOSE_HANDLE(hFile);
    }

    return EXCEPTION_EXECUTE_HANDLER;
}

// CMy2015RemoteApp

BEGIN_MESSAGE_MAP(CMy2015RemoteApp, CWinApp)
    ON_COMMAND(ID_HELP, &CWinApp::OnHelp)
END_MESSAGE_MAP()

std::string GetPwdHash();

// CMy2015RemoteApp 构造

// 自定义压缩文件
#include <windows.h>
#include <shlobj.h>
#include "ZstdArchive.h"

bool RegisterZstaMenu(const std::string& exePath)
{
    HKEY hKey;
    CString _compressText = _TR("压缩为 ZSTA 文件");
    CString _extractText = _TR("解压 ZSTA 文件");
    CString _zstaDesc = _TR("ZSTA 压缩文件");
    const char* compressText = (LPCSTR)_compressText;
    const char* extractText = (LPCSTR)_extractText;
    const char* zstaDesc = (LPCSTR)_zstaDesc;
    const char* zstaExt = "ZstaArchive";

    // 文件右键
    if (RegCreateKeyExA(HKEY_CLASSES_ROOT, "*\\shell\\CompressToZsta", 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        RegSetValueExA(hKey, NULL, 0, REG_SZ, (BYTE*)compressText, strlen(compressText) + 1);
        RegCloseKey(hKey);
    }
    std::string compressCmd = "\"" + exePath + "\" -c \"%1\"";
    if (RegCreateKeyExA(HKEY_CLASSES_ROOT, "*\\shell\\CompressToZsta\\command", 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        RegSetValueExA(hKey, NULL, 0, REG_SZ, (BYTE*)compressCmd.c_str(), compressCmd.size() + 1);
        RegCloseKey(hKey);
    }

    // 文件夹右键
    if (RegCreateKeyExA(HKEY_CLASSES_ROOT, "Directory\\shell\\CompressToZsta", 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        RegSetValueExA(hKey, NULL, 0, REG_SZ, (BYTE*)compressText, strlen(compressText) + 1);
        RegCloseKey(hKey);
    }
    if (RegCreateKeyExA(HKEY_CLASSES_ROOT, "Directory\\shell\\CompressToZsta\\command", 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        RegSetValueExA(hKey, NULL, 0, REG_SZ, (BYTE*)compressCmd.c_str(), compressCmd.size() + 1);
        RegCloseKey(hKey);
    }

    // .zsta 文件关联
    if (RegCreateKeyExA(HKEY_CLASSES_ROOT, ".zsta", 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        RegSetValueExA(hKey, NULL, 0, REG_SZ, (BYTE*)zstaExt, strlen(zstaExt) + 1);
        RegCloseKey(hKey);
    }
    if (RegCreateKeyExA(HKEY_CLASSES_ROOT, "ZstaArchive", 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        RegSetValueExA(hKey, NULL, 0, REG_SZ, (BYTE*)zstaDesc, strlen(zstaDesc) + 1);
        RegCloseKey(hKey);
    }

    // .zsta 右键菜单
    if (RegCreateKeyExA(HKEY_CLASSES_ROOT, "ZstaArchive\\shell\\extract", 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        RegSetValueExA(hKey, NULL, 0, REG_SZ, (BYTE*)extractText, strlen(extractText) + 1);
        RegCloseKey(hKey);
    }
    std::string extractCmd = "\"" + exePath + "\" -x \"%1\"";
    if (RegCreateKeyExA(HKEY_CLASSES_ROOT, "ZstaArchive\\shell\\extract\\command", 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        RegSetValueExA(hKey, NULL, 0, REG_SZ, (BYTE*)extractCmd.c_str(), extractCmd.size() + 1);
        RegCloseKey(hKey);
    }

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
    return true;
}

bool UnregisterZstaMenu()
{
    RegDeleteTreeA(HKEY_CLASSES_ROOT, "*\\shell\\CompressToZsta");
    RegDeleteTreeA(HKEY_CLASSES_ROOT, "Directory\\shell\\CompressToZsta");
    RegDeleteTreeA(HKEY_CLASSES_ROOT, ".zsta");
    RegDeleteTreeA(HKEY_CLASSES_ROOT, "ZstaArchive");
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
    return true;
}

std::string RemoveTrailingSlash(const std::string& path)
{
    std::string p = path;
    while (!p.empty() && (p.back() == '/' || p.back() == '\\')) {
        p.pop_back();
    }
    return p;
}

std::string RemoveZstaExtension(const std::string& path)
{
    if (path.size() >= 5) {
        std::string ext = path.substr(path.size() - 5);
        for (char& c : ext) c = tolower(c);
        if (ext == ".zsta") {
            return path.substr(0, path.size() - 5);
        }
    }
    return path + "_extract";
}

CMy2015RemoteApp::CMy2015RemoteApp()
{
    // 支持重新启动管理器
    m_dwRestartManagerSupportFlags = AFX_RESTART_MANAGER_SUPPORT_RESTART;

    // TODO: 在此处添加构造代码，
    // 将所有重要的初始化放置在 InitInstance 中
    m_Mutex = NULL;
#ifdef _DEBUG
    std::string masterHash(GetMasterHash());
    m_iniFile = GetPwdHash() == masterHash ? new config : new iniFile;
#else
    m_iniFile = new iniFile;
#endif

    srand(static_cast<unsigned int>(time(0)));
}


// 唯一的一个 CMy2015RemoteApp 对象

CMy2015RemoteApp theApp;

// 处理服务相关的命令行参数
// 返回值: TRUE 表示已处理服务命令（程序应退出），FALSE 表示继续正常启动
static BOOL HandleServiceCommandLine()
{
    CString cmdLine = ::GetCommandLine();
    cmdLine.MakeLower();

    // -service: 作为服务运行
    if (cmdLine.Find(_T("-service")) != -1) {
        int r = ServerService_Run();
        Mprintf("[HandleServiceCommandLine] ServerService_Run %s\n", r ? "failed" : "succeed");
        return TRUE;
    }

    // -install: 安装服务
    if (cmdLine.Find(_T("-install")) != -1) {
        BOOL r = ServerService_Install();
        Mprintf("[HandleServiceCommandLine] ServerService_Install %s\n", !r ? "failed" : "succeed");
        return TRUE;
    }

    // -uninstall: 卸载服务
    if (cmdLine.Find(_T("-uninstall")) != -1) {
        BOOL r = ServerService_Uninstall();
        Mprintf("[HandleServiceCommandLine] ServerService_Uninstall %s\n", !r ? "failed" : "succeed");
        return TRUE;
    }

    // -agent 或 -agent-asuser: 由服务启动的GUI代理模式
    // 必须先检查更长的参数 -agent-asuser，避免被 -agent 误匹配
    if (cmdLine.Find(_T("-agent-asuser")) != -1) {
        Mprintf("[HandleServiceCommandLine] Run service agent as USER: '%s'\n", cmdLine.GetString());
        return FALSE;
    }
    if (cmdLine.Find(_T("-agent")) != -1) {
        Mprintf("[HandleServiceCommandLine] Run service agent as SYSTEM: '%s'\n", cmdLine.GetString());
        return FALSE;
    }

    // 无参数时，作为服务启动
    BOOL registered = FALSE;
    BOOL running = FALSE;
    char servicePath[MAX_PATH] = { 0 };
    BOOL r = ServerService_CheckStatus(&registered, &running, servicePath, MAX_PATH);
    Mprintf("[HandleServiceCommandLine] ServerService_CheckStatus %s\n", !r ? "failed" : "succeed");

    char curPath[MAX_PATH];
    GetModuleFileNameA(NULL, curPath, MAX_PATH);

    _strlwr(servicePath);
    _strlwr(curPath);
    BOOL same = (strstr(servicePath, curPath) != 0);
    if (registered && !same) {
        BOOL r = ServerService_Uninstall();
        Mprintf("[HandleServiceCommandLine] ServerService Uninstall %s: %s\n", r ? "succeed" : "failed", servicePath);
        registered = FALSE;
    }
    if (!registered) {
        BOOL r = ServerService_Install();
        Mprintf("[HandleServiceCommandLine] ServerService Install: %s\n", r ? "succeed" : "failed", curPath);
        return r;
    } else if (!running) {
        int r = ServerService_Run();
        Mprintf("[HandleServiceCommandLine] ServerService Run '%s' %s\n", curPath, r == ERROR_SUCCESS ? "succeed" : "failed");
        if (r) {
            r = ServerService_StartSimple();
            Mprintf("[HandleServiceCommandLine] ServerService Start '%s' %s\n", curPath, r == ERROR_SUCCESS ? "succeed" : "failed");
            return r == ERROR_SUCCESS;
        }
        return TRUE;
    }
    return TRUE;
}

// 检查是否以代理模式运行
static BOOL IsAgentMode()
{
    CString cmdLine = ::GetCommandLine();
    cmdLine.MakeLower();
    return cmdLine.Find(_T("-agent")) != -1;
}

// CMy2015RemoteApp 初始化

BOOL IsRunningAsAdmin()
{
    BOOL isAdmin = FALSE;
    PSID administratorsGroup = NULL;

    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&NtAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS,
                                 0, 0, 0, 0, 0, 0, &administratorsGroup)) {
        if (!CheckTokenMembership(NULL, administratorsGroup, &isAdmin)) {
            isAdmin = FALSE;
        }

        FreeSid(administratorsGroup);
    }

    return isAdmin;
}

BOOL LaunchAsAdmin(const char* szFilePath, const char* verb)
{
    CString cmdLine = AfxGetApp()->m_lpCmdLine, arg;
    cmdLine.Trim(); // 去掉前后空格

    if (cmdLine.CompareNoCase(_T("-agent-asuser")) == 0) {
		arg = "-agent-asuser";
    }
    else if (cmdLine.CompareNoCase(_T("-agent")) == 0) {
		arg = "-agent";
    }

    SHELLEXECUTEINFOA shExecInfo;
    ZeroMemory(&shExecInfo, sizeof(SHELLEXECUTEINFOA));
    shExecInfo.cbSize = sizeof(SHELLEXECUTEINFOA);
    shExecInfo.fMask = SEE_MASK_DEFAULT;
    shExecInfo.hwnd = NULL;
    shExecInfo.lpVerb = verb;
    shExecInfo.lpFile = szFilePath;
    shExecInfo.nShow = SW_NORMAL;
	shExecInfo.lpParameters = arg.IsEmpty() ? NULL : (LPCSTR)arg;

    return ShellExecuteExA(&shExecInfo);
}

BOOL CMy2015RemoteApp::ProcessZstaCmd()
{
    // 检查是否已注册右键菜单
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);

    // 检查当前注册的路径是否是自己
    HKEY hKey;
    bool needRegister = false;
    if (RegOpenKeyExA(HKEY_CLASSES_ROOT, "ZstaArchive\\shell\\extract\\command",
                      0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        char regPath[MAX_PATH * 2] = { 0 };
        DWORD size = sizeof(regPath);
        RegQueryValueExA(hKey, NULL, NULL, NULL, (BYTE*)regPath, &size);
        RegCloseKey(hKey);

        // 检查注册的路径是否包含当前程序路径
        if (strstr(regPath, exePath) == NULL) {
            needRegister = true;  // 路径不同，需要重新注册
        }
    } else {
        needRegister = true;  // 未注册
    }

    if (needRegister) {
        RegisterZstaMenu(exePath);
    }
    // 处理自定义压缩和解压命令
    if (__argc >= 3) {
        std::string cmd = __argv[1];
        std::string path = __argv[2];

        // 压缩
        if (cmd == "-c") {
            std::string src = RemoveTrailingSlash(path);
            std::string dst = src + ".zsta";
            auto b = (zsta::CZstdArchive::Compress(src, dst) == zsta::Error::Success);
            Mprintf("压缩%s: %s -> %s\n", b ? "成功" : "失败", src.c_str(), dst.c_str());
            return FALSE;
        }

        // 解压
        if (cmd == "-x") {
            std::string dst = RemoveZstaExtension(path);
            auto b = (zsta::CZstdArchive::Extract(path, dst) == zsta::Error::Success);
            Mprintf("解压%s: %s -> %s\n", b ? "成功" : "失败", path.c_str(), dst.c_str());
            return FALSE;
        }
    }
    return TRUE;
}

BOOL CMy2015RemoteApp::InitInstance()
{
	CString cmdLine = ::GetCommandLine();
    Mprintf("启动运行: %s\n", cmdLine);

    // 防止用户频繁点击导致多实例启动冲突
    // 服务进程、安装/卸载命令不需要互斥量检查（由 SCM 管理）
    // 代理模式和普通模式使用相同互斥量（TCP 监听，只能运行一个）
#ifndef _DEBUG
    {
        CString cmdLineLower = cmdLine;
        cmdLineLower.MakeLower();

        // 以下命令跳过互斥量检查：
        // - 服务进程和安装/卸载命令（由 SCM 管理）
        // - 压缩/解压命令（独立功能，不启动主程序）
        BOOL skipMutex = (cmdLineLower.Find(_T("-service")) != -1) ||
                         (cmdLineLower.Find(_T("-install")) != -1) ||
                         (cmdLineLower.Find(_T("-uninstall")) != -1) ||
                         (cmdLineLower.Find(_T("-zsta")) != -1);

        if (!skipMutex) {
            std::string masterHash(GetMasterHash());
            std::string mu = GetPwdHash()==masterHash ? "MASTER.EXE" : "YAMA.EXE";
            m_Mutex = CreateMutex(NULL, FALSE, mu.c_str());
            if (ERROR_ALREADY_EXISTS == GetLastError()) {
                SAFE_CLOSE_HANDLE(m_Mutex);
                m_Mutex = NULL;
                // 不弹框，静默退出，避免用户频繁点击时弹出多个对话框
                Mprintf("[InitInstance] 一个主控程序已经在运行，静默退出。\n");
                return FALSE;
            }
        }
    }
#endif

    // 安装安全字符串 handler，避免 _s 函数参数无效时崩溃且无 dump
    InstallSafeStringHandler();

    // Check if CPU supports AVX2 instruction set
    if (!IsAVX2Supported()) {
        ::MessageBoxA(NULL,
            "此程序需要支持 AVX2 指令集的 CPU（2013年后的处理器）。您的 CPU 不支持 AVX2，程序无法运行。",
            "CPU 不兼容", MB_ICONERROR);
        return FALSE;
    }

    if (!ProcessZstaCmd()) {
        Mprintf("[InitInstance] 处理自定义压缩/解压命令后退出。\n");
        return FALSE;
    }

    BOOL runNormal = THIS_CFG.GetInt("settings", "RunNormal", 0);

    // 检查代理崩溃保护标志（只在代理模式下检查）
    // 如果服务检测到代理连续崩溃，会设置此标志
    {
        CString cmdLineLower = cmdLine;
        cmdLineLower.MakeLower();
        BOOL isAgentMode = (cmdLineLower.Find(_T("-agent")) != -1);

        if (isAgentMode && THIS_CFG.GetInt(CFG_CRASH_SECTION, CFG_CRASH_PROTECTED, 0) == 1) {
            // 清除崩溃保护标志
            THIS_CFG.SetInt(CFG_CRASH_SECTION, CFG_CRASH_PROTECTED, 0);
            // 切换到正常模式
            THIS_CFG.SetInt("settings", "RunNormal", 1);
            runNormal = 1;
            Mprintf("[InitInstance] 检测到代理崩溃保护标志，切换到正常运行模式。\n");
            MessageBoxL("检测到代理程序连续崩溃，已自动切换到正常运行模式。\n\n"
                        "如需重新启用服务模式，请在设置中手动切换。",
                        "崩溃保护", MB_ICONWARNING);
        }
    }

    char curFile[MAX_PATH] = { 0 };
    GetModuleFileNameA(NULL, curFile, MAX_PATH);
    if (runNormal != 1 && !IsRunningAsAdmin() && LaunchAsAdmin(curFile, "runas")) {
        Mprintf("[InitInstance] 程序没有管理员权限，用户选择以管理员身份重新运行。\n");
        return FALSE;
    }

    // 首先处理服务命令行参数
    if (runNormal != 1 && HandleServiceCommandLine()) {
        Mprintf("[InitInstance] 服务命令已处理，退出。\n");
        return FALSE;  // 服务命令已处理，退出
    }

    Mprintf("[InitInstance] 主控程序启动运行。\n");
    SetUnhandledExceptionFilter(&whenbuged);

    // 设置 AppUserModelID，避免 Windows 10/11 通知不能显示标题
    // 动态加载以兼容 XP/Vista
    typedef HRESULT(WINAPI* PFN_SetAppUserModelID)(PCWSTR);
    HMODULE hShell32 = GetModuleHandleA("shell32.dll");
    if (hShell32) {
        PFN_SetAppUserModelID pfn = (PFN_SetAppUserModelID)GetProcAddress(hShell32, 
            "SetCurrentProcessExplicitAppUserModelID");
        if (pfn) pfn(L"YAMA");
    }

    // 设置线程区域为中文，确保 MBCS 程序在非中文系统上也能正确显示对话框中的中文
    // 必须在创建任何对话框之前调用
    SetChineseThreadLocale();

    // 加载语言包（必须在显示任何文本之前）
    auto lang = THIS_CFG.GetStr("settings", "Language", "en_US");
    auto langDir = THIS_CFG.GetStr("settings", "LangDir", "./lang");
    langDir = langDir.empty() ? "./lang" : langDir;
    if (PathFileExists(langDir.c_str())) {
        g_Lang.Init(langDir.c_str());
        g_Lang.Load(lang.c_str());
        Mprintf("语言包目录已经指定[%s], 语言数量: %d\n", langDir.c_str(), g_Lang.GetLanguageCount());
    }

    // 创建并显示启动画面
    CSplashDlg* pSplash = new CSplashDlg();
    pSplash->Create(NULL);
    pSplash->UpdateProgressDirect(5, _TR("正在初始化系统图标..."));

    SHFILEINFO	sfi = {};
    HIMAGELIST hImageList = (HIMAGELIST)SHGetFileInfo((LPCTSTR)_T(""), 0, &sfi, sizeof(SHFILEINFO), SHGFI_LARGEICON | SHGFI_SYSICONINDEX);
    m_pImageList_Large.Attach(hImageList);
    hImageList = (HIMAGELIST)SHGetFileInfo((LPCTSTR)_T(""), 0, &sfi, sizeof(SHFILEINFO), SHGFI_SMALLICON | SHGFI_SYSICONINDEX);
    m_pImageList_Small.Attach(hImageList);

    pSplash->UpdateProgressDirect(10, _TR("正在初始化公共控件..."));


    // 如果一个运行在 Windows XP 上的应用程序清单指定要
    // 使用 ComCtl32.dll 版本 6 或更高版本来启用可视化方式，
    //则需要 InitCommonControlsEx()。否则，将无法创建窗口。
    INITCOMMONCONTROLSEX InitCtrls;
    InitCtrls.dwSize = sizeof(InitCtrls);
    // 将它设置为包括所有要在应用程序中使用的
    // 公共控件类。
    InitCtrls.dwICC = ICC_WIN95_CLASSES;
    InitCommonControlsEx(&InitCtrls);

    CWinApp::InitInstance();

    AfxEnableControlContainer();

    // 创建 shell 管理器，以防对话框包含
    // 任何 shell 树视图控件或 shell 列表视图控件。
    CShellManager *pShellManager = new CShellManager;

    // 标准初始化
    // 如果未使用这些功能并希望减小
    // 最终可执行文件的大小，则应移除下列
    // 不需要的特定初始化例程
    // 更改用于存储设置的注册表项
    // TODO: 应适当修改该字符串，
    // 例如修改为公司或组织名
    SetRegistryKey(_T("YAMA"));

    // 注册一个事件，用于进程间通信
    // 请勿修改此事件名称，否则可能导致无法启动程序、鉴权失败等问题
    char eventName[64] = { 0 };
    sprintf(eventName, "YAMA_%d", GetCurrentProcessId());
    HANDLE hEvent = CreateEventA(NULL, TRUE, FALSE, eventName);
    if (hEvent == NULL) {
        Mprintf("[InitInstance] 创建事件失败，错误码: %d\n", GetLastError());
    } else {
        Mprintf("[InitInstance] 创建事件成功，事件名: %s\n", eventName);
    }

    CMy2015RemoteDlg dlg(nullptr);
    m_pMainWnd = &dlg;
    INT_PTR nResponse = dlg.DoModal();
    if (nResponse == IDOK) {
        // TODO: 在此放置处理何时用
        //  “确定”来关闭对话框的代码
    } else if (nResponse == IDCANCEL) {
        // TODO: 在此放置处理何时用
        //  “取消”来关闭对话框的代码
    }

    // 删除上面创建的 shell 管理器。
    if (pShellManager != NULL) {
        delete pShellManager;
    }

    if (hEvent) {
        SAFE_CLOSE_HANDLE(hEvent);
        Mprintf("[InitInstance] 关闭事件句柄。\n");
    }

    // 由于对话框已关闭，所以将返回 FALSE 以便退出应用程序，
    //  而不是启动应用程序的消息泵。
    return FALSE;
}


int CMy2015RemoteApp::ExitInstance()
{
    if (m_Mutex) {
        SAFE_CLOSE_HANDLE(m_Mutex);
        m_Mutex = NULL;
    }
    __try {
        Delete();
    } __except(EXCEPTION_EXECUTE_HANDLER) {
    }

    SAFE_DELETE(m_iniFile);

    Mprintf("[InitInstance] 主控程序退出运行。\n");
    Sleep(500);

    // 只有在代理模式退出时才停止服务
    if (IsAgentMode()) {
        Mprintf("[InitInstance] 主控程序为代理模式，停止服务。\n");
        ServerService_Stop();
    }

    return CWinApp::ExitInstance();
}
