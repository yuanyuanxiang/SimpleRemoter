// AudioDlg.cpp : ʵ���ļ�
//

#include "stdafx.h"
#include "2015Remote.h"
#include "AudioDlg.h"
#include "afxdialogex.h"


// CAudioDlg �Ի���

IMPLEMENT_DYNAMIC(CAudioDlg, CDialog)

CAudioDlg::CAudioDlg(CWnd* pParent, IOCPServer* IOCPServer, CONTEXT_OBJECT *ContextObject)
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


// CAudioDlg ��Ϣ�������


BOOL CAudioDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	SetIcon(m_hIcon,FALSE);

	CString strString;
	strString.Format("%s - ��������", m_IPAddress);
	SetWindowText(strString);

	BYTE bToken = COMMAND_NEXT;
	m_iocpServer->OnClientPreSending(m_ContextObject, &bToken, sizeof(BYTE));

	//�����߳� �ж�CheckBox
	m_hWorkThread = CreateThread(NULL, 0, WorkThread, (LPVOID)this, 0, NULL);

	m_bThreadRun = m_hWorkThread ? TRUE : FALSE;

	GetDlgItem(IDC_CHECK)->EnableWindow(TRUE);

	return TRUE;  // return TRUE unless you set the focus to a control
	// �쳣: OCX ����ҳӦ���� FALSE
}

DWORD  CAudioDlg::WorkThread(LPVOID lParam)
{
	CAudioDlg	*This = (CAudioDlg *)lParam;

	while (This->m_bIsWorking)
	{
		if (!This->m_bSend)
		{
			WAIT_n(This->m_bIsWorking, 1, 50);
			continue;
		}
		DWORD	dwBufferSize = 0;
		LPBYTE	szBuffer = This->m_AudioObject.GetRecordBuffer(&dwBufferSize);   //��������

		if (szBuffer != NULL && dwBufferSize > 0)
			This->m_iocpServer->OnClientPreSending(This->m_ContextObject, szBuffer, dwBufferSize); //û����Ϣͷ
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
	switch (m_ContextObject->InDeCompressedBuffer.GetBYTE(0))
	{
	case TOKEN_AUDIO_DATA:
		{
			Buffer tmp = m_ContextObject->InDeCompressedBuffer.GetMyBuffer(1);
			m_AudioObject.PlayBuffer(tmp.Buf(), tmp.length());   //���Ų�������
			break;
		}

	default:
		// ���䷢���쳣����
		break;
	}
}

void CAudioDlg::OnClose()
{
	CancelIO();

	m_bIsWorking = FALSE;
	WaitForSingleObject(m_hWorkThread, INFINITE);
	DialogBase::OnClose();
}

// �����Ƿ��ͱ���������Զ��
void CAudioDlg::OnBnClickedCheck()
{
	UpdateData(true);
}
