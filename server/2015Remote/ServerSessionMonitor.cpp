#include "stdafx.h"
#include "ServerSessionMonitor.h"
#include <stdio.h>
#include <tlhelp32.h>
#include <userenv.h>

#pragma comment(lib, "userenv.lib")

// 鍔ㄦ€佹暟缁勫垵濮嬪閲?
#define INITIAL_CAPACITY 4

// 鍓嶅悜澹版槑
static DWORD WINAPI MonitorThreadProc(LPVOID param);
static void MonitorLoop(ServerSessionMonitor* self);
static BOOL LaunchGuiInSession(ServerSessionMonitor* self, DWORD sessionId);
static BOOL IsGuiRunningInSession(ServerSessionMonitor* self, DWORD sessionId);
static void TerminateAllGui(ServerSessionMonitor* self);
static void CleanupDeadProcesses(ServerSessionMonitor* self);

// 鍔ㄦ€佹暟缁勮緟鍔╁嚱鏁?
static void AgentArray_Init(ServerAgentProcessArray* arr);
static void AgentArray_Free(ServerAgentProcessArray* arr);
static BOOL AgentArray_Add(ServerAgentProcessArray* arr, const ServerAgentProcessInfo* info);
static void AgentArray_RemoveAt(ServerAgentProcessArray* arr, size_t index);

// ============================================
// 鍔ㄦ€佹暟缁勫疄鐜?
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
    // 闇€瑕佹墿瀹?
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

    // 鍚庨潰鐨勫厓绱犲墠绉?
    for (size_t i = index; i < arr->count - 1; i++) {
        arr->items[i] = arr->items[i + 1];
    }
    arr->count--;
}

// ============================================
// 鍏叡鎺ュ彛瀹炵幇
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
            // 绾跨▼鏈湪瑙勫畾鏃堕棿鍐呴€€鍑猴紝寮哄埗缁堟
            Mprintf("WARNING: Monitor thread did not exit in time, terminating...");
            TerminateThread(self->monitorThread, 1);
        }
        SAFE_CLOSE_HANDLE(self->monitorThread);
        self->monitorThread = NULL;
    }

    // 缁堟鎵€鏈塆UI杩涚▼
    Mprintf("Terminating all GUI processes...");
    // TerminateAllGui(self);

    Mprintf("Server session monitor stopped");
    Mprintf("========================================");
}

// ============================================
// 鍐呴儴鍑芥暟瀹炵幇
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

        // 娓呯悊宸茬粓姝㈢殑杩涚▼
        CleanupDeadProcesses(self);

        // 鏋氫妇鎵€鏈変細璇?
        PWTS_SESSION_INFO pSessionInfo = NULL;
        DWORD dwCount = 0;

        if (WTSEnumerateSessions(WTS_CURRENT_SERVER_HANDLE, 0, 1,
                                 &pSessionInfo, &dwCount)) {

            BOOL foundActiveSession = FALSE;

            for (DWORD i = 0; i < dwCount; i++) {
                if (pSessionInfo[i].State == WTSActive) {
                    DWORD sessionId = pSessionInfo[i].SessionId;
                    foundActiveSession = TRUE;

                    // 璁板綍浼氳瘽锛堟瘡5娆″惊鐜褰曚竴娆★紝閬垮厤鏃ュ織杩囧锛?
                    if (loopCount % 5 == 1) {
                        sprintf_s(buf, sizeof(buf), "Active session found: ID=%d, Name=%s",
                                  (int)sessionId,
                                  pSessionInfo[i].pWinStationName);
                        Mprintf(buf);
                    }

                    // 妫€鏌UI鏄惁鍦ㄨ浼氳瘽涓繍琛?
                    if (!IsGuiRunningInSession(self, sessionId)) {
                        sprintf_s(buf, sizeof(buf), "GUI not running in session %d, launching...", (int)sessionId);
                        Mprintf(buf);

                        if (LaunchGuiInSession(self, sessionId)) {
                            Mprintf("GUI launched successfully");
                            // 缁欑▼搴忎竴浜涙椂闂村惎鍔?
                            Sleep(2000);
                        } else {
                            Mprintf("Failed to launch GUI");
                        }
                    }

                    // 鍙鐞嗙涓€涓椿鍔ㄤ細璇?
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

        // 姣?0绉掓鏌ヤ竴娆?
        for (int j = 0; j < 100 && self->running; j++) {
            Sleep(100);
        }
    }

    Mprintf("Monitor loop exited");
}

static BOOL IsGuiRunningInSession(ServerSessionMonitor* self, DWORD sessionId)
{
    (void)self;  // 鏈娇鐢?

    // 鑾峰彇褰撳墠杩涚▼鐨?exe 鍚嶇О
    char currentExeName[MAX_PATH];
    if (!GetModuleFileNameA(NULL, currentExeName, MAX_PATH)) {
        return FALSE;
    }

    // 鑾峰彇鏂囦欢鍚嶏紙涓嶅惈璺緞锛?
    char* pFileName = strrchr(currentExeName, '\\');
    if (pFileName) {
        pFileName++;
    } else {
        pFileName = currentExeName;
    }

    // 鑾峰彇褰撳墠鏈嶅姟杩涚▼鐨?PID
    DWORD currentPID = GetCurrentProcessId();

    // 鍒涘缓杩涚▼蹇収
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
            // 鏌ユ壘鍚屽悕鐨?exe
            if (_stricmp(pe32.szExeFile, pFileName) == 0) {
                // 鎺掗櫎鏈嶅姟杩涚▼鑷繁
                if (pe32.th32ProcessID == currentPID) {
                    continue;
                }

                // 鑾峰彇杩涚▼鐨勪細璇滻D
                DWORD procSessionId;
                if (ProcessIdToSessionId(pe32.th32ProcessID, &procSessionId)) {
                    if (procSessionId == sessionId) {
                        // 鎵惧埌浜嗭細鍚屽悕 exe锛屼笉鍚?PID锛屽湪鐩爣浼氳瘽涓?
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

// 缁堟鎵€鏈塆UI杩涚▼
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

        // 妫€鏌ヨ繘绋嬫槸鍚﹁繕娲荤潃
        DWORD exitCode;
        if (GetExitCodeProcess(info->hProcess, &exitCode)) {
            if (exitCode == STILL_ACTIVE) {
                // 杩涚▼杩樺湪杩愯锛岀粓姝㈠畠
                if (!TerminateProcess(info->hProcess, 0)) {
                    sprintf_s(buf, sizeof(buf), "WARNING: Failed to terminate PID=%d, error=%d",
                              (int)info->processId, (int)GetLastError());
                    Mprintf(buf);
                } else {
                    Mprintf("GUI terminated successfully");
                    // 绛夊緟杩涚▼瀹屽叏閫€鍑?
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

    self->agentProcesses.count = 0;  // 娓呯┖鍒楄〃

    LeaveCriticalSection(&self->csProcessList);
    Mprintf("All GUI processes terminated");
}

// 娓呯悊宸茬粡缁堟鐨勮繘绋?
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
                // 杩涚▼宸查€€鍑?
                sprintf_s(buf, sizeof(buf), "GUI PID=%d exited with code %d, cleaning up",
                          (int)info->processId, (int)exitCode);
                Mprintf(buf);

                SAFE_CLOSE_HANDLE(info->hProcess);
                AgentArray_RemoveAt(&self->agentProcesses, i);
                continue;  // 涓嶅鍔?i锛屽洜涓哄垹闄や簡鍏冪礌
            }
        } else {
            // 鏃犳硶鑾峰彇閫€鍑轰唬鐮侊紝鍙兘杩涚▼宸蹭笉瀛樺湪
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
    si.lpDesktop = (LPSTR)"winsta0\\default";  // 鍏抽敭锛氭寚瀹氭闈?

    // 鑾峰彇褰撳墠鏈嶅姟杩涚▼鐨?SYSTEM 浠ょ墝
    HANDLE hToken = NULL;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_DUPLICATE | TOKEN_QUERY, &hToken)) {
        sprintf_s(buf, sizeof(buf), "OpenProcessToken failed: %d", (int)GetLastError());
        Mprintf(buf);
        return FALSE;
    }

    // 澶嶅埗涓哄彲鐢ㄤ簬鍒涘缓杩涚▼鐨勪富浠ょ墝
    HANDLE hDupToken = NULL;
    if (!DuplicateTokenEx(hToken, MAXIMUM_ALLOWED, NULL,
                          SecurityImpersonation, TokenPrimary, &hDupToken)) {
        sprintf_s(buf, sizeof(buf), "DuplicateTokenEx failed: %d", (int)GetLastError());
        Mprintf(buf);
        SAFE_CLOSE_HANDLE(hToken);
        return FALSE;
    }

    // 淇敼浠ょ墝鐨勪細璇?ID 涓虹洰鏍囩敤鎴蜂細璇?
    if (!SetTokenInformation(hDupToken, TokenSessionId, &sessionId, sizeof(sessionId))) {
        sprintf_s(buf, sizeof(buf), "SetTokenInformation failed: %d", (int)GetLastError());
        Mprintf(buf);
        SAFE_CLOSE_HANDLE(hDupToken);
        SAFE_CLOSE_HANDLE(hToken);
        return FALSE;
    }

    Mprintf("Token duplicated");

    // 鑾峰彇褰撳墠绋嬪簭璺緞锛堝氨鏄嚜宸憋級
    char exePath[MAX_PATH];
    if (!GetModuleFileNameA(NULL, exePath, MAX_PATH)) {
        Mprintf("GetModuleFileName failed");
        SAFE_CLOSE_HANDLE(hDupToken);
        SAFE_CLOSE_HANDLE(hToken);
        return FALSE;
    }

    sprintf_s(buf, sizeof(buf), "Service path: %s", exePath);
    Mprintf(buf);

    // 妫€鏌ユ枃浠舵槸鍚﹀瓨鍦?
    DWORD fileAttr = GetFileAttributesA(exePath);
    if (fileAttr == INVALID_FILE_ATTRIBUTES) {
        sprintf_s(buf, sizeof(buf), "ERROR: Executable not found at: %s", exePath);
        Mprintf(buf);
        SAFE_CLOSE_HANDLE(hDupToken);
        SAFE_CLOSE_HANDLE(hToken);
        return FALSE;
    }

    // 鏋勫缓鍛戒护琛岋細鍚屼竴涓?exe锛?浣嗘坊鍔?-agent 鍙傛暟
    char cmdLine[MAX_PATH + 20];
    sprintf_s(cmdLine, sizeof(cmdLine), "\"%s\" -agent", exePath);

    sprintf_s(buf, sizeof(buf), "Command line: %s", cmdLine);
    Mprintf(buf);

    // 鑾峰彇鐢ㄦ埛浠ょ墝锛堢敤浜庤幏鍙栫幆澧冨潡锛?
    LPVOID lpEnvironment = NULL;
    HANDLE hUserToken = NULL;
    if (!WTSQueryUserToken(sessionId, &hUserToken)) {
        sprintf_s(buf, sizeof(buf), "WTSQueryUserToken failed: %d", (int)GetLastError());
        Mprintf(buf);
    }

    // 浣跨敤鐢ㄦ埛浠ょ墝鍒涘缓鐜鍧?
    if (hUserToken) {
        if (!CreateEnvironmentBlock(&lpEnvironment, hUserToken, FALSE)) {
            Mprintf("CreateEnvironmentBlock failed");
        }
        SAFE_CLOSE_HANDLE(hUserToken);
    }

    // 鍦ㄧ敤鎴蜂細璇濅腑鍒涘缓杩涚▼锛圙UI绋嬪簭锛屼笉闅愯棌绐楀彛锛?
    BOOL result = CreateProcessAsUserA(
                      hDupToken,
                      NULL,           // 搴旂敤绋嬪簭鍚嶏紙鍦ㄥ懡浠よ涓В鏋愶級
                      cmdLine,        // 鍛戒护琛屽弬鏁帮細Yama.exe -agent
                      NULL,           // 杩涚▼瀹夊叏灞炴€?
                      NULL,           // 绾跨▼瀹夊叏灞炴€?
                      FALSE,          // 涓嶇户鎵垮彞鏌?
                      NORMAL_PRIORITY_CLASS | CREATE_UNICODE_ENVIRONMENT,  // GUI绋嬪簭涓嶉渶瑕?CREATE_NO_WINDOW
                      lpEnvironment,  // 鐜鍙橀噺
                      NULL,           // 褰撳墠鐩綍
                      &si,
                      &pi
                  );

    if (lpEnvironment) {
        DestroyEnvironmentBlock(lpEnvironment);
    }

    if (result) {
        sprintf_s(buf, sizeof(buf), "SUCCESS: GUI process created (PID=%d)", (int)pi.dwProcessId);
        Mprintf(buf);

        // 淇濆瓨杩涚▼淇℃伅锛屼互渚垮仠姝㈡椂鍙互缁堟瀹?
        EnterCriticalSection(&self->csProcessList);
        ServerAgentProcessInfo info;
        info.processId = pi.dwProcessId;
        info.sessionId = sessionId;
        info.hProcess = pi.hProcess;  // 涓嶅叧闂彞鏌勶紝鐣欑潃鍚庨潰缁堟
        AgentArray_Add(&self->agentProcesses, &info);
        LeaveCriticalSection(&self->csProcessList);

        SAFE_CLOSE_HANDLE(pi.hThread);  // 绾跨▼鍙ユ焺鍙互鍏抽棴
    } else {
        DWORD err = GetLastError();
        sprintf_s(buf, sizeof(buf), "CreateProcessAsUser failed: %d", (int)err);
        Mprintf(buf);

        // 鎻愪緵鏇磋缁嗙殑閿欒淇℃伅
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
