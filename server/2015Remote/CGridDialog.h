#pragma once
#include <vector>
#include <map>
#include <afxwin.h>

#define WM_CHILD_CLOSED (WM_USER + 100)

class CGridDialog : public CDialog
{
public:
	CGridDialog();

	BOOL AddChild(CDialog* pDlg);			// 动态添加子对话框
	void RemoveChild(CDialog* pDlg);		// 动态移除子对话框
	void SetGrid(int rows, int cols);		// 设置行列数
	void LayoutChildren();					// 布局
	BOOL HasSlot() const {
		return m_children.size() < m_max;
	}

protected:
	virtual BOOL OnInitDialog();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	afx_msg LRESULT OnChildClosed(WPARAM wParam, LPARAM lParam);

	DECLARE_MESSAGE_MAP()

private:
	HICON m_hIcon;
	int m_rows = 0;
	int m_cols = 0;
	int m_max = 0;
	std::vector<CDialog*> m_children;

	// 最大化相关
	CDialog* m_pMaxChild = nullptr;  // 当前最大化的子对话框
	LONG m_parentStyle = 0;           // 父窗口原始样式

	struct ChildState {
		CRect rect;   // 原始位置
		LONG style;   // 原始窗口样式
	};
	std::map<CDialog*, ChildState> m_origState;
};
