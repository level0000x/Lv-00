/**
 * @file test_expr_canonical_ext.c
 * @brief 符号表达式契约测试（批次 C-㊺续34：expr_canonical.h 零覆盖 API）
 *
 * 覆盖零覆盖 API（6 个）：
 *   lv_expr_create_rational_mpq / lv_expr_free（宏→lv_expr_destroy）
 *   lv_expr_get_integer / lv_expr_is_constant / lv_expr_product_n / lv_expr_sum_n
 *
 * 契约要点（与 expr_canonical.c 核对）：
 *   - create_rational_mpq：内部复制 mpq；NULL → NULL。
 *   - is_constant：仅 RATIONAL 类型为常量；NULL → false。
 *   - get_integer：RATIONAL 且分母 1 时输出 int64；否则 false。
 *   - sum_n/product_n：exprs NULL 或 count 0 → NULL；任一项 NULL → NULL；
 *     成功创建 composite 节点（count/operands）。
 *   - free：销毁并置 NULL。
 *
 * @author Lv-00 Project
 */

#include <stdio.h>

#include "lv/expr_canonical.h"

#include "test_unified.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============== 测试：构造与查询 ============== */

static void test_rational_queries(void) {
    /* create_rational_mpq：3/2 */
    mpq_t q;
    mpq_init(q);
    mpq_set_si(q, 3, 2);
    lvExpr *e = lv_expr_create_rational_mpq(q);
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT(lv_expr_is_constant(e), "rational is constant");
    TEST_ASSERT(!lv_expr_get_integer(e, NULL), "out NULL");
    int64_t v = 0;
    TEST_ASSERT(!lv_expr_get_integer(e, &v), "3/2 not integer");
    lv_expr_free(e);
    TEST_ASSERT_NULL(e);

    /* 整数 5/1：get_integer 成功 */
    mpq_set_si(q, 5, 1);
    e = lv_expr_create_rational_mpq(q);
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT(lv_expr_get_integer(e, &v), "5/1 is integer");
    TEST_ASSERT_EQ((long long) v, 5LL);

    /* create_rational_mpq(NULL) */
    TEST_ASSERT_NULL(lv_expr_create_rational_mpq(NULL));
    TEST_ASSERT(!lv_expr_is_constant(NULL), "NULL not constant");
    TEST_ASSERT(!lv_expr_get_integer(NULL, &v), "NULL expr");

    lv_expr_free(e);
    mpq_clear(q);
}

/* ============== 测试：变量非常量 ============== */

static void test_variable(void) {
    lvExpr *x = lv_expr_create_variable("x");
    TEST_ASSERT_NOT_NULL(x);
    TEST_ASSERT(!lv_expr_is_constant(x), "variable not constant");
    int64_t v = 0;
    TEST_ASSERT(!lv_expr_get_integer(x, &v), "variable not integer");
    lv_expr_free(x);
}

/* ============== 测试：N 元复合 ============== */

static void test_composite_n(void) {
    lvExpr *a = lv_expr_create_rational(1, 1);
    lvExpr *b = lv_expr_create_rational(2, 1);
    lvExpr *c = lv_expr_create_variable("x");
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(b);
    TEST_ASSERT_NOT_NULL(c);

    lvExpr *exprs[3] = {a, b, c};

    /* sum_n：3 操作数 */
    lvExpr *sum = lv_expr_sum_n(exprs, 3);
    TEST_ASSERT_NOT_NULL(sum);
    TEST_ASSERT_EQ((int) sum->type, (int) EXPR_TYPE_SUM);
    TEST_ASSERT_EQ((int) sum->data.composite.count, 3);
    TEST_ASSERT(sum->data.composite.operands[0] == a, "operand 0");

    /* product_n：2 操作数 */
    lvExpr *prod = lv_expr_product_n(exprs, 2);
    TEST_ASSERT_NOT_NULL(prod);
    TEST_ASSERT_EQ((int) prod->type, (int) EXPR_TYPE_PRODUCT);
    TEST_ASSERT_EQ((int) prod->data.composite.count, 2);

    /* NULL/0 契约 */
    TEST_ASSERT_NULL(lv_expr_sum_n(NULL, 3));
    TEST_ASSERT_NULL(lv_expr_sum_n(exprs, 0));
    TEST_ASSERT_NULL(lv_expr_product_n(NULL, 2));
    TEST_ASSERT_NULL(lv_expr_product_n(exprs, 0));
    lvExpr *bad[2] = {a, NULL};
    TEST_ASSERT_NULL(lv_expr_sum_n(bad, 2));

    lv_expr_free(sum);
    lv_expr_free(prod);
    lv_expr_free(a);
    lv_expr_free(b);
    lv_expr_free(c);
}

/* ============== Main ============== */

TEST_MAIN_BEGIN("ExprCanonicalExt")

    printf("\n--- expr_canonical (zero-coverage) ---\n");
    TEST_MAIN_RUN(test_rational_queries);
    TEST_MAIN_RUN(test_variable);
    TEST_MAIN_RUN(test_composite_n);

TEST_MAIN_END()
