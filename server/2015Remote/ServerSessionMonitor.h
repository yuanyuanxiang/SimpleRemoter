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
} ServerAgentProcessInfo;

// GUI进程数组（动态数组）
typedef struct ServerAgentProcessArray {
    ServerAgentProcessInfo* items;
    size_t count;
    size_t capacity;
} ServerAgentProcessArray;

// 会话监控器结构
typedef struct ServerSessionMonitor {
    HANDLE monitorThread;
    BOOL running;
    CRITICAL_SECTION csProcessList;
    ServerAgentProcessArray agentProcesses;
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
