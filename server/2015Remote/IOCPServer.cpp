#include "StdAfx.h"
#include "IOCPServer.h"
#include "2015Remote.h"

#include <iostream>
#if USING_ZLIB
#include "zlib.h"
#define Z_FAILED(p) (Z_OK != (p))
#define Z_SUCCESS(p) (!Z_FAILED(p))
#else
#if USING_LZ4
#include "lz4/lz4.h"
#pragma comment(lib, "lz4/lz4.lib")
#define Z_FAILED(p) (0 == (p))
#define Z_SUCCESS(p) (!Z_FAILED(p))
#define compress(dest, destLen, source, sourceLen) LZ4_compress_default((const char*)source, (char*)dest, sourceLen, *(destLen))
#define uncompress(dest, destLen, source, sourceLen) LZ4_decompress_safe((const char*)source, (char*)dest, sourceLen, *(destLen))
#else
#include "zstd/zstd.h"
#pragma comment(lib, "zstd/zstd.lib")
#define Z_FAILED(p) ZSTD_isError(p)
#define Z_SUCCESS(p) (!Z_FAILED(p))
#define compress(dest, destLen, source, sourceLen) ZSTD_compress(dest, *(destLen), source, sourceLen, ZSTD_CLEVEL_DEFAULT)
#define uncompress(dest, destLen, source, sourceLen) ZSTD_decompress(dest, *(destLen), source, sourceLen)
#endif
#endif
using namespace std;

CRITICAL_SECTION IOCPServer::m_cs = {0};

#define HUERISTIC_VALUE 2

#define SAFE_DELETE(p) if(p){ delete (p); (p) = NULL; }

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
	m_hListenThread       = INVALID_HANDLE_VALUE;

	m_ulMaxConnections = ((CMy2015RemoteApp*)AfxGetApp())->m_iniFile.GetInt("settings", "MaxConnection");

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

	memset(m_szPacketFlag, 0, sizeof(m_szPacketFlag));
	memcpy(m_szPacketFlag,"Shine",FLAG_LENGTH);

	m_NotifyProc = NULL;
	m_OfflineProc = NULL;
}


IOCPServer::~IOCPServer(void)
{
	m_bTimeToKill = TRUE;

	Sleep(10);
	SetEvent(m_hKillEvent);

	Sleep(10);

	if (m_hKillEvent!=NULL)
	{
		CloseHandle(m_hKillEvent);
	}

	if (m_sListenSocket!=INVALID_SOCKET)
	{
		closesocket(m_sListenSocket);
		m_sListenSocket = INVALID_SOCKET;
	}

	if (m_hCompletionPort!=INVALID_HANDLE_VALUE)
	{
		CloseHandle(m_hCompletionPort);
		m_hCompletionPort = INVALID_HANDLE_VALUE;
	}

	if (m_hListenEvent!=WSA_INVALID_EVENT)
	{
		CloseHandle(m_hListenEvent);
		m_hListenEvent = WSA_INVALID_EVENT;
	}

	while (!m_ContextConnectionList.IsEmpty())
	{
		CONTEXT_OBJECT *ContextObject = m_ContextConnectionList.GetHead();
		RemoveStaleContext(ContextObject);
	}

	while (!m_ContextFreePoolList.IsEmpty())
	{
		CONTEXT_OBJECT *ContextObject = m_ContextFreePoolList.RemoveHead();
		// 下述语句有崩溃概率，2019.1.14
		//SAFE_DELETE(ContextObject->olps);
		delete ContextObject;
	}

	while (m_ulWorkThreadCount)
		Sleep(10);

	DeleteCriticalSection(&m_cs);
	m_ulWorkThreadCount = 0;

	m_ulThreadPoolMin  = 0; 
	m_ulThreadPoolMax  = 0;
	m_ulCPULowThreadsHold  = 0; 
	m_ulCPUHighThreadsHold = 0;
	m_ulCurrentThread = 0;
	m_ulBusyThread = 0;
	m_ulKeepLiveTime = 0;

	WSACleanup();
}

BOOL IOCPServer::StartServer(pfnNotifyProc NotifyProc, pfnOfflineProc OffProc, USHORT uPort)
{
	m_NotifyProc = NotifyProc;
	m_OfflineProc = OffProc;
	m_hKillEvent = CreateEvent(NULL,FALSE,FALSE,NULL);

	if (m_hKillEvent==NULL)
	{
		return FALSE;
	}

	m_sListenSocket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);     //创建监听套接字

	if (m_sListenSocket == INVALID_SOCKET)
	{
		return FALSE;
	}

	m_hListenEvent = WSACreateEvent();           

	if (m_hListenEvent == WSA_INVALID_EVENT)
	{
		closesocket(m_sListenSocket);

		m_sListenSocket = INVALID_SOCKET;
		return FALSE;
	}

	int iRet = WSAEventSelect(m_sListenSocket,	//将监听套接字与事件进行关联并授予FD_ACCEPT的属性
		m_hListenEvent,
		FD_ACCEPT);

	if (iRet == SOCKET_ERROR)
	{
		closesocket(m_sListenSocket);

		m_sListenSocket = INVALID_SOCKET;
		WSACloseEvent(m_hListenEvent);

		m_hListenEvent = WSA_INVALID_EVENT;

		return FALSE;
	}

	SOCKADDR_IN	ServerAddr;		
	ServerAddr.sin_port   = htons(uPort);
	ServerAddr.sin_family = AF_INET;
	ServerAddr.sin_addr.s_addr = INADDR_ANY; //初始化本地网卡

	//将监听套机字和网卡进行bind
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

		return FALSE;
	}

	iRet = listen(m_sListenSocket, SOMAXCONN);

	if (iRet == SOCKET_ERROR)
	{
		closesocket(m_sListenSocket);

		m_sListenSocket = INVALID_SOCKET;
		WSACloseEvent(m_hListenEvent);

		m_hListenEvent = WSA_INVALID_EVENT;

		return FALSE;
	}

	m_hListenThread =
		(HANDLE)CreateThread(NULL,			
		0,					
		(LPTHREAD_START_ROUTINE)ListenThreadProc, 
		(void*)this,	      //向Thread回调函数传入this 方便我们的线程回调访问类中的成员    
		0,					
		NULL);	
	if (m_hListenThread==INVALID_HANDLE_VALUE)
	{
		closesocket(m_sListenSocket);

		m_sListenSocket = INVALID_SOCKET;
		WSACloseEvent(m_hListenEvent);

		m_hListenEvent = WSA_INVALID_EVENT;
		return FALSE;
	}

	//启动工作线程  1  2
	InitializeIOCP();
	return TRUE;
}


//1创建完成端口
//2创建工作线程
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
	GetSystemInfo(&SystemInfo);  //获得PC中有几核

	m_ulThreadPoolMin  = 1; 
	m_ulThreadPoolMax  = SystemInfo.dwNumberOfProcessors * 2;
	m_ulCPULowThreadsHold  = 10; 
	m_ulCPUHighThreadsHold = 75; 
	m_cpu.Init();

	ULONG ulWorkThreadCount = m_ulThreadPoolMax;

	HANDLE hWorkThread = NULL;
	for (int i=0; i<ulWorkThreadCount; ++i)    
	{
		hWorkThread = (HANDLE)CreateThread(NULL, //创建工作线程目的是处理投递到完成端口中的任务			
			0,						
			(LPTHREAD_START_ROUTINE)WorkThreadProc,     		
			(void*)this,			
			0,						
			NULL);			
		if (hWorkThread == NULL )    		
		{
			CloseHandle(m_hCompletionPort);
			return FALSE;
		}

		m_ulWorkThreadCount++;

		CloseHandle(hWorkThread);
	}

	return TRUE;
}


DWORD IOCPServer::WorkThreadProc(LPVOID lParam)
{
	OutputDebugStringA("======> IOCPServer WorkThreadProc begin \n");

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
		AUTO_TICK(40);
		InterlockedDecrement(&This->m_ulBusyThread);
		// GetQueuedCompletionStatus耗时比较长，导致客户端发送数据的速率提高不了
		BOOL bOk = GetQueuedCompletionStatus(
			hCompletionPort,
			&dwTrans,
			(LPDWORD)&ContextObject,
			&Overlapped, INFINITE);
		STOP_TICK;
		DWORD dwIOError = GetLastError();
		OverlappedPlus = CONTAINING_RECORD(Overlapped, OVERLAPPEDPLUS, m_ol);
		ulBusyThread = InterlockedIncrement(&This->m_ulBusyThread); //1 1
		if ( !bOk && dwIOError != WAIT_TIMEOUT )   //当对方的套机制发生了关闭                    
		{
			if (ContextObject && This->m_bTimeToKill == FALSE &&dwTrans==0)
			{
				ContextObject->olps = NULL;
				OutputDebugStringA("!!! RemoveStaleContext \n");
				This->RemoveStaleContext(ContextObject);
			}
			SAFE_DELETE(OverlappedPlus);
			continue;
		}
		if (!bError) 
		{
			//分配一个新的线程到线程到线程池
			if (ulBusyThread == This->m_ulCurrentThread)      
			{
				if (ulBusyThread < This->m_ulThreadPoolMax)    
				{
					if (ContextObject != NULL)
					{
						HANDLE hThread = (HANDLE)CreateThread(NULL,				
							0,				
							(LPTHREAD_START_ROUTINE)WorkThreadProc,  
							(void*)This,	    
							0,					
							NULL);

						InterlockedIncrement(&This->m_ulWorkThreadCount);

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

		if (!bError)  
		{
			if(bOk && OverlappedPlus!=NULL && ContextObject!=NULL) 
			{
				try
				{   
					This->HandleIO(OverlappedPlus->m_ioType, ContextObject, dwTrans);  

					ContextObject = NULL;
				}
				catch (...) {}
			}
		}

		SAFE_DELETE(OverlappedPlus);
	}
	timeEndPeriod(1);
	SAFE_DELETE(OverlappedPlus);

	InterlockedDecrement(&This->m_ulWorkThreadCount);
	InterlockedDecrement(&This->m_ulCurrentThread);
	InterlockedDecrement(&This->m_ulBusyThread);

	OutputDebugStringA("======> IOCPServer WorkThreadProc end \n");

	return 0;
}

//在工作线程中被调用
BOOL IOCPServer::HandleIO(IOType PacketFlags,PCONTEXT_OBJECT ContextObject, DWORD dwTrans)
{
	AUTO_TICK(20);

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
		OutputDebugStringA("=> HandleIO PacketFlags= IOIdle\n");
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
	CLock cs(m_cs);
	try
	{
		if (dwTrans == 0)    //对方关闭了套接字
		{
			RemoveStaleContext(ContextObject);
			return FALSE;
		}
		//将接收到的数据拷贝到我们自己的内存中wsabuff    8192
		ContextObject->InCompressedBuffer.WriteBuffer((PBYTE)ContextObject->szBuffer,dwTrans);
		//查看数据包里的数据
		while (ContextObject->InCompressedBuffer.GetBufferLength() > HDR_LENGTH)
		{
			char szPacketFlag[FLAG_LENGTH + 3]= {0}; // 8字节对齐
			CopyMemory(szPacketFlag, ContextObject->InCompressedBuffer.GetBuffer(),FLAG_LENGTH);
			if (memcmp(m_szPacketFlag, szPacketFlag, FLAG_LENGTH) != 0)
				throw "Bad Buffer";
			
			//Shine[50][kdjfkdjfkj]
			ULONG ulPackTotalLength = 0;
			CopyMemory(&ulPackTotalLength, ContextObject->InCompressedBuffer.GetBuffer(FLAG_LENGTH), sizeof(ULONG));
			//取出数据包的总长
			//50
			if (ulPackTotalLength && (ContextObject->InCompressedBuffer.GetBufferLength()) >= ulPackTotalLength)  
			{
				ULONG ulOriginalLength = 0;
				ContextObject->InCompressedBuffer.ReadBuffer((PBYTE)szPacketFlag, FLAG_LENGTH);
				ContextObject->InCompressedBuffer.ReadBuffer((PBYTE) &ulPackTotalLength, sizeof(ULONG));
				ContextObject->InCompressedBuffer.ReadBuffer((PBYTE) &ulOriginalLength, sizeof(ULONG));
				ULONG ulCompressedLength = ulPackTotalLength - HDR_LENGTH; //461 - 13  448
				PBYTE CompressedBuffer = new BYTE[ulCompressedLength];  //没有解压
				//从数据包当前将源数据没有解压读取到pData   448
				ContextObject->InCompressedBuffer.ReadBuffer(CompressedBuffer, ulCompressedLength);
#if USING_COMPRESS
				PBYTE DeCompressedBuffer = new BYTE[ulOriginalLength];  //解压过的内存  436
				int	iRet = uncompress(DeCompressedBuffer, &ulOriginalLength, CompressedBuffer, ulCompressedLength);
#else
				PBYTE DeCompressedBuffer = CompressedBuffer;
				int iRet = 0;
#endif
				if (Z_SUCCESS(iRet))
				{
					ContextObject->InDeCompressedBuffer.ClearBuffer();
					ContextObject->InCompressedBuffer.ClearBuffer();
					ContextObject->InDeCompressedBuffer.WriteBuffer(DeCompressedBuffer, ulOriginalLength);
					m_NotifyProc(ContextObject);  //通知窗口
				}else{
					OutputDebugStringA("[ERROR] uncompress failed \n");
					delete [] CompressedBuffer;
#if USING_COMPRESS // 释放内存
					delete [] DeCompressedBuffer;
#endif
					throw "Bad Buffer";
				}
				delete [] CompressedBuffer;
#if USING_COMPRESS // 释放内存
				delete [] DeCompressedBuffer;
#endif
			}else{
				break;
			}
		}
		PostRecv(ContextObject); //投递新的接收数据的请求
	}catch(...)
	{
		ContextObject->InCompressedBuffer.ClearBuffer();
		ContextObject->InDeCompressedBuffer.ClearBuffer();

		PostRecv(ContextObject);
	}
	return TRUE;
}

VOID IOCPServer::OnClientPreSending(CONTEXT_OBJECT* ContextObject, PBYTE szBuffer, ULONG ulOriginalLength)  
{
	assert (ContextObject);

	try
	{
		if (ulOriginalLength > 0)
		{
#if USING_ZLIB
			unsigned long	ulCompressedLength = (double)ulOriginalLength * 1.001  + 12;
#elif USING_LZ4
			unsigned long	ulCompressedLength = LZ4_compressBound(ulOriginalLength);
#else
			unsigned long	ulCompressedLength = ZSTD_compressBound(ulOriginalLength);
#endif
			LPBYTE			CompressedBuffer = new BYTE[ulCompressedLength];
			int	iRet = compress(CompressedBuffer, &ulCompressedLength, (LPBYTE)szBuffer, ulOriginalLength);

			if (Z_FAILED(iRet))
			{
				OutputDebugStringA("[ERROR] compress failed \n");
				delete [] CompressedBuffer;
				return;
			}
#if !USING_ZLIB
			ulCompressedLength = iRet;
#endif
			ULONG ulPackTotalLength = ulCompressedLength + HDR_LENGTH;
			ContextObject->OutCompressedBuffer.WriteBuffer((LPBYTE)m_szPacketFlag,FLAG_LENGTH);
			ContextObject->OutCompressedBuffer.WriteBuffer((PBYTE)&ulPackTotalLength, sizeof(ULONG));
			ContextObject->OutCompressedBuffer.WriteBuffer((PBYTE)&ulOriginalLength, sizeof(ULONG));
			ContextObject->OutCompressedBuffer.WriteBuffer(CompressedBuffer, ulCompressedLength);
			delete [] CompressedBuffer;
		}

		OVERLAPPEDPLUS* OverlappedPlus = new OVERLAPPEDPLUS(IOWrite);
		BOOL bOk = PostQueuedCompletionStatus(m_hCompletionPort, 0, (DWORD)ContextObject, &OverlappedPlus->m_ol);
		if ( (!bOk && GetLastError() != ERROR_IO_PENDING) )  //如果投递失败
		{
			int a = GetLastError();
			OutputDebugStringA("!!! OnClientPreSending 投递消息失败\n");
			RemoveStaleContext(ContextObject);
			SAFE_DELETE(OverlappedPlus);
		}
	}catch(...){}
}

BOOL IOCPServer::OnClientPostSending(CONTEXT_OBJECT* ContextObject,ULONG ulCompletedLength)   
{
	try
	{
		DWORD ulFlags = MSG_PARTIAL;

		ContextObject->OutCompressedBuffer.RemoveComletedBuffer(ulCompletedLength); //将完成的数据从数据结构中去除
		if (ContextObject->OutCompressedBuffer.GetBufferLength() == 0)
		{
			ContextObject->OutCompressedBuffer.ClearBuffer();
			return true;		                             //走到这里说明我们的数据真正完全发送
		}
		else
		{
			OVERLAPPEDPLUS * OverlappedPlus = new OVERLAPPEDPLUS(IOWrite); //数据没有完成  我们继续投递 发送请求

			ContextObject->wsaOutBuffer.buf = (char*)ContextObject->OutCompressedBuffer.GetBuffer();
			ContextObject->wsaOutBuffer.len = ContextObject->OutCompressedBuffer.GetBufferLength(); 
			int iOk = WSASend(ContextObject->sClientSocket, &ContextObject->wsaOutBuffer,1,
				&ContextObject->wsaOutBuffer.len, ulFlags,&OverlappedPlus->m_ol, NULL);
			if ( iOk == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING )
			{
				int a = GetLastError();
				OutputDebugStringA("!!! OnClientPostSending 投递消息失败\n");
				RemoveStaleContext(ContextObject);
				SAFE_DELETE(OverlappedPlus);
			}
		}
	}catch(...){}

	return FALSE;			
}

DWORD IOCPServer::ListenThreadProc(LPVOID lParam)   //监听线程
{
	IOCPServer* This = (IOCPServer*)(lParam);
	WSANETWORKEVENTS NetWorkEvents;

	while(1)
	{
		if (WaitForSingleObject(This->m_hKillEvent, 100) == WAIT_OBJECT_0)
			break;

		DWORD dwRet;
		dwRet = WSAWaitForMultipleEvents(1,&This->m_hListenEvent,FALSE,100,FALSE);  
		if (dwRet == WSA_WAIT_TIMEOUT)
			continue;

		int iRet = WSAEnumNetworkEvents(This->m_sListenSocket,    
			//如果事件授信 我们就将该事件转换成一个网络事件 进行 判断
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

	return 0;
}

void IOCPServer::OnAccept()
{
	SOCKADDR_IN	ClientAddr = {0};     
	SOCKET		sClientSocket = INVALID_SOCKET;

	int iLen = sizeof(SOCKADDR_IN);
	sClientSocket = accept(m_sListenSocket,
		(sockaddr*)&ClientAddr,
		&iLen);                     //通过我们的监听套接字来生成一个与之信号通信的套接字
	if (sClientSocket == SOCKET_ERROR)
	{
		return;
	}

	//我们在这里为每一个到达的信号维护了一个与之关联的数据结构这里简称为用户的上下背景文
	PCONTEXT_OBJECT ContextObject = AllocateContext();   // Context

	if (ContextObject == NULL)
	{
		closesocket(sClientSocket);
		sClientSocket = INVALID_SOCKET;
		return;
	}

	ContextObject->sClientSocket = sClientSocket;

	ContextObject->wsaInBuf.buf = (char*)ContextObject->szBuffer;
	ContextObject->wsaInBuf.len = sizeof(ContextObject->szBuffer);

	HANDLE Handle = CreateIoCompletionPort((HANDLE)sClientSocket, m_hCompletionPort, (DWORD)ContextObject, 0);

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

	//设置套接字的选项卡 Set KeepAlive 开启保活机制 SO_KEEPALIVE 
	//保持连接检测对方主机是否崩溃如果2小时内在此套接口的任一方向都没
	//有数据交换，TCP就自动给对方 发一个保持存活
	m_ulKeepLiveTime = 3;
	const BOOL bKeepAlive = TRUE;
	setsockopt(ContextObject->sClientSocket,SOL_SOCKET,SO_KEEPALIVE,(char*)&bKeepAlive,sizeof(bKeepAlive));

	//设置超时详细信息
	tcp_keepalive	KeepAlive;
	KeepAlive.onoff = 1; // 启用保活
	KeepAlive.keepalivetime = m_ulKeepLiveTime;       //超过3分钟没有数据，就发送探测包
	KeepAlive.keepaliveinterval = 1000 * 10;         //重试间隔为10秒 Resend if No-Reply
	WSAIoctl(ContextObject->sClientSocket, SIO_KEEPALIVE_VALS,&KeepAlive,sizeof(KeepAlive),
		NULL,0,(unsigned long *)&bKeepAlive,0,NULL);

	//在做服务器时，如果发生客户端网线或断电等非正常断开的现象，如果服务器没有设置SO_KEEPALIVE选项，
	//则会一直不关闭SOCKET。因为上的的设置是默认两个小时时间太长了所以我们就修正这个值
	CLock cs(m_cs);
	m_ContextConnectionList.AddTail(ContextObject);     //插入到我们的内存列表中

	OVERLAPPEDPLUS	*OverlappedPlus = new OVERLAPPEDPLUS(IOInitialize); //注意这里的重叠IO请求是 用户请求上线

	BOOL bOk = PostQueuedCompletionStatus(m_hCompletionPort, 0, (DWORD)ContextObject, &OverlappedPlus->m_ol); // 工作线程
	//因为我们接受到了一个用户上线的请求那么我们就将该请求发送给我们的完成端口 让我们的工作线程处理它
	if ( (!bOk && GetLastError() != ERROR_IO_PENDING))  //如果投递失败
	{
		int a = GetLastError();
		OutputDebugStringA("!!! OnAccept 投递消息失败\n");
		RemoveStaleContext(ContextObject);
		SAFE_DELETE(OverlappedPlus);
		return;
	}

	PostRecv(ContextObject);       
}

VOID IOCPServer::PostRecv(CONTEXT_OBJECT* ContextObject)
{
	//向我们的刚上线的用户的投递一个接受数据的请求
	// 如果用户的第一个数据包到达也就就是被控端的登陆请求到达我们的工作线程就
	// 会响应,并调用ProcessIOMessage函数
	OVERLAPPEDPLUS * OverlappedPlus = new OVERLAPPEDPLUS(IORead);
	ContextObject->olps = OverlappedPlus;

	DWORD			dwReturn;
	ULONG			ulFlags = MSG_PARTIAL;
	int iOk = WSARecv(ContextObject->sClientSocket, &ContextObject->wsaInBuf,
		1,&dwReturn, &ulFlags,&OverlappedPlus->m_ol, NULL);

	if (iOk == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING)
	{
		int a = GetLastError();
		OutputDebugStringA("!!! PostRecv 投递消息失败\n");
		RemoveStaleContext(ContextObject);
		SAFE_DELETE(OverlappedPlus);
	}
}

PCONTEXT_OBJECT IOCPServer::AllocateContext()
{
	PCONTEXT_OBJECT ContextObject = NULL;

	CLock cs(m_cs);       

	ContextObject = !m_ContextFreePoolList.IsEmpty() ? m_ContextFreePoolList.RemoveHead() : new CONTEXT_OBJECT;

	if (ContextObject != NULL)
	{
		ContextObject->InitMember();
	}

	return ContextObject;
}

VOID IOCPServer::RemoveStaleContext(CONTEXT_OBJECT* ContextObject)
{
	CLock cs(m_cs);
	if (m_ContextConnectionList.Find(ContextObject))    //在内存中查找该用户的上下文数据结构
	{
		m_OfflineProc(ContextObject);

		CancelIo((HANDLE)ContextObject->sClientSocket);  //取消在当前套接字的异步IO -->PostRecv
		closesocket(ContextObject->sClientSocket);      //关闭套接字
		ContextObject->sClientSocket = INVALID_SOCKET;

		while (!HasOverlappedIoCompleted((LPOVERLAPPED)ContextObject))//判断还有没有异步IO请求在当前套接字上
		{
			Sleep(0);
		}

		MoveContextToFreePoolList(ContextObject);  //将该内存结构回收至内存池
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
		m_ContextFreePoolList.AddTail(ContextObject); //回收至内存池
		m_ContextConnectionList.RemoveAt(Pos); //从内存结构中移除
	}
}
