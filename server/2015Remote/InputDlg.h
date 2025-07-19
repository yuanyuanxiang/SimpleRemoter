#pragma once

#include "resource.h"

// CInputDialog 对话框

class CInputDialog : public CDialogEx
{
	DECLARE_DYNAMIC(CInputDialog)

public:
	CInputDialog(CWnd* pParent = nullptr);   // 标准构造函数
	virtual ~CInputDialog();

	BOOL Init(LPCTSTR caption, LPCTSTR prompt);

	void Init2(LPCTSTR name, LPCTSTR defaultValue);

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_DIALOG_INPUT };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()

	HICON m_hIcon;
	CString m_sCaption;
	CString m_sPrompt;

public:
	CString m_str;
	virtual BOOL OnInitDialog();
	afx_msg void OnBnClickedOk();
	CStatic m_Static2thInput;
	CEdit m_Edit2thInput;
	CString m_sItemName;
	CString m_sSecondInput;
};
