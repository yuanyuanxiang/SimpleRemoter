#include "StdAfx.h"
#include "SortListCtrl.h"

BEGIN_MESSAGE_MAP(CSortListCtrl, CListCtrl)
    ON_NOTIFY_REFLECT(LVN_COLUMNCLICK, CSortListCtrl::OnLvnColumnclick)
END_MESSAGE_MAP()

int CALLBACK ListCompare(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{
    CSortListCtrl* This = (CSortListCtrl*)lParamSort;
    CString item1 = This->GetItemText(lParam1, This->m_nSortedCol);
    CString item2 = This->GetItemText(lParam2, This->m_nSortedCol);
	int asc = This->m_bAsc ? 1 : -1;
    int ret = item1.Compare(item2);
    return ret * asc;
}

void CSortListCtrl::OnLvnColumnclick(NMHDR *pNMHDR, LRESULT *pResult)
{
    LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);

    if(pNMLV->iSubItem == m_nSortedCol) {
        m_bAsc = !m_bAsc;
    } else {
        m_bAsc = TRUE;
        m_nSortedCol = pNMLV->iSubItem;
    }

    SortItemsEx(ListCompare, (DWORD_PTR)this);

    UpdateData(FALSE);

    *pResult = 0;
}
