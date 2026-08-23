/**
 * @file test_interval_arith_ext.c
 * @brief 公共区间算术库契约测试（批次 C-㊺续30：interval_arith.h 零覆盖 API）
 *
 * 覆盖零覆盖 API（7 个）：
 *   lv_interval_abs / cos / log / neg / sin / sqrt / sub
 *
 * 契约要点（与 interval_arith.h / interval_arith.c 核对）：
 *   - sub：[a.lo-b.hi, a.hi-b.lo]，端点向外取整（nextafter 1 ulp）。
 *   - sqrt：负下界截断到 0；is_exact = a.is_exact && (a.lo == a.hi)。
 *   - sin/cos：处理非单调区间与极值点，结果范围 [-1, 1]；
 *     宽区间（≥ 2π）覆盖全范围 [-1, 1]。
 *   - log：非正下界 -> lo = -HUGE_VAL（保守全下界）；a.hi <= 0 时 hi 也 -HUGE_VAL。
 *   - abs：全正保序 / 全负翻转 / 跨零 [0, max(-lo, hi)]。
 *   - neg：[-a.hi, -a.lo]。
 *
 * @author Lv-00 Project
 */

#include <math.h>
#include <stdio.h>

#include "lv/interval_arith.h"

#include "test_unified.h"

int g_pass_count = 0;
int g_fail_count = 0;

#define TOL 1e-12

/* ============== 测试：区间减法 ============== */

static void test_sub_api(void) {
    /* [1,2] - [3,4] = [1-4, 2-3] = [-3, -1] */
    lvInterval a = lv_interval_make(1.0, 2.0, 1);
    lvInterval b = lv_interval_make(3.0, 4.0, 1);
    lvInterval r = lv_interval_sub(a, b);
    TEST_ASSERT_DOUBLE(r.lo, -3.0, TOL);
    TEST_ASSERT_DOUBLE(r.hi, -1.0, TOL);
    TEST_ASSERT_EQ(r.is_exact, 1);

    /* is_exact 传播：任一操作数不精确则结果不精确 */
    lvInterval c = lv_interval_make(0.0, 0.0, 0);
    r = lv_interval_sub(a, c);
    TEST_ASSERT_EQ(r.is_exact, 0);

    /* 退化区间 */
    lvInterval p = lv_interval_make(5.0, 5.0, 1);
    r = lv_interval_sub(p, p);
    TEST_ASSERT_DOUBLE(r.lo, 0.0, TOL);
    TEST_ASSERT_DOUBLE(r.hi, 0.0, TOL);
    TEST_ASSERT_EQ(r.is_exact, 1);
}

/* ============== 测试：区间平方根 ============== */

static void test_sqrt_api(void) {
    /* 退化精确区间：sqrt([0.25,0.25]) = [0.5,0.5]，is_exact 保留 */
    lvInterval a = lv_interval_make(0.25, 0.25, 1);
    lvInterval r = lv_interval_sqrt(a);
    TEST_ASSERT_DOUBLE(r.lo, 0.5, TOL);
    TEST_ASSERT_DOUBLE(r.hi, 0.5, TOL);
    TEST_ASSERT_EQ(r.is_exact, 1);

    /* 非退化：[0,9] -> [0,3] */
    r = lv_interval_sqrt(lv_interval_make(0.0, 9.0, 1));
    TEST_ASSERT_DOUBLE(r.lo, 0.0, TOL);
    TEST_ASSERT_DOUBLE(r.hi, 3.0, TOL);
    TEST_ASSERT_EQ(r.is_exact, 0);

    /* 负下界截断到 0：[-1,4] -> [0,2] */
    r = lv_interval_sqrt(lv_interval_make(-1.0, 4.0, 0));
    TEST_ASSERT_DOUBLE(r.lo, 0.0, TOL);
    TEST_ASSERT_DOUBLE(r.hi, 2.0, TOL);
}

/* ============== 测试：区间正弦 ============== */

static void test_sin_api(void) {
    /* [0, pi/2]：含峰值 pi/2 -> [0, 1] */
    lvInterval a = lv_interval_make(0.0, 3.14159265358979323846 / 2.0, 0);
    lvInterval r = lv_interval_sin(a);
    TEST_ASSERT_DOUBLE(r.lo, 0.0, TOL);
    TEST_ASSERT_DOUBLE(r.hi, 1.0, TOL);

    /* 单点退化：sin(pi/2) = 1 */
    r = lv_interval_sin(lv_interval_make(3.14159265358979323846 / 2.0, 3.14159265358979323846 / 2.0, 1));
    TEST_ASSERT_DOUBLE(r.lo, 1.0, TOL);
    TEST_ASSERT_DOUBLE(r.hi, 1.0, TOL);
    TEST_ASSERT_EQ(r.is_exact, 1);

    /* 宽区间（宽度 > 2π）：覆盖全范围 [-1, 1] */
    r = lv_interval_sin(lv_interval_make(-4.0, 4.0, 0));
    TEST_ASSERT_DOUBLE(r.lo, -1.0, TOL);
    TEST_ASSERT_DOUBLE(r.hi, 1.0, TOL);
}

/* ============== 测试：区间余弦 ============== */

static void test_cos_api(void) {
    /* [0, pi/2]：cos 单调递减 1 -> 0 */
    lvInterval a = lv_interval_make(0.0, 3.14159265358979323846 / 2.0, 0);
    lvInterval r = lv_interval_cos(a);
    TEST_ASSERT_DOUBLE(r.lo, 0.0, TOL);
    TEST_ASSERT_DOUBLE(r.hi, 1.0, TOL);

    /* [0, pi]：含极值点 0（偶次 -> max=1）与 pi（奇次 -> min=-1）-> [-1, 1] */
    r = lv_interval_cos(lv_interval_make(0.0, 3.14159265358979323846, 0));
    TEST_ASSERT_DOUBLE(r.lo, -1.0, TOL);
    TEST_ASSERT_DOUBLE(r.hi, 1.0, TOL);

    /* 宽区间覆盖全范围 */
    r = lv_interval_cos(lv_interval_make(-4.0, 4.0, 0));
    TEST_ASSERT_DOUBLE(r.lo, -1.0, TOL);
    TEST_ASSERT_DOUBLE(r.hi, 1.0, TOL);
}

/* ============== 测试：区间自然对数 ============== */

static void test_log_api(void) {
    /* [1, e] -> [0, 1] */
    lvInterval a = lv_interval_make(1.0, 2.71828182845904523536, 1);
    lvInterval r = lv_interval_log(a);
    TEST_ASSERT_DOUBLE(r.lo, 0.0, TOL);
    TEST_ASSERT_DOUBLE(r.hi, 1.0, TOL);
    TEST_ASSERT_EQ(r.is_exact, 0);

    /* 非正下界：lo = -HUGE_VAL，hi 取 log(a.hi) */
    r = lv_interval_log(lv_interval_make(0.0, 2.71828182845904523536, 0));
    TEST_ASSERT(r.lo == -HUGE_VAL, "log non-positive lo -> -HUGE_VAL");
    TEST_ASSERT_DOUBLE(r.hi, 1.0, TOL);

    /* 全非正区间：两端均为 -HUGE_VAL */
    r = lv_interval_log(lv_interval_make(-2.0, -1.0, 0));
    TEST_ASSERT(r.lo == -HUGE_VAL, "log all-non-positive lo");
    TEST_ASSERT(r.hi == -HUGE_VAL, "log all-non-positive hi");
}

/* ============== 测试：区间绝对值 ============== */

static void test_abs_api(void) {
    /* 全非负：保序 */
    lvInterval r = lv_interval_abs(lv_interval_make(1.0, 2.0, 1));
    TEST_ASSERT_DOUBLE(r.lo, 1.0, TOL);
    TEST_ASSERT_DOUBLE(r.hi, 2.0, TOL);
    TEST_ASSERT_EQ(r.is_exact, 1);

    /* 全非正：翻转 */
    r = lv_interval_abs(lv_interval_make(-2.0, -1.0, 1));
    TEST_ASSERT_DOUBLE(r.lo, 1.0, TOL);
    TEST_ASSERT_DOUBLE(r.hi, 2.0, TOL);

    /* 跨零：[0, max(-lo, hi)] */
    r = lv_interval_abs(lv_interval_make(-2.0, 1.0, 1));
    TEST_ASSERT_DOUBLE(r.lo, 0.0, TOL);
    TEST_ASSERT_DOUBLE(r.hi, 2.0, TOL);

    /* 跨零对称 */
    r = lv_interval_abs(lv_interval_make(-1.0, 2.0, 1));
    TEST_ASSERT_DOUBLE(r.lo, 0.0, TOL);
    TEST_ASSERT_DOUBLE(r.hi, 2.0, TOL);
}

/* ============== 测试：区间取反 ============== */

static void test_neg_api(void) {
    /* [1,2] -> [-2,-1] */
    lvInterval r = lv_interval_neg(lv_interval_make(1.0, 2.0, 1));
    TEST_ASSERT_DOUBLE(r.lo, -2.0, TOL);
    TEST_ASSERT_DOUBLE(r.hi, -1.0, TOL);
    TEST_ASSERT_EQ(r.is_exact, 1);

    /* 跨零：[-2,3] -> [-3,2] */
    r = lv_interval_neg(lv_interval_make(-2.0, 3.0, 0));
    TEST_ASSERT_DOUBLE(r.lo, -3.0, TOL);
    TEST_ASSERT_DOUBLE(r.hi, 2.0, TOL);
    TEST_ASSERT_EQ(r.is_exact, 0);

    /* 两次取反回到原区间 */
    lvInterval orig = lv_interval_make(-2.0, 3.0, 1);
    r = lv_interval_neg(lv_interval_neg(orig));
    TEST_ASSERT_DOUBLE(r.lo, -2.0, TOL);
    TEST_ASSERT_DOUBLE(r.hi, 3.0, TOL);
}

/* ============== Main ============== */

TEST_MAIN_BEGIN("IntervalArithExt")

    printf("\n--- interval_arith (zero-coverage) ---\n");
    TEST_MAIN_RUN(test_sub_api);
    TEST_MAIN_RUN(test_sqrt_api);
    TEST_MAIN_RUN(test_sin_api);
    TEST_MAIN_RUN(test_cos_api);
    TEST_MAIN_RUN(test_log_api);
    TEST_MAIN_RUN(test_abs_api);
    TEST_MAIN_RUN(test_neg_api);

TEST_MAIN_END()
