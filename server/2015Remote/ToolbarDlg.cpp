#include "stdafx.h"
#include "ToolbarDlg.h"
#include "2015Remote.h"
#include "2015RemoteDlg.h"
#include <ScreenSpyDlg.h>

IMPLEMENT_DYNAMIC(CToolbarDlg, CDialogEx)

CToolbarDlg::CToolbarDlg(CScreenSpyDlg* pParent)
    : CDialogLangEx(IDD_TOOLBAR_DLG, pParent)
{
    m_pParent = pParent;
}

CToolbarDlg::~CToolbarDlg()
{
}

void CToolbarDlg::DoDataExchange(CDataExchange* pDX)
{
    __super::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CToolbarDlg, CDialogEx)
    ON_BN_CLICKED(IDC_BTN_EXIT_FULLSCREEN, &CToolbarDlg::OnBnClickedExitFullscreen)
    ON_BN_CLICKED(CONTROL_BTN_ID, &CToolbarDlg::OnBnClickedCtrl)
    ON_BN_CLICKED(IDC_BTN_MINIMIZE, &CToolbarDlg::OnBnClickedMinimize)
    ON_BN_CLICKED(IDC_BTN_CLOSE, &CToolbarDlg::OnBnClickedClose)
    ON_BN_CLICKED(IDC_BTN_LOCK, &CToolbarDlg::OnBnClickedLock)
    ON_BN_CLICKED(IDC_BTN_POSITION, &CToolbarDlg::OnBnClickedPosition)
    ON_BN_CLICKED(IDC_BTN_OPACITY, &CToolbarDlg::OnBnClickedOpacity)
    ON_BN_CLICKED(IDC_BTN_SCREENSHOT, &CToolbarDlg::OnBnClickedScreenshot)
    ON_WM_ERASEBKGND()
END_MESSAGE_MAP()

void CToolbarDlg::CheckMousePosition()
{
    // 如果父窗口最小化，隐藏工具栏
    CWnd* pParent = GetParent();
    if (pParent && pParent->IsIconic()) {
        if (m_bVisible) {
            ShowWindow(SW_HIDE);
            m_bVisible = false;
        }
        return;
    }

    // 如果工具栏已锁定，确保它可见（处理最小化恢复等情况）
    if (m_bLocked) {
        if (!m_bVisible) {
            SlideIn();
        }
        return;
    }

    CPoint pt;
    GetCursorPos(&pt);

    int cxScreen = GetSystemMetrics(SM_CXSCREEN);
    int cyScreen = GetSystemMetrics(SM_CYSCREEN);

    // 计算按钮群的总宽度 (8个按钮 + 间距)
    int totalWidth = 80 * 8 + 10 * 7;
    int leftBound = (cxScreen - totalWidth) / 2;
    int rightBound = (cxScreen + totalWidth) / 2;

    if (m_bOnTop) {
        // 工具栏在上方: 鼠标移到顶端时弹出
        if (pt.y <= 2 && pt.x >= leftBound && pt.x <= rightBound) {
            if (!m_bVisible) SlideIn();
        }
        // 鼠标离开工具栏范围时隐藏
        else if (pt.y > m_nHeight + 10 || pt.x < leftBound - 50 || pt.x > rightBound + 50) {
            if (m_bVisible) SlideOut();
        }
    } else {
        // 工具栏在下方: 鼠标移到底端时弹出
        if (pt.y >= cyScreen - 2 && pt.x >= leftBound && pt.x <= rightBound) {
            if (!m_bVisible) SlideIn();
        }
        // 鼠标离开工具栏范围时隐藏
        else if (pt.y < cyScreen - m_nHeight - 10 || pt.x < leftBound - 50 || pt.x > rightBound + 50) {
            if (m_bVisible) SlideOut();
        }
    }
}

void CToolbarDlg::SlideIn()
{
    if (m_bVisible) return;
    m_bVisible = true;

    // 获取屏幕尺寸
    int cx = GetSystemMetrics(SM_CXSCREEN);
    int cy = GetSystemMetrics(SM_CYSCREEN);

    if (m_bOnTop) {
        // 从上方滑入: 从 -m_nHeight 移动到 0
        for (int i = -m_nHeight; i <= 0; i += 10) {
            SetWindowPos(&wndTopMost, 0, i, cx, m_nHeight, SWP_SHOWWINDOW | SWP_NOACTIVATE);
            UpdateWindow();
            Sleep(10);
        }
        SetWindowPos(&wndTopMost, 0, 0, cx, m_nHeight, SWP_NOACTIVATE);
    } else {
        // 从下方滑入: 从 cy 移动到 cy - m_nHeight
        for (int i = cy; i >= cy - m_nHeight; i -= 10) {
            SetWindowPos(&wndTopMost, 0, i, cx, m_nHeight, SWP_SHOWWINDOW | SWP_NOACTIVATE);
            UpdateWindow();
            Sleep(10);
        }
        SetWindowPos(&wndTopMost, 0, cy - m_nHeight, cx, m_nHeight, SWP_NOACTIVATE);
    }
}

void CToolbarDlg::SlideOut()
{
    int cx = GetSystemMetrics(SM_CXSCREEN);
    int cy = GetSystemMetrics(SM_CYSCREEN);

    if (m_bOnTop) {
        // 向上滑出
        for (int y = 0; y >= -m_nHeight; y -= 8) {
            SetWindowPos(&wndTopMost, 0, y, cx, m_nHeight, SWP_NOACTIVATE);
            Sleep(50);
        }
    } else {
        // 向下滑出
        for (int y = cy - m_nHeight; y <= cy; y += 8) {
            SetWindowPos(&wndTopMost, 0, y, cx, m_nHeight, SWP_NOACTIVATE);
            Sleep(50);
        }
    }
    ShowWindow(SW_HIDE);
    m_bVisible = false;
}

void CToolbarDlg::OnBnClickedExitFullscreen()
{
    // 通知父窗口退出全屏
    GetParent()->PostMessage(WM_COMMAND, ID_EXIT_FULLSCREEN, 0);
}

void CToolbarDlg::OnBnClickedCtrl()
{
    CScreenSpyDlg* pParent = (CScreenSpyDlg*)GetParent();
    pParent->m_bIsCtrl = !pParent->m_bIsCtrl;
    pParent->UpdateCtrlStatus(pParent->m_bIsCtrl);
    GetDlgItem(CONTROL_BTN_ID)->SetWindowTextA(pParent->m_bIsCtrl ? _TR("暂停控制") : _TR("控制屏幕"));
}

void CToolbarDlg::OnBnClickedClose()
{
    GetParent()->PostMessage(WM_CLOSE);
}

BOOL CToolbarDlg::OnInitDialog()
{
    __super::OnInitDialog();

    // 加载用户设置
    LoadSettings();

    // 1. 设置分层窗口样式
    ModifyStyleEx(0, WS_EX_LAYERED);

    // 2. 应用透明度设置
    ApplyOpacity();

    // --- 按钮布局代码 (8个按钮) ---
    int cx = GetSystemMetrics(SM_CXSCREEN);
    int btnWidth = 80;
    int btnHeight = 28;
    int btnSpacing = 10;
    int totalWidth = btnWidth * 8 + btnSpacing * 7;
    int startX = (cx - totalWidth) / 2;
    int y = (m_nHeight - btnHeight) / 2;

    GetDlgItem(IDC_BTN_EXIT_FULLSCREEN)->SetWindowPos(NULL, startX, y, btnWidth, btnHeight, SWP_NOZORDER);

    int nextX = startX + btnWidth + btnSpacing;
    GetDlgItem(CONTROL_BTN_ID)->SetWindowPos(NULL, nextX, y, btnWidth, btnHeight, SWP_NOZORDER);

    nextX += btnWidth + btnSpacing;
    GetDlgItem(IDC_BTN_LOCK)->SetWindowPos(NULL, nextX, y, btnWidth, btnHeight, SWP_NOZORDER);

    nextX += btnWidth + btnSpacing;
    GetDlgItem(IDC_BTN_POSITION)->SetWindowPos(NULL, nextX, y, btnWidth, btnHeight, SWP_NOZORDER);

    nextX += btnWidth + btnSpacing;
    GetDlgItem(IDC_BTN_OPACITY)->SetWindowPos(NULL, nextX, y, btnWidth, btnHeight, SWP_NOZORDER);

    nextX += btnWidth + btnSpacing;
    GetDlgItem(IDC_BTN_SCREENSHOT)->SetWindowPos(NULL, nextX, y, btnWidth, btnHeight, SWP_NOZORDER);

    nextX += btnWidth + btnSpacing;
    GetDlgItem(IDC_BTN_MINIMIZE)->SetWindowPos(NULL, nextX, y, btnWidth, btnHeight, SWP_NOZORDER);

    nextX += btnWidth + btnSpacing;
    GetDlgItem(IDC_BTN_CLOSE)->SetWindowPos(NULL, nextX, y, btnWidth, btnHeight, SWP_NOZORDER);

    // 设置控制按钮文本
    CScreenSpyDlg* pParent = (CScreenSpyDlg*)GetParent();
    GetDlgItem(CONTROL_BTN_ID)->SetWindowTextA(pParent->m_bIsCtrl ? _TR("暂停控制") : _TR("控制屏幕"));

    // 设置锁定按钮文本
    GetDlgItem(IDC_BTN_LOCK)->SetWindowTextA(m_bLocked ? _TR("解锁") : _TR("锁定"));

    // 设置位置按钮文本
    GetDlgItem(IDC_BTN_POSITION)->SetWindowTextA(m_bOnTop ? _TR("放下面") : _TR("放上面"));

    // 设置透明度按钮文本
    GetDlgItem(IDC_BTN_OPACITY)->SetWindowTextA(GetOpacityText());

    // 设置截图按钮文本
    GetDlgItem(IDC_BTN_SCREENSHOT)->SetWindowTextA(_TR("截图"));

    // 如果是锁定状态，立即显示工具栏（否则锁定时无法触发显示）
    if (m_bLocked) {
        m_bVisible = true;
        int cy = GetSystemMetrics(SM_CYSCREEN);
        if (m_bOnTop) {
            SetWindowPos(&wndTopMost, 0, 0, cx, m_nHeight, SWP_SHOWWINDOW | SWP_NOACTIVATE);
        } else {
            SetWindowPos(&wndTopMost, 0, cy - m_nHeight, cx, m_nHeight, SWP_SHOWWINDOW | SWP_NOACTIVATE);
        }
    }

    return TRUE;
}

void CToolbarDlg::OnBnClickedMinimize()
{
    // 隐藏工具栏自身并最小化父窗口
    ShowWindow(SW_HIDE);
    m_bVisible = false;
    GetParent()->ShowWindow(SW_MINIMIZE);
}

BOOL CToolbarDlg::OnEraseBkgnd(CDC* pDC)
{
    CRect rect;
    GetClientRect(&rect);
    // 使用我们在 SetLayeredWindowAttributes 中定义的颜色填充背景
    pDC->FillSolidRect(rect, RGB(255, 0, 255));
    return TRUE;
}

void CToolbarDlg::OnBnClickedLock()
{
    m_bLocked = !m_bLocked;
    GetDlgItem(IDC_BTN_LOCK)->SetWindowTextA(m_bLocked ? _TR("解锁") : _TR("锁定"));
    SaveSettings();
}

void CToolbarDlg::OnBnClickedPosition()
{
    m_bOnTop = !m_bOnTop;
    GetDlgItem(IDC_BTN_POSITION)->SetWindowTextA(m_bOnTop ? _TR("放下面") : _TR("放上面"));
    UpdatePosition();
    SaveSettings();
}

void CToolbarDlg::UpdatePosition()
{
    int cx = GetSystemMetrics(SM_CXSCREEN);
    int cy = GetSystemMetrics(SM_CYSCREEN);

    if (m_bOnTop) {
        // 移动到屏幕上方
        SetWindowPos(&wndTopMost, 0, 0, cx, m_nHeight, SWP_NOACTIVATE);
    } else {
        // 移动到屏幕下方
        SetWindowPos(&wndTopMost, 0, cy - m_nHeight, cx, m_nHeight, SWP_NOACTIVATE);
    }
}

void CToolbarDlg::LoadSettings()
{
    m_bLocked = THIS_CFG.GetInt("toolbar", "Locked", 0) != 0;
    m_bOnTop = THIS_CFG.GetInt("toolbar", "OnTop", 1) != 0;
    m_nOpacityLevel = THIS_CFG.GetInt("toolbar", "OpacityLevel", 0) % 3;
}

void CToolbarDlg::SaveSettings()
{
    THIS_CFG.SetInt("toolbar", "Locked", m_bLocked ? 1 : 0);
    THIS_CFG.SetInt("toolbar", "OnTop", m_bOnTop ? 1 : 0);
    THIS_CFG.SetInt("toolbar", "OpacityLevel", m_nOpacityLevel);
}

void CToolbarDlg::ApplyOpacity()
{
    // 透明度级别: 0=100%(255), 1=75%(191), 2=50%(128)
    BYTE opacity;
    switch (m_nOpacityLevel) {
    case 1:
        opacity = 191;
        break;  // 75%
    case 2:
        opacity = 128;
        break;  // 50%
    default:
        opacity = 255;
        break;  // 100%
    }
    SetLayeredWindowAttributes(RGB(255, 0, 255), opacity, LWA_COLORKEY | LWA_ALPHA);
}

CString CToolbarDlg::GetOpacityText()
{
    switch (m_nOpacityLevel) {
    case 1:
        return _TR("透明75%");
    case 2:
        return _TR("透明50%");
    default:
        return _TR("透明度");
    }
}

void CToolbarDlg::OnBnClickedOpacity()
{
    m_nOpacityLevel = (m_nOpacityLevel + 1) % 3;
    ApplyOpacity();
    GetDlgItem(IDC_BTN_OPACITY)->SetWindowTextA(GetOpacityText());
    SaveSettings();
}

void CToolbarDlg::OnBnClickedScreenshot()
{
    CScreenSpyDlg* pParent = (CScreenSpyDlg*)GetParent();
    if (pParent) {
        pParent->SaveSnapshot();
    }
}
