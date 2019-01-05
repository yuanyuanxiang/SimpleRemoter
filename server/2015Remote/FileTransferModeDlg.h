#pragma once


// CFileTransferModeDlg 对话框

class CFileTransferModeDlg : public CDialog
{
	DECLARE_DYNAMIC(CFileTransferModeDlg)

public:
	CFileTransferModeDlg(CWnd* pParent = NULL);   // 标准构造函数
	virtual ~CFileTransferModeDlg();

	CString m_strFileName;

	// 对话框数据
	enum { IDD = IDD_DIALOG_TRANSMODE };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持
	afx_msg	void OnEndDialog(UINT id);
	DECLARE_MESSAGE_MAP()
public:
	virtual BOOL OnInitDialog();
};
