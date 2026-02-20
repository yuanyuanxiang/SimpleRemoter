#include "stdafx.h"
#include "ToolbarDlg.h"
#include "2015Remote.h"
#include "2015RemoteDlg.h"
#include <ScreenSpyDlg.h>
#include <common/commands.h>

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
    ON_BN_CLICKED(IDC_BTN_SWITCH_SCREEN, &CToolbarDlg::OnBnClickedSwitchScreen)
    ON_BN_CLICKED(IDC_BTN_BLOCK_INPUT, &CToolbarDlg::OnBnClickedBlockInput)
    ON_BN_CLICKED(IDC_BTN_STATUS_INFO, &CToolbarDlg::OnBnClickedStatusInfo)
    ON_BN_CLICKED(IDC_BTN_QUALITY, &CToolbarDlg::OnBnClickedQuality)
    ON_BN_CLICKED(IDC_BTN_RESTORE_CONSOLE, &CToolbarDlg::OnBnClickedRestoreConsole)
    ON_BN_CLICKED(IDC_BTN_SCREENSHOT, &CToolbarDlg::OnBnClickedScreenshot)
    ON_WM_ERASEBKGND()
END_MESSAGE_MAP()

RECT CToolbarDlg::GetParentMonitorRect()
{
    CWnd* pParent = GetParent();
    if (pParent) {
        HMONITOR hMonitor = MonitorFromWindow(pParent->GetSafeHwnd(), MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi = { sizeof(mi) };
        if (GetMonitorInfo(hMonitor, &mi)) {
            return mi.rcMonitor;
        }
    }
    // 回退到主显示器
    RECT rc = { 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
    return rc;
}

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

    RECT rcMonitor = GetParentMonitorRect();
    int monLeft = rcMonitor.left;
    int monRight = rcMonitor.right;
    int monTop = rcMonitor.top;
    int monBottom = rcMonitor.bottom;
    int monWidth = monRight - monLeft;
    int monHeight = monBottom - monTop;

    int btnSize = 32, btnSpacing = 8, btnCount = 12;

    if (m_nPosition <= 1) {
        // 水平模式 (上/下)
        int totalWidth = btnSize * btnCount + btnSpacing * (btnCount - 1);
        int leftBound = monLeft + (monWidth - totalWidth) / 2;
        int rightBound = monLeft + (monWidth + totalWidth) / 2;

        if (m_nPosition == 0) {
            // 工具栏在上方: 鼠标移到顶端时弹出
            if (pt.y <= monTop + 2 && pt.x >= leftBound && pt.x <= rightBound) {
                if (!m_bVisible) SlideIn();
            }
            else if (pt.y > monTop + m_nHeight + 10 || pt.x < leftBound - 50 || pt.x > rightBound + 50) {
                if (m_bVisible) SlideOut();
            }
        } else {
            // 工具栏在下方: 鼠标移到底端时弹出
            if (pt.y >= monBottom - 2 && pt.x >= leftBound && pt.x <= rightBound) {
                if (!m_bVisible) SlideIn();
            }
            else if (pt.y < monBottom - m_nHeight - 10 || pt.x < leftBound - 50 || pt.x > rightBound + 50) {
                if (m_bVisible) SlideOut();
            }
        }
    } else {
        // 垂直模式 (左/右)
        int vw = 40;
        int totalHeight = btnSize * btnCount + btnSpacing * (btnCount - 1);
        int topBound = monTop + (monHeight - totalHeight) / 2;
        int bottomBound = monTop + (monHeight + totalHeight) / 2;

        if (m_nPosition == 2) {
            // 工具栏在左边: 鼠标移到左端时弹出
            if (pt.x <= monLeft + 2 && pt.y >= topBound && pt.y <= bottomBound) {
                if (!m_bVisible) SlideIn();
            }
            else if (pt.x > monLeft + vw + 10 || pt.y < topBound - 50 || pt.y > bottomBound + 50) {
                if (m_bVisible) SlideOut();
            }
        } else {
            // 工具栏在右边: 鼠标移到右端时弹出
            if (pt.x >= monRight - 2 && pt.y >= topBound && pt.y <= bottomBound) {
                if (!m_bVisible) SlideIn();
            }
            else if (pt.x < monRight - vw - 10 || pt.y < topBound - 50 || pt.y > bottomBound + 50) {
                if (m_bVisible) SlideOut();
            }
        }
    }
}

void CToolbarDlg::SlideIn()
{
    if (m_bVisible) return;
    m_bVisible = true;

    RECT rcMonitor = GetParentMonitorRect();
    int monLeft = rcMonitor.left;
    int monTop = rcMonitor.top;
    int monRight = rcMonitor.right;
    int monBottom = rcMonitor.bottom;
    int monWidth = monRight - monLeft;
    int monHeight = monBottom - monTop;

    int hw = 524; // 水平工具栏宽度 (与垂直高度一致)
    int vw = 40;
    int vh = 524;

    // 从边缘展开（改变窗口大小），避免多显示器时跑到相邻屏幕
    switch (m_nPosition) {
    case 0: { // 从上方展开: 顶边固定在 monTop, 高度从1增长到 m_nHeight
        int hx = monLeft + (monWidth - hw) / 2;
        for (int h = 1; h <= m_nHeight; h += 10) {
            SetWindowPos(&wndTopMost, hx, monTop, hw, h, SWP_SHOWWINDOW | SWP_NOACTIVATE);
            UpdateWindow();
            Sleep(10);
        }
        SetWindowPos(&wndTopMost, hx, monTop, hw, m_nHeight, SWP_NOACTIVATE);
        break;
    }
    case 1: { // 从下方展开: 底边固定在 monBottom, 高度从1增长到 m_nHeight
        int hx = monLeft + (monWidth - hw) / 2;
        for (int h = 1; h <= m_nHeight; h += 10) {
            SetWindowPos(&wndTopMost, hx, monBottom - h, hw, h, SWP_SHOWWINDOW | SWP_NOACTIVATE);
            UpdateWindow();
            Sleep(10);
        }
        SetWindowPos(&wndTopMost, hx, monBottom - m_nHeight, hw, m_nHeight, SWP_NOACTIVATE);
        break;
    }
    case 2: { // 从左边展开: 左边固定在 monLeft, 宽度从1增长到 vw
        int vy = monTop + (monHeight - vh) / 2;
        for (int w = 1; w <= vw; w += 10) {
            SetWindowPos(&wndTopMost, monLeft, vy, w, vh, SWP_SHOWWINDOW | SWP_NOACTIVATE);
            UpdateWindow();
            Sleep(10);
        }
        SetWindowPos(&wndTopMost, monLeft, vy, vw, vh, SWP_NOACTIVATE);
        break;
    }
    case 3: { // 从右边展开: 右边固定在 monRight, 宽度从1增长到 vw
        int vy = monTop + (monHeight - vh) / 2;
        for (int w = 1; w <= vw; w += 10) {
            SetWindowPos(&wndTopMost, monRight - w, vy, w, vh, SWP_SHOWWINDOW | SWP_NOACTIVATE);
            UpdateWindow();
            Sleep(10);
        }
        SetWindowPos(&wndTopMost, monRight - vw, vy, vw, vh, SWP_NOACTIVATE);
        break;
    }
    }
}

void CToolbarDlg::SlideOut()
{
    RECT rcMonitor = GetParentMonitorRect();
    int monLeft = rcMonitor.left;
    int monTop = rcMonitor.top;
    int monRight = rcMonitor.right;
    int monBottom = rcMonitor.bottom;
    int monWidth = monRight - monLeft;
    int monHeight = monBottom - monTop;

    int hw = 524;
    int vw = 40;
    int vh = 524;

    CWnd* btns[] = {
        &m_btnExit, &m_btnControl,
        &m_btnLock, &m_btnPosition,
        &m_btnOpacity, &m_btnSwitchScreen,
        &m_btnBlockInput, &m_btnStatusInfo,
        &m_btnQuality, &m_btnScreenshot,
        &m_btnMinimize, &m_btnClose,
    };
    const int N = 12;
    CRect btnRects[N];
    for (int i = 0; i < N; i++) {
        btns[i]->GetWindowRect(&btnRects[i]);
        ScreenToClient(&btnRects[i]);
    }

    // 向边缘缩回，避免多显示器时跑到相邻屏幕
    switch (m_nPosition) {
    case 0: { // 向上缩回: 顶边固定, 底边从下向上收缩, 按钮跟随上移
        int hx = monLeft + (monWidth - hw) / 2;
        for (int h = m_nHeight; h >= 0; h -= 8) {
            int realH = max(h, 1);
            int offset = m_nHeight - realH;
            for (int i = 0; i < N; i++)
                btns[i]->SetWindowPos(NULL, btnRects[i].left, btnRects[i].top - offset, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            SetWindowPos(&wndTopMost, hx, monTop, hw, realH, SWP_NOACTIVATE);
            Sleep(50);
        }
        break;
    }
    case 1: { // 向下缩回: 底边固定, 顶边从上向下收缩, 按钮自然跟随
        int hx = monLeft + (monWidth - hw) / 2;
        for (int h = m_nHeight; h >= 0; h -= 8) {
            SetWindowPos(&wndTopMost, hx, monBottom - max(h, 1), hw, max(h, 1), SWP_NOACTIVATE);
            Sleep(50);
        }
        break;
    }
    case 2: { // 向左缩回: 左边固定, 右边从右向左收缩, 按钮跟随左移
        int vy = monTop + (monHeight - vh) / 2;
        for (int w = vw; w >= 0; w -= 8) {
            int realW = max(w, 1);
            int offset = vw - realW;
            for (int i = 0; i < N; i++)
                btns[i]->SetWindowPos(NULL, btnRects[i].left - offset, btnRects[i].top, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            SetWindowPos(&wndTopMost, monLeft, vy, realW, vh, SWP_NOACTIVATE);
            Sleep(50);
        }
        break;
    }
    case 3: { // 向右缩回: 右边固定, 左边从左向右收缩, 按钮自然跟随
        int vy = monTop + (monHeight - vh) / 2;
        for (int w = vw; w >= 0; w -= 8) {
            SetWindowPos(&wndTopMost, monRight - max(w, 1), vy, max(w, 1), vh, SWP_NOACTIVATE);
            Sleep(50);
        }
        break;
    }
    }

    // 恢复按钮位置（下次 SlideIn 时位置正确）
    for (int i = 0; i < N; i++)
        btns[i]->SetWindowPos(NULL, btnRects[i].left, btnRects[i].top, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
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
    UpdateButtonIcons();
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

    // 1. 设置分层窗口样式 (WS_EX_NOACTIVATE 防止工具栏点击时抢夺焦点)
    ModifyStyleEx(0, WS_EX_LAYERED | WS_EX_NOACTIVATE);

    // 2. 应用透明度设置
    ApplyOpacity();

    // 3. Subclass dialog buttons as CIconButton
    m_btnExit.SubclassDlgItem(IDC_BTN_EXIT_FULLSCREEN, this);
    m_btnControl.SubclassDlgItem(CONTROL_BTN_ID, this);
    m_btnLock.SubclassDlgItem(IDC_BTN_LOCK, this);
    m_btnPosition.SubclassDlgItem(IDC_BTN_POSITION, this);
    m_btnOpacity.SubclassDlgItem(IDC_BTN_OPACITY, this);
    m_btnSwitchScreen.SubclassDlgItem(IDC_BTN_SWITCH_SCREEN, this);
    m_btnBlockInput.SubclassDlgItem(IDC_BTN_BLOCK_INPUT, this);
    m_btnStatusInfo.SubclassDlgItem(IDC_BTN_STATUS_INFO, this);
    m_btnQuality.SubclassDlgItem(IDC_BTN_QUALITY, this);
    m_btnRestoreConsole.SubclassDlgItem(IDC_BTN_RESTORE_CONSOLE, this);
    m_btnScreenshot.SubclassDlgItem(IDC_BTN_SCREENSHOT, this);
    m_btnMinimize.SubclassDlgItem(IDC_BTN_MINIMIZE, this);
    m_btnClose.SubclassDlgItem(IDC_BTN_CLOSE, this);

    // Set static icons for buttons that don't change
    m_btnExit.SetIconDrawFunc(CIconButton::DrawIconExitFullscreen);
    m_btnSwitchScreen.SetIconDrawFunc(CIconButton::DrawIconSwitchScreen);
    m_btnQuality.SetIconDrawFunc(CIconButton::DrawIconQuality);
    m_btnRestoreConsole.SetIconDrawFunc(CIconButton::DrawIconRestoreConsole);
    m_btnScreenshot.SetIconDrawFunc(CIconButton::DrawIconScreenshot);
    m_btnMinimize.SetIconDrawFunc(CIconButton::DrawIconMinimize);
    m_btnClose.SetIconDrawFunc(CIconButton::DrawIconClose);
    m_btnClose.SetIsCloseButton(true);

    // 4. Create tooltip
    m_tooltip.Create(this);
    m_tooltip.Activate(TRUE);
    m_tooltip.SetMaxTipWidth(200);

    CScreenSpyDlg* pParent = (CScreenSpyDlg*)GetParent();
    m_tooltip.AddTool(&m_btnExit, _TR("退出全屏"));
    m_tooltip.AddTool(&m_btnControl, pParent->m_bIsCtrl ? _TR("暂停控制") : _TR("控制屏幕"));
    m_tooltip.AddTool(&m_btnLock, m_bLocked ? _TR("解锁") : _TR("锁定"));
    m_tooltip.AddTool(&m_btnPosition, _TR("放下面"));
    m_tooltip.AddTool(&m_btnOpacity, _TR("透明度"));
    m_tooltip.AddTool(&m_btnSwitchScreen, _TR("切换屏幕"));
    m_tooltip.AddTool(&m_btnBlockInput, _TR("锁定远程输入"));
    m_tooltip.AddTool(&m_btnStatusInfo, m_bShowStatusInfo ? _TR("隐藏状态信息") : _TR("显示状态信息"));
    m_tooltip.AddTool(&m_btnQuality, _TR("屏幕质量"));
    m_tooltip.AddTool(&m_btnRestoreConsole, _TR("RDP会话归位"));
    m_tooltip.AddTool(&m_btnScreenshot, _TR("截图"));
    m_tooltip.AddTool(&m_btnMinimize, _TR("最小化"));
    m_tooltip.AddTool(&m_btnClose, _TR("关闭"));

    // 5. Set state-dependent icons and tooltips
    UpdateButtonIcons();

    // 6. 布局按钮
    LayoutButtons();

    // 如果是锁定状态，立即显示工具栏（否则锁定时无法触发显示）
    if (m_bLocked) {
        m_bVisible = true;
        RECT rcMonitor = GetParentMonitorRect();
        int monWidth = rcMonitor.right - rcMonitor.left;
        int monHeight = rcMonitor.bottom - rcMonitor.top;

        int hw = 524;
        int vw = 40;
        int vh = 524;
        int hx = rcMonitor.left + (monWidth - hw) / 2;

        switch (m_nPosition) {
        case 0:
            SetWindowPos(&wndTopMost, hx, rcMonitor.top, hw, m_nHeight, SWP_SHOWWINDOW | SWP_NOACTIVATE);
            break;
        case 1:
            SetWindowPos(&wndTopMost, hx, rcMonitor.bottom - m_nHeight, hw, m_nHeight, SWP_SHOWWINDOW | SWP_NOACTIVATE);
            break;
        case 2: {
            int vy = rcMonitor.top + (monHeight - vh) / 2;
            SetWindowPos(&wndTopMost, rcMonitor.left, vy, vw, vh, SWP_SHOWWINDOW | SWP_NOACTIVATE);
            break;
        }
        case 3: {
            int vy = rcMonitor.top + (monHeight - vh) / 2;
            SetWindowPos(&wndTopMost, rcMonitor.right - vw, vy, vw, vh, SWP_SHOWWINDOW | SWP_NOACTIVATE);
            break;
        }
        }
    }

    return TRUE;
}

void CToolbarDlg::UpdateButtonIcons()
{
    CScreenSpyDlg* pParent = (CScreenSpyDlg*)GetParent();

    // Control button: Play (start control) or Pause (pause control)
    if (pParent->m_bIsCtrl) {
        m_btnControl.SetIconDrawFunc(CIconButton::DrawIconPause);
        m_tooltip.UpdateTipText(_TR("暂停控制"), &m_btnControl);
    } else {
        m_btnControl.SetIconDrawFunc(CIconButton::DrawIconPlay);
        m_tooltip.UpdateTipText(_TR("控制屏幕"), &m_btnControl);
    }
    m_btnControl.Invalidate(FALSE);

    // Lock button
    if (m_bLocked) {
        m_btnLock.SetIconDrawFunc(CIconButton::DrawIconLock);
        m_tooltip.UpdateTipText(_TR("解锁"), &m_btnLock);
    } else {
        m_btnLock.SetIconDrawFunc(CIconButton::DrawIconUnlock);
        m_tooltip.UpdateTipText(_TR("锁定"), &m_btnLock);
    }
    m_btnLock.Invalidate(FALSE);

    // Position button: show arrow indicating next position
    switch (m_nPosition) {
    case 0: // currently top, click goes to bottom
        m_btnPosition.SetIconDrawFunc(CIconButton::DrawIconArrowDown);
        m_tooltip.UpdateTipText(_TR("放下面"), &m_btnPosition);
        break;
    case 1: // currently bottom, click goes to left
        m_btnPosition.SetIconDrawFunc(CIconButton::DrawIconArrowLeft);
        m_tooltip.UpdateTipText(_TR("放左边"), &m_btnPosition);
        break;
    case 2: // currently left, click goes to right
        m_btnPosition.SetIconDrawFunc(CIconButton::DrawIconArrowRight);
        m_tooltip.UpdateTipText(_TR("放右边"), &m_btnPosition);
        break;
    default: // currently right, click goes to top
        m_btnPosition.SetIconDrawFunc(CIconButton::DrawIconArrowUp);
        m_tooltip.UpdateTipText(_TR("放上面"), &m_btnPosition);
        break;
    }
    m_btnPosition.Invalidate(FALSE);

    // Opacity button
    switch (m_nOpacityLevel) {
    case 1:
        m_btnOpacity.SetIconDrawFunc(CIconButton::DrawIconOpacityMedium);
        m_tooltip.UpdateTipText(_TR("透明75%"), &m_btnOpacity);
        break;
    case 2:
        m_btnOpacity.SetIconDrawFunc(CIconButton::DrawIconOpacityLow);
        m_tooltip.UpdateTipText(_TR("透明50%"), &m_btnOpacity);
        break;
    default:
        m_btnOpacity.SetIconDrawFunc(CIconButton::DrawIconOpacityFull);
        m_tooltip.UpdateTipText(_TR("透明度"), &m_btnOpacity);
        break;
    }
    m_btnOpacity.Invalidate(FALSE);

    // Block input button
    if (m_bBlockInput) {
        m_btnBlockInput.SetIconDrawFunc(CIconButton::DrawIconBlockInput);
        m_tooltip.UpdateTipText(_TR("解除锁定输入"), &m_btnBlockInput);
    } else {
        m_btnBlockInput.SetIconDrawFunc(CIconButton::DrawIconUnblockInput);
        m_tooltip.UpdateTipText(_TR("锁定远程输入"), &m_btnBlockInput);
    }
    m_btnBlockInput.Invalidate(FALSE);

    // Status info button
    if (m_bShowStatusInfo) {
        m_btnStatusInfo.SetIconDrawFunc(CIconButton::DrawIconInfo);
        m_tooltip.UpdateTipText(_TR("隐藏状态信息"), &m_btnStatusInfo);
    } else {
        m_btnStatusInfo.SetIconDrawFunc(CIconButton::DrawIconInfoHide);
        m_tooltip.UpdateTipText(_TR("显示状态信息"), &m_btnStatusInfo);
    }
    m_btnStatusInfo.Invalidate(FALSE);
}

void CToolbarDlg::LayoutButtons()
{
    int btnSize = 32;
    int btnSpacing = 8;
    int btnCount = 13;

    CWnd* btns[] = {
        &m_btnExit,
        &m_btnControl,
        &m_btnLock,
        &m_btnPosition,
        &m_btnOpacity,
        &m_btnSwitchScreen,
        &m_btnBlockInput,
        &m_btnStatusInfo,
        &m_btnQuality,
        &m_btnRestoreConsole,
        &m_btnScreenshot,
        &m_btnMinimize,
        &m_btnClose,
    };

    int margin = (m_nHeight - btnSize) / 2; // 4px 边距

    if (m_nPosition <= 1) {
        // 水平布局 (上/下) — 窗口宽度为 320, 按钮从左边距开始排列
        for (int i = 0; i < btnCount; i++) {
            btns[i]->SetWindowPos(NULL, margin + i * (btnSize + btnSpacing), margin, btnSize, btnSize, SWP_NOZORDER);
        }
    } else {
        // 垂直布局 (左/右) — 窗口宽度为 40, 按钮从上边距开始排列
        for (int i = 0; i < btnCount; i++) {
            btns[i]->SetWindowPos(NULL, margin, margin + i * (btnSize + btnSpacing), btnSize, btnSize, SWP_NOZORDER);
        }
    }
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
    pDC->FillSolidRect(rect, RGB(40, 40, 40));
    return TRUE;
}

void CToolbarDlg::OnBnClickedLock()
{
    m_bLocked = !m_bLocked;
    UpdateButtonIcons();
    SaveSettings();
}

void CToolbarDlg::OnBnClickedPosition()
{
    m_nPosition = (m_nPosition + 1) % 4;
    UpdateButtonIcons();
    LayoutButtons();
    UpdatePosition();
    SaveSettings();
}

void CToolbarDlg::UpdatePosition()
{
    RECT rcMonitor = GetParentMonitorRect();
    int monWidth = rcMonitor.right - rcMonitor.left;
    int monHeight = rcMonitor.bottom - rcMonitor.top;

    int hw = 524;
    int vw = 40;
    int vh = 524;
    int hx = rcMonitor.left + (monWidth - hw) / 2;

    switch (m_nPosition) {
    case 0:
        SetWindowPos(&wndTopMost, hx, rcMonitor.top, hw, m_nHeight, SWP_NOACTIVATE);
        break;
    case 1:
        SetWindowPos(&wndTopMost, hx, rcMonitor.bottom - m_nHeight, hw, m_nHeight, SWP_NOACTIVATE);
        break;
    case 2: {
        int vy = rcMonitor.top + (monHeight - vh) / 2;
        SetWindowPos(&wndTopMost, rcMonitor.left, vy, vw, vh, SWP_NOACTIVATE);
        break;
    }
    case 3: {
        int vy = rcMonitor.top + (monHeight - vh) / 2;
        SetWindowPos(&wndTopMost, rcMonitor.right - vw, vy, vw, vh, SWP_NOACTIVATE);
        break;
    }
    }
}

void CToolbarDlg::LoadSettings()
{
    m_bLocked = THIS_CFG.GetInt("toolbar", "Locked", 0) != 0;
    int pos = THIS_CFG.GetInt("toolbar", "Position", 0);
    m_nPosition = (pos >= 0 && pos < 4) ? pos : 0;
    int opa = THIS_CFG.GetInt("toolbar", "OpacityLevel", 0);
    m_nOpacityLevel = (opa >= 0 && opa < 3) ? opa : 0;
    m_bShowStatusInfo = THIS_CFG.GetInt("toolbar", "ShowStatusInfo", 1) != 0;
}

void CToolbarDlg::SaveSettings()
{
    THIS_CFG.SetInt("toolbar", "Locked", m_bLocked ? 1 : 0);
    THIS_CFG.SetInt("toolbar", "Position", m_nPosition);
    THIS_CFG.SetInt("toolbar", "OpacityLevel", m_nOpacityLevel);
    THIS_CFG.SetInt("toolbar", "ShowStatusInfo", m_bShowStatusInfo ? 1 : 0);
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
    SetLayeredWindowAttributes(0, opacity, LWA_ALPHA);
}

void CToolbarDlg::OnBnClickedOpacity()
{
    m_nOpacityLevel = (m_nOpacityLevel + 1) % 3;
    ApplyOpacity();
    UpdateButtonIcons();
    SaveSettings();

    // 同步状态信息窗口的透明度
    CScreenSpyDlg* pParent = (CScreenSpyDlg*)GetParent();
    if (pParent && pParent->m_pStatusInfoWnd) {
        pParent->m_pStatusInfoWnd->SetOpacityLevel(m_nOpacityLevel);
    }
}

void CToolbarDlg::OnBnClickedSwitchScreen()
{
    GetParent()->PostMessage(WM_SYSCOMMAND, IDM_SWITCHSCREEN, 0);
}

void CToolbarDlg::OnBnClickedBlockInput()
{
    GetParent()->PostMessage(WM_SYSCOMMAND, IDM_BLOCK_INPUT, 0);
}

void CToolbarDlg::OnBnClickedStatusInfo()
{
    m_bShowStatusInfo = !m_bShowStatusInfo;
    UpdateButtonIcons();
    SaveSettings();

    // 通知父窗口显示/隐藏状态窗口
    GetParent()->PostMessage(WM_COMMAND,
        m_bShowStatusInfo ? ID_SHOW_STATUS_INFO : ID_HIDE_STATUS_INFO, 0);
}

void CToolbarDlg::OnBnClickedQuality()
{
    CMenu menu;
    menu.CreatePopupMenu();
    menu.AppendMenuL(MF_STRING, IDM_QUALITY_OFF, "关闭(&O)");
    menu.AppendMenuL(MF_STRING, IDM_ADAPTIVE_QUALITY, "自适应(&A)");
    menu.AppendMenu(MF_SEPARATOR);
    menu.AppendMenuL(MF_STRING, IDM_QUALITY_ULTRA, "Ultra (25FPS, DIFF)");
    menu.AppendMenuL(MF_STRING, IDM_QUALITY_HIGH, "High (20FPS, RGB565)");
    menu.AppendMenuL(MF_STRING, IDM_QUALITY_GOOD, "Good (20FPS, H264)");
    menu.AppendMenuL(MF_STRING, IDM_QUALITY_MEDIUM, "Medium (15FPS, H264)");
    menu.AppendMenuL(MF_STRING, IDM_QUALITY_LOW, "Low (12FPS, H264)");
    menu.AppendMenuL(MF_STRING, IDM_QUALITY_MINIMAL, "Minimal (8FPS, H264)");

    // 勾选当前质量
    CScreenSpyDlg* pParent = (CScreenSpyDlg*)GetParent();
    if (pParent->m_Settings.QualityLevel == QUALITY_DISABLED) {
        menu.CheckMenuItem(IDM_QUALITY_OFF, MF_CHECKED);
    } else if (pParent->m_AdaptiveQuality.enabled) {
        menu.CheckMenuItem(IDM_ADAPTIVE_QUALITY, MF_CHECKED);
    } else if (pParent->m_AdaptiveQuality.currentLevel >= 0 && pParent->m_AdaptiveQuality.currentLevel < QUALITY_COUNT) {
        menu.CheckMenuItem(IDM_QUALITY_ULTRA + pParent->m_AdaptiveQuality.currentLevel, MF_CHECKED);
    }

    // 弹出菜单
    CRect rc;
    m_btnQuality.GetWindowRect(&rc);
    UINT cmd = menu.TrackPopupMenu(TPM_RETURNCMD | TPM_NONOTIFY, rc.right, rc.top, this);
    if (cmd) {
        GetParent()->PostMessage(WM_SYSCOMMAND, cmd, 0);
    }
}

void CToolbarDlg::OnBnClickedRestoreConsole()
{
    CScreenSpyDlg* pParent = (CScreenSpyDlg*)GetParent();
    if (pParent) {
        BeginWaitCursor();
        BYTE bToken = CMD_RESTORE_CONSOLE;
        pParent->m_ContextObject->Send2Client(&bToken, 1);
        Sleep(1000);  // 等待会话切换完成
        EndWaitCursor();
    }
}

void CToolbarDlg::OnBnClickedScreenshot()
{
    CScreenSpyDlg* pParent = (CScreenSpyDlg*)GetParent();
    if (pParent) {
        pParent->SaveSnapshot();
    }
}

BOOL CToolbarDlg::PreTranslateMessage(MSG* pMsg)
{
    if (m_tooltip.GetSafeHwnd()) {
        m_tooltip.RelayEvent(pMsg);
    }
    // 不拦截这些消息，让它们继续传播到父窗口 (ScreenSpyDlg) 处理远程转发
    switch (pMsg->message) {
    case WM_MOUSEWHEEL:
    case WM_MOUSEHWHEEL:
    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
        return FALSE;
    }
    return __super::PreTranslateMessage(pMsg);
}
