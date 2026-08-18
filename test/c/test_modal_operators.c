/**
 * @file test_modal_operators.c
 * @brief 模态算子 —— 对偶转换 UNSUPPORTED 行为（P1-4 回归测试）
 *
 * lvModalFormula 结构（op + inner_prop/sub）不含命题取反节点，
 * 无法表达对偶等式 ◇A ≡ ¬□¬A / □A ≡ ¬◇¬A 中的两处否定。
 * 修复前实现静默返回未取反的嵌套公式（语义错误）；
 * 修复后显式返回 NULL 并设置 lv_ERROR_UNSUPPORTED（红线：宁可显式报错）。
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

/* ============== 对偶转换测试 ============== */

/** 测试可能→必然对偶转换 UNSUPPORTED */
static void test_possible_to_necessary_not(void) {
    /* ◇A → 应返回 NULL（UNSUPPORTED，而非静默的 □A） */
    Proposition *p = proposition_create(0, PROPOSITION_TYPE_ATOMIC);
    lvModalFormula *diamond = lv_modal_formula_create(lv_MODALOP_POSSIBLE, p);
    TEST_ASSERT(diamond != NULL, "diamond formula created");

    lvModalFormula *result = lv_modal_possible_to_necessary_not(diamond);
    TEST_ASSERT(result == NULL, "possible_to_necessary_not must return NULL (UNSUPPORTED)");

    /* NULL 输入 → NULL */
    lvModalFormula *null_res = lv_modal_possible_to_necessary_not(NULL);
    TEST_ASSERT(null_res == NULL, "NULL input -> NULL");

    lv_modal_formula_destroy(diamond);
    proposition_destroy(p);
    PASS();
}

/** 测试必然→可能对偶转换 UNSUPPORTED */
static void test_necessary_to_not_possible(void) {
    Proposition *p = proposition_create(0, PROPOSITION_TYPE_ATOMIC);
    lvModalFormula *box = lv_modal_formula_create(lv_MODALOP_NECESSARY, p);
    TEST_ASSERT(box != NULL, "box formula created");

    lvModalFormula *result = lv_modal_necessary_to_not_possible(box);
    TEST_ASSERT(result == NULL, "necessary_to_not_possible must return NULL (UNSUPPORTED)");

    lvModalFormula *null_res = lv_modal_necessary_to_not_possible(NULL);
    TEST_ASSERT(null_res == NULL, "NULL input -> NULL");

    lv_modal_formula_destroy(box);
    proposition_destroy(p);
    PASS();
}

/* ============== 主函数 ============== */

TEST_MAIN_BEGIN("Modal Operators Test Suite")
    printf("=== Lv-00 Modal Operators Test Suite ===\n\n");
    TEST_MAIN_RUN(test_possible_to_necessary_not);
    TEST_MAIN_RUN(test_necessary_to_not_possible);
TEST_MAIN_END()
