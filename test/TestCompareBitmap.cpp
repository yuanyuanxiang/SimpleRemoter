// Image Diff Algorithm Benchmark
// Compile: cl /O2 /EHsc TestCompareBitmap.cpp
// Or: g++ -O2 -msse2 -o TestCompareBitmap.exe TestCompareBitmap.cpp

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <emmintrin.h>  // SSE2
#include <chrono>

typedef unsigned char BYTE;
typedef BYTE* LPBYTE;
typedef unsigned long ULONG;
typedef ULONG* LPDWORD;

#define ALGORITHM_DIFF 0
#define ALGORITHM_GRAY 1

//============================== Gray Conversion ==============================

inline void ConvertToGray_Original(LPBYTE dst, LPBYTE src, ULONG count)
{
    for (ULONG i = 0; i < count; i += 4, src += 4, dst++) {
        *dst = (306 * src[2] + 601 * src[0] + 117 * src[1]) >> 10;
    }
}

inline void ConvertToGray_SSE2(LPBYTE dst, LPBYTE src, ULONG count)
{
    ULONG pixels = count / 4;
    ULONG i = 0;
    ULONG aligned = pixels & ~3;

    for (; i < aligned; i += 4, src += 16, dst += 4) {
        dst[0] = (306 * src[2]  + 601 * src[0]  + 117 * src[1])  >> 10;
        dst[1] = (306 * src[6]  + 601 * src[4]  + 117 * src[5])  >> 10;
        dst[2] = (306 * src[10] + 601 * src[8]  + 117 * src[9])  >> 10;
        dst[3] = (306 * src[14] + 601 * src[12] + 117 * src[13]) >> 10;
    }

    for (; i < pixels; i++, src += 4, dst++) {
        *dst = (306 * src[2] + 601 * src[0] + 117 * src[1]) >> 10;
    }
}

void ToGray_Original(LPBYTE dst, LPBYTE src, int biSizeImage)
{
    for (ULONG i = 0; i < (ULONG)biSizeImage; i += 4, dst += 4, src += 4) {
        dst[0] = dst[1] = dst[2] = (306 * src[2] + 601 * src[0] + 117 * src[1]) >> 10;
    }
}

void ToGray_SSE2(LPBYTE dst, LPBYTE src, int biSizeImage)
{
    ULONG pixels = biSizeImage / 4;
    for (ULONG i = 0; i < pixels; i++, src += 4, dst += 4) {
        BYTE g = (306 * src[2] + 601 * src[0] + 117 * src[1]) >> 10;
        dst[0] = dst[1] = dst[2] = g;
        dst[3] = 0xFF;
    }
}

//============================== Original Version ==============================
ULONG CompareBitmap_Original(LPBYTE CompareSourData, LPBYTE CompareDestData, LPBYTE szBuffer,
                             DWORD ulCompareLength, BYTE algo, int startPostion = 0)
{
    LPDWORD p1 = (LPDWORD)CompareDestData, p2 = (LPDWORD)CompareSourData;
    LPBYTE p = szBuffer;
    ULONG channel = algo == ALGORITHM_GRAY ? 1 : 4;
    ULONG ratio = algo == ALGORITHM_GRAY ? 4 : 1;

    for (ULONG i = 0; i < ulCompareLength; i += 4, ++p1, ++p2) {
        if (*p1 == *p2)
            continue;
        ULONG index = i;
        LPDWORD pos1 = p1++, pos2 = p2++;
        for (i += 4; i < ulCompareLength && *p1 != *p2; i += 4, ++p1, ++p2);
        ULONG ulCount = i - index;
        memcpy(pos1, pos2, ulCount);

        *(LPDWORD)(p) = index + startPostion;
        *(LPDWORD)(p + sizeof(ULONG)) = ulCount / ratio;
        p += 2 * sizeof(ULONG);

        if (channel != 1) {
            memcpy(p, pos2, ulCount);
            p += ulCount;
        } else {
            for (LPBYTE end = p + ulCount / ratio; p < end; ++p, ++pos2) {
                LPBYTE src = (LPBYTE)pos2;
                *p = (306 * src[2] + 601 * src[0] + 117 * src[1]) >> 10;
            }
        }
    }

    return (ULONG)(p - szBuffer);
}

//============================== SSE2 Version ==============================
ULONG CompareBitmap_SSE2(LPBYTE CompareSourData, LPBYTE CompareDestData, LPBYTE szBuffer,
                         DWORD ulCompareLength, BYTE algo, int startPostion = 0)
{
    LPBYTE p = szBuffer;
    ULONG channel = algo == ALGORITHM_GRAY ? 1 : 4;
    ULONG ratio = algo == ALGORITHM_GRAY ? 4 : 1;

    const ULONG SSE_BLOCK = 16;
    const ULONG alignedLength = ulCompareLength & ~(SSE_BLOCK - 1);

    __m128i* v1 = (__m128i*)CompareDestData;
    __m128i* v2 = (__m128i*)CompareSourData;

    ULONG i = 0;
    while (i < alignedLength) {
        __m128i cmp = _mm_cmpeq_epi32(*v1, *v2);
        int mask = _mm_movemask_epi8(cmp);

        if (mask == 0xFFFF) {
            i += SSE_BLOCK;
            ++v1;
            ++v2;
            continue;
        }

        ULONG index = i;
        LPBYTE pos1 = (LPBYTE)v1;
        LPBYTE pos2 = (LPBYTE)v2;

        do {
            i += SSE_BLOCK;
            ++v1;
            ++v2;
            if (i >= alignedLength) break;
            cmp = _mm_cmpeq_epi32(*v1, *v2);
            mask = _mm_movemask_epi8(cmp);
        } while (mask != 0xFFFF);

        ULONG ulCount = i - index;
        memcpy(pos1, pos2, ulCount);

        *(LPDWORD)(p) = index + startPostion;
        *(LPDWORD)(p + sizeof(ULONG)) = ulCount / ratio;
        p += 2 * sizeof(ULONG);

        if (channel != 1) {
            memcpy(p, pos2, ulCount);
            p += ulCount;
        } else {
            ConvertToGray_SSE2(p, pos2, ulCount);
            p += ulCount / ratio;
        }
    }

    // Handle remaining bytes
    if (i < ulCompareLength) {
        LPDWORD p1 = (LPDWORD)((LPBYTE)CompareDestData + i);
        LPDWORD p2 = (LPDWORD)((LPBYTE)CompareSourData + i);

        for (; i < ulCompareLength; i += 4, ++p1, ++p2) {
            if (*p1 == *p2)
                continue;

            ULONG index = i;
            LPDWORD pos1 = p1++;
            LPDWORD pos2 = p2++;

            for (i += 4; i < ulCompareLength && *p1 != *p2; i += 4, ++p1, ++p2);
            ULONG ulCount = i - index;
            memcpy(pos1, pos2, ulCount);

            *(LPDWORD)(p) = index + startPostion;
            *(LPDWORD)(p + sizeof(ULONG)) = ulCount / ratio;
            p += 2 * sizeof(ULONG);

            if (channel != 1) {
                memcpy(p, pos2, ulCount);
                p += ulCount;
            } else {
                LPDWORD srcPtr = pos2;
                for (LPBYTE end = p + ulCount / ratio; p < end; ++p, ++srcPtr) {
                    LPBYTE src = (LPBYTE)srcPtr;
                    *p = (306 * src[2] + 601 * src[0] + 117 * src[1]) >> 10;
                }
            }
        }
    }

    return (ULONG)(p - szBuffer);
}

//============================== Benchmark ==============================
void RunBenchmark(int width, int height, float diffRatio, int iterations, BYTE algo = ALGORITHM_DIFF)
{
    ULONG dataSize = width * height * 4;

    LPBYTE srcBuffer = (LPBYTE)_aligned_malloc(dataSize, 16);
    LPBYTE dstBuffer = (LPBYTE)_aligned_malloc(dataSize, 16);
    LPBYTE outBuffer1 = (LPBYTE)_aligned_malloc(dataSize * 2, 16);
    LPBYTE outBuffer2 = (LPBYTE)_aligned_malloc(dataSize * 2, 16);

    if (!srcBuffer || !dstBuffer || !outBuffer1 || !outBuffer2) {
        printf("Memory allocation failed!\n");
        return;
    }

    srand(12345);
    for (ULONG i = 0; i < dataSize; i++) {
        srcBuffer[i] = rand() % 256;
        dstBuffer[i] = srcBuffer[i];
    }

    int diffPixels = (int)(width * height * diffRatio);
    for (int i = 0; i < diffPixels; i++) {
        int pos = (rand() % (width * height)) * 4;
        srcBuffer[pos] = rand() % 256;
        srcBuffer[pos + 1] = rand() % 256;
        srcBuffer[pos + 2] = rand() % 256;
    }

    printf("\n========== Test Parameters ==========\n");
    printf("Resolution: %d x %d\n", width, height);
    printf("Data size: %.2f MB\n", dataSize / 1024.0 / 1024.0);
    printf("Diff ratio: %.1f%%\n", diffRatio * 100);
    printf("Algorithm: %s\n", algo == ALGORITHM_GRAY ? "Gray" : "Color");
    printf("Iterations: %d\n", iterations);
    printf("======================================\n\n");

    // Test original version
    LPBYTE testDst1 = (LPBYTE)_aligned_malloc(dataSize, 16);
    memcpy(testDst1, dstBuffer, dataSize);

    auto start1 = std::chrono::high_resolution_clock::now();
    ULONG result1 = 0;
    for (int i = 0; i < iterations; i++) {
        memcpy(testDst1, dstBuffer, dataSize);
        result1 = CompareBitmap_Original(srcBuffer, testDst1, outBuffer1, dataSize, algo);
    }
    auto end1 = std::chrono::high_resolution_clock::now();
    double time1 = std::chrono::duration<double, std::milli>(end1 - start1).count();

    // Test SSE2 version
    LPBYTE testDst2 = (LPBYTE)_aligned_malloc(dataSize, 16);
    memcpy(testDst2, dstBuffer, dataSize);

    auto start2 = std::chrono::high_resolution_clock::now();
    ULONG result2 = 0;
    for (int i = 0; i < iterations; i++) {
        memcpy(testDst2, dstBuffer, dataSize);
        result2 = CompareBitmap_SSE2(srcBuffer, testDst2, outBuffer2, dataSize, algo);
    }
    auto end2 = std::chrono::high_resolution_clock::now();
    double time2 = std::chrono::duration<double, std::milli>(end2 - start2).count();

    printf("Original:\n");
    printf("  Total: %.2f ms\n", time1);
    printf("  Per frame: %.3f ms\n", time1 / iterations);
    printf("  Output size: %lu bytes\n\n", result1);

    printf("SSE2:\n");
    printf("  Total: %.2f ms\n", time2);
    printf("  Per frame: %.3f ms\n", time2 / iterations);
    printf("  Output size: %lu bytes\n\n", result2);

    printf("========== Performance ==========\n");
    printf("Speedup: %.2fx\n", time1 / time2);
    printf("Time saved: %.1f%%\n", (1.0 - time2 / time1) * 100);

    if (result1 == result2 && memcmp(outBuffer1, outBuffer2, result1) == 0) {
        printf("Verify: PASS\n");
    } else {
        printf("Verify: DIFF (size: %lu vs %lu)\n", result1, result2);
    }
    printf("=================================\n");

    _aligned_free(srcBuffer);
    _aligned_free(dstBuffer);
    _aligned_free(outBuffer1);
    _aligned_free(outBuffer2);
    _aligned_free(testDst1);
    _aligned_free(testDst2);
}

//============================== Gray Convert Benchmark ==============================
void RunGrayConvertBenchmark(int width, int height, int iterations)
{
    ULONG dataSize = width * height * 4;
    ULONG graySize = width * height;

    LPBYTE srcBuffer = (LPBYTE)_aligned_malloc(dataSize, 16);
    LPBYTE dstBuffer1 = (LPBYTE)_aligned_malloc(graySize, 16);
    LPBYTE dstBuffer2 = (LPBYTE)_aligned_malloc(graySize, 16);

    if (!srcBuffer || !dstBuffer1 || !dstBuffer2) {
        printf("Memory allocation failed!\n");
        return;
    }

    srand(12345);
    for (ULONG i = 0; i < dataSize; i++) {
        srcBuffer[i] = rand() % 256;
    }

    printf("\n========== BGRA->Gray Test ==========\n");
    printf("Resolution: %d x %d\n", width, height);
    printf("Input: %.2f MB, Output: %.2f MB\n", dataSize / 1024.0 / 1024.0, graySize / 1024.0 / 1024.0);
    printf("Iterations: %d\n", iterations);
    printf("=====================================\n\n");

    // Test original version
    auto start1 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; i++) {
        ConvertToGray_Original(dstBuffer1, srcBuffer, dataSize);
    }
    auto end1 = std::chrono::high_resolution_clock::now();
    double time1 = std::chrono::duration<double, std::milli>(end1 - start1).count();

    // Test SSE2 version
    auto start2 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; i++) {
        ConvertToGray_SSE2(dstBuffer2, srcBuffer, dataSize);
    }
    auto end2 = std::chrono::high_resolution_clock::now();
    double time2 = std::chrono::duration<double, std::milli>(end2 - start2).count();

    printf("Original (per-pixel):\n");
    printf("  Total: %.2f ms, Per frame: %.3f ms\n", time1, time1 / iterations);

    printf("\nSSE2 (4-pixel batch):\n");
    printf("  Total: %.2f ms, Per frame: %.3f ms\n", time2, time2 / iterations);

    printf("\n========== Performance ==========\n");
    printf("Speedup: %.2fx\n", time1 / time2);
    printf("Time saved: %.1f%%\n", (1.0 - time2 / time1) * 100);

    bool match = memcmp(dstBuffer1, dstBuffer2, graySize) == 0;
    printf("Verify: %s\n", match ? "PASS" : "FAIL");
    printf("=================================\n");

    _aligned_free(srcBuffer);
    _aligned_free(dstBuffer1);
    _aligned_free(dstBuffer2);
}

int main()
{
    printf("===== Image Diff Algorithm Benchmark =====\n");

    printf("\n\n########## Color Mode ##########\n");

    printf("\n[1080p 10%% diff - Color]");
    RunBenchmark(1920, 1080, 0.10f, 100, ALGORITHM_DIFF);

    printf("\n[1080p 30%% diff - Color]");
    RunBenchmark(1920, 1080, 0.30f, 100, ALGORITHM_DIFF);

    printf("\n\n########## Gray Mode ##########\n");

    printf("\n[1080p 10%% diff - Gray]");
    RunBenchmark(1920, 1080, 0.10f, 100, ALGORITHM_GRAY);

    printf("\n[1080p 30%% diff - Gray]");
    RunBenchmark(1920, 1080, 0.30f, 100, ALGORITHM_GRAY);

    printf("\n\n########## BGRA->Gray Conversion ##########\n");

    printf("\n[1080p BGRA->Gray]");
    RunGrayConvertBenchmark(1920, 1080, 100);

    printf("\n[4K BGRA->Gray]");
    RunGrayConvertBenchmark(3840, 2160, 50);

    printf("\nDone!\n");
    return 0;
}
