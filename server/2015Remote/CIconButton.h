#pragma once

// CIconButton - Owner-draw button with geometric icon drawing
// Used by the fullscreen toolbar for a modern, icon-based appearance.

#include <functional>

class CIconButton : public CButton
{
    DECLARE_DYNAMIC(CIconButton)

public:
    // Icon drawing function: receives CDC* and icon bounding CRect
    typedef void (*IconDrawFunc)(CDC* pDC, const CRect& rc);

    CIconButton();
    virtual ~CIconButton();

    void SetIconDrawFunc(IconDrawFunc fn) { m_fnDrawIcon = fn; }
    void SetIsCloseButton(bool b) { m_bIsCloseButton = b; }

    // --- Static icon draw functions ---
    static void DrawIconExitFullscreen(CDC* pDC, const CRect& rc);
    static void DrawIconPlay(CDC* pDC, const CRect& rc);
    static void DrawIconPause(CDC* pDC, const CRect& rc);
    static void DrawIconLock(CDC* pDC, const CRect& rc);
    static void DrawIconUnlock(CDC* pDC, const CRect& rc);
    static void DrawIconArrowDown(CDC* pDC, const CRect& rc);
    static void DrawIconArrowUp(CDC* pDC, const CRect& rc);
    static void DrawIconArrowLeft(CDC* pDC, const CRect& rc);
    static void DrawIconArrowRight(CDC* pDC, const CRect& rc);
    static void DrawIconOpacityFull(CDC* pDC, const CRect& rc);
    static void DrawIconOpacityMedium(CDC* pDC, const CRect& rc);
    static void DrawIconOpacityLow(CDC* pDC, const CRect& rc);
    static void DrawIconScreenshot(CDC* pDC, const CRect& rc);
    static void DrawIconSwitchScreen(CDC* pDC, const CRect& rc);
    static void DrawIconBlockInput(CDC* pDC, const CRect& rc);
    static void DrawIconUnblockInput(CDC* pDC, const CRect& rc);
    static void DrawIconQuality(CDC* pDC, const CRect& rc);
    static void DrawIconRestoreConsole(CDC* pDC, const CRect& rc);
    static void DrawIconMinimize(CDC* pDC, const CRect& rc);
    static void DrawIconClose(CDC* pDC, const CRect& rc);
    static void DrawIconInfo(CDC* pDC, const CRect& rc);
    static void DrawIconInfoHide(CDC* pDC, const CRect& rc);

protected:
    virtual void DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct);
    virtual void PreSubclassWindow();

    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    afx_msg void OnMouseLeave();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);

    DECLARE_MESSAGE_MAP()

private:
    IconDrawFunc m_fnDrawIcon;
    bool m_bHover;
    bool m_bIsCloseButton;
    bool m_bTracking;
};
