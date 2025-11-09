#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include "aes.h"
#pragma once

class ObfsBase
{
public:
    bool m_bGenCArray;
    ObfsBase(bool genCArray = true) : m_bGenCArray(genCArray) { }
    virtual ~ObfsBase() { }

    // 对称混淆函数：用于加密和解密
    virtual void ObfuscateBuffer(uint8_t* buf, size_t len, uint32_t seed) {}

    // 解混淆：与加密顺序相反
    virtual void DeobfuscateBuffer(uint8_t* buf, size_t len, uint32_t seed) {}

    virtual bool WriteFile(const char* filename, uint8_t* data, size_t length, const char* arrayName)
    {
        return m_bGenCArray ? WriteBinaryAsCArray(filename, data, length, arrayName) : WriteBinaryFile(filename, data, length);
    }

    // 将二进制数据以 C 数组格式写入文件
    virtual bool WriteBinaryAsCArray(const char* filename, uint8_t* data, size_t length, const char* arrayName)
    {
        FILE* file = fopen(filename, "w");
        if (!file) return false;

        fprintf(file, "unsigned char %s[] = {\n", arrayName);
        for (size_t i = 0; i < length; ++i) {
            if (i % 24 == 0) fprintf(file, "    ");
            fprintf(file, "0x%02X", data[i]);
            if (i != length - 1) fprintf(file, ",");
            if ((i + 1) % 24 == 0 || i == length - 1) fprintf(file, "\n");
            else fprintf(file, " ");
        }
        fprintf(file, "};\n");
        fprintf(file, "unsigned int %s_len = %zu;\n", arrayName, length);

        fclose(file);
        return true;
    }

    // 使用 "wb" 二进制写入模式
    virtual bool WriteBinaryFile(const char* filename, const uint8_t* data, size_t length)
    {
        FILE* file = fopen(filename, "wb");
        if (!file) return false;

        size_t written = fwrite(data, 1, length, file);
        fclose(file);

        return written == length;
    }
};

class Obfs : public ObfsBase
{
private:
    // 左旋8位整数
    static inline uint8_t rol8(uint8_t val, int shift)
    {
        return (val << shift) | (val >> (8 - shift));
    }

    // 右旋8位整数
    static inline uint8_t ror8(uint8_t val, int shift)
    {
        return (val >> shift) | (val << (8 - shift));
    }

public:
    Obfs(bool genCArray = true) : ObfsBase(genCArray) { }

    // 对称混淆函数：用于加密和解密
    virtual void ObfuscateBuffer(uint8_t* buf, size_t len, uint32_t seed)
    {
        uint32_t state = seed;

        for (size_t i = 0; i < len; ++i) {
            uint8_t mask = (uint8_t)((state >> 16) & 0xFF);
            buf[i] = rol8(buf[i] ^ mask, 3);  // 异或+旋转扰乱特征
            state = state * 2654435761u + buf[i]; // LCG + 数据扰动
        }
    }

    // 解混淆：与加密顺序相反
    virtual void DeobfuscateBuffer(uint8_t* buf, size_t len, uint32_t seed)
    {
        uint32_t state = seed;

        for (size_t i = 0; i < len; ++i) {
            uint8_t mask = (uint8_t)((state >> 16) & 0xFF);
            uint8_t orig = buf[i];
            buf[i] = ror8(buf[i], 3) ^ mask;
            state = state * 2654435761u + orig; // 必须用混淆前的原字节更新 state
        }
    }
};

class ObfsAes : public ObfsBase {
private:
    // Please change `aes_key` and `aes_iv`.
	unsigned char aes_key[16] = "It is a example";
	unsigned char aes_iv[16] = "It is a example";

public:
    ObfsAes(bool genCArray = true) : ObfsBase(genCArray) { }

    virtual void ObfuscateBuffer(uint8_t* buf, size_t len, uint32_t seed) {
		struct AES_ctx ctx;
		AES_init_ctx_iv(&ctx, aes_key, aes_iv);
		AES_CBC_encrypt_buffer(&ctx, buf, len);
    }

    virtual void DeobfuscateBuffer(uint8_t* buf, size_t len, uint32_t seed) {
		struct AES_ctx ctx;
		AES_init_ctx_iv(&ctx, aes_key, aes_iv);
		AES_CBC_decrypt_buffer(&ctx, buf, len);
    }
};
