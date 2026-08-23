/**
 * @file test_lv_lexer_ext.c
 * @brief 词法分析器契约测试（批次 C-㊺续32：lv_lexer.h 零覆盖 API）
 *
 * 覆盖零覆盖 API（3 个）：
 *   lv_lexer_peek / lv_lexer_get_loc / lv_token_text
 *
 * 契约要点（与 lv_lexer.c 核对）：
 *   - peek：lookahead 0~31 前瞻（不消费）；越界（<0 或 >=32）返回 LV_TOKEN_ERROR。
 *   - get_loc：返回当前 line/column/offset（创建后 line >= 1，offset 随消费推进）。
 *   - token_text：将 token 源文本复制到缓冲区（截断 + NUL 终止）；
 *     返回复制字符数；token/buf NULL 或 buf_size 0 返回 0。
 *
 * @author Lv-00 Project
 */

#include <stdio.h>
#include <string.h>

#include "lv/lv_lexer.h"

#include "test_unified.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============== 测试：peek 前瞻 ============== */

static void test_peek(void) {
    LvLexer *lexer = lv_lexer_create("x + 1", 5);
    TEST_ASSERT_NOT_NULL(lexer);

    /* 前瞻序列：IDENTIFIER PLUS INTEGER EOF */
    LvToken t0 = lv_lexer_peek(lexer, 0);
    LvToken t1 = lv_lexer_peek(lexer, 1);
    LvToken t2 = lv_lexer_peek(lexer, 2);
    LvToken t3 = lv_lexer_peek(lexer, 3);
    TEST_ASSERT_EQ((int) t0.type, (int) LV_TOKEN_IDENTIFIER);
    TEST_ASSERT_EQ((int) t1.type, (int) LV_TOKEN_PLUS);
    TEST_ASSERT_EQ((int) t2.type, (int) LV_TOKEN_INTEGER);
    TEST_ASSERT_EQ((int) t3.type, (int) LV_TOKEN_EOF);

    /* peek 不消费：next 仍返回第一个 token */
    LvToken n0 = lv_lexer_next(lexer);
    TEST_ASSERT_EQ((int) n0.type, (int) LV_TOKEN_IDENTIFIER);

    /* 越界 lookahead 返回 ERROR */
    LvToken bad = lv_lexer_peek(lexer, -1);
    TEST_ASSERT_EQ((int) bad.type, (int) LV_TOKEN_ERROR);
    bad = lv_lexer_peek(lexer, 32);
    TEST_ASSERT_EQ((int) bad.type, (int) LV_TOKEN_ERROR);

    lv_lexer_destroy(lexer);
}

/* ============== 测试：get_loc ============== */

static void test_get_loc(void) {
    LvLexer *lexer = lv_lexer_create("x + 1", 5);
    TEST_ASSERT_NOT_NULL(lexer);

    LvSourceLoc loc = lv_lexer_get_loc(lexer);
    TEST_ASSERT(loc.line >= 1, "line >= 1");
    TEST_ASSERT(loc.offset >= 0, "offset >= 0");

    /* 消费几个 token 后 offset 推进 */
    lv_lexer_next(lexer);
    lv_lexer_next(lexer);
    LvSourceLoc loc2 = lv_lexer_get_loc(lexer);
    TEST_ASSERT(loc2.offset >= loc.offset, "offset advances");

    lv_lexer_destroy(lexer);
}

/* ============== 测试：token_text ============== */

static void test_token_text(void) {
    LvLexer *lexer = lv_lexer_create("hello \"world\" 123", 17);
    TEST_ASSERT_NOT_NULL(lexer);

    /* 标识符 "hello" */
    LvToken t = lv_lexer_next(lexer);
    char buf[64];
    size_t n = lv_token_text(&t, buf, sizeof(buf));
    TEST_ASSERT_EQ((int) n, 5);
    TEST_ASSERT_STR_EQ(buf, "hello");

    /* 字符串 "world"（token 文本含引号） */
    t = lv_lexer_next(lexer);
    n = lv_token_text(&t, buf, sizeof(buf));
    TEST_ASSERT_EQ((int) n, 7);
    TEST_ASSERT_STR_EQ(buf, "\"world\"");

    /* 整数 123 */
    t = lv_lexer_next(lexer);
    n = lv_token_text(&t, buf, sizeof(buf));
    TEST_ASSERT_EQ((int) n, 3);
    TEST_ASSERT_STR_EQ(buf, "123");

    /* 截断：小缓冲区 */
    LvLexer *l2 = lv_lexer_create("abcdef", 6);
    TEST_ASSERT_NOT_NULL(l2);
    LvToken t2 = lv_lexer_next(l2);
    char small[4];
    n = lv_token_text(&t2, small, sizeof(small));
    TEST_ASSERT_EQ((int) n, 3); /* 截断到 buf_size-1 */
    TEST_ASSERT_STR_EQ(small, "abc");
    lv_lexer_destroy(l2);

    /* NULL 契约 */
    TEST_ASSERT_EQ(lv_token_text(NULL, buf, sizeof(buf)), 0);
    TEST_ASSERT_EQ(lv_token_text(&t, NULL, sizeof(buf)), 0);
    TEST_ASSERT_EQ(lv_token_text(&t, buf, 0), 0);

    lv_lexer_destroy(lexer);
}

/* ============== Main ============== */

TEST_MAIN_BEGIN("LexerExt")

    printf("\n--- lv_lexer (zero-coverage) ---\n");
    TEST_MAIN_RUN(test_peek);
    TEST_MAIN_RUN(test_get_loc);
    TEST_MAIN_RUN(test_token_text);

TEST_MAIN_END()
