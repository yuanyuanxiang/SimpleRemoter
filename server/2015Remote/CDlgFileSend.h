#pragma once

#include "IOCPServer.h"
#include "afxdialogex.h"
#include <resource.h>
#include"file_upload.h"

#define WM_UPDATEFILEPROGRESS (WM_USER + 0x100)
#define WM_FINISHFILESEND    (WM_USER + 0x101)

// CDlgFileSend 对话框

class CDlgFileSend : public DialogBase
{
    DECLARE_DYNAMIC(CDlgFileSend)

public:
    CDlgFileSend(CWnd* pParent = NULL, Server* IOCPServer = NULL,
                 CONTEXT_OBJECT* ContextObject = NULL, BOOL sendFile = TRUE);
    virtual ~CDlgFileSend();
    void OnReceiveComplete(void);
    void UpdateProgress(CString file, const FileChunkPacket *chunk);
    void FinishFileSend(BOOL succeed);

    BOOL m_bIsSending;
    enum { IDD = IDD_DIALOG_FILESEND };

protected:
    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

    DECLARE_MESSAGE_MAP()

    afx_msg LRESULT OnUpdateFileProgress(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnFinishFileSend(WPARAM wParam, LPARAM lParam);

public:
    CProgressCtrl m_Progress;
    virtual BOOL OnInitDialog();
    afx_msg void OnClose();
    afx_msg void OnTimer(UINT_PTR nIDEvent);
};
