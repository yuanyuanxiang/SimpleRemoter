// CPasswordDlg.cpp: 实现文件
//

#include "stdafx.h"
#include "CPasswordDlg.h"
#include "afxdialogex.h"
#include "pwd_gen.h"
#include "2015Remote.h"

// CPasswordDlg 对话框

IMPLEMENT_DYNAMIC(CPasswordDlg, CDialogEx)

// 主控程序唯一标识
char g_MasterID[100] = { PWD_HASH256 };

std::string GetPwdHash(){
	return g_MasterID;
}

std::string GetMasterId() {
	static auto id = std::string(g_MasterID).substr(0, 16);
	return id;
}

CPasswordDlg::CPasswordDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_DIALOG_PASSWORD, pParent)
	, m_sDeviceID(_T(""))
	, m_sPassword(_T(""))
{
	m_hIcon = nullptr;
}

CPasswordDlg::~CPasswordDlg()
{
}

void CPasswordDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_EDIT_DEVICEID, m_EditDeviceID);
	DDX_Control(pDX, IDC_EDIT_DEVICEPWD, m_EditPassword);
	DDX_Text(pDX, IDC_EDIT_DEVICEID, m_sDeviceID);
	DDV_MaxChars(pDX, m_sDeviceID, 19);
	DDX_Text(pDX, IDC_EDIT_DEVICEPWD, m_sPassword);
	DDV_MaxChars(pDX, m_sPassword, 37);
}


BEGIN_MESSAGE_MAP(CPasswordDlg, CDialogEx)
END_MESSAGE_MAP()


BOOL CPasswordDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// TODO:  在此添加额外的初始化
	m_hIcon = LoadIcon(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDI_ICON_PASSWORD));
	SetIcon(m_hIcon, FALSE);

	return TRUE;  // return TRUE unless you set the focus to a control
				  // 异常: OCX 属性页应返回 FALSE
}


// CPasswordDlg 消息处理程序

IMPLEMENT_DYNAMIC(CPwdGenDlg, CDialogEx)

CPwdGenDlg::CPwdGenDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_DIALOG_KEYGEN, pParent)
	, m_sDeviceID(_T(""))
	, m_sPassword(_T(""))
	, m_sUserPwd(_T(""))
	, m_ExpireTm(COleDateTime::GetCurrentTime())
	, m_StartTm(COleDateTime::GetCurrentTime())
{

}

CPwdGenDlg::~CPwdGenDlg()
{
}

void CPwdGenDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_EDIT_DEVICEID, m_EditDeviceID);
	DDX_Control(pDX, IDC_EDIT_DEVICEPWD, m_EditPassword);
	DDX_Control(pDX, IDC_EDIT_USERPWD, m_EditUserPwd);
	DDX_Text(pDX, IDC_EDIT_DEVICEID, m_sDeviceID);
	DDV_MaxChars(pDX, m_sDeviceID, 19);
	DDX_Text(pDX, IDC_EDIT_DEVICEPWD, m_sPassword);
	DDV_MaxChars(pDX, m_sPassword, 37);
	DDX_Text(pDX, IDC_EDIT_USERPWD, m_sUserPwd);
	DDV_MaxChars(pDX, m_sUserPwd, 24);
	DDX_Control(pDX, IDC_EXPIRE_DATE, m_PwdExpireDate);
	DDX_DateTimeCtrl(pDX, IDC_EXPIRE_DATE, m_ExpireTm);
	DDX_Control(pDX, IDC_START_DATE, m_StartDate);
	DDX_DateTimeCtrl(pDX, IDC_START_DATE, m_StartTm);
}


BEGIN_MESSAGE_MAP(CPwdGenDlg, CDialogEx)
	ON_BN_CLICKED(IDC_BUTTON_GENKEY, &CPwdGenDlg::OnBnClickedButtonGenkey)
END_MESSAGE_MAP()


void CPwdGenDlg::OnBnClickedButtonGenkey()
{
	// TODO: 在此添加控件通知处理程序代码
	UpdateData(TRUE);
	if (m_sUserPwd.IsEmpty())return;
	std::string pwdHash = hashSHA256(m_sUserPwd.GetString());
	if (pwdHash != GetPwdHash()) {
		Mprintf("hashSHA256 [%s]: %s\n", m_sUserPwd, pwdHash.c_str());
		MessageBoxA("您输入的密码不正确，无法生成口令!", "提示", MB_OK | MB_ICONWARNING);
		return;
	}
	CString strBeginDate = m_StartTm.Format("%Y%m%d");
	CString strEndDate = m_ExpireTm.Format("%Y%m%d");
	// 密码形式：20250209 - 20350209: SHA256
	std::string password = std::string(strBeginDate.GetString()) + " - " + strEndDate.GetBuffer() + ": " + GetPwdHash();
	std::string finalKey = deriveKey(password, m_sDeviceID.GetString());
	std::string fixedKey = strBeginDate.GetString() + std::string("-") + strEndDate.GetBuffer() + std::string("-") +
		getFixedLengthID(finalKey);
	m_EditPassword.SetWindowTextA(fixedKey.c_str());
	std::string hardwareID = getHardwareID();
	std::string hashedID = hashSHA256(hardwareID);
	std::string deviceID = getFixedLengthID(hashedID);
	if (deviceID == m_sDeviceID.GetString()) { // 授权的是当前主控程序
		auto THIS_APP = (CMy2015RemoteApp*)AfxGetApp();
		auto settings = "settings", pwdKey = "Password";
		THIS_APP->m_iniFile.SetStr(settings, pwdKey, fixedKey.c_str());
	}
}


BOOL CPwdGenDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// TODO:  在此添加额外的初始化
	m_hIcon = LoadIcon(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDI_ICON_PASSWORD));
	SetIcon(m_hIcon, FALSE);

	return TRUE;  // return TRUE unless you set the focus to a control
				  // 异常: OCX 属性页应返回 FALSE
}
