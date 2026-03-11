#pragma once

#include "NotifyConfig.h"
#include <functional>
#include <mutex>

// Forward declaration
class context;

class NotifyManager {
public:
    static NotifyManager& Instance();

    // Initialize the manager (call once at startup)
    void Initialize();

    // Check if PowerShell is available for sending emails
    bool IsPowerShellAvailable() const { return m_powerShellAvailable; }

    // Get/Set configuration (thread-safe copy)
    NotifyConfig GetConfig();
    void SetConfig(const NotifyConfig& config);

    // Load/Save configuration
    void LoadConfig();
    void SaveConfig();

    // Check if notification should be sent for this host
    // If yes, outMatchedKeyword will contain the matched keyword
    // remark: optional host remark (used for COMPUTER_NAME column if not empty)
    bool ShouldNotify(context* ctx, std::string& outMatchedKeyword, const CString& remark = _T(""));

    // Send notification email asynchronously
    // subject and body should be UTF-8 encoded
    void SendNotifyEmailAsync(const std::string& subject, const std::string& body);

    // Send a test email (synchronous, returns success/error message)
    std::string SendTestEmail();

    // Build notification email content for host online event
    void BuildHostOnlineEmail(context* ctx, const std::string& matchedKeyword,
                              std::string& outSubject, std::string& outBody);

private:
    NotifyManager();
    ~NotifyManager() = default;
    NotifyManager(const NotifyManager&) = delete;
    NotifyManager& operator=(const NotifyManager&) = delete;

    // Detect if PowerShell Send-MailMessage is available
    bool DetectPowerShellSupport();

    // Get config file path
    std::string GetConfigPath() const;

    // Simple XOR encryption for password (not secure, just obfuscation)
    std::string EncryptPassword(const std::string& password);
    std::string DecryptPassword(const std::string& encrypted);

    // Escape special characters for PowerShell string
    std::string EscapePowerShell(const std::string& str);

    // Execute PowerShell command and get exit code
    bool ExecutePowerShell(const std::string& command, DWORD* exitCode = nullptr, bool hidden = true);

    // Split string by delimiter
    std::vector<std::string> SplitString(const std::string& str, char delimiter);

    // Trim whitespace from string
    std::string Trim(const std::string& str);

private:
    bool m_initialized;
    bool m_powerShellAvailable;
    NotifyConfig m_config;
    mutable std::mutex m_mutex;  // Protects m_config access
};

// Convenience function
inline NotifyManager& GetNotifyManager() {
    return NotifyManager::Instance();
}
