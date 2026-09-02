#ifndef LV_HASH_H
#define LV_HASH_H

#ifdef __cplusplus
extern "C" {
#endif

/* 与其他公共头一致：防御性导出宏（实际导出由构建系统统一处理） */
#ifndef lv_PUBLIC_API
#define lv_PUBLIC_API
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "lv/sha256.h"
#include "lv_api_spec.h" /* lv_PUBLIC_API（K59） */

/* ============================================================
 * 统一内容哈希抽象（lv_hash）
 *
 * 收敛项目内多份「对结构化对象做字段级哈希」的实现：
 *   - 算法可选 SHA-256 或 FNV-1a 64 位（init 时选定，之后共用同一套
 *     update 调用序列）
 *   - 类型化字段辅助统一 NULL 语义与整型混入宽度，保证与既有实现
 *     逐字节一致
 *   - 统一 hex 输出（SHA-256: 64 字符小写；FNV-1a: 16 字符小写）
 * ============================================================ */

/** 支持的哈希算法 */
typedef enum {
    LV_HASH_SHA256 = 0, /**< SHA-256：32 字节摘要，64 字符 hex */
    LV_HASH_FNV1A = 1   /**< FNV-1a 64 位：8 字节摘要，16 字符 hex（%016llx） */
} lvHashAlgorithm;

/** 统一哈希上下文 */
typedef struct {
    lvHashAlgorithm algorithm;
    union {
        lvSha256Context sha256; /**< SHA-256 上下文 */
        uint64_t fnv1a;         /**< FNV-1a 滚动值（初值 lv_FNV64_OFFSET_BASIS） */
    } u;
} lvHashCtx;

/** 初始化哈希上下文（选定算法） */
lv_PUBLIC_API void lv_hash_init(lvHashCtx *ctx, lvHashAlgorithm algorithm);

/** 混入原始字节（SHA-256/FNV-1a 均按 len 字节原样混入） */
lv_PUBLIC_API void lv_hash_update(lvHashCtx *ctx, const void *data, size_t len);

/** 混入字符串；NULL 统一混入 "(null)"（长度 6） */
lv_PUBLIC_API void lv_hash_str(lvHashCtx *ctx, const char *str);

/** 混入 int（按 sizeof(int) 字节，与既有实现逐字节一致） */
lv_PUBLIC_API void lv_hash_int32(lvHashCtx *ctx, int value);

/** 混入 bool（按 sizeof(bool) 字节） */
lv_PUBLIC_API void lv_hash_bool(lvHashCtx *ctx, bool value);

/** 摘要字节数：SHA-256 → 32，FNV-1a → 8 */
lv_PUBLIC_API size_t lv_hash_digest_size(const lvHashCtx *ctx);

/**
 * 生成小写 hex 字符串（含 '\0'）。
 * SHA-256 需要 buf_size >= 65；FNV-1a 需要 buf_size >= 17。
 * 缓冲区不足时写入空字符串。
 */
lv_PUBLIC_API void lv_hash_to_hex(lvHashCtx *ctx, char *buf, size_t buf_size);

/** 便捷：lv_calloc 分配 hex 字符串并返回（调用方 lv_free 释放） */
lv_PUBLIC_API char *lv_hash_to_hex_alloc(lvHashCtx *ctx);

#ifdef __cplusplus
}
#endif

#endif /* LV_HASH_H */
