/**
 * @file test_logic_check_ext.c
 * @brief 逻辑检查契约测试（批次 C-㊺续31：logic_check.h 零覆盖 API）
 *
 * 覆盖零覆盖 API（3 个）：
 *   lv_logic_check_tautology / contradiction / equivalence
 *
 * 契约要点（与 logic_check.c 核对）：
 *   - 真值表枚举法（大写字母为命题变量），支持 ! ( ) & | ->
 *   - tautology：全真组合 1；否则 0；NULL/空串 -1。
 *   - contradiction：全假组合 1；否则 0；NULL/空串 -1。
 *   - equivalence：两式在所有变量组合下同值 1；不同 0；NULL -1。
 *
 * @author Lv-00 Project
 */

#include <stdio.h>

#include "lv/logic_check.h"

#include "test_unified.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============== 测试：重言式 ============== */

static void test_tautology(void) {
    /* A | !A 恒真 */
    TEST_ASSERT_EQ(lv_logic_check_tautology("A | !A"), 1);

    /* 排中律变体：!(A & !A) */
    TEST_ASSERT_EQ(lv_logic_check_tautology("!(A & !A)"), 1);

    /* 偶然式：A & B 非重言 */
    TEST_ASSERT_EQ(lv_logic_check_tautology("A & B"), 0);

    /* 单一变量非重言 */
    TEST_ASSERT_EQ(lv_logic_check_tautology("A"), 0);

    /* NULL / 空串：-1 */
    TEST_ASSERT_EQ(lv_logic_check_tautology(NULL), -1);
    TEST_ASSERT_EQ(lv_logic_check_tautology(""), -1);
}

/* ============== 测试：矛盾式 ============== */

static void test_contradiction(void) {
    /* A & !A 恒假 */
    TEST_ASSERT_EQ(lv_logic_check_contradiction("A & !A"), 1);

    /* 偶然式非矛盾 */
    TEST_ASSERT_EQ(lv_logic_check_contradiction("A | B"), 0);

    /* NULL / 空串：-1 */
    TEST_ASSERT_EQ(lv_logic_check_contradiction(NULL), -1);
    TEST_ASSERT_EQ(lv_logic_check_contradiction(""), -1);
}

/* ============== 测试：等价性 ============== */

static void test_equivalence(void) {
    /* A -> B 与 !A | B 等价（蕴含律） */
    TEST_ASSERT_EQ(lv_logic_check_equivalence("A -> B", "!A | B"), 1);

    /* 双重否定 */
    TEST_ASSERT_EQ(lv_logic_check_equivalence("A", "!(!A)"), 1);

    /* 不等价 */
    TEST_ASSERT_EQ(lv_logic_check_equivalence("A", "B"), 0);

    /* 相同公式等价 */
    TEST_ASSERT_EQ(lv_logic_check_equivalence("A & B", "A & B"), 1);

    /* NULL 契约 */
    TEST_ASSERT_EQ(lv_logic_check_equivalence(NULL, "A"), -1);
    TEST_ASSERT_EQ(lv_logic_check_equivalence("A", NULL), -1);
}

/* ============== Main ============== */

TEST_MAIN_BEGIN("LogicCheckExt")

    printf("\n--- logic_check (zero-coverage) ---\n");
    TEST_MAIN_RUN(test_tautology);
    TEST_MAIN_RUN(test_contradiction);
    TEST_MAIN_RUN(test_equivalence);

TEST_MAIN_END()
