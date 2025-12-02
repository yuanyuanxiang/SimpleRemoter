#pragma once

// 启动画面对话框 - 显示加载进度
class CSplashDlg : public CWnd
{
public:
    CSplashDlg();
    virtual ~CSplashDlg();

    // 创建并显示启动画面
    BOOL Create(CWnd* pParent = NULL);

    int SafeMessageBox(LPCTSTR lpszText, LPCTSTR lpszCaption, UINT nType);

    // 更新进度 (0-100) - 通过消息队列（用于跨线程）
    void SetProgress(int nPercent);

    // 更新状态文本 - 通过消息队列（用于跨线程）
    void SetStatusText(const CString& strText);

    // 直接更新进度和状态（同一线程使用，立即刷新）
    void UpdateProgressDirect(int nPercent, const CString& strText);

    // 关闭启动画面
    void Close();

protected:
    DECLARE_MESSAGE_MAP()

    afx_msg void OnPaint();
    afx_msg LRESULT OnUpdateProgress(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnUpdateStatus(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnCloseSplash(WPARAM wParam, LPARAM lParam);

private:
    int m_nProgress;           // 当前进度 0-100
    CString m_strStatus;       // 状态文本
    CFont m_fontTitle;         // 标题字体
    CFont m_fontStatus;        // 状态文本字体
    HICON m_hIcon;             // 图标

    // 窗口尺寸
    static const int SPLASH_WIDTH = 400;
    static const int SPLASH_HEIGHT = 200;

    // 自定义消息 (使用较大的偏移避免与其他消息冲突)
    static const UINT WM_SPLASH_PROGRESS = WM_USER + 5000;
    static const UINT WM_SPLASH_STATUS = WM_USER + 5001;
    static const UINT WM_SPLASH_CLOSE = WM_USER + 5002;

    // 绘制相关
    void DrawProgressBar(CDC* pDC, const CRect& rect);
};
