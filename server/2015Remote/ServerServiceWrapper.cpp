#include "stdafx.h"
#include "ServerServiceWrapper.h"
#include "ServerSessionMonitor.h"
#include <stdio.h>
#include <winsvc.h>


// 闈欐€佸彉閲?
static SERVICE_STATUS g_ServiceStatus;
static SERVICE_STATUS_HANDLE g_StatusHandle = NULL;
static HANDLE g_StopEvent = INVALID_HANDLE_VALUE;

// 鍓嶅悜澹版槑
static void WINAPI ServiceMain(DWORD argc, LPTSTR* argv);
static void WINAPI ServiceCtrlHandler(DWORD ctrlCode);

BOOL ServerService_CheckStatus(BOOL* registered, BOOL* running,
                               char* exePath, size_t exePathSize)
{
    *registered = FALSE;
    *running = FALSE;
    if (exePath && exePathSize > 0) {
        exePath[0] = '\0';
    }

    // 鎵撳紑 SCM
    SC_HANDLE hSCM = OpenSCManagerA(NULL, NULL, SC_MANAGER_CONNECT);
    if (!hSCM) {
        return FALSE;
    }

    // 鎵撳紑鏈嶅姟
    SC_HANDLE hService = OpenServiceA(
                             hSCM,
                             SERVER_SERVICE_NAME,
                             SERVICE_QUERY_STATUS | SERVICE_QUERY_CONFIG);
    if (!hService) {
        CloseServiceHandle(hSCM);
        return FALSE;  // 鏈敞鍐?
    }

    *registered = TRUE;

    // 鑾峰彇鏈嶅姟鐘舵€?
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

    // 鑾峰彇 EXE 璺緞
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
    // 鎵撳紑SCM
    SC_HANDLE hSCM = OpenSCManagerA(NULL, NULL, SC_MANAGER_CONNECT);
    if (!hSCM) {
        return (int)GetLastError();
    }

    // 鎵撳紑鏈嶅姟骞跺惎鍔?
    SC_HANDLE hService = OpenServiceA(hSCM, SERVER_SERVICE_NAME, SERVICE_START);
    if (!hService) {
        int err = (int)GetLastError();
        CloseServiceHandle(hSCM);
        return err;
    }

    // 鍚姩鏈嶅姟
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
    // 鎵撳紑SCM
    SC_HANDLE hSCM = OpenSCManagerA(NULL, NULL, SC_MANAGER_CONNECT);
    if (!hSCM) {
        return (int)GetLastError();
    }

    // 鎵撳紑鏈嶅姟
    SC_HANDLE hService = OpenServiceA(hSCM, SERVER_SERVICE_NAME, SERVICE_STOP | SERVICE_QUERY_STATUS);
    if (!hService) {
        int err = (int)GetLastError();
        CloseServiceHandle(hSCM);
        return err;
    }

    // 鏌ヨ褰撳墠鐘舵€?
    SERVICE_STATUS status;
    if (!QueryServiceStatus(hService, &status)) {
        int err = (int)GetLastError();
        CloseServiceHandle(hService);
        CloseServiceHandle(hSCM);
        return err;
    }

    // 濡傛灉鏈嶅姟鏈繍琛岋紝鐩存帴杩斿洖鎴愬姛
    if (status.dwCurrentState == SERVICE_STOPPED) {
        CloseServiceHandle(hService);
        CloseServiceHandle(hSCM);
        return ERROR_SUCCESS;
    }

    // 鍙戦€佸仠姝㈡帶鍒跺懡浠?
    if (!ControlService(hService, SERVICE_CONTROL_STOP, &status)) {
        DWORD err = GetLastError();
        if (err != ERROR_SERVICE_NOT_ACTIVE) {
            CloseServiceHandle(hService);
            CloseServiceHandle(hSCM);
            return (int)err;
        }
    }

    // 绛夊緟鏈嶅姟鍋滄锛堟渶澶?0绉掞級
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

// 鏈嶅姟宸ヤ綔绾跨▼
DWORD WINAPI ServerService_WorkerThread(LPVOID lpParam)
{
    (void)lpParam;
    int heartbeatCount = 0;
    char buf[128];

    Mprintf("========================================");
    Mprintf("Worker thread started");
    Mprintf("Service will launch Yama GUI in user sessions");

    // 鍒濆鍖栦細璇濈洃鎺у櫒
    ServerSessionMonitor monitor;
    ServerSessionMonitor_Init(&monitor);

    if (!ServerSessionMonitor_Start(&monitor)) {
        Mprintf("ERROR: Failed to start session monitor");
        ServerSessionMonitor_Cleanup(&monitor);
        return ERROR_SERVICE_SPECIFIC_ERROR;
    }

    Mprintf("Session monitor started successfully");
    Mprintf("Yama GUI will be launched automatically in user sessions");

    // 涓诲惊鐜紝鍙瓑寰呭仠姝俊鍙?
    while (WaitForSingleObject(g_StopEvent, 10000) != WAIT_OBJECT_0) {
        heartbeatCount++;
        if (heartbeatCount % 6 == 0) {  // 姣?0绉掕褰曚竴娆★紙10绉?* 6 = 60绉掞級
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

    // 娣诲姞 -service 鍙傛暟
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

    // 璁剧疆鏈嶅姟鎻忚堪
    SERVICE_DESCRIPTION sd;
    sd.lpDescription = (LPSTR)SERVER_SERVICE_DESC;
    ChangeServiceConfig2(schService, SERVICE_CONFIG_DESCRIPTION, &sd);

    // 绔嬪嵆鍚姩鏈嶅姟
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

    // 鍋滄鏈嶅姟
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
    // 鍒犻櫎鏈嶅姟
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
