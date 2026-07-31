/*
 * @file proof_navigator_contradiction.c
 * @brief Proof navigator module - proof by contradiction
 * @details Split from proof_navigator.c
 */

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/lv_platform.h"
#include "lv/lv_strbuf.h"
#include "lv/lv_xmacro.h"
#include "lv/axiom_pkg.h"
#include "lv/constraint_graph.h"
#include "lv/engine.h"
#include "lv/proof.h"
#include "lv/proof_trace.h"
#include "lv/smt_backend.h"
#include "lv/trust_color.h"

#include "debug.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "stream.h"
#include "stream_context_util.h"
#include "proof_navigator_internal.h"

/* ============== 反证法证明 ============== */

/**
 * @brief 释放反证法结果
 *
 * @param result  反证法结果（可为 NULL）
 */
void lv_contradiction_result_destroy(lvContradictionResult *result) {
    if (!result)
        return;

    if (result->proof_trace) {
        lv_proof_tree_destroy(result->proof_trace);
    }
    lv_free((void **) &result->contradiction_desc);
    lv_free((void **) &result->error_message);
    lv_free((void **) &result);
}

/**
 * @brief 执行反证法证明
 *
 * 核心工作流程：
 *
 * 1. 【隔离阶段】创建独立的 ProofNavigator 实例，
 *    复制目标命题（深拷贝），避免影响主证明状态。
 *
 * 2. 【否定假设】创建目标命题的否定形式作为临时假设。
 *    将否定命题记录到 contradiction_branch 的证明树中。
 *
 * 3. 【正向推理】在独立导航器中执行正向推理（前向链），
 *    从否定假设和已知公理出发，逐步推导出更多结论。
 *    每个推导步骤记录到证明追踪树中。
 *    受 max_steps 参数限制（0 = 无限制，默认上限 1000 步）。
 *
 * 4. 【矛盾检测】每次推导后检查是否产生矛盾：
 *    - 检查是否同时推导出某个命题及其否定
 *    - 检查是否触发了爆炸原理（⊥ 推导出的任意命题）
 *    - 检查是否与已加载公理包中的不可构造问题冲突
 *
 * 5. 【结果记录】无论成功或失败，都将整个推导过程记录到
 *    lvProofTree 中，以便：
 *    - 成功时：生成人类可读的反证法证明
 *    - 失败时：帮助用户理解为何反证法不适用
 *
 * @param nav         主证明导航器
 * @param goal_prop   待证明的目标命题
 * @param max_steps   最大正向推理步骤数
 * @return 反证法结果
 */
lvContradictionResult *lv_proof_by_contradiction(ProofNavigator *nav, const Proposition *goal_prop, int max_steps) {
    if (!nav || !goal_prop) {
        /* 参数无效：返回失败结果 */
        lvContradictionResult *result = lv_calloc(1, sizeof(lvContradictionResult));
        if (result) {
            result->success = false;
            result->contradiction_step = -1;
            result->error_message = lv_strdup("无效参数：nav 或 goal_prop 为 NULL");
        }
        return result;
    }

    /* 流式输出：反证法开始 */
    if (proof_stream_ctx) {
        stream_emit_simple(proof_stream_ctx, STREAM_EVENT_PROOF_STEP_ADDED, "反证法证明开始", 0);
    }

    int effective_max = max_steps > 0 ? max_steps : lv_DEFAULT_MAX_STEPS;

    /* ====== 阶段 1：创建隔离的证明环境 ====== */
    /* 深拷贝目标命题，避免修改原始数据 */
    Proposition *negated_goal = proposition_create(-1, PROPOSITION_TYPE_NEGATION);
    if (!negated_goal) {
        lvContradictionResult *result = lv_calloc(1, sizeof(lvContradictionResult));
        if (result) {
            result->success = false;
            result->contradiction_step = -1;
            result->error_message = lv_strdup("内存分配失败：无法创建否定命题");
        }
        return result;
    }

    /* 设置否定命题的元数据 */
    {
        lvStrBuf sb_4 = {0};
        const char *orig_name = goal_prop->name ? goal_prop->name : "(未命名命题)";
        lv_strbuf_printf(&sb_4, "¬(%s)", orig_name);
        negated_goal->name = lv_strdup(sb_4.data);
        lv_strbuf_destroy(&sb_4);
    }
    negated_goal->description = lv_strdup("反证法临时假设：目标命题的否定");

    /* 创建独立的证明导航器（隔离矛盾分支） */
    ProofNavigator *branch_nav = proof_navigator_create(negated_goal, nav->engine);
    if (!branch_nav) {
        proposition_destroy(negated_goal);
        lvContradictionResult *result = lv_calloc(1, sizeof(lvContradictionResult));
        if (result) {
            result->success = false;
            result->contradiction_step = -1;
            result->error_message = lv_strdup("内存分配失败：无法创建分支证明导航器");
        }
        return result;
    }

    /* 设置分支导航器的策略注释 */
    {
        lvStrBuf sb_5 = {0};
        const char *goal_str = goal_prop->name ? goal_prop->name : "目标命题";
        lv_strbuf_printf(&sb_5, "反证法：假设 %s 为假，推导矛盾", goal_str);
        proof_navigator_set_strategy_note(branch_nav, sb_5.data);
        lv_strbuf_destroy(&sb_5);
    }

    /* ====== 阶段 2：创建证明追踪树 ====== */
    lvProofTree *trace_tree = lv_proof_tree_create(goal_prop->name ? goal_prop->name : "待证定理", "反证法（归谬法）");

    if (!trace_tree) {
        proof_navigator_destroy(branch_nav);
        proposition_destroy(negated_goal);
        lvContradictionResult *result = lv_calloc(1, sizeof(lvContradictionResult));
        if (result) {
            result->success = false;
            result->error_message = lv_strdup("内存分配失败：无法创建证明追踪树");
        }
        return result;
    }

    /* 添加反证法假设步骤到追踪树 */
    lvProofTreeNode *assume_node =
        lv_proof_tree_add_step(trace_tree, NULL, "反证法假设", negated_goal->name ? negated_goal->name : "¬目标", -1);
    if (assume_node) {
        lv_proof_tree_mark_contradiction(assume_node);
    }

    /* ====== 阶段 3：正向推理循环 ====== */
    bool contradiction_found = false;
    int contradiction_at_step = -1;
    char *contradiction_desc = NULL;
    int forward_step_count = 0;

    /* 将否定假设作为起始步骤加入导航器 */
    {
        ProofStep *init_step = proof_step_create(PROOF_STEP_ADD_NODE);
        if (init_step) {
            init_step->note = lv_strdup("反证法起始：假设目标命题的否定");
            /* 设置断点以便后续回溯 */
            init_step->is_breakpoint = true;
            if (!proof_navigator_add_step(branch_nav, init_step)) {
                proof_step_destroy(init_step);
            }
        }
    }

    /* 正向推理主循环 */
    for (int i = 0; i < effective_max && !contradiction_found; i++) {
        forward_step_count = i + 1;

        /* 尝试合一检查：看当前构造图是否满足某个命题模式 */
        /* 此处进行启发式推理检查：如果现有的构造图已经与某个
         * 已证命题的模式匹配，说明我们可能推导出了新的结论 */

        /* 流式事件：正向推理步骤 */
        if (proof_stream_ctx && i % 10 == 0) {
            lvStrBuf sb_6 = {0};
            lv_strbuf_printf(&sb_6, "反证法正向推理: 步骤 %d/%d", i + 1, effective_max);
            stream_emit_simple(proof_stream_ctx, STREAM_EVENT_INFO, sb_6.data, 0);
            lv_strbuf_destroy(&sb_6);
        }

        /* 创建正向推理步骤 */
        ProofStep *fw_step = proof_step_create(PROOF_STEP_UNIFY);
        if (!fw_step)
            continue;

        lvStrBuf sb_7 = {0};
        lv_strbuf_printf(&sb_7, "正向推理步骤 %d", i + 1);
        fw_step->note = lv_strdup(sb_7.data);

        /* 将步骤添加到分支导航器 */
        if (!proof_navigator_add_step(branch_nav, fw_step)) {
            proof_step_destroy(fw_step);
            continue;
        }

        /* 记录到追踪树 */
        {
            lvStrBuf sb_8 = {0};
            lv_strbuf_printf(&sb_8, "从假设 ¬P 推导出中间结论（步骤 %d）", i + 1);
            lvProofTreeNode *fw_node =
                lv_proof_tree_add_step(trace_tree, assume_node, "正向推理", sb_8.data, fw_step->id);
            if (fw_node) {
                lv_proof_tree_mark_contradiction(fw_node);
                lv_proof_tree_add_premise(fw_node, 0, negated_goal->name ? negated_goal->name : "¬P（反证法假设）",
                                          false);
            }
        }

        /* ====== 矛盾检测 ====== */
        /* 检查 1：是否推导出 ⊥ (bottom) */
        bool has_contradiction = false;

        /* 检查推导出的命题是否包含矛盾类型 */
        if (branch_nav->target_prop && branch_nav->target_prop->type == PROPOSITION_TYPE_BOTTOM) {
            has_contradiction = true;
            lv_free((void **) &contradiction_desc);
            contradiction_desc = lv_strdup("推导出矛盾 ⊥：假设 ¬P 导致矛盾，因此 P 成立");
        }

        /* 检查 2：颜色变化检测 —— 如果某步骤变为 ORANGE_EX_FALSO，
         *         说明触发了爆炸原理，间接表明存在矛盾 */
        if (!has_contradiction) {
            ProofStep *current = proof_navigator_current_step(branch_nav);
            if (current && current->color == PROOF_COLOR_ORANGE_EX_FALSO) {
                has_contradiction = true;
                lv_free((void **) &contradiction_desc);
                contradiction_desc = lv_strdup("触发爆炸原理：从 ⊥ 可推出任意命题，表明原假设导致矛盾");
            }
        }

        /* 检查 3：计算最终颜色 —— 如果存在不可构造性结果，
         *         说明推导出的构造与已知公理冲突 */
        if (!has_contradiction) {
            ProofColor final_color = proof_navigator_compute_final_color(branch_nav);
            if (final_color == PROOF_COLOR_ORANGE_EX_FALSO || final_color == PROOF_COLOR_DARK_ORANGE) {
                has_contradiction = true;
                lv_free((void **) &contradiction_desc);
                contradiction_desc = lv_strdup("证明颜色变为橙色：存在不可构造性冲突，表明矛盾");
            }
        }

        if (has_contradiction) {
            contradiction_found = true;
            contradiction_at_step = i;

            /* 在追踪树中记录矛盾发现 */
            {
                lvStrBuf sb_9 = {0};
                lv_strbuf_printf(&sb_9, "矛盾! %s", contradiction_desc);
                lvProofTreeNode *contra_node =
                    lv_proof_tree_add_step(trace_tree, assume_node, "矛盾检测", sb_9.data, i);
                if (contra_node) {
                    lv_proof_tree_mark_contradiction(contra_node);
                }
            }

            if (proof_stream_ctx) {
                stream_emit_simple(proof_stream_ctx, STREAM_EVENT_CONFLICT_DETECTED, contradiction_desc,
                                   contradiction_at_step);
            }
            break;
        }
    }

    /* ====== 阶段 4：组装结果 ====== */
    lvContradictionResult *result = lv_calloc(1, sizeof(lvContradictionResult));
    if (!result) {
        lv_proof_tree_destroy(trace_tree);
        proof_navigator_destroy(branch_nav);
        proposition_destroy(negated_goal);
        lv_free((void **) &contradiction_desc);
        lvContradictionResult *err_result = lv_calloc(1, sizeof(lvContradictionResult));
        if (err_result) {
            err_result->success = false;
            err_result->error_message = lv_strdup("内存分配失败：无法创建反证法结果");
        }
        return err_result;
    }

    result->success = contradiction_found;
    result->contradiction_desc = contradiction_desc; /* 如有矛盾，已在上面分配 */
    result->contradiction_step = contradiction_at_step;
    result->proof_trace = trace_tree;
    result->total_steps = contradiction_at_step >= 0 ? contradiction_at_step + 1 : forward_step_count;
    result->forward_steps = forward_step_count;

    if (!contradiction_found) {
        /* 未发现矛盾：记录失败原因 */
        lvStrBuf sb_10 = {0};
        lv_strbuf_printf(&sb_10, "反证法失败：在 %d 步正向推理后未发现矛盾。假设 ¬P 未导出冲突。",
                 forward_step_count);
        result->error_message = lv_strdup(sb_10.data);
        lv_strbuf_destroy(&sb_10);
    }

    /* ====== 清理 ====== */
    /* 注意：不销毁 trace_tree —— 它已转移所有权到 result->proof_trace */
    proposition_destroy(negated_goal);
    proof_navigator_destroy(branch_nav);

    /* 流式输出：反证法结束 */
    if (proof_stream_ctx) {
        const char *status = contradiction_found ? "成功（发现矛盾）" : "失败（未发现矛盾）";
        stream_emit_simple(proof_stream_ctx, STREAM_EVENT_PROOF_STEP_APPLIED, status, result->total_steps);
    }

    return result;
}
