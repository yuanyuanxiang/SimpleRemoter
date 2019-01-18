#include "StdAfx.h"
#include "Buffer.h"
#include <math.h>


#define U_PAGE_ALIGNMENT   3
#define F_PAGE_ALIGNMENT 3.0

CBuffer::CBuffer()
{
	m_ulMaxLength = 0;

	m_Ptr = m_Base = NULL;
}


CBuffer::~CBuffer(void)
{
	if (m_Base)
	{
		VirtualFree(m_Base, 0, MEM_RELEASE);
		m_Base = NULL;
	}

	m_Base = m_Ptr = NULL;
	m_ulMaxLength = 0;
}



ULONG CBuffer::ReadBuffer(PBYTE Buffer, ULONG ulLength)
{
	if (ulLength > GetBufferMaxLength())
	{
		return 0;
	}
	if (ulLength > GetBufferLength())
	{
		ulLength = GetBufferLength();
	}

	if (ulLength)
	{
		CopyMemory(Buffer,m_Base,ulLength);  

		MoveMemory(m_Base,m_Base+ulLength,GetBufferMaxLength() - ulLength);
		m_Ptr -= ulLength;
	}

	DeAllocateBuffer(GetBufferLength());   

	return ulLength;
}



ULONG CBuffer::DeAllocateBuffer(ULONG ulLength)        
{
	if (ulLength < GetBufferLength())     
		return 0;

	ULONG ulNewMaxLength = (ULONG)ceil(ulLength / F_PAGE_ALIGNMENT) * U_PAGE_ALIGNMENT;  

	if (GetBufferMaxLength() <= ulNewMaxLength) 
	{
		return 0;
	}
	PBYTE NewBase = (PBYTE) VirtualAlloc(NULL,ulNewMaxLength,MEM_COMMIT,PAGE_READWRITE);

	ULONG ulv1 = GetBufferLength();  //算原先内存的有效长度
	CopyMemory(NewBase,m_Base,ulv1);

	VirtualFree(m_Base,0,MEM_RELEASE);

	m_Base = NewBase;

	m_Ptr = m_Base + ulv1;

	m_ulMaxLength = ulNewMaxLength;

	return m_ulMaxLength;
}


BOOL CBuffer::WriteBuffer(PBYTE Buffer, ULONG ulLength)
{
	if (ReAllocateBuffer(ulLength + GetBufferLength()) == -1)//10 +1   1024
	{
		return false;
	}

	CopyMemory(m_Ptr,Buffer,ulLength);//Hello 5

	m_Ptr+=ulLength;
	return TRUE;
}



ULONG CBuffer::ReAllocateBuffer(ULONG ulLength)
{
	if (ulLength < GetBufferMaxLength())   
		return 0;

	ULONG  ulNewMaxLength = (ULONG)ceil(ulLength / F_PAGE_ALIGNMENT) * U_PAGE_ALIGNMENT;  
	PBYTE  NewBase  = (PBYTE) VirtualAlloc(NULL,ulNewMaxLength,MEM_COMMIT,PAGE_READWRITE);
	if (NewBase == NULL)
	{
		return -1; 
	}

	ULONG ulv1 = GetBufferLength();   //原先的有效数据长度  
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
	m_Ptr = m_Base;

	DeAllocateBuffer(1024);  
}



ULONG CBuffer::GetBufferLength() const //获得有效数据长度
{
	if (m_Base == NULL)
		return 0;

	return (ULONG)m_Ptr - (ULONG)m_Base;
}


ULONG CBuffer::GetBufferMaxLength() const
{
	return m_ulMaxLength;
}

PBYTE CBuffer::GetBuffer(ULONG ulPos) const
{
	if (m_Base==NULL)
	{
		return NULL;
	}
	if (ulPos>=GetBufferLength())
	{
		return NULL;
	}
	return m_Base+ulPos;
}
