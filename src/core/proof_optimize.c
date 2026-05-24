/**
 * @file proof_optimize.c
 * @brief 证明步骤优化与增强回溯搜索（v3.3.0 新增）
 *
 * 提供：
 *   1. 证明步骤压缩 —— 移除冗余步骤、合并等价步骤
 *   2. 增强回溯搜索 —— 多策略竞争搜索、自动回滚
 *   3. 搜索树结果应用 —— 将搜索树找到的路径标记到证明中
 *   4. 从导航器反向构建搜索树
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "constraint_graph.h"
#include "lv00_internal.h"
#include "proof.h"
#include "solver.h"

/* ============================================================================
 * 证明步骤优化 —— 压缩、冗余去除、等价合并
 * ============================================================================ */

/**
 * @brief 检查两个证明步骤是否等价（相同的类型和核心数据）
 *
 * 两个步骤等价当且仅当：
 * - 类型相同
 * - 核心操作对象相同（node_id / constraint_id / rule_id / func_block_id）
 * - 规范化步骤有相同的合并节点集合
 */
static bool proof_steps_equivalent(const ProofStep *a, const ProofStep *b) {
    if (!a || !b)
        return false;
    if (a->type != b->type)
        return false;

    switch (a->type) {
        case PROOF_STEP_ADD_NODE:
            return a->node_id == b->node_id;
        case PROOF_STEP_ADD_CONSTRAINT:
            return a->constraint_id == b->constraint_id;
        case PROOF_STEP_REWRITE:
            return a->rule_id == b->rule_id;
        case PROOF_STEP_FUNCTION_APP:
        case PROOF_STEP_PACK_FUNCTION:
            return a->func_block_id == b->func_block_id;
        case PROOF_STEP_NORMALIZATION:
            if (a->merged_count != b->merged_count)
                return false;
            if (a->retained_node_id != b->retained_node_id)
                return false;
            for (int i = 0; i < a->merged_count; i++) {
                if (a->merged_node_ids[i] != b->merged_node_ids[i])
                    return false;
            }
            return true;
        case PROOF_STEP_UNIFY:
        case PROOF_STEP_EX_FALSO:
        case PROOF_STEP_ORACLE:
            return true;
        default:
            return false;
    }
}

/**
 * @brief 压缩证明导航器中的冗余步骤
 *
 * 压缩策略：
 *   1. 移除相邻等价步骤（如连续两次添加同一个节点），保留第一个并合并注释
 *   2. 合并连续的归一化步骤为一个
 *   3. 调整所有依赖关系引用
 *   4. 更新断点索引
 *
 * @param nav  证明导航器
 * @return 压缩后的步骤数，失败返回 -1
 */
int proof_navigator_compress(ProofNavigator *nav) {
    if (!nav || nav->step_count == 0)
        return -1;

    int original_count = nav->step_count;
    int *keep_flags = (int *) lv00_malloc(nav->step_count * sizeof(int));
    if (!keep_flags)
        return -1;

    /* 初始化：全部标记为保留 */
    for (int i = 0; i < nav->step_count; i++) {
        keep_flags[i] = 1;
    }

    /* 第一遍：检测相邻等价步骤，移除冗余 */
    for (int i = 1; i < nav->step_count; i++) {
        if (!proof_steps_equivalent(nav->steps[i - 1], nav->steps[i]))
            continue;

        /* 合并注释（使用 snprintf 替代 strcpy，确保缓冲区安全） */
        if (nav->steps[i]->note && nav->steps[i]->note[0] != '\0') {
            ProofStep *prev = nav->steps[i - 1];
            if (prev->note) {
                size_t old_len = strlen(prev->note);
                size_t new_len = strlen(nav->steps[i]->note);
                char *merged = lv00_realloc(prev->note, old_len + new_len + 8);
                if (merged) {
                    prev->note = merged;
                    /* [Bug修复] snprintf → lv00_strlcpy 防止缓冲区溢出 */
                    lv00_strlcpy(prev->note + old_len, " | ", 4);
                    lv00_strlcpy(prev->note + old_len + 3, nav->steps[i]->note, new_len + 1);
                }
            } else {
                prev->note = lv00_malloc(strlen(nav->steps[i]->note) + 1);
                if (prev->note)
                    /* [Bug修复] snprintf → lv00_strlcpy 防止缓冲区溢出 */
                    lv00_strlcpy(prev->note, nav->steps[i]->note, strlen(nav->steps[i]->note) + 1);
            }
        }

        /* 转移后继依赖 */
        for (int d = 0; d < nav->steps[i]->dependent_count; d++) {
            int dep_step_id = nav->steps[i]->dependent_step_ids[d];
            if (dep_step_id >= nav->step_count)
                continue;
            ProofStep *succ = nav->steps[dep_step_id];
            for (int dd = 0; dd < succ->dependency_count; dd++) {
                if (succ->dependency_step_ids[dd] == nav->steps[i]->id) {
                    succ->dependency_step_ids[dd] = nav->steps[i - 1]->id;
                }
            }
            int found = 0;
            for (int dd = 0; dd < nav->steps[i - 1]->dependent_count; dd++) {
                if (nav->steps[i - 1]->dependent_step_ids[dd] == dep_step_id) {
                    found = 1;
                    break;
                }
            }
            if (!found) {
                nav->steps[i - 1]->dependent_count++;
                int *new_deps = lv00_realloc(nav->steps[i - 1]->dependent_step_ids,
                                             nav->steps[i - 1]->dependent_count * sizeof(int));
                if (new_deps) {
                    nav->steps[i - 1]->dependent_step_ids = new_deps;
                    nav->steps[i - 1]->dependent_step_ids[nav->steps[i - 1]->dependent_count - 1] = dep_step_id;
                }
            }
        }
        keep_flags[i] = 0;
    }

    /* 第二遍：合并连续归一化步骤 */
    int last_norm_idx = -1;
    for (int i = 0; i < nav->step_count; i++) {
        if (!keep_flags[i])
            continue;
        if (nav->steps[i]->type == PROOF_STEP_NORMALIZATION) {
            if (last_norm_idx >= 0) {
                ProofStep *prev_norm = nav->steps[last_norm_idx];
                ProofStep *cur_norm = nav->steps[i];
                int total_merged = prev_norm->merged_count + cur_norm->merged_count;
                int *new_merged = lv00_realloc(prev_norm->merged_node_ids, total_merged * sizeof(int));
                if (new_merged) {
                    prev_norm->merged_node_ids = new_merged;
                    memcpy(prev_norm->merged_node_ids + prev_norm->merged_count, cur_norm->merged_node_ids,
                           cur_norm->merged_count * sizeof(int));
                    prev_norm->merged_count = total_merged;
                }
                keep_flags[i] = 0;
            } else {
                last_norm_idx = i;
            }
        } else {
            last_norm_idx = -1;
        }
    }

    /* 重建步骤数组 */
    int new_count = 0;
    ProofStep **new_steps = lv00_malloc(nav->step_count * sizeof(ProofStep *));
    int *id_map = (int *) lv00_malloc(nav->step_count * sizeof(int));
    if (!new_steps || !id_map) {
        lv00_free((void **) &keep_flags);
        lv00_free((void **) &new_steps);
        lv00_free((void **) &id_map);
        return -1;
    }

    for (int i = 0; i < nav->step_count; i++) {
        if (keep_flags[i]) {
            id_map[i] = new_count;
            new_steps[new_count] = nav->steps[i];
            new_steps[new_count]->id = new_count;
            new_count++;
        } else {
            id_map[i] = -1;
            proof_step_destroy(nav->steps[i]);
        }
    }

    /* 更新依赖引用 */
    for (int i = 0; i < new_count; i++) {
        ProofStep *step = new_steps[i];
        for (int d = 0; d < step->dependency_count; d++) {
            int old_id = step->dependency_step_ids[d];
            if (old_id >= 0 && old_id < nav->step_count && id_map[old_id] >= 0) {
                step->dependency_step_ids[d] = id_map[old_id];
            }
        }
        int valid_succ = 0;
        for (int d = 0; d < step->dependent_count; d++) {
            int old_id = step->dependent_step_ids[d];
            if (old_id >= 0 && old_id < nav->step_count && id_map[old_id] >= 0) {
                step->dependent_step_ids[valid_succ] = id_map[old_id];
                valid_succ++;
            }
        }
        step->dependent_count = valid_succ;
    }

    lv00_free((void **) &nav->steps);

    nav->steps = lv00_malloc(new_count * sizeof(ProofStep *));
    if (!nav->steps) {
        lv00_free((void **) &new_steps);
        lv00_free((void **) &keep_flags);
        lv00_free((void **) &id_map);
        return -1;
    }
    memcpy(nav->steps, new_steps, new_count * sizeof(ProofStep *));
    nav->step_count = new_count;

    /* 更新断点索引 */
    int new_bp_count = 0;
    for (int i = 0; i < nav->breakpoint_count; i++) {
        int old_idx = nav->breakpoint_indices[i];
        if (old_idx >= 0 && old_idx < original_count && id_map[old_idx] >= 0) {
            new_bp_count++;
        }
    }
    int *new_bps = lv00_malloc(new_bp_count * sizeof(int));
    if (new_bps) {
        int bp_idx = 0;
        for (int i = 0; i < nav->breakpoint_count; i++) {
            int old_idx = nav->breakpoint_indices[i];
            if (old_idx >= 0 && old_idx < original_count && id_map[old_idx] >= 0) {
                new_bps[bp_idx++] = id_map[old_idx];
            }
        }
        lv00_free((void **) &nav->breakpoint_indices);
        nav->breakpoint_indices = new_bps;
        nav->breakpoint_count = new_bp_count;
    }

    if (nav->current_step >= nav->step_count) {
        nav->current_step = nav->step_count > 0 ? nav->step_count - 1 : -1;
    }

    lv00_free((void **) &new_steps);
    lv00_free((void **) &keep_flags);
    lv00_free((void **) &id_map);

    return new_count;
}

/**
 * @brief 获取压缩后的等价步骤数量（不实际执行压缩）
 *
 * @param nav  证明导航器
 * @return 预估的压缩后步骤数，失败返回 -1
 */
int proof_navigator_estimate_compressed_count(const ProofNavigator *nav) {
    if (!nav || nav->step_count == 0)
        return -1;

    int estimated = 0;
    int last_norm = 0;

    for (int i = 0; i < nav->step_count; i++) {
        if (i > 0 && proof_steps_equivalent(nav->steps[i - 1], nav->steps[i])) {
            continue;
        }
        if (nav->steps[i]->type == PROOF_STEP_NORMALIZATION) {
            if (last_norm)
                continue;
            last_norm = 1;
        } else {
            last_norm = 0;
        }
        estimated++;
    }

    return estimated;
}

/* ============================================================================
 * 增强回溯搜索 —— 多策略竞争搜索与剪枝
 * ============================================================================ */

/**
 * @brief 使用多策略引擎执行回溯搜索证明
 *
 * 在证明搜索树上执行实际的多策略搜索：
 *   1. 评估每种策略的适用性
 *   2. 创建搜索树根节点
 *   3. 按回退顺序尝试每种策略
 *   4. 每次策略失败时创建回溯节点并尝试下一个策略
 *   5. 记录成功/失败/剪枝信息到搜索树
 *
 * @param nav       证明导航器（提供目标命题和构造图）
 * @param mse       多策略引擎（提供策略列表）
 * @param tree      搜索树（用于记录搜索过程）
 * @param max_strategies 最多尝试的策略数（0 = 不限制）
 * @return 成功的策略类型，失败返回 PROOF_STRATEGY_COUNT
 */
ProofStrategyType proof_backtrack_search(ProofNavigator *nav, ProofMultiStrategy *mse, ProofSearchTree *tree,
                                         int max_strategies) {
    if (!nav || !mse || !tree)
        return PROOF_STRATEGY_COUNT;

    /* 创建搜索根节点 */
    BacktrackNode *root = backtrack_node_create(BACKTRACK_CHOICE_POINT, "开始多策略证明搜索");
    if (!root)
        return PROOF_STRATEGY_COUNT;
    proof_search_tree_add_child(tree, NULL, root);

    /* 评估所有策略的适用性 */
    ProofStrategyType applicable[PROOF_STRATEGY_COUNT];
    int applicable_count = proof_multi_strategy_evaluate_applicability(mse, nav->construction, nav->target_prop,
                                                                       applicable, PROOF_STRATEGY_COUNT);

    if (applicable_count == 0) {
        BacktrackNode *fail = backtrack_node_create(BACKTRACK_FAILURE, "无适用策略");
        proof_search_tree_add_child(tree, root, fail);
        return PROOF_STRATEGY_COUNT;
    }

    /* 记录起始步骤数，用于失败回滚 */
    int base_step_count = nav->step_count;
    int strategies_tried = 0;
    ProofStrategyType successful = PROOF_STRATEGY_COUNT;

    /* 按回退顺序尝试每种策略 */
    for (int i = 0; i < mse->fallback_count; i++) {
        int strat_idx = mse->fallback_order[i];
        if (strat_idx < 0 || strat_idx >= PROOF_STRATEGY_COUNT)
            continue;

        ProofStrategyDescriptor *desc = &mse->strategies[strat_idx];
        if (desc->status == PROOF_STRATEGY_UNAVAILABLE)
            continue;
        if (!desc->execute)
            continue;

        /* 检查是否为适用策略 */
        int is_applicable = 0;
        for (int a = 0; a < applicable_count; a++) {
            if (applicable[a] == (ProofStrategyType) strat_idx) {
                is_applicable = 1;
                break;
            }
        }
        if (!is_applicable)
            continue;

        /* 创建策略尝试节点 */
        char node_label[256];
        snprintf(node_label, sizeof(node_label), "尝试策略: %s", desc->name);
        BacktrackNode *try_node = backtrack_node_create(BACKTRACK_CHOICE_POINT, node_label);
        if (!try_node)
            continue;

        backtrack_node_mark_backtrack(try_node, desc->name);
        proof_search_tree_add_child(tree, root, try_node);

        /* 激活并执行策略 */
        proof_multi_strategy_activate(mse, (ProofStrategyType) strat_idx);
        int steps_before = nav->step_count;
        bool result = desc->execute(mse, nav);
        strategies_tried++;

        if (result) {
            /* 成功 */
            successful = (ProofStrategyType) strat_idx;
            BacktrackNode *success_node = backtrack_node_create(BACKTRACK_SUCCESS, "策略执行成功");
            if (success_node) {
                success_node->step_index = nav->step_count - 1;
                proof_search_tree_add_child(tree, try_node, success_node);
            }
            tree->success_paths++;
            break;
        } else {
            /* 失败：回滚步骤 */
            int steps_added = nav->step_count - steps_before;
            BacktrackNode *fail_node = backtrack_node_create(BACKTRACK_FAILURE, "策略执行失败");
            if (fail_node) {
                /* 撤销该策略添加的步骤 */
                for (int s = steps_before; s < nav->step_count; s++) {
                    proof_step_destroy(nav->steps[s]);
                }
                nav->step_count = steps_before;
                nav->current_step = steps_before > 0 ? steps_before - 1 : -1;

                char fail_label[320];
                snprintf(fail_label, sizeof(fail_label), "%s 失败 (添加了 %d 步后回滚)", desc->name, steps_added);
                lv00_free((void **) &fail_node->label);
                /* 使用 snprintf 替代 strcpy，确保缓冲区安全 */
                fail_node->label = lv00_malloc(strlen(fail_label) + 1);
                if (fail_node->label)
                    /* [Bug修复] snprintf → lv00_strlcpy 防止缓冲区溢出 */
                    lv00_strlcpy(fail_node->label, fail_label, strlen(fail_label) + 1);
                fail_node->step_index = steps_before;
                proof_search_tree_add_child(tree, try_node, fail_node);
            }
            tree->failure_paths++;
            tree->backtrack_count++;
        }

        /* 限制策略数量 */
        if (max_strategies > 0 && strategies_tried >= max_strategies) {
            BacktrackNode *limit_node = backtrack_node_create(BACKTRACK_PRUNE, "达到策略数量上限");
            proof_search_tree_add_child(tree, root, limit_node);
            break;
        }
    }

    /* 如果没有策略成功，回滚到起始状态 */
    if (successful == PROOF_STRATEGY_COUNT) {
        for (int s = base_step_count; s < nav->step_count; s++) {
            proof_step_destroy(nav->steps[s]);
        }
        nav->step_count = base_step_count;
        nav->current_step = base_step_count > 0 ? base_step_count - 1 : -1;
    } else {
        proof_search_tree_set_strategy(tree, proof_strategy_type_to_string(successful));
    }

    return successful;
}

/**
 * @brief 应用证明搜索树的结果到导航器
 *
 * 将搜索树中找到的成功路径对应的步骤信息标记到证明导航器中。
 *
 * @param nav   证明导航器
 * @param tree  已完成搜索的搜索树
 * @return 成功路径数量
 */
int proof_apply_search_tree_results(ProofNavigator *nav, const ProofSearchTree *tree) {
    if (!nav || !tree)
        return 0;

    int applied = 0;
    for (int i = 0; i < tree->node_count && applied < tree->success_paths; i++) {
        BacktrackNode *node = tree->all_nodes[i];
        if (!node || node->type != BACKTRACK_SUCCESS)
            continue;

        if (node->step_index >= 0 && node->step_index < nav->step_count) {
            ProofStep *step = nav->steps[node->step_index];
            if (step && (!step->note || step->note[0] == '\0')) {
                char tree_note[256];
                snprintf(tree_note, sizeof(tree_note), "[搜索树路径] 策略: %s",
                         node->strategy_name ? node->strategy_name : "未知");
                proof_step_set_note(step, tree_note);
            }
            applied++;
        }
    }

    return applied;
}

/**
 * @brief 从导航器步骤生成搜索树
 *
 * 遍历证明导航器中的步骤，为每个步骤创建对应的搜索树节点。
 * 这允许从现有的手工构建的证明中反向生成搜索树表示。
 *
 * @param nav   证明导航器
 * @param tree  输出搜索树（应已初始化）
 * @return 创建的节点数量，失败返回 -1
 */
int proof_build_search_tree_from_navigator(const ProofNavigator *nav, ProofSearchTree *tree) {
    if (!nav || !tree || nav->step_count == 0)
        return -1;

    /* 创建根节点 */
    BacktrackNode *root = backtrack_node_create(BACKTRACK_CHOICE_POINT, "证明搜索树（从现有导航器重建）");
    if (!root)
        return -1;
    proof_search_tree_add_child(tree, NULL, root);

    BacktrackNode *current_parent = root;
    int created = 1;

    for (int i = 0; i < nav->step_count; i++) {
        ProofStep *step = nav->steps[i];
        if (!step)
            continue;

        /* 根据步骤类型创建对应节点 */
        BacktrackNodeType node_type;
        switch (step->type) {
            case PROOF_STEP_UNIFY:
                node_type = (step->color == PROOF_COLOR_GREEN) ? BACKTRACK_SUCCESS : BACKTRACK_FAILURE;
                break;
            case PROOF_STEP_NORMALIZATION:
            case PROOF_STEP_REWRITE:
            case PROOF_STEP_ORACLE:
                node_type = BACKTRACK_CHOICE_POINT;
                break;
            default:
                node_type = BACKTRACK_CHOICE_POINT;
                break;
        }

        char label[256];
        snprintf(label, sizeof(label), "Step %d: %s [%s]", step->id, proof_step_type_to_string(step->type),
                 proof_color_to_string(step->color));

        BacktrackNode *node = backtrack_node_create(node_type, label);
        if (!node)
            break;

        node->step_index = step->id;
        node->color = step->color;
        node->explored = true;

        proof_search_tree_add_child(tree, current_parent, node);
        created++;

        if (step->is_breakpoint) {
            backtrack_node_mark_backtrack(node, "用户断点");
        }

        if (node_type != BACKTRACK_SUCCESS && node_type != BACKTRACK_FAILURE) {
            current_parent = node;
        }
    }

    return created;
}
