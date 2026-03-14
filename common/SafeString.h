// SafeString.h - 安全字符串函数包装
// 解决 _s 系列函数参数无效时崩溃且无 dump 的问题
//
// 使用方法:
// 1. 在程序入口调用 InstallSafeStringHandler()
// 2. 可选：使用 safe_strcpy 等包装函数替代 strcpy_s

#pragma once
#include <crtdbg.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#ifdef _WIN32
#include <windows.h>
#endif

#ifdef __cplusplus

// ============================================================================
// Invalid Parameter Handler (C++ only)
// ============================================================================

// 静默 handler: 不崩溃，让函数返回错误码
// 注意：不能使用 thread_local 或 Logger，因为 shellcode 线程没有 TLS 初始化
inline void __cdecl SafeInvalidParameterHandler(
    const wchar_t* expression,
    const wchar_t* function,
    const wchar_t* file,
    unsigned int line,
    uintptr_t pReserved)
{
    (void)expression; (void)function; (void)file; (void)line; (void)pReserved;

#ifdef _DEBUG
    // Debug 模式输出到调试器（OutputDebugStringW 是线程安全的）
    OutputDebugStringW(L"[SafeString] Invalid parameter in CRT function!\n");
#endif

    // 不调用 abort()，让函数返回错误码 (EINVAL/ERANGE)
}

inline void InstallSafeStringHandler()
{
    // 设置线程级 handler
    _set_invalid_parameter_handler(SafeInvalidParameterHandler);

    // Debug 模式下禁用 CRT 断言弹窗
    #ifdef _DEBUG
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_DEBUG);
    #endif
}

extern "C" {
#endif

// ============================================================================
// 安全包装函数 (C/C++ 通用)
// ============================================================================
// 在调用前检查参数，避免触发 handler

// 安全 strcpy - 不会崩溃，失败返回 EINVAL/ERANGE
inline errno_t safe_strcpy(char* dest, size_t destSize, const char* src)
{
    if (!dest || destSize == 0) return EINVAL;
    if (!src) {
        dest[0] = '\0';
        return EINVAL;
    }

    size_t srcLen = strlen(src);
    if (srcLen >= destSize) {
        // 截断而不是崩溃
        memcpy(dest, src, destSize - 1);
        dest[destSize - 1] = '\0';
        return ERANGE;
    }

    memcpy(dest, src, srcLen + 1);
    return 0;
}

// 安全 strncpy
inline errno_t safe_strncpy(char* dest, size_t destSize, const char* src, size_t count)
{
    if (!dest || destSize == 0) return EINVAL;
    if (!src) {
        dest[0] = '\0';
        return EINVAL;
    }

    size_t srcLen = strlen(src);
    size_t copyLen = (srcLen < count) ? srcLen : count;

    if (copyLen >= destSize) {
        copyLen = destSize - 1;
    }

    memcpy(dest, src, copyLen);
    dest[copyLen] = '\0';
    return (srcLen >= destSize || srcLen > count) ? ERANGE : 0;
}

// 安全 strcat
inline errno_t safe_strcat(char* dest, size_t destSize, const char* src)
{
    if (!dest || destSize == 0) return EINVAL;
    if (!src) return EINVAL;

    size_t destLen = strlen(dest);
    if (destLen >= destSize) return EINVAL;

    size_t srcLen = strlen(src);
    size_t remaining = destSize - destLen - 1;

    if (srcLen > remaining) {
        // 截断
        memcpy(dest + destLen, src, remaining);
        dest[destSize - 1] = '\0';
        return ERANGE;
    }

    memcpy(dest + destLen, src, srcLen + 1);
    return 0;
}

// 安全 sprintf (返回写入的字符数，失败返回 -1)
inline int safe_sprintf(char* dest, size_t destSize, const char* format, ...)
{
    if (!dest || destSize == 0 || !format) {
        if (dest && destSize > 0) dest[0] = '\0';
        return -1;
    }

    va_list args;
    va_start(args, format);
    int result = _vsnprintf_s(dest, destSize, _TRUNCATE, format, args);
    va_end(args);

    return result;
}

// 安全 memcpy
inline errno_t safe_memcpy(void* dest, size_t destSize, const void* src, size_t count)
{
    if (!dest || destSize == 0) return EINVAL;
    if (!src && count > 0) return EINVAL;
    if (count == 0) return 0;

    if (count > destSize) {
        // 只复制能放下的部分
        memcpy(dest, src, destSize);
        return ERANGE;
    }

    memcpy(dest, src, count);
    return 0;
}

// 宽字符版本
inline errno_t safe_wcscpy(wchar_t* dest, size_t destSize, const wchar_t* src)
{
    if (!dest || destSize == 0) return EINVAL;
    if (!src) {
        dest[0] = L'\0';
        return EINVAL;
    }

    size_t srcLen = wcslen(src);
    if (srcLen >= destSize) {
        wmemcpy(dest, src, destSize - 1);
        dest[destSize - 1] = L'\0';
        return ERANGE;
    }

    wmemcpy(dest, src, srcLen + 1);
    return 0;
}

inline errno_t safe_wcscat(wchar_t* dest, size_t destSize, const wchar_t* src)
{
    if (!dest || destSize == 0) return EINVAL;
    if (!src) return EINVAL;

    size_t destLen = wcslen(dest);
    if (destLen >= destSize) return EINVAL;

    size_t srcLen = wcslen(src);
    size_t remaining = destSize - destLen - 1;

    if (srcLen > remaining) {
        wmemcpy(dest + destLen, src, remaining);
        dest[destSize - 1] = L'\0';
        return ERANGE;
    }

    wmemcpy(dest + destLen, src, srcLen + 1);
    return 0;
}

#ifdef __cplusplus
}
#endif
