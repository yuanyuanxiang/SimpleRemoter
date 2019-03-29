#include "StdAfx.h"
#include "LoginServer.h"
#include "Common.h"
#include <string>

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
	DWORD dwMajor, dwMinor, dwBuildNumber;
	NTPROC proc = (NTPROC)GetProcAddress(hinst, "RtlGetNtVersionNumbers"); 
	proc(&dwMajor, &dwMinor, &dwBuildNumber); 
	if (dwMajor == 6 && dwMinor == 3)	//win 8.1
	{
		vname = "Windows 8.1";
		printf_s("此电脑的版本为:%s\n", vname.c_str());
		return vname;
	}
	if (dwMajor == 10 && dwMinor == 0)	//win 10
	{
		vname = "Windows 10";
		printf_s("此电脑的版本为:%s\n", vname.c_str());
		return vname;
	}
	//下面不能判断Win Server，因为本人还未有这种系统的机子，暂时不给出

	//判断win8.1以下的版本
	SYSTEM_INFO info;                //用SYSTEM_INFO结构判断64位AMD处理器
	GetSystemInfo(&info);            //调用GetSystemInfo函数填充结构
	OSVERSIONINFOEX os;
	os.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
	if (GetVersionEx((OSVERSIONINFO *)&os))
	{
		//下面根据版本信息判断操作系统名称
		switch (os.dwMajorVersion)
		{                    //判断主版本号
		case 4:
			switch (os.dwMinorVersion)
			{                //判断次版本号
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
			switch (os.dwMinorVersion)
			{               //再比较dwMinorVersion的值
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
			switch (os.dwMinorVersion)
			{
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
		printf_s("此电脑的版本为:%s\n", vname.c_str());
	}
	else
		printf_s("版本获取失败\n");
	return vname;
}


int SendLoginInfo(IOCPClient* ClientObject,DWORD dwSpeed)
{
	LOGIN_INFOR  LoginInfor = {0};
	LoginInfor.bToken = TOKEN_LOGIN; // 令牌为登录
	//获得操作系统信息
	strcpy_s(LoginInfor.OsVerInfoEx, getSystemName().c_str());

	//获得PCName
	char szPCName[MAX_PATH] = {0};
	gethostname(szPCName, MAX_PATH);  

	//获得ClientIP
	sockaddr_in  ClientAddr;
	memset(&ClientAddr, 0, sizeof(ClientAddr));
	int iLen = sizeof(sockaddr_in);
	getsockname(ClientObject->m_sClientSocket, (SOCKADDR*)&ClientAddr, &iLen);

	DWORD	dwCPUMHz;
	dwCPUMHz = CPUClockMHz();

	BOOL bWebCamIsExist = WebCamIsExist();

	memcpy(LoginInfor.szPCName,szPCName,MAX_PATH);
	LoginInfor.dwSpeed  = dwSpeed;
	LoginInfor.dwCPUMHz = dwCPUMHz;
	LoginInfor.ClientAddr = ClientAddr.sin_addr;
	LoginInfor.bWebCamIsExist = bWebCamIsExist;

	int iRet = ClientObject->OnServerSending((char*)&LoginInfor, sizeof(LOGIN_INFOR));   

	return iRet;
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
	for (int i = 0; i < 10 && !bOk; ++i)
	{
		bOk = capGetDriverDescription(i, szDeviceName, sizeof(szDeviceName),     
			//系统的API函数
			szVer, sizeof(szVer));
	}
	return bOk;
}
