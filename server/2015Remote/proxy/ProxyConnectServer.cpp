#include "stdafx.h"
#include "ProxyConnectServer.h"

#define MAX_SEND_BUFFER		65535	// 最大发送数据长度 1024*64
#define	MAX_RECV_BUFFER		65535   // 最大接收数据长度

CProxyConnectServer::CProxyConnectServer(void) :m_TcpServer(this)
{
    Mprintf("CProxyConnectServer\r\n");
    m_bIsRun = TRUE;
    m_dwIndex = 0;
    memset(m_IDs, 0, sizeof(m_IDs));
}


CProxyConnectServer::~CProxyConnectServer(void)
{
    if (m_TcpServer->GetState() != SS_STOPPED)
        m_TcpServer->Stop();
    while (m_TcpServer->GetState() != SS_STOPPED) {
        Sleep(300);
    }
    Mprintf("~CProxyConnectServer\r\n");
}

BOOL CProxyConnectServer::Initialize(NOTIFYPROC pNotifyProc, void*user, int nMaxConnections, int nPort)
{
    m_nMaxConnection = nMaxConnections;
    m_TcpServer->SetMaxConnectionCount(nMaxConnections);
    m_TcpServer->SetSendPolicy(SP_DIRECT);
    m_TcpServer->SetNoDelay(TRUE);
    m_pNotifyProc = pNotifyProc;
    m_pUser = user;
    m_nPort = nPort;
    m_bStop = FALSE;
    return m_TcpServer->Start(_T("0.0.0.0"), nPort);
}

EnHandleResult CProxyConnectServer::OnPrepareListen(ITcpServer* pSender, SOCKET soListen)
{
    SYS_SSO_SendBuffSize(soListen, MAX_SEND_BUFFER);
    SYS_SSO_RecvBuffSize(soListen, MAX_RECV_BUFFER);
    return HR_OK;
}

EnHandleResult CProxyConnectServer::OnAccept(ITcpServer* pSender, CONNID dwConnID, UINT_PTR soClient)
{
    if (!m_bIsRun)return HR_ERROR;

    ClientContext* pContext = NULL;
	{
		m_Locker.lock();
		if (!m_listFreePool.IsEmpty()) {
			pContext = m_listFreePool.RemoveHead();
		}
		else {
			pContext = new(std::nothrow) ClientContext;
		}
		m_Locker.unlock();
	}
    if (pContext == NULL)
        return HR_ERROR;
    
    pContext->InitMember();
    pContext->m_Socket = dwConnID;
    char szAddress[64] = {};
    int iAddressLen = sizeof(szAddress);
    USHORT usPort = 0;
    pSender->GetRemoteAddress(dwConnID, szAddress, iAddressLen, usPort);
    Mprintf("CProxyConnectServer: new connection %s:%d\n", szAddress, usPort);
    pContext->ID = dwConnID;
    m_TcpServer->SetConnectionExtra(dwConnID, pContext);
    m_pNotifyProc(m_pUser, pContext, NC_CLIENT_CONNECT);
    return HR_OK;
}

EnHandleResult CProxyConnectServer::OnSend(ITcpServer* pSender, CONNID dwConnID, const BYTE* pData, int iLength)
{
    return HR_OK;
}

EnHandleResult CProxyConnectServer::OnReceive(ITcpServer* pSender, CONNID dwConnID, int iLength)
{
    ClientContext* pContext = NULL;
    if ((!m_TcpServer->GetConnectionExtra(dwConnID, (PVOID*)&pContext)) && (pContext != nullptr) && (iLength <= 0))
        return HR_ERROR;

    PBYTE pData = new BYTE[iLength];
    m_TcpServer->Fetch(dwConnID, pData, iLength);
    pContext->InDeCompressedBuffer.ClearBuffer();
    BYTE bToken = COMMAND_PROXY_DATA;
    pContext->InDeCompressedBuffer.Write(&bToken, sizeof(bToken));
    pContext->InDeCompressedBuffer.Write((LPBYTE)&pContext->ID, sizeof(DWORD));
    pContext->InDeCompressedBuffer.Write((PBYTE)pData, iLength);
    SAFE_DELETE_ARRAY(pData);
    m_pNotifyProc(m_pUser, pContext, NC_RECEIVE);
    return HR_OK;
}

EnHandleResult CProxyConnectServer::OnClose(ITcpServer* pSender, CONNID dwConnID, EnSocketOperation enOperation, int iErrorCode)
{
    ClientContext* pContext = NULL;
    if (m_TcpServer->GetConnectionExtra(dwConnID, (PVOID*)&pContext) && pContext != nullptr)
        m_TcpServer->SetConnectionExtra(dwConnID, NULL);
    if (!pContext)
        return HR_OK;

    m_pNotifyProc(m_pUser, pContext, NC_CLIENT_DISCONNECT);
    pContext->InCompressedBuffer.ClearBuffer();
    pContext->InDeCompressedBuffer.ClearBuffer();
    pContext->OutCompressedBuffer.ClearBuffer();
    m_Locker.lock();
    m_listFreePool.AddTail(pContext);
    m_Locker.unlock();
    return HR_OK;
}

EnHandleResult CProxyConnectServer::OnShutdown(ITcpServer* pSender)
{
    return HR_OK;
}


BOOL CProxyConnectServer::Send(ClientContext* pContext, LPBYTE lpData, UINT nSize)
{
    if (pContext == NULL)
        return FALSE;

    BOOL rt = FALSE;;
    if (nSize > 0 && m_bIsRun) {
        pContext->OutCompressedBuffer.Write(lpData, nSize);
        rt = SendWithSplit(pContext->m_Socket, pContext->OutCompressedBuffer.GetBuffer(0), 
            pContext->OutCompressedBuffer.GetBufferLength(), MAX_SEND_BUFFER);
        pContext->OutCompressedBuffer.ClearBuffer();
    }
    return rt;
}


BOOL CProxyConnectServer::SendWithSplit(CONNID dwConnID, LPBYTE lpData, UINT nSize, UINT nSplitSize)
{
    int nSend = 0;
    UINT nSendRetry = 0;
    BOOL rt = TRUE;
    if (nSize >= nSplitSize) {
        UINT i = 0;
        nSendRetry = nSize / nSplitSize;
        for (i = 0; i < nSendRetry; i++) {
            rt = m_TcpServer->Send(dwConnID, lpData, nSplitSize);
            if (!rt)
                return rt;
            lpData += nSplitSize;
            nSend += nSplitSize;
        }
        if (nSize - nSend < nSplitSize) {
            if (nSize - nSend > 0) {
                rt = m_TcpServer->Send(dwConnID, lpData, nSize - nSend);
                if (!rt)
                    return rt;
            }
        }
    } else {
        rt = m_TcpServer->Send(dwConnID, lpData, nSize);
        if (!rt)
            return rt;
    }
    return  TRUE;
}


void CProxyConnectServer::Shutdown()
{
    DWORD dwCount = 65535;
    CONNID *pIDs = new CONNID[dwCount]();
    BOOL status = m_TcpServer->GetAllConnectionIDs(pIDs, dwCount);
    if (status && (dwCount > 0)) {
        for (DWORD i = 0; i < dwCount; i++) {
            Disconnect(pIDs[i]);
        }
    }
    m_TcpServer->Stop();
    m_bIsRun = FALSE;
    while (m_TcpServer->GetState() != SS_STOPPED)
        Sleep(10);

    m_Locker.lock();
    while (!m_listFreePool.IsEmpty())
        delete m_listFreePool.RemoveTail();
    m_Locker.unlock();

    SAFE_DELETE_ARRAY(pIDs);
}

void CProxyConnectServer::ClearClient()
{
	DWORD dwCount = 65535;
	CONNID* pIDs = new CONNID[dwCount]();
    BOOL status = m_TcpServer->GetAllConnectionIDs(pIDs, dwCount);
    if (status && (dwCount > 0)) {
        for (DWORD i = 0; i < dwCount; i++) {
            m_TcpServer->Disconnect(pIDs[i]);
        }
    }

	SAFE_DELETE_ARRAY(pIDs);
}

BOOL CProxyConnectServer::Disconnect(CONNID dwConnID)
{
    m_TcpServer->Disconnect(dwConnID);
    return 0;
}

BOOL CProxyConnectServer::IsConnected(CONNID dwConnID)
{
    return	m_TcpServer->IsConnected(dwConnID);
}

BOOL CProxyConnectServer::IsOverMaxConnectionCount()
{
    return (m_TcpServer->GetConnectionCount() > (DWORD)m_nMaxConnection);
}
