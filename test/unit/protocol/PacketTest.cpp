/**
 * @file PacketTest.cpp
 * @brief 协议数据包结构测试
 *
 * 测试覆盖：
 * - 数据包结构大小验证
 * - 字段偏移和对齐
 * - 序列化/反序列化
 * - 边界条件
 */

#include <gtest/gtest.h>
#include <cstring>
#include <vector>
#include <cstdint>

// ============================================
// 协议数据结构定义（测试专用副本）
// ============================================

#pragma pack(push, 1)

// V1 文件传输包
struct FileChunkPacket {
    uint8_t     cmd;
    uint32_t    fileIndex;
    uint32_t    totalNum;
    uint64_t    fileSize;
    uint64_t    offset;
    uint64_t    dataLength;
    uint64_t    nameLength;
};  // 应该是 41 bytes

// V2 标志位
enum FileFlagsV2 : uint16_t {
    FFV2_NONE         = 0x0000,
    FFV2_LAST_CHUNK   = 0x0001,
    FFV2_RESUME_REQ   = 0x0002,
    FFV2_RESUME_RESP  = 0x0004,
    FFV2_CANCEL       = 0x0008,
    FFV2_DIRECTORY    = 0x0010,
    FFV2_COMPRESSED   = 0x0020,
    FFV2_ERROR        = 0x0040,
};

// V2 文件传输包
struct FileChunkPacketV2 {
    uint8_t     cmd;
    uint64_t    transferID;
    uint64_t    srcClientID;
    uint64_t    dstClientID;
    uint32_t    fileIndex;
    uint32_t    totalFiles;
    uint64_t    fileSize;
    uint64_t    offset;
    uint64_t    dataLength;
    uint64_t    nameLength;
    uint16_t    flags;
    uint16_t    checksum;
    uint8_t     reserved[8];
};  // 应该是 77 bytes

// V2 断点续传区间
struct FileRangeV2 {
    uint64_t    offset;
    uint64_t    length;
};  // 16 bytes

// V2 断点续传控制包
struct FileResumePacketV2 {
    uint8_t     cmd;
    uint64_t    transferID;
    uint64_t    srcClientID;
    uint64_t    dstClientID;
    uint32_t    fileIndex;
    uint64_t    fileSize;
    uint64_t    receivedBytes;
    uint16_t    flags;
    uint16_t    rangeCount;
};  // 49 bytes

// C2C 准备包
struct C2CPreparePacket {
    uint8_t     cmd;
    uint64_t    transferID;
    uint64_t    srcClientID;
};  // 17 bytes

// C2C 准备响应包
struct C2CPrepareRespPacket {
    uint8_t     cmd;
    uint64_t    transferID;
    uint64_t    srcClientID;
    uint16_t    pathLength;
};  // 19 bytes

// V2 文件完成校验包
struct FileCompletePacketV2 {
    uint8_t     cmd;
    uint64_t    transferID;
    uint64_t    srcClientID;
    uint64_t    dstClientID;
    uint32_t    fileIndex;
    uint64_t    fileSize;
    uint8_t     sha256[32];
};  // 69 bytes

#pragma pack(pop)

// 命令常量
enum Commands {
    COMMAND_SEND_FILE = 68,
    COMMAND_SEND_FILE_V2 = 85,
    COMMAND_FILE_RESUME = 86,
    COMMAND_CLIPBOARD_V2 = 87,
    COMMAND_FILE_QUERY_RESUME = 88,
    COMMAND_C2C_PREPARE = 89,
    COMMAND_C2C_TEXT = 90,
    COMMAND_FILE_COMPLETE_V2 = 91,
    COMMAND_C2C_PREPARE_RESP = 92,
};

// ============================================
// 结构大小测试
// ============================================

TEST(PacketSizeTest, FileChunkPacket_Size) {
    EXPECT_EQ(sizeof(FileChunkPacket), 41u);
}

TEST(PacketSizeTest, FileChunkPacketV2_Size) {
    EXPECT_EQ(sizeof(FileChunkPacketV2), 77u);
}

TEST(PacketSizeTest, FileRangeV2_Size) {
    EXPECT_EQ(sizeof(FileRangeV2), 16u);
}

TEST(PacketSizeTest, FileResumePacketV2_Size) {
    EXPECT_EQ(sizeof(FileResumePacketV2), 49u);
}

TEST(PacketSizeTest, C2CPreparePacket_Size) {
    EXPECT_EQ(sizeof(C2CPreparePacket), 17u);
}

TEST(PacketSizeTest, C2CPrepareRespPacket_Size) {
    EXPECT_EQ(sizeof(C2CPrepareRespPacket), 19u);
}

TEST(PacketSizeTest, FileCompletePacketV2_Size) {
    EXPECT_EQ(sizeof(FileCompletePacketV2), 69u);
}

// ============================================
// 字段偏移测试
// ============================================

TEST(PacketOffsetTest, FileChunkPacketV2_FieldOffsets) {
    EXPECT_EQ(offsetof(FileChunkPacketV2, cmd), 0u);
    EXPECT_EQ(offsetof(FileChunkPacketV2, transferID), 1u);
    EXPECT_EQ(offsetof(FileChunkPacketV2, srcClientID), 9u);
    EXPECT_EQ(offsetof(FileChunkPacketV2, dstClientID), 17u);
    EXPECT_EQ(offsetof(FileChunkPacketV2, fileIndex), 25u);
    EXPECT_EQ(offsetof(FileChunkPacketV2, totalFiles), 29u);
    EXPECT_EQ(offsetof(FileChunkPacketV2, fileSize), 33u);
    EXPECT_EQ(offsetof(FileChunkPacketV2, offset), 41u);
    EXPECT_EQ(offsetof(FileChunkPacketV2, dataLength), 49u);
    EXPECT_EQ(offsetof(FileChunkPacketV2, nameLength), 57u);
    EXPECT_EQ(offsetof(FileChunkPacketV2, flags), 65u);
    EXPECT_EQ(offsetof(FileChunkPacketV2, checksum), 67u);
    EXPECT_EQ(offsetof(FileChunkPacketV2, reserved), 69u);
}

TEST(PacketOffsetTest, FileResumePacketV2_FieldOffsets) {
    EXPECT_EQ(offsetof(FileResumePacketV2, cmd), 0u);
    EXPECT_EQ(offsetof(FileResumePacketV2, transferID), 1u);
    EXPECT_EQ(offsetof(FileResumePacketV2, srcClientID), 9u);
    EXPECT_EQ(offsetof(FileResumePacketV2, dstClientID), 17u);
    EXPECT_EQ(offsetof(FileResumePacketV2, fileIndex), 25u);
    EXPECT_EQ(offsetof(FileResumePacketV2, fileSize), 29u);
    EXPECT_EQ(offsetof(FileResumePacketV2, receivedBytes), 37u);
    EXPECT_EQ(offsetof(FileResumePacketV2, flags), 45u);
    EXPECT_EQ(offsetof(FileResumePacketV2, rangeCount), 47u);
}

// ============================================
// 序列化/反序列化测试
// ============================================

class PacketSerializationTest : public ::testing::Test {
protected:
    std::vector<uint8_t> buffer;

    template<typename T>
    void SerializePacket(const T& pkt) {
        buffer.resize(sizeof(T));
        memcpy(buffer.data(), &pkt, sizeof(T));
    }

    template<typename T>
    T DeserializePacket() {
        T pkt;
        memcpy(&pkt, buffer.data(), sizeof(T));
        return pkt;
    }
};

TEST_F(PacketSerializationTest, FileChunkPacketV2_RoundTrip) {
    FileChunkPacketV2 original = {};
    original.cmd = COMMAND_SEND_FILE_V2;
    original.transferID = 0x123456789ABCDEF0ULL;
    original.srcClientID = 0x1111111111111111ULL;
    original.dstClientID = 0x2222222222222222ULL;
    original.fileIndex = 42;
    original.totalFiles = 100;
    original.fileSize = 1024 * 1024 * 1024;  // 1GB
    original.offset = 512 * 1024;
    original.dataLength = 4096;
    original.nameLength = 256;
    original.flags = FFV2_LAST_CHUNK | FFV2_COMPRESSED;
    original.checksum = 0xABCD;
    memset(original.reserved, 0x55, sizeof(original.reserved));

    SerializePacket(original);
    auto restored = DeserializePacket<FileChunkPacketV2>();

    EXPECT_EQ(restored.cmd, original.cmd);
    EXPECT_EQ(restored.transferID, original.transferID);
    EXPECT_EQ(restored.srcClientID, original.srcClientID);
    EXPECT_EQ(restored.dstClientID, original.dstClientID);
    EXPECT_EQ(restored.fileIndex, original.fileIndex);
    EXPECT_EQ(restored.totalFiles, original.totalFiles);
    EXPECT_EQ(restored.fileSize, original.fileSize);
    EXPECT_EQ(restored.offset, original.offset);
    EXPECT_EQ(restored.dataLength, original.dataLength);
    EXPECT_EQ(restored.nameLength, original.nameLength);
    EXPECT_EQ(restored.flags, original.flags);
    EXPECT_EQ(restored.checksum, original.checksum);
    EXPECT_EQ(memcmp(restored.reserved, original.reserved, 8), 0);
}

TEST_F(PacketSerializationTest, FileResumePacketV2_RoundTrip) {
    FileResumePacketV2 original = {};
    original.cmd = COMMAND_FILE_RESUME;
    original.transferID = 0xDEADBEEFCAFEBABEULL;
    original.srcClientID = 12345;
    original.dstClientID = 67890;
    original.fileIndex = 5;
    original.fileSize = 0xFFFFFFFFFFFFFFFFULL;  // 最大值
    original.receivedBytes = 0x8000000000000000ULL;  // 大值
    original.flags = FFV2_RESUME_RESP;
    original.rangeCount = 10;

    SerializePacket(original);
    auto restored = DeserializePacket<FileResumePacketV2>();

    EXPECT_EQ(restored.cmd, original.cmd);
    EXPECT_EQ(restored.transferID, original.transferID);
    EXPECT_EQ(restored.srcClientID, original.srcClientID);
    EXPECT_EQ(restored.dstClientID, original.dstClientID);
    EXPECT_EQ(restored.fileIndex, original.fileIndex);
    EXPECT_EQ(restored.fileSize, original.fileSize);
    EXPECT_EQ(restored.receivedBytes, original.receivedBytes);
    EXPECT_EQ(restored.flags, original.flags);
    EXPECT_EQ(restored.rangeCount, original.rangeCount);
}

TEST_F(PacketSerializationTest, FileCompletePacketV2_RoundTrip) {
    FileCompletePacketV2 original = {};
    original.cmd = COMMAND_FILE_COMPLETE_V2;
    original.transferID = 99999;
    original.srcClientID = 1;
    original.dstClientID = 2;
    original.fileIndex = 0;
    original.fileSize = 12345678;
    // 填充 SHA-256
    for (int i = 0; i < 32; i++) {
        original.sha256[i] = (uint8_t)(i * 8);
    }

    SerializePacket(original);
    auto restored = DeserializePacket<FileCompletePacketV2>();

    EXPECT_EQ(restored.cmd, original.cmd);
    EXPECT_EQ(restored.transferID, original.transferID);
    EXPECT_EQ(restored.fileSize, original.fileSize);
    EXPECT_EQ(memcmp(restored.sha256, original.sha256, 32), 0);
}

// ============================================
// 标志位测试
// ============================================

TEST(FlagsTest, FileFlagsV2_SingleFlags) {
    EXPECT_EQ(FFV2_NONE, 0x0000);
    EXPECT_EQ(FFV2_LAST_CHUNK, 0x0001);
    EXPECT_EQ(FFV2_RESUME_REQ, 0x0002);
    EXPECT_EQ(FFV2_RESUME_RESP, 0x0004);
    EXPECT_EQ(FFV2_CANCEL, 0x0008);
    EXPECT_EQ(FFV2_DIRECTORY, 0x0010);
    EXPECT_EQ(FFV2_COMPRESSED, 0x0020);
    EXPECT_EQ(FFV2_ERROR, 0x0040);
}

TEST(FlagsTest, FileFlagsV2_Combinations) {
    uint16_t flags = FFV2_LAST_CHUNK | FFV2_COMPRESSED;
    EXPECT_TRUE(flags & FFV2_LAST_CHUNK);
    EXPECT_TRUE(flags & FFV2_COMPRESSED);
    EXPECT_FALSE(flags & FFV2_DIRECTORY);
    EXPECT_FALSE(flags & FFV2_ERROR);
}

TEST(FlagsTest, FileFlagsV2_NoBitOverlap) {
    // 验证各标志位不重叠
    uint16_t allFlags[] = {
        FFV2_LAST_CHUNK, FFV2_RESUME_REQ, FFV2_RESUME_RESP,
        FFV2_CANCEL, FFV2_DIRECTORY, FFV2_COMPRESSED, FFV2_ERROR
    };

    for (size_t i = 0; i < sizeof(allFlags)/sizeof(allFlags[0]); i++) {
        for (size_t j = i + 1; j < sizeof(allFlags)/sizeof(allFlags[0]); j++) {
            EXPECT_EQ(allFlags[i] & allFlags[j], 0)
                << "Flags overlap: " << allFlags[i] << " & " << allFlags[j];
        }
    }
}

// ============================================
// 命令常量测试
// ============================================

TEST(CommandTest, CommandValues_AreUnique) {
    std::vector<int> commands = {
        COMMAND_SEND_FILE,
        COMMAND_SEND_FILE_V2,
        COMMAND_FILE_RESUME,
        COMMAND_CLIPBOARD_V2,
        COMMAND_FILE_QUERY_RESUME,
        COMMAND_C2C_PREPARE,
        COMMAND_C2C_TEXT,
        COMMAND_FILE_COMPLETE_V2,
        COMMAND_C2C_PREPARE_RESP
    };

    // 验证无重复
    for (size_t i = 0; i < commands.size(); i++) {
        for (size_t j = i + 1; j < commands.size(); j++) {
            EXPECT_NE(commands[i], commands[j])
                << "Duplicate command values at index " << i << " and " << j;
        }
    }
}

TEST(CommandTest, CommandValues_FitInByte) {
    EXPECT_LE(COMMAND_SEND_FILE, 255);
    EXPECT_LE(COMMAND_SEND_FILE_V2, 255);
    EXPECT_LE(COMMAND_FILE_RESUME, 255);
    EXPECT_LE(COMMAND_FILE_COMPLETE_V2, 255);
}

// ============================================
// 边界条件测试
// ============================================

TEST(BoundaryTest, MaxFileSize) {
    FileChunkPacketV2 pkt = {};
    pkt.fileSize = UINT64_MAX;
    EXPECT_EQ(pkt.fileSize, 0xFFFFFFFFFFFFFFFFULL);
}

TEST(BoundaryTest, MaxOffset) {
    FileChunkPacketV2 pkt = {};
    pkt.offset = UINT64_MAX;
    EXPECT_EQ(pkt.offset, 0xFFFFFFFFFFFFFFFFULL);
}

TEST(BoundaryTest, ZeroValues) {
    FileChunkPacketV2 pkt = {};
    memset(&pkt, 0, sizeof(pkt));

    EXPECT_EQ(pkt.cmd, 0);
    EXPECT_EQ(pkt.transferID, 0u);
    EXPECT_EQ(pkt.fileSize, 0u);
    EXPECT_EQ(pkt.flags, FFV2_NONE);
}

// ============================================
// 带变长数据的包测试
// ============================================

TEST(VariableLengthTest, FileChunkPacketV2_WithFilename) {
    const char* filename = "test/folder/file.txt";
    size_t nameLen = strlen(filename);
    size_t totalSize = sizeof(FileChunkPacketV2) + nameLen;

    std::vector<uint8_t> buffer(totalSize);
    auto* pkt = reinterpret_cast<FileChunkPacketV2*>(buffer.data());

    pkt->cmd = COMMAND_SEND_FILE_V2;
    pkt->nameLength = static_cast<uint64_t>(nameLen);
    memcpy(buffer.data() + sizeof(FileChunkPacketV2), filename, nameLen);

    // 验证可以正确读取
    EXPECT_EQ(pkt->nameLength, nameLen);
    std::string extractedName(
        reinterpret_cast<char*>(buffer.data() + sizeof(FileChunkPacketV2)),
        pkt->nameLength
    );
    EXPECT_EQ(extractedName, filename);
}

TEST(VariableLengthTest, FileChunkPacketV2_WithFilenameAndData) {
    const char* filename = "data.bin";
    size_t nameLen = strlen(filename);
    std::vector<uint8_t> fileData = {0x01, 0x02, 0x03, 0x04, 0x05};
    size_t dataLen = fileData.size();

    size_t totalSize = sizeof(FileChunkPacketV2) + nameLen + dataLen;
    std::vector<uint8_t> buffer(totalSize);

    auto* pkt = reinterpret_cast<FileChunkPacketV2*>(buffer.data());
    pkt->cmd = COMMAND_SEND_FILE_V2;
    pkt->nameLength = nameLen;
    pkt->dataLength = dataLen;

    // 写入文件名
    memcpy(buffer.data() + sizeof(FileChunkPacketV2), filename, nameLen);
    // 写入数据
    memcpy(buffer.data() + sizeof(FileChunkPacketV2) + nameLen, fileData.data(), dataLen);

    // 验证
    EXPECT_EQ(buffer.size(), totalSize);

    // 提取文件名
    std::string extractedName(
        reinterpret_cast<char*>(buffer.data() + sizeof(FileChunkPacketV2)),
        pkt->nameLength
    );
    EXPECT_EQ(extractedName, filename);

    // 提取数据
    std::vector<uint8_t> extractedData(
        buffer.begin() + sizeof(FileChunkPacketV2) + nameLen,
        buffer.end()
    );
    EXPECT_EQ(extractedData, fileData);
}

// ============================================
// 断点续传区间测试
// ============================================

TEST(ResumeRangeTest, SingleRange) {
    FileRangeV2 range = {0, 1024};
    EXPECT_EQ(range.offset, 0u);
    EXPECT_EQ(range.length, 1024u);
}

TEST(ResumeRangeTest, MultipleRanges) {
    std::vector<FileRangeV2> ranges = {
        {0, 1024},
        {2048, 512},
        {4096, 2048}
    };

    // 计算总接收字节数
    uint64_t totalReceived = 0;
    for (const auto& r : ranges) {
        totalReceived += r.length;
    }
    EXPECT_EQ(totalReceived, 1024u + 512u + 2048u);
}

TEST(ResumeRangeTest, PacketWithRanges) {
    uint16_t rangeCount = 3;
    size_t totalSize = sizeof(FileResumePacketV2) + rangeCount * sizeof(FileRangeV2);

    std::vector<uint8_t> buffer(totalSize);
    auto* pkt = reinterpret_cast<FileResumePacketV2*>(buffer.data());
    pkt->cmd = COMMAND_FILE_RESUME;
    pkt->rangeCount = rangeCount;

    auto* ranges = reinterpret_cast<FileRangeV2*>(buffer.data() + sizeof(FileResumePacketV2));
    ranges[0] = {0, 1024};
    ranges[1] = {2048, 512};
    ranges[2] = {4096, 2048};

    // 验证
    EXPECT_EQ(pkt->rangeCount, 3);
    EXPECT_EQ(ranges[0].offset, 0u);
    EXPECT_EQ(ranges[1].offset, 2048u);
    EXPECT_EQ(ranges[2].length, 2048u);
}

// ============================================
// 字节序测试（假设小端）
// ============================================

TEST(EndiannessTest, LittleEndian_Uint64) {
    uint64_t value = 0x0102030405060708ULL;
    uint8_t bytes[8];
    memcpy(bytes, &value, 8);

    // 小端：低字节在前
    EXPECT_EQ(bytes[0], 0x08);
    EXPECT_EQ(bytes[1], 0x07);
    EXPECT_EQ(bytes[7], 0x01);
}

TEST(EndiannessTest, FileChunkPacketV2_TransferID) {
    FileChunkPacketV2 pkt = {};
    pkt.transferID = 0x0102030405060708ULL;

    uint8_t* raw = reinterpret_cast<uint8_t*>(&pkt);

    // transferID 从 offset 1 开始
    EXPECT_EQ(raw[1], 0x08);  // 低字节
    EXPECT_EQ(raw[8], 0x01);  // 高字节
}
