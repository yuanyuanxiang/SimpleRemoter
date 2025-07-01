#pragma once

#include "StdAfx.h"
#include <WinSock2.h>
#pragma comment(lib,"ws2_32.lib")
#include "Server.h"

#if USING_CTX
#include "zstd/zstd.h"
#endif

#include <Mstcpip.h>

#define	NC_CLIENT_CONNECT		0x0001
#define	NC_RECEIVE				0x0004
#define NC_RECEIVE_COMPLETE		0x0005 // 完整接收

// ZLIB 压缩库
#include "zlib/zlib.h"

#if USING_LZ4
#include "lz4/lz4.h"
#pragma comment(lib, "lz4/lz4.lib")
#define C_FAILED(p) (0 == (p))
#define C_SUCCESS(p) (!C_FAILED(p))
#define Mcompress(dest, destLen, source, sourceLen) LZ4_compress_default((const char*)source, (char*)dest, sourceLen, *(destLen))
#define Muncompress(dest, destLen, source, sourceLen) LZ4_decompress_safe((const char*)source, (char*)dest, sourceLen, *(destLen))
#else // ZSTD
#include "zstd/zstd.h"
#ifdef _WIN64
#pragma comment(lib, "zstd/zstd_x64.lib")
#else
#pragma comment(lib, "zstd/zstd.lib")
#endif
#define C_FAILED(p) ZSTD_isError(p)
#define C_SUCCESS(p) (!C_FAILED(p))
#define ZSTD_CLEVEL 5
#if USING_CTX
#define Mcompress(dest, destLen, source, sourceLen) ZSTD_compress2(m_Cctx, dest, *(destLen), source, sourceLen)
#define Muncompress(dest, destLen, source, sourceLen) ZSTD_decompressDCtx(m_Dctx, dest, *(destLen), source, sourceLen)
#else
#define Mcompress(dest, destLen, source, sourceLen) ZSTD_compress(dest, *(destLen), source, sourceLen, ZSTD_CLEVEL_DEFAULT)
#define Muncompress(dest, destLen, source, sourceLen) ZSTD_decompress(dest, *(destLen), source, sourceLen)
#endif
#endif


class IOCPServer : public Server
{
	typedef void (CALLBACK* pfnNotifyProc)(CONTEXT_OBJECT* ContextObject);
	typedef void (CALLBACK* pfnOfflineProc)(CONTEXT_OBJECT* ContextObject);
protected:
	SOCKET				m_sListenSocket;
	HANDLE				m_hCompletionPort;
	UINT				m_ulMaxConnections;
	HANDLE				m_hListenEvent;
	HANDLE				m_hListenThread;
	BOOL				m_bTimeToKill;
	HANDLE				m_hKillEvent;
	ULONG				m_ulThreadPoolMin;
	ULONG				m_ulThreadPoolMax;
	ULONG				m_ulCPULowThreadsHold; 
	ULONG				m_ulCPUHighThreadsHold;
	ULONG				m_ulCurrentThread;
	ULONG				m_ulBusyThread;

#if USING_CTX
	ZSTD_CCtx*			m_Cctx; // 压缩上下文
	ZSTD_DCtx*			m_Dctx; // 解压上下文
#endif

	ULONG				m_ulKeepLiveTime;
	pfnNotifyProc		m_NotifyProc;
	pfnOfflineProc		m_OfflineProc;
	ULONG				m_ulWorkThreadCount;
	CRITICAL_SECTION	m_cs;
	ContextObjectList	m_ContextConnectionList;
	ContextObjectList	m_ContextFreePoolList;

private:
	static DWORD WINAPI ListenThreadProc(LPVOID lParam);
	static DWORD WINAPI WorkThreadProc(LPVOID lParam);

	BOOL InitializeIOCP(VOID);
	VOID OnAccept();
	PCONTEXT_OBJECT AllocateContext(SOCKET s);
	VOID RemoveStaleContext(CONTEXT_OBJECT* ContextObject);
	VOID MoveContextToFreePoolList(CONTEXT_OBJECT* ContextObject);
	VOID PostRecv(CONTEXT_OBJECT* ContextObject);
	BOOL HandleIO(IOType PacketFlags, PCONTEXT_OBJECT ContextObject, DWORD dwTrans);
	BOOL OnClientInitializing(PCONTEXT_OBJECT  ContextObject, DWORD dwTrans);
	BOOL OnClientReceiving(PCONTEXT_OBJECT  ContextObject, DWORD dwTrans);
	VOID OnClientPreSending(CONTEXT_OBJECT* ContextObject, PBYTE szBuffer, size_t ulOriginalLength);
	BOOL OnClientPostSending(CONTEXT_OBJECT* ContextObject, ULONG ulCompressedLength);
	int AddWorkThread(int n) {
		EnterCriticalSection(&m_cs);
		m_ulWorkThreadCount += n;
		int ret = m_ulWorkThreadCount;
		LeaveCriticalSection(&m_cs);
		return ret;
	}

public:
	IOCPServer(void);
	~IOCPServer(void);

	UINT StartServer(pfnNotifyProc NotifyProc, pfnOfflineProc OffProc, USHORT uPort);

	VOID Send2Client(CONTEXT_OBJECT* ContextObject, PBYTE szBuffer, ULONG ulOriginalLength) {
		OnClientPreSending(ContextObject, szBuffer, ulOriginalLength);
	}

	void UpdateMaxConnection(int maxConn);

	void Destroy();
	void Disconnect(CONTEXT_OBJECT *ctx){}
};

typedef IOCPServer ISocketBase;

typedef IOCPServer CIOCPServer;

typedef CONTEXT_OBJECT ClientContext;

#define m_Socket sClientSocket
#define m_DeCompressionBuffer InDeCompressedBuffer

// 所有动态创建的对话框的基类
class CDialogBase : public CDialog {
public:
	CONTEXT_OBJECT* m_ContextObject;
	Server* m_iocpServer;
	CString m_IPAddress;
	bool m_bIsClosed;
	bool m_bIsProcessing;
	HICON m_hIcon;
	CDialogBase(UINT nIDTemplate, CWnd* pParent, Server* pIOCPServer, CONTEXT_OBJECT* pContext, int nIcon) :
		m_bIsClosed(false), m_bIsProcessing(false),
		m_ContextObject(pContext),
		m_iocpServer(pIOCPServer),
		CDialog(nIDTemplate, pParent) {

		sockaddr_in  sockAddr;
		memset(&sockAddr, 0, sizeof(sockAddr));
		int nSockAddrLen = sizeof(sockaddr_in);
		BOOL bResult = getpeername(m_ContextObject->sClientSocket, (SOCKADDR*)&sockAddr, &nSockAddrLen);

		m_IPAddress = bResult != INVALID_SOCKET ? inet_ntoa(sockAddr.sin_addr) : "";
		m_hIcon = nIcon > 0 ? LoadIcon(AfxGetInstanceHandle(), MAKEINTRESOURCE(nIcon)) : NULL;
	}
	virtual ~CDialogBase(){}

public:
	virtual void OnReceiveComplete(void) = 0;
	// 标记为是否正在接受数据
	void MarkReceiving(bool recv = true) {
		m_bIsProcessing = recv;
	}
	bool IsProcessing() const {
		return m_bIsProcessing;
	}
	void OnClose() {
		m_bIsClosed = true;
		while (m_bIsProcessing)
			Sleep(200);
		if(m_hIcon) DestroyIcon(m_hIcon);
		m_hIcon = NULL;
		CDialog::OnClose();

		if (GetSafeHwnd())
			DestroyWindow();
	}
	virtual void PostNcDestroy() override
	{
		delete this;
	}
	// 取消 SOCKET 读取，该函数可以被多次调用
	void CancelIO(){
		m_bIsClosed = TRUE;

		m_ContextObject->CancelIO();
	}
};

typedef CDialogBase DialogBase;

BOOL ParseReceivedData(CONTEXT_OBJECT* ContextObject, DWORD dwTrans, pfnNotifyProc m_NotifyProc);

BOOL WriteContextData(CONTEXT_OBJECT* ContextObject, PBYTE szBuffer, size_t ulOriginalLength);
