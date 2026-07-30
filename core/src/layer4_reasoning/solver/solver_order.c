/**
 * @file solver_order.c
 * @brief 变量消元顺序（基于约束图拓扑排序）
 *
 * @details 从 solver.c 拆分出的子模块（Lv-00 项目 v3.3.0+）。
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/constraint_graph.h"
#include "lv/solver.h"
#include "lv/stream.h"
#include "lv/solver_types.h"

#include "debug.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "mpz_poly.h"

/* 前向声明 */


/* ================================================================== */
/*  内部: 基于约束图拓扑排序的变量消元顺序                               */
/* ================================================================== */

int *order_variables_by_dependency(const ConstraintGraph *graph, const int *var_ids, int var_count,
                                          const int *dirty_var_ids, int dirty_count, int *out_count) {
    *out_count = 0;
    if (!graph || var_count == 0)
        lv_RETURN_ERROR_NULL(lv_ERROR_INVALID_PARAM, "order_variables_by_dependency: NULL graph or empty var_count");

    int *id_to_idx = lv_calloc((size_t) var_count, sizeof(int));
    if (!id_to_idx)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "order_variables_by_dependency: lv_calloc for id_to_idx failed (count=%d)", var_count);
    for (int i = 0; i < var_count; i++)
        id_to_idx[i] = -1;
    for (int i = 0; i < var_count; i++) {
        bool dup = false;
        for (int j = 0; j < i; j++) {
            if (var_ids[j] == var_ids[i]) {
                dup = true;
                break;
            }
        }
        if (!dup)
            id_to_idx[i] = i;
    }

    bool **adj = lv_calloc((size_t) var_count, sizeof(bool *));
    for (int i = 0; i < var_count; i++) {
        adj[i] = lv_calloc((size_t) var_count, sizeof(bool));
    }

    int *participation = lv_calloc((size_t) var_count, sizeof(int));
    for (int ci = 0; ci < graph->constraint_count; ci++) {
        const Constraint *c = graph->constraints[ci];
        for (int p = 0; p < c->participant_count; p++) {
            int pid = c->participants[p];
            for (int j = 0; j < var_count; j++) {
                if (var_ids[j] == pid && id_to_idx[j] >= 0) {
                    participation[j]++;
                    break;
                }
            }
        }
    }

    for (int ci = 0; ci < graph->constraint_count; ci++) {
        const Constraint *c = graph->constraints[ci];
        int p_indices[32];
        int p_count = 0;
        for (int p = 0; p < c->participant_count && p < 32; p++) {
            int pid = c->participants[p];
            for (int j = 0; j < var_count; j++) {
                if (var_ids[j] == pid && id_to_idx[j] >= 0) {
                    p_indices[p_count++] = j;
                    break;
                }
            }
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

    int *in_degree = lv_calloc((size_t) var_count, sizeof(int));
    for (int i = 0; i < var_count; i++) {
        for (int j = 0; j < var_count; j++) {
            if (adj[i][j])
                in_degree[j]++;
        }
    }

    bool *in_subset = lv_calloc((size_t) var_count, sizeof(bool));
    bool use_subset = (dirty_var_ids != NULL && dirty_count > 0);
    if (use_subset) {
        for (int i = 0; i < dirty_count; i++) {
            for (int j = 0; j < var_count; j++) {
                if (var_ids[j] == dirty_var_ids[i]) {
                    in_subset[j] = true;
                    break;
                }
            }
        }
    } else {
        for (int i = 0; i < var_count; i++)
            in_subset[i] = true;
    }

    int *order = lv_calloc((size_t) var_count, sizeof(int));
    if (!order) {
        for (int i = 0; i < var_count; i++)
            lv_free((void **) &adj[i]);
        lv_free((void **) &adj);
        lv_free((void **) &participation);
        lv_free((void **) &in_degree);
        lv_free((void **) &in_subset);
        lv_free((void **) &id_to_idx);
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "order_variables_by_dependency: lv_calloc for order failed (count=%d)", var_count);
    }
    int order_count = 0;
    bool *visited = lv_calloc((size_t) var_count, sizeof(bool));

    while (order_count < var_count) {
        int best = -1;
        int best_participation = -1;

        for (int i = 0; i < var_count; i++) {
            if (visited[i])
                continue;
            if (!in_subset[i] && order_count < var_count - (use_subset ? (var_count - dirty_count) : 0)) {
                continue;
            }
            if (in_degree[i] > 0)
                continue;

            if (participation[i] > best_participation ||
                (participation[i] == best_participation && var_ids[i] < var_ids[best])) {
                best = i;
                best_participation = participation[i];
            }
        }

        if (best < 0) {
            int unvisited = 0;
            for (int i = 0; i < var_count; i++) {
                if (!visited[i])
                    unvisited++;
            }
            fprintf(stderr,
                    "order_variables_by_dependency: cycle detected in constraint graph, "
                    "%d variables remain unscheduled\n",
                    unvisited);
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

    for (int i = 0; i < var_count; i++)
        lv_free((void **) &adj[i]);
    lv_free((void **) &adj);
    lv_free((void **) &participation);
    lv_free((void **) &in_degree);
    lv_free((void **) &visited);
    lv_free((void **) &in_subset);
    lv_free((void **) &id_to_idx);

    *out_count = order_count;
    return order;
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

    int *in_degree = lv_calloc((size_t) var_count, sizeof(int));
    for (int i = 0; i < var_count; i++) {
        for (int j = 0; j < var_count; j++) {
            if (adj[i][j])
                in_degree[j]++;
        }
    }

    int *order = lv_calloc((size_t) var_count, sizeof(int));
    if (!order) {
        for (int i = 0; i < var_count; i++)
            lv_free((void **) &adj[i]);
        lv_free((void **) &adj);
        lv_free((void **) &eq_count);
        lv_free((void **) &constraint_count);
        lv_free((void **) &weight);
        lv_free((void **) &in_degree);
        lv_free((void **) &var_ids);
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "compute_elimination_order: lv_calloc for order failed (count=%d)", var_count);
    }
    int order_count = 0;
    bool *visited = lv_calloc((size_t) var_count, sizeof(bool));

    while (order_count < var_count) {
        int best = -1;
        int best_weight = -1;

        for (int i = 0; i < var_count; i++) {
            if (visited[i])
                continue;
            if (in_degree[i] > 0)
                continue;

            if (weight[i] > best_weight || (weight[i] == best_weight && var_ids[i] < var_ids[best])) {
                best = i;
                best_weight = weight[i];
            }
        }

        if (best < 0) {
            int unvisited = 0;
            for (int i = 0; i < var_count; i++) {
                if (!visited[i])
                    unvisited++;
            }
            fprintf(stderr,
                    "compute_elimination_order: cycle detected in constraint graph, "
                    "%d variables remain unscheduled\n",
                    unvisited);
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

    for (int i = 0; i < var_count; i++) {
        lv_free((void **) &adj[i]);
    }
    lv_free((void **) &adj);
    lv_free((void **) &eq_count);
    lv_free((void **) &constraint_count);
    lv_free((void **) &weight);
    lv_free((void **) &in_degree);
    lv_free((void **) &visited);
    lv_free((void **) &var_ids);

    *out_order_count = order_count;
    return order;
}
