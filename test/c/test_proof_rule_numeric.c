/**
 * @file test_proof_rule_numeric.c
 * @brief 数值验证规则（proof_rule_engine.c）入口测试
 *
 * 覆盖 core/src/layer4_reasoning/proof_system/proof_rule_engine.c：
 * - lv_proof_rule_numeric_verification_create：规则实例字段
 *   （名称 = lv_PROOF_RULE_NAME_NUMERIC_VERIFICATION、RULE_REWRITE、
 *   weight 1.0、applicability_check_fn/apply_fn 非 NULL）
 * - 规则引擎路径：rule_engine_add_rule + proof_state_create(字符串目标)
 *   + rule_engine_search：
 *   "1.0/3.0 < 0.3334" / "sqrt(2.0) > 1.414" -> SEARCH_RESULT_FOUND；
 *   "onLine(p0, s1)" / "x1 > 3.14"（变量）/ "2.0 < 1.0"（数值冲突）
 *   -> SEARCH_RESULT_EXHAUSTED
 * - 公开入口 lv_proof_rule_apply("numeric_verification", 目标字符串, &out)：
 *   数值成立 -> 0（FOUND）；不适用/数值冲突 -> 1；NULL 参数 -> -1
 *
 * 调用形态（读实现确认）：
 * - 规则引擎状态目标是字符串：proof_state_create(const char*)；
 * - lv_proof_rule_apply 返回 0 = SEARCH_RESULT_FOUND，1 = 其他状态，
 *   -1 = 参数无效；成功时 *output 为当前目标（数值规则求解后目标栈
 *   为空 -> NULL）。
 *
 * @version 1.0.0
 * @date 2026-08-06
 */

#include <string.h>

#include "proof_rule_engine.h"
#include "proof_session.h" /* 传递包含 proof_rule_engine_internal.h（rule_engine_* / proof_state_* 声明） */
#include "test_helpers.h"

/* ============================================================
 * Global test counters
 * ============================================================ */
int g_pass_count = 0;
int g_fail_count = 0;

/* ============================================================
 * Test: 规则实例创建与字段
 * ============================================================ */
static void test_rule_create_fields(void) {
    lvProofRule *rule = lv_proof_rule_numeric_verification_create();
    TEST_ASSERT_NOT_NULL(rule);
    TEST_ASSERT_STR_EQ(rule->name, lv_PROOF_RULE_NAME_NUMERIC_VERIFICATION);
    TEST_ASSERT_EQ(rule->type, RULE_REWRITE);
    TEST_ASSERT_MSG(rule->weight == 1.0, "numeric rule weight should be 1.0");
    TEST_ASSERT_NOT_NULL((void *) rule->applicability_check_fn);
    TEST_ASSERT_NOT_NULL((void *) rule->apply_fn);

    lv_free((void **) &rule);
}

/* ============================================================
 * Test: 规则引擎路径 —— 数值成立目标 FOUND
 * ============================================================ */
static void test_engine_found(void) {
    static const char *kHolds[] = {
        "1.0/3.0 < 0.3334",
        "sqrt(2.0) > 1.414",
        "tan(1.0) > 1.5",
        "asin(0.5) < 0.6",
        "0.1+0.2 = 0.3"
    };
    for (size_t k = 0; k < sizeof(kHolds) / sizeof(kHolds[0]); k++) {
        lvRuleEngine *engine = rule_engine_create();
        TEST_ASSERT_NOT_NULL(engine);

        lvProofRule *rule = lv_proof_rule_numeric_verification_create();
        TEST_ASSERT_NOT_NULL(rule);
        TEST_ASSERT_MSG(rule_engine_add_rule(engine, rule), "add numeric rule should succeed");
        TEST_ASSERT_EQ(rule_engine_rule_count(engine), 1);

        lvProofState *state = proof_state_create(kHolds[k]);
        TEST_ASSERT_NOT_NULL(state);

        lvSearchResultStatus result = rule_engine_search(engine, state);
        if (result != SEARCH_RESULT_FOUND) {
            fprintf(stderr, "  FAIL: search should be FOUND for '%s' (status=%d)\n", kHolds[k], (int) result);
            proof_state_destroy(state);
            rule_engine_destroy(engine);
            g_fail_count++;
            return;
        }
        g_pass_count++;
        TEST_ASSERT_MSG(proof_state_is_complete(state), "state should be complete after numeric verification");

        proof_state_destroy(state);
        rule_engine_destroy(engine); /* 释放 rule（引擎持有） */
    }
}

/* ============================================================
 * Test: 规则引擎路径 —— 不适用 / 数值冲突 EXHAUSTED
 * ============================================================ */
static void test_engine_exhausted(void) {
    static const char *kNotHolds[] = {
        "onLine(p0, s1)",  /* 几何谓词，非数值 */
        "x1 > 3.14",       /* 含变量 */
        "2.0 < 1.0"        /* 数值但冲突：适用但执行失败 */
    };
    for (size_t k = 0; k < sizeof(kNotHolds) / sizeof(kNotHolds[0]); k++) {
        lvRuleEngine *engine = rule_engine_create();
        TEST_ASSERT_NOT_NULL(engine);
        lvProofRule *rule = lv_proof_rule_numeric_verification_create();
        TEST_ASSERT_NOT_NULL(rule);
        rule_engine_add_rule(engine, rule);

        lvProofState *state = proof_state_create(kNotHolds[k]);
        TEST_ASSERT_NOT_NULL(state);

        lvSearchResultStatus result = rule_engine_search(engine, state);
        if (result == SEARCH_RESULT_FOUND) {
            fprintf(stderr, "  FAIL: search should NOT be FOUND for '%s'\n", kNotHolds[k]);
            proof_state_destroy(state);
            rule_engine_destroy(engine);
            g_fail_count++;
            return;
        }
        g_pass_count++;
        TEST_ASSERT_MSG(!proof_state_is_complete(state), "state should remain incomplete");

        proof_state_destroy(state);
        rule_engine_destroy(engine);
    }
}

/* ============================================================
 * Test: 公开入口 lv_proof_rule_apply
 * ============================================================ */
static void test_apply_api(void) {
    void *out = (void *) 0x1; /* 哨兵：验证失败路径会置 NULL */

    /* 数值成立：返回 0（FOUND）；求解后目标栈为空 -> output 为 NULL */
    int rc = lv_proof_rule_apply(lv_PROOF_RULE_NAME_NUMERIC_VERIFICATION, "1.0/3.0 < 0.3334", &out);
    TEST_ASSERT_EQ(rc, 0);

    /* 数值冲突：返回非 0，output 置 NULL */
    out = (void *) 0x1;
    rc = lv_proof_rule_apply(lv_PROOF_RULE_NAME_NUMERIC_VERIFICATION, "2.0 < 1.0", &out);
    TEST_ASSERT_EQ(rc, 1);
    TEST_ASSERT_NULL(out);

    /* 非数值目标：返回非 0 */
    out = (void *) 0x1;
    rc = lv_proof_rule_apply(lv_PROOF_RULE_NAME_NUMERIC_VERIFICATION, "onLine(p0, s1)", &out);
    TEST_ASSERT_EQ(rc, 1);
    TEST_ASSERT_NULL(out);

    /* 未知规则名：临时规则无 applicability/apply fn -> 不适用 */
    rc = lv_proof_rule_apply("unknown_rule", "1+2=3", &out);
    TEST_ASSERT_EQ(rc, 1);

    /* NULL 参数 -> -1 */
    TEST_ASSERT_EQ(lv_proof_rule_apply(NULL, "1.0/3.0 < 0.3334", &out), -1);
    TEST_ASSERT_EQ(lv_proof_rule_apply(lv_PROOF_RULE_NAME_NUMERIC_VERIFICATION, NULL, &out), -1);
    TEST_ASSERT_EQ(lv_proof_rule_apply(lv_PROOF_RULE_NAME_NUMERIC_VERIFICATION, "1.0/3.0 < 0.3334", NULL), -1);
}

/* ============================================================
 * Main
 * ============================================================ */
TEST_MAIN_BEGIN("ProofRuleNumeric")

    TEST_MAIN_RUN(test_rule_create_fields);
    TEST_MAIN_RUN(test_engine_found);
    TEST_MAIN_RUN(test_engine_exhausted);
    TEST_MAIN_RUN(test_apply_api);

TEST_MAIN_END()
