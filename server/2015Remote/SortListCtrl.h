#pragma once
#include "afxcmn.h"
#include <string.h>

class CSortListCtrl : public CListCtrl
{
public:
    CSortListCtrl(void) : m_bAsc(false), m_nSortedCol(0) {}

    ~CSortListCtrl(void) {}

    // 鏄惁涓哄崌搴?
    bool m_bAsc;
    // 褰撳墠鎺掑垪鐨勫簭
    int m_nSortedCol;

    afx_msg void OnLvnColumnclick(NMHDR *pNMHDR, LRESULT *pResult);
    DECLARE_MESSAGE_MAP()
};
