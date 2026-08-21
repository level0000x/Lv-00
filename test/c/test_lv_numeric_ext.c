/**
 * @file test_lv_numeric_ext.c
 * @brief 数值工具契约测试（批次 C-㊺续6：lv_numeric.h 19 个零覆盖 API）
 *
 * 覆盖 19 个 ctest 零覆盖 API：
 *   - 比较族：lv_is_zero / is_equal / is_positive / is_negative /
 *     is_integer_double
 *   - 范围族：lv_is_in_range / lv_clamp / lv_index_in_range
 *   - 转换族：lv_rad_to_deg / lv_sign / lv_sign_int / lv_mpq_set_d_checked
 *   - 多项式族：lv_evaluate_quadratic / lv_evaluate_cubic
 *   - 差分族：lv_finite_difference / lv_finite_difference_vec /
 *     lv_fd_step_adaptive / lv_fd_step_relative
 *   - 插值族：lv_lerp
 *
 * 契约要点（与头注释核对）：
 *   - is_zero: |x| < epsilon；is_positive: x > epsilon。
 *   - index_in_range: idx >= 0 && idx < bound（bound<=0 恒 false）。
 *   - fd_step_adaptive = eps*max(1,|x|)；fd_step_relative = eps*(|x|+1)。
 *   - finite_difference：f(x)=x² 在 x 处导数 ≈ 2x（中心差分 O(h²)）。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "test_unified.h"
#include "lv/lv_numeric.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============== 测试：浮点比较 ============== */

static void test_compare_api(void) {
    double eps = 1e-9;

    /* is_zero */
    TEST_ASSERT(lv_is_zero(0.0, eps), "0 为零");
    TEST_ASSERT(lv_is_zero(1e-10, eps), "小值近零");
    TEST_ASSERT(!lv_is_zero(1e-8, eps), "大值非零");

    /* is_equal */
    TEST_ASSERT(lv_is_equal(1.0, 1.0 + 1e-10, eps), "近似相等");
    TEST_ASSERT(!lv_is_equal(1.0, 1.0 + 1e-6, eps), "差异超阈值");

    /* is_positive / is_negative */
    TEST_ASSERT(lv_is_positive(1e-8, eps), "大于阈值正");
    TEST_ASSERT(!lv_is_positive(1e-10, eps), "小于阈值非正");
    TEST_ASSERT(lv_is_negative(-1e-8, eps), "小于负阈值负");
    TEST_ASSERT(!lv_is_negative(-1e-10, eps), "接近零非负");

    /* is_integer_double */
    TEST_ASSERT(lv_is_integer_double(3.0, eps), "整数值");
    TEST_ASSERT(lv_is_integer_double(3.0 + 1e-10, eps), "近似整数值");
    TEST_ASSERT(!lv_is_integer_double(3.5, eps), "非整数");

    printf("  test_compare_api: PASSED\n");
}

/* ============== 测试：范围与限制 ============== */

static void test_range_api(void) {
    /* is_in_range：闭区间 */
    TEST_ASSERT(lv_is_in_range(5.0, 0.0, 10.0), "区间内");
    TEST_ASSERT(lv_is_in_range(0.0, 0.0, 10.0), "下界含");
    TEST_ASSERT(lv_is_in_range(10.0, 0.0, 10.0), "上界含");
    TEST_ASSERT(!lv_is_in_range(-1.0, 0.0, 10.0), "低于下界");
    TEST_ASSERT(!lv_is_in_range(11.0, 0.0, 10.0), "高于上界");

    /* clamp */
    TEST_ASSERT_EQ(lv_clamp(5.0, 0.0, 10.0), 5.0);
    TEST_ASSERT_EQ(lv_clamp(-3.0, 0.0, 10.0), 0.0);
    TEST_ASSERT_EQ(lv_clamp(15.0, 0.0, 10.0), 10.0);

    /* index_in_range（inline） */
    TEST_ASSERT(lv_index_in_range(0, 5), "idx 0");
    TEST_ASSERT(lv_index_in_range(4, 5), "idx bound-1");
    TEST_ASSERT(!lv_index_in_range(5, 5), "idx == bound");
    TEST_ASSERT(!lv_index_in_range(-1, 5), "负 idx");
    TEST_ASSERT(!lv_index_in_range(0, 0), "bound 0 恒 false");
    TEST_ASSERT(!lv_index_in_range(0, -1), "负 bound");

    printf("  test_range_api: PASSED\n");
}

/* ============== 测试：转换与符号 ============== */

static void test_convert_api(void) {
    /* rad_to_deg */
    TEST_ASSERT_EQ(lv_rad_to_deg(lv_PI), 180.0);
    TEST_ASSERT_EQ(lv_rad_to_deg(0.0), 0.0);
    TEST_ASSERT_EQ(lv_rad_to_deg(lv_HALF_PI), 90.0);

    /* sign / sign_int */
    TEST_ASSERT_EQ(lv_sign(3.5), 1.0);
    TEST_ASSERT_EQ(lv_sign(-2.5), -1.0);
    TEST_ASSERT_EQ(lv_sign(0.0), 0.0);
    TEST_ASSERT_EQ(lv_sign_int(7), 1);
    TEST_ASSERT_EQ(lv_sign_int(-7), -1);
    TEST_ASSERT_EQ(lv_sign_int(0), 0);

    /* mpq_set_d_checked：double → mpq 精确值 */
    mpq_t q;
    mpq_init(q);
    lv_mpq_set_d_checked(q, 1.5);
    TEST_ASSERT_EQ(mpq_get_d(q), 1.5);
    lv_mpq_set_d_checked(q, -0.25);
    TEST_ASSERT_EQ(mpq_get_d(q), -0.25);
    mpq_clear(q);

    printf("  test_convert_api: PASSED\n");
}

/* ============== 测试：多项式求值 ============== */

static void test_poly_api(void) {
    /* quadratic: 2x² + 3x + 1，x=2 → 8+6+1 = 15 */
    TEST_ASSERT_EQ(lv_evaluate_quadratic(2.0, 3.0, 1.0, 2.0), 15.0);
    TEST_ASSERT_EQ(lv_evaluate_quadratic(2.0, 3.0, 1.0, 0.0), 1.0);

    /* cubic: x³ - 2x² + 4x - 8，x=3 → 27-18+12-8 = 13 */
    TEST_ASSERT_EQ(lv_evaluate_cubic(1.0, -2.0, 4.0, -8.0, 3.0), 13.0);
    TEST_ASSERT_EQ(lv_evaluate_cubic(1.0, -2.0, 4.0, -8.0, 0.0), -8.0);

    printf("  test_poly_api: PASSED\n");
}

/* ============== 测试：有限差分 ============== */

static double sq_func(double x, void *ud) {
    (void)ud;
    return x * x;
}

static int vec_func(double x, void *ud, double *out, int n) {
    (void)ud;
    if (n >= 1)
        out[0] = x * x;
    if (n >= 2)
        out[1] = 2.0 * x;
    return 0;
}

static void test_fd_api(void) {
    /* 中心差分：f(x)=x² 导数在 x=3 ≈ 6 */
    double d = lv_finite_difference(sq_func, 3.0, 1e-5, lv_FD_CENTRAL, NULL);
    TEST_ASSERT(fabs(d - 6.0) < 1e-6, "中心差分 2x≈6");

    /* 前向差分：同样近似 */
    d = lv_finite_difference(sq_func, 3.0, 1e-5, lv_FD_FORWARD, NULL);
    TEST_ASSERT(fabs(d - 6.0) < 1e-4, "前向差分 2x≈6");

    /* 默认步长（h<=0 自适应） */
    d = lv_finite_difference(sq_func, 3.0, 0.0, lv_FD_CENTRAL, NULL);
    TEST_ASSERT(fabs(d - 6.0) < 1e-4, "默认步长中心差分");

    /* 向量差分：F=(x², 2x)，n=2 */
    double df[2] = {0, 0};
    TEST_ASSERT_EQ(lv_finite_difference_vec(vec_func, NULL, 3.0, 1e-5, lv_FD_CENTRAL, NULL, df, 2), 0);
    TEST_ASSERT(fabs(df[0] - 6.0) < 1e-6, "分量0 导数");
    TEST_ASSERT(fabs(df[1] - 2.0) < 1e-4, "分量1 导数");

    /* 步长 helper（inline） */
    TEST_ASSERT_EQ(lv_fd_step_adaptive(3.0, 1e-8), 3e-8);
    TEST_ASSERT_EQ(lv_fd_step_adaptive(0.5, 1e-8), 1e-8); /* |x|<=1 → eps */
    TEST_ASSERT_EQ(lv_fd_step_relative(3.0, 1e-8), 4e-8); /* eps*(3+1) */
    TEST_ASSERT_EQ(lv_fd_step_relative(0.0, 1e-8), 1e-8); /* eps*(0+1) */

    printf("  test_fd_api: PASSED\n");
}

/* ============== 测试：线性插值 ============== */

static void test_lerp_api(void) {
    TEST_ASSERT_EQ(lv_lerp(0.0, 10.0, 0.0), 0.0);
    TEST_ASSERT_EQ(lv_lerp(0.0, 10.0, 1.0), 10.0);
    TEST_ASSERT_EQ(lv_lerp(0.0, 10.0, 0.5), 5.0);
    TEST_ASSERT_EQ(lv_lerp(2.0, 4.0, 0.25), 2.5);
    printf("  test_lerp_api: PASSED\n");
}

/* ============== 测试入口 ============== */

TEST_MAIN_BEGIN("Lv-00 Numeric Ext Test Suite")
    printf("=== Lv-00 Numeric Ext Test Suite (batch C-㊺续6) ===\n\n");
    lv_init();

    TEST_MAIN_RUN(test_compare_api);
    TEST_MAIN_RUN(test_range_api);
    TEST_MAIN_RUN(test_convert_api);
    TEST_MAIN_RUN(test_poly_api);
    TEST_MAIN_RUN(test_fd_api);
    TEST_MAIN_RUN(test_lerp_api);

    lv_cleanup();
TEST_MAIN_END()
