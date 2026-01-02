#ifndef SERVER_SESSION_MONITOR_H
#define SERVER_SESSION_MONITOR_H

#include <windows.h>
#include <wtsapi32.h>

#ifdef __cplusplus
extern "C" {
#endif

#pragma comment(lib, "wtsapi32.lib")

// GUI杩涚▼淇℃伅
typedef struct ServerAgentProcessInfo {
    DWORD processId;
    DWORD sessionId;
    HANDLE hProcess;
} ServerAgentProcessInfo;

// GUI杩涚▼鏁扮粍锛堝姩鎬佹暟缁勶級
typedef struct ServerAgentProcessArray {
    ServerAgentProcessInfo* items;
    size_t count;
    size_t capacity;
} ServerAgentProcessArray;

// 浼氳瘽鐩戞帶鍣ㄧ粨鏋?
typedef struct ServerSessionMonitor {
    HANDLE monitorThread;
    BOOL running;
    CRITICAL_SECTION csProcessList;
    ServerAgentProcessArray agentProcesses;
} ServerSessionMonitor;

// 鍒濆鍖栦細璇濈洃鎺у櫒
void ServerSessionMonitor_Init(ServerSessionMonitor* self);

// 娓呯悊浼氳瘽鐩戞帶鍣ㄨ祫婧?
void ServerSessionMonitor_Cleanup(ServerSessionMonitor* self);

// 鍚姩浼氳瘽鐩戞帶
BOOL ServerSessionMonitor_Start(ServerSessionMonitor* self);

// 鍋滄浼氳瘽鐩戞帶
void ServerSessionMonitor_Stop(ServerSessionMonitor* self);

#ifdef __cplusplus
}
#endif

#endif /* SERVER_SESSION_MONITOR_H */
