#pragma once
#include "afxcmn.h"
#include <string.h>

class CSortListCtrl : public CListCtrl
{
public:
    CSortListCtrl(void) : m_bAsc(false), m_nSortedCol(0) {}

    ~CSortListCtrl(void) {}

    // 是否为升序
    bool m_bAsc;
    // 当前排列的序
    int m_nSortedCol;

    afx_msg void OnLvnColumnclick(NMHDR *pNMHDR, LRESULT *pResult);
    DECLARE_MESSAGE_MAP()
};
