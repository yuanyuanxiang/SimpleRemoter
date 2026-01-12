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
#include <md5.h>


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
    IDM_SAVEAVI,
    IDM_SAVEAVI_H264,
    IDM_SWITCHSCREEN,
    IDM_MULTITHREAD_COMPRESS,
    IDM_FPS_10,
    IDM_FPS_15,
    IDM_FPS_20,
    IDM_FPS_25,
    IDM_FPS_30,
    IDM_FPS_UNLIMITED,
    IDM_ORIGINAL_SIZE,
    IDM_SCREEN_1080P,
};

IMPLEMENT_DYNAMIC(CScreenSpyDlg, CDialog)

#define ALGORITHM_DIFF 1
#define TIMER_ID 132

#ifdef _WIN64
#ifdef _DEBUG
#pragma comment(lib, "FileUpload_Libx64d.lib")
#pragma comment(lib, "PrivateDesktop_Libx64d.lib")
#else
#pragma comment(lib, "FileUpload_Libx64.lib")
#pragma comment(lib, "PrivateDesktop_Libx64.lib")
#endif
#else
#ifdef _DEBUG
#pragma comment(lib, "FileUpload_Libd.lib")
#pragma comment(lib, "PrivateDesktop_Libd.lib")
#else
#pragma comment(lib, "FileUpload_Lib.lib")
#pragma comment(lib, "PrivateDesktop_Lib.lib")
#endif
#endif

extern "C" void* x265_api_get_192()
{
    return nullptr;
}

extern "C" char* __imp_strtok(char* str, const char* delim)
{
    return strtok(str, delim);
}

CScreenSpyDlg::CScreenSpyDlg(CMy2015RemoteDlg* Parent, Server* IOCPServer, CONTEXT_OBJECT* ContextObject)
    : DialogBase(CScreenSpyDlg::IDD, Parent, IOCPServer, ContextObject, 0)
{
    m_pParent = Parent;
    m_hFullDC = NULL;
    m_hFullMemDC = NULL;
    m_BitmapHandle = NULL;

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

    CHAR szFullPath[MAX_PATH];
    GetSystemDirectory(szFullPath, MAX_PATH);
    lstrcat(szFullPath, "\\shell32.dll");  //图标
    m_hIcon = ExtractIcon(THIS_APP->m_hInstance, szFullPath, 17);

    m_bIsFirst = TRUE;
    m_ulHScrollPos = 0;
    m_ulVScrollPos = 0;

    const ULONG	ulBitmapInforLength = sizeof(BITMAPINFOHEADER);
    m_BitmapInfor_Full = (BITMAPINFO *) new BYTE[ulBitmapInforLength];
    m_ContextObject->InDeCompressedBuffer.CopyBuffer(m_BitmapInfor_Full, ulBitmapInforLength, 1);
    m_ContextObject->InDeCompressedBuffer.CopyBuffer(&m_Settings, sizeof(ScreenSettings), 57);

    m_bIsCtrl = FALSE;
    m_bIsTraceCursor = FALSE;
}


VOID CScreenSpyDlg::SendNext(void)
{
    BYTE	bToken[32] = { COMMAND_NEXT };
    uint64_t dlg = (uint64_t)this;
    memcpy(bToken+1, &dlg, sizeof(uint64_t));
    m_ContextObject->Send2Client(bToken, sizeof(bToken));
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

    SAFE_DELETE(m_pToolbar);
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
    ON_WM_TIMER()
    ON_COMMAND(ID_EXIT_FULLSCREEN, &CScreenSpyDlg::OnExitFullscreen)
    ON_MESSAGE(WM_DISCONNECT, &CScreenSpyDlg::OnDisconnect)
    ON_WM_DROPFILES()
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

void CScreenSpyDlg::PrepareDrawing(const LPBITMAPINFO bmp)
{
    if (m_hFullDC) ::ReleaseDC(m_hWnd, m_hFullDC);
    if (m_hFullMemDC) ::DeleteDC(m_hFullMemDC);
    if (m_BitmapHandle) ::DeleteObject(m_BitmapHandle);
    m_BitmapData_Full = NULL;

    CString strString;
    strString.Format("%s - 远程桌面控制 %d×%d", m_IPAddress, bmp->bmiHeader.biWidth, bmp->bmiHeader.biHeight);
    SetWindowText(strString);
    uint64_t dlg = (uint64_t)this;
    Mprintf("%s [对话框ID: %lld]\n", strString.GetString(), dlg);

    m_hFullDC = ::GetDC(m_hWnd);
    SetStretchBltMode(m_hFullDC, HALFTONE);
    SetBrushOrgEx(m_hFullDC, 0, 0, NULL);
    m_hFullMemDC = CreateCompatibleDC(m_hFullDC);
    m_BitmapHandle = CreateDIBSection(m_hFullDC, bmp, DIB_RGB_COLORS, &m_BitmapData_Full, NULL, NULL);

    SelectObject(m_hFullMemDC, m_BitmapHandle);

    SetScrollRange(SB_HORZ, 0, bmp->bmiHeader.biWidth);
    SetScrollRange(SB_VERT, 0, bmp->bmiHeader.biHeight);

    GetClientRect(&m_CRect);
    m_wZoom = ((double)bmp->bmiHeader.biWidth) / ((double)(m_CRect.Width()));
    m_hZoom = ((double)bmp->bmiHeader.biHeight) / ((double)(m_CRect.Height()));
}

BOOL CScreenSpyDlg::OnInitDialog()
{
    CDialog::OnInitDialog();
    SetIcon(m_hIcon,FALSE);
    DragAcceptFiles(TRUE);
    ChangeWindowMessageFilter(WM_DROPFILES, MSGFLT_ADD);
    ChangeWindowMessageFilter(0x0049, MSGFLT_ADD);

    PrepareDrawing(m_BitmapInfor_Full);

    CMenu* SysMenu = GetSystemMenu(FALSE);
    if (SysMenu != NULL) {
        SysMenu->AppendMenu(MF_SEPARATOR);
        SysMenu->AppendMenu(MF_STRING, IDM_CONTROL, "控制屏幕(&Y)");
        SysMenu->AppendMenu(MF_STRING, IDM_FULLSCREEN, "全屏(&F)");
        SysMenu->AppendMenu(MF_STRING, IDM_ADAPTIVE_SIZE, "自适应窗口大小(&A)");
        SysMenu->AppendMenu(MF_STRING, IDM_TRACE_CURSOR, "跟踪被控端鼠标(&T)");
        SysMenu->AppendMenu(MF_STRING, IDM_BLOCK_INPUT, "锁定被控端鼠标和键盘(&L)");
        SysMenu->AppendMenu(MF_SEPARATOR);
        SysMenu->AppendMenu(MF_STRING, IDM_SAVEDIB, "保存快照(&S)");
        SysMenu->AppendMenu(MF_STRING, IDM_SAVEAVI, _T("录像(MJPEG)"));
        SysMenu->AppendMenu(MF_STRING, IDM_SAVEAVI_H264, _T("录像(H264)"));
        SysMenu->AppendMenu(MF_STRING, IDM_GET_CLIPBOARD, "获取剪贴板(&R)");
        SysMenu->AppendMenu(MF_STRING, IDM_SET_CLIPBOARD, "设置剪贴板(&L)");
        SysMenu->AppendMenu(MF_SEPARATOR);
        SysMenu->AppendMenu(MF_STRING, IDM_SWITCHSCREEN, "切换显示器(&1)");
        SysMenu->AppendMenu(MF_STRING, IDM_MULTITHREAD_COMPRESS, "多线程压缩(&2)");
        SysMenu->AppendMenu(MF_STRING, IDM_ORIGINAL_SIZE, "原始分辨率(&3)");
        SysMenu->AppendMenu(MF_STRING, IDM_SCREEN_1080P, "限制为1080P(&4)");
        SysMenu->AppendMenu(MF_SEPARATOR);

        CMenu fpsMenu;
        if (fpsMenu.CreatePopupMenu()) {
            fpsMenu.AppendMenu(MF_STRING, IDM_FPS_10, "最大帧率FPS:10");
            fpsMenu.AppendMenu(MF_STRING, IDM_FPS_15, "最大帧率FPS:15");
            fpsMenu.AppendMenu(MF_STRING, IDM_FPS_20, "最大帧率FPS:20");
            fpsMenu.AppendMenu(MF_STRING, IDM_FPS_25, "最大帧率FPS:25");
            fpsMenu.AppendMenu(MF_STRING, IDM_FPS_30, "最大帧率FPS:30");
            fpsMenu.AppendMenu(MF_STRING, IDM_FPS_UNLIMITED, "最大帧率无限制");
            SysMenu->AppendMenuA(MF_STRING | MF_POPUP, (UINT_PTR)fpsMenu.Detach(), _T("帧率设置"));
        }

        BOOL all = THIS_CFG.GetInt("settings", "MultiScreen");
        SysMenu->EnableMenuItem(IDM_SWITCHSCREEN, all ? MF_ENABLED : MF_GRAYED);
        SysMenu->CheckMenuItem(IDM_MULTITHREAD_COMPRESS, m_Settings.CompressThread ? MF_CHECKED : MF_UNCHECKED);
        if (m_Settings.ScreenStrategy == 0) {
            SysMenu->CheckMenuItem(IDM_SCREEN_1080P, MF_CHECKED);
            SysMenu->CheckMenuItem(IDM_ORIGINAL_SIZE, MF_UNCHECKED);
        }
        else if (m_Settings.ScreenStrategy == 1) {
            SysMenu->CheckMenuItem(IDM_SCREEN_1080P, MF_UNCHECKED);
            SysMenu->CheckMenuItem(IDM_ORIGINAL_SIZE, MF_CHECKED);
        }
		int fpsIndex = IDM_FPS_10 + (m_Settings.MaxFPS - 10)/5;
        for (int i = IDM_FPS_10; i <= IDM_FPS_UNLIMITED; i++) {
            SysMenu->CheckMenuItem(i, MF_UNCHECKED);
		}
		SysMenu->CheckMenuItem(fpsIndex, MF_CHECKED);
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
    ShowScrollBar(SB_BOTH, !m_bAdaptiveSize);

    // 设置合理的"正常"窗口大小（屏幕的 80%），否则还原时窗口极小
    int cxScreen = GetSystemMetrics(SM_CXSCREEN);
    int cyScreen = GetSystemMetrics(SM_CYSCREEN);
    int normalWidth = cxScreen * 80 / 100;
    int normalHeight = cyScreen * 80 / 100;
    int normalX = (cxScreen - normalWidth) / 2;
    int normalY = (cyScreen - normalHeight) / 2;

    // 使用 WINDOWPLACEMENT 确保 rcNormalPosition 被正确设置
    WINDOWPLACEMENT wp = { sizeof(WINDOWPLACEMENT) };
    GetWindowPlacement(&wp);
    wp.rcNormalPosition.left = normalX;
    wp.rcNormalPosition.top = normalY;
    wp.rcNormalPosition.right = normalX + normalWidth;
    wp.rcNormalPosition.bottom = normalY + normalHeight;
    wp.showCmd = SW_MAXIMIZE;
    SetWindowPlacement(&wp);

    // 同时初始化 m_struOldWndpl，供全屏退出时使用
    m_struOldWndpl = wp;
    m_Settings.FullScreen ? EnterFullScreen() : LeaveFullScreen();
    SendNext();

    return TRUE;
}


VOID CScreenSpyDlg::OnClose()
{
	KillTimer(1);
	KillTimer(2);
    if (!m_aviFile.IsEmpty()) {
        KillTimer(TIMER_ID);
        m_aviFile = "";
        m_aviStream.Close();
    }
    if (ShouldReconnect() && SayByeBye()) Sleep(500);
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
        m_bHide = true;
        ShowWindow(SW_HIDE);
        return;
    }

    DialogBase::OnClose();
}

afx_msg LRESULT CScreenSpyDlg::OnDisconnect(WPARAM wParam, LPARAM lParam)
{
    m_bConnected = FALSE;
	m_nDisconnectTime = GetTickCount64();
	// Close the dialog if reconnect not succeed in 15 seconds
    SetTimer(2, 15000, NULL);
    PostMessage(WM_PAINT);
    return S_OK;
}

VOID CScreenSpyDlg::OnReceiveComplete()
{
    if (m_bIsClosed) return;
    assert (m_ContextObject);
    auto cmd = m_ContextObject->InDeCompressedBuffer.GetBYTE(0);
    LPBYTE szBuffer = m_ContextObject->InDeCompressedBuffer.GetBuffer();
    unsigned len = m_ContextObject->InDeCompressedBuffer.GetBufferLen();
    switch(cmd) {
    case COMMAND_GET_FOLDER: {
        std::string folder;
        if (GetCurrentFolderPath(folder)) {
            // 发送目录并准备接收文件
            std::string files(szBuffer + 1, szBuffer + len);
            int len = 1 + folder.length() + files.length() + 1;
            BYTE* cmd = new BYTE[len];
            cmd[0] = COMMAND_GET_FILE;
            memcpy(cmd + 1, folder.c_str(), folder.length());
            cmd[1 + folder.length()] = 0;
            memcpy(cmd + 1 + folder.length() + 1, files.data(), files.length());
            cmd[1 + folder.length() + files.length()] = 0;
            m_ContextObject->Send2Client(cmd, len);
            SAFE_DELETE_ARRAY(cmd);
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
    case TOKEN_BITMAPINFO: {
        SAFE_DELETE(m_BitmapInfor_Full);
        m_bIsFirst = TRUE;
        const ULONG	ulBitmapInforLength = sizeof(BITMAPINFOHEADER);
        m_BitmapInfor_Full = (BITMAPINFO*) new BYTE[ulBitmapInforLength];
        m_ContextObject->InDeCompressedBuffer.CopyBuffer(m_BitmapInfor_Full, ulBitmapInforLength, 1);
        PrepareDrawing(m_BitmapInfor_Full);
        break;
    }
    default: {
        Mprintf("CScreenSpyDlg unknown command: %d!!!\n", int(cmd));
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
    if (FirstScreenData == NULL) return;
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

    if (bChange && !m_bHide) {
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
	if (m_bIsClosed) return;
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
    if (!m_bConnected && GetTickCount64()-m_nDisconnectTime>2000) {
        DrawTipString("正在重连......", 2);
	}
}

VOID CScreenSpyDlg::DrawTipString(CString strString, int fillMode)
{
	// fillMode: 0=不填充, 1=全黑, 2=半透明
	RECT Rect;
	GetClientRect(&Rect);
	int width = Rect.right - Rect.left;
	int height = Rect.bottom - Rect.top;

	if (fillMode == 1)
	{
		// 原来的全黑效果
		COLORREF BackgroundColor = RGB(0x00, 0x00, 0x00);
		SetBkColor(m_hFullDC, BackgroundColor);
		ExtTextOut(m_hFullDC, 0, 0, ETO_OPAQUE, &Rect, NULL, 0, NULL);
	}
	else if (fillMode == 2)
	{
		// 半透明效果
		HDC hMemDC = CreateCompatibleDC(m_hFullDC);
		HBITMAP hBitmap = CreateCompatibleBitmap(m_hFullDC, width, height);
		HBITMAP hOldBitmap = (HBITMAP)SelectObject(hMemDC, hBitmap);

		HBRUSH hBrush = CreateSolidBrush(RGB(0, 0, 0));
		FillRect(hMemDC, &Rect, hBrush);
		DeleteObject(hBrush);

		BLENDFUNCTION blend = { 0 };
		blend.BlendOp = AC_SRC_OVER;
		blend.SourceConstantAlpha = 150;
		blend.AlphaFormat = 0;

		AlphaBlend(m_hFullDC, 0, 0, width, height,
			hMemDC, 0, 0, width, height, blend);

		SelectObject(hMemDC, hOldBitmap);
		DeleteObject(hBitmap);
		DeleteDC(hMemDC);
	}

	SetBkMode(m_hFullDC, TRANSPARENT);
	SetTextColor(m_hFullDC, RGB(0xff, 0x00, 0x00));
	DrawText(m_hFullDC, strString, -1, &Rect, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
}

bool DirectoryExists(const char* path)
{
    DWORD attr = GetFileAttributesA(path);
    return (attr != INVALID_FILE_ATTRIBUTES &&
            (attr & FILE_ATTRIBUTE_DIRECTORY));
}

std::string GetScreenShotPath(CWnd *parent, const CString& ip, const CString &filter, const CString& suffix)
{
    std::string path;
    std::string folder = THIS_CFG.GetStr("settings", "ScreenShot", "");
    if (folder.empty() || !DirectoryExists(folder.c_str())) {
        CString	strFileName = ip + CTime::GetCurrentTime().Format(_T("_%Y%m%d%H%M%S.")) + suffix;
        CFileDialog dlg(FALSE, suffix, strFileName, OFN_OVERWRITEPROMPT, filter, parent);
        if (dlg.DoModal() != IDOK)
            return "";
        folder = dlg.GetFolderPath();
        if (!folder.empty() && folder.back() != '\\') {
            folder += '\\';
        }
        path = dlg.GetPathName();
        THIS_CFG.SetStr("settings", "ScreenShot", folder);
    } else {
        if (!folder.empty() && folder.back() != '\\') {
            folder += '\\';
        }
        path = folder + std::string(ip) + "_" + ToPekingDateTime(0) + "." + std::string(suffix);
    }
    return path;
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
		BYTE cmd[4] = { CMD_FULL_SCREEN, m_Settings.FullScreen = TRUE };
		m_ContextObject->Send2Client(cmd, sizeof(cmd));
        break;
    }
    case IDM_SAVEDIB: {  // 快照保存
        SaveSnapshot();
        break;
    }
    case IDM_SAVEAVI:
    case IDM_SAVEAVI_H264: {
        if (SysMenu->GetMenuState(nID, MF_BYCOMMAND) & MF_CHECKED) {
            KillTimer(TIMER_ID);
            SysMenu->CheckMenuItem(nID, MF_UNCHECKED);
            SysMenu->EnableMenuItem(IDM_SAVEAVI, MF_ENABLED);
            SysMenu->EnableMenuItem(IDM_SAVEAVI_H264, MF_ENABLED);
            m_aviFile = "";
            m_aviStream.Close();
            return;
        }
        m_aviFile = GetScreenShotPath(this, m_IPAddress, "Video(*.avi)|*.avi|", "avi").c_str();
        const int duration = 250, rate = 1000 / duration;
        FCCHandler handler = nID == IDM_SAVEAVI ? ENCODER_MJPEG : ENCODER_H264;
        int code;
        if (code = m_aviStream.Open(m_aviFile, m_BitmapInfor_Full, rate, handler)) {
            MessageBox(CString("Create Video(*.avi) Failed:\n") + m_aviFile + "\r\n错误代码: " +
                       CBmpToAvi::GetErrMsg(code).c_str(), "提示");
            m_aviFile = _T("");
        } else {
            ::SetTimer(m_hWnd, TIMER_ID, duration, NULL);
            SysMenu->CheckMenuItem(nID, MF_CHECKED);
            SysMenu->EnableMenuItem(nID == IDM_SAVEAVI ? IDM_SAVEAVI_H264 : IDM_SAVEAVI, MF_DISABLED);
        }
        break;
    }

    case IDM_SWITCHSCREEN: {
        BYTE	bToken[2] = { COMMAND_SWITCH_SCREEN  };
        m_ContextObject->Send2Client(bToken, sizeof(bToken));
        break;
    }

    case IDM_MULTITHREAD_COMPRESS: {
        int threadNum = m_Settings.CompressThread;
        threadNum = 4 - threadNum;
        BYTE	bToken[2] = { CMD_MULTITHREAD_COMPRESS, (BYTE)threadNum };
        m_ContextObject->Send2Client(bToken, sizeof(bToken));
        SysMenu->CheckMenuItem(nID, threadNum ? MF_CHECKED : MF_UNCHECKED);
		m_Settings.CompressThread = threadNum;
        break;
    }

    case IDM_ORIGINAL_SIZE: {
        const int strategy = 1;
        BYTE cmd[16] = { CMD_SCREEN_SIZE, (BYTE)strategy };
        m_ContextObject->Send2Client(cmd, sizeof(cmd));
		m_Settings.ScreenStrategy = strategy;
		SysMenu->CheckMenuItem(IDM_ORIGINAL_SIZE, MF_CHECKED);
		SysMenu->CheckMenuItem(IDM_SCREEN_1080P, MF_UNCHECKED);
        break;
    }

    case IDM_SCREEN_1080P: {
        const int strategy = 0;
        BYTE cmd[16] = { CMD_SCREEN_SIZE, (BYTE)strategy };
        m_ContextObject->Send2Client(cmd, sizeof(cmd));
        m_Settings.ScreenStrategy = strategy;
		SysMenu->CheckMenuItem(IDM_SCREEN_1080P, MF_CHECKED);
		SysMenu->CheckMenuItem(IDM_ORIGINAL_SIZE, MF_UNCHECKED);
        break;
    }

    case IDM_FPS_10:
    case IDM_FPS_15:
    case IDM_FPS_20:
    case IDM_FPS_25:
    case IDM_FPS_30:
    case IDM_FPS_UNLIMITED: {
        int fps = 10 + (nID - IDM_FPS_10) * 5;
        BYTE	bToken[2] = { CMD_FPS, nID == IDM_FPS_UNLIMITED ? 255 : fps };
        m_ContextObject->Send2Client(bToken, sizeof(bToken));
		m_Settings.MaxFPS = nID == IDM_FPS_UNLIMITED ? 255 : fps;
        for (int i = IDM_FPS_10; i <= IDM_FPS_UNLIMITED; i++) {
			SysMenu->CheckMenuItem(i, MF_UNCHECKED);
		}
		SysMenu->CheckMenuItem(nID, MF_CHECKED);
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

void CScreenSpyDlg::OnTimer(UINT_PTR nIDEvent)
{
    if (!m_aviFile.IsEmpty()) {
        LPCTSTR	lpTipsString = _T("●");

        m_aviStream.Write((BYTE*)m_BitmapData_Full);

        // 提示正在录像
        SetTextColor(m_hFullDC, RGB(0xff, 0x00, 0x00));
        TextOut(m_hFullDC, 0, 0, lpTipsString, lstrlen(lpTipsString));
    }
    if (nIDEvent == 1 && m_Settings.FullScreen && m_pToolbar) {
        m_pToolbar->CheckMousePosition();
    }
    if (nIDEvent == 2) {
        KillTimer(2);
        if (m_bConnected) {
            return;
        }
        CWnd* pMain = AfxGetMainWnd();
        if (pMain)
            ::PostMessageA(pMain->GetSafeHwnd(), WM_SHOWNOTIFY, (WPARAM)new CharMsg("连接已断开"), 
                (LPARAM)new CharMsg(m_IPAddress + " - 远程桌面连接已断开"));
        this->PostMessageA(WM_CLOSE, 0, 0);
        return;
	}
    CDialog::OnTimer(nIDEvent);
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
        if (pMsg->message == WM_KEYDOWN && m_Settings.FullScreen) {
            // Ctrl+Alt+Home 退出全屏（备用）
            if (pMsg->wParam == VK_HOME &&
                (GetKeyState(VK_CONTROL) & 0x8000) &&
                (GetKeyState(VK_MENU) & 0x8000)) {
                LeaveFullScreen();
				BYTE cmd[4] = { CMD_FULL_SCREEN, m_Settings.FullScreen = FALSE };
				m_ContextObject->Send2Client(cmd, sizeof(cmd));
                return TRUE;
            }
        }
        if (pMsg->wParam != VK_LWIN && pMsg->wParam != VK_RWIN) {
            SendScaledMouseMessage(pMsg, true);
        }
        if (pMsg->message == WM_SYSKEYDOWN && pMsg->wParam == VK_F4) {
            return TRUE; // 屏蔽 Alt + F4
        }
        if (pMsg->wParam == VK_RETURN || pMsg->wParam == VK_ESCAPE)
            return TRUE;// 屏蔽Enter和ESC关闭对话
        break;
    }

    return CDialog::PreTranslateMessage(pMsg);
}


void CScreenSpyDlg::SendScaledMouseMessage(MSG* pMsg, bool makeLP)
{
    if (!m_bIsCtrl || !m_bConnected)
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

        bool fastMove = dist_sq > 50 * 50;
        int minInterval = fastMove ? 33 : 16;
        int minDistSq = fastMove ? 10 * 10 : 3 * 3;
        if (time_elapsed < minInterval && dist_sq < minDistSq) {
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
    auto path = GetScreenShotPath(this, m_IPAddress, "位图文件(*.bmp)|*.bmp|", "bmp");
    if (path.empty())
        return FALSE;

    WriteBitmap(m_BitmapInfor_Full, m_BitmapData_Full, path.c_str());

    return true;
}


VOID CScreenSpyDlg::UpdateServerClipboard(char* szBuffer, ULONG ulLength)
{
    if (!::OpenClipboard(NULL))
        return;

    ::EmptyClipboard();

    // UTF-8 转 Unicode
    int wlen = MultiByteToWideChar(CP_UTF8, 0, szBuffer, ulLength, nullptr, 0);
    if (wlen > 0) {
        // 分配 Unicode 缓冲区（+1 确保 null 结尾）
        HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, (wlen + 1) * sizeof(wchar_t));
        if (hGlobal != NULL) {
            wchar_t* pWideStr = (wchar_t*)GlobalLock(hGlobal);
            if (pWideStr) {
                MultiByteToWideChar(CP_UTF8, 0, szBuffer, ulLength, pWideStr, wlen);
                pWideStr[wlen] = L'\0';  // 确保 null 结尾
                GlobalUnlock(hGlobal);

                if (SetClipboardData(CF_UNICODETEXT, hGlobal) == NULL) {
                    GlobalFree(hGlobal);
                }
            } else {
                GlobalFree(hGlobal);
            }
        }
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
    if (1) {
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

        if (!m_pToolbar) {
            m_pToolbar = new CToolbarDlg(this);
            m_pToolbar->Create(IDD_TOOLBAR_DLG, this);
            // OnInitDialog() 会根据 m_bLocked 和 m_bOnTop 设置正确的位置和可见性
            // 如果未锁定，初始隐藏在屏幕外
            if (!m_pToolbar->m_bLocked) {
                int cx = GetSystemMetrics(SM_CXSCREEN);
                int cy = GetSystemMetrics(SM_CYSCREEN);
                int y = m_pToolbar->m_bOnTop ? -40 : cy;  // 根据位置设置隐藏在上方或下方
                m_pToolbar->SetWindowPos(&wndTopMost, 0, y, cx, 40, SWP_HIDEWINDOW);
            }
        }

        // 7. 标记全屏模式
        m_Settings.FullScreen = true;

        SetTimer(1, 50, NULL);
    }
}

// 全屏退出成功则返回true
bool CScreenSpyDlg::LeaveFullScreen()
{
    if (1){
        KillTimer(1);
        if (m_pToolbar) {
            m_pToolbar->DestroyWindow();
            delete m_pToolbar;
            m_pToolbar = nullptr;
        }

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
        m_Settings.FullScreen = false;

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

    if (nState != WA_INACTIVE) {
        // 通知主窗口：远程窗口获得焦点
        ::PostMessage(pMain->GetSafeHwnd(), WM_SESSION_ACTIVATED, (WPARAM)this, 0);
    }
}

void CScreenSpyDlg::UpdateCtrlStatus(BOOL ctrl)
{
    m_bIsCtrl = ctrl;
    SetClassLongPtr(m_hWnd, GCLP_HCURSOR, m_bIsCtrl ? (LONG_PTR)m_hRemoteCursor : (LONG_PTR)LoadCursor(NULL, IDC_NO));
}

void CScreenSpyDlg::OnDropFiles(HDROP hDropInfo)
{
    if (m_bIsCtrl && m_bConnected) {
        UINT nFiles = DragQueryFile(hDropInfo, 0xFFFFFFFF, NULL, 0);
        std::vector<std::string> list;
        for (UINT i = 0; i < nFiles; i++) {
            TCHAR szPath[MAX_PATH];
            DragQueryFile(hDropInfo, i, szPath, MAX_PATH);
            list.push_back(szPath);
        }
        std::string GetPwdHash();
        std::string GetHMAC(int offset);
        auto files = PreprocessFilesSimple(list);
        auto str = BuildMultiStringPath(files);
        BYTE* szBuffer = new BYTE[1 + 80 + str.size()];
        szBuffer[0] = { COMMAND_GET_FOLDER };
        std::string masterId = GetPwdHash(), hmac = GetHMAC(100);
        memcpy((char*)szBuffer + 1, masterId.c_str(), masterId.length());
        memcpy((char*)szBuffer + 1 + masterId.length(), hmac.c_str(), hmac.length());
        memcpy(szBuffer + 1 + 80, str.data(), str.size());
        auto md5 = CalcMD5FromBytes((BYTE*)str.data(), str.size());
        m_pParent->m_CmdList.PutCmd(md5);
        m_ContextObject->Send2Client(szBuffer, 81 + str.size());
        Mprintf("【Ctrl+V】 从本地拖拽文件到远程: %s \n", md5.c_str());
        SAFE_DELETE_ARRAY(szBuffer);
    }

    DragFinish(hDropInfo);

    CDialogBase::OnDropFiles(hDropInfo);
}
