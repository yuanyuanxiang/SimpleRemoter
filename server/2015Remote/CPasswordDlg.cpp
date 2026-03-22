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
#include "generated_hash.h"

// 外部函数声明
extern std::vector<std::string> splitString(const std::string& str, char delimiter);
extern std::string GetFirstMasterIP(const std::string& master);

// CPasswordDlg 对话框

IMPLEMENT_DYNAMIC(CPasswordDlg, CDialogEx)

void WriteHash(const char* pwdHash, const char* upperHash) {
    if (strlen(pwdHash) == 0 || strlen(upperHash) == 0) {
        return;
    }
    memcpy(g_UpperHash + 100, upperHash, 64);
    std::string id = genHMACbyHash(pwdHash, upperHash);
    Validation verify(365, "", 0, id.c_str());
    WritePwdHash(g_MasterID, pwdHash, verify);
}

std::string getUpperHash()
{
    return std::string(g_UpperHash + 100);
}

std::string GetUpperHash()
{
    return std::string(g_UpperHash + 100).empty() ? GetMasterHash() : g_UpperHash + 100;
}

std::string GetPwdHash()
{
    return std::string(g_MasterID, 64);
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
                     const std::string& hmac, const std::string& remark,
                     const std::string& authorization)
{
    std::string iniPath = GetLicensesPath();
    config cfg(iniPath);

    // 以 DeviceID 为 section 名
    cfg.SetStr(deviceID, "SerialNumber", deviceID);
    cfg.SetStr(deviceID, "Passcode", passcode);
    cfg.SetStr(deviceID, "HMAC", hmac);
    cfg.SetStr(deviceID, "Remark", remark);
    cfg.SetStr(deviceID, "Status", LICENSE_STATUS_ACTIVE);  // 默认状态为有效

    // 保存 Authorization（多层授权）
    // 注意：authorization 参数传入时已经是混淆后的格式，直接保存
    if (!authorization.empty()) {
        cfg.SetStr(deviceID, "Authorization", authorization);
    }

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

// IP 列表管理常量
#define MAX_IP_HISTORY 50  // 最多保留 50 个不同的 IP

// 解析 IP 列表字符串为 vector<pair<IP, 时间戳>>
// 格式: "192.168.1.1|260218, 10.0.0.1|260215" (yyMMdd)
static std::vector<std::pair<std::string, std::string>> ParseIPList(const std::string& ipListStr)
{
    std::vector<std::pair<std::string, std::string>> result;
    if (ipListStr.empty()) return result;

    size_t start = 0;
    while (start < ipListStr.length()) {
        // 找到下一个逗号
        size_t end = ipListStr.find(',', start);
        if (end == std::string::npos) end = ipListStr.length();

        // 提取单个 IP 条目
        std::string entry = ipListStr.substr(start, end - start);

        // 去除前后空格
        size_t first = entry.find_first_not_of(' ');
        size_t last = entry.find_last_not_of(' ');
        if (first != std::string::npos && last != std::string::npos) {
            entry = entry.substr(first, last - first + 1);
        }

        // 解析 IP|时间戳
        size_t pipePos = entry.find('|');
        if (pipePos != std::string::npos) {
            std::string ip = entry.substr(0, pipePos);
            std::string ts = entry.substr(pipePos + 1);
            result.push_back({ ip, ts });
        } else if (!entry.empty()) {
            // 兼容旧格式（无时间戳）
            result.push_back({ entry, "" });
        }

        start = end + 1;
    }
    return result;
}

// 将 IP 列表序列化为字符串
static std::string SerializeIPList(const std::vector<std::pair<std::string, std::string>>& ipList)
{
    std::string result;
    for (size_t i = 0; i < ipList.size(); ++i) {
        if (i > 0) result += ", ";
        result += ipList[i].first;
        if (!ipList[i].second.empty()) {
            result += "|";
            result += ipList[i].second;
        }
    }
    return result;
}

// 更新 IP 列表：添加新 IP 或更新已有 IP 的时间戳
// 格式: IP(机器名)|时间戳，例如 "1.2.3.4(PC01)|260219" (yyMMdd 格式)
// 返回更新后的 IP 列表字符串
static std::string UpdateIPList(const std::string& existingIPList, const std::string& newIP, const std::string& machineName = "")
{
    if (newIP.empty()) return existingIPList;

    // 构造 IP 标识：如果有机器名则为 "IP(机器名)"，否则为 "IP"
    std::string ipKey = newIP;
    if (!machineName.empty()) {
        // 机器名可能包含 "/"（如 "PC01/GroupName"），只取第一部分
        std::string shortName = machineName;
        size_t slashPos = machineName.find('/');
        if (slashPos != std::string::npos) {
            shortName = machineName.substr(0, slashPos);
        }
        ipKey = newIP + "(" + shortName + ")";
    }

    // 获取当前时间戳 (yyMMdd 格式，6位)
    SYSTEMTIME st;
    GetLocalTime(&st);
    char timestamp[8];
    sprintf_s(timestamp, "%02d%02d%02d", st.wYear % 100, st.wMonth, st.wDay);

    // 解析现有 IP 列表
    auto ipList = ParseIPList(existingIPList);

    // 提取纯 IP（去掉括号内的机器名）
    auto extractIP = [](const std::string& s) -> std::string {
        size_t pos = s.find('(');
        return pos != std::string::npos ? s.substr(0, pos) : s;
    };

    // 查找是否已存在该 IP（只比较 IP 部分，兼容旧格式）
    bool found = false;
    for (auto& entry : ipList) {
        if (extractIP(entry.first) == newIP) {
            entry.first = ipKey;       // 更新为新格式（带机器名）
            entry.second = timestamp;  // 更新时间戳
            found = true;
            break;
        }
    }

    if (!found) {
        // 新 IP，添加到开头（最近的在前）
        ipList.insert(ipList.begin(), { ipKey, timestamp });

        // 限制数量
        if (ipList.size() > MAX_IP_HISTORY) {
            ipList.resize(MAX_IP_HISTORY);
        }
    }

    return SerializeIPList(ipList);
}

// 获取 IP 列表中的 IP 数量
static int GetIPCount(const std::string& ipListStr)
{
    if (ipListStr.empty()) return 0;
    auto ipList = ParseIPList(ipListStr);
    return (int)ipList.size();
}

// 更新授权活跃信息
bool UpdateLicenseActivity(const std::string& deviceID, const std::string& passcode,
                           const std::string& hmac, const std::string& ip,
                           const std::string& location, const std::string& machineName)
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

    // 更新 IP 列表（追加新 IP 或更新已有 IP 的时间戳）
    // 格式: IP(机器名)|yyMMdd
    if (!ip.empty()) {
        std::string existingIPList = cfg.GetStr(deviceID, "IP", "");
        std::string newIPList = UpdateIPList(existingIPList, ip, machineName);
        cfg.SetStr(deviceID, "IP", newIPList);
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

std::string GetFinderString(const char* buf)
{
    char output32[100] = {};
	memcpy(output32, buf, 64);
    if (GetPwdHash() == GetMasterHash()) {
		return std::string(output32, 100);
    }
    shrink64to32(buf, output32+64);
    return std::string(output32, 96);
}

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
    if (memcmp(output32, s1.c_str(), 32) || memcmp(output4, s2.c_str(), 4)) {
        // 哈希校验失败，尝试离线校验 Authorization
        std::string authObf = THIS_CFG.GetStr("settings", "Authorization", "");
        if (!authObf.empty() && TcpClient::IsAuthorizationValid(authObf)) {
            return true;  // Authorization 签名有效
        }
        return false;
    }

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
    DDX_Control(pDX, IDC_EDIT_ROOT_CERT, m_EditRootCert);
    DDX_Text(pDX, IDC_EDIT_ROOT_CERT, m_sRootCert);
    DDX_CBIndex(pDX, IDC_COMBO_BIND, m_nBindType);
}


BEGIN_MESSAGE_MAP(CPasswordDlg, CDialogEx)
    ON_CBN_SELCHANGE(IDC_COMBO_BIND, &CPasswordDlg::OnCbnSelchangeComboBind)
END_MESSAGE_MAP()


BOOL CPasswordDlg::OnInitDialog()
{
    __super::OnInitDialog();
    // 多语言翻译 - Static控件
    SetDlgItemText(IDC_STATIC_PASSWORD_SERIAL, _TR("序 列 号:"));
    SetDlgItemText(IDC_STATIC_PASSWORD_TOKEN, _TR("授权口令:"));
    SetDlgItemText(IDC_STATIC_PASSWORD_METHOD, _TR("授权方式:"));
    SetDlgItemText(IDC_STATIC_PASSWORD_VERIFY, _TR("验 证 码:"));
    SetDlgItemText(IDC_STATIC_ROOT_CERT, _TR("根 凭 证:"));

    // 设置对话框标题和控件文本（解决英语系统乱码问题）
    SetWindowText(_TR("口令"));
    SetDlgItemText(IDOK, _TR("确定"));
    SetDlgItemText(IDCANCEL, _TR("取消"));

    // TODO:  在此添加额外的初始化
    m_hIcon = LoadIcon(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDI_ICON_PASSWORD));
    SetIcon(m_hIcon, FALSE);

    m_ComboBinding.InsertStringL(0, "计算机硬件信息");
    m_ComboBinding.InsertStringL(1, "主控IP或域名信息");
    m_ComboBinding.SetCurSel(m_nBindType);

    // 加载已保存的根凭证
    std::string savedAuth = THIS_CFG.GetStr("settings", "Authorization", "");
    if (!savedAuth.empty()) {
        m_sRootCert = savedAuth.c_str();
        m_EditRootCert.SetWindowText(m_sRootCert);
    }

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
        MessageBoxL(_L("请确认是否正确设置公网地址（IP或域名）？\r\n"
                    "绑定IP后主控只能使用指定IP，绑定域名后\r\n"
                    "主控只能使用指定域名。当前公网地址: \r\n")+
                    + CString(master.empty() ? _L("未设置") : master.c_str()), "提示", MB_OK | MB_ICONWARNING);
    }
}

void CPasswordDlg::OnOK()
{
    UpdateData(TRUE);
    if (!m_sDeviceID.IsEmpty()) {
        THIS_CFG.SetInt("settings", "BindType", m_nBindType);
        THIS_CFG.SetStr("settings", "SN", m_sDeviceID.GetString());  // 切换绑定方式时同步保存 SN
        THIS_CFG.SetStr("settings", "PwdHmac", m_sPasscodeHmac.GetString());

        // 保存根凭证（可选，用于多层授权）
        if (!m_sRootCert.IsEmpty()) {
            THIS_CFG.SetStr("settings", "Authorization", m_sRootCert.GetString());
        }
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
    , m_nVersion(0)
    , m_sPrivateKeyPath(_T(""))
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
    DDX_Control(pDX, IDC_EDIT_AUTHORIZATION, m_EditAuthorization);
    DDX_Control(pDX, IDC_BUTTON_SAVE_LICENSE, m_BtnSaveLicense);
    DDX_Text(pDX, IDC_EDIT_HMAC, m_sHMAC);
    DDX_Text(pDX, IDC_EDIT_AUTHORIZATION, m_sAuthorization);
    DDX_Control(pDX, IDC_COMBO_VERSION, m_ComboVersion);
    DDX_Control(pDX, IDC_EDIT_PRIVATEKEY, m_EditPrivateKey);
    DDX_Control(pDX, IDC_BUTTON_BROWSE_KEY, m_BtnBrowseKey);
    DDX_Control(pDX, IDC_BUTTON_GEN_KEYPAIR, m_BtnGenKeyPair);
    DDX_CBIndex(pDX, IDC_COMBO_VERSION, m_nVersion);
    DDX_Text(pDX, IDC_EDIT_PRIVATEKEY, m_sPrivateKeyPath);
}


BEGIN_MESSAGE_MAP(CPwdGenDlg, CDialogEx)
    ON_BN_CLICKED(IDC_BUTTON_GENKEY, &CPwdGenDlg::OnBnClickedButtonGenkey)
    ON_BN_CLICKED(IDC_BUTTON_SAVE_LICENSE, &CPwdGenDlg::OnBnClickedButtonSaveLicense)
    ON_CBN_SELCHANGE(IDC_COMBO_VERSION, &CPwdGenDlg::OnCbnSelchangeComboVersion)
    ON_BN_CLICKED(IDC_BUTTON_BROWSE_KEY, &CPwdGenDlg::OnBnClickedButtonBrowseKey)
    ON_BN_CLICKED(IDC_BUTTON_GEN_KEYPAIR, &CPwdGenDlg::OnBnClickedButtonGenKeypair)
END_MESSAGE_MAP()


void CPwdGenDlg::OnBnClickedButtonGenkey()
{
    UpdateData(TRUE);

    CString strBeginDate = m_StartTm.FormatL("%Y%m%d");
    CString strEndDate = m_ExpireTm.FormatL("%Y%m%d");
    CString hostNum;
    hostNum.FormatL("%04d", m_nHostNum);

    std::string fixedKey;
    std::string pwdHmacStr;  // V1: 数字字符串, V2: "v2:..." 签名
    std::string hmacV1;      // 只有 V1 使用

    if (m_nVersion == 0) {
        // V1 (HMAC) 模式
        if (m_sUserPwd.IsEmpty()) return;
        std::string pwdHash = hashSHA256(m_sUserPwd.GetString());
        if (pwdHash != GetPwdHash()) {
            Mprintf("hashSHA256 [%s]: %s\n", m_sUserPwd, pwdHash.c_str());
            MessageBoxL("您输入的密码不正确，无法生成口令!", "提示", MB_OK | MB_ICONWARNING);
            m_sUserPwd.Empty();
            return;
        }
        // 密码形式：20250209 - 20350209: SHA256: HostNum
        std::string password = std::string(strBeginDate.GetString()) + " - " + strEndDate.GetBuffer() + ": " + GetPwdHash() + ": " + hostNum.GetBuffer();
        std::string finalKey = deriveKey(password, m_sDeviceID.GetString());
        fixedKey = strBeginDate.GetString() + std::string("-") + strEndDate.GetBuffer() + std::string("-") + hostNum.GetString() + "-" +
                   getFixedLengthID(finalKey);
        m_sPassword = fixedKey.c_str();
        m_EditPassword.SetWindowTextA(fixedKey.c_str());

        hmacV1 = genHMAC(pwdHash, m_sUserPwd.GetString());
        uint64_t pwdHmac = SignMessage(m_sUserPwd.GetString(), (BYTE*)fixedKey.c_str(), fixedKey.length());
        pwdHmacStr = std::to_string(pwdHmac);
        m_sHMAC = pwdHmacStr.c_str();
        m_EditHMAC.SetWindowTextA(m_sHMAC);

        // 多层授权：检查是否有 Authorization + master（公网地址）
        std::string storedAuthObf = THIS_CFG.GetStr("settings", "Authorization", "");
        std::string storedAuth = TcpClient::DeobfuscateAuthorization(storedAuthObf);  // 还原明文
        std::string masterIP = GetFirstMasterIP(THIS_CFG.GetStr("settings", "master", ""));
        int masterPort = THIS_CFG.Get1Int("settings", "ghost", ';', 6543);
        std::string hmacServer = masterIP.empty() ? "" : masterIP + ":" + std::to_string(masterPort);
        if (!storedAuth.empty() && !hmacServer.empty()) {
            auto authParts = splitString(storedAuth, '|');
            std::string fullAuth;
            if (authParts.size() == 5) {
                // V2 格式（5段）：第一层给第二层
                // 验证 snHashPrefix 匹配（使用 SN/deviceID）
                std::string storedPrefix = authParts[3];
                std::string mySN = THIS_CFG.GetStr("settings", "SN", "");
                std::string expectedPrefix = computeSnHashPrefix(mySN);
                if (storedPrefix == expectedPrefix) {
                    // 组装完整 Authorization (V1 格式 6段)
                    fullAuth = authParts[0] + "|" + authParts[1] + "|" + authParts[2] + "|" + authParts[3] + "|" + hmacServer + "|" + authParts[4];
                    Mprintf("V1 生成包含 Authorization: %s\n", m_sDeviceID.GetString());
                } else {
                    // snHashPrefix 不匹配，不能使用这个 Authorization
                    Mprintf("V1 Authorization snHashPrefix 不匹配: 期望 %s, 存储 %s\n", expectedPrefix.c_str(), storedPrefix.c_str());
                }
            } else if (authParts.size() == 6) {
                // V1 格式（6段）：第二层及以下给下级
                // 替换 hmacServer 为自己的地址
                fullAuth = authParts[0] + "|" + authParts[1] + "|" + authParts[2] + "|" + authParts[3] + "|" + hmacServer + "|" + authParts[5];
                Mprintf("V1 转发 Authorization: %s\n", m_sDeviceID.GetString());
            }

            if (!fullAuth.empty()) {
                m_sAuthorization = TcpClient::ObfuscateAuthorization(fullAuth).c_str();
                m_EditAuthorization.SetWindowText(m_sAuthorization);
            } else {
                m_sAuthorization.Empty();
                m_EditAuthorization.SetWindowText(_T(""));
            }
        } else {
            m_sAuthorization.Empty();
            m_EditAuthorization.SetWindowText(_T(""));
        }
    } else {
        // V2 (ECDSA) 模式
        if (m_sPrivateKeyPath.IsEmpty()) {
            MessageBoxL("请选择私钥文件!", "提示", MB_OK | MB_ICONWARNING);
            return;
        }

        // 检查私钥文件是否存在
        if (GetFileAttributes(m_sPrivateKeyPath) == INVALID_FILE_ATTRIBUTES) {
            MessageBoxL("私钥文件不存在!", "错误", MB_OK | MB_ICONERROR);
            return;
        }

        // V2 口令格式与 V1 相同，但不需要密码验证
        // 使用 deviceId + dates 作为种子生成 key
        std::string seed = m_sDeviceID.GetString() + std::string(strBeginDate.GetString()) +
                           strEndDate.GetString() + hostNum.GetString();
        std::string seedHash = hashSHA256(seed);
        fixedKey = strBeginDate.GetString() + std::string("-") + strEndDate.GetBuffer() +
                   std::string("-") + hostNum.GetString() + "-" + getFixedLengthID(seedHash);

        // 使用私钥签名
        pwdHmacStr = signPasswordV2(m_sDeviceID.GetString(), fixedKey, m_sPrivateKeyPath.GetString());
        if (pwdHmacStr.empty()) {
            MessageBoxL("生成 V2 签名失败!\n请检查私钥文件是否有效。", "错误", MB_OK | MB_ICONERROR);
            return;
        }

        m_sPassword = fixedKey.c_str();
        m_EditPassword.SetWindowTextA(fixedKey.c_str());
        m_sHMAC = pwdHmacStr.c_str();
        m_EditHMAC.SetWindowTextA(pwdHmacStr.c_str());

        // V2 模式：生成 Authorization（使用 deviceID 计算 snHashPrefix）
        // 从 fixedKey 提取 license: "20260317-20270317-0256-..." → "20260317|20270317|0256"
        std::string license = strBeginDate.GetString() + std::string("|") +
                              strEndDate.GetString() + "|" + hostNum.GetString();
        std::string snHashPrefix = computeSnHashPrefix(m_sDeviceID.GetString());
        std::string authSig = signAuthorizationV2(license, snHashPrefix, m_sPrivateKeyPath.GetString());
        if (!authSig.empty()) {
            // Authorization 格式（V2，5段）: startDate|endDate|hostNum|snHashPrefix|signature
            std::string auth = license + "|" + snHashPrefix + "|" + authSig;
            m_sAuthorization = TcpClient::ObfuscateAuthorization(auth).c_str();  // 混淆后显示
            m_EditAuthorization.SetWindowText(m_sAuthorization);
            GetDlgItem(IDC_STATIC_AUTHORIZATION)->ShowWindow(SW_SHOW);
            GetDlgItem(IDC_EDIT_AUTHORIZATION)->ShowWindow(SW_SHOW);
            Mprintf("V2 生成 Authorization: %s (snHashPrefix=%s)\n", m_sDeviceID.GetString(), snHashPrefix.c_str());
        } else {
            m_sAuthorization.Empty();
            m_EditAuthorization.SetWindowText(_T(""));
        }
    }

    // 公共部分：判断是否为本机授权
    std::string hardwareID = CMy2015RemoteDlg::GetHardwareID();
    std::string hashedID = hashSHA256(hardwareID);
    std::string deviceID = getFixedLengthID(hashedID);
    m_bIsLocalDevice = (deviceID == m_sDeviceID.GetString());
    bool isSuperAdmin = GetUpperHash()==GetPwdHash();

    if (m_bIsLocalDevice && isSuperAdmin) {
        // 给自己授权，自动保存
        auto settings = "settings", pwdKey = "Password";
        THIS_CFG.SetStr(settings, pwdKey, fixedKey.c_str());
        THIS_CFG.SetStr("settings", "SN", deviceID);
        THIS_CFG.SetStr(settings, "HMAC", hmacV1);
        THIS_CFG.SetStr(settings, "PwdHmac", pwdHmacStr);
        m_BtnSaveLicense.EnableWindow(FALSE);
    } else if (m_bIsLocalDevice && !isSuperAdmin) {
        // 没有权限给自己生成授权
        MessageBoxL("您无法给自己授权！\n生成的授权信息仅供参考，不会自动保存。",
                    "提示", MB_OK | MB_ICONWARNING);
        m_BtnSaveLicense.EnableWindow(FALSE);
    } else {
        m_BtnSaveLicense.EnableWindow(TRUE);
    }
}


BOOL CPwdGenDlg::OnInitDialog()
{
    __super::OnInitDialog();

    // 设置对话框标题和控件文本（解决英语系统乱码问题）
    SetWindowText(_TR("生成口令"));
    SetDlgItemText(IDC_BUTTON_SAVE_LICENSE, _TR("保存授权"));
    SetDlgItemText(IDCANCEL, _TR("取消"));
    SetDlgItemText(IDC_STATIC_PWD_LABEL, _TR("密  码:"));
    SetDlgItemText(IDC_STATIC_KEYGEN_VERSION, _TR("版  本:"));
    SetDlgItemText(IDC_STATIC_KEYGEN_SERIAL, _TR("序列号:"));
    SetDlgItemText(IDC_STATIC_KEYGEN_EXPIRE, _TR("有效期:"));
    SetDlgItemText(IDC_STATIC_KEYGEN_CONN, _TR("连接数:"));
    SetDlgItemText(IDC_STATIC_KEYGEN_TOKEN, _TR("口  令:"));
    SetDlgItemText(IDC_STATIC_KEYGEN_HMAC_2373, _TR("HMAC:"));
    GetDlgItem(IDC_STATIC_KEYGEN_VERSION)->EnableWindow(GetUpperHash() == GetPwdHash());

    // TODO:  在此添加额外的初始化
    m_hIcon = LoadIcon(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDI_ICON_PASSWORD));
    SetIcon(m_hIcon, FALSE);

    // 初始化版本下拉框
    m_ComboVersion.InsertString(0, _T("V1 (HMAC)"));
    m_ComboVersion.InsertString(1, _T("V2 (ECDSA)"));
    m_ComboVersion.SetCurSel(0);
	m_ComboVersion.EnableWindow(GetUpperHash() == GetPwdHash());
    m_nVersion = 0;

    // 初始状态：V1 模式，隐藏私钥控件
    m_EditPrivateKey.ShowWindow(SW_HIDE);
    m_BtnBrowseKey.ShowWindow(SW_HIDE);
    m_BtnGenKeyPair.ShowWindow(SW_HIDE);
    m_EditUserPwd.ShowWindow(SW_SHOW);

    // 初始状态禁用保存按钮
    m_BtnSaveLicense.EnableWindow(FALSE);

    // 加载已配置的 V2 私钥路径
    std::string v2Key = THIS_CFG.GetStr("settings", "V2PrivateKey", "");
    if (!v2Key.empty()) {
        m_sPrivateKeyPath = v2Key.c_str();
        m_EditPrivateKey.SetWindowText(m_sPrivateKeyPath);
    }

    // 设置根凭证标签翻译
    SetDlgItemText(IDC_STATIC_AUTHORIZATION, _TR("根凭证:"));

    // 检查并显示当前服务端的 Authorization
    std::string storedAuth = THIS_CFG.GetStr("settings", "Authorization", "");
    if (!storedAuth.empty()) {
        // 服务端已获得上级授权，在标题栏提示
        CString title;
        title.Format(_T("%s - %s"), _TR("生成口令"), _TR("已获得上级授权"));
        SetWindowText(title);
    }

    return TRUE;  // return TRUE unless you set the focus to a control
    // 异常: OCX 属性页应返回 FALSE
}

void CPwdGenDlg::OnCbnSelchangeComboVersion()
{
    m_nVersion = m_ComboVersion.GetCurSel();

    // 切换版本时清理已生成的值
    m_EditPassword.SetWindowText(_T(""));
    m_EditHMAC.SetWindowText(_T(""));
    m_EditAuthorization.SetWindowText(_T(""));
    m_sPassword.Empty();
    m_sHMAC.Empty();
    m_sAuthorization.Empty();
    m_BtnSaveLicense.EnableWindow(FALSE);

    // 获取"密码"标签控件和"授权"控件
    CWnd* pLabel = GetDlgItem(IDC_STATIC_PWD_LABEL);
    CWnd* pAuthLabel = GetDlgItem(IDC_STATIC_AUTHORIZATION);

    if (m_nVersion == 0) {
        // V1 (HMAC) 模式：显示密码输入框，隐藏私钥相关控件
        m_EditUserPwd.ShowWindow(SW_SHOW);
        m_EditPrivateKey.ShowWindow(SW_HIDE);
        m_BtnBrowseKey.ShowWindow(SW_HIDE);
        m_BtnGenKeyPair.ShowWindow(SW_HIDE);
        if (pLabel) pLabel->SetWindowText(_TR("密  码:"));
    } else {
        // V2 (ECDSA) 模式：隐藏密码输入框，显示私钥相关控件
        m_EditUserPwd.ShowWindow(SW_HIDE);
        m_EditPrivateKey.ShowWindow(SW_SHOW);
        m_BtnBrowseKey.ShowWindow(SW_SHOW);
        m_BtnGenKeyPair.ShowWindow(SW_SHOW);
        if (pLabel) pLabel->SetWindowText(_TR("私  钥:"));

        // 如果已有有效私钥，禁用浏览和生成按钮，私钥路径只读
        std::string v2Key = THIS_CFG.GetStr("settings", "V2PrivateKey", "");
        bool hasValidKey = !v2Key.empty() && GetFileAttributes(v2Key.c_str()) != INVALID_FILE_ATTRIBUTES;
        m_BtnBrowseKey.EnableWindow(!hasValidKey);
        m_BtnGenKeyPair.EnableWindow(!hasValidKey);
        m_EditPrivateKey.SetReadOnly(hasValidKey);
    }
}

void CPwdGenDlg::OnBnClickedButtonBrowseKey()
{
    // 检查是否有权使用 V2 私钥
    if (GetUpperHash() != GetPwdHash()) {
        MessageBoxL("您无法使用 V2 私钥签名！", "提示", MB_OK | MB_ICONWARNING);
        return;
    }

    CFileDialog dlg(TRUE, _T("key"), nullptr,
                    OFN_FILEMUSTEXIST | OFN_HIDEREADONLY,
                    _T("Key Files (*.key;*.pem)|*.key;*.pem|All Files (*.*)|*.*||"),
                    this);
    dlg.m_ofn.lpstrTitle = _T("Select Private Key");

    if (dlg.DoModal() == IDOK) {
        m_sPrivateKeyPath = dlg.GetPathName();
        m_EditPrivateKey.SetWindowText(m_sPrivateKeyPath);
    }
}

void CPwdGenDlg::OnBnClickedButtonGenKeypair()
{
    // 检查是否有权生成 V2 密钥对
    if (GetUpperHash() != GetPwdHash()) {
        MessageBoxL("您无法生成 V2 密钥对！", "提示", MB_OK | MB_ICONWARNING);
        return;
    }

    // 选择私钥保存位置
    CFileDialog dlg(FALSE, _T("key"), _T("private_key.key"),
                    OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY,
                    _T("Key Files (*.key)|*.key|All Files (*.*)|*.*||"),
                    this);
    dlg.m_ofn.lpstrTitle = _T("Save Private Key");

    if (dlg.DoModal() != IDOK) {
        return;
    }

    CString privateKeyPath = dlg.GetPathName();

    // 生成密钥对
    BYTE publicKey[V2_PUBKEY_SIZE];
    if (!GenerateKeyPairV2(privateKeyPath.GetString(), publicKey)) {
        MessageBoxL("生成密钥对失败!", "错误", MB_OK | MB_ICONERROR);
        return;
    }

    // 更新私钥路径（仅更新界面，不保存到配置，需通过菜单设置）
    m_sPrivateKeyPath = privateKeyPath;
    m_EditPrivateKey.SetWindowText(m_sPrivateKeyPath);

    // 格式化公钥为 C 代码
    std::string pubKeyCode = formatPublicKeyAsCode(publicKey);

    // 生成公钥文件路径（与私钥同目录，扩展名改为 .pub.h）
    CString publicKeyPath = privateKeyPath;
    int dotPos = publicKeyPath.ReverseFind('.');
    if (dotPos > 0) {
        publicKeyPath = publicKeyPath.Left(dotPos) + _T(".pub.h");
    } else {
        publicKeyPath += _T(".pub.h");
    }

    // 保存公钥到文件
    FILE* fp = nullptr;
    if (fopen_s(&fp, publicKeyPath.GetString(), "w") == 0 && fp) {
        fprintf(fp, "// V2 License Public Key\n");
        fprintf(fp, "// Generated: %s\n\n", CTime::GetCurrentTime().Format("%Y-%m-%d %H:%M:%S").GetString());
        fprintf(fp, "%s\n", pubKeyCode.c_str());
        fclose(fp);
    }

    // 复制公钥到剪贴板
    if (OpenClipboard()) {
        EmptyClipboard();
        size_t len = pubKeyCode.length() + 1;
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len);
        if (hMem) {
            char* pMem = (char*)GlobalLock(hMem);
            if (pMem) {
                memcpy(pMem, pubKeyCode.c_str(), len);
                GlobalUnlock(hMem);
                SetClipboardData(CF_TEXT, hMem);
            }
        }
        CloseClipboard();
    }

    // 简短提示
    CString msg;
    msg.Format(_T("密钥对生成成功!\n\n私钥: %s\n公钥: %s\n\n公钥代码已复制到剪贴板"),
               privateKeyPath.GetString(), publicKeyPath.GetString());
    MessageBox(msg, _TR("成功"), MB_OK | MB_ICONINFORMATION);
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
        remark.GetString(),
        m_sAuthorization.GetString()
    );

    if (success) {
        CString msg;
        if (m_sAuthorization.IsEmpty()) {
            msg.FormatL("授权信息已保存!\n\n序列号: %s\n口令: %s\nHMAC: %s\n\n存储位置: %s",
                       m_sDeviceID, m_sPassword, m_sHMAC, GetLicensesPath().c_str());
        } else {
            msg.FormatL("授权信息已保存!\n\n序列号: %s\n口令: %s\nHMAC: %s\nAuthorization: %s\n\n存储位置: %s",
                       m_sDeviceID, m_sPassword, m_sHMAC, m_sAuthorization, GetLicensesPath().c_str());
        }
        MessageBox(msg, _TR("保存成功"), MB_OK | MB_ICONINFORMATION);
    } else {
        MessageBoxL("保存授权信息失败!", "错误", MB_OK | MB_ICONERROR);
    }
}
