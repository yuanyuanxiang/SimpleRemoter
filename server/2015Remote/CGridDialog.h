#pragma once
#include <vector>
#include <map>
#include <afxwin.h>

#define WM_CHILD_CLOSED (WM_USER + 100)

class CGridDialog : public CDialog
{
public:
	CGridDialog();

	BOOL AddChild(CDialog* pDlg);			// ��̬����ӶԻ���
	void RemoveChild(CDialog* pDlg);		// ��̬�Ƴ��ӶԻ���
	void SetGrid(int rows, int cols);		// ����������
	void LayoutChildren();					// ����
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

	// ������
	CDialog* m_pMaxChild = nullptr;  // ��ǰ��󻯵��ӶԻ���
	LONG m_parentStyle = 0;           // ������ԭʼ��ʽ

	struct ChildState {
		CRect rect;   // ԭʼλ��
		LONG style;   // ԭʼ������ʽ
	};
	std::map<CDialog*, ChildState> m_origState;
};
