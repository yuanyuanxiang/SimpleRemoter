// EditDialog.cpp : ʵ���ļ�
//

#include "stdafx.h"
#include "2015Remote.h"
#include "EditDialog.h"
#include "afxdialogex.h"


// CEditDialog �Ի���

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


// CEditDialog ��Ϣ�������


void CEditDialog::OnBnClickedOk()
{
    // TODO: �ڴ���ӿؼ�֪ͨ����������

    UpdateData(TRUE);
    if (m_EditString.IsEmpty()) {
        MessageBeep(0);
        return;   //���رնԻ���
    }
    CDialog::OnOK();
}
