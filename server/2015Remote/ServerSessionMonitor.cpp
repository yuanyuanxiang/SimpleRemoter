#include "stdafx.h"
#include "ServerSessionMonitor.h"
#include <stdio.h>
#include <tlhelp32.h>
#include <userenv.h>

#pragma comment(lib, "userenv.lib")

// 动态数组初始容量
#define INITIAL_CAPACITY 4

// 前向声明
static DWORD WINAPI MonitorThreadProc(LPVOID param);
static void MonitorLoop(ServerSessionMonitor* self);
static BOOL LaunchGuiInSession(ServerSessionMonitor* self, DWORD sessionId);
static BOOL IsGuiRunningInSession(ServerSessionMonitor* self, DWORD sessionId);
static void TerminateAllGui(ServerSessionMonitor* self);
static void CleanupDeadProcesses(ServerSessionMonitor* self);

// 动态数组辅助函数
static void AgentArray_Init(ServerAgentProcessArray* arr);
static void AgentArray_Free(ServerAgentProcessArray* arr);
static BOOL AgentArray_Add(ServerAgentProcessArray* arr, const ServerAgentProcessInfo* info);
static void AgentArray_RemoveAt(ServerAgentProcessArray* arr, size_t index);

// ============================================
// 动态数组实现
// ============================================

static void AgentArray_Init(ServerAgentProcessArray* arr)
{
    arr->items = NULL;
    arr->count = 0;
    arr->capacity = 0;
}

static void AgentArray_Free(ServerAgentProcessArray* arr)
{
    if (arr->items) {
        free(arr->items);
        arr->items = NULL;
    }
    arr->count = 0;
    arr->capacity = 0;
}

static BOOL AgentArray_Add(ServerAgentProcessArray* arr, const ServerAgentProcessInfo* info)
{
    // 需要扩容
    if (arr->count >= arr->capacity) {
        size_t newCapacity = arr->capacity == 0 ? INITIAL_CAPACITY : arr->capacity * 2;
        ServerAgentProcessInfo* newItems = (ServerAgentProcessInfo*)realloc(
                                               arr->items, newCapacity * sizeof(ServerAgentProcessInfo));
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

static void AgentArray_RemoveAt(ServerAgentProcessArray* arr, size_t index)
{
    if (index >= arr->count) {
        return;
    }

    // 后面的元素前移
    for (size_t i = index; i < arr->count - 1; i++) {
        arr->items[i] = arr->items[i + 1];
    }
    arr->count--;
}

// ============================================
// 公共接口实现
// ============================================

void ServerSessionMonitor_Init(ServerSessionMonitor* self)
{
    self->monitorThread = NULL;
    self->running = FALSE;
    InitializeCriticalSection(&self->csProcessList);
    AgentArray_Init(&self->agentProcesses);
}

void ServerSessionMonitor_Cleanup(ServerSessionMonitor* self)
{
    ServerSessionMonitor_Stop(self);
    DeleteCriticalSection(&self->csProcessList);
    AgentArray_Free(&self->agentProcesses);
}

BOOL ServerSessionMonitor_Start(ServerSessionMonitor* self)
{
    if (self->running) {
        Mprintf("Monitor already running");
        return TRUE;
    }

    Mprintf("========================================");
    Mprintf("Starting server session monitor...");

    self->running = TRUE;
    self->monitorThread = CreateThread(NULL, 0, MonitorThreadProc, self, 0, NULL);

    if (!self->monitorThread) {
        Mprintf("ERROR: Failed to create monitor thread");
        self->running = FALSE;
        return FALSE;
    }

    Mprintf("Server session monitor thread created");
    return TRUE;
}

void ServerSessionMonitor_Stop(ServerSessionMonitor* self)
{
    if (!self->running) {
        return;
    }

    Mprintf("Stopping server session monitor...");
    self->running = FALSE;

    if (self->monitorThread) {
        DWORD waitResult = WaitForSingleObject(self->monitorThread, 10000);
        if (waitResult == WAIT_TIMEOUT) {
            // 线程未在规定时间内退出，强制终止
            Mprintf("WARNING: Monitor thread did not exit in time, terminating...");
            TerminateThread(self->monitorThread, 1);
        }
        SAFE_CLOSE_HANDLE(self->monitorThread);
        self->monitorThread = NULL;
    }

    // 终止所有GUI进程
    Mprintf("Terminating all GUI processes...");
    // TerminateAllGui(self);

    Mprintf("Server session monitor stopped");
    Mprintf("========================================");
}

// ============================================
// 内部函数实现
// ============================================

static DWORD WINAPI MonitorThreadProc(LPVOID param)
{
    ServerSessionMonitor* monitor = (ServerSessionMonitor*)param;
    MonitorLoop(monitor);
    return 0;
}

static void MonitorLoop(ServerSessionMonitor* self)
{
    int loopCount = 0;
    char buf[256];

    Mprintf("Monitor loop started");

    while (self->running) {
        loopCount++;

        // 清理已终止的进程
        CleanupDeadProcesses(self);

        // 枚举所有会话
        PWTS_SESSION_INFO pSessionInfo = NULL;
        DWORD dwCount = 0;

        if (WTSEnumerateSessions(WTS_CURRENT_SERVER_HANDLE, 0, 1,
                                 &pSessionInfo, &dwCount)) {

            BOOL foundActiveSession = FALSE;

            for (DWORD i = 0; i < dwCount; i++) {
                if (pSessionInfo[i].State == WTSActive) {
                    DWORD sessionId = pSessionInfo[i].SessionId;
                    foundActiveSession = TRUE;

                    // 记录会话（每5次循环记录一次，避免日志过多）
                    if (loopCount % 5 == 1) {
                        sprintf_s(buf, sizeof(buf), "Active session found: ID=%d, Name=%s",
                                  (int)sessionId,
                                  pSessionInfo[i].pWinStationName);
                        Mprintf(buf);
                    }

                    // 检查GUI是否在该会话中运行
                    if (!IsGuiRunningInSession(self, sessionId)) {
                        sprintf_s(buf, sizeof(buf), "GUI not running in session %d, launching...", (int)sessionId);
                        Mprintf(buf);

                        if (LaunchGuiInSession(self, sessionId)) {
                            Mprintf("GUI launched successfully");
                            // 给程序一些时间启动
                            Sleep(2000);
                        } else {
                            Mprintf("Failed to launch GUI");
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
        for (int j = 0; j < 100 && self->running; j++) {
            Sleep(100);
        }
    }

    Mprintf("Monitor loop exited");
}

static BOOL IsGuiRunningInSession(ServerSessionMonitor* self, DWORD sessionId)
{
    (void)self;  // 未使用

    // 获取当前进程的 exe 名称
    char currentExeName[MAX_PATH];
    if (!GetModuleFileNameA(NULL, currentExeName, MAX_PATH)) {
        return FALSE;
    }

    // 获取文件名（不含路径）
    char* pFileName = strrchr(currentExeName, '\\');
    if (pFileName) {
        pFileName++;
    } else {
        pFileName = currentExeName;
    }

    // 获取当前服务进程的 PID
    DWORD currentPID = GetCurrentProcessId();

    // 创建进程快照
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        Mprintf("CreateToolhelp32Snapshot failed");
        return FALSE;
    }

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);
    BOOL found = FALSE;

    if (Process32First(hSnapshot, &pe32)) {
        do {
            // 查找同名的 exe
            if (_stricmp(pe32.szExeFile, pFileName) == 0) {
                // 排除服务进程自己
                if (pe32.th32ProcessID == currentPID) {
                    continue;
                }

                // 获取进程的会话ID
                DWORD procSessionId;
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

// 终止所有GUI进程
static void TerminateAllGui(ServerSessionMonitor* self)
{
    char buf[256];

    EnterCriticalSection(&self->csProcessList);

    sprintf_s(buf, sizeof(buf), "Terminating %d GUI process(es)", (int)self->agentProcesses.count);
    Mprintf(buf);

    for (size_t i = 0; i < self->agentProcesses.count; i++) {
        ServerAgentProcessInfo* info = &self->agentProcesses.items[i];

        sprintf_s(buf, sizeof(buf), "Terminating GUI PID=%d (Session %d)",
                  (int)info->processId, (int)info->sessionId);
        Mprintf(buf);

        // 检查进程是否还活着
        DWORD exitCode;
        if (GetExitCodeProcess(info->hProcess, &exitCode)) {
            if (exitCode == STILL_ACTIVE) {
                // 进程还在运行，终止它
                if (!TerminateProcess(info->hProcess, 0)) {
                    sprintf_s(buf, sizeof(buf), "WARNING: Failed to terminate PID=%d, error=%d",
                              (int)info->processId, (int)GetLastError());
                    Mprintf(buf);
                } else {
                    Mprintf("GUI terminated successfully");
                    // 等待进程完全退出
                    WaitForSingleObject(info->hProcess, 5000);
                }
            } else {
                sprintf_s(buf, sizeof(buf), "GUI PID=%d already exited with code %d",
                          (int)info->processId, (int)exitCode);
                Mprintf(buf);
            }
        }

        SAFE_CLOSE_HANDLE(info->hProcess);
    }

    self->agentProcesses.count = 0;  // 清空列表

    LeaveCriticalSection(&self->csProcessList);
    Mprintf("All GUI processes terminated");
}

// 清理已经终止的进程
static void CleanupDeadProcesses(ServerSessionMonitor* self)
{
    char buf[256];

    EnterCriticalSection(&self->csProcessList);

    size_t i = 0;
    while (i < self->agentProcesses.count) {
        ServerAgentProcessInfo* info = &self->agentProcesses.items[i];

        DWORD exitCode;
        if (GetExitCodeProcess(info->hProcess, &exitCode)) {
            if (exitCode != STILL_ACTIVE) {
                // 进程已退出
                sprintf_s(buf, sizeof(buf), "GUI PID=%d exited with code %d, cleaning up",
                          (int)info->processId, (int)exitCode);
                Mprintf(buf);

                SAFE_CLOSE_HANDLE(info->hProcess);
                AgentArray_RemoveAt(&self->agentProcesses, i);
                continue;  // 不增加 i，因为删除了元素
            }
        } else {
            // 无法获取退出代码，可能进程已不存在
            sprintf_s(buf, sizeof(buf), "Cannot query GUI PID=%d, removing from list",
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

static BOOL LaunchGuiInSession(ServerSessionMonitor* self, DWORD sessionId)
{
    char buf[512];

    sprintf_s(buf, sizeof(buf), "Attempting to launch GUI in session %d", (int)sessionId);
    Mprintf(buf);

    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof(si));
    memset(&pi, 0, sizeof(pi));

    si.cb = sizeof(STARTUPINFO);
    si.lpDesktop = (LPSTR)"winsta0\\default";  // 关键：指定桌面

    // 获取当前服务进程的 SYSTEM 令牌
    HANDLE hToken = NULL;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_DUPLICATE | TOKEN_QUERY, &hToken)) {
        sprintf_s(buf, sizeof(buf), "OpenProcessToken failed: %d", (int)GetLastError());
        Mprintf(buf);
        return FALSE;
    }

    // 复制为可用于创建进程的主令牌
    HANDLE hDupToken = NULL;
    if (!DuplicateTokenEx(hToken, MAXIMUM_ALLOWED, NULL,
                          SecurityImpersonation, TokenPrimary, &hDupToken)) {
        sprintf_s(buf, sizeof(buf), "DuplicateTokenEx failed: %d", (int)GetLastError());
        Mprintf(buf);
        SAFE_CLOSE_HANDLE(hToken);
        return FALSE;
    }

    // 修改令牌的会话 ID 为目标用户会话
    if (!SetTokenInformation(hDupToken, TokenSessionId, &sessionId, sizeof(sessionId))) {
        sprintf_s(buf, sizeof(buf), "SetTokenInformation failed: %d", (int)GetLastError());
        Mprintf(buf);
        SAFE_CLOSE_HANDLE(hDupToken);
        SAFE_CLOSE_HANDLE(hToken);
        return FALSE;
    }

    Mprintf("Token duplicated");

    // 获取当前程序路径（就是自己）
    char exePath[MAX_PATH];
    if (!GetModuleFileNameA(NULL, exePath, MAX_PATH)) {
        Mprintf("GetModuleFileName failed");
        SAFE_CLOSE_HANDLE(hDupToken);
        SAFE_CLOSE_HANDLE(hToken);
        return FALSE;
    }

    sprintf_s(buf, sizeof(buf), "Service path: %s", exePath);
    Mprintf(buf);

    // 检查文件是否存在
    DWORD fileAttr = GetFileAttributesA(exePath);
    if (fileAttr == INVALID_FILE_ATTRIBUTES) {
        sprintf_s(buf, sizeof(buf), "ERROR: Executable not found at: %s", exePath);
        Mprintf(buf);
        SAFE_CLOSE_HANDLE(hDupToken);
        SAFE_CLOSE_HANDLE(hToken);
        return FALSE;
    }

    // 构建命令行：同一个 exe， 但添加 -agent 参数
    char cmdLine[MAX_PATH + 20];
    sprintf_s(cmdLine, sizeof(cmdLine), "\"%s\" -agent", exePath);

    sprintf_s(buf, sizeof(buf), "Command line: %s", cmdLine);
    Mprintf(buf);

    // 获取用户令牌（用于获取环境块）
    LPVOID lpEnvironment = NULL;
    HANDLE hUserToken = NULL;
    if (!WTSQueryUserToken(sessionId, &hUserToken)) {
        sprintf_s(buf, sizeof(buf), "WTSQueryUserToken failed: %d", (int)GetLastError());
        Mprintf(buf);
    }

    // 使用用户令牌创建环境块
    if (hUserToken) {
        if (!CreateEnvironmentBlock(&lpEnvironment, hUserToken, FALSE)) {
            Mprintf("CreateEnvironmentBlock failed");
        }
        SAFE_CLOSE_HANDLE(hUserToken);
    }

    // 在用户会话中创建进程（GUI程序，不隐藏窗口）
    BOOL result = CreateProcessAsUserA(
                      hDupToken,
                      NULL,           // 应用程序名（在命令行中解析）
                      cmdLine,        // 命令行参数：Yama.exe -agent
                      NULL,           // 进程安全属性
                      NULL,           // 线程安全属性
                      FALSE,          // 不继承句柄
                      NORMAL_PRIORITY_CLASS | CREATE_UNICODE_ENVIRONMENT,  // GUI程序不需要 CREATE_NO_WINDOW
                      lpEnvironment,  // 环境变量
                      NULL,           // 当前目录
                      &si,
                      &pi
                  );

    if (lpEnvironment) {
        DestroyEnvironmentBlock(lpEnvironment);
    }

    if (result) {
        sprintf_s(buf, sizeof(buf), "SUCCESS: GUI process created (PID=%d)", (int)pi.dwProcessId);
        Mprintf(buf);

        // 保存进程信息，以便停止时可以终止它
        EnterCriticalSection(&self->csProcessList);
        ServerAgentProcessInfo info;
        info.processId = pi.dwProcessId;
        info.sessionId = sessionId;
        info.hProcess = pi.hProcess;  // 不关闭句柄，留着后面终止
        AgentArray_Add(&self->agentProcesses, &info);
        LeaveCriticalSection(&self->csProcessList);

        SAFE_CLOSE_HANDLE(pi.hThread);  // 线程句柄可以关闭
    } else {
        DWORD err = GetLastError();
        sprintf_s(buf, sizeof(buf), "CreateProcessAsUser failed: %d", (int)err);
        Mprintf(buf);

        // 提供更详细的错误信息
        if (err == ERROR_FILE_NOT_FOUND) {
            Mprintf("ERROR: Executable not found");
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
