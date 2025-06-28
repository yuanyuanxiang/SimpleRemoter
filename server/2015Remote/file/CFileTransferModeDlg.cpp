// FileTransferModeDlg.cpp : implementation file
//
#include "stdafx.h"
#include "2015Remote.h"
#include "CFileTransferModeDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

/////////////////////////////////////////////////////////////////////////////
// CFileTransferModeDlg dialog

using namespace file;

CFileTransferModeDlg::CFileTransferModeDlg(CWnd* pParent /*=NULL*/)
    : CDialog(CFileTransferModeDlg::IDD, pParent)
{
}


void CFileTransferModeDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(CFileTransferModeDlg, CDialog)
    ON_BN_CLICKED(IDC_OVERWRITE, &CFileTransferModeDlg::OnBnClickedOverwrite)
    ON_BN_CLICKED(IDC_OVERWRITE_ALL, &CFileTransferModeDlg::OnBnClickedOverwriteAll)
    ON_BN_CLICKED(IDC_ADDITION, &CFileTransferModeDlg::OnBnClickedAddition)
    ON_BN_CLICKED(IDC_ADDITION_ALL, &CFileTransferModeDlg::OnBnClickedAdditionAll)
    ON_BN_CLICKED(IDC_JUMP, &CFileTransferModeDlg::OnBnClickedJump)
    ON_BN_CLICKED(IDC_JUMP_ALL, &CFileTransferModeDlg::OnBnClickedJumpAll)
    ON_BN_CLICKED(IDC_CANCEL, &CFileTransferModeDlg::OnBnClickedCancel)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CFileTransferModeDlg message handlers


void CFileTransferModeDlg::OnEndDialog(UINT id)
{
    EndDialog(id);
}

BOOL CFileTransferModeDlg::OnInitDialog()
{
    CDialog::OnInitDialog();

    CString	str;
    str.Format(_T("���ļ����Ѱ���һ����Ϊ��%s�����ļ�"), m_strFileName);

    for (int i = 0; i < str.GetLength(); i += 120) {
        str.Insert(i, _T("\n"));
        i += 1;
    }

    SetDlgItemText(IDC_TIPS, str);
    return TRUE;  // return TRUE unless you set the focus to a control
    // EXCEPTION: OCX Property Pages should return FALSE
}


void CFileTransferModeDlg::OnBnClickedOverwrite()
{
    // TODO: �ڴ���ӿؼ�֪ͨ����������
    EndDialog(IDC_OVERWRITE);
}


void CFileTransferModeDlg::OnBnClickedOverwriteAll()
{
    // TODO: �ڴ���ӿؼ�֪ͨ����������
    EndDialog(IDC_OVERWRITE_ALL);
}


void CFileTransferModeDlg::OnBnClickedAddition()
{
    // TODO: �ڴ���ӿؼ�֪ͨ����������
    EndDialog(IDC_ADDITION);
}


void CFileTransferModeDlg::OnBnClickedAdditionAll()
{
    // TODO: �ڴ���ӿؼ�֪ͨ����������
    EndDialog(IDC_ADDITION_ALL);
}


void CFileTransferModeDlg::OnBnClickedJump()
{
    // TODO: �ڴ���ӿؼ�֪ͨ����������
    EndDialog(IDC_JUMP);
}


void CFileTransferModeDlg::OnBnClickedJumpAll()
{
    // TODO: �ڴ���ӿؼ�֪ͨ����������
    EndDialog(IDC_JUMP_ALL);
}


void CFileTransferModeDlg::OnBnClickedCancel()
{
    // TODO: �ڴ���ӿؼ�֪ͨ����������
    EndDialog(IDC_CANCEL);
}
