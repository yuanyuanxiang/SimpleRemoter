#include "ServiceWrapper.h"
#include "SessionMonitor.h"
#include <stdio.h>

#ifndef Mprintf
#ifdef _DEBUG
#define Mprintf printf
#else
#define Mprintf(format, ...)
#endif
#endif

// 外部声明
extern BOOL RunAsAgent(BOOL block);

// 静态变量
BOOL g_ServiceDirectMode = FALSE;
static SERVICE_STATUS g_ServiceStatus;
static SERVICE_STATUS_HANDLE g_StatusHandle = NULL;
static HANDLE g_StopEvent = INVALID_HANDLE_VALUE;

// 前向声明
static void WINAPI ServiceMain(DWORD argc, LPTSTR* argv);
static void WINAPI ServiceCtrlHandler(DWORD ctrlCode);
static void ServiceWriteLog(const char* message);

// 日志函数
static void ServiceWriteLog(const char* message)
{
    FILE* f;
    SYSTEMTIME st;

    f = fopen("C:\\GhostService.log", "a");
    if (f) {
        GetLocalTime(&st);
        fprintf(f, "[%04d-%02d-%02d %02d:%02d:%02d] %s\n",
                st.wYear, st.wMonth, st.wDay,
                st.wHour, st.wMinute, st.wSecond,
                message);
        fclose(f);
    }
}

BOOL ServiceWrapper_CheckStatus(BOOL* registered, BOOL* running,
                                char* exePath, size_t exePathSize)
{
    SC_HANDLE hSCM = NULL;
    SC_HANDLE hService = NULL;
    BOOL result = FALSE;
    SERVICE_STATUS_PROCESS ssp;
    DWORD bytesNeeded = 0;
    DWORD bufSize = 0;
    LPQUERY_SERVICE_CONFIGA pConfig = NULL;

    *registered = FALSE;
    *running = FALSE;
    if (exePath && exePathSize > 0) {
        exePath[0] = '\0';
    }

    // 打开 SCM
    hSCM = OpenSCManagerA(NULL, NULL, SC_MANAGER_CONNECT);
    if (!hSCM) {
        return FALSE;
    }

    // 打开服务
    hService = OpenServiceA(
                   hSCM,
                   SERVICE_NAME,
                   SERVICE_QUERY_STATUS | SERVICE_QUERY_CONFIG);
    if (!hService) {
        CloseServiceHandle(hSCM);
        return FALSE;  // 未注册
    }

    *registered = TRUE;
    result = TRUE;

    // 获取服务状态
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
        QueryServiceConfigA(hService, NULL, 0, &bufSize);

        if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
            pConfig = (LPQUERY_SERVICE_CONFIGA)malloc(bufSize);

            if (pConfig) {
                if (QueryServiceConfigA(hService, pConfig, bufSize, &bufSize)) {
                    strncpy(exePath, pConfig->lpBinaryPathName, exePathSize - 1);
                    exePath[exePathSize - 1] = '\0';
                }
                free(pConfig);
            }
        }
    }

    CloseServiceHandle(hService);
    CloseServiceHandle(hSCM);
    return result;
}

int ServiceWrapper_StartSimple(void)
{
    SC_HANDLE hSCM = NULL;
    SC_HANDLE hService = NULL;
    BOOL ok;
    int err;

    // 打开SCM
    hSCM = OpenSCManagerA(NULL, NULL, SC_MANAGER_CONNECT);
    if (!hSCM) {
        return (int)GetLastError();
    }

    // 打开服务并启动
    hService = OpenServiceA(hSCM, SERVICE_NAME, SERVICE_START);
    if (!hService) {
        err = (int)GetLastError();
        CloseServiceHandle(hSCM);
        return err;
    }

    // 启动服务
    ok = StartServiceA(hService, 0, NULL);
    err = ok ? ERROR_SUCCESS : (int)GetLastError();

    CloseServiceHandle(hService);
    CloseServiceHandle(hSCM);

    return err;
}

int ServiceWrapper_Run(void)
{
    DWORD err;
    char buffer[256];
    SERVICE_TABLE_ENTRY ServiceTable[2];

    ServiceTable[0].lpServiceName = (LPSTR)SERVICE_NAME;
    ServiceTable[0].lpServiceProc = (LPSERVICE_MAIN_FUNCTION)ServiceMain;
    ServiceTable[1].lpServiceName = NULL;
    ServiceTable[1].lpServiceProc = NULL;

    ServiceWriteLog("========================================");
    ServiceWriteLog("ServiceWrapper_Run() called");

    if (StartServiceCtrlDispatcher(ServiceTable) == FALSE) {
        err = GetLastError();
        sprintf(buffer, "StartServiceCtrlDispatcher failed: %d", (int)err);
        ServiceWriteLog(buffer);
        return (int)err;
    }
    return ERROR_SUCCESS;
}

static void WINAPI ServiceMain(DWORD argc, LPTSTR* argv)
{
    HANDLE hThread;

    (void)argc;
    (void)argv;

    ServiceWriteLog("ServiceMain() called");

    g_StatusHandle = RegisterServiceCtrlHandler(
                         SERVICE_NAME,
                         ServiceCtrlHandler
                     );

    if (g_StatusHandle == NULL) {
        ServiceWriteLog("RegisterServiceCtrlHandler failed");
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
        ServiceWriteLog("CreateEvent failed");
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
    ServiceWriteLog("Service is now running");

    hThread = CreateThread(NULL, 0, ServiceWrapper_WorkerThread, NULL, 0, NULL);
    if (hThread) {
        WaitForSingleObject(hThread, INFINITE);
        CloseHandle(hThread);
    }

    CloseHandle(g_StopEvent);

    g_ServiceStatus.dwControlsAccepted = 0;
    g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
    g_ServiceStatus.dwWin32ExitCode = 0;
    g_ServiceStatus.dwCheckPoint = 3;

    SetServiceStatus(g_StatusHandle, &g_ServiceStatus);
    ServiceWriteLog("Service stopped");
}

static void WINAPI ServiceCtrlHandler(DWORD ctrlCode)
{
    switch (ctrlCode) {
    case SERVICE_CONTROL_STOP:
        ServiceWriteLog("SERVICE_CONTROL_STOP received");

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
DWORD WINAPI ServiceWrapper_WorkerThread(LPVOID lpParam)
{
    SessionMonitor monitor;
    int heartbeatCount = 0;
    char buf[128];

    (void)lpParam;  // 未使用参数

    if (g_ServiceDirectMode) {
        // 直接模式：在服务进程中运行（SYSTEM权限）
        ServiceWriteLog("Running in DIRECT mode (SYSTEM)");
        RunAsAgent(FALSE);
        WaitForSingleObject(g_StopEvent, INFINITE);
        return ERROR_SUCCESS;
    }

    ServiceWriteLog("========================================");
    ServiceWriteLog("Worker thread started");
    ServiceWriteLog("Service will launch agent in user sessions");

    // 初始化会话监控器
    SessionMonitor_Init(&monitor);

    if (!SessionMonitor_Start(&monitor)) {
        ServiceWriteLog("ERROR: Failed to start session monitor");
        SessionMonitor_Cleanup(&monitor);
        return ERROR_SERVICE_SPECIFIC_ERROR;
    }

    ServiceWriteLog("Session monitor started successfully");
    ServiceWriteLog("Agent will be launched automatically");

    // 主循环，只等待停止信号
    // SessionMonitor 会在后台自动：
    // 1. 监控会话
    // 2. 在用户会话中启动 agent.exe
    // 3. 监视代理进程，如果退出自动重启
    while (WaitForSingleObject(g_StopEvent, 10000) != WAIT_OBJECT_0) {
        heartbeatCount++;
        if (heartbeatCount % 6 == 0) {  // 每60秒记录一次
            sprintf(buf, "Service heartbeat - uptime: %d minutes", heartbeatCount);
            ServiceWriteLog(buf);
        }
    }

    ServiceWriteLog("Stop signal received");
    ServiceWriteLog("Stopping session monitor...");
    SessionMonitor_Stop(&monitor);
    SessionMonitor_Cleanup(&monitor);

    ServiceWriteLog("Worker thread exiting");
    ServiceWriteLog("========================================");
    return ERROR_SUCCESS;
}

void ServiceWrapper_Install(void)
{
    SC_HANDLE schSCManager;
    SC_HANDLE schService;
    char szPath[MAX_PATH];
    SERVICE_DESCRIPTION sd;
    SERVICE_STATUS status;
    DWORD err;

    schSCManager = OpenSCManager(
                       NULL,
                       NULL,
                       SC_MANAGER_ALL_ACCESS
                   );

    if (schSCManager == NULL) {
        Mprintf("ERROR: OpenSCManager failed (%d)\n", (int)GetLastError());
        Mprintf("Please run as Administrator\n");
        return;
    }

    if (!GetModuleFileName(NULL, szPath, MAX_PATH)) {
        Mprintf("ERROR: GetModuleFileName failed (%d)\n", (int)GetLastError());
        CloseServiceHandle(schSCManager);
        return;
    }

    Mprintf("Installing service...\n");
    Mprintf("Executable path: %s\n", szPath);

    schService = CreateService(
                     schSCManager,
                     SERVICE_NAME,
                     SERVICE_DISPLAY,
                     SERVICE_ALL_ACCESS,
                     SERVICE_WIN32_OWN_PROCESS,
                     SERVICE_AUTO_START,
                     SERVICE_ERROR_NORMAL,
                     szPath,
                     NULL, NULL, NULL, NULL, NULL
                 );

    if (schService == NULL) {
        err = GetLastError();
        if (err == ERROR_SERVICE_EXISTS) {
            Mprintf("INFO: Service already exists\n");

            // 打开已存在的服务
            schService = OpenService(schSCManager, SERVICE_NAME, SERVICE_ALL_ACCESS);
            if (schService) {
                Mprintf("SUCCESS: Service is already installed\n");
                CloseServiceHandle(schService);
            }
        } else if (err == ERROR_ACCESS_DENIED) {
            Mprintf("ERROR: Access denied. Please run as Administrator\n");
        } else {
            Mprintf("ERROR: CreateService failed (%d)\n", (int)err);
        }
        CloseServiceHandle(schSCManager);
        return;
    }

    Mprintf("SUCCESS: Service created successfully\n");

    // 设置服务描述
    sd.lpDescription = (LPSTR)SERVICE_DESC;
    if (ChangeServiceConfig2(schService, SERVICE_CONFIG_DESCRIPTION, &sd)) {
        Mprintf("SUCCESS: Service description set\n");
    }

    // 立即启动服务
    Mprintf("Starting service...\n");
    if (StartService(schService, 0, NULL)) {
        Mprintf("SUCCESS: Service started successfully\n");

        // 等待服务启动
        Sleep(2000);

        // 检查服务状态
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
        } else {
            Mprintf("WARNING: StartService failed (%d)\n", (int)err);
            Mprintf("You can start it manually using: net start %s\n", SERVICE_NAME);
        }
    }

    CloseServiceHandle(schService);
    CloseServiceHandle(schSCManager);

    Mprintf("\n=== Installation Complete ===\n");
    Mprintf("Service installed successfully!\n");
    Mprintf("\n");
    Mprintf("IMPORTANT: This is a single-executable design.\n");
    Mprintf("The service will launch '%s -agent' in user sessions.\n", szPath);
    Mprintf("\n");
    Mprintf("Logs will be written to:\n");
    Mprintf("  - C:\\GhostService.log (service logs)\n");
    Mprintf("  - C:\\SessionMonitor.log (session monitor logs)\n");
    Mprintf("\n");
    Mprintf("Commands:\n");
    Mprintf("  To verify: sc query %s\n", SERVICE_NAME);
    Mprintf("  To start:  net start %s\n", SERVICE_NAME);
    Mprintf("  To stop:   net stop %s\n", SERVICE_NAME);
}

void ServiceWrapper_Uninstall(void)
{
    SC_HANDLE schSCManager;
    SC_HANDLE schService;
    SERVICE_STATUS status;
    int waitCount;
    DWORD err;

    schSCManager = OpenSCManager(
                       NULL,
                       NULL,
                       SC_MANAGER_ALL_ACCESS
                   );

    if (schSCManager == NULL) {
        Mprintf("ERROR: OpenSCManager failed (%d)\n", (int)GetLastError());
        Mprintf("Please run as Administrator\n");
        return;
    }

    schService = OpenService(
                     schSCManager,
                     SERVICE_NAME,
                     SERVICE_STOP | DELETE
                 );

    if (schService == NULL) {
        Mprintf("ERROR: OpenService failed (%d)\n", (int)GetLastError());
        Mprintf("Service may not be installed\n");
        CloseServiceHandle(schSCManager);
        return;
    }

    Mprintf("Stopping service...\n");
    if (ControlService(schService, SERVICE_CONTROL_STOP, &status)) {
        Mprintf("Waiting for service to stop");
        Sleep(1000);

        waitCount = 0;
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

        if (status.dwCurrentState == SERVICE_STOPPED) {
            Mprintf("SUCCESS: Service stopped\n");
        } else {
            Mprintf("WARNING: Service may not have stopped completely\n");
        }
    } else {
        err = GetLastError();
        if (err == ERROR_SERVICE_NOT_ACTIVE) {
            Mprintf("INFO: Service was not running\n");
        } else {
            Mprintf("WARNING: Failed to stop service (%d)\n", (int)err);
        }
    }

    Mprintf("Deleting service...\n");
    if (DeleteService(schService)) {
        Mprintf("SUCCESS: Service uninstalled successfully\n");
    } else {
        Mprintf("ERROR: DeleteService failed (%d)\n", (int)GetLastError());
    }

    CloseServiceHandle(schService);
    CloseServiceHandle(schSCManager);

    Mprintf("\n=== Uninstallation Complete ===\n");
}
