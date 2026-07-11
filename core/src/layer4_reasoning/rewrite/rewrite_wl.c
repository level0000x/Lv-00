/**
 * @file rewrite_wl.c
 * @brief WL 循环检测与度量验证
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
#include "lv00/rewrite.h"
#include "lv00/constraint_graph.h"
#include "debug.h"
#include "lv00_internal.h"
#include "lv00_utils.h"
#include "mpz_poly.h"

                                break;
                            }
                        }
                        if (!found_bind) { all_match = false; break; }
                    } else {
                        if (pid != tid) { all_match = false; break; }
                    }

                    /* local_equivalence_tolerant 模式下：
                     * 对 POINT 节点使用 symbolic_coord_compare 验证坐标相等 */
                    if (local_equivalence_tolerant && all_match) {
                        GeomNode *p_node = graph_get_node(target_graph, pid);
                        GeomNode *t_node = graph_get_node(target_graph, tid);
                        if (p_node && t_node &&
                            p_node->type == GEOM_POINT && t_node->type == GEOM_POINT &&
                            p_node->coord_count > 0 && t_node->coord_count > 0) {
                            if (p_node->coord_count != t_node->coord_count) {
                                all_match = false;
                            } else {
                                for (int c = 0; c < p_node->coord_count; c++) {
                                    if (!p_node->symbolic_coords[c] ||
                                        !t_node->symbolic_coords[c] ||
                                        symbolic_coord_compare(
                                            p_node->symbolic_coords[c],
                                            t_node->symbolic_coords[c]) != 0) {
                                        all_match = false;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }

                if (all_match) {
                    match->constraint_bindings[pc] = tcon->id;
                    constraint_match_count++;
                    break;
                }
            }
        }

        match->binding_count = constraint_match_count;

        /* 验证所有已添加到模式图的约束都匹配成功 */
        if (constraint_match_count != pattern_graph->constraint_count) {
            lv00_free((void**)&match->node_bindings);
            lv00_free((void**)&match->constraint_bindings);
            lv00_free((void**)&match);
            match = NULL;
        }

        /* 流式输出：匹配成功时发射事件 */
        if (match && rewrite_stream_ctx) {
            stream_emit_simple(rewrite_stream_ctx, STREAM_EVENT_REWRITE_MATCH_FOUND,
                               "VF2 subgraph isomorphism match found", -1);
        }
    }

    vf2_state_destroy(&state);
    graph_destroy(pattern_graph);
    return match;
}

/* ===========================================================================
 * WL (Weisfeiler-Lehman) 图核哈希
 *
 * WL 算法通过迭代精化节点标签来计算图的拓扑哈希。
 * 初始标签基于节点类型和约束拓扑（忽略坐标值），
 * 每轮迭代根据邻居标签更新当前标签，最终聚合为图哈希。
 * ===========================================================================
 */

/* 初始化 WL 哈希历史环形缓冲区 */
void wl_history_init(WLHashHistory *hist) {
    hist->hash_history = lv00_malloc(WL_HISTORY_SIZE * sizeof(uint64_t));
    if (hist->hash_history) memset(hist->hash_history, 0, WL_HISTORY_SIZE * sizeof(uint64_t));
    hist->history_count = 0;
    hist->history_pos = 0;
}

/* uint64_t 比较函数（供 qsort 使用） */
static int uint64_compare(const void *a, const void *b) {
    uint64_t va = *(const uint64_t *)a;
    uint64_t vb = *(const uint64_t *)b;
    if (va < vb) return -1;
    if (va > vb) return 1;
    return 0;
}

/* 销毁 WL 哈希历史，释放内存 */
void wl_history_destroy(WLHashHistory *hist) {
    lv00_free((void**)&hist->hash_history);
    hist->hash_history = NULL;
    hist->history_count = 0;
    hist->history_pos = 0;
}

/* 向环形缓冲区中推入一个新的图哈希（64位完整哈希 + 32位轻量哈希） */
static void wl_history_push(WLHashHistory *hist, uint64_t hash) {
    hist->hash_history[hist->history_pos] = hash;
    hist->history_pos = (hist->history_pos + 1) % WL_HISTORY_SIZE;
    if (hist->history_count < WL_HISTORY_SIZE) {
        hist->history_count++;
    }
    /* 同步更新32位轻量哈希（用于快速预筛选） */
    uint32_t light = (uint32_t)(hash ^ (hash >> 32));
    hist->light_hash_history[hist->light_history_pos] = light;
    hist->light_history_pos = (hist->light_history_pos + 1) % WL_HISTORY_SIZE;
    if (hist->light_history_count < WL_HISTORY_SIZE) {
        hist->light_history_count++;
    }
}

/* 两阶段检查：先用32位轻量哈希快速预筛选，匹配时再用64位确认 */
static bool wl_history_contains_light(WLHashHistory *hist, uint32_t light_hash) {
    for (int i = 0; i < hist->light_history_count; i++) {
        if (hist->light_hash_history[i] == light_hash) {
            return true;
        }
    }
    return false;
}

/* 检查环形缓冲区中是否已包含指定的哈希值 */
static bool wl_history_contains(WLHashHistory *hist, uint64_t hash) {
    /* 阶段1：32位轻量预筛选 */
    uint32_t light = (uint32_t)(hash ^ (hash >> 32));
    if (!wl_history_contains_light(hist, light)) {
        return false; /* 快速路径：轻量哈希不匹配，直接排除 */
    }

    /* 阶段2：64位精确确认 */
    for (int i = 0; i < hist->history_count; i++) {
        if (hist->hash_history[i] == hash) {
            return true;
        }
    }
    return false;
}

/* 计算节点的初始 WL 标签。
 * 标签基于节点类型和约束拓扑（不包含坐标值），
 * 确保结构相同但坐标不同的图具有相同的初始标签。 */
static uint64_t *compute_wl_initial_labels(ConstraintGraph *graph, int node_count) {
    uint64_t *labels = lv00_malloc((size_t)node_count * sizeof(uint64_t));
    if (!labels) return NULL;

    for (int i = 0; i < node_count; i++) {
        GeomNode *n = graph->nodes[i];
        /* 类型 + 信任颜色 + Light Orange子类型 作为增强基础标签 */
        uint64_t label = (uint64_t)(n->type + 1) * 65599
                       + (uint64_t)(n->trust)
                       + ((uint64_t)(n->lo_subtype) << 8);

        /* 统计该节点参与的每种约束类型的数量（拓扑信息） */
        int incidence_count = 0, betweenness_count = 0;
        int intersection_count = 0, containment_count = 0;
        int connection_count = 0;

        for (int c = 0; c < graph->constraint_count; c++) {
            Constraint *con = graph->constraints[c];
            for (int p = 0; p < con->participant_count; p++) {
                if (con->participants[p] == n->id) {
                    switch (con->type) {
                        case INCIDENCE:     incidence_count++; break;
                        case BETWEENNESS:  betweenness_count++; break;
                        case INTERSECTION: intersection_count++; break;
                        case CONTAINMENT:  containment_count++; break;
                        case CONNECTION:   connection_count++; break;
                    }
                    break;
                }
            }
        }

        /* 将约束计数信息混入标签 */
        label = label * 31 + (uint64_t)incidence_count;
        label = label * 31 + (uint64_t)betweenness_count;
        label = label * 31 + (uint64_t)intersection_count;
        label = label * 31 + (uint64_t)containment_count;
        label = label * 31 + (uint64_t)connection_count;

        labels[i] = label;
    }

    return labels;
}

/* 执行一轮 WL 迭代：根据邻居标签精化当前标签。
 * 每个节点的新标签 = hash(旧标签 + 排序后的邻居标签列表)。
 * 返回新分配的标签数组，调用者负责释放。 */
static uint64_t *wl_refine_labels(ConstraintGraph *graph,
                                    uint64_t *labels, int node_count)
{
    uint64_t *new_labels = lv00_malloc((size_t)node_count * sizeof(uint64_t));
    if (!new_labels) return NULL;

    /* 构建节点 id -> 索引的映射 */
    int *id_to_idx = lv00_malloc((size_t)node_count * sizeof(int));
    if (!id_to_idx) { lv00_free((void**)&new_labels); return NULL; }
    for (int i = 0; i < node_count; i++) {
        id_to_idx[i] = -1;
    }
    for (int i = 0; i < node_count; i++) {
        int id = graph->nodes[i]->id;
        /* 使用 id % node_count 作为简单哈希，处理 id 可能不连续的情况 */
        int idx = ((id % node_count) + node_count) % node_count;
        /* 线性探测解决冲突 */
        while (idx < node_count && id_to_idx[idx] >= 0 &&
               graph->nodes[id_to_idx[idx]]->id != id) {
            idx = (idx + 1) % node_count;
        }
        if (idx < node_count) {
            id_to_idx[idx] = i;
        }
    }

    for (int i = 0; i < node_count; i++) {
        uint64_t refined = labels[i];

        /* 收集该节点所有邻居的标签（容量提升至128以支持密集图） */
        uint64_t neighbor_labels[128];
        int neighbor_count = 0;

        for (int c = 0; c < graph->constraint_count; c++) {
            Constraint *con = graph->constraints[c];
            for (int p = 0; p < con->participant_count; p++) {
                if (con->participants[p] == graph->nodes[i]->id) {
                    for (int q = 0; q < con->participant_count; q++) {
                        if (q == p) continue;
                        int neighbor_id = con->participants[q];
                        /* 查找邻居节点在 nodes 数组中的索引 */
                        int nidx = ((neighbor_id % node_count) + node_count) % node_count;
                        while (nidx < node_count && id_to_idx[nidx] >= 0 &&
                               graph->nodes[id_to_idx[nidx]]->id != neighbor_id) {
                            nidx = (nidx + 1) % node_count;
                        }
                        if (nidx < node_count && id_to_idx[nidx] >= 0 &&
                            graph->nodes[id_to_idx[nidx]]->id == neighbor_id) {
                            if (neighbor_count < 128) {
                                neighbor_labels[neighbor_count++] =
                                    labels[id_to_idx[nidx]];
                            }
                        }
                    }
                    break;
                }
            }
        }

        /* 使用标准 qsort 对邻居标签排序以确保确定性 */
        qsort(neighbor_labels, neighbor_count, sizeof(uint64_t),
              (int(*)(const void*, const void*))uint64_compare);

        /* 将邻居标签混入精化标签 */
        for (int n = 0; n < neighbor_count; n++) {
            refined = refined * LV00_REWRITE_WL_HASH_MULTIPLIER + neighbor_labels[n];
        }

        new_labels[i] = refined;
    }

    lv00_free((void**)&id_to_idx);
    return new_labels;
}

/* 计算 WL 图核哈希（2 轮迭代，基于拓扑结构，忽略坐标值）。
 * 将所有节点标签聚合为一个 64 位图哈希。 */
static uint64_t compute_wl_graph_hash(ConstraintGraph *graph) {
    if (!graph || graph->node_count == 0)
        return 0;

    int node_count = graph->node_count;

    /* 计算初始标签 */
    uint64_t *labels = compute_wl_initial_labels(graph, node_count);
    if (!labels) return 0;

    /* 执行 WL_ITERATIONS 轮迭代 */
    for (int iter = 0; iter < WL_ITERATIONS; iter++) {
        uint64_t *new_labels = wl_refine_labels(graph, labels, node_count);
        lv00_free((void**)&labels);
        if (!new_labels) return 0;
        labels = new_labels;
    }

    /* 聚合所有节点标签为图哈希 */
    uint64_t graph_hash = (uint64_t)graph->node_count;
    for (int i = 0; i < node_count; i++) {
        graph_hash = graph_hash * LV00_REWRITE_WL_HASH_MULTIPLIER + labels[i];
    }

    lv00_free((void**)&labels);
    return graph_hash;
}

/* 使用 WL 图核哈希检测重写循环。
 * 计算当前图的 WL 哈希，与历史缓冲区比较。
 * 如果发现重复哈希，说明图回到了之前的状态，存在循环。
 * 新哈希会被推入历史缓冲区（固定 16 步）。 */
RewriteStatus detect_rewrite_loop_wl(ConstraintGraph *graph, WLHashHistory *hist) {
    if (!graph || !hist) return REWRITE_OK;

    uint64_t current_hash = compute_wl_graph_hash(graph);

    /* 检查缓冲区中是否已存在该哈希 */
    if (wl_history_contains(hist, current_hash)) {
        if (rewrite_stream_ctx) {
            stream_emit_simple(rewrite_stream_ctx, STREAM_EVENT_ERROR,
                               "WL rewrite loop detected: graph hash repeated", -1);
        }
        return REWRITE_TERMINATED;
    }

    /* 推入新哈希 */
    wl_history_push(hist, current_hash);
    return REWRITE_OK;
}

/* ===========================================================================
 * 前置条件系统
 *
 * 前置条件允许在匹配成功后、执行替换前进行额外的验证。
 * 只有前置条件评估通过时，才会执行重写替换操作。
 * ===========================================================================
 */

/* 评估重写规则的前置条件。
 * 如果规则没有设置前置条件（condition_func 为 NULL），
 * 默认返回 true（通过）。
 * 前置条件在匹配成功后、替换前调用。 */
static bool evaluate_precondition(ConstraintGraph *graph,
                                   RewriteRule *rule,
                                   RewriteMatch *match)
{
    if (!rule || !rule->condition_func)
        return true;

    return rule->condition_func(graph, match, rule->condition_data);
}

/* ===========================================================================
 * 重写度量验证
 *
 * 在应用重写规则后，验证归约度量是否确实减少了。
 * 度量定义为：节点数 + 约束数。
 * 如果 after 的度量比 before 的度量减少了 expected_reduction 或更多，
 * 则返回 1，否则返回 0。
 * =========================================================================== */

bool rewrite_validate_measure(const ConstraintGraph *graph, const RewriteRule *rule, const GraphSnapshot *graph_before) {
    if (!graph || !rule) return false;

    /* 根据 design_v2.9.md 第6.1节：
     * - reduction_measure > 0：验证缩减量 >= 度量值
     * - reduction_measure == 0：跳过验证
     * - reduction_measure < 0：验证扩展量 <= |度量值| */
    int expected = rule->reduction_measure;
    if (expected == 0) return true; /* 跳过中性规则的验证 */

    if (graph_before) {
        int nodes_before = graph_before->node_count;
        int nodes_after = graph->node_count;

        int node_reduction = nodes_before - nodes_after;

        if (expected > 0) {
            /* 验证节点缩减量是否达到目标 */
            return (node_reduction >= expected);
        } else {
            /* 验证扩展量不超过 |expected| */
            int max_expansion = -expected;
            int node_expansion = nodes_after - nodes_before;
            return (node_expansion <= max_expansion);
        }
    }

    /* 回退：无哈希时的基本检查 */
    return true;
}

/* ===========================================================================
 * 最佳匹配选择
 *
 * 在图中查找所有非重叠的子图匹配，选择匹配子图节点数最多的匹配。
 * 这确保了重写规则应用在最合适的子图上。
 * ===========================================================================
 */

/* 在图中查找最佳匹配（匹配子图节点数最多的匹配）。
 * 使用 VF2 算法进行子图同构匹配，并通过前置条件验证。
 * 返回最佳匹配的 RewriteMatch 对象，或 NULL 表示未找到。 */
RewriteMatch *find_best_match(ConstraintGraph *graph,
                                      RewriteRule *rule,
                                      bool local_equivalence_tolerant)
{
    if (!graph || !rule || !rule->pattern)
        return NULL;

    RewritePattern *pat = rule->pattern;

    /* 使用 VF2 算法查找匹配 */
    RewriteMatch *match = vf2_find_match(graph, pat, local_equivalence_tolerant);
    if (!match)
        return NULL;

    /* 评估前置条件 */
    if (!evaluate_precondition(graph, rule, match)) {
        lv00_free((void**)&match->node_bindings);
        lv00_free((void**)&match->constraint_bindings);
        lv00_free((void**)&match);
        return NULL;
    }

    return match;
}
