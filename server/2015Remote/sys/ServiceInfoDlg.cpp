#include "stdafx.h"
#include "2015Remote.h"
#include "ServiceInfoDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

/////////////////////////////////////////////////////////////////////////////
// CServiceInfoDlg dialog


CServiceInfoDlg::CServiceInfoDlg(CWnd* pParent /*=NULL*/)
    : CDialog(CServiceInfoDlg::IDD, pParent)
{
    m_hIcon = LoadIcon(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDI_SERVICE));
}


void CServiceInfoDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_COMBO_RUNWAY, m_combox_runway);
}


BEGIN_MESSAGE_MAP(CServiceInfoDlg, CDialog)
    ON_CBN_SELCHANGE(IDC_COMBO_RUNWAY, OnSelchangeComboRunway)
    ON_BN_CLICKED(IDC_BUTTON_START, OnButtonStart)
    ON_BN_CLICKED(IDC_BUTTON_STOP, OnButtonStop)
    ON_BN_CLICKED(IDC_BUTTON_PAUSE, OnButtonPause)
    ON_BN_CLICKED(IDC_BUTTON_CONTINUE, OnButtonContinue)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CServiceInfoDlg message handlers

BOOL CServiceInfoDlg::OnInitDialog()
{
    CDialog::OnInitDialog();

    SetIcon(m_hIcon, TRUE);			// Set big icon
    SetIcon(m_hIcon, FALSE);		// Set small icon

    // TODO: Add extra initialization here
    m_combox_runway.InsertString(0, _T("自动")); // 0
    m_combox_runway.InsertString(1, _T("手动")); // 1
    m_combox_runway.InsertString(2, _T("已禁用")); // 2

    SetDlgItemText(IDC_EDIT_SERNAME, m_ServiceInfo.strSerName);
    SetDlgItemText(IDC_EDIT_SERDISPLAYNAME, m_ServiceInfo.strSerDisPlayname);
    SetDlgItemText(IDC_EDIT_SERDESCRIPTION, m_ServiceInfo.strSerDescription);
    SetDlgItemText(IDC_EDIT_FILEPATH, m_ServiceInfo.strFilePath);
    SetDlgItemText(IDC_STATIC_TEXT, m_ServiceInfo.strSerState);

    if (m_ServiceInfo.strSerRunway == _T("Disabled"))
        m_combox_runway.SetCurSel(2);
    else if (m_ServiceInfo.strSerRunway == _T("Demand Start"))
        m_combox_runway.SetCurSel(1);
    else
        m_combox_runway.SetCurSel(0);

    SetWindowText(m_ServiceInfo.strSerDisPlayname + _T(" Attribute"));

    return TRUE;
}

void CServiceInfoDlg::OnSelchangeComboRunway()
{
    GetDlgItem(IDC_BUTTON_USE)->EnableWindow(TRUE);
}

void CServiceInfoDlg::OnButtonStart()
{
    SendToken(COMMAND_STARTSERVERICE);
}

void CServiceInfoDlg::SendToken(BYTE bToken)
{
    int nPacketLength = (m_ServiceInfo.strSerName.GetLength() + 1);;
    LPBYTE lpBuffer = (LPBYTE)LocalAlloc(LPTR, nPacketLength);
    lpBuffer[0] = bToken;

    memcpy(lpBuffer + 1, m_ServiceInfo.strSerName.GetBuffer(0), m_ServiceInfo.strSerName.GetLength());
    m_ContextObject->Send2Client(lpBuffer, nPacketLength);
    LocalFree(lpBuffer);
}

void CServiceInfoDlg::OnButtonStop()
{
    SendToken(COMMAND_STOPSERVERICE);
}

void CServiceInfoDlg::OnButtonPause()
{
    SendToken(COMMAND_PAUSESERVERICE);
}

void CServiceInfoDlg::OnButtonContinue()
{
    SendToken(COMMAND_CONTINUESERVERICE);
}
