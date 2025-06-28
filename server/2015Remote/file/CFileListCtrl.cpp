#include "StdAfx.h"
#include "2015Remote.h"
#include "CFileListCtrl.h"
#include "CFileManagerDlg.h"

using namespace file;

BEGIN_MESSAGE_MAP(CFileListCtrl, CListCtrl)
    ON_WM_DROPFILES()
END_MESSAGE_MAP()

void CFileListCtrl::SetParenDlg(VOID* p_CFileManagerDlg)
{
    m_pCFileManagerDlg = p_CFileManagerDlg;
}

void CFileListCtrl::OnDropFiles(HDROP hDropInfo)
{
    TCHAR filePath[MAX_PATH] = { 0 };
    DragQueryFile(hDropInfo, 0, filePath, sizeof(filePath) * 2 + 2);
    ((CFileManagerDlg*)m_pCFileManagerDlg)->TransferSend(filePath);
    ::DragFinish(hDropInfo);
    CListCtrl::OnDropFiles(hDropInfo);
}
