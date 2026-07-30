/* ============================================================================
 * 关键对计算引擎实现
 *
 * 核心算法：
 *   1. 对规则集执行两两配对（含自配对）
 *   2. 对每对规则，构造包含两者模式变量的临时图来搜索重叠位置
 *   3. 为每个重叠创建 CriticalPair，记录两条规则与统一子
 *   4. critical_pair_compare 分别沿两条规则归约一步，通过规范化 + 合一比较
 *
 * 可信基：依赖 VF2 子图匹配器、图规范化遍引擎和合一算法——
 * 这些组件由内核测试担保，构成极小可信基。
 *
 * 设计文档参考：§3.6 图重写引擎 · 关键对计算
 *
 * ============================================================================ */

#include "lv/critical_pair.h"

#include "lv/lv_file.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "lv/graph_hash.h"
#include "lv/normalization.h"
#include "lv/unify.h"


/* ---- 内部常量 ------------------------------------------------------------- */

/** @brief 关键对集合初始容量 */
#define INITIAL_PAIR_CAPACITY 16
/** @brief 不匹配详情的最大记录数（超过则截断，在 stderr 报告） */
#define MAX_MISMATCHES 8

/* ============================================================================
 * 内部辅助函数
 * ============================================================================ */

/**
 * @brief 通过快照机制深拷贝一个约束图
 *
 * ConstraintGraph 没有内建 clone，使用 GraphSnapshot 间接实现：
 * 将源图序列化为快照，再恢复到新图中。
 *
 * @param src  源约束图（不可为 NULL）
 * @return 独立副本，调用者负责 graph_destroy；失败返回 NULL
 */
static ConstraintGraph *graph_deep_copy(const ConstraintGraph *src) {
    if (!src)
        return NULL;
    GraphSnapshot *snap = graph_snapshot_create(src);
    if (!snap)
        return NULL;
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
 * @brief 使用 VF2 在图模式中查找一个匹配
 *
 * @param graph       目标约束图
 * @param pattern     要匹配的重写模式
 * @param local_equiv 是否启用局部等价容忍（允许近等价结构匹配）
 * @return 找到的匹配（调用者负责 lv_free），未找到返回 NULL
 */
static RewriteMatch *find_one_vf2_match(ConstraintGraph *graph, const RewritePattern *pattern, bool local_equiv) {
    if (!graph || !pattern)
        return NULL;
    return vf2_find_match(graph, (RewritePattern *) pattern, local_equiv);
}

/**
 * @brief 对图应用一次重写规则，返回归约结果
 *
 * 在深拷贝的工作图上查找规则模式并应用。
 * 若无匹配则返回原图副本（归约不适用）。
 *
 * @param graph  被归约的约束图（只读，不修改）
 * @param rule   要应用的重写规则
 * @return 归约后的独立图，调用者负责 graph_destroy；失败返回 NULL
 */
static ConstraintGraph *apply_rule_once(ConstraintGraph *graph, RewriteRule *rule) {
    if (!graph || !rule || !rule->pattern)
        return NULL;

    ConstraintGraph *work = graph_deep_copy(graph);
    if (!work)
        return NULL;

    RewriteMatch *match = find_one_vf2_match(work, rule->pattern, true);
    if (!match) {
        /* 无匹配 → 归约不适用，返回原图副本 */
        return work;
    }

    RewriteStatus status = apply_rewrite(work, rule, match);
    lv_free(match);

    if (status == REWRITE_STATUS_APPLIED || status == REWRITE_STATUS_OK) {
        return work;
    }

    /* 归约失败，返回未修改的副本并丢弃工作图 */
    graph_destroy(work);
    return graph_deep_copy(graph);
}

/**
 * @brief 计算变量的原始 ID（variable_node_ids 中第 idx 项的 ID）
 *
 * 若 variable_node_ids 为 NULL 则回退使用索引本身作为 ID。
 */
static inline int var_orig_id(const RewritePattern *pat, int idx) {
    return pat->variable_node_ids ? pat->variable_node_ids[idx] : idx;
}

/**
 * @brief 将原始约束图的节点 ID 映射到重叠图中的本地索引
 *
 * 在 variable_node_ids 中搜索 orig_id，返回对应索引。
 * 若未找到则返回 0（默认回退）。
 */
static int map_orig_id_to_local(const RewritePattern *pat, int orig_id, int base_count) {
    for (int v = 0; v < base_count; v++) {
        if (var_orig_id(pat, v) == orig_id)
            return v;
    }
    return 0;
}

/**
 * @brief 导出单个归约结果图的节点和边到文件
 *
 * @param f     输出文件句柄
 * @param g     要导出的约束图
 * @param label 标题标签（如 "REDUCED_1 (via rule1)"）
 */
static void export_graph_to_text(FILE *f, const ConstraintGraph *g, const char *label) {
    fprintf(f, "\n## %s\n", label);
    if (!g) {
        fprintf(f, "(none)\n");
        return;
    }

    for (int i = 0; i < g->node_count; i++) {
        GeomNode *n = g->nodes[i];
        if (!n)
            continue;
        fprintf(f, "NODE %d %d", n->id, (int) n->type);
        if (n->symbolic_coords && n->coord_count > 0) {
            fprintf(f, " coords=%d", n->coord_count);
        }
        fprintf(f, "\n");
    }
    for (int i = 0; i < g->constraint_count; i++) {
        Constraint *c = g->constraints[i];
        if (!c)
            continue;
        fprintf(f, "EDGE %d", (int) c->type);
        for (int j = 0; j < c->participant_count; j++) {
            fprintf(f, " %d", c->participants[j]);
        }
        fprintf(f, "\n");
    }
}

/**
 * @brief 填充不匹配详情条目
 *
 * @param m              输出的不匹配条目
 * @param kind           不匹配类型
 * @param na             归约结果 A 的节点/约束 ID
 * @param nb             归约结果 B 的节点/约束 ID
 * @param ca             归约结果 A 的约束 ID
 * @param cb             归约结果 B 的约束 ID
 * @param fmt            描述格式字符串（及后续参数）
 */
static void fill_mismatch(CpMismatch *m, int kind, int na, int nb, int ca, int cb, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    m->kind = kind;
    m->node_id_a = na;
    m->node_id_b = nb;
    m->constraint_id_a = ca;
    m->constraint_id_b = cb;
    vsnprintf(m->description, sizeof(m->description), fmt, args);
    va_end(args);
}

/* ============================================================================
 * 公共 API
 * ============================================================================ */

CriticalPairSet *critical_pair_compute_all(RewriteRule **rules, int rule_count, ConstraintGraph *base_graph) {
    (void) base_graph; /* 保留参数供未来扩展（全局命名空间等） */

    if (!rules || rule_count < 1)
        return NULL;

    CriticalPairSet *set = lv_calloc(1, sizeof(CriticalPairSet));
    if (!set)
        return NULL;
    set->pairs = NULL;
    set->pair_count = 0;
    set->capacity = 0;

    /* ---- 两两配对：对每对规则（含自配对）计算关键对 ---- */
    for (int i = 0; i < rule_count; i++) {
        if (!rules[i] || !rules[i]->pattern)
            continue;
        for (int j = i; j < rule_count; j++) {
            if (!rules[j] || !rules[j]->pattern)
                continue;

            RewritePattern *pat_i = rules[i]->pattern;
            RewritePattern *pat_j = rules[j]->pattern;

            /* 跳过空模式（无变量节点且无模式约束） */
            if (pat_i->var_count == 0 && pat_i->pattern_constraint_count == 0)
                continue;
            if (pat_j->var_count == 0 && pat_j->pattern_constraint_count == 0)
                continue;

            /*
             * ---- 构造重叠图 ----
             * 核心思路：在临时图中为两个模式的变量节点分别创建占位节点，
             * 然后尝试用 VF2 找到 pat_i 匹配到 pat_j 节点集中的位置。
             * 若匹配存在，则 pat_j 的图包含了 pat_i 作为子图 → 形成关键重叠。
             */

            /* 步骤 1：构造临时图 tmp，添加 pat_i 和 pat_j 的变量占位节点 */
            ConstraintGraph *tmp = graph_create();
            if (!tmp)
                continue;

            /* 在 tmp 中为 pat_i 的每个变量节点创建占位点 */
            int *node_map_i = NULL;
            int node_map_i_count = 0;
            if (pat_i->var_count > 0) {
                node_map_i = lv_malloc((size_t) pat_i->var_count * sizeof(int));
                if (!node_map_i) {
                    graph_destroy(tmp);
                    continue;
                }
                for (int k = 0; k < pat_i->var_count; k++) {
                    AddNodeResult res = graph_add_point(tmp, NULL, 0);
                    node_map_i[k] = (res == ADD_NODE_OK) ? k : -1;
                }
                node_map_i_count = pat_i->var_count;
            }

            /* 在 tmp 中为 pat_j 的每个变量节点创建占位点 */
            int *node_map_j = NULL;
            int node_map_j_count = 0;
            if (pat_j->var_count > 0) {
                node_map_j = lv_malloc((size_t) pat_j->var_count * sizeof(int));
                if (!node_map_j) {
                    lv_free(node_map_i);
                    graph_destroy(tmp);
                    continue;
                }
                for (int k = 0; k < pat_j->var_count; k++) {
                    AddNodeResult res = graph_add_point(tmp, NULL, 0);
                    node_map_j[k] = (res == ADD_NODE_OK) ? k : -1;
                }
                node_map_j_count = pat_j->var_count;
            }

            /* 步骤 2：VF2 匹配 —— 尝试在 tmp 中找到 pat_i 匹配 pat_j 的重叠 */
            RewriteMatch *match_ij = find_one_vf2_match(tmp, pat_i, false);
            if (!match_ij && i != j) {
                match_ij = find_one_vf2_match(tmp, pat_j, false);
            }

            /* 步骤 3：创建 overlap 图——取节点数较多者为基础 */
            ConstraintGraph *overlap = graph_create();
            if (!overlap) {
                lv_free(match_ij);
                lv_free(node_map_i);
                lv_free(node_map_j);
                graph_destroy(tmp);
                continue;
            }

            /*
             * 选择较大的模式作为 overlap 的骨架。
             * 在 overlap 中添加骨架模式的变量节点作为占位，
             * 然后将其模式约束逐个映射回添加到 overlap。
             */
            bool use_j_as_base = (pat_j->var_count >= pat_i->var_count);
            int base_count = use_j_as_base ? node_map_j_count : node_map_i_count;
            RewritePattern *base_pat = use_j_as_base ? pat_j : pat_i;

            for (int k = 0; k < base_count; k++) {
                graph_add_point(overlap, NULL, 0);
            }

            for (int k = 0; k < base_pat->pattern_constraint_count; k++) {
                Constraint *c = base_pat->pattern_constraints[k];
                if (!c)
                    continue;
                int *parts = lv_malloc((size_t) c->participant_count * sizeof(int));
                if (!parts)
                    continue;
                for (int p = 0; p < c->participant_count; p++) {
                    parts[p] = map_orig_id_to_local(base_pat, c->participants[p], base_count);
                }
                graph_add_constraint_with_id(overlap, -1, c->type, parts, c->participant_count);
                lv_free((void **) &parts);
            }

            /* 步骤 4：扩展 CriticalPairSet 数组容量 */
            if (set->pair_count >= set->capacity) {
                int new_cap = (set->capacity == 0) ? INITIAL_PAIR_CAPACITY : set->capacity * 2;
                if (set->capacity > 0 && set->capacity > INT_MAX / 2) {
                    graph_destroy(overlap);
                    lv_free((void **) &match_ij);
                    lv_free((void **) &node_map_i);
                    lv_free((void **) &node_map_j);
                    graph_destroy(tmp);
                    continue;
                }
                CriticalPair *new_pairs = lv_realloc(set->pairs, (size_t) new_cap * sizeof(CriticalPair));
                if (!new_pairs) {
                    graph_destroy(overlap);
                    lv_free(match_ij);
                    lv_free(node_map_i);
                    lv_free(node_map_j);
                    graph_destroy(tmp);
                    continue;
                }
                set->pairs = new_pairs;
                set->capacity = new_cap;
            }

            /* 步骤 5：填充关键对条目 */
            CriticalPair *cp = &set->pairs[set->pair_count];
            memset(cp, 0, sizeof(CriticalPair));
            cp->rule1 = rules[i];
            cp->rule2 = rules[j];
            cp->overlap = overlap;
            set->pair_count++;

            lv_free(match_ij);
            lv_free(node_map_i);
            lv_free(node_map_j);
            graph_destroy(tmp);
        }
    }

    return set;
}

bool critical_pair_compare(CriticalPair *cp) {
    if (!cp || !cp->overlap)
        return false;
    if (cp->compared)
        return true; /* 幂等：已比较则直接返回 */

    /* ---- 清理旧比较结果 ---- */
    if (cp->reduced1) {
        graph_destroy(cp->reduced1);
        cp->reduced1 = NULL;
    }
    if (cp->reduced2) {
        graph_destroy(cp->reduced2);
        cp->reduced2 = NULL;
    }
    if (cp->mismatches) {
        lv_free(cp->mismatches);
        cp->mismatches = NULL;
    }
    cp->mismatch_count = 0;

    /* ---- 分别沿两条规则归约一步 ---- */
    cp->reduced1 = apply_rule_once(cp->overlap, cp->rule1);
    cp->reduced2 = apply_rule_once(cp->overlap, cp->rule2);
    if (!cp->reduced1 || !cp->reduced2) {
        cp->compared = true;
        cp->is_confluent = false;
        return false;
    }

    /* ---- 图规范化遍：将两个归约结果分别标准化 ---- */
    NormalizationResult *nr1 = graph_normalize(cp->reduced1, true);
    NormalizationResult *nr2 = graph_normalize(cp->reduced2, true);

    /* ---- 合一检查：比较规范化后的图是否等价 ---- */
    UnifyFailureInfo failure_info;
    memset(&failure_info, 0, sizeof(failure_info));
    UnifyStatus us = unify_construction_with_proposition_detailed(cp->reduced1, cp->reduced2, &failure_info);

    if (us == UNIFY_STATUS_OK) {
        /* 合一成功 → 该关键对汇合 */
        cp->is_confluent = true;
    } else {
        /* 合一失败 → 非汇合，收集不匹配详情 */
        cp->is_confluent = false;
        cp->mismatches = lv_calloc(MAX_MISMATCHES, sizeof(CpMismatch));

        if (cp->mismatches) {
            CpMismatch *m = &cp->mismatches[0];
            cp->mismatch_count = 0;

            /*
             * 转换合一失败原因为不匹配类型码：
             *   0 = 节点结构不匹配
             *   1 = 约束不匹配
             *   2 = 坐标不匹配
             */
            int mk = (failure_info.mismatch_reason == COORD_VALUE_MISMATCH ||
                      failure_info.mismatch_reason == COORD_TYPE_MISMATCH)
                         ? 2
                     : (failure_info.mismatch_reason == CONSTRAINT_TYPE_MISMATCH ||
                        failure_info.mismatch_reason == CONSTRAINT_PARTICIPANT_COUNT_MISMATCH ||
                        failure_info.mismatch_reason == CONSTRAINT_PARTICIPANT_ID_MISMATCH)
                         ? 1
                         : 0;

            fill_mismatch(m, mk, failure_info.failed_node_id, -1, failure_info.failed_constraint_id, -1, "合一失败: %s",
                          failure_info.description ? failure_info.description : "(未知原因)");
            cp->mismatch_count = 1;

            /* 额外：节点数差异 */
            if (cp->reduced1->node_count != cp->reduced2->node_count && cp->mismatch_count < MAX_MISMATCHES) {
                CpMismatch *m2 = &cp->mismatches[cp->mismatch_count];
                fill_mismatch(m2, 0, cp->reduced1->node_count, cp->reduced2->node_count, -1, -1,
                              "节点数不匹配: %d ≠ %d", cp->reduced1->node_count, cp->reduced2->node_count);
                cp->mismatch_count++;
            }

            /* 额外：约束数差异 */
            if (cp->reduced1->constraint_count != cp->reduced2->constraint_count &&
                cp->mismatch_count < MAX_MISMATCHES) {
                CpMismatch *m3 = &cp->mismatches[cp->mismatch_count];
                fill_mismatch(m3, 1, -1, -1, cp->reduced1->constraint_count, cp->reduced2->constraint_count,
                              "约束数不匹配: %d ≠ %d", cp->reduced1->constraint_count, cp->reduced2->constraint_count);
                cp->mismatch_count++;
            }
        }

        unify_failure_info_destroy(&failure_info);
    }

    if (nr1)
        normalization_result_destroy(nr1);
    if (nr2)
        normalization_result_destroy(nr2);

    cp->compared = true;
    return true;
}

int critical_pair_compare_all(CriticalPairSet *set) {
    if (!set)
        return 0;
    int confluent = 0;
    for (int i = 0; i < set->pair_count; i++) {
        critical_pair_compare(&set->pairs[i]);
        if (set->pairs[i].is_confluent)
            confluent++;
    }
    return confluent;
}

bool critical_pair_export_text(const CriticalPair *cp, const char *filepath) {
    if (!cp || !filepath)
        return false;

    FILE *f = lv_file_open(filepath, "w");
    if (!f)
        return false;

    /* ---- 文件头：元信息 ---- */
    fprintf(f, "# Lv-00 关键对导出\n");
    fprintf(f, "# 规则 1: %s\n", cp->rule1 ? cp->rule1->name : "(null)");
    fprintf(f, "# 规则 2: %s\n", cp->rule2 ? cp->rule2->name : "(null)");
    fprintf(f, "# 汇合: %s\n", cp->is_confluent ? "是" : "否");
    fprintf(f, "# 格式: NODE <id> <type> [coords=<n>] | EDGE <type> <participant_ids...>\n");

    /* ---- 两个归约结果（复用同一导出函数） ---- */
    export_graph_to_text(f, cp->reduced1, "REDUCED_1 (沿 rule1)");
    export_graph_to_text(f, cp->reduced2, "REDUCED_2 (沿 rule2)");

    /* ---- 不匹配详情（仅在非汇合时存在） ---- */
    if (!cp->is_confluent && cp->mismatches && cp->mismatch_count > 0) {
        fprintf(f, "\n## 不匹配详情 (%d)\n", cp->mismatch_count);
        for (int i = 0; i < cp->mismatch_count; i++) {
            CpMismatch *m = &cp->mismatches[i];
            fprintf(f, "#%d kind=%d nA=%d nB=%d cA=%d cB=%d %s\n", i, m->kind, m->node_id_a, m->node_id_b,
                    m->constraint_id_a, m->constraint_id_b, m->description);
        }
    }

    lv_file_close(f);
    return true;
}

void critical_pair_set_destroy(CriticalPairSet *set) {
    if (!set)
        return;
    for (int i = 0; i < set->pair_count; i++) {
        CriticalPair *cp = &set->pairs[i];
        if (cp->overlap) {
            graph_destroy(cp->overlap);
            cp->overlap = NULL;
        }
        if (cp->reduced1) {
            graph_destroy(cp->reduced1);
            cp->reduced1 = NULL;
        }
        if (cp->reduced2) {
            graph_destroy(cp->reduced2);
            cp->reduced2 = NULL;
        }
        if (cp->mismatches) {
            lv_free(cp->mismatches);
            cp->mismatches = NULL;
        }
    }
    lv_free(set->pairs);
    lv_free(set);
}

void critical_pair_get_statistics(const CriticalPairSet *set, int *out_total, int *out_confluent, int *out_pending) {
    /* 先清零所有输出参数 */
    if (out_total)
        *out_total = 0;
    if (out_confluent)
        *out_confluent = 0;
    if (out_pending)
        *out_pending = 0;
    if (!set)
        return;

    if (out_total)
        *out_total = set->pair_count;

    int c = 0, p = 0;
    for (int i = 0; i < set->pair_count; i++) {
        if (set->pairs[i].compared) {
            if (set->pairs[i].is_confluent)
                c++;
        } else {
            p++;
        }
    }
    if (out_confluent)
        *out_confluent = c;
    if (out_pending)
        *out_pending = p;
}
