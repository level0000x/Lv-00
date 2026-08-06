/**
 * @file test_interval_arith.c
 * @brief 区间算术公共库（lv/interval_arith.h）扩展函数测试
 *
 * 覆盖 core/src/layer3_geometry/interval_arith.c 的扩展函数
 * （保守端点法，float_error 语义）：
 * - lv_interval_tan ：含 π/2+kπ 奇点 -> [-HUGE_VAL, HUGE_VAL]；单调段端点外扩
 * - lv_interval_atan：全实数域单调递增，端点 nextafter 向外舍入
 * - lv_interval_pow ：正底 4 角点法；底跨零仅整数幂；负底非整数幂 -> 全实数
 * - lv_interval_asin/acos：与定义域 [-1,1] 求交；交空 -> 全实数；单调求值
 * - lv_interval_floor/ceil：结果精确整数，无需向外舍入
 * - 方向舍入：端点 nextafter 向外舍入（ia_round_down / ia_round_up）
 * - 定义域外语义：返回保守全实数区间 [-HUGE_VAL, HUGE_VAL]，
 *   区别于 interval_arithmetic（IEEE 1788 空区间）语义（本文件只测 interval_arith）
 *
 * @version 1.0.0
 * @date 2026-08-06
 */

#include "lv/lv_platform.h"
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/interval_arith.h"
#include "test_helpers.h"

/* ============================================================
 * Global test counters
 * ============================================================ */
int g_pass_count = 0;
int g_fail_count = 0;

/* ============================================================
 * Test: tan —— 奇点与单调段
 * ============================================================ */

static void test_tan_singularity_entire(void) {
    /* [1.5, 1.6] 含奇点 π/2≈1.5708 -> 全实数 */
    lvInterval iv = lv_interval_tan(lv_interval_make(1.5, 1.6, 0));
    TEST_ASSERT_MSG(iv.lo == -HUGE_VAL && iv.hi == HUGE_VAL,
                    "tan interval containing pi/2 singularity should be entire");
    TEST_ASSERT_MSG(iv.is_exact == 0, "entire result should not be exact");
}

static void test_tan_monotone_outward(void) {
    /* [0, 1] 不含奇点，tan 单调递增，端点向外舍入 */
    lvInterval iv = lv_interval_tan(lv_interval_make(0.0, 1.0, 0));
    TEST_ASSERT_MSG(iv.lo == 0.0, "tan([0,1]) lo should be 0 (round_down(0)=-0.0)");
    TEST_ASSERT_MSG(iv.hi > tan(1.0), "tan([0,1]) hi should round up beyond tan(1)");
    TEST_ASSERT_MSG(approx_eq_eps(iv.hi, tan(1.0), 1e-12), "tan([0,1]) hi ~ tan(1)");
}

/* ============================================================
 * Test: atan —— 单调外扩
 * ============================================================ */

static void test_atan_monotone_outward(void) {
    lvInterval iv = lv_interval_atan(lv_interval_make(-1.0, 1.0, 0));
    TEST_ASSERT_MSG(iv.lo < -M_PI / 4.0, "atan([-1,1]) lo should round down below -pi/4");
    TEST_ASSERT_MSG(approx_eq_eps(iv.lo, -M_PI / 4.0, 1e-15), "atan([-1,1]) lo ~ -pi/4");
    TEST_ASSERT_MSG(iv.hi > M_PI / 4.0, "atan([-1,1]) hi should round up above pi/4");
    TEST_ASSERT_MSG(approx_eq_eps(iv.hi, M_PI / 4.0, 1e-15), "atan([-1,1]) hi ~ pi/4");
}

/* ============================================================
 * Test: pow —— 正底 / 底跨零 / 负底
 * ============================================================ */

static void test_pow_positive_base(void) {
    /* [2,3]^[2,3] = [4,27]（4 角点法） */
    lvInterval iv = lv_interval_pow(lv_interval_make(2.0, 3.0, 0), lv_interval_make(2.0, 3.0, 0));
    TEST_ASSERT_MSG(iv.lo < 4.0, "pow([2,3],[2,3]) lo should round down below 4");
    TEST_ASSERT_MSG(approx_eq_eps(iv.lo, 4.0, 1e-12), "pow([2,3],[2,3]) lo ~ 4");
    TEST_ASSERT_MSG(iv.hi > 27.0, "pow([2,3],[2,3]) hi should round up above 27");
    TEST_ASSERT_MSG(approx_eq_eps(iv.hi, 27.0, 1e-12), "pow([2,3],[2,3]) hi ~ 27");
}

static void test_pow_cross_zero_even_int(void) {
    /* [-2,2]^2：底跨零 + 偶整数幂 -> [0,4]（含 0 与端点） */
    lvInterval iv = lv_interval_pow(lv_interval_make(-2.0, 2.0, 0), lv_interval_make(2.0, 2.0, 0));
    TEST_ASSERT_MSG(iv.lo == 0.0, "pow([-2,2],2) lo should be 0 (round_down(0)=-0.0)");
    TEST_ASSERT_MSG(approx_eq_eps(iv.hi, 4.0, 1e-12), "pow([-2,2],2) hi ~ 4");
}

static void test_pow_negative_base_odd_int(void) {
    /* [-3,-1]^3：负底 + 奇整数幂，单调递增 */
    lvInterval iv = lv_interval_pow(lv_interval_make(-3.0, -1.0, 0), lv_interval_make(3.0, 3.0, 0));
    TEST_ASSERT_MSG(iv.lo < -27.0, "pow([-3,-1],3) lo should round down below -27");
    TEST_ASSERT_MSG(approx_eq_eps(iv.lo, -27.0, 1e-12), "pow([-3,-1],3) lo ~ -27");
    TEST_ASSERT_MSG(iv.hi > -1.0 && iv.hi < 0.0, "pow([-3,-1],3) hi ~ -1 rounded up (above -1)");
    TEST_ASSERT_MSG(approx_eq_eps(iv.hi, -1.0, 1e-12), "pow([-3,-1],3) hi ~ -1");
}

static void test_pow_negative_base_noninteger(void) {
    /* [-2,-1]^[0.5,1.5]：负底 + 非整数幂无实数定义 -> 全实数 */
    lvInterval iv = lv_interval_pow(lv_interval_make(-2.0, -1.0, 0), lv_interval_make(0.5, 1.5, 0));
    TEST_ASSERT_MSG(iv.lo == -HUGE_VAL && iv.hi == HUGE_VAL,
                    "pow with negative base and non-integer exponent should be entire");
}

static void test_pow_cross_zero_noninteger(void) {
    /* [-2,2]^[0.5,1.5]：底跨零但指数非单个整数 -> 全实数 */
    lvInterval iv = lv_interval_pow(lv_interval_make(-2.0, 2.0, 0), lv_interval_make(0.5, 1.5, 0));
    TEST_ASSERT_MSG(iv.lo == -HUGE_VAL && iv.hi == HUGE_VAL,
                    "pow with base crossing 0 and non-integer exponent should be entire");
}

/* ============================================================
 * Test: asin —— 定义域交集与单调
 * ============================================================ */

static void test_asin_in_domain_monotone(void) {
    lvInterval iv = lv_interval_asin(lv_interval_make(0.0, 0.5, 0));
    TEST_ASSERT_MSG(iv.lo == 0.0, "asin([0,0.5]) lo should be 0 (round_down(0)=-0.0)");
    TEST_ASSERT_MSG(iv.hi > asin(0.5), "asin([0,0.5]) hi should round up beyond asin(0.5)");
    TEST_ASSERT_MSG(approx_eq_eps(iv.hi, asin(0.5), 1e-12), "asin([0,0.5]) hi ~ asin(0.5)");
}

static void test_asin_out_of_domain_entire(void) {
    /* [1.5,2.0] 与定义域 [-1,1] 交集为空 -> 全实数 */
    lvInterval iv = lv_interval_asin(lv_interval_make(1.5, 2.0, 0));
    TEST_ASSERT_MSG(iv.lo == -HUGE_VAL && iv.hi == HUGE_VAL,
                    "asin out of domain (disjoint) should be entire");
}

static void test_asin_partial_domain(void) {
    /* [0.5,2.0] 截断到 [0.5,1] -> [asin(0.5), pi/2] */
    lvInterval iv = lv_interval_asin(lv_interval_make(0.5, 2.0, 0));
    TEST_ASSERT_MSG(approx_eq_eps(iv.hi, M_PI / 2.0, 1e-12), "asin([0.5,2]) hi ~ pi/2");
    TEST_ASSERT_MSG(iv.hi > M_PI / 2.0, "asin([0.5,2]) hi should round up beyond pi/2");
}

/* ============================================================
 * Test: acos —— 定义域交集与单调递减
 * ============================================================ */

static void test_acos_in_domain_monotone(void) {
    /* acos 单调递减：下界取 hi 端 acos(0.5)=pi/3，上界取 lo 端 acos(-0.5)=2pi/3 */
    lvInterval iv = lv_interval_acos(lv_interval_make(-0.5, 0.5, 0));
    TEST_ASSERT_MSG(iv.lo < acos(0.5), "acos([-0.5,0.5]) lo should round down below acos(0.5)");
    TEST_ASSERT_MSG(approx_eq_eps(iv.lo, acos(0.5), 1e-12), "acos([-0.5,0.5]) lo ~ pi/3");
    TEST_ASSERT_MSG(iv.hi > acos(-0.5), "acos([-0.5,0.5]) hi should round up above acos(-0.5)");
    TEST_ASSERT_MSG(approx_eq_eps(iv.hi, acos(-0.5), 1e-12), "acos([-0.5,0.5]) hi ~ 2pi/3");
}

static void test_acos_out_of_domain_entire(void) {
    /* [2.0,3.0] 与定义域 [-1,1] 交集为空 -> 全实数 */
    lvInterval iv = lv_interval_acos(lv_interval_make(2.0, 3.0, 0));
    TEST_ASSERT_MSG(iv.lo == -HUGE_VAL && iv.hi == HUGE_VAL,
                    "acos out of domain (disjoint) should be entire");
}

/* ============================================================
 * Test: floor / ceil —— 精确整数端点
 * ============================================================ */

static void test_floor_exact_integers(void) {
    lvInterval iv = lv_interval_floor(lv_interval_make(1.2, 3.8, 0));
    TEST_ASSERT_MSG(iv.lo == 1.0 && iv.hi == 3.0, "floor([1.2,3.8]) = [1,3]");

    iv = lv_interval_floor(lv_interval_make(-1.2, 0.5, 0));
    TEST_ASSERT_MSG(iv.lo == -2.0 && iv.hi == 0.0, "floor([-1.2,0.5]) = [-2,0]");

    /* 整数端点原样保留 */
    iv = lv_interval_floor(lv_interval_make(2.0, 3.0, 0));
    TEST_ASSERT_MSG(iv.lo == 2.0 && iv.hi == 3.0, "floor([2,3]) = [2,3]");
}

static void test_ceil_exact_integers(void) {
    lvInterval iv = lv_interval_ceil(lv_interval_make(1.2, 3.8, 0));
    TEST_ASSERT_MSG(iv.lo == 2.0 && iv.hi == 4.0, "ceil([1.2,3.8]) = [2,4]");

    iv = lv_interval_ceil(lv_interval_make(-1.2, 0.5, 0));
    TEST_ASSERT_MSG(iv.lo == -1.0 && iv.hi == 1.0, "ceil([-1.2,0.5]) = [-1,1]");
}

static void test_floor_ceil_point_exact_flag(void) {
    /* 单点精确区间：floor/ceil 结果 is_exact 保持 1 */
    lvInterval iv = lv_interval_floor(lv_interval_make(2.0, 2.0, 1));
    TEST_ASSERT_MSG(iv.lo == 2.0 && iv.hi == 2.0, "floor point [2,2]");
    TEST_ASSERT_MSG(iv.is_exact == 1, "floor of exact point should be exact");

    iv = lv_interval_ceil(lv_interval_make(-1.5, -1.5, 1));
    TEST_ASSERT_MSG(iv.lo == -1.0 && iv.hi == -1.0, "ceil point [-1.5,-1.5] = [-1,-1]");
    TEST_ASSERT_MSG(iv.is_exact == 1, "ceil of exact point should be exact");
}

/* ============================================================
 * Test: float_error 方向舍入语义（端点 nextafter 向外）
 * ============================================================ */

static void test_directed_rounding_outward(void) {
    /* 单点 exp：lo 向 -∞ 外扩、hi 向 +∞ 外扩，且都贴近真实值 */
    lvInterval iv = lv_interval_exp(lv_interval_make(0.5, 0.5, 1));
    TEST_ASSERT_MSG(iv.lo < exp(0.5) && iv.hi > exp(0.5),
                    "exp([0.5,0.5]) should round outward around exp(0.5)");
    TEST_ASSERT_MSG(approx_eq_eps(iv.lo, exp(0.5), 1e-12), "exp lo ~ exp(0.5)");
    TEST_ASSERT_MSG(approx_eq_eps(iv.hi, exp(0.5), 1e-12), "exp hi ~ exp(0.5)");

    /* 加法：0.1+0.2 的真实 double 值被严格包含在 (lo, hi) 内 */
    lvInterval s = lv_interval_add(lv_interval_make(0.1, 0.1, 1), lv_interval_make(0.2, 0.2, 1));
    double exact = 0.1 + 0.2;
    TEST_ASSERT_MSG(s.lo < exact && s.hi > exact,
                    "add([0.1,0.1],[0.2,0.2]) should strictly contain 0.1+0.2");

    /* 乘法同理：2.0*3.0=6.0 */
    lvInterval m = lv_interval_mul(lv_interval_make(2.0, 2.0, 1), lv_interval_make(3.0, 3.0, 1));
    TEST_ASSERT_MSG(m.lo < 6.0 && m.hi > 6.0, "mul([2,2],[3,3]) should round outward around 6.0");
}

/* ============================================================
 * Test: 定义域外语义 —— 与 interval_arithmetic（IEEE 1788 空区间）的区别
 * ============================================================ */

static void test_div_cross_zero_entire_not_empty(void) {
    /* float_error 语义：除数跨零返回全实数区间而非 IEEE 1788 空区间 */
    lvInterval iv = lv_interval_div(lv_interval_make(1.0, 2.0, 0), lv_interval_make(-1.0, 1.0, 0));
    TEST_ASSERT_MSG(iv.lo == -HUGE_VAL && iv.hi == HUGE_VAL,
                    "div by interval containing 0 should be entire [-HUGE_VAL, HUGE_VAL]");
    TEST_ASSERT_MSG(iv.is_exact == 0, "entire result should not be exact");
    TEST_ASSERT_MSG(iv.lo <= iv.hi, "entire interval is not an empty interval (lo<=hi)");
}

/* ============================================================
 * Main
 * ============================================================ */
TEST_MAIN_BEGIN("IntervalArith")

    TEST_MAIN_RUN(test_tan_singularity_entire);
    TEST_MAIN_RUN(test_tan_monotone_outward);
    TEST_MAIN_RUN(test_atan_monotone_outward);

    TEST_MAIN_RUN(test_pow_positive_base);
    TEST_MAIN_RUN(test_pow_cross_zero_even_int);
    TEST_MAIN_RUN(test_pow_negative_base_odd_int);
    TEST_MAIN_RUN(test_pow_negative_base_noninteger);
    TEST_MAIN_RUN(test_pow_cross_zero_noninteger);

    TEST_MAIN_RUN(test_asin_in_domain_monotone);
    TEST_MAIN_RUN(test_asin_out_of_domain_entire);
    TEST_MAIN_RUN(test_asin_partial_domain);
    TEST_MAIN_RUN(test_acos_in_domain_monotone);
    TEST_MAIN_RUN(test_acos_out_of_domain_entire);

    TEST_MAIN_RUN(test_floor_exact_integers);
    TEST_MAIN_RUN(test_ceil_exact_integers);
    TEST_MAIN_RUN(test_floor_ceil_point_exact_flag);

    TEST_MAIN_RUN(test_directed_rounding_outward);
    TEST_MAIN_RUN(test_div_cross_zero_entire_not_empty);

TEST_MAIN_END()
