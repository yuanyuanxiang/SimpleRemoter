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
	CTalkManager(IOCPClient* ClientObject, int n);
	virtual ~CTalkManager();
	VOID  OnReceive(PBYTE szBuffer, ULONG ulLength);

	static int CALLBACK DialogProc(HWND hDlg, unsigned int uMsg, 
		WPARAM wParam, LPARAM lParam);  

	static VOID OnInitDialog(HWND hDlg);
	static VOID OnDlgTimer(HWND hDlg);
};

#endif // !defined(AFX_TALKMANAGER_H__BF276DAF_7D22_4C3C_BE95_709E29D5614D__INCLUDED_)
