// CursorInfor.h: interface for the CCursorInfor class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_CURSORINFOR_H__ABC3705B_9461_4A94_B825_26539717C0D6__INCLUDED_)
#define AFX_CURSORINFOR_H__ABC3705B_9461_4A94_B825_26539717C0D6__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

enum {
    USING_GDI = 0,
    USING_DXGI = 1,
    USING_VIRTUAL = 2,
};

#define ALGORITHM_GRAY 0
#define ALGORITHM_DIFF 1
#define ALGORITHM_DEFAULT 1
#define ALGORITHM_H264 2
#define ALGORITHM_HOME 3

#define MAX_CURSOR_TYPE	16

class CCursorInfo
{
private:
    LPCTSTR	m_CursorResArray[MAX_CURSOR_TYPE];
    HCURSOR	m_CursorHandleArray[MAX_CURSOR_TYPE];

public:
    CCursorInfo()
    {
        LPCTSTR	CursorResArray[MAX_CURSOR_TYPE] = {
            IDC_APPSTARTING,
            IDC_ARROW,
            IDC_CROSS,
            IDC_HAND,
            IDC_HELP,
            IDC_IBEAM,
            IDC_ICON,
            IDC_NO,
            IDC_SIZE,
            IDC_SIZEALL,
            IDC_SIZENESW,
            IDC_SIZENS,
            IDC_SIZENWSE,
            IDC_SIZEWE,
            IDC_UPARROW,
            IDC_WAIT
        };

        for (int i = 0; i < MAX_CURSOR_TYPE; ++i) {
            m_CursorResArray[i] = CursorResArray[i];
            m_CursorHandleArray[i] = LoadCursor(NULL, CursorResArray[i]);
        }
    }

    int getCurrentCursorIndex() const
    {
        CURSORINFO	ci;
        ci.cbSize = sizeof(CURSORINFO);
        if (!GetCursorInfo(&ci) || ci.flags != CURSOR_SHOWING)
            return -1;

        int i;
        for (i = 0; i < MAX_CURSOR_TYPE; ++i) {
            if (ci.hCursor == m_CursorHandleArray[i])
                break;
        }

        int	nIndex = i == MAX_CURSOR_TYPE ? -1 : i;
        return nIndex;
    }

    HCURSOR getCursorHandle( int nIndex ) const
    {
        return (nIndex >= 0 && nIndex < MAX_CURSOR_TYPE) ? m_CursorHandleArray[nIndex] : NULL;
    }
};


#endif // !defined(AFX_CURSORINFOR_H__ABC3705B_9461_4A94_B825_26539717C0D6__INCLUDED_)
