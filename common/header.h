#pragma once
// This file implements a serial of data header encoding methods.
#include <cstring>
#include <common/skCrypter.h>

#define MSG_HEADER "HELL"

enum HeaderEncType {
	HeaderEncUnknown = -1,
	HeaderEncNone,
	HeaderEncV1,
};

// 数据编码格式：标识符 + 编码后长度(4字节) + 解码后长度(4字节)
const int FLAG_COMPLEN = 4;
const int FLAG_LENGTH = 8;
const int HDR_LENGTH = FLAG_LENGTH + 2 * sizeof(unsigned int);
const int MIN_COMLEN = 8;

typedef void (*EncFun)(unsigned char* data, size_t length, unsigned char key);
typedef void (*DecFun)(unsigned char* data, size_t length, unsigned char key);
inline void default_encrypt(unsigned char* data, size_t length, unsigned char key) {
	data[FLAG_LENGTH - 2] = data[FLAG_LENGTH - 1] = 0;
}
inline void default_decrypt(unsigned char* data, size_t length, unsigned char key) {
}

// 加密函数
inline void encrypt(unsigned char* data, size_t length, unsigned char key) {
	if (key == 0) return;
	for (size_t i = 0; i < length; ++i) {
		unsigned char k = static_cast<unsigned char>(key ^ (i * 31));  // 动态扰动 key
		int value = static_cast<int>(data[i]);
		switch (i % 4) {
		case 0:
			value += k;
			break;
		case 1:
			value = value ^ k;
			break;
		case 2:
			value -= k;
			break;
		case 3:
			value = ~(value ^ k);  // 多步变换：先异或再取反
			break;
		}
		data[i] = static_cast<unsigned char>(value & 0xFF);
	}
}

// 解密函数
inline void decrypt(unsigned char* data, size_t length, unsigned char key) {
	if (key == 0) return;
	for (size_t i = 0; i < length; ++i) {
		unsigned char k = static_cast<unsigned char>(key ^ (i * 31));
		int value = static_cast<int>(data[i]);
		switch (i % 4) {
		case 0:
			value -= k;
			break;
		case 1:
			value = value ^ k;
			break;
		case 2:
			value += k;
			break;
		case 3:
			value = ~(value) ^ k;  // 解开：先取反，再异或
			break;
		}
		data[i] = static_cast<unsigned char>(value & 0xFF);
	}
}

inline EncFun GetHeaderEncoder(HeaderEncType type) {
	switch (type)
	{
	case HeaderEncNone:
		return default_encrypt;
	case HeaderEncV1:
		return encrypt;
	default:
		return NULL;
	}
}

typedef struct HeaderFlag {
	char Data[FLAG_LENGTH + 1];
	HeaderFlag(const char header[FLAG_LENGTH + 1]) {
		memcpy(Data, header, sizeof(Data));
	}
	char& operator[](int i) {
		return Data[i];
	}
	const char operator[](int i) const {
		return Data[i];
	}
	const char* data() const {
		return Data;
	}
}HeaderFlag;

// 写入数据包的头
inline HeaderFlag GetHead(EncFun enc) {
	char header[FLAG_LENGTH + 1] = { 'H','E','L','L', 0 };
	HeaderFlag H(header);
	unsigned char key = time(0) % 256;
	H[FLAG_LENGTH - 2] = key;
	H[FLAG_LENGTH - 1] = ~key;
	enc((unsigned char*)H.data(), FLAG_COMPLEN, H[FLAG_LENGTH - 2]);
	return H;
}

enum FlagType {
	FLAG_UNKNOWN = 0,
	FLAG_SHINE = 1,
	FLAG_FUCK = 2,
	FLAG_HELLO = 3,
	FLAG_HELL = 4,
};

inline int compare(const char *flag, const char *magic, int len, DecFun dec, unsigned char key){
	unsigned char buf[32] = {};
	memcpy(buf, flag, MIN_COMLEN);
	dec(buf, len, key);
	if (memcmp(buf, magic, len) == 0) {
		memcpy((void*)flag, buf, MIN_COMLEN);
		return 0;
	}
	return -1;
}

// 比对数据包前几个字节
// 会用指定的解密函数先对数据包头进行解密，再来进行比对
inline FlagType CheckHead(const char* flag, DecFun dec) {
	FlagType type = FLAG_UNKNOWN;
	if (compare(flag, skCrypt(MSG_HEADER), FLAG_COMPLEN, dec, flag[6]) == 0) {
		type = FLAG_HELL;
	}
	else if (compare(flag, skCrypt("Shine"), 5, dec, 0) == 0) {
		type = FLAG_SHINE;
	}
	else if (compare(flag, skCrypt("<<FUCK>>"), 8, dec, 0) == 0) {
		type = FLAG_FUCK;
	}
	else if (compare(flag, skCrypt("Hello?"), 6, dec, flag[6]) == 0) {
		type = FLAG_HELLO;
	}
	else {
		type = FLAG_UNKNOWN;
	}
	return type;
}

// 解密需要尝试多种方法，以便能兼容老版本通讯协议
inline FlagType CheckHead(char* flag, HeaderEncType& funcHit) {
	static const DecFun methods[] = { default_decrypt, decrypt };
	static const int methodNum = sizeof(methods) / sizeof(DecFun);
	char buffer[FLAG_LENGTH + 1] = {};
	for (int i = 0; i < methodNum; ++i) {
		memcpy(buffer, flag, FLAG_LENGTH);
		FlagType type = CheckHead(buffer, methods[i]);
		if (type != FLAG_UNKNOWN) {
			memcpy(flag, buffer, FLAG_LENGTH);
			funcHit = HeaderEncType(i);
			return type;
		}
	}
	funcHit = HeaderEncUnknown;
	return FLAG_UNKNOWN;
}
