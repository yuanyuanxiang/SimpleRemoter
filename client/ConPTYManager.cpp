// ConPTYManager.cpp: Windows ConPTY terminal manager implementation
// Provides xterm.js compatible terminal for Windows 10 1809+

#include "stdafx.h"
#include "ConPTYManager.h"
#include "Common.h"
#include "../common/commands.h"

// Define PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE if not available (older SDK)
#ifndef PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE
#define PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE \
    ProcThreadAttributeValue(22, FALSE, TRUE, FALSE)
#endif

// Static members
PFN_CreatePseudoConsole CConPTYManager::s_pfnCreatePseudoConsole = nullptr;
PFN_ResizePseudoConsole CConPTYManager::s_pfnResizePseudoConsole = nullptr;
PFN_ClosePseudoConsole CConPTYManager::s_pfnClosePseudoConsole = nullptr;
bool CConPTYManager::s_bApiLoaded = false;

bool CConPTYManager::LoadConPTYApi()
{
    if (s_bApiLoaded) {
        return s_pfnCreatePseudoConsole != nullptr;
    }
    s_bApiLoaded = true;

    HMODULE hKernel = GetModuleHandleA("kernel32.dll");
    if (!hKernel) return false;

    s_pfnCreatePseudoConsole = (PFN_CreatePseudoConsole)GetProcAddress(hKernel, "CreatePseudoConsole");
    s_pfnResizePseudoConsole = (PFN_ResizePseudoConsole)GetProcAddress(hKernel, "ResizePseudoConsole");
    s_pfnClosePseudoConsole = (PFN_ClosePseudoConsole)GetProcAddress(hKernel, "ClosePseudoConsole");

    return s_pfnCreatePseudoConsole && s_pfnResizePseudoConsole && s_pfnClosePseudoConsole;
}

bool CConPTYManager::IsConPTYSupported()
{
    return LoadConPTYApi();
}

CConPTYManager::CConPTYManager(IOCPClient* ClientObject, int n, void* user)
    : CManager(ClientObject)
    , m_hPC(nullptr)
    , m_hPipeIn(nullptr)
    , m_hPipeOut(nullptr)
    , m_hShellProcess(nullptr)
    , m_hShellThread(nullptr)
    , m_hReadThread(nullptr)
    , m_bRunning(TRUE)
    , m_cols(80)
    , m_rows(24)
{
    if (!LoadConPTYApi()) {
        Mprintf("[ConPTY] API not available\n");
        return;
    }

    // Initialize with default size, will be resized when server sends size
    if (!InitializeConPTY(m_cols, m_rows)) {
        Mprintf("[ConPTY] Failed to initialize\n");
        return;
    }

    // Send terminal start token
    BYTE bToken = TOKEN_TERMINAL_START;
    HttpMask mask(DEFAULT_HOST, m_ClientObject->GetClientIPHeader());
    m_ClientObject->Send2Server((char*)&bToken, 1, &mask);

    WaitForDialogOpen();

    // Start read thread
    m_hReadThread = __CreateThread(NULL, 0, ReadThread, (LPVOID)this, 0, NULL);
}

CConPTYManager::~CConPTYManager()
{
    m_bRunning = FALSE;

    // Close pipes first to unblock ReadThread
    if (m_hPipeIn) {
        CloseHandle(m_hPipeIn);
        m_hPipeIn = nullptr;
    }
    if (m_hPipeOut) {
        CloseHandle(m_hPipeOut);
        m_hPipeOut = nullptr;
    }

    // Wait for read thread with timeout
    int waitCount = 0;
    while (m_hReadThread && waitCount < 30) {  // 3 second timeout
        Sleep(100);
        waitCount++;
    }

    // Close ConPTY
    if (m_hPC && s_pfnClosePseudoConsole) {
        s_pfnClosePseudoConsole(m_hPC);
        m_hPC = nullptr;
    }

    // Terminate process if still running
    if (m_hShellProcess) {
        DWORD exitCode = 0;
        if (GetExitCodeProcess(m_hShellProcess, &exitCode) && exitCode == STILL_ACTIVE) {
            TerminateProcess(m_hShellProcess, 0);
        }
        CloseHandle(m_hShellProcess);
        m_hShellProcess = nullptr;
    }
    if (m_hShellThread) {
        CloseHandle(m_hShellThread);
        m_hShellThread = nullptr;
    }
}

bool CConPTYManager::InitializeConPTY(int cols, int rows)
{
    // Create pipes
    HANDLE hPipeInRead = nullptr, hPipeInWrite = nullptr;
    HANDLE hPipeOutRead = nullptr, hPipeOutWrite = nullptr;

    if (!CreatePipe(&hPipeInRead, &hPipeInWrite, nullptr, 0)) {
        Mprintf("[ConPTY] CreatePipe(in) failed: %d\n", GetLastError());
        return false;
    }
    if (!CreatePipe(&hPipeOutRead, &hPipeOutWrite, nullptr, 0)) {
        Mprintf("[ConPTY] CreatePipe(out) failed: %d\n", GetLastError());
        CloseHandle(hPipeInRead);
        CloseHandle(hPipeInWrite);
        return false;
    }

    // Create pseudo console
    COORD size = { (SHORT)cols, (SHORT)rows };
    HRESULT hr = s_pfnCreatePseudoConsole(size, hPipeInRead, hPipeOutWrite, 0, &m_hPC);
    if (FAILED(hr)) {
        Mprintf("[ConPTY] CreatePseudoConsole failed: 0x%08X\n", hr);
        CloseHandle(hPipeInRead);
        CloseHandle(hPipeInWrite);
        CloseHandle(hPipeOutRead);
        CloseHandle(hPipeOutWrite);
        return false;
    }

    // We read from hPipeOutRead (cmd output) and write to hPipeInWrite (cmd input)
    m_hPipeIn = hPipeOutRead;
    m_hPipeOut = hPipeInWrite;

    // Close handles passed to ConPTY (they're now owned by ConPTY)
    CloseHandle(hPipeInRead);
    CloseHandle(hPipeOutWrite);

    // Prepare startup info with pseudo console attribute
    STARTUPINFOEXW si = {};
    si.StartupInfo.cb = sizeof(si);

    SIZE_T attrListSize = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attrListSize);
    si.lpAttributeList = (PPROC_THREAD_ATTRIBUTE_LIST)HeapAlloc(GetProcessHeap(), 0, attrListSize);
    if (!si.lpAttributeList) {
        Mprintf("[ConPTY] HeapAlloc failed\n");
        return false;
    }

    if (!InitializeProcThreadAttributeList(si.lpAttributeList, 1, 0, &attrListSize)) {
        Mprintf("[ConPTY] InitializeProcThreadAttributeList failed: %d\n", GetLastError());
        HeapFree(GetProcessHeap(), 0, si.lpAttributeList);
        return false;
    }

    // PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE = 0x00020016
    if (!UpdateProcThreadAttribute(si.lpAttributeList, 0,
            PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, m_hPC, sizeof(m_hPC), nullptr, nullptr)) {
        Mprintf("[ConPTY] UpdateProcThreadAttribute failed: %d\n", GetLastError());
        DeleteProcThreadAttributeList(si.lpAttributeList);
        HeapFree(GetProcessHeap(), 0, si.lpAttributeList);
        return false;
    }

    // Get cmd.exe path
    WCHAR cmdPath[MAX_PATH] = {};
    GetSystemDirectoryW(cmdPath, MAX_PATH);
    wcscat_s(cmdPath, L"\\cmd.exe");

    // Create process
    PROCESS_INFORMATION pi = {};
    if (!CreateProcessW(nullptr, cmdPath, nullptr, nullptr, FALSE,
            EXTENDED_STARTUPINFO_PRESENT, nullptr, nullptr, &si.StartupInfo, &pi)) {
        Mprintf("[ConPTY] CreateProcess failed: %d\n", GetLastError());
        DeleteProcThreadAttributeList(si.lpAttributeList);
        HeapFree(GetProcessHeap(), 0, si.lpAttributeList);
        return false;
    }

    m_hShellProcess = pi.hProcess;
    m_hShellThread = pi.hThread;
    m_cols = cols;
    m_rows = rows;

    DeleteProcThreadAttributeList(si.lpAttributeList);
    HeapFree(GetProcessHeap(), 0, si.lpAttributeList);

    Mprintf("[ConPTY] Started cmd.exe (PID=%d) with %dx%d terminal\n", pi.dwProcessId, cols, rows);
    return true;
}

void CConPTYManager::ResizeTerminal(int cols, int rows)
{
    if (m_hPC && s_pfnResizePseudoConsole) {
        COORD size = { (SHORT)cols, (SHORT)rows };
        HRESULT hr = s_pfnResizePseudoConsole(m_hPC, size);
        if (SUCCEEDED(hr)) {
            m_cols = cols;
            m_rows = rows;
            Mprintf("[ConPTY] Resized to %dx%d\n", cols, rows);
        } else {
            Mprintf("[ConPTY] ResizePseudoConsole failed: 0x%08X\n", hr);
        }
    }
}

VOID CConPTYManager::OnReceive(PBYTE szBuffer, ULONG ulLength)
{
    if (ulLength == 0) return;

    switch (szBuffer[0]) {
    case COMMAND_NEXT:
        NotifyDialogIsOpen();
        break;

    case CMD_TERMINAL_RESIZE:
        // Resize command: [cmd:1][cols:2][rows:2]
        if (ulLength >= 5) {
            int cols = *(short*)(szBuffer + 1);
            int rows = *(short*)(szBuffer + 3);
            ResizeTerminal(cols, rows);
        }
        break;

    default:
        // User input - write to PTY
        if (m_hPipeOut) {
            DWORD dwWritten = 0;
            WriteFile(m_hPipeOut, szBuffer, ulLength, &dwWritten, nullptr);
        }
        break;
    }
}

DWORD WINAPI CConPTYManager::ReadThread(LPVOID lParam)
{
    CConPTYManager* pThis = (CConPTYManager*)lParam;
    char buffer[4096];

    while (pThis->m_bRunning) {
        // Check if process has exited
        if (pThis->m_hShellProcess) {
            DWORD exitCode = 0;
            if (GetExitCodeProcess(pThis->m_hShellProcess, &exitCode)) {
                if (exitCode != STILL_ACTIVE) {
                    Mprintf("[ConPTY] Process exited with code %d\n", exitCode);
                    break;
                }
            }
        }

        // Check if pipe handle is still valid
        if (!pThis->m_hPipeIn) {
            Mprintf("[ConPTY] Pipe handle is null\n");
            break;
        }

        // Check if data is available (non-blocking)
        DWORD dwAvailable = 0;
        if (!PeekNamedPipe(pThis->m_hPipeIn, nullptr, 0, nullptr, &dwAvailable, nullptr)) {
            DWORD err = GetLastError();
            if (err == ERROR_BROKEN_PIPE || err == ERROR_INVALID_HANDLE) {
                Mprintf("[ConPTY] Pipe closed (err=%d)\n", err);
                break;
            }
            // Other error, wait and retry
            Sleep(10);
            continue;
        }

        if (dwAvailable == 0) {
            // No data available, wait a bit
            Sleep(10);
            continue;
        }

        // Read available data
        DWORD dwRead = 0;
        DWORD toRead = min(dwAvailable, (DWORD)sizeof(buffer));
        if (!ReadFile(pThis->m_hPipeIn, buffer, toRead, &dwRead, nullptr)) {
            DWORD err = GetLastError();
            if (err != ERROR_BROKEN_PIPE && err != ERROR_INVALID_HANDLE) {
                Mprintf("[ConPTY] ReadFile failed: %d\n", err);
            }
            break;
        }

        if (dwRead > 0) {
            // Send to server
            pThis->m_ClientObject->Send2Server(buffer, dwRead);
        }
    }

    // Send close notification
    if (pThis->m_ClientObject) {
        BYTE closeToken = TOKEN_TERMINAL_CLOSE;
        pThis->m_ClientObject->Send2Server((char*)&closeToken, 1);
        Mprintf("[ConPTY] Sent TOKEN_TERMINAL_CLOSE\n");
    }

    SAFE_CLOSE_HANDLE(pThis->m_hReadThread);
    pThis->m_hReadThread = nullptr;
    Mprintf("[ConPTY] Read thread exited\n");
    return 0;
}
