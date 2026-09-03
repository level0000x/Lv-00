/**
 * @file test_fptaylor_real_ext.c
 * @brief REAL(MPFR) 高精度表达式重算契约测试（批次 C2 / 256）
 *
 * 覆盖 fptaylor_eval_real_expr：同一文法（数值/+ - * //括号/变量）在 REAL 精度重算；
 * 正确性（与算术语义一致）、除零/解析失败 → NULL、NULL 契约。
 */

#include <stdio.h>
#include <stdlib.h>

#include "test_unified.h"
#include "lv/float_error.h"
#include "lv/lv_number.h"

int g_pass_count = 0;
int g_fail_count = 0;

static void test_fpeval_basic(void) {
    double vars2[2] = {3.0, 4.0};
    double vars1[1] = {1.0};

    lvNumber *r = fptaylor_eval_real_expr("x0*x0 + x1*x1", vars2, 2, 128);
    TEST_ASSERT_NOT_NULL(r);
    TEST_ASSERT_EQ((int) lv_number_type(r), (int) lv_NUMBER_REAL_MPFR);
    TEST_ASSERT_DOUBLE(lv_number_to_double(r), 25.0, 1e-12);
    lv_number_destroy(r);

    r = fptaylor_eval_real_expr("1+2*3", vars1, 1, 128);
    TEST_ASSERT_NOT_NULL(r);
    TEST_ASSERT_DOUBLE(lv_number_to_double(r), 7.0, 1e-12);
    lv_number_destroy(r);

    r = fptaylor_eval_real_expr("x0/2", vars1, 1, 128);
    TEST_ASSERT_NOT_NULL(r);
    TEST_ASSERT_DOUBLE(lv_number_to_double(r), 0.5, 1e-12);
    lv_number_destroy(r);

    double vars36[2] = {3.0, 6.0};
    r = fptaylor_eval_real_expr("(x0+x1)/x0", vars36, 2, 128);
    TEST_ASSERT_NOT_NULL(r);
    TEST_ASSERT_DOUBLE(lv_number_to_double(r), 3.0, 1e-12);
    lv_number_destroy(r);
    printf("  test_fpeval_basic: PASSED\n");
}

static void test_fpeval_error(void) {
    double v0[1] = {0.0};
    /* 语法/语义失败 → NULL */
    TEST_ASSERT_NULL(fptaylor_eval_real_expr("1+", v0, 1, 128));
    TEST_ASSERT_NULL(fptaylor_eval_real_expr("1/ x0", v0, 1, 128)); /* 除零 */
    TEST_ASSERT_NULL(fptaylor_eval_real_expr(NULL, v0, 1, 128));
    TEST_ASSERT_NULL(fptaylor_eval_real_expr("1", NULL, 1, 128));
    TEST_ASSERT_NULL(fptaylor_eval_real_expr("1", v0, 0, 128)); /* var_count<=0 */

    /* 非法 token（如 ^ 不支持）→ NULL */
    TEST_ASSERT_NULL(fptaylor_eval_real_expr("x0^2", v0, 1, 128));
    printf("  test_fpeval_error: PASSED\n");
}

/* ============== 测试入口 ============== */

TEST_MAIN_BEGIN("Lv-00 FPTaylor REAL Ext Test Suite")
    printf("=== Lv-00 FPTaylor REAL Ext Test Suite (C2 REAL re-eval) ===\n\n");
    lv_init();

    TEST_MAIN_RUN(test_fpeval_basic);
    TEST_MAIN_RUN(test_fpeval_error);

    lv_cleanup();
TEST_MAIN_END()
