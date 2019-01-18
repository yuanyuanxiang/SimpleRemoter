#pragma once
#include "afxcmn.h"

#include "IOCPServer.h"

// CRegisterDlg 对话框

class CRegisterDlg : public CDialog
{
	DECLARE_DYNAMIC(CRegisterDlg)

public:
	CRegisterDlg(CWnd* Parent, IOCPServer* IOCPServer=NULL, CONTEXT_OBJECT *ContextObject=NULL);   // 标准构造函数
	virtual ~CRegisterDlg();
	CONTEXT_OBJECT* m_ContextObject;
	IOCPServer*     m_iocpServer;
// 对话框数据
	enum { IDD = IDD_DIALOG_REGISTER };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()
public:
	BOOL m_bIsClosed;   // 即将关闭
	BOOL m_bIsWorking;// 正在处理注册表
	CTreeCtrl m_Tree;
	CImageList	m_ImageListTree;   //树控件上的图标	
	CListCtrl m_ControlList;
	CImageList m_ImageListControlList;         //ControlList上的图标
	HICON     m_hIcon;
	virtual BOOL OnInitDialog();
	afx_msg void OnClose();
	HTREEITEM	m_hRoot;
	HTREEITEM	HKLM;
	HTREEITEM	HKCR;
	HTREEITEM	HKCU;
	HTREEITEM	HKUS;
	HTREEITEM	HKCC;
	HTREEITEM   m_hSelectedItem;
	BOOL m_isEnable;
	char GetFatherPath(CString& strFullPath);
	CString GetFullPath(HTREEITEM hCurrent);
	afx_msg void OnTvnSelchangedTree(NMHDR *pNMHDR, LRESULT *pResult);
	void OnReceiveComplete(void);
	void AddPath(char* szBuffer);
	void AddKey(char* szBuffer);
};
