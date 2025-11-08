#pragma once

#pragma warning(disable: 4996)
#pragma warning(disable: 4819)

// 互斥锁、睡眠函数、自动锁、自动计时、自动日志等

#include "logger.h"

// 自动日志
class CAutoLog
{
private:
    CRITICAL_SECTION *m_cs;
    const char* name;
public:
    CAutoLog(const char* _name, CRITICAL_SECTION *cs=NULL) : name(_name), m_cs(cs)
    {
        Mprintf(">>> Enter thread %s: [%d]\n", name ? name : "", GetCurrentThreadId());
        if (m_cs)EnterCriticalSection(m_cs);
    }

    ~CAutoLog()
    {
        if (m_cs)LeaveCriticalSection(m_cs);
        Mprintf(">>> Leave thread %s: [%d]\n", name ? name : "", GetCurrentThreadId());
    }
};

class CLock
{
public:
    CLock(CRITICAL_SECTION& cs) : m_cs(&cs)
    {
        Lock();
    }
    CLock() : m_cs(nullptr)
    {
        InitializeCriticalSection(&i_cs);
    }
    ~CLock()
    {
        m_cs ? Unlock() : DeleteCriticalSection(&i_cs);
    }

    void Unlock()
    {
        LeaveCriticalSection(m_cs ? m_cs : &i_cs);
    }

    void Lock()
    {
        EnterCriticalSection(m_cs ? m_cs : &i_cs);
    }

    void unlock()
    {
        LeaveCriticalSection(m_cs ? m_cs : &i_cs);
    }

    void lock()
    {
        EnterCriticalSection(m_cs ? m_cs : &i_cs);
    }

protected:
    CRITICAL_SECTION* m_cs; // 外部锁
    CRITICAL_SECTION	i_cs; // 内部锁
};

typedef CLock CLocker;

class CAutoLock
{
private:
    CRITICAL_SECTION &m_cs;

public:
    CAutoLock(CRITICAL_SECTION& cs) : m_cs(cs)
    {
        EnterCriticalSection(&m_cs);
    }

    ~CAutoLock()
    {
        LeaveCriticalSection(&m_cs);
    }
};

class CAutoCLock
{
private:
    CLock& m_cs;

public:
    CAutoCLock(CLock& cs) : m_cs(cs)
    {
        m_cs.Lock();
    }

    ~CAutoCLock()
    {
        m_cs.Unlock();
    }
};

// 智能计时器，计算函数的耗时
class auto_tick
{
private:
    const char* file;
    const char* func;
    int line;
    int span;
    clock_t tick;
    __inline clock_t now() const
    {
        return clock();
    }
    __inline int time() const
    {
        return now() - tick;
    }

public:
    auto_tick(const char* file_name, const char* func_name, int line_no, int th = 5) :
        file(file_name), func(func_name), line(line_no), span(th), tick(now()) { }
    ~auto_tick()
    {
        stop();
    }

    __inline void stop()
    {
        if (span != 0) {
            int s(this->time());
            if (s > span) {
                char buf[1024];
                sprintf_s(buf, "%s(%d) : [%s] cost [%d]ms.\n", file, line, func, s);
                OutputDebugStringA(buf);
            }
            span = 0;
        }
    }
};

#ifdef _DEBUG
// 智能计算当前函数的耗时，超时会打印
#define AUTO_TICK(thresh) auto_tick TICK(__FILE__, __FUNCTION__, __LINE__, thresh)
#define STOP_TICK TICK.stop()
#else
#define AUTO_TICK(thresh)
#define STOP_TICK
#endif

#define AUTO_TICK_C AUTO_TICK

#include <MMSystem.h>
#pragma comment(lib, "winmm.lib")

// 高精度的睡眠函数
#define Sleep_m(ms) { Sleep(ms); }

// 以步长n毫秒在条件C下等待T秒(n是步长，必须能整除1000)
#define WAIT_n(C, T, n) { int s=(1000*(T))/(n); s=max(s,1); do{Sleep(n);}while((C)&&(--s)); }

// 在条件C成立时等待T秒(步长10ms)
#define WAIT(C, T) { WAIT_n(C, T, 10); }

// 在条件C成立时等待T秒(步长1ms)
#define WAIT_1(C, T) { WAIT_n(C, T, 1); }
