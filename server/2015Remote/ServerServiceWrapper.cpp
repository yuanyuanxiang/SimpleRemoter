#include "stdafx.h"
#include "ServerServiceWrapper.h"
#include "ServerSessionMonitor.h"
#include <stdio.h>
#include <winsvc.h>


// 静态变量
static SERVICE_STATUS g_ServiceStatus;
static SERVICE_STATUS_HANDLE g_StatusHandle = NULL;
static HANDLE g_StopEvent = INVALID_HANDLE_VALUE;

// 前向声明
static void WINAPI ServiceMain(DWORD argc, LPTSTR* argv);
static void WINAPI ServiceCtrlHandler(DWORD ctrlCode);
static void ServiceWriteLog(const char* message);

// 获取日志文件路径（程序所在目录）
static void GetServiceLogPath(char* logPath, size_t size)
{
    char exePath[MAX_PATH];
    if (GetModuleFileNameA(NULL, exePath, MAX_PATH)) {
        char* lastSlash = strrchr(exePath, '\\');
        if (lastSlash) {
            *lastSlash = '\0';
            sprintf_s(logPath, size, "%s\\YamaService.log", exePath);
            return;
        }
    }
    // 备用路径：Windows临时目录
    char tempPath[MAX_PATH];
    if (GetTempPathA(MAX_PATH, tempPath)) {
        sprintf_s(logPath, size, "%sYamaService.log", tempPath);
    } else {
        strncpy_s(logPath, size, "YamaService.log", _TRUNCATE);
    }
}

// 日志函数
static void ServiceWriteLog(const char* message)
{
    char logPath[MAX_PATH];
    GetServiceLogPath(logPath, sizeof(logPath));
    FILE* f = fopen(logPath, "a");
    if (f) {
        SYSTEMTIME st;
        GetLocalTime(&st);
        fprintf(f, "[%04d-%02d-%02d %02d:%02d:%02d] %s\n",
            st.wYear, st.wMonth, st.wDay,
            st.wHour, st.wMinute, st.wSecond,
            message);
        fclose(f);
    }
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
        &bytesNeeded))
    {
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

    ServiceWriteLog("========================================");
    ServiceWriteLog("ServerService_Run() called");

    if (StartServiceCtrlDispatcher(ServiceTable) == FALSE) {
        DWORD err = GetLastError();
        char buffer[256];
        sprintf_s(buffer, sizeof(buffer), "StartServiceCtrlDispatcher failed: %d", (int)err);
        ServiceWriteLog(buffer);
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

    ServiceWriteLog("ServiceMain() called");

    g_StatusHandle = RegisterServiceCtrlHandler(
        SERVER_SERVICE_NAME,
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

    HANDLE hThread = CreateThread(NULL, 0, ServerService_WorkerThread, NULL, 0, NULL);
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
DWORD WINAPI ServerService_WorkerThread(LPVOID lpParam)
{
    (void)lpParam;
    int heartbeatCount = 0;
    char buf[128];

    ServiceWriteLog("========================================");
    ServiceWriteLog("Worker thread started");
    ServiceWriteLog("Service will launch Yama GUI in user sessions");

    // 初始化会话监控器
    ServerSessionMonitor monitor;
    ServerSessionMonitor_Init(&monitor);

    if (!ServerSessionMonitor_Start(&monitor)) {
        ServiceWriteLog("ERROR: Failed to start session monitor");
        ServerSessionMonitor_Cleanup(&monitor);
        return ERROR_SERVICE_SPECIFIC_ERROR;
    }

    ServiceWriteLog("Session monitor started successfully");
    ServiceWriteLog("Yama GUI will be launched automatically in user sessions");

    // 主循环，只等待停止信号
    while (WaitForSingleObject(g_StopEvent, 10000) != WAIT_OBJECT_0) {
        heartbeatCount++;
        if (heartbeatCount % 6 == 0) {  // 每60秒记录一次（10秒 * 6 = 60秒）
            sprintf_s(buf, sizeof(buf), "Service heartbeat - uptime: %d minutes", heartbeatCount / 6);
            ServiceWriteLog(buf);
        }
    }

    ServiceWriteLog("Stop signal received");
    ServiceWriteLog("Stopping session monitor...");
    ServerSessionMonitor_Stop(&monitor);
    ServerSessionMonitor_Cleanup(&monitor);

    ServiceWriteLog("Worker thread exiting");
    ServiceWriteLog("========================================");
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
        }
        else if (err == ERROR_ACCESS_DENIED) {
            Mprintf("ERROR: Access denied. Please run as Administrator\n");
        }
        else {
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
            }
            else {
                Mprintf("WARNING: Service state: %d\n", (int)status.dwCurrentState);
            }
        }
    }
    else {
        err = GetLastError();
        if (err == ERROR_SERVICE_ALREADY_RUNNING) {
            Mprintf("INFO: Service is already running\n");
            err = 0;
        }
        else {
            Mprintf("WARNING: StartService failed (%d)\n", (int)err);
        }
    }

    CloseServiceHandle(schService);
    CloseServiceHandle(schSCManager);
    return err == 0;
}

void ServerService_Uninstall(void)
{
    SC_HANDLE schSCManager = OpenSCManager(
        NULL,
        NULL,
        SC_MANAGER_ALL_ACCESS
    );

    if (schSCManager == NULL) {
        Mprintf("ERROR: OpenSCManager failed (%d)\n", (int)GetLastError());
        return;
    }

    SC_HANDLE schService = OpenServiceA(
        schSCManager,
        SERVER_SERVICE_NAME,
        SERVICE_STOP | DELETE | SERVICE_QUERY_STATUS
    );

    if (schService == NULL) {
        Mprintf("ERROR: OpenService failed (%d)\n", (int)GetLastError());
        CloseServiceHandle(schSCManager);
        return;
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
            }
            else {
                break;
            }
        }
        Mprintf("\n");
    }
    else {
        DWORD err = GetLastError();
        if (err != ERROR_SERVICE_NOT_ACTIVE) {
            Mprintf("WARNING: Failed to stop service (%d)\n", (int)err);
        }
    }

    // 删除服务
    Mprintf("Deleting service...\n");
    if (DeleteService(schService)) {
        Mprintf("SUCCESS: Service uninstalled successfully\n");
    }
    else {
        Mprintf("ERROR: DeleteService failed (%d)\n", (int)GetLastError());
    }

    CloseServiceHandle(schService);
    CloseServiceHandle(schSCManager);
}
