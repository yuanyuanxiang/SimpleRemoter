// AudioDlg.cpp : 实现文件
//

#include "stdafx.h"
#include "2015Remote.h"
#include "AudioDlg.h"
#include "afxdialogex.h"


// CAudioDlg 对话框

IMPLEMENT_DYNAMIC(CAudioDlg, CDialog)

CAudioDlg::CAudioDlg(CWnd* pParent, Server* IOCPServer, CONTEXT_OBJECT *ContextObject)
    : DialogBase(CAudioDlg::IDD, pParent, IOCPServer, ContextObject, IDI_ICON_AUDIO)
    , m_bSend(FALSE)
{
    m_bIsWorking	= TRUE;
    m_bThreadRun	= FALSE;

    m_hWorkThread  = NULL;
    m_nTotalRecvBytes = 0;
}

CAudioDlg::~CAudioDlg()
{
    m_bIsWorking = FALSE;
    while (m_bThreadRun)
        Sleep(50);
}

void CAudioDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    DDX_Check(pDX, IDC_CHECK, m_bSend);
}


BEGIN_MESSAGE_MAP(CAudioDlg, CDialog)
    ON_WM_CLOSE()
    ON_BN_CLICKED(IDC_CHECK, &CAudioDlg::OnBnClickedCheck)
END_MESSAGE_MAP()


// CAudioDlg 消息处理程序


BOOL CAudioDlg::OnInitDialog()
{
    CDialog::OnInitDialog();

    SetIcon(m_hIcon,FALSE);

    CString strString;
    strString.Format("%s - 语音监听", m_IPAddress);
    SetWindowText(strString);

    BYTE bToken = COMMAND_NEXT;
    m_ContextObject->Send2Client(&bToken, sizeof(BYTE));

    //启动线程 判断CheckBox
    m_hWorkThread = CreateThread(NULL, 0, WorkThread, (LPVOID)this, 0, NULL);

    m_bThreadRun = m_hWorkThread ? TRUE : FALSE;

    GetDlgItem(IDC_CHECK)->EnableWindow(TRUE);

    return TRUE;  // return TRUE unless you set the focus to a control
    // 异常: OCX 属性页应返回 FALSE
}

DWORD  CAudioDlg::WorkThread(LPVOID lParam)
{
    CAudioDlg	*This = (CAudioDlg *)lParam;

    while (This->m_bIsWorking) {
        if (!This->m_bSend) {
            WAIT_n(This->m_bIsWorking, 1, 50);
            continue;
        }
        DWORD	dwBufferSize = 0;
        LPBYTE	szBuffer = This->m_AudioObject.GetRecordBuffer(&dwBufferSize);   //播放声音

        if (szBuffer != NULL && dwBufferSize > 0)
            This->m_ContextObject->Send2Client(szBuffer, dwBufferSize); //没有消息头
    }
    This->m_bThreadRun = FALSE;

    return 0;
}

void CAudioDlg::OnReceiveComplete(void)
{
    m_nTotalRecvBytes += m_ContextObject->InDeCompressedBuffer.GetBufferLength() - 1;   //1000+ =1000 1
    CString	strString;
    strString.Format("Receive %d KBytes", m_nTotalRecvBytes / 1024);
    SetDlgItemText(IDC_TIPS, strString);
    switch (m_ContextObject->InDeCompressedBuffer.GetBYTE(0)) {
    case TOKEN_AUDIO_DATA: {
        Buffer tmp = m_ContextObject->InDeCompressedBuffer.GetMyBuffer(1);
        m_AudioObject.PlayBuffer(tmp.Buf(), tmp.length());   //播放波形数据
        break;
    }

    default:
        // 传输发生异常数据
        break;
    }
}

void CAudioDlg::OnClose()
{
    CancelIO();
    // 等待数据处理完毕
    if (IsProcessing()) {
        ShowWindow(SW_HIDE);
        return;
    }

    m_bIsWorking = FALSE;
    WaitForSingleObject(m_hWorkThread, INFINITE);
    DialogBase::OnClose();
}

// 处理是否发送本地语音到远程
void CAudioDlg::OnBnClickedCheck()
{
    UpdateData(true);
}
