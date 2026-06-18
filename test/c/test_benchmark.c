/**
 * @file test_benchmark.c
 * @brief 性能基准测试 - 大规模操作、内存使用、执行时间
 *
 * 测试内容：
 * - 大规模图操作（1000+ 节点）
 * - 归一化性能（不同规模图的合并速度）
 * - 统一化性能（命题匹配速度）
 * - 函数块打包/实例化性能
 * - 内存使用统计
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "lv00.h"
#include "test_helpers.h"

/* ============== 计时辅助函数 ============== */

static double get_time_ms(void) {
    clock_t t = clock();
    return (double) t * 1000.0 / CLOCKS_PER_SEC;
}

static void print_result(const char *name, double time_ms, int iterations, const char *unit) {
    printf("  %-40s %8.2f ms (%d %s)\n", name, time_ms, iterations, unit);
}

/* ============== 测试：大规模图创建 ============== */

static int test_large_graph_creation(void) {
    printf("Test: large graph creation...\n");

    int sizes[] = {100, 500, 1000};
    int num_sizes = sizeof(sizes) / sizeof(sizes[0]);

    for (int s = 0; s < num_sizes; s++) {
        int n = sizes[s];
        double start = get_time_ms();
        double elapsed = 0.0;

        ConstraintGraph *g = graph_create();
        if (!g) {
            print_result("Create graph with points and segments", elapsed, n, "nodes");
            printf("  FAILED (graph_create failed at size %d)\n", n);
            return -1;
        }

        /* 创建 n 个点 */
        for (int i = 0; i < n; i++) {
            add_point(g, i, 1, i, 1);
        }

        /* 创建 n/2 条线段 */
        for (int i = 0; i < n / 2; i++) {
            int p1 = i * 2;
            int p2 = i * 2 + 1;
            if (p2 < n) {
                graph_add_line_segment(g, p1, p2);
            }
        }

        elapsed = get_time_ms() - start;
        print_result("Create graph with points and segments", elapsed, n, "nodes");

        if (g->node_count < n) {
            printf("  FAILED (node_count %d < expected %d)\n", g->node_count, n);
            graph_destroy(g);
            return -1;
        }
        graph_destroy(g);
    }

    printf("  PASSED\n");
    return 0;
}

/* ============== 测试：大规模归一化 ============== */

static int test_large_normalization(void) {
    printf("Test: large graph normalization...\n");

    int sizes[] = {50, 100, 200};
    int num_sizes = sizeof(sizes) / sizeof(sizes[0]);

    for (int s = 0; s < num_sizes; s++) {
        int n = sizes[s];

        ConstraintGraph *g = graph_create();

        /* 创建 n 对重复的点（应该被合并） */
        for (int i = 0; i < n; i++) {
            /* 每对点有相同的坐标 */
            add_point(g, i, 1, i, 1);
            add_point(g, i, 1, i, 1); /* 重复 */
        }

        int node_count_before = g->node_count;
        assert(node_count_before == n * 2);

        double start = get_time_ms();
        NormalizationResult *result = graph_normalize(g, false);
        double elapsed = get_time_ms() - start;

        print_result("Normalize duplicate points", elapsed, n * 2, "nodes");

        assert(result != NULL);
        assert(result->merged_count >= n); /* 至少合并了 n 对 */

        normalization_result_destroy(result);
        graph_destroy(g);
    }

    printf("  PASSED\n");
    return 0;
}

/* ============== 测试：约束添加性能 ============== */

static int test_constraint_addition_performance(void) {
    printf("Test: constraint addition performance...\n");

    int sizes[] = {100, 500, 1000};
    int num_sizes = sizeof(sizes) / sizeof(sizes[0]);

    for (int s = 0; s < num_sizes; s++) {
        int n = sizes[s];

        ConstraintGraph *g = graph_create();

        /* 创建点 */
        for (int i = 0; i < n; i++) {
            add_point(g, i, 1, i, 1);
        }

        double start = get_time_ms();

        /* 添加 n 个 INCIDENCE 约束 */
        for (int i = 0; i < n - 1; i++) {
            graph_add_betweenness(g, i, i + 1, i);
        }

        double elapsed = get_time_ms() - start;
        print_result("Add BETWEENNESS constraints", elapsed, n, "constraints");

        assert(g->constraint_count >= n - 1);
        graph_destroy(g);
    }

    printf("  PASSED\n");
    return 0;
}

/* ============== 测试：函数块打包性能 ============== */

static int test_func_block_pack_performance(void) {
    printf("Test: function block pack performance...\n");

    int sizes[] = {10, 50, 100};
    int num_sizes = sizeof(sizes) / sizeof(sizes[0]);

    for (int s = 0; s < num_sizes; s++) {
        int n = sizes[s];

        ConstraintGraph *g = graph_create();

        /* 创建 n 个内部节点 */
        int *internal_ids = malloc(n * sizeof(int));
        for (int i = 0; i < n; i++) {
            internal_ids[i] = add_point(g, i, 1, i, 1);
        }

        /* 创建输入输出端口 */
        int in_port = add_point(g, -1, 1, -1, 1);
        int out_port = add_point(g, -2, 1, -2, 1);

        /* 将端口转换为实际的端口节点 */
        graph_add_port(g, PORT_INPUT, -1, -1);
        int in_port_id = g->next_node_id - 1;
        graph_add_port(g, PORT_OUTPUT, -1, -1);
        int out_port_id = g->next_node_id - 1;

        double start = get_time_ms();

        FuncBlock *fb = NULL;
        PackResult result = func_block_pack(g, internal_ids, n, &in_port_id, 1, &out_port_id, 1, NULL, 0, &fb);

        double elapsed = get_time_ms() - start;
        print_result("Pack function block", elapsed, n, "internal nodes");

        assert(result == PACK_RESULT_OK);
        assert(fb != NULL);

        func_block_destroy(fb);
        lv00_free_ptr(internal_ids);
        graph_destroy(g);
    }

    printf("  PASSED\n");
    return 0;
}

/* ============== 测试：统一化性能 ============== */

static int test_unification_performance(void) {
    printf("Test: unification performance...\n");

    int sizes[] = {10, 50, 100};
    int num_sizes = sizeof(sizes) / sizeof(sizes[0]);

    for (int s = 0; s < num_sizes; s++) {
        int n = sizes[s];

        ConstraintGraph *construction = graph_create();
        ConstraintGraph *proposition = graph_create();

        /* 创建构造图 */
        for (int i = 0; i < n; i++) {
            add_point(construction, i, 1, i, 1);
        }

        /* 创建命题图（相同的结构） */
        for (int i = 0; i < n; i++) {
            add_point(proposition, i, 1, i, 1);
        }

        double start = get_time_ms();

        UnifyStatus status = unify_construction_with_proposition(construction, proposition);

        double elapsed = get_time_ms() - start;
        print_result("Unify construction with proposition", elapsed, n, "nodes");

        assert(status == UNIFY_STATUS_OK);

        graph_destroy(construction);
        graph_destroy(proposition);
    }

    printf("  PASSED\n");
    return 0;
}

/* ============== 测试：内存使用估算 ============== */

static int test_memory_usage(void) {
    printf("Test: memory usage estimation...\n");

    /* 测试不同规模图的内存使用 */
    int sizes[] = {100, 500, 1000};
    int num_sizes = sizeof(sizes) / sizeof(sizes[0]);

    for (int s = 0; s < num_sizes; s++) {
        int n = sizes[s];

        ConstraintGraph *g = graph_create();

        /* 创建 n 个点 */
        for (int i = 0; i < n; i++) {
            add_point(g, i, 1, i, 1);
        }

        /* 创建 n 条线段 */
        for (int i = 0; i < n / 2; i++) {
            int p1 = i * 2;
            int p2 = i * 2 + 1;
            if (p2 < n) {
                graph_add_line_segment(g, p1, p2);
            }
        }

        /* 添加约束 */
        for (int i = 0; i < n / 2; i++) {
            int p1 = i * 2;
            int p2 = i * 2 + 1;
            if (p2 < n) {
                graph_add_betweenness(g, p1, p2, p1);
            }
        }

        /* 估算内存使用 */
        size_t node_mem = g->node_count * sizeof(GeomNode);
        size_t constraint_mem = g->constraint_count * sizeof(Constraint);
        size_t total_estimated = node_mem + constraint_mem + sizeof(ConstraintGraph);

        printf("  Graph with %d points:\n", n);
        printf("    Nodes: %d, Constraints: %d\n", g->node_count, g->constraint_count);
        printf("    Estimated memory: %zu KB\n", total_estimated / 1024);

        graph_destroy(g);
    }

    printf("  PASSED\n");
    return 0;
}

/* ============== 测试：复杂场景综合性能 ============== */

static int test_complex_scenario(void) {
    printf("Test: complex scenario performance...\n");

    double total_start = get_time_ms();

    ConstraintGraph *g = graph_create();

    /* 阶段1: 创建 100 个点的网格 */
    double start = get_time_ms();
    int grid_size = 10;
    for (int i = 0; i < grid_size; i++) {
        for (int j = 0; j < grid_size; j++) {
            add_point(g, i, 1, j, 1);
        }
    }
    printf("  Create 10x10 grid: %.2f ms\n", get_time_ms() - start);

    /* 阶段2: 添加水平线段 */
    start = get_time_ms();
    for (int i = 0; i < grid_size; i++) {
        for (int j = 0; j < grid_size - 1; j++) {
            int p1 = i * grid_size + j;
            int p2 = i * grid_size + (j + 1);
            graph_add_line_segment(g, p1, p2);
        }
    }
    printf("  Add horizontal segments: %.2f ms\n", get_time_ms() - start);

    /* 阶段3: 添加垂直线段 */
    start = get_time_ms();
    for (int i = 0; i < grid_size - 1; i++) {
        for (int j = 0; j < grid_size; j++) {
            int p1 = i * grid_size + j;
            int p2 = (i + 1) * grid_size + j;
            graph_add_line_segment(g, p1, p2);
        }
    }
    printf("  Add vertical segments: %.2f ms\n", get_time_ms() - start);

    /* 阶段4: 归一化 */
    start = get_time_ms();
    NormalizationResult *norm_result = graph_normalize(g, false);
    printf("  Normalize graph: %.2f ms\n", get_time_ms() - start);
    assert(norm_result != NULL);
    normalization_result_destroy(norm_result);

    /* 阶段5: 创建函数块 */
    start = get_time_ms();
    int internal_nodes[50];
    for (int i = 0; i < 50; i++) {
        internal_nodes[i] = i;
    }
    graph_add_port(g, PORT_INPUT, -1, -1);
    int in_port = g->next_node_id - 1;
    graph_add_port(g, PORT_OUTPUT, -1, -1);
    int out_port = g->next_node_id - 1;

    FuncBlock *fb = NULL;
    PackResult pack_result = func_block_pack(g, internal_nodes, 50, &in_port, 1, &out_port, 1, NULL, 0, &fb);
    printf("  Pack function block: %.2f ms\n", get_time_ms() - start);
    assert(pack_result == PACK_RESULT_OK);
    func_block_destroy(fb);

    double total_elapsed = get_time_ms() - total_start;
    printf("  Total time: %.2f ms\n", total_elapsed);

    graph_destroy(g);
    printf("  PASSED\n");
    return 0;
}

/* ============== 测试：符号坐标操作性能 ============== */

static int test_symbolic_coord_performance(void) {
    printf("Test: symbolic coordinate performance...\n");

    int iterations = 10000;

    /* 测试有理数创建 */
    double start = get_time_ms();
    for (int i = 0; i < iterations; i++) {
        Rational *r = rational_create(i, 100);
        rational_destroy(r);
    }
    print_result("Create/destroy rational", get_time_ms() - start, iterations, "ops");

    /* 测试有理数加法 */
    Rational *r1 = rational_create(1, 3);
    Rational *r2 = rational_create(1, 6);
    start = get_time_ms();
    for (int i = 0; i < iterations; i++) {
        Rational *sum = rational_add(r1, r2);
        rational_destroy(sum);
    }
    print_result("Rational addition", get_time_ms() - start, iterations, "ops");
    rational_destroy(r1);
    rational_destroy(r2);

    /* 测试符号坐标创建 */
    start = get_time_ms();
    for (int i = 0; i < iterations / 10; i++) {
        SymbolicCoord *c = symbolic_coord_create_rational(i, 100);
        symbolic_coord_destroy(c);
    }
    print_result("Create/destroy symbolic coord", get_time_ms() - start, iterations / 10, "ops");

    printf("  PASSED\n");
    return 0;
}

/* ============== 主函数 ============== */

int main(void) {
    printf("=== Lv-00 Performance Benchmark Suite ===\n\n");

    printf("Platform: Windows\n");
    printf("Timer resolution: CLOCKS_PER_SEC = %ld\n\n", (long) CLOCKS_PER_SEC);

    /* 大规模操作测试 */
    test_large_graph_creation();
    printf("\n");

    test_large_normalization();
    printf("\n");

    test_constraint_addition_performance();
    printf("\n");

    /* 函数块性能测试 */
    test_func_block_pack_performance();
    printf("\n");

    /* 统一化性能测试 */
    test_unification_performance();
    printf("\n");

    /* 内存使用测试 */
    test_memory_usage();
    printf("\n");

    /* 复杂场景测试 */
    test_complex_scenario();
    printf("\n");

    /* 符号坐标性能测试 */
    test_symbolic_coord_performance();
    printf("\n");

    printf("=== All benchmark tests completed! ===\n");
    return 0;
}
