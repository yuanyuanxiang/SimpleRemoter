// IOCPClient.cpp: implementation of the IOCPClient class.
//
//////////////////////////////////////////////////////////////////////
#ifdef _WIN32
#include "stdafx.h"
#include <WS2tcpip.h>
#else
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netinet/in.h>  // For struct sockaddr_in
#include <unistd.h>      // For close()
#include <cstring>       // For memset()
inline int WSAGetLastError() { return -1; }
#define USING_COMPRESS 1
#endif
#include "IOCPClient.h"
#include <assert.h>
#include <string>
#if USING_ZLIB
#include "zlib/zlib.h"
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
#ifdef _WIN64
#pragma comment(lib, "zstd/zstd_x64.lib")
#else
#pragma comment(lib, "zstd/zstd.lib")
#endif
#define Z_FAILED(p) ZSTD_isError(p)
#define Z_SUCCESS(p) (!Z_FAILED(p))
#define ZSTD_CLEVEL 5
#if USING_CTX
#define compress(dest, destLen, source, sourceLen) ZSTD_compress2(m_Cctx, dest, *(destLen), source, sourceLen)
#define uncompress(dest, destLen, source, sourceLen) ZSTD_decompressDCtx(m_Dctx, dest, *(destLen), source, sourceLen)
#else
#define compress(dest, destLen, source, sourceLen) ZSTD_compress(dest, *(destLen), source, sourceLen, ZSTD_CLEVEL_DEFAULT)
#define uncompress(dest, destLen, source, sourceLen) ZSTD_decompress(dest, *(destLen), source, sourceLen)
#endif
#endif
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

#ifndef _WIN32
BOOL SetKeepAliveOptions(int socket, int nKeepAliveSec = 180) {
	// ���� TCP ����ѡ��
	int enable = 1;
	if (setsockopt(socket, SOL_SOCKET, SO_KEEPALIVE, &enable, sizeof(enable)) < 0) {
		Mprintf("Failed to enable TCP keep-alive\n");
		return FALSE;
	}

	// ���� TCP_KEEPIDLE (3���ӿ��к�ʼ���� keep-alive ��)
	if (setsockopt(socket, IPPROTO_TCP, TCP_KEEPIDLE, &nKeepAliveSec, sizeof(nKeepAliveSec)) < 0) {
		Mprintf("Failed to set TCP_KEEPIDLE\n");
		return FALSE;
	}

	// ���� TCP_KEEPINTVL (5������Լ��)
	int keepAliveInterval = 5;  // 5��
	if (setsockopt(socket, IPPROTO_TCP, TCP_KEEPINTVL, &keepAliveInterval, sizeof(keepAliveInterval)) < 0) {
		Mprintf("Failed to set TCP_KEEPINTVL\n");
		return FALSE;
	}

	// ���� TCP_KEEPCNT (���5��̽�������Ϊ���ӶϿ�)
	int keepAliveProbes = 5;
	if (setsockopt(socket, IPPROTO_TCP, TCP_KEEPCNT, &keepAliveProbes, sizeof(keepAliveProbes)) < 0) {
		Mprintf("Failed to set TCP_KEEPCNT\n");
		return FALSE;
	}

	Mprintf("TCP keep-alive settings applied successfully\n");
	return TRUE;
}
#endif

VOID IOCPClient::setManagerCallBack(void* Manager,  DataProcessCB dataProcess)
{
	m_Manager = Manager;

	m_DataProcess = dataProcess;
}


IOCPClient::IOCPClient(State&bExit, bool exit_while_disconnect) : g_bExit(bExit)
{
	m_nHostPort = 0;
	m_Manager = NULL;
#ifdef _WIN32
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

	m_sClientSocket = INVALID_SOCKET;
	m_hWorkThread   = NULL;
	m_bWorkThread = S_STOP;

	memset(m_szPacketFlag, 0, sizeof(m_szPacketFlag));
	memcpy(m_szPacketFlag,"Shine",FLAG_LENGTH);

	m_bIsRunning = TRUE;
	m_bConnected = FALSE;

	m_exit_while_disconnect = exit_while_disconnect;
#if USING_CTX
	m_Cctx = ZSTD_createCCtx();
	m_Dctx = ZSTD_createDCtx();
	ZSTD_CCtx_setParameter(m_Cctx, ZSTD_c_compressionLevel, ZSTD_CLEVEL);
#endif
}

IOCPClient::~IOCPClient()
{
	m_bIsRunning = FALSE;

	if (m_sClientSocket!=INVALID_SOCKET)
	{
		closesocket(m_sClientSocket);
		m_sClientSocket = INVALID_SOCKET;
	}

	if (m_hWorkThread!=NULL)
	{
		CloseHandle(m_hWorkThread);
		m_hWorkThread = NULL;
	}

#ifdef _WIN32
	WSACleanup();
#endif

	while (S_RUN == m_bWorkThread)
		Sleep(10);

	m_bWorkThread = S_END;
#if USING_CTX
	ZSTD_freeCCtx(m_Cctx);
	ZSTD_freeDCtx(m_Dctx);
#endif
}

// ��������ȡIP��ַ
std::string GetIPAddress(const char *hostName)
{
#ifdef _WIN32
	struct sockaddr_in sa = { 0 };
	if (inet_pton(AF_INET, hostName, &(sa.sin_addr)) == 1) {
		return hostName;
	}
	struct hostent *host = gethostbyname(hostName);
#ifdef _DEBUG
	if (host == NULL) return "";
	Mprintf("��������IP����Ϊ: %s.\n", host->h_addrtype == AF_INET ? "IPV4" : "IPV6");
	for (int i = 0; host->h_addr_list[i]; ++i)
		Mprintf("��ȡ�ĵ�%d��IP: %s\n", i+1, inet_ntoa(*(struct in_addr*)host->h_addr_list[i]));
#endif
	if (host == NULL || host->h_addr_list == NULL)
		return "";
	return host->h_addr_list[0] ? inet_ntoa(*(struct in_addr*)host->h_addr_list[0]) : "";
#else
	struct addrinfo hints, * res;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET; // IPv4
	hints.ai_socktype = SOCK_STREAM; // TCP socket

	int status = getaddrinfo(hostName, nullptr, &hints, &res);
	if (status != 0) {
		Mprintf("getaddrinfo failed: %s\n", gai_strerror(status));
		return "";
	}

	struct sockaddr_in* addr = reinterpret_cast<struct sockaddr_in*>(res->ai_addr);
	char ip[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &(addr->sin_addr), ip, sizeof(ip));

	Mprintf("IP Address: %s \n", ip);

	freeaddrinfo(res); // ��Ҫ�����ͷŵ�ַ��Ϣ
	return ip;
#endif
}

BOOL IOCPClient::ConnectServer(const char* szServerIP, unsigned short uPort)
{
	if (szServerIP != NULL && uPort != 0) {
		SetServerAddress(szServerIP, uPort);
	}
	m_sCurIP = m_Domain.SelectIP();
	unsigned short port = m_nHostPort;

	m_sClientSocket = socket(AF_INET,SOCK_STREAM, IPPROTO_TCP);    //�����

	if (m_sClientSocket == SOCKET_ERROR)   
	{ 
		return FALSE;   
	}

#ifdef _WIN32
	//����sockaddr_in�ṹ Ҳ�������ض˵Ľṹ
	sockaddr_in	ServerAddr;
	ServerAddr.sin_family	= AF_INET;               //�����  IP
	ServerAddr.sin_port	= htons(port);
	ServerAddr.sin_addr.S_un.S_addr = inet_addr(m_sCurIP.c_str());

	if (connect(m_sClientSocket,(SOCKADDR *)&ServerAddr,sizeof(sockaddr_in)) == SOCKET_ERROR) 
	{
		if (m_sClientSocket!=INVALID_SOCKET)
		{
			closesocket(m_sClientSocket);
			m_sClientSocket = INVALID_SOCKET;
		}
		return FALSE;
	}
#else
	sockaddr_in ServerAddr = {};
	ServerAddr.sin_family = AF_INET;   // ����� IP
	ServerAddr.sin_port = htons(port);
	// ��szServerIP�����ֿ�ͷ������Ϊ�������������IPת��
	// ʹ�� inet_pton ��� inet_addr (inet_pton ����֧�� IPv4 �� IPv6)
	if (inet_pton(AF_INET, m_sCurIP.c_str(), &ServerAddr.sin_addr) <= 0) {
		Mprintf("Invalid address or address not supported\n");
		return false;
	}

	// �����׽���
	m_sClientSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (m_sClientSocket == -1) {
		Mprintf("Failed to create socket\n");
		return false;
	}

	// ���ӵ�������
	if (connect(m_sClientSocket, (struct sockaddr*)&ServerAddr, sizeof(ServerAddr)) == -1) {
		Mprintf("Connection failed\n");
		close(m_sClientSocket);
		m_sClientSocket = -1;  // ����׽�����Ч
		return false;
	}
#endif

	const int chOpt = 1; // True
	// Set KeepAlive �����������, ��ֹ����˲���������
	if (setsockopt(m_sClientSocket, SOL_SOCKET, SO_KEEPALIVE,
		(char *)&chOpt, sizeof(chOpt)) == 0)
	{
#ifdef _WIN32
		// ���ó�ʱ��ϸ��Ϣ
		tcp_keepalive	klive;
		klive.onoff = 1; // ���ñ���
		klive.keepalivetime = 1000 * 60 * 3; // 3���ӳ�ʱ Keep Alive
		klive.keepaliveinterval = 1000 * 5;  // ���Լ��Ϊ5�� Resend if No-Reply
		WSAIoctl(m_sClientSocket, SIO_KEEPALIVE_VALS,&klive,sizeof(tcp_keepalive),
			NULL,	0,(unsigned long *)&chOpt,0,NULL);
#else
		// ���ñ���ѡ��
		SetKeepAliveOptions(m_sClientSocket);
#endif
	}
	if (m_hWorkThread == NULL){
#ifdef _WIN32
		m_hWorkThread = (HANDLE)CreateThread(NULL, 0, WorkThreadProc,(LPVOID)this, 0, NULL);
		m_bWorkThread = m_hWorkThread ? S_RUN : S_STOP;
#else
		pthread_t id = 0;
		m_hWorkThread = (HANDLE)pthread_create(&id, nullptr, (void* (*)(void*))IOCPClient::WorkThreadProc, this);
#endif
	}
	Mprintf("���ӷ���˳ɹ�.\n");
	m_bConnected = TRUE;
	return TRUE;
}

DWORD WINAPI IOCPClient::WorkThreadProc(LPVOID lParam)
{
	IOCPClient* This = (IOCPClient*)lParam;
	char* szBuffer = new char[MAX_RECV_BUFFER];
	fd_set fd;
	struct timeval tm = { 2, 0 };
	This->m_CompressedBuffer.ClearBuffer();

	while (This->IsRunning()) // û���˳�����һֱ�������ѭ����
	{
		if(!This->IsConnected())
		{
			Sleep(50);
			continue;
		}
		FD_ZERO(&fd);
		FD_SET(This->m_sClientSocket, &fd);
#ifdef _WIN32
		int iRet = select(NULL, &fd, NULL, NULL, &tm);
#else
		int iRet = select(This->m_sClientSocket + 1, &fd, NULL, NULL, &tm);
#endif
		if (iRet <= 0)      
		{
			if (iRet == 0) Sleep(50);
			else
			{
				Mprintf("[select] return %d, GetLastError= %d. \n", iRet, WSAGetLastError());
				This->Disconnect(); //���մ�����
				This->m_CompressedBuffer.ClearBuffer();
				if(This->m_exit_while_disconnect)
					break;
			}
		}
		else if (iRet > 0)
		{
			int iReceivedLength = recv(This->m_sClientSocket, szBuffer, MAX_RECV_BUFFER-1, 0);
			if (iReceivedLength <= 0)
			{
				int a = WSAGetLastError();
				This->Disconnect(); //���մ�����
				This->m_CompressedBuffer.ClearBuffer();
				if(This->m_exit_while_disconnect)
					break;
			}else{
				szBuffer[iReceivedLength] = 0;
				//��ȷ���վ͵���OnRead����,ת��OnRead
				This->OnServerReceiving(szBuffer, iReceivedLength);
			}
		}
	}
	CloseHandle(This->m_hWorkThread);
	This->m_hWorkThread = NULL;
	This->m_bWorkThread = S_STOP;
	This->m_bIsRunning = FALSE;
	delete[] szBuffer;

	return 0xDEAD;
}

// ���쳣��������ݴ����߼�:
// ��� f ִ��ʱ û�д���ϵͳ�쳣������ʳ�ͻ�������� 0
// ��� f ִ�й����� �׳����쳣�������ָ����ʣ������� __except ���񣬷����쳣�루�� 0xC0000005 ��ʾ����Υ�棩
int DataProcessWithSEH(DataProcessCB f, void* manager, LPBYTE data, ULONG len) {
	__try {
		if (f) f(manager, data, len);
		return 0;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return GetExceptionCode();
	}
}


VOID IOCPClient::OnServerReceiving(char* szBuffer, ULONG ulLength)
{
	try
	{
		assert (ulLength > 0);	
		//���½ӵ����ݽ��н�ѹ��
		m_CompressedBuffer.WriteBuffer((LPBYTE)szBuffer, ulLength);

		//��������Ƿ��������ͷ��С ��������ǾͲ�����ȷ������
		while (m_CompressedBuffer.GetBufferLength() > HDR_LENGTH)
		{
			char szPacketFlag[FLAG_LENGTH + 3] = {0};
			LPBYTE src = m_CompressedBuffer.GetBuffer();
			CopyMemory(szPacketFlag, src, FLAG_LENGTH);
			//�ж�����ͷ
			if (memcmp(m_szPacketFlag, szPacketFlag, FLAG_LENGTH) != 0)
			{
				Mprintf("[ERROR] OnServerReceiving memcmp fail: unknown header '%s'\n", szPacketFlag);
				m_CompressedBuffer.ClearBuffer();
				break;
			}

			ULONG ulPackTotalLength = 0;
			CopyMemory(&ulPackTotalLength, m_CompressedBuffer.GetBuffer(FLAG_LENGTH), sizeof(ULONG));

			//--- ���ݵĴ�С��ȷ�ж�
			ULONG len = m_CompressedBuffer.GetBufferLength();
			if (ulPackTotalLength && len >= ulPackTotalLength)
			{
				ULONG ulOriginalLength = 0;

				m_CompressedBuffer.ReadBuffer((PBYTE)szPacketFlag, FLAG_LENGTH);//��ȡ����ͷ�� shine
				m_CompressedBuffer.ReadBuffer((PBYTE) &ulPackTotalLength, sizeof(ULONG));
				m_CompressedBuffer.ReadBuffer((PBYTE) &ulOriginalLength, sizeof(ULONG));

				ULONG ulCompressedLength = ulPackTotalLength - HDR_LENGTH;
				const int bufSize = 512;
				BYTE buf1[bufSize], buf2[bufSize];
				PBYTE CompressedBuffer = ulCompressedLength > bufSize ? new BYTE[ulCompressedLength] : buf1;
				PBYTE DeCompressedBuffer = ulCompressedLength > bufSize ? new BYTE[ulOriginalLength] : buf2;

				m_CompressedBuffer.ReadBuffer(CompressedBuffer, ulCompressedLength);

				size_t	iRet = uncompress(DeCompressedBuffer, &ulOriginalLength, CompressedBuffer, ulCompressedLength);

				if (Z_SUCCESS(iRet))//�����ѹ�ɹ�
				{
					//��ѹ�õ����ݺͳ��ȴ��ݸ�����Manager���д��� ע�����������˶�̬
					//����m_pManager�е����಻һ����ɵ��õ�OnReceive������һ��
					int ret = DataProcessWithSEH(m_DataProcess, m_Manager, DeCompressedBuffer, ulOriginalLength);
					if (ret) {
						Mprintf("[ERROR] DataProcessWithSEH return exception code: [0x%08X]\n", ret);
					}
				}
				else{
					Mprintf("[ERROR] uncompress fail: dstLen %d, srcLen %d\n", ulOriginalLength, ulCompressedLength);
					m_CompressedBuffer.ClearBuffer();
				}

				if (CompressedBuffer != buf1)delete [] CompressedBuffer;
				if (DeCompressedBuffer != buf2)delete [] DeCompressedBuffer;
			}
			else {
				break; // received data is incomplete
			}
		}
	}catch(...) { 
		m_CompressedBuffer.ClearBuffer();
		Mprintf("[ERROR] OnServerReceiving catch an error \n");
	}
}


// ��server�������ݣ�ѹ�������ȽϺ�ʱ��
// �ر�ѹ������ʱ��SendWithSplit�ȽϺ�ʱ��
BOOL IOCPClient::OnServerSending(const char* szBuffer, ULONG ulOriginalLength)  //Hello
{
	AUTO_TICK(50);
	assert (ulOriginalLength > 0);
	{
		//����1.001�������Ҳ��������ѹ����ռ�õ��ڴ�ռ��ԭ��һ�� +12
		//��ֹ���������//  HelloWorld  10   22
		//����ѹ�� ѹ���㷨 ΢���ṩ
		//nSize   = 436
		//destLen = 448
#if USING_ZLIB
		unsigned long	ulCompressedLength = (double)ulOriginalLength * 1.001  + 12;
#elif USING_LZ4
		unsigned long	ulCompressedLength = LZ4_compressBound(ulOriginalLength);
#else
		unsigned long	ulCompressedLength = ZSTD_compressBound(ulOriginalLength);
#endif
		BYTE			buf[1024];
		LPBYTE			CompressedBuffer = ulCompressedLength>1024 ? new BYTE[ulCompressedLength] : buf;

		int	iRet = compress(CompressedBuffer, &ulCompressedLength, (PBYTE)szBuffer, ulOriginalLength);
		if (Z_FAILED(iRet))
		{
			Mprintf("[ERROR] compress failed: srcLen %d, dstLen %d \n", ulOriginalLength, ulCompressedLength);
			if (CompressedBuffer != buf)  delete [] CompressedBuffer;
			return FALSE;
		}
#if !USING_ZLIB
		ulCompressedLength = iRet;
#endif
		ULONG ulPackTotalLength = ulCompressedLength + HDR_LENGTH;
		CBuffer m_WriteBuffer;

		m_WriteBuffer.WriteBuffer((PBYTE)m_szPacketFlag, FLAG_LENGTH);

		m_WriteBuffer.WriteBuffer((PBYTE) &ulPackTotalLength,sizeof(ULONG));

		m_WriteBuffer.WriteBuffer((PBYTE)&ulOriginalLength, sizeof(ULONG));

		m_WriteBuffer.WriteBuffer(CompressedBuffer,ulCompressedLength);

		if (CompressedBuffer != buf) delete [] CompressedBuffer;

		// �ֿ鷢��
		return SendWithSplit((char*)m_WriteBuffer.GetBuffer(), m_WriteBuffer.GetBufferLength(), MAX_SEND_BUFFER);
	}
}

//  5    2   //  2  2  1
BOOL IOCPClient::SendWithSplit(const char* szBuffer, ULONG ulLength, ULONG ulSplitLength)
{
	AUTO_TICK(25);
	int			 iReturn = 0;   //���������˶���
	const char*  Travel = szBuffer;
	int			 i = 0;
	int			 ulSended = 0;
	const int	 ulSendRetry = 15;
	// ���η���
	for (i = ulLength; i >= ulSplitLength; i -= ulSplitLength)
	{
		int j = 0;
		for (; j < ulSendRetry; ++j)
		{
			iReturn = send(m_sClientSocket, Travel, ulSplitLength, 0);
			if (iReturn > 0)
			{
				break;
			}
		}
		if (j == ulSendRetry)
		{
			return FALSE;
		}

		ulSended += iReturn;
		Travel += ulSplitLength;
	}
	// �������Ĳ���
	if (i>0)  //1024
	{
		int j = 0;
		for (; j < ulSendRetry; j++)
		{
			iReturn = send(m_sClientSocket, (char*)Travel,i,0);

			if (iReturn > 0)
			{
				break;
			}
		}
		if (j == ulSendRetry)
		{
			return FALSE;
		}
		ulSended += iReturn;
	}

	return (ulSended == ulLength) ? TRUE : FALSE;
}


VOID IOCPClient::Disconnect() 
{
	if (m_sClientSocket == INVALID_SOCKET)
		return;

	Mprintf("Disconnect with [%s:%d].\n", m_sCurIP.c_str(), m_nHostPort);

	CancelIo((HANDLE)m_sClientSocket);
	closesocket(m_sClientSocket);	
	m_sClientSocket = INVALID_SOCKET;

	m_bConnected = FALSE;
}


VOID IOCPClient::RunEventLoop(const BOOL &bCondition)
{
	Mprintf("======> RunEventLoop begin\n");
	while ((m_bIsRunning && bCondition) || bCondition == FOREVER_RUN)
		Sleep(200);
	setManagerCallBack(NULL, NULL);
	Mprintf("======> RunEventLoop end\n");
}
