/**
 * @file lv_hash.c
 * @brief 统一内容哈希抽象（lv_hash）：SHA-256 / FNV-1a 字段级哈希 + 统一 hex 输出
 *
 * @details 在 sha256.c（lv_sha256_*）与 lv_utils.h（lv_fnv1a_update、
 *          lv_FNV64_OFFSET_BASIS）之上封装，收敛项目内多份手工逐字段哈希。
 *          字段混入宽度（sizeof(int)/sizeof(bool)）与 NULL→"(null)" 语义
 *          严格对齐既有实现，保证替换后哈希值逐字节不变。
 */

#include "lv/lv_hash.h"

#include "lv/config.h"    /* lv_FNV64_OFFSET_BASIS */
#include "lv/lv_utils.h"  /* lv_calloc, lv_fnv1a_update */

#include <stdio.h>
#include <string.h>

/* 小写 hex 转换（等价于逐字节 "%02x" 循环） */
static void lv_hash_hex_lower(char *out, const uint8_t *bytes, size_t n) {
    static const char kDigits[] = "0123456789abcdef";
    for (size_t i = 0; i < n; i++) {
        out[i * 2] = kDigits[bytes[i] >> 4];
        out[i * 2 + 1] = kDigits[bytes[i] & 0x0F];
    }
}

void lv_hash_init(lvHashCtx *ctx, lvHashAlgorithm algorithm) {
    if (!ctx)
        return;
    ctx->algorithm = algorithm;
    if (algorithm == LV_HASH_SHA256) {
        lv_sha256_init(&ctx->u.sha256);
    } else {
        ctx->u.fnv1a = lv_FNV64_OFFSET_BASIS;
    }
}

void lv_hash_update(lvHashCtx *ctx, const void *data, size_t len) {
    if (!ctx)
        return;
    if (ctx->algorithm == LV_HASH_SHA256) {
        lv_sha256_update(&ctx->u.sha256, (const uint8_t *) data, len);
    } else {
        ctx->u.fnv1a = lv_fnv1a_update(ctx->u.fnv1a, data, len);
    }
}

void lv_hash_str(lvHashCtx *ctx, const char *str) {
    if (str) {
        lv_hash_update(ctx, str, strlen(str));
    } else {
        lv_hash_update(ctx, "(null)", 6);
    }
}

void lv_hash_int32(lvHashCtx *ctx, int value) {
    lv_hash_update(ctx, &value, sizeof(value)); /* sizeof(int) 字节，与既有实现一致 */
}

void lv_hash_bool(lvHashCtx *ctx, bool value) {
    lv_hash_update(ctx, &value, sizeof(value)); /* sizeof(bool) 字节 */
}

size_t lv_hash_digest_size(const lvHashCtx *ctx) {
    if (!ctx)
        return 0;
    return ctx->algorithm == LV_HASH_SHA256 ? (size_t) 32 : (size_t) 8;
}

void lv_hash_to_hex(lvHashCtx *ctx, char *buf, size_t buf_size) {
    if (!ctx || !buf || buf_size == 0)
        return;
    const size_t digest = lv_hash_digest_size(ctx);
    if (buf_size < digest * 2 + 1) {
        buf[0] = '\0';
        return;
    }
    if (ctx->algorithm == LV_HASH_SHA256) {
        uint8_t hash[32];
        lv_sha256_final(&ctx->u.sha256, hash);
        lv_hash_hex_lower(buf, hash, 32);
    } else {
        snprintf(buf, buf_size, "%016llx", (unsigned long long) ctx->u.fnv1a);
    }
    buf[digest * 2] = '\0';
}

char *lv_hash_to_hex_alloc(lvHashCtx *ctx) {
    if (!ctx)
        return NULL;
    const size_t digest = lv_hash_digest_size(ctx);
    char *result = (char *) lv_calloc(digest * 2 + 1, 1);
    if (result) {
        lv_hash_to_hex(ctx, result, digest * 2 + 1);
    }
    return result;
}
