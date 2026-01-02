#pragma once
// This file implements a serial of data encoding methods.
#include <vector>
extern "C" {
#include "aes.h"
}

#define ALIGN16(n)  ( (( (n) + 15) / 16) * 16 )

// Encoder interface. The default encoder will do nothing.
class Encoder
{
public:
    virtual ~Encoder() {}
    // Encode data before compress.
    virtual void Encode(unsigned char* data, int len, unsigned char* param = 0) {}
    // Decode data after uncompress.
    virtual void Decode(unsigned char* data, int len, unsigned char* param = 0) {}
};

// XOR Encoder implementation.
class XOREncoder : public Encoder
{
private:
    std::vector<char> Keys;

public:
    XOREncoder(const std::vector<char>& keys = { 0 }) : Keys(keys) {}

    virtual void Encode(unsigned char* data, int len, unsigned char* param = 0)
    {
        XOR(data, len, Keys);
    }

    virtual void Decode(unsigned char* data, int len, unsigned char* param = 0)
    {
        static std::vector<char> reversed(Keys.rbegin(), Keys.rend());
        XOR(data, len, reversed);
    }

protected:
    void XOR(unsigned char* data, int len, const std::vector<char>& keys) const
    {
        for (char key : keys) {
            for (int i = 0; i < len; ++i) {
                data[i] ^= key;
            }
        }
    }
};

// XOREncoder16 A simple Encoder for the TCP body. It's using for `HELL` protocol.
// This method is provided by ChatGPT. Encode data according to the 6th and 7th elem.
class XOREncoder16 : public Encoder
{
private:
    static uint16_t pseudo_random(uint16_t seed, int index)
    {
        return ((seed ^ (index * 251 + 97)) * 733) ^ (seed >> 3);
    }

    void encrypt_internal(unsigned char* data, int len, unsigned char k1, unsigned char k2) const
    {
        uint16_t key = ((k1 << 8) | k2);
        for (int i = 0; i < len; ++i) {
            data[i] ^= (k1 + i * 13) ^ (k2 ^ (i << 1));
        }

        // Two rounds of pseudo-random swaps
        for (int round = 0; round < 2; ++round) {
            for (int i = 0; i < len; ++i) {
                int j = pseudo_random(key, i + round * 100) % len;
                std::swap(data[i], data[j]);
            }
        }
    }

    void decrypt_internal(unsigned char* data, int len, unsigned char k1, unsigned char k2) const
    {
        uint16_t key = ((k1 << 8) | k2);
        for (int round = 1; round >= 0; --round) {
            for (int i = len - 1; i >= 0; --i) {
                int j = pseudo_random(key, i + round * 100) % len;
                std::swap(data[i], data[j]);
            }
        }

        for (int i = 0; i < len; ++i) {
            data[i] ^= (k1 + i * 13) ^ (k2 ^ (i << 1));
        }
    }

#ifndef NO_AES
    void aes_encrypt(unsigned char* data, int len, const unsigned char* key, const unsigned char* iv)
    {
        if (!data || !key || !iv || len <= 0 || len % 16 != 0) {
            return; // AES CBC requires data length to be multiple of 16
        }

        struct AES_ctx ctx;
        AES_init_ctx_iv(&ctx, key, iv);
        AES_CBC_encrypt_buffer(&ctx, data, len);
    }

    void aes_decrypt(unsigned char* data, int len, const unsigned char* key, const unsigned char* iv)
    {
        if (!data || !key || !iv || len <= 0 || len % 16 != 0)
            return;

        struct AES_ctx ctx;
        AES_init_ctx_iv(&ctx, key, iv);
        AES_CBC_decrypt_buffer(&ctx, data, len);
    }
#endif

public:
    XOREncoder16() {}

    void Encode(unsigned char* data, int len, unsigned char* param) override
    {
        if (param[6] == 0 && param[7] == 0) return;
        if (param[7] == 1) {
#ifndef NO_AES
            static const unsigned char aes_key[16] = {
                0x5A, 0xC3, 0x17, 0xF0,	0x89, 0xB6, 0x4E, 0x7D,	0x1A, 0x22, 0x9F, 0xC8,	0xD3, 0xE6, 0x73, 0xB1
            };
            return aes_encrypt(data, len, aes_key, param + 8);
#endif
        }
        encrypt_internal(data, len, param[6], param[7]);
    }

    void Decode(unsigned char* data, int len, unsigned char* param) override
    {
        if (param[6] == 0 && param[7] == 0) return;
        decrypt_internal(data, len, param[6], param[7]);
    }
};

class WinOsEncoder : public Encoder
{
public:
    virtual ~WinOsEncoder() {}
    // Encode data before compress.
    virtual void Encode(unsigned char* data, int len, unsigned char* param = 0)
    {
        return XOR(data, len, param);
    }
    // Decode data after uncompress.
    virtual void Decode(unsigned char* data, int len, unsigned char* param = 0)
    {
        return XOR(data, len, param);
    }
private:
    void XOR(unsigned char* data, int len, unsigned char* password)
    {
        for (int i = 0, j = 0; i < len; i++) {
            ((char*)data)[i] ^= (password[j++]) % 456 + 54;
            if (i % (10) == 0)
                j = 0;
        }
    }
};
