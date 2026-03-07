/**
 * @file ChunkManagerTest.cpp
 * @brief 分块管理和区间跟踪测试
 *
 * 测试覆盖：
 * - FileRangeV2 区间操作
 * - 接收状态跟踪
 * - 区间合并算法
 * - 断点续传区间计算
 */

#include <gtest/gtest.h>
#include <vector>
#include <algorithm>
#include <cstdint>

// ============================================
// 区间结构（测试专用）
// ============================================

struct Range {
    uint64_t offset;
    uint64_t length;

    Range(uint64_t o = 0, uint64_t l = 0) : offset(o), length(l) {}

    uint64_t end() const { return offset + length; }

    bool operator<(const Range& other) const {
        return offset < other.offset;
    }

    bool operator==(const Range& other) const {
        return offset == other.offset && length == other.length;
    }
};

// ============================================
// 区间管理类（断点续传核心逻辑）
// ============================================

class RangeManager {
public:
    RangeManager(uint64_t fileSize = 0) : m_fileSize(fileSize), m_receivedBytes(0) {}

    // 添加已接收区间
    void AddRange(uint64_t offset, uint64_t length) {
        if (length == 0) return;

        Range newRange(offset, length);
        m_ranges.push_back(newRange);
        MergeRanges();
        UpdateReceivedBytes();
    }

    // 获取已接收区间列表
    const std::vector<Range>& GetRanges() const {
        return m_ranges;
    }

    // 获取缺失区间列表
    std::vector<Range> GetMissingRanges() const {
        std::vector<Range> missing;

        if (m_fileSize == 0) return missing;

        uint64_t currentPos = 0;
        for (const auto& r : m_ranges) {
            if (r.offset > currentPos) {
                missing.emplace_back(currentPos, r.offset - currentPos);
            }
            currentPos = r.end();
        }

        if (currentPos < m_fileSize) {
            missing.emplace_back(currentPos, m_fileSize - currentPos);
        }

        return missing;
    }

    // 获取已接收字节数
    uint64_t GetReceivedBytes() const {
        return m_receivedBytes;
    }

    // 是否完整接收
    bool IsComplete() const {
        return m_receivedBytes >= m_fileSize && m_fileSize > 0;
    }

    // 清空
    void Clear() {
        m_ranges.clear();
        m_receivedBytes = 0;
    }

    // 设置文件大小
    void SetFileSize(uint64_t size) {
        m_fileSize = size;
    }

    uint64_t GetFileSize() const {
        return m_fileSize;
    }

private:
    void MergeRanges() {
        if (m_ranges.size() <= 1) return;

        std::sort(m_ranges.begin(), m_ranges.end());

        std::vector<Range> merged;
        merged.push_back(m_ranges[0]);

        for (size_t i = 1; i < m_ranges.size(); ++i) {
            Range& last = merged.back();
            const Range& current = m_ranges[i];

            // 检查是否可以合并（相邻或重叠）
            if (current.offset <= last.end()) {
                // 扩展现有区间
                uint64_t newEnd = std::max(last.end(), current.end());
                last.length = newEnd - last.offset;
            } else {
                // 新的独立区间
                merged.push_back(current);
            }
        }

        m_ranges = std::move(merged);
    }

    void UpdateReceivedBytes() {
        m_receivedBytes = 0;
        for (const auto& r : m_ranges) {
            m_receivedBytes += r.length;
        }
    }

    std::vector<Range> m_ranges;
    uint64_t m_fileSize;
    uint64_t m_receivedBytes;
};

// ============================================
// 接收状态类（模拟 FileRecvStateV2）
// ============================================

class FileRecvState {
public:
    FileRecvState() : m_fileSize(0), m_fileIndex(0), m_transferID(0) {}

    void Initialize(uint64_t transferID, uint32_t fileIndex, uint64_t fileSize, const std::string& filePath) {
        m_transferID = transferID;
        m_fileIndex = fileIndex;
        m_fileSize = fileSize;
        m_filePath = filePath;
        m_rangeManager.SetFileSize(fileSize);
    }

    void AddChunk(uint64_t offset, uint64_t length) {
        m_rangeManager.AddRange(offset, length);
    }

    bool IsComplete() const {
        return m_rangeManager.IsComplete();
    }

    uint64_t GetReceivedBytes() const {
        return m_rangeManager.GetReceivedBytes();
    }

    double GetProgress() const {
        if (m_fileSize == 0) return 0.0;
        return static_cast<double>(GetReceivedBytes()) / m_fileSize * 100.0;
    }

    std::vector<Range> GetMissingRanges() const {
        return m_rangeManager.GetMissingRanges();
    }

    const std::vector<Range>& GetReceivedRanges() const {
        return m_rangeManager.GetRanges();
    }

    uint64_t GetTransferID() const { return m_transferID; }
    uint32_t GetFileIndex() const { return m_fileIndex; }
    uint64_t GetFileSize() const { return m_fileSize; }
    const std::string& GetFilePath() const { return m_filePath; }

private:
    uint64_t m_transferID;
    uint32_t m_fileIndex;
    uint64_t m_fileSize;
    std::string m_filePath;
    RangeManager m_rangeManager;
};

// ============================================
// Range 基础测试
// ============================================

class RangeTest : public ::testing::Test {};

TEST_F(RangeTest, DefaultConstruction) {
    Range r;
    EXPECT_EQ(r.offset, 0u);
    EXPECT_EQ(r.length, 0u);
    EXPECT_EQ(r.end(), 0u);
}

TEST_F(RangeTest, Construction) {
    Range r(100, 200);
    EXPECT_EQ(r.offset, 100u);
    EXPECT_EQ(r.length, 200u);
    EXPECT_EQ(r.end(), 300u);
}

TEST_F(RangeTest, Comparison) {
    Range r1(0, 100);
    Range r2(100, 100);
    Range r3(0, 100);

    EXPECT_TRUE(r1 < r2);
    EXPECT_FALSE(r2 < r1);
    EXPECT_TRUE(r1 == r3);
    EXPECT_FALSE(r1 == r2);
}

// ============================================
// RangeManager 测试
// ============================================

class RangeManagerTest : public ::testing::Test {};

TEST_F(RangeManagerTest, InitialState) {
    RangeManager rm(1000);
    EXPECT_EQ(rm.GetReceivedBytes(), 0u);
    EXPECT_EQ(rm.GetFileSize(), 1000u);
    EXPECT_FALSE(rm.IsComplete());
    EXPECT_TRUE(rm.GetRanges().empty());
}

TEST_F(RangeManagerTest, AddSingleRange) {
    RangeManager rm(1000);
    rm.AddRange(0, 500);

    EXPECT_EQ(rm.GetReceivedBytes(), 500u);
    ASSERT_EQ(rm.GetRanges().size(), 1u);
    EXPECT_EQ(rm.GetRanges()[0].offset, 0u);
    EXPECT_EQ(rm.GetRanges()[0].length, 500u);
}

TEST_F(RangeManagerTest, AddZeroLengthRange) {
    RangeManager rm(1000);
    rm.AddRange(0, 0);

    EXPECT_EQ(rm.GetReceivedBytes(), 0u);
    EXPECT_TRUE(rm.GetRanges().empty());
}

TEST_F(RangeManagerTest, AddNonOverlappingRanges) {
    RangeManager rm(1000);
    rm.AddRange(0, 100);
    rm.AddRange(200, 100);
    rm.AddRange(400, 100);

    EXPECT_EQ(rm.GetReceivedBytes(), 300u);
    ASSERT_EQ(rm.GetRanges().size(), 3u);
}

TEST_F(RangeManagerTest, MergeAdjacentRanges) {
    RangeManager rm(1000);
    rm.AddRange(0, 100);
    rm.AddRange(100, 100);  // 紧邻

    EXPECT_EQ(rm.GetReceivedBytes(), 200u);
    ASSERT_EQ(rm.GetRanges().size(), 1u);
    EXPECT_EQ(rm.GetRanges()[0].offset, 0u);
    EXPECT_EQ(rm.GetRanges()[0].length, 200u);
}

TEST_F(RangeManagerTest, MergeOverlappingRanges) {
    RangeManager rm(1000);
    rm.AddRange(0, 150);
    rm.AddRange(100, 150);  // 重叠

    EXPECT_EQ(rm.GetReceivedBytes(), 250u);
    ASSERT_EQ(rm.GetRanges().size(), 1u);
    EXPECT_EQ(rm.GetRanges()[0].offset, 0u);
    EXPECT_EQ(rm.GetRanges()[0].length, 250u);
}

TEST_F(RangeManagerTest, MergeContainedRange) {
    RangeManager rm(1000);
    rm.AddRange(0, 500);
    rm.AddRange(100, 100);  // 完全被包含

    EXPECT_EQ(rm.GetReceivedBytes(), 500u);
    ASSERT_EQ(rm.GetRanges().size(), 1u);
    EXPECT_EQ(rm.GetRanges()[0].length, 500u);
}

TEST_F(RangeManagerTest, MergeOutOfOrder) {
    RangeManager rm(1000);
    rm.AddRange(500, 100);
    rm.AddRange(100, 100);
    rm.AddRange(0, 100);

    EXPECT_EQ(rm.GetReceivedBytes(), 300u);
    // 0-100 和 100-200 被合并成 0-200，加上 500-600，共 2 个区间
    ASSERT_EQ(rm.GetRanges().size(), 2u);

    // 验证排序和合并
    EXPECT_EQ(rm.GetRanges()[0].offset, 0u);
    EXPECT_EQ(rm.GetRanges()[0].length, 200u);  // 合并后
    EXPECT_EQ(rm.GetRanges()[1].offset, 500u);
    EXPECT_EQ(rm.GetRanges()[1].length, 100u);
}

TEST_F(RangeManagerTest, MergeMultipleOverlapping) {
    RangeManager rm(1000);
    rm.AddRange(0, 100);
    rm.AddRange(200, 100);
    rm.AddRange(50, 200);  // 跨越两个区间

    EXPECT_EQ(rm.GetReceivedBytes(), 300u);
    ASSERT_EQ(rm.GetRanges().size(), 1u);
    EXPECT_EQ(rm.GetRanges()[0].offset, 0u);
    EXPECT_EQ(rm.GetRanges()[0].length, 300u);
}

TEST_F(RangeManagerTest, GetMissingRanges_Empty) {
    RangeManager rm(1000);
    auto missing = rm.GetMissingRanges();

    ASSERT_EQ(missing.size(), 1u);
    EXPECT_EQ(missing[0].offset, 0u);
    EXPECT_EQ(missing[0].length, 1000u);
}

TEST_F(RangeManagerTest, GetMissingRanges_Partial) {
    RangeManager rm(1000);
    rm.AddRange(0, 100);
    rm.AddRange(500, 100);

    auto missing = rm.GetMissingRanges();
    ASSERT_EQ(missing.size(), 2u);
    EXPECT_EQ(missing[0].offset, 100u);
    EXPECT_EQ(missing[0].length, 400u);
    EXPECT_EQ(missing[1].offset, 600u);
    EXPECT_EQ(missing[1].length, 400u);
}

TEST_F(RangeManagerTest, GetMissingRanges_Complete) {
    RangeManager rm(1000);
    rm.AddRange(0, 1000);

    auto missing = rm.GetMissingRanges();
    EXPECT_TRUE(missing.empty());
}

TEST_F(RangeManagerTest, IsComplete_Exact) {
    RangeManager rm(1000);
    rm.AddRange(0, 1000);

    EXPECT_TRUE(rm.IsComplete());
}

TEST_F(RangeManagerTest, IsComplete_OverReceived) {
    RangeManager rm(1000);
    rm.AddRange(0, 1500);  // 超过文件大小

    EXPECT_TRUE(rm.IsComplete());
}

TEST_F(RangeManagerTest, IsComplete_Partial) {
    RangeManager rm(1000);
    rm.AddRange(0, 999);

    EXPECT_FALSE(rm.IsComplete());
}

TEST_F(RangeManagerTest, Clear) {
    RangeManager rm(1000);
    rm.AddRange(0, 500);
    rm.Clear();

    EXPECT_EQ(rm.GetReceivedBytes(), 0u);
    EXPECT_TRUE(rm.GetRanges().empty());
}

// ============================================
// FileRecvState 测试
// ============================================

class FileRecvStateTest : public ::testing::Test {};

TEST_F(FileRecvStateTest, Initialize) {
    FileRecvState state;
    state.Initialize(12345, 0, 1024, "C:\\test\\file.txt");

    EXPECT_EQ(state.GetTransferID(), 12345u);
    EXPECT_EQ(state.GetFileIndex(), 0u);
    EXPECT_EQ(state.GetFileSize(), 1024u);
    EXPECT_EQ(state.GetFilePath(), "C:\\test\\file.txt");
    EXPECT_EQ(state.GetReceivedBytes(), 0u);
    EXPECT_FALSE(state.IsComplete());
}

TEST_F(FileRecvStateTest, AddChunks) {
    FileRecvState state;
    state.Initialize(1, 0, 1000, "file.txt");

    state.AddChunk(0, 100);
    EXPECT_EQ(state.GetReceivedBytes(), 100u);
    EXPECT_NEAR(state.GetProgress(), 10.0, 0.01);

    state.AddChunk(100, 400);
    EXPECT_EQ(state.GetReceivedBytes(), 500u);
    EXPECT_NEAR(state.GetProgress(), 50.0, 0.01);

    state.AddChunk(500, 500);
    EXPECT_EQ(state.GetReceivedBytes(), 1000u);
    EXPECT_TRUE(state.IsComplete());
    EXPECT_NEAR(state.GetProgress(), 100.0, 0.01);
}

TEST_F(FileRecvStateTest, OutOfOrderChunks) {
    FileRecvState state;
    state.Initialize(1, 0, 1000, "file.txt");

    state.AddChunk(500, 200);
    state.AddChunk(0, 200);
    state.AddChunk(800, 200);

    EXPECT_EQ(state.GetReceivedBytes(), 600u);

    auto missing = state.GetMissingRanges();
    ASSERT_EQ(missing.size(), 2u);
    EXPECT_EQ(missing[0].offset, 200u);
    EXPECT_EQ(missing[0].length, 300u);
    EXPECT_EQ(missing[1].offset, 700u);
    EXPECT_EQ(missing[1].length, 100u);
}

TEST_F(FileRecvStateTest, DuplicateChunks) {
    FileRecvState state;
    state.Initialize(1, 0, 1000, "file.txt");

    state.AddChunk(0, 500);
    state.AddChunk(0, 500);  // 重复
    state.AddChunk(250, 250);  // 重叠

    EXPECT_EQ(state.GetReceivedBytes(), 500u);
}

// ============================================
// 断点续传场景测试
// ============================================

class ResumeScenarioTest : public ::testing::Test {};

TEST_F(ResumeScenarioTest, SimulateInterruptedTransfer) {
    FileRecvState state;
    state.Initialize(12345, 0, 10000, "large_file.bin");

    // 模拟接收了一些数据后中断
    state.AddChunk(0, 2000);
    state.AddChunk(2000, 2000);
    state.AddChunk(5000, 1000);

    EXPECT_EQ(state.GetReceivedBytes(), 5000u);
    EXPECT_NEAR(state.GetProgress(), 50.0, 0.01);

    // 获取需要续传的区间
    auto missing = state.GetMissingRanges();
    ASSERT_EQ(missing.size(), 2u);

    // 验证缺失区间
    EXPECT_EQ(missing[0].offset, 4000u);
    EXPECT_EQ(missing[0].length, 1000u);
    EXPECT_EQ(missing[1].offset, 6000u);
    EXPECT_EQ(missing[1].length, 4000u);
}

TEST_F(ResumeScenarioTest, ResumeAndComplete) {
    FileRecvState state;
    state.Initialize(12345, 0, 10000, "large_file.bin");

    // 初始接收
    state.AddChunk(0, 3000);
    state.AddChunk(7000, 3000);

    EXPECT_FALSE(state.IsComplete());

    // 续传缺失部分
    auto missing = state.GetMissingRanges();
    for (const auto& r : missing) {
        state.AddChunk(r.offset, r.length);
    }

    EXPECT_TRUE(state.IsComplete());
    EXPECT_EQ(state.GetReceivedBytes(), 10000u);
}

TEST_F(ResumeScenarioTest, SmallChunksReassembly) {
    FileRecvState state;
    state.Initialize(1, 0, 1000, "file.txt");

    // 模拟接收很多小块
    for (uint64_t i = 0; i < 1000; i += 10) {
        state.AddChunk(i, 10);
    }

    EXPECT_TRUE(state.IsComplete());

    // 验证区间已合并
    const auto& ranges = state.GetReceivedRanges();
    EXPECT_EQ(ranges.size(), 1u);
    EXPECT_EQ(ranges[0].offset, 0u);
    EXPECT_EQ(ranges[0].length, 1000u);
}

// ============================================
// 大文件场景测试
// ============================================

class LargeFileScenarioTest : public ::testing::Test {};

TEST_F(LargeFileScenarioTest, FileGreaterThan4GB) {
    FileRecvState state;
    uint64_t fileSize = 5ULL * 1024 * 1024 * 1024;  // 5 GB
    state.Initialize(1, 0, fileSize, "huge.bin");

    uint64_t chunkSize = 64 * 1024;  // 64 KB chunks

    // 添加几个大区间
    state.AddChunk(0, 1ULL * 1024 * 1024 * 1024);  // 1 GB
    state.AddChunk(3ULL * 1024 * 1024 * 1024, 1ULL * 1024 * 1024 * 1024);  // 1 GB at 3GB

    EXPECT_EQ(state.GetReceivedBytes(), 2ULL * 1024 * 1024 * 1024);

    auto missing = state.GetMissingRanges();
    ASSERT_EQ(missing.size(), 2u);

    // 1GB-3GB 缺失
    EXPECT_EQ(missing[0].offset, 1ULL * 1024 * 1024 * 1024);
    EXPECT_EQ(missing[0].length, 2ULL * 1024 * 1024 * 1024);

    // 4GB-5GB 缺失
    EXPECT_EQ(missing[1].offset, 4ULL * 1024 * 1024 * 1024);
    EXPECT_EQ(missing[1].length, 1ULL * 1024 * 1024 * 1024);
}

// ============================================
// 边界条件测试
// ============================================

class ChunkBoundaryTest : public ::testing::Test {};

TEST_F(ChunkBoundaryTest, ZeroFileSize) {
    FileRecvState state;
    state.Initialize(1, 0, 0, "empty.txt");

    EXPECT_FALSE(state.IsComplete());  // 0大小文件不算完成
    EXPECT_TRUE(state.GetMissingRanges().empty());
}

TEST_F(ChunkBoundaryTest, SingleByteFile) {
    FileRecvState state;
    state.Initialize(1, 0, 1, "tiny.txt");

    state.AddChunk(0, 1);
    EXPECT_TRUE(state.IsComplete());
}

TEST_F(ChunkBoundaryTest, MaxValues) {
    RangeManager rm(UINT64_MAX);

    // 添加接近最大值的区间
    rm.AddRange(UINT64_MAX - 1000, 500);
    EXPECT_EQ(rm.GetReceivedBytes(), 500u);
}

TEST_F(ChunkBoundaryTest, OverlappingAtBoundary) {
    RangeManager rm(1000);

    rm.AddRange(0, 500);
    rm.AddRange(499, 2);  // 重叠1字节

    EXPECT_EQ(rm.GetReceivedBytes(), 501u);
    ASSERT_EQ(rm.GetRanges().size(), 1u);
}

// ============================================
// 性能相关测试
// ============================================

class ChunkPerformanceTest : public ::testing::Test {};

TEST_F(ChunkPerformanceTest, ManySmallRanges) {
    RangeManager rm(1000000);

    // 添加大量不连续的小区间
    for (uint64_t i = 0; i < 1000000; i += 20) {
        rm.AddRange(i, 10);
    }

    // 验证区间数量合理
    EXPECT_LE(rm.GetRanges().size(), 50000u);
    EXPECT_EQ(rm.GetReceivedBytes(), 500000u);
}

TEST_F(ChunkPerformanceTest, ManyContiguousRanges) {
    RangeManager rm(1000000);

    // 添加大量连续的小区间（应该全部合并）
    for (uint64_t i = 0; i < 1000; ++i) {
        rm.AddRange(i * 1000, 1000);
    }

    // 应该合并成单个区间
    ASSERT_EQ(rm.GetRanges().size(), 1u);
    EXPECT_EQ(rm.GetReceivedBytes(), 1000000u);
    EXPECT_TRUE(rm.IsComplete());
}

