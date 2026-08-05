/**
 * @file proof_contradiction.c
 * @brief 反证法证明实现（从 proof_engine_enhanced.c 拆分）
 *
 * @details 矛盾路径的创建/销毁/扩展、矛盾检测（几何/代数/逻辑）、
 *          矛盾路径验证与反证法证明入口。
 */

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

/* ============== 反证法路径操作 ============== */

/**
 * @brief 创建矛盾路径
 *
 * 分配并初始化一个空的矛盾路径结构。
 * 初始节点容量为 CONTRADICTION_PATH_INITIAL_CAPACITY。
 *
 * @return 新矛盾路径指针，失败返回 NULL
 */
lvContradictionPath *lv_contradiction_path_create(void) {
    lvContradictionPath *path = (lvContradictionPath *) lv_calloc(1, sizeof(lvContradictionPath));
    if (!path)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "lv_contradiction_path_create: calloc failed");

    lv_darray_init(&path->nodes, sizeof(lvContradictionPathNode));
    if (!lv_darray_reserve(&path->nodes, CONTRADICTION_PATH_INITIAL_CAPACITY)) {
        lv_free((void **) &path);
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "lv_contradiction_path_create: failed to reserve nodes");
    }

    path->type = CONTRADICTION_TYPE_P_AND_NOT_P;
    path->contradiction_desc[0] = '\0';
    path->trace_tree = NULL;
    path->is_valid = false;

    return path;
}

/**
 * @brief 销毁矛盾路径
 *
 * 释放矛盾路径中的节点数组和关联的溯源树。
 *
 * @param path 矛盾路径指针（可为 NULL，此时直接返回）
 */
void lv_contradiction_path_destroy(lvContradictionPath *path) {
    if (!path)
        return;

    lv_darray_free(&path->nodes);

    /* 溯源树由引擎管理，此处不销毁 */
    path->trace_tree = NULL;

    lv_free((void **) &path);
}

/**
 * @brief 添加节点到矛盾路径
 *
 * 在矛盾路径末尾追加一个新的推导节点。每个节点记录一条陈述和
 * 其证明理由，并标记是否为初始假设。
 *
 * @param path          矛盾路径
 * @param statement     陈述内容
 * @param justification 证明理由
 * @param is_assumption 是否为初始假设
 * @return 新节点的 ID（从 0 开始），失败返回 (uint32_t)-1
 */
uint32_t lv_contradiction_path_add_node(lvContradictionPath *path, const char *statement, const char *justification,
                                        bool is_assumption) {
    if (!path || !statement) {
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "lv_contradiction_path_add_node: path or statement is NULL");
    }

    /* 准备新节点 */
    lvContradictionPathNode node;
    memset(&node, 0, sizeof(node));
    node.id = (uint32_t)path->nodes.count;

    safe_strncpy(node.statement, statement, sizeof(node.statement));
    if (justification) {
        safe_strncpy(node.justification, justification, sizeof(node.justification));
    } else {
        node.justification[0] = '\0';
    }
    node.is_assumption = is_assumption;
    node.leads_to_contradiction = false;

    return (uint32_t)lv_darray_push(&path->nodes, &node);
}

/**
 * @brief 检测约束图和证明导航器中的矛盾
 *
 * 系统性检查 6 种矛盾类型：
 *   1. P AND NOT P：同一命题同时成立和不成立
 *   2. FALSE DERIVED：从前提推导出了假（矛盾命题）
 *   3. CYCLE：存在循环依赖
 *   4. TYPE MISMATCH：类型系统检测到不一致
 *   5. ARITHMETIC：算术矛盾（如 0=1）
 *   6. GEOMETRIC：几何矛盾（如点同时在线段的两侧）
 *
 * @param graph    约束图（可为 NULL）
 * @param nav      证明导航器（可为 NULL）
 * @param out_type 输出矛盾类型
 * @param out_desc 输出矛盾描述（缓冲区至少 512 字节）
 * @return true 检测到矛盾，false 未检测到矛盾或参数无效
 */
bool lv_detect_contradiction(const ConstraintGraph *graph, const ProofNavigator *nav, lvContradictionType *out_type,
                             char *out_desc) {
    if (!out_type || !out_desc) {
        lv_RETURN_ERROR_BOOL(lv_ERROR_NULL_POINTER, "lv_detect_contradiction: out_type or out_desc is NULL");
    }

    *out_type = CONTRADICTION_TYPE_P_AND_NOT_P;
    out_desc[0] = '\0';

    /* ---- 检测类型 1: P AND NOT P ---- */
    if (nav && nav->steps) {
        for (int i = 0; i < nav->step_count; i++) {
            ProofStep *step_i = nav->steps[i];
            if (!step_i || !step_i->is_completed)
                continue;

            /* 检查是否存在否定步骤与原步骤冲突 */
            for (int j = i + 1; j < nav->step_count; j++) {
                ProofStep *step_j = nav->steps[j];
                if (!step_j || !step_j->is_completed)
                    continue;

                /* 如果两个步骤的约束 ID 相同但类型冲突 */
                if (step_i->constraint_id > 0 && step_i->constraint_id == step_j->constraint_id &&
                    step_i->type == PROOF_STEP_ADD_CONSTRAINT && step_j->type == PROOF_STEP_EX_FALSO) {
                    *out_type = CONTRADICTION_TYPE_P_AND_NOT_P;
                    snprintf(out_desc, 512, "命题 P (步骤 %d) 与其否定 NOT P (步骤 %d) 同时成立", step_i->id,
                             step_j->id);
                    return true;
                }
            }
        }
    }

    /* ---- 检测类型 2: FALSE DERIVED ---- */
    if (nav && nav->steps) {
        for (int i = 0; i < nav->step_count; i++) {
            ProofStep *step = nav->steps[i];
            if (step && step->type == PROOF_STEP_EX_FALSO && step->is_completed) {
                *out_type = CONTRADICTION_TYPE_FALSE_DERIVED;
                snprintf(out_desc, 512, "从前提推导出矛盾 (步骤 %d, 爆炸原理)", step->id);
                return true;
            }
        }
    }

    /* ---- 检测类型 3: CYCLE ---- */
    if (nav && nav->steps) {
        for (int i = 0; i < nav->step_count; i++) {
            ProofStep *step = nav->steps[i];
            if (!step)
                continue;

            /* 检查循环依赖：步骤 i 依赖步骤 j，而步骤 j 又依赖步骤 i */
            for (int d = 0; d < step->dependency_count; d++) {
                int dep_id = step->dependency_step_ids[d];
                if (dep_id == step->id) {
                    *out_type = CONTRADICTION_TYPE_CYCLE;
                    snprintf(out_desc, 512, "步骤 %d 存在自依赖（循环依赖）", step->id);
                    return true;
                }

                /* 查找被依赖的步骤 */
                for (int j = 0; j < nav->step_count; j++) {
                    if (nav->steps[j] && nav->steps[j]->id == dep_id) {
                        for (int d2 = 0; d2 < nav->steps[j]->dependency_count; d2++) {
                            if (nav->steps[j]->dependency_step_ids[d2] == step->id) {
                                *out_type = CONTRADICTION_TYPE_CYCLE;
                                snprintf(out_desc, 512, "步骤 %d 和步骤 %d 之间存在循环依赖", step->id, dep_id);
                                return true;
                            }
                        }
                        break;
                    }
                }
            }
        }
    }

    /* ---- 检测类型 4: TYPE MISMATCH ---- */
    if (graph) {
        /* 检查约束图中是否存在类型不匹配的连接约束 */
        for (int c = 0; c < graph->constraint_count; c++) {
            Constraint *con = graph->constraints[c];
            if (!con || con->type != CONNECTION)
                continue;

            /* 检查连接的端口类型是否兼容 */
            for (int p = 0; p < con->participant_count - 1; p++) {
                int node_id_a = con->participants[p];
                int node_id_b = con->participants[p + 1];

                GeomNode *node_a = NULL;
                GeomNode *node_b = NULL;

                /* 通过哈希索引查找节点（走 graph_get_node，避免直索引越界） */
                node_a = graph_get_node(graph, node_id_a);
                node_b = graph_get_node(graph, node_id_b);

                if (node_a && node_b && node_a->type == GEOM_PORT && node_b->type == GEOM_PORT) {
                    Port *port_a = node_a->data.port;
                    Port *port_b = node_b->data.port;
                    if (port_a && port_b && port_a->type_region && port_b->type_region) {
                        /* 如果两个连接的端口类型不同且不是多态的 */
                        if (port_a->type == port_b->type && !port_a->is_polymorphic && !port_b->is_polymorphic) {
                            /* 检查类型区域是否兼容 */
                            TypeRegion *tr_a = port_a->type_region;
                            TypeRegion *tr_b = port_b->type_region;

                            /* 快速检查：如果类型种类不同，直接判定为矛盾 */
                            if (tr_a->kind != tr_b->kind) {
                                *out_type = CONTRADICTION_TYPE_TYPE_MISMATCH;
                                snprintf(out_desc, 512,
                                         "端口 %d 和端口 %d 类型不兼容："
                                         "类型种类不同（%d vs %d）",
                                         port_a->id, port_b->id, (int) tr_a->kind, (int) tr_b->kind);
                                return true;
                            }

                            /* 检查类型级别是否兼容 */
                            if (tr_a->level != tr_b->level) {
                                *out_type = CONTRADICTION_TYPE_TYPE_MISMATCH;
                                snprintf(out_desc, 512,
                                         "端口 %d 和端口 %d 类型不兼容："
                                         "类型级别不同（%d vs %d）",
                                         port_a->id, port_b->id, tr_a->level, tr_b->level);
                                return true;
                            }

                            /* 类型种类和级别相同，检查变量ID */
                            if (tr_a->variable_id != tr_b->variable_id && tr_a->variable_id > 0 &&
                                tr_b->variable_id > 0) {
                                /* 不同具体变量，记录矛盾 */
                                *out_type = CONTRADICTION_TYPE_TYPE_MISMATCH;
                                snprintf(out_desc, 512,
                                         "端口 %d 和端口 %d 类型不兼容："
                                         "类型变量不同（var_%d vs var_%d）",
                                         port_a->id, port_b->id, tr_a->variable_id, tr_b->variable_id);
                                return true;
                            }
                        }
                    }
                }
            }
        }
    }

    /* ---- 检测类型 5: ARITHMETIC ---- */
    if (nav && nav->steps) {
        for (int i = 0; i < nav->step_count; i++) {
            ProofStep *step = nav->steps[i];
            if (!step || step->type != PROOF_STEP_NORMALIZATION)
                continue;

            /* 规范化步骤可能发现算术矛盾 */
            if (step->retained_node_id < 0 && step->merged_count > 0) {
                *out_type = CONTRADICTION_TYPE_ARITHMETIC;
                snprintf(out_desc, 512, "规范化步骤 %d 发现算术矛盾：合并节点时产生不一致", step->id);
                return true;
            }
        }
    }

    /* ---- 检测类型 6: GEOMETRIC ---- */
    if (graph) {
        /* 检查几何约束冲突 */
        for (int c = 0; c < graph->constraint_count; c++) {
            Constraint *con = graph->constraints[c];
            if (!con)
                continue;

            /* 检查同一对节点之间是否存在冲突的约束 */
            for (int c2 = c + 1; c2 < graph->constraint_count; c2++) {
                Constraint *con2 = graph->constraints[c2];
                if (!con2)
                    continue;

                /* 如果两个约束涉及相同的节点但类型冲突 */
                if (con->participant_count == con2->participant_count && con->participant_count >= 2) {
                    bool same_participants = true;
                    for (int p = 0; p < con->participant_count; p++) {
                        if (con->participants[p] != con2->participants[p]) {
                            same_participants = false;
                            break;
                        }
                    }
                    if (same_participants && con->type != con2->type) {
                        *out_type = CONTRADICTION_TYPE_GEOMETRIC;
                        snprintf(out_desc, 512, "约束 %d (%d) 和约束 %d (%d) 在相同节点上冲突", con->id,
                                 (int) con->type, con2->id, (int) con2->type);
                        return true;
                    }
                }
            }
        }
    }

    return false;
}

/**
 * @brief 验证反证法证明路径的有效性
 *
 * 检查矛盾路径是否构成有效的反证法证明：
 *   1. 路径非空且至少包含一个假设节点
 *   2. 路径中存在导致矛盾的节点
 *   3. 假设节点在路径开头
 *   4. 矛盾描述非空
 *
 * @param path 矛盾路径
 * @return true 路径有效，false 路径无效或参数为 NULL
 */
bool lv_contradiction_path_validate(lvContradictionPath *path) {
    if (!path) {
        lv_RETURN_ERROR_BOOL(lv_ERROR_NULL_POINTER, "lv_contradiction_path_validate: path is NULL");
    }

    /* 路径必须非空 */
    if (path->nodes.count == 0) {
        lv_RETURN_ERROR_BOOL(lv_ERROR_PROOF_INCOMPLETE, "lv_contradiction_path_validate: path is empty");
    }

    /* 第一个节点必须是假设 */
    lvContradictionPathNode *first = (lvContradictionPathNode *)lv_darray_get(&path->nodes, 0);
    if (!first->is_assumption) {
        lv_RETURN_ERROR_BOOL(lv_ERROR_PROOF_INVALID, "lv_contradiction_path_validate: first node is not an assumption");
    }

    /* 必须存在导致矛盾的节点 */
    bool has_contradiction = false;
    for (int i = 0; i < path->nodes.count; i++) {
        lvContradictionPathNode *pn = (lvContradictionPathNode *)lv_darray_get(&path->nodes, i);
        if (pn->leads_to_contradiction) {
            has_contradiction = true;
            break;
        }
    }

    if (!has_contradiction) {
        lv_RETURN_ERROR_BOOL(lv_ERROR_PROOF_INCOMPLETE, "lv_contradiction_path_validate: no contradiction node found");
    }

    /* 矛盾描述不能为空 */
    if (path->contradiction_desc[0] == '\0') {
        lv_RETURN_ERROR_BOOL(lv_ERROR_PROOF_INCOMPLETE, "lv_contradiction_path_validate: contradiction description is empty");
    }

    path->is_valid = true;
    return true;
}

/* ============== 反证法证明 ============== */

/**
 * @brief 执行反证法证明
 *
 * 完整的反证法证明流程：
 *   1. 假设目标命题的否定成立
 *   2. 将否定假设加入证明环境
 *   3. 使用规则库进行正向推理
 *   4. 在每一步检测矛盾
 *   5. 如果发现矛盾，构建矛盾路径并返回
 *   6. 如果达到最大步数仍未发现矛盾，返回失败
 *
 * @param engine    证明引擎
 * @param goal      目标命题
 * @param max_steps 最大步骤数
 * @param out_path  输出矛盾路径（调用者负责释放）
 * @return true 反证法成功（发现矛盾），false 失败
 */
bool lv_engine_proof_by_contradiction(lvProofEngine *engine, const Proposition *goal, uint32_t max_steps,
                                      lvContradictionPath **out_path) {
    if (!engine || !goal || !out_path) {
        lv_RETURN_ERROR_BOOL(lv_ERROR_NULL_POINTER, "lv_engine_proof_by_contradiction: NULL param");
    }

    *out_path = NULL;

    /* 创建矛盾路径 */
    lvContradictionPath *path = lv_contradiction_path_create();
    if (!path)
        lv_RETURN_ERROR_BOOL(lv_ERROR_ALLOCATION_FAILED, "lv_engine_proof_by_contradiction: path creation failed");

    /* 创建溯源树 */
    lvProofTraceTree *tree = lv_trace_tree_create(NULL);
    if (!tree) {
        lv_contradiction_path_destroy(path);
        lv_RETURN_ERROR_BOOL(lv_ERROR_ALLOCATION_FAILED, "lv_engine_proof_by_contradiction: tree creation failed");
    }

    /* 步骤 1: 添加否定假设 */
    char assumption_stmt[512];
    if (goal->name) {
        snprintf(assumption_stmt, sizeof(assumption_stmt), "假设 NOT (%s) 成立", goal->name);
    } else {
        snprintf(assumption_stmt, sizeof(assumption_stmt), "假设目标命题的否定成立");
    }

    lv_contradiction_path_add_node(path, assumption_stmt, "反证法初始假设", true);

    /* 创建假设节点 */
    lvProofTraceNode *hyp_node = lv_trace_node_create(TRACE_NODE_HYPOTHESIS, "Negation Hypothesis");
    if (hyp_node) {
        lv_trace_node_add_child(tree->root, hyp_node);
        trace_tree_register_node(tree, hyp_node);
    }

    /* 步骤 2: 正向推理，搜索矛盾 */
    bool found_contradiction = false;
    uint32_t step = 0;

    while (step < max_steps && !found_contradiction) {
        /* 在约束图中检测矛盾 */
        lvContradictionType ctype;
        char cdesc[512];

        if (lv_detect_contradiction(engine->graph, engine->navigator, &ctype, cdesc)) {
            /* 发现矛盾 */
            found_contradiction = true;
            path->type = ctype;
            safe_strncpy(path->contradiction_desc, cdesc, sizeof(path->contradiction_desc));

            /* 标记最后一个节点为矛盾节点 */
            if (path->nodes.count > 0) {
                lvContradictionPathNode *last = (lvContradictionPathNode *)lv_darray_get(&path->nodes, path->nodes.count - 1);
                last->leads_to_contradiction = true;
            }

            /* 添加矛盾节点到溯源树 */
            lvProofTraceNode *contra_node = lv_trace_node_create(TRACE_NODE_CONTRADICTION, "Contradiction Found");
            if (contra_node) {
                safe_strncpy(contra_node->description, cdesc, sizeof(contra_node->description));
                lv_trace_node_set_status(contra_node, TRACE_STATUS_PROVED);
                if (hyp_node) {
                    lv_trace_node_add_child(hyp_node, contra_node);
                } else {
                    lv_trace_node_add_child(tree->root, contra_node);
                }
                trace_tree_register_node(tree, contra_node);
            }

            break;
        }

        /* 尝试应用规则进行推理 */
        if (engine->rule_library && engine->graph) {
            lvRuleMatch **matches = (lvRuleMatch **) lv_malloc(16 * sizeof(lvRuleMatch *));
            if (matches) {
                uint32_t match_count =
                    lv_rule_find_matches(engine->rule_library, engine->graph, engine->navigator, matches, 16);

                if (match_count > 0) {
                    /* 应用第一个匹配的规则 */
                    ProofStep **new_steps = (ProofStep **) lv_malloc(8 * sizeof(ProofStep *));
                    if (new_steps) {
                        uint32_t step_count =
                            lv_rule_apply_match(matches[0], engine->graph, engine->navigator, new_steps, 8);

                        if (step_count > 0) {
                            /* 记录推导步骤 */
                            char step_stmt[512];
                            snprintf(step_stmt, sizeof(step_stmt), "应用规则 '%s' 进行推导",
                                     matches[0]->rule ? matches[0]->rule->name : "unknown");
                            lv_contradiction_path_add_node(
                                path, step_stmt, matches[0]->rule ? matches[0]->rule->name : "rule application", false);

                            /* 创建推导节点 */
                            lvProofTraceNode *deriv_node = lv_trace_node_create(
                                TRACE_NODE_DERIVATION, matches[0]->rule ? matches[0]->rule->name : "Derivation");
                            if (deriv_node) {
                                deriv_node->rule = matches[0]->rule;
                                lv_trace_node_set_status(deriv_node, TRACE_STATUS_EXPLORING);
                                if (hyp_node) {
                                    lv_trace_node_add_child(hyp_node, deriv_node);
                                } else {
                                    lv_trace_node_add_child(tree->root, deriv_node);
                                }
                                trace_tree_register_node(tree, deriv_node);
                            }
                        }

                        lv_free((void **) &new_steps);
                    }
                }

                /* 释放匹配结果 */
                for (uint32_t m = 0; m < match_count; m++) {
                    lv_rule_match_destroy(matches[m]);
                }
                lv_free((void **) &matches);
            }
        }

        step++;
    }

    /* 设置溯源树状态 */
    if (found_contradiction) {
        tree->is_complete = true;
        tree->final_color = TRUST_GREEN;
        if (tree->root) {
            lv_trace_node_set_status(tree->root, TRACE_STATUS_PROVED);
        }
    } else {
        tree->is_complete = false;
        tree->final_color = TRUST_BLUE_UNEXPLORED;
        if (tree->root) {
            lv_trace_node_set_status(tree->root, TRACE_STATUS_BLOCKED);
        }
    }

    trace_tree_update_stats(tree);
    path->trace_tree = tree;

    /* 验证路径 */
    if (found_contradiction) {
        lv_contradiction_path_validate(path);
    }

    *out_path = path;
    return found_contradiction;
}
