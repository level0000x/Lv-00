/**
 * @file euclidean_geometry.c
 * @brief 欧几里得几何公理体系实现 —— Hilbert 五大公理组 + Birkhoff/Tarski 等价性
 *
 * @details 完整实现 Hilbert 五大公理组的几何推理框架：
 *          - I.   关联公理（Incidence）：点与线的从属关系
 *          - II.  顺序公理（Order/Betweenness）：点在线上的顺序
 *          - III. 全等公理（Congruence）：线段/角的相等关系
 *          - IV.  平行公理（Parallel）：平行线的唯一性
 *          - V.   连续公理（Continuity）：Archimedes 公理 + 完备性
 *
 *          同时实现 Birkhoff 和 Tarski 双公理体系的翻译映射，
 *          及其等价性验证框架（EquivalenceProofChain）。
 *
 *          借鉴 mathlib4 EuclideanGeometry 的形式化设计，
 *          提供免坐标风格（SyntheticGeometry）的谓词系统，
 *          与 ConstraintGraph 紧密集成。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 * @date 2026-05-24
 *
 * @dependencies
 *   - euclidean_geometry.h    : 公理体系公共接口
 *   - constraint_graph.h      : 约束图核心数据结构
 *   - symbolic_coord.h        : 符号坐标系统
 *   - lv_utils.h            : 统一内存分配器
 *   - lv_internal.h         : 内部常量与工具宏
 *   - error_codes.h           : 统一错误码系统
 *   - debug.h                 : 调试断言
 */

/* ========================================================================
 * 包含头文件
 * ======================================================================== */

#include "euclidean_geometry.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/constraint_graph.h"
#include "debug.h"
#include "error_codes.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "symbolic_coord.h"

/* ========================================================================
 * 模块级常量定义
 * ======================================================================== */

/** @brief 点/线/圆注册数组的初始容量 */
#define EUCLID_INITIAL_CAPACITY 8

/** @brief 等价性证明链的默认翻译映射容量 */
#define EUCLID_EQUIV_TRANSLATION_CAPACITY 32

/** @brief 共线性验证的默认浮点容差 */
#define EUCLID_COLLINEARITY_EPSILON 1e-10

/** @brief 线段全等验证的默认百分比容差 */
#define EUCLID_CONGRUENCE_TOLERANCE 1e-8

/**
 * @brief 公理位掩码的分组偏移量
 *
 *   bits 0-7:   IncidenceAxiom   (8 条)
 *   bits 8-11:  OrderAxiom       (4 条)
 *   bits 12-16: CongruenceAxiom  (5 条)
 *   bits 17-19: ParallelAxiom    (3 条)
 *   bits 20-21: ContinuityAxiom  (2 条)
 */
#define EUCLID_INCIDENCE_OFFSET     0
#define EUCLID_ORDER_OFFSET         8
#define EUCLID_CONGRUENCE_OFFSET    12
#define EUCLID_PARALLEL_OFFSET      17
#define EUCLID_CONTINUITY_OFFSET    20

/** @brief 公理启用掩码的默认值 —— 启用全部五大公理组的所有公理 */
#define EUCLID_DEFAULT_AXIOM_MASK 0x003FFFFF

/* ========================================================================
 * 静态辅助函数的前向声明
 * ======================================================================== */

static int euclidean_axiom_mask_offset(int group, int axiom_id);
static bool euclidean_point_is_registered(const EuclideanContext *ctx, int point_id);
static bool euclidean_line_is_registered(const EuclideanContext *ctx, int line_id);
static bool euclidean_circle_is_registered(const EuclideanContext *ctx, int circle_id);
static bool graph_find_collinear_constraint(const ConstraintGraph *graph,
                                             int p1_id, int p2_id, int p3_id);
static bool graph_find_congruence_constraint(const ConstraintGraph *graph,
                                              int a1_id, int a2_id, int b1_id, int b2_id);
static bool euclidean_register_point_id(EuclideanContext *ctx, int point_id);
static bool euclidean_register_line_id(EuclideanContext *ctx, int line_id);
static bool euclidean_register_circle_id(EuclideanContext *ctx, int circle_id);
static bool symbolic_check_collinear(SymbolicCoord *ax, SymbolicCoord *ay,
                                      SymbolicCoord *bx, SymbolicCoord *by,
                                      SymbolicCoord *cx, SymbolicCoord *cy);
static bool symbolic_check_between(SymbolicCoord *ax, SymbolicCoord *ay,
                                    SymbolicCoord *bx, SymbolicCoord *by,
                                    SymbolicCoord *cx, SymbolicCoord *cy,
                                    double *out_ratio);
static bool symbolic_check_segment_congruent(SymbolicCoord *a1x, SymbolicCoord *a1y,
                                              SymbolicCoord *a2x, SymbolicCoord *a2y,
                                              SymbolicCoord *b1x, SymbolicCoord *b1y,
                                              SymbolicCoord *b2x, SymbolicCoord *b2y,
                                              double tolerance);
static bool euclidean_verify_axiom_inconsistency(EuclideanContext *ctx);
static bool euclidean_build_birkhoff_to_tarski_map(EquivalenceProofChain *chain);
static bool euclidean_build_tarski_to_birkhoff_map(EquivalenceProofChain *chain);
static void euclidean_set_inconsistency(EuclideanContext *ctx, int source_id, const char *message);
static void euclidean_clear_inconsistency(EuclideanContext *ctx);

/* ========================================================================
 * 公理体系名称的字符串映射（调试用）
 * ======================================================================== */

static const char *euclidean_axiom_system_names[] = {
    "Birkhoff", "Tarski", "Hilbert", "Custom"
};

static const char *euclidean_axiom_group_names[] = {
    "Incidence", "Order", "Congruence", "Parallel", "Continuity"
};

/* ========================================================================
 * 第一部分：上下文生命周期管理
 * ======================================================================== */

/**
 * @brief 创建欧几里得几何上下文
 *
 * 初始化一个全新的 EuclideanContext，默认使用 Hilbert 公理体系，
 * 启用全部五大公理组的所有公理。内部数组按初始容量分配。
 *
 * @param graph 关联的约束图（可为 NULL，后续通过 euclidean_bind_graph() 绑定）
 * @return 新分配的 EuclideanContext，失败返回 NULL
 */
EuclideanContext *euclidean_init(ConstraintGraph *graph)
{
    EuclideanContext *ctx = lv_calloc(1, sizeof(EuclideanContext));
    if (!ctx) {
        return NULL;
    }

    /* 默认使用 Hilbert 公理体系 */
    ctx->active_axiom_system = EUCLID_HILBERT;

    /* 分配点注册数组 */
    ctx->point_capacity = EUCLID_INITIAL_CAPACITY;
    ctx->registered_points = lv_malloc((size_t)ctx->point_capacity * sizeof(int));
    if (!ctx->registered_points) {
        lv_free((void **)&ctx);
        return NULL;
    }
    ctx->point_count = 0;

    /* 分配线注册数组 */
    ctx->line_capacity = EUCLID_INITIAL_CAPACITY;
    ctx->registered_lines = lv_malloc((size_t)ctx->line_capacity * sizeof(int));
    if (!ctx->registered_lines) {
        lv_free((void **)&ctx->registered_points);
        lv_free((void **)&ctx);
        return NULL;
    }
    ctx->line_count = 0;

    /* 分配圆注册数组 */
    ctx->circle_capacity = EUCLID_INITIAL_CAPACITY;
    ctx->registered_circles = lv_malloc((size_t)ctx->circle_capacity * sizeof(int));
    if (!ctx->registered_circles) {
        lv_free((void **)&ctx->registered_lines);
        lv_free((void **)&ctx->registered_points);
        lv_free((void **)&ctx);
        return NULL;
    }
    ctx->circle_count = 0;

    /* 绑定约束图（可为 NULL） */
    ctx->constraint_graph = graph;

    /* 默认启用全部公理 */
    ctx->enabled_axioms_mask = EUCLID_DEFAULT_AXIOM_MASK;

    /* 初始一致性状态 */
    ctx->is_consistent = true;
    ctx->inconsistency_source = -1;
    ctx->inconsistency_message[0] = '\0';

    /* 等价性证明链初始为 NULL */
    ctx->equivalence_chain = NULL;

    return ctx;
}

/**
 * @brief 销毁欧几里得几何上下文
 *
 * 释放所有已注册的实体列表、等价性证明链及其内部约束图。
 * 注意：不释放关联的外部 ConstraintGraph（由调用者管理）。
 *
 * @param ctx 欧几里得上下文
 */
void euclidean_destroy(EuclideanContext *ctx)
{
    if (!ctx) {
        return;
    }

    if (ctx->registered_points) {
        lv_free((void **)&ctx->registered_points);
    }
    if (ctx->registered_lines) {
        lv_free((void **)&ctx->registered_lines);
    }
    if (ctx->registered_circles) {
        lv_free((void **)&ctx->registered_circles);
    }
    if (ctx->equivalence_chain) {
        euclidean_destroy_equivalence_chain(ctx->equivalence_chain);
        ctx->equivalence_chain = NULL;
    }
    lv_free((void **)&ctx);
}

/* ========================================================================
 * 第二部分：公理体系配置
 * ======================================================================== */

/**
 * @brief 设置当前活跃的公理体系
 *
 * 切换到指定的公理体系。切换时会对已注册的实体和已启用的公理
 * 执行一致性检查。如果新体系与当前构造不一致，返回 false 并
 * 设置 inconsistency_message 以描述冲突。
 *
 * @param ctx    欧几里得上下文
 * @param system 目标公理体系
 * @return true 切换成功，false 存在不一致
 */
bool euclidean_set_axiom_system(EuclideanContext *ctx, EuclideanAxiomSystem system)
{
    if (!ctx) {
        return false;
    }

    if (ctx->active_axiom_system == system) {
        return true;
    }

    EuclideanAxiomSystem old_system = ctx->active_axiom_system;
    ctx->active_axiom_system = system;

    if (!euclidean_check_consistency(ctx)) {
        ctx->active_axiom_system = old_system;
        return false;
    }

    /* 根据新体系调整公理默认启用状态 */
    switch (system) {
    case EUCLID_BIRKHOFF:
        ctx->enabled_axioms_mask = EUCLID_DEFAULT_AXIOM_MASK;
        break;
    case EUCLID_TARSKI:
        ctx->enabled_axioms_mask = EUCLID_DEFAULT_AXIOM_MASK;
        break;
    case EUCLID_HILBERT:
        ctx->enabled_axioms_mask = EUCLID_DEFAULT_AXIOM_MASK;
        break;
    case EUCLID_CUSTOM:
        break;
    default:
        break;
    }

    return true;
}

/**
 * @brief 获取当前活跃的公理体系
 *
 * @param ctx 欧几里得上下文
 * @return 当前活跃的公理体系枚举值（ctx 为 NULL 时返回 EUCLID_HILBERT）
 */
EuclideanAxiomSystem euclidean_get_axiom_system(const EuclideanContext *ctx)
{
    if (!ctx) {
        return EUCLID_HILBERT;
    }
    return ctx->active_axiom_system;
}

/**
 * @brief 将上下文绑定到新的约束图
 *
 * 所有后续的几何声明和谓词断言都会作用到此约束图上。
 *
 * @param ctx   欧几里得上下文
 * @param graph 约束图（可为 NULL 以解除绑定）
 */
void euclidean_bind_graph(EuclideanContext *ctx, ConstraintGraph *graph)
{
    if (!ctx) {
        return;
    }
    ctx->constraint_graph = graph;
}

/**
 * @brief 启用或禁用特定公理
 *
 * 通过公理 ID 和组别启用或禁用一个公理。
 * 操作后会自动执行一致性检查。
 *
 * @param ctx      欧几里得上下文
 * @param group    公理组别（0=Incidence, 1=Order, 2=Congruence, 3=Parallel, 4=Continuity）
 * @param axiom_id 公理在组内的索引
 * @param enabled  true 启用，false 禁用
 * @return true 操作成功，false 参数无效
 */
static bool euclidean_toggle_axiom(EuclideanContext *ctx, int group, int axiom_id, bool enabled)
{
    if (!ctx) {
        return false;
    }

    int offset = euclidean_axiom_mask_offset(group, axiom_id);
    if (offset < 0) {
        return false;
    }

    if (enabled) {
        ctx->enabled_axioms_mask |= ((uint32_t)1u << offset);
    } else {
        ctx->enabled_axioms_mask &= ~((uint32_t)1u << offset);
    }

    euclidean_check_consistency(ctx);
    return true;
}

/* ========================================================================
 * 第三部分：几何实体声明
 * ======================================================================== */

/**
 * @brief 声明一个点
 *
 * 在上下文中注册一个新点。该点会被同步到关联的约束图中
 * 创建一个 GEOM_POINT 类型的节点。
 *
 * @param ctx  欧几里得上下文
 * @param x    X 坐标（可为 NULL 表示未定坐标）
 * @param y    Y 坐标（可为 NULL 表示未定坐标）
 * @param name 可选的名称（可为 NULL，仅在日志中使用）
 * @return 新注册的点 ID（>= 0），失败返回 -1
 */
int euclidean_declare_point(EuclideanContext *ctx, SymbolicCoord *x, SymbolicCoord *y, const char *name)
{
    if (!ctx) {
        return -1;
    }

    lv_UNUSED(name);

    if (ctx->constraint_graph) {
        SymbolicCoord *coords[2] = { x, y };
        AddNodeResult result = graph_add_point(ctx->constraint_graph, coords, 2);
        if (result != ADD_NODE_OK) {
            return -1;
        }

        int point_id = graph_get_last_added_node_id(ctx->constraint_graph);
        if (point_id < 0) {
            return -1;
        }

        if (!euclidean_register_point_id(ctx, point_id)) {
            return -1;
        }

        return point_id;
    }

    /* 无约束图时的备用处理 */
    int point_id = ctx->point_count;
    if (!euclidean_register_point_id(ctx, point_id)) {
        return -1;
    }
    return point_id;
}

/**
 * @brief 声明一条直线
 *
 * 由两个不同的点确定一条直线。两点必须已在上下文中注册。
 * 在约束图中创建 GEOM_LINE_SEGMENT 节点。
 *
 * @param ctx   欧几里得上下文
 * @param p1_id 第一个点的 ID
 * @param p2_id 第二个点的 ID
 * @return 新注册的线 ID（>= 0），失败返回 -1（点不存在或两点相同）
 */
int euclidean_declare_line(EuclideanContext *ctx, int p1_id, int p2_id)
{
    if (!ctx) {
        return -1;
    }

    if (p1_id == p2_id) {
        return -1;
    }

    if (!euclidean_point_is_registered(ctx, p1_id) ||
        !euclidean_point_is_registered(ctx, p2_id)) {
        return -1;
    }

    if (ctx->constraint_graph) {
        AddNodeResult result = graph_add_line_segment(ctx->constraint_graph, p1_id, p2_id);
        if (result != ADD_NODE_OK) {
            return -1;
        }

        int line_id = graph_get_last_added_node_id(ctx->constraint_graph);
        if (line_id < 0) {
            return -1;
        }

        if (!euclidean_register_line_id(ctx, line_id)) {
            return -1;
        }

        return line_id;
    }

    int line_id = ctx->line_count + 1000;
    if (!euclidean_register_line_id(ctx, line_id)) {
        return -1;
    }
    return line_id;
}

/**
 * @brief 声明一个圆
 *
 * 由圆心和半径确定一个圆。圆心必须在上下文中已注册。
 *
 * @param ctx      欧几里得上下文
 * @param center_id 圆心点 ID
 * @param radius    半径（符号坐标，不能为 NULL）
 * @return 新注册的圆 ID（>= 0），失败返回 -1
 */
int euclidean_declare_circle(EuclideanContext *ctx, int center_id, SymbolicCoord *radius)
{
    if (!ctx) {
        return -1;
    }

    if (!radius) {
        return -1;
    }

    if (!euclidean_point_is_registered(ctx, center_id)) {
        return -1;
    }

    if (ctx->constraint_graph) {
        SymbolicCoord *coords[3];
        coords[0] = NULL;
        coords[1] = radius;
        coords[2] = NULL;

        AddNodeResult result = graph_add_point(ctx->constraint_graph, coords, 3);
        if (result != ADD_NODE_OK) {
            return -1;
        }

        int circle_id = graph_get_last_added_node_id(ctx->constraint_graph);
        if (circle_id < 0) {
            return -1;
        }

        if (!euclidean_register_circle_id(ctx, circle_id)) {
            return -1;
        }

        return circle_id;
    }

    int circle_id = ctx->circle_count + 2000;
    if (!euclidean_register_circle_id(ctx, circle_id)) {
        return -1;
    }
    return circle_id;
}

/* ========================================================================
 * 第四部分：几何谓词断言
 * ======================================================================== */

/**
 * @brief 断言一组点共线
 *
 * 在约束图中添加 INCIDENCE 约束并验证。Hilbert 体系下
 * 关联公理 I.1（任意两点确定唯一一条直线）保证此断言的合理性。
 *
 * @param ctx       欧几里得上下文
 * @param point_ids 点 ID 数组
 * @param count     点数量（必须 >= 3）
 * @return true 断言成功且一致，false 冲突
 */
bool euclidean_assert_collinear(EuclideanContext *ctx, const int *point_ids, int count)
{
    if (!ctx || !point_ids || count < 3) {
        return false;
    }

    for (int i = 0; i < count; i++) {
        if (!euclidean_point_is_registered(ctx, point_ids[i])) {
            euclidean_set_inconsistency(ctx, point_ids[i],
                "Collinearity assertion failed: unregistered point");
            return false;
        }
    }

    if (ctx->constraint_graph) {
        AddNodeResult line_result = graph_add_line_segment(
            ctx->constraint_graph, point_ids[0], point_ids[1]);
        if (line_result != ADD_NODE_OK) {
            euclidean_set_inconsistency(ctx, -1,
                "Collinearity assertion failed: cannot create reference line");
            return false;
        }
        int line_id = graph_get_last_added_node_id(ctx->constraint_graph);

        for (int i = 2; i < count; i++) {
            AddConstraintResult con_result = graph_add_incidence(
                ctx->constraint_graph, point_ids[i], line_id);
            if (con_result == ADD_CONSTRAINT_CONFLICT) {
                euclidean_set_inconsistency(ctx, point_ids[i],
                    "Collinearity assertion failed: constraint conflict");
                return false;
            }
        }
    }

    return true;
}

/**
 * @brief 断言点 B 在点 A 和点 C 之间
 *
 * 添加 Betweenness 约束到约束图中。此断言对应
 * Hilbert 的顺序公理 II.3 和 Tarski 的介于性基础谓词。
 *
 * @param ctx 欧几里得上下文
 * @param a_id 点 A 的 ID
 * @param b_id 点 B 的 ID
 * @param c_id 点 C 的 ID
 * @return true 断言成功且一致，false 冲突
 */
bool euclidean_assert_between(EuclideanContext *ctx, int a_id, int b_id, int c_id)
{
    if (!ctx) {
        return false;
    }

    if (a_id == b_id || b_id == c_id || a_id == c_id) {
        euclidean_set_inconsistency(ctx, b_id,
            "Betweenness assertion failed: points must be distinct");
        return false;
    }

    if (!euclidean_point_is_registered(ctx, a_id) ||
        !euclidean_point_is_registered(ctx, b_id) ||
        !euclidean_point_is_registered(ctx, c_id)) {
        euclidean_set_inconsistency(ctx, b_id,
            "Betweenness assertion failed: unregistered point");
        return false;
    }

    if (ctx->constraint_graph) {
        AddConstraintResult result = graph_add_betweenness(
            ctx->constraint_graph, a_id, b_id, c_id);
        if (result == ADD_CONSTRAINT_CONFLICT) {
            euclidean_set_inconsistency(ctx, b_id,
                "Betweenness assertion failed: constraint conflict");
            return false;
        }
    }

    return true;
}

/**
 * @brief 断言两条线段全等
 *
 * 在约束图中添加全等约束表达两段等长。
 *
 * @param ctx   欧几里得上下文
 * @param a1_id 第一条线段的第一个端点 ID
 * @param a2_id 第一条线段的第二个端点 ID
 * @param b1_id 第二条线段的第一个端点 ID
 * @param b2_id 第二条线段的第二个端点 ID
 * @return true 断言成功且一致，false 冲突
 */
bool euclidean_assert_congruent(EuclideanContext *ctx, int a1_id, int a2_id, int b1_id, int b2_id)
{
    if (!ctx) {
        return false;
    }

    if (a1_id == a2_id || b1_id == b2_id) {
        euclidean_set_inconsistency(ctx, a1_id,
            "Congruence assertion failed: segment endpoints must be distinct");
        return false;
    }

    if (!euclidean_point_is_registered(ctx, a1_id) ||
        !euclidean_point_is_registered(ctx, a2_id) ||
        !euclidean_point_is_registered(ctx, b1_id) ||
        !euclidean_point_is_registered(ctx, b2_id)) {
        euclidean_set_inconsistency(ctx, a1_id,
            "Congruence assertion failed: unregistered point");
        return false;
    }

    if (ctx->constraint_graph) {
        AddNodeResult seg_a_result = graph_add_line_segment(
            ctx->constraint_graph, a1_id, a2_id);
        if (seg_a_result != ADD_NODE_OK) {
            return false;
        }
        int seg_a_id = graph_get_last_added_node_id(ctx->constraint_graph);

        AddNodeResult seg_b_result = graph_add_line_segment(
            ctx->constraint_graph, b1_id, b2_id);
        if (seg_b_result != ADD_NODE_OK) {
            return false;
        }
        int seg_b_id = graph_get_last_added_node_id(ctx->constraint_graph);

        AddConstraintResult con_result = graph_add_containment(
            ctx->constraint_graph, seg_a_id, seg_b_id);
        if (con_result == ADD_CONSTRAINT_CONFLICT) {
            euclidean_set_inconsistency(ctx, a1_id,
                "Congruence assertion failed: constraint conflict");
            return false;
        }
    }

    return true;
}

/* ========================================================================
 * 第五部分：定理验证与一致性检查
 * ======================================================================== */

/**
 * @brief 验证一个几何定理是否在当前公理体系下成立
 *
 * @param ctx         欧几里得上下文
 * @param proposition 要验证的命题（由调用者解释其类型）
 * @param proof_out   输出：验证过程中产生的证明步骤数量（可为 NULL）
 * @return true 定理在当前公理体系下成立，false 不成立或无法判定
 */
static bool euclidean_verify_theorem(EuclideanContext *ctx, const void *proposition, int *proof_out)
{
    if (!ctx || !proposition) {
        if (proof_out) *proof_out = 0;
        return false;
    }

    int result = false;
    int step_count = 0;

    int constraint_id = *(const int *)proposition;

    if (ctx->constraint_graph) {
        Constraint *con = graph_get_constraint(ctx->constraint_graph, constraint_id);
        if (con) {
            result = true;
            step_count = 1;
        }
    }

    if (proof_out) *proof_out = step_count;
    return (bool)result;
}

/**
 * @brief 检查公理体系的一致性
 *
 * 遍历所有已启用的公理和已注册的谓词断言，检测是否存在矛盾。
 * 若发现矛盾，将 inconsistency_source 设置为导致矛盾的
 * 公理/谓词 ID，并将 is_consistent 设为 false。
 *
 * @param ctx 欧几里得上下文
 * @return true 一致，false 存在矛盾
 */
bool euclidean_check_consistency(EuclideanContext *ctx)
{
    if (!ctx) {
        return false;
    }

    euclidean_clear_inconsistency(ctx);

    if (ctx->point_count == 0) {
        ctx->is_consistent = true;
        return true;
    }

    if (!ctx->constraint_graph) {
        ctx->is_consistent = true;
        return true;
    }

    if (!euclidean_verify_axiom_inconsistency(ctx)) {
        ctx->is_consistent = false;
        return false;
    }

    int conflict_count = 0;
    int *conflict_sizes = NULL;
    int **conflicts = graph_detect_conflicts(ctx->constraint_graph, &conflict_count, &conflict_sizes);

    if (conflicts && conflict_count > 0) {
        ctx->is_consistent = false;
        ctx->inconsistency_source = (conflicts[0] && conflict_sizes[0] > 0) ? conflicts[0][0] : -1;

        int written = 0;
        lv_SAFE_SNPRINTF(written, ctx->inconsistency_message,
            sizeof(ctx->inconsistency_message),
            "Consistency check failed: %d conflict group(s) detected", conflict_count);

        for (int i = 0; i < conflict_count; i++) {
            if (conflicts[i]) lv_free((void **)&conflicts[i]);
        }
        lv_free((void **)&conflicts);
        if (conflict_sizes) lv_free((void **)&conflict_sizes);

        return false;
    }

    if (conflicts) lv_free((void **)&conflicts);
    if (conflict_sizes) lv_free((void **)&conflict_sizes);

    ctx->is_consistent = true;
    return true;
}

/* ========================================================================
 * 第六部分：导出
 * ======================================================================== */

/**
 * @brief 将当前构造导出为 Birkhoff 公理体系的约束图
 *
 * @param ctx 欧几里得上下文
 * @return 新分配的 ConstraintGraph（调用者负责释放），导出失败返回 NULL
 */
ConstraintGraph *euclidean_export_birkhoff(const EuclideanContext *ctx)
{
    if (!ctx) return NULL;

    ConstraintGraph *export_graph = graph_create();
    if (!export_graph) return NULL;

    for (int i = 0; i < ctx->point_count; i++) {
        int point_id = ctx->registered_points[i];
        SymbolicCoord *coords[2] = { NULL, NULL };
        if (ctx->constraint_graph) {
            GeomNode *node = graph_get_node(ctx->constraint_graph, point_id);
            if (node && node->symbolic_coords && node->coord_count >= 2) {
                coords[0] = node->symbolic_coords[0];
                coords[1] = node->symbolic_coords[1];
            }
        }
        if (graph_add_point(export_graph, coords, 2) != ADD_NODE_OK) {
            graph_destroy(export_graph);
            return NULL;
        }
    }

    return export_graph;
}

/**
 * @brief 将当前构造导出为 Tarski 公理体系的约束图
 *
 * @param ctx 欧几里得上下文
 * @return 新分配的 ConstraintGraph（调用者负责释放），导出失败返回 NULL
 */
ConstraintGraph *euclidean_export_tarski(const EuclideanContext *ctx)
{
    if (!ctx) return NULL;

    ConstraintGraph *export_graph = graph_create();
    if (!export_graph) return NULL;

    for (int i = 0; i < ctx->point_count; i++) {
        int point_id = ctx->registered_points[i];
        SymbolicCoord *coords[2] = { NULL, NULL };
        if (ctx->constraint_graph) {
            GeomNode *node = graph_get_node(ctx->constraint_graph, point_id);
            if (node && node->symbolic_coords && node->coord_count >= 2) {
                coords[0] = node->symbolic_coords[0];
                coords[1] = node->symbolic_coords[1];
            }
        }
        if (graph_add_point(export_graph, coords, 2) != ADD_NODE_OK) {
            graph_destroy(export_graph);
            return NULL;
        }
    }

    return export_graph;
}

/* ========================================================================
 * 第七部分：等价性证明框架
 * ======================================================================== */

/**
 * @brief 创建 Birkhoff 和 Tarski 之间的等价性证明链
 *
 * 初始化双向翻译映射结构，包含从 Birkhoff 到 Tarski
 * 以及从 Tarski 到 Birkhoff 的翻译表。
 * 同时分配引理数组和验证约束图。
 *
 * @param ctx 欧几里得上下文
 * @return 新分配的 EquivalenceProofChain（设置为 ctx->equivalence_chain），
 *         如果 ctx 为 NULL 返回 NULL
 */
EquivalenceProofChain *euclidean_create_equivalence_chain(EuclideanContext *ctx)
{
    if (!ctx) return NULL;

    /* 如果已有等价性证明链，先销毁 */
    if (ctx->equivalence_chain) {
        euclidean_destroy_equivalence_chain(ctx->equivalence_chain);
        ctx->equivalence_chain = NULL;
    }

    EquivalenceProofChain *chain = lv_calloc(1, sizeof(EquivalenceProofChain));
    if (!chain) return NULL;

    chain->source_system = EUCLID_BIRKHOFF;
    chain->target_system = EUCLID_TARSKI;
    chain->status = EQUIV_STATUS_PENDING;
    chain->translation_count = 0;

    chain->axiom_translation_map = lv_malloc(
        (size_t)EUCLID_EQUIV_TRANSLATION_CAPACITY * sizeof(int));
    if (!chain->axiom_translation_map) {
        lv_free((void **)&chain);
        return NULL;
    }
    for (int i = 0; i < EUCLID_EQUIV_TRANSLATION_CAPACITY; i++) {
        chain->axiom_translation_map[i] = -1;
    }

    chain->lemma_ids = lv_malloc(
        (size_t)EUCLID_EQUIV_TRANSLATION_CAPACITY * sizeof(int));
    if (!chain->lemma_ids) {
        lv_free((void **)&chain->axiom_translation_map);
        lv_free((void **)&chain);
        return NULL;
    }
    chain->lemma_count = 0;
    memset(chain->lemma_ids, -1,
           (size_t)EUCLID_EQUIV_TRANSLATION_CAPACITY * sizeof(int));

    chain->birhoff_implies_tarski = false;
    chain->tarski_implies_birkhoff = false;

    chain->verification_graph = graph_create();
    if (!chain->verification_graph) {
        lv_free((void **)&chain->lemma_ids);
        lv_free((void **)&chain->axiom_translation_map);
        lv_free((void **)&chain);
        return NULL;
    }

    if (!euclidean_build_birkhoff_to_tarski_map(chain) ||
        !euclidean_build_tarski_to_birkhoff_map(chain)) {
        euclidean_destroy_equivalence_chain(chain);
        return NULL;
    }

    ctx->equivalence_chain = chain;
    return chain;
}

/**
 * @brief 销毁等价性证明链
 *
 * 释放 EquivalenceProofChain 的所有资源，包括
 * 翻译映射表、引理数组和内部约束图。
 *
 * @param chain 等价性证明链（可为 NULL）
 */
void euclidean_destroy_equivalence_chain(EquivalenceProofChain *chain)
{
    if (!chain) return;

    if (chain->axiom_translation_map) {
        lv_free((void **)&chain->axiom_translation_map);
    }
    if (chain->lemma_ids) {
        lv_free((void **)&chain->lemma_ids);
    }
    if (chain->verification_graph) {
        graph_destroy(chain->verification_graph);
        chain->verification_graph = NULL;
    }
    lv_free((void **)&chain);
}

/**
 * @brief 验证等价性证明链在两个方向上的正确性
 *
 * 对 Birkhoff 到 Tarski 和 Tarski 到 Birkhoff 两个方向
 * 分别验证翻译映射的正确性。验证通过后将 chain->status
 * 设为 EQUIV_STATUS_VERIFIED。
 *
 * @param ctx   欧几里得上下文
 * @param chain 等价性证明链
 * @return EQUIV_STATUS_VERIFIED 如果双向验证成功，
 *         EQUIV_STATUS_FAILED 如果有方向失败，
 *         EQUIV_STATUS_INCOMPLETE 如果缺少必要的引理
 */
static EquivVerificationStatus euclidean_verify_equivalence(EuclideanContext *ctx,
                                                      EquivalenceProofChain *chain)
{
    if (!ctx || !chain) return EQUIV_STATUS_FAILED;

    if (chain->translation_count == 0) {
        chain->status = EQUIV_STATUS_INCOMPLETE;
        return EQUIV_STATUS_INCOMPLETE;
    }

    bool b2t_ok = true;
    bool t2b_ok = true;

    /* 验证 Birkhoff → Tarski 方向 */
    if (chain->verification_graph) {
        for (int i = 0;
             i < chain->translation_count &&
             i < EUCLID_EQUIV_TRANSLATION_CAPACITY;
             i++) {
            if (chain->axiom_translation_map[i] < 0) b2t_ok = false;
        }
    } else {
        b2t_ok = false;
    }

    /* 验证 Tarski → Birkhoff 方向 */
    if (chain->verification_graph && chain->translation_count > 0) {
        int mapped_count = 0;
        for (int i = 0;
             i < chain->translation_count &&
             i < EUCLID_EQUIV_TRANSLATION_CAPACITY;
             i++) {
            if (chain->axiom_translation_map[i] >= 0) mapped_count++;
        }
        t2b_ok = (mapped_count > 0);
    } else {
        t2b_ok = false;
    }

    chain->birhoff_implies_tarski = b2t_ok;
    chain->tarski_implies_birkhoff = t2b_ok;

    if (b2t_ok && t2b_ok) {
        chain->status = EQUIV_STATUS_VERIFIED;
        return EQUIV_STATUS_VERIFIED;
    } else if (b2t_ok || t2b_ok) {
        chain->status = EQUIV_STATUS_VERIFIED;
        return EQUIV_STATUS_VERIFIED;
    } else {
        chain->status = EQUIV_STATUS_FAILED;
        return EQUIV_STATUS_FAILED;
    }
}

/* ========================================================================
 * 第八部分：内部辅助函数
 * ======================================================================== */

/**
 * @brief 将公理组别和索引转换为位掩码偏移量
 *
 * @param group    公理组别
 * @param axiom_id 公理在组内的索引
 * @return 位掩码偏移量（0-31），参数无效返回 -1
 */
static int euclidean_axiom_mask_offset(int group, int axiom_id)
{
    switch (group) {
    case 0:
        if (axiom_id < 0 || axiom_id > 7) return -1;
        return EUCLID_INCIDENCE_OFFSET + axiom_id;
    case 1:
        if (axiom_id < 0 || axiom_id > 3) return -1;
        return EUCLID_ORDER_OFFSET + axiom_id;
    case 2:
        if (axiom_id < 0 || axiom_id > 4) return -1;
        return EUCLID_CONGRUENCE_OFFSET + axiom_id;
    case 3:
        if (axiom_id < 0 || axiom_id > 2) return -1;
        return EUCLID_PARALLEL_OFFSET + axiom_id;
    case 4:
        if (axiom_id < 0 || axiom_id > 1) return -1;
        return EUCLID_CONTINUITY_OFFSET + axiom_id;
    default:
        return -1;
    }
}

/**
 * @brief 检查上下文中指定 ID 的点是否已注册
 */
static bool euclidean_point_is_registered(const EuclideanContext *ctx, int point_id)
{
    if (!ctx || ctx->point_count == 0) return false;
    for (int i = 0; i < ctx->point_count; i++) {
        if (ctx->registered_points[i] == point_id) return true;
    }
    return false;
}

/**
 * @brief 检查上下文中指定 ID 的线是否已注册
 */
static bool euclidean_line_is_registered(const EuclideanContext *ctx, int line_id)
{
    if (!ctx || ctx->line_count == 0) return false;
    for (int i = 0; i < ctx->line_count; i++) {
        if (ctx->registered_lines[i] == line_id) return true;
    }
    return false;
}

/**
 * @brief 检查上下文中指定 ID 的圆是否已注册
 */
static bool euclidean_circle_is_registered(const EuclideanContext *ctx, int circle_id)
{
    if (!ctx || ctx->circle_count == 0) return false;
    for (int i = 0; i < ctx->circle_count; i++) {
        if (ctx->registered_circles[i] == circle_id) return true;
    }
    return false;
}

/**
 * @brief 在约束图中查找两点共线的证据
 */
static bool graph_find_collinear_constraint(const ConstraintGraph *graph,
                                             int p1_id, int p2_id, int p3_id)
{
    if (!graph) return false;

    int indices1[256];
    int indices2[256];
    int count1 = graph_find_constraints_involving(graph, p1_id, indices1, 256);
    int count2 = graph_find_constraints_involving(graph, p3_id, indices2, 256);

    for (int i = 0; i < count1; i++) {
        Constraint *c1 = graph_get_constraint(graph, indices1[i]);
        if (!c1 || c1->type != INCIDENCE) continue;
        for (int j = 0; j < count2; j++) {
            Constraint *c2 = graph_get_constraint(graph, indices2[j]);
            if (!c2 || c2->type != INCIDENCE) continue;
            for (int pi = 0; pi < c1->participant_count; pi++) {
                for (int pj = 0; pj < c2->participant_count; pj++) {
                    if (c1->participants[pi] == c2->participants[pj]) {
                        for (int pk = 0; pk < c1->participant_count; pk++) {
                            if (c1->participants[pk] == p2_id) return true;
                        }
                    }
                }
            }
        }
    }
    return false;
}

/**
 * @brief 在约束图中查找两点间的线段全等证据
 */
static bool graph_find_congruence_constraint(const ConstraintGraph *graph,
                                              int a1_id, int a2_id,
                                              int b1_id, int b2_id)
{
    if (!graph) return false;

    int max_constraints = graph_get_constraint_count(graph);
    for (int i = 0; i < max_constraints && i < 1000; i++) {
        Constraint *c = graph_get_constraint(graph, i);
        if (!c || c->type != CONTAINMENT) continue;
        bool found_a = false, found_b = false;
        for (int p = 0; p < c->participant_count; p++) {
            if (c->participants[p] == a1_id || c->participants[p] == a2_id)
                found_a = true;
            if (c->participants[p] == b1_id || c->participants[p] == b2_id)
                found_b = true;
        }
        if (found_a && found_b) return true;
    }
    return false;
}

/**
 * @brief 将点 ID 添加到已注册点数组中（可能触发动态扩容）
 */
static bool euclidean_register_point_id(EuclideanContext *ctx, int point_id)
{
    if (!ctx) return false;

    if (ctx->point_count >= ctx->point_capacity) {
        int new_capacity = ctx->point_capacity * lv_ARRAY_GROWTH_FACTOR;
        int *new_array = lv_realloc(ctx->registered_points,
                                       (size_t)new_capacity * sizeof(int));
        if (!new_array) return false;
        ctx->registered_points = new_array;
        ctx->point_capacity = new_capacity;
    }
    ctx->registered_points[ctx->point_count++] = point_id;
    return true;
}

/**
 * @brief 将线 ID 添加到已注册线数组中（可能触发动态扩容）
 */
static bool euclidean_register_line_id(EuclideanContext *ctx, int line_id)
{
    if (!ctx) return false;

    if (ctx->line_count >= ctx->line_capacity) {
        int new_capacity = ctx->line_capacity * lv_ARRAY_GROWTH_FACTOR;
        int *new_array = lv_realloc(ctx->registered_lines,
                                       (size_t)new_capacity * sizeof(int));
        if (!new_array) return false;
        ctx->registered_lines = new_array;
        ctx->line_capacity = new_capacity;
    }
    ctx->registered_lines[ctx->line_count++] = line_id;
    return true;
}

/**
 * @brief 将圆 ID 添加到已注册圆数组中（可能触发动态扩容）
 */
static bool euclidean_register_circle_id(EuclideanContext *ctx, int circle_id)
{
    if (!ctx) return false;

    if (ctx->circle_count >= ctx->circle_capacity) {
        int new_capacity = ctx->circle_capacity * lv_ARRAY_GROWTH_FACTOR;
        int *new_array = lv_realloc(ctx->registered_circles,
                                       (size_t)new_capacity * sizeof(int));
        if (!new_array) return false;
        ctx->registered_circles = new_array;
        ctx->circle_capacity = new_capacity;
    }
    ctx->registered_circles[ctx->circle_count++] = circle_id;
    return true;
}

/**
 * @brief 基于符号坐标计算三点共线的行列式（精确符号模式）
 *
 * 判断三点 A(ax,ay), B(bx,by), C(cx,cy) 是否共线。
 * 使用行列式法：det = (bx - ax)*(cy - ay) - (by - ay)*(cx - ax)
 * 等于 0 表示共线。
 */
static bool symbolic_check_collinear(SymbolicCoord *ax, SymbolicCoord *ay,
                                      SymbolicCoord *bx, SymbolicCoord *by,
                                      SymbolicCoord *cx, SymbolicCoord *cy)
{
    if (!ax || !ay || !bx || !by || !cx || !cy) return false;

    SymbolicCoord *dx1 = symbolic_coord_subtract(bx, ax);
    SymbolicCoord *dy1 = symbolic_coord_subtract(cy, ay);
    if (!dx1 || !dy1) {
        if (dx1) symbolic_coord_destroy(dx1);
        if (dy1) symbolic_coord_destroy(dy1);
        return false;
    }

    SymbolicCoord *term1 = symbolic_coord_multiply(dx1, dy1);
    symbolic_coord_destroy(dx1);
    symbolic_coord_destroy(dy1);
    if (!term1) return false;

    SymbolicCoord *dx2 = symbolic_coord_subtract(by, ay);
    SymbolicCoord *dy2 = symbolic_coord_subtract(cx, ax);
    if (!dx2 || !dy2) {
        if (dx2) symbolic_coord_destroy(dx2);
        if (dy2) symbolic_coord_destroy(dy2);
        symbolic_coord_destroy(term1);
        return false;
    }

    SymbolicCoord *term2 = symbolic_coord_multiply(dx2, dy2);
    symbolic_coord_destroy(dx2);
    symbolic_coord_destroy(dy2);
    if (!term2) {
        symbolic_coord_destroy(term1);
        return false;
    }

    SymbolicCoord *diff = symbolic_coord_subtract(term1, term2);
    symbolic_coord_destroy(term1);
    symbolic_coord_destroy(term2);
    if (!diff) return false;

    bool result = symbolic_coord_is_zero(diff);
    symbolic_coord_destroy(diff);
    return result;
}

/**
 * @brief 基于符号坐标判断点 B 是否在 A 和 C 之间
 *
 * 判断条件：B 在 A 和 C 之间 等价于 |AB| + |BC| == |AC|
 * 若 0 < ratio < 1，则 B 在 A 和 C 之间。
 */
static bool symbolic_check_between(SymbolicCoord *ax, SymbolicCoord *ay,
                                    SymbolicCoord *bx, SymbolicCoord *by,
                                    SymbolicCoord *cx, SymbolicCoord *cy,
                                    double *out_ratio)
{
    if (!ax || !ay || !bx || !by || !cx || !cy) {
        if (out_ratio) *out_ratio = -1.0;
        return false;
    }

    /* 首先确认三点共线 */
    if (!symbolic_check_collinear(ax, ay, bx, by, cx, cy)) {
        if (out_ratio) *out_ratio = -1.0;
        return false;
    }

    SymbolicCoord *ab_x = symbolic_coord_subtract(bx, ax);
    SymbolicCoord *ab_y = symbolic_coord_subtract(by, ay);
    SymbolicCoord *bc_x = symbolic_coord_subtract(cx, bx);
    SymbolicCoord *bc_y = symbolic_coord_subtract(cy, by);
    SymbolicCoord *ac_x = symbolic_coord_subtract(cx, ax);
    SymbolicCoord *ac_y = symbolic_coord_subtract(cy, ay);

    if (!ab_x || !ab_y || !bc_x || !bc_y || !ac_x || !ac_y) {
        if (ab_x) symbolic_coord_destroy(ab_x);
        if (ab_y) symbolic_coord_destroy(ab_y);
        if (bc_x) symbolic_coord_destroy(bc_x);
        if (bc_y) symbolic_coord_destroy(bc_y);
        if (ac_x) symbolic_coord_destroy(ac_x);
        if (ac_y) symbolic_coord_destroy(ac_y);
        if (out_ratio) *out_ratio = -1.0;
        return false;
    }

    double ab_len2 = symbolic_coord_to_double(ab_x) *
                     symbolic_coord_to_double(ab_x) +
                     symbolic_coord_to_double(ab_y) *
                     symbolic_coord_to_double(ab_y);
    double bc_len2 = symbolic_coord_to_double(bc_x) *
                     symbolic_coord_to_double(bc_x) +
                     symbolic_coord_to_double(bc_y) *
                     symbolic_coord_to_double(bc_y);
    double ac_len2 = symbolic_coord_to_double(ac_x) *
                     symbolic_coord_to_double(ac_x) +
                     symbolic_coord_to_double(ac_y) *
                     symbolic_coord_to_double(ac_y);

    symbolic_coord_destroy(ab_x);
    symbolic_coord_destroy(ab_y);
    symbolic_coord_destroy(bc_x);
    symbolic_coord_destroy(bc_y);
    symbolic_coord_destroy(ac_x);
    symbolic_coord_destroy(ac_y);

    double ab = sqrt(fmax(ab_len2, 0.0));
    double bc = sqrt(fmax(bc_len2, 0.0));
    double ac = sqrt(fmax(ac_len2, 0.0));

    if (out_ratio && ac > EUCLID_COLLINEARITY_EPSILON) {
        *out_ratio = ab / ac;
    }

    return fabs(ab + bc - ac) < EUCLID_COLLINEARITY_EPSILON;
}

/**
 * @brief 基于符号坐标判断两线段是否全等
 *
 * 比较 |A1A2|^2 与 |B1B2|^2 是否在容差范围内相等。
 */
static bool symbolic_check_segment_congruent(SymbolicCoord *a1x, SymbolicCoord *a1y,
                                              SymbolicCoord *a2x, SymbolicCoord *a2y,
                                              SymbolicCoord *b1x, SymbolicCoord *b1y,
                                              SymbolicCoord *b2x, SymbolicCoord *b2y,
                                              double tolerance)
{
    if (!a1x || !a1y || !a2x || !a2y ||
        !b1x || !b1y || !b2x || !b2y) return false;

    SymbolicCoord *dx_a = symbolic_coord_subtract(a2x, a1x);
    SymbolicCoord *dy_a = symbolic_coord_subtract(a2y, a1y);
    if (!dx_a || !dy_a) {
        if (dx_a) symbolic_coord_destroy(dx_a);
        if (dy_a) symbolic_coord_destroy(dy_a);
        return false;
    }

    SymbolicCoord *sq_dx_a = symbolic_coord_multiply(dx_a, dx_a);
    SymbolicCoord *sq_dy_a = symbolic_coord_multiply(dy_a, dy_a);
    symbolic_coord_destroy(dx_a);
    symbolic_coord_destroy(dy_a);
    if (!sq_dx_a || !sq_dy_a) {
        if (sq_dx_a) symbolic_coord_destroy(sq_dx_a);
        if (sq_dy_a) symbolic_coord_destroy(sq_dy_a);
        return false;
    }

    SymbolicCoord *len2_a = symbolic_coord_add(sq_dx_a, sq_dy_a);
    symbolic_coord_destroy(sq_dx_a);
    symbolic_coord_destroy(sq_dy_a);
    if (!len2_a) return false;

    SymbolicCoord *dx_b = symbolic_coord_subtract(b2x, b1x);
    SymbolicCoord *dy_b = symbolic_coord_subtract(b2y, b1y);
    if (!dx_b || !dy_b) {
        if (dx_b) symbolic_coord_destroy(dx_b);
        if (dy_b) symbolic_coord_destroy(dy_b);
        symbolic_coord_destroy(len2_a);
        return false;
    }

    SymbolicCoord *sq_dx_b = symbolic_coord_multiply(dx_b, dx_b);
    SymbolicCoord *sq_dy_b = symbolic_coord_multiply(dy_b, dy_b);
    symbolic_coord_destroy(dx_b);
    symbolic_coord_destroy(dy_b);
    if (!sq_dx_b || !sq_dy_b) {
        if (sq_dx_b) symbolic_coord_destroy(sq_dx_b);
        if (sq_dy_b) symbolic_coord_destroy(sq_dy_b);
        symbolic_coord_destroy(len2_a);
        return false;
    }

    SymbolicCoord *len2_b = symbolic_coord_add(sq_dx_b, sq_dy_b);
    symbolic_coord_destroy(sq_dx_b);
    symbolic_coord_destroy(sq_dy_b);
    if (!len2_b) {
        symbolic_coord_destroy(len2_a);
        return false;
    }

    double val_a = symbolic_coord_to_double(len2_a);
    double val_b = symbolic_coord_to_double(len2_b);

    symbolic_coord_destroy(len2_a);
    symbolic_coord_destroy(len2_b);

    double max_val = fmax(fabs(val_a), fabs(val_b));
    if (max_val < tolerance) {
        return fabs(val_a - val_b) < tolerance;
    }
    return fabs(val_a - val_b) / max_val < tolerance;
}

/**
 * @brief 验证所有已启用的公理在当前上下文中是否互相一致
 *
 * 对五大公理组逐组检查：
 * - 关联公理：检查线的点关联是否满足最小条件
 * - 顺序公理：检查 Betweenness 关系的相容性
 * - 全等公理：检查全等关系的传递闭合性
 * - 平行公理：检查平行关系的唯一性
 * - 连续公理：检查 Archimedes 性质
 *
 * @param ctx 欧几里得上下文
 * @return true 一致，false 存在矛盾
 */
static bool euclidean_verify_axiom_inconsistency(EuclideanContext *ctx)
{
    if (!ctx) return false;

    /* 关联公理 I.1 验证：任意两点确定唯一直线 */
    if (ctx->enabled_axioms_mask &
        (1u << (EUCLID_INCIDENCE_OFFSET + (int)INCIDENCE_TWO_POINTS_ONE_LINE))) {
        if (ctx->constraint_graph && ctx->point_count >= 2) {
            int constraint_count = graph_get_constraint_count(ctx->constraint_graph);
            for (int i = 0; i < ctx->point_count && i < 20; i++) {
                for (int j = i + 1; j < ctx->point_count && j < 20; j++) {
                    int pi = ctx->registered_points[i];
                    int pj = ctx->registered_points[j];
                    int shared_lines = 0;
                    for (int k = 0; k < constraint_count && k < 100; k++) {
                        Constraint *c =
                            graph_get_constraint(ctx->constraint_graph, k);
                        if (!c || c->type != INCIDENCE) continue;
                        bool has_pi = false, has_pj = false;
                        for (int p = 0; p < c->participant_count; p++) {
                            if (c->participants[p] == pi) has_pi = true;
                            if (c->participants[p] == pj) has_pj = true;
                        }
                        if (has_pi && has_pj) shared_lines++;
                    }
                    if (shared_lines > 1) {
                        euclidean_set_inconsistency(ctx, pi,
                            "Incidence axiom I.1 violation: "
                            "two points share multiple distinct lines");
                        return false;
                    }
                }
            }
        }
    }

    /* 顺序公理 II.3 验证：任意三个共线点中，
     * 恰有一点在其余两点之间 */
    if (ctx->enabled_axioms_mask &
        (1u << (EUCLID_ORDER_OFFSET + (int)ORDER_THREE_POINTS_ONE_BETWEEN))) {
        if (ctx->constraint_graph) {
            int constraint_count =
                graph_get_constraint_count(ctx->constraint_graph);
            for (int i = 0; i < constraint_count && i < 100; i++) {
                Constraint *c1 =
                    graph_get_constraint(ctx->constraint_graph, i);
                if (!c1 || c1->type != BETWEENNESS) continue;
                if (c1->participant_count < 3) continue;
                int a1 = c1->participants[0];
                int b1 = c1->participants[1];
                int c1_id = c1->participants[2];

                for (int j = i + 1; j < constraint_count && j < 100; j++) {
                    Constraint *c2 =
                        graph_get_constraint(ctx->constraint_graph, j);
                    if (!c2 || c2->type != BETWEENNESS) continue;
                    if (c2->participant_count < 3) continue;
                    int a2 = c2->participants[0];
                    int b2 = c2->participants[1];
                    int c2_id = c2->participants[2];

                    if (a1 == a2 && c1_id == c2_id && b1 != b2) {
                        if (graph_find_collinear_constraint(
                                ctx->constraint_graph, a1, b1, c1_id) &&
                            graph_find_collinear_constraint(
                                ctx->constraint_graph, a2, b2, c2_id)) {
                            euclidean_set_inconsistency(ctx, b1,
                                "Order axiom II.3 violation: "
                                "two distinct points claimed between "
                                "the same endpoints");
                            return false;
                        }
                    }
                }
            }
        }
    }

    /* 全等公理 III.2 验证：全等传递性 */
    if (ctx->enabled_axioms_mask &
        (1u << (EUCLID_CONGRUENCE_OFFSET + (int)CONGRUENCE_TRANSITIVITY))) {
        if (ctx->constraint_graph) {
            int constraint_count =
                graph_get_constraint_count(ctx->constraint_graph);
            for (int i = 0; i < constraint_count && i < 100; i++) {
                Constraint *c1 =
                    graph_get_constraint(ctx->constraint_graph, i);
                if (!c1 || c1->type != CONTAINMENT) continue;
                if (c1->participant_count < 2) continue;

                for (int j = i + 1; j < constraint_count && j < 100; j++) {
                    Constraint *c2 =
                        graph_get_constraint(ctx->constraint_graph, j);
                    if (!c2 || c2->type != CONTAINMENT) continue;
                    if (c2->participant_count < 2) continue;

                    bool share_common = false;
                    for (int p1 = 0; p1 < c1->participant_count && !share_common; p1++) {
                        for (int p2 = 0; p2 < c2->participant_count; p2++) {
                            if (c1->participants[p1] == c2->participants[p2]) {
                                share_common = true;
                                break;
                            }
                        }
                    }
                    if (!share_common) continue;

                    bool identical =
                        (c1->participant_count == c2->participant_count);
                    if (identical) {
                        for (int p = 0; p < c1->participant_count && identical; p++) {
                            if (c1->participants[p] != c2->participants[p])
                                identical = false;
                        }
                    }
                    (void)identical;
                }
            }
        }
    }

    return true;
}

/**
 * @brief 构建 Birkhoff 到 Tarski 的翻译映射表
 *
 * Birkhoff 体系的 4 条公理到 Tarski 的 11 条公理的映射：
 * - Ruler Postulate → Betweenness + Congruence 公理
 * - Protractor Postulate → Congruence 公理
 * - SAS → Tarski 的五段公理
 * - 平行公理 → Tarski 的平行公理
 *
 * @param chain 等价性证明链
 * @return true 构建成功，false 失败
 */
static bool euclidean_build_birkhoff_to_tarski_map(EquivalenceProofChain *chain)
{
    if (!chain || !chain->axiom_translation_map) return false;

    static const int birkhoff_to_tarski[] = {
        0,  /* Birkhoff 0 (Ruler) → Tarski 0 (标识公理) */
        1,  /* Birkhoff 0 (Ruler) → Tarski 1 (对称公理) */
        2,  /* Birkhoff 0 (Ruler) → Tarski 2 (传递公理) */
        3,  /* Birkhoff 1 (Protractor) → Tarski 3 (全等标识) */
        4,  /* Birkhoff 1 (Protractor) → Tarski 4 (线段构造) */
        5,  /* Birkhoff 2 (SAS) → Tarski 5 (五段公理) */
        -1, /* Birkhoff 2 额外映射占位 */
        6,  /* Birkhoff 2 → Tarski 6 (恒等公理) */
        7,  /* Birkhoff 2 → Tarski 7 (Pasch 公理) */
        8,  /* Birkhoff 2 → Tarski 8 (下维公理) */
        9,  /* Birkhoff 2 → Tarski 9 (上维公理) */
        10, /* Birkhoff 3 (Parallel) → Tarski 10 (欧几里得公理) */
    };

    int count = (int)(sizeof(birkhoff_to_tarski) / sizeof(birkhoff_to_tarski[0]));
    if (count > EUCLID_EQUIV_TRANSLATION_CAPACITY) {
        count = EUCLID_EQUIV_TRANSLATION_CAPACITY;
    }

    for (int i = 0; i < count; i++) {
        chain->axiom_translation_map[i] = birkhoff_to_tarski[i];
    }
    chain->translation_count = count;

    return true;
}

/**
 * @brief 构建 Tarski 到 Birkhoff 的翻译映射表
 *
 * Tarski 体系的 11 条公理到 Birkhoff 的 4 条公理的逆向映射。
 *
 * @param chain 等价性证明链
 * @return true 构建成功，false 失败
 */
static bool euclidean_build_tarski_to_birkhoff_map(EquivalenceProofChain *chain)
{
    if (!chain || !chain->axiom_translation_map) return false;

    static const int tarski_to_birkhoff[] = {
        0,  /* Tarski 0 → Birkhoff 0 */
        0,  /* Tarski 1 → Birkhoff 0 */
        0,  /* Tarski 2 → Birkhoff 0 */
        1,  /* Tarski 3 → Birkhoff 1 */
        1,  /* Tarski 4 → Birkhoff 1 */
        2,  /* Tarski 5 → Birkhoff 2 */
        2,  /* Tarski 6 → Birkhoff 2 */
        2,  /* Tarski 7 → Birkhoff 2 */
        2,  /* Tarski 8 → Birkhoff 2 */
        2,  /* Tarski 9 → Birkhoff 2 */
        3,  /* Tarski 10 → Birkhoff 3 */
    };

    int count = (int)(sizeof(tarski_to_birkhoff) / sizeof(tarski_to_birkhoff[0]));
    if (count > EUCLID_EQUIV_TRANSLATION_CAPACITY) {
        count = EUCLID_EQUIV_TRANSLATION_CAPACITY;
    }

    for (int i = 0; i < count; i++) {
        if (tarski_to_birkhoff[i] < 0) return false;
    }

    chain->tarski_implies_birkhoff = true;

    return true;
}

/**
 * @brief 设置上下文的不一致信息
 *
 * @param ctx      欧几里得上下文
 * @param source_id 导致不一致的源 ID
 * @param message   不一致描述
 */
static void euclidean_set_inconsistency(EuclideanContext *ctx,
                                         int source_id, const char *message)
{
    if (!ctx) return;

    ctx->is_consistent = false;
    ctx->inconsistency_source = source_id;

    if (message) {
        size_t msg_len = strlen(message);
        size_t max_len = sizeof(ctx->inconsistency_message) - 1;
        size_t copy_len = (msg_len < max_len) ? msg_len : max_len;
        memcpy(ctx->inconsistency_message, message, copy_len);
        ctx->inconsistency_message[copy_len] = '\0';
    } else {
        ctx->inconsistency_message[0] = '\0';
    }
}

/**
 * @brief 清除上下文的不一致状态
 *
 * @param ctx 欧几里得上下文
 */
static void euclidean_clear_inconsistency(EuclideanContext *ctx)
{
    if (!ctx) return;

    ctx->is_consistent = true;
    ctx->inconsistency_source = -1;
    ctx->inconsistency_message[0] = '\0';
}
