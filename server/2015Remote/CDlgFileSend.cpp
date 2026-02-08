// CDlgFileSend.cpp: 实现文件
//

#include "stdafx.h"
#include "CDlgFileSend.h"


// CDlgFileSend 对话框

IMPLEMENT_DYNAMIC(CDlgFileSend, CDialog)

CDlgFileSend::CDlgFileSend(CWnd* pParent, Server* IOCPServer, CONTEXT_OBJECT* ContextObject, BOOL sendFile)
    : DialogBase(CDlgFileSend::IDD, pParent, IOCPServer, ContextObject, IDI_File), m_bIsSending(sendFile)
{
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
    std::string hash = GetPwdHash(), hmac = GetHMAC(100);
    CONNECT_ADDRESS addr = { 0 };
    memcpy(addr.pwdHash, hash.c_str(), min(hash.length(), sizeof(addr.pwdHash)));
    int n = RecvFileChunk((char*)szBuffer, len, &addr, RecvData, hash, hmac);
    if (n) {
        Mprintf("RecvFileChunk failed: %d. hash: %s, hmac: %s\n", n, hash.c_str(), hmac.c_str());
    }
    BYTE* name = szBuffer + sizeof(FileChunkPacket);
    FileChunkPacket* chunk = (FileChunkPacket*)szBuffer;
    UpdateProgress(CString((char*)name, chunk->nameLength), chunk);
}

void CDlgFileSend::UpdateProgress(CString file, const FileChunkPacket *chunk)
{
    if (!GetSafeHwnd()) return;
    PostMessageA(WM_UPDATEFILEPROGRESS, (WPARAM)new CString(file), (LPARAM)new FileChunkPacket(*chunk));
}

void CDlgFileSend::FinishFileSend(BOOL succeed)
{
    if (!GetSafeHwnd()) return;
    PostMessageA(WM_FINISHFILESEND, NULL, (LPARAM)succeed);
}

LRESULT CDlgFileSend::OnUpdateFileProgress(WPARAM wParam, LPARAM lParam)
{
    CString* pFile = (CString*)wParam;
    FileChunkPacket* pChunk = (FileChunkPacket*)lParam;

    CString status;
    double percent = pChunk->fileSize ? (pChunk->offset + pChunk->dataLength) * 100. / pChunk->fileSize : 100.;
    m_bIsSending ?
    status.FormatL("发送文件(%d/%d): %.2f%%", 1 + pChunk->fileIndex, pChunk->totalNum, percent):
          status.FormatL("接收文件(%d/%d): %.2f%%", 1 + pChunk->fileIndex, pChunk->totalNum, percent);
    SetDlgItemTextA(IDC_STATIC_CURRENTINDEX, status);
    SetDlgItemTextA(IDC_STATIC_CURRENT_FILE, *pFile);
    m_Progress.SetPos(percent);
    ShowWindow(SW_SHOW);
    BringWindowToTop();
    SetForegroundWindow();
    if (percent>=100.) SetTimer(1, 3000, NULL);

    delete pChunk;
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
    ShowWindow(SW_SHOW);

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
    CancelIO();
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
