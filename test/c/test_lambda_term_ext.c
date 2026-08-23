/**
 * @file test_lambda_term_ext.c
 * @brief λ-项求值器契约测试（批次 C-㊺续35：lambda_term.h 零覆盖 API）
 *
 * 覆盖零覆盖 API（2 个）：
 *   lv_lambda_eval_full / lv_lambda_eval_set_max_steps
 *
 * 契约要点（与 lambda_term.h / lambda_eval 实现核对）：
 *   - eval_full：语义别名 eval（不动点求值）；term NULL → NULL。
 *   - eval_set_max_steps：<= 0 恢复默认 10000；正数设置上限。
 *
 * @author Lv-00 Project
 */

#include <stdio.h>

#include "lv/lambda_term.h"

#include "test_unified.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============== 测试：完整求值 ============== */

static void test_eval_full(void) {
    /* NULL 契约 */
    TEST_ASSERT_NULL(lv_lambda_eval_full(NULL));

    /* (λx.x) (λx.x) → 规范形 λx.x（ABS 节点）；两个子项独立避免 double-free */
    LvLambdaTerm *id1 = lv_lambda_create_abs(0, lv_lambda_create_var(0));
    LvLambdaTerm *id2 = lv_lambda_create_abs(0, lv_lambda_create_var(0));
    LvLambdaTerm *app = lv_lambda_create_app(id1, id2);

    LvLambdaTerm *nf = lv_lambda_eval_full(app);
    TEST_ASSERT_NOT_NULL(nf);
    TEST_ASSERT_EQ((int) nf->type, (int) LV_LAMBDA_ABS);

    lv_lambda_destroy(nf);
    lv_lambda_destroy(app);

    /* 变量项：自由变量保留 */
    LvLambdaTerm *var = lv_lambda_create_var(0);
    LvLambdaTerm *r = lv_lambda_eval_full(var);
    TEST_ASSERT_NOT_NULL(r);
    TEST_ASSERT_EQ((int) r->type, (int) LV_LAMBDA_VAR);
    lv_lambda_destroy(r);
    lv_lambda_destroy(var);
}

/* ============== 测试：步数上限 ============== */

static void test_set_max_steps(void) {
    /* 设置小上限后求值仍正常（简单项；两个独立子项） */
    lv_lambda_eval_set_max_steps(5);

    LvLambdaTerm *id1a = lv_lambda_create_abs(0, lv_lambda_create_var(0));
    LvLambdaTerm *id1b = lv_lambda_create_abs(0, lv_lambda_create_var(0));
    LvLambdaTerm *app = lv_lambda_create_app(id1a, id1b);
    LvLambdaTerm *nf = lv_lambda_eval_full(app);
    TEST_ASSERT_NOT_NULL(nf);
    lv_lambda_destroy(nf);
    lv_lambda_destroy(app);

    /* 恢复默认 */
    lv_lambda_eval_set_max_steps(0);
    lv_lambda_eval_set_max_steps(-1);

    /* 恢复默认后仍正常（两个独立子项） */
    LvLambdaTerm *id2a = lv_lambda_create_abs(0, lv_lambda_create_var(0));
    LvLambdaTerm *id2b = lv_lambda_create_abs(0, lv_lambda_create_var(0));
    LvLambdaTerm *app2 = lv_lambda_create_app(id2a, id2b);
    nf = lv_lambda_eval_full(app2);
    TEST_ASSERT_NOT_NULL(nf);
    lv_lambda_destroy(nf);
    lv_lambda_destroy(app2);
}

/* ============== Main ============== */

TEST_MAIN_BEGIN("LambdaTermExt")

    printf("\n--- lambda_term (zero-coverage) ---\n");
    TEST_MAIN_RUN(test_eval_full);
    TEST_MAIN_RUN(test_set_max_steps);

TEST_MAIN_END()
