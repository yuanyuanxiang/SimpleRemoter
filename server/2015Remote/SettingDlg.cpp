// SettingDlg.cpp : 实现文件
//

#include "stdafx.h"
#include "2015Remote.h"
#include "SettingDlg.h"
#include "afxdialogex.h"
#include "client/CursorInfo.h"


// CSettingDlg 对话框

IMPLEMENT_DYNAMIC(CSettingDlg, CDialog)

CSettingDlg::CSettingDlg(CWnd* pParent)
	: CDialog(CSettingDlg::IDD, pParent)
	, m_nListenPort(0)
	, m_nMax_Connect(0)
	, m_sScreenCapture(_T("GDI"))
	, m_sScreenCompress(_T("屏幕差异算法"))
{
}

CSettingDlg::~CSettingDlg()
{
}

void CSettingDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDIT_PORT, m_nListenPort);
	DDX_Text(pDX, IDC_EDIT_MAX, m_nMax_Connect);
	DDX_Control(pDX, IDC_BUTTON_SETTINGAPPLY, m_ApplyButton);
	DDX_Control(pDX, IDC_COMBO_SCREEN_CAPTURE, m_ComboScreenCapture);
	DDX_CBString(pDX, IDC_COMBO_SCREEN_CAPTURE, m_sScreenCapture);
	DDV_MaxChars(pDX, m_sScreenCapture, 32);
	DDX_Control(pDX, IDC_COMBO_SCREEN_COMPRESS, m_ComboScreenCompress);
	DDX_CBString(pDX, IDC_COMBO_SCREEN_COMPRESS, m_sScreenCompress);
}

BEGIN_MESSAGE_MAP(CSettingDlg, CDialog)
	ON_BN_CLICKED(IDC_BUTTON_SETTINGAPPLY, &CSettingDlg::OnBnClickedButtonSettingapply)
	ON_EN_CHANGE(IDC_EDIT_PORT, &CSettingDlg::OnEnChangeEditPort)
	ON_EN_CHANGE(IDC_EDIT_MAX, &CSettingDlg::OnEnChangeEditMax)
END_MESSAGE_MAP()


// CSettingDlg 消息处理程序


BOOL CSettingDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	int nPort = ((CMy2015RemoteApp*)AfxGetApp())->m_iniFile.GetInt("settings", "ghost");         
	//读取ini 文件中的监听端口
	int nMaxConnection = ((CMy2015RemoteApp*)AfxGetApp())->m_iniFile.GetInt("settings", "MaxConnection");    

	int DXGI = ((CMy2015RemoteApp*)AfxGetApp())->m_iniFile.GetInt("settings", "DXGI");

	CString algo = ((CMy2015RemoteApp*)AfxGetApp())->m_iniFile.GetStr("settings", "ScreenCompress", "");

	m_nListenPort = (nPort<=0 || nPort>65535) ? 6543 : nPort;
	m_nMax_Connect  = nMaxConnection<=0 ? 10000 : nMaxConnection;

	int n = algo.IsEmpty() ? ALGORITHM_DIFF : atoi(algo.GetString());
	switch (n)
	{
	case ALGORITHM_GRAY:
		m_sScreenCompress = "灰度图像传输";
		break;
	case ALGORITHM_DIFF:
		m_sScreenCompress = "屏幕差异算法";
		break;
	case ALGORITHM_H264:
		m_sScreenCompress = "H264压缩算法";
		break;
	default:
		break;
	}
	m_ComboScreenCompress.InsertString(ALGORITHM_GRAY, "灰度图像传输");
	m_ComboScreenCompress.InsertString(ALGORITHM_DIFF, "屏幕差异算法");
	m_ComboScreenCompress.InsertString(ALGORITHM_H264, "H264压缩算法");

	m_ComboScreenCapture.InsertString(0, "GDI");
	m_ComboScreenCapture.InsertString(1, "DXGI");
	m_sScreenCapture = DXGI ? "DXGI" : "GDI";

	UpdateData(FALSE);

	return TRUE; 
}


void CSettingDlg::OnBnClickedButtonSettingapply()
{
	UpdateData(TRUE);
	((CMy2015RemoteApp *)AfxGetApp())->m_iniFile.SetInt("settings", "ghost", m_nListenPort);      
	//向ini文件中写入值
	((CMy2015RemoteApp *)AfxGetApp())->m_iniFile.SetInt("settings", "MaxConnection", m_nMax_Connect);

	int n = m_ComboScreenCapture.GetCurSel();
	((CMy2015RemoteApp*)AfxGetApp())->m_iniFile.SetInt("settings", "DXGI", n);

	n = m_ComboScreenCompress.GetCurSel();
	((CMy2015RemoteApp*)AfxGetApp())->m_iniFile.SetInt("settings", "ScreenCompress", n);

	m_ApplyButton.EnableWindow(FALSE);
	m_ApplyButton.ShowWindow(SW_HIDE);
}


void CSettingDlg::OnEnChangeEditPort()
{
	// TODO:  如果该控件是 RICHEDIT 控件，它将不
	// 发送此通知，除非重写 CDialog::OnInitDialog()
	// 函数并调用 CRichEditCtrl().SetEventMask()，
	// 同时将 ENM_CHANGE 标志“或”运算到掩码中。

	// TODO:  在此添加控件通知处理程序代码

	// 给Button添加变量
	m_ApplyButton.ShowWindow(SW_NORMAL);
	m_ApplyButton.EnableWindow(TRUE);
}

void CSettingDlg::OnEnChangeEditMax()
{
	// TODO:  如果该控件是 RICHEDIT 控件，它将不
	// 发送此通知，除非重写 CDialog::OnInitDialog()
	// 函数并调用 CRichEditCtrl().SetEventMask()，
	// 同时将 ENM_CHANGE 标志“或”运算到掩码中。

	// TODO:  在此添加控件通知处理程序代码
	HWND hApplyButton = ::GetDlgItem(m_hWnd,IDC_BUTTON_SETTINGAPPLY);

	::ShowWindow(hApplyButton,SW_NORMAL);
	::EnableWindow(hApplyButton,TRUE);
}


void CSettingDlg::OnOK()
{
	OnBnClickedButtonSettingapply();

	CDialog::OnOK();
}
