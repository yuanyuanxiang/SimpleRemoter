// FileManager.h: interface for the CFileManager class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_FILEMANAGER_H__FA7A4DE1_0123_47FD_84CE_85F4B24149CE__INCLUDED_)
#define AFX_FILEMANAGER_H__FA7A4DE1_0123_47FD_84CE_85F4B24149CE__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Manager.h"
#include "IOCPClient.h"


typedef struct 
{
	DWORD	dwSizeHigh;
	DWORD	dwSizeLow;
}FILE_SIZE;

class CFileManager : public CManager  
{
public:
	CFileManager(IOCPClient* ClientObject, int n);
	virtual ~CFileManager();

	VOID  OnReceive(PBYTE szBuffer, ULONG ulLength);
	ULONG CFileManager::SendDiskDriverList()  ;
	ULONG CFileManager::SendFilesList(char* szDirectoryPath);
	VOID CFileManager::CreateClientRecvFile(LPBYTE szBuffer);

	BOOL  CFileManager::MakeSureDirectoryPathExists(char* szDirectoryFullPath);
	char   m_szOperatingFileName[MAX_PATH];
	__int64 m_OperatingFileLength;
	VOID CFileManager::GetFileData()  ;
	VOID CFileManager::WriteClientRecvFile(LPBYTE szBuffer, ULONG ulLength);

	ULONG m_ulTransferMode;
	VOID CFileManager::SetTransferMode(LPBYTE szBuffer);
	VOID  CFileManager::Rename(char* szExistingFileFullPath,char* szNewFileFullPath);
};

#endif // !defined(AFX_FILEMANAGER_H__FA7A4DE1_0123_47FD_84CE_85F4B24149CE__INCLUDED_)
