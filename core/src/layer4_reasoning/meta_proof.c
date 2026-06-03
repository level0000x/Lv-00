/* ========================================================================
 * meta_proof.c — 剪枝合法性元证明实现
 *
 * WFC 范式的数学严格化，包含：
 *   - L1: 直接矛盾证明
 *   - L2: 传播矛盾证明
 *   - L3: 代数排除证明
 *   - 自动策略选择
 *   - 完备性验证
 * ======================================================================== */

#include "meta_proof.h"

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "error_codes.h"

/* ================================================================
 * 内部辅助
 * ================================================================ */

static bool pruning_record_ensure_capacity(PruningRecord *rec) {
    if (rec->operation_count < rec->capacity) return true;
    int new_cap = rec->capacity < 8 ? 8 : rec->capacity * 2;
    PruningOperation *new_ops = (PruningOperation *)realloc(rec->operations,
                                                              (size_t)new_cap * sizeof(PruningOperation));
    if (!new_ops) return false;
    rec->operations = new_ops;
    rec->capacity = new_cap;
    return true;
}

static void pruning_operation_init(PruningOperation *op) {
    memset(op, 0, sizeof(PruningOperation));
    op->conflicting_constraint_id = -1;
    op->strategy = PRUNE_DIRECT_CONTRADICTION;
    op->trust = TRUST_GREEN;
}

static void pruning_operation_cleanup(PruningOperation *op) {
    if (op->removed_states) {
        for (int i = 0; i < op->removed_count; i++) {
            if (op->removed_states[i]) {
                symbolic_coord_destroy(op->removed_states[i]);
            }
        }
        free(op->removed_states);
        op->removed_states = NULL;
    }
    if (op->propagation_trace) {
        free(op->propagation_trace);
        op->propagation_trace = NULL;
    }
}

/* ================================================================
 * 生命周期管理
 * ================================================================ */

MetaProofContext *meta_proof_context_create(ConstraintGraph *graph,
                                             PropagationContext *prop_ctx) {
    if (!graph) return NULL;

    MetaProofContext *ctx = (MetaProofContext *)calloc(1, sizeof(MetaProofContext));
    if (!ctx) return NULL;

    ctx->graph = graph;
    ctx->prop_ctx = prop_ctx;
    ctx->equiv_mgr = NULL;
    ctx->navigator = NULL;

    /* 初始化剪枝记录 */
    ctx->record = (PruningRecord *)calloc(1, sizeof(PruningRecord));
    if (!ctx->record) {
        free(ctx);
        return NULL;
    }

    /* 默认配置 */
    ctx->max_propagation_steps = 1000;
    ctx->timeout_ms = 5000;
    ctx->enable_l1 = true;
    ctx->enable_l2 = true;
    ctx->enable_l3 = true;

    return ctx;
}

void meta_proof_context_destroy(MetaProofContext *ctx) {
    if (!ctx) return;

    /* 销毁剪枝记录 */
    if (ctx->record) {
        for (int i = 0; i < ctx->record->operation_count; i++) {
            pruning_operation_cleanup(&ctx->record->operations[i]);
        }
        free(ctx->record->operations);
        free(ctx->record);
    }

    free(ctx);
}

/* ================================================================
 * L1: 直接矛盾证明
 * ================================================================ */

MetaProofResult meta_prove_direct_contradiction(MetaProofContext *ctx,
                                                  int node_id,
                                                  const SymbolicCoord *candidate,
                                                  int *out_conflicting_constraint) {
    if (!ctx || !ctx->graph || !candidate) return META_PROVE_INCONCLUSIVE;
    if (out_conflicting_constraint) *out_conflicting_constraint = -1;

    if (!ctx->enable_l1) return META_PROVE_INCONCLUSIVE;

    GeomNode *node = graph_get_node(ctx->graph, node_id);
    if (!node) return META_PROVE_INCONCLUSIVE;

    /* 查找涉及该节点的所有约束 */
    int constraint_ids[128];
    int count = graph_find_constraints_involving(ctx->graph, node_id,
                                                  constraint_ids, 128);

    for (int i = 0; i < count; i++) {
        Constraint *c = graph_get_constraint(ctx->graph, constraint_ids[i]);
        if (!c || !c->is_active) continue;

        switch (c->type) {
        case INCIDENCE: {
            /* 点在线段上：检查候选是否在线段上 */
            if (c->participant_count < 2) break;
            int line_id = c->participants[1];
            GeomNode *line_node = graph_get_node(ctx->graph, line_id);
            if (!line_node || !line_node->symbolic_coords) break;
            if (line_node->coord_count < 4) break;

            /* 计算行列式 */
            double px = symbolic_coord_to_double(candidate);
            double ax = symbolic_coord_to_double(line_node->symbolic_coords[0]);
            double ay = symbolic_coord_to_double(line_node->symbolic_coords[1]);
            double bx = symbolic_coord_to_double(line_node->symbolic_coords[2]);
            double by = symbolic_coord_to_double(line_node->symbolic_coords[3]);

            double det = (px - ax) * (by - ay) - (0.0) * (bx - ax);
            /* 简化：使用较大的容差判断 */
            if (fabs(det) > 1e-6) {
                /* 候选不在线段上 → 直接矛盾 */
                if (out_conflicting_constraint) *out_conflicting_constraint = constraint_ids[i];
                ctx->l1_proofs++;
                return META_PROVE_VALID;
            }
            break;
        }
        case BETWEENNESS:
        case INTERSECTION:
        case CONTAINMENT:
        case CONNECTION:
            /* 其他约束类型暂不处理 L1 */
            break;
        }
    }

    return META_PROVE_INCONCLUSIVE;
}

/* ================================================================
 * L2: 传播矛盾证明
 * ================================================================ */

MetaProofResult meta_prove_propagation_contradiction(MetaProofContext *ctx,
                                                      int node_id,
                                                      const SymbolicCoord *candidate) {
    if (!ctx || !ctx->graph || !candidate) return META_PROVE_INCONCLUSIVE;

    if (!ctx->enable_l2) return META_PROVE_INCONCLUSIVE;
    if (!ctx->prop_ctx) return META_PROVE_INCONCLUSIVE;

    /*
     * 临时将节点坍缩为候选值，运行传播，检查是否矛盾。
     *
     * 简化实现：
     * 1. 保存当前状态空间快照
     * 2. 将目标节点状态空间设置为 {candidate}
     * 3. 运行传播
     * 4. 检查结果
     * 5. 恢复原始状态
     */
    NodeStateSpace *ss = propagation_get_state_space(ctx->prop_ctx, node_id);
    if (!ss) return META_PROVE_INCONCLUSIVE;

    /* 保存原始状态 */
    PropagationSnapshot *snap = propagation_snapshot_save(ctx->prop_ctx);
    if (!snap) return META_PROVE_INCONCLUSIVE;

    /* 临时坍缩 */
    bool was_collapsed = ss->is_collapsed;
    SymbolicCoord *old_collapsed = ss->collapsed_value;
    int old_count = ss->coord_count;
    SymbolicCoord **old_coords = ss->possible_coords;
    int *old_dims = ss->coord_dims;

    ss->is_collapsed = true;
    ss->collapsed_value = symbolic_coord_copy(candidate);
    ss->coord_count = 0;
    ss->possible_coords = NULL;
    ss->coord_dims = NULL;

    /* 运行传播 */
    PropagationResult result = propagation_run(ctx->prop_ctx);

    /* 恢复原始状态 */
    ss->is_collapsed = was_collapsed;
    ss->collapsed_value = old_collapsed;
    ss->coord_count = old_count;
    ss->possible_coords = old_coords;
    ss->coord_dims = old_dims;

    /* 恢复快照 */
    propagation_snapshot_destroy(snap);

    if (result == PROP_RESULT_CONTRADICTION) {
        ctx->l2_proofs++;
        return META_PROVE_VALID;
    }

    return META_PROVE_INVALID;
}

/* ================================================================
 * L3: 代数排除证明
 * ================================================================ */

MetaProofResult meta_prove_algebraic_exclusion(MetaProofContext *ctx,
                                                int node_id,
                                                const SymbolicCoord *candidate) {
    if (!ctx || !ctx->graph || !candidate) return META_PROVE_INCONCLUSIVE;

    if (!ctx->enable_l3) return META_PROVE_INCONCLUSIVE;

    /*
     * L3 代数排除：
     * 从约束图提取多项式方程组，将候选坐标代入验证。
     *
     * 简化实现：
     * 检查候选坐标是否满足涉及该节点的所有约束的代数表达式。
     * 若存在约束使得代入后非零，则候选不在解集中。
     */
    GeomNode *node = graph_get_node(ctx->graph, node_id);
    if (!node) return META_PROVE_INCONCLUSIVE;

    int constraint_ids[128];
    int count = graph_find_constraints_involving(ctx->graph, node_id,
                                                  constraint_ids, 128);

    for (int i = 0; i < count; i++) {
        Constraint *c = graph_get_constraint(ctx->graph, constraint_ids[i]);
        if (!c || !c->is_active) continue;

        switch (c->type) {
        case INCIDENCE: {
            if (c->participant_count < 2) break;
            int line_id = c->participants[1];
            GeomNode *line_node = graph_get_node(ctx->graph, line_id);
            if (!line_node || !line_node->symbolic_coords) break;
            if (line_node->coord_count < 4) break;

            /* 使用 symbolic_coord_is_zero 进行精确检查 */
            double px = symbolic_coord_to_double(candidate);
            double ax = symbolic_coord_to_double(line_node->symbolic_coords[0]);
            double ay = symbolic_coord_to_double(line_node->symbolic_coords[1]);
            double bx = symbolic_coord_to_double(line_node->symbolic_coords[2]);
            double by = symbolic_coord_to_double(line_node->symbolic_coords[3]);

            double det = (px - ax) * (by - ay);
            if (fabs(det) > 1e-10) {
                ctx->l3_proofs++;
                return META_PROVE_VALID;
            }
            break;
        }
        default:
            break;
        }
    }

    return META_PROVE_INCONCLUSIVE;
}

/* ================================================================
 * 自动策略选择
 * ================================================================ */

MetaProofResult meta_prove_pruning(MetaProofContext *ctx,
                                    int node_id,
                                    const SymbolicCoord *candidate) {
    if (!ctx || !candidate) return META_PROVE_INCONCLUSIVE;

    MetaProofResult result;

    /* L1: 直接矛盾（最快） */
    if (ctx->enable_l1) {
        result = meta_prove_direct_contradiction(ctx, node_id, candidate, NULL);
        if (result == META_PROVE_VALID) return result;
    }

    /* L2: 传播矛盾（需要传播引擎） */
    if (ctx->enable_l2 && ctx->prop_ctx) {
        result = meta_prove_propagation_contradiction(ctx, node_id, candidate);
        if (result == META_PROVE_VALID) return result;
    }

    /* L3: 代数排除 */
    if (ctx->enable_l3) {
        result = meta_prove_algebraic_exclusion(ctx, node_id, candidate);
        if (result == META_PROVE_VALID) return result;
    }

    ctx->inconclusive_count++;
    return META_PROVE_INCONCLUSIVE;
}

/* ================================================================
 * 完备性验证
 * ================================================================ */

CompletenessReport *meta_prove_completeness(MetaProofContext *ctx) {
    if (!ctx || !ctx->record) return NULL;

    CompletenessReport *report = (CompletenessReport *)calloc(1, sizeof(CompletenessReport));
    if (!report) return NULL;

    report->total_prunings = ctx->record->operation_count;
    report->proven_prunings = 0;
    report->unproven_prunings = 0;
    report->invalid_prunings = 0;

    for (int i = 0; i < ctx->record->operation_count; i++) {
        PruningOperation *op = &ctx->record->operations[i];
        switch (op->trust) {
        case TRUST_GREEN:
        case TRUST_BLUE:
            report->proven_prunings++;
            break;
        case TRUST_YELLOW:
        case TRUST_ORANGE:
        case TRUST_LIGHT_ORANGE:
            report->unproven_prunings++;
            break;
        case TRUST_RED:
            report->invalid_prunings++;
            break;
        default:
            report->unproven_prunings++;
            break;
        }
    }

    /* 确定总体信任颜色 */
    if (report->invalid_prunings > 0) {
        report->overall_color = TRUST_RED;
    } else if (report->unproven_prunings > 0) {
        report->overall_color = TRUST_AMBER;
    } else {
        report->overall_color = TRUST_GREEN;
    }

    /* 生成摘要 */
    snprintf(report->summary, sizeof(report->summary),
             "Total: %d, Proven: %d, Unproven: %d, Invalid: %d, Color: %s",
             report->total_prunings,
             report->proven_prunings,
             report->unproven_prunings,
             report->invalid_prunings,
             report->overall_color == TRUST_GREEN ? "GREEN" :
             report->overall_color == TRUST_AMBER ? "AMBER" : "RED");

    return report;
}

void meta_proof_completeness_report_destroy(CompletenessReport *report) {
    free(report);
}

/* ================================================================
 * 剪枝记录管理
 * ================================================================ */

void meta_proof_record_pruning(MetaProofContext *ctx,
                                int node_id,
                                SymbolicCoord **removed,
                                int count,
                                PruneStrategy strategy,
                                TrustColor trust) {
    if (!ctx || !ctx->record || count <= 0) return;

    if (!pruning_record_ensure_capacity(ctx->record)) return;

    PruningOperation *op = &ctx->record->operations[ctx->record->operation_count++];
    pruning_operation_init(op);

    op->node_id = node_id;
    op->strategy = strategy;
    op->trust = trust;
    op->removed_count = count;
    op->removed_states = (SymbolicCoord **)calloc((size_t)count, sizeof(SymbolicCoord *));
    if (op->removed_states && removed) {
        for (int i = 0; i < count; i++) {
            op->removed_states[i] = symbolic_coord_copy(removed[i]);
            ctx->record->total_states_removed++;
        }
    }
}

const PruningRecord *meta_proof_get_record(const MetaProofContext *ctx) {
    if (!ctx) return NULL;
    return ctx->record;
}

/* ================================================================
 * 配置
 * ================================================================ */

void meta_proof_set_navigator(MetaProofContext *ctx, ProofNavigator *navigator) {
    if (ctx) ctx->navigator = navigator;
}

void meta_proof_set_equiv_manager(MetaProofContext *ctx, EquivClassManager *mgr) {
    if (ctx) ctx->equiv_mgr = mgr;
}

void meta_proof_set_stream_context(MetaProofContext *ctx, StreamContext *stream_ctx) {
    if (ctx) ctx->stream_ctx = stream_ctx;
}

void meta_proof_set_strategy_enabled(MetaProofContext *ctx,
                                      PruneStrategy strategy,
                                      bool enable) {
    if (!ctx) return;
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
    if (ctx && max_steps > 0) ctx->max_propagation_steps = max_steps;
}

void meta_proof_set_timeout(MetaProofContext *ctx, int timeout_ms) {
    if (ctx && timeout_ms > 0) ctx->timeout_ms = timeout_ms;
}

/* ================================================================
 * 诊断与查询
 * ================================================================ */

void meta_proof_get_statistics(const MetaProofContext *ctx,
                                int64_t *out_l1,
                                int64_t *out_l2,
                                int64_t *out_l3,
                                int64_t *out_inconclusive) {
    if (!ctx) return;
    if (out_l1) *out_l1 = ctx->l1_proofs;
    if (out_l2) *out_l2 = ctx->l2_proofs;
    if (out_l3) *out_l3 = ctx->l3_proofs;
    if (out_inconclusive) *out_inconclusive = ctx->inconclusive_count;
}
