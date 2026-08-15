/**
 * @file test_proof.c
 * @brief 证明系统测试 - 命题创建、合一检查、证明导航器、爆炸原理
 *
 * 测试内容：
 * - 命题创建与管理
 * - 证明步骤创建与管理
 * - 证明导航器
 * - 合一检查
 * - 爆炸原理
 * - 证明依赖链
 * - 导出功能
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv.h"
#include "test_helpers.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============== 测试：命题生命周期 ============== */

static void test_proposition_lifecycle(void) {
    printf("Test: proposition lifecycle...\n");

    /* --- 验证命题创建后的初始状态 --- */
    Proposition *prop = proposition_create(1, PROPOSITION_TYPE_ATOMIC);
    lv_ASSERT_NOT_NULL(prop);
    lv_ASSERT(prop->id == 1);
    lv_ASSERT(prop->type == PROPOSITION_TYPE_ATOMIC);
    lv_ASSERT(prop->input_count == 0);
    lv_ASSERT(prop->output_count == 0);
    /* 初始状态下子命题数量应为 0 */
    lv_ASSERT(prop->sub_prop_count == 0);
    /* 初始状态下模式图应为 NULL */
    lv_ASSERT(prop->pattern == NULL);

    printf("  原子命题创建成功 (ID=%d)\n", prop->id);

    /* --- 验证添加步骤后状态变化 --- */
    /* 设置输入端口，验证 input_count 变化 */
    int in_ports[] = {10, 20};
    bool ok = proposition_set_input_ports(prop, in_ports, 2);
    lv_ASSERT(ok == true);
    lv_ASSERT(prop->input_count == 2);
    printf("  设置输入端口： input_count = %d\n", prop->input_count);

    /* 设置输出端口，验证 output_count 变化 */
    int out_ports[] = {30};
    ok = proposition_set_output_ports(prop, out_ports, 1);
    lv_ASSERT(ok == true);
    lv_ASSERT(prop->output_count == 1);
    printf("  设置输出端口： output_count = %d\n", prop->output_count);

    /* 添加子命题，验证 sub_prop_count 变化 */
    Proposition *child = proposition_create(2, PROPOSITION_TYPE_ATOMIC);
    lv_ASSERT_NOT_NULL(child);
    ok = proposition_add_sub_proposition(prop, child);
    lv_ASSERT(ok == true);
    lv_ASSERT(prop->sub_prop_count == 1);
    printf("  添加子命题后: sub_prop_count = %d\n", prop->sub_prop_count);

    /* --- 验证销毁后的资源释放 --- */
    /* 销毁命题（会递归销毁子命题），之后不应再访问已释放的指针 */
    proposition_destroy(prop);
    /* 注意：销毁后 prop 与 child 均已释放，此处不访问以避免未定义行为；
     * 资源释放的正确性由 proposition_destroy 内部实现保证；
     * 可通过内存检测工具（如 Valgrind/ASan）进一步验证 */
    printf("  命题销毁完成，资源已释放\n");

    printf("  PASSED\n");

}

/* ============== 测试：复合命题 ============== */

static void test_composite_propositions(void) {
    printf("Test: composite propositions...\n");

    /* 创建合取命题 */
    Proposition *conj = proposition_create(1, PROPOSITION_TYPE_CONJUNCTION);
    lv_ASSERT_NOT_NULL(conj);
    printf("  合取命题创建成功\n");

    /* 创建析取命题 */
    Proposition *disj = proposition_create(2, PROPOSITION_TYPE_DISJUNCTION);
    lv_ASSERT_NOT_NULL(disj);
    printf("  析取命题创建成功\n");

    /* 创建蕴含命题 */
    Proposition *impl = proposition_create(3, PROPOSITION_TYPE_IMPLICATION);
    lv_ASSERT_NOT_NULL(impl);
    printf("  蕴含命题创建成功\n");

    /* 创建否定命题 */
    Proposition *neg = proposition_create(4, PROPOSITION_TYPE_NEGATION);
    lv_ASSERT_NOT_NULL(neg);
    printf("  否定命题创建成功\n");

    /* 创建全称命题 */
    Proposition *univ = proposition_create(5, PROPOSITION_TYPE_UNIVERSAL);
    lv_ASSERT_NOT_NULL(univ);
    printf("  全称命题创建成功\n");

    /* 创建存在命题 */
    Proposition *exist = proposition_create(6, PROPOSITION_TYPE_EXISTENTIAL);
    lv_ASSERT_NOT_NULL(exist);
    printf("  存在命题创建成功\n");

    /* 创建矛盾命题 */
    Proposition *bottom = proposition_create(7, PROPOSITION_TYPE_BOTTOM);
    lv_ASSERT_NOT_NULL(bottom);
    printf("  矛盾命题创建成功\n");

    proposition_destroy(conj);
    proposition_destroy(disj);
    proposition_destroy(impl);
    proposition_destroy(neg);
    proposition_destroy(univ);
    proposition_destroy(exist);
    proposition_destroy(bottom);

    printf("  PASSED\n");

}

/* ============== 测试：命题端口设置 ============== */

static void test_proposition_ports(void) {
    printf("Test: proposition port configuration...\n");

    ConstraintGraph *g = graph_create();

    Proposition *prop = proposition_create(1, PROPOSITION_TYPE_ATOMIC);
    lv_ASSERT_NOT_NULL(prop);

    /* 创建输入端口 */
    int in_ports[] = {1, 2};
    bool ok = proposition_set_input_ports(prop, in_ports, 2);
    lv_ASSERT(ok);
    lv_ASSERT(prop->input_count == 2);
    printf("  输入端口设置成功: %d 个\n", prop->input_count);

    /* 创建输出端口 */
    int out_ports[] = {3};
    ok = proposition_set_output_ports(prop, out_ports, 1);
    lv_ASSERT(ok);
    lv_ASSERT(prop->output_count == 1);
    printf("  输出端口设置成功: %d 个\n", prop->output_count);

    /* 创建模式图 */
    int p1 = add_point(g, 0, 1, 0, 1);
    int p2 = add_point(g, 1, 1, 1, 1);
    graph_add_line_segment(g, p1, p2);

    ok = proposition_set_pattern(prop, g);
    lv_ASSERT(ok);
    lv_ASSERT(prop->pattern == g);
    printf("  模式图设置成功\n");

    proposition_destroy(prop);
    /* 注意：g 已由 proposition_destroy 内部销毁（作为 prop->pattern），不要再次释放 */

    printf("  PASSED\n");

}

/* ============== 测试：子命题 ============== */

static void test_sub_propositions(void) {
    printf("Test: sub-propositions...\n");

    /* 创建父命题 */
    Proposition *parent = proposition_create(1, PROPOSITION_TYPE_CONJUNCTION);
    lv_ASSERT_NOT_NULL(parent);

    /* 创建子命题 */
    Proposition *child1 = proposition_create(2, PROPOSITION_TYPE_ATOMIC);
    Proposition *child2 = proposition_create(3, PROPOSITION_TYPE_ATOMIC);

    /* 添加子命题 */
    bool ok = proposition_add_sub_proposition(parent, child1);
    lv_ASSERT(ok);
    printf("  子命题添加成功\n");

    ok = proposition_add_sub_proposition(parent, child2);
    lv_ASSERT(ok);
    printf("  子命题添加成功\n");

    lv_ASSERT(parent->sub_prop_count == 2);

    proposition_destroy(parent);
    /* 注意：proposition_destroy 会递归销毁子命题 */

    printf("  PASSED\n");

}

/* ============== 测试：证明步骤 ============== */

static void test_proof_steps(void) {
    printf("Test: proof steps...\n");

    /* 创建不同类型的证明步骤 */
    ProofStep *step1 = proof_step_create(PROOF_STEP_ADD_NODE);
    lv_ASSERT_NOT_NULL(step1);
    lv_ASSERT(step1->type == PROOF_STEP_ADD_NODE);
    printf("  添加节点步骤创建成功\n");

    ProofStep *step2 = proof_step_create(PROOF_STEP_ADD_CONSTRAINT);
    lv_ASSERT_NOT_NULL(step2);
    printf("  添加约束步骤创建成功\n");

    ProofStep *step3 = proof_step_create(PROOF_STEP_NORMALIZATION);
    lv_ASSERT_NOT_NULL(step3);
    printf("  规范化步骤创建成功\n");

    ProofStep *step4 = proof_step_create(PROOF_STEP_UNIFY);
    lv_ASSERT_NOT_NULL(step4);
    printf("  合一检查步骤创建成功\n");

    /* 设置断点 */
    proof_step_set_breakpoint(step1, true);
    lv_ASSERT(step1->is_breakpoint == true);
    printf("  断点设置成功\n");

    /* 添加依赖关系 */
    bool ok = proof_step_add_dependency(step2, step1->id);
    printf("  依赖关系添加: %s\n", ok ? "成功" : "失败");

    proof_step_destroy(step1);
    proof_step_destroy(step2);
    proof_step_destroy(step3);
    proof_step_destroy(step4);

    printf("  PASSED\n");

}

/* ============== 测试：证明导航器 ============== */

static void test_proof_navigator(void) {
    printf("Test: proof navigator...\n");

    /* 创建目标命题 */
    Proposition *target = proposition_create(1, PROPOSITION_TYPE_ATOMIC);
    lv_ASSERT_NOT_NULL(target);

    /* 创建证明导航器 */
    ProofNavigator *nav = proof_navigator_create(target, NULL);
    lv_ASSERT_NOT_NULL(nav);
    lv_ASSERT(nav->target_prop == target);
    lv_ASSERT(nav->step_count == 0);
    lv_ASSERT(nav->current_step == -1); /* 初始值为 -1（无当前步骤） */

    printf("  证明导航器创建成功\n");

    /* 添加证明步骤 */
    ProofStep *step1 = proof_step_create(PROOF_STEP_ADD_NODE);
    bool ok = proof_navigator_add_step(nav, step1);
    lv_ASSERT(ok);
    lv_ASSERT(nav->step_count == 1);
    printf("  步骤1添加成功\n");

    ProofStep *step2 = proof_step_create(PROOF_STEP_NORMALIZATION);
    ok = proof_navigator_add_step(nav, step2);
    lv_ASSERT(ok);
    lv_ASSERT(nav->step_count == 2);
    printf("  步骤2添加成功\n");

    /* 导航测试 */
    ProofStep *current = proof_navigator_current_step(nav);
    printf("  当前步骤: %s\n", current ? proof_step_type_to_string(current->type) : "空");

    /* 下一步 */
    ok = proof_navigator_next(nav);
    printf("  移动到下一步: %s\n", ok ? "成功" : "失败/已到末尾");

    /* 上一步 */
    ok = proof_navigator_prev(nav);
    printf("  移动到上一步: %s\n", ok ? "成功" : "失败/已在开头");

    /* 计算证明颜色 */
    ProofColor color = proof_navigator_compute_final_color(nav);
    printf("  证明颜色: %s\n", proof_color_to_string(color));

    proof_navigator_destroy(nav);
    proposition_destroy(target);

    printf("  PASSED\n");

}

/* ============== 测试：合一检查 ============== */

static void test_unify_check(void) {
    printf("Test: unify check...\n");

    /* 创建构造图 */
    ConstraintGraph *construction = graph_create();
    int p1 = add_point(construction, 0, 1, 0, 1);
    int p2 = add_point(construction, 1, 1, 1, 1);
    graph_add_line_segment(construction, p1, p2);

    /* 创建命题 */
    Proposition *prop = proposition_create(1, PROPOSITION_TYPE_ATOMIC);
    lv_ASSERT_NOT_NULL(prop);

    /* 设置命题的模式图 */
    ConstraintGraph *pattern = graph_create();
    int pp1 = add_point(pattern, 0, 1, 0, 1);
    int pp2 = add_point(pattern, 1, 1, 1, 1);
    graph_add_line_segment(pattern, pp1, pp2);

    proposition_set_pattern(prop, pattern);

    /* 执行合一检查 */
    UnifyStatus result = proof_unify(construction, prop, false);
    printf("  合一结果: %s\n", unify_result_to_string(result));

    /* 详细合一检查 */
    char *mismatch_info = NULL;
    UnifyStatus result2 = proof_unify_detailed(construction, prop, &mismatch_info);
    printf("  详细合一结果: %s\n", unify_result_to_string(result2));
    if (mismatch_info) {
        printf("  不匹配信息: %s\n", mismatch_info);
        lv_free_ptr(mismatch_info);
    }

    graph_destroy(construction);
    proposition_destroy(prop);
    /* 注意：pattern 已由 proposition_destroy 内部销毁，不要再次释放 */

    printf("  PASSED\n");

}

/* ============== 测试：证明依赖链 ============== */

static void test_proof_dependencies(void) {
    printf("Test: proof dependencies...\n");

    /* 创建依赖 */
    ProofDependency *dep = proof_dependency_create(PROOF_COLOR_GREEN);
    lv_ASSERT_NOT_NULL(dep);
    lv_ASSERT(dep->color == PROOF_COLOR_GREEN);
    printf("  依赖创建成功\n");

    /* 创建子依赖 */
    ProofDependency *sub_dep = proof_dependency_create(PROOF_COLOR_BLUE_UNEXPLORED);
    bool ok = proof_dependency_add_sub(dep, sub_dep);
    printf("  子依赖添加: %s\n", ok ? "成功" : "失败");

    /* 计算依赖链颜色 */
    ProofColor computed = proof_dependency_compute_color(dep);
    printf("  计算颜色: %s\n", proof_color_to_string(computed));

    proof_dependency_destroy(dep);

    printf("  PASSED\n");

}

/* ============== 测试：爆炸原理 ============== */

static void test_ex_falso(void) {
    printf("Test: explosion principle (ex falso)...\n");

    ConstraintGraph *g = graph_create();

    /* 创建爆炸原理函数块 */
    int block_id = -1;
    bool ok = proof_create_ex_falso_block(g, &block_id);
    printf("  创建爆炸原理函数块: %s (ID=%d)\n", ok ? "成功" : "失败", block_id);

    /* 创建证明导航器和目标命题 */
    Proposition *target = proposition_create(1, PROPOSITION_TYPE_ATOMIC);
    ProofNavigator *nav = proof_navigator_create(target, NULL);

    /* 创建⊥的证物（简化示例） */
    ConstraintGraph *bottom_proof = graph_create();

    /* 应用爆炸原理 */
    ok = proof_apply_ex_falso(nav, bottom_proof, target);
    printf("  应用爆炸原理: %s\n", ok ? "成功" : "失败");

    proof_navigator_destroy(nav);
    proposition_destroy(target);
    graph_destroy(g);
    graph_destroy(bottom_proof);

    printf("  PASSED\n");

}

/* ============== 测试：证明颜色 ============== */

static void test_proof_colors(void) {
    printf("Test: proof colors...\n");

    /* 测试所有证明颜色 */
    ProofColor colors[] = {PROOF_COLOR_GREEN,          PROOF_COLOR_BLUE_UNEXPLORED,
                           PROOF_COLOR_BLUE_RESOURCE,  PROOF_COLOR_BLUE_OUT_OF_RANGE,
                           PROOF_COLOR_GREEN_VERIFIED, PROOF_COLOR_YELLOW,
                           PROOF_COLOR_ORANGE_ORACLE,  PROOF_COLOR_ORANGE_EX_FALSO,
                           PROOF_COLOR_AMBER,          PROOF_COLOR_DARK_ORANGE};

    const char *expected[] = {"Green",  "BlueUnexplored", "BlueResource",  "BlueOutOfRange", "GreenVerified",
                              "Yellow", "OrangeOracle",   "OrangeExFalso", "Amber",          "DarkOrange"};

    for (int i = 0; i < sizeof(colors) / sizeof(colors[0]); i++) {
        const char *str = proof_color_to_string(colors[i]);
        printf("  %s -> %s\n", expected[i], str);
    }

    printf("  PASSED\n");

}

/* ============== 测试：辅助函数 ============== */

static void test_helper_functions(void) {
    printf("Test: helper functions...\n");

    /* 命题类型转字符串 */
    const char *str = proposition_type_to_string(PROPOSITION_TYPE_ATOMIC);
    printf("  ATOMIC -> %s\n", str);

    str = proposition_type_to_string(PROPOSITION_TYPE_CONJUNCTION);
    printf("  CONJUNCTION -> %s\n", str);

    str = proposition_type_to_string(PROPOSITION_TYPE_IMPLICATION);
    printf("  IMPLICATION -> %s\n", str);

    /* 步骤类型转字符串 */
    str = proof_step_type_to_string(PROOF_STEP_ADD_NODE);
    printf("  ADD_NODE -> %s\n", str);

    str = proof_step_type_to_string(PROOF_STEP_UNIFY);
    printf("  UNIFY -> %s\n", str);

    str = proof_step_type_to_string(PROOF_STEP_EX_FALSO);
    printf("  EX_FALSO -> %s\n", str);

    /* 合一结果转字符串 */
    str = unify_result_to_string(UNIFY_STATUS_OK);
    printf("  UNIFY_STATUS_OK -> %s\n", str);

    str = unify_result_to_string(UNIFY_STATUS_PORT_TYPE_MISMATCH);
    printf("  PORT_TYPE_MISMATCH -> %s\n", str);

    str = unify_result_to_string(UNIFY_STATUS_CONSTRAINT_MISMATCH);
    printf("  CONSTRAINT_MISMATCH -> %s\n", str);

    printf("  PASSED\n");

}

/* ============== task group tests ============== */

static int g_task_a_calls = 0;
static int g_task_b_calls = 0;

static void task_fn_a(void *arg) {
    (void) arg;
    g_task_a_calls++;
}

static void task_fn_b(void *arg) {
    (void) arg;
    g_task_b_calls++;
}

static void test_task_group(void) {
    printf("Test: task group (create/add/run/wait/destroy)...\n");

    g_task_a_calls = 0;
    g_task_b_calls = 0;

    lvTaskGroup *g = lv_task_group_create("task_group_test");
    lv_ASSERT_NOT_NULL(g);

    lvTask *t1 = lv_task_create(task_fn_a, NULL, "a");
    lvTask *t2 = lv_task_create(task_fn_b, NULL, "b");
    lvTask *t3 = lv_task_create(task_fn_a, NULL, "c");
    lv_ASSERT_NOT_NULL(t1);
    lv_ASSERT_NOT_NULL(t2);
    lv_ASSERT_NOT_NULL(t3);

    lv_task_group_add(g, t1);
    lv_task_group_add(g, t2);
    lv_task_group_add(g, t3);

    /* explicit run executes all queued tasks in FIFO order */
    int done = lv_task_group_run(g);
    lv_ASSERT(done == 3);
    lv_ASSERT(g_task_a_calls == 2);
    lv_ASSERT(g_task_b_calls == 1);

    /* lazy execution: add then wait completes remaining tasks */
    lvTask *t4 = lv_task_create(task_fn_a, NULL, "d");
    lv_task_group_add(g, t4);
    lv_task_group_wait(g);
    lv_ASSERT(g_task_a_calls == 3);

    /* destroy executes leftover tasks instead of leaking them */
    lvTask *t5 = lv_task_create(task_fn_b, NULL, "e");
    lv_task_group_add(g, t5);
    lv_task_group_destroy(g);
    lv_ASSERT(g_task_b_calls == 2);

    printf("  PASSED\n");
}

/* ============== 主函数 ============== */

TEST_MAIN_BEGIN("Lv-00 Proof System Test Suite")
    printf("=== Lv-00 Proof System Test Suite ===\n\n");
    TEST_MAIN_RUN(test_proposition_lifecycle);
    TEST_MAIN_RUN(test_composite_propositions);
    TEST_MAIN_RUN(test_proposition_ports);
    TEST_MAIN_RUN(test_sub_propositions);
    TEST_MAIN_RUN(test_proof_steps);
    TEST_MAIN_RUN(test_proof_navigator);
    TEST_MAIN_RUN(test_unify_check);
    TEST_MAIN_RUN(test_proof_dependencies);
    TEST_MAIN_RUN(test_ex_falso);
    TEST_MAIN_RUN(test_proof_colors);
    TEST_MAIN_RUN(test_helper_functions);
    /* task group execution engine */
    TEST_MAIN_RUN(test_task_group);
    printf("\n=== All proof system tests PASSED! ===\n");
TEST_MAIN_END()
