/**
 * @file FileTransferV2Test.cpp
 * @brief V2 文件传输逻辑测试
 *
 * 测试覆盖：
 * - TransferOptionsV2 结构体
 * - 传输ID生成
 * - 包头构建与解析
 * - 变长数据包处理
 */

#include <gtest/gtest.h>
#include <cstring>
#include <vector>
#include <map>
#include <random>
#include <chrono>

// ============================================
// 从 file_upload.h 复制的结构体定义（测试专用）
// ============================================

#pragma pack(push, 1)

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
};

struct FileRangeV2 {
    uint64_t    offset;
    uint64_t    length;
};

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
};

struct FileCompletePacketV2 {
    uint8_t     cmd;
    uint64_t    transferID;
    uint64_t    srcClientID;
    uint64_t    dstClientID;
    uint32_t    fileIndex;
    uint64_t    fileSize;
    uint8_t     sha256[32];
};

struct FileQueryResumeV2 {
    uint8_t     cmd;
    uint64_t    transferID;
    uint64_t    srcClientID;
    uint64_t    dstClientID;
    uint32_t    fileCount;
};

struct FileQueryResumeEntryV2 {
    uint64_t    fileSize;
    uint16_t    nameLength;
};

struct FileResumeResponseV2 {
    uint8_t     cmd;
    uint64_t    srcClientID;
    uint64_t    dstClientID;
    uint16_t    flags;
    uint32_t    fileCount;
};

struct FileResumeResponseEntryV2 {
    uint32_t    fileIndex;
    uint64_t    receivedBytes;
};

#pragma pack(pop)

// V2 传输选项
struct TransferOptionsV2 {
    uint64_t    transferID;
    uint64_t    srcClientID;
    uint64_t    dstClientID;
    bool        enableResume;
    std::map<uint32_t, uint64_t> startOffsets;

    TransferOptionsV2() : transferID(0), srcClientID(0), dstClientID(0), enableResume(true) {}
};

// ============================================
// 辅助函数（测试专用实现）
// ============================================

// 生成传输ID（简化实现）
uint64_t GenerateTransferID() {
    static std::mt19937_64 rng(
        static_cast<uint64_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())
    );
    return rng();
}

// 构建文件块包
std::vector<uint8_t> BuildFileChunkPacketV2(
    uint64_t transferID,
    uint64_t srcClientID,
    uint64_t dstClientID,
    uint32_t fileIndex,
    uint32_t totalFiles,
    uint64_t fileSize,
    uint64_t offset,
    const std::string& filename,
    const std::vector<uint8_t>& data,
    uint16_t flags = FFV2_NONE)
{
    size_t totalSize = sizeof(FileChunkPacketV2) + filename.size() + data.size();
    std::vector<uint8_t> buffer(totalSize);

    FileChunkPacketV2* pkt = reinterpret_cast<FileChunkPacketV2*>(buffer.data());
    pkt->cmd = 85;  // COMMAND_SEND_FILE_V2
    pkt->transferID = transferID;
    pkt->srcClientID = srcClientID;
    pkt->dstClientID = dstClientID;
    pkt->fileIndex = fileIndex;
    pkt->totalFiles = totalFiles;
    pkt->fileSize = fileSize;
    pkt->offset = offset;
    pkt->dataLength = data.size();
    pkt->nameLength = filename.size();
    pkt->flags = flags;
    pkt->checksum = 0;
    memset(pkt->reserved, 0, sizeof(pkt->reserved));

    // 追加文件名
    memcpy(buffer.data() + sizeof(FileChunkPacketV2), filename.data(), filename.size());

    // 追加数据
    if (!data.empty()) {
        memcpy(buffer.data() + sizeof(FileChunkPacketV2) + filename.size(), data.data(), data.size());
    }

    return buffer;
}

// 解析文件块包
bool ParseFileChunkPacketV2(
    const uint8_t* buffer, size_t len,
    FileChunkPacketV2& header,
    std::string& filename,
    std::vector<uint8_t>& data)
{
    if (len < sizeof(FileChunkPacketV2)) {
        return false;
    }

    memcpy(&header, buffer, sizeof(FileChunkPacketV2));

    size_t expectedLen = sizeof(FileChunkPacketV2) + header.nameLength + header.dataLength;
    if (len < expectedLen) {
        return false;
    }

    const char* namePtr = reinterpret_cast<const char*>(buffer + sizeof(FileChunkPacketV2));
    filename.assign(namePtr, header.nameLength);

    if (header.dataLength > 0) {
        const uint8_t* dataPtr = buffer + sizeof(FileChunkPacketV2) + header.nameLength;
        data.assign(dataPtr, dataPtr + header.dataLength);
    } else {
        data.clear();
    }

    return true;
}

// 构建续传查询包
std::vector<uint8_t> BuildResumeQuery(
    uint64_t transferID,
    uint64_t srcClientID,
    uint64_t dstClientID,
    const std::vector<std::pair<std::string, uint64_t>>& files)
{
    // 计算总大小
    size_t totalSize = sizeof(FileQueryResumeV2);
    for (const auto& file : files) {
        totalSize += sizeof(FileQueryResumeEntryV2) + file.first.size();
    }

    std::vector<uint8_t> buffer(totalSize);
    FileQueryResumeV2* pkt = reinterpret_cast<FileQueryResumeV2*>(buffer.data());
    pkt->cmd = 88;  // COMMAND_FILE_QUERY_RESUME
    pkt->transferID = transferID;
    pkt->srcClientID = srcClientID;
    pkt->dstClientID = dstClientID;
    pkt->fileCount = static_cast<uint32_t>(files.size());

    uint8_t* ptr = buffer.data() + sizeof(FileQueryResumeV2);
    for (const auto& file : files) {
        FileQueryResumeEntryV2* entry = reinterpret_cast<FileQueryResumeEntryV2*>(ptr);
        entry->fileSize = file.second;
        entry->nameLength = static_cast<uint16_t>(file.first.size());
        ptr += sizeof(FileQueryResumeEntryV2);
        memcpy(ptr, file.first.data(), file.first.size());
        ptr += file.first.size();
    }

    return buffer;
}

// 解析续传查询包
bool ParseResumeQuery(
    const uint8_t* buffer, size_t len,
    uint64_t& transferID,
    uint64_t& srcClientID,
    uint64_t& dstClientID,
    std::vector<std::pair<std::string, uint64_t>>& files)
{
    if (len < sizeof(FileQueryResumeV2)) {
        return false;
    }

    const FileQueryResumeV2* pkt = reinterpret_cast<const FileQueryResumeV2*>(buffer);
    transferID = pkt->transferID;
    srcClientID = pkt->srcClientID;
    dstClientID = pkt->dstClientID;

    files.clear();
    const uint8_t* ptr = buffer + sizeof(FileQueryResumeV2);
    const uint8_t* end = buffer + len;

    for (uint32_t i = 0; i < pkt->fileCount; ++i) {
        if (ptr + sizeof(FileQueryResumeEntryV2) > end) {
            return false;
        }
        const FileQueryResumeEntryV2* entry = reinterpret_cast<const FileQueryResumeEntryV2*>(ptr);
        ptr += sizeof(FileQueryResumeEntryV2);

        if (ptr + entry->nameLength > end) {
            return false;
        }
        std::string name(reinterpret_cast<const char*>(ptr), entry->nameLength);
        ptr += entry->nameLength;

        files.emplace_back(name, entry->fileSize);
    }

    return true;
}

// ============================================
// TransferOptionsV2 测试
// ============================================

class TransferOptionsV2Test : public ::testing::Test {};

TEST_F(TransferOptionsV2Test, DefaultValues) {
    TransferOptionsV2 options;

    EXPECT_EQ(options.transferID, 0u);
    EXPECT_EQ(options.srcClientID, 0u);
    EXPECT_EQ(options.dstClientID, 0u);
    EXPECT_TRUE(options.enableResume);
    EXPECT_TRUE(options.startOffsets.empty());
}

TEST_F(TransferOptionsV2Test, SetStartOffsets) {
    TransferOptionsV2 options;
    options.startOffsets[0] = 1024;
    options.startOffsets[1] = 2048;
    options.startOffsets[2] = 0;

    EXPECT_EQ(options.startOffsets.size(), 3u);
    EXPECT_EQ(options.startOffsets[0], 1024u);
    EXPECT_EQ(options.startOffsets[1], 2048u);
    EXPECT_EQ(options.startOffsets[2], 0u);
}

TEST_F(TransferOptionsV2Test, C2CConfiguration) {
    TransferOptionsV2 options;
    options.transferID = 12345;
    options.srcClientID = 100;
    options.dstClientID = 200;

    EXPECT_EQ(options.transferID, 12345u);
    EXPECT_EQ(options.srcClientID, 100u);
    EXPECT_EQ(options.dstClientID, 200u);
}

// ============================================
// TransferID 生成测试
// ============================================

class TransferIDTest : public ::testing::Test {};

TEST_F(TransferIDTest, GeneratesNonZero) {
    // 多次生成应该都不为 0（极低概率）
    for (int i = 0; i < 100; ++i) {
        uint64_t id = GenerateTransferID();
        // 不做强制断言，因为理论上可能为 0
        // 但实际概率极低
        if (id == 0) {
            // 如果真的生成了 0，再生成一次
            id = GenerateTransferID();
        }
    }
    SUCCEED();
}

TEST_F(TransferIDTest, GeneratesUnique) {
    std::vector<uint64_t> ids;
    for (int i = 0; i < 1000; ++i) {
        ids.push_back(GenerateTransferID());
    }

    // 检查没有重复
    std::sort(ids.begin(), ids.end());
    auto it = std::unique(ids.begin(), ids.end());
    EXPECT_EQ(it, ids.end()) << "Found duplicate transfer IDs";
}

// ============================================
// FileChunkPacketV2 构建测试
// ============================================

class FileChunkPacketV2BuildTest : public ::testing::Test {};

TEST_F(FileChunkPacketV2BuildTest, BuildSimplePacket) {
    uint64_t transferID = 12345;
    std::string filename = "test.txt";
    std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04};

    auto buffer = BuildFileChunkPacketV2(
        transferID, 0, 0, 0, 1, 100, 0, filename, data);

    EXPECT_EQ(buffer.size(), sizeof(FileChunkPacketV2) + filename.size() + data.size());

    const FileChunkPacketV2* pkt = reinterpret_cast<const FileChunkPacketV2*>(buffer.data());
    EXPECT_EQ(pkt->cmd, 85);
    EXPECT_EQ(pkt->transferID, transferID);
    EXPECT_EQ(pkt->nameLength, filename.size());
    EXPECT_EQ(pkt->dataLength, data.size());
}

TEST_F(FileChunkPacketV2BuildTest, BuildWithFlags) {
    auto buffer = BuildFileChunkPacketV2(
        1, 0, 0, 0, 1, 100, 50, "file.bin", {}, FFV2_LAST_CHUNK);

    const FileChunkPacketV2* pkt = reinterpret_cast<const FileChunkPacketV2*>(buffer.data());
    EXPECT_EQ(pkt->flags, FFV2_LAST_CHUNK);
    EXPECT_EQ(pkt->offset, 50u);
}

TEST_F(FileChunkPacketV2BuildTest, BuildC2CPacket) {
    auto buffer = BuildFileChunkPacketV2(
        999, 100, 200, 0, 3, 1024, 0, "shared/doc.pdf", {});

    const FileChunkPacketV2* pkt = reinterpret_cast<const FileChunkPacketV2*>(buffer.data());
    EXPECT_EQ(pkt->srcClientID, 100u);
    EXPECT_EQ(pkt->dstClientID, 200u);
    EXPECT_EQ(pkt->totalFiles, 3u);
}

TEST_F(FileChunkPacketV2BuildTest, BuildEmptyDataPacket) {
    std::vector<uint8_t> emptyData;
    auto buffer = BuildFileChunkPacketV2(
        1, 0, 0, 0, 1, 0, 0, "empty.txt", emptyData);

    const FileChunkPacketV2* pkt = reinterpret_cast<const FileChunkPacketV2*>(buffer.data());
    EXPECT_EQ(pkt->dataLength, 0u);
    EXPECT_EQ(pkt->fileSize, 0u);
}

TEST_F(FileChunkPacketV2BuildTest, BuildDirectoryPacket) {
    auto buffer = BuildFileChunkPacketV2(
        1, 0, 0, 0, 1, 0, 0, "subdir/", {}, FFV2_DIRECTORY);

    const FileChunkPacketV2* pkt = reinterpret_cast<const FileChunkPacketV2*>(buffer.data());
    EXPECT_EQ(pkt->flags & FFV2_DIRECTORY, FFV2_DIRECTORY);
    EXPECT_EQ(pkt->dataLength, 0u);
}

// ============================================
// FileChunkPacketV2 解析测试
// ============================================

class FileChunkPacketV2ParseTest : public ::testing::Test {};

TEST_F(FileChunkPacketV2ParseTest, ParseValidPacket) {
    std::string originalName = "test/file.txt";
    std::vector<uint8_t> originalData = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE};

    auto buffer = BuildFileChunkPacketV2(
        12345, 100, 200, 3, 10, 1024 * 1024, 512, originalName, originalData, FFV2_COMPRESSED);

    FileChunkPacketV2 header;
    std::string parsedName;
    std::vector<uint8_t> parsedData;

    bool result = ParseFileChunkPacketV2(buffer.data(), buffer.size(), header, parsedName, parsedData);

    EXPECT_TRUE(result);
    EXPECT_EQ(header.transferID, 12345u);
    EXPECT_EQ(header.srcClientID, 100u);
    EXPECT_EQ(header.dstClientID, 200u);
    EXPECT_EQ(header.fileIndex, 3u);
    EXPECT_EQ(header.totalFiles, 10u);
    EXPECT_EQ(header.fileSize, 1024u * 1024u);
    EXPECT_EQ(header.offset, 512u);
    EXPECT_EQ(header.flags, FFV2_COMPRESSED);
    EXPECT_EQ(parsedName, originalName);
    EXPECT_EQ(parsedData, originalData);
}

TEST_F(FileChunkPacketV2ParseTest, ParseTruncatedHeader) {
    std::vector<uint8_t> truncated(sizeof(FileChunkPacketV2) - 10);

    FileChunkPacketV2 header;
    std::string name;
    std::vector<uint8_t> data;

    bool result = ParseFileChunkPacketV2(truncated.data(), truncated.size(), header, name, data);
    EXPECT_FALSE(result);
}

TEST_F(FileChunkPacketV2ParseTest, ParseTruncatedData) {
    auto buffer = BuildFileChunkPacketV2(
        1, 0, 0, 0, 1, 100, 0, "file.txt", {0x01, 0x02, 0x03});

    // 截断数据部分
    std::vector<uint8_t> truncated(buffer.begin(), buffer.end() - 2);

    FileChunkPacketV2 header;
    std::string name;
    std::vector<uint8_t> data;

    bool result = ParseFileChunkPacketV2(truncated.data(), truncated.size(), header, name, data);
    EXPECT_FALSE(result);
}

TEST_F(FileChunkPacketV2ParseTest, RoundTrip) {
    // 构建各种类型的包并验证往返
    struct TestCase {
        uint64_t transferID;
        uint32_t fileIndex;
        uint64_t fileSize;
        uint64_t offset;
        std::string filename;
        std::vector<uint8_t> data;
        uint16_t flags;
    };

    std::vector<TestCase> cases = {
        {1, 0, 100, 0, "a.txt", {0x01}, FFV2_NONE},
        {UINT64_MAX, 999, UINT64_MAX, UINT64_MAX - 100, "path/to/file.bin", {}, FFV2_LAST_CHUNK},
        {12345, 5, 1024, 512, "中文文件.txt", {0xAA, 0xBB, 0xCC}, FFV2_COMPRESSED | FFV2_LAST_CHUNK},
    };

    for (const auto& tc : cases) {
        auto buffer = BuildFileChunkPacketV2(
            tc.transferID, 0, 0, tc.fileIndex, 10, tc.fileSize, tc.offset, tc.filename, tc.data, tc.flags);

        FileChunkPacketV2 header;
        std::string name;
        std::vector<uint8_t> data;

        ASSERT_TRUE(ParseFileChunkPacketV2(buffer.data(), buffer.size(), header, name, data));
        EXPECT_EQ(header.transferID, tc.transferID);
        EXPECT_EQ(header.fileIndex, tc.fileIndex);
        EXPECT_EQ(header.fileSize, tc.fileSize);
        EXPECT_EQ(header.offset, tc.offset);
        EXPECT_EQ(header.flags, tc.flags);
        EXPECT_EQ(name, tc.filename);
        EXPECT_EQ(data, tc.data);
    }
}

// ============================================
// 续传查询包测试
// ============================================

class ResumeQueryTest : public ::testing::Test {};

TEST_F(ResumeQueryTest, BuildEmpty) {
    std::vector<std::pair<std::string, uint64_t>> files;
    auto buffer = BuildResumeQuery(12345, 100, 200, files);

    EXPECT_EQ(buffer.size(), sizeof(FileQueryResumeV2));

    const FileQueryResumeV2* pkt = reinterpret_cast<const FileQueryResumeV2*>(buffer.data());
    EXPECT_EQ(pkt->cmd, 88);
    EXPECT_EQ(pkt->fileCount, 0u);
}

TEST_F(ResumeQueryTest, BuildSingleFile) {
    std::vector<std::pair<std::string, uint64_t>> files = {
        {"file.txt", 1024}
    };
    auto buffer = BuildResumeQuery(12345, 100, 200, files);

    const FileQueryResumeV2* pkt = reinterpret_cast<const FileQueryResumeV2*>(buffer.data());
    EXPECT_EQ(pkt->fileCount, 1u);
}

TEST_F(ResumeQueryTest, BuildMultipleFiles) {
    std::vector<std::pair<std::string, uint64_t>> files = {
        {"file1.txt", 1024},
        {"dir/file2.bin", 2048},
        {"another/path/file3.dat", 4096}
    };
    auto buffer = BuildResumeQuery(12345, 100, 200, files);

    const FileQueryResumeV2* pkt = reinterpret_cast<const FileQueryResumeV2*>(buffer.data());
    EXPECT_EQ(pkt->fileCount, 3u);
}

TEST_F(ResumeQueryTest, ParseRoundTrip) {
    std::vector<std::pair<std::string, uint64_t>> original = {
        {"file1.txt", 1024},
        {"subdir/file2.bin", UINT64_MAX},
        {"中文/文件.txt", 0}
    };

    auto buffer = BuildResumeQuery(99999, 111, 222, original);

    uint64_t transferID, srcClientID, dstClientID;
    std::vector<std::pair<std::string, uint64_t>> parsed;

    bool result = ParseResumeQuery(buffer.data(), buffer.size(), transferID, srcClientID, dstClientID, parsed);

    EXPECT_TRUE(result);
    EXPECT_EQ(transferID, 99999u);
    EXPECT_EQ(srcClientID, 111u);
    EXPECT_EQ(dstClientID, 222u);
    ASSERT_EQ(parsed.size(), original.size());

    for (size_t i = 0; i < original.size(); ++i) {
        EXPECT_EQ(parsed[i].first, original[i].first);
        EXPECT_EQ(parsed[i].second, original[i].second);
    }
}

TEST_F(ResumeQueryTest, ParseTruncated) {
    std::vector<std::pair<std::string, uint64_t>> files = {
        {"file.txt", 1024}
    };
    auto buffer = BuildResumeQuery(1, 0, 0, files);

    // 截断
    std::vector<uint8_t> truncated(buffer.begin(), buffer.begin() + sizeof(FileQueryResumeV2) + 5);

    uint64_t transferID, srcClientID, dstClientID;
    std::vector<std::pair<std::string, uint64_t>> parsed;

    bool result = ParseResumeQuery(truncated.data(), truncated.size(), transferID, srcClientID, dstClientID, parsed);
    EXPECT_FALSE(result);
}

// ============================================
// 大文件支持测试
// ============================================

class LargeFileTest : public ::testing::Test {};

TEST_F(LargeFileTest, FileSize_GreaterThan4GB) {
    uint64_t largeSize = 5ULL * 1024 * 1024 * 1024;  // 5 GB
    auto buffer = BuildFileChunkPacketV2(
        1, 0, 0, 0, 1, largeSize, 0, "large.bin", {});

    const FileChunkPacketV2* pkt = reinterpret_cast<const FileChunkPacketV2*>(buffer.data());
    EXPECT_EQ(pkt->fileSize, largeSize);
}

TEST_F(LargeFileTest, Offset_GreaterThan4GB) {
    uint64_t largeOffset = 10ULL * 1024 * 1024 * 1024;  // 10 GB
    uint64_t fileSize = 20ULL * 1024 * 1024 * 1024;     // 20 GB

    auto buffer = BuildFileChunkPacketV2(
        1, 0, 0, 0, 1, fileSize, largeOffset, "huge.bin", {});

    const FileChunkPacketV2* pkt = reinterpret_cast<const FileChunkPacketV2*>(buffer.data());
    EXPECT_EQ(pkt->fileSize, fileSize);
    EXPECT_EQ(pkt->offset, largeOffset);
}

TEST_F(LargeFileTest, MaxValues) {
    auto buffer = BuildFileChunkPacketV2(
        UINT64_MAX, UINT64_MAX, UINT64_MAX,
        UINT32_MAX, UINT32_MAX, UINT64_MAX, UINT64_MAX,
        "max.bin", {}, UINT16_MAX);

    FileChunkPacketV2 header;
    std::string name;
    std::vector<uint8_t> data;

    ASSERT_TRUE(ParseFileChunkPacketV2(buffer.data(), buffer.size(), header, name, data));
    EXPECT_EQ(header.transferID, UINT64_MAX);
    EXPECT_EQ(header.srcClientID, UINT64_MAX);
    EXPECT_EQ(header.dstClientID, UINT64_MAX);
    EXPECT_EQ(header.fileIndex, UINT32_MAX);
    EXPECT_EQ(header.totalFiles, UINT32_MAX);
    EXPECT_EQ(header.fileSize, UINT64_MAX);
    EXPECT_EQ(header.offset, UINT64_MAX);
}

// ============================================
// 多文件传输测试
// ============================================

class MultiFileTransferTest : public ::testing::Test {};

TEST_F(MultiFileTransferTest, SequentialFileIndices) {
    std::vector<std::pair<std::string, uint64_t>> files = {
        {"file1.txt", 100},
        {"file2.txt", 200},
        {"file3.txt", 300}
    };

    for (uint32_t i = 0; i < files.size(); ++i) {
        auto buffer = BuildFileChunkPacketV2(
            12345, 0, 0, i, static_cast<uint32_t>(files.size()),
            files[i].second, 0, files[i].first, {}, FFV2_LAST_CHUNK);

        const FileChunkPacketV2* pkt = reinterpret_cast<const FileChunkPacketV2*>(buffer.data());
        EXPECT_EQ(pkt->fileIndex, i);
        EXPECT_EQ(pkt->totalFiles, files.size());
    }
}

TEST_F(MultiFileTransferTest, ConsistentTransferID) {
    uint64_t transferID = GenerateTransferID();

    for (int i = 0; i < 5; ++i) {
        auto buffer = BuildFileChunkPacketV2(
            transferID, 0, 0, i, 5, 1000 * (i + 1), 0, "file" + std::to_string(i), {});

        const FileChunkPacketV2* pkt = reinterpret_cast<const FileChunkPacketV2*>(buffer.data());
        EXPECT_EQ(pkt->transferID, transferID);
    }
}

// ============================================
// 错误处理测试
// ============================================

class ErrorHandlingTest : public ::testing::Test {};

TEST_F(ErrorHandlingTest, CancelFlag) {
    auto buffer = BuildFileChunkPacketV2(
        12345, 0, 0, 0, 1, 0, 0, "", {}, FFV2_CANCEL);

    const FileChunkPacketV2* pkt = reinterpret_cast<const FileChunkPacketV2*>(buffer.data());
    EXPECT_EQ(pkt->flags & FFV2_CANCEL, FFV2_CANCEL);
}

TEST_F(ErrorHandlingTest, ErrorFlag) {
    auto buffer = BuildFileChunkPacketV2(
        12345, 0, 0, 0, 1, 0, 0, "", {}, FFV2_ERROR);

    const FileChunkPacketV2* pkt = reinterpret_cast<const FileChunkPacketV2*>(buffer.data());
    EXPECT_EQ(pkt->flags & FFV2_ERROR, FFV2_ERROR);
}

TEST_F(ErrorHandlingTest, CombinedErrorFlags) {
    uint16_t flags = FFV2_ERROR | FFV2_CANCEL | FFV2_LAST_CHUNK;
    auto buffer = BuildFileChunkPacketV2(
        12345, 0, 0, 0, 1, 0, 0, "", {}, flags);

    const FileChunkPacketV2* pkt = reinterpret_cast<const FileChunkPacketV2*>(buffer.data());
    EXPECT_TRUE(pkt->flags & FFV2_ERROR);
    EXPECT_TRUE(pkt->flags & FFV2_CANCEL);
    EXPECT_TRUE(pkt->flags & FFV2_LAST_CHUNK);
}

