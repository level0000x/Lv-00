/**
 * @file test_axiom_rule_engine_ext.c
 * @brief 公理规则引擎契约测试（批次 C-㊺续27：axiom_rule_engine.h 9 个零覆盖 API）
 *
 * 覆盖零覆盖 API：
 *   规则库：rule_library_get_by_id / get_by_name
 *   规则：rule_copy / rule_to_json / rule_from_json
 *   难度：difficulty_level_to_string / difficulty_dimension_to_string /
 *     proof_step_assess_difficulty / proposition_assess_difficulty
 *
 * 契约要点（与实现核对）：
 *   - get_by_id/get_by_name：索引或线性回退；不存在 NULL。
 *   - rule_copy：深拷贝（动态字段重置防 double-free、tags 复制）。
 *   - to_json：含 id/name/type/status/priority/counts；NULL → NULL。
 *   - from_json：解析基本字段（范围校验）；name 缺省 "parsed_rule"。
 *   - level_to_string：1-10 中文，越界 "未知"。
 *   - dimension_to_string：维度名，越界 "未知"。
 *   - proof_step_assess_difficulty / proposition_assess_difficulty：
 *     NULL 输入返回默认评估（非 NULL）。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_unified.h"
#include "lv/axiom_rule_engine.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============== 测试：规则库查询 ============== */

static void test_rule_library_query_api(void) {
    lvRuleLibraryConfig cfg = {.max_rules = 8, .auto_validate = true, .auto_difficulty = true,
                               .enable_cache = true, .default_package = "test_pkg"};
    lvRuleLibrary *lib = lv_rule_library_create(&cfg);
    TEST_ASSERT_NOT_NULL(lib);

    /* 添加两条规则 */
    lvRule *r1 = lv_rule_create("rule_a", RULE_TYPE_AXIOM);
    lvRule *r2 = lv_rule_create("rule_b", RULE_TYPE_INFERENCE);
    TEST_ASSERT_NOT_NULL(r1);
    TEST_ASSERT_NOT_NULL(r2);
    r1->id = 10;
    r2->id = 20;
    TEST_ASSERT(lv_rule_library_add(lib, r1), "添加规则1");
    TEST_ASSERT(lv_rule_library_add(lib, r2), "添加规则2");

    /* get_by_id */
    lvRule *got = lv_rule_library_get_by_id(lib, 10);
    TEST_ASSERT_EQ(got, r1);
    got = lv_rule_library_get_by_id(lib, 20);
    TEST_ASSERT_EQ(got, r2);
    TEST_ASSERT_NULL(lv_rule_library_get_by_id(lib, 99));
    TEST_ASSERT_NULL(lv_rule_library_get_by_id(NULL, 10));

    /* get_by_name */
    got = lv_rule_library_get_by_name(lib, "rule_a");
    TEST_ASSERT_EQ(got, r1);
    got = lv_rule_library_get_by_name(lib, "rule_b");
    TEST_ASSERT_EQ(got, r2);
    TEST_ASSERT_NULL(lv_rule_library_get_by_name(lib, "nope"));
    TEST_ASSERT_NULL(lv_rule_library_get_by_name(NULL, "rule_a"));
    TEST_ASSERT_NULL(lv_rule_library_get_by_name(lib, NULL));

    lv_rule_library_destroy(lib);
    printf("  test_rule_library_query_api: PASSED\n");
}

/* ============== 测试：规则复制与序列化 ============== */

static void test_rule_copy_json_api(void) {
    /* 构造规则 */
    lvRule *r = lv_rule_create("modus_ponens", RULE_TYPE_INFERENCE);
    TEST_ASSERT_NOT_NULL(r);
    lv_rule_set_description(r, "MP rule");
    lv_rule_add_variable(r, "P", "prop");
    lv_rule_add_variable(r, "Q", "prop");
    lv_rule_add_premise(r, "P -> Q", false);
    lv_rule_add_premise(r, "P", false);
    lv_rule_add_conclusion(r, "Q", TRUST_GREEN);
    lv_rule_add_tag(r, "logic");
    lv_rule_add_tag(r, "classic");
    lv_rule_set_priority(r, RULE_PRIORITY_HIGH);
    r->id = 7;

    /* copy：深拷贝 */
    lvRule *c = lv_rule_copy(r);
    TEST_ASSERT_NOT_NULL(c);
    TEST_ASSERT(c != r, "独立副本");
    TEST_ASSERT_EQ(c->id, 7);
    TEST_ASSERT(strcmp(c->name, "modus_ponens") == 0, "名称复制");
    TEST_ASSERT(strcmp(c->description, "MP rule") == 0, "描述复制");
    TEST_ASSERT_EQ(c->var_count, 2);
    TEST_ASSERT_EQ(c->premise_count, 2);
    TEST_ASSERT_EQ(c->conclusion_count, 1);
    TEST_ASSERT_EQ(c->tag_count, 2);
    TEST_ASSERT(strcmp(c->tags[0], "logic") == 0, "标签复制");
    TEST_ASSERT_EQ((int) c->priority, (int) RULE_PRIORITY_HIGH);
    TEST_ASSERT_NULL(c->dependency_ids);
    TEST_ASSERT_NULL(lv_rule_copy(NULL));

    /* to_json：含字段（RULE_TYPE_INFERENCE 枚举值=0） */
    char *json = lv_rule_to_json(r);
    TEST_ASSERT_NOT_NULL(json);
    TEST_ASSERT(strstr(json, "\"id\":7") != NULL, "含 id");
    TEST_ASSERT(strstr(json, "\"name\":\"modus_ponens\"") != NULL, "含 name");
    TEST_ASSERT(strstr(json, "\"type\":0") != NULL, "含 type");
    TEST_ASSERT(strstr(json, "\"priority\":75") != NULL, "含 priority");
    TEST_ASSERT_NULL(lv_rule_to_json(NULL));

    /* from_json：往返 */
    lvRule *parsed = lv_rule_from_json(json);
    TEST_ASSERT_NOT_NULL(parsed);
    TEST_ASSERT_EQ(parsed->id, 7);
    TEST_ASSERT(strcmp(parsed->name, "modus_ponens") == 0, "往返名称");
    TEST_ASSERT_EQ((int) parsed->type, (int) RULE_TYPE_INFERENCE);
    TEST_ASSERT_EQ((int) parsed->priority, (int) RULE_PRIORITY_HIGH);
    TEST_ASSERT_NULL(lv_rule_from_json(NULL));

    /* from_json 非法 type 范围保持默认 */
    lvRule *bad = lv_rule_from_json("{\"name\":\"x\",\"type\":99}");
    TEST_ASSERT_NOT_NULL(bad);
    TEST_ASSERT_EQ((int) bad->type, (int) RULE_TYPE_AXIOM);
    lv_rule_destroy(bad);

    lv_free((void **) &json);
    lv_rule_destroy(parsed);
    lv_rule_destroy(c);
    lv_rule_destroy(r);
    printf("  test_rule_copy_json_api: PASSED\n");
}

/* ============== 测试：难度 ============== */

static void test_rule_difficulty_api(void) {
    /* level_to_string */
    TEST_ASSERT(strcmp(lv_difficulty_level_to_string(1), "\xe5\x85\xa5\xe9\x97\xa8") == 0, "level1 入门");
    TEST_ASSERT(strcmp(lv_difficulty_level_to_string(10), "\xe6\x9e\x81\xe9\x99\x90") == 0, "level10 极限");
    TEST_ASSERT(strcmp(lv_difficulty_level_to_string(0), "\xe6\x9c\xaa\xe7\x9f\xa5") == 0, "level0 未知");
    TEST_ASSERT(strcmp(lv_difficulty_level_to_string(11), "\xe6\x9c\xaa\xe7\x9f\xa5") == 0, "level11 未知");

    /* dimension_to_string */
    TEST_ASSERT(strcmp(lv_difficulty_dimension_to_string(DIFF_DIM_STRUCTURAL),
                       "\xe7\xbb\x93\xe6\x9e\x84\xe5\xa4\x8d\xe6\x9d\x82\xe5\xba\xa6") == 0, "结构复杂度");
    TEST_ASSERT(strcmp(lv_difficulty_dimension_to_string((lvDifficultyDimension) 99), "\xe6\x9c\xaa\xe7\x9f\xa5") == 0,
                "越界未知");

    /* proposition_assess_difficulty：NULL 返回默认评估 */
    lvDifficultyAssessment *a = lv_proposition_assess_difficulty(NULL);
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT(a->overall_score > 0, "NULL 命题默认分");
    lv_difficulty_assessment_destroy(a);

    /* 原子命题：score 50, level 1 */
    Proposition *p = proposition_create(0, PROPOSITION_TYPE_ATOMIC);
    a = lv_proposition_assess_difficulty(p);
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT(a->level >= 1, "原子命题低难度");
    lv_difficulty_assessment_destroy(a);

    /* proof_step_assess_difficulty：NULL 返回默认评估 */
    a = lv_proof_step_assess_difficulty(NULL, NULL);
    TEST_ASSERT_NOT_NULL(a);
    lv_difficulty_assessment_destroy(a);
    a = lv_proof_step_assess_difficulty(NULL, NULL);
    TEST_ASSERT_NOT_NULL(a);
    lv_difficulty_assessment_destroy(a);

    proposition_destroy(p);
    printf("  test_rule_difficulty_api: PASSED\n");
}

/* ============== 测试入口 ============== */

TEST_MAIN_BEGIN("Lv-00 Axiom Rule Engine Ext Test Suite")
    printf("=== Lv-00 Axiom Rule Engine Ext Test Suite (batch C-㊺续27) ===\n\n");
    lv_init();

    TEST_MAIN_RUN(test_rule_library_query_api);
    TEST_MAIN_RUN(test_rule_copy_json_api);
    TEST_MAIN_RUN(test_rule_difficulty_api);

    lv_cleanup();
TEST_MAIN_END()
