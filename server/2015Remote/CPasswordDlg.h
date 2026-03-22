#pragma once

#include <afx.h>
#include <afxwin.h>
#include "Resource.h"
#include "common/commands.h"
#include "LangManager.h"

// CPasswordDlg 对话框
namespace TcpClient {
    std::string ObfuscateAuthorization(const std::string& auth);
    std::string DeobfuscateAuthorization(const std::string& obfuscated);
    bool IsAuthorizationValid(const std::string& authObfuscated);
}

void WriteHash(const char* pwdHash, const char* upperHash);
std::string getUpperHash();
std::string GetUpperHash();
std::string GetFinderString(const char* buf);

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
    CEdit m_EditRootCert;
    CString m_sRootCert;
    int m_nBindType;
    virtual void OnOK();
};


// 授权信息保存辅助函数
std::string GetLicensesPath();
bool SaveLicenseInfo(const std::string& deviceID, const std::string& passcode,
                     const std::string& hmac, const std::string& remark = "",
                     const std::string& authorization = "");
bool LoadLicenseInfo(const std::string& deviceID, std::string& passcode,
                     std::string& hmac, std::string& remark);
// 更新授权活跃信息（IP、位置、最后活跃时间）
// 如果授权不存在则自动创建记录
// machineName: 机器名，用于区分同一公网IP下的不同机器
bool UpdateLicenseActivity(const std::string& deviceID, const std::string& passcode,
                           const std::string& hmac, const std::string& ip = "",
                           const std::string& location = "", const std::string& machineName = "");
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
    CEdit m_EditAuthorization;  // 多层授权显示框
    CString m_sAuthorization;  // 多层授权: Authorization 字段

    // V2 授权相关
    CComboBox m_ComboVersion;   // 版本选择下拉框
    CEdit m_EditPrivateKey;     // 私钥文件路径
    CButton m_BtnBrowseKey;     // 浏览私钥文件按钮
    CButton m_BtnGenKeyPair;    // 生成密钥对按钮
    int m_nVersion;             // 0=V1(HMAC), 1=V2(ECDSA)
    CString m_sPrivateKeyPath;  // 私钥文件路径

    afx_msg void OnCbnSelchangeComboVersion();  // 版本切换事件
    afx_msg void OnBnClickedButtonBrowseKey();  // 浏览私钥文件事件
    afx_msg void OnBnClickedButtonGenKeypair(); // 生成密钥对事件
};
