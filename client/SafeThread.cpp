#include "stdafx.h"
#include "SafeThread.h"
#include <stdexcept>
#include <map>

// RoutineInfo ��¼�߳������Ϣ.
typedef struct RoutineInfo {
    DWORD tid;							// �߳�ID

    LPTHREAD_START_ROUTINE Func;		// �̺߳���
    LPVOID Param;						// �̲߳���

    OnException Excep;					// �쳣������
    LPVOID User;						// �������

    std::string File;					// �����̵߳��ļ�
    int Line;							// �ļ�����
    std::string Name;					// ��������
    bool Trace;							// ׷���߳��������

} RoutineInfo;

DWORD HandleCppException(RoutineInfo& ri)
{
    try {
        return ri.Func(ri.Param); // ����ʵ���̺߳���
    } catch (const std::exception& e) {
        if (ri.Excep) {
            Mprintf("[%d] ���� C++ �쳣: %s. [%s:%d]\n", ri.tid, e.what(), ri.File.c_str(), ri.Line);
            return ri.Excep(ri.User, ri.Param);
        }
        Mprintf("[%d] ���� C++ �쳣: %s. û���ṩ�쳣�������[%s:%d]!\n", ri.tid, e.what(), ri.File.c_str(), ri.Line);
    } catch (...) {
        if (ri.Excep) {
            Mprintf("[%d] ����δ֪ C++ �쳣. [%s:%d]\n", ri.tid, ri.File.c_str(), ri.Line);
            return ri.Excep(ri.User, ri.Param);
        }
        Mprintf("[%d] ����δ֪ C++ �쳣. û���ṩ�쳣�������[%s:%d]!\n", ri.tid, ri.File.c_str(), ri.Line);
    }
    return 0xDEAD0002;
}

DWORD HandleSEHException(RoutineInfo & ri)
{
    __try {
        // ִ��ʵ���̺߳���
        return HandleCppException(ri);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        if (ri.Excep) {
            Mprintf("[%d] ����Ӳ���쳣���̲߳������. [%s:%d] Code=%08X\n", ri.tid, ri.File.c_str(), ri.Line, GetExceptionCode());
            return ri.Excep(ri.User, ri.Param);
        }
        Mprintf("[%d] ����Ӳ���쳣. û���ṩ�쳣�������[%s:%d]! Code=%08X\n", ri.tid, ri.File.c_str(), ri.Line, GetExceptionCode());
        return 0xDEAD0001; // ���ش���״̬
    }
}

// ͨ���쳣��װ����
DWORD WINAPI ThreadWrapper(LPVOID lpParam)
{
    RoutineInfo *ri = (RoutineInfo *)lpParam;
    ri->tid = GetCurrentThreadId();
    RoutineInfo pRealThreadFunc = *ri;
    delete ri;

    if (pRealThreadFunc.Trace) {
        CAutoLog Log(pRealThreadFunc.Name.c_str());
        // �쳣����
        return HandleSEHException(pRealThreadFunc);
    }
    // �쳣����
    return HandleSEHException(pRealThreadFunc);
}

// �������쳣�������̣߳���¼�����̵߳��ļ��������ͺ�������
HANDLE CreateSafeThread(const char*file, int line, const char* fname, OnException excep, LPVOID user, SIZE_T dwStackSize,
                        LPTHREAD_START_ROUTINE lpStartAddress, LPVOID lpParameter, DWORD dwCreationFlags, LPDWORD lpThreadId)
{

    if (excep) assert(user); // �쳣�������Ͳ�������ͬʱ�ṩ
    if (excep && !user) {
        Mprintf("[ERROR] �ṩ���쳣�������� user Ϊ NULL, �ܾ������߳�[%s:%d]!\n", file, line);
        return NULL;
    }

    auto ri = new RoutineInfo{ 0, lpStartAddress, lpParameter, excep, user, file ? file : "", line, fname, dwStackSize == 0 };

    HANDLE hThread = ::CreateThread(NULL, dwStackSize, ThreadWrapper, ri, dwCreationFlags, lpThreadId);
    if (!hThread) {
        Mprintf("[ERROR] �����߳�ʧ�ܣ�GetLastError=%lu [%s:%d]\n", GetLastError(), file, line);
        delete ri;
        return NULL;
    }

    return hThread;
}
