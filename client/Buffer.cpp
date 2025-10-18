#ifdef _WIN32
#include "StdAfx.h"
#endif

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
    if (m_Base) {
        MVirtualFree(m_Base, 0, MEM_RELEASE);
        m_Base = NULL;
    }

    m_Base = m_Ptr = NULL;
    m_ulMaxLength = 0;
}



ULONG CBuffer::ReadBuffer(PBYTE Buffer, ULONG ulLength)
{
    if (ulLength > m_ulMaxLength) {
        return 0;
    }
    ULONG len = m_Ptr - m_Base;
    if (ulLength > len) {
        ulLength = len;
    }

    if (ulLength) {
        CopyMemory(Buffer,m_Base,ulLength);

        MoveMemory(m_Base,m_Base+ulLength, m_ulMaxLength - ulLength);
        m_Ptr -= ulLength;
    }

    DeAllocateBuffer(m_Ptr - m_Base);

    return ulLength;
}


// 重新分配内存大小
VOID CBuffer::DeAllocateBuffer(ULONG ulLength)
{
    int len = m_Ptr - m_Base;
    if (ulLength < len)
        return;

    ULONG ulNewMaxLength = ceil(ulLength / F_PAGE_ALIGNMENT) * U_PAGE_ALIGNMENT;

    if (m_ulMaxLength <= ulNewMaxLength) {
        return;
    }
    PBYTE NewBase = (PBYTE) MVirtualAlloc(NULL,ulNewMaxLength,MEM_COMMIT,PAGE_READWRITE);
    if (NewBase == NULL)
        return;

    CopyMemory(NewBase,m_Base,len);

    MVirtualFree(m_Base,0,MEM_RELEASE);

    m_Base = NewBase;

    m_Ptr = m_Base + len;

    m_ulMaxLength = ulNewMaxLength;
}


BOOL CBuffer::WriteBuffer(PBYTE Buffer, ULONG ulLength)
{
    if (ReAllocateBuffer(ulLength + (m_Ptr - m_Base)) == FALSE) {
        return FALSE;
    }

    CopyMemory(m_Ptr, Buffer, ulLength);
    m_Ptr+=ulLength;

    return TRUE;
}


// 当缓存长度不足时重新分配
BOOL CBuffer::ReAllocateBuffer(ULONG ulLength)
{
    if (ulLength < m_ulMaxLength)
        return TRUE;

    ULONG  ulNewMaxLength = ceil(ulLength / F_PAGE_ALIGNMENT) * U_PAGE_ALIGNMENT;
    PBYTE  NewBase  = (PBYTE) MVirtualAlloc(NULL,ulNewMaxLength,MEM_COMMIT,PAGE_READWRITE);
    if (NewBase == NULL) {
        return FALSE;
    }

    ULONG len = m_Ptr - m_Base;
    CopyMemory(NewBase, m_Base, len);

    if (m_Base) {
        MVirtualFree(m_Base,0,MEM_RELEASE);
    }
    m_Base = NewBase;
    m_Ptr = m_Base + len;
    m_ulMaxLength = ulNewMaxLength;

    return TRUE;
}


VOID CBuffer::ClearBuffer()
{
    m_Ptr = m_Base;

    DeAllocateBuffer(1024);
}



ULONG CBuffer::GetBufferLength() const
{
    return m_Ptr - m_Base;
}


void CBuffer::Skip(ULONG ulPos)
{
    if (ulPos == 0)
        return;
    MoveMemory(m_Base, m_Base + ulPos, m_ulMaxLength - ulPos);
    m_Ptr -= ulPos;
}


PBYTE CBuffer::GetBuffer(ULONG ulPos) const
{
    if (m_Base==NULL || ulPos>=(m_Ptr - m_Base)) {
        return NULL;
    }
    return m_Base+ulPos;
}
