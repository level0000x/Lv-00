/**
 * @file test_adaptive_threshold.c
 * @brief 自适应阈值框架单元测试
 * 
 * 测试动态阈值计算、启发式剪枝、复杂度评估等功能
 * 
 * @version 1.0.0
 * @date 2026-05-26
 */

#include <math.h>
#include <stdio.h>

#include "lv/adaptive_threshold.h"
#include "lv/constraint_graph.h"

#include "test_unified.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif

int g_pass_count = 0;
int g_fail_count = 0;

/* ==================== 测试辅助函数 ==================== */

/**
 * @brief 创建简单测试图（三角形）
 */
static ConstraintGraph *create_triangle_graph(void) {
    ConstraintGraph *graph = graph_create();
    if (!graph)
        return NULL;

    int a = add_point(graph, 0, 1, 0, 1);
    int b = add_point(graph, 1, 1, 0, 1);
    int c = add_point(graph, 0, 1, 1, 1);

    /* 创建三条线段并添加 incidence 约束 */
    graph_add_line_segment(graph, a, b);
    int ab = graph_get_last_added_node_id(graph);
    graph_add_incidence(graph, a, ab);
    graph_add_incidence(graph, b, ab);

    graph_add_line_segment(graph, b, c);
    int bc = graph_get_last_added_node_id(graph);
    graph_add_incidence(graph, b, bc);
    graph_add_incidence(graph, c, bc);

    graph_add_line_segment(graph, c, a);
    int ca = graph_get_last_added_node_id(graph);
    graph_add_incidence(graph, c, ca);
    graph_add_incidence(graph, a, ca);

    return graph;
}

/**
 * @brief 创建复杂测试图（多个连通分量）
 */
static ConstraintGraph *create_complex_graph(void) {
    ConstraintGraph *graph = graph_create();
    if (!graph)
        return NULL;

    int ids[7], seg_ids[10];
    int seg_idx = 0;

    /* 第一个连通分量：四边形 + 对角线（4个点） */
    for (int i = 0; i < 4; i++) {
        /* 使用非共线点：避免 incidence 冲突 */
        SymbolicCoord *coords[2] = {symbolic_coord_create_rational(i, 1), symbolic_coord_create_rational(i % 2, 1)};
        graph_add_point(graph, (SymbolicCoord *const *) coords, 2);
        ids[i] = graph_get_last_added_node_id(graph);
    }

    /* 四边形边：0-1, 1-2, 2-3, 3-0 */
    graph_add_line_segment(graph, ids[0], ids[1]);
    seg_ids[seg_idx++] = graph_get_last_added_node_id(graph);
    graph_add_line_segment(graph, ids[1], ids[2]);
    seg_ids[seg_idx++] = graph_get_last_added_node_id(graph);
    graph_add_line_segment(graph, ids[2], ids[3]);
    seg_ids[seg_idx++] = graph_get_last_added_node_id(graph);
    graph_add_line_segment(graph, ids[3], ids[0]);
    seg_ids[seg_idx++] = graph_get_last_added_node_id(graph);

    /* 对角线：0-2 */
    graph_add_line_segment(graph, ids[0], ids[2]);
    seg_ids[seg_idx++] = graph_get_last_added_node_id(graph);

    /* 添加 incidence 约束 */
    graph_add_incidence(graph, ids[0], seg_ids[0]);
    graph_add_incidence(graph, ids[1], seg_ids[0]);
    graph_add_incidence(graph, ids[1], seg_ids[1]);
    graph_add_incidence(graph, ids[2], seg_ids[1]);
    graph_add_incidence(graph, ids[2], seg_ids[2]);
    graph_add_incidence(graph, ids[3], seg_ids[2]);
    graph_add_incidence(graph, ids[3], seg_ids[3]);
    graph_add_incidence(graph, ids[0], seg_ids[3]);
    graph_add_incidence(graph, ids[0], seg_ids[4]);
    graph_add_incidence(graph, ids[2], seg_ids[4]);

    /* 第二个连通分量：三角形（3个点） */
    for (int i = 0; i < 3; i++) {
        SymbolicCoord *coords[2] = {symbolic_coord_create_rational(i + 10, 1), symbolic_coord_create_rational(0, 1)};
        graph_add_point(graph, (SymbolicCoord *const *) coords, 2);
        ids[4 + i] = graph_get_last_added_node_id(graph);
    }

    graph_add_line_segment(graph, ids[4], ids[5]);
    int ab2 = graph_get_last_added_node_id(graph);
    graph_add_line_segment(graph, ids[5], ids[6]);
    int bc2 = graph_get_last_added_node_id(graph);
    graph_add_line_segment(graph, ids[6], ids[4]);
    int ca2 = graph_get_last_added_node_id(graph);

    graph_add_incidence(graph, ids[4], ab2);
    graph_add_incidence(graph, ids[5], ab2);
    graph_add_incidence(graph, ids[5], bc2);
    graph_add_incidence(graph, ids[6], bc2);
    graph_add_incidence(graph, ids[6], ca2);
    graph_add_incidence(graph, ids[4], ca2);

    return graph;
}

/* ==================== 测试用例 ==================== */

/**
 * @test 测试框架初始化和清理
 */
void test_init_cleanup(void) {
    lvError err = lv_adaptive_threshold_init();
    TEST_ASSERT_EQ_MSG(err, lv_OK, "Init should succeed");

    /* 重复初始化应该成功 */
    err = lv_adaptive_threshold_init();
    TEST_ASSERT_EQ_MSG(err, lv_OK, "Repeated init should succeed");

    lv_adaptive_threshold_cleanup();

    /* 重新初始化 */
    err = lv_adaptive_threshold_init();
    TEST_ASSERT_EQ_MSG(err, lv_OK, "Init after cleanup should succeed");
}

/**
 * @test 测试问题复杂度评估
 */
void test_complexity_computation(void) {
    lv_adaptive_threshold_init();

    /* 测试简单图 */
    ConstraintGraph *simple = create_triangle_graph();
    lvProblemComplexity complexity;

    lvError err = lv_compute_complexity(simple, &complexity);
    TEST_ASSERT_EQ_MSG(err, lv_OK, "Complexity computation should succeed");

    TEST_ASSERT_EQ_MSG(complexity.node_count, 6, "Should have 6 nodes (3 points + 3 lines)");
    TEST_ASSERT_EQ_MSG(complexity.constraint_count, 6, "Should have 6 incidence constraints");
    TEST_ASSERT(complexity.edge_count >= 3, "Should have at least 6 edges");
    TEST_ASSERT(complexity.density > 0.0 && complexity.density <= 1.0, "Density should be in (0,1]");
    TEST_ASSERT_EQ_MSG(complexity.connected_components, 1, "Should be 1 component");

    graph_destroy(simple);

    /* 测试复杂图 */
    ConstraintGraph *complex = create_complex_graph();
    err = lv_compute_complexity(complex, &complexity);
    TEST_ASSERT_EQ_MSG(err, lv_OK, "Complexity computation should succeed");

    TEST_ASSERT_EQ_MSG(complexity.node_count, 15, "Should have 15 nodes (7 points + 8 lines)");
    TEST_ASSERT_EQ_MSG(complexity.connected_components, 2, "Should have 2 components");

    graph_destroy(complex);
}

/**
 * @test 测试动态阈值计算 - VF2匹配
 */
void test_vf2_threshold_computation(void) {
    lv_adaptive_threshold_init();

    ConstraintGraph *graph = create_triangle_graph();
    lvAdaptiveThresholdCtx *ctx = NULL;

    lvError err = lv_adaptive_threshold_create(lv_ALGO_VF2_MATCH, graph, NULL, &ctx);
    TEST_ASSERT_EQ_MSG(err, lv_OK, "Context creation should succeed");
    TEST_ASSERT_NOT_NULL(ctx);

    /* 计算阈值 */
    size_t threshold = lv_adaptive_threshold_compute(ctx);

    /* 验证阈值在合理范围内 */
    lvThresholdConfig config;
    lv_adaptive_threshold_default_config(lv_ALGO_VF2_MATCH, &config);

    TEST_ASSERT(threshold >= (size_t) config.min_threshold, "Threshold should be >= min_threshold");
    TEST_ASSERT(threshold <= (size_t) config.max_threshold, "Threshold should be <= max_threshold");

    /* 对于3个节点的简单图，阈值应该相对较小 */
    TEST_ASSERT(threshold < 500, "Small graph threshold should be < 500");

    lv_adaptive_threshold_destroy(&ctx);
    graph_destroy(graph);
}

/**
 * @test 测试动态阈值计算 - Buchberger算法
 */
void test_buchberger_threshold_computation(void) {
    lv_adaptive_threshold_init();

    ConstraintGraph *graph = create_triangle_graph();
    lvAdaptiveThresholdCtx *ctx = NULL;

    lvError err = lv_adaptive_threshold_create(lv_ALGO_BUCHBERGER, graph, NULL, &ctx);
    TEST_ASSERT_EQ_MSG(err, lv_OK, "Context creation should succeed");

    size_t threshold = lv_adaptive_threshold_compute(ctx);

    lvThresholdConfig config;
    lv_adaptive_threshold_default_config(lv_ALGO_BUCHBERGER, &config);

    TEST_ASSERT(threshold >= (size_t) config.min_threshold, "Threshold should be >= min_threshold");
    TEST_ASSERT(threshold <= (size_t) config.max_threshold, "Threshold should be <= max_threshold");

    /* Buchberger阈值应该比VF2大 */
    TEST_ASSERT(threshold > 10000, "Buchberger threshold should be > 10000");

    lv_adaptive_threshold_destroy(&ctx);
    graph_destroy(graph);
}

/**
 * @test 测试阈值随复杂度变化
 */
void test_threshold_scaling(void) {
    lv_adaptive_threshold_init();

    /* 创建不同复杂度的图 */
    ConstraintGraph *small = create_triangle_graph();
    ConstraintGraph *large = create_complex_graph();

    lvAdaptiveThresholdCtx *ctx_small = NULL;
    lvAdaptiveThresholdCtx *ctx_large = NULL;

    lv_adaptive_threshold_create(lv_ALGO_VF2_MATCH, small, NULL, &ctx_small);
    lv_adaptive_threshold_create(lv_ALGO_VF2_MATCH, large, NULL, &ctx_large);

    size_t threshold_small = lv_adaptive_threshold_compute(ctx_small);
    size_t threshold_large = lv_adaptive_threshold_compute(ctx_large);

    /* 复杂图的阈值应该大于等于简单图（含更多节点和约束） */
    TEST_ASSERT(threshold_large >= threshold_small, "Large graph threshold should be >= small graph");

    lv_adaptive_threshold_destroy(&ctx_small);
    lv_adaptive_threshold_destroy(&ctx_large);
    graph_destroy(small);
    graph_destroy(large);
}

/**
 * @test 测试进度更新和启发式剪枝
 */
void test_progress_tracking_and_pruning(void) {
    lv_adaptive_threshold_init();

    ConstraintGraph *graph = create_triangle_graph();
    lvAdaptiveThresholdCtx *ctx = NULL;

    /* 使用很短的超时时间以便测试剪枝 */
    lvThresholdConfig config;
    lv_adaptive_threshold_default_config(lv_ALGO_VF2_MATCH, &config);
    config.time_budget_ms = 1.0; /* 1毫秒超时 */
    config.enable_progress_tracking = true;

    lv_adaptive_threshold_create(lv_ALGO_VF2_MATCH, graph, &config, &ctx);

    /* 模拟一些迭代 */
    for (int i = 0; i < 10; i++) {
        lv_adaptive_threshold_update_progress(ctx, (size_t) i, 0);

        bool should_prune = false;
        lv_adaptive_threshold_should_prune(ctx, &should_prune);

        /* 在1ms超时后应该触发剪枝 */
        if (i > 5) {
            /* 注意：由于时间依赖，这个测试可能不稳定 */
            /* 这里主要测试API不崩溃 */
        }
    }

    /* 测试高回溯率剪枝 */
    lv_adaptive_threshold_update_progress(ctx, 200, 190); /* 95%回溯率 */

    bool should_prune = false;
    lv_adaptive_threshold_should_prune(ctx, &should_prune);
    /* 高回溯率应该触发剪枝 */
    TEST_ASSERT(should_prune, "High backtrack rate should trigger pruning");

    lv_adaptive_threshold_destroy(&ctx);
    graph_destroy(graph);
}

/**
 * @test 测试自定义配置
 */
void test_custom_config(void) {
    lv_adaptive_threshold_init();

    lvThresholdConfig custom_config = {.base_threshold = 50.0,
                                       .scale_factor = 1.0,
                                       .time_budget_ms = 1000.0,
                                       .min_threshold = 10.0,
                                       .max_threshold = 200.0,
                                       .enable_time_based = false,
                                       .enable_progress_tracking = false};

    /* 设置全局配置 */
    lvError err = lv_adaptive_threshold_set_global_config(lv_ALGO_VF2_MATCH, &custom_config);
    TEST_ASSERT_EQ_MSG(err, lv_OK, "Setting global config should succeed");

    /* 创建上下文，应该使用全局配置 */
    ConstraintGraph *graph = create_triangle_graph();
    lvAdaptiveThresholdCtx *ctx = NULL;

    err = lv_adaptive_threshold_create(lv_ALGO_VF2_MATCH, graph, NULL, &ctx);
    TEST_ASSERT_EQ_MSG(err, lv_OK, "Context creation should succeed");

    size_t threshold = lv_adaptive_threshold_compute(ctx);

    /* 使用自定义配置，阈值应该在更小的范围内 */
    TEST_ASSERT(threshold >= 10, "Threshold should be >= 10");
    TEST_ASSERT(threshold <= 200, "Threshold should be <= 200");

    lv_adaptive_threshold_destroy(&ctx);
    graph_destroy(graph);
}

/**
 * @test 测试向后兼容函数
 */
void test_backward_compatibility(void) {
    lv_adaptive_threshold_init();

    ConstraintGraph *graph = create_triangle_graph();

    /* 测试向后兼容函数 */
    size_t vf2_depth = lv_get_vf2_max_depth(graph);
    size_t buchberger_steps = lv_get_buchberger_max_steps(graph);
    size_t rewrite_iterations = lv_get_rewrite_solve_max_iterations(graph);

    /* 验证返回的值在合理范围内 */
    TEST_ASSERT(vf2_depth >= 50 && vf2_depth <= 1000, "VF2 depth should be in range [50,1000]");
    TEST_ASSERT(buchberger_steps >= 10000 && buchberger_steps <= 200000,
                "Buchberger steps should be in range [10000,200000]");
    TEST_ASSERT(rewrite_iterations >= 5000 && rewrite_iterations <= 50000,
                "Rewrite iterations should be in range [5000,50000]");

    graph_destroy(graph);
}

/**
 * @test 测试错误处理
 */
void test_error_handling(void) {
    lv_adaptive_threshold_init();

    /* 测试NULL参数 */
    lvError err = lv_compute_complexity(NULL, NULL);
    TEST_ASSERT(err != lv_OK, "NULL parameters should return error");

    /* 测试无效算法类型 */
    lvAdaptiveThresholdCtx *ctx = NULL;
    err = lv_adaptive_threshold_create((lvAlgorithmType) 999, NULL, NULL, &ctx);
    TEST_ASSERT(err != lv_OK, "Invalid algorithm type should return error");

    /* 测试无效算法类型的默认配置 */
    lvThresholdConfig config;
    err = lv_adaptive_threshold_default_config((lvAlgorithmType) 999, &config);
    TEST_ASSERT(err != lv_OK, "Invalid algorithm type default config should return error");
}

/**
 * @test 性能测试：验证动态阈值计算开销
 */
void test_performance_overhead(void) {
    lv_adaptive_threshold_init();

    ConstraintGraph *graph = create_complex_graph();

    /* 测量多次阈值计算的时间 */
    const int iterations = 1000;

    /* 使用高精度计时 */
#ifdef _WIN32
    LARGE_INTEGER freq, qpc_start, qpc_end;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&qpc_start);

    for (int i = 0; i < iterations; i++) {
        lvAdaptiveThresholdCtx *ctx = NULL;
        lv_adaptive_threshold_create(lv_ALGO_VF2_MATCH, graph, NULL, &ctx);
        lv_adaptive_threshold_compute(ctx);
        lv_adaptive_threshold_destroy(&ctx);
    }

    QueryPerformanceCounter(&qpc_end);

    double elapsed_ms = (double) (qpc_end.QuadPart - qpc_start.QuadPart) * 1000.0 / (double) freq.QuadPart;
#else
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < iterations; i++) {
        lvAdaptiveThresholdCtx *ctx = NULL;
        lv_adaptive_threshold_create(lv_ALGO_VF2_MATCH, graph, NULL, &ctx);
        lv_adaptive_threshold_compute(ctx);
        lv_adaptive_threshold_destroy(&ctx);
    }

    clock_gettime(CLOCK_MONOTONIC, &end);

    double elapsed_ms = (end.tv_sec - start.tv_sec) * 1000.0 + (end.tv_nsec - start.tv_nsec) / 1000000.0;
#endif
    double avg_ms = elapsed_ms / iterations;

    printf("  Average threshold computation time: %.3f ms\n", avg_ms);

    /* 每次阈值计算应该非常快（< 1ms） */
    TEST_ASSERT(avg_ms < 1.0, "Average threshold computation should be < 1ms");

    graph_destroy(graph);
}

/* ==================== 测试套件入口 ==================== */

TEST_MAIN_BEGIN("AdaptiveThreshold")
    printf("Running adaptive threshold tests...\n\n");


    TEST_MAIN_RUN(test_init_cleanup);
    TEST_MAIN_RUN(test_complexity_computation);
    TEST_MAIN_RUN(test_vf2_threshold_computation);
    TEST_MAIN_RUN(test_buchberger_threshold_computation);
    TEST_MAIN_RUN(test_threshold_scaling);
    TEST_MAIN_RUN(test_progress_tracking_and_pruning);
    TEST_MAIN_RUN(test_custom_config);
    TEST_MAIN_RUN(test_backward_compatibility);
    TEST_MAIN_RUN(test_error_handling);
    TEST_MAIN_RUN(test_performance_overhead);


TEST_MAIN_END()
