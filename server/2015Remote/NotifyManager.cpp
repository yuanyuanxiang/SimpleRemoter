#include "stdafx.h"
#include "NotifyManager.h"
#include "context.h"
#include <thread>
#include <fstream>
#include <sstream>
#include <shlobj.h>
#include <ctime>

// Get config directory path (same as GetDbPath directory)
static std::string GetConfigDir()
{
    static char path[MAX_PATH];
    static std::string ret;
    if (ret.empty()) {
        if (FAILED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, path))) {
            ret = ".\\";
        } else {
            ret = std::string(path) + "\\YAMA\\";
        }
        CreateDirectoryA(ret.c_str(), NULL);
    }
    return ret;
}

NotifyManager& NotifyManager::Instance()
{
    static NotifyManager instance;
    return instance;
}

NotifyManager::NotifyManager()
    : m_initialized(false)
    , m_powerShellAvailable(false)
{
}

void NotifyManager::Initialize()
{
    if (m_initialized) return;

    m_powerShellAvailable = DetectPowerShellSupport();
    LoadConfig();
    m_initialized = true;
}

bool NotifyManager::DetectPowerShellSupport()
{
    // Check if PowerShell Send-MailMessage command is available
    std::string cmd = "powershell -NoProfile -Command \"Get-Command Send-MailMessage -ErrorAction SilentlyContinue\"";
    DWORD exitCode = 1;
    ExecutePowerShell(cmd, &exitCode, true);
    return (exitCode == 0);
}

std::string NotifyManager::GetConfigPath() const
{
    return GetConfigDir() + "notify.ini";
}

NotifyConfig NotifyManager::GetConfig()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_config;
}

void NotifyManager::SetConfig(const NotifyConfig& config)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    // Preserve lastNotifyTime from current config
    auto lastNotifyTime = m_config.lastNotifyTime;
    m_config = config;
    m_config.lastNotifyTime = lastNotifyTime;
}

void NotifyManager::LoadConfig()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string path = GetConfigPath();
    char buffer[1024];

    // SMTP settings
    GetPrivateProfileStringA("SMTP", "Server", "smtp.gmail.com", buffer, sizeof(buffer), path.c_str());
    m_config.smtp.server = buffer;

    m_config.smtp.port = GetPrivateProfileIntA("SMTP", "Port", 587, path.c_str());
    m_config.smtp.useSSL = GetPrivateProfileIntA("SMTP", "UseSSL", 1, path.c_str()) != 0;

    GetPrivateProfileStringA("SMTP", "Username", "", buffer, sizeof(buffer), path.c_str());
    m_config.smtp.username = buffer;

    GetPrivateProfileStringA("SMTP", "Password", "", buffer, sizeof(buffer), path.c_str());
    m_config.smtp.password = DecryptPassword(buffer);

    GetPrivateProfileStringA("SMTP", "Recipient", "", buffer, sizeof(buffer), path.c_str());
    m_config.smtp.recipient = buffer;

    // Rule settings (currently only one rule)
    NotifyRule& rule = m_config.GetRule();
    rule.enabled = GetPrivateProfileIntA("Rule_0", "Enabled", 0, path.c_str()) != 0;
    rule.triggerType = (NotifyTriggerType)GetPrivateProfileIntA("Rule_0", "TriggerType", NOTIFY_TRIGGER_HOST_ONLINE, path.c_str());
    rule.columnIndex = GetPrivateProfileIntA("Rule_0", "ColumnIndex", ONLINELIST_COMPUTER_NAME, path.c_str());

    GetPrivateProfileStringA("Rule_0", "MatchPattern", "", buffer, sizeof(buffer), path.c_str());
    rule.matchPattern = buffer;
}

void NotifyManager::SaveConfig()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string path = GetConfigPath();
    char buffer[32];

    // SMTP settings
    WritePrivateProfileStringA("SMTP", "Server", m_config.smtp.server.c_str(), path.c_str());

    sprintf_s(buffer, "%d", m_config.smtp.port);
    WritePrivateProfileStringA("SMTP", "Port", buffer, path.c_str());

    WritePrivateProfileStringA("SMTP", "UseSSL", m_config.smtp.useSSL ? "1" : "0", path.c_str());
    WritePrivateProfileStringA("SMTP", "Username", m_config.smtp.username.c_str(), path.c_str());
    WritePrivateProfileStringA("SMTP", "Password", EncryptPassword(m_config.smtp.password).c_str(), path.c_str());
    WritePrivateProfileStringA("SMTP", "Recipient", m_config.smtp.recipient.c_str(), path.c_str());

    // Rule settings
    const NotifyRule& rule = m_config.GetRule();
    WritePrivateProfileStringA("Rule_0", "Enabled", rule.enabled ? "1" : "0", path.c_str());

    sprintf_s(buffer, "%d", (int)rule.triggerType);
    WritePrivateProfileStringA("Rule_0", "TriggerType", buffer, path.c_str());

    sprintf_s(buffer, "%d", rule.columnIndex);
    WritePrivateProfileStringA("Rule_0", "ColumnIndex", buffer, path.c_str());

    WritePrivateProfileStringA("Rule_0", "MatchPattern", rule.matchPattern.c_str(), path.c_str());
}

bool NotifyManager::ShouldNotify(context* ctx, std::string& outMatchedKeyword, const CString& remark)
{
    if (!m_powerShellAvailable) return false;

    std::lock_guard<std::mutex> lock(m_mutex);

    const NotifyRule& rule = m_config.GetRule();
    if (!rule.enabled) return false;
    if (rule.triggerType != NOTIFY_TRIGGER_HOST_ONLINE) return false;
    if (rule.matchPattern.empty()) return false;
    if (!m_config.smtp.IsValid()) return false;

    uint64_t clientId = ctx->GetClientID();
    time_t now = time(nullptr);

    // Cooldown check
    auto it = m_config.lastNotifyTime.find(clientId);
    if (it != m_config.lastNotifyTime.end()) {
        time_t elapsed = now - it->second;
        if (elapsed < NOTIFY_COOLDOWN_MINUTES * 60) {
            return false;  // Still in cooldown period
        }
    }

    // Get column text (for COMPUTER_NAME column, prefer remark if available)
    CString colText;
    if (rule.columnIndex == ONLINELIST_COMPUTER_NAME && !remark.IsEmpty()) {
        colText = remark;
    } else {
        colText = ctx->GetClientData(rule.columnIndex);
    }
    if (colText.IsEmpty()) return false;

    // Convert to std::string for matching
    std::string colTextStr = CT2A(colText, CP_UTF8);

    // Split pattern by semicolon and check each keyword
    std::vector<std::string> keywords = SplitString(rule.matchPattern, ';');
    for (const auto& kw : keywords) {
        std::string trimmed = Trim(kw);
        if (trimmed.empty()) continue;

        // Case-insensitive substring search
        std::string colLower = colTextStr;
        std::string kwLower = trimmed;
        std::transform(colLower.begin(), colLower.end(), colLower.begin(), ::tolower);
        std::transform(kwLower.begin(), kwLower.end(), kwLower.begin(), ::tolower);

        if (colLower.find(kwLower) != std::string::npos) {
            outMatchedKeyword = trimmed;
            m_config.lastNotifyTime[clientId] = now;
            return true;
        }
    }

    return false;
}

void NotifyManager::BuildHostOnlineEmail(context* ctx, const std::string& matchedKeyword,
                                         std::string& outSubject, std::string& outBody)
{
    // Copy rule info under lock
    int columnIndex;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        columnIndex = m_config.GetRule().columnIndex;
    }

    // Get host info
    std::string computerName = CT2A(ctx->GetClientData(ONLINELIST_COMPUTER_NAME), CP_UTF8);
    std::string ip = CT2A(ctx->GetClientData(ONLINELIST_IP), CP_UTF8);
    std::string location = CT2A(ctx->GetClientData(ONLINELIST_LOCATION), CP_UTF8);
    std::string os = CT2A(ctx->GetClientData(ONLINELIST_OS), CP_UTF8);
    std::string version = CT2A(ctx->GetClientData(ONLINELIST_VERSION), CP_UTF8);

    // Get current time
    time_t now = time(nullptr);
    char timeStr[64];
    struct tm tm_info;
    localtime_s(&tm_info, &now);
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &tm_info);

    // Build subject
    std::ostringstream ss;
    ss << "[SimpleRemoter] Host Online: " << computerName << " matched \"" << matchedKeyword << "\"";
    outSubject = ss.str();

    // Build body (HTML format)
    ss.str("");
    ss << "<b>Host Online Notification</b><br><br>";
    ss << "Trigger Time: " << timeStr << "<br>";
    ss << "Match Rule: " << GetColumnName(columnIndex)
       << " contains \"" << matchedKeyword << "\"<br><br>";
    ss << "<b>Host Information:</b><br>";
    ss << "&nbsp;&nbsp;IP Address: " << ip << "<br>";
    ss << "&nbsp;&nbsp;Location: " << location << "<br>";
    ss << "&nbsp;&nbsp;Computer Name: " << computerName << "<br>";
    ss << "&nbsp;&nbsp;OS: " << os << "<br>";
    ss << "&nbsp;&nbsp;Version: " << version << "<br>";
    ss << "<br>--<br><i>This email was sent automatically by SimpleRemoter</i>";

    outBody = ss.str();
}

void NotifyManager::SendNotifyEmailAsync(const std::string& subject, const std::string& body)
{
    if (!m_powerShellAvailable) return;

    // Copy SMTP config under lock
    SmtpConfig smtp;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_config.smtp.IsValid()) return;
        smtp = m_config.smtp;
    }

    std::string subjectCopy = subject;
    std::string bodyCopy = body;

    std::thread([this, smtp, subjectCopy, bodyCopy]() {
        // Build PowerShell command
        std::ostringstream ps;
        ps << "powershell -NoProfile -ExecutionPolicy Bypass -Command \"";
        ps << "$pass = ConvertTo-SecureString '" << EscapePowerShell(smtp.password) << "' -AsPlainText -Force; ";
        ps << "$cred = New-Object PSCredential('" << EscapePowerShell(smtp.username) << "', $pass); ";
        ps << "Send-MailMessage ";
        ps << "-From '" << EscapePowerShell(smtp.username) << "' ";
        ps << "-To '" << EscapePowerShell(smtp.GetRecipient()) << "' ";
        ps << "-Subject '" << EscapePowerShell(subjectCopy) << "' ";
        ps << "-Body '" << EscapePowerShell(bodyCopy) << "' ";
        ps << "-SmtpServer '" << EscapePowerShell(smtp.server) << "' ";
        ps << "-Port " << smtp.port << " ";
        if (smtp.useSSL) {
            ps << "-UseSsl ";
        }
        ps << "-Credential $cred ";
        ps << "-Encoding UTF8 -BodyAsHtml\"";

        DWORD exitCode;
        ExecutePowerShell(ps.str(), &exitCode, true);
    }).detach();
}

std::string NotifyManager::SendTestEmail()
{
    if (!m_powerShellAvailable) {
        return "PowerShell is not available. Requires Windows 10 or later.";
    }

    // Copy SMTP config under lock
    SmtpConfig smtp;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_config.smtp.IsValid()) {
            return "SMTP configuration is incomplete.";
        }
        smtp = m_config.smtp;
    }

    std::string subject = "[SimpleRemoter] Test Email";
    std::string body = "This is a test email from SimpleRemoter notification system.<br><br>If you received this email, the configuration is correct.";

    // Build PowerShell command - output error to temp file for capture
    char tempPath[MAX_PATH], tempFile[MAX_PATH];
    GetTempPathA(MAX_PATH, tempPath);
    GetTempFileNameA(tempPath, "notify", 0, tempFile);

    std::ostringstream ps;
    ps << "powershell -NoProfile -ExecutionPolicy Bypass -Command \"";
    ps << "$ErrorActionPreference = 'Stop'; ";
    ps << "try { ";
    ps << "$pass = ConvertTo-SecureString '" << EscapePowerShell(smtp.password) << "' -AsPlainText -Force; ";
    ps << "$cred = New-Object PSCredential('" << EscapePowerShell(smtp.username) << "', $pass); ";
    ps << "Send-MailMessage ";
    ps << "-From '" << EscapePowerShell(smtp.username) << "' ";
    ps << "-To '" << EscapePowerShell(smtp.GetRecipient()) << "' ";
    ps << "-Subject '" << EscapePowerShell(subject) << "' ";
    ps << "-Body '" << EscapePowerShell(body) << "' ";
    ps << "-SmtpServer '" << EscapePowerShell(smtp.server) << "' ";
    ps << "-Port " << smtp.port << " ";
    if (smtp.useSSL) {
        ps << "-UseSsl ";
    }
    ps << "-Credential $cred ";
    ps << "-Encoding UTF8 -BodyAsHtml; ";
    ps << "'SUCCESS' | Out-File -FilePath '" << tempFile << "' -Encoding UTF8 ";
    ps << "} catch { $_.Exception.Message | Out-File -FilePath '" << tempFile << "' -Encoding UTF8; exit 1 }\"";

    DWORD exitCode = 1;
    ExecutePowerShell(ps.str(), &exitCode, true);

    // Read result from temp file (skip UTF-8 BOM if present)
    std::string result;
    std::ifstream ifs(tempFile, std::ios::binary);
    if (ifs.is_open()) {
        std::getline(ifs, result);
        // Skip UTF-8 BOM (EF BB BF) or UTF-16 LE BOM (FF FE)
        if (result.size() >= 3 && (unsigned char)result[0] == 0xEF &&
            (unsigned char)result[1] == 0xBB && (unsigned char)result[2] == 0xBF) {
            result = result.substr(3);
        } else if (result.size() >= 2 && (unsigned char)result[0] == 0xFF &&
                   (unsigned char)result[1] == 0xFE) {
            // UTF-16 LE - convert to ASCII (simple case)
            std::string converted;
            for (size_t i = 2; i < result.size(); i += 2) {
                if (result[i] != 0) converted += result[i];
            }
            result = converted;
        }
        ifs.close();
    }
    DeleteFileA(tempFile);

    if (exitCode == 0 && result.find("SUCCESS") != std::string::npos) {
        return "success";
    } else {
        // Log detailed error for debugging
        if (result.empty()) {
            TRACE("[Notify] SendTestEmail failed, exit code: %d\n", exitCode);
        } else {
            TRACE("[Notify] SendTestEmail failed: %s\n", result.c_str());
        }
        return "failed";
    }
}

bool NotifyManager::ExecutePowerShell(const std::string& command, DWORD* exitCode, bool hidden)
{
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi = { 0 };

    if (hidden) {
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
    }

    // Create command buffer (must be modifiable for CreateProcessA)
    std::vector<char> cmdBuffer(command.begin(), command.end());
    cmdBuffer.push_back('\0');

    BOOL result = CreateProcessA(
        NULL,
        cmdBuffer.data(),
        NULL, NULL,
        FALSE,
        hidden ? CREATE_NO_WINDOW : 0,
        NULL, NULL,
        &si, &pi
    );

    if (!result) {
        if (exitCode) *exitCode = GetLastError();
        return false;
    }

    // Wait for process to complete (with timeout for test email)
    WaitForSingleObject(pi.hProcess, 30000);  // 30 second timeout

    if (exitCode) {
        GetExitCodeProcess(pi.hProcess, exitCode);
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return true;
}

std::string NotifyManager::EscapePowerShell(const std::string& str)
{
    std::string result;
    result.reserve(str.size() * 2);

    for (char c : str) {
        if (c == '\'') {
            result += "''";  // Escape single quote by doubling
        } else if (c == '\n') {
            result += "`n";  // PowerShell newline escape
        } else if (c == '\r') {
            result += "`r";
        } else {
            result += c;
        }
    }

    return result;
}

std::string NotifyManager::EncryptPassword(const std::string& password)
{
    // Simple XOR obfuscation (not secure, just prevents casual reading)
    const char key[] = "YamaNotify2026";
    std::string result;
    result.reserve(password.size() * 2);

    for (size_t i = 0; i < password.size(); i++) {
        char c = password[i] ^ key[i % (sizeof(key) - 1)];
        char hex[3];
        sprintf_s(hex, "%02X", (unsigned char)c);
        result += hex;
    }

    return result;
}

std::string NotifyManager::DecryptPassword(const std::string& encrypted)
{
    if (encrypted.empty() || encrypted.size() % 2 != 0) {
        return "";
    }

    const char key[] = "YamaNotify2026";
    std::string result;
    result.reserve(encrypted.size() / 2);

    for (size_t i = 0; i < encrypted.size(); i += 2) {
        char hex[3] = { encrypted[i], encrypted[i + 1], 0 };
        char c = (char)strtol(hex, nullptr, 16);
        c ^= key[(i / 2) % (sizeof(key) - 1)];
        result += c;
    }

    return result;
}

std::vector<std::string> NotifyManager::SplitString(const std::string& str, char delimiter)
{
    std::vector<std::string> tokens;
    std::istringstream stream(str);
    std::string token;

    while (std::getline(stream, token, delimiter)) {
        tokens.push_back(token);
    }

    return tokens;
}

std::string NotifyManager::Trim(const std::string& str)
{
    size_t start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";

    size_t end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}
