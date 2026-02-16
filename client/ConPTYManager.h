// ConPTYManager.h: Windows ConPTY terminal manager for xterm.js support
// Requires Windows 10 1809+ for ConPTY API

#ifndef CONPTYMANAGER_H
#define CONPTYMANAGER_H

#include "Manager.h"
#include "IOCPClient.h"

// ConPTY API types (dynamically loaded)
typedef VOID* HPCON;
typedef HRESULT (WINAPI *PFN_CreatePseudoConsole)(COORD size, HANDLE hInput, HANDLE hOutput, DWORD dwFlags, HPCON* phPC);
typedef HRESULT (WINAPI *PFN_ResizePseudoConsole)(HPCON hPC, COORD size);
typedef void    (WINAPI *PFN_ClosePseudoConsole)(HPCON hPC);

class CConPTYManager : public CManager
{
public:
    CConPTYManager(IOCPClient* ClientObject, int n, void* user = nullptr);
    virtual ~CConPTYManager();

    VOID OnReceive(PBYTE szBuffer, ULONG ulLength);

    // Check if ConPTY is supported on this system
    static bool IsConPTYSupported();

private:
    // ConPTY handles
    HPCON m_hPC;
    HANDLE m_hPipeIn;       // Read from cmd output
    HANDLE m_hPipeOut;      // Write to cmd input
    HANDLE m_hShellProcess;
    HANDLE m_hShellThread;
    HANDLE m_hReadThread;

    // State
    BOOL m_bRunning;
    int m_cols;
    int m_rows;

    // ConPTY API function pointers
    static PFN_CreatePseudoConsole s_pfnCreatePseudoConsole;
    static PFN_ResizePseudoConsole s_pfnResizePseudoConsole;
    static PFN_ClosePseudoConsole s_pfnClosePseudoConsole;
    static bool s_bApiLoaded;

    // Load ConPTY API
    static bool LoadConPTYApi();

    // Initialize ConPTY and start cmd.exe
    bool InitializeConPTY(int cols, int rows);

    // Resize terminal
    void ResizeTerminal(int cols, int rows);

    // Thread to read from PTY
    static DWORD WINAPI ReadThread(LPVOID lParam);
};

#endif // CONPTYMANAGER_H
