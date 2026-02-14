#pragma once

#include <afx.h>
#include <afxwin.h>
#include "Resource.h"
#include "common/commands.h"
#include "LangManager.h"

// CPasswordDlg 对话框

// 获取密码哈希值
std::string GetPwdHash();

const Validation* GetValidation(int offset=100);

std::string GetMasterId();

std::string GetHMAC(int offset=100);

void SetHMAC(const std::string str, int offset=100);

bool IsPwdHashValid(const char* pwdHash = nullptr);

bool WritePwdHash(char* target, const std::string& pwdHash, const Validation &verify);

class CPasswordDlg : public CDialogLangEx
{
    DECLARE_DYNAMIC(CPasswordDlg)

public:
    CPasswordDlg(CWnd* pParent = nullptr);   // 标准构造函数
    virtual ~CPasswordDlg();

    enum { IDD = IDD_DIALOG_PASSWORD };

protected:
    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

    DECLARE_MESSAGE_MAP()
public:
    HICON m_hIcon;
    CEdit m_EditDeviceID;
    CEdit m_EditPassword;
    CString m_sDeviceID;
    CString m_sPassword;
    virtual BOOL OnInitDialog();
    CComboBox m_ComboBinding;
    afx_msg void OnCbnSelchangeComboBind();
    CEdit m_EditPasscodeHmac;
    CString m_sPasscodeHmac;
    int m_nBindType;
    virtual void OnOK();
};


// 授权信息保存辅助函数
std::string GetLicensesPath();
bool SaveLicenseInfo(const std::string& deviceID, const std::string& passcode,
                     const std::string& hmac, const std::string& remark = "");
bool LoadLicenseInfo(const std::string& deviceID, std::string& passcode,
                     std::string& hmac, std::string& remark);
// 更新授权活跃信息（IP、位置、最后活跃时间）
// 如果授权不存在则自动创建记录
bool UpdateLicenseActivity(const std::string& deviceID, const std::string& passcode,
                           const std::string& hmac, const std::string& ip = "",
                           const std::string& location = "");
// 检查授权是否已被撤销
bool IsLicenseRevoked(const std::string& deviceID);

class CPwdGenDlg : public CDialogLangEx
{
    DECLARE_DYNAMIC(CPwdGenDlg)

public:
    CPwdGenDlg(CWnd* pParent = nullptr);   // 标准构造函数
    virtual ~CPwdGenDlg();

    enum {
        IDD = IDD_DIALOG_KEYGEN
    };

protected:
    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

    DECLARE_MESSAGE_MAP()
public:
    HICON m_hIcon;
    CEdit m_EditDeviceID;
    CEdit m_EditPassword;
    CEdit m_EditUserPwd;
    CString m_sDeviceID;
    CString m_sPassword;
    CString m_sUserPwd;
    afx_msg void OnBnClickedButtonGenkey();
    afx_msg void OnBnClickedButtonSaveLicense();
    CDateTimeCtrl m_PwdExpireDate;
    COleDateTime m_ExpireTm;
    CDateTimeCtrl m_StartDate;
    COleDateTime m_StartTm;
    virtual BOOL OnInitDialog();
    CEdit m_EditHostNum;
    int m_nHostNum;
    CEdit m_EditHMAC;
    CButton m_BtnSaveLicense;
    BOOL m_bIsLocalDevice;  // 是否为本机授权
    CString m_sHMAC;        // HMAC 值
};
