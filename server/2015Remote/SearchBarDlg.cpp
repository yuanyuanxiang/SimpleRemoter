// SearchBarDlg.cpp - 浮动搜索工具栏实现
#include "stdafx.h"
#include "SearchBarDlg.h"
#include "2015RemoteDlg.h"

IMPLEMENT_DYNAMIC(CSearchBarDlg, CDialogEx)

CSearchBarDlg::CSearchBarDlg(CMy2015RemoteDlg* pParent)
    : CDialogLangEx(IDD_SEARCH_BAR, pParent)
    , m_pParent(pParent)
    , m_nCurrentIndex(-1)
{
    m_brushEdit.CreateSolidBrush(RGB(60, 60, 60));
    m_brushStatic.CreateSolidBrush(RGB(40, 40, 40));
}

CSearchBarDlg::~CSearchBarDlg()
{
    m_brushEdit.DeleteObject();
    m_brushStatic.DeleteObject();
}

void CSearchBarDlg::DoDataExchange(CDataExchange* pDX)
{
    __super::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_SEARCH_EDIT, m_editSearch);
    DDX_Control(pDX, IDC_SEARCH_COUNT, m_staticCount);
}

BEGIN_MESSAGE_MAP(CSearchBarDlg, CDialogEx)
    ON_BN_CLICKED(IDC_SEARCH_PREV, &CSearchBarDlg::OnBnClickedPrev)
    ON_BN_CLICKED(IDC_SEARCH_NEXT, &CSearchBarDlg::OnBnClickedNext)
    ON_BN_CLICKED(IDC_SEARCH_CLOSE, &CSearchBarDlg::OnBnClickedClose)
    ON_EN_CHANGE(IDC_SEARCH_EDIT, &CSearchBarDlg::OnEnChangeSearch)
    ON_WM_TIMER()
    ON_WM_ERASEBKGND()
    ON_WM_CTLCOLOR()
END_MESSAGE_MAP()

BOOL CSearchBarDlg::OnInitDialog()
{
    __super::OnInitDialog();

    // 设置分层窗口样式（透明 + 不抢焦点）
    ModifyStyleEx(0, WS_EX_LAYERED | WS_EX_NOACTIVATE);
    SetLayeredWindowAttributes(0, 200, LWA_ALPHA);

    // 子类化按钮为 CIconButton
    m_btnPrev.SubclassDlgItem(IDC_SEARCH_PREV, this);
    m_btnNext.SubclassDlgItem(IDC_SEARCH_NEXT, this);
    m_btnClose.SubclassDlgItem(IDC_SEARCH_CLOSE, this);

    // 设置图标
    m_btnPrev.SetIconDrawFunc(CIconButton::DrawIconArrowLeft);
    m_btnNext.SetIconDrawFunc(CIconButton::DrawIconArrowRight);
    m_btnClose.SetIconDrawFunc(CIconButton::DrawIconClose);
    m_btnClose.SetIsCloseButton(true);

    // 创建工具提示
    m_tooltip.Create(this);
    m_tooltip.Activate(TRUE);
    m_tooltip.AddTool(&m_btnPrev, _TR("上一个 (Shift+Enter)"));
    m_tooltip.AddTool(&m_btnNext, _TR("下一个 (Enter)"));
    m_tooltip.AddTool(&m_btnClose, _TR("关闭 (Esc)"));

    // 设置输入框提示文本
    m_editSearch.SetCueBanner(L"Search...", TRUE);

    // 初始化计数文本
    m_staticCount.SetWindowText(_T(""));

    return TRUE;
}

BOOL CSearchBarDlg::PreTranslateMessage(MSG* pMsg)
{
    if (m_tooltip.GetSafeHwnd()) {
        m_tooltip.RelayEvent(pMsg);
    }

    if (pMsg->message == WM_KEYDOWN) {
        if (pMsg->wParam == VK_ESCAPE) {
            Hide();
            return TRUE;
        }
        if (pMsg->wParam == VK_RETURN) {
            if (GetKeyState(VK_SHIFT) & 0x8000) {
                GotoPrev();
            } else {
                GotoNext();
            }
            return TRUE;
        }
        // F3 = 下一个, Shift+F3 = 上一个
        if (pMsg->wParam == VK_F3) {
            if (GetKeyState(VK_SHIFT) & 0x8000) {
                GotoPrev();
            } else {
                GotoNext();
            }
            return TRUE;
        }
    }

    return __super::PreTranslateMessage(pMsg);
}

BOOL CSearchBarDlg::OnEraseBkgnd(CDC* pDC)
{
    CRect rect;
    GetClientRect(&rect);
    pDC->FillSolidRect(rect, RGB(40, 40, 40));
    return TRUE;
}

HBRUSH CSearchBarDlg::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
    HBRUSH hbr = __super::OnCtlColor(pDC, pWnd, nCtlColor);

    // 输入框深色背景
    if (nCtlColor == CTLCOLOR_EDIT && pWnd->GetDlgCtrlID() == IDC_SEARCH_EDIT) {
        pDC->SetTextColor(RGB(240, 240, 240));
        pDC->SetBkColor(RGB(60, 60, 60));
        return m_brushEdit;
    }

    // 静态文本（计数）- 使用实色背景避免重绘问题
    if (nCtlColor == CTLCOLOR_STATIC) {
        pDC->SetTextColor(RGB(180, 180, 180));
        pDC->SetBkColor(RGB(40, 40, 40));
        return m_brushStatic;
    }

    return hbr;
}

void CSearchBarDlg::Show()
{
    if (!GetSafeHwnd()) {
        Create(IDD_SEARCH_BAR, m_pParent);
    }

    UpdatePosition();
    ShowWindow(SW_SHOWNOACTIVATE);
    SetWindowPos(&wndTopMost, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

    // 让输入框获得焦点并选中所有文本
    m_editSearch.SetFocus();
    m_editSearch.SetSel(0, -1);
}

void CSearchBarDlg::Hide()
{
    KillTimer(TIMER_SEARCH);  // 清理定时器
    ShowWindow(SW_HIDE);
    // 将焦点还给主窗口列表
    if (m_pParent) {
        m_pParent->SetFocus();
    }
}

void CSearchBarDlg::UpdatePosition()
{
    if (!m_pParent || !GetSafeHwnd()) return;

    CRect rcParent;
    m_pParent->GetWindowRect(&rcParent);

    CRect rcThis;
    GetWindowRect(&rcThis);

    // 居中显示
    int x = rcParent.left + (rcParent.Width() - rcThis.Width()) / 2;
    int y = rcParent.top + (rcParent.Height() - rcThis.Height()) / 2;

    SetWindowPos(NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void CSearchBarDlg::OnEnChangeSearch()
{
    // 延迟搜索：重置定时器，避免每次输入都搜索
    KillTimer(TIMER_SEARCH);
    SetTimer(TIMER_SEARCH, SEARCH_DELAY_MS, NULL);
}

void CSearchBarDlg::OnTimer(UINT_PTR nIDEvent)
{
    if (nIDEvent == TIMER_SEARCH) {
        KillTimer(TIMER_SEARCH);
        DoSearch();
    }
    __super::OnTimer(nIDEvent);
}

void CSearchBarDlg::DoSearch()
{
    CString searchText;
    m_editSearch.GetWindowText(searchText);

    // 空文本时清空结果
    if (searchText.IsEmpty()) {
        m_Results.clear();
        m_nCurrentIndex = -1;
        m_strLastSearch.Empty();
        UpdateCountText();
        return;
    }

    // 相同文本不重复搜索
    if (searchText == m_strLastSearch && !m_Results.empty()) {
        return;
    }
    m_strLastSearch = searchText;

    // 大小写不敏感
    searchText.MakeLower();
    m_Results.clear();

    if (!m_pParent) return;

    // 获取当前选中项，用于确定搜索起始位置
    int nSelectedItem = -1;
    POSITION pos = m_pParent->m_CList_Online.GetFirstSelectedItemPosition();
    if (pos) {
        nSelectedItem = m_pParent->m_CList_Online.GetNextSelectedItem(pos);
    }

    // 加锁保护：访问 m_FilteredIndices, m_HostList, context
    EnterCriticalSection(&m_pParent->m_cs);

    // 遍历过滤后的列表
    int count = (int)m_pParent->m_FilteredIndices.size();
    for (int i = 0; i < count; i++) {
        context* ctx = m_pParent->GetContextByListIndex(i);
        if (!ctx) continue;

        bool matched = false;

        // 搜索各列：IP(0), 地理位置(2), 计算机名/备注(3), 操作系统(4), 版本(8)
        int cols[] = { 0, 2, 3, 4, 8 };
        for (int col : cols) {
            CString colText;

            // 备注列特殊处理
            if (col == 3) {  // ONLINELIST_COMPUTER_NAME
                colText = m_pParent->m_ClientMap->GetClientMapData(ctx->GetClientID(), MAP_NOTE);
                if (colText.IsEmpty()) {
                    colText = ctx->GetClientData(col);
                }
            } else {
                colText = ctx->GetClientData(col);
            }

            colText.MakeLower();
            if (colText.Find(searchText) >= 0) {
                matched = true;
                break;
            }
        }

        if (matched) {
            m_Results.push_back(i);
        }
    }

    LeaveCriticalSection(&m_pParent->m_cs);

    // 定位到最接近当前选中项的结果
    m_nCurrentIndex = -1;
    if (!m_Results.empty()) {
        m_nCurrentIndex = 0;
        if (nSelectedItem >= 0) {
            for (int i = 0; i < (int)m_Results.size(); i++) {
                if (m_Results[i] >= nSelectedItem) {
                    m_nCurrentIndex = i;
                    break;
                }
            }
        }
        GotoResult(m_nCurrentIndex);
    }

    UpdateCountText();
}

void CSearchBarDlg::GotoPrev()
{
    if (m_Results.empty()) return;

    m_nCurrentIndex--;
    if (m_nCurrentIndex < 0) {
        m_nCurrentIndex = (int)m_Results.size() - 1;  // 循环
    }
    GotoResult(m_nCurrentIndex);
    UpdateCountText();
}

void CSearchBarDlg::GotoNext()
{
    if (m_Results.empty()) return;

    m_nCurrentIndex++;
    if (m_nCurrentIndex >= (int)m_Results.size()) {
        m_nCurrentIndex = 0;  // 循环
    }
    GotoResult(m_nCurrentIndex);
    UpdateCountText();
}

void CSearchBarDlg::GotoResult(int index)
{
    if (index < 0 || index >= (int)m_Results.size()) return;
    if (!m_pParent) return;

    int listIndex = m_Results[index];

    // 验证索引有效性（列表可能已变化）
    int itemCount = m_pParent->m_CList_Online.GetItemCount();
    if (listIndex < 0 || listIndex >= itemCount) {
        // 索引已失效，清空搜索结果
        m_Results.clear();
        m_nCurrentIndex = -1;
        UpdateCountText();
        return;
    }

    // 取消所有选中
    int nItem = -1;
    while ((nItem = m_pParent->m_CList_Online.GetNextItem(-1, LVNI_SELECTED)) != -1) {
        m_pParent->m_CList_Online.SetItemState(nItem, 0, LVIS_SELECTED | LVIS_FOCUSED);
    }

    // 选中并滚动到目标项
    m_pParent->m_CList_Online.SetItemState(listIndex, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    m_pParent->m_CList_Online.EnsureVisible(listIndex, FALSE);
}

void CSearchBarDlg::UpdateCountText()
{
    CString text;
    if (m_Results.empty()) {
        CString searchText;
        m_editSearch.GetWindowText(searchText);
        if (!searchText.IsEmpty()) {
            text = _T("Not found");
        }
    } else {
        text.Format(_T("%d/%d"), m_nCurrentIndex + 1, (int)m_Results.size());
    }
    m_staticCount.SetWindowText(text);
    m_staticCount.Invalidate();
    m_staticCount.UpdateWindow();
}

void CSearchBarDlg::OnBnClickedPrev()
{
    GotoPrev();
}

void CSearchBarDlg::OnBnClickedNext()
{
    GotoNext();
}

void CSearchBarDlg::OnBnClickedClose()
{
    Hide();
}
