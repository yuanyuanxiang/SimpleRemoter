#include "stdafx.h"
#include "IOCPKCPServer.h"
#include "IOCPServer.h"

IUINT32 IOCPKCPServer::iclock()
{
    static LARGE_INTEGER freq = {};
    static BOOL useQpc = QueryPerformanceFrequency(&freq);
    if (useQpc) {
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        return (IUINT32)(1000 * now.QuadPart / freq.QuadPart);
    } else {
        return GetTickCount();
    }
}


CONTEXT_KCP* IOCPKCPServer::FindOrCreateClient(const sockaddr_in& addr, SOCKET sClientSocket)
{
    char buf[64];
    sprintf_s(buf, "%s:%d", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
    std::string key = buf;
    std::lock_guard<std::mutex> lock(m_contextsMutex);

    auto it = m_clients.find(key);
    if (it != m_clients.end()) {
        return it->second;
    }

    // 新建 CONTEXT_KCP
    CONTEXT_KCP* ctx = new CONTEXT_KCP();
    ctx->InitMember(sClientSocket, this);
    ctx->clientAddr = addr;

    // 初始化 kcp
    IUINT32 conv = KCP_SESSION_ID;
    ctx->kcp = ikcp_create(conv, ctx);

    ctx->kcp->output = [](const char* buf, int len, ikcpcb* kcp, void* user) -> int {
        CONTEXT_KCP* c = (CONTEXT_KCP*)user;
        WSABUF wsaBuf = { len, (CHAR*)buf };
        DWORD sent = 0;
        // 根据ctx存储的IP端口发送
        // 注意：要保证 ctx 对应客户端地址，且 sClientSocket 正确
        int ret = WSASendTo(c->sClientSocket, &wsaBuf, 1, &sent, 0,
                            (sockaddr*)&c->clientAddr, c->addrLen, NULL, NULL);
        if (ret == SOCKET_ERROR)
        {
            DWORD err = WSAGetLastError();
            // 可以打印错误日志
            return -1;
        }
        return 0;
    };

    ikcp_nodelay(ctx->kcp, 1, 10, 2, 1);
    ikcp_wndsize(ctx->kcp, 128, 128);

    m_clients[key] = ctx;

    return ctx;
}

UINT IOCPKCPServer::StartServer(pfnNotifyProc NotifyProc, pfnOfflineProc OffProc, USHORT uPort)
{
    if (m_running) return 1;

    m_port = uPort;
    m_notify = NotifyProc;
    m_offline = OffProc;

    m_socket = WSASocket(AF_INET, SOCK_DGRAM, IPPROTO_UDP, NULL, 0, WSA_FLAG_OVERLAPPED);
    if (m_socket == INVALID_SOCKET) return 2;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(uPort);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(m_socket, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) return 3;

    m_hIOCP = CreateIoCompletionPort((HANDLE)m_socket, NULL, 0, 0);
    if (!m_hIOCP) return 4;

    m_running = true;

    // 启动IOCP工作线程
    m_hThread = CreateThread(NULL, 0, [](LPVOID param) -> DWORD {
        ((IOCPKCPServer*)param)->WorkerThread();
        return 0;
    }, this, 0, NULL);

    // 启动KCP定时更新线程
    m_kcpUpdateThread = std::thread(&IOCPKCPServer::KCPUpdateLoop, this);

    Mprintf("IOCPKCPServer StartServer: %p\n", this);

    return 0;
}

void IOCPKCPServer::WorkerThread()
{
    char buf[1500];
    sockaddr_in clientAddr;
    int addrLen = sizeof(clientAddr);

    while (m_running) {
        int ret = recvfrom(m_socket, buf, sizeof(buf), 0, (sockaddr*)&clientAddr, &addrLen);
        if (ret > 0) {
            CONTEXT_KCP* ctx = FindOrCreateClient(clientAddr, m_socket);
            if (ctx && ctx->kcp) {
                {
                    std::lock_guard<std::mutex> lock(m_contextsMutex);
                    ikcp_input(ctx->kcp, buf, ret);
                }

                char recvbuf[4096];
                int n = 0;
                do {
                    {
                        std::lock_guard<std::mutex> lock(m_contextsMutex);
                        n = ikcp_recv(ctx->kcp, recvbuf, sizeof(recvbuf));
                    }
                    if (n > 0&& m_notify) {
                        memcpy(ctx->szBuffer, recvbuf, n);
                        BOOL ret = ParseReceivedData(ctx, n, m_notify);
                    }
                } while (n>0);
            }
        } else {
            DWORD err = WSAGetLastError();
            if (err != WSAEWOULDBLOCK && err != WSAEINTR) {
                // 打印错误或做其他处理
            }
        }
    }
    Mprintf("IOCPKCPServer WorkerThread DONE: %p\n", this);
}

void IOCPKCPServer::KCPUpdateLoop()
{
    while (m_running) {
        IUINT32 current = iclock();

        std::lock_guard<std::mutex> lock(m_contextsMutex);
        for (auto& kv : m_clients) {
            CONTEXT_KCP* ctx = kv.second;
            if (ctx && ctx->kcp) {
                ikcp_update(ctx->kcp, current);
            }
        }

        Sleep(10);
    }
}

void IOCPKCPServer::Send2Client(CONTEXT_OBJECT* ContextObject, PBYTE szBuffer, ULONG ulOriginalLength)
{
    if (!ContextObject || !ContextObject->kcp) return;
    ContextObject->OutCompressedBuffer.ClearBuffer();
    if (!WriteContextData(ContextObject, szBuffer, ulOriginalLength))
        return;
    {
        std::lock_guard<std::mutex> lock(m_contextsMutex);

        ikcp_send(ContextObject->kcp,
                  (const char*)ContextObject->OutCompressedBuffer.GetBuffer(),
                  (int)ContextObject->OutCompressedBuffer.GetBufferLength());
        ikcp_flush(ContextObject->kcp);
    }
}

void IOCPKCPServer::Destroy()
{
    Mprintf("IOCPKCPServer Destroy: %p\n", this);

    m_running = false;

    if (m_socket != INVALID_SOCKET) {
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
    }

    if (m_hThread) {
        WaitForSingleObject(m_hThread, INFINITE);
        CloseHandle(m_hThread);
        m_hThread = NULL;
    }

    if (m_kcpUpdateThread.joinable())
        m_kcpUpdateThread.join();

    if (m_hIOCP) {
        CloseHandle(m_hIOCP);
        m_hIOCP = NULL;
    }

    // 清理所有客户端
    std::lock_guard<std::mutex> lock(m_contextsMutex);
    for (auto& kv : m_clients) {
        if (kv.second) {
            if (kv.second->kcp) {
                ikcp_release(kv.second->kcp);
                kv.second->kcp = nullptr;
            }
            delete kv.second;
        }
    }
    m_clients.clear();
}

void IOCPKCPServer::Disconnect(CONTEXT_OBJECT* ctx)
{
    if (!ctx) return;

    std::string key = ctx->PeerName;
    {
        std::lock_guard<std::mutex> lock(m_contextsMutex);
        auto it = m_clients.find(key);
        if (it != m_clients.end()) {
            if (it->second == ctx) {
                if (m_offline) m_offline(ctx);
                if (ctx->kcp) {
                    ikcp_release(ctx->kcp);
                    ctx->kcp = nullptr;
                }
                delete ctx;
                m_clients.erase(it);
            }
        }
    }
}
