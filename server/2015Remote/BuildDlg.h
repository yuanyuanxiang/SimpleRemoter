#pragma once


// CBuildDlg 对话框

class CBuildDlg : public CDialog
{
	DECLARE_DYNAMIC(CBuildDlg)

public:
	CBuildDlg(CWnd* pParent = NULL);   // 标准构造函数
	virtual ~CBuildDlg();

// 对话框数据
	enum { IDD = IDD_DIALOG_BUILD };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()
public:
	CString m_strIP;
	CString m_strPort;
	afx_msg void OnBnClickedOk();
};
