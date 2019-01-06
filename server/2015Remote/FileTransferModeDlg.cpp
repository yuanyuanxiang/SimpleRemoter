// FileTransferModeDlg.cpp : implementation file
//

#include "stdafx.h"
#include "2015Remote.h"
#include "FileTransferModeDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CFileTransferModeDlg dialog


CFileTransferModeDlg::CFileTransferModeDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CFileTransferModeDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CFileTransferModeDlg)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}


void CFileTransferModeDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CFileTransferModeDlg)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CFileTransferModeDlg, CDialog)
	//{{AFX_MSG_MAP(CFileTransferModeDlg)
	ON_CONTROL_RANGE(BN_CLICKED, IDC_OVERWRITE, IDC_CANCEL, OnEndDialog)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CFileTransferModeDlg message handlers


void CFileTransferModeDlg::OnEndDialog(UINT id)
{
	// TODO: Add your control notification handler code here
	EndDialog(id);
}

BOOL CFileTransferModeDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
	// TODO: Add extra initialization here
	CString	str;
	str.Format("此文件夹已包含一个名为“%s”的文件", m_strFileName);

	for (int i = 0; i < str.GetLength(); i += 120)
	{
		str.Insert(i, "\n");
		i += 1;
	}

	SetDlgItemText(IDC_TIPS, str);
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}
