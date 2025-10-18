// TalkDlg.cpp : 实现文件
//

#include "stdafx.h"
#include "2015Remote.h"
#include "TalkDlg.h"
#include "afxdialogex.h"

// CTalkDlg 对话框

IMPLEMENT_DYNAMIC(CTalkDlg, CDialog)

CTalkDlg::CTalkDlg(CWnd* pParent, Server* IOCPServer, CONTEXT_OBJECT* ContextObject)
    : DialogBase(CTalkDlg::IDD, pParent, IOCPServer, ContextObject, IDR_MAINFRAME)
{
}

CTalkDlg::~CTalkDlg()
{
}

void CTalkDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_EDIT_TALK, m_EditTalk);
    m_EditTalk.SetLimitText(TALK_DLG_MAXLEN);
}


BEGIN_MESSAGE_MAP(CTalkDlg, CDialog)
    ON_BN_CLICKED(IDC_BUTTON_TALK, &CTalkDlg::OnBnClickedButtonTalk)
    ON_WM_CLOSE()
END_MESSAGE_MAP()


// CTalkDlg 消息处理程序


BOOL CTalkDlg::OnInitDialog()
{
    CDialog::OnInitDialog();

    SetIcon(m_hIcon, FALSE);
    BYTE bToken = COMMAND_NEXT;
    m_ContextObject->Send2Client(&bToken, sizeof(BYTE));

    return TRUE;  // return TRUE unless you set the focus to a control
    // 异常: OCX 属性页应返回 FALSE
}


void CTalkDlg::OnBnClickedButtonTalk()
{
    int iLength = m_EditTalk.GetWindowTextLength();

    if (!iLength) {
        return;
    }

    CString strData;
    m_EditTalk.GetWindowText(strData);

    char szBuffer[4096] = {0};
    strcpy(szBuffer,strData.GetBuffer(0));

    m_EditTalk.SetWindowText(NULL);

    m_ContextObject->Send2Client((LPBYTE)szBuffer, strlen(szBuffer));
}


BOOL CTalkDlg::PreTranslateMessage(MSG* pMsg)
{
    if (pMsg->message == WM_KEYDOWN) {
        // 屏蔽VK_ESCAPE、VK_DELETE
        if (pMsg->wParam == VK_ESCAPE)
            return true;
        //如果是可编辑框的回车键
        if (pMsg->wParam == VK_RETURN && pMsg->hwnd == m_EditTalk.m_hWnd) {
            OnBnClickedButtonTalk();

            return TRUE;
        }
    }

    return CDialog::PreTranslateMessage(pMsg);
}


void CTalkDlg::OnClose()
{
    CancelIO();
    // 等待数据处理完毕
    if (IsProcessing()) {
        ShowWindow(SW_HIDE);
        return;
    }

    DialogBase::OnClose();
}
