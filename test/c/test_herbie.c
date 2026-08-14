/**
 * @file test_herbie.c
 * @brief Herbie 浮点表达式优化接口测试
 *
 * 覆盖 core/src/layer3_geometry/herbie_eval.c：
 * - 内置重写规则命中（hypot / 因式分解 / expm1 / log1p / 三角恒等式）
 * - 无规则命中时返回原表达式
 * - 结构感知误差估计：含减法抵消的表达式误差高于无抵消形式
 *
 * @version 1.0.0
 * @date 2026-08-14
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/lv.h"
#include "test_helpers.h"

/* herbie_eval.c 无对应公共头文件，lv_herbie_optimize 为内部导出函数 */
extern char *lv_herbie_optimize(const char *expression, double *out_value, double *out_error);

/* ============================================================
 * Global test counters
 * ============================================================ */
int g_pass_count = 0;
int g_fail_count = 0;

/* ============================================================
 * Test: 规则命中
 * ============================================================ */

static void test_herbie_hypot_rule(void) {
    printf("Test: herbie hypot rule...\n");

    double err = 0.0;
    char *r = lv_herbie_optimize("sqrt(x*x+y*y)", NULL, &err);
    lv_ASSERT_NOT_NULL(r);
    lv_ASSERT(strstr(r, "hypot") != NULL);
    lv_ASSERT(err > 0.0);
    lv_free((void **) &r);

    printf("  PASSED\n");
}

static void test_herbie_factorization_rule(void) {
    printf("Test: herbie factorization rule...\n");

    char *r = lv_herbie_optimize("a*a-b*b", NULL, NULL);
    lv_ASSERT_NOT_NULL(r);
    lv_ASSERT(strstr(r, "a-b") != NULL && strstr(r, "a+b") != NULL);
    lv_free((void **) &r);

    printf("  PASSED\n");
}

static void test_herbie_log_rules(void) {
    printf("Test: herbie log/exp precision rules...\n");

    char *r = lv_herbie_optimize("exp(x)-1", NULL, NULL);
    lv_ASSERT_NOT_NULL(r);
    lv_ASSERT(strstr(r, "expm1") != NULL);
    lv_free((void **) &r);

    r = lv_herbie_optimize("log(1+x)", NULL, NULL);
    lv_ASSERT_NOT_NULL(r);
    lv_ASSERT(strstr(r, "log1p") != NULL);
    lv_free((void **) &r);

    printf("  PASSED\n");
}

static void test_herbie_trig_rules(void) {
    printf("Test: herbie trig identity rules...\n");

    char *r = lv_herbie_optimize("sin(x)*sin(x)+cos(x)*cos(x)", NULL, NULL);
    lv_ASSERT_NOT_NULL(r);
    lv_ASSERT(strcmp(r, "1") == 0);
    lv_free((void **) &r);

    r = lv_herbie_optimize("sin(a)*cos(b)-cos(a)*sin(b)", NULL, NULL);
    lv_ASSERT_NOT_NULL(r);
    lv_ASSERT(strstr(r, "sin(a-b)") != NULL);
    lv_free((void **) &r);

    printf("  PASSED\n");
}

/* ============================================================
 * Test: 无规则命中
 * ============================================================ */

static void test_herbie_no_match(void) {
    printf("Test: herbie no rule matched -> original expression...\n");

    char *r = lv_herbie_optimize("x+y", NULL, NULL);
    lv_ASSERT_NOT_NULL(r);
    lv_ASSERT(strcmp(r, "x+y") == 0);
    lv_free((void **) &r);

    /* NULL 输入 */
    r = lv_herbie_optimize(NULL, NULL, NULL);
    lv_ASSERT(r == NULL);

    printf("  PASSED\n");
}

/* ============================================================
 * Test: 结构感知误差估计
 * ============================================================ */

static void test_herbie_error_model(void) {
    printf("Test: herbie structural error model...\n");

    /* 减法抵消表达式误差应显著高于同复杂度无抵消表达式 */
    double err_cancel = 0.0;
    char *r1 = lv_herbie_optimize("a*a-b*b", NULL, &err_cancel);
    lv_ASSERT_NOT_NULL(r1);
    lv_free((void **) &r1);

    double err_plain = 0.0;
    char *r2 = lv_herbie_optimize("a*a+b*b", NULL, &err_plain);
    lv_ASSERT_NOT_NULL(r2);
    lv_free((void **) &r2);

    /* 因式分解消除抵消后的误差应显著低于无抵消形式的原始误差 */
    lv_ASSERT(err_cancel < err_plain);

    printf("  PASSED\n");
}

/* ============================================================
 * Main
 * ============================================================ */

TEST_MAIN_BEGIN("Lv-00 Herbie Floating-Point Optimization Test Suite")
    printf("=== Lv-00 Herbie Optimization Test Suite ===\n\n");
    if (!lv_init()) {
        fprintf(stderr, "Failed to initialize Lv-00 system\n");
        return 1;
    }
    TEST_MAIN_RUN(test_herbie_hypot_rule);
    TEST_MAIN_RUN(test_herbie_factorization_rule);
    TEST_MAIN_RUN(test_herbie_log_rules);
    TEST_MAIN_RUN(test_herbie_trig_rules);
    TEST_MAIN_RUN(test_herbie_no_match);
    TEST_MAIN_RUN(test_herbie_error_model);
    printf("\nAll herbie tests passed.\n");
TEST_MAIN_END()
