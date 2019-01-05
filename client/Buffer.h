#pragma once
#include <Windows.h>


class CBuffer
{
public:
	CBuffer(void);
	~CBuffer(void);

	ULONG CBuffer::GetBufferMaxLength();
	ULONG CBuffer::ReadBuffer(PBYTE Buffer, ULONG ulLength);
	ULONG CBuffer::GetBufferLength(); //获得有效数据长度;
	ULONG CBuffer::DeAllocateBuffer(ULONG ulLength);
	VOID CBuffer::ClearBuffer();
	ULONG CBuffer::ReAllocateBuffer(ULONG ulLength);
	BOOL CBuffer::WriteBuffer(PBYTE Buffer, ULONG ulLength);
	PBYTE CBuffer::GetBuffer(ULONG ulPos=0);

protected:
	PBYTE	m_Base;
	PBYTE	m_Ptr;
	ULONG	m_ulMaxLength;
	CRITICAL_SECTION  m_cs;
};
