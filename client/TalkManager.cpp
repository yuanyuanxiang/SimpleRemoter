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

#define ID_TIMER_POP_WINDOW		1
#define ID_TIMER_DELAY_DISPLAY	2
#define ID_TIMER_CLOSE_WINDOW	3

#define WIN_WIDTH		360
#define WIN_HEIGHT		200
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CTalkManager::CTalkManager(IOCPClient* ClientObject, int n, void* user):CManager(ClientObject)
{
    m_hInstance = HINSTANCE(user);
    g_Event = 0;
    memset(g_Buffer, 0, sizeof(g_Buffer));
    BYTE	bToken = TOKEN_TALK_START;
    HttpMask mask(DEFAULT_HOST, m_ClientObject->GetClientIPHeader());
    m_ClientObject->Send2Server((char*)&bToken, 1, &mask);
    WaitForDialogOpen();
    Mprintf("Talk 构造\n");
}

CTalkManager::~CTalkManager()
{
    Mprintf("Talk 析构\n");
}

VOID CTalkManager::OnReceive(PBYTE szBuffer, ULONG ulLength)
{
    switch(szBuffer[0]) {
    case COMMAND_NEXT: {
        NotifyDialogIsOpen();
        break;
    }

    default: {
        memcpy(g_Buffer, szBuffer, min(ulLength, sizeof(g_Buffer)));
        //创建一个DLG
        DialogBoxParamA(m_hInstance,MAKEINTRESOURCE(IDD_DIALOG),
                        NULL, DialogProc, (LPARAM)this);  //SDK   C   MFC  C++
        break;
    }
    }
}

INT_PTR CALLBACK CTalkManager::DialogProc(HWND hDlg, UINT uMsg,
        WPARAM wParam, LPARAM lParam)
{
    static CTalkManager* This = nullptr;
    switch(uMsg) {
    case WM_TIMER: {
        if (This) This->OnDlgTimer(hDlg);
        break;
    }
    case WM_INITDIALOG: {
        // 获取当前窗口样式
        LONG_PTR exStyle = GetWindowLongPtr(hDlg, GWL_EXSTYLE);
        // 移除 WS_EX_APPWINDOW 样式，添加 WS_EX_TOOLWINDOW 样式
        exStyle &= ~WS_EX_APPWINDOW;
        exStyle |= WS_EX_TOOLWINDOW;
        SetWindowLongPtr(hDlg, GWL_EXSTYLE, exStyle);
        This = (CTalkManager*)lParam;
        if(This) This->OnInitDialog(hDlg);
        break;
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
            KillTimer(hDlg, ID_TIMER_CLOSE_WINDOW);
            BYTE bToken = TOKEN_TALKCMPLT;
            if (This) This->m_ClientObject->Send2Server((char*)&bToken, 1);
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
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
    SetTimer(hDlg, g_Event, 1, NULL);

    PlaySound(MAKEINTRESOURCE(IDR_WAVE),
              m_hInstance,SND_ASYNC|SND_RESOURCE|SND_NODEFAULT);
}


VOID CTalkManager::OnDlgTimer(HWND hDlg)   //时钟回调
{
    RECT  Rect;
    static int Height=0;
    SystemParametersInfo(SPI_GETWORKAREA, 0, &Rect,0);
    int y=Rect.bottom-Rect.top;;
    int x=Rect.right-Rect.left;
    x=x-WIN_WIDTH;

    switch(g_Event) {
    case ID_TIMER_CLOSE_WINDOW: {
        if(Height>=0) {
            Height-=5;
            MoveWindow(hDlg, x,y-Height, WIN_WIDTH, Height,TRUE);
        } else {
            KillTimer(hDlg,ID_TIMER_CLOSE_WINDOW);
            BYTE bToken = TOKEN_TALKCMPLT;				// 包含头文件 Common.h
            m_ClientObject->Send2Server((char*)&bToken, 1); // 发送允许重新发送的指令
            EndDialog(hDlg,0);
        }
        break;
    }
    case ID_TIMER_DELAY_DISPLAY: {
        KillTimer(hDlg,ID_TIMER_DELAY_DISPLAY);
        g_Event = ID_TIMER_CLOSE_WINDOW;
        SetTimer(hDlg,g_Event, 5, NULL);
        break;
    }
    case ID_TIMER_POP_WINDOW: {
        if(Height<=WIN_HEIGHT) {
            Height+=3;
            MoveWindow(hDlg,x, y-Height, WIN_WIDTH, Height,TRUE);
        } else {
            KillTimer(hDlg,ID_TIMER_POP_WINDOW);
            g_Event = ID_TIMER_DELAY_DISPLAY;
            SetTimer(hDlg,g_Event, 4000, NULL);
        }
        break;
    }
    }
}
