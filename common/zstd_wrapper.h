#ifndef ZSTD_WRAPPER_H
#define ZSTD_WRAPPER_H

#include "zstd/zstd.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 鏅鸿兘鍘嬬缉鍑芥暟锛堣嚜鍔ㄩ€夋嫨鍗曠嚎绋?澶氱嚎绋嬶級
 * @param cctx      鍘嬬缉涓婁笅鏂囷紙闇€鎻愬墠鍒涘缓锛?
 * @param dst       杈撳嚭缂撳啿鍖?
 * @param dstCapacity 杈撳嚭缂撳啿鍖哄ぇ灏?
 * @param src       杈撳叆鏁版嵁
 * @param srcSize   杈撳叆鏁版嵁澶у皬
 * @param threshold 瑙﹀彂澶氱嚎绋嬬殑鏈€灏忔暟鎹ぇ灏忥紙寤鸿 >= 1MB锛?
 * @return          鍘嬬缉鍚庣殑鏁版嵁澶у皬锛堥敊璇爜閫氳繃 ZSTD_isError() 妫€鏌ワ級
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
