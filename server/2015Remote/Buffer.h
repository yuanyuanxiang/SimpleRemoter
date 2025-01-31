#pragma once
#include <Windows.h>
#include <string>

// Buffer 带引用计数的缓存.
class Buffer {
private:
	PBYTE buf;
	ULONG len;
	ULONG *ref;
	void AddRef() {
		(*ref)++;
	}
	void DelRef() {
		(*ref)--;
	}
public:
	LPBYTE &Buf() {
		return buf;
	}
	~Buffer() {
		DelRef();
		if (*ref == 0) {
			if (buf!=NULL)
			{
				delete[] buf;
				buf = NULL;
			}
			delete ref;
			ref = NULL;
		}
	}
	Buffer():buf(NULL), len(0), ref(new ULONG(1)) {

	}
	Buffer(const BYTE * b, int n):len(n), ref(new ULONG(1)){
		buf = new BYTE[n];
		memcpy(buf, b, n);
	}
	Buffer(Buffer& o) {
		o.AddRef();
		buf = o.buf;
		len = o.len;
		ref = o.ref;
	}
	Buffer operator =(Buffer &o) {
		o.AddRef();
		buf = o.buf;
		len = o.len;
		ref = o.ref;
		return *this;
	}
	char* c_str() const {
		return (char*)buf;
	}
	ULONG length()const {
		return len;
	}
};

class CBuffer
{
public:
	CBuffer(void);
	~CBuffer(void);

	ULONG ReadBuffer(PBYTE Buffer, ULONG ulLength);
	ULONG GetBufferLength(); // 获得有效数据长度
	VOID ClearBuffer();
	BOOL WriteBuffer(PBYTE Buffer, ULONG ulLength);
	BOOL Write(PBYTE Buffer, ULONG ulLength) { return WriteBuffer(Buffer, ulLength); }
	LPBYTE GetBuffer(ULONG ulPos);
	Buffer GetMyBuffer(ULONG ulPos);
	BYTE GetBYTE(ULONG ulPos);
	BOOL CopyBuffer(PVOID pDst, ULONG nLen, ULONG ulPos);
	ULONG RemoveCompletedBuffer(ULONG ulLength);

protected:
	PBYTE	m_Base;
	PBYTE	m_Ptr;
	ULONG	m_ulMaxLength;
	CRITICAL_SECTION  m_cs;
	ULONG DeAllocateBuffer(ULONG ulLength); // 私有
	ULONG ReAllocateBuffer(ULONG ulLength); // 私有
};
