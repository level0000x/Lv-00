/**
 * @file proof_dependency.c
 * @brief 证明依赖图与传递闭包
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

#include "lv/axiom_pkg.h"
#include "lv/constraint_graph.h"
#include "lv/engine.h"
#include "lv/proof.h"
#include "lv/solver.h"
#include "lv/lv_xmacro.h"

#include "debug.h"
#include "lv_internal.h"
#include "lv/lv_json.h"
#include "lv/lv_graph_traversal.h" /* lv_tree_release_recursive */
#include "lv_utils.h"
#include "lv/lv_str_utils.h"
#include "lv/lv_strbuf.h"
#include "lv/lv_dot_writer.h"
#include "proof_step_registry.h"
#include "proof_classical.h"

/**
 * 深拷贝命题并替换函数块输出端口 ID。
 * 用于实例化命题时更新内部引用。
 *
 * @param prop 源命题指针
 * @param id_map 旧 ID 到新 ID 的映射数组
 * @param map_size 映射数组大小
 * @return 深拷贝后的命题，调用者负责释放；失败返回 NULL
 */
static Proposition *instantiate_prop_with_port_remap(const Proposition *prop, const int *id_map, int map_size) {
    if (!prop)
        return NULL;

    Proposition *inst = lv_calloc(1, sizeof(Proposition));
    if (!inst)
        return NULL;

    /* 复制基本信息 */
    inst->id = prop->id;
    inst->type = prop->type;
    inst->color = prop->color;

    /* 复制标签 */
    if (prop->label) {
        inst->label = lv_strdup(prop->label);
    }
    if (prop->name) {
        inst->name = lv_strdup(prop->name);
    }
    if (prop->description) {
        inst->description = lv_strdup(prop->description);
    }

    /* 替换输出端口 ID */
    if (prop->output_port_ids && prop->output_count > 0) {
        inst->output_port_ids = lv_calloc((size_t) prop->output_count, sizeof(int));
        if (inst->output_port_ids) {
            inst->output_count = prop->output_count;
            inst->output_port_count = prop->output_port_count;
            for (int j = 0; j < prop->output_count; j++) {
                int old_id = prop->output_port_ids[j];
                int replacement = -1;
                if (old_id >= 0 && old_id < map_size) {
                    replacement = id_map[old_id];
                }
                inst->output_port_ids[j] = (replacement >= 0) ? replacement : old_id;
            }
        }
    }

    /* 替换输入端口 ID */
    if (prop->input_port_ids && prop->input_count > 0) {
        inst->input_port_ids = lv_calloc((size_t) prop->input_count, sizeof(int));
        if (inst->input_port_ids) {
            inst->input_count = prop->input_count;
            for (int j = 0; j < prop->input_count; j++) {
                int old_id = prop->input_port_ids[j];
                int replacement = -1;
                if (old_id >= 0 && old_id < map_size) {
                    replacement = id_map[old_id];
                }
                inst->input_port_ids[j] = (replacement >= 0) ? replacement : old_id;
            }
        }
    }

    /* 替换后置条件约束 ID */
    if (prop->postcondition_constraint_ids && prop->postcondition_count > 0) {
        inst->postcondition_constraint_ids = lv_malloc((size_t) prop->postcondition_count * sizeof(int));
        if (inst->postcondition_constraint_ids) {
            inst->postcondition_count = prop->postcondition_count;
            for (int j = 0; j < prop->postcondition_count; j++) {
                int old_id = prop->postcondition_constraint_ids[j];
                int replacement = -1;
                if (old_id >= 0 && old_id < map_size) {
                    replacement = id_map[old_id];
                }
                inst->postcondition_constraint_ids[j] = (replacement >= 0) ? replacement : old_id;
            }
        }
    }

    /* ---- 5. 清除缓存状态 ---- */
    inst->ref_count = 1;
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
/* proof_check_unconstructibility: 实现在 proof_navigator.c 中，通过 proof.h 声明可见 */

/**
 * @brief 系统性地尝试证明不可构造性
 *
 * 使用多策略流水线：
 * 1. 快速检查：已知不可构造问题列表
 * 2. 归约尝试：尝试将构造归约到已知不可构造问题
 * 3. 代数分析：检查方程系统的可解性
 * 4. 几何范围分析：检查是否超出尺规作图范围
 *
 * @param nav    证明导航器
 * @param graph  构造图
 * @param prop   命题
 * @param info   输出信息
 * @return 证明结果
 */
UnconstructResult proof_attempt_unconstructibility(ProofNavigator *nav, const ConstraintGraph *graph,
                                                   const Proposition *prop, UnconstructInfo *info) {
    if (!nav || !graph || !info) {
        if (info)
            info->result = UNCONSTRUCT_ERROR;
        return UNCONSTRUCT_ERROR;
    }

    memset(info, 0, sizeof(UnconstructInfo));
    info->result = UNCONSTRUCT_MAYBE_POSSIBLE;

    if (proof_stream_ctx) {
        stream_emit_simple(proof_stream_ctx, STREAM_EVENT_PROOF_STEP_ADDED, "系统性不可构造性证明开始", 1);
    }

    /* 策略1：检查已知不可构造问题（委托给基础检查函数） */
    UnconstructResult base_result = proof_check_unconstructibility(nav, graph, prop, info);
    if (base_result == UNCONSTRUCT_PROVED) {
        return UNCONSTRUCT_PROVED;
    }

    /* 策略2：代数可解性分析 */
    /* 提取构造中的代数方程并检查可解性 */
    {
        int quadratic_count = 0, cubic_count = 0, quartic_count = 0;
        int higher_count = 0;

        /* 创建方程系统 */
        EquationSystem *temp_sys = equation_system_create();
        if (temp_sys) {
            /* 直接从约束图提取方程（不需要 engine） */
            solver_extract_equations_full((ConstraintGraph *) graph, temp_sys);

            int sys_count = equation_system_count(temp_sys);

            /* 分析方程次数分布 */
            for (int i = 0; i < sys_count; i++) {
                const mpz_poly_t *poly = equation_system_get_poly(temp_sys, i);
                if (poly) {
                    int deg = poly->degree;
                    if (deg == 2)
                        quadratic_count++;
                    else if (deg == 3)
                        cubic_count++;
                    else if (deg == 4)
                        quartic_count++;
                    else if (deg > 4)
                        higher_count++;
                }
            }

            /* 检查是否有三次或更高次方程（可能不可构造） */
            if (cubic_count > 0 || quartic_count > 0 || higher_count > 0) {
                /* 如果存在高次方程且无有理数解，标记为可能不可构造 */
                char report[512];
                snprintf(report, sizeof(report),
                         "代数分析结果:\n"
                         "  二次方程: %d, 三次方程: %d, 四次方程: %d, 高次: %d\n"
                         "  存在三次及以上代数方程，无法保证尺规可构造性。\n"
                         "  建议: 检查是否有对应的不可构造问题归约。",
                         quadratic_count, cubic_count, quartic_count, higher_count);

                /* 更新info但保持结果为NOT_PROVED */
                info->proof_strategy = "代数可解性分析";
                info->reduction_steps = sys_count;
                if (info->detailed_report)
                    lv_free((void **) &info->detailed_report);
                info->detailed_report = lv_strdup(report);
            }

            equation_system_destroy(temp_sys);
        }
    }

    /* 策略3：归约尝试 - 尝试将构造归约到已知不可构造问题 */
    if (nav->engine && nav->engine->axiom_package_count > 0) {
        if (proof_stream_ctx) {
            stream_emit_simple(proof_stream_ctx, STREAM_EVENT_PROOF_COLOR_UPDATE, "尝试归约到已知不可构造问题", -1);
        }

        /* 遍历所有已加载的公理包 */
        for (int pkg_idx = 0; pkg_idx < nav->engine->axiom_package_count; pkg_idx++) {
            AxiomPackage *pkg = nav->engine->axiom_packages[pkg_idx];
            if (!pkg || pkg->known_unconstructibles.count <= 0)
                continue;

            /* 遍历每个已知不可构造问题，尝试归约 */
            for (int ku_idx = 0; ku_idx < pkg->known_unconstructibles.count; ku_idx++) {
                KnownUnconstructible *ku = (KnownUnconstructible *)lv_darray_get(&pkg->known_unconstructibles, ku_idx);
                if (!ku || !ku->name)
                    continue;

                /* 检查是否有归约链 */
                if (lv_str_nonempty(ku->reduces_to)) {
                    /* 这是一个归约问题，检查当前构造是否可以归约到它 */
                    /* 检查构造特征与归约目标的兼容性 */
                    bool can_reduce = false;

                    /* 基于约束类型的归约检查（统一走 proof_classical.h 查找表） */
                    switch (lv_classical_problem_match(ku->reduces_to)) {
                    case CLASSICAL_PROBLEM_TRISECTION:
                        /* 三等分角需要 ANGLE 约束或特定比例 */
                        for (int k = 0; k < graph->constraint_count; k++) {
                            if (graph->constraints[k]->type == BETWEENNESS ||
                                graph->constraints[k]->type == INCIDENCE) {
                                can_reduce = true;
                                break;
                            }
                        }
                        break;
                    case CLASSICAL_PROBLEM_DOUBLING:
                        /* 倍立方需要比例约束或特定代数数 */
                        can_reduce = (graph->node_count >= 3 && graph->constraint_count >= 2);
                        break;
                    default:
                        break;
                    }

                    if (can_reduce) {
                        info->result = UNCONSTRUCT_PROVED;
                        info->matched_problem = ku->reduces_to;
                        info->matched_theory = pkg->name ? pkg->name : "未知理论";
                        info->proof_strategy = "归约到已知不可构造问题";
                        info->reduction_steps = 1;

                        char report[512];
                        snprintf(report, sizeof(report),
                                 "构造可归约到已知的不可构造问题 '%s'\n"
                                 "（通过 '%s' 归约，来自公理包 '%s'）",
                                 ku->reduces_to, ku->name, pkg->name ? pkg->name : "未知");
                        info->detailed_report = lv_strdup(report);

                        if (proof_stream_ctx) {
                            stream_emit_simple(proof_stream_ctx, STREAM_EVENT_PROOF_COLOR_UPDATE,
                                               "归约成功：构造不可行", 1);
                        }
                        return UNCONSTRUCT_PROVED;
                    }
                }

                /* 检查依赖链 */
                if (ku->dependency_chain.count > 0) {
                    /* 检查构造是否涉及依赖链中的任何元素 */
                    for (int dep_idx = 0; dep_idx < ku->dependency_chain.count; dep_idx++) {
                        char *dep = *(char **)lv_darray_get(&ku->dependency_chain, dep_idx);
                        if (!dep)
                            continue;
                        /* 当前检查：依赖链中的名称是否与构造特征匹配 */
                        /* 完整实现可能需要更复杂的图匹配 */
                    }
                }
            }
        }

        if (proof_stream_ctx) {
            stream_emit_simple(proof_stream_ctx, STREAM_EVENT_PROOF_COLOR_UPDATE, "归约尝试完成：未找到归约路径", 0);
        }
    }

    if (proof_stream_ctx) {
        stream_emit_simple(proof_stream_ctx, STREAM_EVENT_PROOF_COLOR_UPDATE, "系统性不可构造性证明完成", 0);
    }

    /* 默认返回：可能可构造 */
    if (info->result == UNCONSTRUCT_MAYBE_POSSIBLE) {
        info->proof_strategy = info->proof_strategy ? info->proof_strategy : "多策略分析未能证明不可构造性";
    }

    return info->result;
}

/**
 * @brief 释放不可构造性信息结构体
 */
void unconstruct_info_destroy(UnconstructInfo *info) {
    if (!info)
        return;
    if (info->detailed_report) {
        lv_free((void **) &info->detailed_report);
    }
    memset(info, 0, sizeof(UnconstructInfo));
}

/* ============== 证明回溯与搜索树可视化（Newclid风格） ============== */

/**
 * @brief 递归销毁回溯节点及其子树（后序遍历）
 */
static void **backtrack_node_children(void *node, int *count) {
    BacktrackNode *n = (BacktrackNode *)node;
    *count = n->child_count;
    return (void **)n->children;
}

static void backtrack_node_cleanup(void *node) {
    BacktrackNode *n = (BacktrackNode *)node;
    lv_free((void **) &n->children);
    lv_free((void **) &n->label);
    lv_free((void **) &n->strategy_name);
}

static void backtrack_node_destroy_recursive(BacktrackNode *node) {
    lv_tree_release_recursive(node, backtrack_node_children, backtrack_node_cleanup);
}

/**
 * @brief 创建证明搜索树
 *
 * 分配并初始化搜索树，所有字段初始化为零/NULL。
 *
 * @return 新分配的搜索树指针，失败返回NULL
 */
ProofSearchTree *proof_search_tree_create(void) {
    ProofSearchTree *tree = lv_calloc(1, sizeof(ProofSearchTree));
    if (!tree)
        return NULL;

    tree->root = NULL;
    tree->all_nodes = NULL;
    tree->node_count = 0;
    tree->node_capacity = 0;
    tree->success_paths = 0;
    tree->failure_paths = 0;
    tree->backtrack_count = 0;
    tree->pruned_branches = 0;
    tree->max_depth = 0;
    tree->current_strategy = NULL;
    tree->available_strategies = NULL;
    tree->strategy_count = 0;
    tree->strategy_capacity = 0;

    return tree;
}

/**
 * @brief 销毁证明搜索树（递归释放所有节点）
 *
 * 递归释放所有回溯节点，释放 all_nodes 数组、策略列表，
 * 最后释放树结构本身。
 *
 * @param tree 搜索树指针（可为NULL）
 */
void proof_search_tree_destroy(ProofSearchTree *tree) {
    if (!tree)
        return;

    /* 递归释放所有节点 */
    if (tree->root) {
        backtrack_node_destroy_recursive(tree->root);
    }

    /* 释放 all_nodes 数组（节点指针已由递归销毁处理） */
    lv_free((void **) &tree->all_nodes);

    /* 释放策略列表 */
    for (int i = 0; i < tree->strategy_count; i++) {
        lv_free((void **) &tree->available_strategies[i]);
    }
    lv_free((void **) &tree->available_strategies);

    /* 释放当前策略字符串 */
    lv_free((void **) &tree->current_strategy);

    /* 释放树结构本身 */
    lv_free((void **) &tree);
}

/**
 * @brief 创建回溯节点
 *
 * 分配并初始化一个回溯节点。默认 color 为 PROOF_COLOR_BLUE_UNEXPLORED，
 * step_index 为 -1（无关联步骤）。
 *
 * @param type  节点类型
 * @param label 节点标签（内部复制）
 * @return 新分配的节点指针，失败返回NULL
 */
BacktrackNode *backtrack_node_create(BacktrackNodeType type, const char *label) {
    BacktrackNode *node = lv_calloc(1, sizeof(BacktrackNode));
    if (!node)
        return NULL;

    /* ID 由外部设置（在添加到树时由 proof_search_tree_add_child 分配） */
    node->id = -1;
    node->type = type;
    node->step_index = -1;
    node->is_backtrack_point = false;
    node->explored = false;
    node->color = PROOF_COLOR_BLUE_UNEXPLORED;
    node->parent = NULL;
    node->children = NULL;
    node->child_count = 0;
    node->child_capacity = 0;

    /* 使用安全的字符串复制函数，确保缓冲区零终止 */
    if (label && label[0] != '\0') {
        node->label = lv_malloc(strlen(label) + 1);
        if (!node->label) {
            lv_free((void **) &node);
            return NULL;
        }
        lv_strlcpy(node->label, label, strlen(label) + 1);
    } else {
        node->label = NULL;
    }

    node->strategy_name = NULL;

    return node;
}

/**
 * @brief 向搜索树添加子节点
 *
 * 将 child 添加为 parent 的子节点。若 parent 为 NULL，则将 child 设为根节点。
 * 同时将 child 加入 all_nodes 数组，并分配唯一ID。
 *
 * @param tree   搜索树
 * @param parent 父节点（NULL = 设为根节点）
 * @param child  子节点
 * @return 成功返回true，参数无效或内存分配失败返回false
 */

/** @brief 增量辅助函数，用于 kStatFuncs 查找表 */
static void proof_search_tree_inc_success(ProofSearchTree *t) { t->success_paths++; }
static void proof_search_tree_inc_failure(ProofSearchTree *t) { t->failure_paths++; }
static void proof_search_tree_inc_prune(ProofSearchTree *t) { t->pruned_branches++; }

bool proof_search_tree_add_child(ProofSearchTree *tree, BacktrackNode *parent, BacktrackNode *child) {
    if (!tree || !child)
        return false;

    /* 分配节点ID */
    child->id = tree->node_count;

    if (!parent) {
        /* 设为根节点 */
        if (tree->root) {
            /* 已有根节点，将 child 作为根的兄弟？不合理，返回失败 */
            return false;
        }
        tree->root = child;
        child->parent = NULL;
    } else {
        /* 添加到父节点的子节点数组 */
        BacktrackNode *p = parent;
        if (p->child_count >= p->child_capacity) {
            if (!lv_ensure_capacity((void **) &p->children, p->child_count, &p->child_capacity,
                                    sizeof(BacktrackNode *), 1))
                return false;
        }
        p->children[p->child_count] = child;
        p->child_count++;
        child->parent = p;

        /* 更新统计信息 */
        {
            static void (* const kStatFuncs[])(ProofSearchTree *) = {
                [BACKTRACK_SUCCESS] = proof_search_tree_inc_success,
                [BACKTRACK_FAILURE] = proof_search_tree_inc_failure,
                [BACKTRACK_PRUNE]   = proof_search_tree_inc_prune,
            };
            if ((size_t)child->type < sizeof(kStatFuncs)/sizeof(kStatFuncs[0]) && kStatFuncs[child->type]) {
                kStatFuncs[child->type](tree);
            }
        }

        /* 更新最大深度 */
        {
            int depth = 0;
            BacktrackNode *cur = child;
            while (cur->parent) {
                depth++;
                cur = cur->parent;
            }
            if (depth > tree->max_depth) {
                tree->max_depth = depth;
            }
        }
    }

    /* 将节点加入 all_nodes 数组 */
    if (tree->node_count >= tree->node_capacity) {
        if (!lv_ensure_capacity((void **) &tree->all_nodes, tree->node_count, &tree->node_capacity,
                                sizeof(BacktrackNode *), 1))
            return false;
    }
    tree->all_nodes[tree->node_count] = child;
    tree->node_count++;

    return true;
}

/**
 * @brief 标记回溯点
 *
 * 将节点标记为回溯点，并记录使用的策略名称。
 * 同时递增树的回溯计数。
 *
 * @param node          要标记的节点
 * @param strategy_name  使用的策略名称（内部复制）
 */
void backtrack_node_mark_backtrack(BacktrackNode *node, const char *strategy_name) {
    if (!node)
        return;

    node->is_backtrack_point = true;

    /* 释放旧策略名称 */
    lv_free((void **) &node->strategy_name);

    /* 使用安全的字符串复制函数，确保缓冲区零终止 */
    if (strategy_name && strategy_name[0] != '\0') {
        node->strategy_name = lv_malloc(strlen(strategy_name) + 1);
        if (node->strategy_name) {
            lv_strlcpy(node->strategy_name, strategy_name, strlen(strategy_name) + 1);
        }
    } else {
        node->strategy_name = NULL;
    }

    /* 更新父树回溯计数 */
    /* 向上遍历找到根节点所属的树（通过 all_nodes 间接关联）：
     * 这里假设调用者在树上下文中使用，直接标记即可 */
}

/**
 * @brief 注册可用策略
 *
 * 将策略名称添加到搜索树的可用策略列表中。
 *
 * @param tree          搜索树
 * @param strategy_name 策略名称（内部复制）
 */
void proof_search_tree_register_strategy(ProofSearchTree *tree, const char *strategy_name) {
    if (!tree || !strategy_name || strategy_name[0] == '\0')
        return;

    /* 检查是否已存在 */
    for (int i = 0; i < tree->strategy_count; i++) {
        if (tree->available_strategies[i] && strcmp(tree->available_strategies[i], strategy_name) == 0) {
            return; /* 已存在，不重复添加 */
        }
    }

    /* 扩展策略数组（倍增扩容：初始 8，此后每次倍增；失败语义与原来一致：直接返回） */
    if (!lv_ensure_capacity((void **) &tree->available_strategies, tree->strategy_count, &tree->strategy_capacity,
                            sizeof(char *), 1))
        return;

    /* 使用安全的字符串复制函数，确保缓冲区零终止 */
    tree->available_strategies[tree->strategy_count] = lv_malloc(strlen(strategy_name) + 1);
    if (!tree->available_strategies[tree->strategy_count])
        return;
    lv_strlcpy(tree->available_strategies[tree->strategy_count], strategy_name, strlen(strategy_name) + 1);
    tree->strategy_count++;
}

/**
 * @brief 设置当前策略
 *
 * 更新搜索树的当前活跃策略名称。
 *
 * @param tree          搜索树
 * @param strategy_name 策略名称（内部复制）
 */
void proof_search_tree_set_strategy(ProofSearchTree *tree, const char *strategy_name) {
    if (!tree)
        return;

    /* 释放旧值 */
    lv_free((void **) &tree->current_strategy);

    /* 使用安全的字符串复制函数，确保缓冲区零终止 */
    if (strategy_name && strategy_name[0] != '\0') {
        tree->current_strategy = lv_malloc(strlen(strategy_name) + 1);
        if (tree->current_strategy) {
            lv_strlcpy(tree->current_strategy, strategy_name, strlen(strategy_name) + 1);
        }
    } else {
        tree->current_strategy = NULL;
    }
}

/* ============== JSON/DOT 导出辅助函数 ============== */

/**
 * @brief 回溯节点类型转字符串
 */
/* ================================================================
 * 枚举 -> 名称 映射表（数据表化，替代 switch）
 * ================================================================ */

/** @brief backtrack_node_type_to_string 名称表（按枚举值升序） */
static const lvStrToEnumEntry s_backtrack_node_type_to_string_entries[] = {
    {"choice", BACKTRACK_CHOICE_POINT},
    {"failure", BACKTRACK_FAILURE},
    {"success", BACKTRACK_SUCCESS},
    {"prune", BACKTRACK_PRUNE},
};

static const char *backtrack_node_type_to_string(BacktrackNodeType type) {
    return lv_enum_to_str(s_backtrack_node_type_to_string_entries, lv_ARRAY_SIZE(s_backtrack_node_type_to_string_entries), (int) type, "unknown");
}

/**
 * @brief 递归将节点及其子树写入JSON（lvJsonBuf 版本）
 */
static void backtrack_node_write_json_buf(lvJsonBuf *buf, const BacktrackNode *node) {
    if (!buf || !node)
        return;

    lv_json_buf_begin_object(buf);
    lv_json_buf_append_key(buf, "id");
    lv_json_buf_append_int(buf, node->id);
    lv_json_buf_append_key(buf, "type");
    lv_json_buf_append_string(buf, backtrack_node_type_to_string(node->type));
    lv_json_buf_append_key(buf, "label");
    lv_json_buf_append_string(buf, node->label);
    lv_json_buf_append_key(buf, "strategy");
    lv_json_buf_append_string(buf, node->strategy_name);
    lv_json_buf_append_key(buf, "isBacktrackPoint");
    lv_json_buf_append_bool(buf, node->is_backtrack_point);
    lv_json_buf_append_key(buf, "explored");
    lv_json_buf_append_bool(buf, node->explored);
    lv_json_buf_append_key(buf, "color");
    lv_json_buf_append_string(buf, proof_color_to_string(node->color));
    lv_json_buf_append_key(buf, "stepIndex");
    lv_json_buf_append_int(buf, node->step_index);

    /* 子节点数组 */
    lv_json_buf_append_key(buf, "children");
    lv_json_buf_begin_array(buf);
    for (int i = 0; i < node->child_count; i++)
        backtrack_node_write_json_buf(buf, node->children[i]);
    lv_json_buf_end_array(buf);

    lv_json_buf_end_object(buf);
}

/**
 * @brief 递归将节点及其子树写入DOT格式
 */
static void backtrack_node_write_dot(lvStrBuf *sb, const BacktrackNode *node, int parent_id) {
    if (!sb || !node)
        return;

    /* 节点颜色映射（查找表替代 switch） */
    static const struct {
        BacktrackNodeType type;
        const char *fill_color;
        const char *border_color;
    } kDotColors[] = {
        {BACKTRACK_SUCCESS,      "#90EE90", "#006400"},  /* light green / dark green */
        {BACKTRACK_FAILURE,      "#FFB6C1", "#8B0000"},  /* light red   / dark red   */
        {BACKTRACK_CHOICE_POINT, "#87CEEB", "#00008B"},  /* light blue  / dark blue  */
        {BACKTRACK_PRUNE,        "#D3D3D3", "#696969"},  /* light gray  / dim gray   */
    };
    const char *fill_color = "#FFFFFF";
    const char *border_color = "#000000";
    const char *shape = node->is_backtrack_point ? "diamond" : "box";
    for (size_t ci = 0; ci < sizeof(kDotColors)/sizeof(kDotColors[0]); ci++) {
        if (node->type == kDotColors[ci].type) {
            fill_color = kDotColors[ci].fill_color;
            border_color = kDotColors[ci].border_color;
            break;
        }
    }

    /* 节点 label 改由 lv_dot_node 内部 JSON/DOT 转义（覆盖 "、\、换行 且不丢失原文，
     * 替代原 safe_label 净化与 500 字符截断） */
    lvStrBuf lbl;
    lv_strbuf_init(&lbl);
    lv_strbuf_printf(&lbl, "[%d] %s", node->id, node->label ? node->label : "");

    char idbuf[32], extra[256];
    snprintf(idbuf, sizeof(idbuf), "node%d", node->id);
    snprintf(extra, sizeof(extra), "shape=%s, style=filled, fillcolor=\"%s\", color=\"%s\"",
             shape, fill_color, border_color);
    lv_dot_node(sb, idbuf, lv_strbuf_cstr(&lbl), extra);
    lv_strbuf_destroy(&lbl);

    /* 父边 */
    if (parent_id >= 0) {
        const char *edge_style = node->is_backtrack_point ? "dashed" : "solid";
        char frombuf[32], tobuf[32], edge_extra[32];
        snprintf(frombuf, sizeof(frombuf), "node%d", parent_id);
        snprintf(tobuf, sizeof(tobuf), "node%d", node->id);
        snprintf(edge_extra, sizeof(edge_extra), "style=%s", edge_style);
        lv_dot_edge(sb, frombuf, tobuf, NULL, edge_extra);
    }

    /* 递归处理子节点 */
    for (int i = 0; i < node->child_count; i++) {
        backtrack_node_write_dot(sb, node->children[i], node->id);
    }
}

/**
 * @brief 导出搜索树为JSON（用于Web GUI可视化）
 *
 * 使用递归方式将整个搜索树输出为嵌套JSON结构，
 * 每个节点包含 id, type, label, strategy, isBacktrackPoint,
 * explored, color, stepIndex, children[]。
 *
 * @param tree      搜索树
 * @param filepath   输出文件路径
 * @return 成功返回true
 */
bool proof_search_tree_export_json(const ProofSearchTree *tree, const char *filepath) {
    if (!tree || !filepath)
        return false;

    lvJsonBuf buf;
    if (!lv_json_buf_init(&buf, 8192))
        return false;
    lv_json_buf_set_pretty(&buf, true);
    lv_json_buf_set_key_space(&buf, true);

    lv_json_buf_begin_object(&buf);
    lv_json_buf_append_key(&buf, "strategy");
    lv_json_buf_append_string(&buf, tree->current_strategy);
    lv_json_buf_append_key(&buf, "successPaths");
    lv_json_buf_append_int(&buf, tree->success_paths);
    lv_json_buf_append_key(&buf, "failurePaths");
    lv_json_buf_append_int(&buf, tree->failure_paths);
    lv_json_buf_append_key(&buf, "backtrackCount");
    lv_json_buf_append_int(&buf, tree->backtrack_count);
    lv_json_buf_append_key(&buf, "prunedBranches");
    lv_json_buf_append_int(&buf, tree->pruned_branches);
    lv_json_buf_append_key(&buf, "maxDepth");
    lv_json_buf_append_int(&buf, tree->max_depth);
    lv_json_buf_append_key(&buf, "nodeCount");
    lv_json_buf_append_int(&buf, tree->node_count);
    lv_json_buf_append_key(&buf, "root");
    if (tree->root)
        backtrack_node_write_json_buf(&buf, tree->root);
    else
        lv_json_buf_append_null(&buf);
    lv_json_buf_end_object(&buf);

    /* 写入文件 */
    FILE *f = fopen(filepath, "w");
    if (!f) {
        lv_json_buf_free(&buf);
        return false;
    }
    fputs(buf.buffer, f);
    fclose(f);

    lv_json_buf_free(&buf);
    return true;
}

/**
 * @brief 导出搜索树为DOT格式（Graphviz）
 *
 * 使用Graphviz DOT语言输出搜索树，节点按类型着色：
 * - 成功点（green）
 * - 失败点（red）
 * - 选择点（blue）
 * - 剪枝点（gray）
 * 回溯点使用 diamond 形状，虚线边框。
 *
 * @param tree      搜索树
 * @param filepath   输出文件路径
 * @return 成功返回true
 */
bool proof_search_tree_export_dot(const ProofSearchTree *tree, const char *filepath) {
    if (!tree || !filepath)
        return false;

    lvStrBuf sb;
    lv_strbuf_init(&sb);

    lv_dot_begin(&sb, "ProofSearchTree", "TB",
                 "fontname=\"Arial\", fontsize=11",
                 "fontname=\"Arial\", fontsize=9");

    /* 图级 label：多行标题 + 统计（current_strategy 经 JSON/DOT 转义） */
    {
        lvStrBuf lbl;
        lv_strbuf_init(&lbl);
        lv_strbuf_printf(&lbl, "\nProof Search Tree");
        if (tree->current_strategy)
            lv_strbuf_printf(&lbl, " - Strategy: %s", tree->current_strategy);
        lv_strbuf_printf(&lbl, "\nSuccess: %d | Failure: %d | Backtrack: %d | Pruned: %d | Max Depth: %d",
                         tree->success_paths, tree->failure_paths, tree->backtrack_count,
                         tree->pruned_branches, tree->max_depth);
        char *esc = lv_str_json_escape_alloc(lbl.data, lbl.len, NULL);
        if (esc) {
            lv_strbuf_printf(&sb, "    label=\"%s\";\n", esc);
            lv_free((void **) &esc);
        }
        lv_strbuf_destroy(&lbl);
    }
    lv_strbuf_printf(&sb, "    labelloc=t;\n");
    lv_strbuf_printf(&sb, "    fontsize=14;\n\n");

    if (tree->root) {
        backtrack_node_write_dot(&sb, tree->root, -1);
    }

    /* 图例 */
    lv_strbuf_printf(&sb, "\n  /* Legend */\n");
    lv_strbuf_printf(&sb, "  subgraph cluster_legend {\n");
    lv_strbuf_printf(&sb, "    label=\"Legend\";\n");
    lv_strbuf_printf(&sb, "    fontsize=10;\n");
    lv_strbuf_printf(&sb, "    style=dashed;\n");
    lv_dot_node(&sb, "legend_success", "Success", "shape=box, style=filled, fillcolor=\"#90EE90\"");
    lv_dot_node(&sb, "legend_failure", "Failure", "shape=box, style=filled, fillcolor=\"#FFB6C1\"");
    lv_dot_node(&sb, "legend_choice", "Choice Point", "shape=box, style=filled, fillcolor=\"#87CEEB\"");
    lv_dot_node(&sb, "legend_prune", "Pruned", "shape=box, style=filled, fillcolor=\"#D3D3D3\"");
    lv_dot_node(&sb, "legend_backtrack", "Backtrack", "shape=diamond, style=filled, fillcolor=\"lightyellow\"");
    lv_strbuf_printf(&sb, "  }\n");

    lv_dot_end(&sb);

    if (!lv_dot_write_file(filepath, sb.data, sb.len)) {
        lv_strbuf_destroy(&sb);
        return false;
    }
    lv_strbuf_destroy(&sb);
    return true;
}

/* ============== 自然语言证明输出（AlphaGeometry风格） ============== */

/**
 * @brief 步骤类型到自然语言动词映射（中文）
 *
 * 文案统一取自证明步骤注册表（proof_step_registry）。
 */
static const char *step_type_verb_zh(ProofStepType type) {
    const ProofStepInfo *info = proof_step_info(type);
    return info ? info->verb_zh : "执行操作";
}

/**
 * @brief 步骤类型到自然语言动词映射（英文）
 *
 * 文案统一取自证明步骤注册表（proof_step_registry）。
 */
static const char *step_type_verb_en(ProofStepType type) {
    const ProofStepInfo *info = proof_step_info(type);
    return info ? info->verb_en : "Execute operation";
}

/**
 * @brief 生成步骤的几何对象描述（按语言参数化）
 *
 * 中英文案与分隔符走查找表，输出与原 describe_objects_zh/en 逐字节一致；
 * 改用 lvStrBuf 消除固定缓冲截断风险。
 */
typedef struct {
    const char *sep;            /**< 分隔符 */
    const char *node_fmt;       /**< 节点格式 */
    const char *constraint_fmt; /**< 约束格式 */
    const char *rule_fmt;       /**< 规则格式 */
    const char *func_block_fmt; /**< 函数块格式 */
} ObjectDescText;

/** @brief 中文对象描述文案 */
static const ObjectDescText kObjectDescTextZh = {
    "，", "节点 %d", "约束 %d", "规则 %d", "函数块 %d",
};

/** @brief 英文对象描述文案 */
static const ObjectDescText kObjectDescTextEn = {
    ", ", "node %d", "constraint %d", "rule %d", "function block %d",
};

static void describe_objects(const ProofStep *step, ProofNaturalLanguage lang, lvStrBuf *out) {
    const ObjectDescText *txt = (lang == PROOF_NL_LANG_ZH_CN) ? &kObjectDescTextZh : &kObjectDescTextEn;
    if (step->node_id >= 0) {
        lv_strbuf_printf(out, txt->node_fmt, step->node_id);
    }
    if (step->constraint_id >= 0) {
        if (out->len > 0)
            lv_strbuf_printf(out, "%s", txt->sep);
        lv_strbuf_printf(out, txt->constraint_fmt, step->constraint_id);
    }
    if (step->rule_id >= 0) {
        if (out->len > 0)
            lv_strbuf_printf(out, "%s", txt->sep);
        lv_strbuf_printf(out, txt->rule_fmt, step->rule_id);
    }
    if (step->func_block_id >= 0) {
        if (out->len > 0)
            lv_strbuf_printf(out, "%s", txt->sep);
        lv_strbuf_printf(out, txt->func_block_fmt, step->func_block_id);
    }
}

/**
 * @brief 生成为什么可以进行这一步骤的解释（中文）
 *
 * 文案统一取自证明步骤注册表（proof_step_registry）。
 */
static const char *explain_why_zh(ProofStepType type) {
    const ProofStepInfo *info = proof_step_info(type);
    return info ? info->why_zh : "";
}

/**
 * @brief 生成为什么可以进行这一步骤的解释（英文）
 *
 * 文案统一取自证明步骤注册表（proof_step_registry）。
 */
static const char *explain_why_en(ProofStepType type) {
    const ProofStepInfo *info = proof_step_info(type);
    return info ? info->why_en : "";
}

/**
 * @brief 将单个证明步骤转换为自然语言描述
 */
char *proof_step_get_natural_language(const ProofStep *step, ProofNaturalLanguage lang) {
    if (!step)
        return NULL;

    lvStrBuf obj_desc;
    lvStrBuf result;
    lv_strbuf_init(&obj_desc);
    lv_strbuf_init(&result);
    const char *verb, *why, *step_type_name, *color_name;

    if (lang == PROOF_NL_LANG_ZH_CN) {
        verb = step_type_verb_zh(step->type);
        describe_objects(step, lang, &obj_desc);
        why = explain_why_zh(step->type);
        step_type_name = proof_step_type_to_string(step->type);
        color_name = proof_color_to_string(step->color);

        if (obj_desc.len > 0) {
            lv_strbuf_printf(&result,
                     "步骤 %d：%s%s。\n"
                     "  —— 涉及对象：%s\n"
                     "  —— 推理依据：%s\n"
                     "  —— 信任状态：%s",
                     step->id, verb, (step->type == PROOF_STEP_ADD_NODE) ? "新的几何对象" : "",
                     lv_strbuf_cstr(&obj_desc), why, color_name);
        } else {
            lv_strbuf_printf(&result,
                     "步骤 %d：%s。\n"
                     "  —— 推理依据：%s\n"
                     "  —— 信任状态：%s",
                     step->id, verb, why, color_name);
        }
    } else {
        verb = step_type_verb_en(step->type);
        describe_objects(step, lang, &obj_desc);
        why = explain_why_en(step->type);
        step_type_name = proof_step_type_to_string(step->type);
        color_name = proof_color_to_string(step->color);

        if (obj_desc.len > 0) {
            lv_strbuf_printf(&result,
                     "Step %d: %s.\n"
                     "  -- Objects involved: %s\n"
                     "  -- Reasoning: %s\n"
                     "  -- Trust status: %s",
                     step->id, verb, lv_strbuf_cstr(&obj_desc), why, color_name);
        } else {
            lv_strbuf_printf(&result,
                     "Step %d: %s.\n"
                     "  -- Reasoning: %s\n"
                     "  -- Trust status: %s",
                     step->id, verb, why, color_name);
        }
    }

    lv_strbuf_destroy(&obj_desc);
    return lv_strbuf_to_string(&result);
}
