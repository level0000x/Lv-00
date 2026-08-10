/**
 * @file meta_proof.c
 * @brief 剪枝合法性元证明实现
 *
 * @details 实现 WFC 范式的数学严格化，包括：
 *          - L1 直接矛盾证明
 *          - L2 传播矛盾证明
 *          - L3 代数排除证明
 *          - 完备性验证
 *
 * @version 5.0.0
 */

#include "lv/meta_proof.h"

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/conflict_detector.h"
#include "lv/constraint_graph.h"
#include "lv/groebner_engine.h"
#include "lv/lv.h"
#include "lv/lv_lifecycle.h"
#include "lv/geo_utils.h"
#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/propagation.h"
#include "lv/symbolic_coord.h"

/* ── 约束类型别名：兼容 meta_proof.c 中使用的命名 ── */
#ifndef CONSTRAINT_INCIDENCE
#define CONSTRAINT_INCIDENCE INCIDENCE
#endif
#ifndef CONSTRAINT_BETWEEN
#define CONSTRAINT_BETWEEN BETWEENNESS
#endif

/* ── 前向声明：内部辅助函数 ── */
static int constraint_graph_get_constraints_for_node(const ConstraintGraph *graph, int node_id, int *out_ids,
                                                     int max_count);
static ConstraintType constraint_graph_get_constraint_type(const ConstraintGraph *graph, int constraint_id);
static PropagationResult propagation_run_with_assignment(PropagationContext *ctx, int node_id,
                                                         const SymbolicCoord *coord, int max_steps);

/* ── L1 矛盾检测辅助函数 ── */

/* 前向声明 */
static bool meta_groebner_candidate_excluded(const ConstraintGraph *graph, int node_id,
                                             const SymbolicCoord *candidate);

/**
 * @brief 检查 INCIDENCE 约束是否与候选坐标矛盾
 *
 * INCIDENCE(point, line_segment) 要求点在线上。
 * 通过计算叉积 (x2-x1)*(yp-y1) - (y2-y1)*(xp-x1) 检查。
 * 叉积非零则点不在线上 → 矛盾。
 *
 * @param graph       约束图
 * @param node_id     被检查的节点 ID（POINT）
 * @param candidate   候选坐标
 * @param con_id      约束 ID
 * @return true 检测到矛盾（点不在线上）
 */
static bool check_incidence_contradiction(const ConstraintGraph *graph, int node_id,
                                           const SymbolicCoord *candidate, int con_id) {
    Constraint *con = NULL;
    for (int i = 0; i < graph->constraint_count; i++) {
        if (graph->constraints[i] && graph->constraints[i]->id == con_id) {
            con = graph->constraints[i];
            break;
        }
    }
    if (!con || con->participant_count != 2)
        return false;

    int point_id = con->participants[0];
    int seg_id = con->participants[1];

    /* 验证 point_id 与当前待检查节点 node_id 一致 */
    if (point_id != node_id) {
        return false;
    }

    /* 查找线段的端点：寻找另一个 INCIDENCE(_, seg_id) 约束 */
    int ep1 = -1, ep2 = -1;
    ep1 = node_id; /* 当前节点是其中一个端点 */

    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *c = graph->constraints[i];
        if (!c || !c->is_active || c->id == con_id)
            continue;
        if (c->type != INCIDENCE || c->participant_count != 2)
            continue;
        if (c->participants[1] == seg_id) {
            int other = c->participants[0];
            if (other != node_id) {
                ep2 = other;
                break;
            }
        }
    }
    if (ep2 < 0)
        return false; /* 无法找到另一个端点，不做判断 */

    /* 获取端点坐标 */
    GeomNode *n1 = NULL, *n2 = NULL;
    for (int i = 0; i < graph->node_count; i++) {
        if (graph->nodes[i] && graph->nodes[i]->id == ep1) n1 = graph->nodes[i];
        if (graph->nodes[i] && graph->nodes[i]->id == ep2) n2 = graph->nodes[i];
    }
    if (!n1 || !n2 || !n1->symbolic_coords || !n2->symbolic_coords)
        return false;
    if (n1->coord_count < 2 || n2->coord_count < 2)
        return false;

    /* 提取坐标值（转为 double 做叉积检测） */
    double x1 = symbolic_coord_to_double(n1->symbolic_coords[0]);
    double y1 = symbolic_coord_to_double(n1->symbolic_coords[1]);
    double x2 = symbolic_coord_to_double(n2->symbolic_coords[0]);
    double y2 = symbolic_coord_to_double(n2->symbolic_coords[1]);
    double xp = symbolic_coord_to_double(candidate);
    double yp = (n1->coord_count >= 2) ? symbolic_coord_to_double(candidate + 1) : 0.0;

    /* 叉积 = (x2-x1)*(yp-y1) - (y2-y1)*(xp-x1) */
    double cross = (x2 - x1) * (yp - y1) - (y2 - y1) * (xp - x1);

    return fabs(cross) > lv_EPSILON_HIGH; /* 叉积非零 → 不在线上 */
}

/* ── L1 约束真实求值辅助函数 ── */

/* 几何判定容差 */
#define META_PROOF_GEOM_EPS lv_GEO_COLLINEAR_EPSILON
#define META_PROOF_ANGLE_EPS lv_EPSILON_LOW
#define META_PROOF_PI 3.14159265358979323846

/**
 * @brief 获取节点的符号坐标（double 值）
 *
 * @param graph   约束图
 * @param node_id 节点 ID
 * @param out_x   输出 x 坐标
 * @param out_y   输出 y 坐标
 * @return true 成功获取，false 节点不存在或无坐标
 */
static bool graph_node_coords(const ConstraintGraph *graph, int node_id, double *out_x, double *out_y) {
    if (!graph || !out_x || !out_y || node_id < 0)
        return false;

    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node || node->id != node_id || !node->symbolic_coords || node->coord_count < 2)
            continue;
        *out_x = symbolic_coord_to_double(node->symbolic_coords[0]);
        *out_y = symbolic_coord_to_double(node->symbolic_coords[1]);
        return true;
    }
    return false;
}

/**
 * @brief 计算角度（度）：以 (vx, vy) 为顶点，两条射线到 (p1x, p1y) 与 (p2x, p2y) 的夹角
 *
 * @return 夹角度数（0~180），任一向量退化时返回 -1
 */
static double compute_angle_degrees(double vx, double vy, double p1x, double p1y, double p2x, double p2y) {
    double ax = p1x - vx, ay = p1y - vy;
    double bx = p2x - vx, by = p2y - vy;
    double la = geo_norm_2d(ax, ay);
    double lb = geo_norm_2d(bx, by);
    if (la < META_PROOF_GEOM_EPS || lb < META_PROOF_GEOM_EPS)
        return -1.0; /* 向量退化 */
    double dot = (ax * bx + ay * by) / (la * lb);
    if (dot > 1.0)
        dot = 1.0;
    if (dot < -1.0)
        dot = -1.0;
    return acos(dot) * 180.0 / META_PROOF_PI;
}

/**
 * @brief 判断点 B 是否位于线段 AC 上（共线且投影参数在 [0,1]）
 *
 * @return true 位于线段上，false 不位于或无法判定
 */
static bool is_point_between_segment(double ax, double ay, double bx, double by, double cx, double cy) {
    /* 共线检测：叉积近似为零 */
    double cross = (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
    if (fabs(cross) > META_PROOF_GEOM_EPS)
        return false;
    /* 投影参数 t = (B-A)·(C-A) / |C-A|² ∈ [0,1] ⇔ B 位于线段 AC 上 */
    double dx = cx - ax, dy = cy - ay;
    double len2 = dx * dx + dy * dy;
    if (len2 < META_PROOF_GEOM_EPS * META_PROOF_GEOM_EPS)
        return false; /* A 与 C 重合，无法判定有序关系 */
    double t = ((bx - ax) * dx + (by - ay) * dy) / len2;
    return t >= -META_PROOF_GEOM_EPS && t <= 1.0 + META_PROOF_GEOM_EPS;
}

/**
 * @brief 检查候选点是否位于指定几何对象上
 *
 * 依据对象类型执行坐标级判定：
 * - GEOM_POINT：与对象坐标重合
 * - GEOM_LINE_SEGMENT：通过 INCIDENCE 约束收集两个端点，检查点是否在线段上
 * - GEOM_CIRCLE：到圆心距离等于半径
 * - 其他类型（区域/端口/函数块）：不做坐标级判定，返回 false
 *
 * @param graph  约束图
 * @param px     候选 x 坐标
 * @param py     候选 y 坐标
 * @param obj_id 几何对象节点 ID
 * @return true 候选点在对象上，false 不在或无法判定
 */
/* ── check_point_on_object 处理器 ── */
typedef bool (*PointOnObjHandler)(const ConstraintGraph *graph, double px, double py, int obj_id, const GeomNode *obj);

static bool point_on_point(const ConstraintGraph *graph, double px, double py, int obj_id, const GeomNode *obj) {
    (void)graph; (void)obj_id;
    if (!obj->symbolic_coords || obj->coord_count < 2) return false;
    double ox = symbolic_coord_to_double(obj->symbolic_coords[0]);
    double oy = symbolic_coord_to_double(obj->symbolic_coords[1]);
    return fabs(px - ox) <= META_PROOF_GEOM_EPS && fabs(py - oy) <= META_PROOF_GEOM_EPS;
}

static bool point_on_line_segment(const ConstraintGraph *graph, double px, double py, int obj_id, const GeomNode *obj) {
    (void)obj;
    double ep[2][2];
    int ep_count = 0;
    for (int i = 0; i < graph->constraint_count && ep_count < 2; i++) {
        Constraint *c = graph->constraints[i];
        if (!c || !c->is_active || c->type != INCIDENCE || c->participant_count != 2) continue;
        if (c->participants[1] != obj_id) continue;
        double ex, ey;
        if (!graph_node_coords(graph, c->participants[0], &ex, &ey)) continue;
        bool dup = false;
        for (int k = 0; k < ep_count; k++) {
            if (fabs(ep[k][0] - ex) <= META_PROOF_GEOM_EPS && fabs(ep[k][1] - ey) <= META_PROOF_GEOM_EPS) {
                dup = true; break;
            }
        }
        if (!dup) { ep[ep_count][0] = ex; ep[ep_count][1] = ey; ep_count++; }
    }
    if (ep_count < 2) return false;
    return is_point_between_segment(ep[0][0], ep[0][1], px, py, ep[1][0], ep[1][1]);
}

static bool point_on_circle(const ConstraintGraph *graph, double px, double py, int obj_id, const GeomNode *obj) {
    (void)obj_id;
    double ox, oy, rx, ry;
    if (!graph_node_coords(graph, obj->data.circle.center_node_id, &ox, &oy)) return false;
    if (!graph_node_coords(graph, obj->data.circle.radius_node_id, &rx, &ry)) return false;
    double radius = geo_distance_2d(ox, oy, rx, ry);
    double dist = geo_distance_2d(ox, oy, px, py);
    return fabs(dist - radius) <= META_PROOF_GEOM_EPS;
}

static bool point_on_default(const ConstraintGraph *graph, double px, double py, int obj_id, const GeomNode *obj) {
    (void)graph; (void)px; (void)py; (void)obj_id; (void)obj; return false;
}

static const PointOnObjHandler point_on_obj_table[] = {
    [GEOM_POINT]        = point_on_point,
    [GEOM_LINE_SEGMENT] = point_on_line_segment,
    [GEOM_CIRCLE]       = point_on_circle,
};

static bool check_point_on_object(const ConstraintGraph *graph, double px, double py, int obj_id) {
    GeomNode *obj = NULL;
    for (int i = 0; i < graph->node_count; i++) {
        if (graph->nodes[i] && graph->nodes[i]->id == obj_id) {
            obj = graph->nodes[i];
            break;
        }
    }
    if (!obj || !obj->is_active) return false;
    PointOnObjHandler h = ((unsigned)obj->type < sizeof(point_on_obj_table)/sizeof(point_on_obj_table[0]))
                          ? point_on_obj_table[obj->type] : NULL;
    return h ? h(graph, px, py, obj_id, obj) : point_on_default(graph, px, py, obj_id, obj);
}

/* ── constraint_eval_contradiction 查找表 ── */
typedef bool (*ConEvalHandler)(const ConstraintGraph *graph, int node_id, double cx, double cy, int con_id, const Constraint *con, const SymbolicCoord *candidate);

static bool eval_incidence(const ConstraintGraph *graph, int node_id, double cx, double cy, int con_id, const Constraint *con, const SymbolicCoord *candidate) {
    (void)cx; (void)cy; (void)con; return check_incidence_contradiction(graph, node_id, candidate, con_id);
}

static bool eval_betweenness(const ConstraintGraph *graph, int node_id, double cx, double cy, int con_id, const Constraint *con, const SymbolicCoord *candidate) {
    (void)con_id; (void)candidate;
    if (con->participant_count != 3) return false;
    int pa = con->participants[0], pb = con->participants[1], pc = con->participants[2];
    double ax, ay, bx, by, ccx, ccy;
    if (node_id == pb) {
        if (!graph_node_coords(graph, pa, &ax, &ay)) return false;
        if (!graph_node_coords(graph, pc, &ccx, &ccy)) return false;
        bx = cx; by = cy;
    } else if (node_id == pa) {
        if (!graph_node_coords(graph, pb, &bx, &by)) return false;
        if (!graph_node_coords(graph, pc, &ccx, &ccy)) return false;
        ax = cx; ay = cy;
    } else if (node_id == pc) {
        if (!graph_node_coords(graph, pa, &ax, &ay)) return false;
        if (!graph_node_coords(graph, pb, &bx, &by)) return false;
        ccx = cx; ccy = cy;
    } else { return false; }
    return !is_point_between_segment(ax, ay, bx, by, ccx, ccy);
}

static bool eval_intersection(const ConstraintGraph *graph, int node_id, double cx, double cy, int con_id, const Constraint *con, const SymbolicCoord *candidate) {
    (void)con_id; (void)candidate;
    if (con->participant_count != 3) return false;
    if (node_id != con->participants[2]) return false;
    if (!check_point_on_object(graph, cx, cy, con->participants[0])) return true;
    if (!check_point_on_object(graph, cx, cy, con->participants[1])) return true;
    return false;
}

static bool eval_containment(const ConstraintGraph *graph, int node_id, double cx, double cy, int con_id, const Constraint *con, const SymbolicCoord *candidate) {
    (void)con_id; (void)candidate;
    if (con->participant_count != 2) return false;
    if (node_id != con->participants[0]) return false;
    return !check_point_on_object(graph, cx, cy, con->participants[1]);
}

static bool eval_angle(const ConstraintGraph *graph, int node_id, double cx, double cy, int con_id, const Constraint *con, const SymbolicCoord *candidate) {
    (void)con_id; (void)candidate;
    if (con->participant_count != 3) return false;
    int pv = con->participants[0], p1 = con->participants[1], p2 = con->participants[2];
    double vx, vy, x1, y1, x2, y2;
    if (node_id == pv) {
        vx = cx; vy = cy;
        if (!graph_node_coords(graph, p1, &x1, &y1)) return false;
        if (!graph_node_coords(graph, p2, &x2, &y2)) return false;
    } else if (node_id == p1) {
        if (!graph_node_coords(graph, pv, &vx, &vy)) return false;
        x1 = cx; y1 = cy;
        if (!graph_node_coords(graph, p2, &x2, &y2)) return false;
    } else if (node_id == p2) {
        if (!graph_node_coords(graph, pv, &vx, &vy)) return false;
        if (!graph_node_coords(graph, p1, &x1, &y1)) return false;
        x2 = cx; y2 = cy;
    } else { return false; }
    double measured = compute_angle_degrees(vx, vy, x1, y1, x2, y2);
    if (measured < 0.0) return false;
    return fabs(measured - con->numeric_value) > META_PROOF_ANGLE_EPS;
}

static bool eval_default(const ConstraintGraph *graph, int node_id, double cx, double cy, int con_id, const Constraint *con, const SymbolicCoord *candidate) {
    (void)graph; (void)node_id; (void)cx; (void)cy; (void)con_id; (void)con; (void)candidate; return false;
}

static const ConEvalHandler con_eval_table[] = {
    [INCIDENCE]    = eval_incidence,
    [BETWEENNESS]  = eval_betweenness,
    [INTERSECTION] = eval_intersection,
    [CONTAINMENT]  = eval_containment,
    [ANGLE]        = eval_angle,
    [CONNECTION]   = eval_default,
};

/**
 * @brief 约束求值：判断候选坐标是否与指定约束产生矛盾
 *
 * 将候选坐标作为 node_id 的位置代入约束，依据约束的几何语义执行真实求值：
 * - INCIDENCE：候选点必须落在目标对象上
 * - BETWEENNESS：候选（参与点）必须位于另两个参与点之间
 * - INTERSECTION：候选交点必须同时位于两个对象上
 * - CONTAINMENT：候选（内对象）必须位于外对象内
 * - ANGLE：以候选为相关顶点/臂点计算的角度必须等于约束值
 * - CONNECTION：端口数据流约束与坐标无关，不产生几何矛盾
 *
 * @param graph     约束图
 * @param node_id   被检查的节点 ID
 * @param candidate 候选坐标
 * @param con_id    约束 ID
 * @return true 表示候选不满足该约束（矛盾），false 表示满足或无法判定
 */
static bool constraint_eval_contradiction(const ConstraintGraph *graph, int node_id,
                                          const SymbolicCoord *candidate, int con_id) {
    if (!graph || !candidate || node_id < 0)
        return false;

    Constraint *con = NULL;
    for (int i = 0; i < graph->constraint_count; i++) {
        if (graph->constraints[i] && graph->constraints[i]->id == con_id) {
            con = graph->constraints[i];
            break;
        }
    }
    if (!con || !con->is_active)
        return false;

    double cx = symbolic_coord_to_double(candidate);
    double cy = symbolic_coord_to_double(candidate + 1);

    ConEvalHandler h = (con->type >= 0 && (size_t)con->type < sizeof(con_eval_table)/sizeof(con_eval_table[0]))
                       ? con_eval_table[con->type] : NULL;
    return h ? h(graph, node_id, cx, cy, con_id, con, candidate) : eval_default(graph, node_id, cx, cy, con_id, con, candidate);
}

/* ============================================================
 * 内部辅助函数
 * ============================================================ */

/** 创建剪枝记录 */
static PruningRecord *create_pruning_record(void) {
    PruningRecord *record = lv_calloc(1, sizeof(PruningRecord));
    if (!record)
        return NULL;

    lv_darray_init(&record->operations, sizeof(PruningOperation));
    if (!lv_darray_reserve(&record->operations, 64)) {
        lv_free((void **) &record);
        return NULL;
    }

    record->total_states_removed = 0;
    record->total_states_remaining = 0;

    return record;
}

/* PruningOperation 元素：释放 removed_states 数组及其中的 SymbolicCoord、
 * propagation_trace 序列（operations 为值数组，无元素 dtor，逐元素回调） */
static void destroy_pruning_operation_elem(void *elem) {
    PruningOperation *op = (PruningOperation *) elem;
    if (!op)
        return;
    if (op->removed_states) {
        for (int j = 0; j < op->removed_count; j++) {
            if (op->removed_states[j])
                symbolic_coord_destroy(op->removed_states[j]);
        }
        lv_free((void **) &op->removed_states);
    }
    if (op->propagation_trace)
        lv_free((void **) &op->propagation_trace);
}

/* destroy_pruning_record 字段描述表：operations 逐元素销毁后整体释放 */
static const lvFieldDesc s_pruning_record_destroy_fields[] = {
    lv_FIELD_DARRAY_ELEMS(PruningRecord, operations, destroy_pruning_operation_elem),
};

/** 销毁剪枝记录 */
static void destroy_pruning_record(PruningRecord *record) {
    if (!record)
        return;
    lv_obj_destroy_fields(record, s_pruning_record_destroy_fields,
                          sizeof(s_pruning_record_destroy_fields) / sizeof(s_pruning_record_destroy_fields[0]));
    lv_free((void **) &record);
}

/** 添加剪枝操作到记录 */
static bool add_pruning_operation(PruningRecord *record, const PruningOperation *op) {
    if (!record || !op)
        return false;

    if (lv_darray_push(&record->operations, op) < 0)
        return false;
    record->total_states_removed += op->removed_count;

    return true;
}

/* ============================================================
 * 生命周期管理
 * ============================================================ */

MetaProofContext *meta_proof_context_create(ConstraintGraph *graph, PropagationContext *prop_ctx) {
    if (!graph)
        return NULL;

    MetaProofContext *ctx = lv_calloc(1, sizeof(MetaProofContext));
    if (!ctx)
        return NULL;

    ctx->graph = graph;
    ctx->prop_ctx = prop_ctx;

    /* 创建剪枝记录 */
    ctx->record = create_pruning_record();
    if (!ctx->record) {
        lv_free((void **) &ctx);
        return NULL;
    }

    /* 默认配置 */
    ctx->max_propagation_steps = 100;
    ctx->timeout_ms = lv_config_get_int(LV_CFG_META_PROOF_TIMEOUT_MS, 5000); /* 5 秒 */
    ctx->enable_l1 = true;
    ctx->enable_l2 = true;
    ctx->enable_l3 = true;

    return ctx;
}

void meta_proof_context_destroy(MetaProofContext *ctx) {
    if (!ctx)
        return;

    if (ctx->record) {
        destroy_pruning_record(ctx->record);
    }

    lv_free((void **) &ctx);
}

/* ============================================================
 * L1: 直接矛盾证明
 * ============================================================ */

MetaProofResult meta_prove_direct_contradiction(MetaProofContext *ctx, int node_id, const SymbolicCoord *candidate,
                                                int *out_conflicting_constraint) {
    if (!ctx || !candidate || !ctx->graph) {
        return META_PROVE_INCONCLUSIVE;
    }

    if (!ctx->enable_l1) {
        return META_PROVE_INCONCLUSIVE;
    }

    /* 获取节点的所有约束 */
    int constraint_count = constraint_graph_get_constraints_for_node(ctx->graph, node_id, NULL, 0);

    if (constraint_count <= 0) {
        return META_PROVE_INCONCLUSIVE; /* 无约束，无法证明 */
    }

    /* 分配约束 ID 数组 */
    int *constraint_ids = lv_malloc((size_t) constraint_count * sizeof(int));
    if (!constraint_ids) {
        return META_PROVE_INCONCLUSIVE;
    }

    constraint_graph_get_constraints_for_node(ctx->graph, node_id, constraint_ids, constraint_count);

    /* 检查每个约束是否与候选矛盾：对约束执行真实求值 */
    for (int i = 0; i < constraint_count; i++) {
        int cid = constraint_ids[i];

        /* 将候选坐标代入涉及该节点的约束，依据约束的几何语义
         * （关联/之间/相交/包含/角度）执行真实求值并判定矛盾 */
        if (constraint_eval_contradiction(ctx->graph, node_id, candidate, cid)) {
            if (out_conflicting_constraint) {
                *out_conflicting_constraint = cid;
            }
            lv_free((void **) &constraint_ids);
            ctx->l1_proofs++;
            return META_PROVE_VALID;
        }
    }

    lv_free((void **) &constraint_ids);
    return META_PROVE_INCONCLUSIVE;
}

/* ============================================================
 * L2: 传播矛盾证明
 * ============================================================ */

MetaProofResult meta_prove_propagation_contradiction(MetaProofContext *ctx, int node_id,
                                                     const SymbolicCoord *candidate) {
    if (!ctx || !candidate || !ctx->graph) {
        return META_PROVE_INCONCLUSIVE;
    }

    if (!ctx->enable_l2 || !ctx->prop_ctx) {
        return META_PROVE_INCONCLUSIVE;
    }

    /* 临时坍缩节点为候选状态后运行真实约束传播：
     * 传播引擎（AC-3 弧一致性）对图中每个约束执行相容性检查
     * （check_constraint_compatible），当任意节点的状态空间被
     * 排空时即检测到矛盾，返回 PROP_RESULT_CONTRADICTION。 */

    /* 运行传播 */
    PropagationResult result =
        propagation_run_with_assignment(ctx->prop_ctx, node_id, candidate, ctx->max_propagation_steps);

    if (result == PROP_RESULT_CONTRADICTION) {
        ctx->l2_proofs++;
        return META_PROVE_VALID;
    } else if (result == PROP_RESULT_TIMEOUT) {
        return META_PROVE_TIMEOUT;
    }

    return META_PROVE_INCONCLUSIVE;
}

/* ============================================================
 * L3: 代数排除证明
 * ============================================================ */

MetaProofResult meta_prove_algebraic_exclusion(MetaProofContext *ctx, int node_id, const SymbolicCoord *candidate) {
    if (!ctx || !candidate || !ctx->graph) {
        return META_PROVE_INCONCLUSIVE;
    }

    if (!ctx->enable_l3) {
        return META_PROVE_INCONCLUSIVE;
    }

    /* 代数排除：将约束图编码为多项式理想，并以候选坐标构造附加方程
     * { x_v - cx = 0, y_v - cy = 0 } 加入理想，计算 Groebner 基后
     * 判定 1 ∈ J：若理想为整个多项式环，则候选不可能满足全部约束 */
    if (meta_groebner_candidate_excluded(ctx->graph, node_id, candidate)) {
        /* 候选被约束代数系统排除 → 可以被剪枝 */
        ctx->l3_proofs++;
        return META_PROVE_VALID;
    }

    /* 候选与约束系统相容 → 无法通过代数方式排除 */
    return META_PROVE_INCONCLUSIVE;
}

/* ============================================================
 * 自动选择策略证明
 * ============================================================ */

MetaProofResult meta_prove_pruning(MetaProofContext *ctx, int node_id, const SymbolicCoord *candidate) {
    if (!ctx || !candidate) {
        return META_PROVE_INCONCLUSIVE;
    }

    /* 按优先级尝试 L1 → L2 → L3 */

    /* L1: 直接矛盾 */
    if (ctx->enable_l1) {
        MetaProofResult result = meta_prove_direct_contradiction(ctx, node_id, candidate, NULL);
        if (result == META_PROVE_VALID) {
            return result;
        }
    }

    /* L2: 传播矛盾 */
    if (ctx->enable_l2 && ctx->prop_ctx) {
        MetaProofResult result = meta_prove_propagation_contradiction(ctx, node_id, candidate);
        if (result == META_PROVE_VALID) {
            return result;
        }
        if (result == META_PROVE_TIMEOUT) {
            return result;
        }
    }

    /* L3: 代数排除 */
    if (ctx->enable_l3) {
        MetaProofResult result = meta_prove_algebraic_exclusion(ctx, node_id, candidate);
        if (result == META_PROVE_VALID) {
            return result;
        }
    }

    ctx->inconclusive_count++;
    return META_PROVE_INCONCLUSIVE;
}

/* ============================================================
 * 完备性验证
 * ============================================================ */

/* TrustColor → 计数桶索引 静态查找表
 * 0 = proven 计数桶（GREEN / BLUE_UNEXPLORED / BLUE_EXCEEDED）
 * 1 = unproven 计数桶（BLUE_OUT_OF_SCOPE / YELLOW）
 * 越界（如 AMBER 及以上）归入 invalid，与旧 default 行为一致 */
static const int kTrustToCountBucketTable[] = {
    [TRUST_GREEN]             = 0, /* proven 计数桶 */
    [TRUST_BLUE_UNEXPLORED]   = 0, /* proven 计数桶 */
    [TRUST_BLUE_EXCEEDED]     = 0, /* proven 计数桶 */
    [TRUST_BLUE_OUT_OF_SCOPE] = 1, /* unproven 计数桶 */
    [TRUST_YELLOW]            = 1, /* unproven 计数桶 */
};

CompletenessReport *meta_prove_completeness(MetaProofContext *ctx) {
    if (!ctx || !ctx->record) {
        return NULL;
    }

    CompletenessReport *report = lv_calloc(1, sizeof(CompletenessReport));
    if (!report)
        return NULL;

    /* 统计剪枝记录 */
    report->total_prunings = ctx->record->operations.count;
    report->proven_prunings = 0;
    report->unproven_prunings = 0;
    report->invalid_prunings = 0;

    for (int i = 0; i < ctx->record->operations.count; i++) {
        PruningOperation *op = (PruningOperation *)lv_darray_get(&ctx->record->operations, i);

        /* 查询信任颜色 → 计数桶索引：0=proven, 1=unproven；越界视为非法 */
        int bucket = -1;
        if ((unsigned)op->trust < sizeof(kTrustToCountBucketTable) / sizeof(kTrustToCountBucketTable[0]))
            bucket = kTrustToCountBucketTable[op->trust];

        if (bucket == 0) {
            report->proven_prunings++;
        } else if (bucket == 1) {
            report->unproven_prunings++;
        } else {
            report->invalid_prunings++;
        }
    }

    /* 确定总体信任颜色 */
    if (report->invalid_prunings > 0) {
        report->overall_color = 8; /* RED */
    } else if (report->unproven_prunings > 0) {
        report->overall_color = 3; /* YELLOW */
    } else {
        report->overall_color = 0; /* GREEN */
    }

    /* 生成摘要 */
    snprintf(report->summary, sizeof(report->summary), "完备性报告: %d 次剪枝, %d 已证明, %d 未证明, %d 非法",
             report->total_prunings, report->proven_prunings, report->unproven_prunings, report->invalid_prunings);

    return report;
}

void meta_proof_completeness_report_destroy(CompletenessReport *report) {
    lv_free((void **) &report);
}

/* ============================================================
 * 剪枝记录管理
 * ============================================================ */

void meta_proof_record_pruning(MetaProofContext *ctx, int node_id, SymbolicCoord **removed, int count,
                               PruneStrategy strategy, TrustColor trust) {
    if (!ctx || !ctx->record || !removed || count <= 0)
        return;

    PruningOperation op;
    memset(&op, 0, sizeof(op));

    op.node_id = node_id;
    op.strategy = strategy;
    op.trust = trust;
    op.removed_count = count;

    /* 复制被移除的状态 */
    op.removed_states = lv_calloc((size_t) count, sizeof(SymbolicCoord *));
    if (op.removed_states) {
        for (int i = 0; i < count; i++) {
            if (removed[i]) {
                op.removed_states[i] = symbolic_coord_copy(removed[i]);
            }
        }
    }

    add_pruning_operation(ctx->record, &op);
}

const PruningRecord *meta_proof_get_record(const MetaProofContext *ctx) {
    return ctx ? ctx->record : NULL;
}

/* ============================================================
 * 配置
 * ============================================================ */

void meta_proof_set_navigator(MetaProofContext *ctx, ProofNavigator *navigator) {
    if (ctx)
        ctx->navigator = navigator;
}

void meta_proof_set_equiv_manager(MetaProofContext *ctx, EquivClassManager *mgr) {
    if (ctx)
        ctx->equiv_mgr = mgr;
}

void meta_proof_set_stream_context(MetaProofContext *ctx, StreamContext *stream_ctx) {
    if (ctx)
        ctx->stream_ctx = stream_ctx;
}

/* ── strategy_table 文件作用域处理器 ── */
typedef void (*StrategySetter)(MetaProofContext *, bool);
static void set_l1(MetaProofContext *c, bool e) { c->enable_l1 = e; }
static void set_l2(MetaProofContext *c, bool e) { c->enable_l2 = e; }
static void set_l3(MetaProofContext *c, bool e) { c->enable_l3 = e; }
static const StrategySetter strategy_table[] = {
    [PRUNE_DIRECT_CONTRADICTION]      = set_l1,
    [PRUNE_PROPAGATION_CONTRADICTION] = set_l2,
    [PRUNE_ALGEBRAIC_EXCLUSION]       = set_l3,
};

void meta_proof_set_strategy_enabled(MetaProofContext *ctx, PruneStrategy strategy, bool enable) {
    if (!ctx) return;
    LV_DISPATCH_VOID(strategy_table, strategy, ctx, enable);
}

void meta_proof_set_max_propagation_steps(MetaProofContext *ctx, int max_steps) {
    if (ctx && max_steps > 0) {
        ctx->max_propagation_steps = max_steps;
    }
}

void meta_proof_set_timeout(MetaProofContext *ctx, int timeout_ms) {
    if (ctx && timeout_ms > 0) {
        ctx->timeout_ms = timeout_ms;
    }
}

/* ============================================================
 * 诊断与查询
 * ============================================================ */

void meta_proof_get_statistics(const MetaProofContext *ctx, int64_t *out_l1, int64_t *out_l2, int64_t *out_l3,
                               int64_t *out_inconclusive) {
    if (!ctx)
        return;

    if (out_l1)
        *out_l1 = ctx->l1_proofs;
    if (out_l2)
        *out_l2 = ctx->l2_proofs;
    if (out_l3)
        *out_l3 = ctx->l3_proofs;
    if (out_inconclusive)
        *out_inconclusive = ctx->inconclusive_count;
}

/* ============================================================
 * 内部辅助函数实现
 * ============================================================ */

/**
 * @brief 获取与指定节点关联的所有约束 ID
 *
 * 遍历约束图中的所有约束，将包含 node_id 作为参与者的约束 ID
 * 写入 out_ids 数组（最多 max_count 个）。返回符合条件的约束总数。
 * 若 out_ids 为 NULL 或 max_count 为 0，仅返回计数。
 */
static int constraint_graph_get_constraints_for_node(const ConstraintGraph *graph, int node_id, int *out_ids,
                                                     int max_count) {
    if (!graph)
        return 0;
    int count = 0;
    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *c = graph->constraints[i];
        if (!c)
            continue;
        for (int p = 0; p < c->participant_count; p++) {
            if (c->participants[p] == node_id) {
                if (out_ids && count < max_count) {
                    out_ids[count] = c->id;
                }
                count++;
                break;
            }
        }
    }
    return count;
}

/**
 * @brief 获取指定约束 ID 的约束类型
 *
 * 在约束图中查找 ID 为 constraint_id 的约束，返回其类型。
 * 未找到时返回 -1（强制转为 ConstraintType）。
 */
static ConstraintType constraint_graph_get_constraint_type(const ConstraintGraph *graph, int constraint_id) {
    if (!graph)
        return (ConstraintType) (-1);
    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *c = graph->constraints[i];
        if (c && c->id == constraint_id) {
            return c->type;
        }
    }
    return (ConstraintType) (-1);
}

/**
 * @brief 带节点赋值的约束传播
 *
 * 临时将节点的候选坐标设为指定值，然后运行约束传播。
 * 传播完成后恢复原始坐标。返回传播结果。
 *
 * 实现步骤：
 *   1. 保存传播上下文快照
 *   2. 获取目标节点的状态空间
 *   3. 坍缩为仅含候选值
 *   4. 运行 AC-3 约束传播
 *   5. 恢复快照（恢复原始状态空间）
 */
static PropagationResult propagation_run_with_assignment(PropagationContext *ctx, int node_id,
                                                         const SymbolicCoord *coord, int max_steps) {
    if (!ctx)
        return PROP_RESULT_CONTRADICTION;

    /* 保存快照 */
    PropagationSnapshot *snap = propagation_snapshot_save(ctx);
    if (!snap)
        return PROP_RESULT_CONTRADICTION;

    /* 获取节点的状态空间 */
    NodeStateSpace *space = propagation_get_state_space(ctx, node_id);
    if (!space) {
        propagation_snapshot_destroy(snap);
        return PROP_RESULT_CONTRADICTION;
    }

    /* 坍缩节点为候选值 */
    space->is_collapsed = true;
    space->collapsed_value = symbolic_coord_copy(coord);

    /* 清空候选坐标列表（已坍缩，不需要候选集） */
    if (space->candidates_da.data && space->candidates_da.count > 0) {
        CoordCandidate *cand = (CoordCandidate *)space->candidates_da.data;
        for (int i = 0; i < space->candidates_da.count; i++) {
            if (cand[i].coord)
                symbolic_coord_destroy(cand[i].coord);
        }
    }
    lv_darray_free(&space->candidates_da);
    space->is_unbounded = false;

    /* 设置传播步数上限：必须在 propagation_run 之前写入，
     * 否则传播循环内不会采用 max_steps 作为迭代上限 */
    int saved_max_iterations = ctx->max_iterations;
    if (max_steps > 0) {
        ctx->max_iterations = max_steps;
    }

    /* 运行约束传播：
     * 传播引擎对每个约束执行相容性检查（check_constraint_compatible），
     * 当某节点状态空间被排空时即检测到矛盾（PROP_RESULT_CONTRADICTION），
     * 即候选赋值与约束系统矛盾的传播级证据。 */
    PropagationResult result = propagation_run(ctx);

    /* 恢复默认传播步数上限 */
    ctx->max_iterations = saved_max_iterations;

    /* 恢复快照（propagation_snapshot_restore 销毁当前状态和快照） */
    propagation_snapshot_restore(ctx, snap);

    if (result == PROP_RESULT_CONTRADICTION)
        return PROP_RESULT_CONTRADICTION;
    if (result == PROP_RESULT_TIMEOUT)
        return PROP_RESULT_TIMEOUT;

    return PROP_RESULT_CONSISTENT;
}

/**
 * @brief 创建常数多项式（值恒为 coeff）
 *
 * lvPolynomial 结构为公开定义，此处直接构造单项式常数项。
 * 用于判定 1 ∈ J（理想是否为整个多项式环）。
 *
 * @param registry 环注册表
 * @param ring_id  环 ID
 * @param coeff    常数值
 * @param label    多项式标签
 * @return 多项式 ID，失败返回 -1
 */
static int meta_poly_create_constant(lvRingRegistry *registry, int ring_id, double coeff, const char *label) {
    int pid = poly_create(registry, ring_id, 1, label);
    if (pid < 0)
        return -1;
    const lvPolynomial *poly = poly_get(registry, pid);
    if (!poly) {
        poly_destroy(registry, pid);
        return -1;
    }
    lvPolynomial *p = (lvPolynomial *) poly;
    /* 常数项：所有变量指数为 0 */
    for (int v = 0; v < p->var_count; v++)
        p->powers[v] = 0;
    ((double *) p->coeffs)[0] = coeff;
    p->term_count = 1;
    p->total_degree = 0;
    return pid;
}

/**
 * @brief 创建线性多项式 x_var - constant（一个变量项 + 一个常数项）
 *
 * 用于把候选坐标编码为附加方程：x_var - cx = 0、y_var - cy = 0。
 * 采用 RING_FIELD_REAL 域，系数按 double 存储。
 *
 * @param registry 环注册表
 * @param ring_id  环 ID
 * @param var_idx  变量索引
 * @param constant 常数项
 * @param label    多项式标签
 * @return 多项式 ID，失败返回 -1
 */
static int meta_poly_create_linear(lvRingRegistry *registry, int ring_id, int var_idx, double constant,
                                   const char *label) {
    int pid = poly_create(registry, ring_id, 2, label);
    if (pid < 0)
        return -1;
    const lvPolynomial *poly = poly_get(registry, pid);
    if (!poly) {
        poly_destroy(registry, pid);
        return -1;
    }
    lvPolynomial *p = (lvPolynomial *) poly;
    if (var_idx < 0 || var_idx >= p->var_count) {
        poly_destroy(registry, pid);
        return -1;
    }
    /* 项 0：1 * x_var */
    for (int v = 0; v < p->var_count; v++)
        p->powers[v] = 0;
    p->powers[var_idx] = 1;
    /* 项 1：常数项 -constant */
    for (int v = 0; v < p->var_count; v++)
        p->powers[p->term_capacity + v] = 0;
    ((double *) p->coeffs)[0] = 1.0;
    ((double *) p->coeffs)[1] = -constant;
    p->term_count = 2;
    p->total_degree = 1;
    return pid;
}

/* meta_groebner_candidate_excluded 的 defer 守卫：变量名数组（含字符串）与
 * 环注册表（rings 数组 + 外壳）在函数出口统一释放（失败/成功路径一致） */
typedef struct {
    char **var_names;        /* 变量名数组（元素为 lv_strdup 的字符串） */
    int var_count;           /* 数组元素个数 */
    lvRingRegistry *registry; /* 环注册表 */
} MetaGroebnerGuard;

static void meta_groebner_guard_cleanup(void *p) {
    MetaGroebnerGuard *g = (MetaGroebnerGuard *) p;
    if (g->var_names) {
        for (int i = 0; i < g->var_count; i++) {
            if (g->var_names[i])
                lv_free((void **) &g->var_names[i]);
        }
        lv_free((void **) &g->var_names);
    }
    if (g->registry) {
        lv_free((void **) &g->registry->rings);
        lv_free((void **) &g->registry);
    }
}

/**
 * @brief 使用 Groebner 基判定候选坐标是否被约束代数系统排除
 *
 * 实现步骤：
 *   1. 统计 POINT 节点数量，创建多项式环（每点 2 个变量 x、y）
 *   2. 将约束图编码为多项式理想 I（constraint_graph_to_ideal）
 *   3. 以候选坐标构造附加方程 x_v - cx = 0、y_v - cy = 0 加入理想得 J
 *   4. 计算 J 的 Groebner 基（groebner_compute）
 *   5. 判定 1 ∈ J（ideal_membership）：若恒 1 多项式属于理想，则 J 为
 *      整个多项式环，约束系统与候选坐标矛盾 → 候选被代数排除
 *
 * @param graph     约束图
 * @param node_id   被检查的节点 ID（POINT）
 * @param candidate 候选坐标
 * @return true 表示候选被代数排除，false 表示未被排除或计算失败
 */
static bool meta_groebner_candidate_excluded(const ConstraintGraph *graph, int node_id,
                                             const SymbolicCoord *candidate) {
    if (!graph || !candidate || node_id < 0)
        return false;

    /* 统计 POINT 节点数量（每个点 2 个变量：x、y） */
    int point_count = 0;
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (node && node->is_active && node->type == GEOM_POINT)
            point_count++;
    }
    if (point_count == 0)
        return false;

    int var_count = point_count * 2;

    /* 变量名数组与注册表由 lv_DEFER 守卫在函数出口统一释放（失败/成功路径一致），
     * 任一失败分支直接 return false 即可 */
    MetaGroebnerGuard guard = {NULL, 0, NULL};
    lv_DEFER(meta_groebner_guard_cleanup, &guard);
    guard.var_count = var_count;
    guard.registry = ring_registry_create(8);
    if (!guard.registry)
        return false;

    /* 生成变量名 p{id}_x / p{id}_y，变量索引顺序与
     * constraint_graph_to_ideal 的 POINT 节点遍历顺序保持一致 */
    guard.var_names = (char **) lv_calloc((size_t) var_count, sizeof(char *));
    if (!guard.var_names)
        return false;

    int vi = 0;
    int node_vi = -1; /* node_id 的 x 变量索引 */
    for (int i = 0; i < graph->node_count && vi < var_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node || !node->is_active || node->type != GEOM_POINT)
            continue;
        char name[64];
        snprintf(name, sizeof(name), "p%d_x", node->id);
        guard.var_names[vi] = lv_strdup(name);
        if (!guard.var_names[vi])
            return false;
        snprintf(name, sizeof(name), "p%d_y", node->id);
        guard.var_names[vi + 1] = lv_strdup(name);
        if (!guard.var_names[vi + 1])
            return false;
        if (node->id == node_id)
            node_vi = vi;
        vi += 2;
    }
    if (node_vi < 0 || node_vi + 1 >= var_count)
        return false;

    int ring_id = ring_create(guard.registry, (const char **) guard.var_names, var_count, RING_FIELD_REAL,
                              MONOMIAL_GREVLEX, "meta_proof_l3");
    if (ring_id < 0)
        return false;

    /* 将约束图编码为多项式理想 */
    int ideal_id = constraint_graph_to_ideal(guard.registry, graph, ring_id, "meta_proof_constraint_ideal");
    if (ideal_id < 0)
        return false;

    /* 构造候选坐标方程：x_v - cx = 0、y_v - cy = 0 */
    double cx = symbolic_coord_to_double(candidate);
    double cy = symbolic_coord_to_double(candidate + 1);
    int px_id = meta_poly_create_linear(guard.registry, ring_id, node_vi, cx, "candidate_x");
    int py_id = meta_poly_create_linear(guard.registry, ring_id, node_vi + 1, cy, "candidate_y");
    if (px_id < 0 || py_id < 0)
        return false;

    /* 将候选方程加入理想：J = I + <x_v - cx, y_v - cy> */
    if (ideal_add_generator(guard.registry, ideal_id, px_id) != 0)
        return false;
    if (ideal_add_generator(guard.registry, ideal_id, py_id) != 0)
        return false;

    /* 计算 Groebner 基 */
    if (groebner_compute(guard.registry, ideal_id, GROEBNER_AUTO) != 0)
        return false;

    /* 判定 1 ∈ J：若恒 1 多项式属于理想，则 J 为整个多项式环，
     * 约束系统与候选坐标矛盾 → 候选被代数排除 */
    int one_id = meta_poly_create_constant(guard.registry, ring_id, 1.0, "constant_one");
    if (one_id < 0)
        return false;
    int member = ideal_membership(guard.registry, ideal_id, one_id);

    /* 清理本次创建的多项式（理想/环销毁由下述调用完成；
     * 注意 ring_registry_destroy 会清空全局池，故此处不调用它，
     * 避免影响其他环的 Groebner 数据） */
    poly_destroy(guard.registry, one_id);
    poly_destroy(guard.registry, px_id);
    poly_destroy(guard.registry, py_id);
    ideal_destroy(guard.registry, ideal_id);
    ring_destroy(guard.registry, ring_id);
    /* 变量名数组与注册表由守卫在函数出口统一释放 */

    return member == 1;
}
