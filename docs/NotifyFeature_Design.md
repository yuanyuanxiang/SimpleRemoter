# 通知功能设计方案

## 概述

在主对话框菜单"菜单(&F)"下增加"通知(&N)"功能，支持配置 SMTP 邮件服务，当主机上线且匹配指定条件时自动发送邮件通知。

## 系统要求

- **PowerShell 5.1+**: 内置于 Windows 10 / Server 2016+
- Windows 7/8.1 需手动安装 WMF 5.1
- 不支持 PowerShell 的系统将禁用此功能（对话框控件灰显）

## 核心特性

1. **SMTP 配置**: 支持 Gmail 等主流邮件服务（需应用专用密码）
2. **触发规则**: 主机上线时匹配指定列的文本
3. **备注优先**: 匹配"计算机名"列时，优先使用备注（如有）
4. **多关键词匹配**: 分号分隔，任一匹配即触发（大小写不敏感）
5. **频率控制**: 同一主机 60 分钟内只通知一次
6. **线程安全**: 使用 mutex 保护配置访问
7. **异步发送**: 邮件发送不阻塞主线程
8. **多语言支持**: 支持简体中文、英文、繁体中文

---

## 数据结构

### 常量定义

```cpp
#define NOTIFY_COOLDOWN_MINUTES  15  // 同一主机通知冷却时间（分钟）
```

### 通知类型枚举

```cpp
enum NotifyTriggerType {
    NOTIFY_TRIGGER_NONE = 0,
    NOTIFY_TRIGGER_HOST_ONLINE = 1,   // 主机上线
    // 未来可扩展:
    // NOTIFY_TRIGGER_HOST_OFFLINE = 2,
    // NOTIFY_TRIGGER_FILE_TRANSFER = 3,
};
```

### 单条通知规则

```cpp
struct NotifyRule {
    bool enabled;                      // 是否启用
    NotifyTriggerType triggerType;     // 触发类型
    int columnIndex;                   // 列编号 (0-based)
    std::string matchPattern;          // 匹配字符串，分号分隔
};
```

### SMTP 配置

```cpp
struct SmtpConfig {
    std::string server;          // smtp.gmail.com
    int port;                    // 587
    bool useSSL;                 // true
    std::string username;        // 发件人邮箱
    std::string password;        // 应用专用密码 (XOR 加密存储)
    std::string recipient;       // 收件人 (可选，为空则发给自己)

    // 获取实际收件人
    std::string GetRecipient() const {
        return recipient.empty() ? username : recipient;
    }
};
```

### 完整通知配置

```cpp
struct NotifyConfig {
    SmtpConfig smtp;
    std::vector<NotifyRule> rules;     // 规则列表，支持多条

    // 频率控制：记录每个主机最后通知时间 (仅内存，不持久化)
    std::unordered_map<uint64_t, time_t> lastNotifyTime;
};
```

---

## 配置对话框 UI

```
┌─ 通知设置 ──────────────────────────────────────────────────┐
│                                                              │
│  ⚠️ Warning: Requires Windows 10 or later with PowerShell   │
│     5.1+  (仅不支持时显示)                                   │
│                                                              │
│  ┌─ SMTP 配置 ────────────────────────────────────────────┐ │
│  │ 服务器:   [smtp.gmail.com        ] 端口: [587 ]        │ │
│  │ ☑ 使用 SSL/TLS                                         │ │
│  │ 用户名:   [your@gmail.com        ]                     │ │
│  │ 密码:     [****************      ]                     │ │
│  │           (Gmail 需使用应用专用密码)                    │ │
│  │ 收件人:   [                      ] [测试]              │ │
│  └────────────────────────────────────────────────────────┘ │
│                                                              │
│  ┌─ 通知规则 ────────────────────────────────────────────┐  │
│  │ ☑ 启用通知                                            │  │
│  │ 触发条件: [主机上线           ▼]                      │  │
│  │ 匹配列:   [3 - 计算机名       ▼]                      │  │
│  │ 关键词:   [CEO;CFO;财务;服务器 ]                       │  │
│  │ (多个关键词用分号分隔，匹配任一项即触发通知)           │  │
│  │ 提示: 同一主机 60 分钟内仅通知一次                     │  │
│  └────────────────────────────────────────────────────────┘  │
│                                                              │
│                              [确定]  [取消]                  │
└──────────────────────────────────────────────────────────────┘
```

**备注说明**: 收件人为空时，邮件发送给发件人自己（自我通知）。

---

## 邮件内容格式

邮件使用 HTML 格式发送 (`-BodyAsHtml`)。

### 主题

```
[SimpleRemoter] Host Online: WIN-PC01 matched "服务器"
```

### 正文 (HTML)

```html
<b>Host Online Notification</b><br><br>
Trigger Time: 2026-03-11 15:30:45<br>
Match Rule: ComputerName contains "服务器"<br><br>
<b>Host Information:</b><br>
&nbsp;&nbsp;IP Address: 192.168.1.100<br>
&nbsp;&nbsp;Location: 中国-北京<br>
&nbsp;&nbsp;Computer Name: WIN-SERVER-01<br>
&nbsp;&nbsp;OS: Windows Server 2022<br>
&nbsp;&nbsp;Version: 1.2.7<br>
<br>--<br><i>This email was sent automatically by SimpleRemoter</i>
```

---

## 配置持久化

**存储位置**: `%APPDATA%\YAMA\notify.ini`

```ini
[SMTP]
Server=smtp.gmail.com
Port=587
UseSSL=1
Username=your@gmail.com
Password=<XOR加密后的字符串>
Recipient=

[Rule_0]
Enabled=1
TriggerType=1
ColumnIndex=3
MatchPattern=CEO;CFO;服务器

; 未来扩展多规则
;[Rule_1]
;Enabled=1
;...
```

**注意**: `lastNotifyTime` 仅保存在内存中，服务端重启后清空。

---

## 核心逻辑

### PowerShell 检测

```cpp
bool DetectPowerShellSupport() {
    std::string cmd = "powershell -NoProfile -Command "
                      "\"Get-Command Send-MailMessage -ErrorAction SilentlyContinue\"";
    DWORD exitCode = 1;
    ExecutePowerShell(cmd, &exitCode, true);  // hidden
    return (exitCode == 0);
}
```

### 频率控制 + 匹配检查 (线程安全)

```cpp
bool NotifyManager::ShouldNotify(context* ctx, std::string& outMatchedKeyword,
                                  const CString& remark)
{
    if (!m_powerShellAvailable) return false;

    std::lock_guard<std::mutex> lock(m_mutex);

    const NotifyRule& rule = m_config.GetRule();
    if (!rule.enabled) return false;
    if (rule.matchPattern.empty()) return false;
    if (!m_config.smtp.IsValid()) return false;

    uint64_t clientId = ctx->GetClientID();
    time_t now = time(nullptr);

    // 冷却检查
    auto it = m_config.lastNotifyTime.find(clientId);
    if (it != m_config.lastNotifyTime.end()) {
        if (now - it->second < NOTIFY_COOLDOWN_MINUTES * 60) {
            return false;
        }
    }

    // 获取匹配文本 (计算机名列优先使用备注)
    CString colText;
    if (rule.columnIndex == ONLINELIST_COMPUTER_NAME && !remark.IsEmpty()) {
        colText = remark;
    } else {
        colText = ctx->GetClientData(rule.columnIndex);
    }
    if (colText.IsEmpty()) return false;

    // 大小写不敏感匹配
    std::string colLower = ToLower(CT2A(colText, CP_UTF8));
    for (const auto& kw : SplitString(rule.matchPattern, ';')) {
        std::string kwLower = ToLower(Trim(kw));
        if (!kwLower.empty() && colLower.find(kwLower) != std::string::npos) {
            outMatchedKeyword = Trim(kw);
            m_config.lastNotifyTime[clientId] = now;
            return true;
        }
    }
    return false;
}
```

### 异步邮件发送

```cpp
void NotifyManager::SendNotifyEmailAsync(const std::string& subject,
                                          const std::string& body)
{
    if (!m_powerShellAvailable) return;

    SmtpConfig smtp;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_config.smtp.IsValid()) return;
        smtp = m_config.smtp;
    }

    std::thread([this, smtp, subject, body]() {
        std::ostringstream ps;
        ps << "powershell -NoProfile -ExecutionPolicy Bypass -Command \"";
        ps << "$pass = ConvertTo-SecureString '" << EscapePowerShell(smtp.password)
           << "' -AsPlainText -Force; ";
        ps << "$cred = New-Object PSCredential('" << smtp.username << "', $pass); ";
        ps << "Send-MailMessage ";
        ps << "-From '" << smtp.username << "' ";
        ps << "-To '" << smtp.GetRecipient() << "' ";
        ps << "-Subject '" << EscapePowerShell(subject) << "' ";
        ps << "-Body '" << EscapePowerShell(body) << "' ";
        ps << "-SmtpServer '" << smtp.server << "' ";
        ps << "-Port " << smtp.port << " ";
        if (smtp.useSSL) ps << "-UseSsl ";
        ps << "-Credential $cred -Encoding UTF8 -BodyAsHtml\"";

        DWORD exitCode;
        ExecutePowerShell(ps.str(), &exitCode, true);  // hidden
    }).detach();
}
```

---

## 集成点

在 `2015RemoteDlg.cpp` 的 `AddList()` 函数中:

```cpp
// 现有代码
m_ClientIndex[id] = m_HostList.size();
m_HostList.push_back(ContextObject);

// ========== 新增：通知检查 (带异常保护) ==========
try {
    std::string matchedKeyword;
    CString remark = m_ClientMap->GetClientMapData(ContextObject->GetClientID(), MAP_NOTE);
    if (GetNotifyManager().ShouldNotify(ContextObject, matchedKeyword, remark)) {
        std::string subject, body;
        GetNotifyManager().BuildHostOnlineEmail(ContextObject, matchedKeyword, subject, body);
        GetNotifyManager().SendNotifyEmailAsync(subject, body);
    }
} catch (...) {
    TRACE("[Notify] Exception in notification check\n");
}
// =================================================

ShowMessage(_TR("操作成功"), ...);
```

初始化在 `OnInitDialog()` 中:

```cpp
m_ClientMap->LoadFromFile(GetDbPath());

// 初始化通知管理器
GetNotifyManager().Initialize();
```

---

## 文件改动清单

| 文件 | 改动类型 | 说明 |
|------|---------|------|
| `resource.h` | 修改 | 新增菜单ID、对话框ID、控件ID (33028-33029, 332, 2408-2432) |
| `2015Remote.rc` | 修改 | 添加菜单项、对话框模板、右键菜单"上线提醒" |
| `NotifyConfig.h` | **新增** | 配置结构体定义 |
| `NotifySettingsDlg.h` | **新增** | 配置对话框类声明 (继承 CDialogLangEx) |
| `NotifySettingsDlg.cpp` | **新增** | 配置对话框实现，显式设置控件文本支持翻译 |
| `NotifyManager.h` | **新增** | 通知管理器声明 (单例，线程安全) |
| `NotifyManager.cpp` | **新增** | 检测、发送、配置读写逻辑 |
| `2015RemoteDlg.h` | 修改 | 添加 `OnMenuNotifySettings()`, `OnOnlineLoginNotify()` 方法 |
| `2015RemoteDlg.cpp` | 修改 | 菜单处理 + 上线时调用检查 + 右键快捷添加主机 |
| `lang/en_US.ini` | 修改 | 添加英文翻译 |
| `lang/zh_TW.ini` | 修改 | 添加繁体中文翻译 |

**实际代码量**: 约 1400+ 行

---

## 多语言支持

### 语言文件编码

**重要**: 语言文件使用 **GB2312** 编码，不是 UTF-8！

- 位置: `server/2015Remote/lang/*.ini`
- 格式: `简体中文=翻译文本`

### 源文件编码

含中文字符的 C++ 源文件必须使用 **UTF-8 with BOM**，否则 MSVC 会报错 `C2001: 常量中有换行符`。

### 翻译示例

```ini
; en_US.ini
通知设置=Notify Settings
SMTP 配置=SMTP Configuration
测试邮件发送成功!=Test email sent successfully!
测试邮件发送失败，请检查SMTP配置=Test email failed. Please check SMTP settings.

; zh_TW.ini
通知设置=通知設定
SMTP 配置=SMTP 配置
测试邮件发送成功!=測試郵件發送成功!
```

---

## 测试邮件

- **成功**: 显示 "测试邮件发送成功!"
- **失败**: 显示 "测试邮件发送失败，请检查SMTP配置"
- 详细错误信息使用 `TRACE()` 记录到调试日志

---

## 安全考虑

| 风险 | 处理方式 |
|------|---------|
| 密码明文存储 | XOR 混淆后存储 (非安全加密，仅防止直接可见) |
| PowerShell 注入 | 转义单引号 `'` → `''`，转义换行 |
| 邮件发送失败 | 静默失败，异步执行不影响主流程 |
| 并发访问 | 使用 `std::mutex` 保护配置读写 |
| 通知异常 | try-catch 包裹，异常不影响主机上线 |

---

## 列编号与匹配说明

| 列编号 | 列名称 | 匹配说明 |
|--------|--------|----------|
| 0 | IP地址 | 匹配原始 IP |
| 1 | 地址 | 匹配端口号 |
| 2 | 地理位置 | 匹配位置信息 |
| 3 | 计算机名 | **优先匹配备注**，无备注时匹配计算机名 |
| 4 | 操作系统 | 匹配 OS 信息 |
| 5 | CPU | 匹配 CPU 信息 |
| 6 | 摄像头 | 匹配有/无 |
| 7 | 延迟 | 匹配 RTT 值 |
| 8 | 版本 | 匹配客户端版本 |
| 9 | 安装时间 | 匹配安装时间 |
| 10 | 活动窗口 | 匹配当前窗口标题 |
| 11 | 客户端类型 | 匹配类型标识 |

---

## 使用示例

1. 打开菜单 "菜单(&F)" → "通知(&N)"
2. 配置 SMTP:
   - 服务器: `smtp.gmail.com`
   - 端口: `587`
   - 勾选 SSL/TLS
   - 用户名: 你的 Gmail 地址
   - 密码: [Gmail 应用专用密码](https://myaccount.google.com/apppasswords)
3. 点击"测试"验证配置
4. 配置规则:
   - 勾选"启用通知"
   - 选择"计算机名"列
   - 输入关键词: `CEO;CFO;财务部`
5. 点击确定保存

当有主机上线且其备注或计算机名包含任一关键词时，你将收到邮件通知。

### 快捷添加主机

右键点击在线主机列表中的主机，选择"上线提醒"可快速将该主机添加到通知关键词列表：

- 自动使用主机备注（如有）或计算机名作为关键词
- 自动去重，已存在的主机不会重复添加
- 添加后自动启用通知规则
