#include "stdafx.h"

#pragma once
#include <2015RemoteDlg.h>

/////////////////////////////////////////////////////////////////////////////
// CMachineDlg dialog

// TODO: ʵ��IP��ȡ.
#include "common/location.h"


class CMachineDlg : public DialogBase
{
public:
    CMachineDlg(CWnd* pParent = NULL, Server* pIOCPServer = NULL, ClientContext* pContext = NULL);
    ~CMachineDlg();

    enum { IDD = IDD_MACHINE };
    CListCtrl   m_list;
    CTabCtrl    m_tab;

    void OnReceiveComplete();
    static int CALLBACK CompareFunction(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort);

protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    afx_msg void OnClose();
    virtual BOOL OnNotify(WPARAM wParam, LPARAM lParam, LRESULT* pResult);

    int                 m_nSortedCol;
    bool                m_bAscending;
    bool                m_bIsReceiving;
    CMy2015RemoteDlg*   m_pMainWnd;
    CStatusBar          m_wndStatusBar;
    IPConverter*        m_IPConverter;
    CLocker             m_Locker;
    bool IsReceivingData() {
        m_Locker.Lock();
        auto r = m_bIsReceiving;
        m_Locker.Unlock();
        return r;
    }
    void SetReceivingStatus(bool b) {
		m_Locker.Lock();
		m_bIsReceiving = b;
		m_Locker.Unlock();
    }
    virtual BOOL OnInitDialog();
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnDblclkList(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnRclickList(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnSelChangeTab(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnSelChangingTab(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg LRESULT OnShowMessage(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnWaitMessage(WPARAM wParam, LPARAM lParam);
    void SortColumn(int iCol, bool bAsc);
    CString oleTime2Str(double time);
    void reflush();

    DECLARE_MESSAGE_MAP()
public:
    void SendToken(BYTE bToken);
    void AdjustList();
    void OpenInfoDlg();
    void SetClipboardText(CString& Data);
    CString __MakePriority(DWORD dwPriClass);
    void DeleteList();
    void ShowProcessList(); //����
    void ShowWindowsList();//����
    void ShowNetStateList();//����
    void ShowSoftWareList();//����б�
    void ShowIEHistoryList();//�����¼
    void ShowFavoritesUrlList();//�ղؼ�
    void ShowServiceList(); //����
    void ShowTaskList();//�ƻ�����
    void ShowHostsList();//HOSTS

    //��Ӧ�˵�
    void ShowProcessList_menu(); //����
    void ShowWindowsList_menu();//����
    void ShowNetStateList_menu();//����
    void ShowSoftWareList_menu();//����б�
    void ShowIEHistoryList_menu();//�����¼
    void ShowFavoritesUrlList_menu();//�ղؼ�
    void ShowServiceList_menu();//����
    void ShowTaskList_menu();//�ƻ�����
    void ShowHostsList_menu();//HOSTS
};

struct  Browsinghistory {
    TCHAR strTime[100];
    TCHAR strTitle[1024];
    TCHAR strUrl[1024];
};

struct  InjectData {
    DWORD ExeIsx86;
    DWORD mode;		        //ע��ģʽ
    DWORD dwProcessID;      //����ID
    DWORD datasize;         //�������ݳߴ�
    TCHAR strpath[1024];    //Զ�����Ŀ¼
};
