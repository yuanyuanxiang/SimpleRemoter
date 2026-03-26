#pragma once
// CrashReport.h - 崩溃报告相关定义
// 集中管理配置键名、退出代码等，提高可维护性

#include <windows.h>

// ============================================
// 配置键名定义 - [crash] section
// ============================================
#define CFG_CRASH_SECTION           "crash"

// 崩溃统计（永久）
#define CFG_CRASH_COUNT             "count"         // 总崩溃次数
#define CFG_CRASH_LAST_TIME         "lastTime"      // 最后崩溃时间
#define CFG_CRASH_LAST_CODE         "lastCode"      // 最后退出代码
#define CFG_CRASH_LAST_RUN_MS       "lastRunMs"     // 最后运行时间(ms), string存储uint64

// MTBF 统计（永久）
#define CFG_CRASH_STARTS            "starts"        // 启动次数
#define CFG_CRASH_TOTAL_RUN_MS      "totalRunMs"    // 累计运行时间(ms), string存储uint64

// 崩溃窗口状态（临时，窗口过期或系统重启后清除）
#define CFG_CRASH_WIN_COUNT         "winCount"      // 窗口期内崩溃次数
#define CFG_CRASH_WIN_START         "winStart"      // 窗口起始时间(ms, GetTickCount64)

// 崩溃保护标志（一次性，程序启动时读取并清除）
#define CFG_CRASH_PROTECTED         "protected"     // 崩溃保护标志

// ============================================
// 特殊退出代码
// ============================================
#define EXIT_CODE_UNKNOWN           0xFFFFFFFF  // 无法获取退出代码

// 自定义退出代码范围 (1000-1999)
#define EXIT_CODE_CUSTOM_BASE       1000
#define EXIT_NORMAL                 0
#define EXIT_CONFIG_ERROR           1001
#define EXIT_NETWORK_ERROR          1002
#define EXIT_AUTH_FAILED            1003
#define EXIT_RESTART_REQUEST        1004
#define EXIT_MANUAL_STOP            1005

// ============================================
// Windows 异常代码
// ============================================
#define EXCEPTION_ACCESS_VIOLATION_CODE      0xC0000005
#define EXCEPTION_INT_DIVIDE_BY_ZERO_CODE    0xC0000094
#define EXCEPTION_STACK_OVERFLOW_CODE        0xC00000FD
#define EXCEPTION_STACK_BUFFER_OVERRUN_CODE  0xC0000409
#define EXCEPTION_ILLEGAL_INSTRUCTION_CODE   0xC000001D
#define EXCEPTION_NONCONTINUABLE_CODE        0xC0000025
#define EXCEPTION_BREAKPOINT_CODE            0x80000003

// ============================================
// 退出代码描述
// ============================================
inline const char* GetExitCodeDescription(DWORD exitCode)
{
    switch (exitCode) {
        // Windows 异常
        case EXCEPTION_ACCESS_VIOLATION_CODE:      return "ACCESS_VIOLATION";
        case EXCEPTION_INT_DIVIDE_BY_ZERO_CODE:    return "INTEGER_DIVIDE_BY_ZERO";
        case EXCEPTION_STACK_OVERFLOW_CODE:        return "STACK_OVERFLOW";
        case EXCEPTION_STACK_BUFFER_OVERRUN_CODE:  return "STACK_BUFFER_OVERRUN";
        case EXCEPTION_ILLEGAL_INSTRUCTION_CODE:   return "ILLEGAL_INSTRUCTION";
        case EXCEPTION_NONCONTINUABLE_CODE:        return "NONCONTINUABLE_EXCEPTION";
        case EXCEPTION_BREAKPOINT_CODE:            return "BREAKPOINT";
        // 自定义退出代码
        case EXIT_NORMAL:                          return "NORMAL";
        case EXIT_CONFIG_ERROR:                    return "CONFIG_ERROR";
        case EXIT_NETWORK_ERROR:                   return "NETWORK_ERROR";
        case EXIT_AUTH_FAILED:                     return "AUTH_FAILED";
        case EXIT_RESTART_REQUEST:                 return "RESTART_REQUEST";
        case EXIT_MANUAL_STOP:                     return "MANUAL_STOP";
        // 特殊值
        case EXIT_CODE_UNKNOWN:                    return "UNKNOWN";
        default:                                   return NULL;
    }
}

// 判断是否为异常退出（崩溃）
inline BOOL IsExceptionExitCode(DWORD exitCode)
{
    // Windows 异常代码通常以 0xC0000000 或 0x80000000 开头
    return (exitCode & 0xC0000000) != 0;
}

// 判断是否为自定义退出代码
inline BOOL IsCustomExitCode(DWORD exitCode)
{
    return exitCode >= EXIT_CODE_CUSTOM_BASE && exitCode < EXIT_CODE_CUSTOM_BASE + 1000;
}

// ============================================
// MTBF 计算辅助函数
// ============================================

// 计算 MTBF (Mean Time Between Failures)
// 返回值单位：毫秒，如果无法计算返回 0
inline ULONGLONG CalculateMTBF(ULONGLONG totalRuntimeMs, int crashCount)
{
    if (crashCount <= 0) {
        return 0;  // 无崩溃，无法计算 MTBF
    }
    return totalRuntimeMs / (ULONGLONG)crashCount;
}

// 计算失败率 (Failure Rate)
// 返回值：崩溃次数 / 启动次数，如果无法计算返回 -1.0
inline double CalculateFailureRate(int crashCount, int startCount)
{
    if (startCount <= 0) {
        return -1.0;  // 无法计算
    }
    return (double)crashCount / (double)startCount;
}

// 格式化运行时间为可读字符串
// 例如: "5d 3h 29m" 或 "2h 15m 30s"
inline void FormatRuntime(ULONGLONG runtimeMs, char* buffer, size_t bufferSize)
{
    ULONGLONG seconds = runtimeMs / 1000;
    ULONGLONG minutes = seconds / 60;
    ULONGLONG hours = minutes / 60;
    ULONGLONG days = hours / 24;

    seconds %= 60;
    minutes %= 60;
    hours %= 24;

    if (days > 0) {
        sprintf_s(buffer, bufferSize, "%llud %lluh %llum", days, hours, minutes);
    } else if (hours > 0) {
        sprintf_s(buffer, bufferSize, "%lluh %llum %llus", hours, minutes, seconds);
    } else if (minutes > 0) {
        sprintf_s(buffer, bufferSize, "%llum %llus", minutes, seconds);
    } else {
        sprintf_s(buffer, bufferSize, "%llus", seconds);
    }
}
