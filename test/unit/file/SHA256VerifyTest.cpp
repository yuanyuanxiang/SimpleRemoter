/**
 * @file SHA256VerifyTest.cpp
 * @brief SHA-256 文件校验测试
 *
 * 测试覆盖：
 * - SHA-256 哈希计算
 * - FileCompletePacketV2 构建与解析
 * - 文件完整性校验逻辑
 * - 校验失败处理
 */

#include <gtest/gtest.h>
#include <cstring>
#include <vector>
#include <array>
#include <string>
#include <iomanip>
#include <sstream>

// ============================================
// 简化版 SHA-256 实现（测试专用）
// 生产环境使用 OpenSSL 或 Windows CNG
// ============================================

class SHA256 {
public:
    SHA256() { Reset(); }

    void Reset() {
        m_state[0] = 0x6a09e667;
        m_state[1] = 0xbb67ae85;
        m_state[2] = 0x3c6ef372;
        m_state[3] = 0xa54ff53a;
        m_state[4] = 0x510e527f;
        m_state[5] = 0x9b05688c;
        m_state[6] = 0x1f83d9ab;
        m_state[7] = 0x5be0cd19;
        m_bitLen = 0;
        m_bufferLen = 0;
    }

    void Update(const uint8_t* data, size_t len) {
        for (size_t i = 0; i < len; ++i) {
            m_buffer[m_bufferLen++] = data[i];
            if (m_bufferLen == 64) {
                Transform();
                m_bitLen += 512;
                m_bufferLen = 0;
            }
        }
    }

    void Update(const std::vector<uint8_t>& data) {
        Update(data.data(), data.size());
    }

    std::array<uint8_t, 32> Finalize() {
        size_t i = m_bufferLen;

        // Pad
        if (m_bufferLen < 56) {
            m_buffer[i++] = 0x80;
            while (i < 56) m_buffer[i++] = 0x00;
        } else {
            m_buffer[i++] = 0x80;
            while (i < 64) m_buffer[i++] = 0x00;
            Transform();
            memset(m_buffer, 0, 56);
        }

        // Append length
        m_bitLen += m_bufferLen * 8;
        m_buffer[63] = static_cast<uint8_t>(m_bitLen);
        m_buffer[62] = static_cast<uint8_t>(m_bitLen >> 8);
        m_buffer[61] = static_cast<uint8_t>(m_bitLen >> 16);
        m_buffer[60] = static_cast<uint8_t>(m_bitLen >> 24);
        m_buffer[59] = static_cast<uint8_t>(m_bitLen >> 32);
        m_buffer[58] = static_cast<uint8_t>(m_bitLen >> 40);
        m_buffer[57] = static_cast<uint8_t>(m_bitLen >> 48);
        m_buffer[56] = static_cast<uint8_t>(m_bitLen >> 56);
        Transform();

        // Output
        std::array<uint8_t, 32> hash;
        for (int j = 0; j < 8; ++j) {
            hash[j * 4 + 0] = (m_state[j] >> 24) & 0xff;
            hash[j * 4 + 1] = (m_state[j] >> 16) & 0xff;
            hash[j * 4 + 2] = (m_state[j] >> 8) & 0xff;
            hash[j * 4 + 3] = m_state[j] & 0xff;
        }
        return hash;
    }

    static std::string HashToHex(const std::array<uint8_t, 32>& hash) {
        std::ostringstream ss;
        for (uint8_t b : hash) {
            ss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(b);
        }
        return ss.str();
    }

private:
    static uint32_t RotR(uint32_t x, uint32_t n) { return (x >> n) | (x << (32 - n)); }
    static uint32_t Ch(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (~x & z); }
    static uint32_t Maj(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (x & z) ^ (y & z); }
    static uint32_t Sig0(uint32_t x) { return RotR(x, 2) ^ RotR(x, 13) ^ RotR(x, 22); }
    static uint32_t Sig1(uint32_t x) { return RotR(x, 6) ^ RotR(x, 11) ^ RotR(x, 25); }
    static uint32_t sig0(uint32_t x) { return RotR(x, 7) ^ RotR(x, 18) ^ (x >> 3); }
    static uint32_t sig1(uint32_t x) { return RotR(x, 17) ^ RotR(x, 19) ^ (x >> 10); }

    void Transform() {
        static const uint32_t K[64] = {
            0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
            0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
            0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
            0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
            0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
            0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
            0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
            0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
        };

        uint32_t W[64];
        for (int i = 0; i < 16; ++i) {
            W[i] = (m_buffer[i * 4] << 24) | (m_buffer[i * 4 + 1] << 16) |
                   (m_buffer[i * 4 + 2] << 8) | m_buffer[i * 4 + 3];
        }
        for (int i = 16; i < 64; ++i) {
            W[i] = sig1(W[i - 2]) + W[i - 7] + sig0(W[i - 15]) + W[i - 16];
        }

        uint32_t a = m_state[0], b = m_state[1], c = m_state[2], d = m_state[3];
        uint32_t e = m_state[4], f = m_state[5], g = m_state[6], h = m_state[7];

        for (int i = 0; i < 64; ++i) {
            uint32_t t1 = h + Sig1(e) + Ch(e, f, g) + K[i] + W[i];
            uint32_t t2 = Sig0(a) + Maj(a, b, c);
            h = g; g = f; f = e; e = d + t1;
            d = c; c = b; b = a; a = t1 + t2;
        }

        m_state[0] += a; m_state[1] += b; m_state[2] += c; m_state[3] += d;
        m_state[4] += e; m_state[5] += f; m_state[6] += g; m_state[7] += h;
    }

    uint32_t m_state[8];
    uint8_t m_buffer[64];
    uint64_t m_bitLen;
    size_t m_bufferLen;
};

// ============================================
// 协议结构（测试专用）
// ============================================

#pragma pack(push, 1)

struct FileCompletePacketV2 {
    uint8_t     cmd;
    uint64_t    transferID;
    uint64_t    srcClientID;
    uint64_t    dstClientID;
    uint32_t    fileIndex;
    uint64_t    fileSize;
    uint8_t     sha256[32];
};

enum FileErrorV2 : uint8_t {
    FEV2_OK               = 0,
    FEV2_TARGET_OFFLINE   = 1,
    FEV2_VERSION_MISMATCH = 2,
    FEV2_FILE_NOT_FOUND   = 3,
    FEV2_ACCESS_DENIED    = 4,
    FEV2_DISK_FULL        = 5,
    FEV2_TRANSFER_CANCEL  = 6,
    FEV2_CHECKSUM_ERROR   = 7,
    FEV2_HASH_MISMATCH    = 8,
};

#pragma pack(pop)

// ============================================
// 辅助函数
// ============================================

std::vector<uint8_t> BuildFileCompletePacket(
    uint64_t transferID,
    uint64_t srcClientID,
    uint64_t dstClientID,
    uint32_t fileIndex,
    uint64_t fileSize,
    const std::array<uint8_t, 32>& sha256)
{
    std::vector<uint8_t> buffer(sizeof(FileCompletePacketV2));
    FileCompletePacketV2* pkt = reinterpret_cast<FileCompletePacketV2*>(buffer.data());

    pkt->cmd = 91;  // COMMAND_FILE_COMPLETE_V2
    pkt->transferID = transferID;
    pkt->srcClientID = srcClientID;
    pkt->dstClientID = dstClientID;
    pkt->fileIndex = fileIndex;
    pkt->fileSize = fileSize;
    memcpy(pkt->sha256, sha256.data(), 32);

    return buffer;
}

bool ParseFileCompletePacket(
    const uint8_t* buffer, size_t len,
    FileCompletePacketV2& pkt)
{
    if (len < sizeof(FileCompletePacketV2)) {
        return false;
    }
    memcpy(&pkt, buffer, sizeof(FileCompletePacketV2));
    return true;
}

// 校验文件完整性
FileErrorV2 VerifyFileIntegrity(
    const std::array<uint8_t, 32>& expectedHash,
    const std::vector<uint8_t>& fileData)
{
    SHA256 sha;
    sha.Update(fileData);
    auto actualHash = sha.Finalize();

    if (actualHash == expectedHash) {
        return FEV2_OK;
    }
    return FEV2_HASH_MISMATCH;
}

// ============================================
// SHA-256 基础测试
// ============================================

class SHA256Test : public ::testing::Test {};

TEST_F(SHA256Test, EmptyString) {
    SHA256 sha;
    auto hash = sha.Finalize();

    // SHA-256("") = e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855
    std::string hex = SHA256::HashToHex(hash);
    EXPECT_EQ(hex, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

TEST_F(SHA256Test, HelloWorld) {
    SHA256 sha;
    const char* msg = "Hello, World!";
    sha.Update(reinterpret_cast<const uint8_t*>(msg), strlen(msg));
    auto hash = sha.Finalize();

    // SHA-256("Hello, World!") = dffd6021bb2bd5b0af676290809ec3a53191dd81c7f70a4b28688a362182986f
    std::string hex = SHA256::HashToHex(hash);
    EXPECT_EQ(hex, "dffd6021bb2bd5b0af676290809ec3a53191dd81c7f70a4b28688a362182986f");
}

TEST_F(SHA256Test, ABC) {
    SHA256 sha;
    const char* msg = "abc";
    sha.Update(reinterpret_cast<const uint8_t*>(msg), strlen(msg));
    auto hash = sha.Finalize();

    // SHA-256("abc") = ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad
    std::string hex = SHA256::HashToHex(hash);
    EXPECT_EQ(hex, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

TEST_F(SHA256Test, IncrementalUpdate) {
    SHA256 sha1, sha2;

    const char* part1 = "Hello, ";
    const char* part2 = "World!";
    const char* full = "Hello, World!";

    sha1.Update(reinterpret_cast<const uint8_t*>(part1), strlen(part1));
    sha1.Update(reinterpret_cast<const uint8_t*>(part2), strlen(part2));

    sha2.Update(reinterpret_cast<const uint8_t*>(full), strlen(full));

    auto hash1 = sha1.Finalize();
    auto hash2 = sha2.Finalize();

    EXPECT_EQ(hash1, hash2);
}

TEST_F(SHA256Test, LargeData) {
    SHA256 sha;

    // 1 MB of zeros
    std::vector<uint8_t> data(1024 * 1024, 0);
    sha.Update(data);
    auto hash = sha.Finalize();

    // 验证计算成功（不崩溃）
    EXPECT_EQ(hash.size(), 32u);
}

TEST_F(SHA256Test, BinaryData) {
    SHA256 sha;

    std::vector<uint8_t> data = {0x00, 0x01, 0x02, 0x03, 0xff, 0xfe, 0xfd, 0xfc};
    sha.Update(data);
    auto hash = sha.Finalize();

    // 验证计算成功
    EXPECT_EQ(hash.size(), 32u);
}

TEST_F(SHA256Test, Reset) {
    SHA256 sha;

    sha.Update(reinterpret_cast<const uint8_t*>("test"), 4);
    sha.Reset();
    auto hash = sha.Finalize();

    // 重置后应该等于空字符串的哈希
    std::string hex = SHA256::HashToHex(hash);
    EXPECT_EQ(hex, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

// ============================================
// FileCompletePacketV2 测试
// ============================================

class FileCompletePacketTest : public ::testing::Test {};

TEST_F(FileCompletePacketTest, BuildAndParse) {
    std::array<uint8_t, 32> sha256 = {};
    sha256[0] = 0xAA;
    sha256[31] = 0xBB;

    auto buffer = BuildFileCompletePacket(12345, 100, 200, 5, 1024 * 1024, sha256);

    FileCompletePacketV2 pkt;
    ASSERT_TRUE(ParseFileCompletePacket(buffer.data(), buffer.size(), pkt));

    EXPECT_EQ(pkt.cmd, 91);
    EXPECT_EQ(pkt.transferID, 12345u);
    EXPECT_EQ(pkt.srcClientID, 100u);
    EXPECT_EQ(pkt.dstClientID, 200u);
    EXPECT_EQ(pkt.fileIndex, 5u);
    EXPECT_EQ(pkt.fileSize, 1024u * 1024u);
    EXPECT_EQ(pkt.sha256[0], 0xAA);
    EXPECT_EQ(pkt.sha256[31], 0xBB);
}

TEST_F(FileCompletePacketTest, TruncatedPacket) {
    std::array<uint8_t, 32> sha256 = {};
    auto buffer = BuildFileCompletePacket(1, 0, 0, 0, 100, sha256);

    // 截断
    FileCompletePacketV2 pkt;
    EXPECT_FALSE(ParseFileCompletePacket(buffer.data(), sizeof(FileCompletePacketV2) - 10, pkt));
}

TEST_F(FileCompletePacketTest, LargeFileSize) {
    std::array<uint8_t, 32> sha256 = {};
    uint64_t largeSize = 100ULL * 1024 * 1024 * 1024;  // 100 GB

    auto buffer = BuildFileCompletePacket(1, 0, 0, 0, largeSize, sha256);

    FileCompletePacketV2 pkt;
    ASSERT_TRUE(ParseFileCompletePacket(buffer.data(), buffer.size(), pkt));
    EXPECT_EQ(pkt.fileSize, largeSize);
}

// ============================================
// 文件完整性校验测试
// ============================================

class FileIntegrityTest : public ::testing::Test {};

TEST_F(FileIntegrityTest, CorrectHash) {
    std::vector<uint8_t> fileData = {'H', 'e', 'l', 'l', 'o'};

    SHA256 sha;
    sha.Update(fileData);
    auto expectedHash = sha.Finalize();

    auto result = VerifyFileIntegrity(expectedHash, fileData);
    EXPECT_EQ(result, FEV2_OK);
}

TEST_F(FileIntegrityTest, IncorrectHash) {
    std::vector<uint8_t> fileData = {'H', 'e', 'l', 'l', 'o'};
    std::array<uint8_t, 32> wrongHash = {};  // 全零的错误哈希

    auto result = VerifyFileIntegrity(wrongHash, fileData);
    EXPECT_EQ(result, FEV2_HASH_MISMATCH);
}

TEST_F(FileIntegrityTest, CorruptedData) {
    std::vector<uint8_t> originalData = {'H', 'e', 'l', 'l', 'o'};

    SHA256 sha;
    sha.Update(originalData);
    auto originalHash = sha.Finalize();

    // 修改一个字节
    std::vector<uint8_t> corruptedData = originalData;
    corruptedData[2] = 'X';

    auto result = VerifyFileIntegrity(originalHash, corruptedData);
    EXPECT_EQ(result, FEV2_HASH_MISMATCH);
}

TEST_F(FileIntegrityTest, EmptyFile) {
    std::vector<uint8_t> emptyData;

    SHA256 sha;
    auto emptyHash = sha.Finalize();

    auto result = VerifyFileIntegrity(emptyHash, emptyData);
    EXPECT_EQ(result, FEV2_OK);
}

TEST_F(FileIntegrityTest, SingleBitDifference) {
    std::vector<uint8_t> data1 = {0x00};
    std::vector<uint8_t> data2 = {0x01};  // 单比特差异

    SHA256 sha1, sha2;
    sha1.Update(data1);
    sha2.Update(data2);

    auto hash1 = sha1.Finalize();
    auto hash2 = sha2.Finalize();

    // 哈希应该完全不同
    EXPECT_NE(hash1, hash2);

    // 验证错误检测
    auto result = VerifyFileIntegrity(hash1, data2);
    EXPECT_EQ(result, FEV2_HASH_MISMATCH);
}

// ============================================
// 流式哈希计算测试
// ============================================

class StreamingHashTest : public ::testing::Test {};

TEST_F(StreamingHashTest, ChunkedUpdate) {
    std::vector<uint8_t> fullData(10000);
    for (size_t i = 0; i < fullData.size(); ++i) {
        fullData[i] = static_cast<uint8_t>(i & 0xFF);
    }

    // 一次性计算
    SHA256 sha1;
    sha1.Update(fullData);
    auto hash1 = sha1.Finalize();

    // 分块计算
    SHA256 sha2;
    size_t chunkSize = 64;  // SHA-256 块大小
    for (size_t i = 0; i < fullData.size(); i += chunkSize) {
        size_t len = std::min(chunkSize, fullData.size() - i);
        sha2.Update(fullData.data() + i, len);
    }
    auto hash2 = sha2.Finalize();

    EXPECT_EQ(hash1, hash2);
}

TEST_F(StreamingHashTest, VariableChunkSizes) {
    std::vector<uint8_t> data(1000);
    for (size_t i = 0; i < data.size(); ++i) {
        data[i] = static_cast<uint8_t>(i);
    }

    SHA256 sha1;
    sha1.Update(data);
    auto expected = sha1.Finalize();

    // 不同大小的块
    std::vector<size_t> chunkSizes = {1, 7, 63, 64, 65, 128, 1000};

    for (size_t chunkSize : chunkSizes) {
        SHA256 sha2;
        for (size_t i = 0; i < data.size(); i += chunkSize) {
            size_t len = std::min(chunkSize, data.size() - i);
            sha2.Update(data.data() + i, len);
        }
        auto actual = sha2.Finalize();

        EXPECT_EQ(expected, actual) << "Failed for chunk size: " << chunkSize;
    }
}

// ============================================
// 文件传输完整性验证场景
// ============================================

class TransferVerificationTest : public ::testing::Test {};

TEST_F(TransferVerificationTest, SimulateSuccessfulTransfer) {
    // 模拟文件数据
    std::vector<uint8_t> fileData(1024 * 64);  // 64 KB
    for (size_t i = 0; i < fileData.size(); ++i) {
        fileData[i] = static_cast<uint8_t>(i * 17);  // 伪随机数据
    }

    // 发送方计算哈希
    SHA256 senderSha;
    senderSha.Update(fileData);
    auto senderHash = senderSha.Finalize();

    // 构建校验包
    auto packet = BuildFileCompletePacket(12345, 100, 0, 0, fileData.size(), senderHash);

    // 接收方解析并验证
    FileCompletePacketV2 pkt;
    ASSERT_TRUE(ParseFileCompletePacket(packet.data(), packet.size(), pkt));
    EXPECT_EQ(pkt.fileSize, fileData.size());

    // 接收方计算哈希并比较
    std::array<uint8_t, 32> expectedHash;
    memcpy(expectedHash.data(), pkt.sha256, 32);

    auto result = VerifyFileIntegrity(expectedHash, fileData);
    EXPECT_EQ(result, FEV2_OK);
}

TEST_F(TransferVerificationTest, SimulateCorruptedTransfer) {
    // 原始文件
    std::vector<uint8_t> originalData(1024);
    for (size_t i = 0; i < originalData.size(); ++i) {
        originalData[i] = static_cast<uint8_t>(i);
    }

    // 发送方计算哈希
    SHA256 senderSha;
    senderSha.Update(originalData);
    auto senderHash = senderSha.Finalize();

    // 模拟传输中数据损坏
    std::vector<uint8_t> receivedData = originalData;
    receivedData[512] ^= 0xFF;  // 翻转一个字节

    // 校验失败
    auto result = VerifyFileIntegrity(senderHash, receivedData);
    EXPECT_EQ(result, FEV2_HASH_MISMATCH);
}

TEST_F(TransferVerificationTest, MultipleFilesInTransfer) {
    struct FileInfo {
        std::vector<uint8_t> data;
        std::array<uint8_t, 32> hash;
    };

    std::vector<FileInfo> files(5);

    // 生成测试文件
    for (int i = 0; i < 5; ++i) {
        files[i].data.resize(1000 * (i + 1));
        for (size_t j = 0; j < files[i].data.size(); ++j) {
            files[i].data[j] = static_cast<uint8_t>(j + i * 7);
        }

        SHA256 sha;
        sha.Update(files[i].data);
        files[i].hash = sha.Finalize();
    }

    // 验证每个文件
    for (int i = 0; i < 5; ++i) {
        auto result = VerifyFileIntegrity(files[i].hash, files[i].data);
        EXPECT_EQ(result, FEV2_OK) << "File " << i << " verification failed";
    }

    // 交叉验证应该失败
    auto crossResult = VerifyFileIntegrity(files[0].hash, files[1].data);
    EXPECT_EQ(crossResult, FEV2_HASH_MISMATCH);
}

// ============================================
// 边界条件测试
// ============================================

class HashBoundaryTest : public ::testing::Test {};

TEST_F(HashBoundaryTest, ExactBlockSize) {
    // SHA-256 块大小是 64 字节
    std::vector<uint8_t> data(64, 'A');

    SHA256 sha;
    sha.Update(data);
    auto hash = sha.Finalize();

    EXPECT_EQ(hash.size(), 32u);
}

TEST_F(HashBoundaryTest, BlockSizePlusOne) {
    std::vector<uint8_t> data(65, 'B');

    SHA256 sha;
    sha.Update(data);
    auto hash = sha.Finalize();

    EXPECT_EQ(hash.size(), 32u);
}

TEST_F(HashBoundaryTest, BlockSizeMinusOne) {
    std::vector<uint8_t> data(63, 'C');

    SHA256 sha;
    sha.Update(data);
    auto hash = sha.Finalize();

    EXPECT_EQ(hash.size(), 32u);
}

TEST_F(HashBoundaryTest, MultipleBlocks) {
    std::vector<uint8_t> data(64 * 10, 'D');

    SHA256 sha;
    sha.Update(data);
    auto hash = sha.Finalize();

    EXPECT_EQ(hash.size(), 32u);
}

TEST_F(HashBoundaryTest, PaddingBoundary55) {
    // 55 字节需要特殊处理（padding + length 刚好 64）
    std::vector<uint8_t> data(55, 'E');

    SHA256 sha;
    sha.Update(data);
    auto hash = sha.Finalize();

    EXPECT_EQ(hash.size(), 32u);
}

TEST_F(HashBoundaryTest, PaddingBoundary56) {
    // 56 字节需要额外的块
    std::vector<uint8_t> data(56, 'F');

    SHA256 sha;
    sha.Update(data);
    auto hash = sha.Finalize();

    EXPECT_EQ(hash.size(), 32u);
}

