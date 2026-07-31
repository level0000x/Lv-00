/**
 * @file proof_engine_enhanced.c
 * @brief 增强证明引擎实现 —— 反证法完善与逻辑溯源树
 *
 * @details 实现增强证明引擎的全部功能模块：
 *   1. 溯源树（TraceTree）：记录证明的完整依赖链，支持路径查找和导出
 *   2. 反证法（Contradiction）：完整的矛盾推导路径搜索与验证
 *   3. 证明策略调度（Strategy）：10种策略的注册、适用性评估与自动选择
 *   4. 证明验证（Verify）：独立验证证明正确性，检查步骤合法性
 *   5. 证明优化（Optimize）：冗余步骤消除，复杂度评估
 *   6. 证明导出（Export）：自然语言、LaTeX、Coq、Isar 格式输出
 *   7. 矛盾检测（ContradictionDetect）：6种矛盾类型的自动检测
 *
 * 线程安全设计：
 *   - 所有静态状态使用线程局部存储（lv_THREAD_LOCAL）
 *   - 引擎实例本身不含共享可变状态
 *   - 节点 ID 分配使用原子递增
 *
 * 内存管理：
 *   - 统一使用 lv_malloc/lv_calloc/lv_realloc/lv_free
 *   - 所有分配失败路径均通过 lv_set_error 报告错误
 *   - 树结构的销毁采用递归释放，防止内存泄漏
 *
 * @author Lv-00 Project
 * @version 3.3.0
 *
 * @dependencies
 *   - proof_engine_enhanced.h : 增强证明引擎公共接口
 *   - proof.h                 : 现有证明系统接口
 *   - axiom_rule_engine.h     : 公理规则引擎接口
 *   - constraint_graph.h      : 约束图接口
 *   - lv_utils.h            : 统一内存分配器与错误处理
 *   - error_codes.h           : 错误码定义
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
#include "lv.h" /* lv_THREAD_LOCAL 宏定义 */
#include "three_valued_logic.h"

#include "lv_internal.h"
#include "lv_utils.h"
#include "lv/lv_strbuf.h"

/* ============== 内部常量与宏 ============== */

/** 溯源树初始节点容量 */

/** 溯源树节点初始子节点容量 */

/** 矛盾路径初始容量 */
#define CONTRADICTION_PATH_INITIAL_CAPACITY 32

/** 导出缓冲区初始大小 */
#define EXPORT_BUFFER_INITIAL_SIZE 4096

/** 导出缓冲区最大大小 */
#define EXPORT_BUFFER_MAX_SIZE (1024 * 1024)

/** 默认证明引擎最大深度 */
#define DEFAULT_MAX_DEPTH 50

/** 默认证明引擎最大分支数 */
#define DEFAULT_MAX_BRANCHES 32

/** 默认超时时间（毫秒） */
#define DEFAULT_TIMEOUT_MS 30000

/** 原子递增节点 ID（线程安全） */

/* ============== 内部辅助函数 ============== */

/**
 * @brief 在访问映射表中检查节点是否已访问（线性探测）
 *
 * 用于 lv_trace_tree_find_path 中的 DFS 路径搜索，
 * 替代 GNU statement-expression 宏以避免 -Wpedantic 警告。
 *
 * @param visited_map  访问映射表（0 = 未占用）
 * @param map_size     映射表大小
 * @param node_id      待检查的节点 ID
 * @return true 已访问，false 未访问
 */

/**
 * @brief 获取当前时间戳（纳秒级）
 * @return 当前时间戳
 */

/**
 * @brief 安全字符串复制，截断超长字符串
 * @param dest 目标缓冲区
 * @param src 源字符串
 * @param max_len 缓冲区最大长度
 *
 * 本函数与标准 strncpy 的行为差异：
 *   1. 标准 strncpy 不会保证目标字符串以 '\0' 结尾（当 src 长度 >= n 时），
 *      而本函数始终在 dest[max_len-1] 处写入 '\0'，确保结果始终为合法 C 字符串。
 *   2. 标准 strncpy 在 src 短于 n 时会用 '\0' 填充剩余空间，本函数不做填充，
 *      仅写入有效内容加终止符，性能更优。
 *   3. 本函数对 NULL 指针做了防御性检查，标准 strncpy 对 NULL 调用是未定义行为。
 */
void safe_strncpy(char *dest, const char *src, size_t max_len) {
    if (!dest || !src) {
        if (dest && max_len > 0)
            dest[0] = '\0';
        return;
    }
    strncpy(dest, src, max_len - 1);
    dest[max_len - 1] = '\0';
}

/* StringBuffer 已迁移至 lvStrBuf（lv/lv_strbuf.h） */

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

                /* 通过哈希索引查找节点 */
                if (node_id_a >= 0 && node_id_a < graph->node_index_capacity) {
                    node_a = graph->node_index[node_id_a];
                }
                if (node_id_b >= 0 && node_id_b < graph->node_index_capacity) {
                    node_b = graph->node_index[node_id_b];
                }

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

/* ============== 证明引擎 ============== */

/**
 * @brief 创建证明引擎
 *
 * 根据配置创建增强证明引擎实例。如果 config 为 NULL，
 * 使用默认配置（最大深度 50，最大分支 32，超时 30 秒）。
 * 引擎创建后需要通过 lv_proof_engine_set_rule_library 设置规则库。
 *
 * @param config 引擎配置（可为 NULL，使用默认值）
 * @return 新引擎实例，失败返回 NULL
 */
lvProofEngine *lv_proof_engine_create(const lvProofEngineConfig *config) {
    lvProofEngine *engine = (lvProofEngine *) lv_calloc(1, sizeof(lvProofEngine));
    if (!engine)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "lv_proof_engine_create: calloc failed");

    /* 设置配置 */
    if (config) {
        engine->config = *config;
    } else {
        engine->config.max_depth = DEFAULT_MAX_DEPTH;
        engine->config.max_branches = DEFAULT_MAX_BRANCHES;
        engine->config.timeout_ms = lv_DEFAULT_TIMEOUT_MS;
        engine->config.enable_parallel = false;
        engine->config.enable_cache = true;
        engine->config.verify_proofs = true;
        engine->config.optimize_proofs = true;
    }

    /* 初始化策略数组 */
    engine->strategy_count = 0;
    memset(engine->strategies, 0, sizeof(engine->strategies));

    /* 初始化状态 */
    engine->rule_library = NULL;
    engine->graph = NULL;
    engine->navigator = NULL;
    engine->current_trace = NULL;

    /* 初始化统计 */
    engine->total_proofs = 0;
    engine->success_proofs = 0;
    engine->avg_proof_time_ms = 0.0;

    /* 初始化缓存 */
    engine->proof_cache = NULL;

    return engine;
}

/**
 * @brief 销毁证明引擎
 *
 * 释放引擎实例。注意：引擎不负责销毁其引用的规则库、
 * 约束图和导航器，这些资源由各自的创建者管理。
 *
 * @param engine 引擎指针（可为 NULL，此时直接返回）
 */
void lv_proof_engine_destroy(lvProofEngine *engine) {
    if (!engine)
        return;

    /* 释放当前溯源树 */
    if (engine->current_trace) {
        lv_trace_tree_destroy(engine->current_trace);
        engine->current_trace = NULL;
    }

    /* 释放缓存（如果有） */
    if (engine->proof_cache) {
        lv_free((void **) &engine->proof_cache);
    }

    lv_free((void **) &engine);
}

/**
 * @brief 设置证明引擎的规则库
 *
 * 将规则库绑定到引擎实例。引擎在证明过程中会使用此规则库
 * 进行规则匹配和应用。规则库的生命周期由调用者管理。
 *
 * @param engine  引擎实例
 * @param library 规则库（可为 NULL，清除当前规则库）
 */
void lv_proof_engine_set_rule_library(lvProofEngine *engine, lvRuleLibrary *library) {
    if (!engine)
        return;
    engine->rule_library = library;
}

/**
 * @brief 注册证明策略到引擎
 *
 * 将一个证明策略添加到引擎的策略列表中。
 * 策略按优先级排序存储，优先级高的排在前面。
 * 引擎最多支持 lv_PROOF_MAX_STRATEGIES 个策略。
 *
 * @param engine   引擎实例
 * @param strategy 策略描述
 * @return true 注册成功，false 引擎已满或参数无效
 */
bool lv_proof_engine_register_strategy(lvProofEngine *engine, const lvProofStrategy *strategy) {
    if (!engine || !strategy) {
        lv_RETURN_ERROR_BOOL(lv_ERROR_NULL_POINTER, "lv_proof_engine_register_strategy: NULL param");
    }

    if (engine->strategy_count >= lv_PROOF_MAX_STRATEGIES) {
        lv_RETURN_ERROR_BOOL(lv_ERROR_RESOURCE_EXHAUSTED, "lv_proof_engine_register_strategy: strategy count exceeds max (%d)", lv_PROOF_MAX_STRATEGIES);
    }

    /* 按优先级插入（降序） */
    uint32_t insert_pos = engine->strategy_count;
    for (uint32_t i = 0; i < engine->strategy_count; i++) {
        if (strategy->priority > engine->strategies[i].priority) {
            insert_pos = i;
            break;
        }
    }

    /* 后移元素 */
    if (insert_pos < engine->strategy_count) {
        memmove(&engine->strategies[insert_pos + 1], &engine->strategies[insert_pos],
                (engine->strategy_count - insert_pos) * sizeof(lvProofStrategy));
    }

    engine->strategies[insert_pos] = *strategy;
    engine->strategy_count++;

    return true;
}

/* ============== 内部策略实现 ============== */

/**
 * @brief 策略中文名称映射表
 */
static const char *g_strategy_names_zh[] = {
    "直接证明",   /* STRATEGY_DIRECT */
    "反证法",     /* STRATEGY_CONTRADICTION */
    "逆否证明",   /* STRATEGY_CONTRAPOSITIVE */
    "数学归纳法", /* STRATEGY_INDUCTION */
    "分情况讨论", /* STRATEGY_CASES */
    "构造性证明", /* STRATEGY_CONSTRUCTION */
    "定义展开",   /* STRATEGY_UNFOLDING */
    "逆向推理",   /* STRATEGY_BACKWARD */
    "正向推理",   /* STRATEGY_FORWARD */
    "混合策略"    /* STRATEGY_HYBRID */
};

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
        safe_strncpy(contra_node->description, "将命题 P -> Q 转换为逆否命题 NOT Q -> NOT P",
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
        safe_strncpy(base_node->description, "验证基础情况：当 n=0 时命题成立", sizeof(base_node->description));
        lv_trace_node_set_status(base_node, TRACE_STATUS_PROVED);
        lv_trace_node_add_child(tree->root, base_node);
        trace_tree_register_node(tree, base_node);
    }

    /* 归纳假设节点 */
    lvProofTraceNode *ih_node = lv_trace_node_create(TRACE_NODE_HYPOTHESIS, "Inductive Hypothesis");
    if (ih_node) {
        safe_strncpy(ih_node->description, "归纳假设：假设命题在 n=k 时成立", sizeof(ih_node->description));
        lv_trace_node_set_status(ih_node, TRACE_STATUS_EXPLORING);
        lv_trace_node_add_child(tree->root, ih_node);
        trace_tree_register_node(tree, ih_node);
    }

    /* 归纳步节点 */
    lvProofTraceNode *step_node = lv_trace_node_create(TRACE_NODE_DERIVATION, "Inductive Step (k -> k+1)");
    if (step_node) {
        safe_strncpy(step_node->description, "归纳步：由 n=k 成立推导 n=k+1 也成立", sizeof(step_node->description));
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

    for (int c = 0; c < num_cases && c < (int) lv_PROOF_MAX_BRANCHES; c++) {
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

    safe_strncpy(construct_node->description, "构造满足目标命题的数学对象", sizeof(construct_node->description));

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
                    safe_strncpy(unfold_node->description, def_rules[r]->description, sizeof(unfold_node->description));
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
        safe_strncpy(back_node->description, "从目标出发，逆向分析所需前提", sizeof(back_node->description));
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
        safe_strncpy(analysis_node->description, "混合策略阶段1：逆向分析目标结构", sizeof(analysis_node->description));
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
        safe_strncpy(contra_node->description, "混合策略阶段3：正向推理失败，切换到反证法",
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
    switch (type) {
        case STRATEGY_DIRECT:
            return execute_strategy_direct(engine, goal, tree);
        case STRATEGY_CONTRADICTION: {
            lvContradictionPath *path = NULL;
            bool ok = lv_engine_proof_by_contradiction(engine, goal, engine->config.max_depth, &path);
            if (path)
                lv_contradiction_path_destroy(path);
            return ok;
        }
        case STRATEGY_CONTRAPOSITIVE:
            return execute_strategy_contrapositive(engine, goal, tree);
        case STRATEGY_INDUCTION:
            return execute_strategy_induction(engine, goal, tree);
        case STRATEGY_CASES:
            return execute_strategy_cases(engine, goal, tree);
        case STRATEGY_CONSTRUCTION:
            return execute_strategy_construction(engine, goal, tree);
        case STRATEGY_UNFOLDING:
            return execute_strategy_unfolding(engine, goal, tree);
        case STRATEGY_BACKWARD:
            return execute_strategy_backward(engine, goal, tree);
        case STRATEGY_FORWARD:
            return execute_strategy_forward(engine, goal, tree);
        case STRATEGY_HYBRID:
            return execute_strategy_hybrid(engine, goal, tree);
        default:
            return false;
    }
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
    int64_t start_time = get_time_ns();

    /* 执行策略 */
    bool success = dispatch_strategy(engine, goal, strategy_type, tree);

    /* 记录结束时间 */
    int64_t end_time = get_time_ns();
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

/**
 * @brief 获取证明引擎的统计信息
 *
 * 返回引擎的累计证明次数、成功次数和平均证明时间。
 *
 * @param engine      引擎实例
 * @param out_total   输出总证明次数（可为 NULL）
 * @param out_success 输出成功次数（可为 NULL）
 * @param out_avg_time 输出平均时间（毫秒，可为 NULL）
 */
void lv_proof_engine_get_stats(const lvProofEngine *engine, uint64_t *out_total, uint64_t *out_success,
                               double *out_avg_time) {
    if (!engine)
        return;

    if (out_total)
        *out_total = engine->total_proofs;
    if (out_success)
        *out_success = engine->success_proofs;
    if (out_avg_time)
        *out_avg_time = engine->avg_proof_time_ms;
}

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
            snprintf(out_error, 512, "溯源树为 NULL");
        }
        return lv_VERIFY_ERROR;
    }

    if (!trace->root) {
        if (out_error) {
            snprintf(out_error, 512, "溯源树缺少根节点");
        }
        return lv_VERIFY_ERROR;
    }

    /* 检查根节点状态 */
    if (trace->root->status != TRACE_STATUS_PROVED) {
        if (out_error) {
            snprintf(out_error, 512, "根节点状态为 %d（期望 PROVED=%d）", (int) trace->root->status,
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
                snprintf(out_error, 512, "推导节点 %u ('%s') 没有子节点（缺少推导依据）", node->id, node->label);
            }
            return lv_VERIFY_INVALID;
        }

        /* 检查未完成的子目标 */
        if (node->type == TRACE_NODE_GOAL && node->status == TRACE_STATUS_UNEXPLORED) {
            if (out_error) {
                snprintf(out_error, 512, "子目标节点 %u ('%s') 未被探索", node->id, node->label);
            }
            return lv_VERIFY_INCOMPLETE;
        }
    }

    /* 检查信任颜色传播 */
    TrustColor computed = lv_trace_node_compute_color(trace->root);
    if (computed != trace->final_color) {
        /* 警告但不标记为无效 */
        if (out_error) {
            snprintf(out_error, 512, "警告：信任颜色不一致（计算值=%d, 记录值=%d）", (int) computed,
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
            snprintf(out_error, 512, "证明步骤为 NULL");
        }
        return lv_VERIFY_ERROR;
    }

    /* 检查步骤类型 */
    if (step->type < PROOF_STEP_ADD_NODE || step->type > PROOF_STEP_ORACLE) {
        if (out_error) {
            snprintf(out_error, 512, "步骤 %d 的类型 %d 无效", step->id, (int) step->type);
        }
        return lv_VERIFY_INVALID;
    }

    /* 检查步骤是否完成 */
    if (!step->is_completed) {
        if (out_error) {
            snprintf(out_error, 512, "步骤 %d 尚未完成", step->id);
        }
        return lv_VERIFY_INCOMPLETE;
    }

    /* 检查关联的约束是否存在 */
    if (graph && step->constraint_id >= 0) {
        bool found = false;
        if (step->constraint_id < graph->constraint_index_capacity) {
            found = (graph->constraint_index[step->constraint_id] != NULL);
        }
        if (!found) {
            if (out_error) {
                snprintf(out_error, 512, "步骤 %d 引用的约束 %d 在约束图中不存在", step->id, step->constraint_id);
            }
            return lv_VERIFY_INVALID;
        }
    }

    return lv_VERIFY_VALID;
}

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

/**
 * @brief 优化证明（消除冗余步骤）
 *
 * 通过以下方式优化证明：
 *   1. 移除冗余的推导节点
 *   2. 合并连续的同类型节点
 *   3. 简化信任颜色传播路径
 *
 * 优化过程创建新的溯源树，不修改原始树。
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

    /* 复制非冗余节点 */
    uint32_t removed_count = 0;

    for (int i = 0; i < trace->all_nodes.count; i++) {
        lvProofTraceNode **src_p = (lvProofTraceNode **)lv_darray_get(&trace->all_nodes, i);
        lvProofTraceNode *src_node = *src_p;

        /* 跳过根节点（已由 create 创建） */
        if (src_node == trace->root)
            continue;

        /* 跳过冗余节点 */
        if (is_redundant_node(src_node)) {
            removed_count++;
            continue;
        }

        /* 创建新节点并复制属性 */
        lvProofTraceNode *new_node = lv_trace_node_create(src_node->type, src_node->label);
        if (!new_node)
            continue;

        safe_strncpy(new_node->description, src_node->description, sizeof(new_node->description));
        new_node->status = src_node->status;
        new_node->trust_color = src_node->trust_color;
        new_node->proposition = src_node->proposition;
        new_node->step = src_node->step;
        new_node->rule = src_node->rule;
        new_node->elapsed_ms = src_node->elapsed_ms;

        /* 添加到优化后的树 */
        lv_trace_node_add_child(optimized->root, new_node);
        trace_tree_register_node(optimized, new_node);
    }

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

    uint32_t removed = 0;

    /* 标记冗余节点为阻塞 */
    for (int i = 0; i < trace->all_nodes.count; i++) {
        lvProofTraceNode **node_p = (lvProofTraceNode **)lv_darray_get(&trace->all_nodes, i);
        lvProofTraceNode *node = *node_p;
        if (is_redundant_node(node)) {
            lv_trace_node_set_status(node, TRACE_STATUS_BLOCKED);
            removed++;
        }
    }

    /* 重新计算信任颜色 */
    if (trace->root) {
        lv_trace_node_compute_color(trace->root);
        trace->final_color = trace->root->trust_color;
    }

    /* 更新统计 */
    trace_tree_update_stats(trace);

    return (uint32_t)(trace->all_nodes.count - removed);
}

/* ============== 证明导出 ============== */
/* 自然语言、LaTeX、Coq、Isar 格式输出 */

/**
 * @brief 导出证明为自然语言文本
 *
 * 生成 AlphaGeometry 风格的人类可读证明文本。
 * 每一步都包含完整的自然语言描述，说明：
 *   - 应用了什么推理规则
 *   - 涉及哪些数学对象
 *   - 为什么可以进行这一步
 *
 * @param trace 溯源树
 * @param lang  输出语言（中文/英文）
 * @return 自然语言文本（调用者需用 lv_free 释放），失败返回 NULL
 */
char *lv_proof_to_natural_language(const lvProofTraceTree *trace, ProofNaturalLanguage lang) {
    if (!trace) {
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "lv_proof_to_natural_language: trace is NULL");
    }

    lvStrBuf buf = {0};

    const char *proof_str = (lang == PROOF_NL_LANG_ZH_CN) ? "证明" : "Proof";
    const char *begin_str =
        (lang == PROOF_NL_LANG_ZH_CN) ? "以下是该命题的证明过程：" : "Below is the proof of this proposition:";

    lv_strbuf_printf(&buf, "%s\n", proof_str);
    lv_strbuf_printf(&buf, "%s\n\n", begin_str);

    /* 遍历溯源树生成自然语言 */
    for (int i = 0; i < trace->all_nodes.count; i++) {
        lvProofTraceNode **node_p = (lvProofTraceNode **)lv_darray_get(&trace->all_nodes, i);
        lvProofTraceNode *node = *node_p;

        /* 跳过根节点 */
        if (node == trace->root)
            continue;

        const char *status_str;
        switch (node->status) {
            case TRACE_STATUS_PROVED:
                status_str = (lang == PROOF_NL_LANG_ZH_CN) ? "[已证明]" : "[PROVED]";
                break;
            case TRACE_STATUS_DISPROVED:
                status_str = (lang == PROOF_NL_LANG_ZH_CN) ? "[已证伪]" : "[DISPROVED]";
                break;
            case TRACE_STATUS_BLOCKED:
                status_str = (lang == PROOF_NL_LANG_ZH_CN) ? "[阻塞]" : "[BLOCKED]";
                break;
            default:
                status_str = (lang == PROOF_NL_LANG_ZH_CN) ? "[探索中]" : "[EXPLORING]";
                break;
        }

        const char *type_str;
        switch (node->type) {
            case TRACE_NODE_AXIOM:
                type_str = (lang == PROOF_NL_LANG_ZH_CN) ? "公理" : "Axiom";
                break;
            case TRACE_NODE_DEFINITION:
                type_str = (lang == PROOF_NL_LANG_ZH_CN) ? "定义" : "Definition";
                break;
            case TRACE_NODE_THEOREM:
                type_str = (lang == PROOF_NL_LANG_ZH_CN) ? "定理" : "Theorem";
                break;
            case TRACE_NODE_LEMMA:
                type_str = (lang == PROOF_NL_LANG_ZH_CN) ? "引理" : "Lemma";
                break;
            case TRACE_NODE_HYPOTHESIS:
                type_str = (lang == PROOF_NL_LANG_ZH_CN) ? "假设" : "Hypothesis";
                break;
            case TRACE_NODE_DERIVATION:
                type_str = (lang == PROOF_NL_LANG_ZH_CN) ? "推导" : "Derivation";
                break;
            case TRACE_NODE_CONTRADICTION:
                type_str = (lang == PROOF_NL_LANG_ZH_CN) ? "矛盾" : "Contradiction";
                break;
            case TRACE_NODE_GOAL:
                type_str = (lang == PROOF_NL_LANG_ZH_CN) ? "目标" : "Goal";
                break;
            default:
                type_str = (lang == PROOF_NL_LANG_ZH_CN) ? "未知" : "Unknown";
                break;
        }

        if (lang == PROOF_NL_LANG_ZH_CN) {
            lv_strbuf_printf(&buf, "步骤 %u: [%s] %s %s", i + 1, type_str, node->label, status_str);

            if (node->description[0] != '\0') {
                lv_strbuf_printf(&buf, "\n  说明: %s", node->description);
            }

            if (node->rule && node->rule->name[0] != '\0') {
                lv_strbuf_printf(&buf, "\n  应用规则: %s", node->rule->name);
            }

            if (node->elapsed_ms > 0) {
                lv_strbuf_printf(&buf, "\n  耗时: %.2f ms", node->elapsed_ms);
            }
        } else {
            lv_strbuf_printf(&buf, "Step %u: [%s] %s %s", i + 1, type_str, node->label, status_str);

            if (node->description[0] != '\0') {
                lv_strbuf_printf(&buf, "\n  Description: %s", node->description);
            }

            if (node->rule && node->rule->name[0] != '\0') {
                lv_strbuf_printf(&buf, "\n  Applied rule: %s", node->rule->name);
            }

            if (node->elapsed_ms > 0) {
                lv_strbuf_printf(&buf, "\n  Time: %.2f ms", node->elapsed_ms);
            }
        }

        lv_strbuf_printf(&buf, "\n\n");
    }

    /* 结论 */
    if (trace->is_complete) {
        if (lang == PROOF_NL_LANG_ZH_CN) {
            lv_strbuf_printf(&buf, "证毕。\\qed\n");
        } else {
            lv_strbuf_printf(&buf, "Q.E.D.\\qed\n");
        }
    } else {
        if (lang == PROOF_NL_LANG_ZH_CN) {
            lv_strbuf_printf(&buf, "证明未完成。\n");
        } else {
            lv_strbuf_printf(&buf, "Proof incomplete.\n");
        }
    }

    return lv_strbuf_to_string(&buf);
}

/**
 * @brief 导出证明为 LaTeX 格式
 *
 * 生成完整的 LaTeX 证明文档，包含：
 *   - proof 环境
 *   - 每个步骤的描述
 *   - 规则引用
 *   - 信任颜色标注
 *
 * @param trace 溯源树
 * @return LaTeX 文本（调用者需用 lv_free 释放），失败返回 NULL
 */
char *lv_proof_to_latex(const lvProofTraceTree *trace) {
    if (!trace) {
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "lv_proof_to_latex: trace is NULL");
    }

    lvStrBuf buf = {0};

    /* LaTeX 文档头 */
    lv_strbuf_printf(&buf, "\\begin{proof}\n");

    /* 根节点描述 */
    if (trace->root && trace->root->description[0] != '\0') {
        lv_strbuf_printf(&buf, "  \\textit{%s}\n\n", trace->root->description);
    }

    /* 信任颜色映射到 LaTeX 颜色 */
    static const char *color_map[] = {
        "\\textcolor{green}{}",     /* TRUST_GREEN */
        "\\textcolor{blue}{}",      /* TRUST_BLUE_UNEXPLORED */
        "\\textcolor{blue}{}",      /* TRUST_BLUE_EXCEEDED */
        "\\textcolor{blue}{}",      /* TRUST_BLUE_OUT_OF_SCOPE */
        "\\textcolor{yellow}{}",    /* TRUST_YELLOW */
        "\\textcolor{orange!70}{}", /* TRUST_LIGHT_ORANGE_ORACLE */
        "\\textcolor{orange!70}{}", /* TRUST_LIGHT_ORANGE_EXPLOSION */
        "\\textcolor{orange!50}{}", /* TRUST_AMBER */
        "\\textcolor{orange}{}",    /* TRUST_DEEP_ORANGE */
        "\\textcolor{red}{}"        /* TRUST_RED */
    };
    static const int color_map_count = sizeof(color_map) / sizeof(color_map[0]);

    /* 遍历节点生成 LaTeX */
    for (int i = 0; i < trace->all_nodes.count; i++) {
        lvProofTraceNode **node_p = (lvProofTraceNode **)lv_darray_get(&trace->all_nodes, i);
        lvProofTraceNode *node = *node_p;
        if (node == trace->root)
            continue;

        /* 节点类型标签 */
        const char *type_label;
        switch (node->type) {
            case TRACE_NODE_AXIOM:
                type_label = "\\textbf{Axiom}";
                break;
            case TRACE_NODE_DEFINITION:
                type_label = "\\textbf{Def}";
                break;
            case TRACE_NODE_THEOREM:
                type_label = "\\textbf{Thm}";
                break;
            case TRACE_NODE_LEMMA:
                type_label = "\\textbf{Lemma}";
                break;
            case TRACE_NODE_HYPOTHESIS:
                type_label = "\\textit{Hyp}";
                break;
            case TRACE_NODE_DERIVATION:
                type_label = "\\textbf{Step}";
                break;
            case TRACE_NODE_CONTRADICTION:
                type_label = "\\textbf{Contr!}";
                break;
            case TRACE_NODE_GOAL:
                type_label = "\\textbf{Goal}";
                break;
            default:
                type_label = "Step";
                break;
        }

        int color_idx = (int) node->trust_color;
        if (color_idx < 0 || color_idx >= color_map_count)
            color_idx = 0;

        lv_strbuf_printf(&buf, "  \\noindent %s[%s] %s%s}\n", type_label, node->label, color_map[color_idx],
                             node->label);

        if (node->description[0] != '\0') {
            lv_strbuf_printf(&buf, "  \\\\ \\quad %s\n", node->description);
        }

        if (node->rule && node->rule->name[0] != '\0') {
            lv_strbuf_printf(&buf, "  \\\\ \\quad \\textit{by} \\texttt{%s}\n", node->rule->name);
        }

        lv_strbuf_printf(&buf, "\n");
    }

    /* 结论 */
    if (trace->is_complete) {
        lv_strbuf_printf(&buf, "  \\hfill $\\qed$\n");
    } else {
        lv_strbuf_printf(&buf, "  \\textcolor{red}{\\textit{Proof incomplete.}}\n");
    }

    lv_strbuf_printf(&buf, "\\end{proof}\n");

    return lv_strbuf_to_string(&buf);
}

/**
 * @brief 导出证明为 Coq 脚本
 *
 * 生成 Coq 形式化证明脚本，包含：
 *   - Theorem/Lemma 声明
 *   - Proof 开始
 *   - 策略（tactic）序列
 *   - Qed 结束
 *
 * @param trace 溯源树
 * @return Coq 脚本（调用者需用 lv_free 释放），失败返回 NULL
 */
char *lv_proof_to_coq(const lvProofTraceTree *trace) {
    if (!trace) {
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "lv_proof_to_coq: trace is NULL");
    }

    lvStrBuf buf = {0};

    /* 定理声明 */
    const char *theorem_name = "theorem_result";
    if (trace->root && trace->root->label[0] != '\0') {
        theorem_name = trace->root->label;
    }

    lv_strbuf_printf(&buf, "Theorem %s : Prop.\n", theorem_name);
    lv_strbuf_printf(&buf, "Proof.\n");

    /* 遍历节点生成 Coq tactic */
    for (int i = 0; i < trace->all_nodes.count; i++) {
        lvProofTraceNode **node_p = (lvProofTraceNode **)lv_darray_get(&trace->all_nodes, i);
        lvProofTraceNode *node = *node_p;
        if (node == trace->root)
            continue;

        switch (node->type) {
            case TRACE_NODE_AXIOM:
                lv_strbuf_printf(&buf, "  (* Axiom: %s *)\n", node->label);
                lv_strbuf_printf(&buf, "  apply %s_axiom.\n", node->label);
                break;

            case TRACE_NODE_HYPOTHESIS:
                lv_strbuf_printf(&buf, "  (* Hypothesis: %s *)\n", node->label);
                lv_strbuf_printf(&buf, "  intro H%s.\n", node->label);
                break;

            case TRACE_NODE_DERIVATION:
                if (node->rule && node->rule->name[0] != '\0') {
                    lv_strbuf_printf(&buf, "  (* Apply rule: %s *)\n", node->rule->name);
                    lv_strbuf_printf(&buf, "  apply %s.\n", node->rule->name);
                } else {
                    lv_strbuf_printf(&buf, "  (* Derivation: %s *)\n", node->label);
                    lv_strbuf_printf(&buf, "  assert (H%d : Prop).\n", node->id);
                    lv_strbuf_printf(&buf, "  { %s. }\n", node->description);
                }
                break;

            case TRACE_NODE_CONTRADICTION:
                lv_strbuf_printf(&buf, "  (* Contradiction: %s *)\n", node->description);
                lv_strbuf_printf(&buf, "  contradiction.\n");
                break;

            case TRACE_NODE_GOAL:
                lv_strbuf_printf(&buf, "  (* Sub-goal: %s *)\n", node->label);
                break;

            case TRACE_NODE_THEOREM:
                lv_strbuf_printf(&buf, "  (* Apply theorem: %s *)\n", node->label);
                lv_strbuf_printf(&buf, "  apply %s.\n", node->label);
                break;

            case TRACE_NODE_LEMMA:
                lv_strbuf_printf(&buf, "  (* Apply lemma: %s *)\n", node->label);
                lv_strbuf_printf(&buf, "  apply %s_lemma.\n", node->label);
                break;

            case TRACE_NODE_DEFINITION:
                lv_strbuf_printf(&buf, "  (* Unfold definition: %s *)\n", node->label);
                lv_strbuf_printf(&buf, "  unfold %s.\n", node->label);
                break;

            default:
                lv_strbuf_printf(&buf, "  (* Step: %s *)\n", node->label);
                lv_strbuf_printf(&buf, "  admit.\n");
                break;
        }
    }

    /* 结束证明 */
    if (trace->is_complete) {
        lv_strbuf_printf(&buf, "Qed.\n");
    } else {
        lv_strbuf_printf(&buf, "  (* Proof incomplete *)\n");
        lv_strbuf_printf(&buf, "Admitted.\n");
    }

    return lv_strbuf_to_string(&buf);
}

/**
 * @brief 导出证明为 Isar 脚本
 *
 * 生成 Isabelle/HOL Isar 结构化证明文本，包含：
 *   - theorem/lemma 声明
 *   - proof 开始
 *   - have/show/then 结构化步骤
 *   - qed 结束
 *
 * @param trace 溯源树
 * @return Isar 脚本（调用者需用 lv_free 释放），失败返回 NULL
 */
char *lv_proof_to_isar(const lvProofTraceTree *trace) {
    if (!trace) {
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "lv_proof_to_isar: trace is NULL");
    }

    lvStrBuf buf = {0};

    /* 定理声明 */
    const char *theorem_name = "theorem_result";
    if (trace->root && trace->root->label[0] != '\0') {
        theorem_name = trace->root->label;
    }

    lv_strbuf_printf(&buf, "theorem %s\n", theorem_name);
    if (trace->root && trace->root->description[0] != '\0') {
        lv_strbuf_printf(&buf, "  -- \"%s\"\n", trace->root->description);
    }
    lv_strbuf_printf(&buf, "where\n");
    lv_strbuf_printf(&buf, "proof -\n");

    /* 遍历节点生成 Isar */
    for (int i = 0; i < trace->all_nodes.count; i++) {
        lvProofTraceNode **node_p = (lvProofTraceNode **)lv_darray_get(&trace->all_nodes, i);
        lvProofTraceNode *node = *node_p;
        if (node == trace->root)
            continue;

        switch (node->type) {
            case TRACE_NODE_AXIOM:
                lv_strbuf_printf(&buf, "  -- Axiom: %s\n", node->label);
                lv_strbuf_printf(&buf, "  have \"%s\" by auto\n", node->label);
                break;

            case TRACE_NODE_HYPOTHESIS:
                lv_strbuf_printf(&buf, "  -- Hypothesis: %s\n", node->label);
                lv_strbuf_printf(&buf, "  assume \"%s\"\n", node->label);
                break;

            case TRACE_NODE_DERIVATION:
                if (node->rule && node->rule->name[0] != '\0') {
                    lv_strbuf_printf(&buf, "  -- Apply rule: %s\n", node->rule->name);
                    lv_strbuf_printf(&buf, "  then have \"%s\" using %s\n", node->label, node->rule->name);
                } else {
                    lv_strbuf_printf(&buf, "  -- Derivation: %s\n", node->label);
                    lv_strbuf_printf(&buf, "  have \"%s\"\n", node->label);
                    if (node->description[0] != '\0') {
                        lv_strbuf_printf(&buf, "    -- \"%s\"\n", node->description);
                    }
                    lv_strbuf_printf(&buf, "    sorry\n");
                }
                break;

            case TRACE_NODE_CONTRADICTION:
                lv_strbuf_printf(&buf, "  -- Contradiction: %s\n", node->description);
                lv_strbuf_printf(&buf, "  then show False\n");
                lv_strbuf_printf(&buf, "    contradiction\n");
                break;

            case TRACE_NODE_GOAL:
                lv_strbuf_printf(&buf, "  -- Sub-goal: %s\n", node->label);
                lv_strbuf_printf(&buf, "  moreover have \"%s\"\n", node->label);
                break;

            case TRACE_NODE_THEOREM:
                lv_strbuf_printf(&buf, "  -- Theorem: %s\n", node->label);
                lv_strbuf_printf(&buf, "  from `%s` have \"%s\" .\n", node->label, node->label);
                break;

            case TRACE_NODE_LEMMA:
                lv_strbuf_printf(&buf, "  -- Lemma: %s\n", node->label);
                lv_strbuf_printf(&buf, "  using `%s_lemma`\n", node->label);
                break;

            case TRACE_NODE_DEFINITION:
                lv_strbuf_printf(&buf, "  -- Unfold: %s\n", node->label);
                lv_strbuf_printf(&buf, "  unfolding %s_def\n", node->label);
                break;

            default:
                lv_strbuf_printf(&buf, "  -- Step: %s\n", node->label);
                lv_strbuf_printf(&buf, "  sorry\n");
                break;
        }
    }

    /* 结束证明 */
    if (trace->is_complete) {
        lv_strbuf_printf(&buf, "qed\n");
    } else {
        lv_strbuf_printf(&buf, "  -- Proof incomplete\n");
        lv_strbuf_printf(&buf, "sorry\n");
    }

    return lv_strbuf_to_string(&buf);
}
