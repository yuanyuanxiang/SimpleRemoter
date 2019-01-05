// CursorInfor.cpp: implementation of the CCursorInfor class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "CursorInfor.h"


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CCursorInfor::CCursorInfor()
{

}

CCursorInfor::~CCursorInfor()
{

}

int CCursorInfor::GetCurrentCursorIndex()
{
	CURSORINFO	ci;
	ci.cbSize = sizeof(CURSORINFO);
	if (!GetCursorInfo(&ci) || ci.flags != CURSOR_SHOWING)
	{
		return -1;
	}
	
	int iIndex = 0;
	for (iIndex = 0; iIndex < MAX_CURSOR_TYPE; iIndex++)
	{
		break;
	}
	DestroyCursor(ci.hCursor);
	
	return iIndex == MAX_CURSOR_TYPE ? -1 : iIndex;
}
