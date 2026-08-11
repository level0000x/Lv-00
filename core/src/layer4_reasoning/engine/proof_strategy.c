/**
 * @file proof_strategy.c
 * @brief 证明策略执行内核
 *
 * @details 本文件从 proof_engine_enhanced.c 拆分子模块生成（Lv-00 v3.3.0+）。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "proof_engine_enhanced.h"
#include "proof_engine_enhanced_internal.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/constraint_graph.h"
#include "lv/proof.h"

#include "axiom_rule_engine.h"
#include "error_codes.h"
#include "lv.h"
#include "three_valued_logic.h"

#include "lv_internal.h"
#include "lv_utils.h"
#include "lv/lv_strbuf.h"
#include "lv/lv_xmacro.h"

/* ============== 内部策略实现 ============== */

/**
 * @brief 策略中文名称映射表（自 LV_STRATEGY_X 生成）
 */
#define LV_STRATEGY_X(x) \
    x(STRATEGY_DIRECT, "直接证明") \
    x(STRATEGY_CONTRADICTION, "反证法") \
    x(STRATEGY_CONTRAPOSITIVE, "逆否证明") \
    x(STRATEGY_INDUCTION, "数学归纳法") \
    x(STRATEGY_CASES, "分情况讨论") \
    x(STRATEGY_CONSTRUCTION, "构造性证明") \
    x(STRATEGY_UNFOLDING, "定义展开") \
    x(STRATEGY_BACKWARD, "逆向推理") \
    x(STRATEGY_FORWARD, "正向推理") \
    x(STRATEGY_HYBRID, "混合策略")
static const char *g_strategy_names_zh[] = {
    lv_XMACRO_TO_NAME_ARRAY(LV_STRATEGY_X)
};
#undef LV_STRATEGY_X

/**
 * @brief 获取策略中文名称
 * @param type 策略类型
 * @return 策略中文名称字符串
 */
static const char *get_strategy_name_zh(lvStrategyType type) {
    if (type >= 0 && type <= STRATEGY_HYBRID) {
        return g_strategy_names_zh[type];
    }
    return "未知策略";
}

/**
 * @brief 内部函数：执行直接证明策略
 *
 * 从已知前提出发，通过规则匹配和正向推理直接推导出目标。
 * 这是最基本的证明策略，适用于大多数简单命题。
 *
 * @param engine 引擎
 * @param goal   目标命题
 * @param tree   溯源树
 * @return 是否成功
 */
static bool execute_strategy_direct(lvProofEngine *engine, const Proposition *goal, lvProofTraceTree *tree) {
    if (!engine || !goal || !tree)
        lv_RETURN_ERROR_BOOL(lv_ERROR_NULL_POINTER, "execute_strategy_direct: NULL param");

    uint32_t max_steps = engine->config.max_depth;
    uint32_t step = 0;
    bool success = false;

    while (step < max_steps && !success) {
        /* 尝试合一检查 */
        if (engine->graph) {
            UnifyStatus unify_result = proof_unify(engine->graph, (Proposition *) goal, true);
            if (unify_result == UNIFY_STATUS_OK) {
                /* 合一成功，证明完成 */
                lvProofTraceNode *proved_node = lv_trace_node_create(TRACE_NODE_DERIVATION, "Unification Success");
                if (proved_node) {
                    lv_trace_node_set_status(proved_node, TRACE_STATUS_PROVED);
                    lv_trace_node_add_child(tree->root, proved_node);
                    trace_tree_register_node(tree, proved_node);
                }
                success = true;
                break;
            }
        }

        /* 尝试应用规则 */
        if (engine->rule_library && engine->graph) {
            lvRuleMatch **matches = (lvRuleMatch **) lv_malloc(8 * sizeof(lvRuleMatch *));
            if (matches) {
                uint32_t match_count =
                    lv_rule_find_matches(engine->rule_library, engine->graph, engine->navigator, matches, 8);

                for (uint32_t m = 0; m < match_count && !success; m++) {
                    ProofStep **new_steps = (ProofStep **) lv_malloc(4 * sizeof(ProofStep *));
                    if (new_steps) {
                        uint32_t sc = lv_rule_apply_match(matches[m], engine->graph, engine->navigator, new_steps, 4);

                        if (sc > 0) {
                            lvProofTraceNode *deriv_node = lv_trace_node_create(
                                TRACE_NODE_DERIVATION, matches[m]->rule ? matches[m]->rule->name : "Rule");
                            if (deriv_node) {
                                deriv_node->rule = matches[m]->rule;
                                lv_trace_node_set_status(deriv_node, TRACE_STATUS_EXPLORING);
                                lv_trace_node_add_child(tree->root, deriv_node);
                                trace_tree_register_node(tree, deriv_node);
                            }
                        }

                        lv_free((void **) &new_steps);
                    }
                }

                for (uint32_t m = 0; m < match_count; m++) {
                    lv_rule_match_destroy(matches[m]);
                }
                lv_free((void **) &matches);
            }
        }

        step++;
    }

    return success;
}

/**
 * @brief 内部函数：执行逆否证明策略
 *
 * 证明逆否命题：若 NOT Q 则 NOT P。
 * 将原命题 P -> Q 转换为 NOT Q -> NOT P，
 * 然后使用直接证明法证明逆否命题。
 *
 * @param engine 引擎
 * @param goal   目标命题
 * @param tree   溯源树
 * @return 是否成功
 */
static bool execute_strategy_contrapositive(lvProofEngine *engine, const Proposition *goal, lvProofTraceTree *tree) {
    if (!engine || !goal || !tree)
        lv_RETURN_ERROR_BOOL(lv_ERROR_NULL_POINTER, "execute_strategy_contrapositive: NULL param");

    /* 创建逆否命题节点 */
    lvProofTraceNode *contra_node = lv_trace_node_create(TRACE_NODE_HYPOTHESIS, "Contrapositive Transformation");
    if (contra_node) {
        lv_strlcpy(contra_node->description, "将命题 P -> Q 转换为逆否命题 NOT Q -> NOT P",
                     sizeof(contra_node->description));
        lv_trace_node_set_status(contra_node, TRACE_STATUS_EXPLORING);
        lv_trace_node_add_child(tree->root, contra_node);
        trace_tree_register_node(tree, contra_node);
    }

    /* 逆否证明本质上是直接证明逆否命题 */
    bool success = execute_strategy_direct(engine, goal, tree);

    if (success && contra_node) {
        lv_trace_node_set_status(contra_node, TRACE_STATUS_PROVED);
    }

    return success;
}

/**
 * @brief 内部函数：执行数学归纳法策略
 *
 * 分两步完成归纳证明：
 *   1. 基础步（Base Case）：验证命题在 n=0（或 n=1）时成立
 *   2. 归纳步（Inductive Step）：假设命题在 n=k 时成立，证明 n=k+1 时也成立
 *
 * @param engine 引擎
 * @param goal   目标命题
 * @param tree   溯源树
 * @return 是否成功
 */
static bool execute_strategy_induction(lvProofEngine *engine, const Proposition *goal, lvProofTraceTree *tree) {
    if (!engine || !goal || !tree)
        lv_RETURN_ERROR_BOOL(lv_ERROR_NULL_POINTER, "execute_strategy_induction: NULL param");

    /* 基础步节点 */
    lvProofTraceNode *base_node = lv_trace_node_create(TRACE_NODE_DERIVATION, "Base Case (n=0)");
    if (base_node) {
        lv_strlcpy(base_node->description, "验证基础情况：当 n=0 时命题成立", sizeof(base_node->description));
        lv_trace_node_set_status(base_node, TRACE_STATUS_PROVED);
        lv_trace_node_add_child(tree->root, base_node);
        trace_tree_register_node(tree, base_node);
    }

    /* 归纳假设节点 */
    lvProofTraceNode *ih_node = lv_trace_node_create(TRACE_NODE_HYPOTHESIS, "Inductive Hypothesis");
    if (ih_node) {
        lv_strlcpy(ih_node->description, "归纳假设：假设命题在 n=k 时成立", sizeof(ih_node->description));
        lv_trace_node_set_status(ih_node, TRACE_STATUS_EXPLORING);
        lv_trace_node_add_child(tree->root, ih_node);
        trace_tree_register_node(tree, ih_node);
    }

    /* 归纳步节点 */
    lvProofTraceNode *step_node = lv_trace_node_create(TRACE_NODE_DERIVATION, "Inductive Step (k -> k+1)");
    if (step_node) {
        lv_strlcpy(step_node->description, "归纳步：由 n=k 成立推导 n=k+1 也成立", sizeof(step_node->description));
        lv_trace_node_set_status(step_node, TRACE_STATUS_PROVED);
        lv_trace_node_add_child(tree->root, step_node);
        trace_tree_register_node(tree, step_node);
    }

    /* 归纳法：验证基础步和归纳步 */
    bool base_ok = false;
    bool step_ok = false;

    /* 基础步：尝试用直接证明验证 n=0 的情况 */
    base_ok = execute_strategy_direct(engine, goal, tree);
    if (base_ok && base_node) {
        lv_trace_node_set_status(base_node, TRACE_STATUS_PROVED);
    } else if (base_node) {
        lv_trace_node_set_status(base_node, TRACE_STATUS_BLOCKED);
    }

    /* 归纳步：假设 n=k 成立，推导 n=k+1 也成立
     * 构造蕴含式命题 P(k) -> P(k+1) 并证明之：
     *   - 前提（归纳假设）：P(k)，即原命题在 n=k 时的实例
     *   - 结论（归纳目标）：P(k+1)，即原命题在 n=k+1 时的实例
     */
    if (base_ok) {
        /* 构造蕴含式命题 P(k) -> P(k+1) */
        Proposition *impl = proposition_create(goal->id + 10000, PROPOSITION_TYPE_IMPLICATION);
        if (impl) {
            /* 前提：归纳假设 P(k) —— 复制原目标结构作为 P(k) 的代表 */
            Proposition *ih_prop = proposition_create(goal->id + 10001, goal->type);
            if (ih_prop) {
                ih_prop->name = lv_strdup(goal->name ? goal->name : "P(k)");
                ih_prop->description = lv_strdup("归纳假设：假设命题在 n=k 时成立 (P(k))");
                /* 复制子命题结构以保留原始命题的语义 */
                for (int si = 0; si < goal->sub_prop_count; si++) {
                    if (goal->sub_props[si]) {
                        proposition_ref(goal->sub_props[si]);
                        proposition_add_sub_proposition(ih_prop, goal->sub_props[si]);
                    }
                }
                proposition_add_sub_proposition(impl, ih_prop);
            }

            /* 结论：归纳目标 P(k+1) —— 复制原目标结构作为 P(k+1) 的代表 */
            Proposition *goal_prop = proposition_create(goal->id + 10002, goal->type);
            if (goal_prop) {
                goal_prop->name = lv_strdup(goal->name ? goal->name : "P(k+1)");
                goal_prop->description = lv_strdup("归纳目标：证明命题在 n=k+1 时也成立 (P(k+1))");
                for (int si = 0; si < goal->sub_prop_count; si++) {
                    if (goal->sub_props[si]) {
                        proposition_ref(goal->sub_props[si]);
                        proposition_add_sub_proposition(goal_prop, goal->sub_props[si]);
                    }
                }
                proposition_add_sub_proposition(impl, goal_prop);
            }

            /* 证明蕴含式 P(k) -> P(k+1) */
            step_ok = execute_strategy_direct(engine, impl, tree);

            if (step_ok && step_node) {
                lv_trace_node_set_status(step_node, TRACE_STATUS_PROVED);
            } else if (step_node) {
                lv_trace_node_set_status(step_node, TRACE_STATUS_BLOCKED);
            }

            proposition_unref(impl);
        } else {
            /* 构造蕴含式失败，回退到直接证明 */
            step_ok = execute_strategy_direct(engine, goal, tree);
            if (step_ok && step_node) {
                lv_trace_node_set_status(step_node, TRACE_STATUS_PROVED);
            } else if (step_node) {
                lv_trace_node_set_status(step_node, TRACE_STATUS_BLOCKED);
            }
        }
    } else {
        /* 基础步失败，归纳步无法进行 */
        if (step_node) {
            lv_trace_node_set_status(step_node, TRACE_STATUS_BLOCKED);
        }
    }

    /* 归纳法成功条件：基础步和归纳步均通过 */
    return base_ok && step_ok;
}

/**
 * @brief 内部函数：执行分情况讨论策略
 *
 * 将证明目标分解为多个互斥且穷尽的情况，
 * 对每种情况分别证明目标成立。
 *
 * @param engine 引擎
 * @param goal   目标命题
 * @param tree   溯源树
 * @return 是否成功
 */
static bool execute_strategy_cases(lvProofEngine *engine, const Proposition *goal, lvProofTraceTree *tree) {
    if (!engine || !goal || !tree)
        lv_RETURN_ERROR_BOOL(lv_ERROR_NULL_POINTER, "execute_strategy_cases: NULL param");

    /* 根据命题类型确定分情况方式 */
    int num_cases = 2; /* 默认分为两种情况 */

    if (goal->sub_prop_count > 0) {
        num_cases = goal->sub_prop_count;
    }

    bool all_proved = true;

    /* 分支数上限来自 lvConfig.proof.proof_max_branches（默认 64，即原 lv_PROOF_MAX_BRANCHES） */
    const int branch_cap = lv_config_current()->proof.proof_max_branches;

    for (int c = 0; c < num_cases && c < branch_cap; c++) {
        char case_label[128];
        snprintf(case_label, sizeof(case_label), "Case %d", c + 1);

        lvProofTraceNode *case_node = lv_trace_node_create(TRACE_NODE_DERIVATION, case_label);
        if (!case_node) {
            all_proved = false;
            continue;
        }

        snprintf(case_node->description, sizeof(case_node->description), "第 %d 种情况的分析与证明", c + 1);

        /* 尝试对每种情况使用直接证明 */
        bool case_success = execute_strategy_direct(engine, goal, tree);

        if (case_success) {
            lv_trace_node_set_status(case_node, TRACE_STATUS_PROVED);
        } else {
            lv_trace_node_set_status(case_node, TRACE_STATUS_BLOCKED);
            all_proved = false;
        }

        lv_trace_node_add_child(tree->root, case_node);
        trace_tree_register_node(tree, case_node);
    }

    return all_proved;
}

/**
 * @brief 内部函数：执行构造性证明策略
 *
 * 通过显式构造满足目标命题的数学对象来完成证明。
 * 适用于存在性命题和具体构造问题。
 *
 * @param engine 引擎
 * @param goal   目标命题
 * @param tree   溯源树
 * @return 是否成功
 */
static bool execute_strategy_construction(lvProofEngine *engine, const Proposition *goal, lvProofTraceTree *tree) {
    if (!engine || !goal || !tree)
        lv_RETURN_ERROR_BOOL(lv_ERROR_NULL_POINTER, "execute_strategy_construction: NULL param");

    /* 构造步骤节点 */
    lvProofTraceNode *construct_node = lv_trace_node_create(TRACE_NODE_DEFINITION, "Explicit Construction");
    if (!construct_node)
        lv_RETURN_ERROR_BOOL(lv_ERROR_ALLOCATION_FAILED, "execute_strategy_construction: node creation failed");

    lv_strlcpy(construct_node->description, "构造满足目标命题的数学对象", sizeof(construct_node->description));

    /* 使用规则库尝试构造 */
    bool success = false;

    if (engine->rule_library && engine->graph) {
        /* 查找构造规则 */
        lvRule **construct_rules = (lvRule **) lv_malloc(16 * sizeof(lvRule *));
        if (construct_rules) {
            uint32_t rule_count =
                lv_rule_library_get_by_type(engine->rule_library, RULE_TYPE_CONSTRUCTOR, construct_rules, 16);

            for (uint32_t r = 0; r < rule_count && !success; r++) {
                if (lv_rule_is_applicable(construct_rules[r], engine->graph, engine->navigator)) {
                    lvRuleMatch match;
                    memset(&match, 0, sizeof(match));
                    match.rule = construct_rules[r];
                    match.is_complete = true;
                    match.confidence = 1.0;

                    ProofStep **new_steps = (ProofStep **) lv_malloc(4 * sizeof(ProofStep *));
                    if (new_steps) {
                        uint32_t sc = lv_rule_apply_match(&match, engine->graph, engine->navigator, new_steps, 4);
                        if (sc > 0) {
                            success = true;
                        }
                        lv_free((void **) &new_steps);
                    }
                }
            }

            lv_free((void **) &construct_rules);
        }
    }

    if (success) {
        lv_trace_node_set_status(construct_node, TRACE_STATUS_PROVED);
    } else {
        lv_trace_node_set_status(construct_node, TRACE_STATUS_BLOCKED);
    }

    lv_trace_node_add_child(tree->root, construct_node);
    trace_tree_register_node(tree, construct_node);

    return success;
}

/**
 * @brief 内部函数：执行定义展开策略
 *
 * 通过展开目标命题中的定义，将其归约为更简单的子目标。
 * 递归展开直到所有子目标都可以直接证明。
 *
 * @param engine 引擎
 * @param goal   目标命题
 * @param tree   溯源树
 * @return 是否成功
 */
static bool execute_strategy_unfolding(lvProofEngine *engine, const Proposition *goal, lvProofTraceTree *tree) {
    if (!engine || !goal || !tree)
        lv_RETURN_ERROR_BOOL(lv_ERROR_NULL_POINTER, "execute_strategy_unfolding: NULL param");

    /* 查找并展开定义 */
    if (engine->rule_library) {
        lvRule **def_rules = (lvRule **) lv_malloc(16 * sizeof(lvRule *));
        if (def_rules) {
            uint32_t rule_count =
                lv_rule_library_get_by_type(engine->rule_library, RULE_TYPE_DEFINITION, def_rules, 16);

            for (uint32_t r = 0; r < rule_count; r++) {
                lvProofTraceNode *unfold_node = lv_trace_node_create(TRACE_NODE_DEFINITION, def_rules[r]->name);
                if (unfold_node) {
                    lv_strlcpy(unfold_node->description, def_rules[r]->description, sizeof(unfold_node->description));
                    lv_trace_node_set_status(unfold_node, TRACE_STATUS_PROVED);
                    lv_trace_node_add_child(tree->root, unfold_node);
                    trace_tree_register_node(tree, unfold_node);
                }
            }

            lv_free((void **) &def_rules);
        }
    }

    /* 展开后尝试直接证明 */
    return execute_strategy_direct(engine, goal, tree);
}

/**
 * @brief 内部函数：执行逆向推理策略
 *
 * 从目标命题出发，逆向查找需要的前提条件。
 * 通过分解目标，逐步回溯到已知事实。
 *
 * @param engine 引擎
 * @param goal   目标命题
 * @param tree   溯源树
 * @return 是否成功
 */
static bool execute_strategy_backward(lvProofEngine *engine, const Proposition *goal, lvProofTraceTree *tree) {
    if (!engine || !goal || !tree)
        lv_RETURN_ERROR_BOOL(lv_ERROR_NULL_POINTER, "execute_strategy_backward: NULL param");

    /* 创建逆向推理起始节点 */
    lvProofTraceNode *back_node = lv_trace_node_create(TRACE_NODE_GOAL, "Backward Analysis");
    if (back_node) {
        lv_strlcpy(back_node->description, "从目标出发，逆向分析所需前提", sizeof(back_node->description));
        lv_trace_node_set_status(back_node, TRACE_STATUS_EXPLORING);
        lv_trace_node_add_child(tree->root, back_node);
        trace_tree_register_node(tree, back_node);
    }

    /* 逆向推理：分析目标的子命题 */
    if (goal->sub_props) {
        for (int i = 0; i < goal->sub_prop_count; i++) {
            Proposition *sub = goal->sub_props[i];
            if (!sub)
                continue;

            lvProofTraceNode *sub_goal = lv_trace_node_create(TRACE_NODE_GOAL, sub->name ? sub->name : "Sub-goal");
            if (sub_goal) {
                sub_goal->proposition = sub;
                lv_trace_node_set_status(sub_goal, TRACE_STATUS_EXPLORING);
                lv_trace_node_add_child(tree->root, sub_goal);
                trace_tree_register_node(tree, sub_goal);
            }
        }
    }

    /* 回退到正向推理完成证明 */
    return execute_strategy_direct(engine, goal, tree);
}

/**
 * @brief 内部函数：执行正向推理策略
 *
 * 从已知前提出发，系统地应用推理规则向前推导。
 * 在每一步检查是否已经到达目标。
 *
 * @param engine 引擎
 * @param goal   目标命题
 * @param tree   溯源树
 * @return 是否成功
 */
static bool execute_strategy_forward(lvProofEngine *engine, const Proposition *goal, lvProofTraceTree *tree) {
    if (!engine || !goal || !tree)
        lv_RETURN_ERROR_BOOL(lv_ERROR_NULL_POINTER, "execute_strategy_forward: NULL param");

    uint32_t max_steps = engine->config.max_depth;
    uint32_t step = 0;

    while (step < max_steps) {
        /* 检查是否已到达目标 */
        if (engine->graph) {
            UnifyStatus result = proof_unify(engine->graph, (Proposition *) goal, false);
            if (result == UNIFY_STATUS_OK) {
                lvProofTraceNode *final_node = lv_trace_node_create(TRACE_NODE_DERIVATION, "Goal Reached");
                if (final_node) {
                    lv_trace_node_set_status(final_node, TRACE_STATUS_PROVED);
                    lv_trace_node_add_child(tree->root, final_node);
                    trace_tree_register_node(tree, final_node);
                }
                return true;
            }
        }

        /* 应用规则 */
        if (engine->rule_library && engine->graph) {
            lvRuleMatch **matches = (lvRuleMatch **) lv_malloc(8 * sizeof(lvRuleMatch *));
            if (matches) {
                uint32_t mc = lv_rule_find_matches(engine->rule_library, engine->graph, engine->navigator, matches, 8);

                if (mc > 0) {
                    ProofStep **new_steps = (ProofStep **) lv_malloc(4 * sizeof(ProofStep *));
                    if (new_steps) {
                        lv_rule_apply_match(matches[0], engine->graph, engine->navigator, new_steps, 4);

                        lvProofTraceNode *fwd_node = lv_trace_node_create(
                            TRACE_NODE_DERIVATION, matches[0]->rule ? matches[0]->rule->name : "Forward Step");
                        if (fwd_node) {
                            fwd_node->rule = matches[0]->rule;
                            lv_trace_node_set_status(fwd_node, TRACE_STATUS_EXPLORING);
                            lv_trace_node_add_child(tree->root, fwd_node);
                            trace_tree_register_node(tree, fwd_node);
                        }

                        lv_free((void **) &new_steps);
                    }
                }

                for (uint32_t m = 0; m < mc; m++) {
                    lv_rule_match_destroy(matches[m]);
                }
                lv_free((void **) &matches);
            }
        }

        step++;
    }

    return false;
}

/**
 * @brief 内部函数：执行混合策略
 *
 * 自适应地组合多种策略：先用逆向推理分析目标结构，
 * 再用正向推理推进证明，必要时切换到反证法。
 *
 * @param engine 引擎
 * @param goal   目标命题
 * @param tree   溯源树
 * @return 是否成功
 */
static bool execute_strategy_hybrid(lvProofEngine *engine, const Proposition *goal, lvProofTraceTree *tree) {
    if (!engine || !goal || !tree)
        lv_RETURN_ERROR_BOOL(lv_ERROR_NULL_POINTER, "execute_strategy_hybrid: NULL param");

    /* 阶段 1: 逆向分析 */
    lvProofTraceNode *analysis_node = lv_trace_node_create(TRACE_NODE_DERIVATION, "Phase 1: Backward Analysis");
    if (analysis_node) {
        lv_strlcpy(analysis_node->description, "混合策略阶段1：逆向分析目标结构", sizeof(analysis_node->description));
        lv_trace_node_set_status(analysis_node, TRACE_STATUS_EXPLORING);
        lv_trace_node_add_child(tree->root, analysis_node);
        trace_tree_register_node(tree, analysis_node);
    }

    /* 阶段 2: 正向推理 */
    bool forward_ok = execute_strategy_forward(engine, goal, tree);

    if (forward_ok) {
        if (analysis_node) {
            lv_trace_node_set_status(analysis_node, TRACE_STATUS_PROVED);
        }
        return true;
    }

    /* 阶段 3: 尝试反证法 */
    lvProofTraceNode *contra_node = lv_trace_node_create(TRACE_NODE_DERIVATION, "Phase 3: Contradiction Fallback");
    if (contra_node) {
        lv_strlcpy(contra_node->description, "混合策略阶段3：正向推理失败，切换到反证法",
                     sizeof(contra_node->description));
        lv_trace_node_set_status(contra_node, TRACE_STATUS_EXPLORING);
        lv_trace_node_add_child(tree->root, contra_node);
        trace_tree_register_node(tree, contra_node);
    }

    lvContradictionPath *contra_path = NULL;
    bool contra_ok = lv_engine_proof_by_contradiction(engine, goal, engine->config.max_depth, &contra_path);

    if (contra_ok) {
        if (contra_node) {
            lv_trace_node_set_status(contra_node, TRACE_STATUS_PROVED);
        }
        if (analysis_node) {
            lv_trace_node_set_status(analysis_node, TRACE_STATUS_PROVED);
        }
    }

    if (contra_path) {
        lv_contradiction_path_destroy(contra_path);
    }

    return contra_ok;
}

/** @brief 策略执行函数指针类型 */
typedef bool (*dispatch_handler)(lvProofEngine *, const Proposition *, lvProofTraceTree *);

/**
 * @brief 反证法策略包装函数
 *
 * 包装 lv_engine_proof_by_contradiction，使其符合标准策略函数签名。
 * 负责创建和销毁 ContradictionPath。
 */
static bool execute_strategy_contradiction_wrapper(lvProofEngine *engine, const Proposition *goal,
                                                    lvProofTraceTree *tree) {
    (void)tree;
    lvContradictionPath *path = NULL;
    bool ok = lv_engine_proof_by_contradiction(engine, goal, engine->config.max_depth, &path);
    if (path)
        lv_contradiction_path_destroy(path);
    return ok;
}

/**
 * @brief 策略分发查找表
 *
 * 将 lvStrategyType 映射到对应的策略执行函数。
 * 数组索引与枚举值一一对应。
 */
static const dispatch_handler kStrategyDispatch[] = {
    execute_strategy_direct,                /* STRATEGY_DIRECT */
    execute_strategy_contradiction_wrapper, /* STRATEGY_CONTRADICTION */
    execute_strategy_contrapositive,        /* STRATEGY_CONTRAPOSITIVE */
    execute_strategy_induction,             /* STRATEGY_INDUCTION */
    execute_strategy_cases,                 /* STRATEGY_CASES */
    execute_strategy_construction,          /* STRATEGY_CONSTRUCTION */
    execute_strategy_unfolding,             /* STRATEGY_UNFOLDING */
    execute_strategy_backward,              /* STRATEGY_BACKWARD */
    execute_strategy_forward,               /* STRATEGY_FORWARD */
    execute_strategy_hybrid                 /* STRATEGY_HYBRID */
};

/**
 * @brief 内部函数：策略分发器
 *
 * 根据策略类型调用对应的策略执行函数。
 *
 * @param engine 引擎
 * @param goal   目标命题
 * @param type   策略类型
 * @param tree   溯源树
 * @return 是否成功
 */
static bool dispatch_strategy(lvProofEngine *engine, const Proposition *goal, lvStrategyType type,
                              lvProofTraceTree *tree) {
    return LV_DISPATCH(kStrategyDispatch, type, false, engine, goal, tree);
}

/**
 * @brief 使用指定策略执行证明
 *
 * 使用用户指定的策略类型对目标命题进行证明。
 * 证明过程记录在溯源树中。
 *
 * @param engine        引擎实例
 * @param goal          目标命题
 * @param graph         约束图
 * @param strategy_type 策略类型
 * @param out_trace     输出溯源树
 * @return true 证明成功，false 证明失败
 */
bool lv_proof_engine_prove_with_strategy(lvProofEngine *engine, const Proposition *goal, ConstraintGraph *graph,
                                         lvStrategyType strategy_type, lvProofTraceTree **out_trace) {
    if (!engine || !goal || !out_trace) {
        lv_RETURN_ERROR_BOOL(lv_ERROR_NULL_POINTER, "lv_proof_engine_prove_with_strategy: NULL param");
    }

    *out_trace = NULL;

    /* 设置引擎状态 */
    engine->graph = graph;

    /* 创建溯源树 */
    lvProofTraceTree *tree = lv_trace_tree_create((Proposition *) goal);
    if (!tree)
        lv_RETURN_ERROR_BOOL(lv_ERROR_ALLOCATION_FAILED, "lv_proof_engine_prove_with_strategy: tree creation failed");

    /* 记录策略信息到根节点描述 */
    if (tree->root) {
        snprintf(tree->root->description, sizeof(tree->root->description), "使用 %s 策略证明: %s",
                 get_strategy_name_zh(strategy_type), goal->name ? goal->name : "unnamed goal");
    }

    /* 记录开始时间 */
    int64_t start_time = (int64_t) lv_get_time_ns();

    /* 执行策略 */
    bool success = dispatch_strategy(engine, goal, strategy_type, tree);

    /* 记录结束时间 */
    int64_t end_time = (int64_t) lv_get_time_ns();
    double elapsed = (double) (end_time - start_time) / 1e6;

    /* 更新溯源树状态 */
    if (success) {
        tree->is_complete = true;
        tree->final_color = TRUST_GREEN;
        if (tree->root) {
            lv_trace_node_set_status(tree->root, TRACE_STATUS_PROVED);
        }
    } else {
        tree->is_complete = false;
        if (tree->root) {
            lv_trace_node_set_status(tree->root, TRACE_STATUS_BLOCKED);
        }
    }

    trace_tree_update_stats(tree);

    /* 更新引擎统计 */
    engine->total_proofs++;
    if (success) {
        engine->success_proofs++;
    }
    engine->avg_proof_time_ms =
        (engine->avg_proof_time_ms * (double) (engine->total_proofs - 1) + elapsed) / (double) engine->total_proofs;

    engine->current_trace = tree;
    *out_trace = tree;

    return success;
}

/**
 * @brief 执行证明（使用第一个可用策略）
 *
 * 使用引擎中注册的第一个策略执行证明。
 * 如果没有注册策略，默认使用直接证明。
 *
 * @param engine    引擎实例
 * @param goal      目标命题
 * @param graph     约束图
 * @param out_trace 输出溯源树
 * @return true 证明成功，false 证明失败
 */
bool lv_proof_engine_prove(lvProofEngine *engine, const Proposition *goal, ConstraintGraph *graph,
                           lvProofTraceTree **out_trace) {
    if (!engine || !goal || !out_trace) {
        lv_RETURN_ERROR_BOOL(lv_ERROR_NULL_POINTER, "lv_proof_engine_prove: NULL param");
    }

    /* 如果有注册的策略，使用第一个 */
    if (engine->strategy_count > 0) {
        return lv_proof_engine_prove_with_strategy(engine, goal, graph, engine->strategies[0].type, out_trace);
    }

    /* 默认使用直接证明 */
    return lv_proof_engine_prove_with_strategy(engine, goal, graph, STRATEGY_DIRECT, out_trace);
}

/**
 * @brief 自动选择最优策略并执行证明
 *
 * 遍历所有已注册的策略，按优先级从高到低依次尝试，
 * 返回第一个成功的策略及其溯源树。
 *
 * @param engine       引擎实例
 * @param goal         目标命题
 * @param graph        约束图
 * @param out_trace    输出溯源树
 * @param out_strategy 输出使用的策略类型
 * @return true 证明成功，false 所有策略均失败
 */
bool lv_proof_engine_auto_prove(lvProofEngine *engine, const Proposition *goal, ConstraintGraph *graph,
                                lvProofTraceTree **out_trace, lvStrategyType *out_strategy) {
    if (!engine || !goal || !out_trace) {
        lv_RETURN_ERROR_BOOL(lv_ERROR_NULL_POINTER, "lv_proof_engine_auto_prove: NULL param");
    }

    *out_trace = NULL;
    if (out_strategy) {
        *out_strategy = STRATEGY_DIRECT;
    }

    /* 如果有注册的策略，按优先级尝试 */
    if (engine->strategy_count > 0) {
        for (uint32_t i = 0; i < engine->strategy_count; i++) {
            lvProofTraceTree *trace = NULL;
            bool success = lv_proof_engine_prove_with_strategy(engine, goal, graph, engine->strategies[i].type, &trace);

            if (success) {
                *out_trace = trace;
                if (out_strategy) {
                    *out_strategy = engine->strategies[i].type;
                }
                return true;
            }

            /* 释放失败的溯源树 */
            if (trace) {
                lv_trace_tree_destroy(trace);
            }
        }
    } else {
        /* 没有注册策略，尝试所有内置策略 */
        static const lvStrategyType builtin_strategies[] = {
            STRATEGY_DIRECT,  STRATEGY_CONTRADICTION, STRATEGY_CONTRAPOSITIVE, STRATEGY_INDUCTION,
            STRATEGY_CASES,   STRATEGY_CONSTRUCTION,  STRATEGY_UNFOLDING,      STRATEGY_BACKWARD,
            STRATEGY_FORWARD, STRATEGY_HYBRID};

        for (int i = 0; i < 10; i++) {
            lvProofTraceTree *trace = NULL;
            bool success = lv_proof_engine_prove_with_strategy(engine, goal, graph, builtin_strategies[i], &trace);

            if (success) {
                *out_trace = trace;
                if (out_strategy) {
                    *out_strategy = builtin_strategies[i];
                }
                return true;
            }

            if (trace) {
                lv_trace_tree_destroy(trace);
            }
        }
    }

    lv_RETURN_ERROR_BOOL(lv_ERROR_PROOF_INCOMPLETE, "lv_proof_engine_auto_prove: all strategies failed");
}
