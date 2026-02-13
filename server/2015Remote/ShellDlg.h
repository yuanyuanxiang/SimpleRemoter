#pragma once
#include "IOCPServer.h"
#include "afxwin.h"

// 限制光标不能移动到历史输出区域，只能在当前命令行内编辑
class CAutoEndEdit : public CEdit
{
public:
    CAutoEndEdit() : m_nMinEditPos(0) {}
    UINT m_nMinEditPos;  // 最小可编辑位置（历史输出的末尾）
    afx_msg void OnChar(UINT nChar, UINT nRepCnt, UINT nFlags);
    DECLARE_MESSAGE_MAP()
};


// CShellDlg 对话框

class CShellDlg : public DialogBase
{
    DECLARE_DYNAMIC(CShellDlg)

public:
    CShellDlg(CWnd* pParent = NULL, Server* IOCPServer = NULL, CONTEXT_OBJECT *ContextObject = NULL);
    virtual ~CShellDlg();

private:
    CBrush m_brBackground;  // 背景画刷，避免 GDI 泄漏

    VOID OnReceiveComplete();

    UINT m_nReceiveLength;
    VOID AddKeyBoardData(void);
    int m_nCurSel;   //获得当前数据所在位置;

// 对话框数据
    enum { IDD = IDD_DIALOG_SHELL };

protected:
    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

    DECLARE_MESSAGE_MAP()
public:
    virtual BOOL OnInitDialog();
    afx_msg void OnClose();
    CAutoEndEdit m_Edit;
    virtual BOOL PreTranslateMessage(MSG* pMsg);
    afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
    afx_msg void OnSize(UINT nType, int cx, int cy);
};
