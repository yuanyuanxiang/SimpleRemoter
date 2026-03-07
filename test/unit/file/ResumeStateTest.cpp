/**
 * @file ResumeStateTest.cpp
 * @brief 断点续传状态管理测试
 *
 * 测试覆盖：
 * - 续传状态序列化/反序列化
 * - 续传请求/响应包构建
 * - 状态文件格式
 * - 多文件续传管理
 */

#include <gtest/gtest.h>
#include <cstring>
#include <vector>
#include <map>
#include <string>
#include <cstdint>

// ============================================
// 协议结构（测试专用）
// ============================================

#pragma pack(push, 1)

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

enum FileFlagsV2 : uint16_t {
    FFV2_NONE         = 0x0000,
    FFV2_RESUME_REQ   = 0x0002,
    FFV2_RESUME_RESP  = 0x0004,
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

// ============================================
// 续传状态管理类（测试专用实现）
// ============================================

struct FileResumeEntry {
    uint32_t fileIndex;
    uint64_t fileSize;
    uint64_t receivedBytes;
    std::string relativePath;
    std::vector<std::pair<uint64_t, uint64_t>> receivedRanges;
};

class ResumeStateManager {
public:
    ResumeStateManager() : m_transferID(0), m_srcClientID(0), m_dstClientID(0) {}

    void Initialize(uint64_t transferID, uint64_t srcClientID, uint64_t dstClientID,
                   const std::string& targetDir) {
        m_transferID = transferID;
        m_srcClientID = srcClientID;
        m_dstClientID = dstClientID;
        m_targetDir = targetDir;
        m_entries.clear();
    }

    void AddFile(uint32_t fileIndex, uint64_t fileSize, const std::string& path) {
        FileResumeEntry entry;
        entry.fileIndex = fileIndex;
        entry.fileSize = fileSize;
        entry.receivedBytes = 0;
        entry.relativePath = path;
        m_entries.push_back(entry);
    }

    void UpdateProgress(uint32_t fileIndex, uint64_t offset, uint64_t length) {
        for (auto& entry : m_entries) {
            if (entry.fileIndex == fileIndex) {
                entry.receivedRanges.emplace_back(offset, length);
                entry.receivedBytes += length;
                break;
            }
        }
    }

    bool GetFileState(uint32_t fileIndex, FileResumeEntry& outEntry) const {
        for (const auto& entry : m_entries) {
            if (entry.fileIndex == fileIndex) {
                outEntry = entry;
                return true;
            }
        }
        return false;
    }

    // 序列化为字节流
    std::vector<uint8_t> Serialize() const {
        std::vector<uint8_t> buffer;

        // Header
        auto appendU64 = [&buffer](uint64_t val) {
            for (int i = 0; i < 8; ++i) {
                buffer.push_back(static_cast<uint8_t>(val >> (i * 8)));
            }
        };
        auto appendU32 = [&buffer](uint32_t val) {
            for (int i = 0; i < 4; ++i) {
                buffer.push_back(static_cast<uint8_t>(val >> (i * 8)));
            }
        };
        auto appendU16 = [&buffer](uint16_t val) {
            buffer.push_back(static_cast<uint8_t>(val & 0xFF));
            buffer.push_back(static_cast<uint8_t>(val >> 8));
        };
        auto appendString = [&buffer, &appendU16](const std::string& str) {
            appendU16(static_cast<uint16_t>(str.size()));
            buffer.insert(buffer.end(), str.begin(), str.end());
        };

        // Magic
        buffer.push_back('R');
        buffer.push_back('S');
        buffer.push_back('T');
        buffer.push_back('V');  // Resume State V2

        appendU64(m_transferID);
        appendU64(m_srcClientID);
        appendU64(m_dstClientID);
        appendString(m_targetDir);
        appendU32(static_cast<uint32_t>(m_entries.size()));

        for (const auto& entry : m_entries) {
            appendU32(entry.fileIndex);
            appendU64(entry.fileSize);
            appendU64(entry.receivedBytes);
            appendString(entry.relativePath);
            appendU16(static_cast<uint16_t>(entry.receivedRanges.size()));
            for (const auto& range : entry.receivedRanges) {
                appendU64(range.first);
                appendU64(range.second);
            }
        }

        return buffer;
    }

    // 从字节流反序列化
    bool Deserialize(const std::vector<uint8_t>& buffer) {
        if (buffer.size() < 8) return false;

        size_t pos = 0;

        auto readU64 = [&buffer, &pos]() -> uint64_t {
            if (pos + 8 > buffer.size()) return 0;
            uint64_t val = 0;
            for (int i = 0; i < 8; ++i) {
                val |= static_cast<uint64_t>(buffer[pos++]) << (i * 8);
            }
            return val;
        };
        auto readU32 = [&buffer, &pos]() -> uint32_t {
            if (pos + 4 > buffer.size()) return 0;
            uint32_t val = 0;
            for (int i = 0; i < 4; ++i) {
                val |= static_cast<uint32_t>(buffer[pos++]) << (i * 8);
            }
            return val;
        };
        auto readU16 = [&buffer, &pos]() -> uint16_t {
            if (pos + 2 > buffer.size()) return 0;
            uint16_t val = buffer[pos] | (buffer[pos + 1] << 8);
            pos += 2;
            return val;
        };
        auto readString = [&buffer, &pos, &readU16]() -> std::string {
            uint16_t len = readU16();
            if (pos + len > buffer.size()) return "";
            std::string str(buffer.begin() + pos, buffer.begin() + pos + len);
            pos += len;
            return str;
        };

        // Check magic
        if (buffer[0] != 'R' || buffer[1] != 'S' || buffer[2] != 'T' || buffer[3] != 'V') {
            return false;
        }
        pos = 4;

        m_transferID = readU64();
        m_srcClientID = readU64();
        m_dstClientID = readU64();
        m_targetDir = readString();
        uint32_t entryCount = readU32();

        m_entries.clear();
        for (uint32_t i = 0; i < entryCount; ++i) {
            FileResumeEntry entry;
            entry.fileIndex = readU32();
            entry.fileSize = readU64();
            entry.receivedBytes = readU64();
            entry.relativePath = readString();
            uint16_t rangeCount = readU16();
            for (uint16_t j = 0; j < rangeCount; ++j) {
                uint64_t offset = readU64();
                uint64_t length = readU64();
                entry.receivedRanges.emplace_back(offset, length);
            }
            m_entries.push_back(entry);
        }

        return true;
    }

    uint64_t GetTransferID() const { return m_transferID; }
    uint64_t GetSrcClientID() const { return m_srcClientID; }
    uint64_t GetDstClientID() const { return m_dstClientID; }
    const std::string& GetTargetDir() const { return m_targetDir; }
    size_t GetFileCount() const { return m_entries.size(); }

    // 获取所有文件的接收偏移映射
    std::map<uint32_t, uint64_t> GetReceivedOffsets() const {
        std::map<uint32_t, uint64_t> offsets;
        for (const auto& entry : m_entries) {
            offsets[entry.fileIndex] = entry.receivedBytes;
        }
        return offsets;
    }

private:
    uint64_t m_transferID;
    uint64_t m_srcClientID;
    uint64_t m_dstClientID;
    std::string m_targetDir;
    std::vector<FileResumeEntry> m_entries;
};

// ============================================
// 续传包构建/解析辅助函数
// ============================================

std::vector<uint8_t> BuildResumeRequest(
    uint64_t transferID,
    uint64_t srcClientID,
    uint64_t dstClientID,
    uint32_t fileIndex,
    uint64_t fileSize,
    uint64_t receivedBytes,
    const std::vector<std::pair<uint64_t, uint64_t>>& ranges)
{
    size_t size = sizeof(FileResumePacketV2) + ranges.size() * sizeof(FileRangeV2);
    std::vector<uint8_t> buffer(size);

    FileResumePacketV2* pkt = reinterpret_cast<FileResumePacketV2*>(buffer.data());
    pkt->cmd = 86;  // COMMAND_FILE_RESUME
    pkt->transferID = transferID;
    pkt->srcClientID = srcClientID;
    pkt->dstClientID = dstClientID;
    pkt->fileIndex = fileIndex;
    pkt->fileSize = fileSize;
    pkt->receivedBytes = receivedBytes;
    pkt->flags = FFV2_RESUME_REQ;
    pkt->rangeCount = static_cast<uint16_t>(ranges.size());

    FileRangeV2* rangePtr = reinterpret_cast<FileRangeV2*>(buffer.data() + sizeof(FileResumePacketV2));
    for (size_t i = 0; i < ranges.size(); ++i) {
        rangePtr[i].offset = ranges[i].first;
        rangePtr[i].length = ranges[i].second;
    }

    return buffer;
}

bool ParseResumeRequest(
    const uint8_t* buffer, size_t len,
    FileResumePacketV2& header,
    std::vector<std::pair<uint64_t, uint64_t>>& ranges)
{
    if (len < sizeof(FileResumePacketV2)) {
        return false;
    }

    memcpy(&header, buffer, sizeof(FileResumePacketV2));

    size_t expectedLen = sizeof(FileResumePacketV2) + header.rangeCount * sizeof(FileRangeV2);
    if (len < expectedLen) {
        return false;
    }

    ranges.clear();
    const FileRangeV2* rangePtr = reinterpret_cast<const FileRangeV2*>(buffer + sizeof(FileResumePacketV2));
    for (uint16_t i = 0; i < header.rangeCount; ++i) {
        ranges.emplace_back(rangePtr[i].offset, rangePtr[i].length);
    }

    return true;
}

std::vector<uint8_t> BuildResumeResponse(
    uint64_t srcClientID,
    uint64_t dstClientID,
    const std::map<uint32_t, uint64_t>& offsets)
{
    size_t size = sizeof(FileResumeResponseV2) + offsets.size() * sizeof(FileResumeResponseEntryV2);
    std::vector<uint8_t> buffer(size);

    FileResumeResponseV2* pkt = reinterpret_cast<FileResumeResponseV2*>(buffer.data());
    pkt->cmd = 86;  // COMMAND_FILE_RESUME
    pkt->srcClientID = srcClientID;
    pkt->dstClientID = dstClientID;
    pkt->flags = FFV2_RESUME_RESP;
    pkt->fileCount = static_cast<uint32_t>(offsets.size());

    FileResumeResponseEntryV2* entryPtr = reinterpret_cast<FileResumeResponseEntryV2*>(
        buffer.data() + sizeof(FileResumeResponseV2));

    size_t i = 0;
    for (const auto& kv : offsets) {
        entryPtr[i].fileIndex = kv.first;
        entryPtr[i].receivedBytes = kv.second;
        ++i;
    }

    return buffer;
}

bool ParseResumeResponse(
    const uint8_t* buffer, size_t len,
    std::map<uint32_t, uint64_t>& offsets)
{
    if (len < sizeof(FileResumeResponseV2)) {
        return false;
    }

    const FileResumeResponseV2* pkt = reinterpret_cast<const FileResumeResponseV2*>(buffer);

    if ((pkt->flags & FFV2_RESUME_RESP) == 0) {
        return false;
    }

    size_t expectedLen = sizeof(FileResumeResponseV2) + pkt->fileCount * sizeof(FileResumeResponseEntryV2);
    if (len < expectedLen) {
        return false;
    }

    offsets.clear();
    const FileResumeResponseEntryV2* entryPtr = reinterpret_cast<const FileResumeResponseEntryV2*>(
        buffer + sizeof(FileResumeResponseV2));

    for (uint32_t i = 0; i < pkt->fileCount; ++i) {
        offsets[entryPtr[i].fileIndex] = entryPtr[i].receivedBytes;
    }

    return true;
}

// ============================================
// ResumeStateManager 测试
// ============================================

class ResumeStateManagerTest : public ::testing::Test {};

TEST_F(ResumeStateManagerTest, Initialize) {
    ResumeStateManager mgr;
    mgr.Initialize(12345, 100, 200, "C:\\Downloads\\");

    EXPECT_EQ(mgr.GetTransferID(), 12345u);
    EXPECT_EQ(mgr.GetSrcClientID(), 100u);
    EXPECT_EQ(mgr.GetDstClientID(), 200u);
    EXPECT_EQ(mgr.GetTargetDir(), "C:\\Downloads\\");
    EXPECT_EQ(mgr.GetFileCount(), 0u);
}

TEST_F(ResumeStateManagerTest, AddFiles) {
    ResumeStateManager mgr;
    mgr.Initialize(1, 0, 0, "");
    mgr.AddFile(0, 1000, "file1.txt");
    mgr.AddFile(1, 2000, "subdir/file2.bin");
    mgr.AddFile(2, 3000, "another/path/file3.dat");

    EXPECT_EQ(mgr.GetFileCount(), 3u);

    FileResumeEntry entry;
    ASSERT_TRUE(mgr.GetFileState(1, entry));
    EXPECT_EQ(entry.fileSize, 2000u);
    EXPECT_EQ(entry.relativePath, "subdir/file2.bin");
}

TEST_F(ResumeStateManagerTest, UpdateProgress) {
    ResumeStateManager mgr;
    mgr.Initialize(1, 0, 0, "");
    mgr.AddFile(0, 10000, "file.bin");

    mgr.UpdateProgress(0, 0, 2000);
    mgr.UpdateProgress(0, 2000, 3000);

    FileResumeEntry entry;
    ASSERT_TRUE(mgr.GetFileState(0, entry));
    EXPECT_EQ(entry.receivedBytes, 5000u);
    EXPECT_EQ(entry.receivedRanges.size(), 2u);
}

TEST_F(ResumeStateManagerTest, GetReceivedOffsets) {
    ResumeStateManager mgr;
    mgr.Initialize(1, 0, 0, "");
    mgr.AddFile(0, 1000, "a.txt");
    mgr.AddFile(1, 2000, "b.txt");
    mgr.AddFile(2, 3000, "c.txt");

    mgr.UpdateProgress(0, 0, 500);
    mgr.UpdateProgress(1, 0, 1500);
    mgr.UpdateProgress(2, 0, 2500);

    auto offsets = mgr.GetReceivedOffsets();
    EXPECT_EQ(offsets.size(), 3u);
    EXPECT_EQ(offsets[0], 500u);
    EXPECT_EQ(offsets[1], 1500u);
    EXPECT_EQ(offsets[2], 2500u);
}

// ============================================
// 序列化/反序列化测试
// ============================================

class ResumeSerializationTest : public ::testing::Test {};

TEST_F(ResumeSerializationTest, EmptyState) {
    ResumeStateManager mgr1, mgr2;
    mgr1.Initialize(12345, 100, 200, "C:\\Target\\");

    auto buffer = mgr1.Serialize();
    ASSERT_TRUE(mgr2.Deserialize(buffer));

    EXPECT_EQ(mgr2.GetTransferID(), 12345u);
    EXPECT_EQ(mgr2.GetSrcClientID(), 100u);
    EXPECT_EQ(mgr2.GetDstClientID(), 200u);
    EXPECT_EQ(mgr2.GetTargetDir(), "C:\\Target\\");
    EXPECT_EQ(mgr2.GetFileCount(), 0u);
}

TEST_F(ResumeSerializationTest, SingleFile) {
    ResumeStateManager mgr1, mgr2;
    mgr1.Initialize(1, 0, 0, "/tmp/download/");
    mgr1.AddFile(0, 10000, "test.bin");
    mgr1.UpdateProgress(0, 0, 5000);

    auto buffer = mgr1.Serialize();
    ASSERT_TRUE(mgr2.Deserialize(buffer));

    EXPECT_EQ(mgr2.GetFileCount(), 1u);

    FileResumeEntry entry;
    ASSERT_TRUE(mgr2.GetFileState(0, entry));
    EXPECT_EQ(entry.fileSize, 10000u);
    EXPECT_EQ(entry.receivedBytes, 5000u);
    EXPECT_EQ(entry.relativePath, "test.bin");
}

TEST_F(ResumeSerializationTest, MultipleFiles) {
    ResumeStateManager mgr1, mgr2;
    mgr1.Initialize(99999, 111, 222, "D:\\Backup\\");
    mgr1.AddFile(0, 1000, "file1.txt");
    mgr1.AddFile(1, 2000, "dir/file2.bin");
    mgr1.AddFile(2, 3000, "path/to/file3.dat");

    mgr1.UpdateProgress(0, 0, 1000);  // 完成
    mgr1.UpdateProgress(1, 0, 500);
    mgr1.UpdateProgress(1, 500, 500);
    mgr1.UpdateProgress(2, 0, 1000);
    mgr1.UpdateProgress(2, 2000, 500);

    auto buffer = mgr1.Serialize();
    ASSERT_TRUE(mgr2.Deserialize(buffer));

    EXPECT_EQ(mgr2.GetFileCount(), 3u);

    FileResumeEntry entry;

    ASSERT_TRUE(mgr2.GetFileState(0, entry));
    EXPECT_EQ(entry.receivedBytes, 1000u);

    ASSERT_TRUE(mgr2.GetFileState(1, entry));
    EXPECT_EQ(entry.receivedBytes, 1000u);
    EXPECT_EQ(entry.receivedRanges.size(), 2u);

    ASSERT_TRUE(mgr2.GetFileState(2, entry));
    EXPECT_EQ(entry.receivedBytes, 1500u);
    EXPECT_EQ(entry.receivedRanges.size(), 2u);
}

TEST_F(ResumeSerializationTest, InvalidMagic) {
    std::vector<uint8_t> invalidBuffer = {'X', 'X', 'X', 'X', 0, 0, 0, 0};
    ResumeStateManager mgr;
    EXPECT_FALSE(mgr.Deserialize(invalidBuffer));
}

TEST_F(ResumeSerializationTest, TruncatedBuffer) {
    ResumeStateManager mgr1;
    mgr1.Initialize(1, 0, 0, "test");
    mgr1.AddFile(0, 1000, "file.txt");

    auto buffer = mgr1.Serialize();

    // 截断
    std::vector<uint8_t> truncated(buffer.begin(), buffer.begin() + 10);

    ResumeStateManager mgr2;
    // 可能成功也可能失败，取决于截断位置
    // 主要验证不会崩溃
    mgr2.Deserialize(truncated);
    SUCCEED();
}

// ============================================
// 续传请求/响应包测试
// ============================================

class ResumePacketTest : public ::testing::Test {};

TEST_F(ResumePacketTest, BuildResumeRequest_NoRanges) {
    std::vector<std::pair<uint64_t, uint64_t>> ranges;
    auto buffer = BuildResumeRequest(12345, 100, 200, 5, 10000, 0, ranges);

    EXPECT_EQ(buffer.size(), sizeof(FileResumePacketV2));

    const FileResumePacketV2* pkt = reinterpret_cast<const FileResumePacketV2*>(buffer.data());
    EXPECT_EQ(pkt->cmd, 86);
    EXPECT_EQ(pkt->flags, FFV2_RESUME_REQ);
    EXPECT_EQ(pkt->rangeCount, 0);
}

TEST_F(ResumePacketTest, BuildResumeRequest_WithRanges) {
    std::vector<std::pair<uint64_t, uint64_t>> ranges = {
        {0, 1000},
        {2000, 500},
        {5000, 2000}
    };
    auto buffer = BuildResumeRequest(1, 0, 0, 0, 10000, 3500, ranges);

    FileResumePacketV2 header;
    std::vector<std::pair<uint64_t, uint64_t>> parsedRanges;

    ASSERT_TRUE(ParseResumeRequest(buffer.data(), buffer.size(), header, parsedRanges));
    EXPECT_EQ(header.fileSize, 10000u);
    EXPECT_EQ(header.receivedBytes, 3500u);
    ASSERT_EQ(parsedRanges.size(), 3u);
    EXPECT_EQ(parsedRanges[0].first, 0u);
    EXPECT_EQ(parsedRanges[0].second, 1000u);
    EXPECT_EQ(parsedRanges[2].first, 5000u);
}

TEST_F(ResumePacketTest, BuildResumeResponse) {
    std::map<uint32_t, uint64_t> offsets = {
        {0, 1000},
        {1, 0},
        {2, 5000}
    };
    auto buffer = BuildResumeResponse(100, 200, offsets);

    std::map<uint32_t, uint64_t> parsedOffsets;
    ASSERT_TRUE(ParseResumeResponse(buffer.data(), buffer.size(), parsedOffsets));

    EXPECT_EQ(parsedOffsets.size(), 3u);
    EXPECT_EQ(parsedOffsets[0], 1000u);
    EXPECT_EQ(parsedOffsets[1], 0u);
    EXPECT_EQ(parsedOffsets[2], 5000u);
}

TEST_F(ResumePacketTest, ParseTruncatedRequest) {
    auto buffer = BuildResumeRequest(1, 0, 0, 0, 1000, 0, {{0, 500}});

    // 截断
    FileResumePacketV2 header;
    std::vector<std::pair<uint64_t, uint64_t>> ranges;

    EXPECT_FALSE(ParseResumeRequest(buffer.data(), sizeof(FileResumePacketV2) - 5, header, ranges));
}

TEST_F(ResumePacketTest, ParseTruncatedResponse) {
    std::map<uint32_t, uint64_t> offsets = {{0, 1000}};
    auto buffer = BuildResumeResponse(0, 0, offsets);

    // 截断
    std::map<uint32_t, uint64_t> parsedOffsets;
    EXPECT_FALSE(ParseResumeResponse(buffer.data(), sizeof(FileResumeResponseV2) - 5, parsedOffsets));
}

// ============================================
// 续传场景测试
// ============================================

class ResumeScenarioTest : public ::testing::Test {};

TEST_F(ResumeScenarioTest, SimulateInterruptAndResume) {
    // 第一次传输，接收了部分数据
    ResumeStateManager session1;
    session1.Initialize(12345, 100, 0, "C:\\Downloads\\");
    session1.AddFile(0, 10000, "large_file.bin");
    session1.AddFile(1, 5000, "small_file.txt");

    session1.UpdateProgress(0, 0, 3000);  // 30%
    session1.UpdateProgress(1, 0, 5000);  // 100%

    // 保存状态
    auto savedState = session1.Serialize();

    // 模拟程序重启，恢复状态
    ResumeStateManager session2;
    ASSERT_TRUE(session2.Deserialize(savedState));

    // 获取续传偏移
    auto offsets = session2.GetReceivedOffsets();
    EXPECT_EQ(offsets[0], 3000u);  // 从 3000 继续
    EXPECT_EQ(offsets[1], 5000u);  // 已完成

    // 继续传输
    session2.UpdateProgress(0, 3000, 7000);

    FileResumeEntry entry;
    ASSERT_TRUE(session2.GetFileState(0, entry));
    EXPECT_EQ(entry.receivedBytes, 10000u);
}

TEST_F(ResumeScenarioTest, C2CResumeNegotiation) {
    // 源客户端查询续传状态
    std::vector<std::pair<std::string, uint64_t>> files = {
        {"file1.txt", 1000},
        {"file2.txt", 2000},
        {"file3.txt", 3000}
    };

    // 目标客户端已有部分数据
    std::map<uint32_t, uint64_t> receivedOffsets = {
        {0, 500},   // file1: 50%
        {1, 2000},  // file2: 100%
        {2, 0}      // file3: 0%
    };

    // 构建响应
    auto response = BuildResumeResponse(100, 200, receivedOffsets);

    // 源客户端解析响应
    std::map<uint32_t, uint64_t> parsedOffsets;
    ASSERT_TRUE(ParseResumeResponse(response.data(), response.size(), parsedOffsets));

    // 根据偏移决定发送策略
    for (size_t i = 0; i < files.size(); ++i) {
        uint32_t fileIndex = static_cast<uint32_t>(i);
        uint64_t startOffset = parsedOffsets[fileIndex];

        if (startOffset >= files[i].second) {
            // 已完成，跳过
            EXPECT_EQ(i, 1u) << "Only file2 should be complete";
        } else if (startOffset > 0) {
            // 部分完成，从偏移继续
            EXPECT_EQ(i, 0u) << "Only file1 should be partial";
            EXPECT_EQ(startOffset, 500u);
        } else {
            // 未开始，从头发送
            EXPECT_EQ(i, 2u) << "Only file3 should start from beginning";
        }
    }
}

TEST_F(ResumeScenarioTest, LargeFileResume) {
    ResumeStateManager mgr;
    uint64_t fileSize = 10ULL * 1024 * 1024 * 1024;  // 10 GB
    mgr.Initialize(1, 0, 0, "/data/");
    mgr.AddFile(0, fileSize, "huge.bin");

    // 模拟接收了 5 GB
    mgr.UpdateProgress(0, 0, 5ULL * 1024 * 1024 * 1024);

    auto offsets = mgr.GetReceivedOffsets();
    EXPECT_EQ(offsets[0], 5ULL * 1024 * 1024 * 1024);

    // 序列化/反序列化
    auto buffer = mgr.Serialize();
    ResumeStateManager mgr2;
    ASSERT_TRUE(mgr2.Deserialize(buffer));

    FileResumeEntry entry;
    ASSERT_TRUE(mgr2.GetFileState(0, entry));
    EXPECT_EQ(entry.fileSize, 10ULL * 1024 * 1024 * 1024);
    EXPECT_EQ(entry.receivedBytes, 5ULL * 1024 * 1024 * 1024);
}

// ============================================
// 边界条件测试
// ============================================

class ResumeBoundaryTest : public ::testing::Test {};

TEST_F(ResumeBoundaryTest, EmptyFileName) {
    ResumeStateManager mgr;
    mgr.Initialize(1, 0, 0, "");
    mgr.AddFile(0, 100, "");

    auto buffer = mgr.Serialize();
    ResumeStateManager mgr2;
    ASSERT_TRUE(mgr2.Deserialize(buffer));

    FileResumeEntry entry;
    ASSERT_TRUE(mgr2.GetFileState(0, entry));
    EXPECT_EQ(entry.relativePath, "");
}

TEST_F(ResumeBoundaryTest, SpecialCharactersInPath) {
    ResumeStateManager mgr;
    mgr.Initialize(1, 0, 0, "C:\\My Files\\测试目录\\");
    mgr.AddFile(0, 100, "文件 (1).txt");

    auto buffer = mgr.Serialize();
    ResumeStateManager mgr2;
    ASSERT_TRUE(mgr2.Deserialize(buffer));

    EXPECT_EQ(mgr2.GetTargetDir(), "C:\\My Files\\测试目录\\");

    FileResumeEntry entry;
    ASSERT_TRUE(mgr2.GetFileState(0, entry));
    EXPECT_EQ(entry.relativePath, "文件 (1).txt");
}

TEST_F(ResumeBoundaryTest, ManyRanges) {
    ResumeStateManager mgr;
    mgr.Initialize(1, 0, 0, "");
    mgr.AddFile(0, 100000, "fragmented.bin");

    // 添加很多小区间
    for (uint64_t i = 0; i < 100000; i += 20) {
        mgr.UpdateProgress(0, i, 10);
    }

    auto buffer = mgr.Serialize();
    ResumeStateManager mgr2;
    ASSERT_TRUE(mgr2.Deserialize(buffer));

    FileResumeEntry entry;
    ASSERT_TRUE(mgr2.GetFileState(0, entry));
    EXPECT_EQ(entry.receivedRanges.size(), 5000u);
}

TEST_F(ResumeBoundaryTest, MaxFileCount) {
    ResumeStateManager mgr;
    mgr.Initialize(1, 0, 0, "");

    // 添加大量文件
    for (uint32_t i = 0; i < 1000; ++i) {
        mgr.AddFile(i, 100 * (i + 1), "file" + std::to_string(i) + ".txt");
    }

    auto buffer = mgr.Serialize();
    ResumeStateManager mgr2;
    ASSERT_TRUE(mgr2.Deserialize(buffer));

    EXPECT_EQ(mgr2.GetFileCount(), 1000u);
}

