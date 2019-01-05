// EditDialog.cpp : 实现文件
//

#include "stdafx.h"
#include "2015Remote.h"
#include "EditDialog.h"
#include "afxdialogex.h"


// CEditDialog 对话框

IMPLEMENT_DYNAMIC(CEditDialog, CDialog)

CEditDialog::CEditDialog(CWnd* pParent)
	: CDialog(CEditDialog::IDD, pParent)
	, m_EditString(_T(""))
{

}

CEditDialog::~CEditDialog()
{
}

void CEditDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDIT_STRING, m_EditString);
}


BEGIN_MESSAGE_MAP(CEditDialog, CDialog)
	ON_BN_CLICKED(IDOK, &CEditDialog::OnBnClickedOk)
END_MESSAGE_MAP()


// CEditDialog 消息处理程序


void CEditDialog::OnBnClickedOk()
{
	// TODO: 在此添加控件通知处理程序代码

	UpdateData(TRUE);
	if (m_EditString.IsEmpty()) {
		MessageBeep(0);
		return;   //不关闭对话框
	}
	CDialog::OnOK();
}
