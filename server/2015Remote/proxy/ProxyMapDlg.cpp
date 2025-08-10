// ProxyMapDlg.cpp : implementation file
//

#include "stdafx.h"
#include "2015Remote.h"
#include "ProxyMapDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

/////////////////////////////////////////////////////////////////////////////
// CProxyMapDlg dialog

#define IDM_PROXY_CHROME      8000

CProxyMapDlg::CProxyMapDlg(CWnd* pParent, Server* pIOCPServer, ClientContext* pContext)
    : CDialogBase(CProxyMapDlg::IDD, pParent, pIOCPServer, pContext, IDI_Proxifier)
{
    m_iocpLocal = NULL;
}

void CProxyMapDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_EDIT, m_Edit);
    DDX_Control(pDX, IDC_EDIT_OTHER, m_EditOther);
}

BEGIN_MESSAGE_MAP(CProxyMapDlg, CDialog)
    ON_WM_SYSCOMMAND()
    ON_WM_SIZE()
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////

BOOL CProxyMapDlg::OnInitDialog()
{
    CDialog::OnInitDialog();

    SetIcon(m_hIcon, TRUE);			// Set big icon
    SetIcon(m_hIcon, FALSE);		// Set small icon
    // TODO: Add extra initialization here
    m_iocpLocal = new CProxyConnectServer;

    if (m_iocpLocal == NULL) {
        return FALSE;
    }

    m_Edit.SetLimitText(MAXDWORD);
    m_EditOther.SetLimitText(MAXDWORD);
    CString		str;

    // 开启IPCP服务器
    m_nPort = 5543;
	if (!m_iocpLocal->Initialize(NotifyProc, this, 100000, m_nPort)) {
		MessageBox("初始化代理服务器失败!", "提示");
        return FALSE;
	}
    TCHAR ip[256] = {};
    int len = sizeof(ip);
    m_iocpLocal->m_TcpServer->GetListenAddress(ip, len, m_nPort);

	CString strString;
	strString.Format("%s - 代理服务", m_IPAddress);
	SetWindowText(strString);

    str.Format(_T("SOCKS 代理软件请设置服务器为: <127.0.0.1:%d>\r\n"), m_nPort);
    AddLog(str.GetBuffer(0));

    CMenu* pSysMenu = GetSystemMenu(FALSE);
    if (pSysMenu != NULL) {
        pSysMenu->AppendMenu(MF_SEPARATOR);
        pSysMenu->AppendMenu(MF_STRING, IDM_PROXY_CHROME, _T("代理打开Chrome(请关闭所有Chrome进程)(&P)"));
    }

    return TRUE;
}

void CProxyMapDlg::OnCancel()
{
    m_bIsClosed = true;
	// 等待数据处理完毕
	if (IsProcessing()) {
		ShowWindow(SW_HIDE);
		return;
	}
	m_iocpLocal->Shutdown();
	SAFE_DELETE(m_iocpLocal);

    CancelIO();

    DialogBase::OnClose();
}

void CALLBACK CProxyMapDlg::NotifyProc(void *user, ClientContext* pContext, UINT nCode)
{
    CProxyMapDlg* g_pProxyMap = (CProxyMapDlg*)user;
    if (g_pProxyMap->m_bIsClosed) return;

    DWORD index = pContext->ID;
    TCHAR szMsg[200] = { 0 };
    try {
        switch (nCode) {
        case NC_CLIENT_CONNECT:
            wsprintf(szMsg, _T("%d 新连接\r\n"), index);
            break;
        case NC_CLIENT_DISCONNECT:
            if (pContext->m_bProxyConnected) {
                BYTE lpData[5] = "";
                lpData[0] = COMMAND_PROXY_CLOSE;
                memcpy(lpData + 1, &index, sizeof(DWORD));
                g_pProxyMap->m_ContextObject->Send2Client(lpData, 5);
            }
            wsprintf(szMsg, _T("%d 本地连接断开\r\n"), index);
            break;
        case NC_TRANSMIT:
            break;
        case NC_RECEIVE:
            if (pContext->m_bProxyConnected == 2) {
                g_pProxyMap->m_ContextObject->Send2Client(pContext->InDeCompressedBuffer.GetBuffer(0),
                                                pContext->InDeCompressedBuffer.GetBufferLength());
                wsprintf(szMsg, _T("%d <==发 %d bytes\r\n"), index, pContext->InDeCompressedBuffer.GetBufferLength() - 5);
            } else if (pContext->m_bProxyConnected == 0) {
                char msg_auth_ok[] = { 0X05, 0X00 }; // VERSION SOCKS, AUTH MODE, OK
                LPBYTE lpData = pContext->InDeCompressedBuffer.GetBuffer(5);
                pContext->m_bProxyConnected = 1;
                g_pProxyMap->m_iocpLocal->Send(pContext, (LPBYTE)msg_auth_ok, sizeof(msg_auth_ok));
                wsprintf(szMsg, _T("%d 返回标示 %d %d %d\r\n"), index, lpData[0], lpData[1], lpData[2]);
            } else if (pContext->m_bProxyConnected == 1) {
                LPBYTE lpData = pContext->InDeCompressedBuffer.GetBuffer(5);
                BYTE buf[11] = {};
                if (lpData[0] == 5 && lpData[1] == 1 && (pContext->InDeCompressedBuffer.GetBufferLength() > 10)) {
                    if (lpData[3] == 1) { // ipv4
                        buf[0] = COMMAND_PROXY_CONNECT; // 1个字节 ip v4 连接
                        memcpy(buf + 1, &index, 4);		 // 四个字节 套接字的编号
                        memcpy(buf + 5, lpData + 4, 6);	 // 4字节ip 2字节端口
                        g_pProxyMap->m_ContextObject->Send2Client(buf, sizeof(buf));
                        in_addr inaddr = {};
                        inaddr.s_addr = *(DWORD*)(buf + 5);
                        char szmsg1[MAX_PATH];
                        wsprintfA(szmsg1, "%d IPV4 连接 %s:%d...\r\n", index, inet_ntoa(inaddr), ntohs(*(USHORT*)(buf + 9)));
                    } else if (lpData[3] == 3) { // 域名
                        Socks5Info* Socks5Request = (Socks5Info*)lpData;
                        BYTE* HostName = new BYTE[Socks5Request->IP_LEN + 8];
                        ZeroMemory(HostName, Socks5Request->IP_LEN + 8);
                        HostName[0] = COMMAND_PROXY_CONNECT_HOSTNAME;
                        memcpy(HostName + 7, &Socks5Request->szIP, Socks5Request->IP_LEN);
                        memcpy(HostName + 1, &index, 4);
                        memcpy(HostName + 5, &Socks5Request->szIP + Socks5Request->IP_LEN, 2);
                        g_pProxyMap->m_ContextObject->Send2Client(HostName, Socks5Request->IP_LEN + 8);
                        SAFE_DELETE_ARRAY(HostName);
                        wsprintf(szMsg, _T("域名 连接 %d \r\n"), index);
                    } else if (lpData[3] == 4) { //ipv6
                        char msg_ipv6_nok[] = { 0X05, 0X08, 0X00, 0X01, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00 }; // IPv6 not support
                        wsprintf(szMsg, _T("%d IPV6连接 不支持..."), index);
                        g_pProxyMap->m_iocpLocal->Send(pContext, (LPBYTE)msg_ipv6_nok, sizeof(msg_ipv6_nok));
                        g_pProxyMap->m_iocpLocal->Disconnect(pContext->m_Socket);
                        break;
                    }
                } else {
                    buf[0] = 5;
                    buf[1] = 7;
                    buf[2] = 0;
                    buf[3] = lpData[3];
                    g_pProxyMap->m_iocpLocal->Send(pContext, buf, sizeof(buf));
                    g_pProxyMap->m_iocpLocal->Disconnect(pContext->m_Socket);
                    wsprintf(szMsg, _T("%d 不符要求, 断开 %d %d %d\r\n"), index, lpData[0], lpData[1], lpData[3]);
                }
            }
            break;
        }
    } catch (...) {}
    if (szMsg[0])
        g_pProxyMap->AddLog_other(szMsg);
    return;
}

void CProxyMapDlg::OnReceive()
{
}

void CProxyMapDlg::OnReceiveComplete()
{
    if (m_iocpLocal == NULL)
        return;

    if (m_iocpLocal->m_TcpServer->HasStarted() == FALSE || m_bIsClosed)
        return;

    LPBYTE buf = m_ContextObject->m_DeCompressionBuffer.GetBuffer(0);
    DWORD index = *(DWORD*)&buf[1];
    TCHAR szMsg[200];
    switch (buf[0]) {
    case TOKEN_PROXY_CONNECT_RESULT: {
        char msg_request_co_ok[] = { 0X05, 0X00, 0X00, 0X01, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00 }; // Request connect OK

        BYTE sendbuf[10] = "";
        sendbuf[0] = 5;
        sendbuf[1] = (buf[9] || buf[10]) ? 0 : 5;
        sendbuf[2] = 0;
        sendbuf[3] = 1;
        memcpy(&sendbuf[4], &buf[5], 6);

        ClientContext* pContext_proxy = NULL;
        if (m_iocpLocal->m_TcpServer->GetConnectionExtra((CONNID)index, (PVOID*)&pContext_proxy) && pContext_proxy != nullptr) {
            if (sendbuf[1] == 0) {
                pContext_proxy->m_bProxyConnected = 2;
                wsprintf(szMsg, _T("%d 连接成功\r\n"), index);
            } else
                wsprintf(szMsg, _T("%d 连接失败\r\n"), index);
            m_iocpLocal->Send(pContext_proxy, sendbuf, sizeof(sendbuf));
            AddLog(szMsg);
        }
    }
    break;
    case TOKEN_PROXY_BIND_RESULT:
        break;
    case TOKEN_PROXY_CLOSE: {
        wsprintf(szMsg, _T("%d TOKEN_PROXY_CLOSE\r\n"), index);

        m_iocpLocal->Disconnect(index);
        AddLog(szMsg);
    }
    break;
    case TOKEN_PROXY_DATA: {
        ClientContext* pContext_proxy = NULL;
        BOOL ok = FALSE;
        if (m_iocpLocal->m_TcpServer->GetConnectionExtra((CONNID)index, (PVOID*)&pContext_proxy) && pContext_proxy != nullptr) {
            ok = m_iocpLocal->Send(pContext_proxy, &buf[5], m_ContextObject->m_DeCompressionBuffer.GetBufferLength() - 5);
            if (ok == FALSE) {
                wsprintf(szMsg, _T("%d TOKEN_PROXY_CLOSE\r\n"), index);
                m_iocpLocal->Disconnect(index);
                AddLog(szMsg);
                return;
            }
            wsprintf(szMsg, _T("%d ==>收 %d bytes\r\n"), index, m_ContextObject->m_DeCompressionBuffer.GetBufferLength() - 5);
            AddLog(szMsg);
        }
    }
    break;
    default:
        // 传输发生异常数据
        break;
    }
}

void CProxyMapDlg::AddLog(TCHAR* lpText)
{
    if (m_bIsClosed == TRUE) return;
    m_Edit.SetSel(-1, -1);
    m_Edit.ReplaceSel(lpText);
}

void CProxyMapDlg::AddLog_other(TCHAR* lpText)
{
    if (m_bIsClosed == TRUE) return;
    m_EditOther.SetSel(-1, -1);
    m_EditOther.ReplaceSel(lpText);
}

void CProxyMapDlg::OnSize(UINT nType, int cx, int cy)
{
    CDialog::OnSize(nType, cx, cy);

    // TODO: Add your message handler code here
    if (!IsWindowVisible())
        return;

    RECT	rectClient;
    RECT	rectEdit = {};
    GetClientRect(&rectClient);
    rectEdit.left = 0;
    rectEdit.top = 0;
    rectEdit.right = rectClient.right;
    rectEdit.bottom = rectClient.bottom;
    m_Edit.MoveWindow(&rectEdit);
}

void CProxyMapDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
    CMenu* pSysMenu = GetSystemMenu(FALSE);
    switch (nID) {
    case IDM_PROXY_CHROME: {
        CString strCommand;
        strCommand.Format(_T(" /c start chrome.exe --show-app-list  --proxy-server=\"SOCKS5://127.0.0.1:%d\""), m_nPort);
        ShellExecute(NULL, _T("open"), _T("cmd.exe"), strCommand, NULL, SW_SHOW);
    }
    break;
    }
    CDialog::OnSysCommand(nID, lParam);
}
