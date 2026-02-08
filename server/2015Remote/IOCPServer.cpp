#include "StdAfx.h"
#include "IOCPServer.h"
#include "2015Remote.h"

#include <iostream>
#include <ws2tcpip.h>

// 根据 socket 获取客户端IP地址.
std::string GetPeerName(SOCKET sock)
{
    sockaddr_in  ClientAddr = {};
    int ulClientAddrLen = sizeof(sockaddr_in);
    int s = getpeername(sock, (SOCKADDR*)&ClientAddr, &ulClientAddrLen);
    return s != INVALID_SOCKET ? inet_ntoa(ClientAddr.sin_addr) : "";
}

// 根据 socket 获取客户端IP地址.
std::string GetRemoteIP(SOCKET sock)
{
    sockaddr_in addr;
    int addrLen = sizeof(addr);

    if (getpeername(sock, (sockaddr*)&addr, &addrLen) == 0) {
        char ipStr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &addr.sin_addr, ipStr, sizeof(ipStr));
        TRACE(">>> 对端 IP 地址: %s\n", ipStr);
        return ipStr;
    }
    TRACE(">>> 获取对端 IP 失败, 错误码: %d\n", WSAGetLastError());
    char buf[10];
    sprintf_s(buf, "%d", sock);
    return buf;
}

IOCPServer::IOCPServer(HWND hWnd)
{
    m_hMainWnd = hWnd;
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2,2), &wsaData)!=0) {
        return;
    }

    m_hCompletionPort = NULL;
    m_sListenSocket   = INVALID_SOCKET;
    m_hListenEvent	      = WSA_INVALID_EVENT;
    m_hListenThread       = NULL;

    m_ulMaxConnections = 10000;

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
}

void IOCPServer::Destroy()
{
    m_bTimeToKill = TRUE;

    if (m_hKillEvent != NULL) {
        SetEvent(m_hKillEvent);
        SAFE_CLOSE_HANDLE(m_hKillEvent);
        m_hKillEvent = NULL;
    }

    if (m_sListenSocket != INVALID_SOCKET) {
        closesocket(m_sListenSocket);
        m_sListenSocket = INVALID_SOCKET;
    }

    if (m_hCompletionPort != INVALID_HANDLE_VALUE) {
        SAFE_CLOSE_HANDLE(m_hCompletionPort);
        m_hCompletionPort = INVALID_HANDLE_VALUE;
    }

    if (m_hListenEvent != WSA_INVALID_EVENT) {
        SAFE_CLOSE_HANDLE(m_hListenEvent);
        m_hListenEvent = WSA_INVALID_EVENT;
    }
}


IOCPServer::~IOCPServer(void)
{
    Destroy();
    while (m_ulWorkThreadCount || m_hListenThread)
        Sleep(10);

    while (!m_ContextConnectionList.IsEmpty()) {
        CONTEXT_OBJECT *ContextObject = m_ContextConnectionList.GetHead();
        RemoveStaleContext(ContextObject);
        SAFE_DELETE(ContextObject->olps);
    }

    while (!m_ContextFreePoolList.IsEmpty()) {
        CONTEXT_OBJECT *ContextObject = m_ContextFreePoolList.RemoveHead();
        // 下述语句有崩溃概率，2019.1.14
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

    WSACleanup();
}

// 返回错误码0代表成功，否则代表错误信息.
UINT IOCPServer::StartServer(pfnNotifyProc NotifyProc, pfnOfflineProc OffProc, USHORT uPort)
{
    m_nPort = uPort;
    m_NotifyProc = NotifyProc;
    m_OfflineProc = OffProc;
    m_hKillEvent = CreateEvent(NULL,FALSE,FALSE,NULL);

    if (m_hKillEvent==NULL) {
        return 1;
    }

    m_sListenSocket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);     //创建监听套接字

    if (m_sListenSocket == INVALID_SOCKET) {
        return 2;
    }

    m_hListenEvent = WSACreateEvent();

    if (m_hListenEvent == WSA_INVALID_EVENT) {
        closesocket(m_sListenSocket);

        m_sListenSocket = INVALID_SOCKET;
        return 3;
    }

    int iRet = WSAEventSelect(m_sListenSocket,	//将监听套接字与事件进行关联并授予FD_ACCEPT的属性
                              m_hListenEvent,
                              FD_ACCEPT);

    if (iRet == SOCKET_ERROR) {
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
    ServerAddr.sin_addr.s_addr = INADDR_ANY; //初始化本地网卡

    //将监听套机字和网卡进行bind
    iRet = bind(m_sListenSocket,
                (sockaddr*)&ServerAddr,
                sizeof(ServerAddr));

    if (iRet == SOCKET_ERROR) {
        int a  = GetLastError();
        closesocket(m_sListenSocket);

        m_sListenSocket = INVALID_SOCKET;
        WSACloseEvent(m_hListenEvent);

        m_hListenEvent = WSA_INVALID_EVENT;

        return a;
    }

    iRet = listen(m_sListenSocket, SOMAXCONN);

    if (iRet == SOCKET_ERROR) {
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
                             (void*)this,	      //向Thread回调函数传入this 方便我们的线程回调访问类中的成员
                             0,
                             NULL);
    if (m_hListenThread==NULL) {
        int a = GetLastError();
        closesocket(m_sListenSocket);

        m_sListenSocket = INVALID_SOCKET;
        WSACloseEvent(m_hListenEvent);

        m_hListenEvent = WSA_INVALID_EVENT;
        return a;
    }

    //启动工作线程  1  2
    InitializeIOCP();
    return 0;
}


//1创建完成端口
//2创建工作线程
BOOL IOCPServer::InitializeIOCP(VOID)
{
    m_hCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0 );
    if ( m_hCompletionPort == NULL ) {
        return FALSE;
    }

    if (m_hCompletionPort==INVALID_HANDLE_VALUE) {
        return FALSE;
    }

    SYSTEM_INFO SystemInfo;
    GetSystemInfo(&SystemInfo);  //获得PC中有几核

    m_ulThreadPoolMin  = 1;
    m_ulThreadPoolMax  = SystemInfo.dwNumberOfProcessors * 2;
    m_ulCPULowThreadsHold  = 10;
    m_ulCPUHighThreadsHold = 75;

    ULONG ulWorkThreadCount = m_ulThreadPoolMax;

    HANDLE hWorkThread = NULL;
    for (int i=0; i<ulWorkThreadCount; ++i) {
        hWorkThread = (HANDLE)CreateThread(NULL, //创建工作线程目的是处理投递到完成端口中的任务
                                           0,
                                           WorkThreadProc,
                                           (void*)this,
                                           0,
                                           NULL);
        if (hWorkThread == NULL ) {
            SAFE_CLOSE_HANDLE(m_hCompletionPort);
            return FALSE;
        }

        AddWorkThread(1);

        SAFE_CLOSE_HANDLE(hWorkThread);
    }

    return TRUE;
}


DWORD IOCPServer::WorkThreadProc(LPVOID lParam)
{
    // 压缩库配置
    ZSTD_DCtx* m_Dctx = ZSTD_createDCtx(); // 解压上下文
    z_stream m_stream = {};
    inflateInit2(&m_stream, 15);
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
    while (This->m_bTimeToKill==FALSE) {
        InterlockedDecrement(&This->m_ulBusyThread);
        // GetQueuedCompletionStatus耗时比较长，导致客户端发送数据的速率提高不了
        BOOL bOk = GetQueuedCompletionStatus(
                       hCompletionPort,
                       &dwTrans,
                       (PULONG_PTR)&ContextObject,
                       &Overlapped, INFINITE);
        DWORD dwIOError = GetLastError();
        OverlappedPlus = CONTAINING_RECORD(Overlapped, OVERLAPPEDPLUS, m_ol);
        ulBusyThread = InterlockedIncrement(&This->m_ulBusyThread); //1 1
        if ( !bOk && dwIOError != WAIT_TIMEOUT ) { //当对方的套机制发生了关闭
            if (ContextObject && This->m_bTimeToKill == FALSE &&dwTrans==0) {
                ContextObject->olps = NULL;
                Mprintf("!!! RemoveStaleContext: %d \n", WSAGetLastError());
                This->RemoveStaleContext(ContextObject);
            }
            SAFE_DELETE(OverlappedPlus);
            continue;
        }
        if (!bError) {
            //分配一个新的线程到线程到线程池
            if (ulBusyThread == This->m_ulCurrentThread) {
                if (ulBusyThread < This->m_ulThreadPoolMax) {
                    if (ContextObject != NULL) {
                        HANDLE hThread = (HANDLE)CreateThread(NULL,
                                                              0,
                                                              WorkThreadProc,
                                                              (void*)This,
                                                              0,
                                                              NULL);

                        This->AddWorkThread(hThread ? 1:0);

                        SAFE_CLOSE_HANDLE(hThread);
                    }
                }
            }

            if (!bOk && dwIOError == WAIT_TIMEOUT) {
                if (ContextObject == NULL) {
                    if (This->m_ulCurrentThread > This->m_ulThreadPoolMin) {
                        break;
                    }

                    bError = TRUE;
                }
            }
        }

        if (!bError && !This->m_bTimeToKill) {
            if(bOk && OverlappedPlus!=NULL && ContextObject!=NULL) {
                try {
                    This->HandleIO(OverlappedPlus->m_ioType, ContextObject, dwTrans, m_Dctx, &m_stream);

                    ContextObject = NULL;
                } catch (...) {
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

    inflateEnd(&m_stream);
    ZSTD_freeDCtx(m_Dctx);

    return 0;
}

//在工作线程中被调用
BOOL IOCPServer::HandleIO(IOType PacketFlags,PCONTEXT_OBJECT ContextObject, DWORD dwTrans, ZSTD_DCtx* ctx, z_stream* z)
{
    BOOL bRet = FALSE;

    switch (PacketFlags) {
    case IOInitialize:
        bRet = OnClientInitializing(ContextObject, dwTrans);
        break;
    case IORead:
        bRet = OnClientReceiving(ContextObject, dwTrans, ctx, z);
        break;
    case IOWrite:
        bRet = OnClientPostSending(ContextObject, dwTrans);
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

// May be this function should be a member of `CONTEXT_OBJECT`.
BOOL ParseReceivedData(CONTEXT_OBJECT * ContextObject, DWORD dwTrans, pfnNotifyProc m_NotifyProc, ZSTD_DCtx* m_Dctx, z_stream* z)
{
    AUTO_TICK(50, ContextObject->GetPeerName());
    BOOL ret = 1;
    try {
        if (dwTrans == 0) {  //对方关闭了套接字
            return FALSE;
        }
        //将接收到的数据拷贝到我们自己的内存中wsabuff    8192
        ContextObject->InCompressedBuffer.WriteBuffer((PBYTE)ContextObject->szBuffer,dwTrans);
        //查看数据包的完整性
        while (true) {
            PR pr = ContextObject->Parse(ContextObject->InCompressedBuffer);
            if (pr.IsFailed()) {
                ContextObject->InCompressedBuffer.ClearBuffer();
                break;
            } else if (pr.IsNeedMore()) {
                break;
            } else if (pr.IsWinOSLogin()) {
                ContextObject->InDeCompressedBuffer.ClearBuffer();
                ULONG ulCompressedLength = 0;
                ULONG ulOriginalLength = 0;
                PBYTE CompressedBuffer = ContextObject->ReadBuffer(ulCompressedLength, ulOriginalLength);
                ContextObject->InDeCompressedBuffer.WriteBuffer(CompressedBuffer, ulCompressedLength);
                if (m_NotifyProc(ContextObject))
                    ret = CompressedBuffer[0] == TOKEN_LOGIN ? 999 : 1;
                // CompressedBuffer 由 CONTEXT_OBJECT 管理，不在此处释放
                break;
            }

            ULONG ulPackTotalLength = 0;
            ContextObject->InCompressedBuffer.CopyBuffer(&ulPackTotalLength, sizeof(ULONG), pr.Result);
            //取出数据包的总长度5字节标识+4字节数据包总长度+4字节原始数据长度
            int bufLen = ContextObject->InCompressedBuffer.GetBufferLength();
            if (ulPackTotalLength && bufLen >= ulPackTotalLength) {
                ULONG ulCompressedLength = 0;
                ULONG ulOriginalLength = 0;
                PBYTE CompressedBuffer = ContextObject->ReadBuffer(ulCompressedLength, ulOriginalLength);
                if (ContextObject->CompressMethod == COMPRESS_UNKNOWN) {
                    // CompressedBuffer 由 CONTEXT_OBJECT 管理，不在此处释放
                    throw "Unknown method";
                } else if (ContextObject->CompressMethod == COMPRESS_NONE) {
                    ContextObject->InDeCompressedBuffer.ClearBuffer();
                    ContextObject->Decode(CompressedBuffer, ulOriginalLength);
                    ContextObject->InDeCompressedBuffer.WriteBuffer(CompressedBuffer, ulOriginalLength);
                    if (m_NotifyProc(ContextObject))
                        ret = CompressedBuffer[0] == TOKEN_LOGIN ? 999 : 1;
                    // CompressedBuffer 由 CONTEXT_OBJECT 管理，不在此处释放
                    continue;
                }
                bool usingZstd = ContextObject->CompressMethod == COMPRESS_ZSTD, zlibFailed = false;
                // 使用预分配缓冲区，避免频繁内存分配
                PBYTE DeCompressedBuffer = ContextObject->GetDecompressBuffer(ulOriginalLength);
                size_t	iRet = usingZstd ?
                               Muncompress(DeCompressedBuffer, &ulOriginalLength, CompressedBuffer, ulCompressedLength) :
                               z_uncompress(z, DeCompressedBuffer, &ulOriginalLength, CompressedBuffer, ulCompressedLength);
                if (usingZstd ? C_SUCCESS(iRet) : (S_OK==iRet)) {
                    ContextObject->InDeCompressedBuffer.ClearBuffer();
                    ContextObject->Decode(DeCompressedBuffer, ulOriginalLength);
                    ContextObject->InDeCompressedBuffer.WriteBuffer(DeCompressedBuffer, ulOriginalLength);
                    if (m_NotifyProc(ContextObject))
                        ret = DeCompressedBuffer[0] == TOKEN_LOGIN ? 999 : 1;
                } else if (usingZstd) {
                    // 尝试用zlib解压缩
                    if (Z_OK == z_uncompress(z, DeCompressedBuffer, &ulOriginalLength, CompressedBuffer, ulCompressedLength)) {
                        ContextObject->CompressMethod = COMPRESS_ZLIB;
                        ContextObject->InDeCompressedBuffer.ClearBuffer();
                        ContextObject->Decode(DeCompressedBuffer, ulOriginalLength);
                        ContextObject->InDeCompressedBuffer.WriteBuffer(DeCompressedBuffer, ulOriginalLength);
                        if (m_NotifyProc(ContextObject))
                            ret = DeCompressedBuffer[0] == TOKEN_LOGIN ? 999 : 1;
                    } else {
                        zlibFailed = true;
                        ContextObject->CompressMethod = COMPRESS_UNKNOWN;
                    }
                } else {
                    zlibFailed = true;
                }
                // CompressedBuffer 和 DeCompressedBuffer 都由 CONTEXT_OBJECT 管理，不在此处释放
                if (zlibFailed) {
                    Mprintf("[ERROR] ZLIB uncompress failed \n");
                    throw "Bad Buffer";
                }
            } else {
                break;
            }
        }
    } catch(...) {
        Mprintf("[ERROR] OnClientReceiving catch an error \n");
        ContextObject->InCompressedBuffer.ClearBuffer();
        ContextObject->InDeCompressedBuffer.ClearBuffer();
    }
    return ret;
}

BOOL IOCPServer::OnClientReceiving(PCONTEXT_OBJECT  ContextObject, DWORD dwTrans, ZSTD_DCtx* ctx, z_stream* z)
{
    if (FALSE == ParseReceivedData(ContextObject, dwTrans, m_NotifyProc, ctx, z)) {
        RemoveStaleContext(ContextObject);
        return FALSE;
    }

    PostRecv(ContextObject); //投递新的接收数据的请求

    return TRUE;
}

BOOL WriteContextData(CONTEXT_OBJECT* ContextObject, PBYTE szBuffer, size_t ulOriginalLength, ZSTD_CCtx* m_Cctx, z_stream* z)
{
    assert(ContextObject);
    // 输出服务端所发送的命令
    int cmd = szBuffer[0];
    if (ulOriginalLength < 100 && cmd != COMMAND_SCREEN_CONTROL && cmd != CMD_HEARTBEAT_ACK &&
        cmd != CMD_DRAW_POINT && cmd != CMD_MOVEWINDOW && cmd != CMD_SET_SIZE) {
        char buf[100] = { 0 };
        if (ulOriginalLength == 1) {
            sprintf_s(buf, "command %d", cmd);
        } else {
            memcpy(buf, szBuffer, ulOriginalLength);
        }
        Mprintf("[COMMAND] Send: %s\r\n", buf);
    }
    try {
        do {
            if (ulOriginalLength <= 0) return FALSE;
            if (ContextObject->CompressMethod == COMPRESS_UNKNOWN) {
                Mprintf("[ERROR] UNKNOWN compress method \n");
                return FALSE;
            } else if (ContextObject->CompressMethod == COMPRESS_NONE) {
                Buffer tmp(szBuffer, ulOriginalLength);
                szBuffer = tmp.Buf();
                ContextObject->WriteBuffer(szBuffer, ulOriginalLength, ulOriginalLength, cmd);
                break;
            }
            bool usingZstd = ContextObject->CompressMethod == COMPRESS_ZSTD;
            unsigned long	ulCompressedLength = usingZstd ?
                                                ZSTD_compressBound(ulOriginalLength) : (unsigned long)((double)ulOriginalLength * 1.001 + 12);
            // 使用预分配缓冲区替代每次 new
            LPBYTE			CompressedBuffer = ContextObject->GetSendCompressBuffer(ulCompressedLength);
            Buffer tmp(szBuffer, ulOriginalLength);
            szBuffer = tmp.Buf();
            ContextObject->Encode(szBuffer, ulOriginalLength);
            if (!m_Cctx) ContextObject->Encode(szBuffer, ulOriginalLength, usingZstd);
            size_t	iRet = usingZstd ?
                           Mcompress(CompressedBuffer, &ulCompressedLength, (LPBYTE)szBuffer, ulOriginalLength, ContextObject->GetZstdLevel()):
                           compress(CompressedBuffer, &ulCompressedLength, (LPBYTE)szBuffer, ulOriginalLength);

            if (usingZstd ? C_FAILED(iRet) : (S_OK != iRet)) {
                Mprintf("[ERROR] compress failed \n");
                // SendCompressBuffer 由 CONTEXT_OBJECT 管理，不在此处释放
                return FALSE;
            }

            ulCompressedLength =  usingZstd ? iRet : ulCompressedLength;

            ContextObject->WriteBuffer(CompressedBuffer, ulCompressedLength, ulOriginalLength, cmd);
            // SendCompressBuffer 由 CONTEXT_OBJECT 管理，不在此处释放
        } while (false);

        return TRUE;
    } catch (...) {
        Mprintf("[ERROR] OnClientPreSending catch an error \n");
        return FALSE;
    }
}

BOOL IOCPServer::OnClientPreSending(CONTEXT_OBJECT* ContextObject, PBYTE szBuffer, size_t ulOriginalLength)
{
    if (WriteContextData(ContextObject, szBuffer, ulOriginalLength, ContextObject->Zcctx)) {
        OVERLAPPEDPLUS* OverlappedPlus = new OVERLAPPEDPLUS(IOWrite);
        BOOL bOk = PostQueuedCompletionStatus(m_hCompletionPort, 0, (ULONG_PTR)ContextObject, &OverlappedPlus->m_ol);
        if ( (!bOk && GetLastError() != ERROR_IO_PENDING) ) { //如果投递失败
            int a = GetLastError();
            Mprintf("!!! OnClientPreSending 投递消息失败\n");
            RemoveStaleContext(ContextObject);
            SAFE_DELETE(OverlappedPlus);
            return FALSE;
        }
        return TRUE;
    }
    return FALSE;
}

BOOL IOCPServer::OnClientPostSending(CONTEXT_OBJECT* ContextObject,ULONG ulCompletedLength)
{
    CAutoCLock L(ContextObject->SendLock);
    try {
        DWORD ulFlags = MSG_PARTIAL;

        ContextObject->OutCompressedBuffer.RemoveCompletedBuffer(ulCompletedLength); //将完成的数据从数据结构中去除
        if (ContextObject->OutCompressedBuffer.GetBufferLength() == 0) {
            ContextObject->OutCompressedBuffer.ClearBuffer();
            return true;		                             //走到这里说明我们的数据真正完全发送
        } else {
            OVERLAPPEDPLUS * OverlappedPlus = new OVERLAPPEDPLUS(IOWrite); //数据没有完成  我们继续投递 发送请求

            ContextObject->wsaOutBuffer.buf = (char*)ContextObject->OutCompressedBuffer.GetBuffer(0);
            ContextObject->wsaOutBuffer.len = ContextObject->OutCompressedBuffer.GetBufferLength();
            int iOk = WSASend(ContextObject->sClientSocket, &ContextObject->wsaOutBuffer,1,
                              NULL, ulFlags,&OverlappedPlus->m_ol, NULL);
            if ( iOk == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING ) {
                if (RemoveStaleContext(ContextObject))
                    Mprintf("!!! OnClientPostSending 投递消息失败: %d\n", WSAGetLastError());
                SAFE_DELETE(OverlappedPlus);
                return FALSE;
            }
            return TRUE;
        }
    } catch(...) {
        Mprintf("[ERROR] OnClientPostSending catch an error \n");
    }

    return FALSE;
}

DWORD IOCPServer::ListenThreadProc(LPVOID lParam)   //监听线程
{
    IOCPServer* This = (IOCPServer*)(lParam);
    WSANETWORKEVENTS NetWorkEvents;

    while(!This->m_bTimeToKill) {
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

        if (NetWorkEvents.lNetworkEvents & FD_ACCEPT) {
            if (NetWorkEvents.iErrorCode[FD_ACCEPT_BIT] == 0) {
                This->OnAccept();
            } else {
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
                           &iLen);                     //通过我们的监听套接字来生成一个与之信号通信的套接字
    if (sClientSocket == SOCKET_ERROR) {
        return;
    }

    //我们在这里为每一个到达的信号维护了一个与之关联的数据结构这里简称为用户的上下背景文
    PCONTEXT_OBJECT ContextObject = AllocateContext(sClientSocket);   // Context

    if (ContextObject == NULL) {
        closesocket(sClientSocket);
        sClientSocket = INVALID_SOCKET;
        return;
    }

    ContextObject->sClientSocket = sClientSocket;

    ContextObject->wsaInBuf.buf = (char*)ContextObject->szBuffer;
    ContextObject->wsaInBuf.len = sizeof(ContextObject->szBuffer);

    HANDLE Handle = CreateIoCompletionPort((HANDLE)sClientSocket, m_hCompletionPort, (ULONG_PTR)ContextObject, 0);

    if (Handle!=m_hCompletionPort) {
        delete ContextObject;
        ContextObject = NULL;

        if (sClientSocket!=INVALID_SOCKET) {
            closesocket(sClientSocket);
            sClientSocket = INVALID_SOCKET;
        }

        return;
    }

    //设置套接字的选项卡 Set KeepAlive 开启保活机制 SO_KEEPALIVE
    //保持连接检测对方主机是否崩溃如果2小时内在此套接口的任一方向都没
    //有数据交换，TCP就自动给对方 发一个保持存活
    m_ulKeepLiveTime = 1000 * 60 * 3;
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
    EnterCriticalSection(&m_cs);
    m_ContextConnectionList.AddTail(ContextObject);     //插入到我们的内存列表中
    LeaveCriticalSection(&m_cs);

    OVERLAPPEDPLUS	*OverlappedPlus = new OVERLAPPEDPLUS(IOInitialize); //注意这里的重叠IO请求是 用户请求上线

    BOOL bOk = PostQueuedCompletionStatus(m_hCompletionPort, 0, (ULONG_PTR)ContextObject, &OverlappedPlus->m_ol); // 工作线程
    //因为我们接受到了一个用户上线的请求那么我们就将该请求发送给我们的完成端口 让我们的工作线程处理它
    if ( (!bOk && GetLastError() != ERROR_IO_PENDING)) { //如果投递失败
        int a = GetLastError();
        Mprintf("!!! OnAccept 投递消息失败\n");
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

    if (iOk == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
        int a = GetLastError();
        Mprintf("!!! PostRecv 投递消息失败\n");
        RemoveStaleContext(ContextObject);
        SAFE_DELETE(OverlappedPlus);
    }
}

PCONTEXT_OBJECT IOCPServer::AllocateContext(SOCKET s)
{
    PCONTEXT_OBJECT ContextObject = NULL;

    CLock cs(m_cs);

    if (m_ContextConnectionList.GetCount() >= m_ulMaxConnections) {
        static uint64_t notifyTime = 0;
        auto now = time(0);
        if (now - notifyTime > 15) {
            notifyTime = now;
            Mprintf("!!! AllocateContext: 达到最大连接数 %lu，拒绝新连接\n", m_ulMaxConnections);
            if (m_hMainWnd) {
                char tip[256];
                sprintf_s(tip, _TRF("达到最大连接数限制: %lu, 请释放连接"), m_ulMaxConnections);
                PostMessageA(m_hMainWnd, WM_SHOWNOTIFY, (WPARAM)new CharMsg(_TR("达到最大连接数")),
                             (LPARAM)new CharMsg(tip));
            }
        }
        return NULL;
    }

    ContextObject = !m_ContextFreePoolList.IsEmpty() ? m_ContextFreePoolList.RemoveHead() : new CONTEXT_OBJECT;

    if (ContextObject != NULL) {
        ContextObject->InitMember(s, this);
    }

    return ContextObject;
}

BOOL IOCPServer::RemoveStaleContext(CONTEXT_OBJECT* ContextObject)
{
    EnterCriticalSection(&m_cs);
    auto find = m_ContextConnectionList.Find(ContextObject);
    LeaveCriticalSection(&m_cs);
    if (find) {  //在内存中查找该用户的上下文数据结构
        m_OfflineProc(ContextObject);

        CancelIo((HANDLE)ContextObject->sClientSocket);  //取消在当前套接字的异步IO -->PostRecv
        closesocket(ContextObject->sClientSocket);      //关闭套接字
        ContextObject->sClientSocket = INVALID_SOCKET;

        while (!HasOverlappedIoCompleted((LPOVERLAPPED)ContextObject)) { //判断还有没有异步IO请求在当前套接字上
            Sleep(0);
        }

        MoveContextToFreePoolList(ContextObject);  //将该内存结构回收至内存池
        return TRUE;
    }
    return FALSE;
}

VOID IOCPServer::MoveContextToFreePoolList(CONTEXT_OBJECT* ContextObject)
{
    CLock cs(m_cs);

    POSITION Pos = m_ContextConnectionList.Find(ContextObject);
    if (Pos) {
        ContextObject->InCompressedBuffer.ClearBuffer();
        ContextObject->InDeCompressedBuffer.ClearBuffer();
        ContextObject->OutCompressedBuffer.ClearBuffer();

        memset(ContextObject->szBuffer,0,8192);
        m_ContextFreePoolList.AddTail(ContextObject); //回收至内存池
        m_ContextConnectionList.RemoveAt(Pos); //从内存结构中移除
    }
}

void IOCPServer::UpdateMaxConnection(int maxConn)
{
    CLock cs(m_cs);
    m_ulMaxConnections = maxConn;
}
