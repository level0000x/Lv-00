/**
 * @file test_expr_canon_ext.c
 * @brief 规范多项式契约测试（批次 C-㊺续32：expr_canon.h 零覆盖 API）
 *
 * 覆盖零覆盖 API（9 个）：
 *   lv_expr_canon / lv_expr_canonical_degree / equal / from_string
 *   / is_zero / mul / neg / scale / term_count
 *
 * 契约要点（与 expr_canon.c 核对）：
 *   - 构造：create + add_term + canonicalize（合并同类项、排序）。
 *   - mul/scale/neg：返回新多项式；零项合并。
 *   - equal：NULL/NULL true；结构逐项比较。
 *   - is_zero：NULL 或 term_count==0 均 true。
 *   - degree：NULL/空 -1；首项总次数。
 *   - term_count：NULL 0。
 *   - from_string：完整解析（头注释"桩实现"已过时）；支持 "2*x + 3*x"。
 *   - lv_expr_canon：解析规范化；失败回退原串副本。
 *
 * @author Lv-00 Project
 */

#include <stdio.h>
#include <string.h>

#include "lv/expr_canon.h"
#include "lv/lv_utils.h"
#include "lv/rational.h"

#include "test_unified.h"

int g_pass_count = 0;
int g_fail_count = 0;

/** @brief 构造规范多项式 2*x（1 变量 x） */
static lvExprCanonical *make_2x(void) {
    const char *names[] = {"x"};
    lvExprCanonical *e = lv_expr_canonical_create(1, names);
    if (!e)
        return NULL;
    lvRational *c = lv_rational_create_from_si(2, 1);
    int exp[1] = {1};
    lv_expr_canonical_add_term(e, c, exp);
    lv_rational_destroy(&c);
    lv_expr_canonicalize(e);
    return e;
}

/* ============== 测试：create/degree/term_count/is_zero ============== */

static void test_basic_queries(void) {
    const char *names[] = {"x"};
    lvExprCanonical *e = lv_expr_canonical_create(1, names);
    TEST_ASSERT_NOT_NULL(e);

    /* 空多项式 */
    TEST_ASSERT_EQ(lv_expr_canonical_term_count(e), 0);
    TEST_ASSERT(lv_expr_canonical_is_zero(e), "empty is zero");
    TEST_ASSERT_EQ(lv_expr_canonical_degree(e), -1);

    /* 加 2*x 与 3*x：合并为 5*x，degree 1 */
    lvRational *c2 = lv_rational_create_from_si(2, 1);
    lvRational *c3 = lv_rational_create_from_si(3, 1);
    int exp[1] = {1};
    TEST_ASSERT(lv_expr_canonical_add_term(e, c2, exp), "add 2x");
    TEST_ASSERT(lv_expr_canonical_add_term(e, c3, exp), "add 3x");
    lv_rational_destroy(&c2);
    lv_rational_destroy(&c3);
    TEST_ASSERT(lv_expr_canonicalize(e), "canonicalize");
    TEST_ASSERT_EQ(lv_expr_canonical_term_count(e), 1);
    TEST_ASSERT_EQ(lv_expr_canonical_degree(e), 1);
    TEST_ASSERT(!lv_expr_canonical_is_zero(e), "5x not zero");

    /* NULL 契约 */
    TEST_ASSERT_EQ(lv_expr_canonical_term_count(NULL), 0);
    TEST_ASSERT(lv_expr_canonical_is_zero(NULL), "NULL is zero");
    TEST_ASSERT_EQ(lv_expr_canonical_degree(NULL), -1);

    lv_expr_canonical_destroy(&e);
    TEST_ASSERT_NULL(e);
}

/* ============== 测试：mul / scale / neg ============== */

static void test_arith(void) {
    lvExprCanonical *a = make_2x();      /* 2*x */
    lvExprCanonical *b = make_2x();      /* 2*x */

    /* mul：2x * 2x = 4x^2 */
    lvExprCanonical *m = lv_expr_canonical_mul(a, b);
    TEST_ASSERT_NOT_NULL(m);
    TEST_ASSERT_EQ(lv_expr_canonical_term_count(m), 1);
    TEST_ASSERT_EQ(lv_expr_canonical_degree(m), 2);

    /* scale：4x^2 * 1/2 = 2x^2 */
    lvRational *half = lv_rational_create_from_si(1, 2);
    lvExprCanonical *s = lv_expr_canonical_scale(m, half);
    lv_rational_destroy(&half);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQ(lv_expr_canonical_degree(s), 2);

    /* neg：-2x */
    lvExprCanonical *n = lv_expr_canonical_neg(a);
    TEST_ASSERT_NOT_NULL(n);
    TEST_ASSERT(!lv_expr_canonical_equal(n, a), "neg differs");
    /* neg + a 结构：-2x 与 2x 不等；double neg 回归 2x */
    lvExprCanonical *nn = lv_expr_canonical_neg(n);
    TEST_ASSERT(lv_expr_canonical_equal(nn, a), "double neg identity");

    /* NULL 契约 */
    TEST_ASSERT_NULL(lv_expr_canonical_mul(NULL, b));
    TEST_ASSERT_NULL(lv_expr_canonical_scale(a, NULL));
    TEST_ASSERT_NULL(lv_expr_canonical_neg(NULL));

    lv_expr_canonical_destroy(&nn);
    lv_expr_canonical_destroy(&n);
    lv_expr_canonical_destroy(&s);
    lv_expr_canonical_destroy(&m);
    lv_expr_canonical_destroy(&b);
    lv_expr_canonical_destroy(&a);
}

/* ============== 测试：equal ============== */

static void test_equal(void) {
    lvExprCanonical *a = make_2x();
    lvExprCanonical *a2 = make_2x();
    lvExprCanonical *b = lv_expr_canonical_clone(a);
    TEST_ASSERT_NOT_NULL(b);

    /* 相同结构 */
    TEST_ASSERT(lv_expr_canonical_equal(a, a2), "same structure");
    TEST_ASSERT(lv_expr_canonical_equal(a, b), "clone equal");

    /* 不同：3x vs 2x */
    const char *names[] = {"x"};
    lvExprCanonical *c = lv_expr_canonical_create(1, names);
    lvRational *c3 = lv_rational_create_from_si(3, 1);
    int exp[1] = {1};
    lv_expr_canonical_add_term(c, c3, exp);
    lv_rational_destroy(&c3);
    lv_expr_canonicalize(c);
    TEST_ASSERT(!lv_expr_canonical_equal(a, c), "different coeff");

    /* NULL 契约 */
    TEST_ASSERT(lv_expr_canonical_equal(NULL, NULL), "NULL NULL equal");
    TEST_ASSERT(!lv_expr_canonical_equal(NULL, a), "NULL vs non-NULL");
    TEST_ASSERT(!lv_expr_canonical_equal(a, NULL), "non-NULL vs NULL");

    lv_expr_canonical_destroy(&c);
    lv_expr_canonical_destroy(&b);
    lv_expr_canonical_destroy(&a2);
    lv_expr_canonical_destroy(&a);
}

/* ============== 测试：from_string / lv_expr_canon ============== */

static void test_from_string(void) {
    const char *names[] = {"x"};

    /* "2*x + 3*x" -> 5*x（合并同类项） */
    lvExprCanonical *e = lv_expr_canonical_from_string("2*x + 3*x", names, 1);
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_EQ(lv_expr_canonical_term_count(e), 1);
    TEST_ASSERT_EQ(lv_expr_canonical_degree(e), 1);
    char *s = lv_expr_canonical_to_string(e);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT(strstr(s, "5") != NULL, "coeff 5");
    lv_free((void **) &s);
    lv_expr_canonical_destroy(&e);

    /* "x^2 + 2*x + 1"：3 项 */
    e = lv_expr_canonical_from_string("x^2 + 2*x + 1", names, 1);
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_EQ(lv_expr_canonical_term_count(e), 3);
    TEST_ASSERT_EQ(lv_expr_canonical_degree(e), 2);
    lv_expr_canonical_destroy(&e);

    /* NULL 契约 */
    TEST_ASSERT_NULL(lv_expr_canonical_from_string(NULL, names, 1));
}

static void test_expr_canon_compat(void) {
    /* "x^2 + 2*x + 1" 规范化字符串 */
    char *s = lv_expr_canon("x^2 + 2*x + 1");
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT(strstr(s, "x") != NULL, "contains variable");
    lv_free((void **) &s);

    /* NULL 契约 */
    TEST_ASSERT_NULL(lv_expr_canon(NULL));
}

/* ============== Main ============== */

TEST_MAIN_BEGIN("ExprCanonExt")

    printf("\n--- expr_canon (zero-coverage) ---\n");
    TEST_MAIN_RUN(test_basic_queries);
    TEST_MAIN_RUN(test_arith);
    TEST_MAIN_RUN(test_equal);
    TEST_MAIN_RUN(test_from_string);
    TEST_MAIN_RUN(test_expr_canon_compat);

TEST_MAIN_END()
