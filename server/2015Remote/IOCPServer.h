#pragma once

#include <WinSock2.h>
#pragma comment(lib,"ws2_32.lib")
#include "CpuUseage.h"
#include "Buffer.h"
#if USING_CTX
#include "zstd/zstd.h"
#endif

#include <Mstcpip.h>
#define PACKET_LENGTH   0x2000

#define FLAG_LENGTH   5
#define HDR_LENGTH   13

#define	NC_CLIENT_CONNECT		0x0001
#define	NC_RECEIVE				0x0004
#define NC_RECEIVE_COMPLETE		0x0005 // 完整接收

std::string GetRemoteIP(SOCKET sock);

// Encoder interface. The default encoder will do nothing.
class Encoder {
public:
	virtual ~Encoder(){}
	// Encode data before compress.
	virtual void Encode(unsigned char* data, int len) const{}
	// Decode data after uncompress.
	virtual void Decode(unsigned char* data, int len) const{}
};

// XOR Encoder implementation.
class XOREncoder : public Encoder {
private:
	std::vector<char> Keys;

public:
	XOREncoder(const std::vector<char>& keys = {0}) : Keys(keys){}

	virtual void Encode(unsigned char* data, int len) const {
		XOR(data, len, Keys);
	}

	virtual void Decode(unsigned char* data, int len) const {
		static std::vector<char> reversed(Keys.rbegin(), Keys.rend());
		XOR(data, len, reversed);
	}

protected:
	void XOR(unsigned char* data, int len, const std::vector<char> &keys) const {
		for (char key : keys) {
			for (int i = 0; i < len; ++i) {
				data[i] ^= key;
			}
		}
	}
};

enum {
	PARSER_FAILED = -1,			// 解析失败
	PARSER_NEEDMORE = 0,		// 需要更多数据
};

typedef struct PR {
	int Result;
	bool IsFailed() const {
		return PARSER_FAILED == Result;
	}
	bool IsNeedMore() const {
		return PARSER_NEEDMORE == Result;
	}
}PR;

enum {
	COMPRESS_UNKNOWN = -2,			// 未知压缩算法
	COMPRESS_ZLIB = -1,				// 以前版本使用的压缩方法
	COMPRESS_ZSTD = 0,				// 当前使用的压缩方法
	COMPRESS_NONE = 1,				// 没有压缩
};

struct CONTEXT_OBJECT;

// Header parser: parse the data to make sure it's from a supported client.
class HeaderParser {
	friend struct CONTEXT_OBJECT;
protected:
	HeaderParser() {
		memset(this, 0, sizeof(HeaderParser));
	}
	virtual ~HeaderParser() {
		Reset();
	}
	PR Parse(CBuffer& buf, int &compressMethod) {
		const int MinimumCount = 8;
		if (buf.GetBufferLength() < MinimumCount) {
			return PR{ PARSER_NEEDMORE };
		}
		char szPacketFlag[32] = { 0 };
		buf.CopyBuffer(szPacketFlag, MinimumCount, 0);
		if (m_bParsed) { // Check if the header has been parsed.
			return memcmp(m_szPacketFlag, szPacketFlag, m_nCompareLen) == 0 ? PR{ m_nFlagLen } : PR{ PARSER_FAILED };
		}
		// More version may be added in the future.
		const char version0[] = "Shine", version1[] = "<<FUCK>>", version2[] = "Hello?";
		if (memcmp(version0, szPacketFlag, sizeof(version0) - 1) == 0) {
			memcpy(m_szPacketFlag, version0, sizeof(version0) - 1);
			m_nCompareLen = strlen(m_szPacketFlag);
			m_nFlagLen = m_nCompareLen;
			m_nHeaderLen = m_nFlagLen + 8;
			m_bParsed = TRUE;
			m_Encoder = new Encoder();
		}
		else if (memcmp(version1, szPacketFlag, sizeof(version1) - 1) == 0) {
			memcpy(m_szPacketFlag, version1, sizeof(version1) - 1);
			m_nCompareLen = strlen(m_szPacketFlag);
			m_nFlagLen = m_nCompareLen + 3;
			m_nHeaderLen = m_nFlagLen + 8;
			m_bParsed = TRUE;
			m_Encoder = new XOREncoder();
		}
		else if (memcmp(version2, szPacketFlag, sizeof(version2) - 1) == 0) {
			memcpy(m_szPacketFlag, version2, sizeof(version2) - 1);
			m_nCompareLen = strlen(m_szPacketFlag);
			m_nFlagLen = 8;
			m_nHeaderLen = m_nFlagLen + 8;
			m_bParsed = TRUE;
			compressMethod = COMPRESS_NONE;
			m_Encoder = new Encoder();
		}
		else {
			return PR{ PARSER_FAILED };
		}
		return PR{ m_nFlagLen };
	}
	HeaderParser& Reset() {
		SAFE_DELETE(m_Encoder);
		memset(this, 0, sizeof(HeaderParser));
		return *this;
	}
	BOOL IsParsed() const {
		return m_bParsed;
	}
	int GetFlagLen() const {
		return m_nFlagLen;
	}
	int GetHeaderLen() const {
		return m_nHeaderLen;
	}
	const char* GetFlag() const {
		return m_szPacketFlag;
	}
	Encoder* GetEncoder() const {
		return m_Encoder;
	}
private:
	BOOL				m_bParsed;					// 数据包是否可以解析
	int					m_nHeaderLen;				// 数据包的头长度
	int					m_nCompareLen;				// 比对字节数
	int					m_nFlagLen;					// 标识长度
	char				m_szPacketFlag[32];			// 对比信息
	Encoder*			m_Encoder;					// 编码器
};

enum IOType 
{
	IOInitialize,
	IORead,
	IOWrite,   
	IOIdle
};

typedef struct CONTEXT_OBJECT 
{
	CString  sClientInfo[10];
	SOCKET   sClientSocket;
	WSABUF   wsaInBuf;
	WSABUF	 wsaOutBuffer;  
	char     szBuffer[PACKET_LENGTH];
	CBuffer				InCompressedBuffer;	        // 接收到的压缩的数据
	CBuffer				InDeCompressedBuffer;	    // 解压后的数据
	CBuffer             OutCompressedBuffer;
	int				    v1;
	HANDLE              hDlg;
	void				*olps;						// OVERLAPPEDPLUS
	int					CompressMethod;				// 压缩算法
	HeaderParser		Parser;						// 解析数据协议
	BOOL 				bLogin;						// 是否 login

	VOID InitMember()
	{
		memset(szBuffer,0,sizeof(char)*PACKET_LENGTH);
		v1 = 0;
		hDlg = NULL;
		sClientSocket = INVALID_SOCKET;
		memset(&wsaInBuf,0,sizeof(WSABUF));
		memset(&wsaOutBuffer,0,sizeof(WSABUF));
		olps = NULL;
		CompressMethod = COMPRESS_ZSTD;
		Parser.Reset();
		bLogin = FALSE;
	}
	VOID SetClientInfo(CString s[10]){
		for (int i=0; i<sizeof(sClientInfo)/sizeof(CString);i++)
		{
			sClientInfo[i] = s[i];
		}
	}
	CString GetClientData(int index) const{
		return sClientInfo[index];
	}
	// Write compressed buffer.
	void WriteBuffer(LPBYTE data, ULONG dataLen, ULONG originLen) {
		if (Parser.IsParsed()) {
			ULONG totalLen = dataLen + Parser.GetHeaderLen();
			OutCompressedBuffer.WriteBuffer((LPBYTE)Parser.GetFlag(), Parser.GetFlagLen());
			OutCompressedBuffer.WriteBuffer((PBYTE)&totalLen, sizeof(ULONG));
			OutCompressedBuffer.WriteBuffer((PBYTE)&originLen, sizeof(ULONG));
			OutCompressedBuffer.WriteBuffer(data, dataLen);
		}
	}
	// Read compressed buffer.
	PBYTE ReadBuffer(ULONG &dataLen, ULONG &originLen) {
		if (Parser.IsParsed()) {
			ULONG totalLen = 0;
			char szPacketFlag[32] = {};
			InCompressedBuffer.ReadBuffer((PBYTE)szPacketFlag, Parser.GetFlagLen());
			InCompressedBuffer.ReadBuffer((PBYTE)&totalLen, sizeof(ULONG));
			InCompressedBuffer.ReadBuffer((PBYTE)&originLen, sizeof(ULONG));
			dataLen = totalLen - Parser.GetHeaderLen();
			PBYTE CompressedBuffer = new BYTE[dataLen];
			InCompressedBuffer.ReadBuffer(CompressedBuffer, dataLen);
			return CompressedBuffer;
		}
		return nullptr;
	}
	// Parse the data to make sure it's from a supported client. The length of `Header Flag` will be returned.
	PR Parse(CBuffer& buf) {
		return Parser.Parse(buf, CompressMethod);
	}
	// Encode data before compress.
	void Encode(PBYTE data, int len) const {
		Parser.GetEncoder()->Encode((unsigned char*)data, len);
	}
	// Decode data after uncompress.
	void Decode(PBYTE data, int len) const {
		Parser.GetEncoder()->Decode((unsigned char*)data, len);
	}
	std::string RemoteAddr() const {
		sockaddr_in  ClientAddr = {};
		int ulClientAddrLen = sizeof(sockaddr_in);
		int s = getpeername(sClientSocket, (SOCKADDR*)&ClientAddr, &ulClientAddrLen);
		return s != INVALID_SOCKET ? inet_ntoa(ClientAddr.sin_addr) : "";
	}
}CONTEXT_OBJECT,*PCONTEXT_OBJECT;

typedef CList<PCONTEXT_OBJECT> ContextObjectList;

class IOCPServer
{
public:
	SOCKET    m_sListenSocket;
	HANDLE    m_hCompletionPort;
	UINT      m_ulMaxConnections;          // 最大连接数
	HANDLE    m_hListenEvent;
	HANDLE    m_hListenThread;
	BOOL      m_bTimeToKill;
	HANDLE    m_hKillEvent;

	ULONG m_ulThreadPoolMin;
	ULONG m_ulThreadPoolMax;
	ULONG m_ulCPULowThreadsHold; 
	ULONG m_ulCPUHighThreadsHold;
	ULONG m_ulCurrentThread;
	ULONG m_ulBusyThread;

#if USING_CTX
	ZSTD_CCtx* m_Cctx; // 压缩上下文
	ZSTD_DCtx* m_Dctx; // 解压上下文
#endif

	CCpuUsage m_cpu;

	ULONG  m_ulKeepLiveTime;

	typedef void (CALLBACK *pfnNotifyProc)(CONTEXT_OBJECT* ContextObject);
	typedef void (CALLBACK *pfnOfflineProc)(CONTEXT_OBJECT* ContextObject);
	UINT StartServer(pfnNotifyProc NotifyProc, pfnOfflineProc OffProc, USHORT uPort);

	static DWORD WINAPI ListenThreadProc(LPVOID lParam);
	BOOL InitializeIOCP(VOID);
	static DWORD WINAPI WorkThreadProc(LPVOID lParam);
	ULONG   m_ulWorkThreadCount;
	VOID OnAccept();
	CRITICAL_SECTION m_cs;

	/************************************************************************/
	//上下背景文对象
	ContextObjectList				m_ContextConnectionList;
	ContextObjectList               m_ContextFreePoolList;
	PCONTEXT_OBJECT AllocateContext();
	VOID RemoveStaleContext(CONTEXT_OBJECT* ContextObject);
	VOID MoveContextToFreePoolList(CONTEXT_OBJECT* ContextObject);

	VOID PostRecv(CONTEXT_OBJECT* ContextObject);

	int AddWorkThread(int n) { 
		EnterCriticalSection(&m_cs); 
		m_ulWorkThreadCount += n;
		int ret = m_ulWorkThreadCount;
		LeaveCriticalSection(&m_cs);
		return ret;
	}

	/************************************************************************/
	//请求得到完成
	BOOL HandleIO(IOType PacketFlags,PCONTEXT_OBJECT ContextObject, DWORD dwTrans);
	BOOL OnClientInitializing(PCONTEXT_OBJECT  ContextObject, DWORD dwTrans);
	BOOL OnClientReceiving(PCONTEXT_OBJECT  ContextObject, DWORD dwTrans);  
	VOID OnClientPreSending(CONTEXT_OBJECT* ContextObject, PBYTE szBuffer , size_t ulOriginalLength);
	VOID Send(CONTEXT_OBJECT* ContextObject, PBYTE szBuffer, ULONG ulOriginalLength) {
		OnClientPreSending(ContextObject, szBuffer, ulOriginalLength);
	}
	BOOL OnClientPostSending(CONTEXT_OBJECT* ContextObject,ULONG ulCompressedLength);
	void UpdateMaxConnection(int maxConn);
	IOCPServer(void);
	~IOCPServer(void);
	void Destroy();

	pfnNotifyProc m_NotifyProc;
	pfnOfflineProc m_OfflineProc;
};

class CLock     
{
public:
	CLock(CRITICAL_SECTION& cs)
	{
		m_cs = &cs;
		Lock();
	}
	~CLock()
	{
		Unlock();
	}

	void Unlock()
	{
		LeaveCriticalSection(m_cs);
	}

	void Lock()
	{
		EnterCriticalSection(m_cs);
	}

protected:
	CRITICAL_SECTION*	m_cs;
};

#define TRACK_OVERLAPPEDPLUS 0

class OVERLAPPEDPLUS   
{
public:

	OVERLAPPED			m_ol;    
	IOType				m_ioType;

	OVERLAPPEDPLUS(IOType ioType)
	{
#if TRACK_OVERLAPPEDPLUS
		char szLog[100];
		sprintf_s(szLog, "=> [new] OVERLAPPEDPLUS %p by thread [%d].\n", this, GetCurrentThreadId());
		Mprintf(szLog);
#endif
		ZeroMemory(this, sizeof(OVERLAPPEDPLUS));
		m_ioType = ioType;
	}

	~OVERLAPPEDPLUS()
	{
#if TRACK_OVERLAPPEDPLUS
		char szLog[100];
		sprintf_s(szLog, "=> [delete] OVERLAPPEDPLUS %p by thread [%d].\n", this, GetCurrentThreadId());
		Mprintf(szLog);
#endif
	}
};

typedef IOCPServer CIOCPServer;

typedef CONTEXT_OBJECT ClientContext;

#define m_Socket sClientSocket
#define m_DeCompressionBuffer InDeCompressedBuffer
