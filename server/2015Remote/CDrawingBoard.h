#pragma once
#include "IOCPServer.h"
#include "afxwin.h"

// CDrawingBoard 对话框

class CDrawingBoard : public DialogBase
{
    DECLARE_DYNAMIC(CDrawingBoard)

public:
    CDrawingBoard(CWnd* pParent = nullptr, Server* IOCPServer = NULL, CONTEXT_OBJECT* ContextObject = NULL);
    virtual ~CDrawingBoard();

#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_DRAWING_BOARD };
#endif

    VOID OnReceiveComplete();

    afx_msg void OnPaint();
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
    afx_msg void OnWindowPosChanged(WINDOWPOS* lpwndpos);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnRButtonDown(UINT nFlags, CPoint point);

protected:
    virtual void DoDataExchange(CDataExchange* pDX);

    void OnClose();

    DECLARE_MESSAGE_MAP()

    BOOL PreTranslateMessage(MSG* pMsg);

    void SendTextsToRemote(const CPoint& pt, const CString& inputText);

private:
    bool m_bTopMost;							 // 置顶
    bool m_bTransport;							 // 半透明
    bool m_bMoving;								 // 位置跟随
    bool m_bSizing;								 // 大小跟随
    bool m_bDrawing;                             // 是否正在绘图
    std::vector<CPoint> m_currentPath;           // 当前路径点
    std::vector<std::vector<CPoint>> m_paths;    // 所有路径
    CPoint m_RightClickPos;						 // 右键点击位置
    CEdit* m_pInputEdit = nullptr;				 // 类成员变量
    std::vector<std::pair<CPoint, CString>> m_Texts;
    CFont m_font;								 // 添加字体成员
    CPen m_pen;									 // 画笔
public:
    afx_msg void OnDrawingTopmost();
    afx_msg void OnDrawingTransport();
    afx_msg void OnDrawingMove();
    afx_msg void OnDrawingSize();
    virtual BOOL OnInitDialog();
    afx_msg void OnDrawingClear();
    afx_msg void OnDrawingText();
};
