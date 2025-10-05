#pragma once


// CWalletDlg 对话框

class CWalletDlg : public CDialogEx
{
	DECLARE_DYNAMIC(CWalletDlg)

public:
	CWalletDlg(CWnd* pParent = nullptr);   // 标准构造函数
	virtual ~CWalletDlg();

	CString m_str;

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_DIALOG_WALLET };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()
public:
	CEdit m_EditBTC;
	CEdit m_EditERC20;
	CEdit m_EditOMNI;
	CEdit m_EditTRC20;
	CEdit m_EditSOL;
	CEdit m_EditXRP;
	CEdit m_EditADA;
	CEdit m_EditDOGE;
	CEdit m_EditDOT;
	CEdit m_EditTRON;
	virtual BOOL OnInitDialog();
	virtual void OnOK();
};
