#include "stdafx.h"
#include "afxdialogex.h"
#include "CGridDialog.h"
#include "Resource.h"

BEGIN_MESSAGE_MAP(CGridDialog, CDialog)
    ON_WM_SIZE()
    ON_WM_LBUTTONDBLCLK()
    ON_MESSAGE(WM_CHILD_CLOSED, &CGridDialog::OnChildClosed)
END_MESSAGE_MAP()

CGridDialog::CGridDialog() : CDialog()
{
}

BOOL CGridDialog::OnInitDialog()
{
    m_hIcon = ::LoadIconA(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDR_MAINFRAME));
    SetIcon(m_hIcon, FALSE);

    CDialog::OnInitDialog();
    return TRUE;
}

void CGridDialog::SetGrid(int rows, int cols)
{
    m_rows = rows;
    m_cols = cols;
    m_max = rows * cols;
    LayoutChildren();
}

BOOL CGridDialog::AddChild(CDialog* pDlg)
{
    if (!pDlg || !::IsWindow(pDlg->GetSafeHwnd()) || m_children.size() >= m_max)
        return FALSE;

    pDlg->SetParent(this);
    pDlg->ShowWindow(SW_SHOW);

    // ȥ���������͵�����С
    LONG style = ::GetWindowLong(pDlg->GetSafeHwnd(), GWL_STYLE);
    style &= ~(WS_CAPTION | WS_THICKFRAME | WS_SIZEBOX | WS_BORDER);
    ::SetWindowLong(pDlg->GetSafeHwnd(), GWL_STYLE, style);
    ::SetWindowPos(pDlg->GetSafeHwnd(), nullptr, 0, 0, 0, 0, SWP_NOZORDER | SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED);

    m_children.push_back(pDlg);
    LayoutChildren();
    return TRUE;
}

void CGridDialog::RemoveChild(CDialog* pDlg)
{
    auto it = std::find(m_children.begin(), m_children.end(), pDlg);
    if (it != m_children.end()) {
        (*it)->ShowWindow(SW_HIDE);
        (*it)->SetParent(nullptr);
        m_children.erase(it);

        // ɾ�� m_origState �ж�Ӧ��Ŀ
        auto itState = m_origState.find(pDlg);
        if (itState != m_origState.end())
            m_origState.erase(itState);

        // ����رյ��Ӵ����ǵ�ǰ��󻯴��ڣ����� m_pMaxChild
        if (m_pMaxChild == pDlg)
            m_pMaxChild = nullptr;

        LayoutChildren();
    }
}

LRESULT CGridDialog::OnChildClosed(WPARAM wParam, LPARAM lParam)
{
    CDialog* pDlg = (CDialog*)wParam;
    RemoveChild(pDlg);
    return 0;
}

void CGridDialog::LayoutChildren()
{
    if (m_children.size() == 0) {
        // �ָ������ڱ�����
        if (m_parentStyle != 0) {
            ::SetWindowLong(m_hWnd, GWL_STYLE, m_parentStyle);
            ::SetWindowPos(m_hWnd, nullptr, 0, 0, 0, 0,
                           SWP_NOZORDER | SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED);
            m_parentStyle = 0;
        }
        ShowWindow(SW_HIDE);
        return;
    }

    if (m_rows <= 0 || m_cols <= 0 || m_children.empty() || m_pMaxChild != nullptr)
        return;

    CRect rcClient;
    GetClientRect(&rcClient);

    int w = rcClient.Width() / m_cols;
    int h = rcClient.Height() / m_rows;

    for (size_t i = 0; i < m_children.size(); ++i) {
        int r = (int)i / m_cols;
        int c = (int)i % m_cols;

        if (r >= m_rows)
            break; // ��������Χ

        int x = c * w;
        int y = r * h;

        m_children[i]->MoveWindow(x, y, w, h, TRUE);
        m_children[i]->ShowWindow(SW_SHOW);
    }
}

void CGridDialog::OnSize(UINT nType, int cx, int cy)
{
    CDialog::OnSize(nType, cx, cy);

    if (m_pMaxChild == nullptr) {
        LayoutChildren();
    } else {
        // ���״̬�£������������Ի���
        CRect rcClient;
        GetClientRect(&rcClient);
        m_pMaxChild->MoveWindow(rcClient, TRUE);
    }
}

void CGridDialog::OnLButtonDblClk(UINT nFlags, CPoint point)
{
    // �����ǰ����󻯵��Ӵ��ڣ�˫���κεط����ָ�
    if (m_pMaxChild != nullptr) {
        // �ָ��Ӵ�����ʽ��λ��
        for (auto& kv : m_origState) {
            CDialog* dlg = kv.first;
            const ChildState& state = kv.second;

            ::SetWindowLong(dlg->GetSafeHwnd(), GWL_STYLE, state.style);
            ::SetWindowPos(dlg->GetSafeHwnd(), nullptr, 0, 0, 0, 0,
                           SWP_NOZORDER | SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED);

            dlg->MoveWindow(state.rect, TRUE);
            dlg->ShowWindow(SW_SHOW);
        }

        // �ָ������ڱ�����
        if (m_parentStyle != 0) {
            ::SetWindowLong(m_hWnd, GWL_STYLE, m_parentStyle);
            ::SetWindowPos(m_hWnd, nullptr, 0, 0, 0, 0,
                           SWP_NOZORDER | SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED);
            m_parentStyle = 0;
        }

        // ˢ�¸�����
        m_pMaxChild = nullptr;
        m_origState.clear();
        LayoutChildren();
        return; // �Ѵ�������
    }

    // û������Ӵ��ڣ���ԭ�߼��ҵ�������Ӵ��ڽ������
    for (auto dlg : m_children) {
        CRect rc;
        dlg->GetWindowRect(&rc);
        ScreenToClient(&rc);

        if (rc.PtInRect(point)) {
            // ���������Ӵ���ԭʼ״̬
            m_origState.clear();
            for (auto d : m_children) {
                ChildState state;
                d->GetWindowRect(&state.rect);
                ScreenToClient(&state.rect);
                state.style = ::GetWindowLong(d->GetSafeHwnd(), GWL_STYLE);
                m_origState[d] = state;
            }

            // ��󻯵�����Ӵ���
            LONG style = m_origState[dlg].style;
            style |= (WS_CAPTION | WS_THICKFRAME | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX);
            ::SetWindowLong(dlg->GetSafeHwnd(), GWL_STYLE, style);
            ::SetWindowPos(dlg->GetSafeHwnd(), nullptr, 0, 0, 0, 0,
                           SWP_NOZORDER | SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED);

            // ���ظ����ڱ�����
            if (m_parentStyle == 0)
                m_parentStyle = ::GetWindowLong(m_hWnd, GWL_STYLE);
            LONG parentStyle = m_parentStyle & ~(WS_CAPTION | WS_THICKFRAME);
            ::SetWindowLong(m_hWnd, GWL_STYLE, parentStyle);
            ::SetWindowPos(m_hWnd, nullptr, 0, 0, 0, 0,
                           SWP_NOZORDER | SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED);

            // ȫ����ʾ�Ӵ���
            CRect rcClient;
            GetClientRect(&rcClient);
            dlg->MoveWindow(rcClient, TRUE);
            m_pMaxChild = dlg;

            // ���������Ӵ���
            for (auto d : m_children)
                if (d != dlg) d->ShowWindow(SW_HIDE);

            break;
        }
    }

    CDialog::OnLButtonDblClk(nFlags, point);
}

BOOL CGridDialog::PreTranslateMessage(MSG* pMsg)
{
    if (pMsg->wParam == VK_RETURN || pMsg->wParam == VK_ESCAPE) {
        return TRUE;// ����Enter��ESC�رնԻ�
    }
    return CDialog::PreTranslateMessage(pMsg);
}
