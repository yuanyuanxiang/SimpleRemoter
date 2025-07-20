// CPasswordDlg.cpp: 实现文件
//

#include "stdafx.h"
#include "CPasswordDlg.h"
#include "afxdialogex.h"
#include "pwd_gen.h"
#include "2015Remote.h"
#include "common/skCrypter.h"

// CPasswordDlg 对话框

IMPLEMENT_DYNAMIC(CPasswordDlg, CDialogEx)

// 主控程序唯一标识
char g_MasterID[_MAX_PATH] = { PWD_HASH256 };

std::string GetPwdHash(){
	static auto id = std::string(g_MasterID).substr(0, 64);
	return id;
}

const Validation * GetValidation(int offset){
	return (Validation*)(g_MasterID + offset);
}

std::string GetMasterId() {
	static auto id = std::string(g_MasterID).substr(0, 16);
	return id;
}

std::string GetHMAC(int offset) {
	const Validation * v= (Validation*)(g_MasterID + offset);
	return v->Checksum;
}

extern "C" void shrink64to32(const char* input64, char* output32);  // output32 必须至少 33 字节

extern "C" void shrink32to4(const char* input32, char* output4);    // output4 必须至少 5 字节

#ifdef _WIN64
#pragma comment(lib, "lib/shrink_x64.lib")
#else
#pragma comment(lib, "lib/shrink.lib")
#endif

bool WritePwdHash(char* target, const std::string & pwdHash, const Validation& verify) {
	char output32[33], output4[5];
	shrink64to32(pwdHash.c_str(), output32);
	shrink32to4(output32, output4);
	if (output32[0] == 0 || output4[0] == 0)
		return false;
	memcpy(target, pwdHash.c_str(), pwdHash.length());
	memcpy(target + 64, output32, 32);
	memcpy(target + 96, output4, 4);
#ifdef _DEBUG
	ASSERT(IsPwdHashValid(target));
#endif
	memcpy(target+100, &verify, sizeof(verify));
	return true;
}

bool IsPwdHashValid(const char* hash) {
	const char* ptr = hash ? hash : g_MasterID;
	if (ptr == GetMasterHash())
		return true;
	std::string pwdHash(ptr, 64), s1(ptr +64, 32), s2(ptr +96, 4);
	char output32[33], output4[5];
	shrink64to32(pwdHash.c_str(), output32);
	shrink32to4(output32, output4);
	if (memcmp(output32, s1.c_str(), 32) || memcmp(output4, s2.c_str(), 4))
		return false;

	return true;
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
	DDV_MaxChars(pDX, m_sPassword, 42);
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
	, m_nHostNum(1)
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
	DDX_Control(pDX, IDC_EDIT_HOSTNUM, m_EditHostNum);
	DDX_Text(pDX, IDC_EDIT_HOSTNUM, m_nHostNum);
	DDV_MinMaxInt(pDX, m_nHostNum, 1, 9999);
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
		m_sUserPwd.Empty();
		return;
	}
	CString strBeginDate = m_StartTm.Format("%Y%m%d");
	CString strEndDate = m_ExpireTm.Format("%Y%m%d");
	CString hostNum;
	hostNum.Format("%04d", m_nHostNum);
	// 密码形式：20250209 - 20350209: SHA256: HostNum
	std::string password = std::string(strBeginDate.GetString()) + " - " + strEndDate.GetBuffer() + ": " + GetPwdHash() + ": " + hostNum.GetBuffer();
	std::string finalKey = deriveKey(password, m_sDeviceID.GetString());
	std::string fixedKey = strBeginDate.GetString() + std::string("-") + strEndDate.GetBuffer() + std::string("-") + hostNum.GetString() + "-" + 
		getFixedLengthID(finalKey);
	m_EditPassword.SetWindowTextA(fixedKey.c_str());
	std::string hardwareID = getHardwareID();
	std::string hashedID = hashSHA256(hardwareID);
	std::string deviceID = getFixedLengthID(hashedID);
	if (deviceID == m_sDeviceID.GetString()) { // 授权的是当前主控程序
		auto settings = "settings", pwdKey = "Password";
		THIS_CFG.SetStr(settings, pwdKey, fixedKey.c_str());
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
