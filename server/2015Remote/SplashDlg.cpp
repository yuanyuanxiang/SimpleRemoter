#include "stdafx.h"
#include "SplashDlg.h"
#include "resource.h"
#include "2015Remote.h"

BEGIN_MESSAGE_MAP(CSplashDlg, CWnd)
    ON_WM_PAINT()
    ON_MESSAGE(WM_USER + 5000, OnUpdateProgress)  // WM_SPLASH_PROGRESS
    ON_MESSAGE(WM_USER + 5001, OnUpdateStatus)    // WM_SPLASH_STATUS
    ON_MESSAGE(WM_USER + 5002, OnCloseSplash)     // WM_SPLASH_CLOSE
END_MESSAGE_MAP()

CSplashDlg::CSplashDlg()
    : m_nProgress(0)
    , m_strStatus(_T("正在初始化..."))
    , m_hIcon(NULL)
{
}

CSplashDlg::~CSplashDlg()
{
    if (m_fontTitle.m_hObject)
        m_fontTitle.DeleteObject();
    if (m_fontStatus.m_hObject)
        m_fontStatus.DeleteObject();
}

BOOL CSplashDlg::Create(CWnd* pParent)
{
    // 注册窗口类
    CString strClassName = AfxRegisterWndClass(
                               CS_HREDRAW | CS_VREDRAW,
                               ::LoadCursor(NULL, IDC_ARROW),
                               (HBRUSH)(COLOR_WINDOW + 1),
                               NULL);

    // 计算居中位置
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int x = (screenWidth - SPLASH_WIDTH) / 2;
    int y = (screenHeight - SPLASH_HEIGHT) / 2;

    // 创建无边框窗口
    DWORD dwStyle = WS_POPUP | WS_VISIBLE;
    DWORD dwExStyle = WS_EX_TOPMOST | WS_EX_TOOLWINDOW;

    if (!CreateEx(dwExStyle, strClassName, _T(""), dwStyle,
                  x, y, SPLASH_WIDTH, SPLASH_HEIGHT,
                  pParent ? pParent->GetSafeHwnd() : NULL, NULL)) {
        return FALSE;
    }

    // 创建字体
    m_fontTitle.CreateFont(
        28, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, _T("Microsoft YaHei"));

    m_fontStatus.CreateFont(
        14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, _T("Microsoft YaHei"));

    // 加载图标
    m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);

    // 注册到应用程序
    THIS_APP->SetSplash(this);

    ShowWindow(SW_SHOW);
    UpdateWindow();

    return TRUE;
}

void CSplashDlg::SetProgress(int nPercent)
{
    if (nPercent < 0) nPercent = 0;
    if (nPercent > 100) nPercent = 100;

    if (GetSafeHwnd()) {
        PostMessage(WM_SPLASH_PROGRESS, nPercent, 0);
    }
}

void CSplashDlg::SetStatusText(const CString& strText)
{
    if (GetSafeHwnd()) {
        CString* pText = new CString(strText);
        PostMessage(WM_SPLASH_STATUS, (WPARAM)pText, 0);
    }
}

void CSplashDlg::UpdateProgressDirect(int nPercent, const CString& strText)
{
    if (nPercent < 0) nPercent = 0;
    if (nPercent > 100) nPercent = 100;

    m_nProgress = nPercent;
    m_strStatus = strText;

    if (GetSafeHwnd()) {
        InvalidateRect(NULL, FALSE);
        UpdateWindow();

        // 处理待处理的消息，确保界面响应
        MSG msg;
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
}

void CSplashDlg::Close()
{
    if (GetSafeHwnd()) {
        PostMessage(WM_SPLASH_CLOSE, 0, 0);
    }
}

LRESULT CSplashDlg::OnUpdateProgress(WPARAM wParam, LPARAM lParam)
{
    m_nProgress = (int)wParam;
    InvalidateRect(NULL, FALSE);
    UpdateWindow();
    return 0;
}

LRESULT CSplashDlg::OnUpdateStatus(WPARAM wParam, LPARAM lParam)
{
    CString* pText = (CString*)wParam;
    if (pText) {
        m_strStatus = *pText;
        delete pText;
        InvalidateRect(NULL, FALSE);
        UpdateWindow();
    }
    return 0;
}

LRESULT CSplashDlg::OnCloseSplash(WPARAM wParam, LPARAM lParam)
{
    THIS_APP->SetSplash(nullptr);
    DestroyWindow();
    return 0;
}

void CSplashDlg::OnPaint()
{
    CPaintDC dc(this);
    CRect rect;
    GetClientRect(&rect);

    // 双缓冲绘制
    CDC memDC;
    memDC.CreateCompatibleDC(&dc);
    CBitmap memBitmap;
    memBitmap.CreateCompatibleBitmap(&dc, rect.Width(), rect.Height());
    CBitmap* pOldBitmap = memDC.SelectObject(&memBitmap);

    // 绘制背景 - 渐变效果
    for (int y = 0; y < rect.Height(); y++) {
        int r = 45 + (y * 20 / rect.Height());
        int g = 45 + (y * 20 / rect.Height());
        int b = 55 + (y * 25 / rect.Height());
        CPen pen(PS_SOLID, 1, RGB(r, g, b));
        CPen* pOldPen = memDC.SelectObject(&pen);
        memDC.MoveTo(0, y);
        memDC.LineTo(rect.Width(), y);
        memDC.SelectObject(pOldPen);
    }

    // 绘制边框
    CPen borderPen(PS_SOLID, 2, RGB(100, 100, 120));
    CPen* pOldPen = memDC.SelectObject(&borderPen);
    memDC.SelectStockObject(NULL_BRUSH);
    memDC.Rectangle(rect);
    memDC.SelectObject(pOldPen);

    // 绘制图标
    if (m_hIcon) {
        DrawIconEx(memDC.GetSafeHdc(), 30, 30, m_hIcon, 48, 48, 0, NULL, DI_NORMAL);
    }

    // 绘制标题
    memDC.SetBkMode(TRANSPARENT);
    memDC.SetTextColor(RGB(255, 255, 255));
    CFont* pOldFont = memDC.SelectObject(&m_fontTitle);
    memDC.TextOut(95, 35, _T("YAMA"));
    memDC.SelectObject(pOldFont);

    // 绘制副标题
    memDC.SetTextColor(RGB(180, 180, 190));
    pOldFont = memDC.SelectObject(&m_fontStatus);
    memDC.TextOut(95, 70, _T("Remote Administration Tool"));
    memDC.SelectObject(pOldFont);

    // 绘制进度条背景
    CRect progressRect(30, 120, rect.Width() - 30, 140);
    DrawProgressBar(&memDC, progressRect);

    // 绘制状态文本
    memDC.SetTextColor(RGB(200, 200, 210));
    pOldFont = memDC.SelectObject(&m_fontStatus);
    CRect textRect(30, 150, rect.Width() - 30, 180);
    memDC.DrawText(m_strStatus, textRect, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    memDC.SelectObject(pOldFont);

    // 绘制进度百分比
    CString strPercent;
    strPercent.Format(_T("%d%%"), m_nProgress);
    memDC.SetTextColor(RGB(100, 200, 255));
    textRect = CRect(rect.Width() - 80, 150, rect.Width() - 30, 180);
    memDC.DrawText(strPercent, textRect, DT_RIGHT | DT_SINGLELINE | DT_VCENTER);

    // 复制到屏幕
    dc.BitBlt(0, 0, rect.Width(), rect.Height(), &memDC, 0, 0, SRCCOPY);

    memDC.SelectObject(pOldBitmap);
}

void CSplashDlg::DrawProgressBar(CDC* pDC, const CRect& rect)
{
    // 进度条背景
    CBrush bgBrush(RGB(30, 30, 40));
    pDC->FillRect(rect, &bgBrush);

    // 进度条边框
    CPen borderPen(PS_SOLID, 1, RGB(80, 80, 100));
    CPen* pOldPen = pDC->SelectObject(&borderPen);
    pDC->SelectStockObject(NULL_BRUSH);
    pDC->Rectangle(rect);
    pDC->SelectObject(pOldPen);

    // 计算进度条填充区域
    if (m_nProgress > 0) {
        CRect fillRect = rect;
        fillRect.DeflateRect(2, 2);
        fillRect.right = fillRect.left + (fillRect.Width() * m_nProgress / 100);

        // 渐变填充
        for (int x = fillRect.left; x < fillRect.right; x++) {
            float ratio = (float)(x - fillRect.left) / (float)fillRect.Width();
            int r = (int)(50 + ratio * 50);
            int g = (int)(150 + ratio * 50);
            int b = (int)(200 + ratio * 55);
            CPen pen(PS_SOLID, 1, RGB(r, g, b));
            CPen* pOld = pDC->SelectObject(&pen);
            pDC->MoveTo(x, fillRect.top);
            pDC->LineTo(x, fillRect.bottom);
            pDC->SelectObject(pOld);
        }
    }
}

