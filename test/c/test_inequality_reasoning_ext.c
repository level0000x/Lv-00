/**
 * @file test_inequality_reasoning_ext.c
 * @brief 不等式推理契约测试（批次 C-㊺续32：inequality_reasoning.h 零覆盖 API）
 *
 * 覆盖零覆盖 API（7 个）：
 *   lv_ineq_add / copy / mul / negate
 *   lv_ineq_to_string / proof_to_string / proof_to_latex
 *
 * 契约要点（与 inequality_reasoning_transform/core/serialize.c 核对）：
 *   - copy：浅拷贝表达式指针（表达式所有权在调用者），label 深拷贝。
 *   - add：两边加表达式，类型与状态不变。
 *   - mul：expr_sign > 0 方向不变；< 0 类型方向反转（ineq_negate_type）；
 *     == 0 返回 NULL。
 *   - negate：类型方向反转（本批修正头注释与实现对齐：< -> >、<= -> >=）；
 *     status PROVED <-> DISPROVED 翻转。
 *   - to_string：返回 "label: left <TYPE> right"（lv_malloc，调用者 lv_free）；
 *     NULL 返回空串。
 *   - proof_to_string：返回 "Proof: STATUS, N steps"。
 *   - proof_to_latex：\begin{proof} 环境；NULL 返回空串。
 *
 * @author Lv-00 Project
 */

#include <stdio.h>
#include <string.h>

#include "lv/expr_canonical.h"
#include "lv/inequality_reasoning.h"
#include "lv/lv_utils.h"

#include "test_unified.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============== 测试：copy ============== */

static void test_copy(void) {
    lvExpr *x = lv_expr_create_variable("x");
    lvExpr *one = lv_expr_create_rational(1, 1);
    TEST_ASSERT_NOT_NULL(x);
    TEST_ASSERT_NOT_NULL(one);

    lvInequality *ineq = lv_ineq_create(x, INEQ_LESS_EQUAL, one);
    TEST_ASSERT_NOT_NULL(ineq);

    lvInequality *copy = lv_ineq_copy(ineq);
    TEST_ASSERT_NOT_NULL(copy);
    /* 浅拷贝：表达式指针共享，类型/状态相同 */
    TEST_ASSERT(copy->left == ineq->left, "left shared");
    TEST_ASSERT(copy->right == ineq->right, "right shared");
    TEST_ASSERT_EQ((int) copy->type, (int) ineq->type);
    TEST_ASSERT_EQ((int) copy->status, (int) ineq->status);

    /* NULL 契约 */
    TEST_ASSERT_NULL(lv_ineq_copy(NULL));

    lv_ineq_destroy(copy);
    lv_ineq_destroy(ineq);
    lv_expr_free(x);
    lv_expr_free(one);
}

/* ============== 测试：add ============== */

static void test_add(void) {
    lvExpr *x = lv_expr_create_variable("x");
    lvExpr *one = lv_expr_create_rational(1, 1);
    lvExpr *two = lv_expr_create_rational(2, 1);

    lvInequality *ineq = lv_ineq_create(x, INEQ_LESS_EQUAL, one);
    TEST_ASSERT_NOT_NULL(ineq);

    lvInequality *r = lv_ineq_add(ineq, two);
    TEST_ASSERT_NOT_NULL(r);
    TEST_ASSERT_EQ((int) r->type, (int) INEQ_LESS_EQUAL);
    TEST_ASSERT_EQ((int) r->status, (int) ineq->status);
    /* 新表达式：left/right 均新分配 */
    TEST_ASSERT(r->left != ineq->left, "left rebuilt");
    TEST_ASSERT(r->right != ineq->right, "right rebuilt");

    /* NULL 契约 */
    TEST_ASSERT_NULL(lv_ineq_add(NULL, two));
    TEST_ASSERT_NULL(lv_ineq_add(ineq, NULL));

    /* 清理：add 生成的 left/right 由调用者释放 */
    lv_expr_free(r->left);
    lv_expr_free(r->right);
    lv_ineq_destroy(r);
    lv_ineq_destroy(ineq);
    lv_expr_free(x);
    lv_expr_free(one);
    lv_expr_free(two);
}

/* ============== 测试：mul ============== */

static void test_mul(void) {
    lvExpr *x = lv_expr_create_variable("x");
    lvExpr *one = lv_expr_create_rational(1, 1);
    lvExpr *two = lv_expr_create_rational(2, 1);

    lvInequality *ineq = lv_ineq_create(x, INEQ_LESS_EQUAL, one);
    TEST_ASSERT_NOT_NULL(ineq);

    /* 正乘：方向不变 */
    lvInequality *r = lv_ineq_mul(ineq, two, 1);
    TEST_ASSERT_NOT_NULL(r);
    TEST_ASSERT_EQ((int) r->type, (int) INEQ_LESS_EQUAL);
    lv_expr_free(r->left);
    lv_expr_free(r->right);
    lv_ineq_destroy(r);

    /* 负乘：方向反转（<= -> >=） */
    r = lv_ineq_mul(ineq, two, -1);
    TEST_ASSERT_NOT_NULL(r);
    TEST_ASSERT_EQ((int) r->type, (int) INEQ_GREATER_EQUAL);
    lv_expr_free(r->left);
    lv_expr_free(r->right);
    lv_ineq_destroy(r);

    /* 零符号：NULL */
    TEST_ASSERT_NULL(lv_ineq_mul(ineq, two, 0));

    /* NULL 契约 */
    TEST_ASSERT_NULL(lv_ineq_mul(NULL, two, 1));
    TEST_ASSERT_NULL(lv_ineq_mul(ineq, NULL, 1));

    lv_ineq_destroy(ineq);
    lv_expr_free(x);
    lv_expr_free(one);
    lv_expr_free(two);
}

/* ============== 测试：negate ============== */

static void test_negate(void) {
    lvExpr *x = lv_expr_create_variable("x");
    lvExpr *one = lv_expr_create_rational(1, 1);

    lvInequality *ineq = lv_ineq_create(x, INEQ_LESS_EQUAL, one);
    TEST_ASSERT_NOT_NULL(ineq);
    ineq->status = INEQ_STATUS_PROVED;

    /* 类型方向反转：<= -> >= */
    lvInequality *r = lv_ineq_negate(ineq);
    TEST_ASSERT_NOT_NULL(r);
    TEST_ASSERT_EQ((int) r->type, (int) INEQ_GREATER_EQUAL);
    /* status：PROVED -> DISPROVED */
    TEST_ASSERT_EQ((int) r->status, (int) INEQ_STATUS_DISPROVED);

    /* NULL 契约 */
    TEST_ASSERT_NULL(lv_ineq_negate(NULL));

    lv_ineq_destroy(r);
    lv_ineq_destroy(ineq);
    lv_expr_free(x);
    lv_expr_free(one);
}

/* ============== 测试：序列化 ============== */

static void test_serialize(void) {
    lvExpr *x = lv_expr_create_variable("x");
    lvExpr *one = lv_expr_create_rational(1, 1);

    lvInequality *ineq = lv_ineq_create(x, INEQ_LESS_EQUAL, one);
    TEST_ASSERT_NOT_NULL(ineq);

    /* to_string：label: left <= right */
    char *s = lv_ineq_to_string(ineq);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT(strstr(s, "<=") != NULL, "type symbol present");
    lv_free((void **) &s);

    /* NULL -> 空串 */
    s = lv_ineq_to_string(NULL);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_STR_EQ(s, "");
    lv_free((void **) &s);

    /* proof_to_string / latex（手工构造证明，steps 可空） */
    lvInequalityProof proof;
    memset(&proof, 0, sizeof(proof));
    proof.status = INEQ_STATUS_PROVED;
    proof.step_count = 2;

    s = lv_ineq_proof_to_string(&proof);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT(strstr(s, "PROVED") != NULL, "status present");
    TEST_ASSERT(strstr(s, "2 steps") != NULL, "step count present");
    lv_free((void **) &s);

    s = lv_ineq_proof_to_latex(&proof);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT(strstr(s, "\\begin{proof}") != NULL, "latex begin");
    TEST_ASSERT(strstr(s, "\\end{proof}") != NULL, "latex end");
    lv_free((void **) &s);

    /* NULL -> 空串 */
    s = lv_ineq_proof_to_string(NULL);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_STR_EQ(s, "");
    lv_free((void **) &s);
    s = lv_ineq_proof_to_latex(NULL);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_STR_EQ(s, "");
    lv_free((void **) &s);

    lv_ineq_destroy(ineq);
    lv_expr_free(x);
    lv_expr_free(one);
}

/* ============== Main ============== */

TEST_MAIN_BEGIN("InequalityReasoningExt")

    printf("\n--- inequality_reasoning (zero-coverage) ---\n");
    TEST_MAIN_RUN(test_copy);
    TEST_MAIN_RUN(test_add);
    TEST_MAIN_RUN(test_mul);
    TEST_MAIN_RUN(test_negate);
    TEST_MAIN_RUN(test_serialize);

TEST_MAIN_END()
