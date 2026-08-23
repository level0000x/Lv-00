/**
 * @file test_lv_str_utils_ext.c
 * @brief 字符串设施契约测试（批次 C-㊺续34：lv_str_utils.h 零覆盖 API）
 *
 * 覆盖零覆盖 API（5 个）：
 *   lv_mpq_to_string / lv_strbuf_append_cell / lv_strbuf_append_sep
 *   lv_strbuf_join / lv_strtok_r
 *
 * 契约要点（与 lv_str_utils.c 核对）：
 *   - mpq_to_string：q NULL → NULL；omit && den==1 → "num"；否则 "num/den"。
 *   - append_sep：追加 count 个字符；NULL sb 安全。
 *   - append_cell：左对齐补空格（text NULL 按空串）。
 *   - strbuf_join：sb NULL → false；items NULL 或 count 0 → true；分隔符 NULL → ""。
 *   - strtok_r：delim/saveptr NULL → NULL；委托 strtok_r 语义。
 *
 * @author Lv-00 Project
 */

#include <stdio.h>
#include <string.h>

#include "lv/lv_str_utils.h"
#include "lv/lv_strbuf.h"
#include "lv/lv_utils.h"

#include "test_unified.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============== 测试：mpq_to_string ============== */

static void test_mpq_to_string(void) {
    mpq_t q;
    mpq_init(q);

    /* 3/2 → "3/2" */
    mpq_set_si(q, 3, 2);
    char *s = lv_mpq_to_string(q, false);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_STR_EQ(s, "3/2");
    lv_free((void **) &s);

    /* 5/1 omit=false → "5/1"；omit=true → "5" */
    mpq_set_si(q, 5, 1);
    s = lv_mpq_to_string(q, false);
    TEST_ASSERT_STR_EQ(s, "5/1");
    lv_free((void **) &s);
    s = lv_mpq_to_string(q, true);
    TEST_ASSERT_STR_EQ(s, "5");
    lv_free((void **) &s);

    /* NULL 契约 */
    TEST_ASSERT_NULL(lv_mpq_to_string(NULL, true));

    mpq_clear(q);
}

/* ============== 测试：报告表格辅助 ============== */

static void test_table_helpers(void) {
    lvStrBuf sb = {0};
    lv_strbuf_init(&sb);

    /* append_sep */
    lv_strbuf_append_sep(&sb, '=', 4);
    TEST_ASSERT_STR_EQ(lv_strbuf_cstr(&sb), "====");
    lv_strbuf_reset(&sb);
    lv_strbuf_append_sep(&sb, '-', 2);
    TEST_ASSERT_STR_EQ(lv_strbuf_cstr(&sb), "--");
    lv_strbuf_append_sep(NULL, '=', 3);

    /* append_cell：左对齐补空格 */
    lv_strbuf_reset(&sb);
    lv_strbuf_append_cell(&sb, "ab", 4);
    TEST_ASSERT_STR_EQ(lv_strbuf_cstr(&sb), "ab  ");
    lv_strbuf_reset(&sb);
    lv_strbuf_append_cell(&sb, "abcdef", 4); /* 超宽不截断 */
    TEST_ASSERT_STR_EQ(lv_strbuf_cstr(&sb), "abcdef");
    lv_strbuf_reset(&sb);
    lv_strbuf_append_cell(&sb, NULL, 3);
    TEST_ASSERT_STR_EQ(lv_strbuf_cstr(&sb), "   ");
    lv_strbuf_append_cell(NULL, "x", 3);

    /* strbuf_join */
    lv_strbuf_reset(&sb);
    const char *items[] = {"a", "b", "c"};
    TEST_ASSERT(lv_strbuf_join(&sb, items, 3, ", "), "join 3");
    TEST_ASSERT_STR_EQ(lv_strbuf_cstr(&sb), "a, b, c");
    lv_strbuf_reset(&sb);
    TEST_ASSERT(lv_strbuf_join(&sb, items, 3, NULL), "join sep NULL");
    TEST_ASSERT_STR_EQ(lv_strbuf_cstr(&sb), "abc");
    TEST_ASSERT(lv_strbuf_join(&sb, NULL, 0, ", "), "join empty true");
    TEST_ASSERT(!lv_strbuf_join(NULL, items, 3, ", "), "join NULL sb false");

    lv_strbuf_destroy(&sb);
}

/* ============== 测试：strtok_r ============== */

static void test_strtok_r(void) {
    char buf[] = "a,b,c";
    char *save = NULL;
    char *tok = lv_strtok_r(buf, ",", &save);
    TEST_ASSERT_STR_EQ(tok, "a");
    tok = lv_strtok_r(NULL, ",", &save);
    TEST_ASSERT_STR_EQ(tok, "b");
    tok = lv_strtok_r(NULL, ",", &save);
    TEST_ASSERT_STR_EQ(tok, "c");
    tok = lv_strtok_r(NULL, ",", &save);
    TEST_ASSERT_NULL(tok);

    /* NULL 契约 */
    TEST_ASSERT_NULL(lv_strtok_r(buf, NULL, &save));
    TEST_ASSERT_NULL(lv_strtok_r(buf, ",", NULL));
}

/* ============== Main ============== */

TEST_MAIN_BEGIN("StrUtilsExt")

    printf("\n--- lv_str_utils (zero-coverage) ---\n");
    TEST_MAIN_RUN(test_mpq_to_string);
    TEST_MAIN_RUN(test_table_helpers);
    TEST_MAIN_RUN(test_strtok_r);

TEST_MAIN_END()
