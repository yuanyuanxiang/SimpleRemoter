#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <ctime>

// Notification cooldown period (minutes)
// Same host will only trigger one notification within this period
#define NOTIFY_COOLDOWN_MINUTES  60

// Notification trigger types
enum NotifyTriggerType {
    NOTIFY_TRIGGER_NONE = 0,
    NOTIFY_TRIGGER_HOST_ONLINE = 1,   // Host comes online
    // Future extensions:
    // NOTIFY_TRIGGER_HOST_OFFLINE = 2,
    // NOTIFY_TRIGGER_FILE_TRANSFER = 3,
};

// Single notification rule
struct NotifyRule {
    bool enabled;                      // Whether this rule is enabled
    NotifyTriggerType triggerType;     // Trigger type
    int columnIndex;                   // Column index (0-based)
    std::string matchPattern;          // Match pattern (semicolon-separated keywords)

    NotifyRule()
        : enabled(false)
        , triggerType(NOTIFY_TRIGGER_NONE)
        , columnIndex(0)
    {}
};

// SMTP configuration
struct SmtpConfig {
    std::string server;          // smtp.gmail.com
    int port;                    // 587
    bool useSSL;                 // true for TLS
    std::string username;        // sender email
    std::string password;        // app-specific password (encrypted in storage)
    std::string recipient;       // recipient email

    SmtpConfig()
        : port(587)
        , useSSL(true)
    {}

    bool IsValid() const {
        return !server.empty() && port > 0 && !username.empty() && !password.empty();
    }

    // Get effective recipient (fallback to username if empty)
    std::string GetRecipient() const {
        return recipient.empty() ? username : recipient;
    }
};

// Complete notification configuration
struct NotifyConfig {
    SmtpConfig smtp;
    std::vector<NotifyRule> rules;     // Rule list (supports multiple rules in future)

    // Frequency control: record last notification time for each host
    // key = clientID, value = last notification timestamp
    std::unordered_map<uint64_t, time_t> lastNotifyTime;

    NotifyConfig() {
        // Initialize with one default rule
        rules.push_back(NotifyRule());
    }

    // Get the first (and currently only) rule
    NotifyRule& GetRule() {
        if (rules.empty()) {
            rules.push_back(NotifyRule());
        }
        return rules[0];
    }

    const NotifyRule& GetRule() const {
        static NotifyRule emptyRule;
        return rules.empty() ? emptyRule : rules[0];
    }

    // Check if any rule is enabled
    bool HasEnabledRule() const {
        for (const auto& rule : rules) {
            if (rule.enabled) return true;
        }
        return false;
    }
};

// Column name mapping for UI
inline const char* GetColumnName(int index) {
    static const char* names[] = {
        "IP",              // 0
        "Address",              // 1
        "Location",             // 2
        "ComputerName",    // 3
        "OS",         // 4
        "CPU",                  // 5
        "Camera",            // 6
        "RTT",                  // 7
        "Version",              // 8
        "InstallTime",        // 9
        "ActiveWindow",       // 10
        "ClientType",       // 11
    };
    if (index >= 0 && index < sizeof(names)/sizeof(names[0])) {
        return names[index];
    }
    return "Unknown";
}

// Trigger type name mapping for UI
inline const char* GetTriggerTypeName(NotifyTriggerType type) {
    switch (type) {
        case NOTIFY_TRIGGER_HOST_ONLINE: return "Host Online";
        default: return "None";
    }
}
