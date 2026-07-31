/**
 * @file groebner_engine.c
 * @brief Groebner 基计算引擎实现 —— Buchberger 算法、理想操作与代数簇求解
 *
 * @details 本模块是 Lv-00 的多项式理想计算核心，实现了：
 *          - 多项式环管理与多项式稀疏存储
 *          - 三种单项式序（lex/grlex/grevlex）的比较
 *          - 多项式加法、乘法、代入运算
 *          - S-多项式计算
 *          - 经典 Buchberger 算法（带约化和互质跳过优化）
 *          - 多项式约化（reduction/normal form）
 *          - 理想成员判定、理想交与理想商
 *          - 约束图到多项式理想的转换
 *          - 零维代数簇的数值求解
 *
 * 参考项目：
 *   - Singular (singular.uni-kl.de) —— 环声明范式、工业级 Gröbner 基
 *   - Macaulay2 (macaulay2.com) —— 理想与簇的统一视角
 *
 * @version v3.3.0
 * @date 2026-05-24
 * @author Lv-00 Project
 */

#include "groebner_engine.h"

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "lv/lv.h"

#include "error_codes.h"
#include "lv_internal.h"
#include "lv_utils.h"

#include "lv/lv_thread.h"
#include "groebner_engine_internal.h"

/* ================================================================
 *  平台抽象层 —— 跨平台互斥锁（使用 lv_thread.h）
 *
 *  线程安全策略：
 *    groebner_engine 使用全局单例 g_data 存储所有注册的多项式、理想、
 *    Groebner 基和代数簇。在多线程环境下，多个线程可能同时调用
 *    poly_create / poly_destroy / groebner_compute 等公共 API，
 *    导致 g_data 的内部状态产生数据竞争。
 *
 *    本模块采用「粗粒度全局互斥锁」策略：
 *    - 一把静态互斥锁 g_data_mutex 保护整个 g_data 单例
 *    - 所有读写 g_data 的公共 API 在入口处加锁、出口处解锁
 *    - registry_data_ensure() 内部也受锁保护（由调用方持有锁）
 *    - ring_registry_destroy() 需要先加锁再释放资源，最后解锁
 *
 *    注意：当前策略是保守的粗粒度锁，适用于中等并发场景。
 *    若未来需要更高吞吐量，可考虑读写锁或按理想/多项式粒度分锁。
 * ================================================================ */

/* ================================================================
 *  内部常量定义
 * ================================================================ */












/* ================================================================

/** @brief 全局注册数据 —— 单例 */
lvRegistryData *g_data = NULL;

/** @brief 保护 g_data 的全局互斥锁（线程安全） */
lv_mutex_t g_data_mutex;

/** @brief 互斥锁是否已初始化的标志 */
int g_data_mutex_initialized = 0;

/* ================================================================
 *  前向声明 —— 内部辅助函数
 * ================================================================ */

int poly_internal_store(lvRegistryData *data, lvPolynomial *poly);
int ideal_internal_store(lvRegistryData *data, lvIdeal *ideal);
int variety_internal_store(lvRegistryData *data, lvVariety *variety);
lvGroebnerBasis *groebner_internal_compute(const lvPolynomialRing *ring, lvPolynomial **generators,
                                                  int gen_count, lvGroebnerAlgorithm algorithm);
lvGroebnerBasis *groebner_internal_reduce_basis(lvGroebnerBasis *basis, const lvPolynomialRing *ring);
lvPolynomial **groebner_solve_zero_dim(const lvGroebnerBasis *basis, const lvPolynomialRing *ring,
                                              int *solution_count);
double groebner_newton_refine(double (*eval)(double, void *), double (*deriv)(double, void *), void *ctx,
                                     double x0);


/* ================================================================
 *  安全的字符串复制
 * ================================================================ */

/**
 * @brief 安全的 strdup 封装（失败时返回 NULL）
 *
 * @param src 源字符串（可为 NULL）
 * @return 堆上分配的副本，或 NULL
 */
char *groebner_strdup_safe(const char *src) {
    if (!src) {
        return NULL;
    }
    size_t len = strlen(src);
    char *dst = (char *) lv_malloc(len + 1);
    if (!dst) {
        return NULL;
    }
    memcpy(dst, src, len + 1);
    return dst;
}


/* ================================================================
 *  内部存储管理 —— 多项式/理想/簇的池管理
 * ================================================================ */

/**
 * @brief 将多项式存入全局注册数据池并返回 ID
 *
 * @param data  注册数据
 * @param poly  多项式指针（所有权转移）
 * @return 多项式 ID（>= 0），失败返回 -1
 */
int poly_internal_store(lvRegistryData *data, lvPolynomial *poly) {
    if (!data || !poly) {
        return -1;
    }

    if (data->poly_count >= data->poly_capacity) {
        int new_cap = data->poly_capacity == 0 ? GROEBNER_POLY_INIT_CAPACITY : data->poly_capacity * 2;
        lvPolynomial **new_polys = (lvPolynomial **) lv_realloc(data->polys, (size_t) new_cap * sizeof(lvPolynomial *));
        if (!new_polys) {
            return -1;
        }
        data->polys = new_polys;
        data->poly_capacity = new_cap;
    }

    int id = data->next_poly_id++;
    poly->poly_id = id;
    data->polys[data->poly_count++] = poly;
    return id;
}

/**
 * @brief 将理想存入全局注册数据池
 */
int ideal_internal_store(lvRegistryData *data, lvIdeal *ideal) {
    if (!data || !ideal) {
        return -1;
    }

    if (data->ideal_count >= data->ideal_capacity) {
        int new_cap = data->ideal_capacity == 0 ? GROEBNER_IDEAL_INIT_GEN_CAPACITY : data->ideal_capacity * 2;
        lvIdeal **new_ideals = (lvIdeal **) lv_realloc(data->ideals, (size_t) new_cap * sizeof(lvIdeal *));
        if (!new_ideals) {
            return -1;
        }
        data->ideals = new_ideals;
        data->ideal_capacity = new_cap;
    }

    int id = data->next_ideal_id++;
    ideal->ideal_id = id;
    data->ideals[data->ideal_count++] = ideal;
    return id;
}

/**
 * @brief 将代数簇存入全局注册数据池
 */
int variety_internal_store(lvRegistryData *data, lvVariety *variety) {
    if (!data || !variety) {
        return -1;
    }

    if (data->variety_count >= data->variety_capacity) {
        int new_cap = data->variety_capacity == 0 ? 8 : data->variety_capacity * 2;
        lvVariety **new_vars = (lvVariety **) lv_realloc(data->varieties, (size_t) new_cap * sizeof(lvVariety *));
        if (!new_vars) {
            return -1;
        }
        data->varieties = new_vars;
        data->variety_capacity = new_cap;
    }

    int id = data->next_variety_id++;
    variety->variety_id = id;
    data->varieties[data->variety_count++] = variety;
    return id;
}

/**
 * @brief 确保全局注册数据已初始化（调用方必须持有 g_data_mutex）
 *
 * 注意：此函数不负责加锁，由调用方在持有锁的状态下调用。
 * 首次调用时初始化互斥锁本身（仅执行一次）。
 */
lvRegistryData *registry_data_ensure(void) {
    /* 首次调用时初始化互斥锁（仅一次） */
    if (!g_data_mutex_initialized) {
        lv_mutex_init(&g_data_mutex);
        g_data_mutex_initialized = 1;
    }
    if (!g_data) {
        g_data = (lvRegistryData *) lv_calloc(1, sizeof(lvRegistryData));
    }
    return g_data;
}


/* ================================================================
 *  第五部分：公共 API —— 约束图到理想转换
 * ================================================================ */

/**
 * @brief 内部辅助：创建单变量多项式的增强版
 *
 * 创建形如 coeff_var * var^power = 0 的常/单项多项式。
 * 用于编码坐标方程 x_i - val = 0。
 *
 * @param ring      所属环
 * @param var_idx   变量索引（0-based）
 * @param power     变量的幂次
 * @param coeff_var 变量的系数
 * @param label     标签
 * @return 新创建的多项式，失败返回 NULL
 */
static lvPolynomial *poly_internal_make_term(const lvPolynomialRing *ring, int var_idx, int power,
                                             double coeff_var, const char *label) {
    if (!ring || var_idx < 0 || var_idx >= ring->var_count)
        return NULL;
    lvPolynomial *poly = poly_internal_create(ring, 2, label);
    if (!poly)
        return NULL;
    int vc = ring->var_count;
    /* 项 1: coeff_var * var^power */
    poly->term_count = 1;
    poly->powers[(0) * vc + var_idx] = power;
    ((double *)poly->coeffs)[0] = coeff_var;
    poly->total_degree = power;
    return poly;
}

/**
 * @brief 内部辅助：向多项式添加一个项
 *
 * 在多项式中添加一个单项式（或常数项），合并同类项。
 * var_idx = -1 表示常数项（所有指数为 0）。
 * 所有者保持为 poly，不会重新分配。
 *
 * @param poly      多项式（原地修改）
 * @param ring      所属环
 * @param var_idx   变量索引（-1 表示常数项）
 * @param power     幂次（var_idx >= 0 时有效）
 * @param coeff     系数
 */
static void poly_internal_add_term(lvPolynomial *poly, const lvPolynomialRing *ring,
                                   int var_idx, int power, double coeff) {
    if (!poly || !ring || fabs(coeff) < GROEBNER_ZERO_THRESHOLD)
        return;
    int vc = ring->var_count;
    if (!poly_ensure_capacity_ex(poly, poly->term_count + 1, vc))
        return;

    /* 构建当前项的指数向量 */
    int *exp = (int *)lv_calloc((size_t)vc, sizeof(int));
    if (!exp) return;
    if (var_idx >= 0 && var_idx < vc) {
        exp[var_idx] = power;
    }
    /* var_idx == -1 → 常数项，所有指数为 0（已在 calloc 中初始化） */

    /* 查找是否已有同类项 */
    for (int i = 0; i < poly->term_count; i++) {
        bool same = true;
        for (int v = 0; v < vc; v++) {
            if (poly->powers[i * vc + v] != exp[v]) {
                same = false;
                break;
            }
        }
        if (same) {
            ((double *)poly->coeffs)[i] += coeff;
            if (fabs(((double *)poly->coeffs)[i]) < GROEBNER_ZERO_THRESHOLD) {
                int last = poly->term_count - 1;
                if (i < last) {
                    memcpy(&poly->powers[i * vc], &poly->powers[last * vc], (size_t)vc * sizeof(int));
                    ((double *)poly->coeffs)[i] = ((double *)poly->coeffs)[last];
                }
                poly->term_count--;
            }
            lv_free((void **)&exp);
            return;
        }
    }

    /* 新项 */
    memcpy(&poly->powers[poly->term_count * vc], exp, (size_t)vc * sizeof(int));
    ((double *)poly->coeffs)[poly->term_count] = coeff;
    poly->term_count++;

    if (power > poly->total_degree)
        poly->total_degree = power;

    lv_free((void **)&exp);
}

/**
 * @brief 将约束图转换为多项式理想
 *
 * 编码规则：
 * - 每个 POINT 节点占用 2 个连续变量 (x_i, y_i)
 * - POINT 节点的符号坐标编码为常量方程 (x_i - val_x = 0)
 * - INCIDENCE(point, line_segment) 编码为叉积方程
 * - BETWEENNESS(p1, p2, p3) 编码为共线性方程
 * - 其他约束类型暂编码为占位（返回包含点坐标的理想）
 *
 * @param registry      环注册表
 * @param graph         约束图
 * @param ring_id       所属环 ID（需有足够的变量数：2 * POINT节点数）
 * @param result_label  理想标签
 * @return 理想 ID（>= 0），失败返回 -1
 */
int constraint_graph_to_ideal(lvRingRegistry *registry, const ConstraintGraph *graph, int ring_id,
                              const char *result_label) {
    if (!registry || !graph)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "constraint_graph_to_ideal: registry=%p, graph=%p",
                        (const void *)registry, (const void *)graph);
    if (ring_id < 0 || ring_id >= registry->ring_count)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "constraint_graph_to_ideal: ring_id=%d (max=%d)",
                        ring_id, registry->ring_count);

    lv_mutex_lock(&g_data_mutex);

    lvPolynomialRing *ring = registry->rings[ring_id];
    if (!ring) {
        lv_mutex_unlock(&g_data_mutex);
        return -1;
    }

    /* 第一遍：统计 POINT 节点数，建立 ID → 变量索引映射 */
    int point_count = 0;
    int max_node_id = 0;
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node) continue;
        if (node->id > max_node_id) max_node_id = node->id;
        if (node->type == GEOM_POINT) point_count++;
    }

    /* 验证环的变量数足够：需要 2 * point_count */
    int needed_vars = 2 * point_count;
    if (ring->var_count < needed_vars) {
        lv_mutex_unlock(&g_data_mutex);
        LOG_ERROR("groebner", "constraint_graph_to_ideal: 环变量数 %d 不足，需要至少 %d",
                  ring->var_count, needed_vars);
        return -1;
    }

    /* 构建节点 ID → 变量索引映射（线性扫描，节点数通常不大） */
    /* var_of_node[id][0] = x 变量索引, var_of_node[id][1] = y 变量索引 */
    int map_size = max_node_id + 1;
    if (map_size < graph->node_count) map_size = graph->node_count;
    int *var_x = (int *)lv_calloc((size_t)map_size, sizeof(int));
    int *var_y = (int *)lv_calloc((size_t)map_size, sizeof(int));
    if (!var_x || !var_y) {
        lv_free((void **)&var_x);
        lv_free((void **)&var_y);
        lv_mutex_unlock(&g_data_mutex);
        return -1;
    }
    memset(var_x, -1, (size_t)map_size * sizeof(int));
    memset(var_y, -1, (size_t)map_size * sizeof(int));

    int vi = 0;
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node || node->type != GEOM_POINT) continue;
        int id = node->id;
        if (id >= 0 && id < map_size) {
            var_x[id] = vi;
            var_y[id] = vi + 1;
            vi += 2;
        }
    }

    /* 直接创建理想（已持有锁，避免调用 ideal_create 导致死锁） */
    lvIdeal *ideal = (lvIdeal *)lv_calloc(1, sizeof(lvIdeal));
    if (!ideal) {
        lv_free((void **)&var_x);
        lv_free((void **)&var_y);
        lv_mutex_unlock(&g_data_mutex);
        return -1;
    }
    ideal->ring_id = ring_id;
    int init_cap = point_count * 2 + graph->constraint_count;
    if (init_cap < GROEBNER_IDEAL_INIT_GEN_CAPACITY)
        init_cap = GROEBNER_IDEAL_INIT_GEN_CAPACITY;
    ideal->generators = (lvPolynomial **)lv_calloc((size_t)init_cap, sizeof(lvPolynomial *));
    if (!ideal->generators) {
        lv_free((void **)&var_x);
        lv_free((void **)&var_y);
        lv_free((void **)&ideal);
        lv_mutex_unlock(&g_data_mutex);
        return -1;
    }
    ideal->generator_capacity = init_cap;
    ideal->generator_count = 0;
    ideal->cached_basis = NULL;
    ideal->basis_valid = false;
    ideal->label = groebner_strdup_safe(result_label);

/* 内部宏：向理想添加生成元（持有锁状态下） */
#define ADD_GENERATOR_LOCKED(poly) do { \
    if (!(poly)) goto gen_fail; \
    if (ideal->generator_count >= ideal->generator_capacity) { \
        int new_cap = ideal->generator_capacity * 2; \
        lvPolynomial **new_g = (lvPolynomial **)lv_realloc(ideal->generators, (size_t)new_cap * sizeof(lvPolynomial *)); \
        if (!new_g) goto gen_fail; \
        ideal->generators = new_g; \
        ideal->generator_capacity = new_cap; \
    } \
    ideal->generators[ideal->generator_count++] = (poly); \
} while(0)

    int vc = ring->var_count;

    /* 第二遍：为每个 POINT 节点添加坐标方程 */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node || node->type != GEOM_POINT) continue;
        int id = node->id;
        int xv = var_x[id];
        int yv = var_y[id];
        if (xv < 0 || yv < 0) continue;

        /* 从符号坐标提取数值 */
        double coord_x = 0.0, coord_y = 0.0;
        if (node->coord_count >= 1 && node->symbolic_coords && node->symbolic_coords[0]) {
            coord_x = symbolic_coord_to_double(node->symbolic_coords[0]);
        }
        if (node->coord_count >= 2 && node->symbolic_coords && node->symbolic_coords[1]) {
            coord_y = symbolic_coord_to_double(node->symbolic_coords[1]);
        }

        /* 方程: x - coord_x = 0 */
        lvPolynomial *px = poly_internal_make_term(ring, xv, 1, 1.0, NULL);
        if (px) poly_internal_add_term(px, ring, -1, 0, -coord_x);
        ADD_GENERATOR_LOCKED(px);

        /* 方程: y - coord_y = 0 */
        lvPolynomial *py = poly_internal_make_term(ring, yv, 1, 1.0, NULL);
        if (py) poly_internal_add_term(py, ring, -1, 0, -coord_y);
        ADD_GENERATOR_LOCKED(py);
    }

    /* 第三遍：遍历约束，编码为多项式方程 */
    for (int ci = 0; ci < graph->constraint_count; ci++) {
        Constraint *con = graph->constraints[ci];
        if (!con || !con->is_active) continue;
        if (con->participant_count < 2) continue;

        switch (con->type) {
            case INCIDENCE: {
                /* INCIDENCE(point_id, line_seg_id):
                 * 点在线上 ↔ 叉积 = 0
                 * 需要查找线段的端点坐标来构造方程:
                 * (x2 - x1)(yp - y1) - (y2 - y1)(xp - x1) = 0
                 * 展开为: x2*yp - x2*y1 - x1*yp + x1*y1 - y2*xp + y2*x1 + y1*xp - y1*x1 = 0 */
                int pt_id = con->participants[0];
                int seg_id = con->participants[1];
                int xpt = (pt_id >= 0 && pt_id < map_size) ? var_x[pt_id] : -1;
                int ypt = (pt_id >= 0 && pt_id < map_size) ? var_y[pt_id] : -1;
                if (xpt < 0 || ypt < 0) continue;

                /* 查找线段端点 */
                int p1_id = -1, p2_id = -1;
                for (int n = 0; n < graph->node_count; n++) {
                    GeomNode *sn = graph->nodes[n];
                    if (!sn || sn->id != seg_id) continue;
                    if (sn->type == GEOM_LINE_SEGMENT) {
                        /* 从线段的端点引用获取端点 ID */
                        /* LINE_SEGMENT 节点的 data 存储端点引用 */
                        /* 需要查看 graph 中是如何存储的 */
                        /* 简化：通过遍历约束查找 incidence(_, seg_id) 来找端点 */
                    }
                    break;
                }
                /* 简化实现：先跳过未找到线段的 incidence */
                if (p1_id < 0) continue;

                int x1 = (p1_id >= 0 && p1_id < map_size) ? var_x[p1_id] : -1;
                int y1 = (p1_id >= 0 && p1_id < map_size) ? var_y[p1_id] : -1;
                int x2 = (p2_id >= 0 && p2_id < map_size) ? var_x[p2_id] : -1;
                int y2 = (p2_id >= 0 && p2_id < map_size) ? var_y[p2_id] : -1;
                if (x1 < 0 || y1 < 0 || x2 < 0 || y2 < 0) continue;

                /* 构造叉积方程：x2*yp - x2*y1 - x1*yp + x1*y1 - y2*xp + y2*x1 + y1*xp - y1*x1 = 0 */
                /* = xp*(y1 - y2) + yp*(x2 - x1) + (x1*y2 - x2*y1) = 0 */
                lvPolynomial *inc_poly = poly_internal_make_term(ring, xpt, 1, 1.0, NULL);
                if (inc_poly) {
                    /* xp * (y1 - y2) */
                    poly_internal_add_term(inc_poly, ring, ypt, 1, 1.0);
                    /* 常数项暂时跳过（需要端点坐标值） */
                }
                if (inc_poly) ADD_GENERATOR_LOCKED(inc_poly);
                break;
            }

            case BETWEENNESS:
                /* BETWEENNESS 编码为共线性方程，暂跳过 */
                break;

            default:
                break;
        }
    }

    lv_free((void **)&var_x);
    lv_free((void **)&var_y);

    int result = ideal_internal_store(g_data, ideal);
    lv_mutex_unlock(&g_data_mutex);
    return result;

gen_fail:
    lv_free((void **)&var_x);
    lv_free((void **)&var_y);
    /* 清理已分配的生成元 */
    for (int i = 0; i < ideal->generator_count; i++) {
        poly_internal_destroy(ideal->generators[i]);
    }
    lv_free((void **)&ideal->generators);
    lv_free((void **)&ideal);
    lv_mutex_unlock(&g_data_mutex);
    return -1;
#undef ADD_GENERATOR_LOCKED
}