// SettingDlg.cpp : ʵ���ļ�
//

#include "stdafx.h"
#include "2015Remote.h"
#include "SettingDlg.h"
#include "afxdialogex.h"
#include "client/CursorInfo.h"
#include "common/location.h"

// CSettingDlg �Ի���

IMPLEMENT_DYNAMIC(CSettingDlg, CDialog)

CSettingDlg::CSettingDlg(CWnd* pParent)
	: CDialog(CSettingDlg::IDD, pParent)
	, m_nListenPort("6543")
	, m_nMax_Connect(0)
	, m_sScreenCapture(_T("GDI"))
	, m_sScreenCompress(_T("��Ļ�����㷨"))
	, m_nReportInterval(5)
	, m_sSoftwareDetect(_T("����ͷ"))
	, m_sPublicIP(_T(""))
{
}

CSettingDlg::~CSettingDlg()
{
}

void CSettingDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDIT_PORT, m_nListenPort);
	DDV_MaxChars(pDX, m_nListenPort, 32);
	DDX_Text(pDX, IDC_EDIT_MAX, m_nMax_Connect);
	DDX_Control(pDX, IDC_BUTTON_SETTINGAPPLY, m_ApplyButton);
	DDX_Control(pDX, IDC_COMBO_SCREEN_CAPTURE, m_ComboScreenCapture);
	DDX_CBString(pDX, IDC_COMBO_SCREEN_CAPTURE, m_sScreenCapture);
	DDV_MaxChars(pDX, m_sScreenCapture, 32);
	DDX_Control(pDX, IDC_COMBO_SCREEN_COMPRESS, m_ComboScreenCompress);
	DDX_CBString(pDX, IDC_COMBO_SCREEN_COMPRESS, m_sScreenCompress);
	DDX_Control(pDX, IDC_EDIT_REPORTINTERVAL, m_EditReportInterval);
	DDX_Text(pDX, IDC_EDIT_REPORTINTERVAL, m_nReportInterval);
	DDV_MinMaxInt(pDX, m_nReportInterval, 0, 3600);
	DDX_Control(pDX, IDC_COMBO_SOFTWAREDETECT, m_ComboSoftwareDetect);
	DDX_CBString(pDX, IDC_COMBO_SOFTWAREDETECT, m_sSoftwareDetect);
	DDV_MaxChars(pDX, m_sSoftwareDetect, 256);
	DDX_Control(pDX, IDC_EDIT_PUBLIC_IP, m_EditPublicIP);
	DDX_Text(pDX, IDC_EDIT_PUBLIC_IP, m_sPublicIP);
	DDV_MaxChars(pDX, m_sPublicIP, 100);
}

BEGIN_MESSAGE_MAP(CSettingDlg, CDialog)
	ON_BN_CLICKED(IDC_BUTTON_SETTINGAPPLY, &CSettingDlg::OnBnClickedButtonSettingapply)
	ON_EN_CHANGE(IDC_EDIT_PORT, &CSettingDlg::OnEnChangeEditPort)
	ON_EN_CHANGE(IDC_EDIT_MAX, &CSettingDlg::OnEnChangeEditMax)
	ON_BN_CLICKED(IDC_RADIO_ALL_SCREEN, &CSettingDlg::OnBnClickedRadioAllScreen)
	ON_BN_CLICKED(IDC_RADIO_MAIN_SCREEN, &CSettingDlg::OnBnClickedRadioMainScreen)
END_MESSAGE_MAP()


// CSettingDlg ��Ϣ�������


BOOL CSettingDlg::OnInitDialog()
{
	CDialog::OnInitDialog();
	IPConverter cvt;
	m_sPublicIP = THIS_CFG.GetStr("settings", "master", "").c_str();
	m_sPublicIP = m_sPublicIP.IsEmpty() ? cvt.getPublicIP().c_str() : m_sPublicIP;
	std::string nPort = THIS_CFG.GetStr("settings", "ghost", "6543");

	int DXGI = THIS_CFG.GetInt("settings", "DXGI");

	CString algo = THIS_CFG.GetStr("settings", "ScreenCompress", "").c_str();

	m_nListenPort = nPort.c_str();

	int n = algo.IsEmpty() ? ALGORITHM_DIFF : atoi(algo.GetString());
	switch (n)
	{
	case ALGORITHM_GRAY:
		m_sScreenCompress = "�Ҷ�ͼ����";
		break;
	case ALGORITHM_DIFF:
		m_sScreenCompress = "��Ļ�����㷨";
		break;
	case ALGORITHM_H264:
		m_sScreenCompress = "H264ѹ���㷨";
		break;
	default:
		break;
	}
	m_ComboScreenCompress.InsertString(ALGORITHM_GRAY, "�Ҷ�ͼ����");
	m_ComboScreenCompress.InsertString(ALGORITHM_DIFF, "��Ļ�����㷨");
	m_ComboScreenCompress.InsertString(ALGORITHM_H264, "H264ѹ���㷨");

	m_ComboScreenCapture.InsertString(0, "GDI");
	m_ComboScreenCapture.InsertString(1, "DXGI");
	m_ComboScreenCapture.InsertString(2, "VIRTUAL");
	m_sScreenCapture = DXGI==1 ? "DXGI" : (DXGI == 2 ? "VIRTUAL" : "GDI");

	m_ComboSoftwareDetect.InsertString(SOFTWARE_CAMERA, "����ͷ");
	m_ComboSoftwareDetect.InsertString(SOFTWARE_TELEGRAM, "�籨");
	auto str = THIS_CFG.GetStr("settings", "ReportInterval", "5");
	m_nReportInterval = atoi(str.c_str());
	n = THIS_CFG.GetInt("settings", "SoftwareDetect");
	switch (n)
	{
	case SOFTWARE_CAMERA:
		m_sSoftwareDetect = "����ͷ";
		break;
	case SOFTWARE_TELEGRAM:
		m_sSoftwareDetect = "�籨";
		break;
	default:
		m_sSoftwareDetect = "����ͷ";
		break;
	}
	BOOL all = THIS_CFG.GetInt("settings", "MultiScreen");
	((CButton*)GetDlgItem(IDC_RADIO_ALL_SCREEN))->SetCheck(!all);
	((CButton*)GetDlgItem(IDC_RADIO_MAIN_SCREEN))->SetCheck(all);
	UpdateData(FALSE);

	return TRUE; 
}


void CSettingDlg::OnBnClickedButtonSettingapply()
{
	UpdateData(TRUE);
	THIS_CFG.SetStr("settings", "master", m_sPublicIP.GetBuffer());
	THIS_CFG.SetStr("settings", "ghost", m_nListenPort.GetString());

	int n = m_ComboScreenCapture.GetCurSel();
	THIS_CFG.SetInt("settings", "DXGI", n);

	n = m_ComboScreenCompress.GetCurSel();
	THIS_CFG.SetInt("settings", "ScreenCompress", n);

	THIS_CFG.SetInt("settings", "ReportInterval", m_nReportInterval);
	n = m_ComboSoftwareDetect.GetCurSel();
	THIS_CFG.SetInt("settings", "SoftwareDetect", n);

	BOOL all = ((CButton*)GetDlgItem(IDC_RADIO_MAIN_SCREEN))->GetCheck();
	THIS_CFG.SetInt("settings", "MultiScreen", all);

	m_ApplyButton.EnableWindow(FALSE);
	m_ApplyButton.ShowWindow(SW_HIDE);
}


void CSettingDlg::OnEnChangeEditPort()
{
	// TODO:  ����ÿؼ��� RICHEDIT �ؼ���������
	// ���ʹ�֪ͨ��������д CDialog::OnInitDialog()
	// ���������� CRichEditCtrl().SetEventMask()��
	// ͬʱ�� ENM_CHANGE ��־�������㵽�����С�

	// TODO:  �ڴ���ӿؼ�֪ͨ����������

	// ��Button��ӱ���
	m_ApplyButton.ShowWindow(SW_NORMAL);
	m_ApplyButton.EnableWindow(TRUE);
}

void CSettingDlg::OnEnChangeEditMax()
{
	// TODO:  ����ÿؼ��� RICHEDIT �ؼ���������
	// ���ʹ�֪ͨ��������д CDialog::OnInitDialog()
	// ���������� CRichEditCtrl().SetEventMask()��
	// ͬʱ�� ENM_CHANGE ��־�������㵽�����С�

	// TODO:  �ڴ���ӿؼ�֪ͨ����������
	HWND hApplyButton = ::GetDlgItem(m_hWnd,IDC_BUTTON_SETTINGAPPLY);

	::ShowWindow(hApplyButton,SW_NORMAL);
	::EnableWindow(hApplyButton,TRUE);
}


void CSettingDlg::OnOK()
{
	OnBnClickedButtonSettingapply();

	CDialog::OnOK();
}


void CSettingDlg::OnBnClickedRadioAllScreen()
{
	BOOL b = ((CButton*)GetDlgItem(IDC_RADIO_ALL_SCREEN))->GetCheck();
	((CButton*)GetDlgItem(IDC_RADIO_MAIN_SCREEN))->SetCheck(!b);
}


void CSettingDlg::OnBnClickedRadioMainScreen()
{
	BOOL b = ((CButton*)GetDlgItem(IDC_RADIO_MAIN_SCREEN))->GetCheck();
	((CButton*)GetDlgItem(IDC_RADIO_ALL_SCREEN))->SetCheck(!b);
}
