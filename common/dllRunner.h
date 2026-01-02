#pragma once
#include <windows.h>

// A DLL runner.
class DllRunner
{
public:
    virtual ~DllRunner() {}
    virtual void* LoadLibraryA(const char* path, int size = 0) = 0;
    virtual FARPROC GetProcAddress(void* mod, const char* lpProcName) = 0;
    virtual BOOL FreeLibrary(void* mod) = 0;
};

// Default DLL runner.
class DefaultDllRunner : public DllRunner
{
private:
    std::string m_path;
    HMODULE m_mod;
public:
    DefaultDllRunner(const std::string &path="") :m_path(path), m_mod(nullptr) {}
    // Load DLL from the disk.
    virtual void* LoadLibraryA(const char* path, int size = 0)
    {
        return m_mod = ::LoadLibraryA(size ? m_path.c_str() : path);
    }
    virtual FARPROC GetProcAddress(void* mod, const char* lpProcName)
    {
        return ::GetProcAddress(m_mod, lpProcName);
    }
    virtual BOOL FreeLibrary(void* mod)
    {
        return ::FreeLibrary(m_mod);
    }
};
