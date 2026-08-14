/**
 * @file bootstrap_test_random.c
 * @brief Lv-00 自举差分测试框架 —— 随机生成器
 *
 * @details 由 bootstrap_test.c 按功能组件拆分而来。
 *          共享兼容定义与框架状态见 bootstrap_test_internal.h。
 *
 * @author Lv-00 Project
 * @version 1.0.0
 */

#include "lv/bootstrap_test.h"
#include "lv/lv_log.h"

#include "lv/lv_file.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "lv/constraint_graph.h"
#include "lv/cross_platform.h"
#include "lv/engine.h"
#include "lv/lv_strbuf.h"
#include "lv/lv_utils.h"
#include "lv/proof_trace.h"
#include "lv/lv_internal.h"

#include "bootstrap_test_internal.h"

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
RandomGeneratorConfig random_generator_default_config(void) {
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

    config.seed = (uint64_t) time(NULL);

    return config;
}

/**
 * @brief 创建随机生成器
 *
 * @param config 生成器配置（为 NULL 时使用默认配置）
 * @return 新创建的 RandomGenerator 指针，失败返回 NULL
 */
RandomGenerator *random_generator_create(const RandomGeneratorConfig *config) {
    RandomGenerator *gen = lv_calloc(1, sizeof(RandomGenerator));
    if (!gen) {
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "random_generator_create: calloc failed");
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
void random_generator_destroy(RandomGenerator *gen) {
    lv_free((void **) &gen);
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
void *random_generator_generate_graph(RandomGenerator *gen) {
    if (!gen) {
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "random_generator_generate_graph: gen is NULL");
    }

    /* 随机图生成：创建随机几何实体和约束 */
    ConstraintGraph *graph = graph_create();
    if (!graph) {
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "random_generator_generate_graph: graph_create failed");
    }

    /* 确定实体数量 */
    uint32_t point_count = gen->config.min_points + (lv_random_int(0, gen->config.max_points - gen->config.min_points));
    uint32_t line_count = gen->config.min_lines + (lv_random_int(0, gen->config.max_lines - gen->config.min_lines));

    /* 创建随机点（使用符号坐标） */
    for (uint32_t i = 0; i < point_count; i++) {
        double x = lv_random_double(gen->config.coord_min, gen->config.coord_max);
        double y = lv_random_double(gen->config.coord_min, gen->config.coord_max);
        SymbolicCoord *sx = symbolic_coord_create_rational((long long) (x * 1000), 1000);
        SymbolicCoord *sy = symbolic_coord_create_rational((long long) (y * 1000), 1000);
        SymbolicCoord *coords[] = {sx, sy};
        graph_add_point(graph, coords, 2);
        symbolic_coord_destroy(sx);
        symbolic_coord_destroy(sy);
    }

    /* 创建随机线段 */
    for (uint32_t i = 0; i < line_count; i++) {
        int a = lv_random_int(0, (int) point_count - 1);
        int b = lv_random_int(0, (int) point_count - 1);
        if (a != b) {
            graph_add_line_segment(graph, a, b);
        }
    }

    /* 添加随机约束 */
    for (uint32_t i = 0; i < point_count - 1; i++) {
        if (lv_random_double(0.0, 1.0) < gen->config.constraint_density) {
            int a = (int) i;
            int b = (int) i + 1;
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
char *random_generator_generate_dsl(RandomGenerator *gen) {
    if (!gen) {
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "random_generator_generate_dsl: gen is NULL");
    }

    /* 随机 DSL 生成：根据配置生成几何构造 DSL */
    uint32_t n_points = gen->config.min_points + (lv_random_int(0, gen->config.max_points - gen->config.min_points));

    /* 使用 lvStrBuf 动态构建 DSL，替代预估 buffer + pos/remaining 游标 */
    lvStrBuf sb = {0};

    lv_strbuf_printf(&sb, "#version 5.0.0\n");

    /* 生成点声明 */
    for (uint32_t i = 0; i < n_points; i++) {
        double x = lv_random_double(gen->config.coord_min, gen->config.coord_max);
        double y = lv_random_double(gen->config.coord_min, gen->config.coord_max);
        lv_strbuf_printf(&sb, "Point P%u = (%.2f, %.2f);\n", i, x, y);
    }

    /* 生成随机约束 */
    const char *constraint_types[] = {"collinear", "distance", "parallel", "perpendicular"};
    int n_constraints = lv_random_int(1, (int) n_points / 2 + 1);
    for (int c = 0; c < n_constraints; c++) {
        int type_idx = lv_random_int(0, 3);
        int a = lv_random_int(0, (int) n_points - 1);
        int b = lv_random_int(0, (int) n_points - 1);
        if (a == b)
            b = (b + 1) % (int) n_points;
        lv_strbuf_printf(&sb, "Constraint %s(P%u, P%u);\n", constraint_types[type_idx], a, b);
    }

    lv_strbuf_printf(&sb, "Prove;\n");

    /* 转换为堆分配字符串并清理（调用者 lv_free） */
    return lv_strbuf_to_string(&sb);
}

/**
 * @brief 批量生成随机约束图
 *
 * @param gen       随机生成器
 * @param out_graphs 输出图指针数组（须预先分配 count 个元素空间）
 * @param count      生成数量
 * @return 成功生成的图数量
 */
uint32_t random_generator_generate_batch(RandomGenerator *gen, void **out_graphs, uint32_t count) {
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
void random_generator_reset_seed(RandomGenerator *gen, uint64_t seed) {
    if (gen) {
        gen->current_seed = seed;
        lv_random_init(seed);
    }
}

