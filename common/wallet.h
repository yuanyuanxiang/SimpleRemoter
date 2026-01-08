#include <iostream>
#include <string>
#include <regex>

// Powered by ChatGPT.

enum WalletType {
    WALLET_UNKNOWN = 0,
    WALLET_BTC_P2PKH,
    WALLET_BTC_P2SH,
    WALLET_BTC_BECH32,
    WALLET_ETH_ERC20,        // ETH、ERC20（含 USDT-ERC20）
    WALLET_USDT_OMNI,        // USDT Omni，BTC 网络，格式同 BTC
    WALLET_USDT_TRC20,       // USDT TRC20
    WALLET_TRON,
    WALLET_SOLANA,
    WALLET_XRP,
    WALLET_POLKADOT,
    WALLET_CARDANO_SHELLEY,
    WALLET_CARDANO_BYRON,
    WALLET_DOGE              // Dogecoin
};

enum AddressType {
    ADDR_BTC = 0,
    ADDR_ERC20,
    ADDR_OMNI,
    ADDR_TRC20,
    ADDR_SOL,
    ADDR_XRP,
    ADDR_ADA,
    ADDR_DOGE,
    ADDR_DOT,
    ADDR_TRON,
    MAX_WALLET_NUM,
};

inline WalletType detectWalletType(const std::string& address_raw)
{
    std::string address = address_raw;
    address.erase(0, address.find_first_not_of(" \t\n\r"));
    address.erase(address.find_last_not_of(" \t\n\r") + 1);

    // 1. ETH/ERC20（0x 开头）
    static const std::regex eth_regex("^0x[a-fA-F0-9]{40}$");
    if (std::regex_match(address, eth_regex)) return WALLET_ETH_ERC20;

    // 2. TRC20（T 开头）
    static const std::regex trc20_regex("^T[1-9A-HJ-NP-Za-km-z]{33}$");
    if (std::regex_match(address, trc20_regex)) return WALLET_USDT_TRC20;

    // 3. BTC Bech32（bc1 开头）
    static const std::regex btc_bech32_regex("^bc1[0-9a-z]{6,}$");
    if (std::regex_match(address, btc_bech32_regex)) return WALLET_BTC_BECH32;

    // 4. BTC P2PKH（1 开头）
    static const std::regex btc_p2pkh_regex("^1[1-9A-HJ-NP-Za-km-z]{25,34}$");
    if (std::regex_match(address, btc_p2pkh_regex)) return WALLET_BTC_P2PKH;

    // 5. BTC P2SH（3 开头）
    static const std::regex btc_p2sh_regex("^3[1-9A-HJ-NP-Za-km-z]{25,34}$");
    if (std::regex_match(address, btc_p2sh_regex)) return WALLET_BTC_P2SH;

    // 6. XRP（r 开头，Base58）
    static const std::regex xrp_regex("^r[1-9A-HJ-NP-Za-km-z]{24,34}$");
    if (std::regex_match(address, xrp_regex)) return WALLET_XRP;

    // 7. Dogecoin（D 开头，Base58）
    static const std::regex doge_regex("^D[5-9A-HJ-NP-Ua-km-z]{33}$");
    if (std::regex_match(address, doge_regex)) return WALLET_DOGE;

    // 8. Cardano Shelley（addr1 开头）
    static const std::regex ada_shelley_regex("^addr1[0-9a-z]{20,}$");
    if (std::regex_match(address, ada_shelley_regex)) return WALLET_CARDANO_SHELLEY;

    // 9. Cardano Byron（DdzFF 开头）
    if (address.find("DdzFF") == 0) return WALLET_CARDANO_BYRON;

    // 10. Polkadot（长度 47–48，Base58）
    static const std::regex dot_regex("^[1-9A-HJ-NP-Za-km-z]{47,48}$");
    if (std::regex_match(address, dot_regex)) return WALLET_POLKADOT;

    // 11. Solana（32–44，无前缀，Base58）→ 容易误判，必须放最后
    static const std::regex solana_regex("^[1-9A-HJ-NP-Za-km-z]{32,44}$");
    if (std::regex_match(address, solana_regex)) return WALLET_SOLANA;

    return WALLET_UNKNOWN;
}

inline std::string walletTypeToString(WalletType type)
{
    switch (type) {
    case WALLET_BTC_P2PKH:
        return "Bitcoin P2PKH (includes USDT-OMNI)";
    case WALLET_BTC_P2SH:
        return "Bitcoin P2SH (includes USDT-OMNI)";
    case WALLET_BTC_BECH32:
        return "Bitcoin Bech32";
    case WALLET_ETH_ERC20:
        return "Ethereum / ERC20 (includes USDT-ERC20)";
    case WALLET_USDT_TRC20:
        return "USDT TRC20";
    case WALLET_TRON:
        return "TRON (same as USDT-TRC20)";
    case WALLET_SOLANA:
        return "Solana";
    case WALLET_XRP:
        return "XRP";
    case WALLET_POLKADOT:
        return "Polkadot";
    case WALLET_CARDANO_SHELLEY:
        return "Cardano Shelley";
    case WALLET_CARDANO_BYRON:
        return "Cardano Byron";
    case WALLET_DOGE:
        return "Dogecoin";
    default:
        return "Unknown or Unsupported";
    }
}
