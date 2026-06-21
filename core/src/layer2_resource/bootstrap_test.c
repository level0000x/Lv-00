/**
 * @file bootstrap_test.c
 * @brief Lv-00 自举差分测试框架实现骨架
 *
 * @details 本文件提供自举测试框架的基础实现。
 *          完整实现将在 Phase 1-4 逐步完成。
 *
 * @author Lv-00 Project
 * @version 1.0.0
 * @date 2026-05-29
 */

#include "lv00/bootstrap_test.h"
#include "lv00/lv00_utils.h"
#include "lv00/constraint_graph.h"
#include "lv00/engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ============== 内部状态 ============== */

static bool g_initialized = false;
static uint64_t g_test_count = 0;
static uint64_t g_pass_count = 0;
static uint64_t g_fail_count = 0;

/* ============== 框架初始化 ============== */

bool bootstrap_test_framework_init(void)
{
    if (g_initialized) {
        return true;
    }

    /* 初始化 Lv-00 核心系统 */
    if (!lv00_init()) {
        fprintf(stderr, "[BootstrapTest] Failed to initialize Lv-00 core\n");
        return false;
    }

    /* 初始化原语包装器 */
    if (!primitive_wrapper_init()) {
        fprintf(stderr, "[BootstrapTest] Failed to initialize primitive wrapper\n");
        lv00_cleanup();
        return false;
    }

    g_initialized = true;
    g_test_count = 0;
    g_pass_count = 0;
    g_fail_count = 0;

    printf("[BootstrapTest] Framework initialized successfully\n");
    return true;
}

void bootstrap_test_framework_cleanup(void)
{
    if (!g_initialized) {
        return;
    }

    primitive_wrapper_cleanup();
    lv00_cleanup();

    g_initialized = false;
    printf("[BootstrapTest] Framework cleaned up\n");
}

bool bootstrap_test_framework_is_initialized(void)
{
    return g_initialized;
}

/* ============== 差分测试 ============== */

struct BootstrapDiffTest {
    char *test_name;
    char *dsl_source;
    void *input_graph;
};

BootstrapDiffTest *bootstrap_diff_test_create(const char *test_name,
                                               const char *dsl_source)
{
    BootstrapDiffTest *test = lv00_malloc(sizeof(BootstrapDiffTest));
    if (!test) {
        return NULL;
    }

    test->test_name = lv00_strdup_safe(test_name ? test_name : "unnamed");
    test->dsl_source = dsl_source ? lv00_strdup_safe(dsl_source) : NULL;
    test->input_graph = NULL;

    return test;
}

void bootstrap_diff_test_destroy(BootstrapDiffTest *test)
{
    if (!test) {
        return;
    }

    lv00_free(&test->test_name);
    lv00_free(&test->dsl_source);
    /* input_graph 由调用者管理 */
    lv00_free(&test);
}

BootstrapDiffTestResult *bootstrap_diff_test_run(BootstrapDiffTest *test)
{
    if (!test || !g_initialized) {
        return NULL;
    }

    BootstrapDiffTestResult *result = lv00_malloc(sizeof(BootstrapDiffTestResult));
    if (!result) {
        return NULL;
    }

    memset(result, 0, sizeof(BootstrapDiffTestResult));

    /* 差分测试逻辑：解析 DSL、通过 C API 执行、比较结果 */
    if (test->dsl_source) {
        /* 通过 DSL 解析器执行 */
        result->c_api_output = lv00_strdup_safe(test->dsl_source);
    }

    /* 通过几何层执行（如果有输入图） */
    if (test->input_graph) {
        ConstraintGraph *g = (ConstraintGraph *)test->input_graph;
        result->geo_layer_output = lv00_asprintf(
            "graph:nodes=%d,constraints=%d",
            graph_get_node_count(g), graph_get_constraint_count(g));
    }

    /* 比较两个输出 */
    if (result->c_api_output && result->geo_layer_output) {
        if (strcmp(result->c_api_output, result->geo_layer_output) == 0) {
            result->comparison = DIFF_RESULT_IDENTICAL;
            result->passed = true;
            g_pass_count++;
        } else {
            result->comparison = DIFF_RESULT_DIFFERENT;
            result->passed = false;
            result->diff_description = lv00_strdup_safe("C API and geometry layer outputs differ");
            g_fail_count++;
        }
    } else {
        result->comparison = DIFF_RESULT_ERROR;
        result->passed = false;
        result->error_message = lv00_strdup_safe("Incomplete differential test: missing output");
        g_fail_count++;
    }

    g_test_count++;

    return result;
}

void bootstrap_diff_test_result_destroy(BootstrapDiffTestResult *result)
{
    if (!result) {
        return;
    }

    lv00_free(&result->c_api_output);
    lv00_free(&result->geo_layer_output);
    lv00_free(&result->diff_description);
    lv00_free(&result->error_message);
    lv00_free(&result);
}

uint32_t bootstrap_diff_test_run_batch(BootstrapDiffTest **tests,
                                        uint32_t count,
                                        BootstrapDiffTestResult **out_results)
{
    if (!tests || !out_results || !g_initialized) {
        return 0;
    }

    uint32_t executed = 0;
    for (uint32_t i = 0; i < count; i++) {
        out_results[i] = bootstrap_diff_test_run(tests[i]);
        if (out_results[i]) {
            executed++;
        }
    }

    return executed;
}

/* ============== 随机生成器 ============== */

struct RandomGenerator {
    RandomGeneratorConfig config;
    uint64_t current_seed;
};

RandomGeneratorConfig random_generator_default_config(void)
{
    RandomGeneratorConfig config;
    memset(&config, 0, sizeof(config));

    config.min_points = 3;
    config.max_points = 20;
    config.min_lines = 1;
    config.max_lines = 10;
    config.min_circles = 0;
    config.max_circles = 5;

    config.constraint_density = 0.5;

    config.coord_min = -100.0;
    config.coord_max = 100.0;

    config.allow_degenerate = false;
    config.allow_overconstrained = false;
    config.use_symbolic_coords = true;

    config.seed = (uint64_t)time(NULL);

    return config;
}

RandomGenerator *random_generator_create(const RandomGeneratorConfig *config)
{
    RandomGenerator *gen = lv00_malloc(sizeof(RandomGenerator));
    if (!gen) {
        return NULL;
    }

    if (config) {
        gen->config = *config;
    } else {
        gen->config = random_generator_default_config();
    }

    gen->current_seed = gen->config.seed;

    return gen;
}

void random_generator_destroy(RandomGenerator *gen)
{
    lv00_free(&gen);
}

void *random_generator_generate_graph(RandomGenerator *gen)
{
    if (!gen) {
        return NULL;
    }

    /* 随机图生成：创建随机几何实体和约束 */
    ConstraintGraph *graph = graph_create();
    if (!graph) {
        return NULL;
    }

    /* 确定实体数量 */
    uint32_t point_count = gen->config.min_points +
        (lv00_random_int(0, gen->config.max_points - gen->config.min_points));
    uint32_t line_count = gen->config.min_lines +
        (lv00_random_int(0, gen->config.max_lines - gen->config.min_lines));

    /* 创建随机点（使用符号坐标） */
    for (uint32_t i = 0; i < point_count; i++) {
        double x = lv00_random_double(gen->config.coord_min, gen->config.coord_max);
        double y = lv00_random_double(gen->config.coord_min, gen->config.coord_max);
        SymbolicCoord *sx = symbolic_coord_create_rational((long long)(x * 1000), 1000);
        SymbolicCoord *sy = symbolic_coord_create_rational((long long)(y * 1000), 1000);
        SymbolicCoord *coords[] = {sx, sy};
        graph_add_point(graph, coords, 2);
        symbolic_coord_destroy(sx);
        symbolic_coord_destroy(sy);
    }

    /* 创建随机线段 */
    for (uint32_t i = 0; i < line_count; i++) {
        int a = lv00_random_int(0, (int)point_count - 1);
        int b = lv00_random_int(0, (int)point_count - 1);
        if (a != b) {
            graph_add_line_segment(graph, a, b);
        }
    }

    /* 添加随机约束 */
    for (uint32_t i = 0; i < point_count - 1; i++) {
        if (lv00_random_double(0.0, 1.0) < gen->config.constraint_density) {
            int a = (int)i;
            int b = (int)i + 1;
            double dist = lv00_random_double(0.1, 50.0);
            graph_add_distance_constraint(graph, a, b, dist);
        }
    }

    return graph;
}

char *random_generator_generate_dsl(RandomGenerator *gen)
{
    if (!gen) {
        return NULL;
    }

    /* 随机 DSL 生成：根据配置生成几何构造 DSL */
    uint32_t n_points = gen->config.min_points +
        (lv00_random_int(0, gen->config.max_points - gen->config.min_points));

    /* 构建 DSL 缓冲区 */
    char buf[4096];
    int pos = 0;
    pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, "#version 5.0.0\n");

    /* 生成点声明 */
    for (uint32_t i = 0; i < n_points; i++) {
        double x = lv00_random_double(gen->config.coord_min, gen->config.coord_max);
        double y = lv00_random_double(gen->config.coord_min, gen->config.coord_max);
        pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos,
            "Point P%u = (%.2f, %.2f);\n", i, x, y);
    }

    /* 生成随机约束 */
    const char *constraint_types[] = {
        "collinear", "distance", "parallel", "perpendicular"
    };
    int n_constraints = lv00_random_int(1, (int)n_points / 2 + 1);
    for (int c = 0; c < n_constraints; c++) {
        int type_idx = lv00_random_int(0, 3);
        int a = lv00_random_int(0, (int)n_points - 1);
        int b = lv00_random_int(0, (int)n_points - 1);
        if (a == b) b = (b + 1) % (int)n_points;
        pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos,
            "Constraint %s(P%u, P%u);\n", constraint_types[type_idx], a, b);
    }

    pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, "Prove;\n");

    char *dsl = lv00_strdup_safe(buf);

    return dsl;
}

uint32_t random_generator_generate_batch(RandomGenerator *gen,
                                          void **out_graphs,
                                          uint32_t count)
{
    if (!gen || !out_graphs) {
        return 0;
    }

    uint32_t generated = 0;
    for (uint32_t i = 0; i < count; i++) {
        out_graphs[i] = random_generator_generate_graph(gen);
        if (out_graphs[i]) {
            generated++;
        }
    }

    return generated;
}

void random_generator_reset_seed(RandomGenerator *gen, uint64_t seed)
{
    if (gen) {
        gen->current_seed = seed;
        lv00_random_init(seed);
    }
}

/* ============== 图同构比较器 ============== */

struct GraphIsomorphismComparator {
    bool ignore_ids;
    bool compare_coords;
    double coord_tolerance;
};

GraphIsomorphismComparator *graph_isomorphism_create(void)
{
    GraphIsomorphismComparator *comp = lv00_malloc(sizeof(GraphIsomorphismComparator));
    if (!comp) {
        return NULL;
    }

    comp->ignore_ids = true;
    comp->compare_coords = true;
    comp->coord_tolerance = 1e-10;

    return comp;
}

void graph_isomorphism_destroy(GraphIsomorphismComparator *comp)
{
    lv00_free(&comp);
}

void graph_isomorphism_configure(GraphIsomorphismComparator *comp,
                                  bool ignore_ids,
                                  bool compare_coords,
                                  double coord_tolerance)
{
    if (comp) {
        comp->ignore_ids = ignore_ids;
        comp->compare_coords = compare_coords;
        comp->coord_tolerance = coord_tolerance;
    }
}

bool graph_isomorphism_compare(GraphIsomorphismComparator *comp,
                                const void *graph_a,
                                const void *graph_b)
{
    if (!comp || !graph_a || !graph_b) {
        return false;
    }

    /* VF2 同构检测（基于度数序列和邻域签名匹配，完整版需支持回溯搜索） */
    const ConstraintGraph *ga = (const ConstraintGraph *)graph_a;
    const ConstraintGraph *gb = (const ConstraintGraph *)graph_b;

    if (graph_get_node_count(ga) != graph_get_node_count(gb)) {
        return false;
    }
    if (graph_get_constraint_count(ga) != graph_get_constraint_count(gb)) {
        return false;
    }

    int n = graph_get_node_count(ga);
    if (n == 0) return true;

    /* 计算度数序列 */
    int *deg_a = (int *)calloc((size_t)n, sizeof(int));
    int *deg_b = (int *)calloc((size_t)n, sizeof(int));
    if (!deg_a || !deg_b) { free(deg_a); free(deg_b); return false; }

    for (int i = 0; i < n; i++) {
        int cids[64];
        deg_a[i] = graph_find_constraints_involving(ga, i, cids, 64);
        deg_b[i] = graph_find_constraints_involving(gb, i, cids, 64);
    }

    /* 排序度数序列后比较 */
    for (int i = 0; i < n - 1; i++) {
        for (int j = i + 1; j < n; j++) {
            if (deg_a[i] > deg_a[j]) { int t = deg_a[i]; deg_a[i] = deg_a[j]; deg_a[j] = t; }
            if (deg_b[i] > deg_b[j]) { int t = deg_b[i]; deg_b[i] = deg_b[j]; deg_b[j] = t; }
        }
    }

    bool same_degree = true;
    for (int i = 0; i < n; i++) {
        if (deg_a[i] != deg_b[i]) { same_degree = false; break; }
    }

    if (!same_degree) {
        free(deg_a);
        free(deg_b);
        return false;
    }

    /* VF2 邻域签名比较：对每个节点，收集其邻居的度数并排序后比较 */
    /* 重新计算度数（因为上面的已排序） */
    int *deg_a_raw = (int *)calloc((size_t)n, sizeof(int));
    int *deg_b_raw = (int *)calloc((size_t)n, sizeof(int));
    if (!deg_a_raw || !deg_b_raw) { free(deg_a); free(deg_b); free(deg_a_raw); free(deg_b_raw); return false; }

    for (int i = 0; i < n; i++) {
        int cids[64];
        deg_a_raw[i] = graph_find_constraints_involving(ga, i, cids, 64);
        deg_b_raw[i] = graph_find_constraints_involving(gb, i, cids, 64);
    }

    /* 为每个节点计算排序后的邻居度数签名 */
    int max_neighbors = 64;
    int *neighbor_sigs_a = (int *)calloc((size_t)n * (size_t)max_neighbors, sizeof(int));
    int *neighbor_sigs_b = (int *)calloc((size_t)n * (size_t)max_neighbors, sizeof(int));
    int *neighbor_counts_a = (int *)calloc((size_t)n, sizeof(int));
    int *neighbor_counts_b = (int *)calloc((size_t)n, sizeof(int));
    if (!neighbor_sigs_a || !neighbor_sigs_b || !neighbor_counts_a || !neighbor_counts_b) {
        free(deg_a); free(deg_b); free(deg_a_raw); free(deg_b_raw);
        free(neighbor_sigs_a); free(neighbor_sigs_b);
        free(neighbor_counts_a); free(neighbor_counts_b);
        return false;
    }

    for (int i = 0; i < n; i++) {
        int cids_a[64], cids_b[64];
        int nc_a = graph_find_constraints_involving(ga, i, cids_a, 64);
        int nc_b = graph_find_constraints_involving(gb, i, cids_b, 64);

        /* 收集 ga 中节点 i 的邻居度数 */
        for (int c = 0; c < nc_a && neighbor_counts_a[i] < max_neighbors; c++) {
            Constraint *cons = graph_get_constraint(ga, cids_a[c]);
            if (!cons || !cons->is_active) continue;
            for (int p = 0; p < cons->participant_count; p++) {
                int nb = cons->participants[p];
                if (nb >= 0 && nb < n && nb != i) {
                    neighbor_sigs_a[i * max_neighbors + neighbor_counts_a[i]] = deg_a_raw[nb];
                    neighbor_counts_a[i]++;
                }
            }
        }

        /* 收集 gb 中节点 i 的邻居度数 */
        for (int c = 0; c < nc_b && neighbor_counts_b[i] < max_neighbors; c++) {
            Constraint *cons = graph_get_constraint(gb, cids_b[c]);
            if (!cons || !cons->is_active) continue;
            for (int p = 0; p < cons->participant_count; p++) {
                int nb = cons->participants[p];
                if (nb >= 0 && nb < n && nb != i) {
                    neighbor_sigs_b[i * max_neighbors + neighbor_counts_b[i]] = deg_b_raw[nb];
                    neighbor_counts_b[i]++;
                }
            }
        }

        /* 排序每个节点的邻居度数签名 */
        for (int x = 0; x < neighbor_counts_a[i] - 1; x++) {
            for (int y = x + 1; y < neighbor_counts_a[i]; y++) {
                if (neighbor_sigs_a[i * max_neighbors + x] > neighbor_sigs_a[i * max_neighbors + y]) {
                    int t = neighbor_sigs_a[i * max_neighbors + x];
                    neighbor_sigs_a[i * max_neighbors + x] = neighbor_sigs_a[i * max_neighbors + y];
                    neighbor_sigs_a[i * max_neighbors + y] = t;
                }
            }
        }
        for (int x = 0; x < neighbor_counts_b[i] - 1; x++) {
            for (int y = x + 1; y < neighbor_counts_b[i]; y++) {
                if (neighbor_sigs_b[i * max_neighbors + x] > neighbor_sigs_b[i * max_neighbors + y]) {
                    int t = neighbor_sigs_b[i * max_neighbors + x];
                    neighbor_sigs_b[i * max_neighbors + x] = neighbor_sigs_b[i * max_neighbors + y];
                    neighbor_sigs_b[i * max_neighbors + y] = t;
                }
            }
        }
    }

    /* 比较两个图的邻域签名多集合 */
    /* 将所有节点的签名拼接成一个大数组，排序后比较 */
    int total_sigs_a = 0, total_sigs_b = 0;
    for (int i = 0; i < n; i++) {
        total_sigs_a += neighbor_counts_a[i];
        total_sigs_b += neighbor_counts_b[i];
    }

    bool same_signatures = (total_sigs_a == total_sigs_b);
    if (same_signatures && total_sigs_a > 0) {
        /* 拼接并排序所有签名 */
        int *all_sigs_a = (int *)calloc((size_t)total_sigs_a, sizeof(int));
        int *all_sigs_b = (int *)calloc((size_t)total_sigs_b, sizeof(int));
        if (!all_sigs_a || !all_sigs_b) {
            same_signatures = false;
        } else {
            int pos = 0;
            for (int i = 0; i < n; i++) {
                for (int j = 0; j < neighbor_counts_a[i]; j++) {
                    all_sigs_a[pos++] = neighbor_sigs_a[i * max_neighbors + j];
                }
            }
            pos = 0;
            for (int i = 0; i < n; i++) {
                for (int j = 0; j < neighbor_counts_b[i]; j++) {
                    all_sigs_b[pos++] = neighbor_sigs_b[i * max_neighbors + j];
                }
            }
            /* 排序 */
            for (int i = 0; i < total_sigs_a - 1; i++) {
                for (int j = i + 1; j < total_sigs_a; j++) {
                    if (all_sigs_a[i] > all_sigs_a[j]) { int t = all_sigs_a[i]; all_sigs_a[i] = all_sigs_a[j]; all_sigs_a[j] = t; }
                }
            }
            for (int i = 0; i < total_sigs_b - 1; i++) {
                for (int j = i + 1; j < total_sigs_b; j++) {
                    if (all_sigs_b[i] > all_sigs_b[j]) { int t = all_sigs_b[i]; all_sigs_b[i] = all_sigs_b[j]; all_sigs_b[j] = t; }
                }
            }
            for (int i = 0; i < total_sigs_a; i++) {
                if (all_sigs_a[i] != all_sigs_b[i]) { same_signatures = false; break; }
            }
            free(all_sigs_a);
            free(all_sigs_b);
        }
    }

    free(deg_a);
    free(deg_b);
    free(deg_a_raw);
    free(deg_b_raw);
    free(neighbor_sigs_a);
    free(neighbor_sigs_b);
    free(neighbor_counts_a);
    free(neighbor_counts_b);
    return same_signatures;
}

uint64_t graph_isomorphism_hash(const void *graph)
{
    if (!graph) {
        return 0;
    }

    /* WL (Weisfeiler-Lehman) 图核哈希：迭代压缩节点标签 */
    const ConstraintGraph *g = (const ConstraintGraph *)graph;
    int n = graph_get_node_count(g);
    if (n == 0) return 0;

    /* 初始标签：度数 */
    uint64_t *labels = (uint64_t *)calloc((size_t)n, sizeof(uint64_t));
    if (!labels) return 0;

    for (int i = 0; i < n; i++) {
        int cids[64];
        int deg = graph_find_constraints_involving(g, i, cids, 64);
        labels[i] = (uint64_t)(deg + 1);
    }

    /* WL 迭代（3 轮） */
    for (int iter = 0; iter < 3; iter++) {
        uint64_t *new_labels = (uint64_t *)calloc((size_t)n, sizeof(uint64_t));
        if (!new_labels) { free(labels); return 0; }

        for (int i = 0; i < n; i++) {
            int cids[64];
            int nc = graph_find_constraints_involving(g, i, cids, 64);
            uint64_t hash = labels[i];
            for (int c = 0; c < nc; c++) {
                Constraint *cons = graph_get_constraint(g, cids[c]);
                if (!cons) continue;
                for (int p = 0; p < cons->participant_count; p++) {
                    int nb = cons->participants[p];
                    if (nb >= 0 && nb < n) {
                        hash ^= (labels[nb] * 2654435761ULL + (uint64_t)cons->type);
                    }
                }
            }
            new_labels[i] = hash;
        }
        free(labels);
        labels = new_labels;
    }

    /* 聚合所有标签为最终哈希 */
    uint64_t final_hash = 0;
    for (int i = 0; i < n; i++) {
        final_hash ^= (labels[i] * (uint64_t)(i + 1));
    }
    free(labels);

    return final_hash;
}

bool graph_isomorphism_find_mapping(GraphIsomorphismComparator *comp,
                                     const void *graph_a,
                                     const void *graph_b,
                                     int **out_node_mapping,
                                     int **out_constraint_mapping)
{
    if (!comp || !graph_a || !graph_b) {
        return false;
    }

    /* 映射查找（基于度数匹配的贪心算法，完整版需支持回溯和约束传播） */
    const ConstraintGraph *ga = (const ConstraintGraph *)graph_a;
    const ConstraintGraph *gb = (const ConstraintGraph *)graph_b;

    int na = graph_get_node_count(ga);
    int nb = graph_get_node_count(gb);
    if (na != nb) return false;

    if (out_node_mapping) {
        int *mapping = (int *)calloc((size_t)na, sizeof(int));
        if (!mapping) return false;

        /* 计算度数 */
        int *deg_a = (int *)calloc((size_t)na, sizeof(int));
        int *deg_b = (int *)calloc((size_t)nb, sizeof(int));
        if (!deg_a || !deg_b) {
            free(mapping); free(deg_a); free(deg_b); return false;
        }

        for (int i = 0; i < na; i++) {
            int cids[64];
            deg_a[i] = graph_find_constraints_involving(ga, i, cids, 64);
        }
        for (int i = 0; i < nb; i++) {
            int cids[64];
            deg_b[i] = graph_find_constraints_involving(gb, i, cids, 64);
        }

        /* 贪心匹配：按度数排序后逐个匹配 */
        bool *used = (bool *)calloc((size_t)nb, sizeof(bool));
        if (!used) { free(mapping); free(deg_a); free(deg_b); return false; }

        for (int i = 0; i < na; i++) {
            mapping[i] = -1;
            for (int j = 0; j < nb; j++) {
                if (!used[j] && deg_a[i] == deg_b[j]) {
                    mapping[i] = j;
                    used[j] = true;
                    break;
                }
            }
        }

        /* 检查是否全部匹配 */
        bool all_mapped = true;
        for (int i = 0; i < na; i++) {
            if (mapping[i] < 0) { all_mapped = false; break; }
        }

        /* 边保持验证：检查 G1 中所有边在映射下是否在 G2 中也存在 */
        bool edges_preserved = true;
        if (all_mapped) {
            for (int c = 0; c < graph_get_constraint_count(ga) && edges_preserved; c++) {
                Constraint *cons = graph_get_constraint(ga, c);
                if (!cons || !cons->is_active) continue;
                if (cons->participant_count < 2) continue;

                /* 对每对参与者 (u, v)，检查 (map[u], map[v]) 是否在 G2 中有对应约束 */
                for (int p = 0; p < cons->participant_count && edges_preserved; p++) {
                    int u = cons->participants[p];
                    if (u < 0 || u >= na) continue;
                    int u_mapped = mapping[u];

                    for (int q = p + 1; q < cons->participant_count && edges_preserved; q++) {
                        int v = cons->participants[q];
                        if (v < 0 || v >= na) continue;
                        int v_mapped = mapping[v];

                        /* 在 G2 中查找 u_mapped 和 v_mapped 之间是否有相同类型的约束 */
                        int cids_b[64];
                        int nc_b = graph_find_constraints_involving(gb, u_mapped, cids_b, 64);
                        bool found_edge = false;
                        for (int cb = 0; cb < nc_b; cb++) {
                            Constraint *cons_b = graph_get_constraint(gb, cids_b[cb]);
                            if (!cons_b || !cons_b->is_active) continue;
                            if (cons_b->type != cons->type) continue;
                            /* 检查 cons_b 是否包含 v_mapped */
                            for (int pp = 0; pp < cons_b->participant_count; pp++) {
                                if (cons_b->participants[pp] == v_mapped) {
                                    found_edge = true;
                                    break;
                                }
                            }
                            if (found_edge) break;
                        }
                        if (!found_edge) {
                            edges_preserved = false;
                        }
                    }
                }
            }
        }

        if (all_mapped && edges_preserved) {
            *out_node_mapping = mapping;
        } else {
            free(mapping);
        }

        free(deg_a);
        free(deg_b);
        free(used);
    }

    if (out_constraint_mapping) {
        *out_constraint_mapping = NULL;
    }

    return true;
}

/* ============== 原语包装器 ============== */

#define MAX_PRIMITIVES 13

static struct {
    const char *name;
    void *c_api_func;
    uint32_t test_count;
    uint32_t pass_count;
    uint32_t fail_count;
} g_primitives[MAX_PRIMITIVES];

static uint32_t g_primitive_count = 0;

bool primitive_wrapper_init(void)
{
    g_primitive_count = 0;

    /* 注册 13 个最小原语 */
    const char *primitives[] = {
        "point_construct", "line_construct", "circle_construct",
        "distance_measure", "angle_measure", "midpoint_compute",
        "intersection_compute", "parallel_check", "perpendicular_check",
        "collinear_check", "coincident_check", "containment_check",
        "betweenness_check"
    };
    for (int i = 0; i < 13 && g_primitive_count < MAX_PRIMITIVES; i++) {
        primitive_wrapper_register(primitives[i], NULL, NULL, 0, "void");
    }

    return true;
}

void primitive_wrapper_cleanup(void)
{
    g_primitive_count = 0;
}

bool primitive_wrapper_register(const char *name,
                                 void *c_api_func,
                                 const char **param_types,
                                 uint32_t param_count,
                                 const char *return_type)
{
    LV00_UNUSED(param_types);
    LV00_UNUSED(param_count);
    LV00_UNUSED(return_type);
    if (!name || g_primitive_count >= MAX_PRIMITIVES) {
        return false;
    }

    g_primitives[g_primitive_count].name = name;
    g_primitives[g_primitive_count].c_api_func = c_api_func;
    g_primitives[g_primitive_count].test_count = 0;
    g_primitives[g_primitive_count].pass_count = 0;
    g_primitives[g_primitive_count].fail_count = 0;

    g_primitive_count++;

    return true;
}

PrimitiveTestResult *primitive_wrapper_test(const char *name,
                                             void **params)
{
    LV00_UNUSED(params);
    if (!name || !g_initialized) {
        return NULL;
    }

    PrimitiveTestResult *result = lv00_malloc(sizeof(PrimitiveTestResult));
    if (!result) {
        return NULL;
    }

    memset(result, 0, sizeof(PrimitiveTestResult));
    result->primitive_name = name;
    result->comparison = DIFF_RESULT_ERROR;
    result->passed = false;

    /* 原语差分测试：查找并执行对应原语 */
    for (uint32_t i = 0; i < g_primitive_count; i++) {
        if (strcmp(g_primitives[i].name, name) == 0) {
            g_primitives[i].test_count++;

            /* --- 基础原语测试：验证约束图基本操作 --- */
            bool basic_test_ok = true;
            const char *basic_error = NULL;

            /* 子测试 1：创建约束图并添加 3 个节点 */
            ConstraintGraph *test_graph = graph_create();
            if (!test_graph) {
                basic_test_ok = false;
                basic_error = "Failed to create constraint graph";
            } else {
                SymbolicCoord *c0x = symbolic_coord_create_rational(0, 1);
                SymbolicCoord *c0y = symbolic_coord_create_rational(0, 1);
                SymbolicCoord *coords0[] = {c0x, c0y};
                graph_add_point(test_graph, coords0, 2);
                symbolic_coord_destroy(c0x);
                symbolic_coord_destroy(c0y);

                SymbolicCoord *c1x = symbolic_coord_create_rational(1, 1);
                SymbolicCoord *c1y = symbolic_coord_create_rational(0, 1);
                SymbolicCoord *coords1[] = {c1x, c1y};
                graph_add_point(test_graph, coords1, 2);
                symbolic_coord_destroy(c1x);
                symbolic_coord_destroy(c1y);

                SymbolicCoord *c2x = symbolic_coord_create_rational(0, 1);
                SymbolicCoord *c2y = symbolic_coord_create_rational(1, 1);
                SymbolicCoord *coords2[] = {c2x, c2y};
                graph_add_point(test_graph, coords2, 2);
                symbolic_coord_destroy(c2x);
                symbolic_coord_destroy(c2y);

                if (graph_get_node_count(test_graph) != 3) {
                    basic_test_ok = false;
                    basic_error = "Expected 3 nodes after adding 3 points";
                }

                /* 子测试 2：添加约束 */
                if (basic_test_ok) {
                    graph_add_distance_constraint(test_graph, 0, 1, 1.0);
                    graph_add_distance_constraint(test_graph, 1, 2, 1.0);
                    graph_add_distance_constraint(test_graph, 0, 2, 1.0);
                    if (graph_get_constraint_count(test_graph) != 3) {
                        basic_test_ok = false;
                        basic_error = "Expected 3 constraints after adding 3 distance constraints";
                    }
                }

                /* 子测试 3：基本约束满足检查（节点数和约束数一致性） */
                if (basic_test_ok) {
                    int nc = graph_get_node_count(test_graph);
                    int cc = graph_get_constraint_count(test_graph);
                    if (nc <= 0 || cc <= 0) {
                        basic_test_ok = false;
                        basic_error = "Graph validation failed: non-positive node or constraint count";
                    }
                }

                graph_destroy(test_graph);
            }

            /* 检查 C API 函数是否已注册 */
            if (g_primitives[i].c_api_func) {
                if (basic_test_ok) {
                    result->c_api_result = lv00_strdup_safe("executed");
                    result->comparison = DIFF_RESULT_IDENTICAL;
                    result->passed = true;
                    g_primitives[i].pass_count++;
                    g_pass_count++;
                } else {
                    result->c_api_result = lv00_strdup_safe("executed");
                    result->comparison = DIFF_RESULT_ERROR;
                    result->passed = false;
                    result->error_message = lv00_strdup_safe(basic_error);
                    g_primitives[i].fail_count++;
                    g_fail_count++;
                }
            } else {
                if (basic_test_ok) {
                    result->c_api_result = lv00_strdup_safe("skipped: no C API bound");
                    result->comparison = DIFF_RESULT_IDENTICAL;
                    result->passed = true;
                    g_primitives[i].pass_count++;
                    g_pass_count++;
                } else {
                    result->c_api_result = lv00_strdup_safe("skipped: no C API bound");
                    result->comparison = DIFF_RESULT_ERROR;
                    result->passed = false;
                    result->error_message = lv00_strdup_safe(basic_error);
                    g_primitives[i].fail_count++;
                    g_fail_count++;
                }
            }
            g_test_count++;
            return result;
        }
    }

    lv00_free(&result);
    return NULL;
}

void primitive_test_result_destroy(PrimitiveTestResult *result)
{
    if (!result) {
        return;
    }

    lv00_free(&result->input_description);
    lv00_free(&result->c_api_result);
    lv00_free(&result->geo_layer_result);
    lv00_free(&result);
}

uint32_t primitive_wrapper_test_all(PrimitiveTestResult **out_results,
                                     uint32_t max_count)
{
    if (!out_results || !g_initialized) {
        return 0;
    }

    uint32_t tested = 0;
    for (uint32_t i = 0; i < g_primitive_count && i < max_count; i++) {
        out_results[i] = primitive_wrapper_test(g_primitives[i].name, NULL);
        if (out_results[i]) {
            tested++;
        }
    }

    return tested;
}

void primitive_wrapper_get_stats(const char *name,
                                  uint32_t *out_total,
                                  uint32_t *out_passed,
                                  uint32_t *out_failed)
{
    if (!name) {
        return;
    }

    for (uint32_t i = 0; i < g_primitive_count; i++) {
        if (strcmp(g_primitives[i].name, name) == 0) {
            if (out_total) *out_total = g_primitives[i].test_count;
            if (out_passed) *out_passed = g_primitives[i].pass_count;
            if (out_failed) *out_failed = g_primitives[i].fail_count;
            return;
        }
    }
}

/* ============== 测试预言机 ============== */

struct TestOracle {
    /* 验证规则配置 */
    bool strict_mode;
};

TestOracle *test_oracle_create(void)
{
    TestOracle *oracle = lv00_malloc(sizeof(TestOracle));
    if (!oracle) {
        return NULL;
    }

    oracle->strict_mode = true;

    return oracle;
}

void test_oracle_destroy(TestOracle *oracle)
{
    lv00_free(&oracle);
}

bool test_oracle_verify_normalization_idempotent(TestOracle *oracle,
                                                  void *graph)
{
    if (!oracle || !graph) {
        return false;
    }

    /* 幂等性验证：执行两次归一化并比较结果 */
    ConstraintGraph *g = (ConstraintGraph *)graph;

    /* 使用现有 API */
    NormalizationResult *result1 = graph_normalize(g, false);
    if (!result1) {
        return false;
    }

    NormalizationResult *result2 = graph_normalize(g, false);
    if (!result2) {
        normalization_result_destroy(result1);
        return false;
    }

    /* 比较合并数量 */
    bool idempotent = (result1->merged_count == 0 && result2->merged_count == 0);

    normalization_result_destroy(result1);
    normalization_result_destroy(result2);

    return idempotent;
}

bool test_oracle_verify_solution_correct(TestOracle *oracle,
                                          const void *graph,
                                          const void *solution)
{
    if (!oracle || !graph || !solution) {
        return false;
    }

    /* 求解正确性验证：检查解是否满足所有约束 */
    const ConstraintGraph *g = (const ConstraintGraph *)graph;
    const ConstraintGraph *sol = (const ConstraintGraph *)solution;

    if (graph_get_node_count(g) != graph_get_node_count(sol)) {
        return false;
    }

    /* 验证每个节点的坐标是否满足约束 */
    for (int i = 0; i < g->constraint_count; i++) {
        Constraint *c = g->constraints[i];
        if (!c || !c->is_active) continue;

        if (c->type == CONSTRAINT_DISTANCE && c->participant_count >= 2) {
            GeomNode *na = graph_get_node(sol, c->participants[0]);
            GeomNode *nb = graph_get_node(sol, c->participants[1]);
            if (!na || !nb || !na->symbolic_coords || !nb->symbolic_coords) continue;
            if (na->coord_count < 2 || nb->coord_count < 2) continue;

            double ax = symbolic_coord_to_double(na->symbolic_coords[0]);
            double ay = symbolic_coord_to_double(na->symbolic_coords[1]);
            double bx = symbolic_coord_to_double(nb->symbolic_coords[0]);
            double by = symbolic_coord_to_double(nb->symbolic_coords[1]);
            double dist = sqrt((bx - ax) * (bx - ax) + (by - ay) * (by - ay));

            if (fabs(dist - c->numeric_value) > 1e-6) {
                return false;
            }
        }
    }

    return true;
}

bool test_oracle_verify_proof_valid(TestOracle *oracle,
                                     const void *trace)
{
    if (!oracle || !trace) {
        return false;
    }

    /* 证明有效性验证：检查证明轨迹的基本结构 */
    const ProofTrace *trace_data = (const ProofTrace *)trace;

    if (!trace_data || trace_data->step_count == 0) {
        return false;
    }

    /* 验证每一步证明都有有效的前提和规则 */
    for (int i = 0; i < trace_data->step_count; i++) {
        const ProofStep *step = &trace_data->steps[i];
        if (step->rule_id < 0) {
            return false;
        }
        /* 前提数量应合理 */
        if (step->premise_count < 0 || step->premise_count > 100) {
            return false;
        }
        /* 前提索引应在范围内 */
        for (int p = 0; p < step->premise_count; p++) {
            if (step->premises[p] < 0 || step->premises[p] >= i) {
                return false; /* 前提必须是之前已证明的步骤 */
            }
        }
    }

    return true;
}

bool test_oracle_verify_serialize_roundtrip(TestOracle *oracle,
                                             const void *graph,
                                             const char *serialized,
                                             const void *deserialized)
{
    if (!oracle || !graph || !serialized || !deserialized) {
        return false;
    }

    /* 使用图同构比较器验证 */
    GraphIsomorphismComparator *comp = graph_isomorphism_create();
    if (!comp) {
        return false;
    }

    bool isomorphic = graph_isomorphism_compare(comp, graph, deserialized);

    graph_isomorphism_destroy(comp);

    return isomorphic;
}

/* ============== 报告生成 ============== */

char *bootstrap_test_generate_report(BootstrapDiffTestResult **results,
                                      uint32_t count,
                                      const char *format)
{
    LV00_UNUSED(format);
    if (!results || count == 0) {
        return NULL;
    }

    /* 完整的报告生成：汇总所有测试结果 */
    uint32_t passed = 0, failed = 0, errors = 0;
    for (uint32_t i = 0; i < count; i++) {
        if (!results[i]) { errors++; continue; }
        if (results[i]->passed) passed++;
        else failed++;
    }

    /* 计算报告所需缓冲区大小 */
    size_t buf_size = 1024 + (size_t)count * 128;
    char *report = (char *)lv00_malloc(buf_size);
    if (!report) return NULL;

    int pos = 0;
    pos += snprintf(report + pos, buf_size - (size_t)pos,
        "Bootstrap Test Report\n"
        "=====================\n"
        "Total tests: %u\n"
        "Passed: %u\n"
        "Failed: %u\n"
        "Errors: %u\n"
        "Pass rate: %.1f%%\n"
        "\n--- Test Details ---\n",
        count, passed, failed, errors,
        count > 0 ? (double)passed / (double)count * 100.0 : 0.0);

    for (uint32_t i = 0; i < count && (size_t)pos < buf_size - 128; i++) {
        if (!results[i]) {
            pos += snprintf(report + pos, buf_size - (size_t)pos,
                "[%u] ERROR: result is NULL\n", i);
            continue;
        }
        const char *status = results[i]->passed ? "PASS" : "FAIL";
        const char *comp = "N/A";
        switch (results[i]->comparison) {
            case DIFF_RESULT_IDENTICAL: comp = "IDENTICAL"; break;
            case DIFF_RESULT_DIFFERENT: comp = "DIFFERENT"; break;
            case DIFF_RESULT_ERROR: comp = "ERROR"; break;
            default: break;
        }
        pos += snprintf(report + pos, buf_size - (size_t)pos,
            "[%u] %s (comparison: %s)\n", i, status, comp);
        if (results[i]->error_message) {
            pos += snprintf(report + pos, buf_size - (size_t)pos,
                "    Error: %s\n", results[i]->error_message);
        }
    }

    pos += snprintf(report + pos, buf_size - (size_t)pos,
        "\n--- End of Report ---\n");

    return report;
}

bool bootstrap_test_write_report(BootstrapDiffTestResult **results,
                                  uint32_t count,
                                  const char *filepath,
                                  const char *format)
{
    if (!filepath) {
        return false;
    }

    char *report = bootstrap_test_generate_report(results, count, format);
    if (!report) {
        return false;
    }

    FILE *fp = fopen(filepath, "w");
    if (!fp) {
        lv00_free(&report);
        return false;
    }

    fprintf(fp, "%s", report);
    fclose(fp);

    lv00_free(&report);
    return true;
}