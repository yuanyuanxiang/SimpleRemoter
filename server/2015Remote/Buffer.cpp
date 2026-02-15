#include "StdAfx.h"
#include "Buffer.h"

#include <math.h>

// 增大页面对齐大小，减少重新分配次数 (4KB对齐)
#define U_PAGE_ALIGNMENT   4096
#define F_PAGE_ALIGNMENT   4096.0
// 压缩阈值：当已读取数据超过此比例时才进行压缩
#define COMPACT_THRESHOLD  0.5

CBuffer::CBuffer(void)
{
    m_ulMaxLength = 0;
    m_ulReadOffset = 0;

    m_Ptr = m_Base = NULL;

    InitializeCriticalSection(&m_cs);
}

CBuffer::~CBuffer(void)
{
    if (m_Base) {
        VirtualFree(m_Base, 0, MEM_RELEASE);
        m_Base = NULL;
    }

    DeleteCriticalSection(&m_cs);

    m_Base = m_Ptr = NULL;
    m_ulMaxLength = 0;
    m_ulReadOffset = 0;
}


ULONG CBuffer::RemoveCompletedBuffer(ULONG ulLength)
{
    EnterCriticalSection(&m_cs);

    ULONG dataLen = m_Ptr - m_Base;
    if (ulLength > m_ulMaxLength) { //请求长度比内存总长度还大
        LeaveCriticalSection(&m_cs);
        return 0;
    }
    if (ulLength > dataLen) { //请求长度比有效数据长度还大
        ulLength = dataLen;
    }

    if (ulLength) {
        // 使用延迟移动策略：只更新读取偏移，不立即移动数据
        m_ulReadOffset += ulLength;

        // 当已读取数据超过阈值时才进行压缩
        if (m_ulReadOffset > m_ulMaxLength * COMPACT_THRESHOLD) {
            CompactBuffer();
        }
    }

    LeaveCriticalSection(&m_cs);

    return ulLength;
}

// 压缩缓冲区，移除已读取的数据
VOID CBuffer::CompactBuffer()
{
    // 此函数应在持有锁的情况下调用
    if (m_ulReadOffset > 0 && m_Base) {
        ULONG remainingData = (m_Ptr - m_Base) - m_ulReadOffset;
        if (remainingData > 0) {
            MoveMemory(m_Base, m_Base + m_ulReadOffset, remainingData);
        }
        m_Ptr = m_Base + remainingData;
        m_ulReadOffset = 0;

        // 尝试缩减缓冲区
        DeAllocateBuffer(remainingData);
    }
}

ULONG CBuffer::ReadBuffer(PBYTE Buffer, ULONG ulLength)
{
    EnterCriticalSection(&m_cs);

    // 计算有效数据长度（考虑读取偏移）
    ULONG effectiveDataLen = (m_Ptr - m_Base) - m_ulReadOffset;

    if (ulLength > effectiveDataLen) {
        ulLength = effectiveDataLen;
    }

    if (ulLength) {
        // 从当前读取位置拷贝数据
        CopyMemory(Buffer, m_Base + m_ulReadOffset, ulLength);

        // 更新读取偏移而不是移动数据
        m_ulReadOffset += ulLength;

        // 当已读取数据超过阈值时才进行压缩
        if (m_ulReadOffset > m_ulMaxLength * COMPACT_THRESHOLD) {
            CompactBuffer();
        }
    }

    LeaveCriticalSection(&m_cs);
    return ulLength;
}

// 私有: 缩减缓存
ULONG CBuffer::DeAllocateBuffer(ULONG ulLength)
{
    if (ulLength < (m_Ptr - m_Base))
        return 0;

    ULONG ulNewMaxLength = (ULONG)ceil(ulLength / F_PAGE_ALIGNMENT) * U_PAGE_ALIGNMENT;

    if (m_ulMaxLength <= ulNewMaxLength) {
        return 0;
    }
    PBYTE NewBase = (PBYTE) VirtualAlloc(NULL,ulNewMaxLength,MEM_COMMIT,PAGE_READWRITE);

    ULONG ulv1 = m_Ptr - m_Base;  //从原来内存中的有效数据
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

    if (ReAllocateBuffer(ulLength + (m_Ptr - m_Base)) == -1) { //10 +1   1024
        LeaveCriticalSection(&m_cs);
        return false;
    }

    CopyMemory(m_Ptr,Buffer,ulLength);

    m_Ptr+=ulLength;
    LeaveCriticalSection(&m_cs);
    return TRUE;
}

// 私有: 扩展缓存
ULONG CBuffer::ReAllocateBuffer(ULONG ulLength)
{
    if (ulLength < m_ulMaxLength)
        return 0;

    ULONG  ulNewMaxLength = (ULONG)ceil(ulLength / F_PAGE_ALIGNMENT) * U_PAGE_ALIGNMENT;
    PBYTE  NewBase  = (PBYTE) VirtualAlloc(NULL,ulNewMaxLength,MEM_COMMIT,PAGE_READWRITE);
    if (NewBase == NULL) {
        return -1;
    }


    ULONG ulv1 = m_Ptr - m_Base; //原先的有效数据长度

    CopyMemory(NewBase,m_Base,ulv1);

    if (m_Base) {
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
    m_ulReadOffset = 0;  // 重置读取偏移

    DeAllocateBuffer(1024);
    LeaveCriticalSection(&m_cs);
}

ULONG CBuffer::GetBufferLength() // 返回有效数据长度
{
    EnterCriticalSection(&m_cs);
    if (m_Base == NULL) {
        LeaveCriticalSection(&m_cs);
        return 0;
    }
    // 有效数据长度需要减去已读取的偏移量
    ULONG len = (m_Ptr - m_Base) - m_ulReadOffset;
    LeaveCriticalSection(&m_cs);

    return len;
}

std::string CBuffer::Skip(ULONG ulPos)
{
    if (ulPos == 0)
        return "";

    EnterCriticalSection(&m_cs);
    // 从当前读取位置开始跳过
    std::string ret((char*)(m_Base + m_ulReadOffset), (char*)(m_Base + m_ulReadOffset + ulPos));

    // 使用延迟移动策略
    m_ulReadOffset += ulPos;

    // 当已读取数据超过阈值时才进行压缩
    if (m_ulReadOffset > m_ulMaxLength * COMPACT_THRESHOLD) {
        CompactBuffer();
    }

    LeaveCriticalSection(&m_cs);
    return ret;
}

// 此函数是多线程安全的. 只能远程调用使用它.
LPBYTE CBuffer::GetBuffer(ULONG ulPos)
{
    EnterCriticalSection(&m_cs);
    // 计算有效数据长度
    ULONG effectiveDataLen = (m_Ptr - m_Base) - m_ulReadOffset;
    if (m_Base == NULL || ulPos >= effectiveDataLen) {
        LeaveCriticalSection(&m_cs);
        return NULL;
    }
    // 返回相对于当前读取位置的指针
    LPBYTE result = m_Base + m_ulReadOffset + ulPos;
    LeaveCriticalSection(&m_cs);

    return result;
}

// 此函数是多线程安全的. 获取缓存，得到Buffer对象.
Buffer CBuffer::GetMyBuffer(ULONG ulPos)
{
    EnterCriticalSection(&m_cs);
    ULONG effectiveDataLen = (m_Ptr - m_Base) - m_ulReadOffset;
    if (m_Base == NULL || ulPos >= effectiveDataLen) {
        LeaveCriticalSection(&m_cs);
        return Buffer();
    }
    Buffer result = Buffer(m_Base + m_ulReadOffset + ulPos, effectiveDataLen - ulPos);
    LeaveCriticalSection(&m_cs);

    return result;
}

// 此函数是多线程安全的. 获取缓存指定位置处的字节值.
BYTE CBuffer::GetBYTE(ULONG ulPos)
{
    EnterCriticalSection(&m_cs);
    ULONG effectiveDataLen = (m_Ptr - m_Base) - m_ulReadOffset;
    if (m_Base == NULL || ulPos >= effectiveDataLen) {
        LeaveCriticalSection(&m_cs);
        return 0;
    }
    BYTE p = *(m_Base + m_ulReadOffset + ulPos);
    LeaveCriticalSection(&m_cs);

    return p;
}

// 此函数是多线程安全的. 将缓存拷贝到目标内存中.
BOOL CBuffer::CopyBuffer(PVOID pDst, ULONG nLen, ULONG ulPos)
{
    EnterCriticalSection(&m_cs);
    ULONG effectiveDataLen = (m_Ptr - m_Base) - m_ulReadOffset;
    if (m_Base == NULL || effectiveDataLen - ulPos < nLen || pDst == NULL) {
        LeaveCriticalSection(&m_cs);
        return FALSE;
    }
    memcpy(pDst, m_Base + m_ulReadOffset + ulPos, nLen);
    LeaveCriticalSection(&m_cs);
    return TRUE;
}

// 获取可直接写入的缓冲区指针，用于零拷贝接收
LPBYTE CBuffer::GetWriteBuffer(ULONG requiredSize, ULONG& availableSize)
{
    EnterCriticalSection(&m_cs);

    // 先压缩缓冲区以获得更多空间
    if (m_ulReadOffset > 0) {
        CompactBuffer();
    }

    // 确保有足够空间
    ULONG currentDataLen = m_Ptr - m_Base;
    if (ReAllocateBuffer(currentDataLen + requiredSize) == (ULONG)-1) {
        LeaveCriticalSection(&m_cs);
        availableSize = 0;
        return NULL;
    }

    availableSize = m_ulMaxLength - currentDataLen;
    LPBYTE result = m_Ptr;
    LeaveCriticalSection(&m_cs);

    return result;
}

// 确认写入完成，更新内部指针
VOID CBuffer::CommitWrite(ULONG writtenSize)
{
    EnterCriticalSection(&m_cs);
    m_Ptr += writtenSize;
    LeaveCriticalSection(&m_cs);
}
