/**
 * @file solver_order.c
 * @brief 变量消元顺序（基于约束图拓扑排序）
 *
 * @details 从 solver.c 拆分出的子模块（Lv-00 项目 v3.3.0+）。
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "solver_common.h"

/* 前向声明 */


/* ================================================================== */
/*  内部: 基于约束图拓扑排序的变量消元顺序                               */
/* ================================================================== */

/* 共享 Kahn 拓扑排序（按优先级逐轮选择）的优先级回调：
 * 返回节点 idx 的排序优先级（越大越先选）；返回 -1 表示该节点本轮不可选
 * （由子集/窗口限制等调用方规则决定）。 */
typedef int (*kahn_priority_fn)(void *ctx, int idx, int order_count);

/* 共享 Kahn 主循环：入度统计 + 逐轮按优先级选择 + 环回退（剩余未访问节点
 * 全部输出）。邻接矩阵 adj 由调用方按各自规则构建；var_ids 为节点 id
 * （优先级平局时按 id 小者优先，与原实现一致）。成功返回 calloc 的 order
 * 数组并通过 *out_count 输出长度；失败返回 NULL。 */
static int *kahn_priority_order(const int *var_ids, int var_count,
                                const bool *const *adj,
                                kahn_priority_fn priority_cb, void *ctx,
                                const char *log_prefix, int *out_count) {
    *out_count = 0;
    if (!var_ids || var_count <= 0 || !adj || !priority_cb || !log_prefix)
        lv_RETURN_ERROR_NULL(lv_ERROR_INVALID_PARAM, "kahn_priority_order: invalid param");

    int *in_degree = lv_calloc((size_t) var_count, sizeof(int));
    bool *visited = lv_calloc((size_t) var_count, sizeof(bool));
    int *order = lv_calloc((size_t) var_count, sizeof(int));
    if (!in_degree || !visited || !order) {
        lv_free((void **) &in_degree);
        lv_free((void **) &visited);
        lv_free((void **) &order);
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "kahn_priority_order: allocation failed");
    }

    for (int i = 0; i < var_count; i++) {
        for (int j = 0; j < var_count; j++) {
            if (adj[i][j])
                in_degree[j]++;
        }
    }

    int order_count = 0;
    while (order_count < var_count) {
        int best = -1;
        int best_priority = -1;

        for (int i = 0; i < var_count; i++) {
            if (visited[i])
                continue;
            if (in_degree[i] > 0)
                continue;
            int prio = priority_cb(ctx, i, order_count);
            if (prio < 0)
                continue;
            if (prio > best_priority ||
                (prio == best_priority && var_ids[i] < var_ids[best])) {
                best = i;
                best_priority = prio;
            }
        }

        if (best < 0) {
            int unvisited = 0;
            for (int i = 0; i < var_count; i++) {
                if (!visited[i])
                    unvisited++;
            }
            lv_log(lv_LOG_WARN,
                   "%s: cycle detected in constraint graph, "
                   "%d variables remain unscheduled\n",
                   log_prefix, unvisited);
            for (int i = 0; i < var_count; i++) {
                if (!visited[i]) {
                    order[order_count++] = var_ids[i];
                    visited[i] = true;
                }
            }
            break;
        }

        order[order_count++] = var_ids[best];
        visited[best] = true;

        for (int j = 0; j < var_count; j++) {
            if (adj[best][j]) {
                in_degree[j]--;
            }
        }
    }

    lv_free((void **) &in_degree);
    lv_free((void **) &visited);

    *out_count = order_count;
    return order;
}

/* order_variables_by_dependency 的优先级上下文：participation 为主键，in_subset
 * 限定窗口（前 subset_limit 个位置仅允许子集内节点，即 dirty 变量）。 */
typedef struct {
    const int *participation;
    const bool *in_subset;
    int subset_limit;
} DependencyOrderCtx;

static int dependency_order_priority(void *ctx, int idx, int order_count) {
    DependencyOrderCtx *c = (DependencyOrderCtx *) ctx;
    if (!c->in_subset[idx] && order_count < c->subset_limit)
        return -1;
    return c->participation[idx];
}

int *order_variables_by_dependency(const ConstraintGraph *graph, const int *var_ids, int var_count,
                                          const int *dirty_var_ids, int dirty_count, int *out_count) {
    *out_count = 0;
    if (!graph || var_count == 0)
        lv_RETURN_ERROR_NULL(lv_ERROR_INVALID_PARAM, "order_variables_by_dependency: NULL graph or empty var_count");

    lvHashtable *id_to_idx = lv_hashtable_int_create((size_t) var_count);
    if (!id_to_idx)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "order_variables_by_dependency: lv_hashtable_int_create failed (count=%d)", var_count);
    for (int i = 0; i < var_count; i++) {
        if (!lv_hashtable_int_contains(id_to_idx, var_ids[i]))
            lv_hashtable_int_insert(id_to_idx, var_ids[i], (void *) (intptr_t) (i + 1));
    }

    bool **adj = lv_calloc((size_t) var_count, sizeof(bool *));
    for (int i = 0; i < var_count; i++) {
        adj[i] = lv_calloc((size_t) var_count, sizeof(bool));
    }

    int *participation = lv_calloc((size_t) var_count, sizeof(int));
    for (int ci = 0; ci < graph->constraint_count; ci++) {
        const Constraint *c = graph->constraints[ci];
        for (int p = 0; p < c->participant_count; p++) {
            void *v = lv_hashtable_int_get(id_to_idx, c->participants[p]);
            if (v)
                participation[(int) (intptr_t) v - 1]++;
        }
    }

    for (int ci = 0; ci < graph->constraint_count; ci++) {
        const Constraint *c = graph->constraints[ci];
        int p_indices[32];
        int p_count = 0;
        for (int p = 0; p < c->participant_count && p < 32; p++) {
            void *v = lv_hashtable_int_get(id_to_idx, c->participants[p]);
            if (v)
                p_indices[p_count++] = (int) (intptr_t) v - 1;
        }
        for (int a = 0; a < p_count; a++) {
            for (int b = a + 1; b < p_count; b++) {
                int ia = p_indices[a], ib = p_indices[b];
                if (participation[ia] >= participation[ib]) {
                    adj[ia][ib] = true;
                } else {
                    adj[ib][ia] = true;
                }
            }
        }
    }

    bool *in_subset = lv_calloc((size_t) var_count, sizeof(bool));
    bool use_subset = (dirty_var_ids != NULL && dirty_count > 0);
    if (use_subset) {
        for (int i = 0; i < dirty_count; i++) {
            void *v = lv_hashtable_int_get(id_to_idx, dirty_var_ids[i]);
            if (v)
                in_subset[(int) (intptr_t) v - 1] = true;
        }
    } else {
        for (int i = 0; i < var_count; i++)
            in_subset[i] = true;
    }

    /* 共享 Kahn 主循环：入度统计、逐轮按 participation（子集窗口限制）选择、
     * 环回退输出剩余变量 */
    DependencyOrderCtx dep_ctx;
    dep_ctx.participation = participation;
    dep_ctx.in_subset = in_subset;
    dep_ctx.subset_limit = var_count - (use_subset ? (var_count - dirty_count) : 0);

    int order_count = 0;
    int *order = kahn_priority_order(var_ids, var_count, adj,
                                     dependency_order_priority, &dep_ctx,
                                     "order_variables_by_dependency", &order_count);
    if (!order) {
        for (int i = 0; i < var_count; i++)
            lv_free((void **) &adj[i]);
        lv_free((void **) &adj);
        lv_free((void **) &participation);
        lv_free((void **) &in_subset);
        lv_hashtable_int_destroy(id_to_idx);
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "order_variables_by_dependency: kahn_priority_order failed (count=%d)", var_count);
    }

    for (int i = 0; i < var_count; i++)
        lv_free((void **) &adj[i]);
    lv_free((void **) &adj);
    lv_free((void **) &participation);
    lv_free((void **) &in_subset);
    lv_hashtable_int_destroy(id_to_idx);

    *out_count = order_count;
    return order;
}

/* compute_elimination_order 的优先级上下文：weight 越大越先消元 */
typedef struct {
    const int *weight;
} EliminationOrderCtx;

static int elimination_order_priority(void *ctx, int idx, int order_count) {
    (void) order_count;
    return ((EliminationOrderCtx *) ctx)->weight[idx];
}

static int *compute_elimination_order(const ConstraintGraph *graph, EquationSystem *sys, int *out_order_count) {
    *out_order_count = 0;

    if (!graph || !sys)
        lv_RETURN_ERROR_NULL(lv_ERROR_INVALID_PARAM, "compute_elimination_order: NULL graph or sys");

    PolyEquation *const eqs = (PolyEquation *)sys->eqs.data;
    const int eq_count_total = sys->eqs.count;

    int *var_ids = lv_calloc((size_t) eq_count_total, sizeof(int));
    if (!var_ids)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "compute_elimination_order: lv_calloc for var_ids failed (count=%d)", eq_count_total);
    int var_count = 0;

    for (int i = 0; i < eq_count_total; i++) {
        if (eqs[i].poly.degree < 0)
            continue;
        int vid = eqs[i].var_node_id;
        bool found = false;
        for (int j = 0; j < var_count; j++) {
            if (var_ids[j] == vid) {
                found = true;
                break;
            }
        }
        if (!found)
            var_ids[var_count++] = vid;
    }

    if (var_count == 0) {
        lv_free((void **) &var_ids);
        lv_RETURN_ERROR_NULL(lv_ERROR_NOT_FOUND, "compute_elimination_order: no valid variables in equation system");
    }

    int *eq_count = lv_calloc((size_t) var_count, sizeof(int));
    for (int i = 0; i < eq_count_total; i++) {
        if (eqs[i].poly.degree < 0)
            continue;
        for (int j = 0; j < var_count; j++) {
            if (var_ids[j] == eqs[i].var_node_id) {
                eq_count[j]++;
                break;
            }
        }
    }

    int *constraint_count = lv_calloc((size_t) var_count, sizeof(int));
    for (int ci = 0; ci < graph->constraint_count; ci++) {
        Constraint *c = graph->constraints[ci];
        for (int p = 0; p < c->participant_count; p++) {
            int pid = c->participants[p];
            for (int j = 0; j < var_count; j++) {
                if (var_ids[j] == pid) {
                    constraint_count[j]++;
                    break;
                }
            }
        }
    }

    int *weight = lv_calloc((size_t) var_count, sizeof(int));
    if (!weight) {
        lv_free((void **) &var_ids);
        lv_free((void **) &eq_count);
        lv_free((void **) &constraint_count);
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "compute_elimination_order: lv_calloc for weight failed (count=%d)", var_count);
    }
    for (int i = 0; i < var_count; i++) {
        weight[i] = eq_count[i] * 2 + constraint_count[i];
    }

    bool **adj = lv_calloc((size_t) var_count, sizeof(bool *));
    for (int i = 0; i < var_count; i++) {
        adj[i] = lv_calloc((size_t) var_count, sizeof(bool));
    }

    for (int ci = 0; ci < graph->constraint_count; ci++) {
        Constraint *c = graph->constraints[ci];
        int *participants = lv_calloc((size_t) c->participant_count, sizeof(int));
        if (!participants)
            continue;
        int p_var_count = 0;
        for (int p = 0; p < c->participant_count; p++) {
            int pid = c->participants[p];
            for (int j = 0; j < var_count; j++) {
                if (var_ids[j] == pid) {
                    participants[p_var_count++] = j;
                    break;
                }
            }
        }
        for (int a = 0; a < p_var_count; a++) {
            for (int b = a + 1; b < p_var_count; b++) {
                adj[participants[a]][participants[b]] = true;
                adj[participants[b]][participants[a]] = true;
            }
        }
        lv_free((void **) &participants);
    }

    /* 共享 Kahn 主循环：入度统计、逐轮按 weight 选择、环回退输出剩余变量 */
    EliminationOrderCtx elim_ctx;
    elim_ctx.weight = weight;

    int order_count = 0;
    int *order = kahn_priority_order(var_ids, var_count, adj,
                                     elimination_order_priority, &elim_ctx,
                                     "compute_elimination_order", &order_count);
    if (!order) {
        for (int i = 0; i < var_count; i++)
            lv_free((void **) &adj[i]);
        lv_free((void **) &adj);
        lv_free((void **) &eq_count);
        lv_free((void **) &constraint_count);
        lv_free((void **) &weight);
        lv_free((void **) &var_ids);
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "compute_elimination_order: kahn_priority_order failed (count=%d)", var_count);
    }

    for (int i = 0; i < var_count; i++) {
        lv_free((void **) &adj[i]);
    }
    lv_free((void **) &adj);
    lv_free((void **) &eq_count);
    lv_free((void **) &constraint_count);
    lv_free((void **) &weight);
    lv_free((void **) &var_ids);

    *out_order_count = order_count;
    return order;
}
