#pragma once
#include "Resource.h"

class CToolbarDlg : public CDialogEx
{
	DECLARE_DYNAMIC(CToolbarDlg)
private:
	int m_lastY = 0; // 记录上一次的 Y 坐标

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
	afx_msg void OnBnClickedMinimize();
	afx_msg void OnBnClickedClose();
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	virtual BOOL OnInitDialog();
};
