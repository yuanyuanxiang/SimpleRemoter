#pragma once
#include "afxdialogex.h"
#include "HostInfo.h"
#include "Resource.h"
#include <vector>
#include <algorithm>
#include "2015RemoteDlg.h"

// CClientListDlg 对话框

class CClientListDlg : public CDialogEx
{
	DECLARE_DYNAMIC(CClientListDlg)

public:
	CClientListDlg(_ClientList* clients, CMy2015RemoteDlg* pParent = nullptr);
	virtual ~CClientListDlg();

	// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_DIALOG_CLIENTLIST };
#endif

protected:
	_ClientList* g_ClientList;
	CMy2015RemoteDlg* g_pParent;
	std::vector<std::pair<ClientKey, ClientValue>> m_clients;  // 数据副本
	int m_nSortColumn;      // 当前排序列
	BOOL m_bSortAscending;  // 是否升序

	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()
public:
	CListCtrl m_ClientList;
	virtual BOOL OnInitDialog();
	void RefreshClientList();
	void DisplayClients();
	void AdjustColumnWidths();
	void SortByColumn(int nColumn, BOOL bAscending);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnColumnClick(NMHDR* pNMHDR, LRESULT* pResult);
	virtual void OnCancel();
	virtual void PostNcDestroy();
	virtual void OnOK();
};
