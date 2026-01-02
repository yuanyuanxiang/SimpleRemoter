#include "zstd_wrapper.h"
#include <string.h> // memcpy

size_t zstd_compress_auto(
    ZSTD_CCtx* cctx,
    void* dst, size_t dstCapacity,
    const void* src, size_t srcSize,
    size_t threshold
)
{
    // 妫€鏌ヨ緭鍏ユ湁鏁堟€?
    if (!cctx || !dst || !src) {
        return ZSTD_error_GENERIC;
    }

    // --- 灏忔暟鎹垨搴撲笉鏀寔澶氱嚎绋?鈫?閫€鍥炲埌鍗曠嚎绋?ZSTD_compress2 ---
    if (srcSize < threshold) {
        return ZSTD_compress2(cctx, dst, dstCapacity, src, srcSize);
    }

    // --- 澶氱嚎绋嬫祦寮忓帇缂?---
    ZSTD_inBuffer  input  = {src, srcSize, 0};
    ZSTD_outBuffer output = {dst, dstCapacity, 0};

    // 寰幆鍘嬬缉杈撳叆鏁版嵁
    size_t ret = 0;
    while (input.pos < input.size) {
        ret = ZSTD_compressStream2(cctx, &output, &input, ZSTD_e_continue);
        if (ZSTD_isError(ret)) break;

        // 杈撳嚭缂撳啿鍖哄凡婊★紙鐞嗚涓婁笉搴斿彂鐢燂紝鍥?dstCapacity >= ZSTD_compressBound锛?
        if (output.pos == output.size) {
            return ZSTD_error_dstSize_tooSmall;
        }
    }

    // 缁撴潫鍘嬬缉锛堢‘淇濇墍鏈夌嚎绋嬪畬鎴愶級
    if (!ZSTD_isError(ret)) {
        ret = ZSTD_compressStream2(cctx, &output, &input, ZSTD_e_end);
    }

    return ZSTD_isError(ret) ? ret : output.pos;
}
