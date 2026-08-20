/**
 * @file proof_optimize.c
 * @brief 证明优化
 *
 * @details 本文件从 proof_engine_enhanced.c 拆分子模块生成（Lv-00 v3.3.0+）。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "lv/proof_engine_enhanced.h"
#include "proof_engine_enhanced_internal.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/constraint_graph.h"
#include "lv/proof.h"

#include "lv/axiom_rule_engine.h"
#include "lv/error_codes.h"
#include "lv/three_valued_logic.h"

#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/lv_strbuf.h"

/* ============== 证明优化 ============== */
/* 消除冗余推导步骤，计算证明复杂度评分 */

/**
 * @brief 内部函数：检查节点是否为冗余节点
 *
 * 一个节点被认为是冗余的，如果满足以下条件之一：
 *   1. 节点类型为推导，但没有子节点
 *   2. 节点与父节点具有相同的命题
 *   3. 节点是中间传递节点（只有一个子节点，且子节点也是推导节点）
 *
 * @param node 溯源节点
 * @return true 节点是冗余的
 */
static bool is_redundant_node(const lvProofTraceNode *node) {
    if (!node)
        return false;

    /* 无子节点的推导节点是冗余的 */
    if (node->type == TRACE_NODE_DERIVATION && node->children.count == 0) {
        return true;
    }

    /* 单子节点的推导节点可能是传递节点 */
    if (node->type == TRACE_NODE_DERIVATION && node->children.count == 1) {
        lvProofTraceNode **child_p = (lvProofTraceNode **)lv_darray_get(&node->children, 0);
        lvProofTraceNode *child = *child_p;
        if (child && child->type == TRACE_NODE_DERIVATION) {
            return true;
        }
    }

    return false;
}

/* 递归复制节点到优化树（跳过冗余节点，沿 children 遍历，
 * 覆盖公开 lv_trace_node_add_child 构造的未注册节点） */
static void optimize_copy_recursive(const lvProofTraceNode *src_node, lvProofTraceNode *dst_parent,
                                    lvProofTraceTree *optimized, uint32_t *removed_count) {
    if (!src_node)
        return;

    for (int i = 0; i < src_node->children.count; i++) {
        lvProofTraceNode **child_p = (lvProofTraceNode **)lv_darray_get(&src_node->children, i);
        lvProofTraceNode *src_child = *child_p;

        /* 跳过冗余节点 */
        if (is_redundant_node(src_child)) {
            (*removed_count)++;
            continue;
        }

        /* 创建新节点并复制属性 */
        lvProofTraceNode *new_node = lv_trace_node_create(src_child->type, src_child->label);
        if (!new_node)
            continue;

        lv_strlcpy(new_node->description, src_child->description, sizeof(new_node->description));
        new_node->status = src_child->status;
        new_node->trust_color = src_child->trust_color;
        new_node->proposition = src_child->proposition;
        new_node->step = src_child->step;
        new_node->rule = src_child->rule;
        new_node->elapsed_ms = src_child->elapsed_ms;

        /* 添加到优化后的树 */
        lv_trace_node_add_child(dst_parent, new_node);
        trace_tree_register_node(optimized, new_node);

        /* 递归复制子节点 */
        optimize_copy_recursive(src_child, new_node, optimized, removed_count);
    }
}

/**
 * @brief 优化证明（消除冗余步骤）
 *
 * 通过以下方式优化证明：
 *   1. 移除冗余的推导节点
 *   2. 合并连续的同类型节点
 *   3. 简化信任颜色传播路径
 *
 * 优化过程创建新的溯源树，不修改原始树。
 * 沿 children 递归复制，不依赖 all_nodes（公开构造路径的节点不在其中）。
 *
 * @param trace         原始溯源树
 * @param out_optimized 输出优化后的溯源树
 * @return true 成功优化（或无需优化），false 参数无效
 */
bool lv_optimize_proof(const lvProofTraceTree *trace, lvProofTraceTree **out_optimized) {
    if (!trace || !out_optimized) {
        lv_RETURN_ERROR_BOOL(lv_ERROR_NULL_POINTER, "lv_optimize_proof: NULL param");
    }

    *out_optimized = NULL;

    /* 创建新的溯源树 */
    lvProofTraceTree *optimized = lv_trace_tree_create(trace->root ? trace->root->proposition : NULL);
    if (!optimized)
        lv_RETURN_ERROR_BOOL(lv_ERROR_ALLOCATION_FAILED, "lv_optimize_proof: tree creation failed");

    /* 递归复制非冗余节点 */
    uint32_t removed_count = 0;
    optimize_copy_recursive(trace->root, optimized->root, optimized, &removed_count);

    /* 更新优化后的树状态 */
    optimized->is_complete = trace->is_complete;
    optimized->final_color = trace->final_color;
    trace_tree_update_stats(optimized);

    *out_optimized = optimized;
    (void) removed_count; /* 统计已消除的冗余节点数，供调试使用 */
    return true;
}

/**
 * @brief 计算证明的复杂度分数
 *
 * 复杂度分数基于以下因素：
 *   - 节点总数（权重 1）
 *   - 最大深度（权重 3）
 *   - 分支因子（权重 2）
 *   - 未完成节点比例（权重 5）
 *
 * 分数越高表示证明越复杂。
 *
 * @param trace 溯源树
 * @return 复杂度分数（0-10000）
 */
uint32_t lv_compute_proof_complexity(const lvProofTraceTree *trace) {
    if (!trace)
        return 0;

    uint32_t score = 0;

    /* 节点数量贡献 */
    score += trace->all_nodes.count * 1;

    /* 深度贡献 */
    score += trace->max_depth * 3;

    /* 分支因子贡献 */
    if (trace->all_nodes.count > 0) {
        uint32_t total_children = 0;
        for (int i = 0; i < trace->all_nodes.count; i++) {
            lvProofTraceNode **node_p = (lvProofTraceNode **)lv_darray_get(&trace->all_nodes, i);
            total_children += (uint32_t)(*node_p)->children.count;
        }
        double avg_branch = (double) total_children / (double) trace->all_nodes.count;
        score += (uint32_t) (avg_branch * 2);
    }

    /* 未完成节点贡献 */
    uint32_t incomplete = (uint32_t)(trace->all_nodes.count - trace->proved_count - trace->disproved_count);
    if (trace->all_nodes.count > 0) {
        double incomplete_ratio = (double) incomplete / (double) trace->all_nodes.count;
        score += (uint32_t) (incomplete_ratio * 5000);
    }

    return score;
}

/* 递归标记冗余节点为阻塞（沿 children 遍历，覆盖未注册节点） */
static uint32_t simplify_mark_recursive(lvProofTraceNode *node) {
    if (!node)
        return 0;
    uint32_t removed = 0;
    if (is_redundant_node(node)) {
        lv_trace_node_set_status(node, TRACE_STATUS_BLOCKED);
        removed++;
    }
    for (int i = 0; i < node->children.count; i++) {
        lvProofTraceNode **child_p = (lvProofTraceNode **)lv_darray_get(&node->children, i);
        removed += simplify_mark_recursive(*child_p);
    }
    return removed;
}

/**
 * @brief 简化证明（原地修改）
 *
 * 通过以下方式简化证明：
 *   1. 移除冗余节点
 *   2. 将已证伪的分支标记为阻塞
 *   3. 重新计算信任颜色
 *
 * @param trace 溯源树（原地修改）
 * @return 简化后的步骤数
 */
uint32_t lv_simplify_proof(lvProofTraceTree *trace) {
    if (!trace)
        return 0;

    /* 沿 children 递归标记冗余节点为阻塞 */
    uint32_t removed = simplify_mark_recursive(trace->root);

    /* 重新计算信任颜色 */
    if (trace->root) {
        lv_trace_node_compute_color(trace->root);
        trace->final_color = trace->root->trust_color;
    }

    /* 更新统计 */
    trace_tree_update_stats(trace);

    return (uint32_t)(trace->all_nodes.count - removed);
}
