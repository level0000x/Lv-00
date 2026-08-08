/**
 * @file test_template_test.c
 * @brief 约束模板测试框架测试
 *
 * 测试内容：
 * - 测试用例生命周期（创建/销毁/字段验证）
 * - 测试执行器（axiom_template_test_run 运行用例）
 * - 高层测试运行器（axiom_template_run_tests 双层运行）
 * - 正则形式验证
 * - NULL 输入和空数据等边界情况
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "test_unified.h"

int g_pass_count = 0;
int g_fail_count = 0;

#include "lv/lv_utils.h"

#include "lv.h"

/* ============== 辅助展开函数 ============== */

/** 测试用展开函数：生成基础约束（创建1个POINT节点 + 1个约束） */
static void expand_basic(SymbolicCoord **params, ConstraintGraph *target) {
    (void) params;
    if (!target)
        return;
    /* 使用 graph 创建节点和约束以保证 graph 结构一致性 */
    SymbolicCoord *origin = mk_rat(0, 1);
    SymbolicCoord *point = mk_rat(1, 1);
    SymbolicCoord *coords[] = {origin, point};
    int ids[2];
    for (int i = 0; i < 2; i++) {
        GeomNode *n = graph_add_node_with_id(target, target->next_node_id, GEOM_POINT, &coords[i], 1);
        ids[i] = n ? n->id : -1;
    }
    if (ids[0] >= 0 && ids[1] >= 0) {
        int parts[2] = {ids[0], ids[1]};
        graph_add_constraint_with_id(target, -1, (ConstraintType) INCIDENCE, parts, 2);
    }
    symbolic_coord_destroy(origin);
    symbolic_coord_destroy(point);
}

/** 测试用展开函数：不生成任何约束 */
static void expand_empty(SymbolicCoord **params, ConstraintGraph *target) {
    (void) params;
    (void) target;
    /* 不添加任何节点或约束 */
}

/** 测试用展开函数：生成多个约束 */
static void expand_multi(SymbolicCoord **params, ConstraintGraph *target) {
    (void) params;
    if (!target)
        return;
    /* 创建3个节点和3个约束确保展开后 constraint_count > 0 */
    SymbolicCoord *coords[3];
    int ids[3];
    for (int i = 0; i < 3; i++) {
        coords[i] = symbolic_coord_create_rational((int64_t) i, 1);
        GeomNode *n = graph_add_node_with_id(target, target->next_node_id, GEOM_POINT, &coords[i], 1);
        ids[i] = n ? n->id : -1;
        symbolic_coord_destroy(coords[i]);
    }
    for (int i = 0; i < 3; i++) {
        if (ids[i] < 0 || ids[(i + 1) % 3] < 0)
            continue;
        int parts[2] = {ids[i], ids[(i + 1) % 3]};
        graph_add_constraint_with_id(target, -1, (ConstraintType) INCIDENCE, parts, 2);
    }
}

/* ============== 辅助：创建并注册测试模板 ============== */

static ConstraintTemplate *create_and_register_template(AxiomPackage *pkg, const char *name, int param_count,
                                                        void (*expand)(SymbolicCoord **, ConstraintGraph *)) {
    ConstraintTemplate *tmpl = lv_calloc(1, sizeof(ConstraintTemplate));
    TEST_ASSERT_CONTINUE(tmpl != NULL, "tmpl != NULL");
    tmpl->name = lv_strdup_safe(name);
    tmpl->param_count = param_count;
    tmpl->expand = expand;
    tmpl->verified = true;
    tmpl->params = NULL;
    tmpl->param_desc_count = 0;
    memset(&tmpl->normal_form, 0, sizeof(tmpl->normal_form));
    tmpl->level = TEMPLATE_LEVEL_ONE;
    tmpl->is_compressed = false;
    tmpl->compressed_subgraph = NULL;

    bool ok = axiom_package_register_template(pkg, tmpl);
    TEST_ASSERT_CONTINUE(ok, "ok");

    /* 注册后释放包装内存（包内已深拷贝 name；params 被置 NULL 由包管理） */
    lv_free((void **) &tmpl->name);
    lv_free((void **) &tmpl);
    return NULL;
}

/* ============== 测试组1：测试用例生命周期 ============== */

static void test_case_lifecycle(void) {
    printf("Test: template test case lifecycle...\n");

    /* --- 创建工厂测试用例 --- */
    TemplateTestCase *tc = axiom_template_test_case_create("MidpointTest", TEST_CASE_FACTORY, 2, true);
    lv_ASSERT_NOT_NULL(tc);
    lv_ASSERT_NOT_NULL(tc->template_name);
    lv_ASSERT_STR_EQ(tc->template_name, "MidpointTest");
    lv_ASSERT(tc->type == TEST_CASE_FACTORY);
    lv_ASSERT(tc->param_count == 2);
    lv_ASSERT(tc->expected_result == true);
    lv_ASSERT(tc->params == NULL);
    lv_ASSERT(tc->expected_graph == NULL);
    printf("  工厂用例: name='%s', type=TEST_CASE_FACTORY, param_count=%d, expected=%d\n", tc->template_name,
           tc->param_count, tc->expected_result);

    axiom_template_test_case_destroy(tc);
    printf("  工厂用例销毁成功\n");

    /* --- 创建用户测试用例 --- */
    tc = axiom_template_test_case_create("UserCustomTest", TEST_CASE_USER, 0, false);
    lv_ASSERT_NOT_NULL(tc);
    lv_ASSERT_STR_EQ(tc->template_name, "UserCustomTest");
    lv_ASSERT(tc->type == TEST_CASE_USER);
    lv_ASSERT(tc->param_count == 0);
    lv_ASSERT(tc->expected_result == false);
    printf("  用户用例: name='%s', type=TEST_CASE_USER, param_count=%d, expected=%d\n", tc->template_name,
           tc->param_count, tc->expected_result);

    axiom_template_test_case_destroy(tc);
    printf("  用户用例销毁成功\n");

    /* --- NULL / 无效输入边界 --- */
    TemplateTestCase *null_tc = axiom_template_test_case_create(NULL, TEST_CASE_FACTORY, 1, true);
    lv_ASSERT(null_tc == NULL);
    printf("  name=NULL → 返回 NULL (正确)\n");

    null_tc = axiom_template_test_case_create("Test", TEST_CASE_FACTORY, -1, true);
    lv_ASSERT(null_tc == NULL);
    printf("  param_count=-1 → 返回 NULL (正确)\n");

    /* 销毁 NULL → 不崩溃 */
    axiom_template_test_case_destroy(NULL);
    printf("  destroy(NULL) → 无崩溃\n");

    printf("  PASSED\n");

}

/* ============== 测试组2：测试执行器（axiom_template_test_run）============== */

static void test_runner_basic(void) {
    printf("Test: template test runner basic...\n");

    AxiomPackage *pkg = axiom_package_create("RunnerPkg", "1.0");
    lv_ASSERT_NOT_NULL(pkg);

    create_and_register_template(pkg, "BasicTpl", 1, expand_basic);
    create_and_register_template(pkg, "EmptyTpl", 1, expand_empty);
    printf("  模板注册完成\n");

    /* --- 测试1：期望通过，实际通过 --- */
    TemplateTestCase *tc_pass = axiom_template_test_case_create("BasicTpl", TEST_CASE_FACTORY, 1, true);
    lv_ASSERT_NOT_NULL(tc_pass);

    TemplateTestCase *cases[] = {tc_pass};
    int passed = 0, failed = 0;
    char **failures = NULL;

    int ret = axiom_template_test_run(pkg, cases, 1, &passed, &failed, &failures);
    lv_ASSERT(passed == 1);
    lv_ASSERT(failed == 0);
    printf("  测试1 (期望通过): passed=%d, failed=%d, ret=%d\n", passed, failed, ret);

    if (failures) {
        for (int i = 0; i < failed; i++)
            lv_free((void **) &failures[i]);
        lv_free((void **) &failures);
        failures = NULL;
    }
    axiom_template_test_case_destroy(tc_pass);

    /* --- 测试2：期望失败，实际也失败（空展开 → constraint_count=0） --- */
    TemplateTestCase *tc_fail = axiom_template_test_case_create("EmptyTpl", TEST_CASE_FACTORY, 1, false);
    lv_ASSERT_NOT_NULL(tc_fail);

    TemplateTestCase *cases2[] = {tc_fail};
    passed = 0;
    failed = 0;
    ret = axiom_template_test_run(pkg, cases2, 1, &passed, &failed, &failures);
    lv_ASSERT(passed == 1);
    lv_ASSERT(failed == 0);
    printf("  测试2 (期望失败): passed=%d, failed=%d, ret=%d\n", passed, failed, ret);

    if (failures) {
        for (int i = 0; i < failed; i++)
            lv_free((void **) &failures[i]);
        lv_free((void **) &failures);
        failures = NULL;
    }
    axiom_template_test_case_destroy(tc_fail);

    /* --- 测试3：期望通过，实际失败（不一致） --- */
    TemplateTestCase *tc_mismatch = axiom_template_test_case_create("EmptyTpl", TEST_CASE_FACTORY, 1, true);
    lv_ASSERT_NOT_NULL(tc_mismatch);

    TemplateTestCase *cases3[] = {tc_mismatch};
    passed = 0;
    failed = 0;
    ret = axiom_template_test_run(pkg, cases3, 1, &passed, &failed, &failures);
    lv_ASSERT(passed == 0);
    lv_ASSERT(failed >= 1);
    printf("  测试3 (期望通过→失败): passed=%d, failed=%d, ret=%d\n", passed, failed, ret);

    if (failures && failed > 0) {
        printf("    失败消息: %s\n", failures[0]);
    }

    if (failures) {
        for (int i = 0; i < failed; i++)
            lv_free((void **) &failures[i]);
        lv_free((void **) &failures);
        failures = NULL;
    }
    axiom_template_test_case_destroy(tc_mismatch);

    axiom_package_destroy(pkg);
    printf("  PASSED\n");

}

/* ============== 测试组3：高层测试运行器（axiom_template_run_tests）============== */

static void test_runner_high_level(void) {
    printf("Test: template test runner high-level...\n");

    AxiomPackage *pkg = axiom_package_create("HighLevelPkg", "1.0");
    lv_ASSERT_NOT_NULL(pkg);

    create_and_register_template(pkg, "MultiTpl", 2, expand_multi);

    /* --- 工厂测试数组（深拷贝后保留副本，测试完成后再释放） --- */
    TemplateTestCase factory_tests[2];
    TemplateTestCase *factory_heap[2];
    for (int i = 0; i < 2; i++) {
        factory_heap[i] = axiom_template_test_case_create("MultiTpl", TEST_CASE_FACTORY, 2, true);
        lv_ASSERT_NOT_NULL(factory_heap[i]);
        factory_tests[i] = *factory_heap[i];
        factory_tests[i].template_name = lv_strdup_safe(factory_heap[i]->template_name);
        factory_tests[i].description = lv_strdup_safe(factory_heap[i]->description);
        axiom_template_test_case_destroy(factory_heap[i]);
    }

    /* --- 用户测试数组 --- */
    TemplateTestCase user_tests[2];
    {
        TemplateTestCase *utc0 = axiom_template_test_case_create("MultiTpl", TEST_CASE_USER, 2, true);
        lv_ASSERT_NOT_NULL(utc0);
        user_tests[0] = *utc0;
        user_tests[0].template_name = lv_strdup_safe(utc0->template_name);
        user_tests[0].description = lv_strdup_safe(utc0->description);
        axiom_template_test_case_destroy(utc0);
    }
    {
        TemplateTestCase *utc1 = axiom_template_test_case_create("MultiTpl", TEST_CASE_USER, 1, false);
        lv_ASSERT_NOT_NULL(utc1);
        user_tests[1] = *utc1;
        user_tests[1].template_name = lv_strdup_safe(utc1->template_name);
        user_tests[1].description = lv_strdup_safe(utc1->description);
        axiom_template_test_case_destroy(utc1);
    }

    /* 运行测试 */
    TemplateTestResult result = axiom_template_run_tests(pkg, "MultiTpl", factory_tests, 2, user_tests, 2);

    lv_ASSERT(result.total == 4);
    lv_ASSERT(result.passed >= 3); /* 前三个 expand_multi → constraint_count=3 > 0，应通过 */
    printf("  高层运行器: total=%d, passed=%d, failed=%d\n", result.total, result.passed, result.failed);

    if (result.failed > 0 && result.failure_messages) {
        for (int i = 0; i < result.failed; i++) {
            if (result.failure_messages[i]) {
                printf("  失败消息[%d]: %s\n", i, result.failure_messages[i]);
            }
        }
    }

    axiom_template_test_result_destroy(&result);
    for (int i = 0; i < 2; i++) {
        lv_free((void **) &factory_tests[i].template_name);
        lv_free((void **) &factory_tests[i].description);
        lv_free((void **) &user_tests[i].template_name);
        lv_free((void **) &user_tests[i].description);
    }
    axiom_package_destroy(pkg);
    printf("  PASSED\n");

}

/* ============== 测试组4：正则形式验证 ============== */

static void test_normal_form_verification(void) {
    printf("Test: normal form verification...\n");

    AxiomPackage *pkg = axiom_package_create("NormPkg", "1.0");
    lv_ASSERT_NOT_NULL(pkg);

    /* 创建模板并设置 normal_form */
    ConstraintTemplate *tmpl = lv_calloc(1, sizeof(ConstraintTemplate));
    lv_ASSERT_NOT_NULL(tmpl);
    tmpl->name = lv_strdup_safe("NormTpl");
    tmpl->param_count = 2;
    tmpl->expand = expand_basic;
    tmpl->verified = true;
    tmpl->params = NULL;
    tmpl->param_desc_count = 0;
    tmpl->normal_form.constraint_type_count = 1;
    tmpl->normal_form.node_type_count = 2;
    tmpl->normal_form.expected_constraint_types[0] = 0;
    tmpl->normal_form.expected_node_types[0] = 0;
    tmpl->normal_form.expected_node_types[1] = 0;
    tmpl->level = TEMPLATE_LEVEL_ONE;
    tmpl->is_compressed = false;
    tmpl->compressed_subgraph = NULL;

    bool ok = axiom_package_register_template(pkg, tmpl);
    lv_ASSERT(ok);
    lv_free((void **) &tmpl->name);
    lv_free((void **) &tmpl);

    /* 验证正则形式 */
    bool verified = axiom_template_verify_normal_form(pkg, "NormTpl");
    lv_ASSERT(verified == true);
    printf("  正则形式验证通过\n");

    /* 验证未定义 normal_form 的模板（应跳过，返回 true） */
    create_and_register_template(pkg, "NoNormTpl", 1, expand_basic);
    bool skip_verified = axiom_template_verify_normal_form(pkg, "NoNormTpl");
    lv_ASSERT(skip_verified == true);
    printf("  无 normal_form 模板跳过验证 (正确)\n");

    /* 验证不存在的模板（应返回 false） */
    bool not_found = axiom_template_verify_normal_form(pkg, "NonExistentTpl");
    lv_ASSERT(not_found == false);
    printf("  不存在的模板返回 false (正确)\n");

    /* NULL 输入 */
    bool null_pkg = axiom_template_verify_normal_form(NULL, "NormTpl");
    lv_ASSERT(null_pkg == false);
    printf("  NULL pkg 返回 false (正确)\n");

    bool null_name = axiom_template_verify_normal_form(pkg, NULL);
    lv_ASSERT(null_name == false);
    printf("  NULL name 返回 false (正确)\n");

    axiom_package_destroy(pkg);
    printf("  PASSED\n");

}

/* ============== 测试组5：边界情况 ============== */

static void test_edge_cases(void) {
    printf("Test: edge cases...\n");

    AxiomPackage *pkg = axiom_package_create("EdgePkg", "1.0");
    lv_ASSERT_NOT_NULL(pkg);

    create_and_register_template(pkg, "EdgeTpl", 1, expand_basic);

    /* --- 测试1：NULL 输入到 axiom_template_test_run --- */
    int passed = 0, failed = 0;
    char **failures = NULL;
    bool ok;
    TemplateTestCase *tc = axiom_template_test_case_create("EdgeTpl", TEST_CASE_FACTORY, 1, true);
    lv_ASSERT_NOT_NULL(tc);
    TemplateTestCase *valid_cases[] = {tc};

    int ret = axiom_template_test_run(NULL, valid_cases, 1, &passed, &failed, &failures);
    lv_ASSERT(ret == -1);
    printf("  NULL pkg → ret=%d (正确)\n", ret);

    ret = axiom_template_test_run(pkg, NULL, 1, &passed, &failed, &failures);
    lv_ASSERT(ret == -1);
    printf("  NULL test_cases → ret=%d (正确)\n", ret);

    ret = axiom_template_test_run(pkg, valid_cases, 0, &passed, &failed, &failures);
    lv_ASSERT(ret == -1);
    printf("  count=0 → ret=%d (正确)\n", ret);

    ret = axiom_template_test_run(pkg, valid_cases, -1, &passed, &failed, &failures);
    lv_ASSERT(ret == -1);
    printf("  count=-1 → ret=%d (正确)\n", ret);

    axiom_template_test_case_destroy(tc);

    /* --- 测试2：模板不存在 --- */
    TemplateTestCase *tc2 = axiom_template_test_case_create("NonExistent", TEST_CASE_FACTORY, 1, true);
    lv_ASSERT_NOT_NULL(tc2);
    TemplateTestCase *cases2[] = {tc2};
    passed = 0;
    failed = 0;
    failures = NULL;
    ret = axiom_template_test_run(pkg, cases2, 1, &passed, &failed, &failures);
    lv_ASSERT(passed == 0);
    lv_ASSERT(failed >= 1);
    printf("  不存在的模板: passed=%d, failed=%d (正确)\n", passed, failed);

    if (failures) {
        for (int i = 0; i < failed; i++)
            lv_free((void **) &failures[i]);
        lv_free((void **) &failures);
    }
    axiom_template_test_case_destroy(tc2);

    /* --- 测试3：空包运行 --- */
    TemplateTestResult empty_result = axiom_template_run_tests(pkg, "EdgeTpl", NULL, 0, NULL, 0);
    lv_ASSERT(empty_result.total == 0);
    printf("  空测试集: total=%d (正确)\n", empty_result.total);
    /* 空结果不需要 destroy（failure_messages 为 NULL） */

    /* --- 测试4：高层运行器带 NULL 包 --- */
    TemplateTestResult null_pkg_result = axiom_template_run_tests(NULL, "EdgeTpl", NULL, 0, NULL, 0);
    lv_ASSERT(null_pkg_result.total == 0);
    printf("  高层运行器 NULL pkg: total=%d (正确)\n", null_pkg_result.total);

    /* --- 测试5：高层运行器带空模板名 --- */
    TemplateTestResult null_name_result = axiom_template_run_tests(pkg, NULL, NULL, 0, NULL, 0);
    lv_ASSERT(null_name_result.total == 0);
    printf("  高层运行器 NULL template_name: total=%d (正确)\n", null_name_result.total);

    /* --- 测试6：result_destroy 接受 NULL --- */
    axiom_template_test_result_destroy(NULL);
    printf("  result_destroy(NULL) → 无崩溃\n");

    /* --- 测试7：模板无 expand 函数 --- */
    ConstraintTemplate *no_expand_tmpl = lv_calloc(1, sizeof(ConstraintTemplate));
    lv_ASSERT_NOT_NULL(no_expand_tmpl);
    no_expand_tmpl->name = lv_strdup_safe("NoExpandTpl");
    no_expand_tmpl->param_count = 1;
    no_expand_tmpl->expand = NULL;
    no_expand_tmpl->verified = false;
    no_expand_tmpl->params = NULL;
    no_expand_tmpl->param_desc_count = 0;
    memset(&no_expand_tmpl->normal_form, 0, sizeof(no_expand_tmpl->normal_form));
    no_expand_tmpl->level = TEMPLATE_LEVEL_ONE;
    no_expand_tmpl->is_compressed = false;
    no_expand_tmpl->compressed_subgraph = NULL;
    ok = axiom_package_register_template(pkg, no_expand_tmpl);
    lv_ASSERT(ok);
    lv_free((void **) &no_expand_tmpl->name);
    lv_free((void **) &no_expand_tmpl);

    TemplateTestCase *tc3 = axiom_template_test_case_create("NoExpandTpl", TEST_CASE_FACTORY, 1, true);
    lv_ASSERT_NOT_NULL(tc3);
    TemplateTestCase *cases3[] = {tc3};
    passed = 0;
    failed = 0;
    failures = NULL;
    ret = axiom_template_test_run(pkg, cases3, 1, &passed, &failed, &failures);
    lv_ASSERT(passed == 0);
    lv_ASSERT(failed >= 1);
    printf("  无 expand 模板: passed=%d, failed=%d (正确)\n", passed, failed);

    if (failures) {
        for (int i = 0; i < failed; i++)
            lv_free((void **) &failures[i]);
        lv_free((void **) &failures);
    }
    axiom_template_test_case_destroy(tc3);

    axiom_package_destroy(pkg);
    printf("  PASSED\n");

}

/* ============== 主函数 ============== */

TEST_MAIN_BEGIN("Lv-00 Template Test Framework Test Suite")
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== Lv-00 Template Test Framework Test Suite ===\n\n");
    lv_init();
    printf("lv_init() done\n");
    TEST_MAIN_RUN(test_case_lifecycle);
    TEST_MAIN_RUN(test_runner_basic);
    TEST_MAIN_RUN(test_runner_high_level);
    TEST_MAIN_RUN(test_normal_form_verification);
    TEST_MAIN_RUN(test_edge_cases);
    printf("\n=== All template test framework tests PASSED! ===\n");
TEST_MAIN_END()
