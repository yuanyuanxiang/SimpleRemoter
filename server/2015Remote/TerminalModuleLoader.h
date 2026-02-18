#pragma once
// TerminalModuleLoader.h - Dynamic loader for TerminalModule DLL

#include <windows.h>

// Terminal handle type
typedef void* HTERMINAL;

// Callback function types
typedef void (*TerminalInputCallback)(void* userData, const char* data, size_t len);
typedef void (*TerminalResizeCallback)(void* userData, int cols, int rows);
typedef void (*TerminalCloseCallback)(void* userData);
typedef void (*TerminalReadyCallback)(void* userData, int cols, int rows);

// Callback structure
typedef struct {
    void* userData;
    TerminalInputCallback onInput;
    TerminalResizeCallback onResize;
    TerminalCloseCallback onClose;
    TerminalReadyCallback onReady;
} TerminalCallbacks;

// Function pointer types
typedef HTERMINAL (*PFN_CreateTerminal)(HWND hParent, const TerminalCallbacks* callbacks, const char* title);
typedef HTERMINAL (*PFN_CreateEmbeddedTerminal)(HWND hHost, const TerminalCallbacks* callbacks);
typedef void (*PFN_TerminalWrite)(HTERMINAL hTerm, const char* data, size_t len);
typedef void (*PFN_TerminalSetSize)(HTERMINAL hTerm, int cols, int rows);
typedef void (*PFN_CloseTerminal)(HTERMINAL hTerm);
typedef int (*PFN_IsTerminalValid)(HTERMINAL hTerm);
typedef const char* (*PFN_GetTerminalVersion)(void);

// Global function pointers
inline PFN_CreateTerminal pfnCreateTerminal = nullptr;
inline PFN_CreateEmbeddedTerminal pfnCreateEmbeddedTerminal = nullptr;
inline PFN_TerminalWrite pfnTerminalWrite = nullptr;
inline PFN_TerminalSetSize pfnTerminalSetSize = nullptr;
inline PFN_CloseTerminal pfnCloseTerminal = nullptr;
inline PFN_IsTerminalValid pfnIsTerminalValid = nullptr;
inline PFN_GetTerminalVersion pfnGetTerminalVersion = nullptr;
inline HMODULE g_hTerminalModule = nullptr;

// Load the TerminalModule DLL
inline bool LoadTerminalModule()
{
    if (g_hTerminalModule) return true;

    // Get EXE directory
    char exePath[MAX_PATH] = {0};
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    char* lastSlash = strrchr(exePath, '\\');
    if (lastSlash) *(lastSlash + 1) = '\0';

    // DLL names to try
    const char* dllNames[] = {
#ifdef _WIN64
        "TerminalModule_x64.dll",
        "TerminalModule_x64d.dll",
#else
        "TerminalModule_x86.dll",
        "TerminalModule_x86d.dll",
#endif
        nullptr
    };

    // Try EXE directory first, then default search path
    for (int i = 0; dllNames[i]; i++) {
        // Try full path (EXE directory)
        char fullPath[MAX_PATH];
        strcpy_s(fullPath, exePath);
        strcat_s(fullPath, dllNames[i]);
        g_hTerminalModule = LoadLibraryA(fullPath);
        if (g_hTerminalModule) break;

        // Try default search path
        g_hTerminalModule = LoadLibraryA(dllNames[i]);
        if (g_hTerminalModule) break;
    }

    if (!g_hTerminalModule) {
        return false;
    }

    // Get function pointers
    pfnCreateTerminal = (PFN_CreateTerminal)GetProcAddress(g_hTerminalModule, "CreateTerminal");
    pfnCreateEmbeddedTerminal = (PFN_CreateEmbeddedTerminal)GetProcAddress(g_hTerminalModule, "CreateEmbeddedTerminal");
    pfnTerminalWrite = (PFN_TerminalWrite)GetProcAddress(g_hTerminalModule, "TerminalWrite");
    pfnTerminalSetSize = (PFN_TerminalSetSize)GetProcAddress(g_hTerminalModule, "TerminalSetSize");
    pfnCloseTerminal = (PFN_CloseTerminal)GetProcAddress(g_hTerminalModule, "CloseTerminal");
    pfnIsTerminalValid = (PFN_IsTerminalValid)GetProcAddress(g_hTerminalModule, "IsTerminalValid");
    pfnGetTerminalVersion = (PFN_GetTerminalVersion)GetProcAddress(g_hTerminalModule, "GetTerminalVersion");

    // Verify required functions
    if (!pfnCreateEmbeddedTerminal || !pfnTerminalWrite || !pfnCloseTerminal || !pfnIsTerminalValid) {
        FreeLibrary(g_hTerminalModule);
        g_hTerminalModule = nullptr;
        return false;
    }

    return true;
}

// Unload the TerminalModule DLL
inline void UnloadTerminalModule()
{
    if (g_hTerminalModule) {
        FreeLibrary(g_hTerminalModule);
        g_hTerminalModule = nullptr;
        pfnCreateTerminal = nullptr;
        pfnCreateEmbeddedTerminal = nullptr;
        pfnTerminalWrite = nullptr;
        pfnTerminalSetSize = nullptr;
        pfnCloseTerminal = nullptr;
        pfnIsTerminalValid = nullptr;
        pfnGetTerminalVersion = nullptr;
    }
}

// Check if module is loaded
inline bool IsTerminalModuleLoaded()
{
    return g_hTerminalModule != nullptr;
}

// Check if WebView2 Runtime is installed (cached)
inline bool IsWebView2Available()
{
    static int cached = -1;  // -1 = not checked, 0 = no, 1 = yes
    if (cached >= 0) return cached == 1;

    bool available = false;

    // Check registry for WebView2 installation
    const char* regPaths[] = {
        "SOFTWARE\\Microsoft\\EdgeUpdate\\Clients\\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}",
        "SOFTWARE\\WOW6432Node\\Microsoft\\EdgeUpdate\\Clients\\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}",
        nullptr
    };

    for (int i = 0; regPaths[i]; i++) {
        HKEY hKey = nullptr;
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, regPaths[i], 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            char version[128] = {0};
            DWORD size = sizeof(version);
            if (RegQueryValueExA(hKey, "pv", nullptr, nullptr, (LPBYTE)version, &size) == ERROR_SUCCESS) {
                if (version[0] != '\0' && strcmp(version, "0.0.0.0") != 0) {
                    available = true;
                }
            }
            RegCloseKey(hKey);
            if (available) break;
        }
    }

    // Also check per-user installation
    if (!available) {
        HKEY hKey = nullptr;
        if (RegOpenKeyExA(HKEY_CURRENT_USER,
            "SOFTWARE\\Microsoft\\EdgeUpdate\\Clients\\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}",
            0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            char version[128] = {0};
            DWORD size = sizeof(version);
            if (RegQueryValueExA(hKey, "pv", nullptr, nullptr, (LPBYTE)version, &size) == ERROR_SUCCESS) {
                if (version[0] != '\0' && strcmp(version, "0.0.0.0") != 0) {
                    available = true;
                }
            }
            RegCloseKey(hKey);
        }
    }

    cached = available ? 1 : 0;
    return available;
}

// Wrapper functions (for convenience)
inline HTERMINAL CreateTerminal(HWND hParent, const TerminalCallbacks* callbacks, const char* title)
{
    if (!LoadTerminalModule() || !pfnCreateTerminal) return nullptr;
    return pfnCreateTerminal(hParent, callbacks, title);
}

inline HTERMINAL CreateEmbeddedTerminal(HWND hHost, const TerminalCallbacks* callbacks)
{
    if (!LoadTerminalModule() || !pfnCreateEmbeddedTerminal) return nullptr;
    return pfnCreateEmbeddedTerminal(hHost, callbacks);
}

inline void TerminalWrite(HTERMINAL hTerm, const char* data, size_t len)
{
    if (pfnTerminalWrite) pfnTerminalWrite(hTerm, data, len);
}

inline void TerminalSetSize(HTERMINAL hTerm, int cols, int rows)
{
    if (pfnTerminalSetSize) pfnTerminalSetSize(hTerm, cols, rows);
}

inline void CloseTerminal(HTERMINAL hTerm)
{
    if (pfnCloseTerminal) pfnCloseTerminal(hTerm);
}

inline int IsTerminalValid(HTERMINAL hTerm)
{
    if (pfnIsTerminalValid) return pfnIsTerminalValid(hTerm);
    return 0;
}

inline const char* GetTerminalVersion()
{
    if (pfnGetTerminalVersion) return pfnGetTerminalVersion();
    return "0.0.0";
}
