#pragma once
// This file implements a serial of data header encoding methods.
#include <cstring>
#include <common/skCrypter.h>
#include "common/encfuncs.h"

#define MSG_HEADER "HELL"

enum HeaderEncType {
    HeaderEncUnknown = -1,
    HeaderEncNone,
    HeaderEncV0,
    HeaderEncV1,
    HeaderEncV2,
    HeaderEncV3,
    HeaderEncV4,
    HeaderEncV5,
    HeaderEncV6,
    HeaderEncNum,
};

// ���ݱ����ʽ����ʶ�� + ����󳤶�(4�ֽ�) + ����󳤶�(4�ֽ�)
const int FLAG_COMPLEN = 4;
const int FLAG_LENGTH = 8;
const int HDR_LENGTH = FLAG_LENGTH + 2 * sizeof(unsigned int);
const int MIN_COMLEN = 12;

typedef void (*EncFun)(unsigned char* data, size_t length, unsigned char key);
typedef void (*DecFun)(unsigned char* data, size_t length, unsigned char key);
inline void default_encrypt(unsigned char* data, size_t length, unsigned char key)
{
    data[FLAG_LENGTH - 2] = data[FLAG_LENGTH - 1] = 0;
}
inline void default_decrypt(unsigned char* data, size_t length, unsigned char key)
{
}

// ���ܺ���
inline void encrypt(unsigned char* data, size_t length, unsigned char key)
{
    if (key == 0) return;
    for (size_t i = 0; i < length; ++i) {
        unsigned char k = static_cast<unsigned char>(key ^ (i * 31));  // ��̬�Ŷ� key
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
            value = ~(value ^ k);  // �ಽ�任���������ȡ��
            break;
        }
        data[i] = static_cast<unsigned char>(value & 0xFF);
    }
}

// ���ܺ���
inline void decrypt(unsigned char* data, size_t length, unsigned char key)
{
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
            value = ~(value) ^ k;  // �⿪����ȡ���������
            break;
        }
        data[i] = static_cast<unsigned char>(value & 0xFF);
    }
}

inline EncFun GetHeaderEncoder(HeaderEncType type)
{
    static const DecFun methods[] = { default_encrypt, encrypt, encrypt_v1, encrypt_v2, encrypt_v3, encrypt_v4, encrypt_v5, encrypt_v6 };
    return methods[type];
}

typedef struct HeaderFlag {
    char Data[FLAG_LENGTH + 1];
    HeaderFlag(const char header[FLAG_LENGTH + 1])
    {
        memcpy(Data, header, sizeof(Data));
    }
    char& operator[](int i)
    {
        return Data[i];
    }
    const char operator[](int i) const
    {
        return Data[i];
    }
    const char* data() const
    {
        return Data;
    }
} HeaderFlag;

// д�����ݰ���ͷ
inline HeaderFlag GetHead(EncFun enc)
{
    char header[FLAG_LENGTH + 1] = { 'H','E','L','L', 0 };
    HeaderFlag H(header);
    unsigned char key = time(0) % 256;
    H[FLAG_LENGTH - 2] = key;
    H[FLAG_LENGTH - 1] = ~key;
    enc((unsigned char*)H.data(), FLAG_COMPLEN, H[FLAG_LENGTH - 2]);
    return H;
}

enum FlagType {
    FLAG_WINOS = -1,
    FLAG_UNKNOWN = 0,
    FLAG_SHINE = 1,
    FLAG_FUCK = 2,
    FLAG_HELLO = 3,
    FLAG_HELL = 4,
};

inline int compare(const char *flag, const char *magic, int len, DecFun dec, unsigned char key)
{
    unsigned char buf[32] = {};
    memcpy(buf, flag, MIN_COMLEN);
    dec(buf, len, key);
    if (memcmp(buf, magic, len) == 0) {
        memcpy((void*)flag, buf, MIN_COMLEN);
        return 0;
    }
    return -1;
}

// �ȶ����ݰ�ǰ�����ֽ�
// ����ָ���Ľ��ܺ����ȶ����ݰ�ͷ���н��ܣ��������бȶ�
inline FlagType CheckHead(const char* flag, DecFun dec)
{
    FlagType type = FLAG_UNKNOWN;
    if (compare(flag, skCrypt(MSG_HEADER), FLAG_COMPLEN, dec, flag[6]) == 0) {
        type = FLAG_HELL;
    } else if (compare(flag, skCrypt("Shine"), 5, dec, 0) == 0) {
        type = FLAG_SHINE;
    } else if (compare(flag, skCrypt("<<FUCK>>"), 8, dec, flag[9]) == 0) {
        type = FLAG_FUCK;
    } else if (compare(flag, skCrypt("Hello?"), 6, dec, flag[6]) == 0) {
        type = FLAG_HELLO;
    } else {
        type = FLAG_UNKNOWN;
    }
    return type;
}

// ������Ҫ���Զ��ַ������Ա��ܼ����ϰ汾ͨѶЭ��
inline FlagType CheckHead(char* flag, HeaderEncType& funcHit)
{
    static const DecFun methods[] = { default_decrypt, decrypt, decrypt_v1, decrypt_v2, decrypt_v3, decrypt_v4, decrypt_v5, decrypt_v6 };
    static const int methodNum = sizeof(methods) / sizeof(DecFun);
    char buffer[MIN_COMLEN + 4] = {};
    for (int i = 0; i < methodNum; ++i) {
        memcpy(buffer, flag, MIN_COMLEN);
        FlagType type = CheckHead(buffer, methods[i]);
        if (type != FLAG_UNKNOWN) {
            memcpy(flag, buffer, MIN_COMLEN);
            funcHit = HeaderEncType(i);
            return type;
        }
    }
    funcHit = HeaderEncUnknown;
    return FLAG_UNKNOWN;
}
