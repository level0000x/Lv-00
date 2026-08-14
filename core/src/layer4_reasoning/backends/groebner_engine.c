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
#include "lv/lv_constraint_guard.h"
#include "lv/lv_numeric.h"
#include "lv/lv_xmacro.h"
#include "lv/geo_utils.h"
#include "error_codes.h"
#include "lv_internal.h"
#include "lv_utils.h"

#include "lv/lv_thread.h"
#include "groebner_engine_internal.h"
#include "groebner_engine_guard.h"

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
 *  全局单例状态
 * ================================================================ */

/** @brief 全局注册数据 —— 单例 */
lvRegistryData *g_data = NULL;

/** @brief 保护 g_data 的全局互斥锁（线程安全） */
lv_mutex_t g_data_mutex;

/** @brief 互斥锁是否已初始化的标志 */
int g_data_mutex_initialized = 0;

/** @brief 全局互斥锁的一次性初始化控制（进程级生命周期） */
static lv_once_t g_data_mutex_once = lv_ONCE_INIT;

/**
 * @brief 全局互斥锁的一次性初始化回调（仅执行一次）
 */
static void g_data_mutex_ensure_once(void) {
    lv_mutex_init(&g_data_mutex);
    g_data_mutex_initialized = 1;
}

/**
 * @brief 确保全局互斥锁已初始化（线程安全，可在加锁前调用）
 *
 * 全局锁 g_data_mutex 采用进程级生命周期：首次使用时通过 lv_once
 * 恰好初始化一次，之后永不销毁，避免 ring_registry_destroy 等路径
 * 销毁后再次加锁导致的 CRITICAL_SECTION 损坏。
 */
void groebner_mutex_ensure(void) {
    lv_once(&g_data_mutex_once, g_data_mutex_ensure_once);
}

/**
 * @brief 加锁守卫初始化（groebner 引擎专用）
 *
 * 在获取全局锁之前先确保锁已初始化（解决"先锁后初始化"的鸡生蛋问题）。
 * 用法与 lv_lock_guard_init 一致，配合 lv_lock_guard_destroy 使用。
 */
void groebner_lock_guard_init(lvLockGuard *g) {
    groebner_mutex_ensure();
    lv_lock_guard_init(g, &g_data_mutex);
}

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

    /* 扩容多项式池（count >= capacity 时倍增，统一走 lv_ENSURE_ARRAY_CAP） */
    lv_ENSURE_ARRAY_CAP(data->polys, data->poly_count, data->poly_capacity, -1);

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

    /* 扩容理想池（count >= capacity 时倍增，统一走 lv_ENSURE_ARRAY_CAP） */
    lv_ENSURE_ARRAY_CAP(data->ideals, data->ideal_count, data->ideal_capacity, -1);

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

    /* 扩容代数簇池（count >= capacity 时倍增，统一走 lv_ENSURE_ARRAY_CAP） */
    lv_ENSURE_ARRAY_CAP(data->varieties, data->variety_count, data->variety_capacity, -1);

    int id = data->next_variety_id++;
    variety->variety_id = id;
    data->varieties[data->variety_count++] = variety;
    return id;
}

/**
 * @brief 确保全局注册数据已初始化（调用方必须持有 g_data_mutex）
 *
 * 注意：此函数不负责加锁，也不负责初始化互斥锁（互斥锁由
 * groebner_mutex_ensure() 在加锁前完成一次性初始化），
 * 调用方应在持有锁的状态下调用本函数。
 */
lvRegistryData *registry_data_ensure(void) {
    /* exempt: 惰性初始化（g_data 为空时分配），非有效性守卫，保留 */
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
    if (!ring || !lv_index_in_range(var_idx, ring->var_count))
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
    if (!poly || !ring || fabs(coeff) < lv_config_get_double(LV_CFG_GROEBNER_ZERO_THRESHOLD, GROEBNER_ZERO_THRESHOLD))
        return;
    int vc = ring->var_count;
    if (!poly_ensure_capacity_ex(poly, poly->term_count + 1, vc))
        return;

    /* 构建当前项的指数向量 */
    int *exp = (int *)lv_calloc((size_t)vc, sizeof(int));
    if (!exp) return;
    if (lv_index_in_range(var_idx, vc)) {
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
            if (fabs(((double *)poly->coeffs)[i]) < lv_config_get_double(LV_CFG_GROEBNER_ZERO_THRESHOLD, GROEBNER_ZERO_THRESHOLD)) {
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

/* ── Groebner 引擎编码上下文 ── */
typedef struct {
    lvPolynomialRing *ring;
    int vc;
    const int *var_x;
    const int *var_y;
    int map_size;
    const ConstraintGraph *graph;
    lvIdeal *ideal;
} GroebnerEngineEncodeCtx;

/* ── 各约束类型的 Groebner 引擎编码函数（文件作用域，用于查找表） ── */
static void groebner_engine_encode_incidence(const GroebnerEngineEncodeCtx *ctx, const Constraint *con) {
    int pt_id = con->participants[0];
    int seg_id = con->participants[1];
    int xpt = lv_index_in_range(pt_id, ctx->map_size) ? ctx->var_x[pt_id] : -1;
    int ypt = lv_index_in_range(pt_id, ctx->map_size) ? ctx->var_y[pt_id] : -1;
    if (xpt < 0 || ypt < 0) return;

    int p1_id = -1, p2_id = -1;
    for (int n = 0; n < ctx->graph->node_count; n++) {
        GeomNode *sn = ctx->graph->nodes[n];
        if (!sn || sn->id != seg_id) continue;
        if (sn->type == GEOM_LINE_SEGMENT) { }
        break;
    }
    if (p1_id < 0) return;

    int x1 = lv_index_in_range(p1_id, ctx->map_size) ? ctx->var_x[p1_id] : -1;
    int y1 = lv_index_in_range(p1_id, ctx->map_size) ? ctx->var_y[p1_id] : -1;
    int x2 = lv_index_in_range(p2_id, ctx->map_size) ? ctx->var_x[p2_id] : -1;
    int y2 = lv_index_in_range(p2_id, ctx->map_size) ? ctx->var_y[p2_id] : -1;
    if (x1 < 0 || y1 < 0 || x2 < 0 || y2 < 0) return;

    lvPolynomial *inc_poly = poly_internal_make_term(ctx->ring, xpt, 1, 1.0, NULL);
    if (inc_poly) {
        poly_internal_add_term(inc_poly, ctx->ring, ypt, 1, 1.0);
    }
    if (inc_poly) {
        if (ctx->ideal->generator_count >= ctx->ideal->generator_capacity) {
            if (!lv_ensure_capacity((void **) &ctx->ideal->generators, ctx->ideal->generator_count,
                                    &ctx->ideal->generator_capacity, sizeof(lvPolynomial *), 1)) {
                lv_free((void **) &inc_poly);
                return;
            }
        }
        ctx->ideal->generators[ctx->ideal->generator_count++] = inc_poly;
    }
}

/* 编码辅助：将多项式追加到理想生成元列表（所有权转移，失败时释放 poly） */
static void groebner_engine_ideal_append(const GroebnerEngineEncodeCtx *ctx, lvPolynomial *poly) {
    if (!poly) return;
    if (!ctx || !ctx->ideal) {
        poly_internal_destroy(poly);
        return;
    }
    if (ctx->ideal->generator_count >= ctx->ideal->generator_capacity) {
        if (!lv_ensure_capacity((void **) &ctx->ideal->generators, ctx->ideal->generator_count,
                                &ctx->ideal->generator_capacity, sizeof(lvPolynomial *), 1)) {
            poly_internal_destroy(poly);
            return;
        }
    }
    ctx->ideal->generators[ctx->ideal->generator_count++] = poly;
}

/* 编码辅助：取节点 id 的 (x, y) 变量索引；节点无变量映射返回 -1 */
static int groebner_engine_var_xy(const GroebnerEngineEncodeCtx *ctx, int node_id, int *xv, int *yv) {
    if (!ctx || !xv || !yv || !lv_index_in_range(node_id, ctx->map_size))
        return -1;
    if (ctx->var_x[node_id] < 0 || ctx->var_y[node_id] < 0) return -1;
    *xv = ctx->var_x[node_id];
    *yv = ctx->var_y[node_id];
    return 0;
}

/* 编码辅助：提取线段端点坐标（标准布局 [Ax,Ay,Bx,By]，graph_add_line_segment 约定），
 * 任一端点坐标缺失返回 false */
static bool groebner_engine_segment_coords(const ConstraintGraph *graph, int seg_id,
                                           double *ax, double *ay, double *bx, double *by) {
    if (!graph || seg_id < 0) return false;
    GeomNode *seg = graph_get_node(graph, seg_id);
    if (!seg || seg->type != GEOM_LINE_SEGMENT) return false;
    return symbolic_coord_get_segment(seg->symbolic_coords, seg->coord_count, ax, ay, bx, by);
}

/* 编码辅助：构造线性方程 cx*x + cy*y + c0 = 0（近零系数由 add_term 自动剔除） */
static lvPolynomial *groebner_engine_make_linear(const lvPolynomialRing *ring, int xv, int yv,
                                                 double cx, double cy, double c0) {
    if (!ring || !lv_index_in_range(xv, ring->var_count) || !lv_index_in_range(yv, ring->var_count))
        return NULL;
    lvPolynomial *poly = poly_internal_create(ring, 4, NULL);
    if (!poly) return NULL;
    poly_internal_add_term(poly, ring, xv, 1, cx);
    poly_internal_add_term(poly, ring, yv, 1, cy);
    poly_internal_add_term(poly, ring, -1, 0, c0);
    return poly;
}

/* BETWEENNESS(A,B,C)：点 B 在 A 与 C 之间 → 三点共线（组合方程）。
 * 编码：叉积 cross = (B-A)×(C-A) = (x_B-x_A)(y_C-y_A) - (y_B-y_A)(x_C-x_A) = 0，
 * 展开为两个双线性因子之差，用乘法构造后合并。 */
static void groebner_engine_encode_betweenness(const GroebnerEngineEncodeCtx *ctx, const Constraint *con) {
    if (!lv_constraint_has_participants(con, 3)) return;
    int xa = -1, ya = -1, xb = -1, yb = -1, xc = -1, yc = -1;
    if (groebner_engine_var_xy(ctx, con->participants[0], &xa, &ya) < 0) return;
    if (groebner_engine_var_xy(ctx, con->participants[1], &xb, &yb) < 0) return;
    if (groebner_engine_var_xy(ctx, con->participants[2], &xc, &yc) < 0) return;

    lvPolynomial *ux = poly_internal_make_term(ctx->ring, xb, 1, 1.0, NULL);
    if (ux) poly_internal_add_term(ux, ctx->ring, xa, 1, -1.0);
    if (!ux) return;

    lvPolynomial *uy = poly_internal_make_term(ctx->ring, yb, 1, 1.0, NULL);
    if (uy) poly_internal_add_term(uy, ctx->ring, ya, 1, -1.0);
    if (!uy) { poly_internal_destroy(ux); return; }

    lvPolynomial *vx = poly_internal_make_term(ctx->ring, xc, 1, 1.0, NULL);
    if (vx) poly_internal_add_term(vx, ctx->ring, xa, 1, -1.0);
    if (!vx) { poly_internal_destroy(ux); poly_internal_destroy(uy); return; }

    lvPolynomial *vy = poly_internal_make_term(ctx->ring, yc, 1, 1.0, NULL);
    if (vy) poly_internal_add_term(vy, ctx->ring, ya, 1, -1.0);
    if (!vy) { poly_internal_destroy(ux); poly_internal_destroy(uy); poly_internal_destroy(vx); return; }

    lvPolynomial *p1 = poly_internal_multiply(ux, vy, ctx->ring);
    lvPolynomial *p2 = poly_internal_multiply(uy, vx, ctx->ring);
    if (p2) poly_internal_scale(p2, -1.0);
    lvPolynomial *cross = (p1 && p2) ? poly_internal_add(p1, p2, ctx->ring) : NULL;

    poly_internal_destroy(ux);
    poly_internal_destroy(uy);
    poly_internal_destroy(vx);
    poly_internal_destroy(vy);
    poly_internal_destroy(p1);
    poly_internal_destroy(p2);

    groebner_engine_ideal_append(ctx, cross);
}

/* INTERSECTION(AB,CD)→E：两线段交于 E。线段端点坐标为常量（线段 symbolic_coords
 * 布局 [Ax,Ay,Bx,By]），交点 E 为变量。参数方程消元 → E 同时满足两条线的
 * 共线（线性）方程：
 *   cross(E-A, B-A) = dy1*Ex - dx1*Ey + (ay*dx1 - ax*dy1) = 0
 *   cross(E-C, D-C) = dy2*Ex - dx2*Ey + (cy*dx2 - cx*dy2) = 0 */
static void groebner_engine_encode_intersection(const GroebnerEngineEncodeCtx *ctx, const Constraint *con) {
    if (!lv_constraint_has_participants(con, 3)) return;
    double ax = 0.0, ay = 0.0, bx = 0.0, by = 0.0;
    double cx = 0.0, cy = 0.0, dx = 0.0, dy = 0.0;
    if (!groebner_engine_segment_coords(ctx->graph, con->participants[0], &ax, &ay, &bx, &by)) return;
    if (!groebner_engine_segment_coords(ctx->graph, con->participants[1], &cx, &cy, &dx, &dy)) return;
    int xe = -1, ye = -1;
    if (groebner_engine_var_xy(ctx, con->participants[2], &xe, &ye) < 0) return;

    double dx1 = bx - ax, dy1 = by - ay;
    double dx2 = dx - cx, dy2 = dy - cy;

    groebner_engine_ideal_append(ctx,
        groebner_engine_make_linear(ctx->ring, xe, ye, dy1, -dx1, ay * dx1 - ax * dy1));
    groebner_engine_ideal_append(ctx,
        groebner_engine_make_linear(ctx->ring, xe, ye, dy2, -dx2, cy * dx2 - cx * dy2));
}

/* CONTAINMENT(inner, outer)：inner 点被 outer 区域/圆包含 → 坐标方程。
 *  - outer=CIRCLE：点 P 在圆上（包含边界）：(Px-Ox)^2 + (Py-Oy)^2 - r^2 = 0，
 *    r^2 由圆心节点与半径端点节点坐标计算；
 *  - outer=REGION：点 P 在区域首条边界线段上的共线方程（包含的边界退化编码）。
 *  其余组合（inner 非 POINT 等）缺数据，记录诊断并跳过。 */
static void groebner_engine_encode_containment(const GroebnerEngineEncodeCtx *ctx, const Constraint *con) {
    if (!lv_constraint_has_participants(con, 2)) return;
    int inner_id = con->participants[0];
    int outer_id = con->participants[1];

    GeomNode *inner = graph_get_node(ctx->graph, inner_id);
    GeomNode *outer = graph_get_node(ctx->graph, outer_id);
    if (!inner || !outer) return;
    if (inner->type != GEOM_POINT) {
        LOG_ERROR("groebner", "CONTAINMENT 约束 %d: inner 节点 %d 非 POINT，无坐标变量，跳过",
                  con->id, inner_id);
        return;
    }
    int xp = -1, yp = -1;
    if (groebner_engine_var_xy(ctx, inner_id, &xp, &yp) < 0) return;

    if (outer->type == GEOM_CIRCLE) {
        GeomNode *center = graph_get_node(ctx->graph, outer->data.circle.center_node_id);
        GeomNode *radius_pt = graph_get_node(ctx->graph, outer->data.circle.radius_node_id);
        if (!center || !radius_pt || !center->symbolic_coords || center->coord_count < 2 ||
            !radius_pt->symbolic_coords || radius_pt->coord_count < 2 ||
            !center->symbolic_coords[0] || !center->symbolic_coords[1] ||
            !radius_pt->symbolic_coords[0] || !radius_pt->symbolic_coords[1]) {
            LOG_ERROR("groebner", "CONTAINMENT 约束 %d: 圆 %d 的圆心/半径端点坐标缺失，跳过",
                      con->id, outer_id);
            return;
        }
        double ox = symbolic_coord_to_double(center->symbolic_coords[0]);
        double oy = symbolic_coord_to_double(center->symbolic_coords[1]);
        double rx = symbolic_coord_to_double(radius_pt->symbolic_coords[0]);
        double ry = symbolic_coord_to_double(radius_pt->symbolic_coords[1]);
        double r2 = (rx - ox) * (rx - ox) + (ry - oy) * (ry - oy);

        lvPolynomial *poly = poly_internal_make_term(ctx->ring, xp, 2, 1.0, NULL);
        if (!poly) return;
        poly_internal_add_term(poly, ctx->ring, xp, 1, -2.0 * ox);
        poly_internal_add_term(poly, ctx->ring, yp, 2, 1.0);
        poly_internal_add_term(poly, ctx->ring, yp, 1, -2.0 * oy);
        poly_internal_add_term(poly, ctx->ring, -1, 0, geo_norm_sq_2d(ox, oy) - r2);
        groebner_engine_ideal_append(ctx, poly);
        return;
    }

    if (outer->type == GEOM_REGION) {
        GeomNode **segs = outer->data.region.boundary_segments;
        int seg_count = outer->data.region.segment_count;
        if (!segs || seg_count <= 0) {
            LOG_ERROR("groebner", "CONTAINMENT 约束 %d: 区域 %d 无边界线段，跳过",
                      con->id, outer_id);
            return;
        }
        double ax = 0.0, ay = 0.0, bx = 0.0, by = 0.0;
        if (!groebner_engine_segment_coords(ctx->graph, segs[0]->id, &ax, &ay, &bx, &by)) return;
        double dx = bx - ax, dy = by - ay;
        groebner_engine_ideal_append(ctx,
            groebner_engine_make_linear(ctx->ring, xp, yp, dy, -dx, ay * dx - ax * dy));
        return;
    }

    LOG_ERROR("groebner", "CONTAINMENT 约束 %d: outer 节点 %d 类型不支持（仅 CIRCLE/REGION），跳过",
              con->id, outer_id);
}

/* CONNECTION(src_port, dst_port)：端口连接 = 数据流等值 → 两端口坐标相等方程
 *   x_src - x_dst = 0、y_src - y_dst = 0。
 * 端口节点默认无坐标（graph_add_port 置 coord_count=0），缺坐标变量时记录诊断并跳过。 */
static void groebner_engine_encode_connection(const GroebnerEngineEncodeCtx *ctx, const Constraint *con) {
    if (!lv_constraint_has_participants(con, 2)) return;
    int src_id = con->participants[0];
    int dst_id = con->participants[1];
    GeomNode *src = graph_get_node(ctx->graph, src_id);
    GeomNode *dst = graph_get_node(ctx->graph, dst_id);
    if (!src || !dst || src->type != GEOM_PORT || dst->type != GEOM_PORT) return;

    int xs = -1, ys = -1, xd = -1, yd = -1;
    if (groebner_engine_var_xy(ctx, src_id, &xs, &ys) < 0 ||
        groebner_engine_var_xy(ctx, dst_id, &xd, &yd) < 0) {
        LOG_ERROR("groebner", "CONNECTION 约束 %d: 端口 %d/%d 无坐标变量，无法构造坐标相等方程，跳过",
                  con->id, src_id, dst_id);
        return;
    }
    groebner_engine_ideal_append(ctx, groebner_engine_make_linear(ctx->ring, xs, xd, 1.0, -1.0, 0.0));
    groebner_engine_ideal_append(ctx, groebner_engine_make_linear(ctx->ring, ys, yd, 1.0, -1.0, 0.0));
}

/* ANGLE(line1, line2)：两条线段夹角等于约束角度（度）。
 * 方向向量 u = B-A、v = D-C（端点坐标为常量），夹角 θ 的余弦等式：
 *   (u·v)^2 - cos^2θ · |u|^2 · |v|^2 = 0（平方消除根号，等价于余弦/斜率判据）。
 * 结果为常量多项式：角度匹配时近零（跳过），否则作为一致性方程加入理想。 */
static void groebner_engine_encode_angle(const GroebnerEngineEncodeCtx *ctx, const Constraint *con) {
    if (!lv_constraint_has_participants(con, 2)) return;
    double ax = 0.0, ay = 0.0, bx = 0.0, by = 0.0;
    double cx = 0.0, cy = 0.0, dx = 0.0, dy = 0.0;
    if (!groebner_engine_segment_coords(ctx->graph, con->participants[0], &ax, &ay, &bx, &by)) return;
    if (!groebner_engine_segment_coords(ctx->graph, con->participants[1], &cx, &cy, &dx, &dy)) return;

    double ux = bx - ax, uy = by - ay;
    double vx = dx - cx, vy = dy - cy;
    double theta = lv_deg_to_rad(con->numeric_value);
    double cos_sq = cos(theta) * cos(theta);
    double dot = ux * vx + uy * vy;
    double norm_u_sq = geo_norm_sq_2d(ux, uy);
    double norm_v_sq = geo_norm_sq_2d(vx, vy);
    double lhs = dot * dot - cos_sq * norm_u_sq * norm_v_sq;

    if (fabs(lhs) < GROEBNER_ZERO_THRESHOLD) return;
    lvPolynomial *poly = poly_internal_create(ctx->ring, 1, NULL);
    if (!poly) return;
    poly_internal_add_term(poly, ctx->ring, -1, 0, lhs);
    if (poly->term_count <= 0) {
        poly_internal_destroy(poly);
        return;
    }
    groebner_engine_ideal_append(ctx, poly);
}

/* ── Groebner 引擎编码函数查找表（直接索引：ConstraintType 枚举连续 0-5）── */
typedef void (*GroebnerEngineEncodeFn)(const GroebnerEngineEncodeCtx *ctx, const Constraint *con);
static const GroebnerEngineEncodeFn kGroebnerEngineEncodeTable[] = {
    [INCIDENCE] = groebner_engine_encode_incidence,
    [BETWEENNESS] = groebner_engine_encode_betweenness,
    [INTERSECTION] = groebner_engine_encode_intersection,
    [CONTAINMENT] = groebner_engine_encode_containment,
    [CONNECTION] = groebner_engine_encode_connection,
    [ANGLE] = groebner_engine_encode_angle
};

/**
 * @brief 将约束图转换为多项式理想
 *
 * 编码规则：
 * - 每个 POINT 节点占用 2 个连续变量 (x_i, y_i)
 * - POINT 节点的符号坐标编码为常量方程 (x_i - val_x = 0)
 * - INCIDENCE(point, line_segment) 编码为叉积方程
 * - BETWEENNESS(p1, p2, p3) 编码为共线性方程
 * - 其他约束类型（编码表外）按原语义跳过：仅编码点坐标与已有类型方程
 *
 * @param registry      环注册表
 * @param graph         约束图
 * @param ring_id       所属环 ID（需有足够的变量数：2 * POINT节点数）
 * @param result_label  理想标签
 * @return 理想 ID（>= 0），失败返回 -1
 */
int constraint_graph_to_ideal(lvRingRegistry *registry, const ConstraintGraph *graph, int ring_id,
                              const char *result_label) {
    /* exempt: 双指针 NULL 守卫（registry/graph 均须非空），与 id 范围守卫
     * groebner_id_in_range 不同构（不访问计数成员），与 ring_register 的
     * `!registry || !ring` 同构（均为两指针非空对）；保留原形态。 */
    if (!registry || !graph)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "constraint_graph_to_ideal: registry=%p, graph=%p",
                        (const void *)registry, (const void *)graph);
    if (!lv_index_in_range(ring_id, registry->ring_count))
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "constraint_graph_to_ideal: ring_id=%d (max=%d)",
                        ring_id, registry->ring_count);

    int ret = -1;
    GROEBNER_LOCK_GUARD_BEGIN();

    lvPolynomialRing *ring = registry->rings[ring_id];
    if (!ring) {
        goto _gcleanup;
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
        LOG_ERROR("groebner", "constraint_graph_to_ideal: 环变量数 %d 不足，需要至少 %d",
                  ring->var_count, needed_vars);
        goto _gcleanup;
    }

    /* 构建节点 ID → 变量索引映射（线性扫描，节点数通常不大） */
    /* var_of_node[id][0] = x 变量索引, var_of_node[id][1] = y 变量索引 */
    int map_size = max_node_id + 1;
    if (map_size < graph->node_count) map_size = graph->node_count;
    int *var_x = (int *)lv_calloc((size_t)map_size, sizeof(int));
    int *var_y = (int *)lv_calloc((size_t)map_size, sizeof(int));
    if (!var_x || !var_y) {
        lv_free_many(&var_x, &var_y, NULL);
        goto _gcleanup;
    }
    memset(var_x, -1, (size_t)map_size * sizeof(int));
    memset(var_y, -1, (size_t)map_size * sizeof(int));

    int vi = 0;
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node || node->type != GEOM_POINT) continue;
        int id = node->id;
        if (lv_index_in_range(id, map_size)) {
            var_x[id] = vi;
            var_y[id] = vi + 1;
            vi += 2;
        }
    }

    /* 直接创建理想（已持有锁，避免调用 ideal_create 导致死锁） */
    lvIdeal *ideal = (lvIdeal *)lv_calloc(1, sizeof(lvIdeal));
    if (!ideal) {
        lv_free_many(&var_x, &var_y, NULL);
        goto _gcleanup;
    }
    ideal->ring_id = ring_id;
    int init_cap = point_count * 2 + graph->constraint_count;
    if (init_cap < GROEBNER_IDEAL_INIT_GEN_CAPACITY)
        init_cap = GROEBNER_IDEAL_INIT_GEN_CAPACITY;
    ideal->generators = (lvPolynomial **)lv_calloc((size_t)init_cap, sizeof(lvPolynomial *));
    if (!ideal->generators) {
        lv_free_many(&var_x, &var_y, &ideal, NULL);
        goto _gcleanup;
    }
    ideal->generator_capacity = init_cap;
    ideal->generator_count = 0;
    ideal->cached_basis = NULL;
    ideal->basis_valid = false;
    ideal->label = lv_strdup_safe(result_label);

/* 内部宏：向理想添加生成元（持有锁状态下） */
#define ADD_GENERATOR_LOCKED(poly) do { \
    if (!(poly)) goto gen_fail; \
    if (ideal->generator_count >= ideal->generator_capacity) { \
        if (!lv_ensure_capacity((void **) &ideal->generators, ideal->generator_count, \
                                &ideal->generator_capacity, sizeof(lvPolynomial *), 1)) goto gen_fail; \
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

    /* 第三遍：遍历约束，通过查找表编码为多项式方程 */
    for (int ci = 0; ci < graph->constraint_count; ci++) {
        Constraint *con = graph->constraints[ci];
        if (!con || !con->is_active) continue;
        if (!lv_constraint_has_participants(con, 2)) continue;

        GroebnerEngineEncodeCtx gctx;
        gctx.ring = ring;
        gctx.vc = vc;
        gctx.var_x = var_x;
        gctx.var_y = var_y;
        gctx.map_size = map_size;
        gctx.graph = graph;
        gctx.ideal = ideal;

        LV_DISPATCH_VOID(kGroebnerEngineEncodeTable, con->type, &gctx, con);
    }

    lv_free_many(&var_x, &var_y, NULL);

    ret = ideal_internal_store(g_data, ideal);
    goto _gcleanup;

gen_fail:
    lv_free_many(&var_x, &var_y, NULL);
    /* 清理已分配的生成元 */
    for (int i = 0; i < ideal->generator_count; i++) {
        poly_internal_destroy(ideal->generators[i]);
    }
    lv_free((void **)&ideal->generators);
    lv_free((void **)&ideal);
    goto _gcleanup;
#undef ADD_GENERATOR_LOCKED

GROEBNER_LOCK_GUARD_END();
    return ret;
}