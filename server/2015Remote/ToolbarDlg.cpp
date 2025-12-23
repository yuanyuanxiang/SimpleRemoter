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
	ON_BN_CLICKED(IDC_BTN_CLOSE, &CToolbarDlg::OnBnClickedClose)
END_MESSAGE_MAP()

void CToolbarDlg::CheckMousePosition()
{
	CPoint pt;
	GetCursorPos(&pt);

	if (pt.y <= 2) {
		if (!m_bVisible) SlideIn();
	}
	else if (pt.y > m_nHeight + 20) {
		if (m_bVisible) SlideOut();
	}
}

void CToolbarDlg::SlideIn()
{
	m_bVisible = true;
	ShowWindow(SW_SHOWNOACTIVATE);

	int cx = GetSystemMetrics(SM_CXSCREEN);
	for (int y = -m_nHeight; y <= 0; y += 8) {
		SetWindowPos(&wndTopMost, 0, y, cx, m_nHeight, SWP_NOACTIVATE);
		Sleep(100);
	}
	SetWindowPos(&wndTopMost, 0, 0, cx, m_nHeight, SWP_NOACTIVATE);
}

void CToolbarDlg::SlideOut()
{
	int cx = GetSystemMetrics(SM_CXSCREEN);
	for (int y = 0; y >= -m_nHeight; y -= 8) {
		SetWindowPos(&wndTopMost, 0, y, cx, m_nHeight, SWP_NOACTIVATE);
		Sleep(100);
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

	// 设置分层窗口样式
	ModifyStyleEx(0, WS_EX_LAYERED);

	// 设置透明度 (0-255)
	SetLayeredWindowAttributes(0, 100, LWA_ALPHA);

	// 按钮居中代码...
	int cx = GetSystemMetrics(SM_CXSCREEN);
	int btnWidth = 80;
	int btnHeight = 28;
	int btnSpacing = 10;
	int totalWidth = btnWidth * 3 + btnSpacing * 2;
	int startX = (cx - totalWidth) / 2;
	int y = (m_nHeight - btnHeight) / 2;

	GetDlgItem(IDC_BTN_EXIT_FULLSCREEN)->SetWindowPos(NULL,
		startX, y, btnWidth, btnHeight, SWP_NOZORDER);
	GetDlgItem(CONTROL_BTN_ID)->SetWindowPos(NULL,
		startX + btnWidth + btnSpacing, y, btnWidth, btnHeight, SWP_NOZORDER);
	GetDlgItem(IDC_BTN_CLOSE)->SetWindowPos(NULL,
		startX + (btnWidth + btnSpacing) * 2, y, btnWidth, btnHeight, SWP_NOZORDER);

	CScreenSpyDlg* pParent = (CScreenSpyDlg*)GetParent();
	GetDlgItem(CONTROL_BTN_ID)->SetWindowTextA(pParent->m_bIsCtrl ? "暂停控制" : "控制屏幕");

	return TRUE;
}
