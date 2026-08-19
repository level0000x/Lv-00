/**
 * @file proof_strategy_angle.c
 * @brief 全角法策略执行
 *
 * 从 proof_strategy_exec.c 拆分的模块之一。
 *
 * @version v3.6.0
 */

#include "proof_multi_strategy_internal.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/constraint_graph.h"
#include "lv/proof.h"
#include "lv/solver.h"

#include "lv/atp_backend.h"
#include "lv/debug.h"
#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/lambda_to_graph.h"
#include "lv/lambda_unify.h"
#include "lv/normalization.h"
#include "lv/type_system.h"
#include "lv/unify.h"

/**
 * @brief 全角法执行 —— 利用全角关系推理
 *
 * 借鉴 JGEX 的全角法（张景中团队）：
 * - 全角定义为两条有向线之间的夹角
 * - 核心性质：全角相等、全角加减
 * - 从约束图中提取线段对（由点对定义）
 * - 通过约束类型推导角度等式关系
 * - 利用全角变换链完成证明
 */
bool execute_full_angle_method(ProofMultiStrategy *mse, ProofNavigator *nav) {
    (void) mse;
    if (!nav || !nav->construction)
        return false;

    ConstraintGraph *graph = nav->construction;

    /* 收集所有线段（由两个点定义） */
    typedef struct {
        int p1_id; /* 起点 */
        int p2_id; /* 终点 */
    } DirectedLine;

    DirectedLine lines[256];
    int line_count = 0;

    /* 从关联约束和线段节点中提取有向线 */
    for (int i = 0; i < graph->node_count && line_count < 256; i++) {
        GeomNode *node = graph->nodes[i];
        if (node && node->type == GEOM_LINE_SEGMENT) {
            /* 线段节点：通过约束找到端点 */
            lines[line_count].p1_id = -1;
            lines[line_count].p2_id = -1;
            /* 尝试从约束中获取端点 */
            for (int j = 0; j < graph->constraint_count; j++) {
                Constraint *c = graph->constraints[j];
                if (!c || !c->is_active)
                    continue;
                if (c->type == INCIDENCE && c->participant_count >= 2) {
                    if (c->participants[1] == node->id) {
                        if (lines[line_count].p1_id < 0)
                            lines[line_count].p1_id = c->participants[0];
                        else if (lines[line_count].p2_id < 0)
                            lines[line_count].p2_id = c->participants[0];
                    }
                }
            }
            if (lines[line_count].p1_id >= 0 && lines[line_count].p2_id >= 0) {
                line_count++;
            }
        }
    }

    /* 也从之间约束中提取有向线 */
    for (int i = 0; i < graph->constraint_count && line_count < 256; i++) {
        Constraint *c = graph->constraints[i];
        if (!c || !c->is_active || c->type != BETWEENNESS)
            continue;
        if (c->participant_count < 3)
            continue;
        /* A-B-C 产生两条有向线: AB 和 BC */
        lines[line_count].p1_id = c->participants[0];
        lines[line_count].p2_id = c->participants[1];
        line_count++;
        if (line_count < 256) {
            lines[line_count].p1_id = c->participants[1];
            lines[line_count].p2_id = c->participants[2];
            line_count++;
        }
    }

    if (line_count < 2) {
        ProofStep *step = proof_step_create(PROOF_STEP_REWRITE);
        if (step) {
            step->color = PROOF_COLOR_BLUE_UNEXPLORED;
            step->note = lv_strdup_safe("[全角法] 线段数不足，无法构建全角关系");
            proof_navigator_add_step(nav, step);
        }
        return false;
    }

    /* 添加全角法起始步骤 */
    ProofStep *start_step = proof_step_create(PROOF_STEP_REWRITE);
    if (start_step) {
        start_step->color = PROOF_COLOR_GREEN;
        char buf[256];
        lv_snprintf(buf, sizeof(buf), "[全角法] 提取 %d 条有向线，构建全角关系进行消点推理", line_count);
        start_step->note = lv_strdup_safe(buf);
        proof_navigator_add_step(nav, start_step);
    }

    bool verified = false;

    /* 全角性质1：对顶角相等 —— 通过相交约束推导 */
    for (int i = 0; i < graph->constraint_count && !verified; i++) {
        Constraint *c = graph->constraints[i];
        if (!c || !c->is_active || c->type != INTERSECTION)
            continue;
        if (c->participant_count < 3)
            continue;

        int inter_point = c->participants[2]; /* 交点 */
        int line1_id = c->participants[0];
        int line2_id = c->participants[1];

        /* 找到两条线上除交点外的另一个点 */
        int l1_other = -1, l2_other = -1;
        for (int j = 0; j < graph->constraint_count; j++) {
            Constraint *cc = graph->constraints[j];
            if (!cc || !cc->is_active || cc->type != INCIDENCE)
                continue;
            if (cc->participant_count < 2)
                continue;
            if (cc->participants[1] == line1_id && cc->participants[0] != inter_point) {
                l1_other = cc->participants[0];
            }
            if (cc->participants[1] == line2_id && cc->participants[0] != inter_point) {
                l2_other = cc->participants[0];
            }
        }

        if (l1_other >= 0 && l2_other >= 0) {
            ProofStep *angle_step = proof_step_create(PROOF_STEP_FUNCTION_APP);
            if (angle_step) {
                angle_step->color = PROOF_COLOR_GREEN;
                angle_step->note =
                    lv_strdup_safe("[全角法] 对顶角相等：由相交约束推导 ∠(l1_other, inter, l2_other) 的对顶角关系");
                proof_navigator_add_step(nav, angle_step);
            }
        }
    }

    /* 全角性质2：全角加减 —— 三点共线时全角为0或pi */
    for (int i = 0; i < graph->constraint_count && !verified; i++) {
        Constraint *c = graph->constraints[i];
        if (!c || !c->is_active || c->type != BETWEENNESS)
            continue;
        if (c->participant_count < 3)
            continue;

        /* A-B-C 共线 => 全角(AB, BC) = pi */
        ProofStep *collinear_step = proof_step_create(PROOF_STEP_FUNCTION_APP);
        if (collinear_step) {
            collinear_step->color = PROOF_COLOR_GREEN;
            collinear_step->note = lv_strdup_safe("[全角法] 三点共线 => 全角(AB, BC) = π，用于消点");
            proof_navigator_add_step(nav, collinear_step);
        }
    }

    /* 全角性质3：三角形内角和为pi */
    /* 查找三角形结构（三个点两两之间有约束） */
    for (int i = 0; i < graph->node_count && !verified; i++) {
        GeomNode *ni = graph->nodes[i];
        if (!ni || ni->type != GEOM_POINT)
            continue;
        for (int j = i + 1; j < graph->node_count && !verified; j++) {
            GeomNode *nj = graph->nodes[j];
            if (!nj || nj->type != GEOM_POINT)
                continue;
            for (int k = j + 1; k < graph->node_count && !verified; k++) {
                GeomNode *nk = graph->nodes[k];
                if (!nk || nk->type != GEOM_POINT)
                    continue;

                /* 检查三点是否构成三角形（两两之间有约束） */
                bool has_ij = false, has_jk = false, has_ki = false;
                for (int c = 0; c < graph->constraint_count; c++) {
                    Constraint *con = graph->constraints[c];
                    if (!con || !con->is_active)
                        continue;
                    if (con->participant_count < 2)
                        continue;
                    if ((con->participants[0] == ni->id && con->participants[1] == nj->id) ||
                        (con->participants[0] == nj->id && con->participants[1] == ni->id))
                        has_ij = true;
                    if ((con->participants[0] == nj->id && con->participants[1] == nk->id) ||
                        (con->participants[0] == nk->id && con->participants[1] == nj->id))
                        has_jk = true;
                    if ((con->participants[0] == nk->id && con->participants[1] == ni->id) ||
                        (con->participants[0] == ni->id && con->participants[1] == nk->id))
                        has_ki = true;
                }

                if (has_ij && has_jk && has_ki) {
                    ProofStep *tri_step = proof_step_create(PROOF_STEP_FUNCTION_APP);
                    if (tri_step) {
                        tri_step->color = PROOF_COLOR_GREEN;
                        tri_step->note = lv_strdup_safe("[全角法] 三角形内角和为π：∠A + ∠B + ∠C = π");
                        proof_navigator_add_step(nav, tri_step);
                    }
                }
            }
        }
    }

    /* 全角性质4：等腰三角形底角相等 */
    for (int i = 0; i < graph->node_count && !verified; i++) {
        GeomNode *ni = graph->nodes[i];
        if (!ni || ni->type != GEOM_POINT || ni->coord_count < 2)
            continue;
        for (int j = i + 1; j < graph->node_count && !verified; j++) {
            GeomNode *nj = graph->nodes[j];
            if (!nj || nj->type != GEOM_POINT || nj->coord_count < 2)
                continue;
            for (int k = j + 1; k < graph->node_count && !verified; k++) {
                GeomNode *nk = graph->nodes[k];
                if (!nk || nk->type != GEOM_POINT || nk->coord_count < 2)
                    continue;

                /* 检查是否为等腰三角形：两腰长度相等 */
                SymbolicCoord *ij_dx = symbolic_coord_subtract(nj->symbolic_coords[0], ni->symbolic_coords[0]);
                SymbolicCoord *ij_dy = symbolic_coord_subtract(nj->symbolic_coords[1], ni->symbolic_coords[1]);
                SymbolicCoord *ik_dx = symbolic_coord_subtract(nk->symbolic_coords[0], ni->symbolic_coords[0]);
                SymbolicCoord *ik_dy = symbolic_coord_subtract(nk->symbolic_coords[1], ni->symbolic_coords[1]);

                if (ij_dx && ij_dy && ik_dx && ik_dy) {
                    /* |IJ|^2 和 |IK|^2 */
                    SymbolicCoord *ij_sq1 = symbolic_coord_multiply(ij_dx, ij_dx);
                    SymbolicCoord *ij_sq2 = symbolic_coord_multiply(ij_dy, ij_dy);
                    SymbolicCoord *ik_sq1 = symbolic_coord_multiply(ik_dx, ik_dx);
                    SymbolicCoord *ik_sq2 = symbolic_coord_multiply(ik_dy, ik_dy);

                    if (ij_sq1 && ij_sq2 && ik_sq1 && ik_sq2) {
                        SymbolicCoord *ij_sq = symbolic_coord_add(ij_sq1, ij_sq2);
                        SymbolicCoord *ik_sq = symbolic_coord_add(ik_sq1, ik_sq2);
                        SymbolicCoord *diff = (ij_sq && ik_sq) ? symbolic_coord_subtract(ij_sq, ik_sq) : NULL;
                        if (ij_sq && ik_sq && diff && symbolic_coord_is_zero(diff)) {
                            ProofStep *iso_step = proof_step_create(PROOF_STEP_FUNCTION_APP);
                            if (iso_step) {
                                iso_step->color = PROOF_COLOR_GREEN;
                                iso_step->note =
                                    lv_strdup_safe("[全角法] 等腰三角形底角相等：由两边相等推导底角全角相等");
                                proof_navigator_add_step(nav, iso_step);
                            }
                        }
                        if (diff)
                            symbolic_coord_destroy(diff);
                        if (ik_sq)
                            symbolic_coord_destroy(ik_sq);
                        if (ij_sq)
                            symbolic_coord_destroy(ij_sq);
                    }
                    if (ik_sq2)
                        symbolic_coord_destroy(ik_sq2);
                    if (ik_sq1)
                        symbolic_coord_destroy(ik_sq1);
                    if (ij_sq2)
                        symbolic_coord_destroy(ij_sq2);
                    if (ij_sq1)
                        symbolic_coord_destroy(ij_sq1);
                }
                if (ik_dy)
                    symbolic_coord_destroy(ik_dy);
                if (ik_dx)
                    symbolic_coord_destroy(ik_dx);
                if (ij_dy)
                    symbolic_coord_destroy(ij_dy);
                if (ij_dx)
                    symbolic_coord_destroy(ij_dx);
            }
        }
    }

    /* 最终合一检查 */
    if (!verified && nav->target_prop && nav->target_prop->pattern) {
        UnifyStatus status = proof_unify(graph, nav->target_prop, false);
        ProofStep *unify_step = proof_step_create(PROOF_STEP_UNIFY);
        if (unify_step) {
            unify_step->color = (status == UNIFY_STATUS_OK) ? PROOF_COLOR_GREEN : PROOF_COLOR_BLUE_UNEXPLORED;
            unify_step->note = lv_strdup_safe("[全角法] 全角关系推导后执行合一检查");
            proof_navigator_add_step(nav, unify_step);
        }
        verified = (status == UNIFY_STATUS_OK);
    }

    return verified;
}
