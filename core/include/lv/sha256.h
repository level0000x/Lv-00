#ifndef lv_SHA256_H
#define lv_SHA256_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

/* SHA-256 上下文结构体（共 108 字节） */
typedef struct {
    uint8_t  data[64];   /* 当前数据块 */
    uint32_t datalen;    /* data 内的有效字节数 */
    unsigned long long bitlen; /* 总位长 */
    uint32_t state[8];   /* 工作状态（A-H） */
} lvSha256Context;

/** 初始化 SHA-256 上下文 */
void lv_sha256_init(lvSha256Context *ctx);

/** 向 SHA-256 上下文写入数据 */
void lv_sha256_update(lvSha256Context *ctx, const uint8_t *data, size_t len);

/** 完成 SHA-256 计算，输出 32 字节摘要 */
void lv_sha256_final(lvSha256Context *ctx, uint8_t hash[32]);

/** 便捷函数：计算一段数据的 SHA-256 十六进制字符串（64 字符 + null） */
void lv_sha256_hex(const uint8_t *data, size_t len, char hex[65]);

/** 便捷函数：计算字符串的 SHA-256 十六进制字符串 */
void lv_sha256_string(const char *str, char hex[65]);

#ifdef __cplusplus
}
#endif

#endif /* lv_SHA256_H */
