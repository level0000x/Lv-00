/**
 * @file test_sha256_ext.c
 * @brief SHA-256 契约测试（批次 C-㊺续29：sha256.h 5 个零覆盖 API）
 *
 * 覆盖：init / update / final / hex / string
 * 契约：标准测试向量 "abc" → ba7816bf8f01cfea414140de5dae2223
 *   b00361a396177a9cb410ff61f20015ad
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_unified.h"
#include "lv/sha256.h"

int g_pass_count = 0;
int g_fail_count = 0;

static void test_sha256_api(void) {
    const char *abc_hash =
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";

    /* string 快捷：一次调用 */
    char hex[65];
    lv_sha256_string("abc", hex);
    TEST_ASSERT(strcmp(hex, abc_hash) == 0, "abc 标准向量");

    /* hex 快捷 */
    lv_sha256_hex((const uint8_t *) "abc", 3, hex);
    TEST_ASSERT(strcmp(hex, abc_hash) == 0, "hex 快捷同向量");

    /* 分块 init/update/final */
    lvSha256Context ctx;
    lv_sha256_init(&ctx);
    lv_sha256_update(&ctx, (const uint8_t *) "a", 1);
    lv_sha256_update(&ctx, (const uint8_t *) "b", 1);
    lv_sha256_update(&ctx, (const uint8_t *) "c", 1);
    uint8_t digest[32];
    lv_sha256_final(&ctx, digest);

    /* 转 hex 对比 */
    char hexbuf[65];
    for (int i = 0; i < 32; i++) {
        lv_snprintf(hexbuf + i * 2, 3, "%02x", digest[i]);
    }
    hexbuf[64] = '\0';
    TEST_ASSERT(strcmp(hexbuf, abc_hash) == 0, "分块 final 同向量");

    /* 空串 */
    lv_sha256_string("", hex);
    TEST_ASSERT(strlen(hex) == 64, "空串哈希 64 字符");
    TEST_ASSERT(strcmp(hex, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855") == 0,
                "空串标准向量");

    printf("  test_sha256_api: PASSED\n");
}

TEST_MAIN_BEGIN("Lv-00 SHA256 Ext Test Suite")
    printf("=== Lv-00 SHA256 Ext Test Suite (batch C-㊺续29) ===\n\n");
    lv_init();
    TEST_MAIN_RUN(test_sha256_api);
    lv_cleanup();
TEST_MAIN_END()
