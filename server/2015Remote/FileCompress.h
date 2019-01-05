#pragma once


// CFileCompress 对话框

class CFileCompress : public CDialog
{
	DECLARE_DYNAMIC(CFileCompress)

public:
	CFileCompress(CWnd* pParent = NULL,ULONG n = 0);   // 标准构造函数
	virtual ~CFileCompress();

// 对话框数据
	enum { IDD = IDD_DIALOG_FILE_COMPRESS };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()
public:
	CString m_EditRarName;
	ULONG   m_ulType;
	virtual BOOL OnInitDialog();
};
