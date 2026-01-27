#pragma once
#include "afxdialogex.h"
#include "HostInfo.h"
#include "Resource.h"
#include <vector>
#include <algorithm>
#include <map>
#include "2015RemoteDlg.h"
#include "CListCtrlEx.h"

// 分组键：支持任意字段组合
struct GroupKey {
	std::vector<CString> Values;

	bool operator<(const GroupKey& other) const {
		size_t count = (std::min)(Values.size(), other.Values.size());
		for (size_t i = 0; i < count; i++) {
			int cmp = Values[i].Compare(other.Values[i]);
			if (cmp != 0) return cmp < 0;
		}
		return Values.size() < other.Values.size();
	}
};

// 分组信息
struct GroupInfo {
	int GroupId;
	BOOL bExpanded;
	std::vector<std::pair<ClientKey, ClientValue>> Clients;
};

// CClientListDlg 对话框

class CClientListDlg : public CDialogLangEx
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
	std::map<GroupKey, GroupInfo> m_groups;  // 分组数据
	int m_nSortColumn;      // 当前排序列
	BOOL m_bSortAscending;  // 是否升序
	CToolTipCtrl m_ToolTip;
	int m_nTipItem;         // 当前提示的行
	int m_nTipSubItem;      // 当前提示的列

	void BuildGroups();     // 构建分组数据

	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()
public:
	CListCtrlEx m_ClientList;
	virtual BOOL OnInitDialog();
	void RefreshClientList();
	void DisplayClients();
	void AdjustColumnWidths();
	void SortByColumn(int nColumn, BOOL bAscending);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnColumnClick(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnListClick(NMHDR* pNMHDR, LRESULT* pResult);
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	virtual void OnCancel();
	virtual void PostNcDestroy();
	virtual void OnOK();
};
