// FileCompress.cpp : 实现文件
//

#include "stdafx.h"
#include "2015Remote.h"
#include "FileCompress.h"
#include "afxdialogex.h"

// CFileCompress 对话框

IMPLEMENT_DYNAMIC(CFileCompress, CDialog)

CFileCompress::CFileCompress(CWnd* pParent, ULONG n)
	: CDialog(CFileCompress::IDD, pParent)
	, m_EditRarName(_T(""))
{
	m_ulType = n;
}

CFileCompress::~CFileCompress()
{
}

void CFileCompress::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDIT_RARNAME, m_EditRarName);
}


BEGIN_MESSAGE_MAP(CFileCompress, CDialog)
END_MESSAGE_MAP()


// CFileCompress 消息处理程序


BOOL CFileCompress::OnInitDialog()
{
	CDialog::OnInitDialog();

	CString strTips;
	switch(m_ulType)
	{
	case 1:
		{
			strTips = "请输入压缩文件名：";
			SetDlgItemText(IDC_STATIC, strTips);
			break;
		}
	case 2:
		{
			strTips = "请输入解压文件夹：";
			SetDlgItemText(IDC_STATIC, strTips);
			break;
		}
	case 3:
		{
			strTips = "请输入远程压缩文件名：";
			SetDlgItemText(IDC_STATIC, strTips);
			break;
		}
	case 4:
		{
			strTips = "请输入远程解压文件夹：";
			SetDlgItemText(IDC_STATIC, strTips);
		}
	}

	UpdateData(TRUE);

	return TRUE;  // return TRUE unless you set the focus to a control
	// 异常: OCX 属性页应返回 FALSE
}
