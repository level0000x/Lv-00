/**
 * @file proof_strategy_vector.c
 * @brief 向量法策略执行
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

#include "atp_backend.h"
#include "debug.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "lv/lambda_to_graph.h"
#include "lv/lambda_unify.h"
#include "normalization.h"
#include "type_system.h"
#include "unify.h"

/**
 * @brief 向量法执行 —— 使用矢量代数推导
 *
 * 借鉴 JGEX 的向量法：
 * - 从构造图中提取几何点的符号坐标
 * - 构建向量表达式（点坐标差）
 * - 通过向量运算验证目标命题：
 *   - 共线：叉积为零
 *   - 平行：向量成标量倍
 *   - 垂直：点积为零
 *   - 中点：位置向量相等
 *   - 比例：向量模长比较
 */
bool execute_vector_method(ProofMultiStrategy *mse, ProofNavigator *nav) {
    (void) mse;
    if (!nav || !nav->construction)
        return false;

    ConstraintGraph *graph = nav->construction;

    /* 收集所有几何点节点 */
    int point_count = 0;
    int point_ids[256]; /* 最多处理256个点 */
    for (int i = 0; i < graph->node_count && point_count < 256; i++) {
        GeomNode *node = graph->nodes[i];
        if (node && node->type == GEOM_POINT) {
            point_ids[point_count++] = node->id;
        }
    }

    if (point_count < 2) {
        /* 至少需要两个点才能构建向量 */
        ProofStep *step = proof_step_create(PROOF_STEP_REWRITE);
        if (step) {
            step->color = PROOF_COLOR_BLUE_UNEXPLORED;
            step->note = lv_strdup_safe("[向量法] 点数不足，无法构建向量表达式");
            proof_navigator_add_step(nav, step);
        }
        return false;
    }

    /* 添加向量法起始步骤 */
    ProofStep *start_step = proof_step_create(PROOF_STEP_REWRITE);
    if (start_step) {
        start_step->color = PROOF_COLOR_GREEN;
        char buf[256];
        snprintf(buf, sizeof(buf), "[向量法] 提取 %d 个几何点，构建向量表达式进行代数推导", point_count);
        start_step->note = lv_strdup_safe(buf);
        proof_navigator_add_step(nav, start_step);
    }

    /* 构建向量：对每对点计算向量差 */
    bool verified = false;

    /* 检查目标命题是否涉及垂直关系（点积=0） */
    if (nav->target_prop && nav->target_prop->name) {
        if (strstr(nav->target_prop->name, "perpendicular") || strstr(nav->target_prop->name, "垂直")) {
            /* 遍历所有线段对，检查点积 */
            for (int i = 0; i < graph->constraint_count && !verified; i++) {
                Constraint *c = graph->constraints[i];
                if (!c || !c->is_active)
                    continue;
                if (c->type != INTERSECTION)
                    continue;

                /* 找到相交的线段，计算方向向量 */
                for (int j = 0; j < graph->constraint_count && !verified; j++) {
                    if (j == i)
                        continue;
                    Constraint *c2 = graph->constraints[j];
                    if (!c2 || !c2->is_active || c2->type != INTERSECTION)
                        continue;

                    /* 使用符号坐标计算点积 */
                    /* 取两组不同的点对构建向量 */
                    if (c->participant_count >= 3 && c2->participant_count >= 3) {
                        GeomNode *p1 = graph_get_node(graph, c->participants[0]);
                        GeomNode *p2 = graph_get_node(graph, c->participants[1]);
                        GeomNode *p3 = graph_get_node(graph, c2->participants[0]);
                        GeomNode *p4 = graph_get_node(graph, c2->participants[1]);

                        if (p1 && p1->coord_count >= 2 && p2 && p2->coord_count >= 2 && p3 && p3->coord_count >= 2 &&
                            p4 && p4->coord_count >= 2) {
                            /* 向量 v1 = p2 - p1, v2 = p4 - p3 */
                            SymbolicCoord *v1x =
                                symbolic_coord_subtract(p2->symbolic_coords[0], p1->symbolic_coords[0]);
                            SymbolicCoord *v1y =
                                symbolic_coord_subtract(p2->symbolic_coords[1], p1->symbolic_coords[1]);
                            SymbolicCoord *v2x =
                                symbolic_coord_subtract(p4->symbolic_coords[0], p3->symbolic_coords[0]);
                            SymbolicCoord *v2y =
                                symbolic_coord_subtract(p4->symbolic_coords[1], p3->symbolic_coords[1]);

                            if (v1x && v1y && v2x && v2y) {
                                /* 点积: v1x*v2x + v1y*v2y */
                                SymbolicCoord *dot1 = symbolic_coord_multiply(v1x, v2x);
                                SymbolicCoord *dot2 = symbolic_coord_multiply(v1y, v2y);
                                SymbolicCoord *dot = NULL;
                                if (dot1 && dot2) {
                                    dot = symbolic_coord_add(dot1, dot2);
                                }

                                if (dot && symbolic_coord_is_zero(dot)) {
                                    ProofStep *dot_step = proof_step_create(PROOF_STEP_REWRITE);
                                    if (dot_step) {
                                        dot_step->color = PROOF_COLOR_GREEN;
                                        dot_step->note = lv_strdup_safe("[向量法] 点积为零，验证垂直关系成立");
                                        proof_navigator_add_step(nav, dot_step);
                                    }
                                    verified = true;
                                }

                                if (dot)
                                    symbolic_coord_destroy(dot);
                                if (dot2)
                                    symbolic_coord_destroy(dot2);
                                if (dot1)
                                    symbolic_coord_destroy(dot1);
                            }
                            if (v2y)
                                symbolic_coord_destroy(v2y);
                            if (v2x)
                                symbolic_coord_destroy(v2x);
                            if (v1y)
                                symbolic_coord_destroy(v1y);
                            if (v1x)
                                symbolic_coord_destroy(v1x);
                        }
                    }
                }
            }
        }

        /* 检查共线关系（叉积=0） */
        if (!verified && (strstr(nav->target_prop->name, "collinear") || strstr(nav->target_prop->name, "共线"))) {
            for (int i = 0; i < point_count - 2 && !verified; i++) {
                GeomNode *pa = graph_get_node(graph, point_ids[i]);
                GeomNode *pb = graph_get_node(graph, point_ids[i + 1]);
                GeomNode *pc = graph_get_node(graph, point_ids[i + 2]);
                if (!pa || !pb || !pc)
                    continue;
                if (pa->coord_count < 2 || pb->coord_count < 2 || pc->coord_count < 2)
                    continue;

                /* 叉积: (pb-pa) x (pc-pa) = (bx-ax)*(cy-ay) - (by-ay)*(cx-ax) */
                SymbolicCoord *abx = symbolic_coord_subtract(pb->symbolic_coords[0], pa->symbolic_coords[0]);
                SymbolicCoord *aby = symbolic_coord_subtract(pb->symbolic_coords[1], pa->symbolic_coords[1]);
                SymbolicCoord *acx = symbolic_coord_subtract(pc->symbolic_coords[0], pa->symbolic_coords[0]);
                SymbolicCoord *acy = symbolic_coord_subtract(pc->symbolic_coords[1], pa->symbolic_coords[1]);

                if (abx && aby && acx && acy) {
                    SymbolicCoord *term1 = symbolic_coord_multiply(abx, acy);
                    SymbolicCoord *term2 = symbolic_coord_multiply(aby, acx);
                    SymbolicCoord *cross = NULL;
                    if (term1 && term2) {
                        cross = symbolic_coord_subtract(term1, term2);
                    }

                    if (cross && symbolic_coord_is_zero(cross)) {
                        ProofStep *cross_step = proof_step_create(PROOF_STEP_REWRITE);
                        if (cross_step) {
                            cross_step->color = PROOF_COLOR_GREEN;
                            cross_step->note = lv_strdup_safe("[向量法] 叉积为零，验证共线关系成立");
                            proof_navigator_add_step(nav, cross_step);
                        }
                        verified = true;
                    }

                    if (cross)
                        symbolic_coord_destroy(cross);
                    if (term2)
                        symbolic_coord_destroy(term2);
                    if (term1)
                        symbolic_coord_destroy(term1);
                }
                if (acy)
                    symbolic_coord_destroy(acy);
                if (acx)
                    symbolic_coord_destroy(acx);
                if (aby)
                    symbolic_coord_destroy(aby);
                if (abx)
                    symbolic_coord_destroy(abx);
            }
        }

        /* 检查平行关系（向量成标量倍） */
        if (!verified && (strstr(nav->target_prop->name, "parallel") || strstr(nav->target_prop->name, "平行"))) {
            /* 平行检查：两组向量的叉积为零 */
            for (int i = 0; i < graph->constraint_count && !verified; i++) {
                Constraint *c = graph->constraints[i];
                if (!c || !c->is_active || c->type != INCIDENCE)
                    continue;
                for (int j = i + 1; j < graph->constraint_count && !verified; j++) {
                    Constraint *c2 = graph->constraints[j];
                    if (!c2 || !c2->is_active || c2->type != INCIDENCE)
                        continue;

                    /* 取两条线段的方向向量 */
                    if (c->participant_count >= 2 && c2->participant_count >= 2) {
                        /* 通过关联约束找到线段上的点 */
                        int l1 = c->participants[1];  /* 线段ID */
                        int l2 = c2->participants[1]; /* 线段ID */

                        /* 查找线段端点：找到与同一线段关联的所有点 */
                        int l1_points[32], l1_pt_count = 0;
                        int l2_points[32], l2_pt_count = 0;
                        for (int k = 0; k < graph->constraint_count; k++) {
                            Constraint *cc = graph->constraints[k];
                            if (!cc || !cc->is_active || cc->type != INCIDENCE)
                                continue;
                            if (cc->participant_count < 2)
                                continue;
                            if (cc->participants[1] == l1 && l1_pt_count < 32) {
                                l1_points[l1_pt_count++] = cc->participants[0];
                            }
                            if (cc->participants[1] == l2 && l2_pt_count < 32) {
                                l2_points[l2_pt_count++] = cc->participants[0];
                            }
                        }

                        /* 取每条线段的前两个关联点作为方向向量端点 */
                        int l1_p1 = (l1_pt_count >= 1) ? l1_points[0] : -1;
                        int l1_p2 = (l1_pt_count >= 2) ? l1_points[1] : -1;
                        int l2_p1 = (l2_pt_count >= 1) ? l2_points[0] : -1;
                        int l2_p2 = (l2_pt_count >= 2) ? l2_points[1] : -1;

                        if (l1_p1 >= 0 && l1_p2 >= 0 && l2_p1 >= 0 && l2_p2 >= 0) {
                            GeomNode *p1 = graph_get_node(graph, l1_p1);
                            GeomNode *p2 = graph_get_node(graph, l1_p2);
                            GeomNode *p3 = graph_get_node(graph, l2_p1);
                            GeomNode *p4 = graph_get_node(graph, l2_p2);
                            if (p1 && p2 && p3 && p4 && p1->coord_count >= 2 && p2->coord_count >= 2 &&
                                p3->coord_count >= 2 && p4->coord_count >= 2) {
                                SymbolicCoord *v1x =
                                    symbolic_coord_subtract(p2->symbolic_coords[0], p1->symbolic_coords[0]);
                                SymbolicCoord *v1y =
                                    symbolic_coord_subtract(p2->symbolic_coords[1], p1->symbolic_coords[1]);
                                SymbolicCoord *v2x =
                                    symbolic_coord_subtract(p4->symbolic_coords[0], p3->symbolic_coords[0]);
                                SymbolicCoord *v2y =
                                    symbolic_coord_subtract(p4->symbolic_coords[1], p3->symbolic_coords[1]);

                                if (v1x && v1y && v2x && v2y) {
                                    SymbolicCoord *cross_term1 = symbolic_coord_multiply(v1x, v2y);
                                    SymbolicCoord *cross_term2 = symbolic_coord_multiply(v1y, v2x);
                                    SymbolicCoord *cross = NULL;
                                    if (cross_term1 && cross_term2) {
                                        cross = symbolic_coord_subtract(cross_term1, cross_term2);
                                    }
                                    if (cross && symbolic_coord_is_zero(cross)) {
                                        ProofStep *para_step = proof_step_create(PROOF_STEP_REWRITE);
                                        if (para_step) {
                                            para_step->color = PROOF_COLOR_GREEN;
                                            para_step->note =
                                                lv_strdup_safe("[向量法] 方向向量叉积为零，验证平行关系成立");
                                            proof_navigator_add_step(nav, para_step);
                                        }
                                        verified = true;
                                    }
                                    if (cross)
                                        symbolic_coord_destroy(cross);
                                    if (cross_term2)
                                        symbolic_coord_destroy(cross_term2);
                                    if (cross_term1)
                                        symbolic_coord_destroy(cross_term1);
                                }
                                if (v2y)
                                    symbolic_coord_destroy(v2y);
                                if (v2x)
                                    symbolic_coord_destroy(v2x);
                                if (v1y)
                                    symbolic_coord_destroy(v1y);
                                if (v1x)
                                    symbolic_coord_destroy(v1x);
                            }
                        }
                    }
                }
            }
        }

        /* 检查中点关系 */
        if (!verified && (strstr(nav->target_prop->name, "midpoint") || strstr(nav->target_prop->name, "中点"))) {
            for (int i = 0; i < graph->constraint_count && !verified; i++) {
                Constraint *c = graph->constraints[i];
                if (!c || !c->is_active || c->type != BETWEENNESS)
                    continue;
                if (c->participant_count < 3)
                    continue;

                GeomNode *pa = graph_get_node(graph, c->participants[0]);
                GeomNode *pm = graph_get_node(graph, c->participants[1]); /* 中点 */
                GeomNode *pb = graph_get_node(graph, c->participants[2]);
                if (!pa || !pm || !pb)
                    continue;
                if (pa->coord_count < 2 || pm->coord_count < 2 || pb->coord_count < 2)
                    continue;

                /* 中点条件: pm = (pa + pb) / 2, 即 2*pm = pa + pb */
                bool is_mid = true;
                for (int d = 0; d < 2; d++) {
                    SymbolicCoord *sum = symbolic_coord_add(pa->symbolic_coords[d], pb->symbolic_coords[d]);
                    SymbolicCoord *two_m =
                        symbolic_coord_multiply(pm->symbolic_coords[d], symbolic_coord_create_rational(2, 1));
                    if (!sum || !two_m || !symbolic_coord_is_zero(symbolic_coord_subtract(sum, two_m))) {
                        is_mid = false;
                    }
                    if (sum)
                        symbolic_coord_destroy(sum);
                    if (two_m)
                        symbolic_coord_destroy(two_m);
                }

                if (is_mid) {
                    ProofStep *mid_step = proof_step_create(PROOF_STEP_REWRITE);
                    if (mid_step) {
                        mid_step->color = PROOF_COLOR_GREEN;
                        mid_step->note = lv_strdup_safe("[向量法] 位置向量验证中点关系成立：2M = A + B");
                        proof_navigator_add_step(nav, mid_step);
                    }
                    verified = true;
                }
            }
        }
    }

    /* 如果向量运算未能直接验证，回退到合一检查 */
    if (!verified && nav->target_prop && nav->target_prop->pattern) {
        UnifyStatus status = proof_unify(graph, nav->target_prop, false);
        ProofStep *unify_step = proof_step_create(PROOF_STEP_UNIFY);
        if (unify_step) {
            unify_step->color = (status == UNIFY_STATUS_OK) ? PROOF_COLOR_GREEN : PROOF_COLOR_BLUE_UNEXPLORED;
            unify_step->note = lv_strdup_safe("[向量法] 向量代数推导后执行合一检查");
            proof_navigator_add_step(nav, unify_step);
        }
        verified = (status == UNIFY_STATUS_OK);
    }

    return verified;
}

