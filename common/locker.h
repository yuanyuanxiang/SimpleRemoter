#pragma once

#pragma warning(disable: 4996)
#pragma warning(disable: 4819)

// 浜掓枼閿併€佺潯鐪犲嚱鏁般€佽嚜鍔ㄩ攣銆佽嚜鍔ㄨ鏃躲€佽嚜鍔ㄦ棩蹇楃瓑

#include "logger.h"

// 鑷姩鏃ュ織
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
    CRITICAL_SECTION* m_cs; // 澶栭儴閿?
    CRITICAL_SECTION	i_cs; // 鍐呴儴閿?
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

// 鏅鸿兘璁℃椂鍣紝璁＄畻鍑芥暟鐨勮€楁椂
class auto_tick
{
private:
    const char* file;
    const char* func;
    int line;
    int span;
    clock_t tick;
    std::string tag;
    __inline clock_t now() const
    {
        return clock();
    }
    __inline int time() const
    {
        return now() - tick;
    }

public:
    auto_tick(const char* file_name, const char* func_name, int line_no, int th = 5, const std::string &tag="") :
        file(file_name), func(func_name), line(line_no), span(th), tick(now()), tag(tag) { }
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
                tag.empty() ? sprintf_s(buf, "%s(%d) : [%s] cost [%d]ms.\n", file, line, func, s) :
                   sprintf_s(buf, "%s(%d) : [%s] cost [%d]ms. Tag= %s. \n", file, line, func, s, tag.c_str());
                OutputDebugStringA(buf);
            }
            span = 0;
        }
    }
};

#if defined (_DEBUG) || defined (WINDOWS)
// 鏅鸿兘璁＄畻褰撳墠鍑芥暟鐨勮€楁椂锛岃秴鏃朵細鎵撳嵃
#define AUTO_TICK(thresh, tag) auto_tick TICK(__FILE__, __FUNCTION__, __LINE__, thresh, tag)
#define STOP_TICK TICK.stop()
#else
#define AUTO_TICK(thresh, tag)
#define STOP_TICK
#endif

#define AUTO_TICK_C AUTO_TICK

#include <MMSystem.h>
#pragma comment(lib, "winmm.lib")

// 楂樼簿搴︾殑鐫＄湢鍑芥暟
#define Sleep_m(ms) { Sleep(ms); }

// 浠ユ闀縩姣鍦ㄦ潯浠禖涓嬬瓑寰匱绉?n鏄闀匡紝蹇呴』鑳芥暣闄?000)
#define WAIT_n(C, T, n) { int s=(1000*(T))/(n); s=max(s,1); while((C)&&(s--))Sleep(n); }

// 鍦ㄦ潯浠禖鎴愮珛鏃剁瓑寰匱绉?姝ラ暱10ms)
#define WAIT(C, T) { WAIT_n(C, T, 10); }

// 鍦ㄦ潯浠禖鎴愮珛鏃剁瓑寰匱绉?姝ラ暱1ms)
#define WAIT_1(C, T) { WAIT_n(C, T, 1); }
