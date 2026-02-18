#pragma once

#include <afx.h>
#include <afxwin.h>
#include <afxcmn.h>
#include "Resource.h"
#include "LangManager.h"
#include <string>
#include <vector>

// 授权状态常量
#define LICENSE_STATUS_ACTIVE   "Active"
#define LICENSE_STATUS_REVOKED  "Revoked"
#define LICENSE_STATUS_EXPIRED  "Expired"

// 授权信息结构体
struct LicenseInfo {
    std::string SerialNumber;
    std::string Passcode;
    std::string HMAC;
    std::string Status;         // Active, Revoked, Expired
    std::string Remark;
    std::string CreateTime;
    std::string IP;
    std::string Location;
    std::string LastActiveTime;
    std::string PendingExpireDate;  // 预设的新过期日期（如 20270221，空表示无预设）
    int PendingHostNum = 0;         // 预设的并发连接数
};

// 续期信息结构体
struct RenewalInfo {
    std::string ExpireDate;     // 新的过期日期（如 20270221）
    int HostNum = 0;
    bool IsValid() const { return !ExpireDate.empty() && HostNum > 0; }
};

class CMy2015RemoteDlg;  // 前向声明

// 授权列表列索引
enum LicenseColumn {
    LIC_COL_ID = 0,
    LIC_COL_SERIAL,
    LIC_COL_STATUS,
    LIC_COL_EXPIRE,
    LIC_COL_REMARK,
    LIC_COL_PASSCODE,
    LIC_COL_HMAC,
    LIC_COL_IP,
    LIC_COL_LOCATION,
    LIC_COL_LASTACTIVE,
    LIC_COL_CREATETIME
};

// CLicenseDlg 对话框
class CLicenseDlg : public CDialogLangEx
{
    DECLARE_DYNAMIC(CLicenseDlg)

public:
    CLicenseDlg(CMy2015RemoteDlg* pParent = nullptr);
    virtual ~CLicenseDlg();

    enum { IDD = IDD_DIALOG_LICENSE };

protected:
    HICON m_hIcon = NULL;
    CMy2015RemoteDlg* m_pParent = nullptr;
    int m_nSortColumn = -1;
    BOOL m_bSortAscending = TRUE;

    virtual void DoDataExchange(CDataExchange* pDX);
    virtual BOOL OnInitDialog();
    virtual void OnCancel();
    virtual void OnOK();

    DECLARE_MESSAGE_MAP()

public:
    CListCtrl m_ListLicense;
    std::vector<LicenseInfo> m_Licenses;

    void LoadLicenses();
    void RefreshList();
    void InitListColumns();
    void ShowAndRefresh();  // 显示并刷新数据
    void SortByColumn(int nColumn, BOOL bAscending);

    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnNMRClickLicenseList(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnColumnClick(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnLicenseRevoke();
    afx_msg void OnLicenseActivate();
    afx_msg void OnLicenseRenewal();
    afx_msg void OnLicenseEditRemark();
    afx_msg void OnLicenseViewIPs();
};

// 获取所有授权信息
std::vector<LicenseInfo> GetAllLicenses();

// 更新授权状态
bool SetLicenseStatus(const std::string& deviceID, const std::string& status);

// 续期管理函数
bool SetPendingRenewal(const std::string& deviceID, const std::string& expireDate, int hostNum);
RenewalInfo GetPendingRenewal(const std::string& deviceID);
bool ClearPendingRenewal(const std::string& deviceID);

// 从 passcode 解析过期日期（第2段，如 20260221）
std::string ParseExpireDateFromPasscode(const std::string& passcode);

// 从 passcode 解析并发连接数（第3段）
int ParseHostNumFromPasscode(const std::string& passcode);

// 设置授权备注
bool SetLicenseRemark(const std::string& deviceID, const std::string& remark);

// IP 列表辅助函数
int GetIPCountFromList(const std::string& ipListStr);
std::string GetFirstIPFromList(const std::string& ipListStr);
std::string FormatIPDisplay(const std::string& ipListStr);  // 格式化显示: "[3] 192.168.1.1, ..."
