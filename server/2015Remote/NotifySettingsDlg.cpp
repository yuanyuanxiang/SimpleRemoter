#include "stdafx.h"
#include "NotifySettingsDlg.h"
#include "NotifyManager.h"
#include "context.h"

NotifySettingsDlg::NotifySettingsDlg(CWnd* pParent)
    : CDialogLangEx(IDD_DIALOG_NOTIFY_SETTINGS, pParent)
    , m_powerShellAvailable(false)
{
}

NotifySettingsDlg::~NotifySettingsDlg()
{
}

void NotifySettingsDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogLangEx::DoDataExchange(pDX);

    DDX_Control(pDX, IDC_EDIT_SMTP_SERVER, m_editSmtpServer);
    DDX_Control(pDX, IDC_EDIT_SMTP_PORT, m_editSmtpPort);
    DDX_Control(pDX, IDC_CHECK_SMTP_SSL, m_checkSmtpSSL);
    DDX_Control(pDX, IDC_EDIT_SMTP_USER, m_editSmtpUser);
    DDX_Control(pDX, IDC_EDIT_SMTP_PASS, m_editSmtpPass);
    DDX_Control(pDX, IDC_EDIT_SMTP_RECIPIENT, m_editSmtpRecipient);

    DDX_Control(pDX, IDC_CHECK_NOTIFY_ENABLED, m_checkNotifyEnabled);
    DDX_Control(pDX, IDC_COMBO_NOTIFY_TYPE, m_comboNotifyType);
    DDX_Control(pDX, IDC_COMBO_NOTIFY_COLUMN, m_comboNotifyColumn);
    DDX_Control(pDX, IDC_EDIT_NOTIFY_PATTERN, m_editNotifyPattern);

    DDX_Control(pDX, IDC_STATIC_NOTIFY_WARNING, m_staticWarning);
}

BEGIN_MESSAGE_MAP(NotifySettingsDlg, CDialogLangEx)
    ON_BN_CLICKED(IDC_BTN_TEST_EMAIL, &NotifySettingsDlg::OnBnClickedBtnTestEmail)
    ON_BN_CLICKED(IDC_CHECK_NOTIFY_ENABLED, &NotifySettingsDlg::OnBnClickedCheckNotifyEnabled)
END_MESSAGE_MAP()

BOOL NotifySettingsDlg::OnInitDialog()
{
    CDialogLangEx::OnInitDialog();

    // Set translatable text for static controls
    SetDlgItemText(IDC_GROUP_SMTP, _TR(_T("SMTP 配置")));
    SetDlgItemText(IDC_STATIC_SMTP_SERVER, _TR(_T("服务器:")));
    SetDlgItemText(IDC_STATIC_SMTP_PORT, _TR(_T("端口:")));
    SetDlgItemText(IDC_CHECK_SMTP_SSL, _TR(_T("使用 SSL/TLS")));
    SetDlgItemText(IDC_STATIC_SMTP_USER, _TR(_T("用户名:")));
    SetDlgItemText(IDC_STATIC_SMTP_PASS, _TR(_T("密码:")));
    SetDlgItemText(IDC_STATIC_SMTP_HINT, _TR(_T("(Gmail 需使用应用专用密码)")));
    SetDlgItemText(IDC_STATIC_SMTP_RECIPIENT, _TR(_T("收件人:")));
    SetDlgItemText(IDC_BTN_TEST_EMAIL, _TR(_T("测试")));
    SetDlgItemText(IDC_GROUP_NOTIFY_RULE, _TR(_T("通知规则")));
    SetDlgItemText(IDC_CHECK_NOTIFY_ENABLED, _TR(_T("启用通知")));
    SetDlgItemText(IDC_STATIC_TRIGGER, _TR(_T("触发条件:")));
    SetDlgItemText(IDC_STATIC_MATCH_COLUMN, _TR(_T("匹配列:")));
    SetDlgItemText(IDC_STATIC_KEYWORDS, _TR(_T("关键词:")));
    SetDlgItemText(IDC_STATIC_KEYWORDS_HINT, _TR(_T("(多个关键词用分号分隔，匹配任一项即触发通知)")));
    SetDlgItemText(IDC_STATIC_NOTIFY_TIP, _TR(_T("提示: 同一主机 15 分钟内仅通知一次")));

    // Check PowerShell availability
    m_powerShellAvailable = GetNotifyManager().IsPowerShellAvailable();

    // Show warning if PowerShell is not available
    if (!m_powerShellAvailable) {
        m_staticWarning.SetWindowText(_T("Warning: Requires Windows 10 or later with PowerShell 5.1+"));
        // Disable all controls except OK/Cancel
        GetDlgItem(IDC_EDIT_SMTP_SERVER)->EnableWindow(FALSE);
        GetDlgItem(IDC_EDIT_SMTP_PORT)->EnableWindow(FALSE);
        GetDlgItem(IDC_CHECK_SMTP_SSL)->EnableWindow(FALSE);
        GetDlgItem(IDC_EDIT_SMTP_USER)->EnableWindow(FALSE);
        GetDlgItem(IDC_EDIT_SMTP_PASS)->EnableWindow(FALSE);
        GetDlgItem(IDC_EDIT_SMTP_RECIPIENT)->EnableWindow(FALSE);
        GetDlgItem(IDC_BTN_TEST_EMAIL)->EnableWindow(FALSE);
        GetDlgItem(IDC_CHECK_NOTIFY_ENABLED)->EnableWindow(FALSE);
        GetDlgItem(IDC_COMBO_NOTIFY_TYPE)->EnableWindow(FALSE);
        GetDlgItem(IDC_COMBO_NOTIFY_COLUMN)->EnableWindow(FALSE);
        GetDlgItem(IDC_EDIT_NOTIFY_PATTERN)->EnableWindow(FALSE);
    } else {
        m_staticWarning.SetWindowText(_T(""));
    }

    PopulateComboBoxes();
    LoadSettings();
    UpdateControlStates();

    return TRUE;
}

void NotifySettingsDlg::PopulateComboBoxes()
{
    // Trigger type combo (currently only "Host Online")
    m_comboNotifyType.ResetContent();
    m_comboNotifyType.AddString(_TR("主机上线"));
    m_comboNotifyType.SetItemData(0, NOTIFY_TRIGGER_HOST_ONLINE);
    m_comboNotifyType.SetCurSel(0);

    // Column combo - use translatable strings
    m_comboNotifyColumn.ResetContent();
    struct { int id; const TCHAR* name; } columns[] = {
        { 0, _T("IP地址") },
        { 1, _T("地址") },
        { 2, _T("地理位置") },
        { 3, _T("计算机名") },
        { 4, _T("操作系统") },
        { 5, _T("CPU") },
        { 6, _T("摄像头") },
        { 7, _T("延迟") },
        { 8, _T("版本") },
        { 9, _T("安装时间") },
        { 10, _T("活动窗口") },
        { 11, _T("客户端类型") },
    };
    for (const auto& col : columns) {
        CString item;
        item.Format(_T("%d - %s"), col.id, _TR(col.name));
        m_comboNotifyColumn.AddString(item);
        m_comboNotifyColumn.SetItemData(m_comboNotifyColumn.GetCount() - 1, col.id);
    }
    m_comboNotifyColumn.SetCurSel(ONLINELIST_COMPUTER_NAME);  // Default to computer name
}

void NotifySettingsDlg::LoadSettings()
{
    // Get current config from manager
    m_config = GetNotifyManager().GetConfig();

    // SMTP settings
    m_editSmtpServer.SetWindowText(CString(m_config.smtp.server.c_str()));

    CString portStr;
    portStr.Format(_T("%d"), m_config.smtp.port);
    m_editSmtpPort.SetWindowText(portStr);

    m_checkSmtpSSL.SetCheck(m_config.smtp.useSSL ? BST_CHECKED : BST_UNCHECKED);
    m_editSmtpUser.SetWindowText(CString(m_config.smtp.username.c_str()));
    m_editSmtpPass.SetWindowText(CString(m_config.smtp.password.c_str()));
    m_editSmtpRecipient.SetWindowText(CString(m_config.smtp.recipient.c_str()));

    // Rule settings
    const NotifyRule& rule = m_config.GetRule();
    m_checkNotifyEnabled.SetCheck(rule.enabled ? BST_CHECKED : BST_UNCHECKED);

    // Set trigger type (currently only one option)
    m_comboNotifyType.SetCurSel(0);

    // Set column
    if (rule.columnIndex >= 0 && rule.columnIndex < m_comboNotifyColumn.GetCount()) {
        m_comboNotifyColumn.SetCurSel(rule.columnIndex);
    }

    m_editNotifyPattern.SetWindowText(CString(rule.matchPattern.c_str()));
}

void NotifySettingsDlg::SaveSettings()
{
    CString str;

    // SMTP settings
    m_editSmtpServer.GetWindowText(str);
    m_config.smtp.server = CT2A(str, CP_UTF8);

    m_editSmtpPort.GetWindowText(str);
    m_config.smtp.port = _ttoi(str);

    m_config.smtp.useSSL = (m_checkSmtpSSL.GetCheck() == BST_CHECKED);

    m_editSmtpUser.GetWindowText(str);
    m_config.smtp.username = CT2A(str, CP_UTF8);

    m_editSmtpPass.GetWindowText(str);
    m_config.smtp.password = CT2A(str, CP_UTF8);

    m_editSmtpRecipient.GetWindowText(str);
    m_config.smtp.recipient = CT2A(str, CP_UTF8);

    // Rule settings
    NotifyRule& rule = m_config.GetRule();
    rule.enabled = (m_checkNotifyEnabled.GetCheck() == BST_CHECKED);

    int sel = m_comboNotifyType.GetCurSel();
    rule.triggerType = (sel >= 0) ? (NotifyTriggerType)m_comboNotifyType.GetItemData(sel) : NOTIFY_TRIGGER_HOST_ONLINE;

    sel = m_comboNotifyColumn.GetCurSel();
    rule.columnIndex = (sel >= 0) ? (int)m_comboNotifyColumn.GetItemData(sel) : ONLINELIST_COMPUTER_NAME;

    m_editNotifyPattern.GetWindowText(str);
    rule.matchPattern = CT2A(str, CP_UTF8);

    // Update manager config and save
    GetNotifyManager().SetConfig(m_config);
    GetNotifyManager().SaveConfig();
}

void NotifySettingsDlg::UpdateControlStates()
{
    if (!m_powerShellAvailable) return;

    BOOL enabled = (m_checkNotifyEnabled.GetCheck() == BST_CHECKED);
    m_comboNotifyType.EnableWindow(enabled);
    m_comboNotifyColumn.EnableWindow(enabled);
    m_editNotifyPattern.EnableWindow(enabled);
}

void NotifySettingsDlg::OnOK()
{
    SaveSettings();
    CDialogLangEx::OnOK();
}

void NotifySettingsDlg::OnBnClickedBtnTestEmail()
{
    // Temporarily save current UI values to config for testing
    CString str;

    m_editSmtpServer.GetWindowText(str);
    m_config.smtp.server = CT2A(str, CP_UTF8);

    m_editSmtpPort.GetWindowText(str);
    m_config.smtp.port = _ttoi(str);

    m_config.smtp.useSSL = (m_checkSmtpSSL.GetCheck() == BST_CHECKED);

    m_editSmtpUser.GetWindowText(str);
    m_config.smtp.username = CT2A(str, CP_UTF8);

    m_editSmtpPass.GetWindowText(str);
    m_config.smtp.password = CT2A(str, CP_UTF8);

    m_editSmtpRecipient.GetWindowText(str);
    m_config.smtp.recipient = CT2A(str, CP_UTF8);

    // Update manager config temporarily
    NotifyConfig backup = GetNotifyManager().GetConfig();
    NotifyConfig tempConfig = backup;
    tempConfig.smtp = m_config.smtp;
    GetNotifyManager().SetConfig(tempConfig);

    // Disable button during test
    GetDlgItem(IDC_BTN_TEST_EMAIL)->EnableWindow(FALSE);
    SetCursor(LoadCursor(NULL, IDC_WAIT));

    // Send test email (synchronous)
    std::string result = GetNotifyManager().SendTestEmail();

    // Restore config
    GetNotifyManager().SetConfig(backup);

    // Re-enable button
    GetDlgItem(IDC_BTN_TEST_EMAIL)->EnableWindow(TRUE);
    SetCursor(LoadCursor(NULL, IDC_ARROW));

    // Show result
    if (result == "success") {
        MessageBox(_TR(_T("测试邮件发送成功!")), _TR(_T("测试邮件")), MB_OK | MB_ICONINFORMATION);
    } else {
        MessageBox(_TR(_T("测试邮件发送失败，请检查SMTP配置")), _TR(_T("测试邮件")), MB_OK | MB_ICONWARNING);
    }
}

void NotifySettingsDlg::OnBnClickedCheckNotifyEnabled()
{
    UpdateControlStates();
}
