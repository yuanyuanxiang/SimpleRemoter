/**
 * @file PacketFragmentTest.cpp
 * @brief 粘包/分包处理测试
 *
 * 测试覆盖：
 * - 完整包接收和解析
 * - 分包（不完整包）处理
 * - 粘包（多个包粘在一起）处理
 * - 混合场景测试
 * - 边界条件处理
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdint>
#include <vector>
#include <queue>
#include <functional>

// ============================================
// 协议常量
// ============================================

const int FLAG_LENGTH = 8;
const int HDR_LENGTH = 16;  // FLAG(8) + PackedLen(4) + OrigLen(4)

// ============================================
// 简化版 CBuffer（测试专用）
// ============================================

class TestBuffer {
public:
    TestBuffer() {}

    void WriteBuffer(const uint8_t* data, size_t len) {
        m_data.insert(m_data.end(), data, data + len);
    }

    size_t ReadBuffer(uint8_t* dst, size_t len) {
        size_t toRead = std::min(len, m_data.size());
        if (toRead > 0) {
            memcpy(dst, m_data.data(), toRead);
            m_data.erase(m_data.begin(), m_data.begin() + toRead);
        }
        return toRead;
    }

    void Skip(size_t len) {
        size_t toSkip = std::min(len, m_data.size());
        m_data.erase(m_data.begin(), m_data.begin() + toSkip);
    }

    size_t GetBufferLength() const {
        return m_data.size();
    }

    const uint8_t* GetBuffer(size_t pos = 0) const {
        if (pos >= m_data.size()) return nullptr;
        return m_data.data() + pos;
    }

    bool CopyBuffer(uint8_t* dst, size_t pos, size_t len) const {
        if (pos + len > m_data.size()) return false;
        memcpy(dst, m_data.data() + pos, len);
        return true;
    }

    void Clear() {
        m_data.clear();
    }

private:
    std::vector<uint8_t> m_data;
};

// ============================================
// 数据包构建辅助函数
// ============================================

#pragma pack(push, 1)
struct PacketHeader {
    char flag[FLAG_LENGTH];
    uint32_t packedLength;      // 包含头部的总长度
    uint32_t originalLength;    // 原始数据长度
};
#pragma pack(pop)

std::vector<uint8_t> BuildPacket(const std::vector<uint8_t>& payload, uint8_t key = 0x42) {
    std::vector<uint8_t> packet(HDR_LENGTH + payload.size());

    PacketHeader* hdr = reinterpret_cast<PacketHeader*>(packet.data());
    memcpy(hdr->flag, "HELL", 4);
    hdr->flag[6] = key;
    hdr->flag[7] = ~key;
    hdr->packedLength = static_cast<uint32_t>(HDR_LENGTH + payload.size());
    hdr->originalLength = static_cast<uint32_t>(payload.size());

    if (!payload.empty()) {
        memcpy(packet.data() + HDR_LENGTH, payload.data(), payload.size());
    }

    return packet;
}

std::vector<uint8_t> BuildPacketWithData(size_t dataSize, uint8_t fillByte = 0xAA) {
    std::vector<uint8_t> payload(dataSize, fillByte);
    return BuildPacket(payload);
}

// ============================================
// 粘包/分包处理器（模拟 OnServerReceiving 逻辑）
// ============================================

class PacketProcessor {
public:
    using PacketCallback = std::function<void(const std::vector<uint8_t>&)>;

    PacketProcessor(PacketCallback callback) : m_callback(callback) {}

    // 接收数据（模拟网络接收）
    void OnReceive(const uint8_t* data, size_t len) {
        m_buffer.WriteBuffer(data, len);
        ProcessBuffer();
    }

    // 获取待处理的字节数
    size_t GetPendingBytes() const {
        return m_buffer.GetBufferLength();
    }

    // 获取已处理的包数量
    size_t GetProcessedCount() const {
        return m_processedCount;
    }

private:
    void ProcessBuffer() {
        while (m_buffer.GetBufferLength() >= HDR_LENGTH) {
            // 验证头部
            const uint8_t* buf = m_buffer.GetBuffer();
            if (memcmp(buf, "HELL", 4) != 0) {
                // 无效头部，跳过一个字节重试
                m_buffer.Skip(1);
                continue;
            }

            // 读取包长度
            uint32_t packedLength;
            m_buffer.CopyBuffer(reinterpret_cast<uint8_t*>(&packedLength),
                               FLAG_LENGTH, sizeof(uint32_t));

            // 检查长度有效性
            if (packedLength < HDR_LENGTH || packedLength > 100 * 1024 * 1024) {
                // 无效长度，跳过头部
                m_buffer.Skip(FLAG_LENGTH);
                continue;
            }

            // 检查包是否完整
            if (m_buffer.GetBufferLength() < packedLength) {
                // 不完整，等待更多数据
                break;
            }

            // 读取完整包
            std::vector<uint8_t> packet(packedLength);
            m_buffer.ReadBuffer(packet.data(), packedLength);

            // 提取 payload
            std::vector<uint8_t> payload(packet.begin() + HDR_LENGTH, packet.end());
            m_callback(payload);
            m_processedCount++;
        }
    }

    TestBuffer m_buffer;
    PacketCallback m_callback;
    size_t m_processedCount = 0;
};

// ============================================
// 完整包接收测试
// ============================================

class CompletePacketTest : public ::testing::Test {
protected:
    std::vector<std::vector<uint8_t>> receivedPackets;

    void SetUp() override {
        receivedPackets.clear();
    }

    PacketProcessor::PacketCallback GetCallback() {
        return [this](const std::vector<uint8_t>& payload) {
            receivedPackets.push_back(payload);
        };
    }
};

TEST_F(CompletePacketTest, SinglePacket) {
    PacketProcessor processor(GetCallback());

    std::vector<uint8_t> payload = {0x01, 0x02, 0x03, 0x04};
    auto packet = BuildPacket(payload);

    processor.OnReceive(packet.data(), packet.size());

    ASSERT_EQ(receivedPackets.size(), 1u);
    EXPECT_EQ(receivedPackets[0], payload);
    EXPECT_EQ(processor.GetPendingBytes(), 0u);
}

TEST_F(CompletePacketTest, EmptyPayload) {
    PacketProcessor processor(GetCallback());

    std::vector<uint8_t> payload;
    auto packet = BuildPacket(payload);

    processor.OnReceive(packet.data(), packet.size());

    ASSERT_EQ(receivedPackets.size(), 1u);
    EXPECT_TRUE(receivedPackets[0].empty());
}

TEST_F(CompletePacketTest, LargePayload) {
    PacketProcessor processor(GetCallback());

    std::vector<uint8_t> payload(64 * 1024);  // 64 KB
    for (size_t i = 0; i < payload.size(); ++i) {
        payload[i] = static_cast<uint8_t>(i & 0xFF);
    }
    auto packet = BuildPacket(payload);

    processor.OnReceive(packet.data(), packet.size());

    ASSERT_EQ(receivedPackets.size(), 1u);
    EXPECT_EQ(receivedPackets[0], payload);
}

// ============================================
// 分包（不完整包）测试
// ============================================

class FragmentedPacketTest : public ::testing::Test {
protected:
    std::vector<std::vector<uint8_t>> receivedPackets;

    PacketProcessor::PacketCallback GetCallback() {
        return [this](const std::vector<uint8_t>& payload) {
            receivedPackets.push_back(payload);
        };
    }
};

TEST_F(FragmentedPacketTest, TwoFragments) {
    PacketProcessor processor(GetCallback());

    std::vector<uint8_t> payload = {0xAA, 0xBB, 0xCC, 0xDD};
    auto packet = BuildPacket(payload);

    // 分两次发送
    size_t half = packet.size() / 2;
    processor.OnReceive(packet.data(), half);
    EXPECT_EQ(receivedPackets.size(), 0u);
    EXPECT_EQ(processor.GetPendingBytes(), half);

    processor.OnReceive(packet.data() + half, packet.size() - half);
    ASSERT_EQ(receivedPackets.size(), 1u);
    EXPECT_EQ(receivedPackets[0], payload);
}

TEST_F(FragmentedPacketTest, ManyFragments) {
    PacketProcessor processor(GetCallback());

    std::vector<uint8_t> payload(100);
    for (size_t i = 0; i < payload.size(); ++i) {
        payload[i] = static_cast<uint8_t>(i);
    }
    auto packet = BuildPacket(payload);

    // 每次发送 10 字节
    for (size_t i = 0; i < packet.size(); i += 10) {
        size_t len = std::min(size_t(10), packet.size() - i);
        processor.OnReceive(packet.data() + i, len);
    }

    ASSERT_EQ(receivedPackets.size(), 1u);
    EXPECT_EQ(receivedPackets[0], payload);
}

TEST_F(FragmentedPacketTest, OnlyHeader) {
    PacketProcessor processor(GetCallback());

    std::vector<uint8_t> payload = {0x01, 0x02};
    auto packet = BuildPacket(payload);

    // 只发送头部
    processor.OnReceive(packet.data(), HDR_LENGTH);
    EXPECT_EQ(receivedPackets.size(), 0u);
    EXPECT_EQ(processor.GetPendingBytes(), HDR_LENGTH);

    // 发送剩余数据
    processor.OnReceive(packet.data() + HDR_LENGTH, packet.size() - HDR_LENGTH);
    ASSERT_EQ(receivedPackets.size(), 1u);
}

TEST_F(FragmentedPacketTest, PartialHeader) {
    PacketProcessor processor(GetCallback());

    std::vector<uint8_t> payload = {0xFF};
    auto packet = BuildPacket(payload);

    // 发送不完整的头部
    processor.OnReceive(packet.data(), 4);  // 只有 "HELL"
    EXPECT_EQ(receivedPackets.size(), 0u);

    // 发送剩余部分
    processor.OnReceive(packet.data() + 4, packet.size() - 4);
    ASSERT_EQ(receivedPackets.size(), 1u);
}

// ============================================
// 粘包测试
// ============================================

class StickyPacketTest : public ::testing::Test {
protected:
    std::vector<std::vector<uint8_t>> receivedPackets;

    PacketProcessor::PacketCallback GetCallback() {
        return [this](const std::vector<uint8_t>& payload) {
            receivedPackets.push_back(payload);
        };
    }
};

TEST_F(StickyPacketTest, TwoPacketsStuckTogether) {
    PacketProcessor processor(GetCallback());

    std::vector<uint8_t> payload1 = {0x11, 0x22};
    std::vector<uint8_t> payload2 = {0x33, 0x44, 0x55};
    auto packet1 = BuildPacket(payload1);
    auto packet2 = BuildPacket(payload2);

    // 合并两个包
    std::vector<uint8_t> combined;
    combined.insert(combined.end(), packet1.begin(), packet1.end());
    combined.insert(combined.end(), packet2.begin(), packet2.end());

    processor.OnReceive(combined.data(), combined.size());

    ASSERT_EQ(receivedPackets.size(), 2u);
    EXPECT_EQ(receivedPackets[0], payload1);
    EXPECT_EQ(receivedPackets[1], payload2);
}

TEST_F(StickyPacketTest, ThreePacketsStuckTogether) {
    PacketProcessor processor(GetCallback());

    std::vector<uint8_t> payload1 = {0x01};
    std::vector<uint8_t> payload2 = {0x02, 0x03};
    std::vector<uint8_t> payload3 = {0x04, 0x05, 0x06};
    auto packet1 = BuildPacket(payload1);
    auto packet2 = BuildPacket(payload2);
    auto packet3 = BuildPacket(payload3);

    // 合并三个包
    std::vector<uint8_t> combined;
    combined.insert(combined.end(), packet1.begin(), packet1.end());
    combined.insert(combined.end(), packet2.begin(), packet2.end());
    combined.insert(combined.end(), packet3.begin(), packet3.end());

    processor.OnReceive(combined.data(), combined.size());

    ASSERT_EQ(receivedPackets.size(), 3u);
    EXPECT_EQ(receivedPackets[0], payload1);
    EXPECT_EQ(receivedPackets[1], payload2);
    EXPECT_EQ(receivedPackets[2], payload3);
}

TEST_F(StickyPacketTest, ManyPacketsStuckTogether) {
    PacketProcessor processor(GetCallback());

    std::vector<uint8_t> combined;
    const int numPackets = 100;

    for (int i = 0; i < numPackets; ++i) {
        std::vector<uint8_t> payload(i % 10 + 1, static_cast<uint8_t>(i));
        auto packet = BuildPacket(payload);
        combined.insert(combined.end(), packet.begin(), packet.end());
    }

    processor.OnReceive(combined.data(), combined.size());

    EXPECT_EQ(receivedPackets.size(), numPackets);
}

// ============================================
// 混合场景测试
// ============================================

class MixedScenarioTest : public ::testing::Test {
protected:
    std::vector<std::vector<uint8_t>> receivedPackets;

    PacketProcessor::PacketCallback GetCallback() {
        return [this](const std::vector<uint8_t>& payload) {
            receivedPackets.push_back(payload);
        };
    }
};

TEST_F(MixedScenarioTest, OneAndHalfPackets) {
    PacketProcessor processor(GetCallback());

    std::vector<uint8_t> payload1 = {0xAA, 0xBB};
    std::vector<uint8_t> payload2 = {0xCC, 0xDD, 0xEE, 0xFF};
    auto packet1 = BuildPacket(payload1);
    auto packet2 = BuildPacket(payload2);

    // 发送完整包1 + 半个包2
    std::vector<uint8_t> firstSend;
    firstSend.insert(firstSend.end(), packet1.begin(), packet1.end());
    firstSend.insert(firstSend.end(), packet2.begin(), packet2.begin() + packet2.size() / 2);

    processor.OnReceive(firstSend.data(), firstSend.size());
    EXPECT_EQ(receivedPackets.size(), 1u);  // 只处理了包1

    // 发送剩余的半个包2
    processor.OnReceive(packet2.data() + packet2.size() / 2, packet2.size() - packet2.size() / 2);
    ASSERT_EQ(receivedPackets.size(), 2u);
    EXPECT_EQ(receivedPackets[0], payload1);
    EXPECT_EQ(receivedPackets[1], payload2);
}

TEST_F(MixedScenarioTest, HalfPacketThenOneAndHalf) {
    PacketProcessor processor(GetCallback());

    std::vector<uint8_t> payload1 = {0x11};
    std::vector<uint8_t> payload2 = {0x22, 0x33};
    auto packet1 = BuildPacket(payload1);
    auto packet2 = BuildPacket(payload2);

    // 发送半个包1
    processor.OnReceive(packet1.data(), packet1.size() / 2);
    EXPECT_EQ(receivedPackets.size(), 0u);

    // 发送剩余包1 + 完整包2
    std::vector<uint8_t> secondSend;
    secondSend.insert(secondSend.end(), packet1.begin() + packet1.size() / 2, packet1.end());
    secondSend.insert(secondSend.end(), packet2.begin(), packet2.end());

    processor.OnReceive(secondSend.data(), secondSend.size());
    ASSERT_EQ(receivedPackets.size(), 2u);
}

TEST_F(MixedScenarioTest, RandomChunkSizes) {
    PacketProcessor processor(GetCallback());

    // 准备多个包
    std::vector<std::vector<uint8_t>> payloads;
    std::vector<uint8_t> allData;

    for (int i = 0; i < 10; ++i) {
        std::vector<uint8_t> payload(i * 5 + 10, static_cast<uint8_t>(i));
        payloads.push_back(payload);
        auto packet = BuildPacket(payload);
        allData.insert(allData.end(), packet.begin(), packet.end());
    }

    // 使用"随机"大小的块发送
    size_t chunkSizes[] = {1, 7, 15, 16, 17, 31, 32, 33, 64, 128};
    size_t pos = 0;
    size_t chunkIdx = 0;

    while (pos < allData.size()) {
        size_t chunkSize = chunkSizes[chunkIdx % (sizeof(chunkSizes) / sizeof(chunkSizes[0]))];
        size_t len = std::min(chunkSize, allData.size() - pos);
        processor.OnReceive(allData.data() + pos, len);
        pos += len;
        chunkIdx++;
    }

    ASSERT_EQ(receivedPackets.size(), payloads.size());
    for (size_t i = 0; i < payloads.size(); ++i) {
        EXPECT_EQ(receivedPackets[i], payloads[i]) << "Mismatch at packet " << i;
    }
}

// ============================================
// 边界条件测试
// ============================================

class PacketBoundaryTest : public ::testing::Test {
protected:
    std::vector<std::vector<uint8_t>> receivedPackets;

    PacketProcessor::PacketCallback GetCallback() {
        return [this](const std::vector<uint8_t>& payload) {
            receivedPackets.push_back(payload);
        };
    }
};

TEST_F(PacketBoundaryTest, ExactlyHdrLength) {
    PacketProcessor processor(GetCallback());

    // 只有头部，无 payload
    std::vector<uint8_t> packet(HDR_LENGTH);
    PacketHeader* hdr = reinterpret_cast<PacketHeader*>(packet.data());
    memcpy(hdr->flag, "HELL", 4);
    hdr->flag[6] = 0x42;
    hdr->flag[7] = ~0x42;
    hdr->packedLength = HDR_LENGTH;
    hdr->originalLength = 0;

    processor.OnReceive(packet.data(), packet.size());

    ASSERT_EQ(receivedPackets.size(), 1u);
    EXPECT_TRUE(receivedPackets[0].empty());
}

TEST_F(PacketBoundaryTest, SingleBytePayload) {
    PacketProcessor processor(GetCallback());

    std::vector<uint8_t> payload = {0xFF};
    auto packet = BuildPacket(payload);

    processor.OnReceive(packet.data(), packet.size());

    ASSERT_EQ(receivedPackets.size(), 1u);
    EXPECT_EQ(receivedPackets[0].size(), 1u);
    EXPECT_EQ(receivedPackets[0][0], 0xFF);
}

TEST_F(PacketBoundaryTest, ByteByByteReceive) {
    PacketProcessor processor(GetCallback());

    std::vector<uint8_t> payload = {0x12, 0x34, 0x56};
    auto packet = BuildPacket(payload);

    // 每次接收 1 字节
    for (size_t i = 0; i < packet.size(); ++i) {
        processor.OnReceive(packet.data() + i, 1);
    }

    ASSERT_EQ(receivedPackets.size(), 1u);
    EXPECT_EQ(receivedPackets[0], payload);
}

// ============================================
// 数据完整性测试
// ============================================

class DataIntegrityTest : public ::testing::Test {
protected:
    std::vector<std::vector<uint8_t>> receivedPackets;

    PacketProcessor::PacketCallback GetCallback() {
        return [this](const std::vector<uint8_t>& payload) {
            receivedPackets.push_back(payload);
        };
    }
};

TEST_F(DataIntegrityTest, BinaryData) {
    PacketProcessor processor(GetCallback());

    // 包含所有字节值的 payload
    std::vector<uint8_t> payload(256);
    for (int i = 0; i < 256; ++i) {
        payload[i] = static_cast<uint8_t>(i);
    }
    auto packet = BuildPacket(payload);

    processor.OnReceive(packet.data(), packet.size());

    ASSERT_EQ(receivedPackets.size(), 1u);
    EXPECT_EQ(receivedPackets[0], payload);
}

TEST_F(DataIntegrityTest, AllZeros) {
    PacketProcessor processor(GetCallback());

    std::vector<uint8_t> payload(100, 0x00);
    auto packet = BuildPacket(payload);

    processor.OnReceive(packet.data(), packet.size());

    ASSERT_EQ(receivedPackets.size(), 1u);
    for (uint8_t b : receivedPackets[0]) {
        EXPECT_EQ(b, 0x00);
    }
}

TEST_F(DataIntegrityTest, AllOnes) {
    PacketProcessor processor(GetCallback());

    std::vector<uint8_t> payload(100, 0xFF);
    auto packet = BuildPacket(payload);

    processor.OnReceive(packet.data(), packet.size());

    ASSERT_EQ(receivedPackets.size(), 1u);
    for (uint8_t b : receivedPackets[0]) {
        EXPECT_EQ(b, 0xFF);
    }
}

// ============================================
// 性能相关测试
// ============================================

class PacketPerformanceTest : public ::testing::Test {
protected:
    size_t packetCount = 0;

    PacketProcessor::PacketCallback GetCallback() {
        return [this](const std::vector<uint8_t>& payload) {
            packetCount++;
        };
    }
};

TEST_F(PacketPerformanceTest, ManySmallPackets) {
    PacketProcessor processor(GetCallback());

    const int numPackets = 10000;
    std::vector<uint8_t> allData;

    for (int i = 0; i < numPackets; ++i) {
        std::vector<uint8_t> payload = {static_cast<uint8_t>(i & 0xFF)};
        auto packet = BuildPacket(payload);
        allData.insert(allData.end(), packet.begin(), packet.end());
    }

    processor.OnReceive(allData.data(), allData.size());

    EXPECT_EQ(packetCount, numPackets);
}

TEST_F(PacketPerformanceTest, LargePacketInSmallChunks) {
    PacketProcessor processor(GetCallback());

    std::vector<uint8_t> payload(100 * 1024);  // 100 KB
    for (size_t i = 0; i < payload.size(); ++i) {
        payload[i] = static_cast<uint8_t>(i & 0xFF);
    }
    auto packet = BuildPacket(payload);

    // 每次发送 1 KB
    const size_t chunkSize = 1024;
    for (size_t i = 0; i < packet.size(); i += chunkSize) {
        size_t len = std::min(chunkSize, packet.size() - i);
        processor.OnReceive(packet.data() + i, len);
    }

    EXPECT_EQ(packetCount, 1u);
}

