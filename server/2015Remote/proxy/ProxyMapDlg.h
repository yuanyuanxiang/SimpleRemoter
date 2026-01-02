#pragma once
#include "stdafx.h"
#include "ProxyConnectServer.h"
#include "Resource.h"

#ifdef _WIN64
#ifdef _DEBUG
#pragma comment(lib, "HPSocket_x64D.lib")
#else
#pragma comment(lib, "HPSocket_x64.lib")
#endif

#else
#ifdef _DEBUG
#pragma comment(lib, "HPSocket_D.lib")
#else
#pragma comment(lib, "HPSocket.lib")
#endif

#endif

/////////////////////////////////////////////////////////////////////////////
// CProxyMapDlg dialog
typedef struct {
    BYTE Ver;      // Version Number
    BYTE CMD;      // 0x01==TCP CONNECT,0x02==TCP BIND,0x03==UDP ASSOCIATE
    BYTE RSV;
    BYTE ATYP;
    BYTE IP_LEN;
    BYTE szIP;
} Socks5Info;

// 代理测试: curl --socks5 127.0.0.1:5543 https://www.example.com
class CProxyMapDlg : public DialogBase
{
public:
    CProxyMapDlg(CWnd* pParent = NULL, Server* pIOCPServer = NULL, ClientContext* pContext = NULL);

    enum { IDD = IDD_PROXY };

    static void CALLBACK NotifyProc(void* user, ClientContext* pContext, UINT nCode);

    void OnReceiveComplete();
    void OnReceive();
    void AddLog(TCHAR* lpText);
    void AddLog_other(TCHAR* lpText);
    virtual void DoDataExchange(CDataExchange* pDX);
    virtual void OnCancel();
    afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
    virtual BOOL OnInitDialog();
    afx_msg void OnSize(UINT nType, int cx, int cy);
    DECLARE_MESSAGE_MAP()

private:
    CProxyConnectServer*    m_iocpLocal;
    CEdit	                m_Edit;
    USHORT                  m_nPort;
    CEdit                   m_EditOther;
};
