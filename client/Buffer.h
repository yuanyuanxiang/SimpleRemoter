#pragma once
#include <Windows.h>


class CBuffer
{
public:
	CBuffer(void);
	~CBuffer(void);

	ULONG GetBufferMaxLength() const;
	ULONG ReadBuffer(PBYTE Buffer, ULONG ulLength);
	ULONG GetBufferLength() const; //获得有效数据长度
	ULONG DeAllocateBuffer(ULONG ulLength);
	VOID ClearBuffer();
	ULONG ReAllocateBuffer(ULONG ulLength);
	BOOL WriteBuffer(PBYTE Buffer, ULONG ulLength);
	PBYTE GetBuffer(ULONG ulPos=0) const;

protected:
	PBYTE	m_Base;
	PBYTE	m_Ptr;
	ULONG	m_ulMaxLength;
};
