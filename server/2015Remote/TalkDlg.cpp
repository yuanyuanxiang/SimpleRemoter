// TalkDlg.cpp : 实现文件
//

#include "stdafx.h"
#include "2015Remote.h"
#include "TalkDlg.h"
#include "afxdialogex.h"

// CTalkDlg 对话框

IMPLEMENT_DYNAMIC(CTalkDlg, CDialog)

CTalkDlg::CTalkDlg(CWnd* pParent,IOCPServer* IOCPServer, CONTEXT_OBJECT* ContextObject)
	: CDialog(CTalkDlg::IDD, pParent)
{
	m_iocpServer	= IOCPServer;
	m_ContextObject	= ContextObject;
}

CTalkDlg::~CTalkDlg()
{
}

void CTalkDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_EDIT_TALK, m_EditTalk);
}


BEGIN_MESSAGE_MAP(CTalkDlg, CDialog)
	ON_BN_CLICKED(IDC_BUTTON_TALK, &CTalkDlg::OnBnClickedButtonTalk)
	ON_WM_CLOSE()
END_MESSAGE_MAP()


// CTalkDlg 消息处理程序


BOOL CTalkDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	m_hIcon = LoadIcon(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDR_MAINFRAME));
	SetIcon(m_hIcon, FALSE);
	BYTE bToken = COMMAND_NEXT;  
	m_iocpServer->OnClientPreSending(m_ContextObject, &bToken, sizeof(BYTE));

	return TRUE;  // return TRUE unless you set the focus to a control
	// 异常: OCX 属性页应返回 FALSE
}


void CTalkDlg::OnBnClickedButtonTalk()
{
	int iLength = m_EditTalk.GetWindowTextLength();   //EditBox 上获得数据长度

	if (!iLength)
	{
		return;
	}

	CString strData;
	m_EditTalk.GetWindowText(strData);            //EditBox 上获得数据

	char* szBuffer = new char[iLength + 1];		// 2019.1.5 必须加1
	memset(szBuffer,0,sizeof(char)*iLength);

	strcpy(szBuffer,strData.GetBuffer(0));

	m_EditTalk.SetWindowText(NULL);          //EditBox 上的数据清空

	m_iocpServer->OnClientPreSending(m_ContextObject, (LPBYTE)szBuffer, strlen(szBuffer));//将自己内存中的数据发送
	delete [] szBuffer;
}


BOOL CTalkDlg::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg->message == WM_KEYDOWN)
	{
		// 屏蔽VK_ESCAPE、VK_DELETE
		if (pMsg->wParam == VK_ESCAPE)
			return true;
		//如果是可编辑框的回车键
		if (pMsg->wParam == VK_RETURN && pMsg->hwnd == m_EditTalk.m_hWnd)
		{
			OnBnClickedButtonTalk(); 

			return TRUE;
		}
	}

	return CDialog::PreTranslateMessage(pMsg);
}


void CTalkDlg::OnClose()
{
	// TODO: 在此添加消息处理程序代码和/或调用默认值
#if CLOSE_DELETE_DLG
	m_ContextObject->v1 = 0;
#endif
	CancelIo((HANDLE)m_ContextObject->sClientSocket);
	closesocket(m_ContextObject->sClientSocket);
	CDialog::OnClose();
#if CLOSE_DELETE_DLG
	delete this;
#endif
}
