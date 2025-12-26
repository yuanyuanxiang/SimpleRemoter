#include "SessionMonitor.h"
#include <stdio.h>
#include <tlhelp32.h>
#include <userenv.h>

#pragma comment(lib, "userenv.lib")

// 动态数组初始容量
#define INITIAL_CAPACITY 4
#define Mprintf(format, ...) MyLog(__FILE__, __LINE__, format, __VA_ARGS__)

extern void MyLog(const char* file, int line, const char* format, ...);

// 前向声明
static DWORD WINAPI MonitorThreadProc(LPVOID param);
static void MonitorLoop(SessionMonitor* self);
static BOOL LaunchAgentInSession(SessionMonitor* self, DWORD sessionId);
static BOOL IsAgentRunningInSession(SessionMonitor* self, DWORD sessionId);
static void TerminateAllAgents(SessionMonitor* self);
static void CleanupDeadProcesses(SessionMonitor* self);

// 动态数组辅助函数
static void AgentArray_Init(AgentProcessArray* arr);
static void AgentArray_Free(AgentProcessArray* arr);
static BOOL AgentArray_Add(AgentProcessArray* arr, const AgentProcessInfo* info);
static void AgentArray_RemoveAt(AgentProcessArray* arr, size_t index);

// ============================================
// 动态数组实现
// ============================================

static void AgentArray_Init(AgentProcessArray* arr)
{
    arr->items = NULL;
    arr->count = 0;
    arr->capacity = 0;
}

static void AgentArray_Free(AgentProcessArray* arr)
{
    if (arr->items) {
        free(arr->items);
        arr->items = NULL;
    }
    arr->count = 0;
    arr->capacity = 0;
}

static BOOL AgentArray_Add(AgentProcessArray* arr, const AgentProcessInfo* info)
{
    size_t newCapacity;
    AgentProcessInfo* newItems;

    // 需要扩容
    if (arr->count >= arr->capacity) {
        newCapacity = arr->capacity == 0 ? INITIAL_CAPACITY : arr->capacity * 2;
        newItems = (AgentProcessInfo*)realloc(
                       arr->items, newCapacity * sizeof(AgentProcessInfo));
        if (!newItems) {
            return FALSE;
        }
        arr->items = newItems;
        arr->capacity = newCapacity;
    }

    arr->items[arr->count] = *info;
    arr->count++;
    return TRUE;
}

static void AgentArray_RemoveAt(AgentProcessArray* arr, size_t index)
{
    size_t i;

    if (index >= arr->count) {
        return;
    }

    // 将后面的元素前移
    for (i = index; i < arr->count - 1; i++) {
        arr->items[i] = arr->items[i + 1];
    }
    arr->count--;
}

// ============================================
// 公开接口实现
// ============================================

void SessionMonitor_Init(SessionMonitor* self)
{
    self->monitorThread = NULL;
    self->running = FALSE;
    InitializeCriticalSection(&self->csProcessList);
    AgentArray_Init(&self->agentProcesses);
}

void SessionMonitor_Cleanup(SessionMonitor* self)
{
    SessionMonitor_Stop(self);
    DeleteCriticalSection(&self->csProcessList);
    AgentArray_Free(&self->agentProcesses);
}

BOOL SessionMonitor_Start(SessionMonitor* self)
{
    if (self->running) {
        Mprintf("Monitor already running");
        return TRUE;
    }

    Mprintf("========================================");
    Mprintf("Starting session monitor...");

    self->running = TRUE;
    self->monitorThread = CreateThread(NULL, 0, MonitorThreadProc, self, 0, NULL);

    if (!self->monitorThread) {
        Mprintf("ERROR: Failed to create monitor thread");
        self->running = FALSE;
        return FALSE;
    }

    Mprintf("Session monitor thread created");
    return TRUE;
}

void SessionMonitor_Stop(SessionMonitor* self)
{
    if (!self->running) {
        return;
    }

    Mprintf("Stopping session monitor...");
    self->running = FALSE;

    if (self->monitorThread) {
        WaitForSingleObject(self->monitorThread, 10000);
        SAFE_CLOSE_HANDLE(self->monitorThread);
        self->monitorThread = NULL;
    }

    // 终止所有代理进程
    Mprintf("Terminating all agent processes...");
    TerminateAllAgents(self);

    Mprintf("Session monitor stopped");
    Mprintf("========================================");
}

// ============================================
// 内部函数实现
// ============================================

static DWORD WINAPI MonitorThreadProc(LPVOID param)
{
    SessionMonitor* monitor = (SessionMonitor*)param;
    MonitorLoop(monitor);
    return 0;
}

static void MonitorLoop(SessionMonitor* self)
{
    int loopCount = 0;
    PWTS_SESSION_INFO pSessionInfo = NULL;
    DWORD dwCount = 0;
    DWORD i;
    BOOL foundActiveSession;
    DWORD sessionId;
    char buf[256];
    int j;

    Mprintf("Monitor loop started");

    while (self->running) {
        loopCount++;

        // 清理已终止的进程
        CleanupDeadProcesses(self);

        // 枚举所有会话
        pSessionInfo = NULL;
        dwCount = 0;

        if (WTSEnumerateSessions(WTS_CURRENT_SERVER_HANDLE, 0, 1,
                                 &pSessionInfo, &dwCount)) {

            foundActiveSession = FALSE;

            for (i = 0; i < dwCount; i++) {
                if (pSessionInfo[i].State == WTSActive) {
                    sessionId = pSessionInfo[i].SessionId;
                    foundActiveSession = TRUE;

                    // 记录活动会话（每5次循环记录一次，避免日志过多）
                    if (loopCount % 5 == 1) {
                        sprintf(buf, "Active session found: ID=%d, Name=%s",
                                (int)sessionId,
                                pSessionInfo[i].pWinStationName);
                        Mprintf(buf);
                    }

                    // 检查代理是否在该会话中运行
                    if (!IsAgentRunningInSession(self, sessionId)) {
                        sprintf(buf, "Agent not running in session %d, launching...", (int)sessionId);
                        Mprintf(buf);

                        if (LaunchAgentInSession(self, sessionId)) {
                            Mprintf("Agent launched successfully");
                            // 给进程一些时间启动
                            Sleep(2000);
                        } else {
                            Mprintf("Failed to launch agent");
                        }
                    }

                    // 只处理第一个活动会话
                    break;
                }
            }

            if (!foundActiveSession && loopCount % 5 == 1) {
                Mprintf("No active sessions found");
            }

            WTSFreeMemory(pSessionInfo);
        } else {
            if (loopCount % 5 == 1) {
                Mprintf("WTSEnumerateSessions failed");
            }
        }

        // 每10秒检查一次
        for (j = 0; j < 100 && self->running; j++) {
            Sleep(100);
        }
    }

    Mprintf("Monitor loop exited");
}

static BOOL IsAgentRunningInSession(SessionMonitor* self, DWORD sessionId)
{
    char currentExeName[MAX_PATH];
    char* pFileName;
    DWORD currentPID;
    HANDLE hSnapshot;
    PROCESSENTRY32 pe32;
    BOOL found = FALSE;
    DWORD procSessionId;

    (void)self;  // 未使用

    // 获取当前进程的 exe 名称
    if (!GetModuleFileName(NULL, currentExeName, MAX_PATH)) {
        return FALSE;
    }

    // 获取文件名（不含路径）
    pFileName = strrchr(currentExeName, '\\');
    if (pFileName) {
        pFileName++;
    } else {
        pFileName = currentExeName;
    }

    // 获取当前服务进程的 PID
    currentPID = GetCurrentProcessId();

    // 创建进程快照
    hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        Mprintf("CreateToolhelp32Snapshot failed");
        return FALSE;
    }

    pe32.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(hSnapshot, &pe32)) {
        do {
            // 查找同名的 exe（ghost.exe）
            if (_stricmp(pe32.szExeFile, pFileName) == 0) {
                // 排除服务进程自己
                if (pe32.th32ProcessID == currentPID) {
                    continue;
                }

                // 获取进程的会话ID
                if (ProcessIdToSessionId(pe32.th32ProcessID, &procSessionId)) {
                    if (procSessionId == sessionId) {
                        // 找到了：同名 exe，不同 PID，在目标会话中
                        found = TRUE;
                        break;
                    }
                }
            }
        } while (Process32Next(hSnapshot, &pe32));
    }

    SAFE_CLOSE_HANDLE(hSnapshot);
    return found;
}

// 终止所有代理进程
static void TerminateAllAgents(SessionMonitor* self)
{
    char buf[256];
    size_t i;
    AgentProcessInfo* info;
    DWORD exitCode;

    EnterCriticalSection(&self->csProcessList);

    sprintf(buf, "Terminating %d agent process(es)", (int)self->agentProcesses.count);
    Mprintf(buf);

    for (i = 0; i < self->agentProcesses.count; i++) {
        info = &self->agentProcesses.items[i];

        sprintf(buf, "Terminating agent PID=%d (Session %d)",
                (int)info->processId, (int)info->sessionId);
        Mprintf(buf);

        // 检查进程是否还在运行
        if (GetExitCodeProcess(info->hProcess, &exitCode)) {
            if (exitCode == STILL_ACTIVE) {
                // 进程还在运行，终止
                if (!TerminateProcess(info->hProcess, 0)) {
                    sprintf(buf, "WARNING: Failed to terminate PID=%d, error=%d",
                            (int)info->processId, (int)GetLastError());
                    Mprintf(buf);
                } else {
                    Mprintf("Agent terminated successfully");
                    // 等待进程完全退出
                    WaitForSingleObject(info->hProcess, 5000);
                }
            } else {
                sprintf(buf, "Agent PID=%d already exited with code %d",
                        (int)info->processId, (int)exitCode);
                Mprintf(buf);
            }
        }

        SAFE_CLOSE_HANDLE(info->hProcess);
    }

    self->agentProcesses.count = 0;  // 清空数组

    LeaveCriticalSection(&self->csProcessList);
    Mprintf("All agents terminated");
}

// 清理已经终止的进程
static void CleanupDeadProcesses(SessionMonitor* self)
{
    size_t i;
    AgentProcessInfo* info;
    DWORD exitCode;
    char buf[256];

    EnterCriticalSection(&self->csProcessList);

    i = 0;
    while (i < self->agentProcesses.count) {
        info = &self->agentProcesses.items[i];

        if (GetExitCodeProcess(info->hProcess, &exitCode)) {
            if (exitCode != STILL_ACTIVE) {
                // 进程已退出
                sprintf(buf, "Agent PID=%d exited with code %d, cleaning up",
                        (int)info->processId, (int)exitCode);
                Mprintf(buf);

                SAFE_CLOSE_HANDLE(info->hProcess);
                AgentArray_RemoveAt(&self->agentProcesses, i);
                continue;  // 不增加 i，因为删除了元素
            }
        } else {
            // 无法获取退出代码，可能进程已不存在
            sprintf(buf, "Cannot query agent PID=%d, removing from list",
                    (int)info->processId);
            Mprintf(buf);

            SAFE_CLOSE_HANDLE(info->hProcess);
            AgentArray_RemoveAt(&self->agentProcesses, i);
            continue;
        }

        i++;
    }

    LeaveCriticalSection(&self->csProcessList);
}

static BOOL LaunchAgentInSession(SessionMonitor* self, DWORD sessionId)
{
    char buf[512];
    HANDLE hToken = NULL;
    HANDLE hDupToken = NULL;
    HANDLE hUserToken = NULL;
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    LPVOID lpEnvironment = NULL;
    char exePath[MAX_PATH];
    char cmdLine[MAX_PATH + 20];
    DWORD fileAttr;
    BOOL result;
    AgentProcessInfo info;
    DWORD err;

    memset(&si, 0, sizeof(si));
    memset(&pi, 0, sizeof(pi));

    sprintf(buf, "Attempting to launch agent in session %d", (int)sessionId);
    Mprintf(buf);

    si.cb = sizeof(STARTUPINFO);
    si.lpDesktop = (LPSTR)"winsta0\\default";  // 关键：指定桌面

    // 获取当前服务进程的 SYSTEM 令牌
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_DUPLICATE | TOKEN_QUERY, &hToken)) {
        sprintf(buf, "OpenProcessToken failed: %d", (int)GetLastError());
        Mprintf(buf);
        return FALSE;
    }

    // 复制为可用于创建进程的主令牌
    if (!DuplicateTokenEx(hToken, MAXIMUM_ALLOWED, NULL,
                          SecurityImpersonation, TokenPrimary, &hDupToken)) {
        sprintf(buf, "DuplicateTokenEx failed: %d", (int)GetLastError());
        Mprintf(buf);
        SAFE_CLOSE_HANDLE(hToken);
        return FALSE;
    }

    // 修改令牌的会话 ID 为目标用户会话
    if (!SetTokenInformation(hDupToken, TokenSessionId, &sessionId, sizeof(sessionId))) {
        sprintf(buf, "SetTokenInformation failed: %d", (int)GetLastError());
        Mprintf(buf);
        SAFE_CLOSE_HANDLE(hDupToken);
        SAFE_CLOSE_HANDLE(hToken);
        return FALSE;
    }

    Mprintf("Token duplicated");

    // 获取当前进程路径（启动自己）
    if (!GetModuleFileName(NULL, exePath, MAX_PATH)) {
        Mprintf("GetModuleFileName failed");
        SAFE_CLOSE_HANDLE(hDupToken);
        SAFE_CLOSE_HANDLE(hToken);
        return FALSE;
    }

    sprintf(buf, "Service path: %s", exePath);
    Mprintf(buf);

    // 检查文件是否存在
    fileAttr = GetFileAttributes(exePath);
    if (fileAttr == INVALID_FILE_ATTRIBUTES) {
        sprintf(buf, "ERROR: Executable not found at: %s", exePath);
        Mprintf(buf);
        SAFE_CLOSE_HANDLE(hDupToken);
        SAFE_CLOSE_HANDLE(hToken);
        return FALSE;
    }

    // 构建命令行：同一个 exe， 但带上 -agent 参数
    sprintf(cmdLine, "\"%s\" -agent", exePath);

    sprintf(buf, "Command line: %s", cmdLine);
    Mprintf(buf);

    // 获取用户令牌用于环境变量
    if (!WTSQueryUserToken(sessionId, &hUserToken)) {
        sprintf(buf, "WTSQueryUserToken failed: %d", (int)GetLastError());
        Mprintf(buf);
    }

    // 使用用户令牌创建环境块
    if (hUserToken) {
        if (!CreateEnvironmentBlock(&lpEnvironment, hUserToken, FALSE)) {
            Mprintf("CreateEnvironmentBlock failed");
        }
        SAFE_CLOSE_HANDLE(hUserToken);
    }

    // 在用户会话中创建进程
    result = CreateProcessAsUser(
                 hDupToken,
                 NULL,           // 应用程序名（在命令行中解析）
                 cmdLine,        // 命令行参数：ghost.exe -agent
                 NULL,           // 进程安全属性
                 NULL,           // 线程安全属性
                 FALSE,          // 不继承句柄
                 NORMAL_PRIORITY_CLASS | CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT,  // 创建标志
                 lpEnvironment,  // 环境变量
                 NULL,           // 当前目录
                 &si,
                 &pi
             );

    if (lpEnvironment) {
        DestroyEnvironmentBlock(lpEnvironment);
    }

    if (result) {
        sprintf(buf, "SUCCESS: Agent process created (PID=%d)", (int)pi.dwProcessId);
        Mprintf(buf);

        // 保存进程信息，以便停止时可以终止它
        EnterCriticalSection(&self->csProcessList);
        info.processId = pi.dwProcessId;
        info.sessionId = sessionId;
        info.hProcess = pi.hProcess;  // 不关闭句柄，保留用于后续终止
        AgentArray_Add(&self->agentProcesses, &info);
        LeaveCriticalSection(&self->csProcessList);

        SAFE_CLOSE_HANDLE(pi.hThread);  // 线程句柄可以关闭
    } else {
        err = GetLastError();
        sprintf(buf, "CreateProcessAsUser failed: %d", (int)err);
        Mprintf(buf);

        // 提供更详细的错误信息
        if (err == ERROR_FILE_NOT_FOUND) {
            Mprintf("ERROR: agent executable file not found");
        } else if (err == ERROR_ACCESS_DENIED) {
            Mprintf("ERROR: Access denied - service may not have sufficient privileges");
        } else if (err == 1314) {
            Mprintf("ERROR: Service does not have SE_INCREASE_QUOTA privilege");
        }
    }

    SAFE_CLOSE_HANDLE(hDupToken);
    SAFE_CLOSE_HANDLE(hToken);

    return result;
}
