/**
 * @file test_lv_file_ext.c
 * @brief 文件工具契约测试（批次 C-㊺续29：lv_file.h 8 个零覆盖 API）
 *
 * 覆盖：open / close / read_all / read_all_limited / read_text /
 *   write_all / exists / size
 * 契约：读写往返、大小限制、NULL 契约。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_unified.h"
#include "lv/lv_file.h"

int g_pass_count = 0;
int g_fail_count = 0;

static void test_file_rw_api(void) {
    const char *path = "lv_test_file.bin";
    const char *data = "hello file io";
    size_t len = strlen(data);

    /* write_all */
    TEST_ASSERT_EQ(lv_file_write_all(path, data, len), 0);
    TEST_ASSERT(lv_file_exists(path), "文件存在");
    TEST_ASSERT(!lv_file_exists("nonexistent_xyz.bin"), "不存在文件");

    /* size */
    FILE *fp = lv_file_open(path, "rb");
    TEST_ASSERT_NOT_NULL(fp);
    TEST_ASSERT_EQ(lv_file_size(fp), len);
    lv_file_close(fp);

    /* read_all */
    size_t out_len = 0;
    uint8_t *buf = lv_file_read_all(path, &out_len);
    TEST_ASSERT_NOT_NULL(buf);
    TEST_ASSERT_EQ(out_len, len);
    TEST_ASSERT(memcmp(buf, data, len) == 0, "读回内容一致");
    TEST_ASSERT_EQ(buf[len], '\0');
    lv_free((void **) &buf);

    /* read_all_limited：足够大 */
    buf = lv_file_read_all_limited(path, &out_len, 1000);
    TEST_ASSERT_NOT_NULL(buf);
    TEST_ASSERT_EQ(out_len, len);
    lv_free((void **) &buf);

    /* read_all_limited：过小 → NULL */
    TEST_ASSERT_NULL(lv_file_read_all_limited(path, &out_len, 5));
    TEST_ASSERT_EQ(out_len, 0);

    /* read_text */
    char text[64];
    TEST_ASSERT(lv_file_read_text(path, text, sizeof(text)), "read_text 成功");
    TEST_ASSERT(strcmp(text, data) == 0, "read_text 内容");

    /* NULL 契约 */
    TEST_ASSERT_NULL(lv_file_open(NULL, "r"));
    TEST_ASSERT_NULL(lv_file_open(path, NULL));
    TEST_ASSERT_EQ(lv_file_close(NULL), 0);
    TEST_ASSERT_NULL(lv_file_read_all(NULL, &out_len));
    /* out_len 可为 NULL：仍返回缓冲 */
    uint8_t *b2 = lv_file_read_all(path, NULL);
    TEST_ASSERT_NOT_NULL(b2);
    lv_free((void **) &b2);
    TEST_ASSERT_EQ(lv_file_write_all(NULL, data, len), -1);
    TEST_ASSERT(!lv_file_read_text(NULL, text, sizeof(text)), "x");
    TEST_ASSERT(!lv_file_read_text(path, NULL, sizeof(text)), "x");
    TEST_ASSERT(!lv_file_exists(NULL), "x");
    TEST_ASSERT_EQ(lv_file_size(NULL), 0);

    /* 读取不存在文件 */
    TEST_ASSERT_NULL(lv_file_read_all("no_such_file.bin", &out_len));

    remove(path);
    printf("  test_file_rw_api: PASSED\n");
}

TEST_MAIN_BEGIN("Lv-00 File Ext Test Suite")
    printf("=== Lv-00 File Ext Test Suite (batch C-㊺续29) ===\n\n");
    lv_init();
    TEST_MAIN_RUN(test_file_rw_api);
    lv_cleanup();
TEST_MAIN_END()
