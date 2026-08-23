/**
 * @file test_alg_interval_ext.c
 * @brief 代数数域契约测试（批次 C-㊺续13：algebraic_number.h 区间族零覆盖 API）
 *
 * 覆盖 interval 族：
 *   create / point / from_quadratic / add / sub / mul / div / neg /
 *   intersect / hull / contains / contains_rational / is_empty / is_point /
 *   width / midpoint / bisect / to_string / error_string（19 个）+ 跨层
 *   quadratic_to_interval。
 *
 * 契约要点（与头注释核对）：
 *   - create lo>hi 时交换；零分母 → ERR_INVALID（修复点）。
 *   - div 除数区间含零 → ERR_DIV_BY_ZERO。
 *   - intersect 空交集 → ERR_EMPTY。
 *   - 溢出必须上报 ERR_OVERFLOW（修复点：原实现静默丢弃 rational 错误码）。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_unified.h"
#include "lv/algebraic_number.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============== 测试：构造 ============== */

static void test_interval_create_api(void) {
    AlgIntervalError err = lv_alg_interval_OK;

    /* create 正常 */
    AlgInterval iv = lv_alg_interval_create(1, 1, 3, 1, &err);
    TEST_ASSERT_EQ(err, lv_alg_interval_OK);
    TEST_ASSERT_EQ(iv.lo.num, 1);
    TEST_ASSERT_EQ(iv.hi.num, 3);

    /* lo > hi → 交换 */
    iv = lv_alg_interval_create(5, 1, 2, 1, &err);
    TEST_ASSERT_EQ(err, lv_alg_interval_OK);
    TEST_ASSERT_EQ(iv.lo.num, 2);
    TEST_ASSERT_EQ(iv.hi.num, 5);

    /* 分数端点 */
    iv = lv_alg_interval_create(1, 2, 3, 4, &err);
    TEST_ASSERT_EQ(err, lv_alg_interval_OK);
    TEST_ASSERT_EQ(iv.lo.num, 1);
    TEST_ASSERT_EQ(iv.lo.den, 2);
    TEST_ASSERT_EQ(iv.hi.num, 3);
    TEST_ASSERT_EQ(iv.hi.den, 4);

    /* 零分母 → ERR_INVALID（修复点：原实现静默忽略 r_err） */
    iv = lv_alg_interval_create(1, 0, 3, 1, &err);
    TEST_ASSERT_EQ(err, lv_alg_interval_ERR_INVALID);
    iv = lv_alg_interval_create(1, 1, 3, 0, &err);
    TEST_ASSERT_EQ(err, lv_alg_interval_ERR_INVALID);

    /* point：点区间 */
    AlgRational r = lv_alg_rational_create(7, 4, NULL);
    iv = lv_alg_interval_point(&r);
    TEST_ASSERT(lv_alg_interval_is_point(&iv), "点区间");
    TEST_ASSERT_EQ(iv.lo.num, 7);
    TEST_ASSERT_EQ(iv.hi.num, 7);

    /* from_quadratic：完全平方 d → 精确点区间 */
    AlgQuadratic q = lv_alg_quadratic_create(1, 1, 1, 1, 4, NULL); /* 1+1√4 = 3 */
    iv = lv_alg_interval_from_quadratic(&q, &err);
    TEST_ASSERT_EQ(err, lv_alg_interval_OK);
    TEST_ASSERT(lv_alg_interval_is_point(&iv), "完全平方得点区间");
    TEST_ASSERT_EQ(iv.lo.num, 3);

    /* from_quadratic：√2 → 包围区间且包含真实值 */
    q = lv_alg_quadratic_create(0, 1, 1, 1, 2, NULL);
    iv = lv_alg_interval_from_quadratic(&q, &err);
    TEST_ASSERT_EQ(err, lv_alg_interval_OK);
    TEST_ASSERT(!lv_alg_interval_is_empty(&iv), "√2 区间非空");
    /* 包围区间应包含 14142/10000 */
    AlgRational approx = lv_alg_rational_create(14142, 10000, NULL);
    TEST_ASSERT(lv_alg_interval_contains_rational(&iv, &approx), "包围区间包含 1.4142");

    /* from_quadratic NULL → ERR_NULL */
    iv = lv_alg_interval_from_quadratic(NULL, &err);
    TEST_ASSERT_EQ(err, lv_alg_interval_ERR_NULL);

    /* quadratic_to_interval 跨层等价 */
    iv = lv_alg_quadratic_to_interval(&q, &err);
    TEST_ASSERT_EQ(err, lv_alg_interval_OK);
    TEST_ASSERT(!lv_alg_interval_is_empty(&iv), "quadratic_to_interval 非空");

    printf("  test_interval_create_api: PASSED\n");
}

/* ============== 测试：四则 ============== */

static void test_interval_arith_api(void) {
    AlgIntervalError err = lv_alg_interval_OK;
    AlgInterval a = lv_alg_interval_create(1, 1, 2, 1, NULL);   /* [1,2] */
    AlgInterval b = lv_alg_interval_create(3, 1, 4, 1, NULL);   /* [3,4] */

    /* add：[1,2]+[3,4] = [4,6] */
    AlgInterval s = lv_alg_interval_add(&a, &b, &err);
    TEST_ASSERT_EQ(err, lv_alg_interval_OK);
    TEST_ASSERT_EQ(s.lo.num, 4);
    TEST_ASSERT_EQ(s.hi.num, 6);

    /* sub：[1,2]-[3,4] = [1-4, 2-3] = [-3,-1] */
    s = lv_alg_interval_sub(&a, &b, &err);
    TEST_ASSERT_EQ(s.lo.num, -3);
    TEST_ASSERT_EQ(s.hi.num, -1);

    /* mul：[1,2]*[3,4] = [3,8] */
    s = lv_alg_interval_mul(&a, &b, &err);
    TEST_ASSERT_EQ(s.lo.num, 3);
    TEST_ASSERT_EQ(s.hi.num, 8);

    /* mul 跨零：[-2,-1]*[3,4] = 端点 -6,-8,-3,-4 → [-8,-3] */
    AlgInterval neg_a = lv_alg_interval_create(-2, 1, -1, 1, NULL);
    s = lv_alg_interval_mul(&neg_a, &b, &err);
    TEST_ASSERT_EQ(s.lo.num, -8);
    TEST_ASSERT_EQ(s.hi.num, -3);

    /* div：[1,2]/[3,4] = [1/4, 2/3] */
    s = lv_alg_interval_div(&a, &b, &err);
    TEST_ASSERT_EQ(err, lv_alg_interval_OK);
    TEST_ASSERT_EQ(s.lo.num, 1);
    TEST_ASSERT_EQ(s.lo.den, 4);
    TEST_ASSERT_EQ(s.hi.num, 2);
    TEST_ASSERT_EQ(s.hi.den, 3);

    /* div 除数含零 → ERR_DIV_BY_ZERO */
    AlgInterval zero_x = lv_alg_interval_create(-1, 1, 1, 1, NULL);
    s = lv_alg_interval_div(&a, &zero_x, &err);
    TEST_ASSERT_EQ(err, lv_alg_interval_ERR_DIV_BY_ZERO);

    /* neg：-[1,2] = [-2,-1] */
    s = lv_alg_interval_neg(&a);
    TEST_ASSERT_EQ(s.lo.num, -2);
    TEST_ASSERT_EQ(s.hi.num, -1);

    /* NULL → ERR_NULL */
    s = lv_alg_interval_add(NULL, &b, &err);
    TEST_ASSERT_EQ(err, lv_alg_interval_ERR_NULL);

    /* 溢出 → ERR_OVERFLOW（修复点：原实现静默丢弃 rational 溢出） */
    AlgInterval big = lv_alg_interval_create(INT64_MAX, 1, INT64_MAX, 1, NULL);
    s = lv_alg_interval_add(&big, &big, &err);
    TEST_ASSERT_EQ(err, lv_alg_interval_ERR_OVERFLOW);

    printf("  test_interval_arith_api: PASSED\n");
}

/* ============== 测试：集合运算 ============== */

static void test_interval_set_api(void) {
    AlgIntervalError err = lv_alg_interval_OK;
    AlgInterval a = lv_alg_interval_create(1, 1, 3, 1, NULL);   /* [1,3] */
    AlgInterval b = lv_alg_interval_create(2, 1, 4, 1, NULL);   /* [2,4] */

    /* intersect：[1,3]∩[2,4] = [2,3] */
    AlgInterval s = lv_alg_interval_intersect(&a, &b, &err);
    TEST_ASSERT_EQ(err, lv_alg_interval_OK);
    TEST_ASSERT_EQ(s.lo.num, 2);
    TEST_ASSERT_EQ(s.hi.num, 3);

    /* 空交集 → ERR_EMPTY */
    AlgInterval c = lv_alg_interval_create(5, 1, 6, 1, NULL);
    s = lv_alg_interval_intersect(&a, &c, &err);
    TEST_ASSERT_EQ(err, lv_alg_interval_ERR_EMPTY);

    /* hull：[1,3]∪[5,6] = [1,6] */
    s = lv_alg_interval_hull(&a, &c, &err);
    TEST_ASSERT_EQ(err, lv_alg_interval_OK);
    TEST_ASSERT_EQ(s.lo.num, 1);
    TEST_ASSERT_EQ(s.hi.num, 6);

    /* contains：区间包含区间 */
    AlgInterval d = lv_alg_interval_create(0, 1, 5, 1, NULL);   /* [0,5] */
    TEST_ASSERT(lv_alg_interval_contains(&d, &a), "大区间包含子区间");
    TEST_ASSERT(!lv_alg_interval_contains(&a, &d), "子区间不含大区间");

    /* contains_rational */
    AlgRational r = lv_alg_rational_create(2, 1, NULL);
    TEST_ASSERT(lv_alg_interval_contains_rational(&a, &r), "包含 2");
    AlgRational r4 = lv_alg_rational_create(4, 1, NULL);
    TEST_ASSERT(!lv_alg_interval_contains_rational(&a, &r4), "不包含 4");

    /* is_empty / is_point */
    AlgInterval empty;
    empty.lo = lv_alg_rational_create(5, 1, NULL);
    empty.hi = lv_alg_rational_create(2, 1, NULL);
    TEST_ASSERT(lv_alg_interval_is_empty(&empty), "lo>hi 为空区间");
    TEST_ASSERT(lv_alg_interval_is_point(&a) == false, "[1,3] 非点区间");
    AlgInterval pt = lv_alg_interval_create(7, 1, 7, 1, NULL);
    TEST_ASSERT(lv_alg_interval_is_point(&pt), "[7,7] 为点区间");

    printf("  test_interval_set_api: PASSED\n");
}

/* ============== 测试：度量与字符串 ============== */

static void test_interval_metric_api(void) {
    AlgIntervalError err = lv_alg_interval_OK;
    AlgInterval a = lv_alg_interval_create(1, 1, 4, 1, NULL);   /* [1,4] */

    /* width：4-1 = 3 */
    AlgRational w = lv_alg_interval_width(&a, &err);
    TEST_ASSERT_EQ(err, lv_alg_interval_OK);
    TEST_ASSERT_EQ(w.num, 3);

    /* midpoint：(1+4)/2 = 5/2 */
    AlgRational m = lv_alg_interval_midpoint(&a, &err);
    TEST_ASSERT_EQ(err, lv_alg_interval_OK);
    TEST_ASSERT_EQ(m.num, 5);
    TEST_ASSERT_EQ(m.den, 2);

    /* bisect：[1,4] → [1,5/2] 与 [5/2,4] */
    AlgInterval lo, hi;
    lv_alg_interval_bisect(&a, &lo, &hi, &err);
    TEST_ASSERT_EQ(err, lv_alg_interval_OK);
    TEST_ASSERT_EQ(lo.lo.num, 1);
    TEST_ASSERT_EQ(lo.hi.num, 5);
    TEST_ASSERT_EQ(lo.hi.den, 2);
    TEST_ASSERT_EQ(hi.lo.num, 5);
    TEST_ASSERT_EQ(hi.lo.den, 2);
    TEST_ASSERT_EQ(hi.hi.num, 4);

    /* bisect NULL x → ERR_NULL */
    lv_alg_interval_bisect(NULL, &lo, &hi, &err);
    TEST_ASSERT_EQ(err, lv_alg_interval_ERR_NULL);

    /* width NULL → ERR_NULL */
    w = lv_alg_interval_width(NULL, &err);
    TEST_ASSERT_EQ(err, lv_alg_interval_ERR_NULL);

    /* width 溢出 → ERR_OVERFLOW（修复点：原实现静默丢弃 sub 溢出） */
    AlgInterval big = lv_alg_interval_create(INT64_MIN, 1, INT64_MAX, 1, NULL);
    w = lv_alg_interval_width(&big, &err);
    TEST_ASSERT_EQ(err, lv_alg_interval_ERR_OVERFLOW);

    /* to_string："[1, 4]" 与 "[1/2, 3/4]" */
    char buf[64];
    int n = lv_alg_interval_to_string(&a, buf, sizeof(buf));
    TEST_ASSERT(n > 0, "to_string 正长度");
    TEST_ASSERT(strcmp(buf, "[1, 4]") == 0, "整数端点格式");
    AlgInterval frac = lv_alg_interval_create(1, 2, 3, 4, NULL);
    n = lv_alg_interval_to_string(&frac, buf, sizeof(buf));
    TEST_ASSERT(strcmp(buf, "[1/2, 3/4]") == 0, "分数端点格式");

    /* error_string */
    const char *es = lv_alg_interval_error_string(lv_alg_interval_OK);
    TEST_ASSERT_NOT_NULL(es);
    TEST_ASSERT(strlen(es) > 0, "错误码字符串非空");
    es = lv_alg_interval_error_string(lv_alg_interval_ERR_EMPTY);
    TEST_ASSERT_NOT_NULL(es);
    es = lv_alg_interval_error_string(lv_alg_interval_ERR_DIV_BY_ZERO);
    TEST_ASSERT_NOT_NULL(es);

    printf("  test_interval_metric_api: PASSED\n");
}

/* ============== 测试入口 ============== */

TEST_MAIN_BEGIN("Lv-00 Algebraic Interval Ext Test Suite")
    printf("=== Lv-00 Algebraic Interval Ext Test Suite (batch C-㊺续13) ===\n\n");
    lv_init();

    TEST_MAIN_RUN(test_interval_create_api);
    TEST_MAIN_RUN(test_interval_arith_api);
    TEST_MAIN_RUN(test_interval_set_api);
    TEST_MAIN_RUN(test_interval_metric_api);

    lv_cleanup();
TEST_MAIN_END()
