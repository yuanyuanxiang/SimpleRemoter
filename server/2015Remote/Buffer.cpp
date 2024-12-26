#include "StdAfx.h"
#include "Buffer.h"

#include <math.h>


#define U_PAGE_ALIGNMENT   3
#define F_PAGE_ALIGNMENT 3.0

CBuffer::CBuffer(void)
{
	m_ulMaxLength = 0;

	m_Ptr = m_Base = NULL;

	InitializeCriticalSection(&m_cs);
}

CBuffer::~CBuffer(void)
{
	if (m_Base)
	{
		VirtualFree(m_Base, 0, MEM_RELEASE);
		m_Base = NULL;
	}

	DeleteCriticalSection(&m_cs);

	m_Base = m_Ptr = NULL;
	m_ulMaxLength = 0;
}


ULONG CBuffer::RemoveCompletedBuffer(ULONG ulLength)
{
	EnterCriticalSection(&m_cs);

	if (ulLength > m_ulMaxLength)   //如果传进的长度比内存的长度还大
	{
		LeaveCriticalSection(&m_cs);
		return 0;
	}
	if (ulLength > ((ULONG)m_Ptr - (ULONG)m_Base))  //如果传进的长度 比有效的数据长度还大
	{
		ulLength = (ULONG)m_Ptr - (ULONG)m_Base;
	}

	if (ulLength)
	{
		MoveMemory(m_Base,m_Base+ulLength, m_ulMaxLength - ulLength);   //数组前移  [Shinexxxx??]

		m_Ptr -= ulLength;
	}

	DeAllocateBuffer((ULONG)m_Ptr - (ULONG)m_Base);
	LeaveCriticalSection(&m_cs);

	return ulLength;
}

ULONG CBuffer::ReadBuffer(PBYTE Buffer, ULONG ulLength)
{
	EnterCriticalSection(&m_cs);

	if (ulLength > m_ulMaxLength)
	{
		LeaveCriticalSection(&m_cs);
		return 0;
	}

	if (ulLength > ((ULONG)m_Ptr - (ULONG)m_Base))
	{
		ulLength = (ULONG)m_Ptr - (ULONG)m_Base;
	}

	if (ulLength)
	{
		CopyMemory(Buffer,m_Base,ulLength);  

		MoveMemory(m_Base,m_Base+ulLength, m_ulMaxLength - ulLength);
		m_Ptr -= ulLength;
	}

	DeAllocateBuffer((ULONG)m_Ptr - (ULONG)m_Base);

	LeaveCriticalSection(&m_cs);
	return ulLength;
}

// 私有: 无需加锁
ULONG CBuffer::DeAllocateBuffer(ULONG ulLength)        
{
	if (ulLength < ((ULONG)m_Ptr - (ULONG)m_Base))     
		return 0;

	ULONG ulNewMaxLength = (ULONG)ceil(ulLength / F_PAGE_ALIGNMENT) * U_PAGE_ALIGNMENT;  

	if (m_ulMaxLength <= ulNewMaxLength)
	{
		return 0;
	}
	PBYTE NewBase = (PBYTE) VirtualAlloc(NULL,ulNewMaxLength,MEM_COMMIT,PAGE_READWRITE);

	ULONG ulv1 = (ULONG)m_Ptr - (ULONG)m_Base;  //算原先内存的有效长度
	CopyMemory(NewBase,m_Base,ulv1);

	VirtualFree(m_Base,0,MEM_RELEASE);

	m_Base = NewBase;

	m_Ptr = m_Base + ulv1;

	m_ulMaxLength = ulNewMaxLength;

	return m_ulMaxLength;
}


BOOL CBuffer::WriteBuffer(PBYTE Buffer, ULONG ulLength)
{
	EnterCriticalSection(&m_cs);

	if (ReAllocateBuffer(ulLength + ((ULONG)m_Ptr - (ULONG)m_Base)) == -1)//10 +1   1024
	{
		LeaveCriticalSection(&m_cs);
		return false;
	}

	CopyMemory(m_Ptr,Buffer,ulLength);//Hello 5

	m_Ptr+=ulLength;
	LeaveCriticalSection(&m_cs);
	return TRUE;
}

// 私有: 无需加锁
ULONG CBuffer::ReAllocateBuffer(ULONG ulLength)
{
	if (ulLength < m_ulMaxLength)
		return 0;

	ULONG  ulNewMaxLength = (ULONG)ceil(ulLength / F_PAGE_ALIGNMENT) * U_PAGE_ALIGNMENT;  
	PBYTE  NewBase  = (PBYTE) VirtualAlloc(NULL,ulNewMaxLength,MEM_COMMIT,PAGE_READWRITE);
	if (NewBase == NULL)
	{
		return -1; 
	}


	ULONG ulv1 = (ULONG)m_Ptr - (ULONG)m_Base; //原先的有效数据长度
 
	CopyMemory(NewBase,m_Base,ulv1);

	if (m_Base)
	{
		VirtualFree(m_Base,0,MEM_RELEASE);
	}
	m_Base = NewBase;
	m_Ptr = m_Base + ulv1; //1024

	m_ulMaxLength = ulNewMaxLength;  //2048

	return m_ulMaxLength;
}

VOID CBuffer::ClearBuffer()
{
	EnterCriticalSection(&m_cs);
	m_Ptr = m_Base;

	DeAllocateBuffer(1024);  
	LeaveCriticalSection(&m_cs);
}

ULONG CBuffer::GetBufferLength() // 获得有效数据长度
{
	EnterCriticalSection(&m_cs);
	if (m_Base == NULL)
	{
		LeaveCriticalSection(&m_cs);
		return 0;
	}
	ULONG len = (ULONG)m_Ptr - (ULONG)m_Base;
	LeaveCriticalSection(&m_cs);

	return len;
}

// 此函数不是多线程安全的. 只在远程桌面使用了.
LPBYTE CBuffer::GetBuffer(ULONG ulPos)
{
	EnterCriticalSection(&m_cs);
	if (m_Base==NULL || ulPos >= ((ULONG)m_Ptr - (ULONG)m_Base))
	{
		LeaveCriticalSection(&m_cs);
		return NULL;
	}
	LPBYTE result = m_Base + ulPos;
	LeaveCriticalSection(&m_cs);

	return result;
}

// 此函数是多线程安全的. 获取缓存，得到Buffer对象.
Buffer CBuffer::GetMyBuffer(ULONG ulPos)
{
	EnterCriticalSection(&m_cs);
	ULONG len = (ULONG)m_Ptr - (ULONG)m_Base;
	if (m_Base == NULL || ulPos >= len)
	{
		LeaveCriticalSection(&m_cs);
		return Buffer();
	}
	Buffer result = Buffer(m_Base+ulPos, len - ulPos);
	LeaveCriticalSection(&m_cs);

	return result;
}

// 此函数是多线程安全的. 获取缓存指定位置处的数值.
BYTE CBuffer::GetBYTE(ULONG ulPos) {
	EnterCriticalSection(&m_cs);
	if (m_Base == NULL || ulPos >= ((ULONG)m_Ptr - (ULONG)m_Base))
	{
		LeaveCriticalSection(&m_cs);
		return NULL;
	}
	BYTE p = *(m_Base + ulPos);
	LeaveCriticalSection(&m_cs);

	return p;
}

// 此函数是多线程安全的. 将缓存拷贝至目标内存中.
BOOL CBuffer::CopyBuffer(PVOID pDst, ULONG nLen, ULONG ulPos) {
	EnterCriticalSection(&m_cs);
	ULONG len = (ULONG)m_Ptr - (ULONG)m_Base;
	if (m_Base == NULL || len - ulPos < nLen)
	{
		LeaveCriticalSection(&m_cs);
		return FALSE;
	}
	memcpy(pDst, m_Base+ulPos, nLen);
	LeaveCriticalSection(&m_cs);
	return TRUE;
}