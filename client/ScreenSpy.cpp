// ScreenSpy.cpp: implementation of the CScreenSpy class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ScreenSpy.h"
#include "Common.h"
#include <stdio.h>

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CScreenSpy::CScreenSpy(ULONG ulbiBitCount, BYTE algo, BOOL vDesk, int gop, BOOL all) : 
	ScreenCapture(ulbiBitCount, algo, all)
{
	m_GOP = gop;

	m_BitmapInfor_Full = new BITMAPINFO();
	memset(m_BitmapInfor_Full, 0, sizeof(BITMAPINFO));
	BITMAPINFOHEADER* BitmapInforHeader = &(m_BitmapInfor_Full->bmiHeader);
	BitmapInforHeader->biSize = sizeof(BITMAPINFOHEADER);
	BitmapInforHeader->biWidth = m_ulFullWidth; //1080
	BitmapInforHeader->biHeight = m_ulFullHeight; //1920
	BitmapInforHeader->biPlanes = 1;
	BitmapInforHeader->biBitCount = ulbiBitCount; //通常为32
	BitmapInforHeader->biCompression = BI_RGB;
	BitmapInforHeader->biSizeImage =
		((BitmapInforHeader->biWidth * BitmapInforHeader->biBitCount + 31) / 32) * 4 * BitmapInforHeader->biHeight;

	m_hDeskTopDC = GetDC(NULL);

	m_BitmapData_Full = NULL;
	m_hFullMemDC = CreateCompatibleDC(m_hDeskTopDC);
	m_BitmapHandle	= ::CreateDIBSection(m_hDeskTopDC, m_BitmapInfor_Full, DIB_RGB_COLORS, &m_BitmapData_Full, NULL, NULL);
	::SelectObject(m_hFullMemDC, m_BitmapHandle);
	m_FirstBuffer = (LPBYTE)m_BitmapData_Full;

	m_DiffBitmapData_Full = NULL;
	m_hDiffMemDC	= CreateCompatibleDC(m_hDeskTopDC); 
	m_DiffBitmapHandle	= ::CreateDIBSection(m_hDeskTopDC, m_BitmapInfor_Full, DIB_RGB_COLORS, &m_DiffBitmapData_Full, NULL, NULL);
	::SelectObject(m_hDiffMemDC, m_DiffBitmapHandle);

	m_RectBuffer = new BYTE[m_BitmapInfor_Full->bmiHeader.biSizeImage * 2 + 12];

	m_bVirtualPaint = vDesk;
	m_data.Create(m_hDeskTopDC, m_iScreenX, m_iScreenY, m_ulFullWidth, m_ulFullHeight);
}


CScreenSpy::~CScreenSpy()
{
	if (m_BitmapInfor_Full != NULL)
	{
		delete m_BitmapInfor_Full;
		m_BitmapInfor_Full = NULL;
	}

	ReleaseDC(NULL, m_hDeskTopDC);

	if (m_hFullMemDC!=NULL)
	{
		DeleteDC(m_hFullMemDC);

		::DeleteObject(m_BitmapHandle);
		if (m_BitmapData_Full!=NULL)
		{
			m_BitmapData_Full = NULL;
		}

		m_hFullMemDC = NULL;
	}

	if (m_hDiffMemDC!=NULL)
	{
		DeleteDC(m_hDiffMemDC);

		::DeleteObject(m_DiffBitmapHandle);
		if (m_DiffBitmapData_Full!=NULL)
		{
			m_DiffBitmapData_Full = NULL;
		}
	}

	if (m_RectBuffer)
	{
		delete[] m_RectBuffer;
		m_RectBuffer = NULL;
	}
}

LPBYTE CScreenSpy::GetFirstScreenData(ULONG* ulFirstScreenLength)
{
	ScanScreen(m_hFullMemDC, m_hDeskTopDC, m_ulFullWidth, m_ulFullHeight);
	m_RectBuffer[0] = TOKEN_FIRSTSCREEN;
	memcpy(1 + m_RectBuffer, m_BitmapData_Full, m_BitmapInfor_Full->bmiHeader.biSizeImage);
	if (m_bAlgorithm == ALGORITHM_GRAY) {
		ToGray(1 + m_RectBuffer, 1 + m_RectBuffer, m_BitmapInfor_Full->bmiHeader.biSizeImage);
	}
	*ulFirstScreenLength = m_BitmapInfor_Full->bmiHeader.biSizeImage;

	return m_RectBuffer;  //内存
}


VOID CScreenSpy::ScanScreen(HDC hdcDest, HDC hdcSour, ULONG ulWidth, ULONG ulHeight)
{
	if (m_bVirtualPaint){
		int n = 0;
		if (n = EnumWindowsTopToDown(NULL, EnumHwndsPrint, (LPARAM)&m_data.SetScreenDC(hdcDest))) {
			Mprintf("EnumWindowsTopToDown failed: %d!!!\n", n);
		}
		return;
	}
	AUTO_TICK(70);
#if COPY_ALL
	BitBlt(hdcDest, 0, 0, ulWidth, ulHeight, hdcSour, m_iScreenX, m_iScreenY, SRCCOPY);
#else
	const ULONG	ulJumpLine = 50;
	const ULONG	ulJumpSleep = ulJumpLine / 10; 

	for (int i = 0, ulToJump = 0; i < ulHeight; i += ulToJump)
	{
		ULONG  ulv1 = ulHeight - i;  

		if (ulv1 > ulJumpLine)  
			ulToJump = ulJumpLine; 
		else
			ulToJump = ulv1;
		BitBlt(hdcDest, 0, i, ulWidth, ulToJump, hdcSour,0, i, SRCCOPY);
		Sleep(ulJumpSleep);
	}
#endif
}
