/**
 * @file rewrite_match_search.c
 * @brief 重写规则：匹配查找与多匹配选择
 *
 * 从 rewrite_match.c 拆分的模块之一（拆分清单见 rewrite_binding.c）。
 *
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
#include "lv/rewrite.h"

#include "debug.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "mpz_poly.h"

/* 前向声明 */
extern bool evaluate_precondition(ConstraintGraph *graph, RewriteRule *rule, RewriteMatch *match);
/* ---- 模式匹配辅助函数 ---- */

/* 模式变量类型推导：根据模式约束的参与者槽位，推导每个模式变量
 * 可绑定的 GeomType 集合。类型约束与 graph_index.c 中 typed add_*
 * 系列的类型校验保持一致：
 *   INCIDENCE:    [0]=POINT, [1]=LINE_SEGMENT/REGION/CIRCLE
 *   BETWEENNESS:  全部为 POINT
 *   INTERSECTION: [0][1]=LINE_SEGMENT, [2]=POINT
 *   CONTAINMENT:  [0]=POINT/REGION, [1]=REGION/CIRCLE
 *   CONNECTION:   全部为 PORT
 *   ANGLE:        全部为 LINE_SEGMENT
 */

#define REWRITE_GEOM_TYPE_COUNT 6

static unsigned rewrite_constraint_participant_type_mask(ConstraintType type, int position) {
    switch (type) {
        case INCIDENCE:
            return (position == 0) ? (1u << GEOM_POINT)
                                   : ((1u << GEOM_LINE_SEGMENT) | (1u << GEOM_REGION) | (1u << GEOM_CIRCLE));
        case BETWEENNESS:
            return 1u << GEOM_POINT;
        case INTERSECTION:
            return (position < 2) ? (1u << GEOM_LINE_SEGMENT) : (1u << GEOM_POINT);
        case CONTAINMENT:
            return (position == 0) ? ((1u << GEOM_POINT) | (1u << GEOM_REGION))
                                   : ((1u << GEOM_REGION) | (1u << GEOM_CIRCLE));
        case CONNECTION:
            return 1u << GEOM_PORT;
        case ANGLE:
            return 1u << GEOM_LINE_SEGMENT;
        default:
            return (1u << REWRITE_GEOM_TYPE_COUNT) - 1u;
    }
}

static void rewrite_pattern_var_type_masks(const RewritePattern *pat, unsigned *masks, int var_count) {
    const unsigned all_types = (1u << REWRITE_GEOM_TYPE_COUNT) - 1u;
    for (int j = 0; j < var_count; j++)
        masks[j] = all_types;
    for (int c = 0; c < pat->pattern_constraint_count; c++) {
        Constraint *pc = pat->pattern_constraints[c];
        if (!pc)
            continue;
        for (int p = 0; p < pc->participant_count; p++) {
            int pid = pc->participants[p];
            if (pid >= 0)
                continue;
            int slot = -1;
            for (int j = 0; j < var_count; j++) {
                if (pat->variable_node_ids[j] == pid) {
                    slot = j;
                    break;
                }
            }
            if (slot < 0)
                continue;
            masks[slot] &= rewrite_constraint_participant_type_mask(pc->type, p);
        }
    }
}

static bool pattern_var_matches_node(int pattern_var_id, GeomNode *graph_node, const int *bindings, int binding_count) {
    if (pattern_var_id >= 0) {
        return pattern_var_id == graph_node->id;
    }
    for (int i = 0; i < binding_count; i++) {
        if (bindings[i * 2] == pattern_var_id) {
            return bindings[i * 2 + 1] == graph_node->id;
        }
    }
    return false;
}

/**
 * @brief 检查模式约束是否与图约束匹配
 *
 * 检查类型、参与者数量和参与者 ID 是否一致。
 *
 * @param pattern         模式约束
 * @param graph_con       图约束
 * @param bindings        绑定数组
 * @param binding_count   绑定数量
 * @return true 表示匹配成功
 */
static bool pattern_constraint_matches(Constraint *pattern, Constraint *graph_con, const int *bindings,
                                       int binding_count) {
    if (pattern->type != graph_con->type)
        return false;
    if (pattern->participant_count != graph_con->participant_count)
        return false;
    for (int i = 0; i < pattern->participant_count; i++) {
        int pid = pattern->participants[i];
        int gid = graph_con->participants[i];
        if (pid < 0) {
            bool found = false;
            for (int j = 0; j < binding_count; j++) {
                if (bindings[j * 2] == pid) {
                    found = (bindings[j * 2 + 1] == gid);
                    break;
                }
            }
            if (!found)
                return false;
        } else {
            if (pid != gid)
                return false;
        }
    }
    return true;
}

/**
 * @brief 查找与重写规则模式匹配的单个匹配
 *
 * 在约束图中查找与给定重写规则模式匹配的模式变量绑定。
 * 支持 local_equivalence_tolerant 模式，该模式下 POINT 节点
 * 可以通过符号坐标而非节点 ID 进行匹配。
 *
 * @param graph                      约束图指针
 * @param rule                       重写规则指针
 * @param local_equivalence_tolerant 是否启用局部等价容忍模式
 * @return 新分配的匹配结果，失败返回 NULL
 */
RewriteMatch *find_rewrite_match(ConstraintGraph *graph, RewriteRule *rule, bool local_equivalence_tolerant) {
    RewritePattern *pat = rule->pattern;

    /* 空图上不存在任何匹配 */
    if (!graph || graph->node_count == 0)
        return NULL;

    RewriteMatch *match = lv_calloc(1, sizeof(RewriteMatch));
    if (!match)
        return NULL;
    match->node_bindings = lv_malloc(pat->var_count * 2 * sizeof(int));
    if (!match->node_bindings) {
        lv_free((void **) &match);
        return NULL;
    }
    match->constraint_bindings = lv_malloc(pat->pattern_constraint_count * sizeof(int));
    if (!match->constraint_bindings) {
        lv_free((void **) &match->node_bindings);
        lv_free((void **) &match);
        return NULL;
    }
    match->binding_count = 0;
    int binding_count = 0;

    /* 推导每个模式变量可绑定的节点类型集合（依据模式约束的参与者槽位）。
     * 使 Phase 1 绑定具备类型感知能力：例如 INCIDENCE 的 participants[1]
     * 槽位只会接受 LINE_SEGMENT/REGION/CIRCLE，避免把线段变量错误地
     * 绑定到 POINT 节点上。 */
    unsigned *type_masks = NULL;
    if (pat->var_count > 0) {
        type_masks = lv_malloc((size_t) pat->var_count * sizeof(unsigned));
        if (!type_masks) {
            lv_free((void **) &match->node_bindings);
            lv_free((void **) &match->constraint_bindings);
            lv_free((void **) &match);
            return NULL;
        }
        rewrite_pattern_var_type_masks(pat, type_masks, pat->var_count);
    }

    /* --- Phase 1: bind pattern variables to graph nodes --- */
    for (int j = 0; j < pat->var_count; j++) {
        int pattern_var_id = pat->variable_node_ids[j];
        bool bound = false;

        for (int i = 0; i < graph->node_count && !bound; i++) {
            GeomNode *gn = graph->nodes[i];

            /* 跳过已被其他模式变量绑定的图节点（每个图节点
               在单个匹配中只能满足一个模式变量） */
            bool already_used = false;
            for (int k = 0; k < binding_count; k++) {
                if (match->node_bindings[k * 2 + 1] == gn->id) {
                    already_used = true;
                    break;
                }
            }
            if (already_used)
                continue;

            /* 标准ID匹配 */
            if (pattern_var_id >= 0) {
                if (pattern_var_id == gn->id) {
                    match->node_bindings[binding_count * 2] = pattern_var_id;
                    match->node_bindings[binding_count * 2 + 1] = gn->id;
                    binding_count++;
                    bound = true;
                }
                continue;
            }

            /* 模式变量（负值）：先检查是否已绑定 */
            bool was_bound = false;
            for (int k = 0; k < binding_count; k++) {
                if (match->node_bindings[k * 2] == pattern_var_id) {
                    was_bound = true;
                    break;
                }
            }
            if (was_bound)
                continue;

            /* 类型兼容性检查：模式变量只能绑定其允许的 GeomType */
            if (gn->type < 0 || gn->type >= REWRITE_GEOM_TYPE_COUNT)
                continue;
            if (!(type_masks[j] & (1u << (unsigned) gn->type)))
                continue;

            /* 在 local_equivalence_tolerant 模式下，对于 POINT 节点，
               即使ID不同，也接受具有相同符号坐标的节点。
               这允许跨结构等价但独立构造的子图进行匹配。 */
            if (local_equivalence_tolerant && gn->type == GEOM_POINT) {
                /* 尝试查找已绑定节点中是否有相同坐标的节点 */
                bool coord_match = false;
                int existing_bind = -1;
                for (int k = 0; k < binding_count; k++) {
                    if (match->node_bindings[k * 2] == pattern_var_id) {
                        existing_bind = match->node_bindings[k * 2 + 1];
                        break;
                    }
                }
                if (existing_bind >= 0) {
                    GeomNode *existing = graph_get_node(graph, existing_bind);
                    if (existing && existing->type == GEOM_POINT && existing->coord_count == gn->coord_count) {
                        coord_match = true;
                        for (int c = 0; c < gn->coord_count; c++) {
                            if (symbolic_coord_compare(existing->symbolic_coords[c], gn->symbolic_coords[c]) != 0) {
                                coord_match = false;
                                break;
                            }
                        }
                    }
                    if (coord_match) {
                        bound = true;
                    }
                } else {
                    /* 首次出现此变量：绑定它 */
                    match->node_bindings[binding_count * 2] = pattern_var_id;
                    match->node_bindings[binding_count * 2 + 1] = gn->id;
                    binding_count++;
                    bound = true;
                }
            } else {
                /* 标准模式：绑定第一个未绑定节点 */
                match->node_bindings[binding_count * 2] = pattern_var_id;
                match->node_bindings[binding_count * 2 + 1] = gn->id;
                binding_count++;
                bound = true;
            }
        }

        if (!bound) {
            /* 无法绑定此模式变量 */
            lv_free((void **) &type_masks);
            lv_free((void **) &match->node_bindings);
            lv_free((void **) &match->constraint_bindings);
            lv_free((void **) &match);
            return NULL;
        }
    }

    /* --- Phase 2: match pattern constraints against graph constraints --- */
    int constraint_match_count = 0;
    bool *pattern_con_matched = lv_malloc((size_t) pat->pattern_constraint_count * sizeof(bool));
    if (pattern_con_matched)
        memset(pattern_con_matched, 0, (size_t) pat->pattern_constraint_count * sizeof(bool));

    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *gc = graph->constraints[i];
        for (int j = 0; j < pat->pattern_constraint_count; j++) {
            if (pattern_con_matched[j])
                continue;
            if (pattern_constraint_matches(pat->pattern_constraints[j], gc, match->node_bindings, binding_count)) {
                match->constraint_bindings[j] = gc->id;
                pattern_con_matched[j] = true;
                constraint_match_count++;
                break;
            }
        }
    }

    lv_free((void **) &pattern_con_matched);

    if (constraint_match_count != pat->pattern_constraint_count) {
        lv_free((void **) &type_masks);
        lv_free((void **) &match->node_bindings);
        lv_free((void **) &match->constraint_bindings);
        lv_free((void **) &match);
        return NULL;
    }

    /* binding_count 记录的是实际绑定的节点数量（Phase 1 中递增的计数器），
     * 而非 constraint_match_count（约束匹配数量）。两者含义不同：
     *   - binding_count: 模式变量到图节点的绑定对数
     *   - constraint_match_count: 模式约束到图约束的匹配数
     * 此处应使用 binding_count，因为后续代码依赖 binding_count 来遍历
     * node_bindings 数组中的绑定对。 */
    match->binding_count = binding_count;
    lv_free((void **) &type_masks);
    return match;
}

/* ===========================================================================
 * 多非重叠匹配查找
 *
 * 在约束图中查找所有与给定重写规则模式匹配的非重叠子图同构。
 * 每次找到一个匹配后，将其匹配的节点标记为已使用，继续搜索直到
 * 无法找到新的匹配。最终返回按匹配质量（匹配节点数降序）排序的
 * 匹配数组。
 *
 * 设计规范参考：design_v2.9.md Section 6.4
 * ===========================================================================
 */

/**
 * @brief 匹配质量比较函数：按匹配节点数降序排序
 *
 * 用作 qsort 的比较函数。以 binding_count（匹配的约束数量）作为
 * 匹配质量的代理指标，数量多的排前面。
 *
 * @param a 指向第一个 RewriteMatch 指针的指针
 * @param b 指向第二个 RewriteMatch 指针的指针
 * @return >0 表示 a 优于 b，<0 表示 b 优于 a
 */
static int match_quality_cmp(const void *a, const void *b) {
    const RewriteMatch *ma = *(const RewriteMatch **) a;
    const RewriteMatch *mb = *(const RewriteMatch **) b;
    /* binding_count 是匹配的约束数量，作为匹配质量的代理指标 */
    if (ma->binding_count != mb->binding_count) {
        return (mb->binding_count > ma->binding_count) ? 1 : -1;
    }
    return 0;
}

/* 检查匹配是否与已使用的节点集合重叠。
     * 返回 true 如果存在重叠（即匹配中有节点在 used_ids 中）。 */
/**
 * @brief 检查匹配是否与已使用的节点集合重叠
 *
 * @param match                   匹配指针
 * @param used_ids               已使用节点 ID 数组
 * @param used_count             已使用节点数量
 * @param node_binding_pair_count 节点绑定对数量
 * @return true 如果存在重叠
 */
static bool match_overlaps_used(const RewriteMatch *match, const int *used_ids, int used_count,
                                int node_binding_pair_count) {
    for (int i = 0; i < node_binding_pair_count; i++) {
        int graph_node_id = match->node_bindings[i * 2 + 1];
        if (graph_node_id < 0)
            continue;
        for (int u = 0; u < used_count; u++) {
            if (graph_node_id == used_ids[u]) {
                return true;
            }
        }
    }
    return false;
}

/**
 * @brief 将匹配中的所有图节点 ID 添加到已使用集合中
 *
 * @param match                   匹配指针
 * @param used_ids               已使用节点 ID 数组指针
 * @param used_count             已使用节点数量指针
 * @param used_capacity          容量指针
 * @param node_binding_pair_count 节点绑定对数量
 */
static void add_match_to_used(const RewriteMatch *match, int **used_ids, int *used_count, int *used_capacity,
                              int node_binding_pair_count) {
    for (int i = 0; i < node_binding_pair_count; i++) {
        int graph_node_id = match->node_bindings[i * 2 + 1];
        if (graph_node_id < 0)
            continue;

        /* 检查是否已在集合中 */
        bool already = false;
        for (int u = 0; u < *used_count; u++) {
            if ((*used_ids)[u] == graph_node_id) {
                already = true;
                break;
            }
        }
        if (already)
            continue;

        /* 扩容 */
        if (*used_count >= *used_capacity) {
            int new_cap = *used_capacity > 0 ? *used_capacity * 2 : 16;
            int *new_arr = lv_realloc(*used_ids, (size_t) new_cap * sizeof(int));
            if (!new_arr) {
                debug_log_rewrite("内存分配失败：无法扩展 used_ids 数组");
                return;
            }
            *used_ids = new_arr;
            *used_capacity = new_cap;
        }
        (*used_ids)[(*used_count)++] = graph_node_id;
    }
}

int find_all_non_overlapping_matches(ConstraintGraph *graph, RewriteRule *rule, const int *used_node_ids,
                                     int used_count, RewriteMatch ***out_matches, int *out_match_count) {
    if (!graph || !rule || !rule->pattern || !out_matches || !out_match_count)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM,
                        "find_all_non_overlapping_matches: null param (graph=%p, rule=%p, out_matches=%p, out_match_count=%p)",
                        (const void *)graph, (const void *)rule, (const void *)out_matches, (const void *)out_match_count);

    *out_matches = NULL;
    *out_match_count = 0;

    RewritePattern *pat = rule->pattern;
    if (pat->var_count == 0)
        return 0;

    /* 初始化本地已使用节点集合（合并外部传入的已使用节点） */
    int local_used_capacity = used_count > 0 ? used_count + 16 : 16;
    int *local_used = lv_malloc((size_t) local_used_capacity * sizeof(int));
    if (!local_used)
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "find_all_non_overlapping_matches: lv_malloc for local_used failed (cap=%d)",
                        local_used_capacity);
    int local_used_count = 0;

    /* 复制外部传入的已使用节点 */
    for (int i = 0; i < used_count; i++) {
        if (local_used_count >= local_used_capacity) {
            int new_cap = local_used_capacity * 2;
            int *new_arr = lv_realloc(local_used, (size_t) new_cap * sizeof(int));
            if (!new_arr) {
                lv_free((void **) &local_used);
                lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY,
                                "find_all_non_overlapping_matches: lv_realloc for local_used failed (new_cap=%d)", new_cap);
            }
            local_used = new_arr;
            local_used_capacity = new_cap;
        }
        local_used[local_used_count++] = used_node_ids[i];
    }

    /* 创建图快照，以便在搜索过程中临时移除已匹配节点 */
    GraphSnapshot *snapshot = graph_snapshot_create(graph);
    if (!snapshot) {
        lv_free((void **) &local_used);
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "find_all_non_overlapping_matches: graph_snapshot_create failed");
    }

    /* 匹配结果数组 */
    int match_capacity = 8;
    RewriteMatch **matches = lv_malloc((size_t) match_capacity * sizeof(RewriteMatch *));
    if (!matches) {
        graph_snapshot_destroy(snapshot);
        lv_free((void **) &local_used);
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY,
                        "find_all_non_overlapping_matches: lv_malloc for matches failed (cap=%d)", match_capacity);
    }
    int match_count = 0;

    /* 计算节点绑定对的数量：pattern 的 var_count */
    int node_binding_pairs = pat->var_count;

    /* 反复搜索，直到找不到新的非重叠匹配 */
    int max_iterations = graph->node_count + 1; /* 防止无限循环 */
    int iteration = 0;

    while (iteration < max_iterations) {
        iteration++;

        /* 使用 VF2 在当前图状态中查找一个匹配 */
        RewriteMatch *match = vf2_find_match(graph, pat, false);
        if (!match)
            break;

        /* 检查前置条件 */
        if (!evaluate_precondition(graph, rule, match)) {
            lv_free((void **) &match->node_bindings);
            lv_free((void **) &match->constraint_bindings);
            lv_free((void **) &match);
            break; /* 前置条件失败，停止搜索 */
        }

        /* 检查匹配是否与已使用节点重叠 */
        if (match_overlaps_used(match, local_used, local_used_count, node_binding_pairs)) {
            /* 匹配与已使用节点重叠 -- 需要移除已使用的节点后重新搜索。
             * 从图中移除已使用的节点，然后继续循环。 */
            for (int u = 0; u < local_used_count; u++) {
                graph_remove_node(graph, local_used[u]);
            }
            /* 清空本地已使用集合（已从图中移除） */
            local_used_count = 0;

            lv_free((void **) &match->node_bindings);
            lv_free((void **) &match->constraint_bindings);
            lv_free((void **) &match);
            continue;
        }

        /* 找到一个有效的非重叠匹配 -- 保存它 */
        if (match_count >= match_capacity) {
            int new_cap = match_capacity * 2;
            RewriteMatch **new_arr = lv_realloc(matches, (size_t) new_cap * sizeof(RewriteMatch *));
            if (!new_arr) {
                lv_free((void **) &match->node_bindings);
                lv_free((void **) &match->constraint_bindings);
                lv_free((void **) &match);
                break;
            }
            matches = new_arr;
            match_capacity = new_cap;
        }
        matches[match_count++] = match;

        /* 将此匹配的节点添加到已使用集合 */
        add_match_to_used(match, &local_used, &local_used_count, &local_used_capacity, node_binding_pairs);

        /* 从图中移除已匹配的节点，以便下次搜索不会找到重叠匹配 */
        for (int i = 0; i < node_binding_pairs; i++) {
            int graph_node_id = match->node_bindings[i * 2 + 1];
            if (graph_node_id >= 0) {
                graph_remove_node(graph, graph_node_id);
            }
        }
    }

    /* 从快照恢复原始图 */
    if (!graph_snapshot_restore(snapshot, graph)) {
        /* 恢复失败：图已被重置为空图状态，这是一个严重错误。
         * 释放所有已找到的匹配结果并返回错误。 */
        LOG_ERROR("rewrite", "find_all_non_overlapping_matches: 图快照恢复失败，图已被重置为空图");
        graph_snapshot_destroy(snapshot);
        for (int i = 0; i < match_count; i++) {
            lv_free((void **) &matches[i]->node_bindings);
            lv_free((void **) &matches[i]->constraint_bindings);
            lv_free((void **) &matches[i]);
        }
        lv_free((void **) &matches);
        lv_free((void **) &local_used);
        *out_matches = NULL;
        *out_match_count = 0;
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "find_all_non_overlapping_matches: graph snapshot restore failed");
    }
    graph_snapshot_destroy(snapshot);

    /* 按匹配质量排序（匹配约束数降序） */
    if (match_count > 1) {
        qsort(matches, (size_t) match_count, sizeof(RewriteMatch *), match_quality_cmp);
    }

    lv_free((void **) &local_used);

    *out_matches = matches;
    *out_match_count = match_count;
    return 0;
}

/* ===========================================================================
 * 批量应用非重叠匹配
 *
 * 对一组非重叠匹配依次应用重写规则。对每个匹配创建图快照，
 * 尝试应用替换。如果替换产生冲突或失败，回滚到快照状态并跳过。
 * 返回成功应用的替换数量。
 *
 * 设计规范参考：design_v2.9.md Section 6.4
 * ===========================================================================
 */

int rewrite_apply_all_matches(ConstraintGraph *graph, RewriteRule *rule, RewriteMatch *matches, int match_count,
                              int *out_applied_count) {
    if (!graph || !rule || !matches || match_count <= 0 || !out_applied_count)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM,
                        "rewrite_apply_all_matches: invalid params (graph=%p, rule=%p, matches=%p, count=%d, out=%p)",
                        (const void *)graph, (const void *)rule, (const void *)matches, match_count,
                        (const void *)out_applied_count);

    *out_applied_count = 0;

    /* 记录已被前序替换修改过的节点 ID，用于冲突检测 */
    int modified_capacity = 64;
    int *modified_node_ids = lv_malloc((size_t) modified_capacity * sizeof(int));
    if (!modified_node_ids)
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY,
                        "rewrite_apply_all_matches: lv_malloc for modified_node_ids failed (cap=%d)", modified_capacity);
    int modified_count = 0;

    int applied = 0;

    for (int m = 0; m < match_count; m++) {
        RewriteMatch *match = &matches[m];

        /* 检查此匹配的节点是否已被前序替换修改过 */
        bool conflict = false;
        int node_binding_pairs = rule->pattern ? rule->pattern->var_count : 0;
        for (int i = 0; i < node_binding_pairs; i++) {
            int graph_node_id = match->node_bindings[i * 2 + 1];
            if (graph_node_id < 0)
                continue;

            /* 检查节点是否仍然存在于图中 */
            if (!graph_get_node(graph, graph_node_id)) {
                conflict = true;
                break;
            }

            /* 检查节点是否已被修改 */
            for (int k = 0; k < modified_count; k++) {
                if (modified_node_ids[k] == graph_node_id) {
                    conflict = true;
                    break;
                }
            }
            if (conflict)
                break;
        }

        if (conflict) {
            /* 跳过此匹配 -- 与前序替换冲突 */
            continue;
        }

        /* 创建图快照（用于回滚） */
        GraphSnapshot *snap = graph_snapshot_create(graph);
        if (!snap) {
            /* 快照创建失败，停止后续处理 */
            break;
        }

        /* 尝试应用替换 */
        RewriteStatus status = apply_rewrite(graph, rule, match);

        if (status == REWRITE_APPLIED) {
            /* 应用成功 -- 记录被修改的节点 */
            for (int i = 0; i < node_binding_pairs; i++) {
                int graph_node_id = match->node_bindings[i * 2 + 1];
                if (graph_node_id < 0)
                    continue;

                if (modified_count >= modified_capacity) {
                    int new_cap = modified_capacity * 2;
                    int *new_arr = lv_realloc(modified_node_ids, (size_t) new_cap * sizeof(int));
                    if (!new_arr) {
                        debug_log_rewrite("内存分配失败：无法扩展 modified_node_ids 数组");
                        break;
                    }
                    modified_node_ids = new_arr;
                    modified_capacity = new_cap;
                }
                modified_node_ids[modified_count++] = graph_node_id;
            }
            applied++;
        } else {
            /* 应用失败 -- apply_rewrite 内部已通过快照回滚，
             * 无需额外恢复操作 */
            (void) snap;
        }

        /* 注意：apply_rewrite 内部会创建并销毁自己的快照。
         * 此处的快照用于检测 apply_rewrite 是否真正修改了图。
         * 由于 apply_rewrite 在失败时已回滚，我们只需销毁此快照。 */
        graph_snapshot_destroy(snap);
    }

    lv_free((void **) &modified_node_ids);
    *out_applied_count = applied;
    return 0;
}
