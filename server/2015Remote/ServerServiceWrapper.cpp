#include "stdafx.h"
#include "ServerServiceWrapper.h"
#include "ServerSessionMonitor.h"
#include "CrashReport.h"
#include <stdio.h>
#include <winsvc.h>
#include "2015Remote.h"

// 静态变量
static SERVICE_STATUS g_ServiceStatus;
static SERVICE_STATUS_HANDLE g_StatusHandle = NULL;
static HANDLE g_StopEvent = INVALID_HANDLE_VALUE;

// 前向声明
static void WINAPI ServiceMain(DWORD argc, LPTSTR* argv);
static void WINAPI ServiceCtrlHandler(DWORD ctrlCode);

// 代理启动回调：记录启动次数
static void OnAgentStart(DWORD processId, DWORD sessionId)
{
    // 递增启动次数
    int startCount = THIS_CFG.GetInt(CFG_CRASH_SECTION, CFG_CRASH_STARTS, 0) + 1;
    THIS_CFG.SetInt(CFG_CRASH_SECTION, CFG_CRASH_STARTS, startCount);

    char buf[128];
    sprintf_s(buf, sizeof(buf), "Agent started: PID=%d, Session=%d, totalStarts=%d",
              (int)processId, (int)sessionId, startCount);
    Mprintf(buf);
}

// 代理退出回调：累计运行时间（用于 MTBF 计算）
static void OnAgentExit(DWORD exitCode, ULONGLONG runtimeMs)
{
    // 累加总运行时间
    // 注意：使用字符串存储 64 位整数，避免 GetInt/SetInt 的 32 位限制
    char totalStr[32];
    ULONGLONG totalRuntime = 0;
    std::string storedTotal = THIS_CFG.GetStr(CFG_CRASH_SECTION, CFG_CRASH_TOTAL_RUN_MS, "0");
    totalRuntime = _strtoui64(storedTotal.c_str(), NULL, 10);
    totalRuntime += runtimeMs;
    sprintf_s(totalStr, sizeof(totalStr), "%llu", totalRuntime);
    THIS_CFG.SetStr(CFG_CRASH_SECTION, CFG_CRASH_TOTAL_RUN_MS, totalStr);

    // 格式化运行时间
    char runtimeStr[64];
    FormatRuntime(totalRuntime, runtimeStr, sizeof(runtimeStr));

    // 计算 MTBF（如果有崩溃记录）
    int crashCount = THIS_CFG.GetInt(CFG_CRASH_SECTION, CFG_CRASH_COUNT, 0);
    int startCount = THIS_CFG.GetInt(CFG_CRASH_SECTION, CFG_CRASH_STARTS, 0);

    char buf[256];
    if (crashCount > 0) {
        ULONGLONG mtbf = CalculateMTBF(totalRuntime, crashCount);
        char mtbfStr[64];
        FormatRuntime(mtbf, mtbfStr, sizeof(mtbfStr));
        double failureRate = CalculateFailureRate(crashCount, startCount);
        sprintf_s(buf, sizeof(buf),
            "Agent exited: code=0x%08X, runtime=%llums, totalRuntime=%s, MTBF=%s, failureRate=%.2f%%",
            exitCode, runtimeMs, runtimeStr, mtbfStr, failureRate * 100);
    } else {
        sprintf_s(buf, sizeof(buf),
            "Agent exited: code=0x%08X, runtime=%llums, totalRuntime=%s (no crashes yet)",
            exitCode, runtimeMs, runtimeStr);
    }
    Mprintf(buf);
}

// 崩溃统计回调：记录崩溃次数、时间和退出代码
static void OnAgentCrash(DWORD exitCode, ULONGLONG runtimeMs)
{
    // 递增总崩溃次数
    int totalCrashes = THIS_CFG.GetInt(CFG_CRASH_SECTION, CFG_CRASH_COUNT, 0) + 1;
    THIS_CFG.SetInt(CFG_CRASH_SECTION, CFG_CRASH_COUNT, totalCrashes);

    // 记录最后崩溃时间
    char timeStr[32];
    SYSTEMTIME st;
    GetLocalTime(&st);
    sprintf_s(timeStr, sizeof(timeStr), "%04d-%02d-%02d %02d:%02d:%02d",
              st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    THIS_CFG.SetStr(CFG_CRASH_SECTION, CFG_CRASH_LAST_TIME, timeStr);

    // 记录退出代码
    char exitCodeStr[64];
    const char* desc = GetExitCodeDescription(exitCode);
    if (desc) {
        sprintf_s(exitCodeStr, sizeof(exitCodeStr), "0x%08X (%s)", exitCode, desc);
    } else {
        sprintf_s(exitCodeStr, sizeof(exitCodeStr), "0x%08X", exitCode);
    }
    THIS_CFG.SetStr(CFG_CRASH_SECTION, CFG_CRASH_LAST_CODE, exitCodeStr);

    // 记录运行时间（毫秒）- 使用字符串存储 64 位值
    char runtimeStr[32];
    sprintf_s(runtimeStr, sizeof(runtimeStr), "%llu", runtimeMs);
    THIS_CFG.SetStr(CFG_CRASH_SECTION, CFG_CRASH_LAST_RUN_MS, runtimeStr);

    char buf[256];
    sprintf_s(buf, sizeof(buf), "Agent crash recorded: total=%d, time=%s, exitCode=%s, runtime=%llums",
              totalCrashes, timeStr, exitCodeStr, runtimeMs);
    Mprintf(buf);
}

// 崩溃窗口状态变化回调：持久化崩溃窗口状态
static void OnCrashWindowChange(int crashCount, ULONGLONG firstCrashTime)
{
    THIS_CFG.SetInt(CFG_CRASH_SECTION, CFG_CRASH_WIN_COUNT, crashCount);

    // 使用字符串存储 64 位时间戳
    char timeStr[32];
    sprintf_s(timeStr, sizeof(timeStr), "%llu", firstCrashTime);
    THIS_CFG.SetStr(CFG_CRASH_SECTION, CFG_CRASH_WIN_START, timeStr);

    char buf[128];
    sprintf_s(buf, sizeof(buf), "Crash window state saved: count=%d, startTime=%llu", crashCount, firstCrashTime);
    Mprintf(buf);
}

// 崩溃保护回调：写入保护标志
static void OnAgentCrashProtection(void)
{
    THIS_CFG.SetInt(CFG_CRASH_SECTION, CFG_CRASH_PROTECTED, 1);
    // 清除崩溃窗口状态（保护已触发，不需要再保持窗口状态）
    THIS_CFG.SetInt(CFG_CRASH_SECTION, CFG_CRASH_WIN_COUNT, 0);
    THIS_CFG.SetStr(CFG_CRASH_SECTION, CFG_CRASH_WIN_START, "0");
    Mprintf("Crash protection flag written to config");
}

BOOL ServerService_CheckStatus(BOOL* registered, BOOL* running,
                               char* exePath, size_t exePathSize)
{
    *registered = FALSE;
    *running = FALSE;
    if (exePath && exePathSize > 0) {
        exePath[0] = '\0';
    }

    // 打开 SCM
    SC_HANDLE hSCM = OpenSCManagerA(NULL, NULL, SC_MANAGER_CONNECT);
    if (!hSCM) {
        return FALSE;
    }

    // 打开服务
    SC_HANDLE hService = OpenServiceA(
                             hSCM,
                             SERVER_SERVICE_NAME,
                             SERVICE_QUERY_STATUS | SERVICE_QUERY_CONFIG);
    if (!hService) {
        CloseServiceHandle(hSCM);
        return FALSE;  // 未注册
    }

    *registered = TRUE;

    // 获取服务状态
    SERVICE_STATUS_PROCESS ssp;
    DWORD bytesNeeded = 0;
    memset(&ssp, 0, sizeof(ssp));
    if (QueryServiceStatusEx(
            hService,
            SC_STATUS_PROCESS_INFO,
            (LPBYTE)&ssp,
            sizeof(SERVICE_STATUS_PROCESS),
            &bytesNeeded)) {
        *running = (ssp.dwCurrentState == SERVICE_RUNNING);
    }

    // 获取 EXE 路径
    if (exePath && exePathSize > 0) {
        DWORD bufSize = 0;
        QueryServiceConfigA(hService, NULL, 0, &bufSize);

        if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
            LPQUERY_SERVICE_CONFIGA pConfig = (LPQUERY_SERVICE_CONFIGA)malloc(bufSize);
            if (pConfig) {
                if (QueryServiceConfigA(hService, pConfig, bufSize, &bufSize)) {
                    strncpy_s(exePath, exePathSize, pConfig->lpBinaryPathName, _TRUNCATE);
                }
                free(pConfig);
            }
        }
    }

    CloseServiceHandle(hService);
    CloseServiceHandle(hSCM);
    return TRUE;
}

int ServerService_StartSimple(void)
{
    // 打开SCM
    SC_HANDLE hSCM = OpenSCManagerA(NULL, NULL, SC_MANAGER_CONNECT);
    if (!hSCM) {
        return (int)GetLastError();
    }

    // 打开服务并启动
    SC_HANDLE hService = OpenServiceA(hSCM, SERVER_SERVICE_NAME, SERVICE_START);
    if (!hService) {
        int err = (int)GetLastError();
        CloseServiceHandle(hSCM);
        return err;
    }

    // 启动服务
    BOOL ok = StartServiceA(hService, 0, NULL);
    int err = ok ? ERROR_SUCCESS : (int)GetLastError();

    CloseServiceHandle(hService);
    CloseServiceHandle(hSCM);

    return err;
}

int ServerService_Run(void)
{
    SERVICE_TABLE_ENTRY ServiceTable[2];
    ServiceTable[0].lpServiceName = (LPSTR)SERVER_SERVICE_NAME;
    ServiceTable[0].lpServiceProc = (LPSERVICE_MAIN_FUNCTION)ServiceMain;
    ServiceTable[1].lpServiceName = NULL;
    ServiceTable[1].lpServiceProc = NULL;

    Mprintf("========================================");
    Mprintf("ServerService_Run() called");

    if (StartServiceCtrlDispatcher(ServiceTable) == FALSE) {
        DWORD err = GetLastError();
        char buffer[256];
        sprintf_s(buffer, sizeof(buffer), "StartServiceCtrlDispatcher failed: %d", (int)err);
        Mprintf(buffer);
        return (int)err;
    }
    return ERROR_SUCCESS;
}

int ServerService_Stop(void)
{
    // 打开SCM
    SC_HANDLE hSCM = OpenSCManagerA(NULL, NULL, SC_MANAGER_CONNECT);
    if (!hSCM) {
        return (int)GetLastError();
    }

    // 打开服务
    SC_HANDLE hService = OpenServiceA(hSCM, SERVER_SERVICE_NAME, SERVICE_STOP | SERVICE_QUERY_STATUS);
    if (!hService) {
        int err = (int)GetLastError();
        CloseServiceHandle(hSCM);
        return err;
    }

    // 查询当前状态
    SERVICE_STATUS status;
    if (!QueryServiceStatus(hService, &status)) {
        int err = (int)GetLastError();
        CloseServiceHandle(hService);
        CloseServiceHandle(hSCM);
        return err;
    }

    // 如果服务未运行，直接返回成功
    if (status.dwCurrentState == SERVICE_STOPPED) {
        CloseServiceHandle(hService);
        CloseServiceHandle(hSCM);
        return ERROR_SUCCESS;
    }

    // 发送停止控制命令
    if (!ControlService(hService, SERVICE_CONTROL_STOP, &status)) {
        DWORD err = GetLastError();
        if (err != ERROR_SERVICE_NOT_ACTIVE) {
            CloseServiceHandle(hService);
            CloseServiceHandle(hSCM);
            return (int)err;
        }
    }

    // 等待服务停止（最多30秒）
    int waitCount = 0;
    while (status.dwCurrentState != SERVICE_STOPPED && waitCount < 30) {
        Sleep(1000);
        waitCount++;
        if (!QueryServiceStatus(hService, &status)) {
            break;
        }
    }

    int result = (status.dwCurrentState == SERVICE_STOPPED) ? ERROR_SUCCESS : ERROR_TIMEOUT;

    CloseServiceHandle(hService);
    CloseServiceHandle(hSCM);

    return result;
}

static void WINAPI ServiceMain(DWORD argc, LPTSTR* argv)
{
    (void)argc;
    (void)argv;

    Mprintf("ServiceMain() called");

    g_StatusHandle = RegisterServiceCtrlHandler(
                         SERVER_SERVICE_NAME,
                         ServiceCtrlHandler
                     );

    if (g_StatusHandle == NULL) {
        Mprintf("RegisterServiceCtrlHandler failed");
        return;
    }

    ZeroMemory(&g_ServiceStatus, sizeof(g_ServiceStatus));
    g_ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
    g_ServiceStatus.dwControlsAccepted = 0;
    g_ServiceStatus.dwWin32ExitCode = 0;
    g_ServiceStatus.dwServiceSpecificExitCode = 0;
    g_ServiceStatus.dwCheckPoint = 0;
    g_ServiceStatus.dwWaitHint = 0;

    SetServiceStatus(g_StatusHandle, &g_ServiceStatus);

    g_StopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (g_StopEvent == NULL) {
        Mprintf("CreateEvent failed");
        g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
        g_ServiceStatus.dwWin32ExitCode = GetLastError();
        SetServiceStatus(g_StatusHandle, &g_ServiceStatus);
        return;
    }

    g_ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
    g_ServiceStatus.dwCurrentState = SERVICE_RUNNING;
    g_ServiceStatus.dwWin32ExitCode = 0;
    g_ServiceStatus.dwCheckPoint = 0;

    SetServiceStatus(g_StatusHandle, &g_ServiceStatus);
    Mprintf("Service is now running");

    HANDLE hThread = CreateThread(NULL, 0, ServerService_WorkerThread, NULL, 0, NULL);
    if (hThread) {
        WaitForSingleObject(hThread, INFINITE);
        SAFE_CLOSE_HANDLE(hThread);
    }

    SAFE_CLOSE_HANDLE(g_StopEvent);

    g_ServiceStatus.dwControlsAccepted = 0;
    g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
    g_ServiceStatus.dwWin32ExitCode = 0;
    g_ServiceStatus.dwCheckPoint = 3;

    SetServiceStatus(g_StatusHandle, &g_ServiceStatus);
    Mprintf("Service stopped");
}

static void WINAPI ServiceCtrlHandler(DWORD ctrlCode)
{
    switch (ctrlCode) {
    case SERVICE_CONTROL_STOP:
        Mprintf("SERVICE_CONTROL_STOP received");

        if (g_ServiceStatus.dwCurrentState != SERVICE_RUNNING)
            break;

        g_ServiceStatus.dwControlsAccepted = 0;
        g_ServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
        g_ServiceStatus.dwWin32ExitCode = 0;
        g_ServiceStatus.dwCheckPoint = 4;
        g_ServiceStatus.dwWaitHint = 0;

        SetServiceStatus(g_StatusHandle, &g_ServiceStatus);
        SetEvent(g_StopEvent);
        break;

    case SERVICE_CONTROL_INTERROGATE:
        SetServiceStatus(g_StatusHandle, &g_ServiceStatus);
        break;

    default:
        break;
    }
}

// 服务工作线程
DWORD WINAPI ServerService_WorkerThread(LPVOID lpParam)
{
    (void)lpParam;
    int heartbeatCount = 0;
    char buf[128];

    Mprintf("========================================");
    Mprintf("Worker thread started");
    Mprintf("Service will launch Yama GUI in user sessions");

    // 读取配置：运行模式 (RunNormal: 0=服务+SYSTEM, 1=普通模式, 2=服务+User)
	int runNormal = THIS_CFG.GetInt("settings", "RunNormal", 0);

    // 初始化会话监控器
    ServerSessionMonitor monitor;
    ServerSessionMonitor_Init(&monitor);
    monitor.runAsUser = (runNormal == 2);

    // 从配置恢复崩溃窗口状态
    int savedCrashCount = THIS_CFG.GetInt(CFG_CRASH_SECTION, CFG_CRASH_WIN_COUNT, 0);
    std::string savedStartStr = THIS_CFG.GetStr(CFG_CRASH_SECTION, CFG_CRASH_WIN_START, "0");
    ULONGLONG savedFirstCrashTime = _strtoui64(savedStartStr.c_str(), NULL, 10);
    ULONGLONG now = GetTickCount64();

    if (savedCrashCount > 0 && savedFirstCrashTime > 0) {
        // 检查是否仍在窗口期内
        // 注意：GetTickCount64() 在系统重启后会重置，所以如果 savedFirstCrashTime > now，说明系统重启过
        if (savedFirstCrashTime <= now && (now - savedFirstCrashTime) <= CRASH_WINDOW_MS) {
            // 仍在窗口期内，恢复状态
            monitor.crashCount = savedCrashCount;
            monitor.firstCrashTime = savedFirstCrashTime;
            sprintf_s(buf, sizeof(buf), "Crash window state restored: count=%d, elapsed=%llums",
                      savedCrashCount, now - savedFirstCrashTime);
            Mprintf(buf);
        } else {
            // 窗口期已过或系统重启，清除状态
            THIS_CFG.SetInt(CFG_CRASH_SECTION, CFG_CRASH_WIN_COUNT, 0);
            THIS_CFG.SetStr(CFG_CRASH_SECTION, CFG_CRASH_WIN_START, "0");
            Mprintf("Crash window expired or system rebooted, state cleared");
        }
    }

    // 设置回调函数
    monitor.onAgentStart = OnAgentStart;
    monitor.onAgentExit = OnAgentExit;
    monitor.onCrash = OnAgentCrash;
    monitor.onCrashWindowChange = OnCrashWindowChange;
    monitor.onCrashProtection = OnAgentCrashProtection;

    if (!ServerSessionMonitor_Start(&monitor)) {
        Mprintf("ERROR: Failed to start session monitor");
        ServerSessionMonitor_Cleanup(&monitor);
        return ERROR_SERVICE_SPECIFIC_ERROR;
    }

    Mprintf("Session monitor started successfully");
    Mprintf("Yama GUI will be launched automatically in user sessions");

    // 主循环，只等待停止信号
    while (WaitForSingleObject(g_StopEvent, 10000) != WAIT_OBJECT_0) {
        heartbeatCount++;
        if (heartbeatCount % 6 == 0) {  // 每60秒记录一次（10秒 * 6 = 60秒）
            sprintf_s(buf, sizeof(buf), "Service heartbeat - uptime: %d minutes", heartbeatCount / 6);
            Mprintf(buf);
        }
    }

    Mprintf("Stop signal received");
    Mprintf("Stopping session monitor...");
    ServerSessionMonitor_Stop(&monitor);
    ServerSessionMonitor_Cleanup(&monitor);

    Mprintf("Worker thread exiting");
    Mprintf("========================================");
    return ERROR_SUCCESS;
}

BOOL ServerService_Install(void)
{
    SC_HANDLE schSCManager = OpenSCManager(
                                 NULL,
                                 NULL,
                                 SC_MANAGER_ALL_ACCESS
                             );

    if (schSCManager == NULL) {
        Mprintf("ERROR: OpenSCManager failed (%d)\n", (int)GetLastError());
        Mprintf("Please run as Administrator\n");

        return FALSE;
    }

    char szPath[MAX_PATH];
    if (!GetModuleFileNameA(NULL, szPath, MAX_PATH)) {
        Mprintf("ERROR: GetModuleFileName failed (%d)\n", (int)GetLastError());
        CloseServiceHandle(schSCManager);
        return FALSE;
    }

    // 添加 -service 参数
    char szPathWithArg[MAX_PATH + 32];
    sprintf_s(szPathWithArg, sizeof(szPathWithArg), "\"%s\" -service", szPath);

    Mprintf("Installing service...\n");
    Mprintf("Executable path: %s\n", szPathWithArg);

    SC_HANDLE schService = CreateServiceA(
                               schSCManager,
                               SERVER_SERVICE_NAME,
                               SERVER_SERVICE_DISPLAY,
                               SERVICE_ALL_ACCESS,
                               SERVICE_WIN32_OWN_PROCESS,
                               SERVICE_AUTO_START,
                               SERVICE_ERROR_NORMAL,
                               szPathWithArg,
                               NULL, NULL, NULL, NULL, NULL
                           );

    if (schService == NULL) {
        DWORD err = GetLastError();
        if (err == ERROR_SERVICE_EXISTS) {
            Mprintf("INFO: Service already exists\n");
            schService = OpenServiceA(schSCManager, SERVER_SERVICE_NAME, SERVICE_ALL_ACCESS);
            if (schService) {
                Mprintf("SUCCESS: Service is already installed\n");
                CloseServiceHandle(schService);
            }
            return TRUE;
        } else if (err == ERROR_ACCESS_DENIED) {
            Mprintf("ERROR: Access denied. Please run as Administrator\n");
        } else {
            Mprintf("ERROR: CreateService failed (%d)\n", (int)err);
        }
        CloseServiceHandle(schSCManager);
        return FALSE;
    }

    Mprintf("SUCCESS: Service created successfully\n");

    // 设置服务描述
    SERVICE_DESCRIPTION sd;
    sd.lpDescription = (LPSTR)SERVER_SERVICE_DESC;
    ChangeServiceConfig2(schService, SERVICE_CONFIG_DESCRIPTION, &sd);

    // 立即启动服务
    DWORD err = 0;
    Mprintf("Starting service...\n");
    if (StartServiceA(schService, 0, NULL)) {
        Mprintf("SUCCESS: Service started successfully\n");
        Sleep(2000);

        SERVICE_STATUS status;
        if (QueryServiceStatus(schService, &status)) {
            if (status.dwCurrentState == SERVICE_RUNNING) {
                Mprintf("SUCCESS: Service is running\n");
            } else {
                Mprintf("WARNING: Service state: %d\n", (int)status.dwCurrentState);
            }
        }
    } else {
        err = GetLastError();
        if (err == ERROR_SERVICE_ALREADY_RUNNING) {
            Mprintf("INFO: Service is already running\n");
            err = 0;
        } else {
            Mprintf("WARNING: StartService failed (%d)\n", (int)err);
        }
    }

    CloseServiceHandle(schService);
    CloseServiceHandle(schSCManager);
    return err == 0;
}

BOOL ServerService_Uninstall(void)
{
    SC_HANDLE schSCManager = OpenSCManager(
                                 NULL,
                                 NULL,
                                 SC_MANAGER_ALL_ACCESS
                             );

    if (schSCManager == NULL) {
        Mprintf("ERROR: OpenSCManager failed (%d)\n", (int)GetLastError());
        return FALSE;
    }

    SC_HANDLE schService = OpenServiceA(
                               schSCManager,
                               SERVER_SERVICE_NAME,
                               SERVICE_STOP | DELETE | SERVICE_QUERY_STATUS
                           );

    if (schService == NULL) {
        Mprintf("ERROR: OpenService failed (%d)\n", (int)GetLastError());
        CloseServiceHandle(schSCManager);
        return FALSE;
    }

    // 停止服务
    SERVICE_STATUS status;
    Mprintf("Stopping service...\n");
    if (ControlService(schService, SERVICE_CONTROL_STOP, &status)) {
        Mprintf("Waiting for service to stop");
        Sleep(1000);

        int waitCount = 0;
        while (QueryServiceStatus(schService, &status) && waitCount < 30) {
            if (status.dwCurrentState == SERVICE_STOP_PENDING) {
                Mprintf(".");
                Sleep(1000);
                waitCount++;
            } else {
                break;
            }
        }
        Mprintf("\n");
    } else {
        DWORD err = GetLastError();
        if (err != ERROR_SERVICE_NOT_ACTIVE) {
            Mprintf("WARNING: Failed to stop service (%d)\n", (int)err);
        }
    }

    BOOL r = FALSE;
    // 删除服务
    Mprintf("Deleting service...\n");
    if (DeleteService(schService)) {
        Mprintf("SUCCESS: Service uninstalled successfully\n");
        r = TRUE;
    } else {
        Mprintf("ERROR: DeleteService failed (%d)\n", (int)GetLastError());
    }

    CloseServiceHandle(schService);
    CloseServiceHandle(schSCManager);
    return r;
}
