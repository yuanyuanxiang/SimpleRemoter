#pragma once
#include "HPSocket.h"
#include "SocketInterface.h"
#include "Buffer.h"
#include <IOCPServer.h>

#define	NC_CLIENT_CONNECT		0x0001
#define	NC_CLIENT_DISCONNECT	0x0002
#define	NC_TRANSMIT				0x0003
#define	NC_RECEIVE				0x0004

typedef void (CALLBACK* NOTIFYPROC)(void* user, ClientContext* ctx, UINT nCode);

typedef CList<ClientContext*, ClientContext* > ContextList;

class CProxyConnectServer :public CTcpPullServerListener
{
public:
    CProxyConnectServer(void);
    ~CProxyConnectServer(void);

    BOOL Initialize(NOTIFYPROC pNotifyProc, void* user, int nMaxConnections, int nPort);
    BOOL Send(ClientContext* pContext, LPBYTE lpData, UINT nSize);
    BOOL SendWithSplit(CONNID dwConnID, LPBYTE lpData, UINT nSize, UINT nSplitSize);
    void Shutdown();
    void ClearClient();
    BOOL Disconnect(CONNID dwConnID);
    BOOL IsConnected(CONNID dwConnID);
    int IsOverMaxConnectionCount();

    virtual EnHandleResult OnPrepareListen(ITcpServer* pSender, SOCKET soListen);
    virtual EnHandleResult OnAccept(ITcpServer* pSender, CONNID dwConnID, UINT_PTR soClient);
    virtual EnHandleResult OnSend(ITcpServer* pSender, CONNID dwConnID, const BYTE* pData, int iLength);
    virtual EnHandleResult OnReceive(ITcpServer* pSender, CONNID dwConnID, int iLength);
    virtual EnHandleResult OnClose(ITcpServer* pSender, CONNID dwConnID, EnSocketOperation enOperation, int iErrorCode);
    virtual EnHandleResult OnShutdown(ITcpServer* pSender);

    CTcpPullServerPtr       m_TcpServer;

private:
    NOTIFYPROC		        m_pNotifyProc;
    void*                   m_pUser;
    ContextList             m_listFreePool;
    CLock                   m_Locker;
    int                     m_nPort;			    // 插件端口
    CONNID                  m_IDs[65535];           // 所有连接ID
    LONG                    m_bStop;		        // 端口停止上线控制
    int                     m_nMaxConnection;	    // 最大连接数
    BOOL                    m_bIsRun;               // 运行状态
    DWORD					m_dwIndex;              // 连接编号
};
