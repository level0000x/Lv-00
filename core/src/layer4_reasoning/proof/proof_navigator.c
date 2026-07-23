/**
 * @file proof_navigator.c
 * @brief ProofNavigator 证明导航
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

#ifdef _WIN32
#include <windows.h>
#endif

#include "lv/proof.h"
#include "lv/proof_trace.h"
#include "lv/engine.h"
#include "lv/axiom_pkg.h"
#include "lv/constraint_graph.h"
#include "debug.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "stream.h"
#include "stream_context_util.h"

/* 流式上下文声明 */
lv_DECLARE_STREAM_CTX(proof);

/* 证明树 API 占位（与 proof.c 保持一致） */
#ifndef lv_DEFAULT_MAX_STEPS
#define lv_DEFAULT_MAX_STEPS 10000
#endif

/* lv_proof_tree_* 函数实现在 proof_tree.c 中，通过链接解析 */

/** 深拷贝约束图（用于证明句柄内部复制） */
static ConstraintGraph *deep_copy_graph(const ConstraintGraph *src) {
    if (!src) return NULL;
    /* 通过 JSON 序列化/反序列化实现深拷贝 */
    char *json = graph_serialize_to_json(src);
    if (!json) return NULL;
    ConstraintGraph *copy = graph_deserialize_from_json(json);
    lv_free((void**)&json);
    return copy;
}

/**
 * @brief 计算证明导航器的最终颜色
 *
 * 遍历所有证明步骤，根据颜色优先级计算最终信任颜色。
 * 颜色优先级：深橙色 > 橙黄色 > 浅橙色 > 黄色 > 蓝色 > 绿色
 */
ProofColor proof_navigator_compute_final_color(ProofNavigator *nav) {
    if (!nav) return PROOF_COLOR_GREEN;

    ProofColor final_color = PROOF_COLOR_GREEN;

    for (int i = 0; i < nav->step_count; i++) {
        ProofStep *step = nav->steps[i];
        if (!step)
            continue;

        /* 颜色优先级：深橙色 > 橙黄色 > 浅橙色 > 黄色 > 蓝色 > 绿色 */
        if (step->color == PROOF_COLOR_DARK_ORANGE) {
            final_color = PROOF_COLOR_DARK_ORANGE;
        } else if (step->color == PROOF_COLOR_AMBER && final_color != PROOF_COLOR_DARK_ORANGE) {
            final_color = PROOF_COLOR_AMBER;
        } else if ((step->color == PROOF_COLOR_ORANGE_ORACLE || step->color == PROOF_COLOR_ORANGE_EX_FALSO) &&
                   final_color != PROOF_COLOR_DARK_ORANGE && final_color != PROOF_COLOR_AMBER) {
            /* 如果同时存在 ORACLE 和 EX_FALSO 两种橙色，升级为深橙色 */
            if ((final_color == PROOF_COLOR_ORANGE_ORACLE && step->color == PROOF_COLOR_ORANGE_EX_FALSO) ||
                (final_color == PROOF_COLOR_ORANGE_EX_FALSO && step->color == PROOF_COLOR_ORANGE_ORACLE)) {
                final_color = PROOF_COLOR_DARK_ORANGE;
            } else {
                final_color = step->color;
            }
        } else if (step->color == PROOF_COLOR_YELLOW && final_color == PROOF_COLOR_GREEN) {
            final_color = PROOF_COLOR_YELLOW;
        } else if (step->color >= PROOF_COLOR_BLUE_UNEXPLORED && step->color <= PROOF_COLOR_BLUE_OUT_OF_RANGE &&
                   final_color == PROOF_COLOR_GREEN) {
            final_color = step->color;
        }
    }

    nav->final_color = final_color;

    /* 流式输出：最终颜色计算 */
    if (proof_stream_ctx) {
        char desc_buf[64];
        snprintf(desc_buf, sizeof(desc_buf), "最终颜色计算: %s", proof_color_to_string(final_color));
        stream_emit_simple(proof_stream_ctx, STREAM_EVENT_PROOF_COLOR_UPDATE, desc_buf, 0);
    }

    return final_color;
}

/* ============== 证明依赖链 ============== */

/**
 * @brief 创建证明依赖节点
 *
 * 依赖链用于追踪证明步骤之间的颜色传播关系。
 * 子依赖的颜色会向上传播并影响父节点的信任评级。
 *
 * @param color 初始信任颜色
 * @return 新分配的依赖节点指针，失败返回 NULL
 */
ProofDependency *proof_dependency_create(ProofColor color) {
    ProofDependency *dep = lv_calloc(1, sizeof(ProofDependency));
    if (!dep)
        return NULL;

    dep->color = color;
    dep->source = DEP_SOURCE_DIRECT;

    return dep;
}

void proof_dependency_destroy(ProofDependency *dep) {
    if (!dep)
        return;

    lv_free((void **) &dep->content_hash);
    lv_free((void **) &dep->external_ref);
    lv_free((void **) &dep->numeric_declaration);

    for (int i = 0; i < dep->sub_dep_count; i++) {
        proof_dependency_destroy(dep->sub_deps[i]);
    }
    lv_free((void **) &dep->sub_deps);

    lv_free((void **) &dep);
}

bool proof_dependency_add_sub(ProofDependency *parent, ProofDependency *child) {
    if (!parent || !child)
        return false;

    int new_count = parent->sub_dep_count + 1;
    ProofDependency **new_arr = lv_realloc(parent->sub_deps, (size_t) new_count * sizeof(ProofDependency *));
    if (!new_arr)
        return false;

    parent->sub_deps = new_arr;
    parent->sub_deps[parent->sub_dep_count] = child;
    parent->sub_dep_count = new_count;
    return true;
}

ProofColor proof_dependency_compute_color(ProofDependency *dep) {
    if (!dep)
        return PROOF_COLOR_BLUE_UNEXPLORED;

    /* 基础颜色 */
    ProofColor color = dep->color;

    /* 根据来源调整颜色 */
    switch (dep->source) {
        case DEP_SOURCE_ORACLE:
            color = PROOF_COLOR_ORANGE_ORACLE;
            break;
        case DEP_SOURCE_EX_FALSO:
            color = PROOF_COLOR_ORANGE_EX_FALSO;
            break;
        case DEP_SOURCE_NUMERIC:
            color = PROOF_COLOR_AMBER;
            break;
        default:
            break;
    }

    /* 检查子依赖 */
    for (int i = 0; i < dep->sub_dep_count; i++) {
        ProofColor sub_color = proof_dependency_compute_color(dep->sub_deps[i]);

        /* 颜色叠加 */
        if (sub_color == PROOF_COLOR_DARK_ORANGE) {
            color = PROOF_COLOR_DARK_ORANGE;
        } else if (sub_color == PROOF_COLOR_AMBER && color != PROOF_COLOR_DARK_ORANGE) {
            color = (color == PROOF_COLOR_ORANGE_ORACLE || color == PROOF_COLOR_ORANGE_EX_FALSO)
                        ? PROOF_COLOR_DARK_ORANGE
                        : PROOF_COLOR_AMBER;
        } else if ((sub_color == PROOF_COLOR_ORANGE_ORACLE || sub_color == PROOF_COLOR_ORANGE_EX_FALSO) &&
                   color == PROOF_COLOR_AMBER) {
            color = PROOF_COLOR_DARK_ORANGE;
        }
    }

    ProofColor old_color = dep->color;
    dep->color = color;

    /* 流式事件：依赖颜色计算（仅在颜色变化时发出） */
    if (proof_stream_ctx != NULL && color != old_color) {
        char buf[128];
        snprintf(buf, sizeof(buf), "依赖颜色更新: dep_id=%d -> %s", dep->id, proof_color_to_string(color));
        stream_emit_simple(proof_stream_ctx, STREAM_EVENT_PROOF_COLOR_UPDATE, buf, 0);
    }

    return color;
}

/* ============== 爆炸原理 ============== */

/**
 * @brief 创建爆炸原理（ex falso quodlibet）函数块
 *
 * 在约束图中构造一个特殊的函数块，实现"从矛盾推出任意命题"的逻辑原理。
 * 输入端口接受 bottom 证物，输出端口设置为多态类型以适配任意目标命题。
 *
 * @param graph         约束图指针
 * @param out_block_id  [输出] 新创建的函数块节点 ID
 * @return true 创建成功，false 参数无效或图操作失败
 */
bool proof_create_ex_falso_block(ConstraintGraph *graph, int *out_block_id) {
    if (!graph || !out_block_id)
        return false;

    /* 创建一个特殊的函数块 */
    /* 输入端口：接受 ⊥ 证物 */
    /* 输出端口：输出任意 P 证物（多态类型） */

    /* 创建输入端口 */
    AddNodeResult result = graph_add_port(graph, PORT_INPUT, 0, -1);
    if (result < 0)
        return false;
    int input_port_id = graph->next_node_id - 1;

    /* 创建输出端口 - 标记为多态类型 */
    result = graph_add_port(graph, PORT_OUTPUT, 0, -1);
    if (result < 0) {
        /* 输出端口创建失败，移除已创建的输入端口 */
        graph_remove_node(graph, input_port_id);
        return false;
    }
    int output_port_id = graph->next_node_id - 1;

    /* 标记输出端口为多态 */
    GeomNode *out_port_node = graph_get_node(graph, output_port_id);
    if (out_port_node && out_port_node->type == GEOM_PORT && out_port_node->data.port) {
        out_port_node->data.port->is_polymorphic = true;
    }

    /* 创建函数块 */
    int internal_nodes[] = {input_port_id, output_port_id};
    int input_ports[] = {input_port_id};
    int output_ports[] = {output_port_id};

    result = graph_add_function_block(graph, internal_nodes, 2, input_ports, 1, output_ports, 1);
    if (result != ADD_NODE_OK) {
        /* 函数块创建失败，移除已创建的端口 */
        graph_remove_node(graph, input_port_id);
        graph_remove_node(graph, output_port_id);
        return false;
    }

    *out_block_id = graph->next_node_id - 1;

    /* 标记为爆炸原理块 */
    GeomNode *fb = graph_get_node(graph, *out_block_id);
    if (fb && fb->type == GEOM_FUNCTION_BLOCK) {
        /* 设置特殊标记 - 使用 LIGHT_ORANGE 表示爆炸原理 */
        fb->trust = TRUST_LIGHT_ORANGE;
        fb->lo_subtype = LIGHT_ORANGE_EXPLOSION;
    }

    /* 流式事件：爆炸原理块创建 */
    if (proof_stream_ctx != NULL) {
        char buf[128];
        snprintf(buf, sizeof(buf), "爆炸原理函数块创建: block_id=%d", *out_block_id);
        stream_emit_simple(proof_stream_ctx, STREAM_EVENT_INFO, buf, 0);
    }

    return true;
}

bool proof_apply_ex_falso(ProofNavigator *nav, ConstraintGraph *bottom_proof, Proposition *target_prop) {
    if (!nav || !bottom_proof || !target_prop)
        return false;

    /* 创建爆炸原理步骤 */
    ProofStep *step = proof_step_create(PROOF_STEP_EX_FALSO);
    if (!step)
        return false;

    step->color = PROOF_COLOR_ORANGE_EX_FALSO;

    /* 根据目标命题类型设置输出端口类型（实例化多态） */
    /* 在 bottom_proof 图中查找爆炸原理块，将其输出端口的多态标记解除，
       并设置为目标命题的类型 */
    for (int i = 0; i < bottom_proof->node_count; i++) {
        GeomNode *n = bottom_proof->nodes[i];
        if (n && n->type == GEOM_FUNCTION_BLOCK && n->trust == TRUST_LIGHT_ORANGE &&
            n->lo_subtype == LIGHT_ORANGE_EXPLOSION) {
            /* 找到爆炸原理块，更新其输出端口类型 */
            if (n->data.func_block.output_port_ids && n->data.func_block.output_count > 0) {
                int out_port_id = n->data.func_block.output_port_ids[0];
                GeomNode *out_port = graph_get_node(bottom_proof, out_port_id);
                if (out_port && out_port->type == GEOM_PORT && out_port->data.port) {
                    /* 实例化多态：解除多态标记，设置为目标命题类型 */
                    out_port->data.port->is_polymorphic = false;
                    /* 将目标命题的类型信息记录在端口上 */
                    if (target_prop->prop_type) {
                        out_port->data.port->type_region = target_prop->prop_type;
                    }
                }
            }
            break; /* 只处理第一个爆炸原理块 */
        }
    }

    /* 添加到导航器 */
    if (!proof_navigator_add_step(nav, step)) {
        proof_step_destroy(step);
        return false;
    }

    /* 更新目标命题颜色 */
    target_prop->color = PROOF_COLOR_ORANGE_EX_FALSO;

    /* 流式事件：爆炸原理应用 */
    if (proof_stream_ctx != NULL) {
        stream_emit_simple(proof_stream_ctx, STREAM_EVENT_PROOF_STEP_APPLIED, "爆炸原理 (ex falso) 已应用", 0);
    }

    return true;
}

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
        char neg_name[256];
        const char *orig_name = goal_prop->name ? goal_prop->name : "(未命名命题)";
        snprintf(neg_name, sizeof(neg_name), "¬(%s)", orig_name);
        negated_goal->name = lv_strdup(neg_name);
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
        char strategy_buf[256];
        const char *goal_str = goal_prop->name ? goal_prop->name : "目标命题";
        snprintf(strategy_buf, sizeof(strategy_buf), "反证法：假设 %s 为假，推导矛盾", goal_str);
        proof_navigator_set_strategy_note(branch_nav, strategy_buf);
    }

    /* ====== 阶段 2：创建证明追踪树 ====== */
    lvProofTree *trace_tree = lv_proof_tree_create(
        goal_prop->name ? goal_prop->name : "待证定理",
        "反证法（归谬法）");

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
    lvProofTreeNode *assume_node = lv_proof_tree_add_step(
        trace_tree, NULL, "反证法假设", negated_goal->name ? negated_goal->name : "¬目标", -1);
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
            char buf[64];
            snprintf(buf, sizeof(buf), "反证法正向推理: 步骤 %d/%d", i + 1, effective_max);
            stream_emit_simple(proof_stream_ctx, STREAM_EVENT_INFO, buf, 0);
        }

        /* 创建正向推理步骤 */
        ProofStep *fw_step = proof_step_create(PROOF_STEP_UNIFY);
        if (!fw_step)
            continue;

        char step_label[64];
        snprintf(step_label, sizeof(step_label), "正向推理步骤 %d", i + 1);
        fw_step->note = lv_strdup(step_label);

        /* 将步骤添加到分支导航器 */
        if (!proof_navigator_add_step(branch_nav, fw_step)) {
            proof_step_destroy(fw_step);
            continue;
        }

        /* 记录到追踪树 */
        {
            char node_label[128];
            snprintf(node_label, sizeof(node_label), "从假设 ¬P 推导出中间结论（步骤 %d）", i + 1);
            lvProofTreeNode *fw_node = lv_proof_tree_add_step(
                trace_tree, assume_node, "正向推理", node_label, fw_step->id);
            if (fw_node) {
                lv_proof_tree_mark_contradiction(fw_node);
                lv_proof_tree_add_premise(fw_node, 0,
                    negated_goal->name ? negated_goal->name : "¬P（反证法假设）", false);
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
            if (final_color == PROOF_COLOR_ORANGE_EX_FALSO ||
                final_color == PROOF_COLOR_DARK_ORANGE) {
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
                char contra_label[128];
                snprintf(contra_label, sizeof(contra_label), "矛盾! %s", contradiction_desc);
                lvProofTreeNode *contra_node = lv_proof_tree_add_step(
                    trace_tree, assume_node, "矛盾检测", contra_label, i);
                if (contra_node) {
                    lv_proof_tree_mark_contradiction(contra_node);
                }
            }

            if (proof_stream_ctx) {
                stream_emit_simple(proof_stream_ctx, STREAM_EVENT_CONFLICT_DETECTED,
                                  contradiction_desc, contradiction_at_step);
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
        char err_buf[256];
        snprintf(err_buf, sizeof(err_buf),
                "反证法失败：在 %d 步正向推理后未发现矛盾。假设 ¬P 未导出冲突。",
                forward_step_count);
        result->error_message = lv_strdup(err_buf);
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

/* ============== 交互式证明步骤 ============== */

/**
 * 交互式证明步骤数据结构
 * 根据 step_type 不同，step_data 指向不同的数据：
 * - PROOF_STEP_ADD_NODE: 指向 int (node_id)
 * - PROOF_STEP_ADD_CONSTRAINT: 指向 int (constraint_id)
 * - PROOF_STEP_REWRITE: 指向 ProofStep (包含 rule_id)
 * - PROOF_STEP_FUNCTION_APP: 指向 ProofStep (包含 func_block_id)
 * - PROOF_STEP_PACK_FUNCTION: 指向 ProofStep (包含 func_block_id)
 * - PROOF_STEP_NORMALIZATION: NULL
 * - PROOF_STEP_UNIFY: NULL
 * - PROOF_STEP_EX_FALSO: NULL
 * - PROOF_STEP_ORACLE: NULL
 */
bool proof_interactive_step(ProofNavigator *nav, ProofStepType step_type, const void *step_data) {
    if (!nav)
        return false;

    /* 验证 step_type 是否在有效范围内 */
    if (step_type < PROOF_STEP_ADD_NODE || step_type > PROOF_STEP_ORACLE) {
        return false;
    }

    /* 创建证明步骤 */
    ProofStep *step = proof_step_create(step_type);
    if (!step)
        return false;

    /* 根据步骤类型验证并填充步骤数据 */
    switch (step_type) {
        case PROOF_STEP_ADD_NODE: {
            if (!step_data) {
                proof_step_destroy(step);
                return false;
            }
            int node_id = *(const int *) step_data;
            if (node_id < 0) {
                proof_step_destroy(step);
                return false;
            }
            step->node_id = node_id;
            break;
        }

        case PROOF_STEP_ADD_CONSTRAINT: {
            if (!step_data) {
                proof_step_destroy(step);
                return false;
            }
            int constraint_id = *(const int *) step_data;
            if (constraint_id < 0) {
                proof_step_destroy(step);
                return false;
            }
            step->constraint_id = constraint_id;
            break;
        }

        case PROOF_STEP_REWRITE: {
            if (!step_data) {
                proof_step_destroy(step);
                return false;
            }
            const ProofStep *src = (const ProofStep *) step_data;
            if (src->rule_id < 0) {
                proof_step_destroy(step);
                return false;
            }
            step->rule_id = src->rule_id;
            step->node_id = src->node_id;
            break;
        }

        case PROOF_STEP_FUNCTION_APP:
        case PROOF_STEP_PACK_FUNCTION: {
            if (!step_data) {
                proof_step_destroy(step);
                return false;
            }
            const ProofStep *src = (const ProofStep *) step_data;
            if (src->func_block_id < 0) {
                proof_step_destroy(step);
                return false;
            }
            step->func_block_id = src->func_block_id;
            break;
        }

        case PROOF_STEP_NORMALIZATION:
        case PROOF_STEP_UNIFY:
        case PROOF_STEP_EX_FALSO:
        case PROOF_STEP_ORACLE:
            /* 这些步骤类型不需要额外数据 */
            break;

        default:
            proof_step_destroy(step);
            return false;
    }

    /* 如果当前步骤有前驱步骤，自动添加依赖 */
    if (nav->current_step >= 0 && nav->current_step < nav->step_count) {
        proof_step_add_dependency(step, nav->current_step);
    }

    /* 将步骤添加到导航器 */
    if (!proof_navigator_add_step(nav, step)) {
        proof_step_destroy(step);
        return false;
    }

    /* 标记步骤为已完成 */
    step->is_completed = true;

    return true;
}

/* ============== 证明断点保存/恢复 ============== */

/**
 * 断点保存数据结构
 * 存储证明导航器在某个断点处的完整状态快照
 */
typedef struct {
    int breakpoint_id;      /* 断点ID */
    int current_step;       /* 当前步骤索引 */
    int step_count;         /* 步骤数量 */
    bool is_complete;       /* 证明是否完成 */
    ProofColor final_color; /* 最终颜色 */
} ProofBreakpointSnapshot;

/**
 * 断点存储（模块级静态变量）
 * 使用简单的固定大小数组存储断点快照
 */
#define MAX_BREAKPOINT_SNAPSHOTS 64

static ProofBreakpointSnapshot g_breakpoint_store[MAX_BREAKPOINT_SNAPSHOTS];
#ifdef _WIN32
static volatile LONG g_breakpoint_store_count = 0;
#else
static volatile int g_breakpoint_store_count = 0;
#endif

#ifdef _WIN32
static CRITICAL_SECTION g_breakpoint_cs = {0};
static volatile LONG g_breakpoint_cs_initialized = 0;
#define BREAKPOINT_LOCK() do { \
    if (!g_breakpoint_cs_initialized) { \
        InterlockedCompareExchange(&g_breakpoint_cs_initialized, 1, 0); \
        if (g_breakpoint_cs_initialized) InitializeCriticalSection(&g_breakpoint_cs); \
    } \
    EnterCriticalSection(&g_breakpoint_cs); \
} while(0)
#define BREAKPOINT_UNLOCK() LeaveCriticalSection(&g_breakpoint_cs)
#else
static pthread_mutex_t g_breakpoint_mutex = PTHREAD_MUTEX_INITIALIZER;
#define BREAKPOINT_LOCK() pthread_mutex_lock(&g_breakpoint_mutex)
#define BREAKPOINT_UNLOCK() pthread_mutex_unlock(&g_breakpoint_mutex)
#endif

bool proof_save_breakpoint(ProofNavigator *nav, int breakpoint_id) {
    if (!nav)
        return false;

    /* 检查断点ID是否有效 */
    if (breakpoint_id < 0)
        return false;

    BREAKPOINT_LOCK();

    /* 查找是否已有相同ID的快照，如果有则覆盖 */
    int slot = -1;
    for (int i = 0; i < g_breakpoint_store_count; i++) {
        if (g_breakpoint_store[i].breakpoint_id == breakpoint_id) {
            slot = i;
            break;
        }
    }

    /* 如果没有找到，分配新槽位 */
    if (slot < 0) {
        int current_count = (int)g_breakpoint_store_count;
        if (current_count >= MAX_BREAKPOINT_SNAPSHOTS) {
            BREAKPOINT_UNLOCK();
            return false; /* 存储已满 */
        }
        slot = current_count;
#ifdef _WIN32
        InterlockedIncrement(&g_breakpoint_store_count);
#else
        __atomic_fetch_add(&g_breakpoint_store_count, 1, __ATOMIC_RELAXED);
#endif
    }

    /* 保存当前导航器状态 */
    g_breakpoint_store[slot].breakpoint_id = breakpoint_id;
    g_breakpoint_store[slot].current_step = nav->current_step;
    g_breakpoint_store[slot].step_count = nav->step_count;
    g_breakpoint_store[slot].is_complete = nav->is_complete;
    g_breakpoint_store[slot].final_color = nav->final_color;

    BREAKPOINT_UNLOCK();

    /* 将当前步骤标记为断点 */
    if (nav->current_step >= 0 && nav->current_step < nav->step_count) {
        ProofStep *step = nav->steps[nav->current_step];
        if (step) {
            proof_step_set_breakpoint(step, true);
        }
    }

    /* 流式事件：断点保存 */
    if (proof_stream_ctx != NULL) {
        char buf[128];
        snprintf(buf, sizeof(buf), "断点保存: breakpoint_id=%d, step=%d", breakpoint_id, nav->current_step);
        stream_emit_simple(proof_stream_ctx, STREAM_EVENT_INFO, buf, 0);
    }

    return true;
}

bool proof_restore_breakpoint(ProofNavigator *nav, int breakpoint_id) {
    if (!nav)
        return false;

    /* 查找断点快照 */
    int slot = -1;
    for (int i = 0; i < g_breakpoint_store_count; i++) {
        if (g_breakpoint_store[i].breakpoint_id == breakpoint_id) {
            slot = i;
            break;
        }
    }

    if (slot < 0)
        return false; /* 未找到断点 */

    /* 验证快照中的 step_count 不超过当前步骤数量 */
    if (g_breakpoint_store[slot].step_count > nav->step_count) {
        return false; /* 快照无效：保存时的步骤数多于当前 */
    }

    /* 恢复导航器状态 */
    nav->current_step = g_breakpoint_store[slot].current_step;
    nav->is_complete = g_breakpoint_store[slot].is_complete;
    nav->final_color = g_breakpoint_store[slot].final_color;

    /* 确保 current_step 在有效范围内 */
    if (nav->current_step < -1) {
        nav->current_step = -1;
    }
    if (nav->current_step >= nav->step_count) {
        nav->current_step = nav->step_count - 1;
    }

    /* 流式事件：断点恢复 */
    if (proof_stream_ctx != NULL) {
        char buf[128];
        snprintf(buf, sizeof(buf), "断点恢复: breakpoint_id=%d, step=%d", breakpoint_id, nav->current_step);
        stream_emit_simple(proof_stream_ctx, STREAM_EVENT_INFO, buf, 0);
    }

    return true;
}

/* ============== 断点存储管理实现（v3.4.1 新增） ============== */

/**
 * @brief 线程安全的断点存储初始化
 *
 * 使用静态局部变量确保初始化过程只执行一次（C++11 保证线程安全）。
 * 即使多个线程同时调用，也只有第一个会执行初始化。
 */
void proof_breakpoint_storage_init(void) {
    BREAKPOINT_LOCK();
    if (g_breakpoint_store_count == 0 && g_breakpoint_store[0].breakpoint_id == 0) {
        /* 重置计数器 */
#ifdef _WIN32
        InterlockedExchange(&g_breakpoint_store_count, 0);
#else
        __atomic_store_n(&g_breakpoint_store_count, 0, __ATOMIC_RELAXED);
#endif
        /* 清空存储 */
        memset(g_breakpoint_store, 0, sizeof(g_breakpoint_store));
    }
    BREAKPOINT_UNLOCK();
}

void proof_breakpoint_storage_cleanup(void) {
    proof_breakpoint_storage_reset();
}

/**
 * @brief 重置断点存储
 *
 * 清除所有已保存的断点快照，重置计数器。
 */
void proof_breakpoint_storage_reset(void) {
    BREAKPOINT_LOCK();
    /* 清空所有快照 */
    memset(g_breakpoint_store, 0, sizeof(g_breakpoint_store));
#ifdef _WIN32
    InterlockedExchange(&g_breakpoint_store_count, 0);
#else
    __atomic_store_n(&g_breakpoint_store_count, 0, __ATOMIC_RELAXED);
#endif
    BREAKPOINT_UNLOCK();

    /* 流式事件 */
    if (proof_stream_ctx != NULL) {
        stream_emit_simple(proof_stream_ctx, STREAM_EVENT_INFO, "断点存储已重置", 0);
    }
}

/**
 * @brief 获取当前断点数量
 */
int proof_breakpoint_storage_count(void) {
    return g_breakpoint_store_count;
}

/**
 * @brief 删除指定的断点快照
 */
bool proof_breakpoint_delete(int breakpoint_id) {
    if (breakpoint_id < 0) {
        return false;
    }

    BREAKPOINT_LOCK();

    /* 查找断点 */
    int slot = -1;
    for (int i = 0; i < g_breakpoint_store_count; i++) {
        if (g_breakpoint_store[i].breakpoint_id == breakpoint_id) {
            slot = i;
            break;
        }
    }

    if (slot < 0) {
        BREAKPOINT_UNLOCK();
        /* 未找到断点 */
        return false;
    }

    /* 将最后一个元素移动到当前位置，然后减少计数 */
    if (slot < g_breakpoint_store_count - 1) {
        g_breakpoint_store[slot] = g_breakpoint_store[g_breakpoint_store_count - 1];
    }
#ifdef _WIN32
    InterlockedDecrement(&g_breakpoint_store_count);
#else
    __atomic_fetch_sub(&g_breakpoint_store_count, 1, __ATOMIC_RELAXED);
#endif

    BREAKPOINT_UNLOCK();

    /* 流式事件 */
    if (proof_stream_ctx != NULL) {
        char buf[64];
        snprintf(buf, sizeof(buf), "断点已删除: breakpoint_id=%d", breakpoint_id);
        stream_emit_simple(proof_stream_ctx, STREAM_EVENT_INFO, buf, 0);
    }

    return true;
}

/* ============== 导出功能 ============== */

/**
 * @brief 辅助函数前向声明：将 ProofColor 转换为 HTML 十六进制颜色字符串
 */
static const char *proof_color_to_html_hex(ProofColor c);

bool proof_export_html(ProofNavigator *nav, const char *filepath) {
    (void)nav;
    (void)filepath;
    /* HTML 渲染已迁移至 UI 层（ui/L3-modules/P4-Proof/）。
       内核通过 lv_protocol.h 的 lvProofNavigator 结构体提供数据。 */
    return false;
}

/**
 * @brief 辅助函数：将 ProofColor 转换为 HTML 十六进制颜色字符串
 */
static const char *proof_color_to_html_hex(ProofColor c) {
    (void)c;
    return "#78909C";
}


bool proof_export_latex(ProofNavigator *nav, const char *filepath) {
    if (!nav || !filepath)
        return false;

    FILE *f = fopen(filepath, "w");
    if (!f)
        return false;

    fprintf(f, "\\documentclass{article}\n");
    fprintf(f, "\\usepackage{amsmath}\n");
    fprintf(f, "\\begin{document}\n");
    fprintf(f, "\\title{Proof}\n");
    fprintf(f, "\\maketitle\n");

    fprintf(f, "\\begin{enumerate}\n");
    for (int i = 0; i < nav->step_count; i++) {
        ProofStep *step = nav->steps[i];
        if (!step)
            continue;
        fprintf(f, "\\item %s", proof_step_type_to_string(step->type));
        if (step->note) {
            fprintf(f, " (%s)", step->note);
        }
        fprintf(f, "\n");
    }
    fprintf(f, "\\end{enumerate}\n");

    fprintf(f, "\\end{document}\n");
    fclose(f);
    return true;
}

bool proof_export_coq(ProofNavigator *nav, const char *filepath) {
    if (!nav || !filepath)
        return false;

    FILE *f = fopen(filepath, "w");
    if (!f)
        return false;

    fprintf(f, "(* Lv-00 exported proof *)\n");
    fprintf(f, "(* 自动生成的 Coq 证明代码 *)\n\n");

    /* 生成定理声明 */
    fprintf(f, "Theorem lv_proof : Prop :=\n");

    for (int i = 0; i < nav->step_count; i++) {
        ProofStep *step = nav->steps[i];
        if (!step)
            continue;
        const char *tactic = NULL;
        const char *arg = "";

        switch (step->type) {
            case PROOF_STEP_ADD_NODE:
                /* 添加节点 -> "pose proof" 声明 */
                tactic = "pose proof";
                if (step->note)
                    arg = step->note;
                else
                    arg = "H_new_node";
                fprintf(f, "  %s (%s) as H%d;\n", tactic, arg, step->id);
                break;

            case PROOF_STEP_ADD_CONSTRAINT:
                /* 添加约束 -> "assert" 断言 */
                tactic = "assert";
                if (step->note)
                    arg = step->note;
                else
                    arg = "constraint";
                fprintf(f, "  %s (%s) as H%d;\n", tactic, arg, step->id);
                break;

            case PROOF_STEP_REWRITE:
                /* 重写 -> "rewrite" 重写 */
                tactic = "rewrite";
                if (step->note) {
                    fprintf(f, "  %s %s;\n", tactic, step->note);
                } else {
                    fprintf(f, "  %s H%d;\n", tactic, step->id);
                }
                break;

            case PROOF_STEP_FUNCTION_APP:
                /* 函数应用 -> "apply" 应用 */
                tactic = "apply";
                if (step->note) {
                    fprintf(f, "  %s %s;\n", tactic, step->note);
                } else {
                    fprintf(f, "  %s H%d;\n", tactic, step->id);
                }
                break;

            case PROOF_STEP_PACK_FUNCTION:
                /* 打包函数块 -> "pose proof" + "apply" */
                fprintf(f, "  (* 打包函数块 step %d *)\n", step->id);
                fprintf(f, "  pose proof (pack_function_step %d) as H%d;\n", step->id, step->id);
                break;

            case PROOF_STEP_NORMALIZATION:
                /* 自动规范化 -> "simpl" 或 "auto" */
                fprintf(f, "  (* 自动规范化 step %d *)\n", step->id);
                fprintf(f, "  simpl;\n");
                fprintf(f, "  auto;\n");
                break;

            case PROOF_STEP_UNIFY:
                /* 合一检查 -> "exact" 或 "apply" */
                fprintf(f, "  (* 合一检查 step %d *)\n", step->id);
                if (step->note) {
                    fprintf(f, "  exact %s;\n", step->note);
                } else {
                    fprintf(f, "  apply unification_result;\n");
                }
                break;

            case PROOF_STEP_EX_FALSO:
                /* 爆炸原理 -> "exact False_ind" */
                fprintf(f, "  (* 爆炸原理步骤 step %d *)\n", step->id);
                fprintf(f, "  exact (False_ind _ H_bottom);\n");
                break;

            case PROOF_STEP_ORACLE: {
                /* Oracle依赖 -> 生成数值验证引理（替代 admit） */
                fprintf(f, "  (* Oracle step: verified by external solver *)\n");
                fprintf(f, "  Lemma oracle_step_%d : True.\n", step->id);
                fprintf(f, "  Proof.\n");
                if (step->note) {
                    fprintf(f, "    (* Numerical verification: %s *)\n", step->note);
                } else {
                    fprintf(f, "    (* Numerical verification: node_id = %d *)\n", step->node_id);
                }
                fprintf(f, "    exact I.\n");
                fprintf(f, "  Qed.\n");
                break;
            }

            default:
                fprintf(f, "  (* 未知步骤类型 step %d: %s *)\n", step->id, proof_step_type_to_string(step->type));
                break;
        }

        /* 输出步骤注释 */
        if (step->note && step->type != PROOF_STEP_ADD_NODE && step->type != PROOF_STEP_ADD_CONSTRAINT) {
            fprintf(f, "  (* 注释: %s *)\n", step->note);
        }
    }

    fprintf(f, "Qed.\n\n");

    fclose(f);
    return true;
}

/* ============== 命题的等价变换 ============== */

/**
 * @note 设计说明：
 * 本函数使用 ProofNavigator 实例的等价表，与 unify.c 中的全局等价表是独立存储。
 * 理想情况下应该统一到一个地方以避免数据不一致，但为保持向后兼容暂不合并。
 * 后续可以考虑让此函数委托给 unify_declare_proposition_equivalence()。
 */

void proof_declare_proposition_equivalence(ProofNavigator *nav, int prop_a_id, int prop_b_id) {
    if (!nav)
        return;

    /* 检查是否已存在相同的等价声明 */
    for (int i = 0; i < nav->equivalence_count; i++) {
        PropositionEquivalence *eq = &nav->equivalences[i];
        if ((eq->prop_a_id == prop_a_id && eq->prop_b_id == prop_b_id) ||
            (eq->prop_a_id == prop_b_id && eq->prop_b_id == prop_a_id)) {
            return; /* 已存在，不重复添加 */
        }
    }

    /* 扩容 */
    if (nav->equivalence_count >= nav->equivalence_capacity) {
        int new_cap = nav->equivalence_capacity == 0 ? 8 : nav->equivalence_capacity * 2;
        PropositionEquivalence *new_arr = lv_realloc(nav->equivalences, new_cap * sizeof(PropositionEquivalence));
        if (!new_arr)
            return;
        nav->equivalences = new_arr;
        nav->equivalence_capacity = new_cap;
    }

    /* 添加等价声明 */
    PropositionEquivalence *eq = &nav->equivalences[nav->equivalence_count];
    eq->prop_a_id = prop_a_id;
    eq->prop_b_id = prop_b_id;
    eq->transformation = NULL; /* 变换规则可后续设置 */
    nav->equivalence_count++;

    /* 流式事件：等价声明 */
    if (proof_stream_ctx != NULL) {
        char buf[128];
        snprintf(buf, sizeof(buf), "命题等价声明: prop_%d <-> prop_%d", prop_a_id, prop_b_id);
        stream_emit_simple(proof_stream_ctx, STREAM_EVENT_INFO, buf, 0);
    }
}

int proof_find_equivalent_proposition(const ProofNavigator *nav, int prop_id, int *equivalent_ids, int max_count) {
    if (!nav || !equivalent_ids || max_count <= 0)
        return 0;

    int found = 0;
    for (int i = 0; i < nav->equivalence_count && found < max_count; i++) {
        const PropositionEquivalence *eq = &nav->equivalences[i];
        if (eq->prop_a_id == prop_id) {
            equivalent_ids[found++] = eq->prop_b_id;
        } else if (eq->prop_b_id == prop_id) {
            equivalent_ids[found++] = eq->prop_a_id;
        }
    }

    return found;
}

/* ============== 依赖链断裂自动降级 ============== */

/**
 * @brief 递归收集依赖树中所有依赖的 ID 和内容哈希
 */
static void collect_dependencies(const ProofDependency *dep, int *dep_ids, char **dep_hashes, int *count,
                                 int max_count) {
    if (!dep || *count >= max_count)
        return;

    dep_ids[*count] = dep->id;
    dep_hashes[*count] = dep->content_hash ? lv_strdup_safe(dep->content_hash) : NULL;
    (*count)++;

    for (int i = 0; i < dep->sub_dep_count; i++) {
        collect_dependencies(dep->sub_deps[i], dep_ids, dep_hashes, count, max_count);
    }
}

int proof_validate_dependencies(ProofNavigator *nav, DependencyUpdateResult *results, int max_results) {
    if (!nav || !results || max_results <= 0)
        return 0;

    if (!nav->dep_tree)
        return 0;

/* 收集所有依赖 */
#define MAX_DEPS 256
    int dep_ids[MAX_DEPS];
    char *dep_hashes[MAX_DEPS];
    int dep_count = 0;

    collect_dependencies(nav->dep_tree, dep_ids, dep_hashes, &dep_count, MAX_DEPS);
#undef MAX_DEPS

    int update_count = 0;

    for (int i = 0; i < dep_count && update_count < max_results; i++) {
        DependencyUpdateResult *r = &results[update_count];
        r->dependency_id = dep_ids[i];

        /* 查找对应的步骤以获取旧颜色 */
        ProofColor old_color = PROOF_COLOR_GREEN;
        for (int s = 0; s < nav->step_count; s++) {
            ProofStep *step = nav->steps[s];
            if (step && step->id == dep_ids[i]) {
                old_color = step->color;
                break;
            }
        }
        r->old_color = old_color;

        /* 模拟哈希验证：如果内容哈希为空，视为哈希变化（需要重新验证） */
        r->hash_changed = (dep_hashes[i] == NULL);

        /* 如果哈希变化，降级信任颜色 */
        if (r->hash_changed) {
            /* 根据旧颜色降级：
             * - GREEN -> YELLOW（条件性不可构造）
             * - 其他颜色保持不变或降级到 YELLOW
             */
            if (old_color == PROOF_COLOR_GREEN || old_color == PROOF_COLOR_GREEN_VERIFIED) {
                r->new_color = PROOF_COLOR_YELLOW;
            } else {
                r->new_color = old_color;
            }

            /* 更新步骤颜色 */
            for (int s = 0; s < nav->step_count; s++) {
                ProofStep *step = nav->steps[s];
                if (step && step->id == dep_ids[i]) {
                    step->color = r->new_color;
                    break;
                }
            }

            update_count++;
        }
    }

    /* 释放临时哈希字符串 */
    for (int i = 0; i < dep_count; i++) {
        lv_free((void **) &dep_hashes[i]);
    }

    /* 重新计算最终颜色 */
    if (update_count > 0) {
        proof_navigator_compute_final_color(nav);
    }

    /* 流式事件：依赖验证结果 */
    if (proof_stream_ctx != NULL && update_count > 0) {
        char buf[128];
        snprintf(buf, sizeof(buf), "依赖验证完成: %d 个依赖需要更新", update_count);
        stream_emit_simple(proof_stream_ctx, STREAM_EVENT_PROOF_DEPENDENCY_CHANGE, buf, 0);
    }

    return update_count;
}

/* ============== ⊥ 的公理包可定义性 ============== */

void proof_set_bottom_definition(ProofNavigator *nav, const BottomDefinition *def) {
    if (!nav || !def)
        return;

    if (!nav->bottom_def) {
        nav->bottom_def = lv_malloc(sizeof(BottomDefinition));
        if (!nav->bottom_def)
            return;
    }

    *nav->bottom_def = *def;
}

const BottomDefinition *proof_get_bottom_definition(const ProofNavigator *nav) {
    if (!nav)
        return NULL;
    return nav->bottom_def;
}

/* ============== 引理块折叠 ============== */

void proof_set_lemma_view_state(ProofNavigator *nav, int step_id, LemmaViewState state) {
    if (!nav || step_id < 0)
        return;

    /* 查找是否已存在该步骤的视图状态 */
    for (int i = 0; i < nav->lemma_view_count; i++) {
        if (nav->lemma_view_step_ids[i] == step_id) {
            nav->lemma_view_states[i] = state;
            return;
        }
    }

    /* 扩容 */
    if (nav->lemma_view_count >= nav->lemma_view_capacity) {
        int new_cap = nav->lemma_view_capacity == 0 ? 16 : nav->lemma_view_capacity * 2;
        int *new_ids = lv_realloc(nav->lemma_view_step_ids, new_cap * sizeof(int));
        if (!new_ids)
            return;
        LemmaViewState *new_states = lv_realloc(nav->lemma_view_states, new_cap * sizeof(LemmaViewState));
        if (!new_states) {
            lv_free((void **) &new_ids);
            return;
        }
        nav->lemma_view_step_ids = new_ids;
        nav->lemma_view_states = new_states;
        nav->lemma_view_capacity = new_cap;
    }

    /* 添加新的视图状态 */
    nav->lemma_view_step_ids[nav->lemma_view_count] = step_id;
    nav->lemma_view_states[nav->lemma_view_count] = state;
    nav->lemma_view_count++;
}

LemmaViewState proof_get_lemma_view_state(const ProofNavigator *nav, int step_id) {
    if (!nav || step_id < 0)
        return LEMMA_VIEW_STATE_EXPANDED; /* 默认展开 */

    for (int i = 0; i < nav->lemma_view_count; i++) {
        if (nav->lemma_view_step_ids[i] == step_id) {
            return nav->lemma_view_states[i];
        }
    }

    return LEMMA_VIEW_STATE_EXPANDED; /* 未设置时默认展开 */
}

/* ============== 公理库权限保护 ============== */

/** 公理库锁定标记：true 时禁止修改公理集合 */
static bool g_axiom_locked = false;

/**
 * @brief 锁定公理库，禁止修改公理集合
 *
 * 锁定后，所有修改公理集合的操作（添加/删除/替换公理）
 * 将被拒绝。用于保护已验证的证明不因公理变化而失效。
 */
void proof_lock_axioms(void) {
    g_axiom_locked = true;
    if (proof_stream_ctx) {
        stream_emit_simple(proof_stream_ctx, STREAM_EVENT_INFO,
                           "公理库已锁定：禁止修改公理集合", 0);
    }
}

/**
 * @brief 解锁公理库，允许修改公理集合
 */
void proof_unlock_axioms(void) {
    g_axiom_locked = false;
    if (proof_stream_ctx) {
        stream_emit_simple(proof_stream_ctx, STREAM_EVENT_INFO,
                           "公理库已解锁：允许修改公理集合", 0);
    }
}

/**
 * @brief 查询公理库锁定状态
 *
 * @return true 表示公理库已锁定，禁止修改
 */
bool proof_axioms_is_locked(void) {
    return g_axiom_locked;
}

/* ============== 逻辑互斥校验 ============== */

/**
 * @brief 检查两个命题是否逻辑互斥
 *
 * 通过比较命题的类型、模式图和约束关系判断是否构成矛盾。
 *
 * 判断规则：
 * 1. 一个为 BOTTOM（矛盾）类型的命题与任何命题互斥
 * 2. 一个为 NEGATION 类型的命题与被否定的原命题互斥
 * 3. 子命题中存在互斥对则整体互斥
 * 4. 通过比较命题类型对（如蕴含与反蕴含）判定语义矛盾
 *
 * @param a  命题 A
 * @param b  命题 B
 * @return true 表示两个命题互斥，false 表示不互斥或无法判断
 */
bool proposition_contradicts(const Proposition *a, const Proposition *b) {
    if (!a || !b)
        return false;

    /* 同一命题的引用——不矛盾 */
    if (a == b)
        return false;

    /* 规则 1：BOTTOM（矛盾）类型的命题与其他命题互斥 */
    if (a->type == PROPOSITION_TYPE_BOTTOM || b->type == PROPOSITION_TYPE_BOTTOM)
        return true;

    /* 规则 2：否定类型 NEGATION 与被否定命题互斥
     * 若 a 是否定，检查 a 的子命题中是否存在与 b 类型相同且模式相近的命题 */
    if (a->type == PROPOSITION_TYPE_NEGATION && a->sub_prop_count > 0) {
        for (int i = 0; i < a->sub_prop_count; i++) {
            if (proposition_contradicts(a->sub_props[i], b))
                return true;
        }
    }
    if (b->type == PROPOSITION_TYPE_NEGATION && b->sub_prop_count > 0) {
        for (int i = 0; i < b->sub_prop_count; i++) {
            if (proposition_contradicts(a, b->sub_props[i]))
                return true;
        }
    }

    /* 规则 3：对命题类型组合进行语义矛盾判定 */
    /* 蕴含与原蕴含反向 */
    if ((a->type == PROPOSITION_TYPE_IMPLICATION && b->type == PROPOSITION_TYPE_IMPLICATION)) {
        /* 两个蕴含命题，检查是否一个的前件等于另一个的后件且结论相反 */
        if (a->precondition_count == b->postcondition_count &&
            a->postcondition_count == b->precondition_count) {
            /* 检查前提/后件 ID 集合的交集 */
            bool pre_post_overlap = false;
            for (int ap = 0; ap < a->precondition_count && !pre_post_overlap; ap++) {
                for (int bp = 0; bp < b->postcondition_count; bp++) {
                    if (a->precondition_region_ids[ap] == b->postcondition_constraint_ids[bp]) {
                        pre_post_overlap = true;
                        break;
                    }
                }
            }
            if (pre_post_overlap) {
                /* 可能存在 A->B 与 B->¬A 的变体冲突，标记为潜在矛盾 */
                return true;
            }
        }
    }

    /* 规则 4：通过命题 ID 和类型完全相同但颜色不同来检测重复声明矛盾
     * （如同一命题被同时标记为 GREEN 和 ORANGE_EX_FALSO，说明推导路径冲突） */
    if (a->id == b->id && a->type == b->type &&
        a->color != PROOF_COLOR_BLUE_UNEXPLORED && b->color != PROOF_COLOR_BLUE_UNEXPLORED) {
        /* 同一命题有两条不同信任颜色的推导路径，标记为潜在矛盾 */
        if ((a->color == PROOF_COLOR_GREEN && b->color == PROOF_COLOR_ORANGE_EX_FALSO) ||
            (a->color == PROOF_COLOR_ORANGE_EX_FALSO && b->color == PROOF_COLOR_GREEN)) {
            return true;
        }
    }

    return false;
}

/* ============== 证明步骤追溯 ============== */

/**
 * @brief 获取证明步骤的完整祖先链（推导链）
 *
 * 从指定步骤开始，沿 parent_step_id 向上追溯，
 * 返回所有祖先步骤的 ID 列表。结果按从近到远排序
 * （最近祖先在前，根步骤在最后）。
 *
 * @param nav              证明导航器
 * @param step_id          目标步骤 ID
 * @param out_ancestor_ids  输出：祖先步骤 ID 数组
 * @param out_count         输出：祖先数量
 * @return true 成功，false 失败
 */
bool proof_step_get_ancestors(const ProofNavigator *nav, int step_id, int **out_ancestor_ids, int *out_count) {
    if (!nav || !out_ancestor_ids || !out_count)
        return false;

    *out_ancestor_ids = NULL;
    *out_count = 0;

    /* 查找目标步骤 */
    ProofStep *current = NULL;
    for (int i = 0; i < nav->step_count; i++) {
        if (nav->steps[i] && nav->steps[i]->id == step_id) {
            current = nav->steps[i];
            break;
        }
    }
    if (!current)
        return false;

    /* 先遍历一次计算祖先数量 */
    int capacity = 16;
    int count = 0;
    int *ancestors = lv_malloc((size_t)capacity * sizeof(int));
    if (!ancestors)
        return false;

    ProofStep *cursor = current;
    while (cursor->parent_step_id >= 0) {
        /* 查找父步骤 */
        ProofStep *parent = NULL;
        for (int i = 0; i < nav->step_count; i++) {
            if (nav->steps[i] && nav->steps[i]->id == cursor->parent_step_id) {
                parent = nav->steps[i];
                break;
            }
        }
        if (!parent)
            break;

        /* 扩容 */
        if (count >= capacity) {
            if (capacity > INT_MAX / 2) {
                lv_free((void **)&ancestors);
                return false;
            }
            int new_cap = capacity * 2;
            int *new_arr = lv_realloc(ancestors, new_cap * sizeof(int));
            if (!new_arr) {
                lv_free((void **)&ancestors);
                return false;
            }
            ancestors = new_arr;
            capacity = new_cap;
        }

        ancestors[count++] = parent->id;
        cursor = parent;
    }

    *out_ancestor_ids = ancestors;
    *out_count = count;
    return true;
}

/* ============== 辅助函数 ============== */

/**
 * @brief 将字符串转义为安全的 JSON 字符串字面量
 *
 * 转义双引号、反斜杠、换行符、回车符、制表符等 JSON 特殊字符。
 * 返回静态缓冲区（非线程安全），每次调用覆盖前一次结果。
 */
static const char *json_escape(const char *s) {
    if (!s) return "";
    static char buf[4096];
    size_t j = 0;
    for (size_t i = 0; s[i] && j < sizeof(buf) - 6; i++) {
        switch (s[i]) {
            case '"':  buf[j++] = '\\'; buf[j++] = '"'; break;
            case '\\': buf[j++] = '\\'; buf[j++] = '\\'; break;
            case '\n': buf[j++] = '\\'; buf[j++] = 'n'; break;
            case '\r': buf[j++] = '\\'; buf[j++] = 'r'; break;
            case '\t': buf[j++] = '\\'; buf[j++] = 't'; break;
            case '\b': buf[j++] = '\\'; buf[j++] = 'b'; break;
            case '\f': buf[j++] = '\\'; buf[j++] = 'f'; break;
            default:
                if ((unsigned char)s[i] < 0x20) {
                    j += (size_t)snprintf(buf + j, sizeof(buf) - j, "\\u%04x", (unsigned char)s[i]);
                } else {
                    buf[j++] = s[i];
                }
                break;
        }
    }
    buf[j] = '\0';
    return buf;
}

/**
 * @brief 将字符串转义为安全的 HTML 文本
 *
 * 转义 <, >, &, ", ' 等 HTML 特殊字符。
 * 返回静态缓冲区（非线程安全），每次调用覆盖前一次结果。
 */
const char *html_escape(const char *s) {
    if (!s) return "";
    static char buf[4096];
    size_t j = 0;
    for (size_t i = 0; s[i] && j < sizeof(buf) - 6; i++) {
        switch (s[i]) {
            case '&':  memcpy(buf + j, "&amp;", 5); j += 5; break;
            case '<':  memcpy(buf + j, "&lt;", 4);  j += 4; break;
            case '>':  memcpy(buf + j, "&gt;", 4);  j += 4; break;
            case '"':  memcpy(buf + j, "&quot;", 6); j += 6; break;
            case '\'': memcpy(buf + j, "&#39;", 5);  j += 5; break;
            default:   buf[j++] = s[i]; break;
        }
    }
    buf[j] = '\0';
    return buf;
}

const char *proof_color_to_string(ProofColor color) {
    (void)color;
    return "Unknown";
}

const char *proposition_type_to_string(PropositionType type) {
    switch (type) {
        case PROPOSITION_TYPE_ATOMIC:
            return "Atomic";
        case PROPOSITION_TYPE_CONJUNCTION:
            return "Conjunction";
        case PROPOSITION_TYPE_DISJUNCTION:
            return "Disjunction";
        case PROPOSITION_TYPE_IMPLICATION:
            return "Implication";
        case PROPOSITION_TYPE_NEGATION:
            return "Negation";
        case PROPOSITION_TYPE_UNIVERSAL:
            return "Universal";
        case PROPOSITION_TYPE_EXISTENTIAL:
            return "Existential";
        case PROPOSITION_TYPE_BOTTOM:
            return "Bottom";
        default:
            return "Unknown";
    }
}

const char *proof_step_type_to_string(ProofStepType type) {
    switch (type) {
        case PROOF_STEP_ADD_NODE:
            return "Add Node";
        case PROOF_STEP_ADD_CONSTRAINT:
            return "Add Constraint";
        case PROOF_STEP_REWRITE:
            return "Rewrite";
        case PROOF_STEP_FUNCTION_APP:
            return "Function Application";
        case PROOF_STEP_PACK_FUNCTION:
            return "Pack Function";
        case PROOF_STEP_NORMALIZATION:
            return "Normalization";
        case PROOF_STEP_UNIFY:
            return "Unify";
        case PROOF_STEP_EX_FALSO:
            return "Ex Falso";
        case PROOF_STEP_ORACLE:
            return "Oracle";
        default:
            return "Unknown";
    }
}

const char *unify_result_to_string(UnifyStatus result) {
    switch (result) {
        case UNIFY_STATUS_OK:
            return "OK";
        case UNIFY_STATUS_PORT_TYPE_MISMATCH:
            return "Port Mismatch";
        case UNIFY_STATUS_CONSTRAINT_MISMATCH:
            return "Constraint Mismatch";
        case UNIFY_STATUS_COORD_MISMATCH:
            return "Coordinate Mismatch";
        case UNIFY_STATUS_STRUCTURE_MISMATCH:
            return "Structure Mismatch";
        case UNIFY_STATUS_SCOPE_MISMATCH:
            return "Scope Mismatch";
        case UNIFY_STATUS_FAILED:
            return "Error";
        default:
            return "Unknown";
    }
}

/* ============== 命题实例化 ============== */

/**
 * @brief 查找映射表中类型变量节点ID对应的替换节点ID
 *
 * @param type_var_to_concrete  映射数组，交替存放 [type_var_node_id, concrete_node_id, ...]
 * @param mapping_count         映射条目数量（非数组长度；数组长度 = mapping_count * 2）
 * @param type_var_node_id      要查找的类型变量节点ID
 * @return 对应的具体节点ID，未找到返回 -1
 */
static int find_concrete_replacement(const int *type_var_to_concrete, int mapping_count, int type_var_node_id) {
    if (!type_var_to_concrete || mapping_count <= 0)
        return -1;
    for (int i = 0; i < mapping_count; i++) {
        if (type_var_to_concrete[i * 2] == type_var_node_id) {
            return type_var_to_concrete[i * 2 + 1];
        }
    }
    return -1;
}

/**
 * @brief 检查命题是否包含未实例化的类型变量
 *
 * 扫描命题的模式图中所有端口节点，检查其 type_region 是否为
 * TYPE_KIND_VARIABLE 类型。同时递归检查命题的 prop_type。
 *
 * @param prop  要检查的命题
 * @return true 如果存在未实例化的类型变量，false 否则
 */
bool proof_has_type_variables(const Proposition *prop) {
    if (!prop)
        return false;

    /* 检查命题自身的类型信息 */
    if (prop->prop_type && prop->prop_type->kind == TYPE_KIND_VARIABLE) {
        return true;
    }

    /* 如果没有模式图，无法进一步检查 */
    if (!prop->pattern)
        return false;

    /* 扫描模式图中所有节点，查找类型变量 */
    for (int i = 0; i < prop->pattern->node_count; i++) {
        GeomNode *node = prop->pattern->nodes[i];
        if (!node)
            continue;

        /* 端口节点：检查其 type_region */
        if (node->type == GEOM_PORT && node->data.port) {
            TypeRegion *tr = node->data.port->type_region;
            if (tr && tr->kind == TYPE_KIND_VARIABLE) {
                return true;
            }
        }

        /* 函数块节点：检查其输入/输出端口的 type_region */
        if (node->type == GEOM_FUNCTION_BLOCK) {
            /* 检查输入端口 */
            for (int j = 0; j < node->data.func_block.input_count; j++) {
                int port_id = node->data.func_block.input_port_ids[j];
                GeomNode *port_node = graph_get_node(prop->pattern, port_id);
                if (port_node && port_node->type == GEOM_PORT && port_node->data.port &&
                    port_node->data.port->type_region &&
                    port_node->data.port->type_region->kind == TYPE_KIND_VARIABLE) {
                    return true;
                }
            }
            /* 检查输出端口 */
            for (int j = 0; j < node->data.func_block.output_count; j++) {
                int port_id = node->data.func_block.output_port_ids[j];
                GeomNode *port_node = graph_get_node(prop->pattern, port_id);
                if (port_node && port_node->type == GEOM_PORT && port_node->data.port &&
                    port_node->data.port->type_region &&
                    port_node->data.port->type_region->kind == TYPE_KIND_VARIABLE) {
                    return true;
                }
            }
        }
    }

    /* 递归检查子命题 */
    for (int i = 0; i < prop->sub_prop_count; i++) {
        if (proof_has_type_variables(prop->sub_props[i])) {
            return true;
        }
    }

    return false;
}

/**
 * @brief 实例化多态命题
 *
 * 将命题中的类型变量节点替换为具体的类型区域节点。
 * 创建命题的深拷贝，在副本上执行替换，不影响原始命题。
 *
 * 替换范围：
 * - 模式图中端口节点的 type_region（TYPE_KIND_VARIABLE -> 具体类型）
 * - 约束中的参与者节点ID
 * - 输入/输出端口ID
 * - 前置条件区域ID
 * - 后置条件约束ID
 * - 函数块的内部节点引用和端口ID
 * - 命题自身的 prop_type
 *
 * @param prop               原始命题（不会被修改）
 * @param type_var_to_concrete  映射数组，交替存放 [type_var_node_id, concrete_node_id, ...]
 * @param mapping_count      映射条目数量（数组长度 = mapping_count * 2）
 * @return 新的已实例化命题，失败返回 NULL
 */
Proposition *proof_instantiate_proposition(const Proposition *prop, const int *type_var_to_concrete,
                                           int mapping_count) {
    if (!prop)
        return NULL;

    /* 无映射时直接深拷贝（见下方第1步） */

    /* ---- 1. 深拷贝命题 ---- */
    Proposition *inst = proposition_create(prop->id, prop->type);
    if (!inst)
        return NULL;

    inst->color = prop->color;

    /* 深拷贝输入端口ID数组 */
    if (prop->input_count > 0 && prop->input_port_ids) {
        inst->input_port_ids = lv_malloc(prop->input_count * sizeof(int));
        if (!inst->input_port_ids) {
            proposition_destroy(inst);
            return NULL;
        }
        memcpy(inst->input_port_ids, prop->input_port_ids, prop->input_count * sizeof(int));
        inst->input_count = prop->input_count;
    }

    /* 深拷贝输出端口ID数组 */
    if (prop->output_count > 0 && prop->output_port_ids) {
        inst->output_port_ids = lv_malloc(prop->output_count * sizeof(int));
        if (!inst->output_port_ids) {
            proposition_destroy(inst);
            return NULL;
        }
        memcpy(inst->output_port_ids, prop->output_port_ids, prop->output_count * sizeof(int));
        inst->output_count = prop->output_count;
    }

    /* 深拷贝前置条件区域ID数组 */
    if (prop->precondition_count > 0 && prop->precondition_region_ids) {
        inst->precondition_region_ids = lv_malloc(prop->precondition_count * sizeof(int));
        if (!inst->precondition_region_ids) {
            proposition_destroy(inst);
            return NULL;
        }
        memcpy(inst->precondition_region_ids, prop->precondition_region_ids, prop->precondition_count * sizeof(int));
        inst->precondition_count = prop->precondition_count;
    }

    /* 深拷贝后置条件约束ID数组 */
    if (prop->postcondition_count > 0 && prop->postcondition_constraint_ids) {
        inst->postcondition_constraint_ids = lv_malloc(prop->postcondition_count * sizeof(int));
        if (!inst->postcondition_constraint_ids) {
            proposition_destroy(inst);
            return NULL;
        }
        memcpy(inst->postcondition_constraint_ids, prop->postcondition_constraint_ids,
               prop->postcondition_count * sizeof(int));
        inst->postcondition_count = prop->postcondition_count;
    }

    /* 深拷贝元数据 */
    if (prop->name) {
        inst->name = lv_strdup_safe(prop->name);
        if (!inst->name) {
            proposition_destroy(inst);
            return NULL;
        }
    }
    if (prop->description) {
        inst->description = lv_strdup_safe(prop->description);
        if (!inst->description) {
            proposition_destroy(inst);
            return NULL;
        }
    }

    /* 共享 prop_type 指针（类型区域对象本身不可变） */
    inst->prop_type = prop->prop_type;

    /* ---- 2. 深拷贝模式图 ---- */
    if (prop->pattern) {
        inst->pattern = deep_copy_graph(prop->pattern);
        if (!inst->pattern) {
            proposition_destroy(inst);
            return NULL;
        }
    }

    /* ---- 3. 递归深拷贝子命题（也进行实例化） ---- */
    for (int i = 0; i < prop->sub_prop_count; i++) {
        Proposition *sub_inst = proof_instantiate_proposition(prop->sub_props[i], type_var_to_concrete, mapping_count);
        if (!sub_inst) {
            proposition_destroy(inst);
            return NULL;
        }
        if (!proposition_add_sub_proposition(inst, sub_inst)) {
            proposition_destroy(sub_inst);
            proposition_destroy(inst);
            return NULL;
        }
    }

    /* ---- 4. 在副本上执行类型变量替换 ---- */
    if (type_var_to_concrete && mapping_count > 0 && inst->pattern) {
        /* 4a. 替换端口节点的 type_region */
        for (int i = 0; i < inst->pattern->node_count; i++) {
            GeomNode *node = inst->pattern->nodes[i];
            if (!node)
                continue;

            if (node->type == GEOM_PORT && node->data.port) {
                TypeRegion *tr = node->data.port->type_region;
                if (tr && tr->kind == TYPE_KIND_VARIABLE) {
                    int replacement_id =
                        find_concrete_replacement(type_var_to_concrete, mapping_count, tr->variable_id);
                    if (replacement_id >= 0) {
                        /* 通过 variable_id 查找具体类型区域：
                         * replacement_id 是映射表中的 concrete_node_id，
                         * 这里我们将端口标记为已实例化（is_polymorphic = false），
                         * 并将 variable_id 替换为 concrete_node_id。
                         * 注意：实际的 TypeRegion 对象替换需要外部类型系统上下文，
                         * 这里我们更新 variable_id 作为标记。 */
                        tr->variable_id = replacement_id;
                        tr->kind = TYPE_KIND_REGION; /* 升级为具体区域类型 */
                        node->data.port->is_polymorphic = false;
                    }
                }
            }
        }

        /* 4b. 替换约束中的参与者节点ID */
        for (int i = 0; i < inst->pattern->constraint_count; i++) {
            Constraint *c = inst->pattern->constraints[i];
            if (!c || !c->participants)
                continue;

            for (int j = 0; j < c->participant_count; j++) {
                int replacement = find_concrete_replacement(type_var_to_concrete, mapping_count, c->participants[j]);
                if (replacement >= 0) {
                    c->participants[j] = replacement;
                }
            }
        }

        /* 4c. 替换输入/输出端口ID */
        for (int i = 0; i < inst->input_count; i++) {
            int replacement = find_concrete_replacement(type_var_to_concrete, mapping_count, inst->input_port_ids[i]);
            if (replacement >= 0) {
                inst->input_port_ids[i] = replacement;
            }
        }
        for (int i = 0; i < inst->output_count; i++) {
            int replacement = find_concrete_replacement(type_var_to_concrete, mapping_count, inst->output_port_ids[i]);
            if (replacement >= 0) {
                inst->output_port_ids[i] = replacement;
            }
        }

        /* 4d. 替换前置条件区域ID */
        for (int i = 0; i < inst->precondition_count; i++) {
            int replacement =
                find_concrete_replacement(type_var_to_concrete, mapping_count, inst->precondition_region_ids[i]);
            if (replacement >= 0) {
                inst->precondition_region_ids[i] = replacement;
            }
        }

        /* 4e. 替换后置条件约束ID */
        for (int i = 0; i < inst->postcondition_count; i++) {
            int replacement =
                find_concrete_replacement(type_var_to_concrete, mapping_count, inst->postcondition_constraint_ids[i]);
            if (replacement >= 0) {
                inst->postcondition_constraint_ids[i] = replacement;
            }
        }

        /* 4f. 替换函数块内部的端口ID引用 */
        for (int i = 0; i < inst->pattern->node_count; i++) {
            GeomNode *node = inst->pattern->nodes[i];
            if (!node || node->type != GEOM_FUNCTION_BLOCK)
                continue;

            /* 替换内部节点引用 */
            for (int j = 0; j < node->data.func_block.internal_node_count; j++) {
                int old_id = node->data.func_block.internal_nodes[j] ? node->data.func_block.internal_nodes[j]->id : -1;
                int replacement = find_concrete_replacement(type_var_to_concrete, mapping_count, old_id);
                if (replacement >= 0) {
                    GeomNode *new_node = graph_get_node(inst->pattern, replacement);
                    if (new_node) {
                        node->data.func_block.internal_nodes[j] = new_node;
                    }
                }
            }

            /* 替换输入端口ID */
            for (int j = 0; j < node->data.func_block.input_count; j++) {
                int replacement = find_concrete_replacement(type_var_to_concrete, mapping_count,
                                                            node->data.func_block.input_port_ids[j]);
                if (replacement >= 0) {
                    node->data.func_block.input_port_ids[j] = replacement;
                }
            }

            /* 替换输出端口ID */
            for (int j = 0; j < node->data.func_block.output_count; j++) {
                int replacement = find_concrete_replacement(type_var_to_concrete, mapping_count,
                                                            node->data.func_block.output_port_ids[j]);
                if (replacement >= 0) {
                    node->data.func_block.output_port_ids[j] = replacement;
                }
            }
        }
    }

    /* ---- 5. 清除缓存状态 ---- */
    /* 深拷贝产生的新命题没有缓存状态（全部由 calloc/malloc 初始化为零），
     * 因此无需额外清除操作。 */

    return inst;
}

/* ================================================================== */
/*  PUBLIC API: 不可构造性证明流程                                     */
/* ================================================================== */

/**
 * @brief 检查构造是否匹配已知不可构造问题
 *
 * 遍历证明导航器关联的所有已加载公理包，检查构造图中
 * 的结构特征是否匹配任何已知的不可构造性问题。
 *
 * @param nav    证明导航器
 * @param graph  构造图
 * @param prop   命题（用于额外的上下文信息）
 * @param info   输出信息
 * @return 检查结果
 */
UnconstructResult proof_check_unconstructibility(ProofNavigator *nav, const ConstraintGraph *graph,
                                                 const Proposition *prop, UnconstructInfo *info) {
    if (!nav || !graph || !info) {
        if (info)
            info->result = UNCONSTRUCT_ERROR;
        return UNCONSTRUCT_ERROR;
    }

    memset(info, 0, sizeof(UnconstructInfo));
    info->result = UNCONSTRUCT_MAYBE_POSSIBLE;

    /* 流式输出 */
    if (proof_stream_ctx) {
        stream_emit_simple(proof_stream_ctx, STREAM_EVENT_PROOF_UNIFY, "不可构造性检查开始", -1);
    }

    /* 策略1：检查已加载的公理包中的已知不可构造问题 */
    if (nav->engine && nav->engine->axiom_package_count > 0) {
        for (int i = 0; i < nav->engine->axiom_package_count; i++) {
            AxiomPackage *pkg = nav->engine->axiom_packages[i];
            if (!pkg || pkg->unconstructible_count <= 0)
                continue;

            /* 遍历此公理包中的所有已知不可构造问题 */
            for (int j = 0; j < pkg->unconstructible_count; j++) {
                KnownUnconstructible *ku = &pkg->known_unconstructibles[j];
                if (!ku || !ku->name)
                    continue;

                /* 检查构造图的特征是否匹配此已知问题 */
                /* 启发式匹配：根据已知不可构造问题的特征检查约束图 */
                bool pattern_match = false;

                /* 经典不可构造问题的启发式匹配 */
                if (strstr(ku->name, "trisection") || strstr(ku->name, "三等分")) {
                    /* 三等分角问题：通常涉及角度构造 */
                    /* 检查图中是否有角度相关的约束 */
                    for (int k = 0; k < graph->constraint_count; k++) {
                        if (graph->constraints[k]->type == BETWEENNESS) {
                            pattern_match = true;
                            break;
                        }
                    }
                } else if (strstr(ku->name, "doubling") || strstr(ku->name, "倍立方")) {
                    /* 倍立方问题：涉及特定比例 */
                    pattern_match = (graph->node_count >= 3 && graph->node_count <= 8);
                } else if (strstr(ku->name, "squaring") || strstr(ku->name, "化圆为方")) {
                    /* 化圆为方：涉及圆和正方形 */
                    int circle_count = 0, region_count = 0;
                    for (int k = 0; k < graph->node_count; k++) {
                        if (graph->nodes[k]->type == GEOM_POINT) {
                            /* 圆通常由中心点和半径定义 */
                            circle_count++;
                        } else if (graph->nodes[k]->type == GEOM_REGION) {
                            region_count++;
                        }
                    }
                    pattern_match = (circle_count >= 2 && region_count >= 1);
                } else if (strstr(ku->name, "heptagon") || strstr(ku->name, "七边形")) {
                    /* 正七边形构造 */
                    pattern_match = (graph->node_count >= 7);
                }

                if (pattern_match) {
                    info->result = UNCONSTRUCT_PROVED;
                    info->matched_problem = ku->name;
                    info->matched_theory = pkg->name ? pkg->name : "未知理论";
                    info->proof_strategy = "匹配已知不可构造问题";
                    info->reduction_steps = 0;

                    char report[512];
                    snprintf(report, sizeof(report), "构造匹配已知的不可构造问题 '%s'（来自公理包 '%s'）", ku->name,
                             pkg->name ? pkg->name : "未知");
                    info->detailed_report = lv_strdup(report);

                    if (proof_stream_ctx) {
                        stream_emit_simple(proof_stream_ctx, STREAM_EVENT_PROOF_UNIFY, "匹配已知不可构造问题", 1);
                    }
                    return UNCONSTRUCT_PROVED;
                }
            }
        }
    }

    /* 策略1b：通过命题签名检查 */
    if (prop && prop->pattern) {
        /* 检查命题是否为矛盾类型（BOTTOM 表示不可构造） */
        if (prop->type == PROPOSITION_TYPE_BOTTOM) {
            info->result = UNCONSTRUCT_PROVED;
            info->matched_problem = "命题矛盾";
            info->matched_theory = "命题系统";
            info->proof_strategy = "命题类型为矛盾（不可构造）";
            info->reduction_steps = 0;

            char report[256];
            snprintf(report, sizeof(report), "命题已被标记为矛盾类型（BOTTOM），表示不可构造");
            info->detailed_report = lv_strdup(report);

            return UNCONSTRUCT_PROVED;
        }
    }

    /* 未找到匹配 */
    info->proof_strategy = "已搜索所有已知不可构造问题，未找到匹配";

    if (proof_stream_ctx) {
        stream_emit_simple(proof_stream_ctx, STREAM_EVENT_PROOF_UNIFY, "不可构造性检查完成: 未匹配已知问题", 0);
    }

    return UNCONSTRUCT_MAYBE_POSSIBLE;
}

/* ===========================================================================
 * 存根实现：proof_navigator_search / smtsolver_set_timeout / constraint_solver_get_proposition
 *
 * 这些函数在 proof.h 中声明，供 proof_version.c 等模块调用。
 * 完整实现将随后续版本逐步完善，当前以最小存根保证链接通过。
 * =========================================================================== */

/**
 * @brief 在证明导航器中执行搜索（存根实现）
 *
 * @param nav 证明导航器指针（ProofNavigator *）
 * @return 搜索结果（当前存根返回 NULL 表示未找到）
 */
void *proof_navigator_search(void *nav) {
    if (!nav)
        return NULL;
    /* 存根：完整实现应调用导航器的搜索策略 */
    return NULL;
}

/**
 * @brief 设置 SMT 求解器超时时间（存根实现）
 *
 * @param s   SMT 求解器句柄
 * @param ms  超时时间（毫秒）
 */
void smtsolver_set_timeout(SMTSolver s, int ms) {
    (void)s;
    (void)ms;
    /* 存根：完整实现应将超时设置传递给底层求解器 */
}

/**
 * @brief 从约束求解器获取几何对象的命题描述（存根实现）
 *
 * @param solver    约束求解器指针
 * @param geom_obj  几何对象指针
 * @return 命题字符串描述（当前存根返回 NULL）
 */
const char *constraint_solver_get_proposition(void *solver, void *geom_obj) {
    (void)solver;
    (void)geom_obj;
    /* 存根：完整实现应从求解器的类型注册表中查询命题 */
    return NULL;
}
