// FileTransferModeDlg.cpp : 实现文件
//

#include "stdafx.h"
#include "2015Remote.h"
#include "FileTransferModeDlg.h"
#include "afxdialogex.h"


// CFileTransferModeDlg 对话框

IMPLEMENT_DYNAMIC(CFileTransferModeDlg, CDialog)

CFileTransferModeDlg::CFileTransferModeDlg(CWnd* pParent)
	: CDialog(CFileTransferModeDlg::IDD, pParent)
{

}

CFileTransferModeDlg::~CFileTransferModeDlg()
{
}

void CFileTransferModeDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(CFileTransferModeDlg, CDialog)
	ON_CONTROL_RANGE(BN_CLICKED, IDC_OVERWRITE, IDC_JUMP_ALL, OnEndDialog) 
END_MESSAGE_MAP()


// CFileTransferModeDlg 消息处理程序


VOID CFileTransferModeDlg::OnEndDialog(UINT id)
{
	// TODO: Add your control notification handler code here
	EndDialog(id);
}

BOOL CFileTransferModeDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	CString	strTips;
	strTips.Format("衰了 咋办.... \" %s \" ", m_strFileName);

	for (int i = 0; i < strTips.GetLength(); i += 120)
	{
		strTips.Insert(i, "\n");
		i += 1;
	}

	SetDlgItemText(IDC_TIP, strTips);
	return TRUE;  // return TRUE unless you set the focus to a control
	// 异常: OCX 属性页应返回 FALSE
}
