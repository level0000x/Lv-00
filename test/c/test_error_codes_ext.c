/**
 * @file test_error_codes_ext.c
 * @brief 错误码工具契约测试（批次 C-㊺续29：error_codes.h 4 个零覆盖 API）
 *
 * 覆盖：error_category / error_code_from_string / error_is_unknown /
 *   get_error_description
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_unified.h"
#include "lv/error_codes.h"
#include "lv/status_codes.h"

int g_pass_count = 0;
int g_fail_count = 0;

static void test_error_codes_api(void) {
    /* error_category：常见码有类别 */
    const char *cat = lv_error_category(lv_ERROR_UNKNOWN);
    TEST_ASSERT_NOT_NULL(cat);
    cat = lv_error_category(lv_ERROR_INVALID_PARAM);
    TEST_ASSERT_NOT_NULL(cat);
    TEST_ASSERT(strlen(cat) > 0, "类别非空");

    /* error_is_unknown */
    TEST_ASSERT(!lv_error_is_unknown(lv_ERROR_UNKNOWN), "已知码非未知");
    TEST_ASSERT(!lv_error_is_unknown(lv_ERROR_INVALID_PARAM), "已知码非未知");
    TEST_ASSERT(lv_error_is_unknown((lvErrorCode) 99999), "未知名码为未知");

    /* error_code_from_string：往返 */
    lvErrorCode code = lv_error_code_from_string("lv_ERROR_INVALID_PARAM");
    TEST_ASSERT_EQ((int) code, (int) lv_ERROR_INVALID_PARAM);
    /* NULL 或未知名 → 默认 */
    lvErrorCode unknown = lv_error_code_from_string("no_such_code");
    TEST_ASSERT_EQ((int) unknown, (int) lv_ERROR_UNKNOWN);

    /* get_error_description：缓冲写描述 */
    char buf[256];
    /* 先设置一个错误 */
    lv_set_error(lv_ERROR_INVALID_PARAM, "测试错误");
    int n = lv_get_error_description(buf, sizeof(buf));
    TEST_ASSERT(n >= 0, "描述长度非负");
    TEST_ASSERT(strlen(buf) > 0, "描述非空");
    TEST_ASSERT_EQ(lv_get_error_description(NULL, 10), -1);
    TEST_ASSERT_EQ(lv_get_error_description(buf, 0), -1);

    printf("  test_error_codes_api: PASSED\n");
}

static void test_set_error_ctx_api(void) {
    /* lv_set_error_ctx：设置线程局部错误上下文 */
    lv_set_error_ctx(lv_ERROR_NOT_FOUND, __FILE__, __LINE__, __func__, "ctx msg %d", 42);
    TEST_ASSERT_EQ((int) lv_get_last_error_code(), (int) lv_ERROR_NOT_FOUND);
    const char *msg = lv_get_last_error_message();
    TEST_ASSERT_NOT_NULL(msg);
    TEST_ASSERT(strstr(msg, "ctx msg 42") != NULL, "formatted message");
}

static void test_status_category_api(void) {
    /* lv_status_category（error_codes.h 声明，status_codes.c 实现） */
    const char *cat = lv_status_category(0);
    TEST_ASSERT_NOT_NULL(cat);
    TEST_ASSERT(strlen(cat) > 0, "category non-empty");

    /* 负码（警告区间） */
    cat = lv_status_category(-1);
    TEST_ASSERT_NOT_NULL(cat);

    /* 未知范围 */
    cat = lv_status_category(999999);
    TEST_ASSERT_NOT_NULL(cat);
    TEST_ASSERT(strcmp(cat, "未分类") == 0, "unknown category");
}

TEST_MAIN_BEGIN("Lv-00 Error Codes Ext Test Suite")
    printf("=== Lv-00 Error Codes Ext Test Suite (batch C-㊺续29) ===\n\n");
    lv_init();
    TEST_MAIN_RUN(test_error_codes_api);
    TEST_MAIN_RUN(test_set_error_ctx_api);
    TEST_MAIN_RUN(test_status_category_api);
    lv_cleanup();
TEST_MAIN_END()
