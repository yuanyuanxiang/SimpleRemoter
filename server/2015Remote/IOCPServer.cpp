#include "StdAfx.h"
#include "IOCPServer.h"
#include "2015Remote.h"

#include <iostream>
#include <ws2tcpip.h>

// ���� socket ��ȡ�ͻ���IP��ַ.
std::string GetPeerName(SOCKET sock) {
	sockaddr_in  ClientAddr = {};
	int ulClientAddrLen = sizeof(sockaddr_in);
	int s = getpeername(sock, (SOCKADDR*)&ClientAddr, &ulClientAddrLen);
	return s != INVALID_SOCKET ? inet_ntoa(ClientAddr.sin_addr) : "";
}

// ���� socket ��ȡ�ͻ���IP��ַ.
std::string GetRemoteIP(SOCKET sock) {
	sockaddr_in addr;
	int addrLen = sizeof(addr);

	if (getpeername(sock, (sockaddr*)&addr, &addrLen) == 0) {
		char ipStr[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &addr.sin_addr, ipStr, sizeof(ipStr));
		TRACE(">>> �Զ� IP ��ַ: %s\n", ipStr);
		return ipStr;
	}
	TRACE(">>> ��ȡ�Զ� IP ʧ��, ������: %d\n", WSAGetLastError());
	char buf[10];
	sprintf_s(buf, "%d", sock);
	return buf;
}

IOCPServer::IOCPServer(void)
{
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2,2), &wsaData)!=0)
	{
		return;
	}

	m_hCompletionPort = NULL;
	m_sListenSocket   = INVALID_SOCKET;
	m_hListenEvent	      = WSA_INVALID_EVENT;
	m_hListenThread       = NULL;

	m_ulMaxConnections = THIS_CFG.GetInt("settings", "MaxConnection");

	if (m_ulMaxConnections<=0)   
	{
		m_ulMaxConnections = 10000;
	}

	InitializeCriticalSection(&m_cs);

	m_ulWorkThreadCount = 0;

	m_bTimeToKill = FALSE;

	m_ulThreadPoolMin  = 0; 
	m_ulThreadPoolMax  = 0;
	m_ulCPULowThreadsHold  = 0; 
	m_ulCPUHighThreadsHold = 0;
	m_ulCurrentThread = 0;
	m_ulBusyThread = 0;

	m_ulKeepLiveTime = 0;

	m_hKillEvent = NULL;

	m_NotifyProc = NULL;
	m_OfflineProc = NULL;
#if USING_CTX
	m_Cctx = ZSTD_createCCtx();
	m_Dctx = ZSTD_createDCtx();
	ZSTD_CCtx_setParameter(m_Cctx, ZSTD_c_compressionLevel, ZSTD_CLEVEL);
#endif
}

void IOCPServer::Destroy() {
	m_bTimeToKill = TRUE;

	if (m_hKillEvent != NULL)
	{
		SetEvent(m_hKillEvent);
		CloseHandle(m_hKillEvent);
		m_hKillEvent = NULL;
	}

	if (m_sListenSocket != INVALID_SOCKET)
	{
		closesocket(m_sListenSocket);
		m_sListenSocket = INVALID_SOCKET;
	}

	if (m_hCompletionPort != INVALID_HANDLE_VALUE)
	{
		CloseHandle(m_hCompletionPort);
		m_hCompletionPort = INVALID_HANDLE_VALUE;
	}

	if (m_hListenEvent != WSA_INVALID_EVENT)
	{
		CloseHandle(m_hListenEvent);
		m_hListenEvent = WSA_INVALID_EVENT;
	}
}


IOCPServer::~IOCPServer(void)
{
	Destroy();
	while (m_ulWorkThreadCount || m_hListenThread)
		Sleep(10);

	while (!m_ContextConnectionList.IsEmpty())
	{
		CONTEXT_OBJECT *ContextObject = m_ContextConnectionList.GetHead();
		RemoveStaleContext(ContextObject);
		SAFE_DELETE(ContextObject->olps);
	}

	while (!m_ContextFreePoolList.IsEmpty())
	{
		CONTEXT_OBJECT *ContextObject = m_ContextFreePoolList.RemoveHead();
		// ��������б������ʣ�2019.1.14
		//SAFE_DELETE(ContextObject->olps);
		delete ContextObject;
	}

	DeleteCriticalSection(&m_cs);
	m_ulWorkThreadCount = 0;

	m_ulThreadPoolMin  = 0; 
	m_ulThreadPoolMax  = 0;
	m_ulCPULowThreadsHold  = 0; 
	m_ulCPUHighThreadsHold = 0;
	m_ulCurrentThread = 0;
	m_ulBusyThread = 0;
	m_ulKeepLiveTime = 0;

#if USING_CTX
	ZSTD_freeCCtx(m_Cctx);
	ZSTD_freeDCtx(m_Dctx);
#endif

	WSACleanup();
}

// ���ش�����0����ɹ���������������Ϣ.
UINT IOCPServer::StartServer(pfnNotifyProc NotifyProc, pfnOfflineProc OffProc, USHORT uPort)
{
	m_NotifyProc = NotifyProc;
	m_OfflineProc = OffProc;
	m_hKillEvent = CreateEvent(NULL,FALSE,FALSE,NULL);

	if (m_hKillEvent==NULL)
	{
		return 1;
	}

	m_sListenSocket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);     //���������׽���

	if (m_sListenSocket == INVALID_SOCKET)
	{
		return 2;
	}

	m_hListenEvent = WSACreateEvent();           

	if (m_hListenEvent == WSA_INVALID_EVENT)
	{
		closesocket(m_sListenSocket);

		m_sListenSocket = INVALID_SOCKET;
		return 3;
	}

	int iRet = WSAEventSelect(m_sListenSocket,	//�������׽������¼����й���������FD_ACCEPT������
		m_hListenEvent,
		FD_ACCEPT);

	if (iRet == SOCKET_ERROR)
	{
		int a = GetLastError();
		closesocket(m_sListenSocket);

		m_sListenSocket = INVALID_SOCKET;
		WSACloseEvent(m_hListenEvent);

		m_hListenEvent = WSA_INVALID_EVENT;

		return a;
	}

	SOCKADDR_IN	ServerAddr;		
	ServerAddr.sin_port   = htons(uPort);
	ServerAddr.sin_family = AF_INET;
	ServerAddr.sin_addr.s_addr = INADDR_ANY; //��ʼ����������

	//�������׻��ֺ���������bind
	iRet = bind(m_sListenSocket, 
		(sockaddr*)&ServerAddr, 
		sizeof(ServerAddr));

	if (iRet == SOCKET_ERROR)
	{
		int a  = GetLastError();
		closesocket(m_sListenSocket);

		m_sListenSocket = INVALID_SOCKET;
		WSACloseEvent(m_hListenEvent);

		m_hListenEvent = WSA_INVALID_EVENT;

		return a;
	}

	iRet = listen(m_sListenSocket, SOMAXCONN);

	if (iRet == SOCKET_ERROR)
	{
		int a = GetLastError();
		closesocket(m_sListenSocket);

		m_sListenSocket = INVALID_SOCKET;
		WSACloseEvent(m_hListenEvent);

		m_hListenEvent = WSA_INVALID_EVENT;

		return a;
	}

	m_hListenThread =
		(HANDLE)CreateThread(NULL,			
		0,					
		ListenThreadProc, 
		(void*)this,	      //��Thread�ص���������this �������ǵ��̻߳ص��������еĳ�Ա    
		0,					
		NULL);	
	if (m_hListenThread==NULL)
	{
		int a = GetLastError();
		closesocket(m_sListenSocket);

		m_sListenSocket = INVALID_SOCKET;
		WSACloseEvent(m_hListenEvent);

		m_hListenEvent = WSA_INVALID_EVENT;
		return a;
	}

	//���������߳�  1  2
	InitializeIOCP();
	return 0;
}


//1������ɶ˿�
//2���������߳�
BOOL IOCPServer::InitializeIOCP(VOID)
{
	m_hCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0 );
	if ( m_hCompletionPort == NULL ) 
	{
		return FALSE;
	}

	if (m_hCompletionPort==INVALID_HANDLE_VALUE)
	{
		return FALSE;
	}

	SYSTEM_INFO SystemInfo;
	GetSystemInfo(&SystemInfo);  //���PC���м���

	m_ulThreadPoolMin  = 1; 
	m_ulThreadPoolMax  = SystemInfo.dwNumberOfProcessors * 2;
	m_ulCPULowThreadsHold  = 10; 
	m_ulCPUHighThreadsHold = 75; 

	ULONG ulWorkThreadCount = m_ulThreadPoolMax;

	HANDLE hWorkThread = NULL;
	for (int i=0; i<ulWorkThreadCount; ++i)    
	{
		hWorkThread = (HANDLE)CreateThread(NULL, //���������߳�Ŀ���Ǵ���Ͷ�ݵ���ɶ˿��е�����			
			0,						
			WorkThreadProc,     		
			(void*)this,			
			0,						
			NULL);			
		if (hWorkThread == NULL )    		
		{
			CloseHandle(m_hCompletionPort);
			return FALSE;
		}

		AddWorkThread(1);

		CloseHandle(hWorkThread);
	}

	return TRUE;
}


DWORD IOCPServer::WorkThreadProc(LPVOID lParam)
{
	Mprintf("======> IOCPServer WorkThreadProc begin \n");

	IOCPServer* This = (IOCPServer*)(lParam);

	HANDLE   hCompletionPort = This->m_hCompletionPort;
	DWORD    dwTrans = 0;

	PCONTEXT_OBJECT  ContextObject = NULL;
	LPOVERLAPPED     Overlapped    = NULL;
	OVERLAPPEDPLUS*  OverlappedPlus = NULL;
	ULONG            ulBusyThread   = 0;
	BOOL             bError         = FALSE;

	InterlockedIncrement(&This->m_ulCurrentThread);      
	InterlockedIncrement(&This->m_ulBusyThread);         
	timeBeginPeriod(1);
	while (This->m_bTimeToKill==FALSE)
	{
		InterlockedDecrement(&This->m_ulBusyThread);
		// GetQueuedCompletionStatus��ʱ�Ƚϳ������¿ͻ��˷������ݵ�������߲���
		BOOL bOk = GetQueuedCompletionStatus(
			hCompletionPort,
			&dwTrans,
			(PULONG_PTR)&ContextObject,
			&Overlapped, INFINITE);
		DWORD dwIOError = GetLastError();
		OverlappedPlus = CONTAINING_RECORD(Overlapped, OVERLAPPEDPLUS, m_ol);
		ulBusyThread = InterlockedIncrement(&This->m_ulBusyThread); //1 1
		if ( !bOk && dwIOError != WAIT_TIMEOUT )   //���Է����׻��Ʒ����˹ر�                    
		{
			if (ContextObject && This->m_bTimeToKill == FALSE &&dwTrans==0)
			{
				ContextObject->olps = NULL;
				Mprintf("!!! RemoveStaleContext \n");
				This->RemoveStaleContext(ContextObject);
			}
			SAFE_DELETE(OverlappedPlus);
			continue;
		}
		if (!bError) 
		{
			//����һ���µ��̵߳��̵߳��̳߳�
			if (ulBusyThread == This->m_ulCurrentThread)      
			{
				if (ulBusyThread < This->m_ulThreadPoolMax)    
				{
					if (ContextObject != NULL)
					{
						HANDLE hThread = (HANDLE)CreateThread(NULL,				
							0,				
							WorkThreadProc,  
							(void*)This,	    
							0,					
							NULL);

						This->AddWorkThread(hThread ? 1:0);

						CloseHandle(hThread);
					}
				}
			}

			if (!bOk && dwIOError == WAIT_TIMEOUT)      
			{
				if (ContextObject == NULL)
				{
					if (This->m_ulCurrentThread > This->m_ulThreadPoolMin)
					{
						break;
					}

					bError = TRUE;
				}
			}
		}

		if (!bError && !This->m_bTimeToKill)
		{
			if(bOk && OverlappedPlus!=NULL && ContextObject!=NULL) 
			{
				try
				{   
					This->HandleIO(OverlappedPlus->m_ioType, ContextObject, dwTrans);  

					ContextObject = NULL;
				}
				catch (...) {
					Mprintf("This->HandleIO catched an error!!!");
				}
			}
		}

		SAFE_DELETE(OverlappedPlus);
	}
	timeEndPeriod(1);
	SAFE_DELETE(OverlappedPlus);

	InterlockedDecrement(&This->m_ulCurrentThread);
	InterlockedDecrement(&This->m_ulBusyThread);
	int n= This->AddWorkThread(-1);
	if (n == 0) {
		Mprintf("======> IOCPServer All WorkThreadProc done\n");
	}
	Mprintf("======> IOCPServer WorkThreadProc end \n");

	return 0;
}

//�ڹ����߳��б�����
BOOL IOCPServer::HandleIO(IOType PacketFlags,PCONTEXT_OBJECT ContextObject, DWORD dwTrans)
{
	BOOL bRet = FALSE;

	switch (PacketFlags)
	{
	case IOInitialize:
		bRet = OnClientInitializing(ContextObject, dwTrans); 
		break;
	case IORead:
		bRet = OnClientReceiving(ContextObject,dwTrans);
		break;
	case IOWrite:
		bRet = OnClientPostSending(ContextObject,dwTrans);
		break;
	case IOIdle:
		Mprintf("=> HandleIO PacketFlags= IOIdle\n");
		break;
	default:
		break;
	}

	return bRet;
}


BOOL IOCPServer::OnClientInitializing(PCONTEXT_OBJECT  ContextObject, DWORD dwTrans)
{
	return TRUE;
}

BOOL IOCPServer::OnClientReceiving(PCONTEXT_OBJECT  ContextObject, DWORD dwTrans)
{
	try
	{
		if (dwTrans == 0)    //�Է��ر����׽���
		{
			RemoveStaleContext(ContextObject);
			return FALSE;
		}
		//�����յ������ݿ����������Լ����ڴ���wsabuff    8192
		ContextObject->InCompressedBuffer.WriteBuffer((PBYTE)ContextObject->szBuffer,dwTrans);
		//�鿴���ݰ��������
		while (true)
		{
			PR pr = ContextObject->Parse(ContextObject->InCompressedBuffer);
			if (pr.IsFailed())
			{
				ContextObject->InCompressedBuffer.ClearBuffer();
				break;
			}
			else if (pr.IsNeedMore()) {
				break;
			}
			else if (pr.IsWinOSLogin())
			{
				ContextObject->InDeCompressedBuffer.ClearBuffer();
				ULONG ulCompressedLength = 0;
				ULONG ulOriginalLength = 0;
				PBYTE CompressedBuffer = ContextObject->ReadBuffer(ulCompressedLength, ulOriginalLength);
				ContextObject->InDeCompressedBuffer.WriteBuffer(CompressedBuffer, ulCompressedLength);
				m_NotifyProc(ContextObject);
				break;
			}
			
			ULONG ulPackTotalLength = 0;
			ContextObject->InCompressedBuffer.CopyBuffer(&ulPackTotalLength, sizeof(ULONG), pr.Result);
			//ȡ�����ݰ����ܳ���5�ֽڱ�ʶ+4�ֽ����ݰ��ܳ���+4�ֽ�ԭʼ���ݳ���
			int bufLen = ContextObject->InCompressedBuffer.GetBufferLength();
			if (ulPackTotalLength && bufLen >= ulPackTotalLength)
			{
				ULONG ulCompressedLength = 0;
				ULONG ulOriginalLength = 0;
				PBYTE CompressedBuffer = ContextObject->ReadBuffer(ulCompressedLength, ulOriginalLength);
				if (ContextObject->CompressMethod == COMPRESS_UNKNOWN) {
					delete[] CompressedBuffer;
					throw "Unknown method";
				}
				else if (ContextObject->CompressMethod == COMPRESS_NONE) {
					ContextObject->InDeCompressedBuffer.ClearBuffer();
					ContextObject->Decode(CompressedBuffer, ulOriginalLength);
					ContextObject->InDeCompressedBuffer.WriteBuffer(CompressedBuffer, ulOriginalLength);
					m_NotifyProc(ContextObject);
					SAFE_DELETE_ARRAY(CompressedBuffer);
					continue;
				}
				bool usingZstd = ContextObject->CompressMethod == COMPRESS_ZSTD, zlibFailed = false;
				PBYTE DeCompressedBuffer = new BYTE[ulOriginalLength];  //��ѹ�����ڴ�
				size_t	iRet = usingZstd ?
					Muncompress(DeCompressedBuffer, &ulOriginalLength, CompressedBuffer, ulCompressedLength) :
					uncompress(DeCompressedBuffer, &ulOriginalLength, CompressedBuffer, ulCompressedLength);
				if (usingZstd ? C_SUCCESS(iRet) : (S_OK==iRet))
				{
					ContextObject->InDeCompressedBuffer.ClearBuffer();
					ContextObject->Decode(DeCompressedBuffer, ulOriginalLength);
					ContextObject->InDeCompressedBuffer.WriteBuffer(DeCompressedBuffer, ulOriginalLength);
					m_NotifyProc(ContextObject);  //֪ͨ����
				}else if (usingZstd){
					// ������zlib��ѹ��
					if (Z_OK == uncompress(DeCompressedBuffer, &ulOriginalLength, CompressedBuffer, ulCompressedLength)) {
						ContextObject->CompressMethod = COMPRESS_ZLIB;
						ContextObject->InDeCompressedBuffer.ClearBuffer();
						ContextObject->Decode(DeCompressedBuffer, ulOriginalLength);
						ContextObject->InDeCompressedBuffer.WriteBuffer(DeCompressedBuffer, ulOriginalLength);
						m_NotifyProc(ContextObject);
					} else {
						zlibFailed = true;
						ContextObject->CompressMethod = COMPRESS_UNKNOWN;
					}
				} else {
					zlibFailed = true;
				}
				delete [] CompressedBuffer;
				delete [] DeCompressedBuffer;
				if (zlibFailed) {
					Mprintf("[ERROR] ZLIB uncompress failed \n");
					throw "Bad Buffer";
				}
			}else{
				break;
			}
		}
	}catch(...)
	{
		Mprintf("[ERROR] OnClientReceiving catch an error \n");
		ContextObject->InCompressedBuffer.ClearBuffer();
		ContextObject->InDeCompressedBuffer.ClearBuffer();
	}
	PostRecv(ContextObject); //Ͷ���µĽ������ݵ�����

	return TRUE;
}

VOID IOCPServer::OnClientPreSending(CONTEXT_OBJECT* ContextObject, PBYTE szBuffer, size_t ulOriginalLength)
{
	assert (ContextObject);
	// �������������͵�����
	if (ulOriginalLength < 100 && szBuffer[0] != COMMAND_SCREEN_CONTROL && szBuffer[0] != CMD_HEARTBEAT_ACK) {
		char buf[100] = { 0 };
		if (ulOriginalLength == 1){
			sprintf_s(buf, "command %d", int(szBuffer[0]));
		}
		else {
			memcpy(buf, szBuffer, ulOriginalLength);
		}
		Mprintf("[COMMAND] Send: " + CString(buf) + "\r\n");
	}
	try
	{
		do
		{
			if (ulOriginalLength <= 0) return;
			if (ContextObject->CompressMethod == COMPRESS_UNKNOWN) {
				Mprintf("[ERROR] UNKNOWN compress method \n");
				return;
			}
			else if (ContextObject->CompressMethod == COMPRESS_NONE) {
				Buffer tmp(szBuffer, ulOriginalLength); szBuffer = tmp.Buf();
				ContextObject->WriteBuffer(szBuffer, ulOriginalLength, ulOriginalLength);
				break;
			}
			bool usingZstd = ContextObject->CompressMethod == COMPRESS_ZSTD;
#if USING_LZ4
			unsigned long	ulCompressedLength = LZ4_compressBound(ulOriginalLength);
#else
			unsigned long	ulCompressedLength = usingZstd ? 
				ZSTD_compressBound(ulOriginalLength) : (double)ulOriginalLength * 1.001 + 12;
#endif
			BYTE			buf[1024];
			LPBYTE			CompressedBuffer = ulCompressedLength>1024 ? new BYTE[ulCompressedLength]:buf;
			Buffer tmp(szBuffer, ulOriginalLength); szBuffer = tmp.Buf();
			ContextObject->Encode(szBuffer, ulOriginalLength);
			size_t	iRet = usingZstd ?
				Mcompress(CompressedBuffer, &ulCompressedLength, (LPBYTE)szBuffer, ulOriginalLength):
				compress(CompressedBuffer, &ulCompressedLength, (LPBYTE)szBuffer, ulOriginalLength);

			if (usingZstd ? C_FAILED(iRet) : (S_OK != iRet))
			{
				Mprintf("[ERROR] compress failed \n");
				if (CompressedBuffer != buf) delete [] CompressedBuffer;
				return;
			}

			ulCompressedLength =  usingZstd ? iRet : ulCompressedLength;

			ContextObject->WriteBuffer(CompressedBuffer, ulCompressedLength, ulOriginalLength);
			if (CompressedBuffer != buf) delete [] CompressedBuffer;
		}while (false);

		OVERLAPPEDPLUS* OverlappedPlus = new OVERLAPPEDPLUS(IOWrite);
		BOOL bOk = PostQueuedCompletionStatus(m_hCompletionPort, 0, (ULONG_PTR)ContextObject, &OverlappedPlus->m_ol);
		if ( (!bOk && GetLastError() != ERROR_IO_PENDING) )  //���Ͷ��ʧ��
		{
			int a = GetLastError();
			Mprintf("!!! OnClientPreSending Ͷ����Ϣʧ��\n");
			RemoveStaleContext(ContextObject);
			SAFE_DELETE(OverlappedPlus);
		}
	}catch(...){
		Mprintf("[ERROR] OnClientPreSending catch an error \n");
	}
}

BOOL IOCPServer::OnClientPostSending(CONTEXT_OBJECT* ContextObject,ULONG ulCompletedLength)   
{
	try
	{
		DWORD ulFlags = MSG_PARTIAL;

		ContextObject->OutCompressedBuffer.RemoveCompletedBuffer(ulCompletedLength); //����ɵ����ݴ����ݽṹ��ȥ��
		if (ContextObject->OutCompressedBuffer.GetBufferLength() == 0)
		{
			ContextObject->OutCompressedBuffer.ClearBuffer();
			return true;		                             //�ߵ�����˵�����ǵ�����������ȫ����
		}
		else
		{
			OVERLAPPEDPLUS * OverlappedPlus = new OVERLAPPEDPLUS(IOWrite); //����û�����  ���Ǽ���Ͷ�� ��������

			ContextObject->wsaOutBuffer.buf = (char*)ContextObject->OutCompressedBuffer.GetBuffer(0);
			ContextObject->wsaOutBuffer.len = ContextObject->OutCompressedBuffer.GetBufferLength(); 
			int iOk = WSASend(ContextObject->sClientSocket, &ContextObject->wsaOutBuffer,1,
				NULL, ulFlags,&OverlappedPlus->m_ol, NULL);
			if ( iOk == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING )
			{
				int a = GetLastError();
				Mprintf("!!! OnClientPostSending Ͷ����Ϣʧ��\n");
				RemoveStaleContext(ContextObject);
				SAFE_DELETE(OverlappedPlus);
			}
		}
	}catch(...){
		Mprintf("[ERROR] OnClientPostSending catch an error \n");
	}

	return FALSE;			
}

DWORD IOCPServer::ListenThreadProc(LPVOID lParam)   //�����߳�
{
	IOCPServer* This = (IOCPServer*)(lParam);
	WSANETWORKEVENTS NetWorkEvents;

	while(!This->m_bTimeToKill)
	{
		if (WaitForSingleObject(This->m_hKillEvent, 100) == WAIT_OBJECT_0)
			break;

		DWORD dwRet;
		dwRet = WSAWaitForMultipleEvents(1,&This->m_hListenEvent,FALSE,100,FALSE);  
		if (dwRet == WSA_WAIT_TIMEOUT)
			continue;

		int iRet = WSAEnumNetworkEvents(This->m_sListenSocket,    
			//����¼����� ���Ǿͽ����¼�ת����һ�������¼� ���� �ж�
			This->m_hListenEvent,
			&NetWorkEvents);

		if (iRet == SOCKET_ERROR)
			break;

		if (NetWorkEvents.lNetworkEvents & FD_ACCEPT)
		{
			if (NetWorkEvents.iErrorCode[FD_ACCEPT_BIT] == 0)
			{
				This->OnAccept(); 
			}else{
				break;
			}
		}
	}
	This->m_hListenThread = NULL;
	return 0;
}

void IOCPServer::OnAccept()
{
	SOCKADDR_IN	ClientAddr = {0};     
	SOCKET		sClientSocket = INVALID_SOCKET;

	int iLen = sizeof(SOCKADDR_IN);
	sClientSocket = accept(m_sListenSocket,
		(sockaddr*)&ClientAddr,
		&iLen);                     //ͨ�����ǵļ����׽���������һ����֮�ź�ͨ�ŵ��׽���
	if (sClientSocket == SOCKET_ERROR)
	{
		return;
	}

	//����������Ϊÿһ��������ź�ά����һ����֮���������ݽṹ������Ϊ�û������±�����
	PCONTEXT_OBJECT ContextObject = AllocateContext(sClientSocket);   // Context

	if (ContextObject == NULL)
	{
		closesocket(sClientSocket);
		sClientSocket = INVALID_SOCKET;
		return;
	}

	ContextObject->sClientSocket = sClientSocket;

	ContextObject->wsaInBuf.buf = (char*)ContextObject->szBuffer;
	ContextObject->wsaInBuf.len = sizeof(ContextObject->szBuffer);

	HANDLE Handle = CreateIoCompletionPort((HANDLE)sClientSocket, m_hCompletionPort, (ULONG_PTR)ContextObject, 0);

	if (Handle!=m_hCompletionPort)
	{
		delete ContextObject;
		ContextObject = NULL;

		if (sClientSocket!=INVALID_SOCKET)
		{
			closesocket(sClientSocket);
			sClientSocket = INVALID_SOCKET;
		}

		return;
	}

	//�����׽��ֵ�ѡ� Set KeepAlive ����������� SO_KEEPALIVE 
	//�������Ӽ��Է������Ƿ�������2Сʱ���ڴ��׽ӿڵ���һ����û
	//�����ݽ�����TCP���Զ����Է� ��һ�����ִ��
	m_ulKeepLiveTime = 3;
	const BOOL bKeepAlive = TRUE;
	setsockopt(ContextObject->sClientSocket,SOL_SOCKET,SO_KEEPALIVE,(char*)&bKeepAlive,sizeof(bKeepAlive));

	//���ó�ʱ��ϸ��Ϣ
	tcp_keepalive	KeepAlive;
	KeepAlive.onoff = 1; // ���ñ���
	KeepAlive.keepalivetime = m_ulKeepLiveTime;       //����3����û�����ݣ��ͷ���̽���
	KeepAlive.keepaliveinterval = 1000 * 10;         //���Լ��Ϊ10�� Resend if No-Reply
	WSAIoctl(ContextObject->sClientSocket, SIO_KEEPALIVE_VALS,&KeepAlive,sizeof(KeepAlive),
		NULL,0,(unsigned long *)&bKeepAlive,0,NULL);

	//����������ʱ����������ͻ������߻�ϵ�ȷ������Ͽ����������������û������SO_KEEPALIVEѡ�
	//���һֱ���ر�SOCKET����Ϊ�ϵĵ�������Ĭ������Сʱʱ��̫�����������Ǿ��������ֵ
	EnterCriticalSection(&m_cs);
	m_ContextConnectionList.AddTail(ContextObject);     //���뵽���ǵ��ڴ��б���
	LeaveCriticalSection(&m_cs);

	OVERLAPPEDPLUS	*OverlappedPlus = new OVERLAPPEDPLUS(IOInitialize); //ע��������ص�IO������ �û���������

	BOOL bOk = PostQueuedCompletionStatus(m_hCompletionPort, 0, (ULONG_PTR)ContextObject, &OverlappedPlus->m_ol); // �����߳�
	//��Ϊ���ǽ��ܵ���һ���û����ߵ�������ô���Ǿͽ��������͸����ǵ���ɶ˿� �����ǵĹ����̴߳�����
	if ( (!bOk && GetLastError() != ERROR_IO_PENDING))  //���Ͷ��ʧ��
	{
		int a = GetLastError();
		Mprintf("!!! OnAccept Ͷ����Ϣʧ��\n");
		RemoveStaleContext(ContextObject);
		SAFE_DELETE(OverlappedPlus);
		return;
	}

	PostRecv(ContextObject);       
}

VOID IOCPServer::PostRecv(CONTEXT_OBJECT* ContextObject)
{
	//�����ǵĸ����ߵ��û���Ͷ��һ���������ݵ�����
	// ����û��ĵ�һ�����ݰ�����Ҳ�;��Ǳ��ض˵ĵ�½���󵽴����ǵĹ����߳̾�
	// ����Ӧ,������ProcessIOMessage����
	OVERLAPPEDPLUS * OverlappedPlus = new OVERLAPPEDPLUS(IORead);
	ContextObject->olps = OverlappedPlus;

	DWORD			dwReturn;
	ULONG			ulFlags = MSG_PARTIAL;
	int iOk = WSARecv(ContextObject->sClientSocket, &ContextObject->wsaInBuf,
		1,&dwReturn, &ulFlags,&OverlappedPlus->m_ol, NULL);

	if (iOk == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING)
	{
		int a = GetLastError();
		Mprintf("!!! PostRecv Ͷ����Ϣʧ��\n");
		RemoveStaleContext(ContextObject);
		SAFE_DELETE(OverlappedPlus);
	}
}

PCONTEXT_OBJECT IOCPServer::AllocateContext(SOCKET s)
{
	PCONTEXT_OBJECT ContextObject = NULL;

	CLock cs(m_cs);       

	if (m_ContextConnectionList.GetCount() >= m_ulMaxConnections) {
		return NULL;
	}

	ContextObject = !m_ContextFreePoolList.IsEmpty() ? m_ContextFreePoolList.RemoveHead() : new CONTEXT_OBJECT;

	if (ContextObject != NULL)
	{
		ContextObject->InitMember(s);
	}

	return ContextObject;
}

VOID IOCPServer::RemoveStaleContext(CONTEXT_OBJECT* ContextObject)
{
	EnterCriticalSection(&m_cs);
	auto find = m_ContextConnectionList.Find(ContextObject);
	LeaveCriticalSection(&m_cs);
	if (find)    //���ڴ��в��Ҹ��û������������ݽṹ
	{
		m_OfflineProc(ContextObject);

		CancelIo((HANDLE)ContextObject->sClientSocket);  //ȡ���ڵ�ǰ�׽��ֵ��첽IO -->PostRecv
		closesocket(ContextObject->sClientSocket);      //�ر��׽���
		ContextObject->sClientSocket = INVALID_SOCKET;

		while (!HasOverlappedIoCompleted((LPOVERLAPPED)ContextObject))//�жϻ���û���첽IO�����ڵ�ǰ�׽�����
		{
			Sleep(0);
		}

		MoveContextToFreePoolList(ContextObject);  //�����ڴ�ṹ�������ڴ��
	}
}

VOID IOCPServer::MoveContextToFreePoolList(CONTEXT_OBJECT* ContextObject)
{
	CLock cs(m_cs);

	POSITION Pos = m_ContextConnectionList.Find(ContextObject);
	if (Pos) 
	{
		ContextObject->InCompressedBuffer.ClearBuffer();
		ContextObject->InDeCompressedBuffer.ClearBuffer();
		ContextObject->OutCompressedBuffer.ClearBuffer();

		memset(ContextObject->szBuffer,0,8192);
		m_ContextFreePoolList.AddTail(ContextObject); //�������ڴ��
		m_ContextConnectionList.RemoveAt(Pos); //���ڴ�ṹ���Ƴ�
	}
}

void IOCPServer::UpdateMaxConnection(int maxConn) {
	CLock cs(m_cs);
	m_ulMaxConnections = maxConn;
}
