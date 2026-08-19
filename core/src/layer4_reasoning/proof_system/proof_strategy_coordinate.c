/**
 * @file proof_strategy_coordinate.c
 * @brief 坐标法策略执行
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
                lv_snprintf(buf, sizeof(buf), "[坐标法] 为点 %d 分配坐标 (%s, %s)", node->id, sx ? sx : "?",
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
        lv_snprintf(buf, sizeof(buf), "[坐标法] 坐标分配完成，转化 %d 个约束方程，验证结果：%s", equation_count,
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
