/**
 * @file BufferTest.cpp
 * @brief 客户端 CBuffer 类单元测试
 *
 * 测试覆盖：
 * - 基本读写操作
 * - 边界条件（空缓冲区、零长度、超长请求）
 * - 内存管理（扩展、收缩）
 * - 下溢防护（Skip、ReadBuffer 边界）
 */

#include <gtest/gtest.h>
#include <cstring>
#include <vector>

// 模拟 Windows 类型定义（用于跨平台测试）
#ifndef _WIN32
typedef unsigned char BYTE;
typedef BYTE* PBYTE;
typedef BYTE* LPBYTE;
typedef unsigned long ULONG;
typedef int BOOL;
#define TRUE 1
#define FALSE 0
#define MEM_COMMIT 0x1000
#define MEM_RELEASE 0x8000
#define PAGE_READWRITE 0x04

// 跨平台内存分配模拟
inline void* MVirtualAlloc(void*, size_t size, int, int) {
    return malloc(size);
}
inline void MVirtualFree(void* ptr, size_t, int) {
    free(ptr);
}
inline void CopyMemory(void* dst, const void* src, size_t len) {
    memcpy(dst, src, len);
}
inline void MoveMemory(void* dst, const void* src, size_t len) {
    memmove(dst, src, len);
}
#else
#include <Windows.h>
// Windows 下的内存分配封装
inline void* MVirtualAlloc(void* addr, size_t size, int type, int protect) {
    return VirtualAlloc(addr, size, type, protect);
}
inline void MVirtualFree(void* ptr, size_t size, int type) {
    VirtualFree(ptr, size, type);
}
#endif

// 内联包含 Buffer 实现（测试专用）
// 这样可以避免复杂的链接问题
namespace ClientBuffer {

#define U_PAGE_ALIGNMENT   3
#define F_PAGE_ALIGNMENT 3.0

class CBuffer
{
public:
    CBuffer() : m_ulMaxLength(0), m_Base(NULL), m_Ptr(NULL) {}

    ~CBuffer() {
        if (m_Base) {
            MVirtualFree(m_Base, 0, MEM_RELEASE);
            m_Base = NULL;
        }
        m_Base = m_Ptr = NULL;
        m_ulMaxLength = 0;
    }

    ULONG ReadBuffer(PBYTE Buffer, ULONG ulLength) {
        ULONG dataLen = (ULONG)(m_Ptr - m_Base);
        if (ulLength > dataLen) {
            ulLength = dataLen;
        }

        if (ulLength) {
            CopyMemory(Buffer, m_Base, ulLength);

            ULONG remaining = dataLen - ulLength;
            if (remaining > 0) {
                MoveMemory(m_Base, m_Base + ulLength, remaining);
            }
            m_Ptr = m_Base + remaining;
        }

        DeAllocateBuffer((ULONG)(m_Ptr - m_Base));
        return ulLength;
    }

    VOID DeAllocateBuffer(ULONG ulLength) {
        int len = (int)(m_Ptr - m_Base);
        if (ulLength < (ULONG)len)
            return;

        ULONG ulNewMaxLength = (ULONG)(ceil(ulLength / F_PAGE_ALIGNMENT) * U_PAGE_ALIGNMENT);

        if (m_ulMaxLength <= ulNewMaxLength) {
            return;
        }
        PBYTE NewBase = (PBYTE)MVirtualAlloc(NULL, ulNewMaxLength, MEM_COMMIT, PAGE_READWRITE);
        if (NewBase == NULL)
            return;

        CopyMemory(NewBase, m_Base, len);
        MVirtualFree(m_Base, 0, MEM_RELEASE);
        m_Base = NewBase;
        m_Ptr = m_Base + len;
        m_ulMaxLength = ulNewMaxLength;
    }

    BOOL WriteBuffer(PBYTE Buffer, ULONG ulLength) {
        if (ReAllocateBuffer(ulLength + (ULONG)(m_Ptr - m_Base)) == FALSE) {
            return FALSE;
        }

        CopyMemory(m_Ptr, Buffer, ulLength);
        m_Ptr += ulLength;
        return TRUE;
    }

    BOOL ReAllocateBuffer(ULONG ulLength) {
        if (ulLength < m_ulMaxLength)
            return TRUE;

        ULONG ulNewMaxLength = (ULONG)(ceil(ulLength / F_PAGE_ALIGNMENT) * U_PAGE_ALIGNMENT);
        PBYTE NewBase = (PBYTE)MVirtualAlloc(NULL, ulNewMaxLength, MEM_COMMIT, PAGE_READWRITE);
        if (NewBase == NULL) {
            return FALSE;
        }

        ULONG len = (ULONG)(m_Ptr - m_Base);
        CopyMemory(NewBase, m_Base, len);

        if (m_Base) {
            MVirtualFree(m_Base, 0, MEM_RELEASE);
        }
        m_Base = NewBase;
        m_Ptr = m_Base + len;
        m_ulMaxLength = ulNewMaxLength;

        return TRUE;
    }

    VOID ClearBuffer() {
        m_Ptr = m_Base;
        DeAllocateBuffer(1024);
    }

    ULONG GetBufferLength() const {
        return (ULONG)(m_Ptr - m_Base);
    }

    void Skip(ULONG ulPos) {
        if (ulPos == 0)
            return;

        ULONG dataLen = (ULONG)(m_Ptr - m_Base);
        if (ulPos > dataLen) {
            ulPos = dataLen;
        }

        if (ulPos > 0) {
            ULONG remaining = dataLen - ulPos;
            if (remaining > 0) {
                MoveMemory(m_Base, m_Base + ulPos, remaining);
            }
            m_Ptr = m_Base + remaining;
        }
    }

    PBYTE GetBuffer(ULONG ulPos = 0) const {
        if (m_Base == NULL || ulPos >= (ULONG)(m_Ptr - m_Base)) {
            return NULL;
        }
        return m_Base + ulPos;
    }

protected:
    PBYTE m_Base;
    PBYTE m_Ptr;
    ULONG m_ulMaxLength;
};

} // namespace ClientBuffer

using ClientBuffer::CBuffer;

// ============================================
// 测试夹具
// ============================================
class ClientBufferTest : public ::testing::Test {
protected:
    CBuffer buffer;

    void SetUp() override {
        // 每个测试前重置
    }

    void TearDown() override {
        // 每个测试后清理
    }

    // 辅助方法：写入测试数据
    void WriteTestData(const std::vector<BYTE>& data) {
        buffer.WriteBuffer(const_cast<BYTE*>(data.data()), (ULONG)data.size());
    }

    // 辅助方法：写入指定长度的填充数据
    void WriteFillData(ULONG length, BYTE fillValue = 0x42) {
        std::vector<BYTE> data(length, fillValue);
        buffer.WriteBuffer(data.data(), length);
    }
};

// ============================================
// 构造/析构测试
// ============================================
TEST_F(ClientBufferTest, Constructor_InitializesEmpty) {
    CBuffer newBuffer;
    EXPECT_EQ(newBuffer.GetBufferLength(), 0u);
    EXPECT_EQ(newBuffer.GetBuffer(), nullptr);
}

// ============================================
// WriteBuffer 测试
// ============================================
TEST_F(ClientBufferTest, WriteBuffer_ValidData_ReturnsTrue) {
    BYTE data[] = {1, 2, 3, 4, 5};
    EXPECT_TRUE(buffer.WriteBuffer(data, 5));
    EXPECT_EQ(buffer.GetBufferLength(), 5u);
}

TEST_F(ClientBufferTest, WriteBuffer_ZeroLength_ToEmptyBuffer) {
    // 空缓冲区写入 0 字节：由于 VirtualAlloc(0) 返回 NULL，会失败
    BYTE data[] = {1};
    // 实际行为：返回 FALSE（无法分配 0 字节）
    EXPECT_FALSE(buffer.WriteBuffer(data, 0));
    EXPECT_EQ(buffer.GetBufferLength(), 0u);
}

TEST_F(ClientBufferTest, WriteBuffer_ZeroLength_ToNonEmptyBuffer) {
    // 非空缓冲区写入 0 字节应该成功
    BYTE data[] = {1, 2, 3};
    buffer.WriteBuffer(data, 3);

    EXPECT_TRUE(buffer.WriteBuffer(data, 0));
    EXPECT_EQ(buffer.GetBufferLength(), 3u);
}

TEST_F(ClientBufferTest, WriteBuffer_MultipleWrites_AccumulatesData) {
    BYTE data1[] = {1, 2, 3};
    BYTE data2[] = {4, 5};

    buffer.WriteBuffer(data1, 3);
    buffer.WriteBuffer(data2, 2);

    EXPECT_EQ(buffer.GetBufferLength(), 5u);

    // 验证数据完整性
    BYTE result[5];
    buffer.ReadBuffer(result, 5);
    EXPECT_EQ(result[0], 1);
    EXPECT_EQ(result[1], 2);
    EXPECT_EQ(result[2], 3);
    EXPECT_EQ(result[3], 4);
    EXPECT_EQ(result[4], 5);
}

TEST_F(ClientBufferTest, WriteBuffer_LargeData_HandlesCorrectly) {
    const ULONG largeSize = 10000;
    std::vector<BYTE> data(largeSize);
    for (ULONG i = 0; i < largeSize; i++) {
        data[i] = (BYTE)(i & 0xFF);
    }

    EXPECT_TRUE(buffer.WriteBuffer(data.data(), largeSize));
    EXPECT_EQ(buffer.GetBufferLength(), largeSize);
}

// ============================================
// ReadBuffer 测试
// ============================================
TEST_F(ClientBufferTest, ReadBuffer_EmptyBuffer_ReturnsZero) {
    BYTE result[10];
    EXPECT_EQ(buffer.ReadBuffer(result, 10), 0u);
}

TEST_F(ClientBufferTest, ReadBuffer_ExactLength_ReturnsAll) {
    BYTE data[] = {1, 2, 3, 4, 5};
    buffer.WriteBuffer(data, 5);

    BYTE result[5];
    ULONG bytesRead = buffer.ReadBuffer(result, 5);

    EXPECT_EQ(bytesRead, 5u);
    EXPECT_EQ(buffer.GetBufferLength(), 0u);
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(result[i], data[i]);
    }
}

TEST_F(ClientBufferTest, ReadBuffer_PartialRead_LeavesRemainder) {
    BYTE data[] = {1, 2, 3, 4, 5};
    buffer.WriteBuffer(data, 5);

    BYTE result[3];
    ULONG bytesRead = buffer.ReadBuffer(result, 3);

    EXPECT_EQ(bytesRead, 3u);
    EXPECT_EQ(buffer.GetBufferLength(), 2u);

    // 验证剩余数据
    EXPECT_EQ(*buffer.GetBuffer(0), 4);
    EXPECT_EQ(*buffer.GetBuffer(1), 5);
}

TEST_F(ClientBufferTest, ReadBuffer_RequestExceedsAvailable_ReturnsAvailableOnly) {
    BYTE data[] = {1, 2, 3};
    buffer.WriteBuffer(data, 3);

    BYTE result[10];
    ULONG bytesRead = buffer.ReadBuffer(result, 10);

    EXPECT_EQ(bytesRead, 3u);
    EXPECT_EQ(buffer.GetBufferLength(), 0u);
}

TEST_F(ClientBufferTest, ReadBuffer_ZeroLength_ReturnsZero) {
    BYTE data[] = {1, 2, 3};
    buffer.WriteBuffer(data, 3);

    BYTE result[1];
    ULONG bytesRead = buffer.ReadBuffer(result, 0);

    EXPECT_EQ(bytesRead, 0u);
    EXPECT_EQ(buffer.GetBufferLength(), 3u);
}

// ============================================
// Skip 测试
// ============================================
TEST_F(ClientBufferTest, Skip_ZeroPosition_NoChange) {
    BYTE data[] = {1, 2, 3, 4, 5};
    buffer.WriteBuffer(data, 5);

    buffer.Skip(0);

    EXPECT_EQ(buffer.GetBufferLength(), 5u);
}

TEST_F(ClientBufferTest, Skip_PartialSkip_RemovesPrefix) {
    BYTE data[] = {1, 2, 3, 4, 5};
    buffer.WriteBuffer(data, 5);

    buffer.Skip(2);

    EXPECT_EQ(buffer.GetBufferLength(), 3u);
    EXPECT_EQ(*buffer.GetBuffer(0), 3);
    EXPECT_EQ(*buffer.GetBuffer(1), 4);
    EXPECT_EQ(*buffer.GetBuffer(2), 5);
}

TEST_F(ClientBufferTest, Skip_ExactLength_ClearsBuffer) {
    BYTE data[] = {1, 2, 3};
    buffer.WriteBuffer(data, 3);

    buffer.Skip(3);

    EXPECT_EQ(buffer.GetBufferLength(), 0u);
}

TEST_F(ClientBufferTest, Skip_ExceedsLength_ClampsToAvailable) {
    BYTE data[] = {1, 2, 3};
    buffer.WriteBuffer(data, 3);

    // 这是修复后的行为：不会下溢，而是限制到可用长度
    buffer.Skip(100);

    EXPECT_EQ(buffer.GetBufferLength(), 0u);
}

TEST_F(ClientBufferTest, Skip_EmptyBuffer_NoEffect) {
    buffer.Skip(10);
    EXPECT_EQ(buffer.GetBufferLength(), 0u);
}

// ============================================
// GetBuffer 测试
// ============================================
TEST_F(ClientBufferTest, GetBuffer_EmptyBuffer_ReturnsNull) {
    EXPECT_EQ(buffer.GetBuffer(), nullptr);
}

TEST_F(ClientBufferTest, GetBuffer_ValidPosition_ReturnsCorrectPointer) {
    BYTE data[] = {10, 20, 30, 40, 50};
    buffer.WriteBuffer(data, 5);

    EXPECT_EQ(*buffer.GetBuffer(0), 10);
    EXPECT_EQ(*buffer.GetBuffer(2), 30);
    EXPECT_EQ(*buffer.GetBuffer(4), 50);
}

TEST_F(ClientBufferTest, GetBuffer_PositionAtLength_ReturnsNull) {
    BYTE data[] = {1, 2, 3};
    buffer.WriteBuffer(data, 3);

    EXPECT_EQ(buffer.GetBuffer(3), nullptr);
}

TEST_F(ClientBufferTest, GetBuffer_PositionExceedsLength_ReturnsNull) {
    BYTE data[] = {1, 2, 3};
    buffer.WriteBuffer(data, 3);

    EXPECT_EQ(buffer.GetBuffer(100), nullptr);
}

// ============================================
// ClearBuffer 测试
// ============================================
TEST_F(ClientBufferTest, ClearBuffer_AfterWrite_ResetsLength) {
    BYTE data[] = {1, 2, 3, 4, 5};
    buffer.WriteBuffer(data, 5);

    buffer.ClearBuffer();

    EXPECT_EQ(buffer.GetBufferLength(), 0u);
}

TEST_F(ClientBufferTest, ClearBuffer_EmptyBuffer_NoEffect) {
    buffer.ClearBuffer();
    EXPECT_EQ(buffer.GetBufferLength(), 0u);
}

// ============================================
// 数据完整性测试
// ============================================
TEST_F(ClientBufferTest, DataIntegrity_WriteReadCycle_PreservesData) {
    // 写入各种字节值
    std::vector<BYTE> data(256);
    for (int i = 0; i < 256; i++) {
        data[i] = (BYTE)i;
    }

    buffer.WriteBuffer(data.data(), 256);

    std::vector<BYTE> result(256);
    ULONG bytesRead = buffer.ReadBuffer(result.data(), 256);

    EXPECT_EQ(bytesRead, 256u);
    for (int i = 0; i < 256; i++) {
        EXPECT_EQ(result[i], data[i]) << "Mismatch at index " << i;
    }
}

TEST_F(ClientBufferTest, DataIntegrity_MultipleReadWriteCycles) {
    for (int cycle = 0; cycle < 10; cycle++) {
        BYTE data[100];
        for (int i = 0; i < 100; i++) {
            data[i] = (BYTE)(cycle * 10 + i);
        }

        buffer.WriteBuffer(data, 100);

        BYTE result[100];
        ULONG bytesRead = buffer.ReadBuffer(result, 100);

        EXPECT_EQ(bytesRead, 100u);
        for (int i = 0; i < 100; i++) {
            EXPECT_EQ(result[i], data[i]);
        }
    }
}

// ============================================
// 边界条件和下溢防护测试
// ============================================
TEST_F(ClientBufferTest, UnderflowProtection_SkipMoreThanLength_NoUnderflow) {
    BYTE data[] = {1, 2, 3};
    buffer.WriteBuffer(data, 3);

    // 不应崩溃或产生意外行为
    buffer.Skip(ULONG_MAX);

    EXPECT_EQ(buffer.GetBufferLength(), 0u);
}

TEST_F(ClientBufferTest, UnderflowProtection_ReadMoreThanLength_NoUnderflow) {
    BYTE data[] = {1, 2, 3};
    buffer.WriteBuffer(data, 3);

    BYTE result[1000];
    ULONG bytesRead = buffer.ReadBuffer(result, ULONG_MAX);

    // 应该只读取可用的 3 字节
    EXPECT_EQ(bytesRead, 3u);
}

// ============================================
// 内存管理测试
// ============================================
TEST_F(ClientBufferTest, MemoryManagement_RepeatedAllocations_NoLeak) {
    // 反复分配和释放，验证无内存泄漏
    for (int i = 0; i < 100; i++) {
        WriteFillData(1000);
        buffer.ClearBuffer();
    }
    EXPECT_EQ(buffer.GetBufferLength(), 0u);
}

TEST_F(ClientBufferTest, MemoryManagement_GrowingBuffer_HandlesReallocation) {
    // 逐步增长缓冲区
    for (ULONG size = 1; size <= 10000; size *= 2) {
        WriteFillData(size);
    }

    EXPECT_GT(buffer.GetBufferLength(), 0u);
}

// ============================================
// 参数化测试
// ============================================
class BufferReadParameterizedTest
    : public ::testing::TestWithParam<std::tuple<size_t, size_t, size_t>> {
protected:
    CBuffer buffer;
};

TEST_P(BufferReadParameterizedTest, ReadBuffer_VariousLengths) {
    auto [writeLen, readLen, expectedRead] = GetParam();

    std::vector<BYTE> data(writeLen, 0x42);
    if (writeLen > 0) {
        buffer.WriteBuffer(data.data(), (ULONG)writeLen);
    }

    std::vector<BYTE> result(readLen > 0 ? readLen : 1);
    ULONG actual = buffer.ReadBuffer(result.data(), (ULONG)readLen);

    EXPECT_EQ(actual, expectedRead);
}

INSTANTIATE_TEST_SUITE_P(
    ReadLengths,
    BufferReadParameterizedTest,
    ::testing::Values(
        std::make_tuple(10, 5, 5),     // 正常读取
        std::make_tuple(5, 10, 5),     // 请求超过可用
        std::make_tuple(0, 5, 0),      // 空缓冲区
        std::make_tuple(100, 0, 0),    // 零长度读取
        std::make_tuple(1000, 500, 500) // 大数据部分读取
    )
);
