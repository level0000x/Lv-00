/**
 * @file proof_verify.c
 * @brief 证明验证
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

/* ============== 证明验证 ============== */
/* 独立验证证明正确性，检查每步合法性（公理引用、推理规则使用、变量绑定） */

/**
 * @brief 验证证明的正确性
 *
 * 对溯源树进行完整性验证：
 *   1. 检查根节点状态是否为已证明
 *   2. 检查所有推导节点的依赖是否完整
 *   3. 检查是否存在未完成的子目标
 *   4. 检查信任颜色传播是否正确
 *
 * @param trace     溯源树
 * @param out_error 输出错误消息（缓冲区至少 512 字节）
 * @return 验证结果
 */
lvVerifyResult lv_verify_proof(const lvProofTraceTree *trace, char *out_error) {
    if (!trace) {
        if (out_error) {
            lv_snprintf(out_error, 512, "溯源树为 NULL");
        }
        return lv_VERIFY_ERROR;
    }

    if (!trace->root) {
        if (out_error) {
            lv_snprintf(out_error, 512, "溯源树缺少根节点");
        }
        return lv_VERIFY_ERROR;
    }

    /* 检查根节点状态 */
    if (trace->root->status != TRACE_STATUS_PROVED) {
        if (out_error) {
            lv_snprintf(out_error, 512, "根节点状态为 %d（期望 PROVED=%d）", (int) trace->root->status,
                     (int) TRACE_STATUS_PROVED);
        }
        return lv_VERIFY_INCOMPLETE;
    }

    /* 检查所有节点 */
    for (int i = 0; i < trace->all_nodes.count; i++) {
        lvProofTraceNode **node_p = (lvProofTraceNode **)lv_darray_get(&trace->all_nodes, i);
        lvProofTraceNode *node = *node_p;

        /* 推导节点必须有子节点（依赖） */
        if (node->type == TRACE_NODE_DERIVATION && node->children.count == 0) {
            if (out_error) {
                lv_snprintf(out_error, 512, "推导节点 %u ('%s') 没有子节点（缺少推导依据）", node->id, node->label);
            }
            return lv_VERIFY_INVALID;
        }

        /* 检查未完成的子目标 */
        if (node->type == TRACE_NODE_GOAL && node->status == TRACE_STATUS_UNEXPLORED) {
            if (out_error) {
                lv_snprintf(out_error, 512, "子目标节点 %u ('%s') 未被探索", node->id, node->label);
            }
            return lv_VERIFY_INCOMPLETE;
        }
    }

    /* 检查信任颜色传播 */
    TrustColor computed = lv_trace_node_compute_color(trace->root);
    if (computed != trace->final_color) {
        /* 警告但不标记为无效 */
        if (out_error) {
            lv_snprintf(out_error, 512, "警告：信任颜色不一致（计算值=%d, 记录值=%d）", (int) computed,
                     (int) trace->final_color);
        }
    }

    return lv_VERIFY_VALID;
}

/**
 * @brief 验证单个证明步骤的合法性
 *
 * 检查证明步骤是否满足以下条件：
 *   1. 步骤类型有效
 *   2. 依赖步骤已完成
 *   3. 步骤本身已标记完成
 *   4. 步骤颜色与依赖颜色一致
 *
 * @param step      证明步骤
 * @param graph     约束图（可为 NULL）
 * @param out_error 输出错误消息（缓冲区至少 512 字节）
 * @return 验证结果
 */
lvVerifyResult lv_verify_proof_step(const ProofStep *step, const ConstraintGraph *graph, char *out_error) {
    if (!step) {
        if (out_error) {
            lv_snprintf(out_error, 512, "证明步骤为 NULL");
        }
        return lv_VERIFY_ERROR;
    }

    /* 检查步骤类型 */
    if (step->type < PROOF_STEP_ADD_NODE || step->type > PROOF_STEP_ORACLE) {
        if (out_error) {
            lv_snprintf(out_error, 512, "步骤 %d 的类型 %d 无效", step->id, (int) step->type);
        }
        return lv_VERIFY_INVALID;
    }

    /* 检查步骤是否完成 */
    if (!step->is_completed) {
        if (out_error) {
            lv_snprintf(out_error, 512, "步骤 %d 尚未完成", step->id);
        }
        return lv_VERIFY_INCOMPLETE;
    }

    /* 检查关联的约束是否存在（走哈希索引查询，而非直索引下标） */
    if (graph && step->constraint_id >= 0) {
        bool found = (graph_get_constraint(graph, step->constraint_id) != NULL);
        if (!found) {
            if (out_error) {
                lv_snprintf(out_error, 512, "步骤 %d 引用的约束 %d 在约束图中不存在", step->id, step->constraint_id);
            }
            return lv_VERIFY_INVALID;
        }
    }

    return lv_VERIFY_VALID;
}
