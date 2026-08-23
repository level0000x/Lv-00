/**
 * @file test_modal_operators.c
 * @brief 模态算子 —— 对偶转换真实实现回归测试（重构 C-㊺续18）
 *
 * v1.1.0 起 lvModalFormula 通过 lv_MODALOP_NEGATION 否定节点真实实现
 * 对偶转换：◇A ≡ ¬□¬A、□A ≡ ¬◇¬A（此前 UNSUPPORTED 恒 NULL）。
 * 本测试钉住：
 *   - 转换结果结构（¬(□(¬A)) / ¬(◇(¬A))）；
 *   - 与原始公式在给定框架下真值等价（对偶恒等式）。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/modal_operators.h"
#include "lv/error_codes.h"

#define TEST_PASS_STATEMENT g_pass_count++
#define TEST_FAIL_STATEMENT g_fail_count++
#include "test_helpers.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============== 对偶转换结构测试 ============== */

/** 测试可能→必然对偶转换：◇A → ¬□¬A */
static void test_possible_to_necessary_not(void) {
    Proposition *p = proposition_create(0, PROPOSITION_TYPE_ATOMIC);
    lvModalFormula *diamond = lv_modal_formula_create(lv_MODALOP_POSSIBLE, p);
    TEST_ASSERT(diamond != NULL, "diamond formula created");

    lvModalFormula *result = lv_modal_possible_to_necessary_not(diamond);
    TEST_ASSERT(result != NULL, "possible_to_necessary_not 返回新公式");
    /* 结构：¬(□(¬P)) → op=¬, sub=□, sub.sub=¬, sub.sub.inner_prop=P */
    TEST_ASSERT(result->op == lv_MODALOP_NEGATION, "外层为否定");
    TEST_ASSERT(result->sub != NULL, "有子式");
    TEST_ASSERT(result->sub->op == lv_MODALOP_NECESSARY, "子式为必然");
    TEST_ASSERT(result->sub->sub != NULL, "必然的子式为否定");
    TEST_ASSERT(result->sub->sub->op == lv_MODALOP_NEGATION, "内层为否定");
    TEST_ASSERT(result->sub->sub->inner_prop == p, "内层命题共享");

    lv_modal_formula_destroy(result);
    /* NULL 输入 → NULL */
    TEST_ASSERT_NULL(lv_modal_possible_to_necessary_not(NULL));
    /* 非 ◇ 公式 → NULL */
    lvModalFormula *box_only = lv_modal_formula_create(lv_MODALOP_NECESSARY, p);
    TEST_ASSERT_NULL(lv_modal_possible_to_necessary_not(box_only));
    lv_modal_formula_destroy(box_only);

    lv_modal_formula_destroy(diamond);
    proposition_destroy(p);
    PASS();
}

/** 测试必然→可能对偶转换：□A → ¬◇¬A */
static void test_necessary_to_not_possible(void) {
    Proposition *p = proposition_create(0, PROPOSITION_TYPE_ATOMIC);
    lvModalFormula *box = lv_modal_formula_create(lv_MODALOP_NECESSARY, p);
    TEST_ASSERT(box != NULL, "box formula created");

    lvModalFormula *result = lv_modal_necessary_to_not_possible(box);
    TEST_ASSERT(result != NULL, "necessary_to_not_possible 返回新公式");
    TEST_ASSERT(result->op == lv_MODALOP_NEGATION, "外层为否定");
    TEST_ASSERT(result->sub != NULL, "有子式");
    TEST_ASSERT(result->sub->op == lv_MODALOP_POSSIBLE, "子式为可能");
    TEST_ASSERT(result->sub->sub != NULL, "可能的子式为否定");
    TEST_ASSERT(result->sub->sub->op == lv_MODALOP_NEGATION, "内层为否定");

    lv_modal_formula_destroy(result);
    /* NULL 输入 → NULL */
    TEST_ASSERT_NULL(lv_modal_necessary_to_not_possible(NULL));
    /* 非 □ 公式 → NULL */
    lvModalFormula *dia_only = lv_modal_formula_create(lv_MODALOP_POSSIBLE, p);
    TEST_ASSERT_NULL(lv_modal_necessary_to_not_possible(dia_only));
    lv_modal_formula_destroy(dia_only);

    lv_modal_formula_destroy(box);
    proposition_destroy(p);
    PASS();
}

/* ============== 对偶恒等式真值等价测试 ============== */

/** 测试 ◇P 与 ¬□¬P 在框架中真值一致 */
static void test_duality_truth_equivalence(void) {
    lvModalFrame *frame = lv_modal_frame_create();
    lvModalWorld *w1 = lv_modal_world_create(1, "W1", NULL);
    lvModalWorld *w2 = lv_modal_world_create(2, "W2", NULL);
    lv_modal_frame_add_world(frame, w1);
    lv_modal_frame_add_world(frame, w2);
    lv_modal_frame_set_reachability(frame, 1, 2, lv_REACH_RIGID_TRANSFORM);

    Proposition *p = proposition_create(0, PROPOSITION_TYPE_ATOMIC);
    lv_modal_world_assert(w2, p); /* P 仅在 W2 真 */

    /* ◇P 在世界 1：W1 假，W2 可达且真 → TRUE */
    lvModalFormula *dia = lv_modal_formula_create(lv_MODALOP_POSSIBLE, p);
    lvModalEvalResult r1;
    memset(&r1, 0, sizeof(r1));
    TEST_ASSERT_EQ(lv_modal_evaluate(frame, dia, 1, &r1), 0);

    /* ¬□¬P 在世界 1：应等价 TRUE */
    lvModalFormula *conv = lv_modal_possible_to_necessary_not(dia);
    TEST_ASSERT_NOT_NULL(conv);
    lvModalEvalResult r2;
    memset(&r2, 0, sizeof(r2));
    TEST_ASSERT_EQ(lv_modal_evaluate(frame, conv, 1, &r2), 0);
    TEST_ASSERT_EQ((int) r1.truth_value, (int) r2.truth_value);

    lv_modal_eval_result_destroy(&r1);
    lv_modal_eval_result_destroy(&r2);
    lv_modal_formula_destroy(conv);
    lv_modal_formula_destroy(dia);
    proposition_destroy(p);
    lv_modal_frame_destroy(frame);
    PASS();
}

/* ============== 主函数 ============== */

TEST_MAIN_BEGIN("Modal Operators Test Suite")
    printf("=== Lv-00 Modal Operators Test Suite ===\n\n");
    TEST_MAIN_RUN(test_possible_to_necessary_not);
    TEST_MAIN_RUN(test_necessary_to_not_possible);
    TEST_MAIN_RUN(test_duality_truth_equivalence);
TEST_MAIN_END()
