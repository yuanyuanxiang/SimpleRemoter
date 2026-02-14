// CLicenseDlg.cpp: 实现文件
//

#include "stdafx.h"
#include "CLicenseDlg.h"
#include "afxdialogex.h"
#include "CPasswordDlg.h"
#include "common/iniFile.h"
#include "common/IniParser.h"
#include "2015RemoteDlg.h"
#include "InputDlg.h"
#include <algorithm>

// CLicenseDlg 对话框

IMPLEMENT_DYNAMIC(CLicenseDlg, CDialogEx)

CLicenseDlg::CLicenseDlg(CMy2015RemoteDlg* pParent /*=nullptr*/)
    : CDialogLangEx(IDD_DIALOG_LICENSE, pParent)
    , m_pParent(pParent)
{
}

CLicenseDlg::~CLicenseDlg()
{
}

void CLicenseDlg::DoDataExchange(CDataExchange* pDX)
{
    __super::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_LICENSE_LIST, m_ListLicense);
}

BEGIN_MESSAGE_MAP(CLicenseDlg, CDialogEx)
    ON_WM_SIZE()
    ON_NOTIFY(NM_RCLICK, IDC_LICENSE_LIST, &CLicenseDlg::OnNMRClickLicenseList)
    ON_NOTIFY(LVN_COLUMNCLICK, IDC_LICENSE_LIST, &CLicenseDlg::OnColumnClick)
    ON_COMMAND(ID_LICENSE_REVOKE, &CLicenseDlg::OnLicenseRevoke)
    ON_COMMAND(ID_LICENSE_ACTIVATE, &CLicenseDlg::OnLicenseActivate)
    ON_COMMAND(ID_LICENSE_RENEWAL, &CLicenseDlg::OnLicenseRenewal)
    ON_COMMAND(ID_LICENSE_EDIT_REMARK, &CLicenseDlg::OnLicenseEditRemark)
END_MESSAGE_MAP()

// 获取所有授权信息
std::vector<LicenseInfo> GetAllLicenses()
{
    std::vector<LicenseInfo> licenses;
    std::string iniPath = GetLicensesPath();

    CIniParser parser;
    if (!parser.LoadFile(iniPath.c_str()))
        return licenses;

    const auto& sections = parser.GetAllSections();
    for (const auto& sec : sections) {
        const std::string& sectionName = sec.first;
        const auto& kv = sec.second;

        LicenseInfo info;
        info.SerialNumber = sectionName;

        auto it = kv.find("Passcode");
        if (it != kv.end()) info.Passcode = it->second;

        it = kv.find("HMAC");
        if (it != kv.end()) info.HMAC = it->second;

        it = kv.find("Remark");
        if (it != kv.end()) info.Remark = it->second;

        it = kv.find("CreateTime");
        if (it != kv.end()) info.CreateTime = it->second;

        it = kv.find("IP");
        if (it != kv.end()) info.IP = it->second;

        it = kv.find("Location");
        if (it != kv.end()) info.Location = it->second;

        it = kv.find("LastActiveTime");
        if (it != kv.end()) info.LastActiveTime = it->second;

        it = kv.find("Status");
        if (it != kv.end()) info.Status = it->second;
        else info.Status = LICENSE_STATUS_ACTIVE;  // 默认为有效

        it = kv.find("PendingExpireDate");
        if (it != kv.end()) info.PendingExpireDate = it->second;

        it = kv.find("PendingHostNum");
        if (it != kv.end()) info.PendingHostNum = atoi(it->second.c_str());

        licenses.push_back(info);
    }

    return licenses;
}

void CLicenseDlg::InitListColumns()
{
    m_ListLicense.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_LABELTIP | LVS_EX_INFOTIP);

    m_ListLicense.InsertColumnL(0, _T("ID"), LVCFMT_LEFT, 35);
    m_ListLicense.InsertColumnL(1, _T("序列号"), LVCFMT_LEFT, 125);
    m_ListLicense.InsertColumnL(2, _T("状态"), LVCFMT_LEFT, 60);
    m_ListLicense.InsertColumnL(3, _T("到期时间"), LVCFMT_LEFT, 80);
    m_ListLicense.InsertColumnL(4, _T("备注"), LVCFMT_LEFT, 120);
    m_ListLicense.InsertColumnL(5, _T("口令"), LVCFMT_LEFT, 260);
    m_ListLicense.InsertColumnL(6, _T("验证码"), LVCFMT_LEFT, 135);
    m_ListLicense.InsertColumnL(7, _T("IP"), LVCFMT_LEFT, 120);
    m_ListLicense.InsertColumnL(8, _T("位置"), LVCFMT_LEFT, 100);
    m_ListLicense.InsertColumnL(9, _T("最后活跃"), LVCFMT_LEFT, 130);
    m_ListLicense.InsertColumnL(10, _T("创建时间"), LVCFMT_LEFT, 130);
}

void CLicenseDlg::LoadLicenses()
{
    m_Licenses = GetAllLicenses();
}

// 比较两个授权信息
static int CompareLicense(const LicenseInfo& a, const LicenseInfo& b, int nColumn)
{
    switch (nColumn) {
    case LIC_COL_SERIAL:
        return a.SerialNumber.compare(b.SerialNumber);
    case LIC_COL_STATUS:
        return a.Status.compare(b.Status);
    case LIC_COL_EXPIRE:
        return a.PendingExpireDate.compare(b.PendingExpireDate);
    case LIC_COL_REMARK:
        return a.Remark.compare(b.Remark);
    case LIC_COL_PASSCODE:
        return a.Passcode.compare(b.Passcode);
    case LIC_COL_HMAC:
        return a.HMAC.compare(b.HMAC);
    case LIC_COL_IP:
        return a.IP.compare(b.IP);
    case LIC_COL_LOCATION:
        return a.Location.compare(b.Location);
    case LIC_COL_LASTACTIVE:
        return a.LastActiveTime.compare(b.LastActiveTime);
    case LIC_COL_CREATETIME:
        return a.CreateTime.compare(b.CreateTime);
    default:
        return 0;
    }
}

void CLicenseDlg::RefreshList()
{
    m_ListLicense.DeleteAllItems();

    // 创建索引列表用于排序
    std::vector<size_t> indices(m_Licenses.size());
    for (size_t i = 0; i < indices.size(); ++i) {
        indices[i] = i;
    }

    // 如果有排序列，进行排序
    if (m_nSortColumn >= 0) {
        int sortCol = m_nSortColumn;
        BOOL ascending = m_bSortAscending;
        std::sort(indices.begin(), indices.end(),
            [this, sortCol, ascending](size_t a, size_t b) {
                int result = CompareLicense(m_Licenses[a], m_Licenses[b], sortCol);
                return ascending ? (result < 0) : (result > 0);
            });
    }

    for (size_t i = 0; i < indices.size(); ++i) {
        const auto& lic = m_Licenses[indices[i]];
        CString strID;
        strID.Format(_T("%d"), (int)(i + 1));
        int idx = m_ListLicense.InsertItem((int)i, strID);
        m_ListLicense.SetItemData(idx, indices[i]);  // 保存原始索引

        m_ListLicense.SetItemText(idx, 1, lic.SerialNumber.c_str());
        m_ListLicense.SetItemText(idx, 2, lic.Status.c_str());

        // 显示到期时间
        CString strPending;
        if (!lic.PendingExpireDate.empty()) {
            // 格式化显示：20270221 -> 2027-02-21
            std::string d = lic.PendingExpireDate;
            if (d.length() == 8) {
                strPending.Format(_T("%s-%s-%s"), d.substr(0, 4).c_str(),
                    d.substr(4, 2).c_str(), d.substr(6, 2).c_str());
            } else {
                strPending = d.c_str();
            }
        }
        m_ListLicense.SetItemText(idx, 3, strPending);

        m_ListLicense.SetItemText(idx, 4, lic.Remark.c_str());
        m_ListLicense.SetItemText(idx, 5, lic.Passcode.c_str());
        m_ListLicense.SetItemText(idx, 6, lic.HMAC.c_str());
        m_ListLicense.SetItemText(idx, 7, lic.IP.c_str());
        m_ListLicense.SetItemText(idx, 8, lic.Location.c_str());
        m_ListLicense.SetItemText(idx, 9, lic.LastActiveTime.c_str());
        m_ListLicense.SetItemText(idx, 10, lic.CreateTime.c_str());
    }
}

void CLicenseDlg::OnColumnClick(NMHDR* pNMHDR, LRESULT* pResult)
{
    LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
    int nColumn = pNMLV->iSubItem;

    // ID列不排序
    if (nColumn == LIC_COL_ID) {
        *pResult = 0;
        return;
    }

    // 点击同一列切换排序方向
    if (nColumn == m_nSortColumn) {
        m_bSortAscending = !m_bSortAscending;
    } else {
        m_nSortColumn = nColumn;
        m_bSortAscending = TRUE;
    }

    SortByColumn(nColumn, m_bSortAscending);
    *pResult = 0;
}

void CLicenseDlg::SortByColumn(int /*nColumn*/, BOOL /*bAscending*/)
{
    m_ListLicense.SetRedraw(FALSE);
    RefreshList();
    m_ListLicense.SetRedraw(TRUE);
    m_ListLicense.Invalidate();
}

BOOL CLicenseDlg::OnInitDialog()
{
    __super::OnInitDialog();

    m_hIcon = LoadIcon(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDI_ICON_PASSWORD));
    SetIcon(m_hIcon, FALSE);
    SetIcon(m_hIcon, TRUE);

    InitListColumns();
    LoadLicenses();
    RefreshList();

    return TRUE;
}

void CLicenseDlg::OnSize(UINT nType, int cx, int cy)
{
    __super::OnSize(nType, cx, cy);

    if (m_ListLicense.GetSafeHwnd()) {
        m_ListLicense.MoveWindow(7, 7, cx - 14, cy - 14);
    }
}

// 更新授权状态
bool SetLicenseStatus(const std::string& deviceID, const std::string& status)
{
    std::string iniPath = GetLicensesPath();
    config cfg(iniPath);

    // 检查授权是否存在
    std::string existingPasscode = cfg.GetStr(deviceID, "Passcode", "");
    if (existingPasscode.empty()) {
        return false;  // 授权不存在
    }

    cfg.SetStr(deviceID, "Status", status);
    return true;
}

void CLicenseDlg::OnNMRClickLicenseList(NMHDR* pNMHDR, LRESULT* pResult)
{
    LPNMITEMACTIVATE pNMItemActivate = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);
    *pResult = 0;

    int nItem = pNMItemActivate->iItem;
    if (nItem < 0)
        return;

    size_t nIndex = (size_t)m_ListLicense.GetItemData(nItem);
    if (nIndex >= m_Licenses.size())
        return;

    // 创建弹出菜单
    CMenu menu;
    menu.CreatePopupMenu();

    const auto& lic = m_Licenses[nIndex];
    menu.AppendMenuL(MF_STRING, ID_LICENSE_RENEWAL, _T("预设续期(&N)..."));
    menu.AppendMenuL(MF_STRING, ID_LICENSE_EDIT_REMARK, _T("编辑备注(&E)..."));
    menu.AppendMenuL(MF_SEPARATOR, 0, _T(""));
    if (lic.Status == LICENSE_STATUS_ACTIVE) {
        menu.AppendMenuL(MF_STRING, ID_LICENSE_REVOKE, _T("撤销授权(&R)"));
    } else {
        menu.AppendMenuL(MF_STRING, ID_LICENSE_ACTIVATE, _T("激活授权(&A)"));
    }

    // 显示菜单
    CPoint point;
    GetCursorPos(&point);
    menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, point.x, point.y, this);
}

void CLicenseDlg::OnLicenseRevoke()
{
    int nItem = m_ListLicense.GetNextItem(-1, LVNI_SELECTED);
    if (nItem < 0)
        return;

    size_t nIndex = (size_t)m_ListLicense.GetItemData(nItem);
    if (nIndex >= m_Licenses.size())
        return;

    const auto& lic = m_Licenses[nIndex];
    if (SetLicenseStatus(lic.SerialNumber, LICENSE_STATUS_REVOKED)) {
        m_Licenses[nIndex].Status = LICENSE_STATUS_REVOKED;
        m_ListLicense.SetItemText(nItem, LIC_COL_STATUS, LICENSE_STATUS_REVOKED);
    }
}

void CLicenseDlg::OnLicenseActivate()
{
    int nItem = m_ListLicense.GetNextItem(-1, LVNI_SELECTED);
    if (nItem < 0)
        return;

    size_t nIndex = (size_t)m_ListLicense.GetItemData(nItem);
    if (nIndex >= m_Licenses.size())
        return;

    const auto& lic = m_Licenses[nIndex];
    if (SetLicenseStatus(lic.SerialNumber, LICENSE_STATUS_ACTIVE)) {
        m_Licenses[nIndex].Status = LICENSE_STATUS_ACTIVE;
        m_ListLicense.SetItemText(nItem, LIC_COL_STATUS, LICENSE_STATUS_ACTIVE);
    }
}

void CLicenseDlg::OnCancel()
{
    // 隐藏窗口而不是关闭
    ShowWindow(SW_HIDE);
}

void CLicenseDlg::OnOK()
{
    // 不做任何操作，防止回车关闭对话框
}

void CLicenseDlg::ShowAndRefresh()
{
    // 重新加载数据并显示
    LoadLicenses();
    RefreshList();
    ShowWindow(SW_SHOW);
    SetForegroundWindow();
}

// 从 passcode 解析过期日期（第2段，如 20260221）
std::string ParseExpireDateFromPasscode(const std::string& passcode)
{
    size_t pos1 = passcode.find('-');
    if (pos1 == std::string::npos) return "";

    size_t pos2 = passcode.find('-', pos1 + 1);
    if (pos2 == std::string::npos) return "";

    return passcode.substr(pos1 + 1, pos2 - pos1 - 1);
}

// 从 passcode 解析并发连接数（第3段）
int ParseHostNumFromPasscode(const std::string& passcode)
{
    size_t pos1 = passcode.find('-');
    if (pos1 == std::string::npos) return 0;

    size_t pos2 = passcode.find('-', pos1 + 1);
    if (pos2 == std::string::npos) return 0;

    size_t pos3 = passcode.find('-', pos2 + 1);
    if (pos3 == std::string::npos) return 0;

    std::string numStr = passcode.substr(pos2 + 1, pos3 - pos2 - 1);
    return atoi(numStr.c_str());
}

// 设置待续期信息
bool SetPendingRenewal(const std::string& deviceID, const std::string& expireDate, int hostNum)
{
    std::string iniPath = GetLicensesPath();
    config cfg(iniPath);

    // 检查授权是否存在
    std::string existingPasscode = cfg.GetStr(deviceID, "Passcode", "");
    if (existingPasscode.empty()) {
        return false;
    }

    cfg.SetStr(deviceID, "PendingExpireDate", expireDate);
    cfg.SetInt(deviceID, "PendingHostNum", hostNum);
    return true;
}

// 获取待续期信息
RenewalInfo GetPendingRenewal(const std::string& deviceID)
{
    RenewalInfo info;
    std::string iniPath = GetLicensesPath();
    config cfg(iniPath);

    info.ExpireDate = cfg.GetStr(deviceID, "PendingExpireDate", "");
    info.HostNum = cfg.GetInt(deviceID, "PendingHostNum", 0);
    return info;
}

// 清除待续期信息
bool ClearPendingRenewal(const std::string& deviceID)
{
    std::string iniPath = GetLicensesPath();
    config cfg(iniPath);

    cfg.SetStr(deviceID, "PendingExpireDate", "");
    cfg.SetInt(deviceID, "PendingHostNum", 0);
    return true;
}

void CLicenseDlg::OnLicenseRenewal()
{
    int nItem = m_ListLicense.GetNextItem(-1, LVNI_SELECTED);
    if (nItem < 0)
        return;

    size_t nIndex = (size_t)m_ListLicense.GetItemData(nItem);
    if (nIndex >= m_Licenses.size())
        return;

    const auto& lic = m_Licenses[nIndex];

    // 从 passcode 解析当前过期日期和并发连接数
    std::string currentExpireDate = ParseExpireDateFromPasscode(lic.Passcode);
    int defaultHostNum = lic.PendingHostNum > 0 ? lic.PendingHostNum : ParseHostNumFromPasscode(lic.Passcode);
    if (defaultHostNum <= 0) defaultHostNum = 100;

    // 格式化当前过期日期显示
    CString strCurrentExpire;
    if (currentExpireDate.length() == 8) {
        strCurrentExpire.Format(_T("当前到期: %s-%s-%s"),
            currentExpireDate.substr(0, 4).c_str(),
            currentExpireDate.substr(4, 2).c_str(),
            currentExpireDate.substr(6, 2).c_str());
    } else {
        strCurrentExpire = _T("当前到期: 未知");
    }

    // 使用输入对话框获取续期信息
    CInputDialog dlg(this);
    CString strTitle;
    strTitle.Format(_T("预设续期 (%s)"), strCurrentExpire);
    dlg.Init(strTitle, _L("续期天数:"));
    dlg.Init2(_L("并发连接数:"), std::to_string(defaultHostNum).c_str());

    if (dlg.DoModal() != IDOK)
        return;

    int days = atoi(dlg.m_str);
    int hostNum = atoi(dlg.m_sSecondInput);

    if (days <= 0 || hostNum <= 0) {
        MessageBoxL("请输入有效的天数和并发连接数!", "错误", MB_ICONWARNING);
        return;
    }

    // 计算新的过期日期 (从原到期日期开始加天数)
    CTime baseTime;
    if (currentExpireDate.length() == 8) {
        int year = atoi(currentExpireDate.substr(0, 4).c_str());
        int month = atoi(currentExpireDate.substr(4, 2).c_str());
        int day = atoi(currentExpireDate.substr(6, 2).c_str());
        baseTime = CTime(year, month, day, 0, 0, 0);
    } else {
        baseTime = CTime::GetCurrentTime();  // 无法解析时用当前日期
    }
    CTimeSpan span(days, 0, 0, 0);
    CTime newExpireTime = baseTime + span;
    CString strNewExpireDate;
    strNewExpireDate.Format(_T("%04d%02d%02d"),
        newExpireTime.GetYear(), newExpireTime.GetMonth(), newExpireTime.GetDay());
    std::string newExpireDate = CT2A(strNewExpireDate);

    if (SetPendingRenewal(lic.SerialNumber, newExpireDate, hostNum)) {
        m_Licenses[nIndex].PendingExpireDate = newExpireDate;
        m_Licenses[nIndex].PendingHostNum = hostNum;

        // 显示格式化的新过期日期
        CString strPending;
        strPending.Format(_T("%s-%s-%s"),
            newExpireDate.substr(0, 4).c_str(),
            newExpireDate.substr(4, 2).c_str(),
            newExpireDate.substr(6, 2).c_str());
        m_ListLicense.SetItemText(nItem, LIC_COL_EXPIRE, strPending);

        CString msg;
        msg.Format(_T("已预设续期至: %s\n并发连接数: %d\n客户端上线时将自动下发新授权"),
            strPending, hostNum);
        MessageBoxL(msg, "预设成功", MB_ICONINFORMATION);
    }
}

// 设置授权备注
bool SetLicenseRemark(const std::string& deviceID, const std::string& remark)
{
    std::string iniPath = GetLicensesPath();
    config cfg(iniPath);

    // 检查授权是否存在
    std::string existingPasscode = cfg.GetStr(deviceID, "Passcode", "");
    if (existingPasscode.empty()) {
        return false;
    }

    cfg.SetStr(deviceID, "Remark", remark);
    return true;
}

void CLicenseDlg::OnLicenseEditRemark()
{
    int nItem = m_ListLicense.GetNextItem(-1, LVNI_SELECTED);
    if (nItem < 0)
        return;

    size_t nIndex = (size_t)m_ListLicense.GetItemData(nItem);
    if (nIndex >= m_Licenses.size())
        return;

    const auto& lic = m_Licenses[nIndex];

    // 使用输入对话框编辑备注
    CInputDialog dlg(this);
    dlg.Init(_L("编辑备注"), _L("备注:"));
    dlg.m_str = lic.Remark.c_str();

    if (dlg.DoModal() != IDOK)
        return;

    std::string newRemark = CT2A(dlg.m_str);
    if (SetLicenseRemark(lic.SerialNumber, newRemark)) {
        m_Licenses[nIndex].Remark = newRemark;
        m_ListLicense.SetItemText(nItem, LIC_COL_REMARK, newRemark.c_str());
    }
}
