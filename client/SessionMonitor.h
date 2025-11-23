#ifndef SESSION_MONITOR_H
#define SESSION_MONITOR_H

#include <windows.h>
#include <wtsapi32.h>

#ifdef __cplusplus
extern "C" {
#endif

#pragma comment(lib, "wtsapi32.lib")

// 代理进程信息
typedef struct AgentProcessInfo {
    DWORD processId;
    DWORD sessionId;
    HANDLE hProcess;
} AgentProcessInfo;

// 代理进程数组（动态数组）
typedef struct AgentProcessArray {
    AgentProcessInfo* items;
    size_t count;
    size_t capacity;
} AgentProcessArray;

// 会话监控器结构
typedef struct SessionMonitor {
    HANDLE monitorThread;
    BOOL running;
    CRITICAL_SECTION csProcessList;
    AgentProcessArray agentProcesses;
} SessionMonitor;

// 初始化会话监控器
void SessionMonitor_Init(SessionMonitor* self);

// 清理会话监控器资源
void SessionMonitor_Cleanup(SessionMonitor* self);

// 启动会话监控
BOOL SessionMonitor_Start(SessionMonitor* self);

// 停止会话监控
void SessionMonitor_Stop(SessionMonitor* self);

#ifdef __cplusplus
}
#endif

#endif /* SESSION_MONITOR_H */
