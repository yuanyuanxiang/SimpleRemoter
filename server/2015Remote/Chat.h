#pragma once

#include "stdafx.h"

/////////////////////////////////////////////////////////////////////////////
// CChat dialog

class CChat : public DialogBase
{
    // Construction
public:
    CChat(CWnd* pParent = NULL, ISocketBase* pIOCPServer = NULL, ClientContext* pContext = NULL);
    void OnReceiveComplete();
    void OnReceive();

    enum { IDD = IDD_CHAT };

public:
    virtual BOOL PreTranslateMessage(MSG* pMsg);
protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    virtual void PostNcDestroy();
    virtual void OnClose();

// Implementation
protected:
    ClientContext* m_pContext;
    ISocketBase* m_iocpServer;

    virtual BOOL OnInitDialog();
    afx_msg void OnButtonSend();
    afx_msg void OnButtonEnd();

    afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
    afx_msg void OnSetfocusEditChatLog();
    afx_msg void OnKillfocusEditChatLog();

    DECLARE_MESSAGE_MAP()
private:
    HICON m_hIcon;
    BOOL m_bOnClose;
public:
    CEdit m_editTip;
    CEdit m_editNewMsg;
    CEdit m_editChatLog;

    afx_msg void OnBnClickedButton_LOCK();
    afx_msg void OnBnClickedButton_UNLOCK();
};
