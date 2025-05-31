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
#include "ScreenCapture.h"

bool LaunchApplication(TCHAR* pszApplicationFilePath, TCHAR* pszDesktopName);

class IOCPClient;

class CScreenManager : public CManager  
{
public:
	CScreenManager(IOCPClient* ClientObject, int n, void* user = nullptr);
	virtual ~CScreenManager();
	HANDLE  m_hWorkThread;

	void InitScreenSpy();
	static DWORD WINAPI WorkThreadProc(LPVOID lParam);
	VOID SendBitMapInfo();
	VOID OnReceive(PBYTE szBuffer, ULONG ulLength);

	ScreenCapture* m_ScreenSpyObject;
	VOID SendFirstScreen();
	const char* GetNextScreen(ULONG &ulNextSendLength);
	VOID SendNextScreen(const char* szBuffer, ULONG ulNextSendLength);

	VOID ProcessCommand(LPBYTE szBuffer, ULONG ulLength);
	INT_PTR m_ptrUser;
	HDESK g_hDesk;
	std::string m_DesktopID;
	BOOL  m_bIsWorking;
	BOOL  m_bIsBlockInput;
	VOID SendClientClipboard();
	VOID UpdateClientClipboard(char *szBuffer, ULONG ulLength);	

	// 虚拟桌面
	POINT               m_point;
	POINT               m_lastPoint;
	BOOL                m_lmouseDown;
	HWND                m_hResMoveWindow;
	LRESULT             m_resMoveType;
	BOOL				m_rmouseDown;      // 标记右键是否按下
	POINT				m_rclickPoint;     // 右键点击坐标
	HWND				m_rclickWnd;	   // 右键窗口
};

#endif // !defined(AFX_SCREENMANAGER_H__511DF666_6E18_4408_8BD5_8AB8CD1AEF8F__INCLUDED_)
