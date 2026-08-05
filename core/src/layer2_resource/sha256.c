/**
 * @file sha256.c
 * @brief 共享 SHA-256 实现（FIPS 180-4）
 *
 * @details 从 axiom_pkg.c 提取的独立实现，供项目各模块共用。
 *          遵循 lv_ 命名前缀约定。
 */

#include "lv/sha256.h"

#include <stdio.h>
#include <string.h>

/* ============== SHA-256 内部常量与宏 ============== */

static const uint32_t sha256_k[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

static inline uint32_t sha256_rotr(uint32_t x, unsigned int n) {
    return (x >> n) | (x << (32 - n));
}
static inline uint32_t sha256_ch(uint32_t x, uint32_t y, uint32_t z) {
    return (x & y) ^ (~x & z);
}
static inline uint32_t sha256_maj(uint32_t x, uint32_t y, uint32_t z) {
    return (x & y) ^ (x & z) ^ (y & z);
}
static inline uint32_t sha256_big_sigma0(uint32_t x) {
    return sha256_rotr(x, 2) ^ sha256_rotr(x, 13) ^ sha256_rotr(x, 22);
}
static inline uint32_t sha256_big_sigma1(uint32_t x) {
    return sha256_rotr(x, 6) ^ sha256_rotr(x, 11) ^ sha256_rotr(x, 25);
}
static inline uint32_t sha256_small_sigma0(uint32_t x) {
    return sha256_rotr(x, 7) ^ sha256_rotr(x, 18) ^ (x >> 3);
}
static inline uint32_t sha256_small_sigma1(uint32_t x) {
    return sha256_rotr(x, 17) ^ sha256_rotr(x, 19) ^ (x >> 10);
}

/* ============== 内部变换函数 ============== */

static void sha256_transform(lvSha256Context *ctx, const uint8_t block[64]) {
    uint32_t w[64];
    int i;

    /* 准备消息调度 */
    for (i = 0; i < 16; i++) {
        w[i] = ((uint32_t) block[i * 4] << 24) | ((uint32_t) block[i * 4 + 1] << 16) |
               ((uint32_t) block[i * 4 + 2] << 8) | ((uint32_t) block[i * 4 + 3]);
    }
    for (i = 16; i < 64; i++) {
        w[i] = sha256_small_sigma1(w[i - 2]) + w[i - 7] + sha256_small_sigma0(w[i - 15]) + w[i - 16];
    }

    /* 初始化工作变量 */
    uint32_t a = ctx->state[0];
    uint32_t b = ctx->state[1];
    uint32_t c = ctx->state[2];
    uint32_t d = ctx->state[3];
    uint32_t e = ctx->state[4];
    uint32_t f = ctx->state[5];
    uint32_t g = ctx->state[6];
    uint32_t h = ctx->state[7];

    /* 主压缩循环 */
    for (i = 0; i < 64; i++) {
        uint32_t t1 = h + sha256_big_sigma1(e) + sha256_ch(e, f, g) + sha256_k[i] + w[i];
        uint32_t t2 = sha256_big_sigma0(a) + sha256_maj(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }

    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
    ctx->state[5] += f;
    ctx->state[6] += g;
    ctx->state[7] += h;
}

/* ============== 公开 API ============== */

void lv_sha256_init(lvSha256Context *ctx) {
    ctx->state[0] = 0x6a09e667;
    ctx->state[1] = 0xbb67ae85;
    ctx->state[2] = 0x3c6ef372;
    ctx->state[3] = 0xa54ff53a;
    ctx->state[4] = 0x510e527f;
    ctx->state[5] = 0x9b05688c;
    ctx->state[6] = 0x1f83d9ab;
    ctx->state[7] = 0x5be0cd19;
    ctx->bitlen = 0;
    ctx->datalen = 0;
}

void lv_sha256_update(lvSha256Context *ctx, const uint8_t *data, size_t len) {
    size_t i;

    ctx->bitlen += (unsigned long long) len * 8;

    for (i = 0; i < len; i++) {
        ctx->data[ctx->datalen++] = data[i];
        if (ctx->datalen == 64) {
            sha256_transform(ctx, ctx->data);
            ctx->datalen = 0;
        }
    }
}

void lv_sha256_final(lvSha256Context *ctx, uint8_t hash[32]) {
    uint32_t i;

    /* 填充：写入 0x80 标记字节 */
    /* 安全检查：如果缓冲区已满（64字节），需要先处理当前块再写入填充 */
    if (ctx->datalen >= 64) {
        sha256_transform(ctx, ctx->data);
        ctx->datalen = 0;
    }
    ctx->data[ctx->datalen++] = 0x80;

    /* 如果缓冲区空间不够放长度，先处理当前块 */
    if (ctx->datalen > 56) {
        while (ctx->datalen < 64) {
            ctx->data[ctx->datalen++] = 0x00;
        }
        sha256_transform(ctx, ctx->data);
        ctx->datalen = 0;
    }

    /* 填充零到 56 字节 */
    while (ctx->datalen < 56) {
        ctx->data[ctx->datalen++] = 0x00;
    }

    /* 附加位长度（大端序，64位） */
    unsigned long long bitlen = ctx->bitlen;
    ctx->data[56] = (uint8_t) (bitlen >> 56);
    ctx->data[57] = (uint8_t) (bitlen >> 48);
    ctx->data[58] = (uint8_t) (bitlen >> 40);
    ctx->data[59] = (uint8_t) (bitlen >> 32);
    ctx->data[60] = (uint8_t) (bitlen >> 24);
    ctx->data[61] = (uint8_t) (bitlen >> 16);
    ctx->data[62] = (uint8_t) (bitlen >> 8);
    ctx->data[63] = (uint8_t) (bitlen);
    sha256_transform(ctx, ctx->data);

    /* 输出哈希值（大端序） */
    for (i = 0; i < 8; i++) {
        hash[i * 4] = (uint8_t) (ctx->state[i] >> 24);
        hash[i * 4 + 1] = (uint8_t) (ctx->state[i] >> 16);
        hash[i * 4 + 2] = (uint8_t) (ctx->state[i] >> 8);
        hash[i * 4 + 3] = (uint8_t) (ctx->state[i]);
    }
}

void lv_sha256_hex(const uint8_t *data, size_t len, char hex[65]) {
    lvSha256Context ctx;
    uint8_t hash[32];
    int i;

    lv_sha256_init(&ctx);
    lv_sha256_update(&ctx, data, len);
    lv_sha256_final(&ctx, hash);

    for (i = 0; i < 32; i++) {
        /* 使用 snprintf 确保缓冲区安全 */
        snprintf(hex + i * 2, 65 - (size_t) (i * 2), "%02x", hash[i]);
    }
    hex[64] = '\0';
}

void lv_sha256_string(const char *str, char hex[65]) {
    if (str) {
        lv_sha256_hex((const uint8_t *) str, strlen(str), hex);
    } else {
        lv_sha256_hex((const uint8_t *) "(null)", 6, hex);
    }
}
