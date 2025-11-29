// clang_rt_compat.c
// 兼容 32 位 Clang 编译的 libx264 运行时函数

#ifdef _M_IX86

#pragma comment(linker, "/alternatename:__ultof3=_ultof3_impl")
#pragma comment(linker, "/alternatename:__dtoul3_legacy=_dtoul3_impl")
#pragma comment(linker, "/alternatename:__dtol3=_dtol3_impl")
#pragma comment(linker, "/alternatename:__ltod3=_ltod3_impl")
#pragma comment(linker, "/alternatename:__ultod3=_ultod3_impl")

// unsigned long long to float
float __cdecl ultof3_impl(unsigned long long a)
{
    return (float)a;
}

// double to unsigned long long
unsigned long long __cdecl dtoul3_impl(double a)
{
    if (a < 0) return 0;
    if (a >= 18446744073709551616.0) return 0xFFFFFFFFFFFFFFFFULL;
    return (unsigned long long)a;
}

// double to long long
long long __cdecl dtol3_impl(double a)
{
    return (long long)a;
}

// long long to double
double __cdecl ltod3_impl(long long a)
{
    return (double)a;
}

// unsigned long long to double
double __cdecl ultod3_impl(unsigned long long a)
{
    return (double)a;
}

#endif
