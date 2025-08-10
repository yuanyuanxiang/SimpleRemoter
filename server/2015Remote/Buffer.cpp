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

	if (ulLength > m_ulMaxLength)   //��������ĳ��ȱ��ڴ�ĳ��Ȼ���
	{
		LeaveCriticalSection(&m_cs);
		return 0;
	}
	if (ulLength > (m_Ptr - m_Base))  //��������ĳ��� ����Ч�����ݳ��Ȼ���
	{
		ulLength = m_Ptr - m_Base;
	}

	if (ulLength)
	{
		MoveMemory(m_Base,m_Base+ulLength, m_ulMaxLength - ulLength);

		m_Ptr -= ulLength;
	}

	DeAllocateBuffer(m_Ptr - m_Base);
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

	if (ulLength > (m_Ptr - m_Base))
	{
		ulLength = m_Ptr - m_Base;
	}

	if (ulLength)
	{
		CopyMemory(Buffer,m_Base,ulLength);  

		MoveMemory(m_Base,m_Base+ulLength, m_ulMaxLength - ulLength);
		m_Ptr -= ulLength;
	}

	DeAllocateBuffer(m_Ptr - m_Base);

	LeaveCriticalSection(&m_cs);
	return ulLength;
}

// ˽��: �������
ULONG CBuffer::DeAllocateBuffer(ULONG ulLength)        
{
	if (ulLength < (m_Ptr - m_Base))     
		return 0;

	ULONG ulNewMaxLength = (ULONG)ceil(ulLength / F_PAGE_ALIGNMENT) * U_PAGE_ALIGNMENT;  

	if (m_ulMaxLength <= ulNewMaxLength)
	{
		return 0;
	}
	PBYTE NewBase = (PBYTE) VirtualAlloc(NULL,ulNewMaxLength,MEM_COMMIT,PAGE_READWRITE);

	ULONG ulv1 = m_Ptr - m_Base;  //��ԭ���ڴ����Ч����
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

	if (ReAllocateBuffer(ulLength + (m_Ptr - m_Base)) == -1)//10 +1   1024
	{
		LeaveCriticalSection(&m_cs);
		return false;
	}

	CopyMemory(m_Ptr,Buffer,ulLength);

	m_Ptr+=ulLength;
	LeaveCriticalSection(&m_cs);
	return TRUE;
}

// ˽��: �������
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


	ULONG ulv1 = m_Ptr - m_Base; //ԭ�ȵ���Ч���ݳ���
 
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

ULONG CBuffer::GetBufferLength() // �����Ч���ݳ���
{
	EnterCriticalSection(&m_cs);
	if (m_Base == NULL)
	{
		LeaveCriticalSection(&m_cs);
		return 0;
	}
	ULONG len = m_Ptr - m_Base;
	LeaveCriticalSection(&m_cs);

	return len;
}

std::string CBuffer::Skip(ULONG ulPos) {
	if (ulPos == 0)
		return "";

	EnterCriticalSection(&m_cs);
	std::string ret(m_Base, m_Base + ulPos);
	MoveMemory(m_Base, m_Base + ulPos, m_ulMaxLength - ulPos);
	m_Ptr -= ulPos;

	LeaveCriticalSection(&m_cs);
	return ret;
}

// �˺������Ƕ��̰߳�ȫ��. ֻ��Զ������ʹ����.
LPBYTE CBuffer::GetBuffer(ULONG ulPos)
{
	EnterCriticalSection(&m_cs);
	if (m_Base==NULL || ulPos >= (m_Ptr - m_Base))
	{
		LeaveCriticalSection(&m_cs);
		return NULL;
	}
	LPBYTE result = m_Base + ulPos;
	LeaveCriticalSection(&m_cs);

	return result;
}

// �˺����Ƕ��̰߳�ȫ��. ��ȡ���棬�õ�Buffer����.
Buffer CBuffer::GetMyBuffer(ULONG ulPos)
{
	EnterCriticalSection(&m_cs);
	ULONG len = m_Ptr - m_Base;
	if (m_Base == NULL || ulPos >= len)
	{
		LeaveCriticalSection(&m_cs);
		return Buffer();
	}
	Buffer result = Buffer(m_Base+ulPos, len - ulPos);
	LeaveCriticalSection(&m_cs);

	return result;
}

// �˺����Ƕ��̰߳�ȫ��. ��ȡ����ָ��λ�ô�����ֵ.
BYTE CBuffer::GetBYTE(ULONG ulPos) {
	EnterCriticalSection(&m_cs);
	if (m_Base == NULL || ulPos >= (m_Ptr - m_Base))
	{
		LeaveCriticalSection(&m_cs);
		return NULL;
	}
	BYTE p = *(m_Base + ulPos);
	LeaveCriticalSection(&m_cs);

	return p;
}

// �˺����Ƕ��̰߳�ȫ��. �����濽����Ŀ���ڴ���.
BOOL CBuffer::CopyBuffer(PVOID pDst, ULONG nLen, ULONG ulPos) {
	EnterCriticalSection(&m_cs);
	ULONG len = m_Ptr - m_Base;
	if (m_Base == NULL || len - ulPos < nLen)
	{
		LeaveCriticalSection(&m_cs);
		return FALSE;
	}
	memcpy(pDst, m_Base+ulPos, nLen);
	LeaveCriticalSection(&m_cs);
	return TRUE;
}