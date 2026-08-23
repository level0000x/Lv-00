/**
 * @file test_lexer_shared_ext.c
 * @brief 共享词法分析器契约测试（批次 C-㊺续37：lexer_shared.h 零覆盖 API）
 *
 * 覆盖零覆盖 API（4 个）：
 *   lv_lexer_init / clear / skip_whitespace_and_comments / extract_string
 *
 * 契约要点（与 lexer_shared.h 注释核对）：
 *   - init：设置 source/pos/line/col。
 *   - clear：释放 error_msg 并重置。
 *   - skip_whitespace_and_comments：跳过空白与 '#' 注释，更新行列。
 *   - extract_string：从开引号后提取字符串（解码 \n \t \r \" \\）。
 *
 * @author Lv-00 Project
 */

#include <stdio.h>
#include <string.h>

#include "lv/lexer_shared.h"
#include "lv/lv_utils.h"

#include "test_unified.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============== 测试：init/clear ============== */

static void test_init_clear(void) {
    lvLexer lex;
    lv_lexer_init(&lex, "hello world");
    TEST_ASSERT(lex.source == lex.pos || strcmp(lex.pos, "hello world") == 0, "pos at start");
    TEST_ASSERT_EQ(lex.line, 1);
    TEST_ASSERT_EQ(lex.col, 1);
    TEST_ASSERT_NULL(lex.error_msg);

    /* clear：重置（可重新 init） */
    lv_lexer_clear(&lex);
    lv_lexer_init(&lex, "again");
    TEST_ASSERT(strcmp(lex.pos, "again") == 0, "re-init");
    lv_lexer_clear(&lex);
    lv_lexer_clear(NULL);
}

/* ============== 测试：跳过空白与注释 ============== */

static void test_skip_ws_comments(void) {
    lvLexer lex;
    lv_lexer_init(&lex, "  \t\n  # comment\n  abc");
    lv_lexer_skip_whitespace_and_comments(&lex);
    TEST_ASSERT(strncmp(lex.pos, "abc", 3) == 0, "skip to abc");
    TEST_ASSERT(lex.line >= 3, "line advanced");

    /* 无空白 */
    lv_lexer_init(&lex, "xyz");
    lv_lexer_skip_whitespace_and_comments(&lex);
    TEST_ASSERT(strncmp(lex.pos, "xyz", 3) == 0, "no skip needed");

    lv_lexer_skip_whitespace_and_comments(NULL);
    lv_lexer_clear(&lex);
}

/* ============== 测试：字符串提取 ============== */

static void test_extract_string(void) {
    /* 简单字符串：pos 指向 "ab\"c" 内容（开引号后） */
    lvLexer lex;
    lv_lexer_init(&lex, "\"ab\\\"cd\" tail");
    lex.pos = lex.source + 1; /* 跳过开引号 */

    char *s = lv_lexer_extract_string(&lex);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_STR_EQ(s, "ab\"cd");
    lv_free((void **) &s);

    /* 换行转义 */
    lv_lexer_init(&lex, "\"a\\nb\" tail");
    lex.pos = lex.source + 1;
    s = lv_lexer_extract_string(&lex);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_STR_EQ(s, "a\nb");
    lv_free((void **) &s);

    lv_lexer_clear(&lex);
}

/* ============== Main ============== */

TEST_MAIN_BEGIN("LexerSharedExt")

    printf("\n--- lexer_shared (zero-coverage) ---\n");
    TEST_MAIN_RUN(test_init_clear);
    TEST_MAIN_RUN(test_skip_ws_comments);
    TEST_MAIN_RUN(test_extract_string);

TEST_MAIN_END()
