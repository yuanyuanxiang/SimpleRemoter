#pragma once
#include <Windows.h>
#include <string>

// Buffer 带引用计数的缓存.
class Buffer
{
private:
    PBYTE buf;
    ULONG len;
    ULONG padding;
    std::string md5;
    ULONG *ref;
    void AddRef()
    {
        (*ref)++;
    }
    void DelRef()
    {
        (*ref)--;
    }
public:
    LPBYTE &Buf()
    {
        return buf;
    }
    ~Buffer()
    {
        DelRef();
        if (*ref == 0) {
            if (buf!=NULL) {
                delete[] buf;
                buf = NULL;
            }
            delete ref;
            ref = NULL;
        }
    }
    Buffer():buf(NULL), len(0), ref(new ULONG(1)), padding(0)
    {

    }
    Buffer(const BYTE * b, int n, int padding=0, const std::string& md5="") :
        len(n), ref(new ULONG(1)), padding(padding), md5(md5)
    {
        buf = new BYTE[n];
        memcpy(buf, b, n);
    }
    Buffer(Buffer& o)
    {
        o.AddRef();
        buf = o.buf;
        len = o.len;
        ref = o.ref;
    }
    Buffer operator =(Buffer &o)
    {
        o.AddRef();
        buf = o.buf;
        len = o.len;
        ref = o.ref;
        return *this;
    }
    char* c_str() const
    {
        return (char*)buf;
    }
    ULONG length(bool noPadding=false)const
    {
        return noPadding ? len - padding : len;
    }
    std::string MD5() const
    {
        return md5;
    }
    BYTE GetBYTE(int idx=0) const
    {
        return idx >= len ? 0 : buf[idx];
    }
    LPBYTE GetBuffer(int idx=0) const
    {
        return idx >= len ? 0 : buf + idx;
    }
    int GetBufferLength() const
    {
        return len;
    }
    BOOL CopyBuffer(PVOID pDst, ULONG nLen, ULONG ulPos=0)
    {
        if (len - ulPos < nLen) {
            return FALSE;
        }
        memcpy(pDst, buf + ulPos, nLen);
        return TRUE;
    }
};

class CBuffer
{
public:
    CBuffer(void);
    ~CBuffer(void);

    ULONG ReadBuffer(PBYTE Buffer, ULONG ulLength);
    ULONG GetBufferLength(); // 返回有效数据长度
    ULONG GetBufferLen()
    {
        return GetBufferLength();
    }
    VOID ClearBuffer();
    BOOL WriteBuffer(PBYTE Buffer, ULONG ulLength);
    BOOL Write(PBYTE Buffer, ULONG ulLength)
    {
        return WriteBuffer(Buffer, ulLength);
    }
    BOOL WriteBuffer(CBuffer& buf)
    {
        return WriteBuffer(buf.GetBuffer(), buf.GetBufferLen());
    }
    LPBYTE GetBuffer(ULONG ulPos=0);
    Buffer GetMyBuffer(ULONG ulPos=0);
    BYTE GetBYTE(ULONG ulPos);
    BOOL CopyBuffer(PVOID pDst, ULONG nLen, ULONG ulPos);
    ULONG RemoveCompletedBuffer(ULONG ulLength);
    std::string Skip(ULONG ulPos);

    // 获取可直接写入的缓冲区指针，用于零拷贝接收
    // 返回可写入的起始地址，availableSize 返回可用空间大小
    LPBYTE GetWriteBuffer(ULONG requiredSize, ULONG& availableSize);
    // 确认写入完成，更新内部指针
    VOID CommitWrite(ULONG writtenSize);

protected:
    PBYTE	m_Base;
    PBYTE	m_Ptr;
    ULONG	m_ulMaxLength;
    ULONG   m_ulReadOffset;  // 读取偏移，用于延迟数据移动
    CRITICAL_SECTION  m_cs;
    ULONG DeAllocateBuffer(ULONG ulLength); // 私有
    ULONG ReAllocateBuffer(ULONG ulLength); // 私有
    VOID CompactBuffer();  // 压缩缓冲区，移除已读取数据
};
