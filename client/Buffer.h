#pragma once

#include "../common/commands.h"

class CBuffer
{
public:
    CBuffer(void);
    ~CBuffer(void);

    ULONG ReadBuffer(PBYTE Buffer, ULONG ulLength);
    ULONG GetBufferLength() const; //�����Ч���ݳ���
    VOID DeAllocateBuffer(ULONG ulLength);
    VOID ClearBuffer();
    BOOL ReAllocateBuffer(ULONG ulLength);
    BOOL WriteBuffer(PBYTE Buffer, ULONG ulLength);
    PBYTE GetBuffer(ULONG ulPos=0) const;
    void Skip(ULONG ulPos);

protected:
    PBYTE	m_Base;
    PBYTE	m_Ptr;
    ULONG	m_ulMaxLength;
};
