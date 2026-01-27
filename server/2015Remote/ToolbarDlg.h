#pragma once
#include "Resource.h"
#include "LangManager.h"

class CScreenSpyDlg;

class CToolbarDlg : public CDialogLangEx
{
    DECLARE_DYNAMIC(CToolbarDlg)
private:
    int m_lastY = 0; // 记录上一次的 Y 坐标

public:
    CScreenSpyDlg* m_pParent = nullptr;
    CToolbarDlg(CScreenSpyDlg* pParent = nullptr);
    virtual ~CToolbarDlg();

    enum { IDD = IDD_TOOLBAR_DLG };

    int m_nHeight = 40;
    bool m_bVisible = false;
    bool m_bLocked = false;     // 是否锁定工具栏
    bool m_bOnTop = true;       // 是否在屏幕上方 (默认上方)
    int m_nOpacityLevel = 0;    // 透明度级别 (0=100%, 1=75%, 2=50%)

    void SlideIn();
    void SlideOut();
    void CheckMousePosition();
    void UpdatePosition();      // 更新工具栏位置
    void LoadSettings();        // 从注册表加载设置
    void SaveSettings();        // 保存设置到注册表
    void ApplyOpacity();        // 应用透明度
    CString GetOpacityText();   // 获取透明度按钮文本

protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    DECLARE_MESSAGE_MAP()

public:
    afx_msg void OnBnClickedExitFullscreen();
    afx_msg void OnBnClickedCtrl();
    afx_msg void OnBnClickedMinimize();
    afx_msg void OnBnClickedClose();
    afx_msg void OnBnClickedLock();
    afx_msg void OnBnClickedPosition();
    afx_msg void OnBnClickedOpacity();
    afx_msg void OnBnClickedScreenshot();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    virtual BOOL OnInitDialog();
};
