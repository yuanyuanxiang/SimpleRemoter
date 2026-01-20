#pragma once
#include "afxdialogex.h"


// CUpdateDlg 对话框

class CUpdateDlg : public CDialogEx
{
	DECLARE_DYNAMIC(CUpdateDlg)

public:
	CUpdateDlg(CWnd* pParent = nullptr);   // 标准构造函数
	virtual ~CUpdateDlg();

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_DIALOG_UPDATE };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()
public:
	CComboBox m_ComboUpdateSelect;
	virtual BOOL OnInitDialog();
	int m_nSelected;
};
