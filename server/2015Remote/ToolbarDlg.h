#pragma once
#include "Resource.h"

class CToolbarDlg : public CDialogEx
{
	DECLARE_DYNAMIC(CToolbarDlg)

public:
	CToolbarDlg(CWnd* pParent = nullptr);
	virtual ~CToolbarDlg();

	enum { IDD = IDD_TOOLBAR_DLG };

	int m_nHeight = 40;
	bool m_bVisible = false;

	void SlideIn();
	void SlideOut();
	void CheckMousePosition();

protected:
	virtual void DoDataExchange(CDataExchange* pDX);
	DECLARE_MESSAGE_MAP()

public:
	afx_msg void OnBnClickedExitFullscreen();
	afx_msg void OnBnClickedCtrl();
	afx_msg void OnBnClickedClose();
	virtual BOOL OnInitDialog();
};
