/**
 * @file rewrite_vf2.c
 * @brief VF2 子图同构匹配
 *
 * @details 拆分子模块（Lv-00 v3.3.0+）。
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lv/rewrite.h"
#include "lv/constraint_graph.h"
#include "debug.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "mpz_poly.h"

/* ── 前向声明 ── */
uint32_t compute_graph_hash(ConstraintGraph *graph);

/* ── VF2 递归深度限制 ── */
#ifndef REWRITE_VF2_MAX_DEPTH
#define REWRITE_VF2_MAX_DEPTH 64
#endif

static bool detect_rewrite_loop(ConstraintGraph *graph, int *history_hashes, int history_count) {
    uint32_t current_hash = compute_graph_hash(graph);
    for (int i = 0; i < history_count; i++) {
        if (history_hashes[i] == current_hash) {
            return true;
        }
    }
    return false;
}

/* ===========================================================================
 * VF2 子图同构匹配算法
 *
 * VF2 (Vento-Foggia 2) 是一种高效的子图同构验证算法，通过递归搜索
 * 和可行性剪枝来检测模式图是否与目标图的某个子图同构。
 * ===========================================================================
 */

/* 初始化 VF2 匹配状态 */
static void vf2_state_init(VF2State *state, int pattern_size, int target_size) {
    state->pattern_size = pattern_size;
    state->target_size = target_size;
    state->core_count = 0;

    state->core_1 = lv_malloc((size_t)pattern_size * sizeof(int));
    state->core_2 = lv_malloc((size_t)target_size * sizeof(int));
    state->in_1 = lv_malloc((size_t)pattern_size * sizeof(int));
    state->out_1 = lv_malloc((size_t)pattern_size * sizeof(int));
    state->in_2 = lv_malloc((size_t)target_size * sizeof(int));
    state->out_2 = lv_malloc((size_t)target_size * sizeof(int));

    if (state->in_1) memset(state->in_1, 0, (size_t)pattern_size * sizeof(int));
    if (state->out_1) memset(state->out_1, 0, (size_t)pattern_size * sizeof(int));
    if (state->in_2) memset(state->in_2, 0, (size_t)target_size * sizeof(int));
    if (state->out_2) memset(state->out_2, 0, (size_t)target_size * sizeof(int));

    for (int i = 0; i < pattern_size; i++)
        state->core_1[i] = -1;
    for (int i = 0; i < target_size; i++)
        state->core_2[i] = -1;

    /* 初始化 in/out 集合 */
    int initial_capacity = target_size > 0 ? target_size : 8;
    state->in_set = lv_malloc((size_t)initial_capacity * sizeof(int));
    state->out_set = lv_malloc((size_t)initial_capacity * sizeof(int));
    state->in_count = 0;
    state->out_count = 0;
    state->in_capacity = initial_capacity;
    state->out_capacity = initial_capacity;
}

/* 销毁 VF2 匹配状态，释放内存 */
static void vf2_state_destroy(VF2State *state) {
    lv_free((void**)&state->core_1);
    lv_free((void**)&state->core_2);
    lv_free((void**)&state->in_1);
    lv_free((void**)&state->out_1);
    lv_free((void**)&state->in_2);
    lv_free((void**)&state->out_2);
    lv_free((void**)&state->in_set);
    lv_free((void**)&state->out_set);
    state->core_1 = state->core_2 = NULL;
    state->in_1 = state->out_1 = NULL;
    state->in_2 = state->out_2 = NULL;
    state->in_set = state->out_set = NULL;
}

/* 检查模式图中节点 p 的邻居是否与目标图中节点 t 的邻居兼容。
 * 对于已映射的邻居，验证对应关系的一致性。
 * 对于 POINT 节点，使用 symbolic_coord_compare() 进行符号坐标判等。 */
static bool vf2_feasible(VF2State *state, int p, int t,
                          ConstraintGraph *pattern_graph,
                          ConstraintGraph *target_graph,
                          bool local_equivalence_tolerant)
{
    GeomNode *pn = pattern_graph->nodes[p];
    GeomNode *tn = target_graph->nodes[t];

    /* 节点类型必须匹配 */
    if (pn->type != tn->type)
        return false;

    /* 增强语义可行性检查：信任颜色和 Light Orange 子类型 */
    if (pn->trust != tn->trust)
        return false;
    if (pn->lo_subtype != tn->lo_subtype)
        return false;

    /* PORT 节点语义检查：端口类型必须一致 */
    if (pn->type == GEOM_PORT) {
        if (!pn->data.port || !tn->data.port)
            return false;
        if (pn->data.port->type != tn->data.port->type)
            return false;
    }

    /* FUNCTION_BLOCK 节点语义检查：确定性状态必须一致 */
    if (pn->type == GEOM_FUNCTION_BLOCK) {
        if (pn->data.func_block.determinism_state !=
            tn->data.func_block.determinism_state)
            return false;
    }

    /* 对于 POINT 节点，在局部等价容忍模式下检查符号坐标 */
    if (pn->type == GEOM_POINT && local_equivalence_tolerant) {
        if (pn->coord_count != tn->coord_count)
            return false;
        for (int c = 0; c < pn->coord_count; c++) {
            if (symbolic_coord_compare(pn->symbolic_coords[c],
                                       tn->symbolic_coords[c]) != 0) {
                return false;
            }
        }
    }

    /* 对于 LINE_SEGMENT 节点，在局部等价容忍模式下比较端点坐标。
     * LINE_SEGMENT 的两个端点通过 INCIDENCE 约束关联。
     * 分别收集模式图和目标图中线段的两个端点坐标，进行排序后比对，
     * 以支持端点顺序无关的等价判断。 */
    if (pn->type == GEOM_LINE_SEGMENT && local_equivalence_tolerant) {
        /* 收集模式图中 p 的两个端点的符号坐标 */
        SymbolicCoord *p_endpoint_coords[2] = { NULL, NULL };
        int p_ep_count = 0;
        for (int c = 0; c < pattern_graph->constraint_count; c++) {
            Constraint *pc = pattern_graph->constraints[c];
            if (pc->type == INCIDENCE && pc->participant_count == 2) {
                for (int k = 0; k < 2; k++) {
                    if (pc->participants[k] == p) {
                        int ep_idx = pc->participants[1 - k];
                        if (ep_idx >= 0 && ep_idx < pattern_graph->node_count) {
                            GeomNode *ep_node = pattern_graph->nodes[ep_idx];
                            if (ep_node && ep_node->type == GEOM_POINT &&
                                ep_node->coord_count > 0 && ep_node->symbolic_coords) {
                                p_endpoint_coords[p_ep_count++] = ep_node->symbolic_coords[0];
                                if (p_ep_count >= 2) break;
                            }
                        }
                    }
                }
            }
            if (p_ep_count >= 2) break;
        }
        /* 收集目标图中 t 的两个端点的符号坐标 */
        SymbolicCoord *t_endpoint_coords[2] = { NULL, NULL };
        int t_ep_count = 0;
        for (int c = 0; c < target_graph->constraint_count; c++) {
            Constraint *tc = target_graph->constraints[c];
            if (tc->type == INCIDENCE && tc->participant_count == 2) {
                for (int k = 0; k < 2; k++) {
                    if (tc->participants[k] == t) {
                        int ep_idx = tc->participants[1 - k];
                        GeomNode *ep_node = graph_get_node(target_graph, ep_idx);
                        if (ep_node && ep_node->type == GEOM_POINT &&
                            ep_node->coord_count > 0 && ep_node->symbolic_coords) {
                            t_endpoint_coords[t_ep_count++] = ep_node->symbolic_coords[0];
                            if (t_ep_count >= 2) break;
                        }
                    }
                }
            }
            if (t_ep_count >= 2) break;
        }
        /* 如果两端点坐标都收集到了，进行排序后比对 */
        if (p_ep_count == 2 && t_ep_count == 2) {
            /* 排序模式图端点坐标 */
            if (symbolic_coord_compare(p_endpoint_coords[0], p_endpoint_coords[1]) > 0) {
                SymbolicCoord *tmp = p_endpoint_coords[0];
                p_endpoint_coords[0] = p_endpoint_coords[1];
                p_endpoint_coords[1] = tmp;
            }
            /* 排序目标图端点坐标 */
            if (symbolic_coord_compare(t_endpoint_coords[0], t_endpoint_coords[1]) > 0) {
                SymbolicCoord *tmp = t_endpoint_coords[0];
                t_endpoint_coords[0] = t_endpoint_coords[1];
                t_endpoint_coords[1] = tmp;
            }
            /* 逐对比较排序后的端点坐标 */
            if (symbolic_coord_compare(p_endpoint_coords[0], t_endpoint_coords[0]) != 0 ||
                symbolic_coord_compare(p_endpoint_coords[1], t_endpoint_coords[1]) != 0) {
                return false;
            }
        }
    }

    /* 检查已匹配邻居的一致性：
     * 对于模式图中 p 的每个已映射邻居 p'，目标图中 t 必须有对应的
     * 已映射邻居 t'，且 (p', t') 必须在 core 中。 */
    for (int c = 0; c < pattern_graph->constraint_count; c++) {
        Constraint *pc = pattern_graph->constraints[c];
        bool p_participates = false;
        for (int k = 0; k < pc->participant_count; k++) {
            if (pc->participants[k] == p) {
                p_participates = true;
                continue;
            }
            int p_neighbor = pc->participants[k];
            if (state->core_1[p_neighbor] < 0) continue;
            int t_neighbor = state->core_1[p_neighbor];
            /* 检查 t 和 t_neighbor 之间是否存在相同类型的约束 */
            bool found = false;
            for (int ci = 0; ci < target_graph->constraint_count; ci++) {
                Constraint *tc = target_graph->constraints[ci];
                if (tc->type != pc->type) continue;
                bool t_in = false, tn_in = false;
                for (int kk = 0; kk < tc->participant_count; kk++) {
                    if (tc->participants[kk] == t) t_in = true;
                    if (tc->participants[kk] == t_neighbor) tn_in = true;
                }
                if (t_in && tn_in) { found = true; break; }
            }
            if (!found) return false;
        }
        if (!p_participates) continue;
    }

    /* 反向检查：目标图中 t 的已映射邻居在模式图中也必须兼容 */
    for (int c = 0; c < target_graph->constraint_count; c++) {
        Constraint *tc = target_graph->constraints[c];
        bool t_participates = false;
        for (int k = 0; k < tc->participant_count; k++) {
            if (tc->participants[k] == t) {
                t_participates = true;
                continue;
            }
            int t_neighbor = tc->participants[k];
            if (state->core_2[t_neighbor] < 0) continue;
            int p_mapped = state->core_2[t_neighbor];
            /* 在模式图中，p 和 p_mapped 之间必须存在相同类型的约束 */
            bool found = false;
            for (int ci = 0; ci < pattern_graph->constraint_count; ci++) {
                Constraint *pcon = pattern_graph->constraints[ci];
                if (pcon->type != tc->type) continue;
                bool p_in = false, pm_in = false;
                for (int kk = 0; kk < pcon->participant_count; kk++) {
                    if (pcon->participants[kk] == p) p_in = true;
                    if (pcon->participants[kk] == p_mapped) pm_in = true;
                }
                if (p_in && pm_in) { found = true; break; }
            }
            if (!found) return false;
        }
        if (!t_participates) continue;
    }

    return true;
}

/* VF2 前瞻函数：检查匹配 p->t 是否有前景。
 * 验证未映射邻居的兼容性，确保不会因为当前匹配导致
 * 后续无法完成匹配。 */
static bool vf2_lookahead(VF2State *state, int p, int t,
                           ConstraintGraph *pattern_graph,
                           ConstraintGraph *target_graph)
{
    /* 统计模式图中 p 的未映射邻居数量 */
    int p_unmapped_neighbors = 0;
    for (int c = 0; c < pattern_graph->constraint_count; c++) {
        Constraint *pc = pattern_graph->constraints[c];
        bool p_in = false;
        bool all_unmapped = true;
        for (int k = 0; k < pc->participant_count; k++) {
            if (pc->participants[k] == p) {
                p_in = true;
            } else if (state->core_1[pc->participants[k]] >= 0) {
                all_unmapped = false;
            }
        }
        if (p_in && all_unmapped) {
            p_unmapped_neighbors++;
        }
    }

    /* 统计目标图中 t 的未映射邻居数量 */
    int t_unmapped_neighbors = 0;
    for (int c = 0; c < target_graph->constraint_count; c++) {
        Constraint *tc = target_graph->constraints[c];
        bool t_in = false;
        bool all_unmapped = true;
        for (int k = 0; k < tc->participant_count; k++) {
            if (tc->participants[k] == t) {
                t_in = true;
            } else if (state->core_2[tc->participants[k]] >= 0) {
                all_unmapped = false;
            }
        }
        if (t_in && all_unmapped) {
            t_unmapped_neighbors++;
        }
    }

    /* 目标图的未映射邻居数不能少于模式图的未映射邻居数 */
    if (t_unmapped_neighbors < p_unmapped_neighbors)
        return false;

    /* 检查未映射邻居的度数兼容性：
     * 对于模式图中 p 的每个未映射邻居 p'，
     * 目标图中至少要有一个类型兼容的未映射邻居 t' */
    for (int c = 0; c < pattern_graph->constraint_count; c++) {
        Constraint *pc = pattern_graph->constraints[c];
        bool p_in = false;
        for (int k = 0; k < pc->participant_count; k++) {
            if (pc->participants[k] == p) {
                p_in = true;
                continue;
            }
            int p_neighbor = pc->participants[k];
            if (state->core_1[p_neighbor] >= 0) continue; /* 已映射，跳过 */

            GeomNode *pn = pattern_graph->nodes[p_neighbor];

            /* 在目标图中查找类型兼容的未映射邻居 */
            bool has_compatible = false;
            for (int gc = 0; gc < target_graph->constraint_count; gc++) {
                Constraint *tc = target_graph->constraints[gc];
                if (tc->type != pc->type) continue;

                bool t_in = false;
                for (int kk = 0; kk < tc->participant_count; kk++) {
                    if (tc->participants[kk] == t) {
                        t_in = true;
                        continue;
                    }
                    int t_neighbor = tc->participants[kk];
                    if (state->core_2[t_neighbor] >= 0) continue; /* 已映射，跳过 */

                    GeomNode *tn = target_graph->nodes[t_neighbor];
                    if (tn->type == pn->type) {
                        has_compatible = true;
                        break;
                    }
                }
                if (has_compatible) break;
            }
            if (!has_compatible)
                return false;
        }
        if (!p_in) continue;
    }

    return true;
}

/* VF2 递归匹配：尝试将模式图的所有节点映射到目标图。
 * 使用可行性剪枝和前瞻函数来减少搜索空间。
 * 找到的最大匹配会保存在 best_match 中。
 *
 * @param depth 当前递归深度，由上层调用者传入 depth+1。
 *              超过 REWRITE_VF2_MAX_DEPTH 时返回 false，防止栈溢出。 */
static bool vf2_match_recursive(VF2State *state,
                                 ConstraintGraph *pattern_graph,
                                 ConstraintGraph *target_graph,
                                 bool local_equivalence_tolerant,
                                 RewriteMatch *best_match,
                                 int *best_match_size,
                                 int depth)
{
    /* 递归深度保护：超过限制则立即返回失败，防止栈溢出 */
    if (depth > REWRITE_VF2_MAX_DEPTH) {
        LOG_WARN("rewrite", "VF2: max recursion depth (%d) exceeded", REWRITE_VF2_MAX_DEPTH);
        return false;
    }

    /* 基准情况：所有模式节点都已匹配 */
    if (state->core_count >= state->pattern_size) {
        return true;
    }

    /* 选择下一个要匹配的模式节点：
     * 优先选择有最多已映射邻居的未映射节点（MRV 启发式） */
    int best_p = -1;
    int best_mapped_neighbors = -1;

    for (int p = 0; p < state->pattern_size; p++) {
        if (state->core_1[p] >= 0) continue;

        int mapped_neighbors = 0;
        for (int c = 0; c < pattern_graph->constraint_count; c++) {
            Constraint *pc = pattern_graph->constraints[c];
            bool p_in = false;
            for (int k = 0; k < pc->participant_count; k++) {
                if (pc->participants[k] == p) {
                    p_in = true;
                } else if (state->core_1[pc->participants[k]] >= 0) {
                    mapped_neighbors++;
                }
            }
            if (p_in) break;
        }

        if (mapped_neighbors > best_mapped_neighbors) {
            best_mapped_neighbors = mapped_neighbors;
            best_p = p;
        }
    }

    if (best_p < 0) return false;

    /* 计算 best_p 的约束度数（涉及的约束数）用于候选排序 */
    int p_degree = 0;
    for (int c = 0; c < pattern_graph->constraint_count; c++) {
        Constraint *pc = pattern_graph->constraints[c];
        for (int k = 0; k < pc->participant_count; k++) {
            if (pc->participants[k] == best_p) { p_degree++; break; }
        }
    }

    /* 收集所有候选目标节点，按度数兼容性排序 */
    int *candidates = lv_malloc((size_t)state->target_size * sizeof(int));
    int *cand_scores = lv_malloc((size_t)state->target_size * sizeof(int));
    int cand_count = 0;
    if (!candidates || !cand_scores) {
        lv_free((void**)&candidates);
        lv_free((void**)&cand_scores);
        return false;
    }

    for (int t = 0; t < state->target_size; t++) {
        if (state->core_2[t] >= 0) continue;
        /* in/out 集合剪枝：跳过 out_set 中的节点 */
        bool in_out_set = false;
        for (int o = 0; o < state->out_count; o++) {
            if (state->out_set[o] == t) { in_out_set = true; break; }
        }
        if (in_out_set) continue;

        /* 计算目标节点度数 & 兼容性评分 */
        int t_degree = 0;
        for (int c = 0; c < target_graph->constraint_count; c++) {
            Constraint *tc = target_graph->constraints[c];
            for (int k = 0; k < tc->participant_count; k++) {
                if (tc->participants[k] == t) { t_degree++; break; }
            }
        }
        /* 评分 = 1 + (度数匹配分: 越接近 p_degree 越好)
         * 匹配度越高（差值越小）评分越低（优先级越高） */
        int score = 1 + abs(t_degree - p_degree);
        candidates[cand_count] = t;
        cand_scores[cand_count] = score;
        cand_count++;
    }

    /* 按评分升序排列（评分低的优先尝试） */
    for (int i = 0; i < cand_count - 1; i++) {
        int best_idx = i;
        for (int j = i + 1; j < cand_count; j++) {
            if (cand_scores[j] < cand_scores[best_idx]) best_idx = j;
        }
        if (best_idx != i) {
            int tmp_t = candidates[i]; candidates[i] = candidates[best_idx]; candidates[best_idx] = tmp_t;
            int tmp_s = cand_scores[i]; cand_scores[i] = cand_scores[best_idx]; cand_scores[best_idx] = tmp_s;
        }
    }

    /* 尝试将 best_p 匹配到目标图中的候选节点（按度数兼容性排序） */
    for (int ci = 0; ci < cand_count; ci++) {
        int t = candidates[ci];

        /* 可行性检查 */
        if (!vf2_feasible(state, best_p, t, pattern_graph, target_graph,
                          local_equivalence_tolerant))
            continue;

        /* 前瞻检查 */
        if (!vf2_lookahead(state, best_p, t, pattern_graph, target_graph))
            continue;

        /* 执行匹配 */
        state->core_1[best_p] = t;
        state->core_2[t] = best_p;
        state->core_count++;

        /* 更新 in/out 集合：将 t 加入 in_set */
        if (state->in_count >= state->in_capacity) {
            if (state->in_capacity > INT_MAX / 2) {
                LOG_WARN("rewrite", "VF2: in_set capacity overflow");
                state->core_1[best_p] = -1;
                state->core_2[t] = -1;
                state->core_count--;
                lv_free((void**)&candidates);
                lv_free((void**)&cand_scores);
                return false;
            }
            int new_cap = state->in_capacity * 2;
            int *new_in = lv_realloc(state->in_set, (size_t)new_cap * sizeof(int));
            if (!new_in) {
                LOG_WARN("rewrite", "VF2: in_set realloc failed (cap=%d), skipping candidate", new_cap);
                state->core_1[best_p] = -1;
                state->core_2[t] = -1;
                state->core_count--;
                continue;
            }
            state->in_set = new_in;
            state->in_capacity = new_cap;
        }
        state->in_set[state->in_count++] = t;

        /* 递归搜索 */
        int saved_out_count = state->out_count;
        
        if (vf2_match_recursive(state, pattern_graph, target_graph,
                                local_equivalence_tolerant,
                                best_match, best_match_size,
                                depth + 1)) {
            lv_free((void**)&candidates);
            lv_free((void**)&cand_scores);
            return true;
        }

        /* 回溯：从 in_set 中移除 t */
        for (int i = state->in_count - 1; i >= 0; i--) {
            if (state->in_set[i] == t) {
                state->in_set[i] = state->in_set[state->in_count - 1];
                state->in_count--;
                break;
            }
        }

        /* 回溯：恢复 out_set */
        state->out_count = saved_out_count;

        /* 将 t 加入 out_set */
        if (state->out_count >= state->out_capacity) {
            if (state->out_capacity > INT_MAX / 2) {
                LOG_WARN("rewrite", "VF2: out_set capacity overflow");
                lv_free((void**)&candidates);
                lv_free((void**)&cand_scores);
                return false;
            }
            int new_cap = state->out_capacity * 2;
            int *new_out = lv_realloc(state->out_set, (size_t)new_cap * sizeof(int));
            if (!new_out) {
                LOG_WARN("rewrite", "VF2: out_set realloc failed (cap=%d), skipping candidate", new_cap);
                state->core_1[best_p] = -1;
                state->core_2[t] = -1;
                state->core_count--;
                continue;
            }
            state->out_set = new_out;
            state->out_capacity = new_cap;
        }
        state->out_set[state->out_count++] = t;

        state->core_1[best_p] = -1;
        state->core_2[t] = -1;
        state->core_count--;
    }

    lv_free((void**)&candidates);
    lv_free((void**)&cand_scores);
    return false;
}

/* VF2 子图同构匹配的公开接口。
 * 在目标图中搜索与模式图同构的子图，返回匹配结果。
 * 如果找到匹配，返回 RewriteMatch 对象；否则返回 NULL。
 *
 * 当 local_equivalence_tolerant 为 true 时，在可行性检查和
 * 约束匹配阶段，对 POINT 节点使用 symbolic_coord_compare
 * 进行坐标相等性验证（design_v2.9.md Section 6.2）。 */
RewriteMatch *vf2_find_match(ConstraintGraph *target_graph,
                              RewritePattern *pattern,
                              bool local_equivalence_tolerant)
{
    if (!target_graph || !pattern || pattern->var_count == 0)
        return NULL;

    /* ----------------------------------------------------------------
     * 构建模式图的约束图结构。
     *
     * 为每个模式变量节点创建一个 GeomNode（类型为 GEOM_POINT），
     * 节点在 nodes 数组中的索引对应 variable_node_ids 的下标。
     * 节点 ID 设为模式变量的负 ID（如 -1, -2, ...），
     * 以便在可行性检查中通过 graph_get_node 查找。
     *
     * 模式约束中的参与者是模式变量 ID（负数）或固定节点 ID
     * （正数）。我们将参与者映射到模式图中的节点索引：
     *   - 负数参与者 -> 在 variable_node_ids 中查找其下标
     *   - 正数参与者 -> 在 variable_node_ids 中查找其下标
     * 如果参与者不在 variable_node_ids 中，则跳过该约束
     * （该约束涉及外部/边界节点，VF2 仅匹配模式变量之间的结构）。
     * ---------------------------------------------------------------- */
    ConstraintGraph *pattern_graph = graph_create();
    if (!pattern_graph) return NULL;

    /* 为每个模式变量创建节点 */
    for (int i = 0; i < pattern->var_count; i++) {
        GeomNode *node = lv_malloc(sizeof(GeomNode));
        if (!node) { graph_destroy(pattern_graph); return NULL; }
        memset(node, 0, sizeof(GeomNode));
        node->id = pattern->variable_node_ids[i]; /* 负数 ID */
        node->type = GEOM_POINT;
        node->trust = TRUST_GREEN;
        node->coord_count = 0;
        node->symbolic_coords = NULL;

        pattern_graph->nodes = lv_realloc(pattern_graph->nodes,
            (size_t)(pattern_graph->node_count + 1) * sizeof(GeomNode *));
        pattern_graph->nodes[pattern_graph->node_count++] = node;
    }
    pattern_graph->next_node_id = pattern->var_count;

    /* 构建模式变量 ID -> 数组索引的映射表 */
    int *var_id_to_idx = lv_malloc((size_t)pattern->var_count * sizeof(int));
    if (!var_id_to_idx) { graph_destroy(pattern_graph); return NULL; }
    for (int i = 0; i < pattern->var_count; i++) {
        var_id_to_idx[i] = -1;
    }
    for (int i = 0; i < pattern->var_count; i++) {
        /* 使用 abs(variable_node_ids[i]) % var_count 作为哈希索引 */
        int vid = pattern->variable_node_ids[i];
        int idx = ((vid < 0 ? -vid : vid) % pattern->var_count);
        while (var_id_to_idx[idx] >= 0 &&
               pattern->variable_node_ids[var_id_to_idx[idx]] != vid) {
            idx = (idx + 1) % pattern->var_count;
        }
        var_id_to_idx[idx] = i;
    }

    /* 将模式约束添加到模式图中。
     * 参与者 ID 映射到模式图节点的索引（即 nodes 数组下标）。 */
    for (int c = 0; c < pattern->pattern_constraint_count; c++) {
        Constraint *pc = pattern->pattern_constraints[c];

        /* 将参与者 ID 映射到模式图中的节点索引 */
        int *mapped_participants = lv_malloc((size_t)pc->participant_count * sizeof(int));
        if (!mapped_participants) {
            lv_free((void**)&var_id_to_idx);
            graph_destroy(pattern_graph);
            return NULL;
        }

        bool all_mapped = true;
        for (int p = 0; p < pc->participant_count; p++) {
            int pid = pc->participants[p];
            int node_idx = -1;

            /* 在 variable_node_ids 中查找 pid */
            for (int v = 0; v < pattern->var_count; v++) {
                if (pattern->variable_node_ids[v] == pid) {
                    node_idx = v;
                    break;
                }
            }

            if (node_idx < 0) {
                /* 参与者不在模式变量中 -- 跳过此约束 */
                all_mapped = false;
                break;
            }
            mapped_participants[p] = node_idx;
        }

        if (all_mapped) {
            Constraint *new_con = lv_malloc(sizeof(Constraint));
            if (!new_con) {
                lv_free((void**)&mapped_participants);
                lv_free((void**)&var_id_to_idx);
                graph_destroy(pattern_graph);
                return NULL;
            }
            memset(new_con, 0, sizeof(Constraint));
            new_con->id = pattern_graph->next_constraint_id++;
            new_con->type = pc->type;
            new_con->participant_count = pc->participant_count;
            new_con->participants = mapped_participants;

            pattern_graph->constraints = lv_realloc(pattern_graph->constraints,
                (size_t)(pattern_graph->constraint_count + 1) * sizeof(Constraint *));
            pattern_graph->constraints[pattern_graph->constraint_count++] = new_con;
        } else {
            lv_free((void**)&mapped_participants);
        }
    }

    lv_free((void**)&var_id_to_idx);

    /* 初始化 VF2 匹配状态 */
    VF2State state;
    vf2_state_init(&state, pattern_graph->node_count, target_graph->node_count);

    /* 使用模式图和目标图进行 VF2 子图同构匹配 */
    bool found = vf2_match_recursive(&state, pattern_graph, target_graph,
                                     local_equivalence_tolerant,
                                     NULL, NULL, 0);

    RewriteMatch *match = NULL;

    if (found) {
        /* 将 VF2 匹配结果转换为 RewriteMatch 格式 */
        match = lv_malloc(sizeof(RewriteMatch));
        if (!match) { vf2_state_destroy(&state); graph_destroy(pattern_graph); return NULL; }

        match->node_bindings = lv_malloc((size_t)pattern->var_count * 2 * sizeof(int));
        if (!match->node_bindings) { lv_free((void**)&match); vf2_state_destroy(&state); graph_destroy(pattern_graph); return NULL; }

        match->constraint_bindings = lv_malloc(
            (size_t)pattern->pattern_constraint_count * sizeof(int));
        if (!match->constraint_bindings) {
            lv_free((void**)&match->node_bindings);
            lv_free((void**)&match);
            vf2_state_destroy(&state);
            graph_destroy(pattern_graph);
            return NULL;
        }

        /* 填充节点绑定：
         * core_1[i] 是模式图中第 i 个节点在目标图中的索引 */
        for (int i = 0; i < pattern->var_count; i++) {
            int target_node_idx = state.core_1[i];
            match->node_bindings[i * 2] = pattern->variable_node_ids[i];
            if (target_node_idx >= 0 && target_node_idx < target_graph->node_count) {
                match->node_bindings[i * 2 + 1] =
                    target_graph->nodes[target_node_idx]->id;
            } else {
                match->node_bindings[i * 2 + 1] = -1;
            }
        }

        /* 匹配约束：将模式约束与目标约束对应。
         * 在 local_equivalence_tolerant 模式下，对 POINT 节点的参与者
         * 使用 symbolic_coord_compare 进行坐标相等性验证。 */
        int constraint_match_count = 0;
        for (int pc = 0; pc < pattern->pattern_constraint_count; pc++) {
            Constraint *pcon = pattern->pattern_constraints[pc];

            for (int gc = 0; gc < target_graph->constraint_count; gc++) {
                Constraint *tcon = target_graph->constraints[gc];
                if (pcon->type != tcon->type) continue;
                if (pcon->participant_count != tcon->participant_count) continue;

                bool all_match = true;
                for (int k = 0; k < pcon->participant_count; k++) {
                    int pid = pcon->participants[k];
                    int tid = tcon->participants[k];

                    /* 查找模式变量对应的绑定 */
                    if (pid < 0) {
                        bool found_bind = false;
                        for (int b = 0; b < pattern->var_count; b++) {
                            if (pattern->variable_node_ids[b] == pid) {
                                if (match->node_bindings[b * 2 + 1] == tid) {
                                    found_bind = true;
                                }
                                break;
                            }
                        }
                        if (!found_bind) { all_match = false; break; }
                    } else {
                        if (pid != tid) { all_match = false; break; }
                    }
                }
                if (all_match) {
                    match->constraint_bindings[constraint_match_count++] =
                        pcon->id;
                    break;
                }
            }
        }
        match->binding_count = pattern->var_count;
    }

    vf2_state_destroy(&state);
    graph_destroy(pattern_graph);
    return match;
}
