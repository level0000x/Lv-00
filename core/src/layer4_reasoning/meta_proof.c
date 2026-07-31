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
#include "lv/lv.h"
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
static bool check_all_constraints(const ConstraintGraph *graph, int node_id,
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

    return fabs(cross) > 1e-10; /* 叉积非零 → 不在线上 */
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

/** 销毁剪枝记录 */
static void destroy_pruning_record(PruningRecord *record) {
    if (!record)
        return;

    for (int i = 0; i < record->operations.count; i++) {
        PruningOperation *op = (PruningOperation *)lv_darray_get(&record->operations, i);
        if (op->removed_states) {
            for (int j = 0; j < op->removed_count; j++) {
                if (op->removed_states[j]) {
                    symbolic_coord_destroy(op->removed_states[j]);
                }
            }
            lv_free((void **) &op->removed_states);
        }
        if (op->propagation_trace) {
            lv_free((void **) &op->propagation_trace);
        }
    }

    lv_darray_free(&record->operations);
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

    /* 检查每个约束是否与候选矛盾 */
    for (int i = 0; i < constraint_count; i++) {
        int cid = constraint_ids[i];

        /* 获取约束的代数表达式 */
        /* 简化实现：假设约束有 evaluate 方法 */
        /* 实际实现需要根据约束类型进行代入验证 */

        /* 这里简化为检查约束类型 */
        ConstraintType type = constraint_graph_get_constraint_type(ctx->graph, cid);

        /* 根据约束类型进行验证 */
        bool contradicts = false;

        switch (type) {
            case CONSTRAINT_INCIDENCE:
                /* 关联约束：候选必须在线上 */
                contradicts = check_incidence_contradiction(ctx->graph, node_id, candidate, cid);
                break;

            case CONSTRAINT_BETWEEN:
                /* 之间约束：候选必须在两点之间 */
                /* 简化：假设总是满足 */
                contradicts = false;
                break;

            case CONSTRAINT_DISTANCE:
                /* 距离约束：候选距离必须等于指定值 */
                /* 简化：假设总是满足 */
                contradicts = false;
                break;

            case CONSTRAINT_ANGLE:
                /* 角度约束：角度必须等于指定值 */
                /* 简化：假设总是满足 */
                contradicts = false;
                break;

            default:
                /* 未知约束类型 */
                contradicts = false;
                break;
        }

        if (contradicts) {
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

    /* 临时坍缩节点为候选状态 */
    /* 简化实现：假设传播引擎会检测矛盾 */

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

    /* 检查候选是否满足图中所有涉及该节点的约束 */
    if (!check_all_constraints(ctx->graph, node_id, candidate)) {
        /* 候选不满足某约束 → 可以被排除 */
        ctx->l3_proofs++;
        return META_PROVE_VALID;
    }

    /* 所有约束满足 → 无法通过代数方式排除 */
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

        switch (op->trust) {
            case 0: /* GREEN */
                report->proven_prunings++;
                break;
            case 1: /* BLUE */
            case 2: /* BLUE_RANGE */
                report->proven_prunings++;
                break;
            case 3: /* YELLOW */
            case 4: /* AMBER */
                report->unproven_prunings++;
                break;
            default:
                report->invalid_prunings++;
                break;
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

void meta_proof_set_strategy_enabled(MetaProofContext *ctx, PruneStrategy strategy, bool enable) {
    if (!ctx)
        return;

    switch (strategy) {
        case PRUNE_DIRECT_CONTRADICTION:
            ctx->enable_l1 = enable;
            break;
        case PRUNE_PROPAGATION_CONTRADICTION:
            ctx->enable_l2 = enable;
            break;
        case PRUNE_ALGEBRAIC_EXCLUSION:
            ctx->enable_l3 = enable;
            break;
    }
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

    /* 运行约束传播 */
    PropagationResult result = propagation_run(ctx);

    /* 恢复快照（propagation_snapshot_restore 销毁当前状态和快照） */
    propagation_snapshot_restore(ctx, snap);

    /* 超时检测：max_steps 表示传播的最大迭代次数
     * propagation_run 内部循环使用 ctx->max_iterations 作为上限，
     * 当 max_steps > 0 时覆盖默认值以支持步数限制。 */
    if (max_steps > 0) {
        ctx->max_iterations = max_steps;
    }

    if (result == PROP_RESULT_CONTRADICTION)
        return PROP_RESULT_CONTRADICTION;
    if (result == PROP_RESULT_TIMEOUT)
        return PROP_RESULT_TIMEOUT;

    return PROP_RESULT_CONSISTENT;
}

/**
 * @brief 检查候选坐标是否满足图中所有约束
 *
 * 遍历图中所有涉及指定节点的约束，逐一检查候选坐标是否满足。
 * 这是 L3 代数排除的简化实现（直接约束检查而非 Groebner 基）。
 *
 * @param graph     约束图
 * @param node_id   节点 ID
 * @param candidate 候选坐标
 * @return true 表示所有约束满足（候选可能是合法解），
 *         false 表示至少一个约束不满足（候选可被排除）
 */
static bool check_all_constraints(const ConstraintGraph *graph, int node_id,
                                   const SymbolicCoord *candidate) {
    if (!graph || !candidate)
        return true;

    for (int ci = 0; ci < graph->constraint_count; ci++) {
        Constraint *con = graph->constraints[ci];
        if (!con || !con->is_active)
            continue;

        /* 检查该约束是否涉及本节点 */
        bool involves_node = false;
        for (int p = 0; p < con->participant_count; p++) {
            if (con->participants[p] == node_id) {
                involves_node = true;
                break;
            }
        }
        if (!involves_node)
            continue;

        switch (con->type) {
            case INCIDENCE: {
                /* INCIDENCE(point, seg): 叉积 = 0 → 点在线上 */
                int seg_id = con->participants[0] == node_id ? con->participants[1] : con->participants[0];

                /* 查找线段端点 */
                int ep1 = -1, ep2 = -1;
                for (int i = 0; i < graph->constraint_count; i++) {
                    Constraint *ic = graph->constraints[i];
                    if (!ic || ic->id == con->id || ic->type != INCIDENCE)
                        continue;
                    if (ic->participants[1] == seg_id) {
                        int other = ic->participants[0];
                        if (other == node_id) continue;
                        if (ep1 < 0) ep1 = other;
                        else if (ep2 < 0) ep2 = other;
                    }
                }
                if (ep1 < 0 || ep2 < 0) continue;

                /* 获取端点坐标 */
                GeomNode *n1 = NULL, *n2 = NULL;
                for (int j = 0; j < graph->node_count; j++) {
                    GeomNode *n = graph->nodes[j];
                    if (!n) continue;
                    if (n->id == ep1) n1 = n;
                    if (n->id == ep2) n2 = n;
                }
                if (!n1 || !n2 || n1->coord_count < 2 || n2->coord_count < 2)
                    continue;

                double x1 = symbolic_coord_to_double(n1->symbolic_coords[0]);
                double y1 = symbolic_coord_to_double(n1->symbolic_coords[1]);
                double x2 = symbolic_coord_to_double(n2->symbolic_coords[0]);
                double y2 = symbolic_coord_to_double(n2->symbolic_coords[1]);
                double xp = symbolic_coord_to_double(candidate);
                double yp = (n1->coord_count >= 2) ? symbolic_coord_to_double(candidate + 1) : 0.0;

                double cross = (x2 - x1) * (yp - y1) - (y2 - y1) * (xp - x1);
                if (fabs(cross) > 1e-10)
                    return false; /* 不在线上 → 约束不满足 */
                break;
            }

            default:
                break;
        }
    }
    return true;
}
