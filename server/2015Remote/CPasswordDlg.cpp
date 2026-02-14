// CPasswordDlg.cpp: 实现文件
//

#include "stdafx.h"
#include "CPasswordDlg.h"
#include "CLicenseDlg.h"
#include "afxdialogex.h"
#include "pwd_gen.h"
#include "2015Remote.h"
#include "common/skCrypter.h"
#include "2015RemoteDlg.h"

// CPasswordDlg 对话框

IMPLEMENT_DYNAMIC(CPasswordDlg, CDialogEx)

// 主控程序唯一标识
// 密码的哈希值
char g_MasterID[_MAX_PATH] = {  "61f04dd637a74ee34493fc1025de2c131022536da751c29e3ff4e9024d8eec43" };

std::string GetPwdHash()
{
    auto id = std::string(g_MasterID).substr(0, 64);
    return id;
}

const Validation * GetValidation(int offset)
{
    return (Validation*)(g_MasterID + offset);
}

std::string GetMasterId()
{
    auto id = std::string(g_MasterID).substr(0, 16);
    return id;
}

std::string GetHMAC(int offset)
{
    const Validation * v= (Validation*)(g_MasterID + offset);
    std::string hmac(v->Checksum, 16);
    if (hmac.c_str()[0] == 0)
        hmac = THIS_CFG.GetStr("settings", "HMAC");
    return hmac;
}

void SetHMAC(const std::string str, int offset)
{
    Validation* v = (Validation*)(g_MasterID + offset);
    std::string hmac(v->Checksum, 16);
    if (hmac.c_str()[0] == 0) {
        memcpy(v->Checksum, str.c_str(), min(16, str.length()));
        THIS_CFG.SetStr("settings", "HMAC", str);
    }
}

extern "C" void shrink64to32(const char* input64, char* output32);  // output32 必须至少 33 字节

extern "C" void shrink32to4(const char* input32, char* output4);    // output4 必须至少 5 字节

// 获取授权信息存储路径
std::string GetLicensesPath()
{
    std::string dbPath = GetDbPath();
    // GetDbPath() 返回完整文件路径如 "C:\...\YAMA\YAMA.db"
    // 需要去掉末尾文件名，保留目录部分
    size_t pos = dbPath.find_last_of("\\/");
    if (pos != std::string::npos) {
        return dbPath.substr(0, pos + 1) + "licenses.ini";
    }
    return "licenses.ini";
}

// 保存授权信息到 INI 文件
bool SaveLicenseInfo(const std::string& deviceID, const std::string& passcode,
                     const std::string& hmac, const std::string& remark)
{
    std::string iniPath = GetLicensesPath();
    config cfg(iniPath);

    // 以 DeviceID 为 section 名
    cfg.SetStr(deviceID, "SerialNumber", deviceID);
    cfg.SetStr(deviceID, "Passcode", passcode);
    cfg.SetStr(deviceID, "HMAC", hmac);
    cfg.SetStr(deviceID, "Remark", remark);
    cfg.SetStr(deviceID, "Status", LICENSE_STATUS_ACTIVE);  // 默认状态为有效

    // 保存创建时间
    SYSTEMTIME st;
    GetLocalTime(&st);
    char timeStr[32];
    sprintf_s(timeStr, "%04d-%02d-%02d %02d:%02d:%02d",
              st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    cfg.SetStr(deviceID, "CreateTime", timeStr);

    // 初始化扩展字段
    cfg.SetStr(deviceID, "IP", "");
    cfg.SetStr(deviceID, "Location", "");
    cfg.SetStr(deviceID, "LastActiveTime", "");

    return true;
}

// 加载授权信息
bool LoadLicenseInfo(const std::string& deviceID, std::string& passcode,
                     std::string& hmac, std::string& remark)
{
    std::string iniPath = GetLicensesPath();
    config cfg(iniPath);

    passcode = cfg.GetStr(deviceID, "Passcode", "");
    if (passcode.empty())
        return false;

    hmac = cfg.GetStr(deviceID, "HMAC", "");
    remark = cfg.GetStr(deviceID, "Remark", "");
    return true;
}

// 更新授权活跃信息
bool UpdateLicenseActivity(const std::string& deviceID, const std::string& passcode,
                           const std::string& hmac, const std::string& ip,
                           const std::string& location)
{
    std::string iniPath = GetLicensesPath();
    config cfg(iniPath);

    // 检查该授权是否存在
    std::string existingPasscode = cfg.GetStr(deviceID, "Passcode", "");
    if (existingPasscode.empty()) {
        // 授权不存在，但验证成功了，说明是既往授权，自动添加记录
        cfg.SetStr(deviceID, "SerialNumber", deviceID);
        cfg.SetStr(deviceID, "Passcode", passcode);
        cfg.SetStr(deviceID, "HMAC", hmac);
        cfg.SetStr(deviceID, "Remark", "既往授权自动加入");
        cfg.SetStr(deviceID, "Status", LICENSE_STATUS_ACTIVE);  // 新记录默认为有效
    } else {
        // 授权已存在，更新 passcode（续期后 passcode 会变化）
        cfg.SetStr(deviceID, "Passcode", passcode);
        cfg.SetStr(deviceID, "HMAC", hmac);
    }

    // 更新最后活跃时间
    SYSTEMTIME st;
    GetLocalTime(&st);
    char timeStr[32];
    sprintf_s(timeStr, "%04d-%02d-%02d %02d:%02d:%02d",
              st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    cfg.SetStr(deviceID, "LastActiveTime", timeStr);

    // 如果是新添加的记录，设置创建时间
    if (existingPasscode.empty()) {
        cfg.SetStr(deviceID, "CreateTime", timeStr);
    }

    // 如果提供了 IP 和位置，则更新
    if (!ip.empty()) {
        cfg.SetStr(deviceID, "IP", ip);
    }
    if (!location.empty()) {
        cfg.SetStr(deviceID, "Location", location);
    }

    return true;
}

// 检查授权是否已被撤销
bool IsLicenseRevoked(const std::string& deviceID)
{
    std::string iniPath = GetLicensesPath();
    config cfg(iniPath);
    std::string status = cfg.GetStr(deviceID, "Status", LICENSE_STATUS_ACTIVE);
    return (status == LICENSE_STATUS_REVOKED);
}

#ifdef _WIN64
#pragma comment(lib, "lib/shrink_x64.lib")
#else
#pragma comment(lib, "lib/shrink.lib")
#endif

bool WritePwdHash(char* target, const std::string & pwdHash, const Validation& verify)
{
    char output32[33], output4[5];
    shrink64to32(pwdHash.c_str(), output32);
    shrink32to4(output32, output4);
    if (output32[0] == 0 || output4[0] == 0)
        return false;
    memcpy(target, pwdHash.c_str(), pwdHash.length());
    memcpy(target + 64, output32, 32);
    memcpy(target + 96, output4, 4);
#ifdef _DEBUG
    ASSERT(IsPwdHashValid(target));
#endif
    memcpy(target+100, &verify, sizeof(verify));
    return true;
}

bool IsPwdHashValid(const char* hash)
{
    const char* ptr = hash ? hash : g_MasterID;
    if (ptr == GetMasterHash())
        return true;
    std::string pwdHash(ptr, 64), s1(ptr +64, 32), s2(ptr +96, 4);
    char output32[33], output4[5];
    shrink64to32(pwdHash.c_str(), output32);
    shrink32to4(output32, output4);
    if (memcmp(output32, s1.c_str(), 32) || memcmp(output4, s2.c_str(), 4))
        return false;

    return true;
}

CPasswordDlg::CPasswordDlg(CWnd* pParent /*=nullptr*/)
    : CDialogLangEx(IDD_DIALOG_PASSWORD, pParent)
    , m_sDeviceID(_T(""))
    , m_sPassword(_T(""))
    , m_sPasscodeHmac(THIS_CFG.GetStr("settings", "PwdHmac", "").c_str())
    , m_nBindType(THIS_CFG.GetInt("settings", "BindType", 0))
{
    m_hIcon = nullptr;
}

CPasswordDlg::~CPasswordDlg()
{
}

void CPasswordDlg::DoDataExchange(CDataExchange* pDX)
{
    __super::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_EDIT_DEVICEID, m_EditDeviceID);
    DDX_Control(pDX, IDC_EDIT_DEVICEPWD, m_EditPassword);
    DDX_Text(pDX, IDC_EDIT_DEVICEID, m_sDeviceID);
    DDV_MaxChars(pDX, m_sDeviceID, 19);
    DDX_Text(pDX, IDC_EDIT_DEVICEPWD, m_sPassword);
    DDV_MaxChars(pDX, m_sPassword, 42);
    DDX_Control(pDX, IDC_COMBO_BIND, m_ComboBinding);
    DDX_Control(pDX, IDC_EDIT_PASSCODE_HMAC, m_EditPasscodeHmac);
    DDX_Text(pDX, IDC_EDIT_PASSCODE_HMAC, m_sPasscodeHmac);
    DDX_CBIndex(pDX, IDC_COMBO_BIND, m_nBindType);
}


BEGIN_MESSAGE_MAP(CPasswordDlg, CDialogEx)
    ON_CBN_SELCHANGE(IDC_COMBO_BIND, &CPasswordDlg::OnCbnSelchangeComboBind)
END_MESSAGE_MAP()


BOOL CPasswordDlg::OnInitDialog()
{
    __super::OnInitDialog();

    // TODO:  在此添加额外的初始化
    m_hIcon = LoadIcon(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDI_ICON_PASSWORD));
    SetIcon(m_hIcon, FALSE);

    m_ComboBinding.InsertStringL(0, "计算机硬件信息");
    m_ComboBinding.InsertStringL(1, "主控IP或域名信息");
    m_ComboBinding.SetCurSel(m_nBindType);

    return TRUE;  // return TRUE unless you set the focus to a control
    // 异常: OCX 属性页应返回 FALSE
}

void CPasswordDlg::OnCbnSelchangeComboBind()
{
    m_nBindType = m_ComboBinding.GetCurSel();
    std::string hardwareID = CMy2015RemoteDlg::GetHardwareID(m_nBindType);
    m_sDeviceID = getFixedLengthID(hashSHA256(hardwareID)).c_str();
    m_EditDeviceID.SetWindowTextA(m_sDeviceID);
    auto master = THIS_CFG.GetStr("settings", "master", "");
    if (m_nBindType == 1) {
        MessageBoxL("请确认是否正确设置公网地址（IP或域名）？\r\n"
                    "绑定IP后主控只能使用指定IP，绑定域名后\r\n"
                    "主控只能使用指定域名。当前公网地址: \r\n"
                    + CString(master.empty() ? "未设置" : master.c_str()), "提示", MB_OK | MB_ICONWARNING);
    }
}

void CPasswordDlg::OnOK()
{
    UpdateData(TRUE);
    if (!m_sDeviceID.IsEmpty()) {
        THIS_CFG.SetInt("settings", "BindType", m_nBindType);
        THIS_CFG.SetStr("settings", "PwdHmac", m_sPasscodeHmac.GetString());
    }

    __super::OnOK();
}

// CPasswordDlg 消息处理程序

IMPLEMENT_DYNAMIC(CPwdGenDlg, CDialogEx)

CPwdGenDlg::CPwdGenDlg(CWnd* pParent /*=nullptr*/)
    : CDialogLangEx(IDD_DIALOG_KEYGEN, pParent)
    , m_sDeviceID(_T(""))
    , m_sPassword(_T(""))
    , m_sUserPwd(_T(""))
    , m_sHMAC(_T(""))
    , m_ExpireTm(COleDateTime::GetCurrentTime())
    , m_StartTm(COleDateTime::GetCurrentTime())
    , m_nHostNum(2)
    , m_bIsLocalDevice(FALSE)
{

}

CPwdGenDlg::~CPwdGenDlg()
{
}

void CPwdGenDlg::DoDataExchange(CDataExchange* pDX)
{
    __super::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_EDIT_DEVICEID, m_EditDeviceID);
    DDX_Control(pDX, IDC_EDIT_DEVICEPWD, m_EditPassword);
    DDX_Control(pDX, IDC_EDIT_USERPWD, m_EditUserPwd);
    DDX_Text(pDX, IDC_EDIT_DEVICEID, m_sDeviceID);
    DDV_MaxChars(pDX, m_sDeviceID, 19);
    DDX_Text(pDX, IDC_EDIT_DEVICEPWD, m_sPassword);
    DDV_MaxChars(pDX, m_sPassword, 42);
    DDX_Text(pDX, IDC_EDIT_USERPWD, m_sUserPwd);
    DDV_MaxChars(pDX, m_sUserPwd, 24);
    DDX_Control(pDX, IDC_EXPIRE_DATE, m_PwdExpireDate);
    DDX_DateTimeCtrl(pDX, IDC_EXPIRE_DATE, m_ExpireTm);
    DDX_Control(pDX, IDC_START_DATE, m_StartDate);
    DDX_DateTimeCtrl(pDX, IDC_START_DATE, m_StartTm);
    DDX_Control(pDX, IDC_EDIT_HOSTNUM, m_EditHostNum);
    DDX_Text(pDX, IDC_EDIT_HOSTNUM, m_nHostNum);
    DDV_MinMaxInt(pDX, m_nHostNum, 2, 10000);
    DDX_Control(pDX, IDC_EDIT_HMAC, m_EditHMAC);
    DDX_Control(pDX, IDC_BUTTON_SAVE_LICENSE, m_BtnSaveLicense);
    DDX_Text(pDX, IDC_EDIT_HMAC, m_sHMAC);
}


BEGIN_MESSAGE_MAP(CPwdGenDlg, CDialogEx)
    ON_BN_CLICKED(IDC_BUTTON_GENKEY, &CPwdGenDlg::OnBnClickedButtonGenkey)
    ON_BN_CLICKED(IDC_BUTTON_SAVE_LICENSE, &CPwdGenDlg::OnBnClickedButtonSaveLicense)
END_MESSAGE_MAP()


void CPwdGenDlg::OnBnClickedButtonGenkey()
{
    // TODO: 在此添加控件通知处理程序代码
    UpdateData(TRUE);
    if (m_sUserPwd.IsEmpty())return;
    std::string pwdHash = hashSHA256(m_sUserPwd.GetString());
    if (pwdHash != GetPwdHash()) {
        Mprintf("hashSHA256 [%s]: %s\n", m_sUserPwd, pwdHash.c_str());
        MessageBoxL("您输入的密码不正确，无法生成口令!", "提示", MB_OK | MB_ICONWARNING);
        m_sUserPwd.Empty();
        return;
    }
    CString strBeginDate = m_StartTm.FormatL("%Y%m%d");
    CString strEndDate = m_ExpireTm.FormatL("%Y%m%d");
    CString hostNum;
    hostNum.FormatL("%04d", m_nHostNum);
    // 密码形式：20250209 - 20350209: SHA256: HostNum
    std::string password = std::string(strBeginDate.GetString()) + " - " + strEndDate.GetBuffer() + ": " + GetPwdHash() + ": " + hostNum.GetBuffer();
    std::string finalKey = deriveKey(password, m_sDeviceID.GetString());
    std::string fixedKey = strBeginDate.GetString() + std::string("-") + strEndDate.GetBuffer() + std::string("-") + hostNum.GetString() + "-" +
                           getFixedLengthID(finalKey);
    m_sPassword = fixedKey.c_str();
    m_EditPassword.SetWindowTextA(fixedKey.c_str());
    std::string hardwareID = CMy2015RemoteDlg::GetHardwareID();
    std::string hashedID = hashSHA256(hardwareID);
    std::string deviceID = getFixedLengthID(hashedID);
    std::string hmac = genHMAC(pwdHash, m_sUserPwd.GetString());
    uint64_t pwdHmac = SignMessage(m_sUserPwd.GetString(), (BYTE*)fixedKey.c_str(), fixedKey.length());
    m_sHMAC = std::to_string(pwdHmac).c_str();
    m_EditHMAC.SetWindowTextA(m_sHMAC);

    // 判断是否为本机授权
    m_bIsLocalDevice = (deviceID == m_sDeviceID.GetString());

    if (m_bIsLocalDevice) { // 授权的是当前主控程序
        auto settings = "settings", pwdKey = "Password";
        THIS_CFG.SetStr(settings, pwdKey, fixedKey.c_str());
        THIS_CFG.SetStr("settings", "SN", deviceID);
        THIS_CFG.SetStr(settings, "HMAC", hmac);
        THIS_CFG.SetStr(settings, "PwdHmac", std::to_string(pwdHmac));
        // 本机授权，禁用保存按钮
        m_BtnSaveLicense.EnableWindow(FALSE);
    } else {
        // 非本机授权，启用保存按钮
        m_BtnSaveLicense.EnableWindow(TRUE);
    }
}


BOOL CPwdGenDlg::OnInitDialog()
{
    __super::OnInitDialog();

    // TODO:  在此添加额外的初始化
    m_hIcon = LoadIcon(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDI_ICON_PASSWORD));
    SetIcon(m_hIcon, FALSE);

    // 初始状态禁用保存按钮
    m_BtnSaveLicense.EnableWindow(FALSE);

    return TRUE;  // return TRUE unless you set the focus to a control
    // 异常: OCX 属性页应返回 FALSE
}

void CPwdGenDlg::OnBnClickedButtonSaveLicense()
{
    UpdateData(TRUE);

    // 验证数据
    if (m_sDeviceID.IsEmpty() || m_sPassword.IsEmpty() || m_sHMAC.IsEmpty()) {
        MessageBoxL("请先生成口令!", "提示", MB_OK | MB_ICONWARNING);
        return;
    }

    // 提示用户输入备注（可选）
    CString remark;
    // 这里可以弹出一个简单的输入对话框获取备注，或者直接保存

    // 保存授权信息
    bool success = SaveLicenseInfo(
        m_sDeviceID.GetString(),
        m_sPassword.GetString(),
        m_sHMAC.GetString(),
        remark.GetString()
    );

    if (success) {
        CString msg;
        msg.Format(_T("授权信息已保存!\n\n序列号: %s\n口令: %s\nHMAC: %s\n\n存储位置: %s"),
                   m_sDeviceID, m_sPassword, m_sHMAC, GetLicensesPath().c_str());
        MessageBoxL(msg, "保存成功", MB_OK | MB_ICONINFORMATION);
    } else {
        MessageBoxL("保存授权信息失败!", "错误", MB_OK | MB_ICONERROR);
    }
}
