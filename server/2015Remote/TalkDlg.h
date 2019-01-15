#pragma once
#include "IOCPServer.h"
#include "afxwin.h"

// CTalkDlg 对话框

class CTalkDlg : public CDialog
{
	DECLARE_DYNAMIC(CTalkDlg)

public:
	CTalkDlg(CWnd* Parent, IOCPServer* IOCPServer=NULL, CONTEXT_OBJECT *ContextObject=NULL);   // 标准构造函数
	virtual ~CTalkDlg();

	// 对话框数据
	enum { IDD = IDD_DIALOG_TALK };
	CONTEXT_OBJECT* m_ContextObject;
	IOCPServer*     m_iocpServer;
	HICON m_hIcon;
protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()
public:
	CEdit m_EditTalk;
	virtual BOOL OnInitDialog();
	afx_msg void OnBnClickedButtonTalk();
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	afx_msg void OnClose();
};
