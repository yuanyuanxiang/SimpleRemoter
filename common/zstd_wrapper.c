#include "zstd_wrapper.h"
#include <string.h> // memcpy

size_t zstd_compress_auto(
    ZSTD_CCtx* cctx,
    void* dst, size_t dstCapacity,
    const void* src, size_t srcSize,
    size_t threshold
)
{
    // 检查输入有效性
    if (!cctx || !dst || !src) {
        return ZSTD_error_GENERIC;
    }

    // --- 小数据或库不支持多线程 → 退回到单线程 ZSTD_compress2 ---
    if (srcSize < threshold) {
        return ZSTD_compress2(cctx, dst, dstCapacity, src, srcSize);
    }

    // --- 多线程流式压缩 ---
    ZSTD_inBuffer  input = { src, srcSize, 0 };
    ZSTD_outBuffer output = { dst, dstCapacity, 0 };

    // 循环压缩输入数据
    size_t ret = 0;
    while (input.pos < input.size) {
        ret = ZSTD_compressStream2(cctx, &output, &input, ZSTD_e_continue);
        if (ZSTD_isError(ret)) break;

        // 输出缓冲区已满（理论上不应发生，因 dstCapacity >= ZSTD_compressBound）
        if (output.pos == output.size) {
            return ZSTD_error_dstSize_tooSmall;
        }
    }

    // 结束压缩（确保所有线程完成）
    if (!ZSTD_isError(ret)) {
        ret = ZSTD_compressStream2(cctx, &output, &input, ZSTD_e_end);
    }

    return ZSTD_isError(ret) ? ret : output.pos;
}
