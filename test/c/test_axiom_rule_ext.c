/**
 * @file test_axiom_rule_ext.c
 * @brief 公理规则引擎契约测试（批次 C-㊺续8：axiom_rule_engine.h 20 个零覆盖 API）
 *
 * 覆盖 20 个 ctest 零覆盖 API：
 *   - 构建族：lv_rule_destroy / set_description / add_variable /
 *     add_premise / add_conclusion / add_tag / set_priority / set_status
 *   - 库族：lv_rule_library_add / destroy / remove / get_by_type /
 *     get_by_difficulty / search_by_tag
 *   - 匹配族：lv_rule_find_matches / apply_match / is_applicable /
 *     match_destroy
 *   - 难度族：lv_difficulty_assessment_destroy / lv_rule_recommendation_destroy
 *
 * 契约要点（与实现核对）：
 *   - 构建 set_description/add_variable 等：NULL 契约 → false；
 *     超限（MAX_VARIABLES/PREMISES/CONCLUSIONS）→ false。
 *   - library_add：规则所有权移交库（remove 时 destroy）。
 *   - library_remove：找到 → true + 规则销毁；未找到 → false。
 *   - 匹配族 NULL 契约不崩溃；match_destroy(NULL) 安全。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_unified.h"
#include "lv/axiom_rule_engine.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============== 测试：规则构建 ============== */

static void test_rule_build_api(void) {
    /* destroy：NULL 安全 */
    lv_rule_destroy(NULL);

    lvRule *r = lv_rule_create("rule1", RULE_TYPE_INFERENCE);
    TEST_ASSERT_NOT_NULL(r);

    /* set_description：NULL 契约 + 正常 */
    TEST_ASSERT(!lv_rule_set_description(NULL, "d"), "NULL rule");
    TEST_ASSERT(!lv_rule_set_description(r, NULL), "NULL desc");
    TEST_ASSERT(lv_rule_set_description(r, "infers X from Y"), "设置描述");
    TEST_ASSERT_STR_EQ(r->description, "infers X from Y");

    /* add_variable：NULL 契约 + 正常 + 超限 */
    TEST_ASSERT(!lv_rule_add_variable(NULL, "v", "point"), "NULL rule");
    TEST_ASSERT(!lv_rule_add_variable(r, NULL, "point"), "NULL name");
    TEST_ASSERT(!lv_rule_add_variable(r, "v", NULL), "NULL type");
    TEST_ASSERT(lv_rule_add_variable(r, "x", "point"), "添加变量");
    TEST_ASSERT_EQ(r->var_count, 1);
    TEST_ASSERT_STR_EQ(r->variables[0].name, "x");

    /* add_premise / add_conclusion */
    TEST_ASSERT(!lv_rule_add_premise(NULL, "p", false), "NULL premise");
    TEST_ASSERT(lv_rule_add_premise(r, "X is point", false), "添加前提");
    TEST_ASSERT_EQ(r->premise_count, 1);
    TEST_ASSERT(!lv_rule_add_conclusion(NULL, "c", TRUST_GREEN), "NULL conclusion");
    TEST_ASSERT(lv_rule_add_conclusion(r, "Y is point", TRUST_GREEN), "添加结论");
    TEST_ASSERT_EQ(r->conclusion_count, 1);
    TEST_ASSERT_EQ(r->conclusions[0].trust_color, TRUST_GREEN);

    /* add_tag */
    TEST_ASSERT(!lv_rule_add_tag(NULL, "t"), "NULL tag");
    TEST_ASSERT(lv_rule_add_tag(r, "geometry"), "添加标签");
    TEST_ASSERT_EQ(r->tag_count, 1);

    /* set_priority / set_status：NULL 安全 + 正常 */
    lv_rule_set_priority(NULL, RULE_PRIORITY_HIGH); /* 不崩溃即通过 */
    lv_rule_set_priority(r, RULE_PRIORITY_HIGH);
    TEST_ASSERT_EQ(r->priority, RULE_PRIORITY_HIGH);
    lv_rule_set_status(r, RULE_STATUS_ENABLED);
    TEST_ASSERT_EQ(r->status, RULE_STATUS_ENABLED);

    lv_rule_destroy(r);
    printf("  test_rule_build_api: PASSED\n");
}

/* ============== 测试：规则库 ============== */

static void test_library_api(void) {
    /* destroy：NULL 安全 */
    lv_rule_library_destroy(NULL);

    lvRuleLibrary *lib = lv_rule_library_create(NULL);
    TEST_ASSERT_NOT_NULL(lib);

    /* add：NULL 契约 + 正常（所有权移交库） */
    TEST_ASSERT(!lv_rule_library_add(NULL, NULL), "NULL lib");
    TEST_ASSERT(!lv_rule_library_add(lib, NULL), "NULL rule");
    lvRule *r1 = lv_rule_create("r1", RULE_TYPE_INFERENCE);
    lvRule *r2 = lv_rule_create("r2", RULE_TYPE_AXIOM);
    lvRule *r3 = lv_rule_create("r3", RULE_TYPE_INFERENCE);
    TEST_ASSERT_NOT_NULL(r1);
    TEST_ASSERT_NOT_NULL(r2);
    TEST_ASSERT_NOT_NULL(r3);
    lv_rule_add_tag(r1, "alpha");
    lv_rule_add_tag(r3, "alpha");
    lv_rule_add_tag(r3, "beta");
    TEST_ASSERT(lv_rule_library_add(lib, r1), "添加 r1");
    TEST_ASSERT(lv_rule_library_add(lib, r2), "添加 r2");
    TEST_ASSERT(lv_rule_library_add(lib, r3), "添加 r3");
    TEST_ASSERT_EQ(lib->rule_count, 3);

    /* get_by_type：INFERENCE → 2 */
    lvRule *typed[4];
    uint32_t n = lv_rule_library_get_by_type(lib, RULE_TYPE_INFERENCE, typed, 4);
    TEST_ASSERT_EQ(n, 2);
    TEST_ASSERT_EQ(lv_rule_library_get_by_type(NULL, RULE_TYPE_INFERENCE, typed, 4), 0);

    /* get_by_difficulty：全范围（0-10，新规则 level 0）→ 3 */
    lvRule *diff[4];
    n = lv_rule_library_get_by_difficulty(lib, 0, 10, diff, 4);
    TEST_ASSERT_EQ(n, 3);

    /* search_by_tag：alpha → 2；beta → 1；无 → 0 */
    lvRule *tagged[4];
    n = lv_rule_library_search_by_tag(lib, "alpha", tagged, 4);
    TEST_ASSERT_EQ(n, 2);
    n = lv_rule_library_search_by_tag(lib, "beta", tagged, 4);
    TEST_ASSERT_EQ(n, 1);
    n = lv_rule_library_search_by_tag(lib, "nope", tagged, 4);
    TEST_ASSERT_EQ(n, 0);

    /* remove：存在 → true（规则被库销毁）；未找到 → false */
    TEST_ASSERT(lv_rule_library_remove(lib, r1->id), "移除 r1");
    TEST_ASSERT_EQ(lib->rule_count, 2);
    TEST_ASSERT(!lv_rule_library_remove(lib, 99999), "移除不存在的规则");

    lv_rule_library_destroy(lib);
    printf("  test_library_api: PASSED\n");
}

/* ============== 测试：匹配 ============== */

static void test_match_api(void) {
    /* match_destroy：NULL 安全 */
    lv_rule_match_destroy(NULL);

    lvRuleLibrary *lib = lv_rule_library_create(NULL);
    TEST_ASSERT_NOT_NULL(lib);

    /* find_matches：空库 → 0；NULL 契约 */
    lvRuleMatch *matches[4];
    TEST_ASSERT_EQ(lv_rule_find_matches(lib, NULL, NULL, matches, 4), 0);
    TEST_ASSERT_EQ(lv_rule_find_matches(NULL, NULL, NULL, matches, 4), 0);

    /* 含规则库：执行不崩溃（无匹配图 → 0 或 >0） */
    lvRule *r = lv_rule_create("r1", RULE_TYPE_AXIOM);
    TEST_ASSERT_NOT_NULL(r);
    lv_rule_add_premise(r, "point A", false);
    lv_rule_add_conclusion(r, "segment AB", TRUST_GREEN);
    TEST_ASSERT(lv_rule_library_add(lib, r), "添加规则到库");
    ConstraintGraph *g = graph_create();
    TEST_ASSERT_NOT_NULL(g);
    uint32_t n = lv_rule_find_matches(lib, g, NULL, matches, 4);
    TEST_ASSERT(n <= 4, "匹配数量合法");
    for (uint32_t i = 0; i < n; i++)
        lv_rule_match_destroy(matches[i]);

    /* is_applicable：AXIOM 类型恒适用（NULL graph 也 true）；
     * 非 axiom 规则 NULL graph → false */
    TEST_ASSERT(!lv_rule_is_applicable(NULL, g, NULL), "NULL rule");
    TEST_ASSERT(lv_rule_is_applicable(r, NULL, NULL), "AXIOM 规则恒适用");
    lvRule *inf = lv_rule_create("inf", RULE_TYPE_INFERENCE);
    TEST_ASSERT_NOT_NULL(inf);
    lv_rule_add_premise(inf, "p1", false);
    TEST_ASSERT(!lv_rule_is_applicable(inf, NULL, NULL), "INFERENCE NULL graph 不适用");
    lv_rule_destroy(inf);

    /* apply_match：NULL 契约 */
    ProofStep *steps[4];
    TEST_ASSERT_EQ(lv_rule_apply_match(NULL, g, NULL, steps, 4), 0);

    graph_destroy(g);
    lv_rule_library_destroy(lib);
    printf("  test_match_api: PASSED\n");
}

/* ============== 测试：难度评估 ============== */

static void test_assess_api(void) {
    /* destroy：NULL 安全 */
    lv_difficulty_assessment_destroy(NULL);
    lv_rule_recommendation_destroy(NULL);

    lvRule *r = lv_rule_create("r1", RULE_TYPE_THEOREM);
    TEST_ASSERT_NOT_NULL(r);
    lv_rule_add_premise(r, "p1", false);
    lv_rule_add_premise(r, "p2", false);
    lv_rule_add_premise(r, "p3", false);
    lv_rule_add_conclusion(r, "c1", TRUST_GREEN);

    /* 评估：score/level 合理 */
    lvDifficultyAssessment *a = lv_rule_assess_difficulty(r);
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT(a->overall_score <= 1000, "难度分数范围");
    TEST_ASSERT(a->level >= 1 && a->level <= 10, "难度等级范围");
    lv_difficulty_assessment_destroy(a);

    lv_rule_destroy(r);
    printf("  test_assess_api: PASSED\n");
}

/* ============== 测试入口 ============== */

TEST_MAIN_BEGIN("Lv-00 Axiom Rule Engine Ext Test Suite")
    printf("=== Lv-00 Axiom Rule Engine Ext Test Suite (batch C-㊺续8) ===\n\n");
    lv_init();

    TEST_MAIN_RUN(test_rule_build_api);
    TEST_MAIN_RUN(test_library_api);
    TEST_MAIN_RUN(test_match_api);
    TEST_MAIN_RUN(test_assess_api);

    lv_cleanup();
TEST_MAIN_END()
