// IOCPClient.h: interface for the IOCPClient class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#ifdef _WIN32
#include "stdafx.h"
#include <WinSock2.h>
#include <MSTcpIP.h>
#pragma comment(lib,"ws2_32.lib")
#endif

#include "Buffer.h"
#include "zstd/zstd.h"
#include "domain_pool.h"
#include "common/mask.h"
#include "common/header.h"
#define NO_AES
#include "common/encrypt.h"
#include "SafeThread.h"
#include "IOCPBase.h"

#define MAX_RECV_BUFFER  1024*32
#define MAX_SEND_BUFFER  1024*32

enum { S_STOP = 0, S_RUN, S_END };

typedef int (*DataProcessCB)(void* userData, PBYTE szBuffer, ULONG ulLength);


class ProtocolEncoder
{
public:
    virtual ~ProtocolEncoder() {}
    virtual HeaderFlag GetHead() const
    {
        return "Shine";
    }
    virtual int GetHeadLen() const
    {
        return 13;
    }
    virtual int GetFlagLen() const
    {
        return 5;
    }
    virtual void Encode(unsigned char* data, int len, unsigned char* param = 0) {}
    virtual void Decode(unsigned char* data, int len, unsigned char* param = 0) {}
    virtual EncFun GetHeaderEncoder() const
    {
        return nullptr;
    }
};

class HellEncoder : public ProtocolEncoder
{
private:
    EncFun m_HeaderEnc;
    Encoder *m_BodyEnc;
public:
    HellEncoder(EncFun head, Encoder *body)
    {
        m_HeaderEnc = head;
        m_BodyEnc = body;
    }
    ~HellEncoder()
    {
        SAFE_DELETE(m_BodyEnc);
    }
    virtual HeaderFlag GetHead() const override
    {
        return ::GetHead(m_HeaderEnc);
    }
    virtual int GetHeadLen() const override
    {
        return 16;
    }
    virtual int GetFlagLen() const override
    {
        return 8;
    }
    virtual void Encode(unsigned char* data, int len, unsigned char* param = 0) override
    {
        return m_BodyEnc->Encode(data, len, param);
    }
    virtual void Decode(unsigned char* data, int len, unsigned char* param = 0) override
    {
        return m_BodyEnc->Decode(data, len, param);
    }
    virtual EncFun GetHeaderEncoder() const override
    {
        return m_HeaderEnc;
    }
};

class IOCPManager
{
public:
    virtual ~IOCPManager() {}
    virtual BOOL IsAlive() const
    {
        return TRUE;
    }
    virtual BOOL IsReady() const
    {
        return TRUE;
    }
    virtual VOID OnReceive(PBYTE szBuffer, ULONG ulLength) { }

    static int DataProcess(void* user, PBYTE szBuffer, ULONG ulLength)
    {
        IOCPManager* m_Manager = (IOCPManager*)user;
        if (nullptr == m_Manager) {
            Mprintf("IOCPManager DataProcess on NULL ptr: %d\n", unsigned(szBuffer[0]));
            return FALSE;
        }
        // 等待子类准备就绪才能处理数据, 1秒足够了
        int i = 0;
        for (; i < 1000 && !m_Manager->IsReady(); ++i)
            Sleep(1);
        if (!m_Manager->IsReady()) {
            Mprintf("IOCPManager DataProcess is NOT ready: %d\n", unsigned(szBuffer[0]));
            return FALSE;
        }
        if (i) {
            Mprintf("IOCPManager DataProcess wait for %dms: %d\n", i, unsigned(szBuffer[0]));
        }
        m_Manager->OnReceive(szBuffer, ulLength);
        return TRUE;
    }
};

typedef BOOL(*TrailCheck)(void);

class IOCPClient : public IOCPBase
{
public:
    IOCPClient(const State& bExit, bool exit_while_disconnect = false, int mask=0, int encoder=0,
               const std::string&pubIP="");
    virtual ~IOCPClient();

    int SendLoginInfo(const LOGIN_INFOR& logInfo)
    {
        LOGIN_INFOR tmp = logInfo;
        int iRet = Send2Server((char*)&tmp, sizeof(LOGIN_INFOR));

        return iRet;
    }
    virtual BOOL ConnectServer(const char* szServerIP, unsigned short uPort);

    std::string GetClientIP() const
    {
        return m_sLocPublicIP;
    }

    std::map<std::string, std::string> GetClientIPHeader() const
    {
        return m_sLocPublicIP.empty() ? std::map<std::string, std::string> {} :
        std::map<std::string, std::string> { {"X-Forwarded-For", m_sLocPublicIP} };
    }

    BOOL Send2Server(const char* szBuffer, ULONG ulOriginalLength, PkgMask* mask = NULL)
    {
        return OnServerSending(szBuffer, ulOriginalLength, mask);
    }

    void SetServerAddress(const char* szServerIP, unsigned short uPort)
    {
        m_Domain = szServerIP ? szServerIP : "127.0.0.1";
        m_nHostPort = uPort;
    }

    std::string ServerIP() const
    {
        return m_sCurIP;
    }

    int ServerPort() const
    {
        return m_nHostPort;
    }

    BOOL IsRunning() const
    {
        return m_bIsRunning;
    }

    void SetExit()
    {
        m_bIsRunning = FALSE;
    }

    VOID setManagerCallBack(void* Manager, DataProcessCB dataProcess);
    VOID RunEventLoop(TrailCheck checker);
    VOID RunEventLoop(const BOOL &bCondition);
    bool IsConnected() const
    {
        return m_bConnected == TRUE;
    }
    BOOL Reconnect(void* manager)
    {
        Disconnect();
        if (manager) m_Manager = manager;
        return ConnectServer(NULL, 0);
    }
    const State& GetState() const
    {
        return g_bExit;
    }
    void SetMultiThreadCompress(int threadNum=0);
protected:
    virtual int ReceiveData(char* buffer, int bufSize, int flags)
    {
        // TCP版本调用 recv
        return recv(m_sClientSocket, buffer, bufSize - 1, 0);
    }
    virtual bool ProcessRecvData(CBuffer* m_CompressedBuffer, char* szBuffer, int len, int flag);
    virtual VOID Disconnect(); // 函数支持 TCP/UDP
    virtual int SendTo(const char* buf, int len, int flags)
    {
        return ::send(m_sClientSocket, buf, len, flags);
    }
    BOOL OnServerSending(const char* szBuffer, ULONG ulOriginalLength, PkgMask* mask);
    static DWORD WINAPI WorkThreadProc(LPVOID lParam);
    VOID OnServerReceiving(CBuffer *m_CompressedBuffer, char* szBuffer, ULONG ulReceivedLength);
    BOOL SendWithSplit(const char* src, ULONG srcSize, ULONG ulSplitLength, int cmd, PkgMask* mask);

protected:
    sockaddr_in			m_ServerAddr;
    SOCKET				m_sClientSocket;
    BOOL				m_bWorkThread;
    HANDLE				m_hWorkThread;
    BOOL				m_bIsRunning;
    BOOL				m_bConnected;

    CLock               m_Locker;
#if USING_CTX
    ZSTD_CCtx*			m_Cctx;						// 压缩上下文
    ZSTD_DCtx*			m_Dctx;						// 解压上下文
#endif

    const State&		g_bExit;					// 全局状态量
    void*				m_Manager;					// 用户数据
    DataProcessCB		m_DataProcess;				// 处理用户数据
    ProtocolEncoder*	m_Encoder;					// 加密
    DomainPool			m_Domain;
    std::string			m_sCurIP;
    int					m_nHostPort;
    bool				m_exit_while_disconnect;
    PkgMask*			m_masker;
    BOOL				m_EncoderType;
    std::string			m_sLocPublicIP;
};
