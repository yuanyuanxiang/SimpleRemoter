// InputDialog.cpp: 实现文件
//

#include "stdafx.h"
#include "InputDlg.h"
#include "afxdialogex.h"


// CInputDialog 对话框

IMPLEMENT_DYNAMIC(CInputDialog, CDialogEx)

CInputDialog::CInputDialog(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_DIALOG_INPUT, pParent)
{
	m_hIcon = NULL;
}

CInputDialog::~CInputDialog()
{
}

void CInputDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(CInputDialog, CDialogEx)
	ON_BN_CLICKED(IDOK, &CInputDialog::OnBnClickedOk)
END_MESSAGE_MAP()


// CInputDialog 消息处理程序
BOOL CInputDialog::Init(LPCTSTR caption, LPCTSTR prompt) {
	m_sCaption = caption;
	m_sPrompt = prompt;

	return TRUE;
}

BOOL CInputDialog::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	SetIcon(m_hIcon, FALSE);

	SetWindowText(m_sCaption);
	GetDlgItem(IDC_STATIC)->SetWindowText(m_sPrompt);

	return TRUE;  // return TRUE unless you set the focus to a control
				  // 异常: OCX 属性页应返回 FALSE
}


void CInputDialog::OnBnClickedOk()
{
	GetDlgItem(IDC_EDIT_FOLDERNAME)->GetWindowText(m_str);

	CDialogEx::OnOK();
}
