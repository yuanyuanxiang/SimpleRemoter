#ifndef ZSTD_WRAPPER_H
#define ZSTD_WRAPPER_H

#include "zstd/zstd.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 智能压缩函数（自动选择单线程/多线程）
 * @param cctx      压缩上下文（需提前创建）
 * @param dst       输出缓冲区
 * @param dstCapacity 输出缓冲区大小
 * @param src       输入数据
 * @param srcSize   输入数据大小
 * @param threshold 触发多线程的最小数据大小（建议 >= 1MB）
 * @return          压缩后的数据大小（错误码通过 ZSTD_isError() 检查）
 */
size_t zstd_compress_auto(
    ZSTD_CCtx* cctx,
    void* dst, size_t dstCapacity,
    const void* src, size_t srcSize,
    size_t threshold
);

#ifdef __cplusplus
}
#endif

#endif // ZSTD_WRAPPER_H
