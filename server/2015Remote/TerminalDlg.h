#pragma once
#include "IOCPServer.h"
#include <string>

// Dynamic loader for TerminalModule DLL
#include "TerminalModuleLoader.h"

// CTerminalDlg - Linux terminal dialog using TerminalModule DLL
class CTerminalDlg : public DialogBase
{
    DECLARE_DYNAMIC(CTerminalDlg)

public:
    CTerminalDlg(CWnd* pParent = NULL, Server* IOCPServer = NULL, CONTEXT_OBJECT* ContextObject = NULL);
    virtual ~CTerminalDlg();

    enum { IDD = IDD_DIALOG_SHELL };

protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    virtual BOOL OnInitDialog();
    virtual VOID OnReceiveComplete();
    virtual BOOL PreTranslateMessage(MSG* pMsg);

    afx_msg void OnClose();
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnDestroy();
    afx_msg LRESULT OnTerminalData(WPARAM wParam, LPARAM lParam);
    DECLARE_MESSAGE_MAP()

    static const UINT WM_TERMINAL_DATA = WM_USER + 100;

private:
    // Terminal handle from DLL
    HTERMINAL m_hTerminal;

    // Terminal size
    int m_cols;
    int m_rows;

    // Static callbacks
    static void CALLBACK OnTerminalInput(void* userData, const char* data, size_t len);
    static void CALLBACK OnTerminalResize(void* userData, int cols, int rows);
    static void CALLBACK OnTerminalClose(void* userData);
    static void CALLBACK OnTerminalReady(void* userData, int cols, int rows);

    // Send resize command to Linux client
    void SendResizeCommand(int cols, int rows);
};
