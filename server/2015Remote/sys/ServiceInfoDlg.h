#pragma once
#include "MachineDlg.h"

/////////////////////////////////////////////////////////////////////////////
// CServiceInfoDlg dialog

typedef struct {
    CString strSerName;
    CString strSerDisPlayname;
    CString strSerDescription;
    CString strFilePath;
    CString strSerRunway;
    CString strSerState;
} SERVICEINFO;

class CServiceInfoDlg : public CDialog
{
public:
    CServiceInfoDlg(CWnd* pParent = NULL);

	ClientContext* m_ContextObject;

    enum { IDD = IDD_SERVICE_INFO };
    CComboBox	m_combox_runway;

    SERVICEINFO m_ServiceInfo;
    CMachineDlg* m_MachineDlg;
protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    HICON m_hIcon;
    void SendToken(BYTE bToken);
    virtual BOOL OnInitDialog();
    afx_msg void OnSelchangeComboRunway();
    afx_msg void OnButtonStart();
    afx_msg void OnButtonStop();
    afx_msg void OnButtonPause();
    afx_msg void OnButtonContinue();
    DECLARE_MESSAGE_MAP()
};
