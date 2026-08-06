/**
 * @file test_proof_strategy_numeric.c
 * @brief 数值验证策略（PROOF_STRATEGY_NUMERIC_VERIFICATION）测试
 *
 * 覆盖 core/src/layer4_reasoning/proof_system/proof_strategy_numeric.c：
 * - 适用性判定（numeric_verification_applicability_check）：
 *   比较谓词 + 两侧为「仅含实数常量」表达式 -> 适用；含变量/几何谓词/散文 -> 不适用
 * - 执行（execute_numeric_verification）：
 *   常量表达式区间求值（lv_interval_*）+ 差值区间判定 + FPTaylor 误差界
 *   分级（TrustColor：GREEN/BLUE/AMBER 为成功系，RED 冲突失败）
 * - 策略注册：默认策略表已注册 PROOF_STRATEGY_NUMERIC_VERIFICATION，
 *   proof_multi_strategy_try_all 对数值命题最终落到该策略
 *
 * 测试边界说明：
 * - 两个策略函数为非 static 全局符号（声明于 proof_multi_strategy_internal.h，
 *   core/src 内部头文件不在测试 include 路径），此处按同签名 extern 声明，
 *   链接 lv 静态库即可解析符号。
 * - execute 的 mse 参数在实现中被忽略（(void) mse），可传 NULL。
 *
 * @version 1.0.0
 * @date 2026-08-06
 */

#include "lv/lv_platform.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/lv_utils.h"
#include "lv/proof.h"
#include "test_helpers.h"

/* ============================================================
 * 数值验证策略函数（与 proof_multi_strategy_internal.h 签名一致）
 * ============================================================ */
bool numeric_verification_applicability_check(const ProofMultiStrategy *mse, const ConstraintGraph *graph,
                                              const Proposition *prop);
bool execute_numeric_verification(ProofMultiStrategy *mse, ProofNavigator *nav);

/* ============================================================
 * Global test counters
 * ============================================================ */
int g_pass_count = 0;
int g_fail_count = 0;

/* ============================================================
 * 辅助：构造带文本 name 的原子命题
 * ============================================================ */
static Proposition *make_prop(const char *text) {
    Proposition *prop = proposition_create(0, PROPOSITION_TYPE_ATOMIC);
    if (!prop)
        return NULL;
    if (text)
        prop->name = lv_strdup_safe(text);
    return prop;
}

/* ============================================================
 * Test: applicability 判定
 * ============================================================ */

static void test_applicability_accepts_numeric(void) {
    Proposition *prop = make_prop("1.0/3.0 < 0.3334");
    TEST_ASSERT_NOT_NULL(prop);
    TEST_ASSERT_MSG(numeric_verification_applicability_check(NULL, NULL, prop),
                    "1.0/3.0 < 0.3334 should be applicable");
    proposition_unref(prop);

    prop = make_prop("sqrt(2.0) > 1.414");
    TEST_ASSERT_NOT_NULL(prop);
    TEST_ASSERT_MSG(numeric_verification_applicability_check(NULL, NULL, prop),
                    "sqrt(2.0) > 1.414 should be applicable");
    proposition_unref(prop);

    /* 扩展函数：tan/atan/asin/acos/floor/ceil 等均为支持的函数名 */
    prop = make_prop("tan(1.0) > 1.5");
    TEST_ASSERT_NOT_NULL(prop);
    TEST_ASSERT_MSG(numeric_verification_applicability_check(NULL, NULL, prop),
                    "tan(1.0) > 1.5 should be applicable");
    proposition_unref(prop);

    prop = make_prop("asin(0.5) < 0.6");
    TEST_ASSERT_NOT_NULL(prop);
    TEST_ASSERT_MSG(numeric_verification_applicability_check(NULL, NULL, prop),
                    "asin(0.5) < 0.6 should be applicable");
    proposition_unref(prop);
}

static void test_applicability_rejects_non_numeric(void) {
    /* 几何谓词：字母串 onLine 不是受支持函数名 */
    Proposition *prop = make_prop("onLine(p0, s1)");
    TEST_ASSERT_NOT_NULL(prop);
    TEST_ASSERT_MSG(!numeric_verification_applicability_check(NULL, NULL, prop),
                    "onLine(p0, s1) should NOT be applicable");
    proposition_unref(prop);

    /* 含变量：x1 不是受支持函数名/数字 */
    prop = make_prop("x1 > 3.14");
    TEST_ASSERT_NOT_NULL(prop);
    TEST_ASSERT_MSG(!numeric_verification_applicability_check(NULL, NULL, prop),
                    "x1 > 3.14 (variable) should NOT be applicable");
    proposition_unref(prop);

    /* 散文文本：无比较谓词 */
    prop = make_prop("The quick brown fox jumps over the lazy dog");
    TEST_ASSERT_NOT_NULL(prop);
    TEST_ASSERT_MSG(!numeric_verification_applicability_check(NULL, NULL, prop),
                    "prose text should NOT be applicable");
    proposition_unref(prop);

    /* 无比较谓词的纯常量 */
    prop = make_prop("1.0 + 2.0");
    TEST_ASSERT_NOT_NULL(prop);
    TEST_ASSERT_MSG(!numeric_verification_applicability_check(NULL, NULL, prop),
                    "expression without comparator should NOT be applicable");
    proposition_unref(prop);

    /* NULL 命题 */
    TEST_ASSERT_MSG(!numeric_verification_applicability_check(NULL, NULL, NULL),
                    "NULL proposition should NOT be applicable");
}

/* ============================================================
 * Test: execute —— 区间求值 + TrustColor 分级
 * ============================================================ */

static void test_execute_holds_green(void) {
    /* 1.0/3.0 < 0.3334：区间成立 + 误差界 GREEN -> 成功（GREEN_COMPLETE 步骤） */
    Proposition *prop = make_prop("1.0/3.0 < 0.3334");
    TEST_ASSERT_NOT_NULL(prop);
    ProofNavigator *nav = proof_navigator_create(prop, NULL);
    TEST_ASSERT_NOT_NULL(nav);

    bool ok = execute_numeric_verification(NULL, nav);
    TEST_ASSERT_MSG(ok, "1.0/3.0 < 0.3334 should hold");
    TEST_ASSERT_MSG(nav->step_count == 1, "expected one proof step");
    TEST_ASSERT_MSG(nav->steps[0]->color == PROOF_COLOR_GREEN_COMPLETE,
                    "step color should be GREEN_COMPLETE (TRUST_GREEN)");
    TEST_ASSERT_MSG(nav->strategy_note != NULL, "strategy note should be set on success");

    proof_navigator_destroy(nav);
    proposition_unref(prop);
}

static void test_execute_conflict_red(void) {
    /* 2.0 < 1.0：区间判定不成立 -> RED 冲突步骤，execute 失败 */
    Proposition *prop = make_prop("2.0 < 1.0");
    TEST_ASSERT_NOT_NULL(prop);
    ProofNavigator *nav = proof_navigator_create(prop, NULL);
    TEST_ASSERT_NOT_NULL(nav);

    bool ok = execute_numeric_verification(NULL, nav);
    TEST_ASSERT_MSG(!ok, "2.0 < 1.0 should fail");
    TEST_ASSERT_MSG(nav->step_count == 1, "expected one failure step");
    TEST_ASSERT_MSG(nav->steps[0]->color == PROOF_COLOR_RED_CONFLICT,
                    "failure step should be RED_CONFLICT");

    proof_navigator_destroy(nav);
    proposition_unref(prop);
}

static void test_execute_equality_tolerance(void) {
    /* 0.1+0.2 = 0.3：'=' 采用容差语义（零在容差扩展区间内即成立） */
    Proposition *prop = make_prop("0.1+0.2 = 0.3");
    TEST_ASSERT_NOT_NULL(prop);
    ProofNavigator *nav = proof_navigator_create(prop, NULL);
    TEST_ASSERT_NOT_NULL(nav);

    bool ok = execute_numeric_verification(NULL, nav);
    TEST_ASSERT_MSG(ok, "0.1+0.2 = 0.3 should hold within tolerance");
    TEST_ASSERT_MSG(nav->step_count == 1, "expected one proof step");
    TEST_ASSERT_MSG(nav->steps[0]->color == PROOF_COLOR_GREEN_COMPLETE,
                    "equality tolerance step should be GREEN family");

    proof_navigator_destroy(nav);
    proposition_unref(prop);
}

static void test_execute_extended_functions(void) {
    /* 扩展函数表达式：tan/atan/asin 均被 numeric_ce_apply_function 支持 */
    static const char *kCases[] = {
        "tan(1.0) > 1.5",   /* tan(1)≈1.5574 > 1.5 */
        "atan(1.0) > 0.7",  /* atan(1)≈0.7854 > 0.7 */
        "asin(0.5) < 0.6",  /* asin(0.5)≈0.5236 < 0.6 */
        "sqrt(2.0) > 1.414" /* sqrt(2)≈1.41421 > 1.414 */
    };
    for (size_t k = 0; k < sizeof(kCases) / sizeof(kCases[0]); k++) {
        Proposition *prop = make_prop(kCases[k]);
        TEST_ASSERT_NOT_NULL(prop);
        ProofNavigator *nav = proof_navigator_create(prop, NULL);
        TEST_ASSERT_NOT_NULL(nav);

        bool ok = execute_numeric_verification(NULL, nav);
        if (!ok) {
            fprintf(stderr, "  FAIL: execute should hold for '%s'\n", kCases[k]);
            proof_navigator_destroy(nav);
            proposition_unref(prop);
            g_fail_count++;
            return;
        }
        g_pass_count++;

        proof_navigator_destroy(nav);
        proposition_unref(prop);
    }
}

static void test_execute_rejects_unsupported(void) {
    /* 非数值命题：无法提取 -> 策略不适用，execute 返回 false 且不记录步骤 */
    Proposition *prop = make_prop("onLine(p0, s1)");
    TEST_ASSERT_NOT_NULL(prop);
    ProofNavigator *nav = proof_navigator_create(prop, NULL);
    TEST_ASSERT_NOT_NULL(nav);

    bool ok = execute_numeric_verification(NULL, nav);
    TEST_ASSERT_MSG(!ok, "non-numeric proposition should fail in execute");
    TEST_ASSERT_MSG(nav->step_count == 0, "no step should be recorded for non-numeric claim");

    proof_navigator_destroy(nav);
    proposition_unref(prop);
}

/* ============================================================
 * Test: 策略注册与 try_all
 * ============================================================ */

static void test_strategy_registered(void) {
    ProofMultiStrategy *mse = proof_multi_strategy_create(NULL);
    TEST_ASSERT_NOT_NULL(mse);

    const ProofStrategyDescriptor *desc = &mse->strategies[PROOF_STRATEGY_NUMERIC_VERIFICATION];
    TEST_ASSERT_MSG(desc->type == PROOF_STRATEGY_NUMERIC_VERIFICATION, "numeric strategy descriptor type");
    TEST_ASSERT_MSG(desc->name != NULL && strcmp(desc->name, "数值验证") == 0, "numeric strategy name");
    TEST_ASSERT_MSG(desc->applicability_check == numeric_verification_applicability_check,
                    "numeric strategy applicability fn registered");
    TEST_ASSERT_MSG(desc->execute == execute_numeric_verification, "numeric strategy execute fn registered");
    TEST_ASSERT_MSG(desc->status == PROOF_STRATEGY_AVAILABLE, "numeric strategy available");

    bool in_fallback = false;
    for (int i = 0; i < mse->fallback_count; i++) {
        if (mse->fallback_order[i] == PROOF_STRATEGY_NUMERIC_VERIFICATION) {
            in_fallback = true;
            break;
        }
    }
    TEST_ASSERT_MSG(in_fallback, "numeric strategy should be in default fallback order");

    proof_multi_strategy_destroy(mse);
}

static void test_try_all_lands_on_numeric(void) {
    /* try_all 竞争模式：前置策略（构造/面积/坐标等）对纯数值命题均失败，
     * 最终应落到 PROOF_STRATEGY_NUMERIC_VERIFICATION 并成功 */
    Proposition *prop = make_prop("1.0/3.0 < 0.3334");
    TEST_ASSERT_NOT_NULL(prop);
    ProofNavigator *nav = proof_navigator_create(prop, NULL);
    TEST_ASSERT_NOT_NULL(nav);
    ProofMultiStrategy *mse = proof_multi_strategy_create(nav);
    TEST_ASSERT_NOT_NULL(mse);

    ProofStrategyType r = proof_multi_strategy_try_all(mse);
    TEST_ASSERT_MSG(r == PROOF_STRATEGY_NUMERIC_VERIFICATION,
                    "try_all should land on PROOF_STRATEGY_NUMERIC_VERIFICATION");

    proof_multi_strategy_destroy(mse);
    proof_navigator_destroy(nav);
    proposition_unref(prop);
}

/* ============================================================
 * Main
 * ============================================================ */
TEST_MAIN_BEGIN("ProofStrategyNumeric")

    TEST_MAIN_RUN(test_applicability_accepts_numeric);
    TEST_MAIN_RUN(test_applicability_rejects_non_numeric);

    TEST_MAIN_RUN(test_execute_holds_green);
    TEST_MAIN_RUN(test_execute_conflict_red);
    TEST_MAIN_RUN(test_execute_equality_tolerance);
    TEST_MAIN_RUN(test_execute_extended_functions);
    TEST_MAIN_RUN(test_execute_rejects_unsupported);

    TEST_MAIN_RUN(test_strategy_registered);
    TEST_MAIN_RUN(test_try_all_lands_on_numeric);

TEST_MAIN_END()
