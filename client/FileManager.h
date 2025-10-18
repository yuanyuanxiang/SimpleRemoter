// FileManager.h: interface for the CFileManager class.
//
//////////////////////////////////////////////////////////////////////
#include "IOCPClient.h"
#include "common.h"
typedef IOCPClient CClientSocket;

#if !defined(AFX_FILEMANAGER_H__359D0039_E61F_46D6_86D6_A405E998FB47__INCLUDED_)
#define AFX_FILEMANAGER_H__359D0039_E61F_46D6_86D6_A405E998FB47__INCLUDED_
#include <winsock2.h>
#include <list>
#include <string>

#include "Manager.h"

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

typedef struct {
    UINT	nFileSize;	// �ļ���С
    UINT	nSendSize;	// �ѷ��ʹ�С
} SENDFILEPROGRESS, *PSENDFILEPROGRESS;


class CFileManager : public CManager
{
public:
    virtual void OnReceive(PBYTE lpBuffer, ULONG nSize);
    UINT SendDriveList();
    CFileManager(CClientSocket *pClient, int h = 0, void* user=nullptr);
    virtual ~CFileManager();
private:
    std::list <std::string> m_UploadList;
    UINT m_nTransferMode;
    char m_strCurrentProcessFileName[MAX_PATH]; // ��ǰ���ڴ�����ļ�
    __int64 m_nCurrentProcessFileLength; // ��ǰ���ڴ�����ļ��ĳ���
    bool MakeSureDirectoryPathExists(LPCTSTR pszDirPath);
    bool UploadToRemote(LPBYTE lpBuffer);
    bool FixedUploadList(LPCTSTR lpszDirectory);
    void StopTransfer();
    UINT SendFilesList(LPCTSTR lpszDirectory);
    bool DeleteDirectory(LPCTSTR lpszDirectory);
    UINT SendFileSize(LPCTSTR lpszFileName);
    UINT SendFileData(LPBYTE lpBuffer);
    void CreateFolder(LPBYTE lpBuffer);
    void Rename(LPBYTE lpBuffer);
    int	SendToken(BYTE bToken);

    void CreateLocalRecvFile(LPBYTE lpBuffer);
    void SetTransferMode(LPBYTE lpBuffer);
    void GetFileData();
    void WriteLocalRecvFile(LPBYTE lpBuffer, UINT nSize);
    void UploadNext();
    bool OpenFile(LPCTSTR lpFile, INT nShowCmd);
};

#endif // !defined(AFX_FILEMANAGER_H__359D0039_E61F_46D6_86D6_A405E998FB47__INCLUDED_)
