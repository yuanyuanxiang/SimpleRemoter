#include "ServiceWrapper.h"
#include "SessionMonitor.h"
#include <stdio.h>

#ifndef Mprintf
#ifdef _DEBUG
#define Mprintf printf
#define Log(p) ServiceWriteLog(p, "C:\\GhostService.log")
#else
#define Mprintf(format, ...)
#define Log(p) 
#endif
#endif

// 静态变量
static MyService g_MyService = 
{ "RemoteControlService", "Remote Control Service", "Provides remote desktop control functionality."};

static SERVICE_STATUS g_ServiceStatus = { 0 };
static SERVICE_STATUS_HANDLE g_StatusHandle = NULL;
static HANDLE g_StopEvent = NULL;

// 前向声明
static void WINAPI ServiceMain(DWORD argc, LPTSTR* argv);
static void WINAPI ServiceCtrlHandler(DWORD ctrlCode);

void InitWindowsService(MyService info){
    memcpy(&g_MyService, &info, sizeof(MyService));
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
                   g_MyService.Name,
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
    hService = OpenServiceA(hSCM, g_MyService.Name, SERVICE_START);
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

    ServiceTable[0].lpServiceName = (LPSTR)g_MyService.Name;
    ServiceTable[0].lpServiceProc = (LPSERVICE_MAIN_FUNCTION)ServiceMain;
    ServiceTable[1].lpServiceName = NULL;
    ServiceTable[1].lpServiceProc = NULL;

    Log("========================================");
    Log("ServiceWrapper_Run() called");

    if (StartServiceCtrlDispatcher(ServiceTable) == FALSE) {
        err = GetLastError();
        sprintf(buffer, "StartServiceCtrlDispatcher failed: %d", (int)err);
        Log(buffer);
        return (int)err;
    }
    return ERROR_SUCCESS;
}

static void WINAPI ServiceMain(DWORD argc, LPTSTR* argv)
{
    HANDLE hThread;

    (void)argc;
    (void)argv;

    Log("ServiceMain() called");

    g_StatusHandle = RegisterServiceCtrlHandler(
                         g_MyService.Name,
                         ServiceCtrlHandler
                     );

    if (g_StatusHandle == NULL) {
        Log("RegisterServiceCtrlHandler failed");
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
        Log("CreateEvent failed");
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
    Log("Service is now running");

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
    Log("Service stopped");
}

static void WINAPI ServiceCtrlHandler(DWORD ctrlCode)
{
    switch (ctrlCode) {
    case SERVICE_CONTROL_STOP:
        Log("SERVICE_CONTROL_STOP received");

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

    Log("========================================");
    Log("Worker thread started");
    Log("Service will launch agent in user sessions");

    // 初始化会话监控器
    SessionMonitor_Init(&monitor);

    if (!SessionMonitor_Start(&monitor)) {
        Log("ERROR: Failed to start session monitor");
        SessionMonitor_Cleanup(&monitor);
        return ERROR_SERVICE_SPECIFIC_ERROR;
    }

    Log("Session monitor started successfully");
    Log("Agent will be launched automatically");

    // 主循环，只等待停止信号
    // SessionMonitor 会在后台自动：
    // 1. 监控会话
    // 2. 在用户会话中启动 agent.exe
    // 3. 监视代理进程，如果退出自动重启
    while (WaitForSingleObject(g_StopEvent, 10000) != WAIT_OBJECT_0) {
        heartbeatCount++;
        if (heartbeatCount % 6 == 0) {  // 每60秒记录一次
            sprintf(buf, "Service heartbeat - uptime: %d minutes", heartbeatCount / 6);
            Log(buf);
        }
    }

    Log("Stop signal received");
    Log("Stopping session monitor...");
    SessionMonitor_Stop(&monitor);
    SessionMonitor_Cleanup(&monitor);

    Log("Worker thread exiting");
    Log("========================================");
    return ERROR_SUCCESS;
}

BOOL ServiceWrapper_Install(void)
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
        return FALSE;
    }

    if (!GetModuleFileName(NULL, szPath, MAX_PATH)) {
        Mprintf("ERROR: GetModuleFileName failed (%d)\n", (int)GetLastError());
        CloseServiceHandle(schSCManager);
        return FALSE;
    }

    Mprintf("Installing service...\n");
    Mprintf("Executable path: %s\n", szPath);

    schService = CreateService(
                     schSCManager,
                     g_MyService.Name,
                     g_MyService.Display,
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
            schService = OpenService(schSCManager, g_MyService.Name, SERVICE_ALL_ACCESS);
            if (schService) {
                Mprintf("SUCCESS: Service is already installed\n");
                CloseServiceHandle(schService);
				CloseServiceHandle(schSCManager);
				return TRUE;
            }
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
    sd.lpDescription = (LPSTR)g_MyService.Description;
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
            Mprintf("You can start it manually using: net start %s\n", g_MyService.Name);
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
    Mprintf("  To verify: sc query %s\n", g_MyService.Name);
    Mprintf("  To start:  net start %s\n", g_MyService.Name);
    Mprintf("  To stop:   net stop %s\n", g_MyService.Name);

    return TRUE;
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
                     g_MyService.Name,
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

void PrintUsage()
{
	Mprintf("Usage:\n");
	Mprintf("  -install     Install as Windows service\n");
	Mprintf("  -uninstall   Uninstall service\n");
	Mprintf("  -service     Run as service\n");
	Mprintf("  -agent       Run as agent\n");
	Mprintf("  default      Run as normal application\n");
	Mprintf("\n");
}

// 从服务路径中提取可执行文件路径（去除引号和参数）
static void ExtractExePath(const char* input, char* output, size_t outSize)
{
	const char* start = input;
	const char* end;
	size_t len;

	if (outSize == 0) return;
	output[0] = '\0';

	// 跳过开头的引号
	if (*start == '"') {
		start++;
		end = strchr(start, '"');
		if (!end) end = start + strlen(start);
	} else {
		// 找到第一个空格（参数分隔）或字符串结尾
		end = strchr(start, ' ');
		if (!end) end = start + strlen(start);
	}

	len = end - start;
	if (len >= outSize) len = outSize - 1;

	strncpy(output, start, len);
	output[len] = '\0';
}

BOOL RunAsWindowsService(int argc, const char* argv[])
{
	if (argc == 1) {  // 无参数时，作为服务启动
		BOOL registered = FALSE;
		BOOL running = FALSE;
		char servicePath[MAX_PATH] = { 0 };
		char serviceExePath[MAX_PATH] = { 0 };
		char curPath[MAX_PATH] = { 0 };

		ServiceWrapper_CheckStatus(&registered, &running, servicePath, MAX_PATH);
		GetModuleFileName(NULL, curPath, MAX_PATH);

		// 从服务路径中提取可执行文件路径（去除引号和参数）
		ExtractExePath(servicePath, serviceExePath, MAX_PATH);

		// 使用不区分大小写的比较
		if (registered && _stricmp(curPath, serviceExePath) != 0) {
			Mprintf("RunAsWindowsService Uninstall: %s\n", servicePath);
			ServiceWrapper_Uninstall();
			registered = FALSE;
		}
		if (!registered) {
			Mprintf("RunAsWindowsService Install: %s\n", curPath);
			return ServiceWrapper_Install();
		} else if (!running) {
			int r = ServiceWrapper_Run();
			Mprintf("RunAsWindowsService Run '%s' %s\n", curPath, r == ERROR_SUCCESS ? "succeed" : "failed");
			if (r) {
				r = ServiceWrapper_StartSimple();
				Mprintf("RunService Start '%s' %s\n", curPath, r == ERROR_SUCCESS ? "succeed" : "failed");
                return r == ERROR_SUCCESS;
			}
            return TRUE;
		}
        return TRUE;
	} else if (argc > 1) {
		if (_stricmp(argv[1], "-install") == 0) {
			return ServiceWrapper_Install();
		}
		else if (_stricmp(argv[1], "-uninstall") == 0) {
			ServiceWrapper_Uninstall();
			return TRUE;
		}
		else if (_stricmp(argv[1], "-service") == 0) {
			return ServiceWrapper_Run() == ERROR_SUCCESS;
		}
		else if (_stricmp(argv[1], "-agent") == 0) {
			return FALSE;
		}
		else if (_stricmp(argv[1], "-help") == 0 || _stricmp(argv[1], "/?") == 0) {
			PrintUsage();
			return TRUE;
		}
	}

	return FALSE;
}
