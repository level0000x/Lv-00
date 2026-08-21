/**
 * @file test_three_valued_logic_ext.c
 * @brief 三值逻辑系统契约测试（批次 C-㊺续2：three_valued_logic.h 13 个零覆盖 API）
 *
 * 覆盖 13 个 ctest 零覆盖 API：
 *   - 真值表族：lv_tvl_and / or / not / implies / equiv
 *   - 批量族：lv_tvl_and_all / or_all
 *   - 判定族：lv_tvl_is_known / is_true / is_false /
 *     to_bool_conservative / to_bool_optimistic / from_bool
 *
 * 契约（Kleene 强三值逻辑）：
 *   AND: FALSE 吸收；UNKNOWN∧x = UNKNOWN（x≠FALSE）
 *   OR:  TRUE 吸收；UNKNOWN∨x = UNKNOWN（x≠TRUE）
 *   NOT: TRUE↔FALSE 对偶，UNKNOWN 不变
 *   IMPLIES: a→b = ¬a∨b；a=FALSE 平凡成立
 *   EQUIV: a↔b = (a→b)∧(b→a)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_unified.h"
#include "lv/three_valued_logic.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============== 测试：AND/OR/NOT 真值表 ============== */

static void test_and_or_not_api(void) {
    /* AND 真值表 */
    TEST_ASSERT_EQ(lv_tvl_and(lv_TRUE, lv_TRUE), lv_TRUE);
    TEST_ASSERT_EQ(lv_tvl_and(lv_TRUE, lv_FALSE), lv_FALSE);
    TEST_ASSERT_EQ(lv_tvl_and(lv_TRUE, lv_UNKNOWN), lv_UNKNOWN);
    TEST_ASSERT_EQ(lv_tvl_and(lv_FALSE, lv_TRUE), lv_FALSE);
    TEST_ASSERT_EQ(lv_tvl_and(lv_FALSE, lv_FALSE), lv_FALSE);
    TEST_ASSERT_EQ(lv_tvl_and(lv_FALSE, lv_UNKNOWN), lv_FALSE);
    TEST_ASSERT_EQ(lv_tvl_and(lv_UNKNOWN, lv_TRUE), lv_UNKNOWN);
    TEST_ASSERT_EQ(lv_tvl_and(lv_UNKNOWN, lv_FALSE), lv_FALSE);
    TEST_ASSERT_EQ(lv_tvl_and(lv_UNKNOWN, lv_UNKNOWN), lv_UNKNOWN);

    /* OR 真值表 */
    TEST_ASSERT_EQ(lv_tvl_or(lv_TRUE, lv_TRUE), lv_TRUE);
    TEST_ASSERT_EQ(lv_tvl_or(lv_TRUE, lv_FALSE), lv_TRUE);
    TEST_ASSERT_EQ(lv_tvl_or(lv_TRUE, lv_UNKNOWN), lv_TRUE);
    TEST_ASSERT_EQ(lv_tvl_or(lv_FALSE, lv_TRUE), lv_TRUE);
    TEST_ASSERT_EQ(lv_tvl_or(lv_FALSE, lv_FALSE), lv_FALSE);
    TEST_ASSERT_EQ(lv_tvl_or(lv_FALSE, lv_UNKNOWN), lv_UNKNOWN);
    TEST_ASSERT_EQ(lv_tvl_or(lv_UNKNOWN, lv_TRUE), lv_TRUE);
    TEST_ASSERT_EQ(lv_tvl_or(lv_UNKNOWN, lv_FALSE), lv_UNKNOWN);
    TEST_ASSERT_EQ(lv_tvl_or(lv_UNKNOWN, lv_UNKNOWN), lv_UNKNOWN);

    /* NOT */
    TEST_ASSERT_EQ(lv_tvl_not(lv_TRUE), lv_FALSE);
    TEST_ASSERT_EQ(lv_tvl_not(lv_FALSE), lv_TRUE);
    TEST_ASSERT_EQ(lv_tvl_not(lv_UNKNOWN), lv_UNKNOWN);

    printf("  test_and_or_not_api: PASSED\n");
}

/* ============== 测试：IMPLIES/EQUIV 真值表 ============== */

static void test_implies_equiv_api(void) {
    /* IMPLIES 真值表（a→b = ¬a∨b） */
    TEST_ASSERT_EQ(lv_tvl_implies(lv_TRUE, lv_TRUE), lv_TRUE);
    TEST_ASSERT_EQ(lv_tvl_implies(lv_TRUE, lv_FALSE), lv_FALSE);
    TEST_ASSERT_EQ(lv_tvl_implies(lv_TRUE, lv_UNKNOWN), lv_UNKNOWN);
    TEST_ASSERT_EQ(lv_tvl_implies(lv_FALSE, lv_TRUE), lv_TRUE);
    TEST_ASSERT_EQ(lv_tvl_implies(lv_FALSE, lv_FALSE), lv_TRUE);
    TEST_ASSERT_EQ(lv_tvl_implies(lv_FALSE, lv_UNKNOWN), lv_TRUE);
    TEST_ASSERT_EQ(lv_tvl_implies(lv_UNKNOWN, lv_TRUE), lv_TRUE);
    TEST_ASSERT_EQ(lv_tvl_implies(lv_UNKNOWN, lv_FALSE), lv_UNKNOWN);
    TEST_ASSERT_EQ(lv_tvl_implies(lv_UNKNOWN, lv_UNKNOWN), lv_UNKNOWN);

    /* EQUIV 真值表（a↔b = (a→b)∧(b→a)） */
    TEST_ASSERT_EQ(lv_tvl_equiv(lv_TRUE, lv_TRUE), lv_TRUE);
    TEST_ASSERT_EQ(lv_tvl_equiv(lv_TRUE, lv_FALSE), lv_FALSE);
    TEST_ASSERT_EQ(lv_tvl_equiv(lv_TRUE, lv_UNKNOWN), lv_UNKNOWN);
    TEST_ASSERT_EQ(lv_tvl_equiv(lv_FALSE, lv_TRUE), lv_FALSE);
    TEST_ASSERT_EQ(lv_tvl_equiv(lv_FALSE, lv_FALSE), lv_TRUE);
    TEST_ASSERT_EQ(lv_tvl_equiv(lv_FALSE, lv_UNKNOWN), lv_UNKNOWN);
    TEST_ASSERT_EQ(lv_tvl_equiv(lv_UNKNOWN, lv_TRUE), lv_UNKNOWN);
    TEST_ASSERT_EQ(lv_tvl_equiv(lv_UNKNOWN, lv_FALSE), lv_UNKNOWN);
    TEST_ASSERT_EQ(lv_tvl_equiv(lv_UNKNOWN, lv_UNKNOWN), lv_UNKNOWN);

    printf("  test_implies_equiv_api: PASSED\n");
}

/* ============== 测试：批量归约 ============== */

static void test_batch_api(void) {
    /* and_all：归约 + FALSE 短路 */
    lvTruthValue all_true[3] = {lv_TRUE, lv_TRUE, lv_TRUE};
    TEST_ASSERT_EQ(lv_tvl_and_all(all_true, 3), lv_TRUE);
    lvTruthValue with_false[3] = {lv_TRUE, lv_FALSE, lv_UNKNOWN};
    TEST_ASSERT_EQ(lv_tvl_and_all(with_false, 3), lv_FALSE);
    lvTruthValue with_unknown[2] = {lv_TRUE, lv_UNKNOWN};
    TEST_ASSERT_EQ(lv_tvl_and_all(with_unknown, 2), lv_UNKNOWN);

    /* or_all：归约 + TRUE 短路 */
    lvTruthValue all_false[3] = {lv_FALSE, lv_FALSE, lv_FALSE};
    TEST_ASSERT_EQ(lv_tvl_or_all(all_false, 3), lv_FALSE);
    lvTruthValue with_true[3] = {lv_FALSE, lv_TRUE, lv_UNKNOWN};
    TEST_ASSERT_EQ(lv_tvl_or_all(with_true, 3), lv_TRUE);
    lvTruthValue with_unknown2[2] = {lv_FALSE, lv_UNKNOWN};
    TEST_ASSERT_EQ(lv_tvl_or_all(with_unknown2, 2), lv_UNKNOWN);

    /* NULL/空数组契约：不崩溃（count<=0 或 NULL 视为恒等归约） */
    TEST_ASSERT_EQ(lv_tvl_and_all(NULL, 0), lv_TRUE);
    TEST_ASSERT_EQ(lv_tvl_or_all(NULL, 0), lv_FALSE);

    printf("  test_batch_api: PASSED\n");
}

/* ============== 测试：判定辅助与转换 ============== */

static void test_predicate_api(void) {
    /* is_known / is_true / is_false */
    TEST_ASSERT(lv_tvl_is_known(lv_TRUE), "TRUE 已知");
    TEST_ASSERT(lv_tvl_is_known(lv_FALSE), "FALSE 已知");
    TEST_ASSERT(!lv_tvl_is_known(lv_UNKNOWN), "UNKNOWN 未知");
    TEST_ASSERT(lv_tvl_is_true(lv_TRUE), "is_true");
    TEST_ASSERT(!lv_tvl_is_true(lv_FALSE), "非 TRUE");
    TEST_ASSERT(lv_tvl_is_false(lv_FALSE), "is_false");
    TEST_ASSERT(!lv_tvl_is_false(lv_UNKNOWN), "非 FALSE");

    /* from_bool */
    TEST_ASSERT_EQ(lv_tvl_from_bool(true), lv_TRUE);
    TEST_ASSERT_EQ(lv_tvl_from_bool(false), lv_FALSE);

    /* to_bool 保守：仅 TRUE → true */
    TEST_ASSERT(lv_tvl_to_bool_conservative(lv_TRUE), "保守 TRUE");
    TEST_ASSERT(!lv_tvl_to_bool_conservative(lv_FALSE), "保守 FALSE");
    TEST_ASSERT(!lv_tvl_to_bool_conservative(lv_UNKNOWN), "保守 UNKNOWN");

    /* to_bool 乐观：非 FALSE → true */
    TEST_ASSERT(lv_tvl_to_bool_optimistic(lv_TRUE), "乐观 TRUE");
    TEST_ASSERT(!lv_tvl_to_bool_optimistic(lv_FALSE), "乐观 FALSE");
    TEST_ASSERT(lv_tvl_to_bool_optimistic(lv_UNKNOWN), "乐观 UNKNOWN");

    printf("  test_predicate_api: PASSED\n");
}

/* ============== 测试入口 ============== */

TEST_MAIN_BEGIN("Lv-00 Three-Valued Logic Ext Test Suite")
    printf("=== Lv-00 Three-Valued Logic Ext Test Suite (batch C-㊺续2) ===\n\n");
    lv_init();

    TEST_MAIN_RUN(test_and_or_not_api);
    TEST_MAIN_RUN(test_implies_equiv_api);
    TEST_MAIN_RUN(test_batch_api);
    TEST_MAIN_RUN(test_predicate_api);

    lv_cleanup();
TEST_MAIN_END()
