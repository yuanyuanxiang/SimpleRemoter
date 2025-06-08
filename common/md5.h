#pragma once

#include <wincrypt.h>

inline std::string CalcMD5FromBytes(const BYTE* data, DWORD length) {
	HCRYPTPROV hProv = 0;
	HCRYPTHASH hHash = 0;
	BYTE hash[16]; // MD5 输出长度是 16 字节
	DWORD hashLen = sizeof(hash);
	std::ostringstream oss;

	if (!CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
		return "";
	}

	if (!CryptCreateHash(hProv, CALG_MD5, 0, 0, &hHash)) {
		CryptReleaseContext(hProv, 0);
		return "";
	}

	if (!CryptHashData(hHash, data, length, 0)) {
		CryptDestroyHash(hHash);
		CryptReleaseContext(hProv, 0);
		return "";
	}

	if (!CryptGetHashParam(hHash, HP_HASHVAL, hash, &hashLen, 0)) {
		CryptDestroyHash(hHash);
		CryptReleaseContext(hProv, 0);
		return "";
	}

	// 转换为十六进制字符串
	for (DWORD i = 0; i < hashLen; ++i) {
		oss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
	}

	CryptDestroyHash(hHash);
	CryptReleaseContext(hProv, 0);

	return oss.str();
}
