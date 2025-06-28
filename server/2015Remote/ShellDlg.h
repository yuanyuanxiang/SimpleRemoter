#pragma once
#include "IOCPServer.h"
#include "afxwin.h"

// 无论光标位置在哪，新输入的文字总是出现在文本末尾
class CAutoEndEdit : public CEdit {
public:
	afx_msg void OnChar(UINT nChar, UINT nRepCnt, UINT nFlags);
	DECLARE_MESSAGE_MAP()
};


// CShellDlg 对话框

class CShellDlg : public DialogBase
{
	DECLARE_DYNAMIC(CShellDlg)

public:
	CShellDlg(CWnd* pParent = NULL, IOCPServer* IOCPServer = NULL, CONTEXT_OBJECT *ContextObject = NULL); 
	virtual ~CShellDlg();

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
