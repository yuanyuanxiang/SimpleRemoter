#include "stdafx.h"
#include "CIconButton.h"

IMPLEMENT_DYNAMIC(CIconButton, CButton)

BEGIN_MESSAGE_MAP(CIconButton, CButton)
    ON_WM_MOUSEMOVE()
    ON_WM_MOUSELEAVE()
    ON_WM_ERASEBKGND()
END_MESSAGE_MAP()

// Colors
static const COLORREF CLR_NORMAL  = RGB(50, 50, 50);
static const COLORREF CLR_HOVER   = RGB(70, 70, 70);
static const COLORREF CLR_PRESSED = RGB(90, 90, 90);
static const COLORREF CLR_CLOSE_HOVER = RGB(196, 43, 28);
static const COLORREF CLR_ICON    = RGB(255, 255, 255);

CIconButton::CIconButton()
    : m_fnDrawIcon(nullptr)
    , m_bHover(false)
    , m_bIsCloseButton(false)
    , m_bTracking(false)
{
}

CIconButton::~CIconButton()
{
}

void CIconButton::PreSubclassWindow()
{
    // Ensure BS_OWNERDRAW is set
    ModifyStyle(0, BS_OWNERDRAW);
    CButton::PreSubclassWindow();
}

void CIconButton::DrawItem(LPDRAWITEMSTRUCT lpDIS)
{
    CDC* pDC = CDC::FromHandle(lpDIS->hDC);
    CRect rc(lpDIS->rcItem);
    bool bPressed = (lpDIS->itemState & ODS_SELECTED) != 0;

    // Pick background color
    COLORREF clrBg = CLR_NORMAL;
    if (bPressed) {
        clrBg = CLR_PRESSED;
    } else if (m_bHover) {
        clrBg = m_bIsCloseButton ? CLR_CLOSE_HOVER : CLR_HOVER;
    }

    // Draw rounded-rect background
    CBrush brush(clrBg);
    CPen pen(PS_SOLID, 1, clrBg);
    CPen* pOldPen = pDC->SelectObject(&pen);
    CBrush* pOldBrush = pDC->SelectObject(&brush);
    pDC->RoundRect(rc, CPoint(4, 4));
    pDC->SelectObject(pOldBrush);
    pDC->SelectObject(pOldPen);

    // Draw icon in a centered 16x16 area
    if (m_fnDrawIcon) {
        int iconSize = 16;
        CRect rcIcon;
        rcIcon.left   = rc.left + (rc.Width() - iconSize) / 2;
        rcIcon.top    = rc.top  + (rc.Height() - iconSize) / 2;
        rcIcon.right  = rcIcon.left + iconSize;
        rcIcon.bottom = rcIcon.top  + iconSize;
        m_fnDrawIcon(pDC, rcIcon);
    }
}

void CIconButton::OnMouseMove(UINT nFlags, CPoint point)
{
    if (!m_bTracking) {
        TRACKMOUSEEVENT tme = {};
        tme.cbSize = sizeof(tme);
        tme.dwFlags = TME_LEAVE;
        tme.hwndTrack = m_hWnd;
        TrackMouseEvent(&tme);
        m_bTracking = true;
    }
    if (!m_bHover) {
        m_bHover = true;
        Invalidate(FALSE);
    }
    CButton::OnMouseMove(nFlags, point);
}

void CIconButton::OnMouseLeave()
{
    m_bHover = false;
    m_bTracking = false;
    Invalidate(FALSE);
    CButton::OnMouseLeave();
}

BOOL CIconButton::OnEraseBkgnd(CDC* pDC)
{
    return TRUE; // prevent flicker
}

// ====================================================================
// Static icon drawing functions
// All draw white shapes within the given 16x16 CRect.
// ====================================================================

static CPen* SelectWhitePen(CDC* pDC, CPen& pen, int width = 2)
{
    pen.CreatePen(PS_SOLID, width, CLR_ICON);
    return pDC->SelectObject(&pen);
}

// Exit Fullscreen: 4 inward-pointing corner arrows
void CIconButton::DrawIconExitFullscreen(CDC* pDC, const CRect& rc)
{
    CPen pen;
    CPen* pOld = SelectWhitePen(pDC, pen, 2);
    int m = 3; // margin from edge

    // Top-left corner L-shape (pointing inward)
    pDC->MoveTo(rc.left + m, rc.top + m + 5);
    pDC->LineTo(rc.left + m, rc.top + m);
    pDC->LineTo(rc.left + m + 5, rc.top + m);

    // Top-right corner
    pDC->MoveTo(rc.right - m - 5, rc.top + m);
    pDC->LineTo(rc.right - m, rc.top + m);
    pDC->LineTo(rc.right - m, rc.top + m + 5);

    // Bottom-left corner
    pDC->MoveTo(rc.left + m, rc.bottom - m - 5);
    pDC->LineTo(rc.left + m, rc.bottom - m);
    pDC->LineTo(rc.left + m + 5, rc.bottom - m);

    // Bottom-right corner
    pDC->MoveTo(rc.right - m - 5, rc.bottom - m);
    pDC->LineTo(rc.right - m, rc.bottom - m);
    pDC->LineTo(rc.right - m, rc.bottom - m - 5);

    // Inner square
    int inset = 5;
    CRect inner(rc.left + inset, rc.top + inset, rc.right - inset, rc.bottom - inset);
    pDC->SelectStockObject(NULL_BRUSH);
    pDC->Rectangle(inner);

    pDC->SelectObject(pOld);
}

// Play: filled right-pointing triangle
void CIconButton::DrawIconPlay(CDC* pDC, const CRect& rc)
{
    CBrush brush(CLR_ICON);
    CBrush* pOldBrush = pDC->SelectObject(&brush);
    CPen pen(PS_SOLID, 1, CLR_ICON);
    CPen* pOldPen = pDC->SelectObject(&pen);

    int cx = rc.CenterPoint().x;
    int cy = rc.CenterPoint().y;
    POINT pts[3] = {
        { rc.left + 3, rc.top + 2 },
        { rc.right - 2, cy },
        { rc.left + 3, rc.bottom - 2 }
    };
    pDC->Polygon(pts, 3);

    pDC->SelectObject(pOldBrush);
    pDC->SelectObject(pOldPen);
}

// Pause: two vertical bars
void CIconButton::DrawIconPause(CDC* pDC, const CRect& rc)
{
    CBrush brush(CLR_ICON);
    int barW = 3;
    int gap = 3;
    int totalW = barW * 2 + gap;
    int x = rc.left + (rc.Width() - totalW) / 2;
    int y1 = rc.top + 2;
    int y2 = rc.bottom - 2;

    pDC->FillSolidRect(x, y1, barW, y2 - y1, CLR_ICON);
    pDC->FillSolidRect(x + barW + gap, y1, barW, y2 - y1, CLR_ICON);
}

// Lock: closed padlock (U-shackle + body)
void CIconButton::DrawIconLock(CDC* pDC, const CRect& rc)
{
    CPen pen;
    CPen* pOld = SelectWhitePen(pDC, pen, 2);

    int cx = rc.CenterPoint().x;
    // Shackle (arc)
    pDC->Arc(cx - 4, rc.top + 1, cx + 4, rc.top + 9, cx + 4, rc.top + 5, cx - 4, rc.top + 5);

    // Body rectangle
    CBrush brush(CLR_ICON);
    CBrush* pOldBrush = pDC->SelectObject(&brush);
    pDC->Rectangle(cx - 5, rc.top + 7, cx + 5, rc.bottom - 1);
    pDC->SelectObject(pOldBrush);
    pDC->SelectObject(pOld);
}

// Unlock: open padlock
void CIconButton::DrawIconUnlock(CDC* pDC, const CRect& rc)
{
    CPen pen;
    CPen* pOld = SelectWhitePen(pDC, pen, 2);

    int cx = rc.CenterPoint().x;
    // Shackle (open - shifted up and right)
    pDC->Arc(cx - 4, rc.top - 1, cx + 4, rc.top + 7, cx + 4, rc.top + 3, cx - 4, rc.top + 3);

    // Body rectangle
    CBrush brush(CLR_ICON);
    CBrush* pOldBrush = pDC->SelectObject(&brush);
    pDC->Rectangle(cx - 5, rc.top + 7, cx + 5, rc.bottom - 1);
    pDC->SelectObject(pOldBrush);
    pDC->SelectObject(pOld);
}

// Arrow Down: chevron pointing down
void CIconButton::DrawIconArrowDown(CDC* pDC, const CRect& rc)
{
    CPen pen;
    CPen* pOld = SelectWhitePen(pDC, pen, 2);
    int cx = rc.CenterPoint().x;
    int cy = rc.CenterPoint().y;
    pDC->MoveTo(rc.left + 3, cy - 3);
    pDC->LineTo(cx, cy + 3);
    pDC->LineTo(rc.right - 3, cy - 3);
    pDC->SelectObject(pOld);
}

// Arrow Up: chevron pointing up
void CIconButton::DrawIconArrowUp(CDC* pDC, const CRect& rc)
{
    CPen pen;
    CPen* pOld = SelectWhitePen(pDC, pen, 2);
    int cx = rc.CenterPoint().x;
    int cy = rc.CenterPoint().y;
    pDC->MoveTo(rc.left + 3, cy + 3);
    pDC->LineTo(cx, cy - 3);
    pDC->LineTo(rc.right - 3, cy + 3);
    pDC->SelectObject(pOld);
}

// Arrow Left: chevron pointing left
void CIconButton::DrawIconArrowLeft(CDC* pDC, const CRect& rc)
{
    CPen pen;
    CPen* pOld = SelectWhitePen(pDC, pen, 2);
    int cx = rc.CenterPoint().x;
    int cy = rc.CenterPoint().y;
    pDC->MoveTo(cx + 3, rc.top + 3);
    pDC->LineTo(cx - 3, cy);
    pDC->LineTo(cx + 3, rc.bottom - 3);
    pDC->SelectObject(pOld);
}

// Arrow Right: chevron pointing right
void CIconButton::DrawIconArrowRight(CDC* pDC, const CRect& rc)
{
    CPen pen;
    CPen* pOld = SelectWhitePen(pDC, pen, 2);
    int cx = rc.CenterPoint().x;
    int cy = rc.CenterPoint().y;
    pDC->MoveTo(cx - 3, rc.top + 3);
    pDC->LineTo(cx + 3, cy);
    pDC->LineTo(cx - 3, rc.bottom - 3);
    pDC->SelectObject(pOld);
}

// Opacity Full: filled circle
void CIconButton::DrawIconOpacityFull(CDC* pDC, const CRect& rc)
{
    CBrush brush(CLR_ICON);
    CPen pen(PS_SOLID, 1, CLR_ICON);
    CPen* pOldPen = pDC->SelectObject(&pen);
    CBrush* pOldBrush = pDC->SelectObject(&brush);
    int inset = 3;
    pDC->Ellipse(rc.left + inset, rc.top + inset, rc.right - inset, rc.bottom - inset);
    pDC->SelectObject(pOldBrush);
    pDC->SelectObject(pOldPen);
}

// Opacity Medium: circle outline + center dot
void CIconButton::DrawIconOpacityMedium(CDC* pDC, const CRect& rc)
{
    CPen pen;
    CPen* pOld = SelectWhitePen(pDC, pen, 2);
    pDC->SelectStockObject(NULL_BRUSH);
    int inset = 3;
    pDC->Ellipse(rc.left + inset, rc.top + inset, rc.right - inset, rc.bottom - inset);

    // Center dot
    int cx = rc.CenterPoint().x;
    int cy = rc.CenterPoint().y;
    CBrush dotBrush(CLR_ICON);
    CRect dotRc(cx - 2, cy - 2, cx + 2, cy + 2);
    pDC->FillSolidRect(dotRc, CLR_ICON);

    pDC->SelectObject(pOld);
}

// Opacity Low: circle outline only
void CIconButton::DrawIconOpacityLow(CDC* pDC, const CRect& rc)
{
    CPen pen;
    CPen* pOld = SelectWhitePen(pDC, pen, 2);
    pDC->SelectStockObject(NULL_BRUSH);
    int inset = 3;
    pDC->Ellipse(rc.left + inset, rc.top + inset, rc.right - inset, rc.bottom - inset);
    pDC->SelectObject(pOld);
}

// Screenshot: camera outline
void CIconButton::DrawIconScreenshot(CDC* pDC, const CRect& rc)
{
    CPen pen;
    CPen* pOld = SelectWhitePen(pDC, pen, 2);
    pDC->SelectStockObject(NULL_BRUSH);

    // Camera body
    pDC->RoundRect(rc.left + 1, rc.top + 4, rc.right - 1, rc.bottom - 1, 3, 3);

    // Lens (circle)
    int cx = rc.CenterPoint().x;
    int cy = (rc.top + 4 + rc.bottom - 1) / 2;
    pDC->Ellipse(cx - 3, cy - 3, cx + 3, cy + 3);

    // Flash bump on top
    pDC->MoveTo(cx - 2, rc.top + 4);
    pDC->LineTo(cx - 4, rc.top + 1);
    pDC->LineTo(cx + 4, rc.top + 1);
    pDC->LineTo(cx + 2, rc.top + 4);

    pDC->SelectObject(pOld);
}

// Switch Screen: two overlapping rectangles with arrow
void CIconButton::DrawIconSwitchScreen(CDC* pDC, const CRect& rc)
{
    CPen pen;
    CPen* pOld = SelectWhitePen(pDC, pen, 2);
    pDC->SelectStockObject(NULL_BRUSH);

    // Back rectangle (offset top-left)
    pDC->Rectangle(rc.left + 1, rc.top + 1, rc.right - 4, rc.bottom - 4);
    // Front rectangle (offset bottom-right)
    pDC->Rectangle(rc.left + 4, rc.top + 4, rc.right - 1, rc.bottom - 1);

    pDC->SelectObject(pOld);
}

// Block Input: mouse with X overlay
void CIconButton::DrawIconBlockInput(CDC* pDC, const CRect& rc)
{
    CPen pen;
    CPen* pOld = SelectWhitePen(pDC, pen, 2);

    // Mouse body outline
    int cx = rc.CenterPoint().x;
    pDC->SelectStockObject(NULL_BRUSH);
    pDC->RoundRect(cx - 4, rc.top + 1, cx + 4, rc.bottom - 1, 4, 4);
    // Mouse wheel line
    pDC->MoveTo(cx, rc.top + 4);
    pDC->LineTo(cx, rc.top + 7);

    // X overlay (block)
    pDC->MoveTo(rc.left + 2, rc.top + 2);
    pDC->LineTo(rc.right - 2, rc.bottom - 2);
    pDC->MoveTo(rc.right - 2, rc.top + 2);
    pDC->LineTo(rc.left + 2, rc.bottom - 2);

    pDC->SelectObject(pOld);
}

// Unblock Input: mouse without X
void CIconButton::DrawIconUnblockInput(CDC* pDC, const CRect& rc)
{
    CPen pen;
    CPen* pOld = SelectWhitePen(pDC, pen, 2);

    // Mouse body outline
    int cx = rc.CenterPoint().x;
    pDC->SelectStockObject(NULL_BRUSH);
    pDC->RoundRect(cx - 4, rc.top + 1, cx + 4, rc.bottom - 1, 4, 4);
    // Mouse wheel line
    pDC->MoveTo(cx, rc.top + 4);
    pDC->LineTo(cx, rc.top + 7);

    pDC->SelectObject(pOld);
}

// Quality: gear/cog shape (simplified as sliders icon)
void CIconButton::DrawIconQuality(CDC* pDC, const CRect& rc)
{
    CPen pen;
    CPen* pOld = SelectWhitePen(pDC, pen, 2);

    // Three horizontal slider lines with knobs at different positions
    int x1 = rc.left + 2, x2 = rc.right - 2;
    int y1 = rc.top + 3, y2 = rc.CenterPoint().y, y3 = rc.bottom - 3;

    // Line 1 with knob left
    pDC->MoveTo(x1, y1); pDC->LineTo(x2, y1);
    pDC->FillSolidRect(x1 + 2, y1 - 2, 4, 4, CLR_ICON);

    // Line 2 with knob right
    pDC->MoveTo(x1, y2); pDC->LineTo(x2, y2);
    pDC->FillSolidRect(x2 - 6, y2 - 2, 4, 4, CLR_ICON);

    // Line 3 with knob middle
    pDC->MoveTo(x1, y3); pDC->LineTo(x2, y3);
    int mid = (x1 + x2) / 2;
    pDC->FillSolidRect(mid - 2, y3 - 2, 4, 4, CLR_ICON);

    pDC->SelectObject(pOld);
}

// Minimize: horizontal dash
void CIconButton::DrawIconMinimize(CDC* pDC, const CRect& rc)
{
    int cy = rc.CenterPoint().y;
    pDC->FillSolidRect(rc.left + 3, cy - 1, rc.Width() - 6, 2, CLR_ICON);
}

// Close: X mark
void CIconButton::DrawIconClose(CDC* pDC, const CRect& rc)
{
    CPen pen;
    CPen* pOld = SelectWhitePen(pDC, pen, 2);
    int m = 3;
    pDC->MoveTo(rc.left + m, rc.top + m);
    pDC->LineTo(rc.right - m, rc.bottom - m);
    pDC->MoveTo(rc.right - m, rc.top + m);
    pDC->LineTo(rc.left + m, rc.bottom - m);
    pDC->SelectObject(pOld);
}
