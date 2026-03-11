#pragma once

#include "resource.h"
#include "LangManager.h"
#include "NotifyConfig.h"

class NotifySettingsDlg : public CDialogLangEx
{
public:
    NotifySettingsDlg(CWnd* pParent = nullptr);
    virtual ~NotifySettingsDlg();

    enum { IDD = IDD_DIALOG_NOTIFY_SETTINGS };

protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    virtual BOOL OnInitDialog();
    virtual void OnOK();

    DECLARE_MESSAGE_MAP()

    afx_msg void OnBnClickedBtnTestEmail();
    afx_msg void OnBnClickedCheckNotifyEnabled();

private:
    void LoadSettings();
    void SaveSettings();
    void UpdateControlStates();
    void PopulateComboBoxes();

private:
    // SMTP controls
    CEdit m_editSmtpServer;
    CEdit m_editSmtpPort;
    CButton m_checkSmtpSSL;
    CEdit m_editSmtpUser;
    CEdit m_editSmtpPass;
    CEdit m_editSmtpRecipient;

    // Rule controls
    CButton m_checkNotifyEnabled;
    CComboBox m_comboNotifyType;
    CComboBox m_comboNotifyColumn;
    CEdit m_editNotifyPattern;

    // Warning label
    CStatic m_staticWarning;

    // Configuration copy
    NotifyConfig m_config;
    bool m_powerShellAvailable;
};
