#pragma once
#include "stdafx.h"
#include "common/skCrypter.h"

typedef DWORD (*OnException)(LPVOID user, LPVOID param);

// 创建带异常保护的线程
HANDLE CreateSafeThread(const char* file, int line, const char* fname, OnException excep, LPVOID user, SIZE_T dwStackSize,
                        LPTHREAD_START_ROUTINE lpStartAddress, LPVOID lpParameter, DWORD dwCreationFlags, LPDWORD lpThreadId);

#if USING_SAFETHRED
#define __CreateThread(p1,p2,p3,p4,p5,p6) CreateSafeThread(skCrypt(__FILE__),__LINE__,skCrypt(#p3),p1,p2,0,p3,p4,p5,p6)
#define __CreateSmallThread(p1,p2,p3,p4,p5,p6,p7) CreateSafeThread(skCrypt(__FILE__),__LINE__,skCrypt(#p4),p1,p2,p3,p4,p5,p6,p7)
#else
#define __CreateThread(p1,p2,p3,p4,p5,p6) CreateThread(nullptr,0,p3,p4,p5,p6)
#define __CreateSmallThread(p1,p2,p3,p4,p5,p6,p7) CreateThread(nullptr,p3,p4,p5,p6,p7)
#endif
