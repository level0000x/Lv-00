/**
 * @file test_autodiff_ext.c
 * @brief 自动微分引擎契约测试（批次 C-㊺续36：autodiff.h 零覆盖 API）
 *
 * 覆盖零覆盖 API（14 个）：
 *   lv_ad_engine_create / destroy
 *   lv_ad_expr_add / cos / create_const / create_var / destroy / mul / pow / sin
 *   lv_ad_forward_diff / lv_ad_reverse_diff / lv_ad_eval / lv_ad_grad
 *
 * 契约要点（与 autodiff.h / autodiff 实现核对）：
 *   - create：engine->mode 保存模式。
 *   - expr：kind/child_count 正确（二元 2、一元 1）；destroy 递归 + NULL 安全。
 *   - eval：按 var_values 求值。
 *   - forward_diff：f(x)=x^2 at 3 → 9/6；sin at 0 → 0/1。
 *   - reverse_diff：f(x0,x1)=x0*x1 at {3,4} → 12，grad {4,3}。
 *   - grad：reverse 后按变量查询；未找到 0。
 *
 * 注意：复合节点的子项必须独立分配（destroy 递归释放双子项，
 * 同指针会 double-free）。
 *
 * @author Lv-00 Project
 */

#include <math.h>
#include <stdio.h>

#include "lv/autodiff.h"

#include "test_unified.h"

int g_pass_count = 0;
int g_fail_count = 0;

#define TOL 1e-12

/* ============== 测试：引擎生命周期 ============== */

static void test_engine_lifecycle(void) {
    lvADEngine *e = lv_ad_engine_create(AD_FORWARD);
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_EQ((int) e->mode, (int) AD_FORWARD);
    lv_ad_engine_destroy(e);

    e = lv_ad_engine_create(AD_REVERSE);
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_EQ((int) e->mode, (int) AD_REVERSE);
    lv_ad_engine_destroy(e);

    lv_ad_engine_destroy(NULL);
}

/* ============== 测试：表达式构造 ============== */

static void test_expr_construction(void) {
    lvADExpr *c = lv_ad_expr_create_const(3.0);
    TEST_ASSERT_NOT_NULL(c);
    TEST_ASSERT_EQ((int) c->kind, (int) AD_CONST);
    TEST_ASSERT_DOUBLE(c->value, 3.0, TOL);

    lvADExpr *v = lv_ad_expr_create_var(2);
    TEST_ASSERT_NOT_NULL(v);
    TEST_ASSERT_EQ((int) v->kind, (int) AD_VAR);
    TEST_ASSERT_EQ(v->var_index, 2);

    /* 二元（每个复合节点用独立子项，避免递归销毁共享子项 double-free） */
    lvADExpr *c1 = lv_ad_expr_create_const(3.0);
    lvADExpr *v1 = lv_ad_expr_create_var(2);
    lvADExpr *sum = lv_ad_expr_add(c1, v1);
    TEST_ASSERT_NOT_NULL(sum);
    TEST_ASSERT_EQ((int) sum->kind, (int) AD_ADD);
    TEST_ASSERT_EQ((int) sum->child_count, 2);

    lvADExpr *c2 = lv_ad_expr_create_const(3.0);
    lvADExpr *v2 = lv_ad_expr_create_var(2);
    lvADExpr *prod = lv_ad_expr_mul(c2, v2);
    TEST_ASSERT_EQ((int) prod->kind, (int) AD_MUL);
    TEST_ASSERT_EQ((int) prod->child_count, 2);

    /* 幂：c ^ v */
    lvADExpr *c3 = lv_ad_expr_create_const(3.0);
    lvADExpr *v3 = lv_ad_expr_create_var(2);
    lvADExpr *pw = lv_ad_expr_pow(c3, v3);
    TEST_ASSERT_NOT_NULL(pw);
    TEST_ASSERT_EQ((int) pw->kind, (int) AD_POW);
    TEST_ASSERT_EQ((int) pw->child_count, 2);

    /* 一元 */
    lvADExpr *v4 = lv_ad_expr_create_var(2);
    lvADExpr *s = lv_ad_expr_sin(v4);
    TEST_ASSERT_EQ((int) s->kind, (int) AD_SIN);
    TEST_ASSERT_EQ((int) s->child_count, 1);
    lvADExpr *v5 = lv_ad_expr_create_var(2);
    lvADExpr *co = lv_ad_expr_cos(v5);
    TEST_ASSERT_EQ((int) co->kind, (int) AD_COS);
    TEST_ASSERT_EQ((int) co->child_count, 1);

    /* 各自的树独立销毁 */
    lv_ad_expr_destroy(sum);
    lv_ad_expr_destroy(prod);
    lv_ad_expr_destroy(pw);
    lv_ad_expr_destroy(s);
    lv_ad_expr_destroy(co);
    lv_ad_expr_destroy(NULL);
}

/* ============== 测试：求值 ============== */

static void test_eval(void) {
    lvADExpr *x = lv_ad_expr_create_var(0);
    lvADExpr *c3 = lv_ad_expr_create_const(3.0);
    lvADExpr *sum = lv_ad_expr_add(x, c3);

    double vars[1] = {2.0};
    double result = 0.0;
    TEST_ASSERT(lv_ad_eval(sum, vars, 1, &result), "eval ok");
    TEST_ASSERT_DOUBLE(result, 5.0, TOL);

    lv_ad_expr_destroy(sum);
}

/* ============== 测试：前向微分 ============== */

static void test_forward_diff(void) {
    /* f(x) = x^2（两个独立 var 节点同 index 0） */
    lvADExpr *x1 = lv_ad_expr_create_var(0);
    lvADExpr *x2 = lv_ad_expr_create_var(0);
    lvADExpr *sq = lv_ad_expr_mul(x1, x2);

    double value = 0.0, deriv = 0.0;
    TEST_ASSERT(lv_ad_forward_diff(sq, 0, 3.0, &value, &deriv), "fwd ok");
    TEST_ASSERT_DOUBLE(value, 9.0, TOL);
    TEST_ASSERT_DOUBLE(deriv, 6.0, TOL);
    lv_ad_expr_destroy(sq);

    /* f(x) = sin(x) at 0：0 / 1 */
    lvADExpr *v = lv_ad_expr_create_var(0);
    lvADExpr *s = lv_ad_expr_sin(v);
    TEST_ASSERT(lv_ad_forward_diff(s, 0, 0.0, &value, &deriv), "sin fwd");
    TEST_ASSERT_DOUBLE(value, 0.0, TOL);
    TEST_ASSERT_DOUBLE(deriv, 1.0, 1e-9);
    lv_ad_expr_destroy(s);
}

/* ============== 测试：反向微分 ============== */

static void test_reverse_diff(void) {
    /* f(x0, x1) = x0 * x1 at {3, 4}：12，grad {4, 3} */
    lvADExpr *x0 = lv_ad_expr_create_var(0);
    lvADExpr *x1 = lv_ad_expr_create_var(1);
    lvADExpr *prod = lv_ad_expr_mul(x0, x1);

    double vars[2] = {3.0, 4.0};
    double value = 0.0;
    double grads[2] = {0.0, 0.0};
    TEST_ASSERT(lv_ad_reverse_diff(prod, vars, 2, &value, grads), "reverse ok");
    TEST_ASSERT_DOUBLE(value, 12.0, TOL);
    TEST_ASSERT_DOUBLE(grads[0], 4.0, TOL);
    TEST_ASSERT_DOUBLE(grads[1], 3.0, TOL);

    /* grad 查询 */
    TEST_ASSERT_DOUBLE(lv_ad_grad(prod, 0), 4.0, TOL);
    TEST_ASSERT_DOUBLE(lv_ad_grad(prod, 1), 3.0, TOL);
    TEST_ASSERT_DOUBLE(lv_ad_grad(prod, 99), 0.0, TOL);

    lv_ad_expr_destroy(prod);
}

/* ============== Main ============== */

TEST_MAIN_BEGIN("AutodiffExt")

    printf("\n--- autodiff (zero-coverage) ---\n");
    TEST_MAIN_RUN(test_engine_lifecycle);
    TEST_MAIN_RUN(test_expr_construction);
    TEST_MAIN_RUN(test_eval);
    TEST_MAIN_RUN(test_forward_diff);
    TEST_MAIN_RUN(test_reverse_diff);

TEST_MAIN_END()
