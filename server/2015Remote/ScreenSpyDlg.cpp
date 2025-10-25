// ScreenSpyDlg.cpp : 实现文件
//

#include "stdafx.h"
#include "2015Remote.h"
#include "ScreenSpyDlg.h"
#include "afxdialogex.h"
#include <imm.h>
#include <WinUser.h>
#include "CGridDialog.h"
#include "2015RemoteDlg.h"
#include <file_upload.h>


// CScreenSpyDlg 对话框

enum {
    IDM_CONTROL = 0x1010,
    IDM_FULLSCREEN,
    IDM_SEND_CTRL_ALT_DEL,
    IDM_TRACE_CURSOR,	// 跟踪显示远程鼠标
    IDM_BLOCK_INPUT,	// 锁定远程计算机输入
    IDM_SAVEDIB,		// 保存图片
    IDM_GET_CLIPBOARD,	// 获取剪贴板
    IDM_SET_CLIPBOARD,	// 设置剪贴板
    IDM_ADAPTIVE_SIZE,
};

IMPLEMENT_DYNAMIC(CScreenSpyDlg, CDialog)

#define ALGORITHM_DIFF 1

#ifdef _WIN64
#ifdef _DEBUG
#pragma comment(lib, "FileUpload_Libx64d.lib")
#pragma comment(lib, "PrivateDesktop_Libx64d.lib")
#else
#pragma comment(lib, "FileUpload_Libx64.lib")
#pragma comment(lib, "PrivateDesktop_Libx64.lib")
#endif
#else
int InitFileUpload(const std::string hmac, int chunkSizeKb, int sendDurationMs) { return 0; }
int UninitFileUpload() { return 0; }
std::vector<std::string> GetClipboardFiles() { return{}; }
bool GetCurrentFolderPath(std::string& outDir) { return false; }
int FileBatchTransferWorker(const std::vector<std::string>& files, const std::string& targetDir,
	void* user, OnTransform f, OnFinish finish, const std::string& hash, const std::string& hmac) {
	finish(user);
	return 0;
}
int RecvFileChunk(char* buf, size_t len, void* user, OnFinish f, const std::string& hash, const std::string& hmac) {
	return 0;
}
#endif

extern "C" void* x265_api_get_192()
{
    return nullptr;
}

extern "C" char* __imp_strtok(char* str, const char* delim)
{
    return strtok(str, delim);
}

CScreenSpyDlg::CScreenSpyDlg(CWnd* Parent, Server* IOCPServer, CONTEXT_OBJECT* ContextObject)
    : DialogBase(CScreenSpyDlg::IDD, Parent, IOCPServer, ContextObject, 0)
{
    m_lastMouseMove = 0;
    m_lastMousePoint = {};
    m_pCodec = nullptr;
    m_pCodecContext = nullptr;
    memset(&m_AVPacket, 0, sizeof(AVPacket));
    memset(&m_AVFrame, 0, sizeof(AVFrame));

    //创建解码器.
    bool succeed = false;
    m_pCodec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (m_pCodec) {
        m_pCodecContext = avcodec_alloc_context3(m_pCodec);
        if (m_pCodecContext) {
            succeed = (0 == avcodec_open2(m_pCodecContext, m_pCodec, 0));
        }
    }
    m_FrameID = 0;
    ImmDisableIME(0);// 禁用输入法
    m_bFullScreen = FALSE;

    CHAR szFullPath[MAX_PATH];
    GetSystemDirectory(szFullPath, MAX_PATH);
    lstrcat(szFullPath, "\\shell32.dll");  //图标
    m_hIcon = ExtractIcon(THIS_APP->m_hInstance, szFullPath, 17);

    m_bIsFirst = TRUE;
    m_ulHScrollPos = 0;
    m_ulVScrollPos = 0;

    ULONG	ulBitmapInforLength = m_ContextObject->InDeCompressedBuffer.GetBufferLength() - 1;
    m_BitmapInfor_Full = (BITMAPINFO *) new BYTE[ulBitmapInforLength];
    m_ContextObject->InDeCompressedBuffer.CopyBuffer(m_BitmapInfor_Full, ulBitmapInforLength, 1);

    m_bIsCtrl = FALSE;
    m_bIsTraceCursor = FALSE;
}


VOID CScreenSpyDlg::SendNext(void)
{
    BYTE	bToken = COMMAND_NEXT;
    m_ContextObject->Send2Client(&bToken, 1);
}


CScreenSpyDlg::~CScreenSpyDlg()
{
    if (m_BitmapInfor_Full!=NULL) {
        delete m_BitmapInfor_Full;
        m_BitmapInfor_Full = NULL;
    }

    ::ReleaseDC(m_hWnd, m_hFullDC);   //GetDC
    ::DeleteDC(m_hFullMemDC);                //Create匹配内存DC

    ::DeleteObject(m_BitmapHandle);
    if (m_BitmapData_Full!=NULL) {
        m_BitmapData_Full = NULL;
    }
    if (m_pCodecContext) {
        avcodec_free_context(&m_pCodecContext);
        m_pCodecContext = 0;
    }

    m_pCodec = 0;
    // AVFrame需要清除
    av_frame_unref(&m_AVFrame);
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
    ON_WM_MOUSELEAVE()
    ON_WM_KILLFOCUS()
    ON_WM_SIZE()
    ON_WM_LBUTTONDBLCLK()
    ON_WM_ACTIVATE()
END_MESSAGE_MAP()


// CScreenSpyDlg 消息处理程序

void CScreenSpyDlg::OnLButtonDblClk(UINT nFlags, CPoint point)
{
    if (!m_bIsCtrl) {
        CWnd* parent = GetParent();
        if (parent) {
            // 通知父对话框，传递点击点
            CPoint ptScreen = point;
            ClientToScreen(&ptScreen);
            GetParent()->ScreenToClient(&ptScreen);
            GetParent()->SendMessage(WM_LBUTTONDBLCLK, nFlags, MAKELPARAM(ptScreen.x, ptScreen.y));
        }
    }
    CDialog::OnLButtonDblClk(nFlags, point);
}

BOOL CScreenSpyDlg::OnInitDialog()
{
    CDialog::OnInitDialog();
    SetIcon(m_hIcon,FALSE);

    CString strString;
    strString.Format("%s - 远程桌面控制 %d×%d", m_IPAddress,
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
    if (SysMenu != NULL) {
        SysMenu->AppendMenu(MF_SEPARATOR);
        SysMenu->AppendMenu(MF_STRING, IDM_CONTROL, "控制屏幕(&Y)");
        SysMenu->AppendMenu(MF_STRING, IDM_FULLSCREEN, "全屏(&F)");
        SysMenu->AppendMenu(MF_STRING, IDM_ADAPTIVE_SIZE, "自适应窗口大小(&A)");
        SysMenu->AppendMenu(MF_STRING, IDM_TRACE_CURSOR, "跟踪被控端鼠标(&T)");
        SysMenu->AppendMenu(MF_STRING, IDM_BLOCK_INPUT, "锁定被控端鼠标和键盘(&L)");
        SysMenu->AppendMenu(MF_STRING, IDM_SAVEDIB, "保存快照(&S)");
        SysMenu->AppendMenu(MF_SEPARATOR);
        SysMenu->AppendMenu(MF_STRING, IDM_GET_CLIPBOARD, "获取剪贴板(&R)");
        SysMenu->AppendMenu(MF_STRING, IDM_SET_CLIPBOARD, "设置剪贴板(&L)");
        SysMenu->AppendMenu(MF_SEPARATOR);
    }

    m_bIsCtrl = THIS_CFG.GetInt("settings", "DXGI") == USING_VIRTUAL;
    m_bIsTraceCursor = FALSE;  //不是跟踪
    m_ClientCursorPos.x = 0;
    m_ClientCursorPos.y = 0;
    m_bCursorIndex = 0;

    m_hRemoteCursor = LoadCursor(NULL, IDC_ARROW);
    ICONINFO CursorInfo;
    ::GetIconInfo(m_hRemoteCursor, &CursorInfo);
    SysMenu->CheckMenuItem(IDM_CONTROL, m_bIsCtrl ? MF_CHECKED : MF_UNCHECKED);
    SysMenu->CheckMenuItem(IDM_ADAPTIVE_SIZE, m_bAdaptiveSize ? MF_CHECKED : MF_UNCHECKED);
    SetClassLongPtr(m_hWnd, GCLP_HCURSOR, m_bIsCtrl ? (LONG_PTR)m_hRemoteCursor : (LONG_PTR)LoadCursor(NULL, IDC_NO));

    GetClientRect(&m_CRect);
    m_wZoom = ((double)m_BitmapInfor_Full->bmiHeader.biWidth) / ((double)(m_CRect.Width()));
    m_hZoom = ((double)m_BitmapInfor_Full->bmiHeader.biHeight) / ((double)(m_CRect.Height()));
    ShowScrollBar(SB_BOTH, !m_bAdaptiveSize);

    SendNext();

    return TRUE;
}


VOID CScreenSpyDlg::OnClose()
{
    CancelIO();
    // 恢复鼠标状态
    SetClassLongPtr(m_hWnd, GCLP_HCURSOR, (LONG_PTR)LoadCursor(NULL, IDC_ARROW));
    // 通知父窗口
    CWnd* parent = GetParent();
    if (parent)
        parent->SendMessage(WM_CHILD_CLOSED, (WPARAM)this, 0);
    extern CMy2015RemoteDlg *g_2015RemoteDlg;
    if(g_2015RemoteDlg)
        g_2015RemoteDlg->RemoveRemoteWindow(GetSafeHwnd());

    // 等待数据处理完毕
    if (IsProcessing()) {
        ShowWindow(SW_HIDE);
        return;
    }

    DialogBase::OnClose();
}

VOID CScreenSpyDlg::OnReceiveComplete()
{
    assert (m_ContextObject);
    auto cmd = m_ContextObject->InDeCompressedBuffer.GetBYTE(0);
	LPBYTE szBuffer = m_ContextObject->InDeCompressedBuffer.GetBuffer();
	unsigned len = m_ContextObject->InDeCompressedBuffer.GetBufferLen();
    switch(cmd) {
	case COMMAND_GET_FOLDER: {
		std::string folder;
		if (GetCurrentFolderPath(folder)) {
            // 发送目录并准备接收文件
			BYTE cmd[300] = { COMMAND_GET_FILE };
			memcpy(cmd + 1, folder.c_str(), folder.length());
			m_ContextObject->Send2Client(cmd, sizeof(cmd));
		}
		break;
	}
    case TOKEN_FIRSTSCREEN: {
        DrawFirstScreen();
        break;
    }
    case TOKEN_NEXTSCREEN: {
        DrawNextScreenDiff(false);
        break;
    }
    case TOKEN_KEYFRAME: {
        if (!m_bIsFirst) {
            DrawNextScreenDiff(true);
        }
        break;
    }
    case TOKEN_CLIPBOARD_TEXT: {
        Buffer str = m_ContextObject->InDeCompressedBuffer.GetMyBuffer(1);
        UpdateServerClipboard(str.c_str(), str.length());
        break;
    }
    default: {
        TRACE("CScreenSpyDlg unknown command: %d!!!\n", int(cmd));
    }
    }
}

VOID CScreenSpyDlg::DrawFirstScreen(void)
{
    m_bIsFirst = FALSE;

    //得到被控端发来的数据 ，将他拷贝到HBITMAP的缓冲区中，这样一个图像就出现了

    m_ContextObject->InDeCompressedBuffer.CopyBuffer(m_BitmapData_Full, m_BitmapInfor_Full->bmiHeader.biSizeImage, 1);
    PostMessage(WM_PAINT);//触发WM_PAINT消息
}

VOID CScreenSpyDlg::DrawNextScreenDiff(bool keyFrame)
{
    //该函数不是直接画到屏幕上，而是更新一下变化部分的屏幕数据然后调用
    //OnPaint画上去
    //根据鼠标是否移动和屏幕是否变化判断是否重绘鼠标，防止鼠标闪烁
    BOOL	bChange = FALSE;
    ULONG	ulHeadLength = 1 + 1 + sizeof(POINT) + sizeof(BYTE); // 标识 + 算法 + 光标 位置 + 光标类型索引
#if SCREENYSPY_IMPROVE
    int frameID = -1;
    memcpy(&frameID, m_ContextObject->InDeCompressedBuffer.GetBuffer(ulHeadLength), sizeof(int));
    ulHeadLength += sizeof(int);
    if (++m_FrameID != frameID) {
        TRACE("DrawNextScreenDiff [%d] bmp is lost from %d\n", frameID-m_FrameID, m_FrameID);
        m_FrameID = frameID;
    }
#else
    m_FrameID++;
#endif
    LPVOID	FirstScreenData = m_BitmapData_Full;
    LPVOID	NextScreenData = m_ContextObject->InDeCompressedBuffer.GetBuffer(ulHeadLength);
    ULONG	NextScreenLength = m_ContextObject->InDeCompressedBuffer.GetBufferLength() - ulHeadLength;

    POINT	OldClientCursorPos;
    memcpy(&OldClientCursorPos, &m_ClientCursorPos, sizeof(POINT));
    memcpy(&m_ClientCursorPos, m_ContextObject->InDeCompressedBuffer.GetBuffer(2), sizeof(POINT));

    // 鼠标移动了
    if (memcmp(&OldClientCursorPos, &m_ClientCursorPos, sizeof(POINT)) != 0) {
        bChange = TRUE;
    }

    // 光标类型发生变化
    BYTE bOldCursorIndex = m_bCursorIndex;
    m_bCursorIndex = m_ContextObject->InDeCompressedBuffer.GetBuffer(2+sizeof(POINT))[0];
    if (bOldCursorIndex != m_bCursorIndex) {
        bChange = TRUE;
        if (m_bIsCtrl && !m_bIsTraceCursor)//替换指定窗口所属类的WNDCLASSEX结构
#ifdef _WIN64
            SetClassLongPtrA(m_hWnd, GCLP_HCURSOR, (ULONG_PTR)m_CursorInfo.getCursorHandle(m_bCursorIndex == (BYTE)-1 ? 1 : m_bCursorIndex));
#else
            SetClassLongA(m_hWnd, GCL_HCURSOR, (LONG)m_CursorInfo.getCursorHandle(m_bCursorIndex == (BYTE)-1 ? 1 : m_bCursorIndex));
#endif
    }

    // 屏幕是否变化
    if (NextScreenLength > 0) {
        bChange = TRUE;
    }

    BYTE algorithm = m_ContextObject->InDeCompressedBuffer.GetBYTE(1);
    LPBYTE dst = (LPBYTE)FirstScreenData, p = (LPBYTE)NextScreenData;
    if (keyFrame) {
        switch (algorithm) {
        case ALGORITHM_DIFF:
        case ALGORITHM_GRAY: {
            if (m_BitmapInfor_Full->bmiHeader.biSizeImage == NextScreenLength)
                memcpy(dst, p, m_BitmapInfor_Full->bmiHeader.biSizeImage);
            break;
        }
        case ALGORITHM_H264: {
            break;
        }
        default:
            break;
        }
    } else if (0 != NextScreenLength) {
        switch (algorithm) {
        case ALGORITHM_DIFF: {
            for (LPBYTE end = p + NextScreenLength; p < end; ) {
                ULONG ulCount = *(LPDWORD(p + sizeof(ULONG)));
                memcpy(dst + *(LPDWORD)p, p + 2 * sizeof(ULONG), ulCount);

                p += 2 * sizeof(ULONG) + ulCount;
            }
            break;
        }
        case ALGORITHM_GRAY: {
            for (LPBYTE end = p + NextScreenLength; p < end; ) {
                ULONG ulCount = *(LPDWORD(p + sizeof(ULONG)));
                LPBYTE p1 = dst + *(LPDWORD)p, p2 = p + 2 * sizeof(ULONG);
                for (int i = 0; i < ulCount; ++i, p1 += 4)
                    memset(p1, *p2++, sizeof(DWORD));

                p += 2 * sizeof(ULONG) + ulCount;
            }
            break;
        }
        case ALGORITHM_H264: {
            if (Decode((LPBYTE)NextScreenData, NextScreenLength)) {
                bChange = TRUE;
            }
            break;
        }
        default:
            break;
        }
    }

#if SCREENSPY_WRITE
    if (!WriteBitmap(m_BitmapInfor_Full, m_BitmapData_Full, "YAMA", frameID)) {
        TRACE("WriteBitmap [%d] failed!!!\n", frameID);
    }
#endif

    if (bChange) {
        PostMessage(WM_PAINT);
    }
}


bool CScreenSpyDlg::Decode(LPBYTE Buffer, int size)
{
    // 解码数据.
    av_init_packet(&m_AVPacket);

    m_AVPacket.data = (uint8_t*)Buffer;
    m_AVPacket.size = size;

    int err = avcodec_send_packet(m_pCodecContext, &m_AVPacket);
    if (!err) {
        err = avcodec_receive_frame(m_pCodecContext, &m_AVFrame);
        if (err == AVERROR(EAGAIN)) {
            Mprintf("avcodec_receive_frame error: EAGAIN\n");
            return false;
        }

        // 解码数据前会清除m_AVFrame的内容.
        if (!err) {
            LPVOID Image[2] = { 0 };
            LPVOID CursorInfo[2] = { 0 };
            //成功.
            //I420 ---> ARGB.
            //WaitForSingleObject(m_hMutex,INFINITE);

            libyuv::I420ToARGB(
                m_AVFrame.data[0], m_AVFrame.linesize[0],
                m_AVFrame.data[1], m_AVFrame.linesize[1],
                m_AVFrame.data[2], m_AVFrame.linesize[2],
                (uint8_t*)m_BitmapData_Full,
                m_BitmapInfor_Full->bmiHeader.biWidth*4,
                m_BitmapInfor_Full->bmiHeader.biWidth,
                m_BitmapInfor_Full->bmiHeader.biHeight);
            return true;
        }
        Mprintf("avcodec_receive_frame failed with error: %d\n", err);
    } else {
        Mprintf("avcodec_send_packet failed with error: %d\n", err);
    }
    return false;
}

void CScreenSpyDlg::OnPaint()
{
    CPaintDC dc(this); // device context for painting

    if (m_bIsFirst) {
        DrawTipString("请等待......");
        return;
    }

    m_bAdaptiveSize ?
    StretchBlt(m_hFullDC, 0, 0, m_CRect.Width(), m_CRect.Height(), m_hFullMemDC, 0, 0, m_BitmapInfor_Full->bmiHeader.biWidth, m_BitmapInfor_Full->bmiHeader.biHeight, SRCCOPY) :
    BitBlt(m_hFullDC, 0, 0, m_BitmapInfor_Full->bmiHeader.biWidth, m_BitmapInfor_Full->bmiHeader.biHeight, m_hFullMemDC, m_ulHScrollPos, m_ulVScrollPos, SRCCOPY);

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

    switch (nID) {
    case IDM_CONTROL: {
        m_bIsCtrl = !m_bIsCtrl;
        SysMenu->CheckMenuItem(IDM_CONTROL, m_bIsCtrl ? MF_CHECKED : MF_UNCHECKED);
        SetClassLongPtr(m_hWnd, GCLP_HCURSOR, m_bIsCtrl ? (LONG_PTR)m_hRemoteCursor : (LONG_PTR)LoadCursor(NULL, IDC_NO));
        break;
    }
    case IDM_FULLSCREEN: { // 全屏
        EnterFullScreen();
        SysMenu->CheckMenuItem(IDM_FULLSCREEN, MF_CHECKED); //菜单样式
        break;
    }
    case IDM_SAVEDIB: {  // 快照保存
        SaveSnapshot();
        break;
    }
    case IDM_TRACE_CURSOR: { // 跟踪被控端鼠标
        m_bIsTraceCursor = !m_bIsTraceCursor; //这里在改变数据
        SysMenu->CheckMenuItem(IDM_TRACE_CURSOR, m_bIsTraceCursor ? MF_CHECKED : MF_UNCHECKED);//在菜单打钩不打钩
        m_bAdaptiveSize = !m_bIsTraceCursor;
        ShowScrollBar(SB_BOTH, !m_bAdaptiveSize);
        // 重绘消除或显示鼠标
        OnPaint();
        break;
    }
    case IDM_BLOCK_INPUT: { // 锁定服务端鼠标和键盘
        BOOL bIsChecked = SysMenu->GetMenuState(IDM_BLOCK_INPUT, MF_BYCOMMAND) & MF_CHECKED;
        SysMenu->CheckMenuItem(IDM_BLOCK_INPUT, bIsChecked ? MF_UNCHECKED : MF_CHECKED);

        BYTE	bToken[2];
        bToken[0] = COMMAND_SCREEN_BLOCK_INPUT;
        bToken[1] = !bIsChecked;
        m_ContextObject->Send2Client(bToken, sizeof(bToken));
        break;
    }
    case IDM_GET_CLIPBOARD: { //想要Client的剪贴板内容
        BYTE	bToken = COMMAND_SCREEN_GET_CLIPBOARD;
        m_ContextObject->Send2Client(&bToken, sizeof(bToken));
        break;
    }
    case IDM_SET_CLIPBOARD: { //给他
        SendServerClipboard();
        break;
    }
    case IDM_ADAPTIVE_SIZE: {
        m_bAdaptiveSize = !m_bAdaptiveSize;
        ShowScrollBar(SB_BOTH, !m_bAdaptiveSize);
        SysMenu->CheckMenuItem(IDM_ADAPTIVE_SIZE, m_bAdaptiveSize ? MF_CHECKED : MF_UNCHECKED);
        break;
    }
    }

    CDialog::OnSysCommand(nID, lParam);
}


BOOL CScreenSpyDlg::PreTranslateMessage(MSG* pMsg)
{
#define MAKEDWORD(h,l)        (((unsigned long)h << 16) | l)
    switch (pMsg->message) {
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
    case WM_LBUTTONDBLCLK:
    case WM_RBUTTONDBLCLK:
    case WM_MBUTTONDBLCLK:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
    case WM_MOUSEMOVE:
    case WM_MOUSEWHEEL: {
        SendScaledMouseMessage(pMsg, true);
    }
    break;
    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
        if (pMsg->wParam == VK_F11 && LeaveFullScreen()) // F11: 退出全屏
            return TRUE;
        if (pMsg->wParam != VK_LWIN && pMsg->wParam != VK_RWIN) {
            SendScaledMouseMessage(pMsg, true);
        }
        if (pMsg->wParam == VK_RETURN || pMsg->wParam == VK_ESCAPE)
            return TRUE;// 屏蔽Enter和ESC关闭对话
        break;
    }

    return CDialog::PreTranslateMessage(pMsg);
}


void CScreenSpyDlg::SendScaledMouseMessage(MSG* pMsg, bool makeLP)
{
    if (!m_bIsCtrl)
        return;

    MYMSG msg(*pMsg);
    LONG x = LOWORD(pMsg->lParam), y = HIWORD(pMsg->lParam);
    LONG low = m_bAdaptiveSize ? x * m_wZoom : x + m_ulHScrollPos;
    LONG high = m_bAdaptiveSize ? y * m_hZoom : y + m_ulVScrollPos;
    if (makeLP) msg.lParam = MAKELPARAM(low, high);
    msg.pt.x = low;
    msg.pt.y = high;
    SendCommand(&msg);
}

VOID CScreenSpyDlg::SendCommand(const MYMSG* Msg)
{
    if (!m_bIsCtrl)
        return;

    if (Msg->message == WM_MOUSEMOVE) {
        auto now = clock();
        auto time_elapsed = now - m_lastMouseMove;
        int dx = abs(Msg->pt.x - m_lastMousePoint.x);
        int dy = abs(Msg->pt.y - m_lastMousePoint.y);
        int dist_sq = dx * dx + dy * dy;
        if (time_elapsed < 200 && dist_sq < 18 * 18) {
            return;
        }
        m_lastMouseMove = now;
        m_lastMousePoint = Msg->pt;
    }

    const int length = sizeof(MSG64) + 1;
    BYTE szData[length + 3];
    szData[0] = COMMAND_SCREEN_CONTROL;
    memcpy(szData + 1, Msg, sizeof(MSG64));
    szData[length] = 0;
    m_ContextObject->Send2Client(szData, length);
}

BOOL CScreenSpyDlg::SaveSnapshot(void)
{
    CString	strFileName = m_IPAddress + CTime::GetCurrentTime().Format("_%Y-%m-%d_%H-%M-%S.bmp");
    CFileDialog Dlg(FALSE, "bmp", strFileName, OFN_OVERWRITEPROMPT, "位图文件(*.bmp)|*.bmp|", this);
    if(Dlg.DoModal () != IDOK)
        return FALSE;

    WriteBitmap(m_BitmapInfor_Full, m_BitmapData_Full, Dlg.GetPathName().GetBuffer());

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
        if(NULL==SetClipboardData(CF_TEXT, hGlobal))
            GlobalFree(hGlobal);
    }
    CloseClipboard();
}

VOID CScreenSpyDlg::SendServerClipboard(void)
{
    if (!::OpenClipboard(NULL))  //打开剪切板设备
        return;
    HGLOBAL hGlobal = GetClipboardData(CF_TEXT);   //代表着一个内存
    if (hGlobal == NULL) {
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
    m_ContextObject->Send2Client((PBYTE)szBuffer, iPacketLength);
    delete[] szBuffer;
}


void CScreenSpyDlg::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
    if (m_bAdaptiveSize) return;

    SCROLLINFO si = {sizeof(si)};
    si.fMask = SIF_ALL;
    GetScrollInfo(SB_HORZ, &si);

    int nPrevPos = si.nPos;
    switch(nSBCode) {
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
    if (si.nPos != nPrevPos) {
        m_ulHScrollPos += si.nPos - nPrevPos;
        ScrollWindow(nPrevPos - si.nPos, 0, NULL, NULL);
    }

    CDialog::OnHScroll(nSBCode, nPrevPos, pScrollBar);
}


void CScreenSpyDlg::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
    if (m_bAdaptiveSize) return;

    SCROLLINFO si = {sizeof(si)};
    si.fMask = SIF_ALL;
    GetScrollInfo(SB_VERT, &si);

    int nPrevPos = si.nPos;
    switch(nSBCode) {
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
    if (si.nPos != nPrevPos) {
        m_ulVScrollPos += si.nPos - nPrevPos;
        ScrollWindow(0, nPrevPos - si.nPos, NULL, NULL);
    }

    CDialog::OnVScroll(nSBCode, nPrevPos, pScrollBar);
}


void CScreenSpyDlg::EnterFullScreen()
{
    if (!m_bFullScreen) {
        // 1. 获取鼠标位置
        POINT pt;
        GetCursorPos(&pt);

        // 2. 获取鼠标所在显示器
        HMONITOR hMonitor = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi = { sizeof(mi) };
        if (!GetMonitorInfo(hMonitor, &mi))
            return;

        RECT rcMonitor = mi.rcMonitor;

        // 3. 记录当前窗口状态
        GetWindowPlacement(&m_struOldWndpl);

        // 4. 修改窗口样式，移除标题栏、边框
        LONG lStyle = GetWindowLong(m_hWnd, GWL_STYLE);
        lStyle &= ~(WS_CAPTION | WS_THICKFRAME | WS_BORDER);
        SetWindowLong(m_hWnd, GWL_STYLE, lStyle);

        // 5. 隐藏滚动条
        ShowScrollBar(SB_BOTH, !m_bAdaptiveSize);  // 隐藏水平和垂直滚动条

        // 6. 重新调整窗口大小并更新
        SetWindowPos(&CWnd::wndTop, rcMonitor.left, rcMonitor.top, rcMonitor.right - rcMonitor.left,
                     rcMonitor.bottom - rcMonitor.top, SWP_NOZORDER | SWP_FRAMECHANGED);

        // 7. 标记全屏模式
        m_bFullScreen = true;
    }
}

// 全屏退出成功则返回true
bool CScreenSpyDlg::LeaveFullScreen()
{
    if (m_bFullScreen) {
        // 1. 恢复窗口样式
        LONG lStyle = GetWindowLong(m_hWnd, GWL_STYLE);
        lStyle |= (WS_CAPTION | WS_THICKFRAME | WS_BORDER);
        SetWindowLong(m_hWnd, GWL_STYLE, lStyle);

        // 2. 恢复窗口大小
        SetWindowPlacement(&m_struOldWndpl);
        SetWindowPos(&CWnd::wndTop, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED);

        // 3. 显示滚动条
        ShowScrollBar(SB_BOTH, !m_bAdaptiveSize);  // 显示水平和垂直滚动条

        // 4. 标记退出全屏
        m_bFullScreen = false;

        CMenu* SysMenu = GetSystemMenu(FALSE);
        SysMenu->CheckMenuItem(IDM_FULLSCREEN, MF_UNCHECKED); //菜单样式

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
    if (!m_bMouseTracking) {
        m_bMouseTracking = true;
        SetClassLongPtr(m_hWnd, GCLP_HCURSOR, m_bIsCtrl ? (LONG_PTR)m_hRemoteCursor : (LONG_PTR)LoadCursor(NULL, IDC_NO));
    }

    CDialog::OnMouseMove(nFlags, point);
}

void CScreenSpyDlg::OnMouseLeave()
{
    CWnd::OnMouseLeave();

    m_bMouseTracking = false;
    SetClassLongPtr(m_hWnd, GCLP_HCURSOR, m_bIsCtrl ? (LONG_PTR)m_hRemoteCursor : (LONG_PTR)LoadCursor(NULL, IDC_NO));
}

void CScreenSpyDlg::OnKillFocus(CWnd* pNewWnd)
{
    CDialog::OnKillFocus(pNewWnd);
}


void CScreenSpyDlg::OnSize(UINT nType, int cx, int cy)
{
    CDialog::OnSize(nType, cx, cy);

    // TODO: Add your message handler code here
    if (!IsWindowVisible())
        return;

    GetClientRect(&m_CRect);
    m_wZoom = ((double)m_BitmapInfor_Full->bmiHeader.biWidth) / ((double)(m_CRect.Width()));
    m_hZoom = ((double)m_BitmapInfor_Full->bmiHeader.biHeight) / ((double)(m_CRect.Height()));
}

void CScreenSpyDlg::OnActivate(UINT nState, CWnd* pWndOther, BOOL bMinimized)
{
	CDialogBase::OnActivate(nState, pWndOther, bMinimized);

	CWnd* pMain = AfxGetMainWnd();
	if (!pMain)
		return;

	if (nState != WA_INACTIVE){
		// 通知主窗口：远程窗口获得焦点
		::PostMessage(pMain->GetSafeHwnd(), WM_SESSION_ACTIVATED, (WPARAM)this, 0);
	}
}
