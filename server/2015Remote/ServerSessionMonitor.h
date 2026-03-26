#ifndef SERVER_SESSION_MONITOR_H
#define SERVER_SESSION_MONITOR_H

#include <windows.h>
#include <wtsapi32.h>

#ifdef __cplusplus
extern "C" {
#endif

#pragma comment(lib, "wtsapi32.lib")

// GUI进程信息
typedef struct ServerAgentProcessInfo {
    DWORD processId;
    DWORD sessionId;
    HANDLE hProcess;
    ULONGLONG launchTime;  // 启动时间 (GetTickCount64)
} ServerAgentProcessInfo;

// GUI进程数组（动态数组）
typedef struct ServerAgentProcessArray {
    ServerAgentProcessInfo* items;
    size_t count;
    size_t capacity;
} ServerAgentProcessArray;

// 崩溃保护参数
#define CRASH_WINDOW_MS      (5 * 60 * 1000)  // 5分钟窗口
#define CRASH_THRESHOLD      3                 // 3次崩溃触发保护
#define FAST_CRASH_TIME_MS   30000             // 30秒内退出视为快速崩溃

// 回调函数类型
typedef void (*AgentStartCallback)(DWORD processId, DWORD sessionId);  // 代理启动回调
typedef void (*AgentExitCallback)(DWORD exitCode, ULONGLONG runtimeMs);  // 代理退出回调（每次退出，包括正常和崩溃）
typedef void (*CrashCallback)(DWORD exitCode, ULONGLONG runtimeMs);  // 崩溃回调，带退出代码和运行时间
typedef void (*CrashWindowCallback)(int crashCount, ULONGLONG firstCrashTime);  // 崩溃窗口状态变化回调
typedef void (*CrashProtectionCallback)(void);  // 崩溃保护回调

// 会话监控器结构
typedef struct ServerSessionMonitor {
    HANDLE monitorThread;
    BOOL running;
    BOOL runAsUser;  // TRUE: 以用户身份运行代理, FALSE: 以SYSTEM身份运行代理
    CRITICAL_SECTION csProcessList;
    ServerAgentProcessArray agentProcesses;
    // 崩溃保护
    int crashCount;           // 窗口期内崩溃次数
    ULONGLONG firstCrashTime; // 第一次崩溃时间（窗口起点）
    BOOL crashProtected;      // 是否已触发崩溃保护
    // 回调函数
    AgentStartCallback onAgentStart;            // 代理启动时的回调（用于统计启动次数）
    AgentExitCallback onAgentExit;              // 代理退出时的回调（用于统计运行时间）
    CrashCallback onCrash;                      // 每次崩溃时的回调（用于统计）
    CrashWindowCallback onCrashWindowChange;    // 崩溃窗口状态变化时的回调（用于持久化）
    CrashProtectionCallback onCrashProtection;  // 崩溃保护触发时的回调
} ServerSessionMonitor;

// 初始化会话监控器
void ServerSessionMonitor_Init(ServerSessionMonitor* self);

// 清理会话监控器资源
void ServerSessionMonitor_Cleanup(ServerSessionMonitor* self);

// 启动会话监控
BOOL ServerSessionMonitor_Start(ServerSessionMonitor* self);

// 停止会话监控
void ServerSessionMonitor_Stop(ServerSessionMonitor* self);

#ifdef __cplusplus
}
#endif

#endif /* SERVER_SESSION_MONITOR_H */
