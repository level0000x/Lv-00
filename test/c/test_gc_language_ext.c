/**
 * @file test_gc_language_ext.c
 * @brief GC 语言解析契约测试（批次 C-㊺续33：gc_language.h 零覆盖 API）
 *
 * 覆盖零覆盖 API（3 个）：
 *   lv_gc_parse / lv_gc_error / lv_gc_command_count
 *
 * 契约要点（与 gc_language.c 核对）：
 *   - parse：统计关键字命令数；未知字符返回 -1 并记录错误；NULL → -1。
 *   - error：有错误返回错误串，无错误返回 NULL；parse 成功重置错误状态。
 *   - command_count：最近一次 parse 的关键字计数（parse 时重置）。
 *
 * @author Lv-00 Project
 */

#include <stdio.h>
#include <string.h>

#include "lv/gc_language.h"

#include "test_unified.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============== 测试：成功解析 ============== */

static void test_parse_success(void) {
    /* 两个关键字命令：point、line */
    TEST_ASSERT_EQ(lv_gc_parse("point A; line l;", NULL), 0);
    TEST_ASSERT_EQ(lv_gc_command_count(), 2);

    /* 注释不计数 */
    TEST_ASSERT_EQ(lv_gc_parse("// comment\npoint A;", NULL), 0);
    TEST_ASSERT_EQ(lv_gc_command_count(), 1);

    /* 解析成功后无错误 */
    TEST_ASSERT_NULL(lv_gc_error());

    /* 非关键字标识符不计数 */
    TEST_ASSERT_EQ(lv_gc_parse("foo bar baz;", NULL), 0);
    TEST_ASSERT_EQ(lv_gc_command_count(), 0);
}

/* ============== 测试：解析失败 ============== */

static void test_parse_failure(void) {
    /* 未知字符 */
    TEST_ASSERT_EQ(lv_gc_parse("point A @ x", NULL), -1);
    TEST_ASSERT_NOT_NULL(lv_gc_error());
    TEST_ASSERT(lv_gc_error()[0] != '\0', "error message non-empty");

    /* NULL 源 */
    TEST_ASSERT_EQ(lv_gc_parse(NULL, NULL), -1);
    TEST_ASSERT_NOT_NULL(lv_gc_error());
}

/* ============== Main ============== */

TEST_MAIN_BEGIN("GcLanguageExt")

    printf("\n--- gc_language (zero-coverage) ---\n");
    TEST_MAIN_RUN(test_parse_success);
    TEST_MAIN_RUN(test_parse_failure);

TEST_MAIN_END()
