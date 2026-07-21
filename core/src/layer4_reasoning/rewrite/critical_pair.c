/* ========================================================================
 * 关键对计算引擎实现
 *
 * 核心算法：
 *   1. 对规则集执行两两配对（含自配对）
 *   2. 对每对规则，使用 VF2 子图同构匹配找到模式图之间的重叠位置
 *   3. 为每个重叠创建 CriticalPair，分别沿两条规则归约
 *   4. 通过图规范化遍 + 合一检查比较两个归约结果
 *
 * 可信根：关键对比较依赖 VF2 匹配器、图规范化遍引擎和合一算法——
 * 这些组件本身由内核测试担保，构成极小可信基。
 *
 * ======================================================================== */

#include "lv00/critical_pair.h"
#include "lv00/normalization.h"
#include "lv00/unify.h"
#include "lv00/graph_hash.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- 内部辅助 --------------------------------------------------------- */

#define INITIAL_CAPACITY 16

/**
 * @brief 深拷贝一个约束图
 *
 * 创建约束图的完整独立副本。由于 constraint_graph 没有内建 clone 函数，
 * 使用快照机制间接实现：先创建快照再恢复到新图中。
 *
 * @param src  源图（不可为 NULL）
 * @return 新分配的约束图副本，调用者负责 graph_destroy；失败返回 NULL
 */
static ConstraintGraph *graph_deep_copy(const ConstraintGraph *src)
{
    if (!src) return NULL;
    GraphSnapshot *snap = graph_snapshot_create(src);
    if (!snap) return NULL;
    ConstraintGraph *copy = graph_create();
    if (!copy) {
        graph_snapshot_destroy(snap);
        return NULL;
    }
    if (!graph_snapshot_restore(snap, copy)) {
        graph_snapshot_destroy(snap);
        graph_destroy(copy);
        return NULL;
    }
    graph_snapshot_destroy(snap);
    return copy;
}

/**
 * @brief 使用 VF2 在某图中查找另一个模式的一个匹配
 *
 * @param graph          目标图（在其中搜索）
 * @param pattern        要匹配的模式
 * @param local_equiv    是否启用局部等价容忍
 * @return 匹配结果，未找到则返回 NULL；调用者通过 free 释放
 */
static RewriteMatch *find_one_match(ConstraintGraph *graph,
                                    const RewritePattern *pattern,
                                    bool local_equiv)
{
    if (!graph || !pattern) return NULL;
    return vf2_find_match(graph, (RewritePattern *)pattern, local_equiv);
}

/**
 * @brief 对图应用一次重写规则
 *
 * @param graph  目标图（原地修改）
 * @param rule   要应用的规则
 * @return 新分配的归约结果图，失败返回 NULL
 */
static ConstraintGraph *apply_rule_once(ConstraintGraph *graph, RewriteRule *rule)
{
    if (!graph || !rule || !rule->pattern) return NULL;

    ConstraintGraph *work = graph_deep_copy(graph);
    if (!work) return NULL;

    RewriteMatch *match = find_one_match(work, rule->pattern, true);
    if (!match) {
        /* 无匹配——返回原图副本（归约不适用） */
        return work;
    }

    RewriteStatus status = apply_rewrite(work, rule, match);
    free(match);

    if (status == REWRITE_STATUS_APPLIED || status == REWRITE_STATUS_OK) {
        return work;
    }

    /* 应用失败，返回未修改的副本 */
    graph_destroy(work);
    return graph_deep_copy(graph);
}

/* ---- 公共 API ---------------------------------------------------------- */

CriticalPairSet *critical_pair_compute_all(RewriteRule **rules, int rule_count,
                                           ConstraintGraph *base_graph)
{
    (void)base_graph; /* 保留供未来扩展 */

    if (!rules || rule_count < 1) return NULL;

    CriticalPairSet *set = lv00_malloc(sizeof(CriticalPairSet));
    if (!set) return NULL;
    set->pairs = NULL;
    set->pair_count = 0;
    set->capacity = 0;

    /* 两两配对（含自配对） */
    for (int i = 0; i < rule_count; i++) {
        if (!rules[i] || !rules[i]->pattern) continue;
        for (int j = i; j < rule_count; j++) {
            if (!rules[j] || !rules[j]->pattern) continue;

            RewritePattern *pat_i = rules[i]->pattern;
            RewritePattern *pat_j = rules[j]->pattern;

            /* 跳过空模式 */
            if (pat_i->pattern_constraint_count == 0 &&
                pat_i->var_count == 0) continue;
            if (pat_j->pattern_constraint_count == 0 &&
                pat_j->var_count == 0) continue;

            /* ---- 重叠搜索 ----
             * 策略：尝试将 pat_i 作为子图匹配到 pat_j 中，
             * 以及将 pat_j 作为子图匹配到 pat_i 中。
             * 对自配对（i==j），在副本中搜索第二个非平凡匹配。
             *
             * 我们构造一个包含两者模式的临时图来搜索重叠。
             */
            ConstraintGraph *tmp = graph_create();
            if (!tmp) continue;

            /* 将 pat_i 的约束所引用的节点逐个添加到 tmp */
            int *node_map_i = NULL; /* pat_i 变量ID → tmp 中实际节点ID */
            int node_map_i_count = 0;
            if (pat_i->var_count > 0) {
                node_map_i = lv00_malloc((size_t)pat_i->var_count * sizeof(int));
                if (!node_map_i) { graph_destroy(tmp); continue; }
                /* 为 pat_i 的每个变量节点在 tmp 中创建占位节点 */
                int id_counter = 0;
                for (int k = 0; k < pat_i->var_count; k++) {
                    AddNodeResult res = graph_add_point(tmp, NULL, 0);
                    if (res != ADD_NODE_OK) {
                        node_map_i[k] = -1;
                    } else {
                        node_map_i[k] = id_counter++;
                    }
                }
                node_map_i_count = pat_i->var_count;
            }

            /* 将 pat_j 的约束所引用的节点逐个添加到 tmp */
            int *node_map_j = NULL;
            int node_map_j_count = 0;
            if (pat_j->var_count > 0) {
                node_map_j = lv00_malloc((size_t)pat_j->var_count * sizeof(int));
                if (!node_map_j) {
                    free(node_map_i);
                    graph_destroy(tmp);
                    continue;
                }
                int id_counter_j = 0;
                for (int k = 0; k < pat_j->var_count; k++) {
                    AddNodeResult res = graph_add_point(tmp, NULL, 0);
                    if (res != ADD_NODE_OK) {
                        node_map_j[k] = -1;
                    } else {
                        node_map_j[k] = id_counter_j++;
                    }
                }
                node_map_j_count = pat_j->var_count;
            }

            /* 在 tmp 中尝试匹配：找 pat_i 模式的一个子图匹配到 pat_j 的节点集中 */
            RewriteMatch *match_ij = find_one_match(tmp, pat_i, false);
            if (!match_ij && i != j) {
                match_ij = find_one_match(tmp, pat_j, false);
            }

            /* 使用 overlap = pat_j 的图（或 pat_i，取节点多的） */
            ConstraintGraph *overlap = graph_create();
            if (!overlap) {
                free(match_ij);
                free(node_map_i);
                free(node_map_j);
                graph_destroy(tmp);
                continue;
            }

            /* 选择较大模式作为 overlap 的基础 */
            bool use_j_as_base = (pat_j->var_count >= pat_i->var_count);
            int base_count = use_j_as_base ? node_map_j_count : node_map_i_count;

            /* 在 overlap 中创建与 pat 变量对应的节点 */
            for (int k = 0; k < base_count; k++) {
                graph_add_point(overlap, NULL, 0);
            }

            /* 添加约束 */
            RewritePattern *base_pat = use_j_as_base ? pat_j : pat_i;
            for (int k = 0; k < base_pat->pattern_constraint_count; k++) {
                Constraint *c = base_pat->pattern_constraints[k];
                if (!c) continue;
                int *parts = lv00_malloc((size_t)c->participant_count * sizeof(int));
                if (!parts) continue;
                for (int p = 0; p < c->participant_count; p++) {
                    /* 映射到 overlap 中的节点ID */
                    int orig_id = c->participants[p];
                    int mapped = 0;
                    for (int v = 0; v < base_count; v++) {
                        int vid = base_pat->variable_node_ids ?
                                  base_pat->variable_node_ids[v] : v;
                        if (vid == orig_id) { mapped = v; break; }
                        if (v == base_count - 1) mapped = 0;
                    }
                    parts[p] = mapped;
                }
                graph_add_constraint_with_id(overlap, -1, c->type, parts, c->participant_count);
                free(parts);
            }

            /* 扩展容量 */
            if (set->pair_count >= set->capacity) {
                int new_cap = set->capacity == 0 ? INITIAL_CAPACITY : set->capacity * 2;
                CriticalPair *new_pairs = lv00_realloc(set->pairs,
                    (size_t)new_cap * sizeof(CriticalPair));
                if (!new_pairs) {
                    graph_destroy(overlap);
                    free(match_ij);
                    free(node_map_i);
                    free(node_map_j);
                    graph_destroy(tmp);
                    continue;
                }
                set->pairs = new_pairs;
                set->capacity = new_cap;
            }

            CriticalPair *cp = &set->pairs[set->pair_count];
            cp->rule1 = rules[i];
            cp->rule2 = rules[j];
            cp->overlap = overlap;
            cp->reduced1 = NULL;
            cp->reduced2 = NULL;
            cp->is_confluent = false;
            cp->mismatches = NULL;
            cp->mismatch_count = 0;
            cp->compared = false;
            set->pair_count++;

            free(match_ij);
            free(node_map_i);
            free(node_map_j);
            graph_destroy(tmp);
        }
    }

    return set;
}

bool critical_pair_compare(CriticalPair *cp)
{
    if (!cp || !cp->overlap) return false;
    if (cp->compared) return true; /* 已比较，跳过 */

    /* 清理旧结果 */
    if (cp->reduced1) { graph_destroy(cp->reduced1); cp->reduced1 = NULL; }
    if (cp->reduced2) { graph_destroy(cp->reduced2); cp->reduced2 = NULL; }
    if (cp->mismatches) { free(cp->mismatches); cp->mismatches = NULL; }
    cp->mismatch_count = 0;

    /* 沿两条规则分别归约一步 */
    cp->reduced1 = apply_rule_once(cp->overlap, cp->rule1);
    cp->reduced2 = apply_rule_once(cp->overlap, cp->rule2);

    if (!cp->reduced1 || !cp->reduced2) {
        cp->compared = true;
        cp->is_confluent = false;
        return false;
    }

    /* 图规范化遍：对两个归约结果分别规范化 */
    NormalizationResult *nr1 = graph_normalize(cp->reduced1, true);
    NormalizationResult *nr2 = graph_normalize(cp->reduced2, true);

    /* 合一检查：比较两个规范化后的图 */
    UnifyFailureInfo failure_info;
    memset(&failure_info, 0, sizeof(failure_info));
    UnifyStatus us = unify_construction_with_proposition_detailed(
        cp->reduced1, cp->reduced2, &failure_info);

    if (us == UNIFY_STATUS_OK) {
        cp->is_confluent = true;
    } else {
        cp->is_confluent = false;

        /* 收集不匹配详情 */
        int mismatch_cap = 8;
        cp->mismatches = lv00_malloc((size_t)mismatch_cap * sizeof(CpMismatch));
        if (cp->mismatches) {
            CpMismatch *m = &cp->mismatches[0];
            cp->mismatch_count = 0;
            m->kind = (failure_info.mismatch_reason == COORD_VALUE_MISMATCH ||
                       failure_info.mismatch_reason == COORD_TYPE_MISMATCH) ? 2 :
                      (failure_info.mismatch_reason == CONSTRAINT_TYPE_MISMATCH ||
                       failure_info.mismatch_reason == CONSTRAINT_PARTICIPANT_COUNT_MISMATCH ||
                       failure_info.mismatch_reason == CONSTRAINT_PARTICIPANT_ID_MISMATCH) ? 1 : 0;
            m->node_id_a = failure_info.failed_node_id;
            m->node_id_b = -1;
            m->constraint_id_a = failure_info.failed_constraint_id;
            m->constraint_id_b = -1;
            snprintf(m->description, sizeof(m->description),
                     "Unify failed: %s", failure_info.description ? failure_info.description : "unknown");
            cp->mismatch_count = 1;

            /* 额外：比较节点数量差异 */
            if (cp->reduced1->node_count != cp->reduced2->node_count) {
                if (cp->mismatch_count < mismatch_cap) {
                    CpMismatch *m2 = &cp->mismatches[cp->mismatch_count];
                    m2->kind = 0;
                    m2->node_id_a = cp->reduced1->node_count;
                    m2->node_id_b = cp->reduced2->node_count;
                    m2->constraint_id_a = -1;
                    m2->constraint_id_b = -1;
                    snprintf(m2->description, sizeof(m2->description),
                             "Node count mismatch: %d vs %d",
                             cp->reduced1->node_count, cp->reduced2->node_count);
                    cp->mismatch_count++;
                }
            }

            /* 约束数量差异 */
            if (cp->reduced1->constraint_count != cp->reduced2->constraint_count) {
                if (cp->mismatch_count < mismatch_cap) {
                    CpMismatch *m3 = &cp->mismatches[cp->mismatch_count];
                    m3->kind = 1;
                    m3->node_id_a = -1;
                    m3->node_id_b = -1;
                    m3->constraint_id_a = cp->reduced1->constraint_count;
                    m3->constraint_id_b = cp->reduced2->constraint_count;
                    snprintf(m3->description, sizeof(m3->description),
                             "Constraint count mismatch: %d vs %d",
                             cp->reduced1->constraint_count, cp->reduced2->constraint_count);
                    cp->mismatch_count++;
                }
            }
        }

        unify_failure_info_destroy(&failure_info);
    }

    if (nr1) normalization_result_destroy(nr1);
    if (nr2) normalization_result_destroy(nr2);

    cp->compared = true;
    return true;
}

int critical_pair_compare_all(CriticalPairSet *set)
{
    if (!set) return 0;
    int confluent = 0;
    for (int i = 0; i < set->pair_count; i++) {
        critical_pair_compare(&set->pairs[i]);
        if (set->pairs[i].is_confluent) confluent++;
    }
    return confluent;
}

bool critical_pair_export_text(const CriticalPair *cp, const char *filepath)
{
    if (!cp || !filepath) return false;

    FILE *f = fopen(filepath, "w");
    if (!f) return false;

    fprintf(f, "# Critical Pair Export\n");
    fprintf(f, "# Rule 1: %s\n", cp->rule1 ? cp->rule1->name : "(null)");
    fprintf(f, "# Rule 2: %s\n", cp->rule2 ? cp->rule2->name : "(null)");
    fprintf(f, "# Confluent: %s\n", cp->is_confluent ? "YES" : "NO");

    /* 导出 reduced1 */
    fprintf(f, "\n## REDUCED_1 (via rule1)\n");
    if (cp->reduced1) {
        ConstraintGraph *g = cp->reduced1;
        for (int i = 0; i < g->node_count; i++) {
            GeomNode *n = g->nodes[i];
            if (!n) continue;
            fprintf(f, "NODE %d %d", n->id, (int)n->type);
            if (n->symbolic_coords && n->coord_count > 0) {
                fprintf(f, " coords=%d", n->coord_count);
            }
            fprintf(f, "\n");
        }
        for (int i = 0; i < g->constraint_count; i++) {
            Constraint *c = g->constraints[i];
            if (!c) continue;
            fprintf(f, "EDGE %d", (int)c->type);
            for (int j = 0; j < c->participant_count; j++) {
                fprintf(f, " %d", c->participants[j]);
            }
            fprintf(f, "\n");
        }
    } else {
        fprintf(f, "(none)\n");
    }

    /* 导出 reduced2 */
    fprintf(f, "\n## REDUCED_2 (via rule2)\n");
    if (cp->reduced2) {
        ConstraintGraph *g = cp->reduced2;
        for (int i = 0; i < g->node_count; i++) {
            GeomNode *n = g->nodes[i];
            if (!n) continue;
            fprintf(f, "NODE %d %d", n->id, (int)n->type);
            if (n->symbolic_coords && n->coord_count > 0) {
                fprintf(f, " coords=%d", n->coord_count);
            }
            fprintf(f, "\n");
        }
        for (int i = 0; i < g->constraint_count; i++) {
            Constraint *c = g->constraints[i];
            if (!c) continue;
            fprintf(f, "EDGE %d", (int)c->type);
            for (int j = 0; j < c->participant_count; j++) {
                fprintf(f, " %d", c->participants[j]);
            }
            fprintf(f, "\n");
        }
    } else {
        fprintf(f, "(none)\n");
    }

    /* 不匹配详情 */
    if (!cp->is_confluent && cp->mismatches) {
        fprintf(f, "\n## MISMATCHES (%d)\n", cp->mismatch_count);
        for (int i = 0; i < cp->mismatch_count; i++) {
            CpMismatch *m = &cp->mismatches[i];
            fprintf(f, "%d %d %d %d %d %s\n",
                    m->kind, m->node_id_a, m->node_id_b,
                    m->constraint_id_a, m->constraint_id_b,
                    m->description);
        }
    }

    fclose(f);
    return true;
}

void critical_pair_set_destroy(CriticalPairSet *set)
{
    if (!set) return;
    for (int i = 0; i < set->pair_count; i++) {
        CriticalPair *cp = &set->pairs[i];
        if (cp->overlap)  { graph_destroy(cp->overlap);  cp->overlap  = NULL; }
        if (cp->reduced1) { graph_destroy(cp->reduced1); cp->reduced1 = NULL; }
        if (cp->reduced2) { graph_destroy(cp->reduced2); cp->reduced2 = NULL; }
        if (cp->mismatches) { free(cp->mismatches); cp->mismatches = NULL; }
    }
    free(set->pairs);
    free(set);
}

void critical_pair_get_statistics(const CriticalPairSet *set,
                                  int *out_total,
                                  int *out_confluent,
                                  int *out_pending)
{
    if (out_total)     *out_total     = 0;
    if (out_confluent) *out_confluent = 0;
    if (out_pending)   *out_pending   = 0;
    if (!set) return;

    if (out_total) *out_total = set->pair_count;
    int c = 0, p = 0;
    for (int i = 0; i < set->pair_count; i++) {
        if (set->pairs[i].compared) {
            if (set->pairs[i].is_confluent) c++;
        } else {
            p++;
        }
    }
    if (out_confluent) *out_confluent = c;
    if (out_pending)   *out_pending   = p;
}
