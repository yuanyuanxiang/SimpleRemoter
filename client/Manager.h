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
	State&g_bExit; // 1-���ض��˳� 2-���ض��˳�
	BOOL m_bReady;
	CManager(IOCPClient* ClientObject);
	virtual ~CManager();

	virtual VOID OnReceive(PBYTE szBuffer, ULONG ulLength){}
	IOCPClient* m_ClientObject;
	HANDLE	    m_hEventDlgOpen;
	VOID WaitForDialogOpen();
	VOID NotifyDialogIsOpen();

	BOOL IsConnected() const {
		return m_ClientObject->IsConnected();
	}
	BOOL Reconnect() {
		return m_ClientObject ? m_ClientObject->Reconnect(this) : FALSE;
	}
	virtual void Notify() { }
	int Send(LPBYTE lpData, UINT nSize);
	virtual void SetReady(BOOL ready = true) { m_bReady = ready; }
};

#endif // !defined(AFX_MANAGER_H__32F1A4B3_8EA6_40C5_B1DF_E469F03FEC30__INCLUDED_)
