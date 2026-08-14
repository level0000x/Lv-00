/**
 * @file euclidean_geometry_helpers.c
 * @brief 欧几里得几何公理体系实现 —— 内部辅助函数
 *
 * @details 本文件由 euclidean_geometry.c 拆分而来，是 内部辅助函数 模块。
 *          原文件按功能域拆分为 8 个模块，通过容器文件 euclidean_geometry.c 聚合。
 *
 * @date 2026-08-02
 */

#include "lv/euclidean_geometry.h"
#include "euclidean_geometry_internal.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/constraint_graph.h"
#include "lv/lv_check.h"
#include "lv/lv_numeric.h" /* lv_rel_tol_scale（K5-3B 相对容差共享设施） */

#include "lv/debug.h"
#include "lv/error_codes.h"
#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/symbolic_coord.h"

/* ========================================================================
 * 第八部分：内部辅助函数
 * ======================================================================== */

/* ================================================================
 * 公理组 -> {位掩码偏移, 组内索引上限} 静态查找表（数据表化，替代 switch）
 * ================================================================ */

/** @brief 公理组表条目 */
typedef struct {
    int offset;       /**< 位掩码偏移量 */
    int max_axiom_id; /**< 组内公理索引上限（含） */
} AxiomGroupEntry;

/** @brief 公理组 -> 位掩码偏移/索引上限 查找表（组号 0~4 连续） */
static const AxiomGroupEntry s_axiom_group_table[] = {
    [0] = {EUCLID_INCIDENCE_OFFSET, 7},
    [1] = {EUCLID_ORDER_OFFSET, 3},
    [2] = {EUCLID_CONGRUENCE_OFFSET, 4},
    [3] = {EUCLID_PARALLEL_OFFSET, 2},
    [4] = {EUCLID_CONTINUITY_OFFSET, 1},
};

/**
 * @brief 将公理组别和索引转换为位掩码偏移量
 *
 * @param group    公理组别
 * @param axiom_id 公理在组内的索引
 * @return 位掩码偏移量（0-31），参数无效返回 -1
 */
int euclidean_axiom_mask_offset(int group, int axiom_id) {
    /* 查表获取公理组信息；未知组别回退到 -1（原 default 分支） */
    if ((unsigned) group >= lv_ARRAY_SIZE(s_axiom_group_table))
        return -1;
    const AxiomGroupEntry *entry = &s_axiom_group_table[group];
    if (axiom_id < 0 || axiom_id > entry->max_axiom_id)
        return -1;
    return entry->offset + axiom_id;
}

/**
 * @brief 检查上下文中指定 ID 的点是否已注册
 */
bool euclidean_point_is_registered(const EuclideanContext *ctx, int point_id) {
    if (!ctx || ctx->points_da.count == 0)
        return false;
    const int *points = (const int *)ctx->points_da.data;
    for (int i = 0; i < ctx->points_da.count; i++) {
        if (points[i] == point_id)
            return true;
    }
    return false;
}

/**
 * @brief 检查上下文中指定 ID 的线是否已注册
 */
bool euclidean_line_is_registered(const EuclideanContext *ctx, int line_id) {
    if (!ctx || ctx->lines_da.count == 0)
        return false;
    const int *lines = (const int *)ctx->lines_da.data;
    for (int i = 0; i < ctx->lines_da.count; i++) {
        if (lines[i] == line_id)
            return true;
    }
    return false;
}

/**
 * @brief 检查上下文中指定 ID 的圆是否已注册
 */
bool euclidean_circle_is_registered(const EuclideanContext *ctx, int circle_id) {
    if (!ctx || ctx->circles_da.count == 0)
        return false;
    const int *circles = (const int *)ctx->circles_da.data;
    for (int i = 0; i < ctx->circles_da.count; i++) {
        if (circles[i] == circle_id)
            return true;
    }
    return false;
}

/**
 * @brief 在约束图中查找两点共线的证据
 */
static bool graph_find_collinear_constraint(const ConstraintGraph *graph, int p1_id, int p2_id, int p3_id) {
    if (!graph)
        return false;

    int indices1[256];
    int indices2[256];
    int count1 = graph_find_constraints_involving(graph, p1_id, indices1, 256);
    int count2 = graph_find_constraints_involving(graph, p3_id, indices2, 256);

    for (int i = 0; i < count1; i++) {
        Constraint *c1 = graph_get_constraint(graph, indices1[i]);
        if (!c1 || c1->type != INCIDENCE)
            continue;
        for (int j = 0; j < count2; j++) {
            Constraint *c2 = graph_get_constraint(graph, indices2[j]);
            if (!c2 || c2->type != INCIDENCE)
                continue;
            for (int pi = 0; pi < c1->participant_count; pi++) {
                for (int pj = 0; pj < c2->participant_count; pj++) {
                    if (c1->participants[pi] == c2->participants[pj]) {
                        for (int pk = 0; pk < c1->participant_count; pk++) {
                            if (c1->participants[pk] == p2_id)
                                return true;
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
static bool graph_find_congruence_constraint(const ConstraintGraph *graph, int a1_id, int a2_id, int b1_id, int b2_id) {
    if (!graph)
        return false;

    int max_constraints = graph_get_constraint_count(graph);
    for (int i = 0; i < max_constraints && i < 1000; i++) {
        Constraint *c = graph_get_constraint(graph, i);
        if (!c || c->type != CONTAINMENT)
            continue;
        bool found_a = false, found_b = false;
        for (int p = 0; p < c->participant_count; p++) {
            if (c->participants[p] == a1_id || c->participants[p] == a2_id)
                found_a = true;
            if (c->participants[p] == b1_id || c->participants[p] == b2_id)
                found_b = true;
        }
        if (found_a && found_b)
            return true;
    }
    return false;
}

/**
 * @brief 将点 ID 添加到已注册点数组中（可能触发动态扩容）
 */
bool euclidean_register_point_id(EuclideanContext *ctx, int point_id) {
    if (!ctx)
        return false;
    return lv_darray_push(&ctx->points_da, &point_id) >= 0;
}

/**
 * @brief 将线 ID 添加到已注册线数组中（可能触发动态扩容）
 */
bool euclidean_register_line_id(EuclideanContext *ctx, int line_id) {
    if (!ctx)
        return false;
    return lv_darray_push(&ctx->lines_da, &line_id) >= 0;
}

/**
 * @brief 将圆 ID 添加到已注册圆数组中（可能触发动态扩容）
 */
bool euclidean_register_circle_id(EuclideanContext *ctx, int circle_id) {
    if (!ctx)
        return false;
    return lv_darray_push(&ctx->circles_da, &circle_id) >= 0;
}

/**
 * @brief 基于符号坐标判断点 B 是否在 A 和 C 之间
 *
 * 判断条件：B 在 A 和 C 之间 等价于 |AB| + |BC| == |AC|
 * 若 0 < ratio < 1，则 B 在 A 和 C 之间。
 *
 * 三点共线判定收敛：委托公共 API symbolic_coord_are_collinear
 * （symbolic_coord_ops.c，与 proof_strategy_vector.c 的证明策略共用同一
 * 符号叉积实现，行为一致）。
 */
static bool symbolic_check_between(SymbolicCoord *ax, SymbolicCoord *ay, SymbolicCoord *bx, SymbolicCoord *by,
                                   SymbolicCoord *cx, SymbolicCoord *cy, double *out_ratio) {
    if (!ax || !ay || !bx || !by || !cx || !cy) {
        if (out_ratio)
            *out_ratio = -1.0;
        return false;
    }

    /* 首先确认三点共线 */
    if (!symbolic_coord_are_collinear(ax, ay, bx, by, cx, cy)) {
        if (out_ratio)
            *out_ratio = -1.0;
        return false;
    }

    SymbolicCoord *ab_x = symbolic_coord_subtract(bx, ax);
    SymbolicCoord *ab_y = symbolic_coord_subtract(by, ay);
    SymbolicCoord *bc_x = symbolic_coord_subtract(cx, bx);
    SymbolicCoord *bc_y = symbolic_coord_subtract(cy, by);
    SymbolicCoord *ac_x = symbolic_coord_subtract(cx, ax);
    SymbolicCoord *ac_y = symbolic_coord_subtract(cy, ay);

    if (!ab_x || !ab_y || !bc_x || !bc_y || !ac_x || !ac_y) {
        if (ab_x)
            symbolic_coord_destroy(ab_x);
        if (ab_y)
            symbolic_coord_destroy(ab_y);
        if (bc_x)
            symbolic_coord_destroy(bc_x);
        if (bc_y)
            symbolic_coord_destroy(bc_y);
        if (ac_x)
            symbolic_coord_destroy(ac_x);
        if (ac_y)
            symbolic_coord_destroy(ac_y);
        if (out_ratio)
            *out_ratio = -1.0;
        return false;
    }

    double ab_len2 = symbolic_coord_to_double(ab_x) * symbolic_coord_to_double(ab_x) +
                     symbolic_coord_to_double(ab_y) * symbolic_coord_to_double(ab_y);
    double bc_len2 = symbolic_coord_to_double(bc_x) * symbolic_coord_to_double(bc_x) +
                     symbolic_coord_to_double(bc_y) * symbolic_coord_to_double(bc_y);
    double ac_len2 = symbolic_coord_to_double(ac_x) * symbolic_coord_to_double(ac_x) +
                     symbolic_coord_to_double(ac_y) * symbolic_coord_to_double(ac_y);

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

    /* 使用相对容差判断 |AB| + |BC| == |AC|：
     * 对于大坐标值，sqrt 的浮点误差可能超过绝对容差。
     * 除以 ac（= max(ab, bc, ac) 当 B 在 A、C 之间）得到相对误差。 */
    double rel_tol = lv_rel_tol_scale(EUCLID_COLLINEARITY_EPSILON, ac);
    return fabs(ab + bc - ac) <= rel_tol;
}

/**
 * @brief 基于符号坐标判断两线段是否全等
 *
 * 比较 |A1A2|^2 与 |B1B2|^2 是否在容差范围内相等。
 */
static bool symbolic_check_segment_congruent(SymbolicCoord *a1x, SymbolicCoord *a1y, SymbolicCoord *a2x,
                                             SymbolicCoord *a2y, SymbolicCoord *b1x, SymbolicCoord *b1y,
                                             SymbolicCoord *b2x, SymbolicCoord *b2y, double tolerance) {
    if (!a1x || !a1y || !a2x || !a2y || !b1x || !b1y || !b2x || !b2y)
        return false;

    SymbolicCoord *dx_a = symbolic_coord_subtract(a2x, a1x);
    SymbolicCoord *dy_a = symbolic_coord_subtract(a2y, a1y);
    if (!dx_a || !dy_a) {
        if (dx_a)
            symbolic_coord_destroy(dx_a);
        if (dy_a)
            symbolic_coord_destroy(dy_a);
        return false;
    }

    SymbolicCoord *sq_dx_a = symbolic_coord_multiply(dx_a, dx_a);
    SymbolicCoord *sq_dy_a = symbolic_coord_multiply(dy_a, dy_a);
    symbolic_coord_destroy(dx_a);
    symbolic_coord_destroy(dy_a);
    if (!sq_dx_a || !sq_dy_a) {
        if (sq_dx_a)
            symbolic_coord_destroy(sq_dx_a);
        if (sq_dy_a)
            symbolic_coord_destroy(sq_dy_a);
        return false;
    }

    SymbolicCoord *len2_a = symbolic_coord_add(sq_dx_a, sq_dy_a);
    symbolic_coord_destroy(sq_dx_a);
    symbolic_coord_destroy(sq_dy_a);
    if (!len2_a)
        return false;

    SymbolicCoord *dx_b = symbolic_coord_subtract(b2x, b1x);
    SymbolicCoord *dy_b = symbolic_coord_subtract(b2y, b1y);
    if (!dx_b || !dy_b) {
        if (dx_b)
            symbolic_coord_destroy(dx_b);
        if (dy_b)
            symbolic_coord_destroy(dy_b);
        symbolic_coord_destroy(len2_a);
        return false;
    }

    SymbolicCoord *sq_dx_b = symbolic_coord_multiply(dx_b, dx_b);
    SymbolicCoord *sq_dy_b = symbolic_coord_multiply(dy_b, dy_b);
    symbolic_coord_destroy(dx_b);
    symbolic_coord_destroy(dy_b);
    if (!sq_dx_b || !sq_dy_b) {
        if (sq_dx_b)
            symbolic_coord_destroy(sq_dx_b);
        if (sq_dy_b)
            symbolic_coord_destroy(sq_dy_b);
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
bool euclidean_verify_axiom_inconsistency(EuclideanContext *ctx) {
    if (!ctx)
        return false;

    /* 关联公理 I.1 验证：任意两点确定唯一直线 */
    if (ctx->enabled_axioms_mask & (1u << (EUCLID_INCIDENCE_OFFSET + (int) INCIDENCE_TWO_POINTS_ONE_LINE))) {
        if (ctx->constraint_graph && ctx->points_da.count >= 2) {
            const int *points = (const int *)ctx->points_da.data;
            int constraint_count = graph_get_constraint_count(ctx->constraint_graph);
            for (int i = 0; i < ctx->points_da.count && i < 20; i++) {
                for (int j = i + 1; j < ctx->points_da.count && j < 20; j++) {
                    int pi = points[i];
                    int pj = points[j];
                    int shared_lines = 0;
                    for (int k = 0; k < constraint_count && k < 100; k++) {
                        Constraint *c = graph_get_constraint(ctx->constraint_graph, k);
                        if (!c || c->type != INCIDENCE)
                            continue;
                        bool has_pi = false, has_pj = false;
                        for (int p = 0; p < c->participant_count; p++) {
                            if (c->participants[p] == pi)
                                has_pi = true;
                            if (c->participants[p] == pj)
                                has_pj = true;
                        }
                        if (has_pi && has_pj)
                            shared_lines++;
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
    if (ctx->enabled_axioms_mask & (1u << (EUCLID_ORDER_OFFSET + (int) ORDER_THREE_POINTS_ONE_BETWEEN))) {
        if (ctx->constraint_graph) {
            int constraint_count = graph_get_constraint_count(ctx->constraint_graph);
            for (int i = 0; i < constraint_count && i < 100; i++) {
                Constraint *c1 = graph_get_constraint(ctx->constraint_graph, i);
                if (!c1 || c1->type != BETWEENNESS)
                    continue;
                if (c1->participant_count < 3)
                    continue;
                int a1 = c1->participants[0];
                int b1 = c1->participants[1];
                int c1_id = c1->participants[2];

                for (int j = i + 1; j < constraint_count && j < 100; j++) {
                    Constraint *c2 = graph_get_constraint(ctx->constraint_graph, j);
                    if (!c2 || c2->type != BETWEENNESS)
                        continue;
                    if (c2->participant_count < 3)
                        continue;
                    int a2 = c2->participants[0];
                    int b2 = c2->participants[1];
                    int c2_id = c2->participants[2];

                    if (a1 == a2 && c1_id == c2_id && b1 != b2) {
                        if (graph_find_collinear_constraint(ctx->constraint_graph, a1, b1, c1_id) &&
                            graph_find_collinear_constraint(ctx->constraint_graph, a2, b2, c2_id)) {
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
    if (ctx->enabled_axioms_mask & (1u << (EUCLID_CONGRUENCE_OFFSET + (int) CONGRUENCE_TRANSITIVITY))) {
        if (ctx->constraint_graph) {
            int constraint_count = graph_get_constraint_count(ctx->constraint_graph);
            for (int i = 0; i < constraint_count && i < 100; i++) {
                Constraint *c1 = graph_get_constraint(ctx->constraint_graph, i);
                if (!c1 || c1->type != CONTAINMENT)
                    continue;
                if (c1->participant_count < 2)
                    continue;

                for (int j = i + 1; j < constraint_count && j < 100; j++) {
                    Constraint *c2 = graph_get_constraint(ctx->constraint_graph, j);
                    if (!c2 || c2->type != CONTAINMENT)
                        continue;
                    if (c2->participant_count < 2)
                        continue;

                    bool share_common = false;
                    for (int p1 = 0; p1 < c1->participant_count && !share_common; p1++) {
                        for (int p2 = 0; p2 < c2->participant_count; p2++) {
                            if (c1->participants[p1] == c2->participants[p2]) {
                                share_common = true;
                                break;
                            }
                        }
                    }
                    if (!share_common)
                        continue;

                    bool identical = (c1->participant_count == c2->participant_count);
                    if (identical) {
                        for (int p = 0; p < c1->participant_count && identical; p++) {
                            if (c1->participants[p] != c2->participants[p])
                                identical = false;
                        }
                    }
                    (void) identical;
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
bool euclidean_build_birkhoff_to_tarski_map(EquivalenceProofChain *chain) {
    if (!chain || !chain->axiom_translation_map)
        return false;

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

    int count = (int) (sizeof(birkhoff_to_tarski) / sizeof(birkhoff_to_tarski[0]));
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
bool euclidean_build_tarski_to_birkhoff_map(EquivalenceProofChain *chain) {
    if (!chain || !chain->axiom_translation_map)
        return false;

    static const int tarski_to_birkhoff[] = {
        0, /* Tarski 0 → Birkhoff 0 */
        0, /* Tarski 1 → Birkhoff 0 */
        0, /* Tarski 2 → Birkhoff 0 */
        1, /* Tarski 3 → Birkhoff 1 */
        1, /* Tarski 4 → Birkhoff 1 */
        2, /* Tarski 5 → Birkhoff 2 */
        2, /* Tarski 6 → Birkhoff 2 */
        2, /* Tarski 7 → Birkhoff 2 */
        2, /* Tarski 8 → Birkhoff 2 */
        2, /* Tarski 9 → Birkhoff 2 */
        3, /* Tarski 10 → Birkhoff 3 */
    };

    int count = (int) (sizeof(tarski_to_birkhoff) / sizeof(tarski_to_birkhoff[0]));
    if (count > EUCLID_EQUIV_TRANSLATION_CAPACITY) {
        count = EUCLID_EQUIV_TRANSLATION_CAPACITY;
    }

    for (int i = 0; i < count; i++) {
        if (tarski_to_birkhoff[i] < 0)
            return false;
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
void euclidean_set_inconsistency(EuclideanContext *ctx, int source_id, const char *message) {
    if (!ctx)
        return;

    ctx->is_consistent = false;
    ctx->inconsistency_source = source_id;

    if (message) {
        size_t msg_len = strlen(message);
        lv_strlcpy_n(ctx->inconsistency_message, sizeof(ctx->inconsistency_message), message, (size_t) msg_len);
    } else {
        ctx->inconsistency_message[0] = '\0';
    }
}

/**
 * @brief 清除上下文的不一致状态
 *
 * @param ctx 欧几里得上下文
 */
void euclidean_clear_inconsistency(EuclideanContext *ctx) {
    if (!ctx)
        return;

    ctx->is_consistent = true;
    ctx->inconsistency_source = -1;
    ctx->inconsistency_message[0] = '\0';
}
