// CPasswordDlg.cpp: 实现文件
//

#include "stdafx.h"
#include "CPasswordDlg.h"
#include "afxdialogex.h"
#include "pwd_gen.h"
#include "2015Remote.h"

// CPasswordDlg 对话框

IMPLEMENT_DYNAMIC(CPasswordDlg, CDialogEx)

CPasswordDlg::CPasswordDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_DIALOG_PASSWORD, pParent)
	, m_sDeviceID(_T(""))
	, m_sPassword(_T(""))
{

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
