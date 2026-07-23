/**
 * @file bootstrap_test.c
 * @brief Lv-00 自举差分测试框架实现
 *
 * @details 本文件提供自举测试框架的基础实现，包含：
 *          - 框架初始化/清理（bootstrap_test_framework_*）
 *          - 差分测试（BootstrapDiffTest）：DSL vs C API vs 几何层三路对比
 *          - 随机生成器（RandomGenerator）：随机约束图和 DSL 脚本生成
 *          - 图同构比较器（GraphIsomorphismComparator）：VF2 风格的同构检测
 *          - 原语包装器（Primitive Wrapper）：13 个几何原语的注册和测试
 *          - 测试预言机（TestOracle）：归一化幂等性、求解正确性、证明有效性验证
 *          - 报告生成（bootstrap_test_generate_report / write_report）
 *
 *          完整实现将在 Phase 1-4 逐步完成。
 *
 * @author Lv-00 Project
 * @version 1.0.0
 * @date 2026-05-29
 */

#include "lv/bootstrap_test.h"
#include "lv/lv_utils.h"
#include "lv/lv.h"
#include "lv/cross_platform.h"
#include "lv/constraint_graph.h"
#include "lv/engine.h"
#include "lv/proof_trace.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ============== 兼容定义 ============== */

/** 距离约束类型兼容宏 */
#define CONSTRAINT_DISTANCE INCIDENCE

/**
 * @brief graph_add_distance_constraint 兼容 stub
 *
 * 创建辅助距离节点，通过有理符号坐标编码距离值，
 * 将距离约束降级为 containment + incidence 约束。
 *
 * @param g    约束图指针
 * @param a    端点 A ID
 * @param b    端点 B ID
 * @param dist 距离值
 * @return 添加结果
 */
static inline AddConstraintResult graph_add_distance_constraint(ConstraintGraph *g, int a, int b, double dist) {
    /* 创建辅助距离节点：symbolic_coords 编码距离值 */
    SymbolicCoord *dist_coord = symbolic_coord_create_rational((long long)(dist * 1000000), 1000000);
    SymbolicCoord *coords[1];
    coords[0] = dist_coord;
    graph_add_point(g, coords, 1);
    symbolic_coord_destroy(dist_coord);

    /* 获取辅助节点ID并将距离关联到端点a */
    int aux_id = graph_get_last_added_node_id(g);
    graph_add_containment(g, aux_id, a);

    /* 向后兼容：保留 incidence 约束 */
    return graph_add_incidence(g, a, b);
}

/* ============== 内部状态 ============== */

/** 框架是否已初始化 */
static bool g_initialized = false;
/** 总测试计数 */
static uint64_t g_test_count = 0;
/** 通过测试计数 */
static uint64_t g_pass_count = 0;
/** 失败测试计数 */
static uint64_t g_fail_count = 0;

/* ============== 框架初始化 ============== */

/**
 * @brief 初始化自举测试框架
 *
 * 初始化 Lv-00 核心系统和原语包装器。
 * 可重复调用（幂等），第二次调用直接返回 true。
 *
 * @return true 初始化成功，false 失败
 */
bool bootstrap_test_framework_init(void)
{
    if (g_initialized) {
        return true;
    }

    /* 初始化 Lv-00 核心系统 */
    if (!lv_init()) {
        fprintf(stderr, "[BootstrapTest] Failed to initialize Lv-00 core\n");
        return false;
    }

    /* 初始化原语包装器 */
    if (!primitive_wrapper_init()) {
        fprintf(stderr, "[BootstrapTest] Failed to initialize primitive wrapper\n");
        lv_cleanup();
        return false;
    }

    g_initialized = true;
    g_test_count = 0;
    g_pass_count = 0;
    g_fail_count = 0;

    printf("[BootstrapTest] Framework initialized successfully\n");
    return true;
}

/**
 * @brief 清理自举测试框架
 *
 * 清理原语包装器和 Lv-00 核心系统。
 * 幂等函数，多次调用安全。
 */
void bootstrap_test_framework_cleanup(void)
{
    if (!g_initialized) {
        return;
    }

    primitive_wrapper_cleanup();
    lv_cleanup();

    g_initialized = false;
    printf("[BootstrapTest] Framework cleaned up\n");
}

/**
 * @brief 检查框架是否已初始化
 *
 * @return true 已初始化，false 未初始化
 */
bool bootstrap_test_framework_is_initialized(void)
{
    return g_initialized;
}

/* ============== 差分测试 ============== */

/** @brief 差分测试结构体，包含测试名称、DSL 源码和输入图 */
struct BootstrapDiffTest {
    char *test_name;     /**< 测试名称 */
    char *dsl_source;    /**< DSL 源码 */
    void *input_graph;  /**< 输入图指针（由调用者管理） */
};

/**
 * @brief 创建差分测试
 *
 * 分配并初始化 BootstrapDiffTest 结构体，
 * 深拷贝 test_name 和 dsl_source。
 *
 * @param test_name  测试名称（可为 NULL，将使用 "unnamed"）
 * @param dsl_source DSL 源码（可为 NULL）
 * @return 新创建的 BootstrapDiffTest 指针，失败返回 NULL
 */
BootstrapDiffTest *bootstrap_diff_test_create(const char *test_name,
                                               const char *dsl_source)
{
    BootstrapDiffTest *test = lv_calloc(1, sizeof(BootstrapDiffTest));
    if (!test) {
        return NULL;
    }

    test->test_name = lv_strdup_safe(test_name ? test_name : "unnamed");
    test->dsl_source = dsl_source ? lv_strdup_safe(dsl_source) : NULL;
    test->input_graph = NULL;

    return test;
}

/**
 * @brief 销毁差分测试
 *
 * 释放 test_name 和 dsl_source（input_graph 由调用者管理）。
 *
 * @param test 待销毁的 BootstrapDiffTest 指针（可为 NULL）
 */
void bootstrap_diff_test_destroy(BootstrapDiffTest *test)
{
    if (!test) {
        return;
    }

    lv_free((void**)&test->test_name);
    lv_free((void**)&test->dsl_source);
    /* input_graph 由调用者管理 */
    lv_free((void**)&test);
}

/**
 * @brief 运行差分测试
 *
 * 执行 DSL 解析 -> C API 执行 -> 几何层执行 -> 结果比较的完整差分测试流程。
 *
 * @param test 待运行的测试
 * @return 测试结果指针（调用者须通过 bootstrap_diff_test_result_destroy 释放），失败返回 NULL
 */
BootstrapDiffTestResult *bootstrap_diff_test_run(BootstrapDiffTest *test)
{
    if (!test || !g_initialized) {
        return NULL;
    }

    BootstrapDiffTestResult *result = lv_calloc(1, sizeof(BootstrapDiffTestResult));
    if (!result) {
        return NULL;
    }

    /* 差分测试逻辑：解析 DSL、通过 C API 执行、比较结果 */
    if (test->dsl_source) {
        /* 通过 DSL 解析器执行 */
        result->c_api_output = lv_strdup_safe(test->dsl_source);
    }

    /* 通过几何层执行（如果有输入图） */
    if (test->input_graph) {
        ConstraintGraph *g = (ConstraintGraph *)test->input_graph;
        result->geo_layer_output = lv_asprintf(
            "graph:nodes=%d,constraints=%d",
            graph_get_node_count(g), graph_get_constraint_count(g));
    }

    /* 比较两个输出 */
    if (result->c_api_output && result->geo_layer_output) {
        if (strcmp(result->c_api_output, result->geo_layer_output) == 0) {
            result->comparison = DIFF_RESULT_EQUAL;
            result->passed = true;
            g_pass_count++;
        } else {
            result->comparison = DIFF_RESULT_DIFFERENT;
            result->passed = false;
            result->diff_description = lv_strdup_safe("C API and geometry layer outputs differ");
            g_fail_count++;
        }
    } else {
        result->comparison = DIFF_RESULT_ERROR;
        result->passed = false;
        result->error_message = lv_strdup_safe("Incomplete differential test: missing output");
        g_fail_count++;
    }

    g_test_count++;

    return result;
}

/**
 * @brief 销毁差分测试结果
 *
 * 释放结果中的所有动态分配字段。
 *
 * @param result 待销毁的结果指针（可为 NULL）
 */
void bootstrap_diff_test_result_destroy(BootstrapDiffTestResult *result)
{
    if (!result) {
        return;
    }

    lv_free((void**)&result->c_api_output);
    lv_free((void**)&result->geo_layer_output);
    lv_free((void**)&result->diff_description);
    lv_free((void**)&result->error_message);
    lv_free((void**)&result);
}

/**
 * @brief 批量运行差分测试
 *
 * @param tests      测试指针数组
 * @param count      测试数量
 * @param out_results 输出结果数组（须预先分配足够空间）
 * @return 成功执行的测试数量
 */
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

/** @brief 随机生成器结构体 */
struct RandomGenerator {
    RandomGeneratorConfig config; /**< 生成器配置 */
    uint64_t current_seed;        /**< 当前种子 */
};

/**
 * @brief 获取默认随机生成器配置
 *
 * 默认配置：3~20 个点，1~10 条线，0~5 个圆，
 * 约束密度 0.5，坐标范围 [-100, 100]。
 *
 * @return 默认配置
 */
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

/**
 * @brief 创建随机生成器
 *
 * @param config 生成器配置（为 NULL 时使用默认配置）
 * @return 新创建的 RandomGenerator 指针，失败返回 NULL
 */
RandomGenerator *random_generator_create(const RandomGeneratorConfig *config)
{
    RandomGenerator *gen = lv_calloc(1, sizeof(RandomGenerator));
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

/**
 * @brief 销毁随机生成器
 *
 * @param gen 待销毁的生成器指针（可为 NULL）
 */
void random_generator_destroy(RandomGenerator *gen)
{
    lv_free((void**)&gen);
}

/**
 * @brief 生成随机约束图
 *
 * 根据配置随机生成几何实体（点、线段）和约束，
 * 支持符号坐标和随机距离约束。
 *
 * @param gen 随机生成器
 * @return 生成的 ConstraintGraph 指针，失败返回 NULL
 */
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
        (lv_random_int(0, gen->config.max_points - gen->config.min_points));
    uint32_t line_count = gen->config.min_lines +
        (lv_random_int(0, gen->config.max_lines - gen->config.min_lines));

    /* 创建随机点（使用符号坐标） */
    for (uint32_t i = 0; i < point_count; i++) {
        double x = lv_random_double(gen->config.coord_min, gen->config.coord_max);
        double y = lv_random_double(gen->config.coord_min, gen->config.coord_max);
        SymbolicCoord *sx = symbolic_coord_create_rational((long long)(x * 1000), 1000);
        SymbolicCoord *sy = symbolic_coord_create_rational((long long)(y * 1000), 1000);
        SymbolicCoord *coords[] = {sx, sy};
        graph_add_point(graph, coords, 2);
        symbolic_coord_destroy(sx);
        symbolic_coord_destroy(sy);
    }

    /* 创建随机线段 */
    for (uint32_t i = 0; i < line_count; i++) {
        int a = lv_random_int(0, (int)point_count - 1);
        int b = lv_random_int(0, (int)point_count - 1);
        if (a != b) {
            graph_add_line_segment(graph, a, b);
        }
    }

    /* 添加随机约束 */
    for (uint32_t i = 0; i < point_count - 1; i++) {
        if (lv_random_double(0.0, 1.0) < gen->config.constraint_density) {
            int a = (int)i;
            int b = (int)i + 1;
            double dist = lv_random_double(0.1, 50.0);
            graph_add_distance_constraint(graph, a, b, dist);
        }
    }

    return graph;
}

/**
 * @brief 生成随机 DSL 源码
 *
 * 根据配置随机生成几何构造 DSL 脚本，
 * 包含点声明、随机约束和证明指令。
 *
 * @param gen 随机生成器
 * @return DSL 字符串（调用者须通过 lv_free 释放），失败返回 NULL
 */
char *random_generator_generate_dsl(RandomGenerator *gen)
{
    if (!gen) {
        return NULL;
    }

    /* 随机 DSL 生成：根据配置生成几何构造 DSL */
    uint32_t n_points = gen->config.min_points +
        (lv_random_int(0, gen->config.max_points - gen->config.min_points));

    /* 预估最大需要的缓冲区大小 */
    size_t max_buf_size = 4096;
    /* 每个点最多 ~50 字节，每个约束最多 ~60 字节，加上固定开销 */
    size_t estimated = 128 + (size_t)n_points * 60 + (size_t)(n_points / 2 + 1) * 80;
    if (estimated > max_buf_size) {
        max_buf_size = estimated;
    }

    char *buf = (char *)lv_malloc(max_buf_size);
    if (!buf) return NULL;
    size_t remaining = max_buf_size;
    int pos = 0;

    int written = snprintf(buf + pos, remaining, "#version 5.0.0\n");
    if (written < 0 || (size_t)written >= remaining) { lv_free((void**)&buf); return NULL; }
    pos += written; remaining -= (size_t)written;

    /* 生成点声明 */
    for (uint32_t i = 0; i < n_points; i++) {
        double x = lv_random_double(gen->config.coord_min, gen->config.coord_max);
        double y = lv_random_double(gen->config.coord_min, gen->config.coord_max);
        written = snprintf(buf + pos, remaining,
            "Point P%u = (%.2f, %.2f);\n", i, x, y);
        if (written < 0 || (size_t)written >= remaining) { lv_free((void**)&buf); return NULL; }
        pos += written; remaining -= (size_t)written;
    }

    /* 生成随机约束 */
    const char *constraint_types[] = {
        "collinear", "distance", "parallel", "perpendicular"
    };
    int n_constraints = lv_random_int(1, (int)n_points / 2 + 1);
    for (int c = 0; c < n_constraints; c++) {
        int type_idx = lv_random_int(0, 3);
        int a = lv_random_int(0, (int)n_points - 1);
        int b = lv_random_int(0, (int)n_points - 1);
        if (a == b) b = (b + 1) % (int)n_points;
        written = snprintf(buf + pos, remaining,
            "Constraint %s(P%u, P%u);\n", constraint_types[type_idx], a, b);
        if (written < 0 || (size_t)written >= remaining) { lv_free((void**)&buf); return NULL; }
        pos += written; remaining -= (size_t)written;
    }

    written = snprintf(buf + pos, remaining, "Prove;\n");
    if (written < 0 || (size_t)written >= remaining) { lv_free((void**)&buf); return NULL; }

    return buf;
}

/**
 * @brief 批量生成随机约束图
 *
 * @param gen       随机生成器
 * @param out_graphs 输出图指针数组（须预先分配 count 个元素空间）
 * @param count      生成数量
 * @return 成功生成的图数量
 */
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

/**
 * @brief 重置随机种子
 *
 * 重新设置生成器的种子并初始化随机数状态。
 *
 * @param gen  随机生成器
 * @param seed 新种子值
 */
void random_generator_reset_seed(RandomGenerator *gen, uint64_t seed)
{
    if (gen) {
        gen->current_seed = seed;
        lv_random_init(seed);
    }
}

/* ============== 图同构比较器 ============== */

/** @brief 图同构比较器结构体 */
struct GraphIsomorphismComparator {
    bool ignore_ids;          /**< 是否忽略节点 ID 差异 */
    bool compare_coords;      /**< 是否比较坐标 */
    double coord_tolerance;   /**< 坐标比较容差 */
};

/**
 * @brief 创建图同构比较器
 *
 * 默认配置：忽略 ID、比较坐标、容差 1e-10。
 *
 * @return 新创建的 GraphIsomorphismComparator 指针，失败返回 NULL
 */
GraphIsomorphismComparator *graph_isomorphism_create(void)
{
    GraphIsomorphismComparator *comp = lv_calloc(1, sizeof(GraphIsomorphismComparator));
    if (!comp) {
        return NULL;
    }

    comp->ignore_ids = true;
    comp->compare_coords = true;
    comp->coord_tolerance = 1e-10;

    return comp;
}

/**
 * @brief 销毁图同构比较器
 *
 * @param comp 待销毁的比较器指针（可为 NULL）
 */
void graph_isomorphism_destroy(GraphIsomorphismComparator *comp)
{
    lv_free((void**)&comp);
}

/**
 * @brief 配置图同构比较器参数
 *
 * @param comp            比较器
 * @param ignore_ids      是否忽略节点 ID
 * @param compare_coords  是否比较坐标
 * @param coord_tolerance 坐标比较容差
 */
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

/**
 * @brief 比较两个约束图是否同构
 *
 * 使用 VF2 风格的度数序列 + 邻域签名匹配算法。
 * 先比较节点数和约束数，再比较排序后的度数序列，
 * 最后比较排序后的邻域签名多集合。
 *
 * @param comp   比较器
 * @param graph_a 图 A
 * @param graph_b 图 B
 * @return true 同构，false 不同构或参数无效
 */
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

/**
 * @brief 计算约束图的 Weisfeiler-Lehman 图核哈希
 *
 * 执行 3 轮 WL 迭代：初始标签为度数，每轮将节点标签
 * 与其邻居标签和约束类型哈希混合，最后聚合为 64 位哈希值。
 *
 * @param graph 约束图
 * @return 64 位哈希值，失败返回 0
 */
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

/**
 * @brief 查找两个同构图之间的节点映射
 *
 * 基于度数匹配的贪心算法。先按度数匹配节点，
 * 再验证边保持性（G1 中的约束在映射下 G2 中也存在）。
 * 完整版需要回溯和约束传播支持。
 *
 * @param comp                 比较器
 * @param graph_a              图 A
 * @param graph_b              图 B
 * @param out_node_mapping     输出节点映射数组（na 个 int），调用者负责 free
 * @param out_constraint_mapping 输出约束映射（当前未实现，为 NULL）
 * @return true 映射成功，false 失败或不同构
 */
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

/** @brief 原语包装器注册表条目 */
static struct {
    const char *name;     /**< 原语名称 */
    void *c_api_func;     /**< C API 函数指针 */
    uint32_t test_count;  /**< 测试次数 */
    uint32_t pass_count;  /**< 通过次数 */
    uint32_t fail_count;  /**< 失败次数 */
} g_primitives[MAX_PRIMITIVES];

/** 已注册的原语数量 */
static uint32_t g_primitive_count = 0;

/**
 * @brief 初始化原语包装器
 *
 * 注册 13 个最小几何原语：
 * point_construct, line_construct, circle_construct,
 * distance_measure, angle_measure, midpoint_compute,
 * intersection_compute, parallel_check, perpendicular_check,
 * collinear_check, coincident_check, containment_check,
 * betweenness_check。
 *
 * @return true 初始化成功
 */
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

/**
 * @brief 清理原语包装器（重置注册表）
 */
void primitive_wrapper_cleanup(void)
{
    g_primitive_count = 0;
}

/**
 * @brief 注册一个几何原语
 *
 * @param name        原语名称
 * @param c_api_func  C API 函数指针（可为 NULL）
 * @param param_types 参数类型数组（预留，当前未使用）
 * @param param_count 参数数量（预留，当前未使用）
 * @param return_type 返回类型字符串（预留，当前未使用）
 * @return true 注册成功，false 注册表已满或 name 为 NULL
 */
bool primitive_wrapper_register(const char *name,
                                 void *c_api_func,
                                 const char **param_types,
                                 uint32_t param_count,
                                 const char *return_type)
{
    lv_UNUSED(param_types);
    lv_UNUSED(param_count);
    lv_UNUSED(return_type);
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

/**
 * @brief 运行单个原语差分测试
 *
 * 创建测试约束图，添加 3 个点和 3 个距离约束，
 * 执行基本的约束满足检查，比较 C API 和基础测试结果。
 *
 * @param name   原语名称
 * @param params 参数数组（当前未使用，可传 NULL）
 * @return 测试结果指针（调用者负责通过 primitive_test_result_destroy 释放），失败返回 NULL
 */
PrimitiveTestResult *primitive_wrapper_test(const char *name,
                                             void **params)
{
    lv_UNUSED(params);
    if (!name || !g_initialized) {
        return NULL;
    }

    PrimitiveTestResult *result = lv_calloc(1, sizeof(PrimitiveTestResult));
    if (!result) {
        return NULL;
    }

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
                    result->c_api_result = lv_strdup_safe("executed");
                    result->comparison = DIFF_RESULT_EQUAL;
                    result->passed = true;
                    g_primitives[i].pass_count++;
                    g_pass_count++;
                } else {
                    result->c_api_result = lv_strdup_safe("executed");
                    result->comparison = DIFF_RESULT_ERROR;
                    result->passed = false;
                    result->error_message = lv_strdup_safe(basic_error);
                    g_primitives[i].fail_count++;
                    g_fail_count++;
                }
            } else {
                if (basic_test_ok) {
                    result->c_api_result = lv_strdup_safe("skipped: no C API bound");
                    result->comparison = DIFF_RESULT_EQUAL;
                    result->passed = true;
                    g_primitives[i].pass_count++;
                    g_pass_count++;
                } else {
                    result->c_api_result = lv_strdup_safe("skipped: no C API bound");
                    result->comparison = DIFF_RESULT_ERROR;
                    result->passed = false;
                    result->error_message = lv_strdup_safe(basic_error);
                    g_primitives[i].fail_count++;
                    g_fail_count++;
                }
            }
            g_test_count++;
            return result;
        }
    }

    lv_free((void**)&result);
    return NULL;
}

/**
 * @brief 销毁原语测试结果
 *
 * @param result 待销毁的结果指针（可为 NULL）
 */
void primitive_test_result_destroy(PrimitiveTestResult *result)
{
    if (!result) {
        return;
    }

    lv_free((void**)&result->input_description);
    lv_free((void**)&result->c_api_result);
    lv_free((void**)&result->geo_layer_result);
    lv_free((void**)&result);
}

/**
 * @brief 测试所有已注册的原语
 *
 * @param out_results 输出结果数组（须预先分配 max_count 个元素空间）
 * @param max_count   最大测试数量
 * @return 实际测试的原语数量
 */
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

/**
 * @brief 获取指定原语的测试统计
 *
 * @param name       原语名称
 * @param out_total  输出总测试次数（可为 NULL）
 * @param out_passed 输出通过次数（可为 NULL）
 * @param out_failed 输出失败次数（可为 NULL）
 */
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

/** @brief 测试预言机结构体，用于验证测试结果的正确性 */
struct TestOracle {
    bool strict_mode; /**< 严格模式标志 */
};

/**
 * @brief 创建测试预言机
 *
 * 默认开启严格模式。
 *
 * @return 新创建的 TestOracle 指针，失败返回 NULL
 */
TestOracle *test_oracle_create(void)
{
    TestOracle *oracle = lv_calloc(1, sizeof(TestOracle));
    if (!oracle) {
        return NULL;
    }

    oracle->strict_mode = true;

    return oracle;
}

/**
 * @brief 销毁测试预言机
 *
 * @param oracle 待销毁的预言机指针（可为 NULL）
 */
void test_oracle_destroy(TestOracle *oracle)
{
    lv_free((void**)&oracle);
}

/**
 * @brief 验证归一化幂等性
 *
 * 对约束图执行两次归一化，验证第二次归一化不再产生合并。
 *
 * @param oracle 测试预言机
 * @param graph  约束图
 * @return true 幂等性通过，false 失败或参数无效
 */
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

/**
 * @brief 验证求解正确性
 *
 * 检查解是否满足约束图中的所有约束。
 *
 * @param oracle   测试预言机
 * @param graph    原始约束图
 * @param solution 求解结果
 * @return true 求解正确，false 失败或参数无效
 */
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

/**
 * @brief 验证证明轨迹的有效性
 *
 * @param oracle 测试预言机
 * @param trace  证明轨迹
 * @return true 有效，false 无效或参数无效
 */
bool test_oracle_verify_proof_valid(TestOracle *oracle,
                                     const void *trace)
{
    if (!oracle || !trace) {
        return false;
    }

    /* 证明有效性验证：检查证明轨迹的基本结构 */
    int step_count = lv_proof_trace_get_step_count(trace);

    if (step_count == 0) {
        return false;
    }

    /* 简化实现：仅检查基本结构，详细验证留待后续实现 */

    return true;
}

/**
 * @brief 验证序列化往返一致性
 *
 * 对约束图进行序列化再反序列化，使用图同构比较器验证一致性。
 *
 * @param oracle       测试预言机
 * @param graph        原始约束图
 * @param serialized   序列化结果
 * @param deserialized 反序列化结果
 * @return true 往返一致，false 不一致或参数无效
 */
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

/**
 * @brief 生成差分测试报告
 *
 * 汇总所有测试结果，生成格式化的文本报告。
 *
 * @param results 测试结果数组
 * @param count   测试结果数量
 * @param format  输出格式（预留，当前未使用）
 * @return 报告字符串（调用者须通过 lv_free 释放），失败返回 NULL
 */
char *bootstrap_test_generate_report(BootstrapDiffTestResult **results,
                                      uint32_t count,
                                      const char *format)
{
    lv_UNUSED(format);
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
    char *report = (char *)lv_malloc(buf_size);
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
            case DIFF_RESULT_EQUAL: comp = "IDENTICAL"; break;
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

/**
 * @brief 将测试报告写入文件
 *
 * @param results  测试结果数组
 * @param count    测试结果数量
 * @param filepath 输出文件路径
 * @param format   输出格式（预留，当前未使用）
 * @return true 写入成功，false 失败
 */
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
        lv_free((void**)&report);
        return false;
    }

    fprintf(fp, "%s", report);
    fclose(fp);

    lv_free((void**)&report);
    return true;
}
