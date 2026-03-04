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
    : CDialogLang(CFileTransferModeDlg::IDD, pParent)
{
}


void CFileTransferModeDlg::DoDataExchange(CDataExchange* pDX)
{
    __super::DoDataExchange(pDX);
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
    __super::OnInitDialog();

    // 设置对话框标题和控件文本（解决英语系统乱码问题）
    SetWindowText(_TR("确认文件替换"));
    SetDlgItemText(IDC_OVERWRITE, _TR("覆盖"));
    SetDlgItemText(IDC_OVERWRITE_ALL, _TR("全部覆盖"));
    SetDlgItemText(IDC_ADDITION, _TR("继传"));
    SetDlgItemText(IDC_ADDITION_ALL, _TR("全部继传"));
    SetDlgItemText(IDC_JUMP, _TR("跳过"));
    SetDlgItemText(IDC_JUMP_ALL, _TR("全部跳过"));
    SetDlgItemText(IDC_CANCEL, _TR("取消"));

    CString	str;
    str.FormatL(_T("此文件夹已包含一个名为“%s”的文件"), m_strFileName);

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
    // TODO: 在此添加控件通知处理程序代码
    EndDialog(IDC_OVERWRITE);
}


void CFileTransferModeDlg::OnBnClickedOverwriteAll()
{
    // TODO: 在此添加控件通知处理程序代码
    EndDialog(IDC_OVERWRITE_ALL);
}


void CFileTransferModeDlg::OnBnClickedAddition()
{
    // TODO: 在此添加控件通知处理程序代码
    EndDialog(IDC_ADDITION);
}


void CFileTransferModeDlg::OnBnClickedAdditionAll()
{
    // TODO: 在此添加控件通知处理程序代码
    EndDialog(IDC_ADDITION_ALL);
}


void CFileTransferModeDlg::OnBnClickedJump()
{
    // TODO: 在此添加控件通知处理程序代码
    EndDialog(IDC_JUMP);
}


void CFileTransferModeDlg::OnBnClickedJumpAll()
{
    // TODO: 在此添加控件通知处理程序代码
    EndDialog(IDC_JUMP_ALL);
}


void CFileTransferModeDlg::OnBnClickedCancel()
{
    // TODO: 在此添加控件通知处理程序代码
    EndDialog(IDC_CANCEL);
}
