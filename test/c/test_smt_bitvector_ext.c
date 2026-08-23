/**
 * @file test_smt_bitvector_ext.c
 * @brief 位向量位移契约测试（批次 C-㊺续34：smt_bitvector.h 零覆盖 API）
 *
 * 覆盖零覆盖 API（2 个）：
 *   lv_bv_shift_left / lv_bv_shift_right
 *
 * 契约要点（与 smt_bitvector.c 的 bv_shift_* 核对）：
 *   - shift_left：左移，溢出高位丢弃（模 2^width）。
 *   - shift_right：右移，溢出低位丢弃。
 *
 * @author Lv-00 Project
 */

#include <stdio.h>

#include "lv/smt_bitvector.h"

#include "test_unified.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============== 测试：左移 ============== */

static void test_shift_left(void) {
    lvBitVec *a = lv_bv_create(8, 1); /* 0b00000001 */
    TEST_ASSERT_NOT_NULL(a);

    lvBitVec *r = lv_bv_shift_left(a, 1);
    TEST_ASSERT_NOT_NULL(r);
    TEST_ASSERT_EQ((long long) lv_bv_to_int(r), 2LL);

    lv_bv_free(r);

    /* 移出宽度：1 << 8 → 0（8 位截断） */
    r = lv_bv_shift_left(a, 8);
    TEST_ASSERT_NOT_NULL(r);
    TEST_ASSERT_EQ((long long) lv_bv_to_int(r), 0LL);
    lv_bv_free(r);

    /* 0 位移：不变 */
    r = lv_bv_shift_left(a, 0);
    TEST_ASSERT_NOT_NULL(r);
    TEST_ASSERT_EQ((long long) lv_bv_to_int(r), 1LL);
    lv_bv_free(r);

    lv_bv_free(a);
}

/* ============== 测试：右移 ============== */

static void test_shift_right(void) {
    lvBitVec *a = lv_bv_create(8, 4); /* 0b00000100 */
    TEST_ASSERT_NOT_NULL(a);

    lvBitVec *r = lv_bv_shift_right(a, 1);
    TEST_ASSERT_NOT_NULL(r);
    TEST_ASSERT_EQ((long long) lv_bv_to_int(r), 2LL);
    lv_bv_free(r);

    r = lv_bv_shift_right(a, 2);
    TEST_ASSERT_NOT_NULL(r);
    TEST_ASSERT_EQ((long long) lv_bv_to_int(r), 1LL);
    lv_bv_free(r);

    /* 移出全部：0 */
    r = lv_bv_shift_right(a, 8);
    TEST_ASSERT_NOT_NULL(r);
    TEST_ASSERT_EQ((long long) lv_bv_to_int(r), 0LL);
    lv_bv_free(r);

    lv_bv_free(a);
}

/* ============== Main ============== */

TEST_MAIN_BEGIN("SmtBitvectorExt")

    printf("\n--- smt_bitvector (zero-coverage) ---\n");
    TEST_MAIN_RUN(test_shift_left);
    TEST_MAIN_RUN(test_shift_right);

TEST_MAIN_END()
