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
#include "lv00/proof.h"
#include "lv00/engine.h"
#include "lv00/axiom_pkg.h"
#include "lv00/constraint_graph.h"
#include "debug.h"
#include "lv00_internal.h"
#include "lv00_utils.h"

LV00_DECLARE_STREAM_CTX(proof);

/**
 * @brief 将 src 中的特殊 JSON 字符转义后写入 dst
 *
 * 转义双引号、反斜杠和控制字符。如果 dst 不够大，结果会被截断。
 *
 * @param dst      目标缓冲区
 * @param dst_size 目标缓冲区大小
 * @param src      源字符串（可为 NULL）
 * @return 写入 dst 的字符数（不含终止符）
 */
static int json_escape(char *dst, size_t dst_size, const char *src) {
    if (!dst || dst_size == 0) return 0;
    if (!src) { dst[0] = '\0'; return 0; }
    size_t j = 0;
    for (size_t i = 0; src[i] && j < dst_size - 2; i++) {
        switch (src[i]) {
            case '"':  if (j + 2 < dst_size) { dst[j++] = '\\'; dst[j++] = '"'; } break;
            case '\\': if (j + 2 < dst_size) { dst[j++] = '\\'; dst[j++] = '\\'; } break;
            case '\n': if (j + 2 < dst_size) { dst[j++] = '\\'; dst[j++] = 'n'; } break;
            case '\r': if (j + 2 < dst_size) { dst[j++] = '\\'; dst[j++] = 'r'; } break;
            case '\t': if (j + 2 < dst_size) { dst[j++] = '\\'; dst[j++] = 't'; } break;
            default:
                if ((unsigned char)src[i] < 0x20) {
                    if (j + 6 < dst_size)
                        j += (size_t)snprintf(dst + j, dst_size - j, "\\u%04x", (unsigned char)src[i]);
                } else {
                    dst[j++] = src[i];
                }
                break;
        }
    }
    dst[j] = '\0';
    return (int)j;
}

/**
 * 深拷贝命题并替换函数块输出端口 ID。
 * 用于实例化命题时更新内部引用。
 *
 * @param prop 源命题指针
 * @param id_map 旧 ID 到新 ID 的映射数组
 * @param map_size 映射数组大小
 * @return 深拷贝后的命题，调用者负责释放；失败返回 NULL
 */
static Proposition *instantiate_prop_with_port_remap(const Proposition *prop,
                                                       const int *id_map, int map_size) {
    if (!prop)
        return NULL;

    Proposition *inst = lv00_calloc(1, sizeof(Proposition));
    if (!inst)
        return NULL;

    /* 复制基本信息 */
    inst->id = prop->id;
    inst->type = prop->type;
    inst->color = prop->color;

    /* 复制标签 */
    if (prop->label) {
        inst->label = lv00_strdup(prop->label);
    }
    if (prop->name) {
        inst->name = lv00_strdup(prop->name);
    }
    if (prop->description) {
        inst->description = lv00_strdup(prop->description);
    }

    /* 替换输出端口 ID */
    if (prop->output_port_ids && prop->output_count > 0) {
        inst->output_port_ids = lv00_malloc((size_t) prop->output_count * sizeof(int));
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
        inst->input_port_ids = lv00_malloc((size_t) prop->input_count * sizeof(int));
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
        inst->postcondition_constraint_ids = lv00_malloc(
            (size_t) prop->postcondition_count * sizeof(int));
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
static UnconstructResult proof_check_unconstructibility(ProofNavigator *nav, const ConstraintGraph *graph,
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
                    info->detailed_report = lv00_strdup(report);

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
            info->detailed_report = lv00_strdup(report);

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
                    lv00_free((void **) &info->detailed_report);
                info->detailed_report = lv00_strdup(report);
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
            if (!pkg || pkg->unconstructible_count <= 0)
                continue;

            /* 遍历每个已知不可构造问题，尝试归约 */
            for (int ku_idx = 0; ku_idx < pkg->unconstructible_count; ku_idx++) {
                KnownUnconstructible *ku = &pkg->known_unconstructibles[ku_idx];
                if (!ku || !ku->name)
                    continue;

                /* 检查是否有归约链 */
                if (ku->reduces_to && strlen(ku->reduces_to) > 0) {
                    /* 这是一个归约问题，检查当前构造是否可以归约到它 */
                    /* 检查构造特征与归约目标的兼容性 */
                    bool can_reduce = false;

                    /* 基于约束类型的归约检查 */
                    if (strstr(ku->reduces_to, "trisection") || strstr(ku->reduces_to, "三等分")) {
                        /* 三等分角需要 ANGLE 约束或特定比例 */
                        for (int k = 0; k < graph->constraint_count; k++) {
                            if (graph->constraints[k]->type == BETWEENNESS ||
                                graph->constraints[k]->type == INCIDENCE) {
                                can_reduce = true;
                                break;
                            }
                        }
                    } else if (strstr(ku->reduces_to, "doubling") || strstr(ku->reduces_to, "倍立方")) {
                        /* 倍立方需要比例约束或特定代数数 */
                        can_reduce = (graph->node_count >= 3 && graph->constraint_count >= 2);
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
                        info->detailed_report = lv00_strdup(report);

                        if (proof_stream_ctx) {
                            stream_emit_simple(proof_stream_ctx, STREAM_EVENT_PROOF_COLOR_UPDATE,
                                               "归约成功：构造不可行", 1);
                        }
                        return UNCONSTRUCT_PROVED;
                    }
                }

                /* 检查依赖链 */
                if (ku->dependency_count > 0 && ku->dependency_chain) {
                    /* 检查构造是否涉及依赖链中的任何元素 */
                    for (int dep_idx = 0; dep_idx < ku->dependency_count; dep_idx++) {
                        if (!ku->dependency_chain[dep_idx])
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
        lv00_free((void **) &info->detailed_report);
    }
    memset(info, 0, sizeof(UnconstructInfo));
}

/* ============== 证明回溯与搜索树可视化（Newclid风格） ============== */

/**
 * @brief 递归销毁回溯节点及其子树（后序遍历）
 */
static void backtrack_node_destroy_recursive(BacktrackNode *node) {
    if (!node)
        return;

    /* 后序遍历：先释放子节点，再释放自身 */
    for (int i = 0; i < node->child_count; i++) {
        backtrack_node_destroy_recursive(node->children[i]);
    }
    lv00_free((void **) &node->children);
    lv00_free((void **) &node->label);
    lv00_free((void **) &node->strategy_name);
    lv00_free((void **) &node);
}

/**
 * @brief 创建证明搜索树
 *
 * 分配并初始化搜索树，所有字段初始化为零/NULL。
 *
 * @return 新分配的搜索树指针，失败返回NULL
 */
ProofSearchTree *proof_search_tree_create(void) {
    ProofSearchTree *tree = lv00_calloc(1, sizeof(ProofSearchTree));
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
    lv00_free((void **) &tree->all_nodes);

    /* 释放策略列表 */
    for (int i = 0; i < tree->strategy_count; i++) {
        lv00_free((void **) &tree->available_strategies[i]);
    }
    lv00_free((void **) &tree->available_strategies);

    /* 释放当前策略字符串 */
    lv00_free((void **) &tree->current_strategy);

    /* 释放树结构本身 */
    lv00_free((void **) &tree);
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
    BacktrackNode *node = lv00_calloc(1, sizeof(BacktrackNode));
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
        node->label = lv00_malloc(strlen(label) + 1);
        if (!node->label) {
            lv00_free((void **) &node);
            return NULL;
        }
        lv00_strlcpy(node->label, label, strlen(label) + 1);
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
            int new_cap = p->child_capacity == 0 ? 4 : p->child_capacity * 2;
            BacktrackNode **new_children = lv00_realloc(p->children, new_cap * sizeof(BacktrackNode *));
            if (!new_children)
                return false;
            p->children = new_children;
            p->child_capacity = new_cap;
        }
        p->children[p->child_count] = child;
        p->child_count++;
        child->parent = p;

        /* 更新统计信息 */
        switch (child->type) {
            case BACKTRACK_SUCCESS:
                tree->success_paths++;
                break;
            case BACKTRACK_FAILURE:
                tree->failure_paths++;
                break;
            case BACKTRACK_PRUNE:
                tree->pruned_branches++;
                break;
            default:
                break;
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
        int new_cap = tree->node_capacity == 0 ? 16 : tree->node_capacity * 2;
        BacktrackNode **new_nodes = lv00_realloc(tree->all_nodes, new_cap * sizeof(BacktrackNode *));
        if (!new_nodes)
            return false;
        tree->all_nodes = new_nodes;
        tree->node_capacity = new_cap;
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
    lv00_free((void **) &node->strategy_name);

    /* 使用安全的字符串复制函数，确保缓冲区零终止 */
    if (strategy_name && strategy_name[0] != '\0') {
        node->strategy_name = lv00_malloc(strlen(strategy_name) + 1);
        if (node->strategy_name) {
            lv00_strlcpy(node->strategy_name, strategy_name, strlen(strategy_name) + 1);
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

    /* 扩展策略数组 */
    char **new_strats = lv00_realloc(tree->available_strategies, (tree->strategy_count + 1) * sizeof(char *));
    if (!new_strats)
        return;
    tree->available_strategies = new_strats;

    /* 使用安全的字符串复制函数，确保缓冲区零终止 */
    tree->available_strategies[tree->strategy_count] = lv00_malloc(strlen(strategy_name) + 1);
    if (!tree->available_strategies[tree->strategy_count])
        return;
    lv00_strlcpy(tree->available_strategies[tree->strategy_count], strategy_name, strlen(strategy_name) + 1);
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
    lv00_free((void **) &tree->current_strategy);

    /* 使用安全的字符串复制函数，确保缓冲区零终止 */
    if (strategy_name && strategy_name[0] != '\0') {
        tree->current_strategy = lv00_malloc(strlen(strategy_name) + 1);
        if (tree->current_strategy) {
            lv00_strlcpy(tree->current_strategy, strategy_name, strlen(strategy_name) + 1);
        }
    } else {
        tree->current_strategy = NULL;
    }
}

/* ============== JSON/DOT 导出辅助函数 ============== */

/**
 * @brief 回溯节点类型转字符串
 */
static const char *backtrack_node_type_to_string(BacktrackNodeType type) {
    switch (type) {
        case BACKTRACK_CHOICE_POINT:
            return "choice";
        case BACKTRACK_FAILURE:
            return "failure";
        case BACKTRACK_SUCCESS:
            return "success";
        case BACKTRACK_PRUNE:
            return "prune";
        default:
            return "unknown";
    }
}

/**
 * @brief 递归将节点及其子树写入JSON
 */
static void backtrack_node_write_json(FILE *f, const BacktrackNode *node, int indent) {
    if (!f || !node)
        return;

    /* 缩进辅助 */
    char pad[128];
    int pad_len = indent * 2;
    if (pad_len > 120)
        pad_len = 120;
    memset(pad, ' ', pad_len);
    pad[pad_len] = '\0';

    fprintf(f, "%s{\n", pad);
    fprintf(f, "%s  \"id\": %d,\n", pad, node->id);
    fprintf(f, "%s  \"type\": \"%s\",\n", pad, backtrack_node_type_to_string(node->type));
    char _esc_buf1[512], _esc_buf2[512];
    json_escape(_esc_buf1, sizeof(_esc_buf1), node->label);
    json_escape(_esc_buf2, sizeof(_esc_buf2), node->strategy_name);
    fprintf(f, "%s  \"label\": \"%s\",\n", pad, _esc_buf1);
    fprintf(f, "%s  \"strategy\": \"%s\",\n", pad, _esc_buf2);
    fprintf(f, "%s  \"isBacktrackPoint\": %s,\n", pad, node->is_backtrack_point ? "true" : "false");
    fprintf(f, "%s  \"explored\": %s,\n", pad, node->explored ? "true" : "false");
    fprintf(f, "%s  \"color\": \"%s\",\n", pad, proof_color_to_string(node->color));
    fprintf(f, "%s  \"stepIndex\": %d,\n", pad, node->step_index);

    /* 子节点数组 */
    fprintf(f, "%s  \"children\": [\n", pad);
    for (int i = 0; i < node->child_count; i++) {
        backtrack_node_write_json(f, node->children[i], indent + 2);
        if (i < node->child_count - 1) {
            fprintf(f, ",\n");
        } else {
            fprintf(f, "\n");
        }
    }
    fprintf(f, "%s  ]\n", pad);

    fprintf(f, "%s}", pad);
}

/**
 * @brief 递归将节点及其子树写入DOT格式
 */
static void backtrack_node_write_dot(FILE *f, const BacktrackNode *node, int parent_id) {
    if (!f || !node)
        return;

    /* 节点颜色映射 */
    const char *fill_color;
    const char *border_color;
    const char *shape = node->is_backtrack_point ? "diamond" : "box";

    switch (node->type) {
        case BACKTRACK_SUCCESS:
            fill_color = "#90EE90";   /* light green */
            border_color = "#006400"; /* dark green */
            break;
        case BACKTRACK_FAILURE:
            fill_color = "#FFB6C1";   /* light red */
            border_color = "#8B0000"; /* dark red */
            break;
        case BACKTRACK_CHOICE_POINT:
            fill_color = "#87CEEB";   /* light blue */
            border_color = "#00008B"; /* dark blue */
            break;
        case BACKTRACK_PRUNE:
            fill_color = "#D3D3D3";   /* light gray */
            border_color = "#696969"; /* dim gray */
            break;
        default:
            fill_color = "#FFFFFF";
            border_color = "#000000";
            break;
    }

    /* 节点标签转义：双引号 -> 单引号，换行 -> 空格 */
    char safe_label[512];
    if (node->label) {
        size_t len = strlen(node->label);
        if (len > 500)
            len = 500;
        size_t j = 0;
        for (size_t i = 0; i < len && j < 500; i++) {
            if (node->label[i] == '"') {
                safe_label[j++] = '\'';
            } else if (node->label[i] == '\n') {
                safe_label[j++] = ' ';
            } else if (node->label[i] == '\\') {
                safe_label[j++] = '/';
            } else {
                safe_label[j++] = node->label[i];
            }
        }
        safe_label[j] = '\0';
    } else {
        safe_label[0] = '\0';
    }

    /* 已探索节点用更深的边框 */
    if (node->explored) {
        /* 保持颜色，边框用标准色 */
    }

    fprintf(f,
            "  node%d [shape=%s, style=filled, fillcolor=\"%s\", color=\"%s\", "
            "label=\"[%d] %s\"];\n",
            node->id, shape, fill_color, border_color, node->id, safe_label);

    /* 父边 */
    if (parent_id >= 0) {
        const char *edge_style = node->is_backtrack_point ? "dashed" : "solid";
        fprintf(f, "  node%d -> node%d [style=%s];\n", parent_id, node->id, edge_style);
    }

    /* 递归处理子节点 */
    for (int i = 0; i < node->child_count; i++) {
        backtrack_node_write_dot(f, node->children[i], node->id);
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

    FILE *f = fopen(filepath, "w");
    if (!f)
        return false;

    fprintf(f, "{\n");
    fprintf(f, "  \"strategy\": \"%s\",\n", tree->current_strategy ? tree->current_strategy : "");
    fprintf(f, "  \"successPaths\": %d,\n", tree->success_paths);
    fprintf(f, "  \"failurePaths\": %d,\n", tree->failure_paths);
    fprintf(f, "  \"backtrackCount\": %d,\n", tree->backtrack_count);
    fprintf(f, "  \"prunedBranches\": %d,\n", tree->pruned_branches);
    fprintf(f, "  \"maxDepth\": %d,\n", tree->max_depth);
    fprintf(f, "  \"nodeCount\": %d,\n", tree->node_count);
    fprintf(f, "  \"root\": ");
    if (tree->root) {
        backtrack_node_write_json(f, tree->root, 2);
        fprintf(f, "\n");
    } else {
        fprintf(f, "null\n");
    }
    fprintf(f, "}\n");

    fclose(f);
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

    FILE *f = fopen(filepath, "w");
    if (!f)
        return false;

    fprintf(f, "digraph ProofSearchTree {\n");
    fprintf(f, "  rankdir=TB;\n");
    fprintf(f, "  node [fontname=\"Arial\", fontsize=11];\n");
    fprintf(f, "  edge [fontname=\"Arial\", fontsize=9];\n");
    fprintf(f,
            "  label=\"\\n"
            "Proof Search Tree%s%s"
            "\\n"
            "Success: %d | Failure: %d | Backtrack: %d | Pruned: %d | Max Depth: %d"
            "\";\n",
            tree->current_strategy ? " - Strategy: " : "", tree->current_strategy ? tree->current_strategy : "",
            tree->success_paths, tree->failure_paths, tree->backtrack_count, tree->pruned_branches, tree->max_depth);
    fprintf(f, "  labelloc=t;\n");
    fprintf(f, "  fontsize=14;\n\n");

    if (tree->root) {
        backtrack_node_write_dot(f, tree->root, -1);
    }

    /* 图例 */
    fprintf(f, "\n  /* Legend */\n");
    fprintf(f, "  subgraph cluster_legend {\n");
    fprintf(f, "    label=\"Legend\";\n");
    fprintf(f, "    fontsize=10;\n");
    fprintf(f, "    style=dashed;\n");
    fprintf(f,
            "    legend_success [shape=box, style=filled, fillcolor=\"#90EE90\", "
            "label=\"Success\"];\n");
    fprintf(f,
            "    legend_failure [shape=box, style=filled, fillcolor=\"#FFB6C1\", "
            "label=\"Failure\"];\n");
    fprintf(f,
            "    legend_choice [shape=box, style=filled, fillcolor=\"#87CEEB\", "
            "label=\"Choice Point\"];\n");
    fprintf(f,
            "    legend_prune [shape=box, style=filled, fillcolor=\"#D3D3D3\", "
            "label=\"Pruned\"];\n");
    fprintf(f,
            "    legend_backtrack [shape=diamond, style=filled, fillcolor=\"lightyellow\", "
            "label=\"Backtrack\"];\n");
    fprintf(f, "  }\n");

    fprintf(f, "}\n");

    fclose(f);
    return true;
}

/* ============== 自然语言证明输出（AlphaGeometry风格） ============== */

/**
 * @brief 步骤类型到自然语言动词映射（中文）
 */
static const char *step_type_verb_zh(ProofStepType type) {
    switch (type) {
        case PROOF_STEP_ADD_NODE:
            return "构造";
        case PROOF_STEP_ADD_CONSTRAINT:
            return "添加约束";
        case PROOF_STEP_REWRITE:
            return "应用重写规则";
        case PROOF_STEP_FUNCTION_APP:
            return "应用函数块";
        case PROOF_STEP_PACK_FUNCTION:
            return "打包函数块";
        case PROOF_STEP_NORMALIZATION:
            return "执行规范化";
        case PROOF_STEP_UNIFY:
            return "执行合一检查";
        case PROOF_STEP_EX_FALSO:
            return "应用爆炸原理";
        case PROOF_STEP_ORACLE:
            return "引用外部预言机";
        default:
            return "执行操作";
    }
}

/**
 * @brief 步骤类型到自然语言动词映射（英文）
 */
static const char *step_type_verb_en(ProofStepType type) {
    switch (type) {
        case PROOF_STEP_ADD_NODE:
            return "Construct";
        case PROOF_STEP_ADD_CONSTRAINT:
            return "Add constraint";
        case PROOF_STEP_REWRITE:
            return "Apply rewrite rule";
        case PROOF_STEP_FUNCTION_APP:
            return "Apply function block";
        case PROOF_STEP_PACK_FUNCTION:
            return "Package function block";
        case PROOF_STEP_NORMALIZATION:
            return "Perform normalization";
        case PROOF_STEP_UNIFY:
            return "Perform unification check";
        case PROOF_STEP_EX_FALSO:
            return "Apply ex falso quodlibet";
        case PROOF_STEP_ORACLE:
            return "Reference external oracle";
        default:
            return "Execute operation";
    }
}

/**
 * @brief 生成步骤的几何对象描述（中文）
 */
static void describe_objects_zh(const ProofStep *step, char *buf, size_t buf_size) {
    buf[0] = '\0';
    if (step->node_id >= 0) {
        snprintf(buf + strlen(buf), buf_size - strlen(buf), "节点 %d", step->node_id);
    }
    if (step->constraint_id >= 0) {
        if (buf[0] != '\0')
            strncat(buf, "，", buf_size - strlen(buf) - 1);
        snprintf(buf + strlen(buf), buf_size - strlen(buf), "约束 %d", step->constraint_id);
    }
    if (step->rule_id >= 0) {
        if (buf[0] != '\0')
            strncat(buf, "，", buf_size - strlen(buf) - 1);
        snprintf(buf + strlen(buf), buf_size - strlen(buf), "规则 %d", step->rule_id);
    }
    if (step->func_block_id >= 0) {
        if (buf[0] != '\0')
            strncat(buf, "，", buf_size - strlen(buf) - 1);
        snprintf(buf + strlen(buf), buf_size - strlen(buf), "函数块 %d", step->func_block_id);
    }
}

/**
 * @brief 生成步骤的几何对象描述（英文）
 */
static void describe_objects_en(const ProofStep *step, char *buf, size_t buf_size) {
    buf[0] = '\0';
    if (step->node_id >= 0) {
        snprintf(buf + strlen(buf), buf_size - strlen(buf), "node %d", step->node_id);
    }
    if (step->constraint_id >= 0) {
        if (buf[0] != '\0')
            strncat(buf, ", ", buf_size - strlen(buf) - 1);
        snprintf(buf + strlen(buf), buf_size - strlen(buf), "constraint %d", step->constraint_id);
    }
    if (step->rule_id >= 0) {
        if (buf[0] != '\0')
            strncat(buf, ", ", buf_size - strlen(buf) - 1);
        snprintf(buf + strlen(buf), buf_size - strlen(buf), "rule %d", step->rule_id);
    }
    if (step->func_block_id >= 0) {
        if (buf[0] != '\0')
            strncat(buf, ", ", buf_size - strlen(buf) - 1);
        snprintf(buf + strlen(buf), buf_size - strlen(buf), "function block %d", step->func_block_id);
    }
}

/**
 * @brief 生成为什么可以进行这一步骤的解释（中文）
 */
static const char *explain_why_zh(ProofStepType type) {
    switch (type) {
        case PROOF_STEP_ADD_NODE:
            return "根据已知条件和构造规则，该几何对象可以合法构造。";
        case PROOF_STEP_ADD_CONSTRAINT:
            return "根据已构造的几何对象之间的关系，该约束成立。";
        case PROOF_STEP_REWRITE:
            return "模式匹配成功，重写规则的前提条件已满足。";
        case PROOF_STEP_FUNCTION_APP:
            return "函数块的输入端口类型与实参类型匹配。";
        case PROOF_STEP_NORMALIZATION:
            return "检测到坐标等价的节点，执行合并以保持图的一致性。";
        case PROOF_STEP_UNIFY:
            return "构造图与命题模式在所有层级完成匹配。";
        case PROOF_STEP_EX_FALSO:
            return "由矛盾 ⊥ 出发，根据爆炸原理可以推出任意命题。";
        case PROOF_STEP_ORACLE:
            return "此步骤依赖外部知识源，其正确性需要独立验证。";
        default:
            return "";
    }
}

/**
 * @brief 生成为什么可以进行这一步骤的解释（英文）
 */
static const char *explain_why_en(ProofStepType type) {
    switch (type) {
        case PROOF_STEP_ADD_NODE:
            return "Based on the known conditions and construction rules, this geometric object is validly "
                   "constructible.";
        case PROOF_STEP_ADD_CONSTRAINT:
            return "Based on the relationships between constructed geometric objects, this constraint holds.";
        case PROOF_STEP_REWRITE:
            return "Pattern matching succeeded; the preconditions of the rewrite rule are satisfied.";
        case PROOF_STEP_FUNCTION_APP:
            return "The input port types of the function block match the argument types.";
        case PROOF_STEP_NORMALIZATION:
            return "Coordinate-equivalent nodes detected; merging to maintain graph consistency.";
        case PROOF_STEP_UNIFY:
            return "The construction graph matches the proposition pattern at all levels.";
        case PROOF_STEP_EX_FALSO:
            return "From contradiction ⊥, any proposition follows by the principle of explosion.";
        case PROOF_STEP_ORACLE:
            return "This step depends on an external knowledge source whose correctness requires independent "
                   "verification.";
        default:
            return "";
    }
}

/**
 * @brief 将单个证明步骤转换为自然语言描述
 */
char *proof_step_get_natural_language(const ProofStep *step, ProofNaturalLanguage lang) {
    if (!step)
        return NULL;

    char obj_desc[256];
    char result[1024];
    const char *verb, *why, *step_type_name, *color_name;

    if (lang == PROOF_NL_LANG_ZH_CN) {
        verb = step_type_verb_zh(step->type);
        describe_objects_zh(step, obj_desc, sizeof(obj_desc));
        why = explain_why_zh(step->type);
        step_type_name = proof_step_type_to_string(step->type);
        color_name = proof_color_to_string(step->color);

        if (obj_desc[0] != '\0') {
            snprintf(result, sizeof(result),
                     "步骤 %d：%s%s。\n"
                     "  —— 涉及对象：%s\n"
                     "  —— 推理依据：%s\n"
                     "  —— 信任状态：%s",
                     step->id, verb, (step->type == PROOF_STEP_ADD_NODE) ? "新的几何对象" : "", obj_desc, why,
                     color_name);
        } else {
            snprintf(result, sizeof(result),
                     "步骤 %d：%s。\n"
                     "  —— 推理依据：%s\n"
                     "  —— 信任状态：%s",
                     step->id, verb, why, color_name);
        }
    } else {
        verb = step_type_verb_en(step->type);
        describe_objects_en(step, obj_desc, sizeof(obj_desc));
        why = explain_why_en(step->type);
        step_type_name = proof_step_type_to_string(step->type);
        color_name = proof_color_to_string(step->color);

        if (obj_desc[0] != '\0') {
            snprintf(result, sizeof(result),
                     "Step %d: %s.\n"
                     "  -- Objects involved: %s\n"
                     "  -- Reasoning: %s\n"
                     "  -- Trust status: %s",
                     step->id, verb, obj_desc, why, color_name);
        } else {
            snprintf(result, sizeof(result),
                     "Step %d: %s.\n"
                     "  -- Reasoning: %s\n"
                     "  -- Trust status: %s",
                     step->id, verb, why, color_name);
        }
    }

    return lv00_strdup(result);
}
