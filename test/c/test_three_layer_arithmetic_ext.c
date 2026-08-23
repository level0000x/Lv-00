/**
 * @file test_three_layer_arithmetic_ext.c
 * @brief 三层算术安全函数契约测试（批次 C-㊺续36：three_layer_arithmetic.h 零覆盖 API）
 *
 * 覆盖零覆盖 API（3 个 static inline，lv_arith_safe.h 权威实现）：
 *   lv_safe_add_i64 / lv_safe_mul_i64 / lv_safe_sub_i64
 *
 * 契约要点（与 lv_arith_safe.h 核对）：
 *   - 成功返回 true 并输出结果；溢出或 out NULL 返回 false。
 *
 * 注：lv_float_audit_report 为 lv_FLOAT_AUDIT 审计模式的导出接口，
 * 头文件仅注释提及无声明（编译期功能），登记豁免。
 *
 * @author Lv-00 Project
 */

#include <stdint.h>
#include <stdio.h>

#include "lv/three_layer_arithmetic.h"

#include "test_unified.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============== 测试：安全加法 ============== */

static void test_safe_add(void) {
    int64_t r = 0;

    TEST_ASSERT(lv_safe_add_i64(3, 4, &r), "3+4");
    TEST_ASSERT_EQ((long long) r, 7LL);

    /* 溢出：INT64_MAX + 1 */
    TEST_ASSERT(!lv_safe_add_i64(INT64_MAX, 1, &r), "overflow");

    /* NULL 契约 */
    TEST_ASSERT(!lv_safe_add_i64(1, 2, NULL), "NULL out");
}

/* ============== 测试：安全乘法 ============== */

static void test_safe_mul(void) {
    int64_t r = 0;

    TEST_ASSERT(lv_safe_mul_i64(6, 7, &r), "6*7");
    TEST_ASSERT_EQ((long long) r, 42LL);

    /* 溢出：INT64_MAX * 2 */
    TEST_ASSERT(!lv_safe_mul_i64(INT64_MAX, 2, &r), "overflow");

    TEST_ASSERT(!lv_safe_mul_i64(1, 2, NULL), "NULL out");
}

/* ============== 测试：安全减法 ============== */

static void test_safe_sub(void) {
    int64_t r = 0;

    TEST_ASSERT(lv_safe_sub_i64(10, 4, &r), "10-4");
    TEST_ASSERT_EQ((long long) r, 6LL);

    /* 溢出：INT64_MIN - 1 */
    TEST_ASSERT(!lv_safe_sub_i64(INT64_MIN, 1, &r), "overflow");

    TEST_ASSERT(!lv_safe_sub_i64(1, 2, NULL), "NULL out");
}

/* ============== Main ============== */

TEST_MAIN_BEGIN("ThreeLayerArithExt")

    printf("\n--- three_layer_arithmetic (zero-coverage) ---\n");
    TEST_MAIN_RUN(test_safe_add);
    TEST_MAIN_RUN(test_safe_mul);
    TEST_MAIN_RUN(test_safe_sub);

TEST_MAIN_END()
