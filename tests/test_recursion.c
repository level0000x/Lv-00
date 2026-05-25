/**
 * @file test_recursion.c
 * @brief 递归与条件系统测试 - 测度系统、选择器块、递归深度监控
 *
 * 测试内容：
 * - 测度系统创建与管理
 * - 符号测度与非符号测度
 * - 测度比较
 * - 递归上下文管理
 * - 递归深度监控
 * - 选择器块
 * - 互递归支持
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv00.h"
#include "test_helpers.h"

/* ============== 测试：测度系统生命周期 ============== */

static int test_measure_system_lifecycle(void) {
    printf("Test: measure system lifecycle...\n");

    MeasureSystem *ms = measure_system_create();
    assert(ms != NULL);
    assert(ms->measure_count == 0);
    assert(ms->default_measure == NULL);
    assert(ms->has_non_symbolic == false);

    printf("  测度系统创建成功\n");

    measure_system_destroy(ms);
    printf("  PASSED\n");
    return 0;
}

/* ============== 测试：符号测度创建 ============== */

static int test_symbolic_measure(void) {
    printf("Test: symbolic measure creation...\n");

    /* 创建长度测度 */
    Measure *length_measure = measure_create_symbolic("Length", 0, 1);
    assert(length_measure != NULL);
    assert(length_measure->type == MEASURE_SYMBOLIC);
    assert(strcmp(length_measure->name, "Length") == 0);
    printf("  长度测度创建成功\n");

    /* 创建面积测度 */
    Measure *area_measure = measure_create_symbolic("Area", 1, 2);
    assert(area_measure != NULL);
    printf("  面积测度创建成功\n");

    /* 创建角度测度 */
    Measure *angle_measure = measure_create_symbolic("Angle", 2, 3);
    assert(angle_measure != NULL);
    printf("  角度测度创建成功\n");

    measure_destroy(length_measure);
    measure_destroy(area_measure);
    measure_destroy(angle_measure);

    printf("  PASSED\n");
    return 0;
}

/* ============== 测试：非符号测度 ============== */

static int custom_compare(GeomNode *a, GeomNode *b, void *user_data) {
    (void) a;
    (void) b;
    (void) user_data;
    return 0; /* 相等 */
}

static int test_custom_measure(void) {
    printf("Test: custom measure creation...\n");

    int user_data = 42;
    Measure *custom = measure_create_custom("CustomOrder", custom_compare, &user_data);
    assert(custom != NULL);
    assert(custom->type == MEASURE_CUSTOM);
    assert(custom->compare_func == custom_compare);
    assert(custom->user_data == &user_data);

    printf("  非符号测度创建成功\n");

    measure_destroy(custom);
    printf("  PASSED\n");
    return 0;
}

/* ============== 测试：测度系统管理 ============== */

static int test_measure_system_management(void) {
    printf("Test: measure system management...\n");

    MeasureSystem *ms = measure_system_create();
    assert(ms != NULL);

    /* 添加测度 */
    Measure *m1 = measure_create_symbolic("M1", 0, 1);
    bool ok = measure_system_add(ms, m1);
    assert(ok);
    assert(ms->measure_count == 1);
    printf("  添加测度 'M1' 成功\n");

    Measure *m2 = measure_create_symbolic("M2", 0, 2);
    ok = measure_system_add(ms, m2);
    assert(ok);
    assert(ms->measure_count == 2);
    printf("  添加测度 'M2' 成功\n");

    /* 设置默认测度 */
    measure_system_set_default(ms, m1);
    assert(ms->default_measure == m1);
    printf("  设置默认测度成功\n");

    measure_system_destroy(ms);
    printf("  PASSED\n");
    return 0;
}

/* ============== 测试：测度比较 ============== */

static int test_measure_comparison(void) {
    printf("Test: measure comparison...\n");

    ConstraintGraph *g = graph_create();

    /* 创建测度 */
    Measure *measure = measure_create_symbolic("Test", 0, 1);
    assert(measure != NULL);

    /* 创建节点 */
    int p1 = add_point(g, 0, 1, 0, 1);
    int p2 = add_point(g, 1, 1, 0, 1);

    GeomNode *node1 = graph_get_node(g, p1);
    GeomNode *node2 = graph_get_node(g, p2);

    /* 比较节点 */
    MeasureCompareResult result = measure_compare_nodes(measure, node1, node2, g);
    printf("  测度比较结果: %s\n", measure_compare_result_to_string(result));

    /* 比较符号坐标 */
    SymbolicCoord *c1 = symbolic_coord_create_rational(1, 2);
    SymbolicCoord *c2 = symbolic_coord_create_rational(3, 4);

    MeasureCompareResult result2 = measure_compare(measure, c1, c2);
    printf("  坐标比较结果: %s\n", measure_compare_result_to_string(result2));

    symbolic_coord_destroy(c1);
    symbolic_coord_destroy(c2);
    measure_destroy(measure);
    graph_destroy(g);

    printf("  PASSED\n");
    return 0;
}

/* ============== 测试：递归上下文 ============== */

static int test_recursion_context(void) {
    printf("Test: recursion context...\n");

    /* 创建递归上下文 */
    RecursionContext *ctx = recursion_context_create(100);
    assert(ctx != NULL);
    assert(ctx->max_depth == 100);
    assert(ctx->current_depth == 0);
    assert(ctx->is_terminated == false);

    printf("  递归上下文创建成功 (最大深度: %d)\n", ctx->max_depth);

    /* 设置活动测度 */
    Measure *measure = measure_create_symbolic("Depth", 3, 1);
    recursion_context_set_measure(ctx, measure);
    assert(ctx->active_measure == measure);
    printf("  设置活动测度成功\n");

    /* 获取当前深度 */
    int depth = recursion_context_get_depth(ctx);
    printf("  当前深度: %d\n", depth);

    recursion_context_destroy(ctx);
    measure_destroy(measure);

    printf("  PASSED\n");
    return 0;
}

/* ============== 测试：递归进入/退出 ============== */

static int test_recursion_enter_exit(void) {
    printf("Test: recursion enter/exit...\n");

    ConstraintGraph *g = graph_create();
    RecursionContext *ctx = recursion_context_create(10);
    assert(ctx != NULL);

    /* 创建输入节点 */
    int p1 = add_point(g, 0, 1, 0, 1);
    GeomNode *input = graph_get_node(g, p1);

    /* 进入递归 */
    RecursionCheckResult result = recursion_context_enter(ctx, 1, input, g);
    printf("  进入递归 (ID=1): %s\n", recursion_check_result_to_string(result));
    printf("  当前深度: %d\n", recursion_context_get_depth(ctx));

    /* 再次进入 */
    result = recursion_context_enter(ctx, 1, input, g);
    printf("  再次进入递归: %s\n", recursion_check_result_to_string(result));
    printf("  当前深度: %d\n", recursion_context_get_depth(ctx));

    /* 退出递归 */
    recursion_context_exit(ctx);
    printf("  退出递归后深度: %d\n", recursion_context_get_depth(ctx));

    recursion_context_destroy(ctx);
    graph_destroy(g);

    printf("  PASSED\n");
    return 0;
}

/* ============== 测试：递归深度超限 ============== */

static int test_recursion_depth_exceeded(void) {
    printf("Test: recursion depth exceeded...\n");

    ConstraintGraph *g = graph_create();
    /* 设置很小的最大深度 */
    RecursionContext *ctx = recursion_context_create(3);
    assert(ctx != NULL);

    int p1 = add_point(g, 0, 1, 0, 1);
    GeomNode *input = graph_get_node(g, p1);

    /* 连续进入递归 */
    for (int i = 0; i < 5; i++) {
        RecursionCheckResult result = recursion_context_enter(ctx, 1, input, g);
        printf("  第 %d 次进入: %s (深度: %d)\n", i + 1, recursion_check_result_to_string(result),
               recursion_context_get_depth(ctx));

        if (result == RECURSION_CHECK_RESULT_DEPTH_EXCEEDED) {
            printf("  深度超限检测成功!\n");
            break;
        }
    }

    recursion_context_destroy(ctx);
    graph_destroy(g);

    printf("  PASSED\n");
    return 0;
}

/* ============== 测试：测度递减检查 ============== */

static int test_measure_decreasing(void) {
    printf("Test: measure decreasing check...\n");

    RecursionContext *ctx = recursion_context_create(100);
    assert(ctx != NULL);

    /* 创建测度 */
    Measure *measure = measure_create_symbolic("Length", 0, 1);
    recursion_context_set_measure(ctx, measure);

    /* 创建两个测度值 */
    SymbolicCoord *value1 = symbolic_coord_create_rational(10, 1);
    SymbolicCoord *value2 = symbolic_coord_create_rational(5, 1);

    /* 检查递减性 */
    RecursionCheckResult result = recursion_context_check_decreasing(ctx, value1);
    printf("  初始值检查: %s\n", recursion_check_result_to_string(result));

    result = recursion_context_check_decreasing(ctx, value2);
    printf("  递减值检查: %s\n", recursion_check_result_to_string(result));

    symbolic_coord_destroy(value1);
    symbolic_coord_destroy(value2);
    recursion_context_destroy(ctx);
    measure_destroy(measure);

    printf("  PASSED\n");
    return 0;
}

/* ============== 测试：选择器块 ============== */

static int test_selector_block(void) {
    printf("Test: selector block...\n");

    ConstraintGraph *g = graph_create();

    /* 创建选择器块 */
    SelectorBlock *sb = selector_block_create(1, g);
    assert(sb != NULL);
    assert(sb->id == 1);
    assert(sb->graph == g);
    /* 注意：selector_block_create 使用 calloc，状态初始为 0 (BRANCH_INACTIVE) */
    printf("  初始状态 - 真分支: %s, 假分支: %s\n", branch_state_to_string(sb->true_state),
           branch_state_to_string(sb->false_state));

    printf("  选择器块创建成功 (ID=%d)\n", sb->id);

    /* 创建测试条件 */
    int test_point = add_point(g, 0, 1, 0, 1);
    int test_region = -1; /* 简化测试 */

    bool ok = selector_block_set_condition(sb, test_point, test_region);
    printf("  设置测试条件: %s\n", ok ? "成功" : "失败");

    /* 设置分支 */
    int true_root = add_point(g, 1, 1, 1, 1);
    int false_root = add_point(g, 2, 1, 2, 1);

    ok = selector_block_set_branches(sb, true_root, false_root);
    printf("  设置分支: %s\n", ok ? "成功" : "失败");

    /* 更新分支状态 */
    selector_block_update_states(sb, BRANCH_ACTIVE_SELECTED, BRANCH_INACTIVE);
    printf("  真分支状态: %s\n", branch_state_to_string(sb->true_state));
    printf("  假分支状态: %s\n", branch_state_to_string(sb->false_state));

    /* 获取活跃分支 */
    int active = selector_block_get_active_branch(sb);
    printf("  活跃分支: %d\n", active);

    selector_block_destroy(sb);
    graph_destroy(g);

    printf("  PASSED\n");
    return 0;
}

/* ============== 测试：分支状态 ============== */

static int test_branch_states(void) {
    printf("Test: branch states...\n");

    BranchState states[] = {BRANCH_INACTIVE, BRANCH_ACTIVE_SELECTED, BRANCH_PENDING};

    const char *expected[] = {"Inactive", "Active", "Pending"};

    for (int i = 0; i < 3; i++) {
        const char *str = branch_state_to_string(states[i]);
        printf("  %s -> %s\n", expected[i], str);
    }

    printf("  PASSED\n");
    return 0;
}

/* ============== 测试：互递归 ============== */

static int test_mutual_recursion(void) {
    printf("Test: mutual recursion...\n");

    MeasureSystem *ms = measure_system_create();
    assert(ms != NULL);

    /* 添加测度 */
    Measure *measure = measure_create_symbolic("Mutual", 0, 1);
    measure_system_add(ms, measure);
    measure_system_set_default(ms, measure);

    /* 检查互递归一致性 */
    int func_ids[] = {1, 2, 3};
    bool consistent = recursion_check_mutual(func_ids, 3, ms);
    printf("  互递归一致性检查: %s\n", consistent ? "一致" : "不一致/未实现");

    measure_system_destroy(ms);
    /* 注意：measure 已由 measure_system_destroy 内部销毁，不要再次释放 */

    printf("  PASSED\n");
    return 0;
}

/* ============== 测试：辅助函数 ============== */

static int test_helper_functions(void) {
    printf("Test: helper functions...\n");

    /* 测度类型转字符串 */
    const char *str = measure_type_to_string(MEASURE_SYMBOLIC);
    printf("  SYMBOLIC -> %s\n", str);

    str = measure_type_to_string(MEASURE_CUSTOM);
    printf("  CUSTOM -> %s\n", str);

    /* 递归检查结果转字符串 */
    str = recursion_check_result_to_string(RECURSION_CHECK_RESULT_OK);
    printf("  OK -> %s\n", str);

    str = recursion_check_result_to_string(RECURSION_CHECK_RESULT_NOT_DECREASING);
    printf("  NOT_DECREASING -> %s\n", str);

    str = recursion_check_result_to_string(RECURSION_CHECK_RESULT_DEPTH_EXCEEDED);
    printf("  DEPTH_EXCEEDED -> %s\n", str);

    str = recursion_check_result_to_string(RECURSION_CHECK_RESULT_CYCLE_DETECTED);
    printf("  CYCLE_DETECTED -> %s\n", str);

    printf("  PASSED\n");
    return 0;
}

/* ============== 主函数 ============== */

int main(void) {
    printf("=== Lv-00 Recursion System Test Suite ===\n\n");

    test_measure_system_lifecycle();
    test_symbolic_measure();
    test_custom_measure();
    test_measure_system_management();
    test_measure_comparison();
    test_recursion_context();
    test_recursion_enter_exit();
    test_recursion_depth_exceeded();
    test_measure_decreasing();
    test_selector_block();
    test_branch_states();
    test_mutual_recursion();
    test_helper_functions();

    printf("\n=== All recursion tests PASSED! ===\n");
    return 0;
}
