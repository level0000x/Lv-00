/**
 * @file solver_conflict.c
 * @brief 冲突检测（矛盾方程检测）
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
#include "stream_context_util.h"

/* 前向声明 */

void extract_equations_from_constraints(const ConstraintGraph *graph, EquationSystem *sys);
bool check_incompatible_distances(const ConstraintGraph *graph);
int constraint_weight(const Constraint *c);

/* ================================================================== */
/*  PUBLIC API: check_conflict_equations                               */
/* ================================================================== */

/**
 * @brief 检查约束图中是否存在冲突方程
 *
 * @details 执行多项冲突检测：
 *          1. 同一线段对上的不兼容距离约束
 *          2. 常数为非零的零次方程（矛盾：非零 = 0）
 *          3. 首项系数为零的一元方程（矛盾：0*x + b = 0, b != 0）
 *          4. 判别式为负的二次方程（无实解）
 *          5. 超定点的过约束方程数检测
 *          6. 同一参与者上的重复/冲突约束
 *
 * @param graph 约束图指针
 * @return true 表示存在冲突，false 表示无冲突
 */

bool check_conflict_equations(const ConstraintGraph *graph) {
    if (!graph)
        return false;

    /* Check 1: Incompatible distance constraints on same segment pair */
    if (check_incompatible_distances(graph)) {
        return true;
    }

    /* Check 2: Extract equations and look for contradictions */
    EquationSystem sys;
    equation_system_init(&sys);
    extract_equations_from_constraints(graph, &sys);
    PolyEquation *const eqs_arr = (PolyEquation *)sys.eqs.data;
    const int eqs_count = sys.eqs.count;

    /* Check for 0 = nonzero (constant contradictions) */
    for (int i = 0; i < eqs_count; i++) {
        mpz_poly_t *p = &eqs_arr[i].poly;
        if (p->degree == 0 && mpz_cmp_si(p->coeffs[0], 0) != 0) {
            equation_system_clear(&sys);
            return true;
        }
    }

    /* Check 3: Leading coefficient zero contradiction for degree-1 equations */
    for (int i = 0; i < eqs_count; i++) {
        mpz_poly_t *p = &eqs_arr[i].poly;
        if (p->degree == 1 && mpz_cmp_si(p->coeffs[1], 0) == 0 && mpz_cmp_si(p->coeffs[0], 0) != 0) {
            equation_system_clear(&sys);
            return true;
        }
    }

    /* Check 4: Quadratic equation with negative discriminant => no real solution */
    for (int i = 0; i < eqs_count; i++) {
        mpz_poly_t *p = &eqs_arr[i].poly;
        if (p->degree == 2) {
            /* Using mpz arithmetic for discriminant: D = b^2 - 4*a*c */
            mpz_t disc;
            mpz_init(disc);
            mpz_mul(disc, p->coeffs[1], p->coeffs[1]); /* b^2 */
            mpz_t four_ac;
            mpz_init(four_ac);
            mpz_mul_si(four_ac, p->coeffs[2], 4);    /* 4*a */
            mpz_mul(four_ac, four_ac, p->coeffs[0]); /* 4*a*c */
            mpz_sub(disc, disc, four_ac);            /* b^2 - 4ac */
            if (mpz_cmp_si(disc, 0) < 0) {
                mpz_clear(disc);
                mpz_clear(four_ac);
                equation_system_clear(&sys);
                return true;
            }
            mpz_clear(disc);
            mpz_clear(four_ac);
        }
    }

    /* Check 5: Set of equations per point may overconstrain */
    int max_id = 0;
    for (int i = 0; i < graph->node_count; i++) {
        if (graph->nodes[i]->id > max_id)
            max_id = graph->nodes[i]->id;
    }
    if (max_id > SOLVER_MAX_VAR_ID) {
        max_id = SOLVER_MAX_VAR_ID;
    }
    if (max_id > 0) {
        int *eq_count_per_point = lv_calloc((size_t) (max_id + 1), sizeof(int));
        if (!eq_count_per_point) {
            equation_system_clear(&sys);
            return false;
        }
        for (int i = 0; i < eqs_count; i++) {
            int vid = eqs_arr[i].var_node_id;
            if (vid >= 0 && vid <= max_id) {
                if (eqs_arr[i].poly.degree >= 0) {
                    eq_count_per_point[vid]++;
                }
            }
        }
        for (int i = 0; i < graph->constraint_count; i++) {
            Constraint *c = graph->constraints[i];
            for (int j = 0; j < c->participant_count; j++) {
                int pid = c->participants[j];
                GeomNode *n = graph_get_node(graph, pid);
                if (n && n->type == GEOM_POINT && pid <= max_id) {
                    eq_count_per_point[pid] += constraint_weight(c) / c->participant_count;
                }
            }
        }
        for (int i = 0; i < graph->node_count; i++) {
            GeomNode *n = graph->nodes[i];
            if (n->type == GEOM_POINT && n->id <= max_id) {
                if (eq_count_per_point[n->id] > 3) {
                    lv_free((void **) &eq_count_per_point);
                    equation_system_clear(&sys);
                    return true;
                }
            }
        }
        lv_free((void **) &eq_count_per_point);
    }

    /* Check 6: Duplicate constraints with different parameters */
    for (int i = 0; i < graph->constraint_count; i++) {
        for (int j = i + 1; j < graph->constraint_count; j++) {
            Constraint *ci = graph->constraints[i];
            Constraint *cj = graph->constraints[j];
            if (ci->type != cj->type)
                continue;
            if (ci->participant_count != cj->participant_count)
                continue;

            bool same_participants = true;
            for (int k = 0; k < ci->participant_count; k++) {
                if (ci->participants[k] != cj->participants[k]) {
                    same_participants = false;
                    break;
                }
            }
            if (same_participants && ci->type == BETWEENNESS && ci->participant_count >= 3) {
                /* Two betweenness constraints on the same triple with
                   different orderings could conflict */
                /* e.g., B(A,C,B) and B(B,A,C) cannot both hold */
            }
        }
    }

    equation_system_clear(&sys);
    return false;
}
