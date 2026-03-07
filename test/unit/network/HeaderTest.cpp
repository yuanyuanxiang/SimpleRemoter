/**
 * @file HeaderTest.cpp
 * @brief 协议头验证和加密测试
 *
 * 测试覆盖：
 * - 协议头格式和常量
 * - 加密/解密函数正确性
 * - 多版本头部识别
 * - 头部生成和验证往返
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdint>
#include <vector>
#include <array>

// ============================================
// 协议常量定义（测试专用副本）
// ============================================

#define MSG_HEADER "HELL"
const int FLAG_COMPLEN = 4;
const int FLAG_LENGTH = 8;
const int HDR_LENGTH = FLAG_LENGTH + 2 * sizeof(unsigned int);  // 16
const int MIN_COMLEN = 12;

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

enum FlagType {
    FLAG_WINOS = -1,
    FLAG_UNKNOWN = 0,
    FLAG_SHINE = 1,
    FLAG_FUCK = 2,
    FLAG_HELLO = 3,
    FLAG_HELL = 4,
};

// ============================================
// 加密/解密函数（测试专用副本）
// ============================================

inline void default_encrypt(unsigned char* data, size_t length, unsigned char key)
{
    data[FLAG_LENGTH - 2] = data[FLAG_LENGTH - 1] = 0;
}

inline void default_decrypt(unsigned char* data, size_t length, unsigned char key)
{
}

inline void encrypt(unsigned char* data, size_t length, unsigned char key)
{
    if (key == 0) return;
    for (size_t i = 0; i < length; ++i) {
        unsigned char k = static_cast<unsigned char>(key ^ (i * 31));
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
            value = ~(value ^ k);
            break;
        }
        data[i] = static_cast<unsigned char>(value & 0xFF);
    }
}

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
            value = ~(value) ^ k;
            break;
        }
        data[i] = static_cast<unsigned char>(value & 0xFF);
    }
}

// ============================================
// HeaderFlag 结构体
// ============================================

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

typedef void (*EncFun)(unsigned char* data, size_t length, unsigned char key);
typedef void (*DecFun)(unsigned char* data, size_t length, unsigned char key);

// ============================================
// 头部生成函数
// ============================================

inline HeaderFlag GetHead(EncFun enc, unsigned char fixedKey = 0)
{
    char header[FLAG_LENGTH + 1] = { 'H','E','L','L', 0 };
    HeaderFlag H(header);
    unsigned char key = (fixedKey != 0) ? fixedKey : (time(0) % 256);
    H[FLAG_LENGTH - 2] = key;
    H[FLAG_LENGTH - 1] = ~key;
    enc((unsigned char*)H.data(), FLAG_COMPLEN, H[FLAG_LENGTH - 2]);
    return H;
}

// ============================================
// 头部验证函数
// ============================================

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

inline FlagType CheckHead(const char* flag, DecFun dec)
{
    FlagType type = FLAG_UNKNOWN;
    if (compare(flag, MSG_HEADER, FLAG_COMPLEN, dec, flag[6]) == 0) {
        type = FLAG_HELL;
    } else if (compare(flag, "Shine", 5, dec, 0) == 0) {
        type = FLAG_SHINE;
    } else if (compare(flag, "<<FUCK>>", 8, dec, flag[9]) == 0) {
        type = FLAG_FUCK;
    } else if (compare(flag, "Hello?", 6, dec, flag[6]) == 0) {
        type = FLAG_HELLO;
    } else {
        type = FLAG_UNKNOWN;
    }
    return type;
}

inline FlagType CheckHeadMulti(char* flag, HeaderEncType& funcHit)
{
    static const DecFun methods[] = { default_decrypt, decrypt };
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

// ============================================
// 协议常量测试
// ============================================

class HeaderConstantsTest : public ::testing::Test {};

TEST_F(HeaderConstantsTest, FlagLength) {
    EXPECT_EQ(FLAG_LENGTH, 8);
}

TEST_F(HeaderConstantsTest, FlagCompLen) {
    EXPECT_EQ(FLAG_COMPLEN, 4);
}

TEST_F(HeaderConstantsTest, HdrLength) {
    // FLAG_LENGTH(8) + 2 * sizeof(uint32_t)(8) = 16
    EXPECT_EQ(HDR_LENGTH, 16);
}

TEST_F(HeaderConstantsTest, MinComLen) {
    EXPECT_EQ(MIN_COMLEN, 12);
}

TEST_F(HeaderConstantsTest, MsgHeader) {
    EXPECT_STREQ(MSG_HEADER, "HELL");
    EXPECT_EQ(strlen(MSG_HEADER), 4u);
}

// ============================================
// 加密/解密测试
// ============================================

class EncryptionTest : public ::testing::Test {};

TEST_F(EncryptionTest, ZeroKeyNoOp) {
    unsigned char data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    unsigned char original[] = {0x01, 0x02, 0x03, 0x04, 0x05};

    encrypt(data, 5, 0);
    EXPECT_EQ(memcmp(data, original, 5), 0);

    decrypt(data, 5, 0);
    EXPECT_EQ(memcmp(data, original, 5), 0);
}

TEST_F(EncryptionTest, EncryptDecryptRoundTrip) {
    unsigned char original[] = {0x48, 0x45, 0x4C, 0x4C};  // "HELL"
    unsigned char data[4];
    memcpy(data, original, 4);

    unsigned char key = 0x42;

    encrypt(data, 4, key);
    // 加密后应该不同
    EXPECT_NE(memcmp(data, original, 4), 0);

    decrypt(data, 4, key);
    // 解密后应该恢复
    EXPECT_EQ(memcmp(data, original, 4), 0);
}

TEST_F(EncryptionTest, DifferentKeysProduceDifferentResults) {
    unsigned char data1[] = {0x48, 0x45, 0x4C, 0x4C};
    unsigned char data2[] = {0x48, 0x45, 0x4C, 0x4C};

    encrypt(data1, 4, 0x10);
    encrypt(data2, 4, 0x20);

    EXPECT_NE(memcmp(data1, data2, 4), 0);
}

TEST_F(EncryptionTest, PositionDependentEncryption) {
    // 相同值在不同位置加密结果不同
    unsigned char data[] = {0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA};
    unsigned char original[] = {0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA};

    encrypt(data, 8, 0x55);

    // 验证加密后值不全相同（位置相关加密）
    bool allSame = true;
    for (int i = 1; i < 8; ++i) {
        if (data[i] != data[0]) {
            allSame = false;
            break;
        }
    }
    EXPECT_FALSE(allSame);

    // 验证解密恢复
    decrypt(data, 8, 0x55);
    EXPECT_EQ(memcmp(data, original, 8), 0);
}

TEST_F(EncryptionTest, AllKeyValues) {
    // 测试所有可能的密钥值
    unsigned char original[] = {0x12, 0x34, 0x56, 0x78};

    for (int key = 1; key < 256; ++key) {
        unsigned char data[4];
        memcpy(data, original, 4);

        encrypt(data, 4, static_cast<unsigned char>(key));
        decrypt(data, 4, static_cast<unsigned char>(key));

        EXPECT_EQ(memcmp(data, original, 4), 0) << "Failed for key: " << key;
    }
}

TEST_F(EncryptionTest, LargeDataRoundTrip) {
    std::vector<unsigned char> original(1000);
    for (size_t i = 0; i < original.size(); ++i) {
        original[i] = static_cast<unsigned char>(i & 0xFF);
    }

    std::vector<unsigned char> data = original;
    unsigned char key = 0x7F;

    encrypt(data.data(), data.size(), key);
    decrypt(data.data(), data.size(), key);

    EXPECT_EQ(data, original);
}

// ============================================
// HeaderFlag 测试
// ============================================

class HeaderFlagTest : public ::testing::Test {};

TEST_F(HeaderFlagTest, Construction) {
    char header[FLAG_LENGTH + 1] = { 'H','E','L','L', 0, 0, 0, 0, 0 };
    HeaderFlag hf(header);

    EXPECT_EQ(hf[0], 'H');
    EXPECT_EQ(hf[1], 'E');
    EXPECT_EQ(hf[2], 'L');
    EXPECT_EQ(hf[3], 'L');
}

TEST_F(HeaderFlagTest, DataAccess) {
    char header[FLAG_LENGTH + 1] = { 'T','E','S','T', 0x12, 0x34, 0x56, 0x78, 0 };
    HeaderFlag hf(header);

    EXPECT_EQ(memcmp(hf.data(), "TEST", 4), 0);
    EXPECT_EQ(static_cast<unsigned char>(hf[4]), 0x12);
    EXPECT_EQ(static_cast<unsigned char>(hf[5]), 0x34);
}

TEST_F(HeaderFlagTest, Modification) {
    char header[FLAG_LENGTH + 1] = { 0 };
    HeaderFlag hf(header);

    hf[0] = 'A';
    hf[1] = 'B';

    EXPECT_EQ(hf[0], 'A');
    EXPECT_EQ(hf[1], 'B');
}

// ============================================
// GetHead 测试
// ============================================

class GetHeadTest : public ::testing::Test {};

TEST_F(GetHeadTest, GeneratesValidHeader) {
    HeaderFlag hf = GetHead(default_encrypt, 0x42);

    // 检查基础格式
    EXPECT_EQ(hf[0], 'H');
    EXPECT_EQ(hf[1], 'E');
    EXPECT_EQ(hf[2], 'L');
    EXPECT_EQ(hf[3], 'L');
}

TEST_F(GetHeadTest, KeyAndInverseKey) {
    unsigned char key = 0x42;
    HeaderFlag hf = GetHead(default_encrypt, key);

    // 使用 default_encrypt 时，key 位置被清零
    // 但我们需要验证生成逻辑
    char rawHeader[FLAG_LENGTH + 1] = { 'H','E','L','L', 0 };
    HeaderFlag expected(rawHeader);
    expected[FLAG_LENGTH - 2] = key;
    expected[FLAG_LENGTH - 1] = ~key;
    default_encrypt((unsigned char*)expected.data(), FLAG_COMPLEN, expected[FLAG_LENGTH - 2]);

    EXPECT_EQ(memcmp(hf.data(), expected.data(), FLAG_LENGTH), 0);
}

TEST_F(GetHeadTest, EncryptedHeader) {
    unsigned char key = 0x55;
    HeaderFlag hf = GetHead(encrypt, key);

    // 头部应该被加密，不再是明文 "HELL"
    EXPECT_NE(memcmp(hf.data(), "HELL", 4), 0);

    // 密钥应该在正确位置
    unsigned char storedKey = static_cast<unsigned char>(hf[FLAG_LENGTH - 2]);
    unsigned char inverseKey = static_cast<unsigned char>(hf[FLAG_LENGTH - 1]);
    EXPECT_EQ(storedKey, key);
    EXPECT_EQ(inverseKey, static_cast<unsigned char>(~key));
}

// ============================================
// CheckHead 测试
// ============================================

class CheckHeadTest : public ::testing::Test {};

TEST_F(CheckHeadTest, IdentifyHellFlag) {
    char header[MIN_COMLEN + 4] = { 'H','E','L','L', 0, 0, 0x42, static_cast<char>(~0x42), 0 };

    FlagType type = CheckHead(header, default_decrypt);
    EXPECT_EQ(type, FLAG_HELL);
}

TEST_F(CheckHeadTest, IdentifyShineFlag) {
    char header[MIN_COMLEN + 4] = { 'S','h','i','n','e', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

    FlagType type = CheckHead(header, default_decrypt);
    EXPECT_EQ(type, FLAG_SHINE);
}

TEST_F(CheckHeadTest, UnknownFlag) {
    char header[MIN_COMLEN + 4] = { 'X','Y','Z','W', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

    FlagType type = CheckHead(header, default_decrypt);
    EXPECT_EQ(type, FLAG_UNKNOWN);
}

TEST_F(CheckHeadTest, EncryptedHellFlag) {
    // 生成加密的头部
    unsigned char key = 0x33;
    char header[FLAG_LENGTH + 1] = { 'H','E','L','L', 0, 0, 0, 0, 0 };
    header[FLAG_LENGTH - 2] = key;
    header[FLAG_LENGTH - 1] = ~key;
    encrypt((unsigned char*)header, FLAG_COMPLEN, key);

    char buffer[MIN_COMLEN + 4] = {};
    memcpy(buffer, header, FLAG_LENGTH);

    FlagType type = CheckHead(buffer, decrypt);
    EXPECT_EQ(type, FLAG_HELL);
}

// ============================================
// CheckHeadMulti 测试
// ============================================

class CheckHeadMultiTest : public ::testing::Test {};

TEST_F(CheckHeadMultiTest, PlainTextHeader) {
    char header[MIN_COMLEN + 4] = { 'H','E','L','L', 0, 0, 0x42, static_cast<char>(~0x42), 0 };

    HeaderEncType encType;
    FlagType type = CheckHeadMulti(header, encType);

    EXPECT_EQ(type, FLAG_HELL);
    EXPECT_EQ(encType, HeaderEncNone);
}

TEST_F(CheckHeadMultiTest, EncryptedHeader) {
    unsigned char key = 0x77;
    char header[MIN_COMLEN + 4] = { 'H','E','L','L', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    header[FLAG_LENGTH - 2] = key;
    header[FLAG_LENGTH - 1] = ~key;
    encrypt((unsigned char*)header, FLAG_COMPLEN, key);

    HeaderEncType encType;
    FlagType type = CheckHeadMulti(header, encType);

    EXPECT_EQ(type, FLAG_HELL);
    EXPECT_EQ(encType, HeaderEncV0);  // encrypt 对应 V0
}

TEST_F(CheckHeadMultiTest, UnrecognizedHeader) {
    char header[MIN_COMLEN + 4] = { 0xFF, 0xFE, 0xFD, 0xFC, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

    HeaderEncType encType;
    FlagType type = CheckHeadMulti(header, encType);

    EXPECT_EQ(type, FLAG_UNKNOWN);
    EXPECT_EQ(encType, HeaderEncUnknown);
}

// ============================================
// 头部生成和验证往返测试
// ============================================

class HeaderRoundTripTest : public ::testing::Test {};

TEST_F(HeaderRoundTripTest, DefaultEncryptRoundTrip) {
    HeaderFlag hf = GetHead(default_encrypt, 0x42);

    char buffer[MIN_COMLEN + 4] = {};
    memcpy(buffer, hf.data(), FLAG_LENGTH);

    HeaderEncType encType;
    FlagType type = CheckHeadMulti(buffer, encType);

    EXPECT_EQ(type, FLAG_HELL);
}

TEST_F(HeaderRoundTripTest, EncryptRoundTrip) {
    HeaderFlag hf = GetHead(encrypt, 0x88);

    char buffer[MIN_COMLEN + 4] = {};
    memcpy(buffer, hf.data(), FLAG_LENGTH);

    HeaderEncType encType;
    FlagType type = CheckHeadMulti(buffer, encType);

    EXPECT_EQ(type, FLAG_HELL);
}

TEST_F(HeaderRoundTripTest, AllKeyValuesRoundTrip) {
    for (int key = 1; key < 256; ++key) {
        HeaderFlag hf = GetHead(encrypt, static_cast<unsigned char>(key));

        char buffer[MIN_COMLEN + 4] = {};
        memcpy(buffer, hf.data(), FLAG_LENGTH);

        HeaderEncType encType;
        FlagType type = CheckHeadMulti(buffer, encType);

        EXPECT_EQ(type, FLAG_HELL) << "Failed for key: " << key;
    }
}

// ============================================
// 数据包长度字段测试
// ============================================

class PacketLengthTest : public ::testing::Test {};

#pragma pack(push, 1)
struct PacketHeader {
    char flag[FLAG_LENGTH];
    uint32_t packedLength;      // 压缩后长度
    uint32_t originalLength;    // 原始长度
};
#pragma pack(pop)

TEST_F(PacketLengthTest, HeaderSize) {
    EXPECT_EQ(sizeof(PacketHeader), HDR_LENGTH);
}

TEST_F(PacketLengthTest, BuildAndParsePacket) {
    PacketHeader pkt = {};
    memcpy(pkt.flag, "HELL", 4);
    pkt.flag[6] = 0x42;
    pkt.flag[7] = ~0x42;
    pkt.packedLength = 100;
    pkt.originalLength = 200;

    // 解析
    uint32_t packedLen, origLen;
    memcpy(&packedLen, reinterpret_cast<char*>(&pkt) + FLAG_LENGTH, sizeof(uint32_t));
    memcpy(&origLen, reinterpret_cast<char*>(&pkt) + FLAG_LENGTH + sizeof(uint32_t), sizeof(uint32_t));

    EXPECT_EQ(packedLen, 100u);
    EXPECT_EQ(origLen, 200u);
}

TEST_F(PacketLengthTest, TotalPacketLength) {
    // 总包长度 = HDR_LENGTH + 压缩数据长度
    uint32_t dataLen = 1000;
    uint32_t totalLen = HDR_LENGTH + dataLen;

    EXPECT_EQ(totalLen, 1016u);
}

// ============================================
// 边界条件测试
// ============================================

class HeaderBoundaryTest : public ::testing::Test {};

TEST_F(HeaderBoundaryTest, MinimalPacket) {
    // 最小合法包：只有头部
    std::vector<uint8_t> packet(HDR_LENGTH);
    memcpy(packet.data(), "HELL", 4);
    packet[6] = 0x42;
    packet[7] = ~0x42;
    // packedLength = HDR_LENGTH
    uint32_t packedLen = HDR_LENGTH;
    memcpy(packet.data() + FLAG_LENGTH, &packedLen, sizeof(uint32_t));
    // originalLength = 0
    uint32_t origLen = 0;
    memcpy(packet.data() + FLAG_LENGTH + sizeof(uint32_t), &origLen, sizeof(uint32_t));

    EXPECT_EQ(packet.size(), HDR_LENGTH);
}

TEST_F(HeaderBoundaryTest, MaxPacketLength) {
    // 验证大包长度字段
    uint32_t maxDataLen = 100 * 1024 * 1024;  // 100 MB
    uint32_t totalLen = HDR_LENGTH + maxDataLen;

    PacketHeader pkt = {};
    pkt.packedLength = totalLen;
    pkt.originalLength = maxDataLen * 2;  // 压缩前更大

    EXPECT_EQ(pkt.packedLength, totalLen);
    EXPECT_EQ(pkt.originalLength, maxDataLen * 2);
}

TEST_F(HeaderBoundaryTest, TruncatedHeader) {
    // 不完整的头部不应该被识别
    char truncated[4] = { 'H', 'E', 'L', 'L' };

    // 不足以进行完整验证
    // 这里只是验证常量定义正确
    EXPECT_LT(sizeof(truncated), static_cast<size_t>(MIN_COMLEN));
}

// ============================================
// FlagType 枚举测试
// ============================================

class FlagTypeTest : public ::testing::Test {};

TEST_F(FlagTypeTest, EnumValues) {
    EXPECT_EQ(FLAG_WINOS, -1);
    EXPECT_EQ(FLAG_UNKNOWN, 0);
    EXPECT_EQ(FLAG_SHINE, 1);
    EXPECT_EQ(FLAG_FUCK, 2);
    EXPECT_EQ(FLAG_HELLO, 3);
    EXPECT_EQ(FLAG_HELL, 4);
}

TEST_F(FlagTypeTest, HeaderEncTypeValues) {
    EXPECT_EQ(HeaderEncUnknown, -1);
    EXPECT_EQ(HeaderEncNone, 0);
    EXPECT_EQ(HeaderEncV0, 1);
    EXPECT_EQ(HeaderEncNum, 8);
}

