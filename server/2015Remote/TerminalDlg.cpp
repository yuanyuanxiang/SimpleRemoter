// TerminalDlg.cpp - Linux terminal dialog using TerminalModule DLL

#include "stdafx.h"
#include "2015Remote.h"
#include "TerminalDlg.h"
#include "afxdialogex.h"

IMPLEMENT_DYNAMIC(CTerminalDlg, CDialog)

CTerminalDlg::CTerminalDlg(CWnd* pParent, Server* IOCPServer, CONTEXT_OBJECT* ContextObject)
    : DialogBase(IDD, pParent, IOCPServer, ContextObject, IDI_ICON_SHELL)
    , m_hTerminal(NULL)
    , m_cols(80)
    , m_rows(24)
{
}

CTerminalDlg::~CTerminalDlg()
{
}

void CTerminalDlg::DoDataExchange(CDataExchange* pDX)
{
    DialogBase::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CTerminalDlg, CDialog)
    ON_WM_CLOSE()
    ON_WM_SIZE()
    ON_WM_DESTROY()
    ON_MESSAGE(WM_TERMINAL_DATA, OnTerminalData)
END_MESSAGE_MAP()

BOOL CTerminalDlg::OnInitDialog()
{
    DialogBase::OnInitDialog();

    // Destroy the Edit control from ShellDlg template
    CWnd* pEdit = GetDlgItem(IDC_EDIT);
    if (pEdit) {
        pEdit->DestroyWindow();
    }

    SetIcon(m_hIcon, TRUE);
    SetIcon(m_hIcon, FALSE);

    CString title;
    title.Format("%s - Modern %s", m_IPAddress, _TRF("远程终端"));
    SetWindowText(title);

    // Set window size
    SetWindowPos(NULL, 0, 0, 800, 500, SWP_NOMOVE | SWP_NOZORDER);
    CenterWindow();

    // Setup callbacks
    TerminalCallbacks callbacks = {};
    callbacks.userData = this;
    callbacks.onInput = OnTerminalInput;
    callbacks.onResize = OnTerminalResize;
    callbacks.onClose = OnTerminalClose;
    callbacks.onReady = OnTerminalReady;

    // Create embedded terminal using DLL
    m_hTerminal = CreateEmbeddedTerminal(GetSafeHwnd(), &callbacks);
    if (!m_hTerminal) {
        CString errMsg;
        if (!IsTerminalModuleLoaded()) {
            errMsg = _T("TerminalModule.dll not found!\n\nPlease copy TerminalModule_x86.dll to the application directory.");
        } else {
            errMsg = _T("CreateEmbeddedTerminal failed.\n\nPlease ensure Microsoft Edge WebView2 Runtime is installed.");
        }
        MessageBox(errMsg, _T("Error"), MB_ICONERROR);
        PostMessage(WM_CLOSE);
        return TRUE;
    }

    return TRUE;
}

void CTerminalDlg::OnTerminalInput(void* userData, const char* data, size_t len)
{
    CTerminalDlg* pThis = (CTerminalDlg*)userData;
    if (pThis && pThis->m_ContextObject && data && len > 0) {
        pThis->m_ContextObject->Send2Client((PBYTE)data, (ULONG)len);
    }
}

void CTerminalDlg::OnTerminalResize(void* userData, int cols, int rows)
{
    CTerminalDlg* pThis = (CTerminalDlg*)userData;
    if (pThis) {
        pThis->m_cols = cols;
        pThis->m_rows = rows;
        pThis->SendResizeCommand(cols, rows);
    }
}

void CTerminalDlg::OnTerminalClose(void* userData)
{
    CTerminalDlg* pThis = (CTerminalDlg*)userData;
    if (pThis && ::IsWindow(pThis->GetSafeHwnd())) {
        pThis->PostMessage(WM_CLOSE);
    }
}

void CTerminalDlg::OnTerminalReady(void* userData, int cols, int rows)
{
    CTerminalDlg* pThis = (CTerminalDlg*)userData;
    TRACE("OnTerminalReady called: cols=%d, rows=%d\n", cols, rows);

    if (pThis) {
        pThis->m_cols = cols;
        pThis->m_rows = rows;

        // Send initial size to Linux client
        pThis->SendResizeCommand(cols, rows);

        // Tell Linux client to start the shell
        if (pThis->m_ContextObject) {
            TRACE("Sending COMMAND_NEXT to start shell\n");
            BYTE bToken = COMMAND_NEXT;
            pThis->m_ContextObject->Send2Client(&bToken, sizeof(BYTE));
        }
    }
}

void CTerminalDlg::OnReceiveComplete()
{
    if (!m_ContextObject) {
        return;
    }

    PBYTE data = m_ContextObject->InDeCompressedBuffer.GetBuffer(0);
    ULONG len = m_ContextObject->InDeCompressedBuffer.GetBufferLength();

    if (len == 0 || !data) {
        return;
    }

    // Check for terminal close notification
    if (len == 1 && data[0] == TOKEN_TERMINAL_CLOSE) {
        PostMessage(WM_CLOSE);
        return;
    }

    // Copy data and post to UI thread (WebView2 API must be called from UI thread)
    std::string* pData = new std::string((const char*)data, len);
    PostMessage(WM_TERMINAL_DATA, (WPARAM)pData, 0);
}

LRESULT CTerminalDlg::OnTerminalData(WPARAM wParam, LPARAM lParam)
{
    std::string* pData = (std::string*)wParam;
    if (pData) {
        if (m_hTerminal && IsTerminalValid(m_hTerminal)) {
            TerminalWrite(m_hTerminal, pData->c_str(), pData->length());
        }
        delete pData;
    }
    return 0;
}

void CTerminalDlg::SendResizeCommand(int cols, int rows)
{
    if (!m_ContextObject) return;

    // CMD_TERMINAL_RESIZE (1 byte) + cols (2 bytes) + rows (2 bytes)
    BYTE buf[5];
    buf[0] = CMD_TERMINAL_RESIZE;
    *(short*)(buf + 1) = (short)cols;
    *(short*)(buf + 3) = (short)rows;

    m_ContextObject->Send2Client(buf, 5);
}

void CTerminalDlg::OnSize(UINT nType, int cx, int cy)
{
    DialogBase::OnSize(nType, cx, cy);
    // WebView2 bounds are handled automatically by the DLL's subclass
}

void CTerminalDlg::OnClose()
{
    CancelIO();

    if (IsProcessing()) {
        ShowWindow(SW_HIDE);
        return;
    }

    DialogBase::OnClose();
}

void CTerminalDlg::OnDestroy()
{
    // Close terminal
    if (m_hTerminal) {
        CloseTerminal(m_hTerminal);
        m_hTerminal = NULL;
    }

    DialogBase::OnDestroy();
}

BOOL CTerminalDlg::PreTranslateMessage(MSG* pMsg)
{
    // Block ESC key from closing the dialog
    if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_ESCAPE) {
        return TRUE;
    }

    return DialogBase::PreTranslateMessage(pMsg);
}
