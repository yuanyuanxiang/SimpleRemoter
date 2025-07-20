// InputDialog.cpp: 实现文件
//

#include "stdafx.h"
#include "InputDlg.h"
#include "afxdialogex.h"


// CInputDialog 对话框

IMPLEMENT_DYNAMIC(CInputDialog, CDialogEx)

CInputDialog::CInputDialog(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_DIALOG_INPUT, pParent)
	, m_sSecondInput(_T(""))
{
	m_hIcon = NULL;
}

CInputDialog::~CInputDialog()
{
}

void CInputDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_STATIC_SECOND, m_Static2thInput);
	DDX_Control(pDX, IDC_EDIT_SECOND, m_Edit2thInput);
	DDX_Text(pDX, IDC_EDIT_SECOND, m_sSecondInput);
	DDV_MaxChars(pDX, m_sSecondInput, 100);
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

void CInputDialog::Init2(LPCTSTR name, LPCTSTR defaultValue) {
	m_sItemName = name;
	m_sSecondInput = defaultValue;
}

BOOL CInputDialog::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	SetIcon(m_hIcon, FALSE);

	SetWindowText(m_sCaption);
	GetDlgItem(IDC_STATIC)->SetWindowText(m_sPrompt);
	GetDlgItem(IDC_EDIT_FOLDERNAME)->SetWindowText(m_str);
	
	m_Static2thInput.SetWindowTextA(m_sItemName);
	m_Static2thInput.ShowWindow(m_sItemName.IsEmpty() ? SW_HIDE : SW_SHOW);
	m_Edit2thInput.SetWindowTextA(m_sSecondInput);
	m_Edit2thInput.ShowWindow(m_sItemName.IsEmpty() ? SW_HIDE : SW_SHOW);

	return TRUE;  // return TRUE unless you set the focus to a control
				  // 异常: OCX 属性页应返回 FALSE
}


void CInputDialog::OnBnClickedOk()
{
	GetDlgItem(IDC_EDIT_FOLDERNAME)->GetWindowText(m_str);

	CDialogEx::OnOK();
}
