/**
 * @file recursion_demo.c
 * @brief 递归演示 —— 展示递归函数定义、测度系统与深度限制
 *
 * 本示例演示 Lv-00 递归与条件系统的核心功能：
 * 1. 创建测度系统并定义符号测度
 * 2. 定义递归上下文并设置深度限制
 * 3. 模拟递归函数调用（以几何构造的递归分解为例）
 * 4. 使用全局递归深度保护（熔断器机制）
 * 5. 演示选择器块（条件分支）
 * 6. 打印递归执行结果
 *
 * Lv-00 的递归系统基于良基归纳原理：
 * - 每个递归调用必须关联一个严格递减的测度
 * - 测度可以是符号的（几何度量）或非符号的（自定义序）
 * - 全局熔断器在深度超过 LV00_MAX_RECURSION_DEPTH (128) 时自动终止
 *
 * 编译方式：
 *   gcc -o recursion_demo recursion_demo.c -llv00 -lgmp
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv00.h"

/* ============================================================
 * 辅助函数：添加一个有理数坐标的点
 * ============================================================ */
static int add_point(ConstraintGraph *g, int64_t xn, uint64_t xd,
                     int64_t yn, uint64_t yd) {
    SymbolicCoord *cx = symbolic_coord_create_rational(xn, xd);
    SymbolicCoord *cy = symbolic_coord_create_rational(yn, yd);
    SymbolicCoord *coords[] = {cx, cy};
    graph_add_point(g, coords, 2);
    return g->next_node_id - 1;
}

/* ============================================================
 * 演示1：测度系统
 *
 * 测度系统是递归终止性证明的核心。
 * 每个递归函数必须关联一个良基测度，确保递归调用
 * 的测度值严格递减，从而保证终止。
 *
 * Lv-00 支持多种测度类型：
 * - MEASURE_KIND_LENGTH: 线段长度
 * - MEASURE_KIND_AREA:   区域面积
 * - MEASURE_KIND_ANGLE:  角度
 * - MEASURE_KIND_DEPTH:  嵌套深度
 * - MEASURE_KIND_CUSTOM: 自定义测度
 * ============================================================ */
static void demo_measure_system(void) {
    printf("\n--- 测度系统演示 ");
    for (int i = 12; i < 50; i++) printf("-");
    printf("\n");

    /* 创建测度系统 */
    MeasureSystem *ms = measure_system_create();
    if (!ms) {
        fprintf(stderr, "  测度系统创建失败\n");
        return;
    }
    printf("  [OK] 测度系统创建成功\n");

    /* 创建符号测度：线段长度测度
     * 用于递归分解线段时的终止性证明 */
    Measure *length_measure = measure_create_symbolic(
        "segment_length", MEASURE_KIND_LENGTH, 0);
    if (length_measure) {
        length_measure->is_well_founded = true;
        measure_system_add(ms, length_measure);
        measure_system_set_default(ms, length_measure);
        printf("  [OK] 创建线段长度测度 (segment_length)\n");
        printf("       类型: %s\n", measure_type_to_string(length_measure->type));
        printf("       良基: %s\n", length_measure->is_well_founded ? "是" : "否");
    }

    /* 创建符号测度：嵌套深度测度
     * 用于递归构造嵌套结构时的终止性证明 */
    Measure *depth_measure = measure_create_symbolic(
        "nesting_depth", MEASURE_KIND_DEPTH, 0);
    if (depth_measure) {
        depth_measure->is_well_founded = true;
        measure_system_add(ms, depth_measure);
        printf("  [OK] 创建嵌套深度测度 (nesting_depth)\n");
        printf("       类型: %s\n", measure_type_to_string(depth_measure->type));
    }

    /* 创建非符号测度：自定义比较函数
     * 用于无法用符号坐标表达的抽象序结构 */
    Measure *custom_measure = measure_create_custom(
        "custom_order",
        /* 比较函数：简单比较节点 ID */
        NULL,
        NULL);
    if (custom_measure) {
        custom_measure->is_well_founded = true;
        measure_system_add(ms, custom_measure);
        printf("  [OK] 创建自定义测度 (custom_order)\n");
        printf("       类型: %s\n", measure_type_to_string(custom_measure->type));
    }

    printf("  测度系统中共有 %d 个测度\n", ms->measure_count);

    /* 清理 */
    measure_system_destroy(ms);
}

/* ============================================================
 * 演示2：递归上下文与递归调用模拟
 *
 * 递归上下文管理递归调用的状态：
 * - 当前深度
 * - 最大深度限制
 * - 活动测度
 * - 调用栈
 * - 测度值历史
 *
 * 我们模拟一个递归分解线段的场景：
 * 给定一条线段，递归地取其中点，直到线段足够短。
 * ============================================================ */
static void demo_recursion_context(void) {
    printf("\n--- 递归上下文演示 ");
    for (int i = 14; i < 50; i++) printf("-");
    printf("\n");

    /* 创建约束图 */
    ConstraintGraph *g = graph_create();

    /* 创建线段的两个端点 */
    int p1 = add_point(g, 0, 1, 0, 1);   /* (0, 0) */
    int p2 = add_point(g, 8, 1, 0, 1);   /* (8, 0) */
    graph_add_line_segment(g, p1, p2);
    int seg = g->next_node_id - 1;

    printf("  创建线段: 端点(%d, %d), 线段ID=%d\n", p1, p2, seg);

    /* 创建递归上下文，设置最大深度为 10 */
    RecursionContext *ctx = recursion_context_create(10);
    if (!ctx) {
        fprintf(stderr, "  递归上下文创建失败\n");
        graph_destroy(g);
        return;
    }
    printf("  [OK] 递归上下文创建成功 (最大深度=%d)\n", ctx->max_depth);

    /* 创建测度并关联到上下文 */
    Measure *depth_measure = measure_create_symbolic(
        "recursion_depth", MEASURE_KIND_DEPTH, 0);
    if (depth_measure) {
        depth_measure->is_well_founded = true;
        recursion_context_set_measure(ctx, depth_measure);
        printf("  [OK] 设置活动测度: %s\n", depth_measure->name);
    }

    /* 模拟递归调用序列 */
    printf("\n  模拟递归调用序列:\n");

    for (int i = 0; i < 5; i++) {
        /* 获取当前节点用于测度计算 */
        GeomNode *node = graph_get_node(g, p1);
        if (!node) break;

        /* 进入递归调用 */
        RecursionCheckResult result = recursion_context_enter(
            ctx, i + 1, node, g);

        printf("    调用 %d: %s (当前深度=%d)\n",
               i + 1,
               recursion_check_result_to_string(result),
               recursion_context_get_depth(ctx));

        if (result != RECURSION_OK) {
            printf("    递归终止: %s\n",
                   recursion_check_result_to_string(result));
            break;
        }
    }

    printf("  最终递归深度: %d\n", recursion_context_get_depth(ctx));

    /* 检查测度递减性 */
    printf("\n  检查测度递减性:\n");
    SymbolicCoord *test_value = symbolic_coord_create_rational(3, 1);
    RecursionCheckResult dec_result =
        recursion_context_check_decreasing(ctx, test_value);
    printf("    测度递减检查: %s\n",
           recursion_check_result_to_string(dec_result));

    /* 重置上下文 */
    recursion_context_reset(ctx);
    printf("  上下文已重置, 深度=%d\n", recursion_context_get_depth(ctx));

    /* 清理 */
    if (depth_measure) measure_destroy(depth_measure);
    recursion_context_destroy(ctx);
    graph_destroy(g);
}

/* ============================================================
 * 演示3：全局递归深度保护（熔断器）
 *
 * Lv-00 提供全局递归深度保护机制，防止无限递归导致栈溢出。
 * - lv00_recursion_enter(): 进入递归，深度+1
 * - lv00_recursion_leave(): 退出递归，深度-1
 * - LV00_MAX_RECURSION_DEPTH (128): 全局硬限制
 * - 熔断器触发后，所有后续递归调用都会被拒绝
 * ============================================================ */
static void demo_global_recursion_guard(void) {
    printf("\n--- 全局递归深度保护（熔断器） ");
    for (int i = 22; i < 50; i++) printf("-");
    printf("\n");

    /* 确保初始状态干净 */
    lv00_recursion_reset();
    printf("  初始深度: %d\n", lv00_recursion_get_depth());
    printf("  熔断器状态: %s\n",
           lv00_recursion_circuit_breaker_triggered() ? "已触发" : "未触发");

    /* 模拟递归调用直到接近上限 */
    printf("\n  模拟递归调用:\n");
    int max_test_depth = 5;  /* 测试用，不真的到128 */

    for (int i = 0; i < max_test_depth; i++) {
        bool ok = lv00_recursion_enter();
        printf("    深度 %d: %s\n",
               lv00_recursion_get_depth(),
               ok ? "进入成功" : "被拒绝（熔断器触发）");
    }

    printf("  当前深度: %d\n", lv00_recursion_get_depth());

    /* 退出所有递归 */
    for (int i = 0; i < max_test_depth; i++) {
        lv00_recursion_leave();
    }
    printf("  退出后深度: %d\n", lv00_recursion_get_depth());
    printf("  熔断器状态: %s\n",
           lv00_recursion_circuit_breaker_triggered() ? "已触发" : "未触发");

    /* 重置 */
    lv00_recursion_reset();
    printf("  重置后深度: %d\n", lv00_recursion_get_depth());
}

/* ============================================================
 * 演示4：深度超限回调
 *
 * 递归上下文支持注册深度超限回调函数。
 * 当递归深度接近上限时，回调函数被调用，
 * 可以选择继续执行或停止递归。
 * ============================================================ */

/* 回调函数：当递归深度超限时被调用 */
static RecursionAction depth_callback(int current_depth, int max_depth, void *user_data) {
    (void)user_data;
    printf("    [回调] 当前深度=%d, 最大深度=%d\n", current_depth, max_depth);

    /* 如果深度超过最大值的80%，建议停止 */
    if (current_depth >= max_depth * 8 / 10) {
        printf("    [回调] 建议停止递归（深度已达上限的80%%）\n");
        return RECURSION_ACTION_STOP;
    }
    return RECURSION_ACTION_CONTINUE;
}

static void demo_depth_callback(void) {
    printf("\n--- 深度超限回调演示 ");
    for (int i = 16; i < 50; i++) printf("-");
    printf("\n");

    /* 创建递归上下文，设置较小的最大深度 */
    RecursionContext *ctx = recursion_context_create(5);
    if (!ctx) {
        fprintf(stderr, "  递归上下文创建失败\n");
        return;
    }
    printf("  最大深度: %d\n", ctx->max_depth);

    /* 注册深度超限回调 */
    recursion_context_set_depth_callback(ctx, depth_callback, NULL);
    printf("  [OK] 深度超限回调已注册\n");

    /* 创建约束图和测度 */
    ConstraintGraph *g = graph_create();
    int p = add_point(g, 1, 1, 1, 1);
    GeomNode *node = graph_get_node(g, p);

    Measure *m = measure_create_symbolic("test", MEASURE_KIND_DEPTH, 0);
    if (m) {
        m->is_well_founded = true;
        recursion_context_set_measure(ctx, m);
    }

    /* 模拟递归调用 */
    printf("\n  模拟递归调用:\n");
    for (int i = 0; i < 8; i++) {
        RecursionCheckResult result = recursion_context_enter(
            ctx, i + 1, node, g);
        printf("    调用 %d: %s (深度=%d)\n",
               i + 1,
               recursion_check_result_to_string(result),
               recursion_context_get_depth(ctx));

        if (ctx->is_terminated) {
            printf("    递归已终止: %s\n",
                   ctx->termination_reason ? ctx->termination_reason : "未知原因");
            break;
        }
    }

    /* 清理 */
    if (m) measure_destroy(m);
    recursion_context_destroy(ctx);
    graph_destroy(g);
}

/* ============================================================
 * 演示5：选择器块（条件分支）
 *
 * 选择器块是 Lv-00 的条件/分支机制，类似于编程语言中的 if-else。
 * 它根据测试条件选择执行真分支或假分支。
 * ============================================================ */
static void demo_selector_block(void) {
    printf("\n--- 选择器块（条件分支）演示 ");
    for (int i = 18; i < 50; i++) printf("-");
    printf("\n");

    /* 创建约束图 */
    ConstraintGraph *g = graph_create();

    /* 创建测试点和测试区域 */
    int test_point = add_point(g, 1, 1, 1, 1);
    int region_nodes[] = {0, 1, 2};
    graph_add_region(g, region_nodes, 3);
    int test_region = g->next_node_id - 1;

    /* 创建选择器块 */
    SelectorBlock *sb = selector_block_create(1, g);
    if (!sb) {
        fprintf(stderr, "  选择器块创建失败\n");
        graph_destroy(g);
        return;
    }
    printf("  [OK] 选择器块创建成功 (ID=%d)\n", sb->id);

    /* 设置测试条件：检查点是否在区域内 */
    bool ok = selector_block_set_condition(sb, test_point, test_region);
    printf("  设置测试条件: %s\n", ok ? "成功" : "失败");

    /* 设置分支 */
    int true_root = add_point(g, 10, 1, 10, 1);
    int false_root = add_point(g, 20, 1, 20, 1);
    ok = selector_block_set_branches(sb, true_root, false_root);
    printf("  设置分支: %s\n", ok ? "成功" : "失败");

    /* 设置分支子图节点 */
    int true_ids[] = {true_root};
    int false_ids[] = {false_root};
    selector_block_set_branch_nodes(sb, true_ids, 1, false_ids, 1);

    /* 更新分支状态 */
    selector_block_update_states(sb, BRANCH_ACTIVE_SELECTED, BRANCH_SHADOWED);
    printf("  真分支状态: %s\n", branch_state_to_string(sb->true_state));
    printf("  假分支状态: %s\n", branch_state_to_string(sb->false_state));

    /* 获取活跃分支 */
    int active = selector_block_get_active_branch(sb);
    printf("  当前活跃分支: %s\n", active >= 0 ? "真分支" : "假分支");

    /* 验证分支互斥性 */
    bool exclusive = selector_block_validate_branches(sb);
    printf("  分支互斥性: %s\n", exclusive ? "互斥" : "不互斥");

    /* 统计分支节点数 */
    int true_count = 0, false_count = 0;
    selector_block_count_branch_nodes(sb, &true_count, &false_count);
    printf("  真分支节点数: %d\n", true_count);
    printf("  假分支节点数: %d\n", false_count);

    /* 清理 */
    selector_block_destroy(sb);
    graph_destroy(g);
}

/* ============================================================
 * 主函数
 * ============================================================ */
int main(void) {
    printf("========================================\n");
    printf("  Lv-00 递归系统演示\n");
    printf("  测度系统、递归深度与条件分支\n");
    printf("========================================\n");

    /* 初始化系统 */
    printf("\n[初始化] Lv-00 系统版本: %s\n", lv00_get_version_string());
    printf("[初始化] 全局递归深度上限: %d\n", LV00_MAX_RECURSION_DEPTH);

    /* 依次运行各演示 */
    demo_measure_system();
    demo_recursion_context();
    demo_global_recursion_guard();
    demo_depth_callback();
    demo_selector_block();

    printf("\n========================================\n");
    printf("  示例完成！\n");
    printf("========================================\n");
    return 0;
}
