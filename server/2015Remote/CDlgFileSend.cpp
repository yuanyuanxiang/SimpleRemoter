// CDlgFileSend.cpp: 实现文件
//

#include "stdafx.h"
#include "CDlgFileSend.h"
#include "2015RemoteDlg.h"  // For g_2015RemoteDlg->FindHost()


// CDlgFileSend 对话框

IMPLEMENT_DYNAMIC(CDlgFileSend, CDialog)

CDlgFileSend::CDlgFileSend(CMy2015RemoteDlg* pParent, Server* IOCPServer, CONTEXT_OBJECT* ContextObject, BOOL sendFile)
    : DialogBase(CDlgFileSend::IDD, pParent, IOCPServer, ContextObject, IDI_File), m_bIsSending(sendFile)
{
	m_pParent = (CMy2015RemoteDlg*)pParent;
}

CDlgFileSend::~CDlgFileSend()
{
}

void CDlgFileSend::DoDataExchange(CDataExchange* pDX)
{
    __super::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_PROGRESS_FILESEND, m_Progress);
}


BEGIN_MESSAGE_MAP(CDlgFileSend, CDialog)
    ON_WM_CLOSE()
    ON_WM_TIMER()
    ON_MESSAGE(WM_UPDATEFILEPROGRESS, &CDlgFileSend::OnUpdateFileProgress)
    ON_MESSAGE(WM_FINISHFILESEND, &CDlgFileSend::OnFinishFileSend)
END_MESSAGE_MAP()


// CDlgFileSend 消息处理程序
std::string GetPwdHash();
std::string GetHMAC(int offset);

void RecvData(void* ptr)
{
    FileChunkPacket* pkt = (FileChunkPacket*)ptr;
}

void CDlgFileSend::OnReceiveComplete(void)
{
    LPBYTE szBuffer = m_ContextObject->InDeCompressedBuffer.GetBuffer();
    unsigned len = m_ContextObject->InDeCompressedBuffer.GetBufferLen();
    if (len == 0) return;

    BYTE cmd = szBuffer[0];
    std::string hash = GetPwdHash(), hmac = GetHMAC(100);

    // V2 文件完成校验包
    if (cmd == COMMAND_FILE_COMPLETE_V2) {
        if (len < sizeof(FileCompletePacketV2)) {
            Mprintf("[FileSend] FILE_COMPLETE_V2 包太短: %u < %zu\n", len, sizeof(FileCompletePacketV2));
            return;
        }
        FileCompletePacketV2* completePkt = (FileCompletePacketV2*)szBuffer;

        // C2C 包：转发到目标客户端
        if (completePkt->dstClientID != 0) {
            context* dstCtx = m_pParent->FindHost(completePkt->dstClientID);
            if (dstCtx) {
                dstCtx->Send2Client(szBuffer, len);
                Mprintf("[C2C] 转发校验包 -> 客户端 %llu\n", completePkt->dstClientID);
            } else {
                Mprintf("[C2C] 转发校验包失败: 目标客户端 %llu 不存在\n", completePkt->dstClientID);
            }
            return;
        }

        // 目标是服务端，本地校验
        bool verifyOk = HandleFileCompleteV2((const char*)szBuffer, len, 0);
        Mprintf("[FileSend] 文件校验%s\n", verifyOk ? "通过" : "失败");
        return;
    }

    if (cmd == COMMAND_SEND_FILE_V2) {
        // V2 协议
        FileChunkPacketV2* chunk = (FileChunkPacketV2*)szBuffer;

        // C2C 包：转发到目标客户端，同时更新进度
        if (chunk->dstClientID != 0) {
            // 如果已标记目标离线，直接忽略后续数据
            if (m_bTargetOffline) {
                return;
            }

            // 转发到目标客户端
            context* dstCtx = m_pParent->FindHost(chunk->dstClientID);
            if (dstCtx) {
                dstCtx->Send2Client(szBuffer, len);
                double percent = chunk->fileSize ? 100.0 * (chunk->offset + chunk->dataLength) / chunk->fileSize : 100.0;
                Mprintf("[C2C] 转发 %d/%d: %.1f%% (%llu/%llu)\n",
                    1 + chunk->fileIndex, chunk->totalFiles, percent,
                    chunk->offset + chunk->dataLength, chunk->fileSize);
            } else {
                Mprintf("[C2C] CDlgFileSend 转发失败: 目标客户端 %llu 不存在，发送取消包\n", chunk->dstClientID);
                m_bTargetOffline = TRUE;

                // 发送取消包给发送方
                FileResumePacketV2 cancelPkt = {};
                cancelPkt.cmd = COMMAND_FILE_RESUME;
                cancelPkt.transferID = chunk->transferID;
                cancelPkt.srcClientID = 0;  // 来自服务端
                cancelPkt.dstClientID = chunk->srcClientID;  // 回复给发送方
                cancelPkt.fileIndex = chunk->fileIndex;
                cancelPkt.fileSize = chunk->fileSize;
                cancelPkt.receivedBytes = chunk->offset;
                cancelPkt.flags = FFV2_CANCEL;
                cancelPkt.rangeCount = 0;
                m_ContextObject->Send2Client((LPBYTE)&cancelPkt, sizeof(cancelPkt));

                // 关闭此连接
                FinishFileSend(FALSE);
                return;
            }
            BYTE* name = szBuffer + sizeof(FileChunkPacketV2);
            UpdateProgress(CString((char*)name, (int)chunk->nameLength), FileProgressInfo(chunk));
            return;
        }

        // 目标是服务端，本地接收
        int n = RecvFileChunkV2((char*)szBuffer, len, nullptr, nullptr, hash, hmac, 0);
        if (n) {
            Mprintf("RecvFileChunkV2 failed: %d\n", n);
        }
        BYTE* name = szBuffer + sizeof(FileChunkPacketV2);
        UpdateProgress(CString((char*)name, (int)chunk->nameLength), FileProgressInfo(chunk));
    } else {
        // V1 协议
        CONNECT_ADDRESS addr = { 0 };
        memcpy(addr.pwdHash, hash.c_str(), min(hash.length(), sizeof(addr.pwdHash)));
        int n = RecvFileChunk((char*)szBuffer, len, &addr, RecvData, hash, hmac);
        if (n) {
            Mprintf("RecvFileChunk failed: %d. hash: %s, hmac: %s\n", n, hash.c_str(), hmac.c_str());
        }
        FileChunkPacket* chunk = (FileChunkPacket*)szBuffer;
        BYTE* name = szBuffer + sizeof(FileChunkPacket);
        UpdateProgress(CString((char*)name, chunk->nameLength), FileProgressInfo(chunk));
    }
}

void CDlgFileSend::UpdateProgress(CString file, const FileProgressInfo& info)
{
    if (!GetSafeHwnd()) return;
    PostMessageA(WM_UPDATEFILEPROGRESS, (WPARAM)new CString(file), (LPARAM)new FileProgressInfo(info));
}

void CDlgFileSend::FinishFileSend(BOOL succeed)
{
    if (!GetSafeHwnd()) return;
    PostMessageA(WM_FINISHFILESEND, NULL, (LPARAM)succeed);
}

LRESULT CDlgFileSend::OnUpdateFileProgress(WPARAM wParam, LPARAM lParam)
{
    CString* pFile = (CString*)wParam;
    FileProgressInfo* pInfo = (FileProgressInfo*)lParam;

    CString status;
    double percent = pInfo->fileSize ? (pInfo->offset + pInfo->dataLength) * 100. / pInfo->fileSize : 100.;
    m_bIsSending ?
    status.FormatL("发送文件(%d/%d): %.2f%%", 1 + pInfo->fileIndex, pInfo->totalFiles, percent):
          status.FormatL("接收文件(%d/%d): %.2f%%", 1 + pInfo->fileIndex, pInfo->totalFiles, percent);
    SetDlgItemTextA(IDC_STATIC_CURRENTINDEX, status);
    SetDlgItemTextA(IDC_STATIC_CURRENT_FILE, *pFile);
    m_Progress.SetPos((int)percent);

    // 只在第一次显示时置顶，后续更新不抢焦点
    if (!IsWindowVisible()) {
        ShowWindow(SW_SHOW);
        SetWindowPos(&wndTopMost, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }
    if (percent>=100.) SetTimer(1, 3000, NULL);

    delete pInfo;
    delete pFile;
    return 0;
}

LRESULT CDlgFileSend::OnFinishFileSend(WPARAM wParam, LPARAM lParam)
{
    BOOL success = (BOOL)lParam;
    m_bIsSending ?
    SetDlgItemTextA(IDC_STATIC_CURRENTINDEX, success ? "文件发送完成" : "文件发送失败"):
    SetDlgItemTextA(IDC_STATIC_CURRENTINDEX, success ? "文件接收完成" : "文件接收失败");
    if (success)
        m_Progress.SetPos(100);

    // 完成后取消置顶，恢复普通窗口
    SetWindowPos(&wndNoTopMost, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    ShowWindow(SW_SHOW);
    SetTimer(1, 3000, NULL);  // 3秒后自动关闭

    return 0;
}

BOOL CDlgFileSend::OnInitDialog()
{
    DialogBase::OnInitDialog();

    SetIcon(m_hIcon, FALSE);

    SetWindowTextA(m_bIsSending ? "发送文件" : "接收文件");
    CMenu* pSysMenu = GetSystemMenu(FALSE);
    if (pSysMenu != nullptr) {
        pSysMenu->EnableMenuItem(SC_CLOSE, MF_BYCOMMAND | MF_GRAYED);
    }

    return TRUE;
}

void CDlgFileSend::OnClose()
{
    // V2传输使用主连接，关闭对话框时不应断开连接
    if (!m_bKeepConnection) {
        CancelIO();
    }
    // 等待数据处理完毕
    if (IsProcessing()) {
        ShowWindow(SW_HIDE);
        return;
    }

    DialogBase::OnClose();
}

void CDlgFileSend::OnTimer(UINT_PTR nIDEvent)
{
    if (nIDEvent == 1) {
        KillTimer(1);
        PostMessageA(WM_CLOSE, 0, 0);
    }
}
