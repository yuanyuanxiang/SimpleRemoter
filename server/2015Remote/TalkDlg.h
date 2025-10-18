#pragma once
#include "IOCPServer.h"
#include "afxwin.h"

// CTalkDlg �Ի���

class CTalkDlg : public DialogBase
{
    DECLARE_DYNAMIC(CTalkDlg)

public:
    CTalkDlg(CWnd* Parent, Server* IOCPServer=NULL, CONTEXT_OBJECT *ContextObject=NULL);   // ��׼���캯��
    virtual ~CTalkDlg();

    // �Ի�������
    enum { IDD = IDD_DIALOG_TALK };

    void OnReceiveComplete(void)
    {
    }

protected:
    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV ֧��

    DECLARE_MESSAGE_MAP()
public:
    CEdit m_EditTalk;
    virtual BOOL OnInitDialog();
    afx_msg void OnBnClickedButtonTalk();
    virtual BOOL PreTranslateMessage(MSG* pMsg);
    afx_msg void OnClose();
};
