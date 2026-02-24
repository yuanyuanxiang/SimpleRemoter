#pragma once

#include "IOCPServer.h"
#include "afxdialogex.h"
#include <resource.h>
#include"file_upload.h"
#include "2015RemoteDlg.h"

#define WM_UPDATEFILEPROGRESS (WM_USER + 0x100)
#define WM_FINISHFILESEND    (WM_USER + 0x101)

// 协议无关的进度信息结构（支持 V1 和 V2）
struct FileProgressInfo {
    uint32_t fileIndex;
    uint32_t totalFiles;
    uint64_t fileSize;
    uint64_t offset;
    uint64_t dataLength;
    uint64_t nameLength;

    // 从 V1 包构造
    FileProgressInfo(const FileChunkPacket* pkt) :
        fileIndex(pkt->fileIndex), totalFiles(pkt->totalNum),
        fileSize(pkt->fileSize), offset(pkt->offset),
        dataLength(pkt->dataLength), nameLength(pkt->nameLength) {}

    // 从 V2 包构造
    FileProgressInfo(const FileChunkPacketV2* pkt) :
        fileIndex(pkt->fileIndex), totalFiles(pkt->totalFiles),
        fileSize(pkt->fileSize), offset(pkt->offset),
        dataLength(pkt->dataLength), nameLength(pkt->nameLength) {}
};

// CDlgFileSend 对话框

class CDlgFileSend : public DialogBase
{
    DECLARE_DYNAMIC(CDlgFileSend)
    CMy2015RemoteDlg* m_pParent = nullptr;

public:
    CDlgFileSend(CMy2015RemoteDlg* pParent = NULL, Server* IOCPServer = NULL,
                 CONTEXT_OBJECT* ContextObject = NULL, BOOL sendFile = TRUE);
    virtual ~CDlgFileSend();
    void OnReceiveComplete(void);
    void UpdateProgress(CString file, const FileProgressInfo& info);
    void FinishFileSend(BOOL succeed);

    BOOL m_bIsSending;
    BOOL m_bKeepConnection = FALSE;  // V2传输完成后不断开主连接
    BOOL m_bTargetOffline = FALSE;   // C2C目标已离线，已发送取消包
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
