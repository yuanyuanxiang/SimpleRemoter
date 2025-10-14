#include "stdafx.h"
#include "SafeThread.h"
#include <stdexcept>
#include <map>

// RoutineInfo 记录线程相关信息.
typedef struct RoutineInfo {
    DWORD tid;							// 线程ID

    LPTHREAD_START_ROUTINE Func;		// 线程函数
    LPVOID Param;						// 线程参数

    OnException Excep;					// 异常处理函数
    LPVOID User;						// 额外参数

    std::string File;					// 创建线程的文件
    int Line;							// 文件行数
    std::string Name;					// 函数名称
    bool Trace;							// 追踪线程运行情况

} RoutineInfo;

DWORD HandleCppException(RoutineInfo& ri)
{
    try {
        return ri.Func(ri.Param); // 调用实际线程函数
    } catch (const std::exception& e) {
        if (ri.Excep) {
            Mprintf("[%d] 捕获 C++ 异常: %s. [%s:%d]\n", ri.tid, e.what(), ri.File.c_str(), ri.Line);
            return ri.Excep(ri.User, ri.Param);
        }
        Mprintf("[%d] 捕获 C++ 异常: %s. 没有提供异常处理程序[%s:%d]!\n", ri.tid, e.what(), ri.File.c_str(), ri.Line);
    } catch (...) {
        if (ri.Excep) {
            Mprintf("[%d] 捕获未知 C++ 异常. [%s:%d]\n", ri.tid, ri.File.c_str(), ri.Line);
            return ri.Excep(ri.User, ri.Param);
        }
        Mprintf("[%d] 捕获未知 C++ 异常. 没有提供异常处理程序[%s:%d]!\n", ri.tid, ri.File.c_str(), ri.Line);
    }
    return 0xDEAD0002;
}

DWORD HandleSEHException(RoutineInfo & ri)
{
    __try {
        // 执行实际线程函数
        return HandleCppException(ri);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        if (ri.Excep) {
            Mprintf("[%d] 捕获硬件异常，线程不会崩溃. [%s:%d] Code=%08X\n", ri.tid, ri.File.c_str(), ri.Line, GetExceptionCode());
            return ri.Excep(ri.User, ri.Param);
        }
        Mprintf("[%d] 捕获硬件异常. 没有提供异常处理程序[%s:%d]! Code=%08X\n", ri.tid, ri.File.c_str(), ri.Line, GetExceptionCode());
        return 0xDEAD0001; // 返回错误状态
    }
}

// 通用异常包装函数
DWORD WINAPI ThreadWrapper(LPVOID lpParam)
{
    RoutineInfo *ri = (RoutineInfo *)lpParam;
    ri->tid = GetCurrentThreadId();
    RoutineInfo pRealThreadFunc = *ri;
    delete ri;

    if (pRealThreadFunc.Trace) {
        CAutoLog Log(pRealThreadFunc.Name.c_str());
        // 异常捕获
        return HandleSEHException(pRealThreadFunc);
    }
    // 异常捕获
    return HandleSEHException(pRealThreadFunc);
}

// 创建带异常保护的线程，记录创建线程的文件、行数和函数名称
HANDLE CreateSafeThread(const char*file, int line, const char* fname, OnException excep, LPVOID user, SIZE_T dwStackSize,
                        LPTHREAD_START_ROUTINE lpStartAddress, LPVOID lpParameter, DWORD dwCreationFlags, LPDWORD lpThreadId)
{

    if (excep) assert(user); // 异常处理函数和参数必须同时提供
    if (excep && !user) {
        Mprintf("[ERROR] 提供了异常处理函数但 user 为 NULL, 拒绝创建线程[%s:%d]!\n", file, line);
        return NULL;
    }

    auto ri = new RoutineInfo{ 0, lpStartAddress, lpParameter, excep, user, file ? file : "", line, fname, dwStackSize == 0 };

    HANDLE hThread = ::CreateThread(NULL, dwStackSize, ThreadWrapper, ri, dwCreationFlags, lpThreadId);
    if (!hThread) {
        Mprintf("[ERROR] 创建线程失败：GetLastError=%lu [%s:%d]\n", GetLastError(), file, line);
        delete ri;
        return NULL;
    }

    return hThread;
}
