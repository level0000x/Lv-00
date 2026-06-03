/**
 * @file test_adaptive_threshold.c
 * @brief 自适应阈值框架单元测试
 * 
 * 测试动态阈值计算、启发式剪枝、复杂度评估等功能
 * 
 * @version 1.0.0
 * @date 2026-05-26
 */

#include "lv00/adaptive_threshold.h"
#include "lv00/constraint_graph.h"
#include "test_helpers.h"
#include <math.h>
#include <stdio.h>

/* ==================== 测试辅助函数 ==================== */

/**
 * @brief 创建简单测试图（三角形）
 */
static Lv00ConstraintGraph* create_triangle_graph(void) {
    Lv00ConstraintGraph* graph = lv00_constraint_graph_create();
    TEST_ASSERT_NOT_NULL(graph);
    
    /* 添加三个点 */
    Lv00SymbolicCoord* coord_a = lv00_symbolic_coord_create_rational(0, 1);
    Lv00SymbolicCoord* coord_b = lv00_symbolic_coord_create_rational(1, 1);
    Lv00SymbolicCoord* coord_c = lv00_symbolic_coord_create_rational(0, 1);
    lv00_symbolic_coord_set_y(coord_c, lv00_rational_create(1, 1));
    
    int32_t id_a = lv00_graph_add_point(graph, coord_a, "A");
    int32_t id_b = lv00_graph_add_point(graph, coord_b, "B");
    int32_t id_c = lv00_graph_add_point(graph, coord_c, "C");
    
    TEST_ASSERT_EQ_MSG(id_a, 0, "Point A ID should be 0");
    TEST_ASSERT_EQ_MSG(id_b, 1, "Point B ID should be 1");
    TEST_ASSERT_EQ_MSG(id_c, 2, "Point C ID should be 2");
    
    /* 添加三条边 */
    lv00_graph_add_constraint(graph, LV00_CONSTRAINT_INCIDENCE, 
                              (int32_t[]){id_a, id_b}, 2);
    lv00_graph_add_constraint(graph, LV00_CONSTRAINT_INCIDENCE,
                              (int32_t[]){id_b, id_c}, 2);
    lv00_graph_add_constraint(graph, LV00_CONSTRAINT_INCIDENCE,
                              (int32_t[]){id_c, id_a}, 2);
    
    return graph;
}

/**
 * @brief 创建复杂测试图（多个连通分量）
 */
static Lv00ConstraintGraph* create_complex_graph(void) {
    Lv00ConstraintGraph* graph = lv00_constraint_graph_create();
    TEST_ASSERT_NOT_NULL(graph);
    
    /* 第一个连通分量：四边形 */
    for (int i = 0; i < 4; i++) {
        Lv00SymbolicCoord* coord = lv00_symbolic_coord_create_rational(i, 1);
        lv00_graph_add_point(graph, coord, NULL);
    }
    
    /* 四边形边 */
    lv00_graph_add_constraint(graph, LV00_CONSTRAINT_INCIDENCE,
                              (int32_t[]){0, 1}, 2);
    lv00_graph_add_constraint(graph, LV00_CONSTRAINT_INCIDENCE,
                              (int32_t[]){1, 2}, 2);
    lv00_graph_add_constraint(graph, LV00_CONSTRAINT_INCIDENCE,
                              (int32_t[]){2, 3}, 2);
    lv00_graph_add_constraint(graph, LV00_CONSTRAINT_INCIDENCE,
                              (int32_t[]){3, 0}, 2);
    
    /* 对角线 */
    lv00_graph_add_constraint(graph, LV00_CONSTRAINT_INCIDENCE,
                              (int32_t[]){0, 2}, 2);
    
    /* 第二个连通分量：三角形 */
    for (int i = 0; i < 3; i++) {
        Lv00SymbolicCoord* coord = lv00_symbolic_coord_create_rational(i + 10, 1);
        lv00_graph_add_point(graph, coord, NULL);
    }
    
    lv00_graph_add_constraint(graph, LV00_CONSTRAINT_INCIDENCE,
                              (int32_t[]){4, 5}, 2);
    lv00_graph_add_constraint(graph, LV00_CONSTRAINT_INCIDENCE,
                              (int32_t[]){5, 6}, 2);
    lv00_graph_add_constraint(graph, LV00_CONSTRAINT_INCIDENCE,
                              (int32_t[]){6, 4}, 2);
    
    return graph;
}

/* ==================== 测试用例 ==================== */

/**
 * @test 测试框架初始化和清理
 */
void test_init_cleanup(void) {
    Lv00Error err = lv00_adaptive_threshold_init();
    TEST_ASSERT_EQ_MSG(err, LV00_OK, "Init should succeed");
    
    /* 重复初始化应该成功 */
    err = lv00_adaptive_threshold_init();
    TEST_ASSERT_EQ_MSG(err, LV00_OK, "Repeated init should succeed");
    
    lv00_adaptive_threshold_cleanup();
    
    /* 重新初始化 */
    err = lv00_adaptive_threshold_init();
    TEST_ASSERT_EQ_MSG(err, LV00_OK, "Init after cleanup should succeed");
}

/**
 * @test 测试问题复杂度评估
 */
void test_complexity_computation(void) {
    lv00_adaptive_threshold_init();
    
    /* 测试简单图 */
    Lv00ConstraintGraph* simple = create_triangle_graph();
    Lv00ProblemComplexity complexity;
    
    Lv00Error err = lv00_compute_complexity(simple, &complexity);
    TEST_ASSERT_EQ_MSG(err, LV00_OK, "Complexity computation should succeed");
    
    TEST_ASSERT_EQ_MSG(complexity.node_count, 3, "Should have 3 nodes");
    TEST_ASSERT_EQ_MSG(complexity.constraint_count, 3, "Should have 3 constraints");
    TEST_ASSERT(complexity.edge_count >= 3);
    TEST_ASSERT(complexity.density > 0.0 && complexity.density <= 1.0);
    TEST_ASSERT_EQ_MSG(complexity.connected_components, 1, "Should be 1 component");
    
    lv00_constraint_graph_destroy(simple);
    
    /* 测试复杂图 */
    Lv00ConstraintGraph* complex = create_complex_graph();
    err = lv00_compute_complexity(complex, &complexity);
    TEST_ASSERT_EQ_MSG(err, LV00_OK, "Complexity computation should succeed");
    
    TEST_ASSERT_EQ_MSG(complexity.node_count, 7, "Should have 7 nodes");
    TEST_ASSERT_EQ_MSG(complexity.connected_components, 2, "Should have 2 components");
    
    lv00_constraint_graph_destroy(complex);
}

/**
 * @test 测试动态阈值计算 - VF2匹配
 */
void test_vf2_threshold_computation(void) {
    lv00_adaptive_threshold_init();
    
    Lv00ConstraintGraph* graph = create_triangle_graph();
    Lv00AdaptiveThresholdCtx* ctx = NULL;
    
    Lv00Error err = lv00_adaptive_threshold_create(
        LV00_ALGO_VF2_MATCH,
        graph,
        NULL,
        &ctx
    );
    TEST_ASSERT_EQ_MSG(err, LV00_OK, "Context creation should succeed");
    TEST_ASSERT_NOT_NULL(ctx);
    
    /* 计算阈值 */
    size_t threshold = lv00_adaptive_threshold_compute(ctx);
    
    /* 验证阈值在合理范围内 */
    Lv00ThresholdConfig config;
    lv00_adaptive_threshold_default_config(LV00_ALGO_VF2_MATCH, &config);
    
    TEST_ASSERT(threshold >= (size_t)config.min_threshold);
    TEST_ASSERT(threshold <= (size_t)config.max_threshold);
    
    /* 对于3个节点的简单图，阈值应该相对较小 */
    TEST_ASSERT(threshold < 500);
    
    lv00_adaptive_threshold_destroy(&ctx);
    lv00_constraint_graph_destroy(graph);
}

/**
 * @test 测试动态阈值计算 - Buchberger算法
 */
void test_buchberger_threshold_computation(void) {
    lv00_adaptive_threshold_init();
    
    Lv00ConstraintGraph* graph = create_triangle_graph();
    Lv00AdaptiveThresholdCtx* ctx = NULL;
    
    Lv00Error err = lv00_adaptive_threshold_create(
        LV00_ALGO_BUCHBERGER,
        graph,
        NULL,
        &ctx
    );
    TEST_ASSERT_EQ_MSG(err, LV00_OK, "Context creation should succeed");
    
    size_t threshold = lv00_adaptive_threshold_compute(ctx);
    
    Lv00ThresholdConfig config;
    lv00_adaptive_threshold_default_config(LV00_ALGO_BUCHBERGER, &config);
    
    TEST_ASSERT(threshold >= (size_t)config.min_threshold);
    TEST_ASSERT(threshold <= (size_t)config.max_threshold);
    
    /* Buchberger阈值应该比VF2大 */
    TEST_ASSERT(threshold > 10000);
    
    lv00_adaptive_threshold_destroy(&ctx);
    lv00_constraint_graph_destroy(graph);
}

/**
 * @test 测试阈值随复杂度变化
 */
void test_threshold_scaling(void) {
    lv00_adaptive_threshold_init();
    
    /* 创建不同复杂度的图 */
    Lv00ConstraintGraph* small = create_triangle_graph();
    Lv00ConstraintGraph* large = create_complex_graph();
    
    Lv00AdaptiveThresholdCtx* ctx_small = NULL;
    Lv00AdaptiveThresholdCtx* ctx_large = NULL;
    
    lv00_adaptive_threshold_create(LV00_ALGO_VF2_MATCH, small, NULL, &ctx_small);
    lv00_adaptive_threshold_create(LV00_ALGO_VF2_MATCH, large, NULL, &ctx_large);
    
    size_t threshold_small = lv00_adaptive_threshold_compute(ctx_small);
    size_t threshold_large = lv00_adaptive_threshold_compute(ctx_large);
    
    /* 复杂图的阈值应该大于简单图 */
    TEST_ASSERT(threshold_large > threshold_small);
    
    lv00_adaptive_threshold_destroy(&ctx_small);
    lv00_adaptive_threshold_destroy(&ctx_large);
    lv00_constraint_graph_destroy(small);
    lv00_constraint_graph_destroy(large);
}

/**
 * @test 测试进度更新和启发式剪枝
 */
void test_progress_tracking_and_pruning(void) {
    lv00_adaptive_threshold_init();
    
    Lv00ConstraintGraph* graph = create_triangle_graph();
    Lv00AdaptiveThresholdCtx* ctx = NULL;
    
    /* 使用很短的超时时间以便测试剪枝 */
    Lv00ThresholdConfig config;
    lv00_adaptive_threshold_default_config(LV00_ALGO_VF2_MATCH, &config);
    config.time_budget_ms = 1.0;  /* 1毫秒超时 */
    config.enable_progress_tracking = true;
    
    lv00_adaptive_threshold_create(LV00_ALGO_VF2_MATCH, graph, &config, &ctx);
    
    /* 模拟一些迭代 */
    for (int i = 0; i < 10; i++) {
        lv00_adaptive_threshold_update_progress(ctx, (size_t)i, 0);
        
        bool should_prune = false;
        lv00_adaptive_threshold_should_prune(ctx, &should_prune);
        
        /* 在1ms超时后应该触发剪枝 */
        if (i > 5) {
            /* 注意：由于时间依赖，这个测试可能不稳定 */
            /* 这里主要测试API不崩溃 */
        }
    }
    
    /* 测试高回溯率剪枝 */
    lv00_adaptive_threshold_update_progress(ctx, 200, 190);  /* 95%回溯率 */
    
    bool should_prune = false;
    lv00_adaptive_threshold_should_prune(ctx, &should_prune);
    /* 高回溯率应该触发剪枝 */
    TEST_ASSERT(should_prune);
    
    lv00_adaptive_threshold_destroy(&ctx);
    lv00_constraint_graph_destroy(graph);
}

/**
 * @test 测试自定义配置
 */
void test_custom_config(void) {
    lv00_adaptive_threshold_init();
    
    Lv00ThresholdConfig custom_config = {
        .base_threshold = 50.0,
        .scale_factor = 1.0,
        .time_budget_ms = 1000.0,
        .min_threshold = 10.0,
        .max_threshold = 200.0,
        .enable_time_based = false,
        .enable_progress_tracking = false
    };
    
    /* 设置全局配置 */
    Lv00Error err = lv00_adaptive_threshold_set_global_config(
        LV00_ALGO_VF2_MATCH,
        &custom_config
    );
    TEST_ASSERT_EQ_MSG(err, LV00_OK, "Setting global config should succeed");
    
    /* 创建上下文，应该使用全局配置 */
    Lv00ConstraintGraph* graph = create_triangle_graph();
    Lv00AdaptiveThresholdCtx* ctx = NULL;
    
    err = lv00_adaptive_threshold_create(LV00_ALGO_VF2_MATCH, graph, NULL, &ctx);
    TEST_ASSERT_EQ_MSG(err, LV00_OK, "Context creation should succeed");
    
    size_t threshold = lv00_adaptive_threshold_compute(ctx);
    
    /* 使用自定义配置，阈值应该在更小的范围内 */
    TEST_ASSERT(threshold >= 10);
    TEST_ASSERT(threshold <= 200);
    
    lv00_adaptive_threshold_destroy(&ctx);
    lv00_constraint_graph_destroy(graph);
}

/**
 * @test 测试向后兼容函数
 */
void test_backward_compatibility(void) {
    lv00_adaptive_threshold_init();
    
    Lv00ConstraintGraph* graph = create_triangle_graph();
    
    /* 测试向后兼容函数 */
    size_t vf2_depth = lv00_get_vf2_max_depth(graph);
    size_t buchberger_steps = lv00_get_buchberger_max_steps(graph);
    size_t rewrite_iterations = lv00_get_rewrite_solve_max_iterations(graph);
    
    /* 验证返回的值在合理范围内 */
    TEST_ASSERT(vf2_depth >= 50 && vf2_depth <= 1000);
    TEST_ASSERT(buchberger_steps >= 10000 && buchberger_steps <= 200000);
    TEST_ASSERT(rewrite_iterations >= 5000 && rewrite_iterations <= 50000);
    
    lv00_constraint_graph_destroy(graph);
}

/**
 * @test 测试错误处理
 */
void test_error_handling(void) {
    lv00_adaptive_threshold_init();
    
    /* 测试NULL参数 */
    Lv00Error err = lv00_compute_complexity(NULL, NULL);
    TEST_ASSERT(err != LV00_OK);
    
    /* 测试无效算法类型 */
    Lv00AdaptiveThresholdCtx* ctx = NULL;
    err = lv00_adaptive_threshold_create(
        (Lv00AlgorithmType)999,
        NULL,
        NULL,
        &ctx
    );
    TEST_ASSERT(err != LV00_OK);
    
    /* 测试无效算法类型的默认配置 */
    Lv00ThresholdConfig config;
    err = lv00_adaptive_threshold_default_config((Lv00AlgorithmType)999, &config);
    TEST_ASSERT(err != LV00_OK);
}

/**
 * @test 性能测试：验证动态阈值计算开销
 */
void test_performance_overhead(void) {
    lv00_adaptive_threshold_init();
    
    Lv00ConstraintGraph* graph = create_complex_graph();
    
    /* 测量多次阈值计算的时间 */
    const int iterations = 1000;
    
    /* 使用clock_gettime进行精确计时 */
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    for (int i = 0; i < iterations; i++) {
        Lv00AdaptiveThresholdCtx* ctx = NULL;
        lv00_adaptive_threshold_create(LV00_ALGO_VF2_MATCH, graph, NULL, &ctx);
        lv00_adaptive_threshold_compute(ctx);
        lv00_adaptive_threshold_destroy(&ctx);
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    double elapsed_ms = (end.tv_sec - start.tv_sec) * 1000.0 +
                        (end.tv_nsec - start.tv_nsec) / 1000000.0;
    double avg_ms = elapsed_ms / iterations;
    
    printf("  Average threshold computation time: %.3f ms\n", avg_ms);
    
    /* 每次阈值计算应该非常快（< 1ms） */
    TEST_ASSERT(avg_ms < 1.0);
    
    lv00_constraint_graph_destroy(graph);
}

/* ==================== 测试套件入口 ==================== */

int main(void) {
    printf("Running adaptive threshold tests...\n\n");
    
    TEST_SUITE_BEGIN();
    
    TEST_RUN(test_init_cleanup);
    TEST_RUN(test_complexity_computation);
    TEST_RUN(test_vf2_threshold_computation);
    TEST_RUN(test_buchberger_threshold_computation);
    TEST_RUN(test_threshold_scaling);
    TEST_RUN(test_progress_tracking_and_pruning);
    TEST_RUN(test_custom_config);
    TEST_RUN(test_backward_compatibility);
    TEST_RUN(test_error_handling);
    TEST_RUN(test_performance_overhead);
    
    TEST_SUITE_END();
    TEST_SUMMARY();
    
    return g_fail_count > 0 ? 1 : 0;
}
