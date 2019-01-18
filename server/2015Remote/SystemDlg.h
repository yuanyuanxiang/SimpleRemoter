#pragma once
#include "afxcmn.h"

#include "IOCPServer.h"
// CSystemDlg 对话框

class CSystemDlg : public CDialog
{
	DECLARE_DYNAMIC(CSystemDlg)

public:
	CSystemDlg(CWnd* pParent = NULL, IOCPServer* IOCPServer = NULL, CONTEXT_OBJECT *ContextObject = NULL); 
	virtual ~CSystemDlg();
	CONTEXT_OBJECT* m_ContextObject;
	IOCPServer*     m_iocpServer;
	VOID GetProcessList(void);
	VOID ShowProcessList(void);
	void ShowWindowsList(void);
	void GetWindowsList(void);
	void OnReceiveComplete(void);
	BOOL   m_bHow;
// 对话框数据
	enum { IDD = IDD_DIALOG_SYSTEM };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()
public:
	CListCtrl m_ControlList;
	virtual BOOL OnInitDialog();
	afx_msg void OnClose();
	afx_msg void OnNMRClickListSystem(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnPlistKill();
	afx_msg void OnPlistRefresh();
	afx_msg void OnWlistRefresh();
	afx_msg void OnWlistClose();
	afx_msg void OnWlistHide();
	afx_msg void OnWlistRecover();
	afx_msg void OnWlistMax();
	afx_msg void OnWlistMin();
};
