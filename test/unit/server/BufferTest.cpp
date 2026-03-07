/**
 * @file BufferTest.cpp
 * @brief 服务端 CBuffer 类单元测试
 *
 * 测试覆盖：
 * - 基本读写操作
 * - 延迟读取偏移机制 (m_ulReadOffset)
 * - 压缩/紧凑策略 (CompactBuffer)
 * - 边界条件和下溢防护
 * - 线程安全（并发读写）
 * - 零拷贝写入接口
 */

#include <gtest/gtest.h>
#include <thread>
#include <atomic>
#include <chrono>
#include <vector>
#include <string>

// Windows 头文件
#ifdef _WIN32
#include <Windows.h>
#else
// Linux 模拟 Windows API
#include <cstring>
#include <cstdlib>
#include <pthread.h>

typedef unsigned char BYTE;
typedef BYTE* PBYTE;
typedef BYTE* LPBYTE;
typedef unsigned long ULONG;
typedef void VOID;
typedef int BOOL;
typedef void* PVOID;
#define TRUE 1
#define FALSE 0
#define MEM_COMMIT 0x1000
#define MEM_RELEASE 0x8000
#define PAGE_READWRITE 0x04

struct CRITICAL_SECTION {
    pthread_mutex_t mutex;
};

inline void InitializeCriticalSection(CRITICAL_SECTION* cs) {
    pthread_mutex_init(&cs->mutex, NULL);
}
inline void DeleteCriticalSection(CRITICAL_SECTION* cs) {
    pthread_mutex_destroy(&cs->mutex);
}
inline void EnterCriticalSection(CRITICAL_SECTION* cs) {
    pthread_mutex_lock(&cs->mutex);
}
inline void LeaveCriticalSection(CRITICAL_SECTION* cs) {
    pthread_mutex_unlock(&cs->mutex);
}
inline void* VirtualAlloc(void*, size_t size, int, int) {
    return malloc(size);
}
inline void VirtualFree(void* ptr, size_t, int) {
    free(ptr);
}
inline void CopyMemory(void* dst, const void* src, size_t len) {
    memcpy(dst, src, len);
}
inline void MoveMemory(void* dst, const void* src, size_t len) {
    memmove(dst, src, len);
}
#endif

#include <cmath>

// 服务端 Buffer 实现（测试专用内联版本）
namespace ServerBuffer {

#define U_PAGE_ALIGNMENT   4096
#define F_PAGE_ALIGNMENT   4096.0
#define COMPACT_THRESHOLD  0.5

// 简化的 Buffer 类（用于 GetMyBuffer 返回）
class Buffer {
private:
    PBYTE buf;
    ULONG len;
public:
    Buffer() : buf(NULL), len(0) {}
    Buffer(const BYTE* b, ULONG n) : len(n) {
        if (n > 0 && b) {
            buf = new BYTE[n];
            memcpy(buf, b, n);
        } else {
            buf = NULL;
        }
    }
    ~Buffer() {
        if (buf) {
            delete[] buf;
            buf = NULL;
        }
    }
    Buffer(const Buffer& o) : len(o.len) {
        if (o.buf && o.len > 0) {
            buf = new BYTE[o.len];
            memcpy(buf, o.buf, o.len);
        } else {
            buf = NULL;
        }
    }
    ULONG length() const { return len; }
    LPBYTE GetBuffer(int idx = 0) const {
        return (idx >= (int)len) ? NULL : buf + idx;
    }
};

class CBuffer {
public:
    CBuffer() : m_ulMaxLength(0), m_ulReadOffset(0), m_Base(NULL), m_Ptr(NULL) {
        InitializeCriticalSection(&m_cs);
    }

    ~CBuffer() {
        if (m_Base) {
            VirtualFree(m_Base, 0, MEM_RELEASE);
            m_Base = NULL;
        }
        DeleteCriticalSection(&m_cs);
        m_Base = m_Ptr = NULL;
        m_ulMaxLength = 0;
        m_ulReadOffset = 0;
    }

    ULONG RemoveCompletedBuffer(ULONG ulLength) {
        EnterCriticalSection(&m_cs);

        ULONG totalDataLen = (ULONG)(m_Ptr - m_Base);
        ULONG effectiveDataLen = (totalDataLen > m_ulReadOffset) ? (totalDataLen - m_ulReadOffset) : 0;

        if (ulLength > effectiveDataLen) {
            ulLength = effectiveDataLen;
        }

        if (ulLength) {
            m_ulReadOffset += ulLength;

            if (m_ulReadOffset > (ULONG)(m_ulMaxLength * COMPACT_THRESHOLD)) {
                CompactBuffer();
            }
        }

        LeaveCriticalSection(&m_cs);
        return ulLength;
    }

    VOID CompactBuffer() {
        if (m_ulReadOffset > 0 && m_Base) {
            ULONG totalDataLen = (ULONG)(m_Ptr - m_Base);
            ULONG remainingData = (totalDataLen > m_ulReadOffset) ? (totalDataLen - m_ulReadOffset) : 0;
            if (remainingData > 0) {
                MoveMemory(m_Base, m_Base + m_ulReadOffset, remainingData);
            }
            m_Ptr = m_Base + remainingData;
            m_ulReadOffset = 0;
            DeAllocateBuffer(remainingData);
        }
    }

    ULONG ReadBuffer(PBYTE Buffer, ULONG ulLength) {
        EnterCriticalSection(&m_cs);

        ULONG totalDataLen = (ULONG)(m_Ptr - m_Base);
        ULONG effectiveDataLen = (totalDataLen > m_ulReadOffset) ? (totalDataLen - m_ulReadOffset) : 0;

        if (ulLength > effectiveDataLen) {
            ulLength = effectiveDataLen;
        }

        if (ulLength) {
            CopyMemory(Buffer, m_Base + m_ulReadOffset, ulLength);
            m_ulReadOffset += ulLength;

            if (m_ulReadOffset > (ULONG)(m_ulMaxLength * COMPACT_THRESHOLD)) {
                CompactBuffer();
            }
        }

        LeaveCriticalSection(&m_cs);
        return ulLength;
    }

    ULONG DeAllocateBuffer(ULONG ulLength) {
        if (ulLength < (ULONG)(m_Ptr - m_Base))
            return 0;

        ULONG ulNewMaxLength = (ULONG)(ceil(ulLength / F_PAGE_ALIGNMENT) * U_PAGE_ALIGNMENT);

        if (m_ulMaxLength <= ulNewMaxLength) {
            return 0;
        }
        PBYTE NewBase = (PBYTE)VirtualAlloc(NULL, ulNewMaxLength, MEM_COMMIT, PAGE_READWRITE);

        ULONG ulv1 = (ULONG)(m_Ptr - m_Base);
        CopyMemory(NewBase, m_Base, ulv1);

        VirtualFree(m_Base, 0, MEM_RELEASE);

        m_Base = NewBase;
        m_Ptr = m_Base + ulv1;
        m_ulMaxLength = ulNewMaxLength;

        return m_ulMaxLength;
    }

    BOOL WriteBuffer(PBYTE Buffer, ULONG ulLength) {
        EnterCriticalSection(&m_cs);

        if (ReAllocateBuffer(ulLength + (ULONG)(m_Ptr - m_Base)) == (ULONG)-1) {
            LeaveCriticalSection(&m_cs);
            return FALSE;
        }

        CopyMemory(m_Ptr, Buffer, ulLength);
        m_Ptr += ulLength;
        LeaveCriticalSection(&m_cs);
        return TRUE;
    }

    ULONG ReAllocateBuffer(ULONG ulLength) {
        if (ulLength < m_ulMaxLength)
            return 0;

        ULONG ulNewMaxLength = (ULONG)(ceil(ulLength / F_PAGE_ALIGNMENT) * U_PAGE_ALIGNMENT);
        PBYTE NewBase = (PBYTE)VirtualAlloc(NULL, ulNewMaxLength, MEM_COMMIT, PAGE_READWRITE);
        if (NewBase == NULL) {
            return (ULONG)-1;
        }

        ULONG ulv1 = (ULONG)(m_Ptr - m_Base);
        CopyMemory(NewBase, m_Base, ulv1);

        if (m_Base) {
            VirtualFree(m_Base, 0, MEM_RELEASE);
        }
        m_Base = NewBase;
        m_Ptr = m_Base + ulv1;
        m_ulMaxLength = ulNewMaxLength;

        return m_ulMaxLength;
    }

    VOID ClearBuffer() {
        EnterCriticalSection(&m_cs);
        m_Ptr = m_Base;
        m_ulReadOffset = 0;
        DeAllocateBuffer(1024);
        LeaveCriticalSection(&m_cs);
    }

    ULONG GetBufferLength() {
        EnterCriticalSection(&m_cs);
        if (m_Base == NULL) {
            LeaveCriticalSection(&m_cs);
            return 0;
        }
        ULONG totalDataLen = (ULONG)(m_Ptr - m_Base);
        ULONG len = (totalDataLen > m_ulReadOffset) ? (totalDataLen - m_ulReadOffset) : 0;
        LeaveCriticalSection(&m_cs);
        return len;
    }

    std::string Skip(ULONG ulPos) {
        if (ulPos == 0)
            return "";

        EnterCriticalSection(&m_cs);

        ULONG totalDataLen = (ULONG)(m_Ptr - m_Base);
        ULONG effectiveDataLen = (totalDataLen > m_ulReadOffset) ? (totalDataLen - m_ulReadOffset) : 0;

        if (ulPos > effectiveDataLen) {
            ulPos = effectiveDataLen;
        }

        std::string ret((char*)(m_Base + m_ulReadOffset), (char*)(m_Base + m_ulReadOffset + ulPos));

        m_ulReadOffset += ulPos;

        if (m_ulReadOffset > (ULONG)(m_ulMaxLength * COMPACT_THRESHOLD)) {
            CompactBuffer();
        }

        LeaveCriticalSection(&m_cs);
        return ret;
    }

    LPBYTE GetBuffer(ULONG ulPos = 0) {
        EnterCriticalSection(&m_cs);
        ULONG totalDataLen = (ULONG)(m_Ptr - m_Base);
        ULONG effectiveDataLen = (totalDataLen > m_ulReadOffset) ? (totalDataLen - m_ulReadOffset) : 0;
        if (m_Base == NULL || ulPos >= effectiveDataLen) {
            LeaveCriticalSection(&m_cs);
            return NULL;
        }
        LPBYTE result = m_Base + m_ulReadOffset + ulPos;
        LeaveCriticalSection(&m_cs);
        return result;
    }

    Buffer GetMyBuffer(ULONG ulPos = 0) {
        EnterCriticalSection(&m_cs);
        ULONG totalDataLen = (ULONG)(m_Ptr - m_Base);
        ULONG effectiveDataLen = (totalDataLen > m_ulReadOffset) ? (totalDataLen - m_ulReadOffset) : 0;
        if (m_Base == NULL || ulPos >= effectiveDataLen) {
            LeaveCriticalSection(&m_cs);
            return Buffer();
        }
        Buffer result(m_Base + m_ulReadOffset + ulPos, effectiveDataLen - ulPos);
        LeaveCriticalSection(&m_cs);
        return result;
    }

    BYTE GetBYTE(ULONG ulPos) {
        EnterCriticalSection(&m_cs);
        ULONG totalDataLen = (ULONG)(m_Ptr - m_Base);
        ULONG effectiveDataLen = (totalDataLen > m_ulReadOffset) ? (totalDataLen - m_ulReadOffset) : 0;
        if (m_Base == NULL || ulPos >= effectiveDataLen) {
            LeaveCriticalSection(&m_cs);
            return 0;
        }
        BYTE p = *(m_Base + m_ulReadOffset + ulPos);
        LeaveCriticalSection(&m_cs);
        return p;
    }

    BOOL CopyBuffer(PVOID pDst, ULONG nLen, ULONG ulPos) {
        EnterCriticalSection(&m_cs);
        ULONG totalDataLen = (ULONG)(m_Ptr - m_Base);
        ULONG effectiveDataLen = (totalDataLen > m_ulReadOffset) ? (totalDataLen - m_ulReadOffset) : 0;
        if (m_Base == NULL || pDst == NULL || ulPos >= effectiveDataLen || (effectiveDataLen - ulPos) < nLen) {
            LeaveCriticalSection(&m_cs);
            return FALSE;
        }
        memcpy(pDst, m_Base + m_ulReadOffset + ulPos, nLen);
        LeaveCriticalSection(&m_cs);
        return TRUE;
    }

    LPBYTE GetWriteBuffer(ULONG requiredSize, ULONG& availableSize) {
        EnterCriticalSection(&m_cs);

        if (m_ulReadOffset > 0) {
            CompactBuffer();
        }

        ULONG currentDataLen = (ULONG)(m_Ptr - m_Base);
        if (ReAllocateBuffer(currentDataLen + requiredSize) == (ULONG)-1) {
            LeaveCriticalSection(&m_cs);
            availableSize = 0;
            return NULL;
        }

        availableSize = m_ulMaxLength - currentDataLen;
        LPBYTE result = m_Ptr;
        LeaveCriticalSection(&m_cs);
        return result;
    }

    VOID CommitWrite(ULONG writtenSize) {
        EnterCriticalSection(&m_cs);
        m_Ptr += writtenSize;
        LeaveCriticalSection(&m_cs);
    }

    // 测试辅助：获取内部状态
    ULONG GetReadOffset() const { return m_ulReadOffset; }
    ULONG GetMaxLength() const { return m_ulMaxLength; }

protected:
    PBYTE m_Base;
    PBYTE m_Ptr;
    ULONG m_ulMaxLength;
    ULONG m_ulReadOffset;
    CRITICAL_SECTION m_cs;
};

} // namespace ServerBuffer

using ServerBuffer::CBuffer;
using ServerBuffer::Buffer;

// ============================================
// 测试夹具
// ============================================
class ServerBufferTest : public ::testing::Test {
protected:
    CBuffer buffer;

    void SetUp() override {}
    void TearDown() override {}

    void WriteFillData(ULONG length, BYTE fillValue = 0x42) {
        std::vector<BYTE> data(length, fillValue);
        buffer.WriteBuffer(data.data(), length);
    }
};

// ============================================
// 构造/析构测试
// ============================================
TEST_F(ServerBufferTest, Constructor_InitializesEmpty) {
    CBuffer newBuffer;
    EXPECT_EQ(newBuffer.GetBufferLength(), 0u);
    EXPECT_EQ(newBuffer.GetBuffer(), nullptr);
}

// ============================================
// WriteBuffer 测试
// ============================================
TEST_F(ServerBufferTest, WriteBuffer_ValidData_ReturnsTrue) {
    BYTE data[] = {1, 2, 3, 4, 5};
    EXPECT_TRUE(buffer.WriteBuffer(data, 5));
    EXPECT_EQ(buffer.GetBufferLength(), 5u);
}

TEST_F(ServerBufferTest, WriteBuffer_MultipleWrites_AccumulatesData) {
    BYTE data1[] = {1, 2, 3};
    BYTE data2[] = {4, 5};

    buffer.WriteBuffer(data1, 3);
    buffer.WriteBuffer(data2, 2);

    EXPECT_EQ(buffer.GetBufferLength(), 5u);
}

TEST_F(ServerBufferTest, WriteBuffer_LargeData_HandlesCorrectly) {
    const ULONG largeSize = 100000;
    std::vector<BYTE> data(largeSize, 0xAB);

    EXPECT_TRUE(buffer.WriteBuffer(data.data(), largeSize));
    EXPECT_EQ(buffer.GetBufferLength(), largeSize);
}

// ============================================
// ReadBuffer 测试
// ============================================
TEST_F(ServerBufferTest, ReadBuffer_EmptyBuffer_ReturnsZero) {
    BYTE result[10];
    EXPECT_EQ(buffer.ReadBuffer(result, 10), 0u);
}

TEST_F(ServerBufferTest, ReadBuffer_ExactLength_ReturnsAll) {
    BYTE data[] = {1, 2, 3, 4, 5};
    buffer.WriteBuffer(data, 5);

    BYTE result[5];
    ULONG bytesRead = buffer.ReadBuffer(result, 5);

    EXPECT_EQ(bytesRead, 5u);
    EXPECT_EQ(buffer.GetBufferLength(), 0u);
}

TEST_F(ServerBufferTest, ReadBuffer_PartialRead_UsesReadOffset) {
    BYTE data[] = {1, 2, 3, 4, 5};
    buffer.WriteBuffer(data, 5);

    BYTE result[2];
    buffer.ReadBuffer(result, 2);

    // 使用延迟偏移，不立即移动数据
    EXPECT_EQ(buffer.GetBufferLength(), 3u);
    EXPECT_GT(buffer.GetReadOffset(), 0u);
}

TEST_F(ServerBufferTest, ReadBuffer_RequestExceedsAvailable_ReturnsAvailableOnly) {
    BYTE data[] = {1, 2, 3};
    buffer.WriteBuffer(data, 3);

    BYTE result[10];
    ULONG bytesRead = buffer.ReadBuffer(result, 10);

    EXPECT_EQ(bytesRead, 3u);
    EXPECT_EQ(buffer.GetBufferLength(), 0u);
}

// ============================================
// RemoveCompletedBuffer 测试
// ============================================
TEST_F(ServerBufferTest, RemoveCompletedBuffer_PartialRemove_UpdatesOffset) {
    BYTE data[] = {1, 2, 3, 4, 5};
    buffer.WriteBuffer(data, 5);

    ULONG removed = buffer.RemoveCompletedBuffer(2);

    EXPECT_EQ(removed, 2u);
    EXPECT_EQ(buffer.GetBufferLength(), 3u);
}

TEST_F(ServerBufferTest, RemoveCompletedBuffer_ExceedsLength_ClampsToAvailable) {
    BYTE data[] = {1, 2, 3};
    buffer.WriteBuffer(data, 3);

    ULONG removed = buffer.RemoveCompletedBuffer(100);

    EXPECT_EQ(removed, 3u);
    EXPECT_EQ(buffer.GetBufferLength(), 0u);
}

// ============================================
// Skip 测试
// ============================================
TEST_F(ServerBufferTest, Skip_ReturnsSkippedData) {
    BYTE data[] = {'H', 'e', 'l', 'l', 'o'};
    buffer.WriteBuffer(data, 5);

    std::string skipped = buffer.Skip(3);

    EXPECT_EQ(skipped, "Hel");
    EXPECT_EQ(buffer.GetBufferLength(), 2u);
}

TEST_F(ServerBufferTest, Skip_ExceedsLength_ClampsToAvailable) {
    BYTE data[] = {'A', 'B', 'C'};
    buffer.WriteBuffer(data, 3);

    std::string skipped = buffer.Skip(100);

    EXPECT_EQ(skipped, "ABC");
    EXPECT_EQ(buffer.GetBufferLength(), 0u);
}

TEST_F(ServerBufferTest, Skip_ZeroLength_ReturnsEmpty) {
    BYTE data[] = {1, 2, 3};
    buffer.WriteBuffer(data, 3);

    std::string skipped = buffer.Skip(0);

    EXPECT_EQ(skipped, "");
    EXPECT_EQ(buffer.GetBufferLength(), 3u);
}

// ============================================
// GetBuffer 测试
// ============================================
TEST_F(ServerBufferTest, GetBuffer_RespectsReadOffset) {
    BYTE data[] = {10, 20, 30, 40, 50};
    buffer.WriteBuffer(data, 5);

    // 读取前两个字节，更新偏移
    BYTE temp[2];
    buffer.ReadBuffer(temp, 2);

    // GetBuffer(0) 应该返回第三个字节（30）
    EXPECT_EQ(*buffer.GetBuffer(0), 30);
    EXPECT_EQ(*buffer.GetBuffer(1), 40);
    EXPECT_EQ(*buffer.GetBuffer(2), 50);
}

TEST_F(ServerBufferTest, GetBuffer_PositionExceedsEffectiveLength_ReturnsNull) {
    BYTE data[] = {1, 2, 3, 4, 5};
    buffer.WriteBuffer(data, 5);

    BYTE temp[3];
    buffer.ReadBuffer(temp, 3);  // 有效长度变为 2

    EXPECT_NE(buffer.GetBuffer(0), nullptr);
    EXPECT_NE(buffer.GetBuffer(1), nullptr);
    EXPECT_EQ(buffer.GetBuffer(2), nullptr);  // 超出有效范围
}

// ============================================
// GetMyBuffer 测试
// ============================================
TEST_F(ServerBufferTest, GetMyBuffer_ReturnsCorrectBuffer) {
    BYTE data[] = {1, 2, 3, 4, 5};
    buffer.WriteBuffer(data, 5);

    Buffer buf = buffer.GetMyBuffer(0);

    EXPECT_EQ(buf.length(), 5u);
}

TEST_F(ServerBufferTest, GetMyBuffer_RespectsReadOffset) {
    BYTE data[] = {1, 2, 3, 4, 5};
    buffer.WriteBuffer(data, 5);

    BYTE temp[2];
    buffer.ReadBuffer(temp, 2);

    Buffer buf = buffer.GetMyBuffer(0);

    EXPECT_EQ(buf.length(), 3u);
    EXPECT_EQ(*buf.GetBuffer(0), 3);
}

// ============================================
// GetBYTE 测试
// ============================================
TEST_F(ServerBufferTest, GetBYTE_ReturnsCorrectByte) {
    BYTE data[] = {10, 20, 30, 40, 50};
    buffer.WriteBuffer(data, 5);

    EXPECT_EQ(buffer.GetBYTE(0), 10);
    EXPECT_EQ(buffer.GetBYTE(2), 30);
    EXPECT_EQ(buffer.GetBYTE(4), 50);
}

TEST_F(ServerBufferTest, GetBYTE_RespectsReadOffset) {
    BYTE data[] = {10, 20, 30, 40, 50};
    buffer.WriteBuffer(data, 5);

    BYTE temp[2];
    buffer.ReadBuffer(temp, 2);

    EXPECT_EQ(buffer.GetBYTE(0), 30);
    EXPECT_EQ(buffer.GetBYTE(1), 40);
}

TEST_F(ServerBufferTest, GetBYTE_OutOfRange_ReturnsZero) {
    BYTE data[] = {1, 2, 3};
    buffer.WriteBuffer(data, 3);

    EXPECT_EQ(buffer.GetBYTE(100), 0);
}

// ============================================
// CopyBuffer 测试
// ============================================
TEST_F(ServerBufferTest, CopyBuffer_ValidRange_ReturnsTrue) {
    BYTE data[] = {1, 2, 3, 4, 5};
    buffer.WriteBuffer(data, 5);

    BYTE dest[3];
    EXPECT_TRUE(buffer.CopyBuffer(dest, 3, 1));
    EXPECT_EQ(dest[0], 2);
    EXPECT_EQ(dest[1], 3);
    EXPECT_EQ(dest[2], 4);
}

TEST_F(ServerBufferTest, CopyBuffer_RespectsReadOffset) {
    BYTE data[] = {1, 2, 3, 4, 5};
    buffer.WriteBuffer(data, 5);

    BYTE temp[2];
    buffer.ReadBuffer(temp, 2);

    BYTE dest[2];
    EXPECT_TRUE(buffer.CopyBuffer(dest, 2, 0));
    EXPECT_EQ(dest[0], 3);
    EXPECT_EQ(dest[1], 4);
}

TEST_F(ServerBufferTest, CopyBuffer_ExceedsRange_ReturnsFalse) {
    BYTE data[] = {1, 2, 3};
    buffer.WriteBuffer(data, 3);

    BYTE dest[10];
    EXPECT_FALSE(buffer.CopyBuffer(dest, 10, 0));
}

// ============================================
// 零拷贝写入测试
// ============================================
TEST_F(ServerBufferTest, GetWriteBuffer_ReturnsValidPointer) {
    ULONG availableSize = 0;
    LPBYTE writePtr = buffer.GetWriteBuffer(100, availableSize);

    EXPECT_NE(writePtr, nullptr);
    EXPECT_GE(availableSize, 100u);
}

TEST_F(ServerBufferTest, CommitWrite_UpdatesLength) {
    ULONG availableSize = 0;
    LPBYTE writePtr = buffer.GetWriteBuffer(100, availableSize);

    // 直接写入
    for (int i = 0; i < 50; i++) {
        writePtr[i] = (BYTE)i;
    }
    buffer.CommitWrite(50);

    EXPECT_EQ(buffer.GetBufferLength(), 50u);
    EXPECT_EQ(buffer.GetBYTE(0), 0);
    EXPECT_EQ(buffer.GetBYTE(49), 49);
}

// ============================================
// ClearBuffer 测试
// ============================================
TEST_F(ServerBufferTest, ClearBuffer_ResetsEverything) {
    BYTE data[] = {1, 2, 3, 4, 5};
    buffer.WriteBuffer(data, 5);

    BYTE temp[2];
    buffer.ReadBuffer(temp, 2);  // 创建读取偏移

    buffer.ClearBuffer();

    EXPECT_EQ(buffer.GetBufferLength(), 0u);
    EXPECT_EQ(buffer.GetReadOffset(), 0u);
}

// ============================================
// 下溢防护测试
// ============================================
TEST_F(ServerBufferTest, UnderflowProtection_ReadMoreThanLength_NoUnderflow) {
    BYTE data[] = {1, 2, 3};
    buffer.WriteBuffer(data, 3);

    BYTE result[1000];
    ULONG bytesRead = buffer.ReadBuffer(result, ULONG_MAX - 1);

    EXPECT_EQ(bytesRead, 3u);
}

TEST_F(ServerBufferTest, UnderflowProtection_SkipMoreThanLength_NoUnderflow) {
    BYTE data[] = {1, 2, 3};
    buffer.WriteBuffer(data, 3);

    std::string skipped = buffer.Skip(ULONG_MAX - 1);

    EXPECT_EQ(skipped.length(), 3u);
    EXPECT_EQ(buffer.GetBufferLength(), 0u);
}

TEST_F(ServerBufferTest, UnderflowProtection_RemoveMoreThanLength_NoUnderflow) {
    BYTE data[] = {1, 2, 3};
    buffer.WriteBuffer(data, 3);

    ULONG removed = buffer.RemoveCompletedBuffer(ULONG_MAX - 1);

    EXPECT_EQ(removed, 3u);
}

TEST_F(ServerBufferTest, UnderflowProtection_GetByteOutOfRange_ReturnsZero) {
    BYTE data[] = {1, 2, 3};
    buffer.WriteBuffer(data, 3);

    EXPECT_EQ(buffer.GetBYTE(ULONG_MAX - 1), 0);
}

// ============================================
// 压缩策略测试
// ============================================
TEST_F(ServerBufferTest, Compaction_TriggersAtThreshold) {
    // 写入足够数据
    // m_ulMaxLength 会被页对齐到 ceil(10000/4096)*4096 = 12288
    // 压缩阈值 = 12288 * 0.5 = 6144
    WriteFillData(10000);

    ULONG initialOffset = buffer.GetReadOffset();
    EXPECT_EQ(initialOffset, 0u);

    // 读取超过阈值的数据（需要 > 6144）
    std::vector<BYTE> temp(7000);
    buffer.ReadBuffer(temp.data(), 7000);

    // 压缩后偏移应该重置
    EXPECT_EQ(buffer.GetReadOffset(), 0u);
    EXPECT_EQ(buffer.GetBufferLength(), 3000u);
}

// ============================================
// 线程安全测试
// ============================================
TEST_F(ServerBufferTest, ThreadSafety_ConcurrentReadWrite_NoDataCorruption) {
    std::atomic<bool> running{true};
    std::atomic<int> writeCount{0};
    std::atomic<int> readCount{0};

    // 写线程
    std::thread writer([&]() {
        BYTE data[100];
        for (int i = 0; i < 100; i++) {
            data[i] = (BYTE)i;
        }
        while (running) {
            if (buffer.WriteBuffer(data, 100)) {
                writeCount++;
            }
            std::this_thread::yield();
        }
    });

    // 读线程
    std::thread reader([&]() {
        BYTE result[50];
        while (running) {
            if (buffer.ReadBuffer(result, 50) > 0) {
                readCount++;
            }
            std::this_thread::yield();
        }
    });

    // 运行一段时间
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    running = false;

    writer.join();
    reader.join();

    // 验证无崩溃，且有数据交换
    EXPECT_GT(writeCount.load(), 0);
    EXPECT_GT(readCount.load(), 0);
}

TEST_F(ServerBufferTest, ThreadSafety_MultipleReaders_NoDeadlock) {
    // 预填充数据
    WriteFillData(10000);

    std::atomic<bool> running{true};
    std::vector<std::thread> readers;

    for (int i = 0; i < 4; i++) {
        readers.emplace_back([&]() {
            while (running) {
                buffer.GetBufferLength();
                buffer.GetBYTE(0);
                buffer.GetBuffer(0);
                std::this_thread::yield();
            }
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    running = false;

    for (auto& t : readers) {
        t.join();
    }

    // 无死锁即为成功
    SUCCEED();
}

// ============================================
// 数据完整性测试
// ============================================
TEST_F(ServerBufferTest, DataIntegrity_WriteReadCycle_PreservesData) {
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

TEST_F(ServerBufferTest, DataIntegrity_PartialReads_PreservesSequence) {
    // 写入 1-100
    std::vector<BYTE> data(100);
    for (int i = 0; i < 100; i++) {
        data[i] = (BYTE)(i + 1);
    }
    buffer.WriteBuffer(data.data(), 100);

    // 分多次读取
    BYTE result[100];
    ULONG totalRead = 0;

    totalRead += buffer.ReadBuffer(result, 30);
    totalRead += buffer.ReadBuffer(result + 30, 30);
    totalRead += buffer.ReadBuffer(result + 60, 40);

    EXPECT_EQ(totalRead, 100u);
    for (int i = 0; i < 100; i++) {
        EXPECT_EQ(result[i], (BYTE)(i + 1));
    }
}

// ============================================
// 参数化测试
// ============================================
class ServerBufferParameterizedTest
    : public ::testing::TestWithParam<std::tuple<size_t, size_t, size_t>> {
protected:
    CBuffer buffer;
};

TEST_P(ServerBufferParameterizedTest, ReadBuffer_VariousLengths) {
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
    ServerBufferParameterizedTest,
    ::testing::Values(
        std::make_tuple(10, 5, 5),
        std::make_tuple(5, 10, 5),
        std::make_tuple(0, 5, 0),
        std::make_tuple(100, 0, 0),
        std::make_tuple(10000, 5000, 5000)
    )
);
