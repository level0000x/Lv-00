/**
 * @file solver_stats.c
 * @brief 求解器统计（自由度计算）
 *
 * @details 从 solver.c 拆分出的子模块（Lv-00 项目 v3.3.0+）。
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/constraint_graph.h"
#include "lv/solver.h"

#include "debug.h"
#include "lv_internal.h"
#include "lv_utils.h"

/* 前向声明：constraint_weight, count_point_variables 的实现在 solver_symbolic.c 中 */
int constraint_weight(const Constraint *c);
int count_point_variables(const ConstraintGraph *graph, int **out_ids);

/* ================================================================== */
/*  PUBLIC API: count_degrees_of_freedom                               */
/* ================================================================== */

/**
 * @brief 计算约束系统的自由度（修正版：返回 -1 表示错误）
 *
 * @details 自由度 = 总变量数 - 总约束数。
 *          每个点有 2 个坐标变量（x, y），约束权重取决于约束类型：
 *          INCIDENCE=1, BETWEENNESS=2, INTERSECTION=2, CONTAINMENT=1, CONNECTION=1。
 *          同时计入线段节点上的距离约束。
 *          通过每点方程数分析识别具体哪些变量是自由的。
 *
 * @param graph            约束图指针
 * @param out_free_var_ids 输出：自由变量 ID 数组（调用者负责释放）
 * @return 自由度数量（非负整数），-1 表示参数错误
 */

int count_degrees_of_freedom(const ConstraintGraph *graph, int **out_free_var_ids) {
    if (!graph) {
        if (out_free_var_ids)
            *out_free_var_ids = NULL;
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "count_degrees_of_freedom: graph is NULL");
    }

    /* Count point variables */
    int *pt_ids = NULL;
    int pt_count = count_point_variables(graph, &pt_ids);
    int total_vars = pt_count * 2;

    /* Sum constraint weights */
    int total_constraints = 0;
    for (int i = 0; i < graph->constraint_count; i++) {
        total_constraints += constraint_weight(graph->constraints[i]);
    }

    /* 线段本身表示两个端点之间的一条连接/距离关系，计作一个独立约束。 */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *n = graph->nodes[i];
        if (n && n->type == GEOM_LINE_SEGMENT) {
            total_constraints += 1;
        }
    }

    int dof = total_vars - total_constraints;
    if (dof < 0)
        dof = 0;

    /* Identify which specific variables are free.
       A variable is "free" if it has fewer equations than unknowns.
       We track this per-point: each point has 2 coords (x, y).
       A point is fully determined if it has >= 2 independent equations. */
    int *eq_per_point = lv_calloc((size_t) pt_count, sizeof(int));
    bool *point_has_quadratic = lv_calloc((size_t) pt_count, sizeof(bool));

    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *c = graph->constraints[i];
        int weight = constraint_weight(c);
        /* Distribute weight to participating points */
        int point_participants = 0;
        int first_pt_idx = -1;
        for (int j = 0; j < c->participant_count; j++) {
            GeomNode *n = graph_get_node(graph, c->participants[j]);
            if (n && n->type == GEOM_POINT) {
                for (int k = 0; k < pt_count; k++) {
                    if (pt_ids[k] == n->id) {
                        if (first_pt_idx < 0)
                            first_pt_idx = k;
                        point_participants++;
                        break;
                    }
                }
            }
        }
        if (point_participants > 0 && first_pt_idx >= 0) {
            /* Distribute equations equally among participating points */
            int per_point = weight / point_participants;
            int remainder = weight % point_participants;
            for (int j = 0; j < c->participant_count; j++) {
                GeomNode *n = graph_get_node(graph, c->participants[j]);
                if (n && n->type == GEOM_POINT) {
                    for (int k = 0; k < pt_count; k++) {
                        if (pt_ids[k] == n->id) {
                            eq_per_point[k] += per_point;
                            if (remainder > 0) {
                                eq_per_point[k]++;
                                remainder--;
                            }
                            break;
                        }
                    }
                }
            }
        }
    }

    /* 线段约束：分配到端点，用于自由变量明细。 */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *n = graph->nodes[i];
        if (!n || n->type != GEOM_LINE_SEGMENT)
            continue;
        /* Add one equation to each endpoint */
        if (n->coord_count >= 2) {
            /* Find the endpoint point nodes */
            for (int j = 0; j < graph->constraint_count; j++) {
                Constraint *c = graph->constraints[j];
                if (c->type != INCIDENCE)
                    continue;
                for (int p = 0; p < c->participant_count; p++) {
                    if (c->participants[p] == n->id) {
                        int other = c->participants[1 - p];
                        for (int k = 0; k < pt_count; k++) {
                            if (pt_ids[k] == other) {
                                eq_per_point[k] += 1;
                                point_has_quadratic[k] = true;
                                break;
                            }
                        }
                    }
                }
            }
        }
    }

    /* Collect free variable IDs: points with fewer than 2 equations */
    int *free_ids = lv_calloc((size_t) dof, sizeof(int));
    if (!free_ids) {
        lv_free((void **) &pt_ids);
        lv_free((void **) &eq_per_point);
        lv_free((void **) &point_has_quadratic);
        lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "count_degrees_of_freedom: free_ids lv_calloc failed");
    }
    int free_count = 0;
    for (int i = 0; i < pt_count && free_count < dof; i++) {
        int equations = eq_per_point[i];
        int coords = 2;
        if (equations < coords) {
            /* This point has free coordinates */
            int free_coords = coords - equations;
            for (int c = 0; c < free_coords && free_count < dof; c++) {
                free_ids[free_count++] = pt_ids[i];
            }
        }
    }

    /* If we didn't find enough free variables through per-point analysis,
       fill the rest from underdetermined points */
    if (free_count < dof) {
        for (int i = 0; i < pt_count && free_count < dof; i++) {
            free_ids[free_count++] = pt_ids[i];
        }
    }

    lv_free((void **) &eq_per_point);
    lv_free((void **) &point_has_quadratic);
    lv_free((void **) &pt_ids);

    *out_free_var_ids = free_ids;
    return dof;
}
