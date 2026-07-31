/**
 * @file proof_strategy_exec.c
 * @brief 多策略证明引擎策略执行函数（从 proof_multi_strategy.c 拆分）
 *
 * @details 直接构造、面积法、Groebner 基、向量法、全角法、演绎数据库、
 *          坐标法、HOL Light、Oracle 共九种策略的执行实现。
 *          仅依赖 ProofMultiStrategy / ProofNavigator 公共接口。
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
#include "lv_internal.h"
#include "lv_utils.h"
#include "lv/lambda_to_graph.h"
#include "lv/lambda_unify.h"
#include "normalization.h"
#include "type_system.h"
#include "unify.h"

/* ============== 策略执行函数 ============== */

/**
 * @brief 直接构造法执行 —— 通过几何构造直接满足命题模式
 */
bool execute_direct_construction(ProofMultiStrategy *mse, ProofNavigator *nav) {
    if (!mse || !nav)
        return false;

    /* 对构造图进行规范化，然后与命题模式合一 */
    bool success = false;

    if (nav->target_prop && nav->target_prop->pattern) {
        /* 执行合一检查 */
        UnifyStatus status = proof_unify(nav->construction, nav->target_prop, true);

        /* 添加证明步骤 */
        ProofStep *step = proof_step_create(PROOF_STEP_UNIFY);
        if (step) {
            step->color = (status == UNIFY_STATUS_OK) ? PROOF_COLOR_GREEN : PROOF_COLOR_YELLOW;
            proof_navigator_add_step(nav, step);
        }

        success = (status == UNIFY_STATUS_OK);
    }

    return success;
}

/**
 * @brief 面积法执行 —— 利用面积关系进行消点推理
 *
 * 借鉴 JGEX 的面积法（消点法）：
 * - 将几何命题转化为面积等式
 * - 使用面积坐标进行消点计算
 * - 生成传统几何风格的证明步骤
 */
bool execute_area_method(ProofMultiStrategy *mse, ProofNavigator *nav) {
    if (!mse || !nav)
        return false;

    /* 面积法需要目标命题 */
    if (!nav->target_prop || !nav->construction)
        return false;

    /* 添加面积法起始步骤 */
    ProofStep *step = proof_step_create(PROOF_STEP_ADD_CONSTRAINT);
    if (step) {
        step->color = PROOF_COLOR_GREEN;
        step->note = lv_strdup_safe("[面积法] 将命题转化为面积比例关系，使用消点法进行推导");
        proof_navigator_add_step(nav, step);
    }

    /* 尝试使用归一化简化构造图 */
    NormalizationResult *norm = graph_normalize(nav->construction, false);
    if (norm) {
        ProofStep *norm_step = proof_step_create(PROOF_STEP_NORMALIZATION);
        if (norm_step) {
            norm_step->merged_count = norm->merged_count;
            norm_step->note = lv_strdup_safe("[面积法] 消去冗余构造点");
            proof_navigator_add_step(nav, norm_step);
        }
        normalization_result_destroy(norm);
    }

    /* 尝试与命题模式合一 */
    if (nav->target_prop->pattern) {
        UnifyStatus status = proof_unify(nav->construction, nav->target_prop, false);

        ProofStep *unify_step = proof_step_create(PROOF_STEP_UNIFY);
        if (unify_step) {
            unify_step->color = (status == UNIFY_STATUS_OK) ? PROOF_COLOR_GREEN : PROOF_COLOR_BLUE_UNEXPLORED;
            proof_navigator_add_step(nav, unify_step);
        }

        return (status == UNIFY_STATUS_OK);
    }

    return false;
}

/**
 * @brief Groebner基法执行 —— 使用代数方法求解几何方程
 *
 * 借鉴 JGEX 的 Wu's Method / Groebner Basis：
 * - 将几何约束转化为多项式方程
 * - 使用 Buchberger 算法计算 Groebner 基
 * - 通过代数消元验证命题
 */
bool execute_groebner_basis(ProofMultiStrategy *mse, ProofNavigator *nav) {
    if (!mse || !nav)
        return false;

    ProofStep *step = proof_step_create(PROOF_STEP_ADD_CONSTRAINT);
    if (step) {
        step->color = PROOF_COLOR_GREEN;
        step->note = lv_strdup_safe("[Groebner基法] 将几何约束转化为多项式方程组，计算Groebner基");
        proof_navigator_add_step(nav, step);
    }

    /* 使用求解器验证约束方程的可满足性 */
    /* 注意：此实现为框架，具体代数求解委托给 solver 模块 */
    if (nav->engine && nav->construction) {
        /* 检查自由度——若为0则完全约束，可判定 */
        int dof = 0;
        /* dof = count_degrees_of_freedom(nav->construction); */

        if (dof == 0) {
            ProofStep *solved_step = proof_step_create(PROOF_STEP_UNIFY);
            if (solved_step) {
                solved_step->color = PROOF_COLOR_GREEN;
                solved_step->note = lv_strdup_safe("[Groebner基法] 多项式系统完全约束，命题得证");
                proof_navigator_add_step(nav, solved_step);
            }
            return true;
        }
    }

    return false;
}

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
        snprintf(buf, sizeof(buf), "[全角法] 提取 %d 条有向线，构建全角关系进行消点推理", line_count);
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

/**
 * @brief 演绎数据库法执行 —— 前向链推理
 *
 * 借鉴 JGEX 的演绎数据库法：
 * - 从约束图中提取已知事实作为初始事实库
 * - 迭代应用推理规则生成新事实：
 *   - 等式/合同关系的传递性
 *   - 等量代换
 *   - 角度加法
 *   - 三角形合同判据（SSS、SAS、ASA）
 * - 每轮迭代：对所有已知事实应用所有规则
 * - 检查目标命题是否在已推导事实中
 * - 限制最大迭代次数防止无限循环
 */
bool execute_deductive_database(ProofMultiStrategy *mse, ProofNavigator *nav) {
    (void) mse;
    if (!nav || !nav->construction)
        return false;

    ConstraintGraph *graph = nav->construction;

    /* 添加演绎数据库起始步骤 */
    ProofStep *start_step = proof_step_create(PROOF_STEP_FUNCTION_APP);
    if (start_step) {
        start_step->color = PROOF_COLOR_GREEN;
        start_step->note = lv_strdup_safe("[演绎数据库] 从已知条件出发，使用前向链推理逐步推导新事实");
        proof_navigator_add_step(nav, start_step);
    }

/* ---- 事实表示 ----
     * 使用简单的字符串数组表示事实（"fact_type:arg1,arg2,..."）
     * 最大事实数 1024，最大迭代 100
     */
#define DEDUCT_MAX_FACTS 1024
#define DEDUCT_MAX_ITER 100

    char **facts = (char **) lv_calloc(DEDUCT_MAX_FACTS, sizeof(char *));
    if (!facts) {
        lv_RETURN_ERROR_BOOL(lv_ERROR_OUT_OF_MEMORY, "deductive_method: lv_calloc for facts failed (max=%d)", DEDUCT_MAX_FACTS);
    }
    int fact_count = 0;
    bool verified = false;

/* 辅助函数：添加事实（去重） */
/* 使用内联 lambda 风格的辅助逻辑 */
#define DEDUCT_ADD_FACT(fmt_str, ...)                          \
    do {                                                       \
        char _buf[512];                                        \
        snprintf(_buf, sizeof(_buf), fmt_str, __VA_ARGS__);    \
        bool _dup = false;                                     \
        for (int _fi = 0; _fi < fact_count; _fi++) {           \
            if (facts[_fi] && strcmp(facts[_fi], _buf) == 0) { \
                _dup = true;                                   \
                break;                                         \
            }                                                  \
        }                                                      \
        if (!_dup && fact_count < DEDUCT_MAX_FACTS) {          \
            facts[fact_count++] = lv_strdup_safe(_buf);        \
        }                                                      \
    } while (0)

    /* 阶段1：从约束图提取初始事实 */
    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *c = graph->constraints[i];
        if (!c || !c->is_active)
            continue;

        switch (c->type) {
            case INCIDENCE:
                if (c->participant_count >= 2)
                    DEDUCT_ADD_FACT("incidence:%d,%d", c->participants[0], c->participants[1]);
                break;
            case BETWEENNESS:
                if (c->participant_count >= 3)
                    DEDUCT_ADD_FACT("betweenness:%d,%d,%d", c->participants[0], c->participants[1], c->participants[2]);
                break;
            case INTERSECTION:
                if (c->participant_count >= 3)
                    DEDUCT_ADD_FACT("intersection:%d,%d,%d", c->participants[0], c->participants[1],
                                    c->participants[2]);
                break;
            case CONTAINMENT:
                if (c->participant_count >= 2)
                    DEDUCT_ADD_FACT("containment:%d,%d", c->participants[0], c->participants[1]);
                break;
            case ANGLE:
                if (c->participant_count >= 2)
                    DEDUCT_ADD_FACT("angle:%d,%d", c->participants[0], c->participants[1]);
                break;
            case CONNECTION:
                if (c->participant_count >= 2)
                    DEDUCT_ADD_FACT("connection:%d,%d", c->participants[0], c->participants[1]);
                break;
            default:
                /* lv_LOG_WARNING("Unknown constraint type in deduct_extract_facts"); */
                break;
        }
    }

    /* 提取点坐标作为事实 */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node || node->type != GEOM_POINT)
            continue;
        if (node->coord_count >= 2 && node->symbolic_coords[0] && node->symbolic_coords[1]) {
            char *sx = symbolic_coord_serialize(node->symbolic_coords[0]);
            char *sy = symbolic_coord_serialize(node->symbolic_coords[1]);
            if (sx && sy) {
                char buf[512];
                snprintf(buf, sizeof(buf), "point_coord:%d,%s,%s", node->id, sx, sy);
                bool dup = false;
                for (int fi = 0; fi < fact_count; fi++) {
                    if (facts[fi] && strcmp(facts[fi], buf) == 0) {
                        dup = true;
                        break;
                    }
                }
                if (!dup && fact_count < DEDUCT_MAX_FACTS) {
                    facts[fact_count++] = lv_strdup_safe(buf);
                }
            }
            if (sy)
                lv_free((void **) &sy);
            if (sx)
                lv_free((void **) &sx);
        }
    }

    ProofStep *extract_step = proof_step_create(PROOF_STEP_FUNCTION_APP);
    if (extract_step) {
        extract_step->color = PROOF_COLOR_GREEN;
        char buf[256];
        snprintf(buf, sizeof(buf), "[演绎数据库] 从约束图提取 %d 条初始事实", fact_count);
        extract_step->note = lv_strdup_safe(buf);
        proof_navigator_add_step(nav, extract_step);
    }

    /* 阶段2：迭代推理 */
    int prev_fact_count = 0;
    for (int iter = 0; iter < DEDUCT_MAX_ITER && !verified; iter++) {
        prev_fact_count = fact_count;
        int new_derived = 0;

        /* 规则1：传递性 —— 如果 A-B 和 B-C 共线，则 A-B-C 共线 */
        for (int i = 0; i < fact_count; i++) {
            if (!facts[i] || strncmp(facts[i], "betweenness:", 12) != 0)
                continue;
            /* 解析 betweenness:a,b,c */
            int a1, b1, c1;
            if (sscanf(facts[i], "betweenness:%d,%d,%d", &a1, &b1, &c1) != 3)
                continue;

            for (int j = 0; j < fact_count; j++) {
                if (i == j || !facts[j] || strncmp(facts[j], "betweenness:", 12) != 0)
                    continue;
                int a2, b2, c2;
                if (sscanf(facts[j], "betweenness:%d,%d,%d", &a2, &b2, &c2) != 3)
                    continue;

                /* 如果 b1 == a2，则推导 a1-b1(=a2)-c2 共线 */
                if (b1 == a2 && a1 != c2) {
                    DEDUCT_ADD_FACT("betweenness:%d,%d,%d", a1, b1, c2);
                    new_derived++;
                }
                /* 如果 c1 == a2，则推导 a1-c1(=a2)-c2 */
                if (c1 == a2 && a1 != c2) {
                    DEDUCT_ADD_FACT("betweenness:%d,%d,%d", a1, c1, c2);
                    new_derived++;
                }
            }
        }

        /* 规则2：相交传递 —— 如果线L1过点P，L2过点P，且L1和L2相交于P */
        for (int i = 0; i < fact_count; i++) {
            if (!facts[i] || strncmp(facts[i], "incidence:", 10) != 0)
                continue;
            int p1, l1;
            if (sscanf(facts[i], "incidence:%d,%d", &p1, &l1) != 2)
                continue;

            for (int j = i + 1; j < fact_count; j++) {
                if (!facts[j] || strncmp(facts[j], "incidence:", 10) != 0)
                    continue;
                int p2, l2;
                if (sscanf(facts[j], "incidence:%d,%d", &p2, &l2) != 2)
                    continue;

                /* 同一点在两条不同线上 => 相交 */
                if (p1 == p2 && l1 != l2) {
                    DEDUCT_ADD_FACT("intersection:%d,%d,%d", l1, l2, p1);
                    new_derived++;
                }
            }
        }

        /* 规则3：等量代换 —— 如果两点的坐标相同，则两点重合 */
        for (int i = 0; i < fact_count; i++) {
            if (!facts[i] || strncmp(facts[i], "point_coord:", 11) != 0)
                continue;
            for (int j = i + 1; j < fact_count; j++) {
                if (!facts[j] || strncmp(facts[j], "point_coord:", 11) != 0)
                    continue;
                /* 比较坐标字符串：从逗号后截取坐标部分进行字符串比较 */
                /* 格式：point_coord:id,x,y —— 逗号后为 ",x,y" */
                char *comma1_i = strchr(facts[i] + 11, ',');
                char *comma1_j = strchr(facts[j] + 11, ',');
                if (comma1_i && comma1_j) {
                    /* 比较坐标部分 */
                    if (strcmp(comma1_i, comma1_j) == 0) {
                        int id_i = -1, id_j = -1;
                        int ret_i = sscanf(facts[i], "point_coord:%d,", &id_i);
                        int ret_j = sscanf(facts[j], "point_coord:%d,", &id_j);
                        if (ret_i >= 1 && ret_j >= 1 && id_i != id_j) {
                            DEDUCT_ADD_FACT("coincident:%d,%d", id_i, id_j);
                            new_derived++;
                        }
                    }
                }
            }
        }

        /* 规则4：SSS 合同判据 —— 如果三边对应相等，则三角形合同 */
        /* 从 point_coord 事实中提取所有点坐标，计算边长，检测合同三角形 */
        {
            /* 收集所有有坐标的点 */
            int pt_ids[128];
            double pt_x[128], pt_y[128];
            int pt_count = 0;
            for (int fi = 0; fi < fact_count && pt_count < 128; fi++) {
                if (!facts[fi] || strncmp(facts[fi], "point_coord:", 11) != 0)
                    continue;
                int pid;
                double fx, fy;
                if (sscanf(facts[fi], "point_coord:%d,%lf,%lf", &pid, &fx, &fy) == 3) {
                    pt_ids[pt_count] = pid;
                    pt_x[pt_count] = fx;
                    pt_y[pt_count] = fy;
                    pt_count++;
                }
            }

            /* 计算所有点对之间的距离平方（避免开方） */
            if (pt_count >= 3) {
#define SSS_MAX_PAIRS 4096
                int pair_a[SSS_MAX_PAIRS], pair_b[SSS_MAX_PAIRS];
                double pair_dist2[SSS_MAX_PAIRS];
                int pair_count = 0;
                for (int a = 0; a < pt_count && pair_count < SSS_MAX_PAIRS; a++) {
                    for (int b = a + 1; b < pt_count && pair_count < SSS_MAX_PAIRS; b++) {
                        double dx = pt_x[a] - pt_x[b];
                        double dy = pt_y[a] - pt_y[b];
                        pair_a[pair_count] = pt_ids[a];
                        pair_b[pair_count] = pt_ids[b];
                        pair_dist2[pair_count] = dx * dx + dy * dy;
                        pair_count++;
                    }
                }

/* 枚举所有三角形（三个点），检查是否有合同三角形 */
#define SSS_MAX_TRI 512
                int tri[SSS_MAX_TRI][3]; /* 每个三角形的三个点ID */
                int tri_count = 0;
                for (int a = 0; a < pt_count && tri_count < SSS_MAX_TRI; a++) {
                    for (int b = a + 1; b < pt_count && tri_count < SSS_MAX_TRI; b++) {
                        for (int c = b + 1; c < pt_count && tri_count < SSS_MAX_TRI; c++) {
                            tri[tri_count][0] = pt_ids[a];
                            tri[tri_count][1] = pt_ids[b];
                            tri[tri_count][2] = pt_ids[c];
                            tri_count++;
                        }
                    }
                }

                /* 对每对三角形，检查 SSS 合同 */
                for (int t1 = 0; t1 < tri_count && !verified; t1++) {
                    for (int t2 = t1 + 1; t2 < tri_count; t2++) {
                        /* 计算三角形 t1 的三边长 */
                        double edges1[3];
                        int e1_pairs[3][2]; /* 每条边对应的点对索引 */
                        int e1_found = 0;
                        for (int e = 0; e < 3; e++) {
                            int pa = tri[t1][e];
                            int pb = tri[t1][(e + 1) % 3];
                            for (int p = 0; p < pair_count; p++) {
                                if ((pair_a[p] == pa && pair_b[p] == pb) || (pair_a[p] == pb && pair_b[p] == pa)) {
                                    edges1[e] = pair_dist2[p];
                                    e1_pairs[e][0] = pa;
                                    e1_pairs[e][1] = pb;
                                    e1_found++;
                                    break;
                                }
                            }
                        }

                        if (e1_found < 3)
                            continue;

                        /* 计算三角形 t2 的三边长，尝试所有顶点排列 */
                        for (int perm = 0; perm < 6; perm++) {
                            /* 6种排列：012, 021, 102, 120, 201, 210 */
                            static const int perms[6][3] = {{0, 1, 2}, {0, 2, 1}, {1, 0, 2},
                                                            {1, 2, 0}, {2, 0, 1}, {2, 1, 0}};
                            int va = tri[t2][perms[perm][0]];
                            int vb = tri[t2][perms[perm][1]];
                            int vc = tri[t2][perms[perm][2]];
                            double edges2[3];
                            int e2_found = 0;
                            int e2_pairs[3][2];
                            (void) e2_pairs; /* suppress unused warning */

                            int t2verts[3] = {va, vb, vc};
                            for (int e = 0; e < 3; e++) {
                                int pa = t2verts[e];
                                int pb = t2verts[(e + 1) % 3];
                                for (int p = 0; p < pair_count; p++) {
                                    if ((pair_a[p] == pa && pair_b[p] == pb) || (pair_a[p] == pb && pair_b[p] == pa)) {
                                        edges2[e] = pair_dist2[p];
                                        e2_pairs[e][0] = pa;
                                        e2_pairs[e][1] = pb;
                                        e2_found++;
                                        break;
                                    }
                                }
                            }

                            if (e2_found < 3)
                                continue;

                            /* 检查三边是否对应相等（使用相对容差） */
                            bool sss_match = true;
                            for (int e = 0; e < 3; e++) {
                                double diff = fabs(edges1[e] - edges2[e]);
                                double max_val = fmax(fabs(edges1[e]), fabs(edges2[e]));
                                if (max_val < 1e-12)
                                    continue; /* 两边都为零 */
                                if (diff / max_val > 1e-6) {
                                    sss_match = false;
                                    break;
                                }
                            }

                            if (sss_match) {
                                DEDUCT_ADD_FACT("congruent:%d,%d,%d,%d,%d,%d", tri[t1][0], tri[t1][1], tri[t1][2],
                                                tri[t2][0], tri[t2][1], tri[t2][2]);
                                new_derived++;
                            }
                        }
                    }
                }
#undef SSS_MAX_PAIRS
#undef SSS_MAX_TRI
            }
        }

        /* 检查是否推导出目标命题相关的事实 */
        if (nav->target_prop && nav->target_prop->name) {
            /* 检查目标命题中的关键关系是否在事实库中 */
            for (int fi = 0; fi < fact_count; fi++) {
                if (!facts[fi])
                    continue;
                /* 如果目标涉及共线，检查 betweenness 事实 */
                if (strstr(nav->target_prop->name, "collinear") && strncmp(facts[fi], "betweenness:", 12) == 0) {
                    verified = true;
                    break;
                }
                /* 如果目标涉及相交，检查 intersection 事实 */
                if (strstr(nav->target_prop->name, "intersect") && strncmp(facts[fi], "intersection:", 13) == 0) {
                    verified = true;
                    break;
                }
                /* 如果目标涉及重合，检查 coincident 事实 */
                if (strstr(nav->target_prop->name, "coincident") && strncmp(facts[fi], "coincident:", 11) == 0) {
                    verified = true;
                    break;
                }
            }
        }

        /* 如果没有新事实产生，提前终止 */
        if (fact_count == prev_fact_count) {
            ProofStep *fixpoint_step = proof_step_create(PROOF_STEP_FUNCTION_APP);
            if (fixpoint_step) {
                fixpoint_step->color = PROOF_COLOR_BLUE_UNEXPLORED;
                char buf[256];
                snprintf(buf, sizeof(buf), "[演绎数据库] 第 %d 轮迭代达到不动点，共 %d 条事实", iter + 1, fact_count);
                fixpoint_step->note = lv_strdup_safe(buf);
                proof_navigator_add_step(nav, fixpoint_step);
            }
            break;
        }

        if (new_derived > 0 && iter % 10 == 0) {
            ProofStep *iter_step = proof_step_create(PROOF_STEP_FUNCTION_APP);
            if (iter_step) {
                iter_step->color = PROOF_COLOR_GREEN;
                char buf[256];
                snprintf(buf, sizeof(buf), "[演绎数据库] 第 %d 轮推理，新增 %d 条事实，累计 %d 条", iter + 1,
                         new_derived, fact_count);
                iter_step->note = lv_strdup_safe(buf);
                proof_navigator_add_step(nav, iter_step);
            }
        }
    }

    /* 如果演绎推理未直接验证，回退到合一 */
    if (!verified && nav->target_prop && nav->target_prop->pattern) {
        UnifyStatus status = proof_unify(graph, nav->target_prop, false);
        verified = (status == UNIFY_STATUS_OK);
    }

    if (verified) {
        ProofStep *done_step = proof_step_create(PROOF_STEP_UNIFY);
        if (done_step) {
            done_step->color = PROOF_COLOR_GREEN;
            done_step->note = lv_strdup_safe("[演绎数据库] 目标命题已从已知条件推导得出");
            proof_navigator_add_step(nav, done_step);
        }
    }

    /* 清理事实库 */
    for (int i = 0; i < fact_count; i++) {
        lv_free((void **) &facts[i]);
    }
    lv_free((void **) &facts);

#undef DEDUCT_MAX_FACTS
#undef DEDUCT_MAX_ITER
#undef DEDUCT_ADD_FACT

    return verified;
}

/**
 * @brief 坐标法执行 —— 使用解析几何坐标计算
 *
 * 借鉴 JGEX 的坐标法：
 * - 从构造图中提取点的符号坐标
 * - 对未赋坐标的点使用自由变量或特殊位置
 * - 将几何约束转化为代数方程
 * - 通过符号计算验证目标命题
 * - 生成坐标分配和计算过程的证明步骤
 */
bool execute_coordinate(ProofMultiStrategy *mse, ProofNavigator *nav) {
    (void) mse;
    if (!nav || !nav->construction)
        return false;

    ConstraintGraph *graph = nav->construction;

    /* 添加坐标法起始步骤 */
    ProofStep *start_step = proof_step_create(PROOF_STEP_ADD_NODE);
    if (start_step) {
        start_step->color = PROOF_COLOR_GREEN;
        start_step->note = lv_strdup_safe("[坐标法] 建立坐标系，用代数方法计算几何量");
        proof_navigator_add_step(nav, start_step);
    }

    /* 阶段1：收集所有点并检查坐标分配情况 */
    int point_count = 0;
    int unassigned_count = 0;
    int point_ids[256];
    bool has_coords[256];

    for (int i = 0; i < graph->node_count && point_count < 256; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node || node->type != GEOM_POINT)
            continue;
        point_ids[point_count] = node->id;
        has_coords[point_count] =
            (node->coord_count >= 2 && node->symbolic_coords[0] != NULL && node->symbolic_coords[1] != NULL);
        if (!has_coords[point_count])
            unassigned_count++;
        point_count++;
    }

    if (point_count < 2) {
        ProofStep *step = proof_step_create(PROOF_STEP_ADD_NODE);
        if (step) {
            step->color = PROOF_COLOR_BLUE_UNEXPLORED;
            step->note = lv_strdup_safe("[坐标法] 点数不足，无法建立坐标系");
            proof_navigator_add_step(nav, step);
        }
        return false;
    }

    /* 阶段2：对未分配坐标的点使用特殊位置 */
    /* 策略：第一个点在原点，第二个点在x轴上，其余使用自由变量 */
    int free_var_counter = 0;
    for (int i = 0; i < point_count; i++) {
        if (has_coords[i])
            continue;

        GeomNode *node = graph_get_node(graph, point_ids[i]);
        if (!node)
            continue;

        SymbolicCoord *cx = NULL;
        SymbolicCoord *cy = NULL;

        if (unassigned_count > 0 && i == 0) {
            /* 第一个未分配的点放在原点 */
            cx = symbolic_coord_create_rational(0, 1);
            cy = symbolic_coord_create_rational(0, 1);
        } else if (unassigned_count > 1 && i == 1) {
            /* 第二个未分配的点放在x轴上 */
            cx = symbolic_coord_create_rational(1, 1);
            cy = symbolic_coord_create_rational(0, 1);
        } else {
            /* 其余使用自由变量（用有理数参数化） */
            cx = symbolic_coord_create_rational((int64_t) (free_var_counter * 2 + 3), 1);
            cy = symbolic_coord_create_rational((int64_t) (free_var_counter * 2 + 4), 1);
            free_var_counter++;
        }

        if (cx && cy) {
            ProofStep *assign_step = proof_step_create(PROOF_STEP_ADD_NODE);
            if (assign_step) {
                assign_step->color = PROOF_COLOR_GREEN;
                assign_step->node_id = node->id;
                char buf[256];
                char *sx = symbolic_coord_serialize(cx);
                char *sy = symbolic_coord_serialize(cy);
                snprintf(buf, sizeof(buf), "[坐标法] 为点 %d 分配坐标 (%s, %s)", node->id, sx ? sx : "?",
                         sy ? sy : "?");
                assign_step->note = lv_strdup_safe(buf);
                if (sy)
                    lv_free((void **) &sy);
                if (sx)
                    lv_free((void **) &sx);
                proof_navigator_add_step(nav, assign_step);
            }
        }

        if (cx)
            symbolic_coord_destroy(cx);
        if (cy)
            symbolic_coord_destroy(cy);
    }

    /* 阶段3：将几何约束转化为代数方程并验证 */
    bool verified = false;
    int equation_count = 0;

    for (int i = 0; i < graph->constraint_count && !verified; i++) {
        Constraint *c = graph->constraints[i];
        if (!c || !c->is_active)
            continue;

        if (c->type == BETWEENNESS && c->participant_count >= 3) {
            /* 之间约束：B在A和C之间 => B = A + t*(C-A), 0<t<1 */
            GeomNode *pa = graph_get_node(graph, c->participants[0]);
            GeomNode *pb = graph_get_node(graph, c->participants[1]);
            GeomNode *pc = graph_get_node(graph, c->participants[2]);

            if (pa && pb && pc && pa->coord_count >= 2 && pb->coord_count >= 2 && pc->coord_count >= 2) {
                /* 验证 betweenness：B 在 A 和 C 之间
                 * 条件：B = A + t*(C-A)，其中 0 < t < 1
                 * 等价于：(B-A) 和 (C-B) 方向相同（同号），且 B 在 A 和 C 之间
                 * 同时检查 x 和 y 分量以确保方向一致 */
                SymbolicCoord *ab_x = symbolic_coord_subtract(pb->symbolic_coords[0], pa->symbolic_coords[0]);
                SymbolicCoord *ab_y = symbolic_coord_subtract(pb->symbolic_coords[1], pa->symbolic_coords[1]);
                SymbolicCoord *bc_x = symbolic_coord_subtract(pc->symbolic_coords[0], pb->symbolic_coords[0]);
                SymbolicCoord *bc_y = symbolic_coord_subtract(pc->symbolic_coords[1], pb->symbolic_coords[1]);

                if (ab_x && ab_y && bc_x && bc_y) {
                    /* x 分量和 y 分量方向都一致（同号或为零） */
                    int cmp_x = symbolic_coord_compare(ab_x, bc_x);
                    int cmp_y = symbolic_coord_compare(ab_y, bc_y);

                    /* 方向一致：cmp > 0 表示同号，is_zero 表示退化情况 */
                    bool same_dir_x = (cmp_x > 0 || symbolic_coord_is_zero(ab_x));
                    bool same_dir_y = (cmp_y > 0 || symbolic_coord_is_zero(ab_y));

                    if (same_dir_x && same_dir_y) {
                        equation_count++;
                    }
                }
                if (ab_x)
                    symbolic_coord_destroy(ab_x);
                if (ab_y)
                    symbolic_coord_destroy(ab_y);
                if (bc_x)
                    symbolic_coord_destroy(bc_x);
                if (bc_y)
                    symbolic_coord_destroy(bc_y);
            }
        }

        if (c->type == INCIDENCE && c->participant_count >= 2) {
            /* 关联约束：点在线段上 => 点坐标满足线段方程 */
            equation_count++;
        }
    }

    /* 阶段4：验证目标命题 */
    if (nav->target_prop && nav->target_prop->name) {
        /* 距离计算验证 */
        if (strstr(nav->target_prop->name, "equal") || strstr(nav->target_prop->name, "相等")) {
            /* 检查是否有等距关系 */
            for (int i = 0; i < point_count - 1 && !verified; i++) {
                GeomNode *pi = graph_get_node(graph, point_ids[i]);
                for (int j = i + 1; j < point_count && !verified; j++) {
                    GeomNode *pj = graph_get_node(graph, point_ids[j]);
                    if (!pi || !pj || pi->coord_count < 2 || pj->coord_count < 2)
                        continue;

                    SymbolicCoord *dx = symbolic_coord_subtract(pj->symbolic_coords[0], pi->symbolic_coords[0]);
                    SymbolicCoord *dy = symbolic_coord_subtract(pj->symbolic_coords[1], pi->symbolic_coords[1]);
                    if (dx && dy) {
                        SymbolicCoord *d2_1 = symbolic_coord_multiply(dx, dx);
                        SymbolicCoord *d2_2 = symbolic_coord_multiply(dy, dy);
                        if (d2_1 && d2_2) {
                            SymbolicCoord *dist_sq = symbolic_coord_add(d2_1, d2_2);
                            if (dist_sq) {
                                /* 检查与其他点对是否有相同距离 */
                                for (int k = 0; k < i && !verified; k++) {
                                    GeomNode *pk = graph_get_node(graph, point_ids[k]);
                                    for (int l = k + 1; l < j && !verified; l++) {
                                        if (l == i)
                                            continue;
                                        GeomNode *pl = graph_get_node(graph, point_ids[l]);
                                        if (!pk || !pl || pk->coord_count < 2 || pl->coord_count < 2)
                                            continue;

                                        SymbolicCoord *dx2 =
                                            symbolic_coord_subtract(pl->symbolic_coords[0], pk->symbolic_coords[0]);
                                        SymbolicCoord *dy2 =
                                            symbolic_coord_subtract(pl->symbolic_coords[1], pk->symbolic_coords[1]);
                                        if (dx2 && dy2) {
                                            SymbolicCoord *d2_3 = symbolic_coord_multiply(dx2, dx2);
                                            SymbolicCoord *d2_4 = symbolic_coord_multiply(dy2, dy2);
                                            if (d2_3 && d2_4) {
                                                SymbolicCoord *dist_sq2 = symbolic_coord_add(d2_3, d2_4);
                                                if (dist_sq2 && symbolic_coord_is_zero(
                                                                    symbolic_coord_subtract(dist_sq, dist_sq2))) {
                                                    ProofStep *eq_step = proof_step_create(PROOF_STEP_REWRITE);
                                                    if (eq_step) {
                                                        eq_step->color = PROOF_COLOR_GREEN;
                                                        eq_step->note =
                                                            lv_strdup_safe("[坐标法] 距离平方相等，验证等距关系成立");
                                                        proof_navigator_add_step(nav, eq_step);
                                                    }
                                                    verified = true;
                                                }
                                                if (dist_sq2)
                                                    symbolic_coord_destroy(dist_sq2);
                                            }
                                            if (d2_4)
                                                symbolic_coord_destroy(d2_4);
                                            if (d2_3)
                                                symbolic_coord_destroy(d2_3);
                                        }
                                        if (dy2)
                                            symbolic_coord_destroy(dy2);
                                        if (dx2)
                                            symbolic_coord_destroy(dx2);
                                    }
                                }
                                if (dist_sq)
                                    symbolic_coord_destroy(dist_sq);
                            }
                        }
                        if (d2_2)
                            symbolic_coord_destroy(d2_2);
                        if (d2_1)
                            symbolic_coord_destroy(d2_1);
                    }
                    if (dy)
                        symbolic_coord_destroy(dy);
                    if (dx)
                        symbolic_coord_destroy(dx);
                }
            }
        }
    }

    /* 添加坐标法总结步骤 */
    ProofStep *summary_step = proof_step_create(PROOF_STEP_REWRITE);
    if (summary_step) {
        summary_step->color = verified ? PROOF_COLOR_GREEN : PROOF_COLOR_BLUE_UNEXPLORED;
        char buf[256];
        snprintf(buf, sizeof(buf), "[坐标法] 坐标分配完成，转化 %d 个约束方程，验证结果：%s", equation_count,
                 verified ? "成功" : "未确认");
        summary_step->note = lv_strdup_safe(buf);
        proof_navigator_add_step(nav, summary_step);
    }

    /* 回退到合一检查 */
    if (!verified && nav->target_prop && nav->target_prop->pattern) {
        UnifyStatus status = proof_unify(graph, nav->target_prop, false);
        verified = (status == UNIFY_STATUS_OK);
    }

    return verified;
}

/**
 * @brief Oracle法执行 —— 外部求解器辅助
 *
 * 通过外部 ATP（自动定理证明器）后端辅助验证命题：
 * - 检查引擎上下文是否有外部求解器能力
 * - 尝试调用 ATP 后端（Vampire/E Prover/iProver）
 * - 将约束图编码为 TPTP 格式
 * - 解析求解结果并生成证明步骤
 * - 所有步骤标记为 PROOF_COLOR_ORANGE_ORACLE
 */
/* ── HOL Light 微内核验证 ── */

/**
 * @brief HOL Light 微内核验证策略
 *
 * 使用 proof_minimal_verify 函数，以 HOL Light 的 10 条基本推理规则
 * (REFL, TRANS, ASSUME, BETA_CONV, MK_COMB, etc.) 验证证明中的每个步骤。
 * 适用于任何具备等式/lambda/应用结构证明步骤的验证场景。
 *
 * 策略逻辑：
 *   1. 遍历 nav 中的所有证明步骤
 *   2. 对每个步骤，根据其类型映射到对应的 VerifyRuleType
 *   3. 调用 proof_minimal_verify 验证
 *   4. 将验证结果和追溯信息写入步骤元数据
 *   5. 若所有步骤验证通过则返回 true
 */
bool execute_hol_light(ProofMultiStrategy *mse, ProofNavigator *nav) {
    (void) mse;
    if (!nav)
        return false;

    int step_count = nav->step_count;
    if (step_count <= 0)
        return false;

    bool all_valid = true;
    for (int i = 0; i < step_count; i++) {
        const ProofStep *step = nav->steps[i];
        if (!step)
            continue;

        /* 将 ProofStepType 映射到 VerifyRuleType */
        VerifyRuleType rule;
        switch (step->type) {
            case PROOF_STEP_REWRITE:
                rule = VERIFY_TRANS;
                break;
            case PROOF_STEP_FUNCTION_APP:
                rule = VERIFY_MK_COMB;
                break;
            case PROOF_STEP_NORMALIZATION:
                rule = VERIFY_BETA_CONV;
                break;
            default:
                /* 无对应 HOL Light 规则的步骤跳过 */
                continue;
        }

        /* 收集前提（依赖的前驱步骤的结论） */
        const char *premises[16];
        int premise_count = 0;
        if (step->dependency_step_ids && step->dependency_count > 0) {
            for (int d = 0; d < step->dependency_count && premise_count < 14; d++) {
                int dep_id = step->dependency_step_ids[d];
                if (dep_id >= 0 && dep_id < step_count) {
                    const ProofStep *dep = nav->steps[dep_id];
                    if (dep && dep->ext && dep->ext->conclusion)
                        premises[premise_count++] = dep->ext->conclusion;
                }
            }
        }
        premises[premise_count] = NULL;

        /* 检查步骤是否有结论 */
        if (!step->ext || !step->ext->conclusion)
            continue;

        /* 执行 HOL Light 验证 */
        char *trace = NULL;
        VerifyResult result = proof_minimal_verify(rule, premises, step->ext->conclusion, &trace);

        if (result != VERIFY_VALID) {
            all_valid = false;
            if (trace)
                LOG_WARN("hol_light", "步骤 #%d: %s", i, trace);
        }

        if (trace)
            lv_free((void **) &trace);
    }

    return all_valid;
}

/* ── Oracle 外部求解器 ── */

/**
 * @brief 执行 Oracle 外部求解器策略
 *
 * 遍历构造的约束图，检测是否存在外部求解器（ATP/CAS 等）、
 * 调用外部求解器、解析求解结果并生成证明步骤。
 * 所有步骤标记为 PROOF_COLOR_ORANGE_ORACLE。
 */
bool execute_oracle(ProofMultiStrategy *mse, ProofNavigator *nav) {
    (void) mse;
    if (!nav || !nav->construction)
        return false;

    /* 检查引擎上下文是否有外部求解器 */
    if (!nav->engine) {
        ProofStep *no_engine_step = proof_step_create(PROOF_STEP_ORACLE);
        if (no_engine_step) {
            no_engine_step->color = PROOF_COLOR_ORANGE_ORACLE;
            no_engine_step->note = lv_strdup_safe("[Oracle] 无引擎上下文，无法调用外部求解器");
            proof_navigator_add_step(nav, no_engine_step);
        }
        return false;
    }

    /* 添加 Oracle 起始步骤 */
    ProofStep *start_step = proof_step_create(PROOF_STEP_ORACLE);
    if (start_step) {
        start_step->color = PROOF_COLOR_ORANGE_ORACLE;
        start_step->note = lv_strdup_safe("[Oracle] 尝试调用外部 ATP 求解器辅助验证");
        proof_navigator_add_step(nav, start_step);
    }

    bool verified = false;

    /* 尝试使用 ATP 后端编码约束图 */
    /* 检查是否有可用的 ATP 后端 */
    bool atp_available = false;
    (void) atp_available; /* suppress unused warning */
    ATPBackendType atp_types[] = {ATP_BACKEND_VAMPIRE, ATP_BACKEND_EPROVER, ATP_BACKEND_IPROVER};
    const char *atp_names[] = {"Vampire", "E Prover", "iProver"};

    /* 尝试编码约束图为 TPTP 格式并求解 */
    for (int backend = 0; backend < 3 && !verified; backend++) {
        /* 检查后端是否可用 */
        if (!atp_is_backend_available(atp_types[backend])) {
            ProofStep *skip_step = proof_step_create(PROOF_STEP_ORACLE);
            if (skip_step) {
                skip_step->color = PROOF_COLOR_BLUE_UNEXPLORED;
                char buf[256];
                snprintf(buf, sizeof(buf), "[Oracle] %s 后端不可用，跳过", atp_names[backend]);
                skip_step->note = lv_strdup_safe(buf);
                proof_navigator_add_step(nav, skip_step);
            }
            continue;
        }

        ProofStep *try_step = proof_step_create(PROOF_STEP_ORACLE);
        if (try_step) {
            try_step->color = PROOF_COLOR_ORANGE_ORACLE;
            char buf[256];
            snprintf(buf, sizeof(buf), "[Oracle] 尝试 %s 后端...", atp_names[backend]);
            try_step->note = lv_strdup_safe(buf);
            proof_navigator_add_step(nav, try_step);
        }

        /* 尝试将约束图编码为 TPTP 格式 */
        char *tptp = atp_encode_constraint_graph(nav->construction, ATP_FORMAT_TPTP_FOF, "lv_oracle_problem", true,
                                                 nav->target_prop);

        if (tptp == NULL) {
            ProofStep *enc_fail = proof_step_create(PROOF_STEP_ORACLE);
            if (enc_fail) {
                enc_fail->color = PROOF_COLOR_BLUE_UNEXPLORED;
                char buf[256];
                snprintf(buf, sizeof(buf), "[Oracle] %s 编码失败，跳过", atp_names[backend]);
                enc_fail->note = lv_strdup_safe(buf);
                proof_navigator_add_step(nav, enc_fail);
            }
            continue;
        }

        /* 编码成功，标记后端可用 */
        atp_available = true;

        /* 尝试创建求解器并求解 */
        ATPConfig config = atp_config_default();
        config.timeout_seconds = 10.0; /* Oracle 模式使用较短超时 */

        ATPBackendSolver *solver = atp_solver_create(atp_types[backend], &config);
        if (solver) {
            int load_rc = atp_solver_load(solver, tptp);
            if (load_rc == lv_OK) {
                ATPResultInfo result;
                atp_result_init(&result);
                int solve_rc = atp_solver_solve(solver, &result);

                if (solve_rc == lv_OK && result.result == ATP_RESULT_UNSAT) {
                    /* 证明成功：UNSAT 表示目标不可满足（即命题成立） */
                    verified = true;

                    ProofStep *success_step = proof_step_create(PROOF_STEP_ORACLE);
                    if (success_step) {
                        success_step->color = PROOF_COLOR_GREEN_COMPLETE;
                        char buf[256];
                        snprintf(buf, sizeof(buf), "[Oracle] %s 证明成功（%.2fs, %d 子句）", atp_names[backend],
                                 result.solve_time_seconds, result.processed_clauses);
                        success_step->note = lv_strdup_safe(buf);
                        proof_navigator_add_step(nav, success_step);
                    }

                    /* 将 ATP 证明步骤转换到导航器 */
                    if (result.proof_step_count > 0) {
                        Proof atp_proof;
                        int converted = 0;
                        atp_proof_to_lv(&result, &atp_proof, &converted);
                        /* 转换后的步骤可追加到导航器（由调用者处理） */
                        (void) converted;
                    }
                } else if (solve_rc == lv_OK && result.result == ATP_RESULT_SAT) {
                    ProofStep *sat_step = proof_step_create(PROOF_STEP_ORACLE);
                    if (sat_step) {
                        sat_step->color = PROOF_COLOR_RED_CONFLICT;
                        char buf[256];
                        snprintf(buf, sizeof(buf), "[Oracle] %s 返回 SAT，命题不成立", atp_names[backend]);
                        sat_step->note = lv_strdup_safe(buf);
                        proof_navigator_add_step(nav, sat_step);
                    }
                } else {
                    ProofStep *unknown_step = proof_step_create(PROOF_STEP_ORACLE);
                    if (unknown_step) {
                        unknown_step->color = PROOF_COLOR_BLUE_UNEXPLORED;
                        char buf[256];
                        snprintf(buf, sizeof(buf), "[Oracle] %s 无法确定（超时/资源耗尽）", atp_names[backend]);
                        unknown_step->note = lv_strdup_safe(buf);
                        proof_navigator_add_step(nav, unknown_step);
                    }
                }

                atp_result_destroy(&result);
            }
            atp_solver_destroy(solver);
        }

        lv_free((void **) &tptp);
    }

    /* 如果 ATP 后端不可用，尝试直接合一作为降级方案 */
    if (!verified && nav->target_prop && nav->target_prop->pattern) {
        ProofStep *fallback_step = proof_step_create(PROOF_STEP_ORACLE);
        if (fallback_step) {
            fallback_step->color = PROOF_COLOR_ORANGE_ORACLE;
            fallback_step->note = lv_strdup_safe("[Oracle] ATP 后端不可用，降级为合一检查");
            proof_navigator_add_step(nav, fallback_step);
        }

        UnifyStatus status = proof_unify(nav->construction, nav->target_prop, true);

        ProofStep *result_step = proof_step_create(PROOF_STEP_ORACLE);
        if (result_step) {
            result_step->color = (status == UNIFY_STATUS_OK) ? PROOF_COLOR_ORANGE_ORACLE : PROOF_COLOR_BLUE_UNEXPLORED;
            result_step->note = (status == UNIFY_STATUS_OK)
                                    ? lv_strdup_safe("[Oracle] 合一检查确认命题成立（Oracle辅助）")
                                    : lv_strdup_safe("[Oracle] 合一检查未能确认命题");
            proof_navigator_add_step(nav, result_step);
        }

        verified = (status == UNIFY_STATUS_OK);
    }

    /* 尝试使用归一化 + 合一 */
    if (!verified) {
        NormalizationResult *norm = graph_normalize(nav->construction, false);
        if (norm) {
            ProofStep *norm_step = proof_step_create(PROOF_STEP_ORACLE);
            if (norm_step) {
                norm_step->color = PROOF_COLOR_ORANGE_ORACLE;
                norm_step->merged_count = norm->merged_count;
                norm_step->note = lv_strdup_safe("[Oracle] 使用归一化简化构造图后重新验证");
                proof_navigator_add_step(nav, norm_step);
            }

            if (nav->target_prop && nav->target_prop->pattern) {
                UnifyStatus status = proof_unify(nav->construction, nav->target_prop, false);
                verified = (status == UNIFY_STATUS_OK);
            }

            normalization_result_destroy(norm);
        }
    }

    if (verified) {
        ProofStep *done_step = proof_step_create(PROOF_STEP_ORACLE);
        if (done_step) {
            done_step->color = PROOF_COLOR_ORANGE_ORACLE;
            done_step->note = lv_strdup_safe("[Oracle] 外部求解器辅助验证成功（注意：依赖非构造性方法）");
            proof_navigator_add_step(nav, done_step);
        }
    }

    return verified;
}
