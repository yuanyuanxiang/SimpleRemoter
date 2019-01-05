// CursorInfor.h: interface for the CCursorInfor class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_CURSORINFOR_H__ABC3705B_9461_4A94_B825_26539717C0D6__INCLUDED_)
#define AFX_CURSORINFOR_H__ABC3705B_9461_4A94_B825_26539717C0D6__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#define MAX_CURSOR_TYPE 16
class CCursorInfor  
{
public:
	CCursorInfor();
	virtual ~CCursorInfor();

	int GetCurrentCursorIndex();
};

#endif // !defined(AFX_CURSORINFOR_H__ABC3705B_9461_4A94_B825_26539717C0D6__INCLUDED_)
