// ShellDlg.cpp : ʵ���ļ�
//

#include "stdafx.h"
#include "2015Remote.h"
#include "ShellDlg.h"
#include "afxdialogex.h"

#define EDIT_MAXLENGTH 30000

BEGIN_MESSAGE_MAP(CAutoEndEdit, CEdit)
	ON_WM_CHAR()
END_MESSAGE_MAP()

void CAutoEndEdit::OnChar(UINT nChar, UINT nRepCnt, UINT nFlags) {
	// ������ƶ����ı�ĩβ
	int nLength = GetWindowTextLength();
	SetSel(nLength, nLength);

	// ���ø��ദ�������ַ�
	CEdit::OnChar(nChar, nRepCnt, nFlags);
}

// CShellDlg �Ի���

IMPLEMENT_DYNAMIC(CShellDlg, CDialog)

CShellDlg::CShellDlg(CWnd* pParent, IOCPServer* IOCPServer, CONTEXT_OBJECT *ContextObject)
	: DialogBase(CShellDlg::IDD, pParent, IOCPServer, ContextObject, IDI_ICON_SHELL)
{
}

CShellDlg::~CShellDlg()
{
}

void CShellDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_EDIT, m_Edit);
}


BEGIN_MESSAGE_MAP(CShellDlg, CDialog)
	ON_WM_CLOSE()
	ON_WM_CTLCOLOR()
	ON_WM_SIZE()
END_MESSAGE_MAP()


// CShellDlg ��Ϣ�������


BOOL CShellDlg::OnInitDialog()
{
	CDialog::OnInitDialog();
	m_nCurSel = 0;
	m_nReceiveLength = 0;
	SetIcon(m_hIcon, TRUE);
	SetIcon(m_hIcon,FALSE);

	CString str;
	str.Format("%s - Զ���ն�", m_IPAddress);
	SetWindowText(str);

	BYTE bToken = COMMAND_NEXT;
	m_iocpServer->OnClientPreSending(m_ContextObject, &bToken, sizeof(BYTE));  

	m_Edit.SetWindowTextA(">>");
	m_nCurSel = m_Edit.GetWindowTextLengthA();
	m_nReceiveLength = m_nCurSel;
	m_Edit.SetSel((int)m_nCurSel, (int)m_nCurSel);
	m_Edit.PostMessage(EM_SETSEL, m_nCurSel, m_nCurSel);
	m_Edit.SetLimitText(EDIT_MAXLENGTH);

	return TRUE;  // return TRUE unless you set the focus to a control
	// �쳣: OCX ����ҳӦ���� FALSE
}


VOID CShellDlg::OnReceiveComplete()
{
	if (m_ContextObject==NULL)
	{
		return;
	}

	AddKeyBoardData();
	m_nReceiveLength = m_Edit.GetWindowTextLength();
}


#include <regex>
std::string removeAnsiCodes(const std::string& input) {
	std::regex ansi_regex("\x1B\\[[0-9;]*[mK]");
	return std::regex_replace(input, ansi_regex, "");
}

VOID CShellDlg::AddKeyBoardData(void)
{
	// �������0

	//Shit\0
	m_ContextObject->InDeCompressedBuffer.WriteBuffer((LPBYTE)"", 1);           //�ӱ����ƶ�������������Ҫ����һ��\0
	Buffer tmp = m_ContextObject->InDeCompressedBuffer.GetMyBuffer(0);
	bool firstRecv = tmp.c_str() == std::string(">");
	CString strResult = firstRecv ? "" : CString("\r\n") + removeAnsiCodes(tmp.c_str()).c_str(); //������е����� ���� \0

	//�滻��ԭ���Ļ��з�  ����cmd �Ļ���ͬw32�µı༭�ؼ��Ļ��з���һ��   ���еĻس�����   
	strResult.Replace("\n", "\r\n");

	if (strResult.GetLength() + m_Edit.GetWindowTextLength() >= EDIT_MAXLENGTH)
	{
		CString text;
		m_Edit.GetWindowTextA(text);
		auto n = EDIT_MAXLENGTH - strResult.GetLength() - 5; // ��5���ַ�����clear����
		if (n < 0) {
			strResult = strResult.Right(strResult.GetLength() + n);
		}
		m_Edit.SetWindowTextA(text.Right(max(n, 0)));
	}

	//�õ���ǰ���ڵ��ַ�����
	int	iLength = m_Edit.GetWindowTextLength();    //kdfjdjfdir                                 
	//1.txt
	//2.txt
	//dir\r\n

	//����궨λ����λ�ò�ѡ��ָ���������ַ�  Ҳ����ĩβ ��Ϊ�ӱ��ض��������� Ҫ��ʾ�� ���ǵ� ��ǰ���ݵĺ���
	m_Edit.SetSel(iLength, iLength);

	//�ô��ݹ����������滻����λ�õ��ַ�    //��ʾ
	m_Edit.ReplaceSel(strResult);   

	//���µõ��ַ��Ĵ�С

	m_nCurSel = m_Edit.GetWindowTextLength(); 

	//����ע�⵽��������ʹ��Զ���ն�ʱ �����͵�ÿһ�������� ����һ�����з�  ����һ���س�
	//Ҫ�ҵ�����س��Ĵ������Ǿ�Ҫ��PreTranslateMessage�����Ķ���  
}

void CShellDlg::OnClose()
{
	CancelIO();
	// �ȴ����ݴ������
	if (IsProcessing()) {
		ShowWindow(SW_HIDE);
		return;
	}

	DialogBase::OnClose();
}


CString ExtractAfterLastNewline(const CString& str)
{
	int nPos = str.ReverseFind(_T('\n'));
	if (nPos != -1)
	{
		return str.Mid(nPos + 1);
	}

	nPos = str.ReverseFind(_T('\r'));
	if (nPos != -1)
	{
		return str.Mid(nPos + 1);
	}

	return str;
}


BOOL CShellDlg::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg->message == WM_KEYDOWN)
	{
		// ����VK_ESCAPE��VK_DELETE
		if (pMsg->wParam == VK_ESCAPE || pMsg->wParam == VK_DELETE)
			return true;
		//����ǿɱ༭��Ļس���
		if (pMsg->wParam == VK_RETURN && pMsg->hwnd == m_Edit.m_hWnd)
		{
			//�õ����ڵ����ݴ�С
			int	iLength = m_Edit.GetWindowTextLength();
			CString str;
			//�õ����ڵ��ַ�����
			m_Edit.GetWindowText(str);
			//���뻻�з�
			str += "\r\n";
			//�õ������Ļ��������׵�ַ�ټ���ԭ�е��ַ���λ�ã���ʵ�����û���ǰ�����������
			//Ȼ�����ݷ��ͳ�ȥ
			LPBYTE pSrc = (LPBYTE)str.GetBuffer(0) + m_nCurSel;
#ifdef _DEBUG
            TRACE("[Shell]=> %s", (char*)pSrc);
#endif
			if (0 == strcmp((char*)pSrc, "exit\r\n")) { // �˳��ն�
				return PostMessage(WM_CLOSE);
			}
			else if (0 == strcmp((char*)pSrc, "clear\r\n")) { // �����ն�
				str = ExtractAfterLastNewline(str.Left(str.GetLength() - 7));
				m_Edit.SetWindowTextA(str);
				m_nCurSel = m_Edit.GetWindowTextLength();
				m_nReceiveLength = m_nCurSel;
				m_Edit.SetSel(m_nCurSel, m_nCurSel);
				return TRUE;
			}
			int length = str.GetLength() - m_nCurSel;
			m_iocpServer->OnClientPreSending(m_ContextObject, pSrc, length);
			m_nCurSel = m_Edit.GetWindowTextLength();
		}
		// ����VK_BACK
		if (pMsg->wParam == VK_BACK && pMsg->hwnd == m_Edit.m_hWnd)
		{
			if (m_Edit.GetWindowTextLength() <= m_nReceiveLength)  
				return true;
		}
		// ʾ����
		//dir\r\n  5
	}

	return CDialog::PreTranslateMessage(pMsg);
}


HBRUSH CShellDlg::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
	HBRUSH hbr = CDialog::OnCtlColor(pDC, pWnd, nCtlColor);

	if ((pWnd->GetDlgCtrlID() == IDC_EDIT) && (nCtlColor == CTLCOLOR_EDIT))
	{
		COLORREF clr = RGB(255, 255, 255);
		pDC->SetTextColor(clr);   //���ð�ɫ���ı�
		clr = RGB(0,0,0);
		pDC->SetBkColor(clr);     //���ú�ɫ�ı���
		return CreateSolidBrush(clr);  //��ΪԼ�������ر���ɫ��Ӧ��ˢ�Ӿ��
	}
	else
	{
		return CDialog::OnCtlColor(pDC, pWnd, nCtlColor);
	}
	return hbr;
}


void CShellDlg::OnSize(UINT nType, int cx, int cy)
{
	CDialog::OnSize(nType, cx, cy);

	if (!m_Edit.GetSafeHwnd()) return; // ȷ���ؼ��Ѵ���

	// ������λ�úʹ�С
	CRect rc;
	m_Edit.GetWindowRect(&rc);
	ScreenToClient(&rc);

	// �������ÿؼ���С
	m_Edit.MoveWindow(0, 0, cx, cy, TRUE);
}
