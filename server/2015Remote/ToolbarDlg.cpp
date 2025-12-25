#include "stdafx.h"
#include "ToolbarDlg.h"
#include "2015RemoteDlg.h"
#include <ScreenSpyDlg.h>

IMPLEMENT_DYNAMIC(CToolbarDlg, CDialogEx)

CToolbarDlg::CToolbarDlg(CWnd* pParent)
	: CDialogEx(IDD_TOOLBAR_DLG, pParent)
{
}

CToolbarDlg::~CToolbarDlg()
{
}

void CToolbarDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CToolbarDlg, CDialogEx)
	ON_BN_CLICKED(IDC_BTN_EXIT_FULLSCREEN, &CToolbarDlg::OnBnClickedExitFullscreen)
	ON_BN_CLICKED(CONTROL_BTN_ID, &CToolbarDlg::OnBnClickedCtrl)
	ON_BN_CLICKED(IDC_BTN_MINIMIZE, &CToolbarDlg::OnBnClickedMinimize)
	ON_BN_CLICKED(IDC_BTN_CLOSE, &CToolbarDlg::OnBnClickedClose)
	ON_WM_ERASEBKGND()
END_MESSAGE_MAP()

void CToolbarDlg::CheckMousePosition()
{
	CPoint pt;
	GetCursorPos(&pt);

	int cxScreen = GetSystemMetrics(SM_CXSCREEN);

	// 计算按钮群的总宽度 (4个按钮 + 间距)
	int totalWidth = 80 * 4 + 10 * 3;
	int leftBound = (cxScreen - totalWidth) / 2;
	int rightBound = (cxScreen + totalWidth) / 2;

	// 只有在按钮对应的横向范围内从下往上扫到顶端才弹出
	if (pt.y <= 2 && pt.x >= leftBound && pt.x <= rightBound) {
		if (!m_bVisible) SlideIn();
	}
	// 鼠标离开工具栏范围（或者在工具栏左右两侧）时隐藏
	else if (pt.y > m_nHeight + 10 || pt.x < leftBound - 50 || pt.x > rightBound + 50) {
		if (m_bVisible) SlideOut();
	}
}

void CToolbarDlg::SlideIn()
{
	if (m_bVisible) return;
	m_bVisible = true;

	// 获取屏幕宽度，确保位置正确
	int cx = GetSystemMetrics(SM_CXSCREEN);

	// 动画：从 -m_nHeight 移动到 0
	// 步进加大（10像素），等待时间极短（10-15ms）
	for (int i = -m_nHeight; i <= 0; i += 10) {
		// 使用 SWP_NOACTIVATE 极其重要，防止夺取焦点导致的界面闪烁
		SetWindowPos(&wndTopMost, 0, i, cx, m_nHeight, SWP_SHOWWINDOW | SWP_NOACTIVATE);

		// 强制窗口立即重绘按钮，否则背景会是一片漆黑直到动画结束
		UpdateWindow();

		Sleep(10); // 10ms 是人眼感知的流畅极限
	}

	// 确保最终位置精准在 0
	SetWindowPos(&wndTopMost, 0, 0, cx, m_nHeight, SWP_NOACTIVATE);
}

void CToolbarDlg::SlideOut()
{
	int cx = GetSystemMetrics(SM_CXSCREEN);
	for (int y = 0; y >= -m_nHeight; y -= 8) {
		SetWindowPos(&wndTopMost, 0, y, cx, m_nHeight, SWP_NOACTIVATE);
		Sleep(50);
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
	GetDlgItem(CONTROL_BTN_ID)->SetWindowTextA(pParent->m_bIsCtrl ? "暂停控制" : "控制屏幕");
}

void CToolbarDlg::OnBnClickedClose()
{
	GetParent()->PostMessage(WM_CLOSE);
}

BOOL CToolbarDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 1. 设置分层窗口样式
	ModifyStyleEx(0, WS_EX_LAYERED);

	// 2. 关键设置：使用 LWA_COLORKEY 和 LWA_ALPHA 混合
	// RGB(255, 0, 255) 是品红色，我们将它定义为“透明色”
	// 255 代表按钮部分完全不透明（如果你想要按钮也半透明，可以改成 150-200）
	SetLayeredWindowAttributes(RGB(255, 0, 255), 255, LWA_COLORKEY | LWA_ALPHA);

	// --- 按钮布局代码 (保持不变) ---
	int cx = GetSystemMetrics(SM_CXSCREEN);
	int btnWidth = 80;
	int btnHeight = 28;
	int btnSpacing = 10;
	int totalWidth = btnWidth * 4 + btnSpacing * 3;
	int startX = (cx - totalWidth) / 2;
	int y = (m_nHeight - btnHeight) / 2;

	GetDlgItem(IDC_BTN_EXIT_FULLSCREEN)->SetWindowPos(NULL, startX, y, btnWidth, btnHeight, SWP_NOZORDER);

	int nextX = startX + btnWidth + btnSpacing;
	GetDlgItem(CONTROL_BTN_ID)->SetWindowPos(NULL, nextX, y, btnWidth, btnHeight, SWP_NOZORDER);

	nextX += btnWidth + btnSpacing;
	GetDlgItem(IDC_BTN_MINIMIZE)->SetWindowPos(NULL, nextX, y, btnWidth, btnHeight, SWP_NOZORDER); // 放置最小化按钮

	nextX += btnWidth + btnSpacing;
	GetDlgItem(IDC_BTN_CLOSE)->SetWindowPos(NULL, nextX, y, btnWidth, btnHeight, SWP_NOZORDER);

	// 设置控制按钮文本
	CScreenSpyDlg* pParent = (CScreenSpyDlg*)GetParent();
	GetDlgItem(CONTROL_BTN_ID)->SetWindowTextA(pParent->m_bIsCtrl ? "暂停控制" : "控制屏幕");

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
