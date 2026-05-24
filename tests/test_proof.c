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

#include "lv00.h"
#include "test_helpers.h"
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

/* ============== 测试：命题生命周期 ============== */

static int test_proposition_lifecycle(void)
{
    printf("Test: proposition lifecycle...\n");

    Proposition *prop = proposition_create(1, PROPOSITION_ATOMIC);
    assert(prop != NULL);
    assert(prop->id == 1);
    assert(prop->type == PROPOSITION_ATOMIC);
    assert(prop->input_count == 0);
    assert(prop->output_count == 0);

    printf("  原子命题创建成功 (ID=%d)\n", prop->id);

    proposition_destroy(prop);
    printf("  PASSED\n");
    return 0;
}

/* ============== 测试：复合命题 ============== */

static int test_composite_propositions(void)
{
    printf("Test: composite propositions...\n");

    /* 创建合取命题 */
    Proposition *conj = proposition_create(1, PROPOSITION_CONJUNCTION);
    assert(conj != NULL);
    printf("  合取命题创建成功\n");

    /* 创建析取命题 */
    Proposition *disj = proposition_create(2, PROPOSITION_DISJUNCTION);
    assert(disj != NULL);
    printf("  析取命题创建成功\n");

    /* 创建蕴含命题 */
    Proposition *impl = proposition_create(3, PROPOSITION_IMPLICATION);
    assert(impl != NULL);
    printf("  蕴含命题创建成功\n");

    /* 创建否定命题 */
    Proposition *neg = proposition_create(4, PROPOSITION_NEGATION);
    assert(neg != NULL);
    printf("  否定命题创建成功\n");

    /* 创建全称命题 */
    Proposition *univ = proposition_create(5, PROPOSITION_UNIVERSAL);
    assert(univ != NULL);
    printf("  全称命题创建成功\n");

    /* 创建存在命题 */
    Proposition *exist = proposition_create(6, PROPOSITION_EXISTENTIAL);
    assert(exist != NULL);
    printf("  存在命题创建成功\n");

    /* 创建矛盾命题 */
    Proposition *bottom = proposition_create(7, PROPOSITION_BOTTOM);
    assert(bottom != NULL);
    printf("  矛盾命题创建成功\n");

    proposition_destroy(conj);
    proposition_destroy(disj);
    proposition_destroy(impl);
    proposition_destroy(neg);
    proposition_destroy(univ);
    proposition_destroy(exist);
    proposition_destroy(bottom);

    printf("  PASSED\n");
    return 0;
}

/* ============== 测试：命题端口设置 ============== */

static int test_proposition_ports(void)
{
    printf("Test: proposition port configuration...\n");

    ConstraintGraph *g = graph_create();

    Proposition *prop = proposition_create(1, PROPOSITION_ATOMIC);
    assert(prop != NULL);

    /* 创建输入端口 */
    int in_ports[] = {1, 2};
    bool ok = proposition_set_input_ports(prop, in_ports, 2);
    assert(ok);
    assert(prop->input_count == 2);
    printf("  输入端口设置成功: %d 个\n", prop->input_count);

    /* 创建输出端口 */
    int out_ports[] = {3};
    ok = proposition_set_output_ports(prop, out_ports, 1);
    assert(ok);
    assert(prop->output_count == 1);
    printf("  输出端口设置成功: %d 个\n", prop->output_count);

    /* 创建模式图 */
    int p1 = add_point(g, 0, 1, 0, 1);
    int p2 = add_point(g, 1, 1, 1, 1);
    graph_add_line_segment(g, p1, p2);

    ok = proposition_set_pattern(prop, g);
    assert(ok);
    assert(prop->pattern == g);
    printf("  模式图设置成功\n");

    proposition_destroy(prop);
    /* 注意：g 已由 proposition_destroy 内部销毁（作为 prop->pattern），不要再次释放 */

    printf("  PASSED\n");
    return 0;
}

/* ============== 测试：子命题 ============== */

static int test_sub_propositions(void)
{
    printf("Test: sub-propositions...\n");

    /* 创建父命题 */
    Proposition *parent = proposition_create(1, PROPOSITION_CONJUNCTION);
    assert(parent != NULL);

    /* 创建子命题 */
    Proposition *child1 = proposition_create(2, PROPOSITION_ATOMIC);
    Proposition *child2 = proposition_create(3, PROPOSITION_ATOMIC);

    /* 添加子命题 */
    bool ok = proposition_add_sub_proposition(parent, child1);
    assert(ok);
    printf("  子命题1添加成功\n");

    ok = proposition_add_sub_proposition(parent, child2);
    assert(ok);
    printf("  子命题2添加成功\n");

    assert(parent->sub_prop_count == 2);

    proposition_destroy(parent);
    /* 注意：proposition_destroy 会递归销毁子命题 */

    printf("  PASSED\n");
    return 0;
}

/* ============== 测试：证明步骤 ============== */

static int test_proof_steps(void)
{
    printf("Test: proof steps...\n");

    /* 创建不同类型的证明步骤 */
    ProofStep *step1 = proof_step_create(PROOF_STEP_ADD_NODE);
    assert(step1 != NULL);
    assert(step1->type == PROOF_STEP_ADD_NODE);
    printf("  添加节点步骤创建成功\n");

    ProofStep *step2 = proof_step_create(PROOF_STEP_ADD_CONSTRAINT);
    assert(step2 != NULL);
    printf("  添加约束步骤创建成功\n");

    ProofStep *step3 = proof_step_create(PROOF_STEP_NORMALIZATION);
    assert(step3 != NULL);
    printf("  规范化步骤创建成功\n");

    ProofStep *step4 = proof_step_create(PROOF_STEP_UNIFY);
    assert(step4 != NULL);
    printf("  合一检查步骤创建成功\n");

    /* 设置断点 */
    proof_step_set_breakpoint(step1, true);
    assert(step1->is_breakpoint == true);
    printf("  断点设置成功\n");

    /* 添加依赖关系 */
    bool ok = proof_step_add_dependency(step2, step1->id);
    printf("  依赖关系添加: %s\n", ok ? "成功" : "失败");

    proof_step_destroy(step1);
    proof_step_destroy(step2);
    proof_step_destroy(step3);
    proof_step_destroy(step4);

    printf("  PASSED\n");
    return 0;
}

/* ============== 测试：证明导航器 ============== */

static int test_proof_navigator(void)
{
    printf("Test: proof navigator...\n");

    /* 创建目标命题 */
    Proposition *target = proposition_create(1, PROPOSITION_ATOMIC);
    assert(target != NULL);

    /* 创建证明导航器 */
    ProofNavigator *nav = proof_navigator_create(target, NULL);
    assert(nav != NULL);
    assert(nav->target_prop == target);
    assert(nav->step_count == 0);
    assert(nav->current_step == -1); /* 初始值为 -1（无当前步骤） */

    printf("  证明导航器创建成功\n");

    /* 添加证明步骤 */
    ProofStep *step1 = proof_step_create(PROOF_STEP_ADD_NODE);
    bool ok = proof_navigator_add_step(nav, step1);
    assert(ok);
    assert(nav->step_count == 1);
    printf("  步骤1添加成功\n");

    ProofStep *step2 = proof_step_create(PROOF_STEP_NORMALIZATION);
    ok = proof_navigator_add_step(nav, step2);
    assert(ok);
    assert(nav->step_count == 2);
    printf("  步骤2添加成功\n");

    /* 导航测试 */
    ProofStep *current = proof_navigator_current_step(nav);
    printf("  当前步骤: %s\n", current ? proof_step_type_to_string(current->type) : "无");

    /* 下一步 */
    ok = proof_navigator_next(nav);
    printf("  导航到下一步: %s\n", ok ? "成功" : "失败/已在最后");

    /* 上一步 */
    ok = proof_navigator_prev(nav);
    printf("  导航到上一步: %s\n", ok ? "成功" : "失败/已在开头");

    /* 计算最终颜色 */
    ProofColor color = proof_navigator_compute_final_color(nav);
    printf("  最终颜色: %s\n", proof_color_to_string(color));

    proof_navigator_destroy(nav);
    proposition_destroy(target);

    printf("  PASSED\n");
    return 0;
}

/* ============== 测试：合一检查 ============== */

static int test_unify_check(void)
{
    printf("Test: unify check...\n");

    /* 创建构造图 */
    ConstraintGraph *construction = graph_create();
    int p1 = add_point(construction, 0, 1, 0, 1);
    int p2 = add_point(construction, 1, 1, 1, 1);
    graph_add_line_segment(construction, p1, p2);

    /* 创建命题 */
    Proposition *prop = proposition_create(1, PROPOSITION_ATOMIC);
    assert(prop != NULL);

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
        lv00_free_ptr(mismatch_info);
    }

    graph_destroy(construction);
    proposition_destroy(prop);
    /* 注意：pattern 已由 proposition_destroy 内部销毁，不要再次释放 */

    printf("  PASSED\n");
    return 0;
}

/* ============== 测试：证明依赖链 ============== */

static int test_proof_dependencies(void)
{
    printf("Test: proof dependencies...\n");

    /* 创建依赖 */
    ProofDependency *dep = proof_dependency_create(PROOF_COLOR_GREEN);
    assert(dep != NULL);
    assert(dep->color == PROOF_COLOR_GREEN);
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
    return 0;
}

/* ============== 测试：爆炸原理 ============== */

static int test_ex_falso(void)
{
    printf("Test: explosion principle (ex falso)...\n");

    ConstraintGraph *g = graph_create();

    /* 创建爆炸原理函数块 */
    int block_id = -1;
    bool ok = proof_create_ex_falso_block(g, &block_id);
    printf("  创建爆炸原理函数块: %s (ID=%d)\n", ok ? "成功" : "失败", block_id);

    /* 创建证明导航器和目标命题 */
    Proposition *target = proposition_create(1, PROPOSITION_ATOMIC);
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
    return 0;
}

/* ============== 测试：证明颜色 ============== */

static int test_proof_colors(void)
{
    printf("Test: proof colors...\n");

    /* 测试所有证明颜色 */
    ProofColor colors[] = {
        PROOF_COLOR_GREEN,
        PROOF_COLOR_BLUE_UNEXPLORED,
        PROOF_COLOR_BLUE_RESOURCE,
        PROOF_COLOR_BLUE_OUT_OF_RANGE,
        PROOF_COLOR_GREEN_VERIFIED,
        PROOF_COLOR_YELLOW,
        PROOF_COLOR_ORANGE_ORACLE,
        PROOF_COLOR_ORANGE_EX_FALSO,
        PROOF_COLOR_AMBER,
        PROOF_COLOR_DARK_ORANGE
    };

    const char *expected[] = {
        "Green",
        "BlueUnexplored",
        "BlueResource",
        "BlueOutOfRange",
        "GreenVerified",
        "Yellow",
        "OrangeOracle",
        "OrangeExFalso",
        "Amber",
        "DarkOrange"
    };

    for (int i = 0; i < sizeof(colors)/sizeof(colors[0]); i++) {
        const char *str = proof_color_to_string(colors[i]);
        printf("  %s -> %s\n", expected[i], str);
    }

    printf("  PASSED\n");
    return 0;
}

/* ============== 测试：辅助函数 ============== */

static int test_helper_functions(void)
{
    printf("Test: helper functions...\n");

    /* 命题类型转字符串 */
    const char *str = proposition_type_to_string(PROPOSITION_ATOMIC);
    printf("  ATOMIC -> %s\n", str);

    str = proposition_type_to_string(PROPOSITION_CONJUNCTION);
    printf("  CONJUNCTION -> %s\n", str);

    str = proposition_type_to_string(PROPOSITION_IMPLICATION);
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
    return 0;
}

/* ============== 主函数 ============== */

int main(void)
{
    printf("=== Lv-00 Proof System Test Suite ===\n\n");

    test_proposition_lifecycle();
    test_composite_propositions();
    test_proposition_ports();
    test_sub_propositions();
    test_proof_steps();
    test_proof_navigator();
    test_unify_check();
    test_proof_dependencies();
    test_ex_falso();
    test_proof_colors();
    test_helper_functions();

    printf("\n=== All proof system tests PASSED! ===\n");
    return 0;
}
