#pragma once
#include "IOCPServer.h"
#include "afxwin.h"

// CShellDlg 对话框

class CShellDlg : public CDialog
{
	DECLARE_DYNAMIC(CShellDlg)

public:
	CShellDlg(CWnd* pParent = NULL, IOCPServer* IOCPServer = NULL, CONTEXT_OBJECT *ContextObject = NULL); 
	virtual ~CShellDlg();
	CONTEXT_OBJECT* m_ContextObject;
	IOCPServer* m_iocpServer;
	VOID OnReceiveComplete();
	HICON       m_hIcon;
	UINT m_nReceiveLength;
	VOID AddKeyBoardData(void);
	UINT m_nCurSel;   //获得当前数据所在位置;

// 对话框数据
	enum { IDD = IDD_DIALOG_SHELL };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()
public:
	virtual BOOL OnInitDialog();
	afx_msg void OnClose();
	CEdit m_Edit;
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
};
