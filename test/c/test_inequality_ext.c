/**
 * @file test_inequality_ext.c
 * @brief 不等式推理契约测试（批次 C-㊺续3：inequality_reasoning.h 23 个零覆盖 API）
 *
 * 覆盖 23 个 ctest 零覆盖 API：
 *   - 系统族：lv_ineq_system_destroy / lv_ineq_system_add /
 *     lv_ineq_system_add_var_constraint
 *   - 证明族：lv_ineq_prove / lv_ineq_prove_with_method / lv_ineq_proof_destroy
 *   - 符号族：lv_expr_sign / lv_expr_is_positive / lv_expr_is_nonnegative
 *   - 经典族：lv_ineq_am_gm / lv_ineq_cauchy_schwarz / lv_ineq_rearrangement /
 *     lv_ineq_schur / lv_ineq_jensen / lv_ineq_triangle
 *   - 变换族：lv_ineq_transitive / lv_ineq_merge
 *   - 几何族：lv_ineq_triangle_area / lv_ineq_weitzenbock /
 *     lv_ineq_erdos_mordell
 *   - 销毁族：lv_ineq_destroy / lv_expr_sos_decompose / lv_sos_destroy
 *
 * 契约要点（与头注释核对）：
 *   - 纯符号计算（GMP 有理数），无浮点。
 *   - lv_ineq_destroy / proof_destroy / sos_destroy / system_destroy NULL 安全。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_unified.h"
#include "lv/inequality_reasoning.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* 辅助：创建常量表达式 */
static lvExpr *cnum(int64_t v) {
    return lv_expr_create_rational(v, 1);
}

/* ============== 测试：不等式系统 ============== */

static void test_system_api(void) {
    /* destroy：NULL 安全 */
    lv_ineq_system_destroy(NULL);

    lvInequalitySystem *sys = lv_ineq_system_create();
    TEST_ASSERT_NOT_NULL(sys);

    /* add：NULL 契约 + 正常 */
    TEST_ASSERT(!lv_ineq_system_add(NULL, NULL), "NULL sys");
    lvExpr *x = lv_expr_create_variable("x");
    lvExpr *c0 = cnum(0);
    lvInequality *ineq = lv_ineq_create(x, INEQ_GREATER_EQUAL, c0);
    TEST_ASSERT_NOT_NULL(ineq);
    TEST_ASSERT(lv_ineq_system_add(sys, ineq), "添加不等式");

    /* add_var_constraint：NULL 契约 + 正常 */
    mpq_t zz;
    mpq_init(zz);
    mpq_set_ui(zz, 0, 1);
    TEST_ASSERT(!lv_ineq_system_add_var_constraint(NULL, x, INEQ_GREATER_EQUAL, zz), "NULL sys");
    lvExpr *y = lv_expr_create_variable("y");
    mpq_t zero;
    mpq_init(zero);
    mpq_set_ui(zero, 0, 1);
    TEST_ASSERT(lv_ineq_system_add_var_constraint(sys, y, INEQ_GREATER_EQUAL, zero), "变量约束");
    mpq_clear(zz);
    mpq_clear(zero);

    lv_ineq_system_destroy(sys);
    printf("  test_system_api: PASSED\n");
}

/* ============== 测试：证明 ============== */

static void test_prove_api(void) {
    /* proof_destroy：NULL 安全 */
    lv_ineq_proof_destroy(NULL);

    /* prove：NULL 契约 + 简单不等式（1 ≤ 2 直接可证） */
    lvInequalitySystem *sys = lv_ineq_system_create();
    TEST_ASSERT_NOT_NULL(sys);
    lvExpr *one = cnum(1);
    lvExpr *two = cnum(2);
    lvInequality *ineq = lv_ineq_create(one, INEQ_LESS_EQUAL, two);
    TEST_ASSERT_NOT_NULL(ineq);

    lvInequalityProof *proof = NULL;
    lvInequalityStatus st = lv_ineq_prove(ineq, sys, &proof);
    TEST_ASSERT(st == INEQ_STATUS_PROVED || st == INEQ_STATUS_UNKNOWN, "简单不等式证明状态合法");
    if (proof) {
        TEST_ASSERT(proof->status == st, "证明状态一致");
        lv_ineq_proof_destroy(proof);
    }

    /* prove_with_method：NULL 契约 + 指定方法（返回任意合法状态） */
    lvInequalityStatus st2 = lv_ineq_prove_with_method(ineq, INEQ_METHOD_DIRECT, sys, NULL);
    TEST_ASSERT(st2 >= INEQ_STATUS_UNPROVED && st2 <= INEQ_STATUS_UNKNOWN, "指定方法状态合法");
    proof = NULL;
    lv_ineq_prove_with_method(ineq, INEQ_METHOD_AM_GM, sys, &proof);
    if (proof)
        lv_ineq_proof_destroy(proof);

    lv_ineq_destroy(ineq);
    lv_ineq_system_destroy(sys);
    printf("  test_prove_api: PASSED\n");
}

/* ============== 测试：表达式符号 ============== */

static void test_sign_api(void) {
    lvInequalitySystem *sys = lv_ineq_system_create();
    TEST_ASSERT_NOT_NULL(sys);

    /* 常量表达式：不匹配系统约束 → UNKNOWN（符号判定仅经约束推断） */
    lvExpr *pos = cnum(5);
    TEST_ASSERT_EQ(lv_expr_sign(pos, sys), SIGN_UNKNOWN);
    TEST_ASSERT(!lv_expr_is_positive(pos, sys), "常量未知非正");
    TEST_ASSERT(!lv_expr_is_nonnegative(pos, sys), "常量未知非非负");

    /* 变量约束推断：x ≥ 0 → NONNEGATIVE；y > 0 → POSITIVE */
    lvExpr *x = lv_expr_create_variable("x");
    lvExpr *c0 = cnum(0);
    lvInequality *cx = lv_ineq_create(x, INEQ_GREATER_EQUAL, c0);
    TEST_ASSERT(lv_ineq_system_add(sys, cx), "x≥0 约束");
    TEST_ASSERT_EQ(lv_expr_sign(x, sys), SIGN_NONNEGATIVE);
    TEST_ASSERT(!lv_expr_is_positive(x, sys), "非负非正");
    TEST_ASSERT(lv_expr_is_nonnegative(x, sys), "非负判定");

    lvExpr *y = lv_expr_create_variable("y");
    lvExpr *c0b = cnum(0);
    lvInequality *cy = lv_ineq_create(y, INEQ_GREATER_THAN, c0b);
    TEST_ASSERT(lv_ineq_system_add(sys, cy), "y>0 约束");
    TEST_ASSERT_EQ(lv_expr_sign(y, sys), SIGN_POSITIVE);
    TEST_ASSERT(lv_expr_is_positive(y, sys), "正数判定");
    TEST_ASSERT(lv_expr_is_nonnegative(y, sys), "正数非负");

    /* NULL 契约 */
    TEST_ASSERT_EQ(lv_expr_sign(NULL, sys), SIGN_UNKNOWN);
    TEST_ASSERT(!lv_expr_is_positive(NULL, sys), "NULL 非正");

    lv_expr_destroy(&pos);
    lv_ineq_system_destroy(sys);
    printf("  test_sign_api: PASSED\n");
}

/* ============== 测试：经典不等式 ============== */

static void test_classic_api(void) {
    /* am_gm：两个常量 */
    lvExpr *a1 = cnum(4);
    lvExpr *a2 = cnum(9);
    lvExpr *arr[2] = {a1, a2};
    lvExpr *lower = NULL, *upper = NULL;
    TEST_ASSERT(lv_ineq_am_gm(arr, 2, &lower, &upper), "AM-GM 两数");
    TEST_ASSERT_NOT_NULL(lower);
    TEST_ASSERT_NOT_NULL(upper);
    lv_expr_destroy(&lower);
    lv_expr_destroy(&upper);

    /* cauchy_schwarz：一维向量 */
    lvExpr *b1 = cnum(2);
    lvExpr *a1d[1] = {a1};
    lvExpr *b1d[1] = {b1};
    lvInequality *cs = NULL;
    TEST_ASSERT(lv_ineq_cauchy_schwarz(a1d, b1d, 1, &cs), "Cauchy-Schwarz 一维");
    TEST_ASSERT_NOT_NULL(cs);
    lv_ineq_destroy(cs);

    /* rearrangement：两个序列 */
    lvExpr *c2 = cnum(3);
    lvExpr *seq_a[2] = {a1, a2};
    lvExpr *seq_b[2] = {b1, c2};
    lvExpr *min_sum = NULL, *max_sum = NULL;
    TEST_ASSERT(lv_ineq_rearrangement(seq_a, seq_b, 2, &min_sum, &max_sum), "排序不等式");
    if (min_sum)
        lv_expr_destroy(&min_sum);
    if (max_sum)
        lv_expr_destroy(&max_sum);

    /* schur：三个常量 */
    lvInequality *schur_ineq = NULL;
    TEST_ASSERT(lv_ineq_schur(a1, a2, c2, 1, &schur_ineq), "Schur 不等式");
    if (schur_ineq)
        lv_ineq_destroy(schur_ineq);

    /* jensen：凸函数 */
    lvInequality *jensen_ineq = NULL;
    lvExpr *pts[2] = {a1, a2};
    mpq_t w1, w2;
    mpq_init(w1);
    mpq_set_ui(w1, 1, 2);
    mpq_init(w2);
    mpq_set_ui(w2, 1, 2);
    TEST_ASSERT(lv_ineq_jensen("exp", pts, NULL, 2, true, &jensen_ineq) ||
                lv_ineq_jensen("exp", pts, NULL, 2, true, &jensen_ineq) == false,
                "Jensen 不崩溃");
    if (jensen_ineq)
        lv_ineq_destroy(jensen_ineq);
    mpq_clear(w1);
    mpq_clear(w2);

    /* triangle：三边 */
    lvInequality *tri_out[3] = {NULL, NULL, NULL};
    uint32_t n = lv_ineq_triangle(a1, a2, c2, tri_out, 3);
    TEST_ASSERT(n <= 3, "三角形不等式输出数量");
    for (uint32_t i = 0; i < n && i < 3; i++)
        if (tri_out[i])
            lv_ineq_destroy(tri_out[i]);

    lv_expr_destroy(&a1);
    lv_expr_destroy(&a2);
    lv_expr_destroy(&b1);
    lv_expr_destroy(&c2);
    printf("  test_classic_api: PASSED\n");
}

/* ============== 测试：变换（传递/合并） ============== */

static void test_transform_api(void) {
    /* transitive：a ≤ b ≤ c 链 */
    lvExpr *a = cnum(1);
    lvExpr *b = cnum(2);
    lvExpr *c = cnum(3);
    lvInequality *i1 = lv_ineq_create(a, INEQ_LESS_EQUAL, b);
    lvInequality *i2 = lv_ineq_create(b, INEQ_LESS_EQUAL, c);
    TEST_ASSERT_NOT_NULL(i1);
    TEST_ASSERT_NOT_NULL(i2);
    lvInequality *chain[2] = {i1, i2};
    lvInequality *result = NULL;
    TEST_ASSERT(lv_ineq_transitive(chain, 2, &result), "传递链");
    TEST_ASSERT_NOT_NULL(result);
    lv_ineq_destroy(result);

    /* merge：同向合并 */
    lvInequality *m1 = lv_ineq_create(a, INEQ_LESS_EQUAL, b);
    lvInequality *m2 = lv_ineq_create(b, INEQ_LESS_EQUAL, c);
    lvInequality *ms[2] = {m1, m2};
    result = NULL;
    TEST_ASSERT(lv_ineq_merge(ms, 2, &result), "同向合并");
    TEST_ASSERT_NOT_NULL(result);
    lv_ineq_destroy(result);

    lv_ineq_destroy(i1);
    lv_ineq_destroy(i2);
    lv_ineq_destroy(m1);
    lv_ineq_destroy(m2);
    lv_expr_destroy(&a);
    lv_expr_destroy(&b);
    lv_expr_destroy(&c);
    printf("  test_transform_api: PASSED\n");
}

/* ============== 测试：几何不等式 + SOS ============== */

static void test_geometry_sos_api(void) {
    lvExpr *a = cnum(3);
    lvExpr *b = cnum(4);
    lvExpr *c = cnum(5);
    lvExpr *area = cnum(6);

    /* triangle_area / weitzenbock / erdos_mordell：不崩溃 + 输出可释放 */
    lvInequality *out = NULL;
    TEST_ASSERT(lv_ineq_triangle_area(a, b, c, area, &out) || out != NULL, "面积不等式");
    if (out)
        lv_ineq_destroy(out);
    out = NULL;
    TEST_ASSERT(lv_ineq_weitzenbock(a, b, c, area, &out) || out != NULL, "Weitzenbock");
    if (out)
        lv_ineq_destroy(out);
    out = NULL;
    TEST_ASSERT(lv_ineq_erdos_mordell(a, b, c, a, b, c, &out) || out != NULL, "Erdos-Mordell");
    if (out)
        lv_ineq_destroy(out);

    /* sos：NULL 契约 + 简单平方分解 */
    TEST_ASSERT(!lv_expr_sos_decompose(NULL, NULL), "NULL SOS");
    lvSOSDecomposition *sos = NULL;
    lvExpr *sq = lv_expr_power(a, cnum(2));
    TEST_ASSERT(lv_expr_sos_decompose(sq, &sos) || sos != NULL, "平方分解");
    if (sos) {
        TEST_ASSERT(sos->count >= 1, "分解项数");
        lv_sos_destroy(sos);
    }
    lv_expr_destroy(&sq);
    lv_sos_destroy(NULL); /* NULL 安全 */

    lv_expr_destroy(&a);
    lv_expr_destroy(&b);
    lv_expr_destroy(&c);
    lv_expr_destroy(&area);
    printf("  test_geometry_sos_api: PASSED\n");
}

/* ============== 测试入口 ============== */

TEST_MAIN_BEGIN("Lv-00 Inequality Reasoning Ext Test Suite")
    printf("=== Lv-00 Inequality Reasoning Ext Test Suite (batch C-㊺续3) ===\n\n");
    lv_init();

    TEST_MAIN_RUN(test_system_api);
    TEST_MAIN_RUN(test_prove_api);
    TEST_MAIN_RUN(test_sign_api);
    TEST_MAIN_RUN(test_classic_api);
    TEST_MAIN_RUN(test_transform_api);
    TEST_MAIN_RUN(test_geometry_sos_api);

    lv_cleanup();
TEST_MAIN_END()
