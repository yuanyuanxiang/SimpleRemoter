#pragma once

#include "Server.h"

class CONTEXT_KCP : public CONTEXT_OBJECT
{
public:
    int					addrLen = 0;
    sockaddr_in			clientAddr = {};
    CONTEXT_KCP()
    {
    }
    virtual ~CONTEXT_KCP()
    {
    }
    std::string GetProtocol() const override
    {
        return "KCP";
    }
    VOID InitMember(SOCKET s, Server* svr) override
    {
        CONTEXT_OBJECT::InitMember(s, svr);
        clientAddr = {};
        addrLen = sizeof(sockaddr_in);
    }
    void Destroy() override
    {
    }
    virtual std::string GetPeerName() const override
    {
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientAddr.sin_addr, client_ip, INET_ADDRSTRLEN);
        return client_ip;
    }
    virtual int GetPort() const override
    {
        int client_port = ntohs(clientAddr.sin_port);
        return client_port;
    }
};

class IOCPKCPServer : public Server
{
public:
    IOCPKCPServer() {}
    virtual ~IOCPKCPServer() {}

    virtual int GetPort() const override
    {
        return m_port;
    }
    virtual UINT StartServer(pfnNotifyProc NotifyProc, pfnOfflineProc OffProc, USHORT uPort) override;
    virtual BOOL Send2Client(CONTEXT_OBJECT* ContextObject, PBYTE szBuffer, ULONG ulOriginalLength) override;
    virtual void Destroy() override;
    virtual void Disconnect(CONTEXT_OBJECT* ctx) override;

private:
    SOCKET m_socket = INVALID_SOCKET;
    HANDLE m_hIOCP = NULL;
    HANDLE m_hThread = NULL;
    bool m_running = false;
    USHORT m_port = 0;

    pfnNotifyProc m_notify = nullptr;
    pfnOfflineProc m_offline = nullptr;

    std::mutex m_contextsMutex;
    std::unordered_map<std::string, CONTEXT_KCP*> m_clients; // key: "IP:port"

    std::thread m_kcpUpdateThread;

    CONTEXT_KCP* FindOrCreateClient(const sockaddr_in& addr, SOCKET sClientSocket);

    void WorkerThread();

    void KCPUpdateLoop();

    static IUINT32 iclock();
};
