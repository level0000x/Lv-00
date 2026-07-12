/**
 * @file proof_proposition.c
 * @brief Proposition 命题系统
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
#include "lv00/constraint_graph.h"
#include "debug.h"
#include "lv00_internal.h"
#include "lv00_utils.h"

/**
 * @brief 销毁命题并递归释放所有资源
 *
 * 释放端口ID数组、前提/后件区域ID数组、模式约束图，
 * 并递归销毁所有子命题。
 *
 * 使用迭代+栈的方式替代纯递归，防止深层嵌套命题导致栈溢出。
 *
 * @param prop 命题指针（可为 NULL）
 */
void proposition_destroy(Proposition *prop) {
    if (!prop)
        return;

    /* ====== 使用动态分配栈进行迭代销毁，防止递归栈溢出 ====== */
    /* 初始容量 128，按需动态扩容，避免深层嵌套命题导致内存泄漏 */
    int stack_capacity = PROOF_DESTROY_STACK_INITIAL_CAPACITY;
    int stack_top = 0;
    Proposition **destroy_stack = (Proposition **) lv00_malloc((size_t)stack_capacity * sizeof(Proposition *));
    if (!destroy_stack) {
        /* 分配失败时的降级处理：直接释放命题本身的资源
         * 注意：这种情况下子命题可能泄漏，但至少避免崩溃 */
        lv00_free((void **) &prop->input_port_ids);
        lv00_free((void **) &prop->output_port_ids);
        lv00_free((void **) &prop->precondition_region_ids);
        lv00_free((void **) &prop->postcondition_constraint_ids);
        if (prop->pattern)
            graph_destroy(prop->pattern);
        lv00_free((void **) &prop->sub_props);
        lv00_free((void **) &prop->name);
        lv00_free((void **) &prop->description);
        if (prop->prop_type)
            type_region_destroy(prop->prop_type);
        lv00_free((void **) &prop);
        return;
    }
    Proposition *current = prop;

    while (current || stack_top > 0) {
        while (current) {
            /* 将需要递归销毁的子命题入栈 */
            for (int i = 0; i < current->sub_prop_count; i++) {
                /* 栈满时动态扩容 */
                if (stack_top >= stack_capacity) {
                    /* 检查栈容量扩大的乘法是否会导致整数溢出 */
                    if (stack_capacity > INT_MAX / 2) {
                        /* 容量已达上限，停止扩容并清理已分配资源 */
                        for (int i = 0; i < stack_top; i++) {
                            proposition_unref(destroy_stack[i]);
                        }
                        lv00_free((void **)&destroy_stack);
                        return;
                    }
                    int new_cap = stack_capacity * 2;
                    if (new_cap <= stack_capacity) {
                        /* 整数溢出保护：使用循环逐个释放子命题的基本资源，避免递归导致栈溢出 */
                        for (int j = i; j < current->sub_prop_count; j++) {
                            if (current->sub_props[j]) {
                                Proposition *child = current->sub_props[j];
                                lv00_free((void **) &child->input_port_ids);
                                lv00_free((void **) &child->output_port_ids);
                                lv00_free((void **) &child->precondition_region_ids);
                                lv00_free((void **) &child->postcondition_constraint_ids);
                                if (child->pattern)
                                    graph_destroy(child->pattern);
                                lv00_free((void **) &child->sub_props);
                                lv00_free((void **) &child->name);
                                lv00_free((void **) &child->description);
                                if (child->prop_type)
                                    type_region_destroy(child->prop_type);
                                lv00_free((void **) &child);
                            }
                        }
                        break;
                    }
                    Proposition **new_stack =
                        (Proposition **) lv00_realloc(destroy_stack, new_cap * sizeof(Proposition *));
                    if (!new_stack) {
                        /* 栈扩容失败：使用循环安全释放所有子命题的内部资源 */
                        for (int j = i; j < current->sub_prop_count; j++) {
                            if (current->sub_props[j]) {
                                /* 使用非递归方式释放子命题，避免栈溢出 */
                                Proposition *child = current->sub_props[j];
                                lv00_free((void **) &child->input_port_ids);
                                lv00_free((void **) &child->output_port_ids);
                                lv00_free((void **) &child->precondition_region_ids);
                                lv00_free((void **) &child->postcondition_constraint_ids);
                                if (child->pattern)
                                    graph_destroy(child->pattern);
                                lv00_free((void **) &child->sub_props);
                                lv00_free((void **) &child->name);
                                lv00_free((void **) &child->description);
                                if (child->prop_type)
                                    type_region_destroy(child->prop_type);
                                lv00_free((void **) &child);
                            }
                        }
                        break;
                    }
                    destroy_stack = new_stack;
                    stack_capacity = new_cap;
                }
                destroy_stack[stack_top++] = current->sub_props[i];
            }

            /* 释放当前命题的资源 */
            lv00_free((void **) &current->input_port_ids);
            lv00_free((void **) &current->output_port_ids);
            lv00_free((void **) &current->precondition_region_ids);
            lv00_free((void **) &current->postcondition_constraint_ids);

            if (current->pattern) {
                graph_destroy(current->pattern);
                current->pattern = NULL;
            }

            lv00_free((void **) &current->sub_props);
            lv00_free((void **) &current->name);
            lv00_free((void **) &current->description);
            if (current->prop_type) {
                type_region_destroy(current->prop_type);
                current->prop_type = NULL;
            }

            /* 获取下一个待处理的子命题 */
            current = (stack_top > 0) ? destroy_stack[--stack_top] : NULL;
        }
        /* 内层 while 退出时 stack_top 必为 0，故此外层弹出为死代码，已移除 */
    }

    /* 最后释放命题本身 */
    lv00_free((void **) &prop);
    /* 释放动态分配的销毁栈 */
    lv00_free((void **) &destroy_stack);
}

/**
 * @brief 泛型命题 ID 列表设置辅助函数
 *
 * 统一实现 proposition_set_input_ports、set_output_ports、
 * set_preconditions、set_postconditions 四个函数的共同逻辑。
 * 先分配新内存，成功后再释放旧内存，保持状态一致性。
 *
 * @param target     目标 ID 列表指针的地址（如 &prop->input_port_ids）
 * @param count_ptr  对应的数量指针的地址（如 &prop->input_count）
 * @param ids        新的 ID 数组（count > 0 时不可为 NULL）
 * @param count      新的 ID 数量
 * @return true 表示成功，false 表示参数无效或内存分配失败
 */
static bool proposition_set_id_list(int **target, int *count_ptr, const int *ids, int count) {
    if (!target || !count_ptr)
        return false;

    int *new_ids = NULL;
    if (count > 0) {
        if (!ids)
            return false;
        new_ids = lv00_malloc((size_t)count * sizeof(int));
        if (!new_ids)
            return false;
        memcpy(new_ids, ids, count * sizeof(int));
    }
    lv00_free((void **) target);
    *target = new_ids;
    *count_ptr = count;
    return true;
}

/**
 * @brief 设置命题的输入端口列表
 *
 * 先分配新内存，成功后再释放旧内存，保持状态一致性。
 *
 * @param prop     命题指针
 * @param port_ids 端口 ID 数组（count > 0 时不可为 NULL）
 * @param count    端口数量
 * @return true 表示成功，false 表示参数无效或内存分配失败
 */
bool proposition_set_input_ports(Proposition *prop, const int *port_ids, int count) {
    if (!prop)
        return false;
    bool result = proposition_set_id_list(&prop->input_port_ids, &prop->input_count, port_ids, count);
    if (result) {
        prop->last_modified = time(NULL);
    }
    return result;
}

/**
 * @brief 设置命题的输出端口列表
 *
 * 与输入端口设置相同，先分配新内存再释放旧内存。
 *
 * @param prop     命题指针
 * @param port_ids 端口 ID 数组（count > 0 时不可为 NULL）
 * @param count    端口数量
 * @return true 表示成功，false 表示参数无效或内存分配失败
 */
bool proposition_set_output_ports(Proposition *prop, const int *port_ids, int count) {
    if (!prop)
        return false;
    bool result = proposition_set_id_list(&prop->output_port_ids, &prop->output_count, port_ids, count);
    if (result) {
        prop->last_modified = time(NULL);
    }
    return result;
}

/**
 * @brief 设置命题的模式约束图
 *
 * 替换命题中现有的模式约束图。如果已有模式图，先销毁再设置新图。
 * 设置后命题拥有 pattern 的所有权，调用者不应再手动销毁。
 *
 * @param prop    命题指针
 * @param pattern 模式约束图指针（命题接管所有权）
 * @return true 设置成功，false 参数无效
 */
bool proposition_set_pattern(Proposition *prop, ConstraintGraph *pattern) {
    if (!prop)
        return false;

    if (prop->pattern) {
        graph_destroy(prop->pattern);
    }
    prop->pattern = pattern;
    prop->last_modified = time(NULL);
    return true;
}

/**
 * @brief 设置命题的前提区域 ID 列表
 *
 * @param prop       命题指针
 * @param region_ids 区域 ID 数组（函数内部拷贝）
 * @param count      区域 ID 数量
 * @return true 设置成功，false 参数无效或内存分配失败
 */
bool proposition_set_preconditions(Proposition *prop, const int *region_ids, int count) {
    if (!prop)
        return false;
    bool result = proposition_set_id_list(&prop->precondition_region_ids, &prop->precondition_count, region_ids, count);
    if (result) {
        prop->last_modified = time(NULL);
    }
    return result;
}

/**
 * @brief 设置命题的后件约束 ID 列表
 *
 * @param prop           命题指针
 * @param constraint_ids 约束 ID 数组（函数内部拷贝）
 * @param count          约束 ID 数量
 * @return true 设置成功，false 参数无效或内存分配失败
 */
bool proposition_set_postconditions(Proposition *prop, const int *constraint_ids, int count) {
    if (!prop)
        return false;
    bool result = proposition_set_id_list(&prop->postcondition_constraint_ids, &prop->postcondition_count, constraint_ids,
                                   count);
    if (result) {
        prop->last_modified = time(NULL);
    }
    return result;
}

/**
 * @brief 向命题添加子命题
 *
 * 将子命题追加到父命题的子命题数组中，动态扩容。
 *
 * @param parent 父命题指针
 * @param child  子命题指针
 * @return true 添加成功，false 参数无效或内存分配失败
 */
bool proposition_add_sub_proposition(Proposition *parent, Proposition *child) {
    if (!parent || !child)
        return false;

    int new_count = parent->sub_prop_count + 1;
    Proposition **new_arr = lv00_realloc(parent->sub_props, (size_t) new_count * sizeof(Proposition *));
    if (!new_arr)
        return false;

    parent->sub_props = new_arr;
    parent->sub_props[parent->sub_prop_count] = child;
    parent->sub_prop_count = new_count;
    parent->last_modified = time(NULL);
    return true;
}


/**
 * 深拷贝整个 ConstraintGraph。
 * 返回一个完全独立的新图，规范化副本不会影响原图。
 *
 * 拷贝策略：
 * - GeomNode: 新结构体，共享 SymbolicCoord 对象（规范化不改坐标值）
 * - Port: 新结构体，connected_to 置 NULL
 * - Constraint: 新结构体，新 participants 数组
 * - 内部指针（boundary_segments, internal_nodes）指向新图中的节点
 */
static ConstraintGraph *deep_copy_graph(const ConstraintGraph *orig) {
    if (!orig)
        return NULL;

    ConstraintGraph *copy = graph_create();
    if (!copy)
        return NULL;

    /* 拷贝元数据 */
    copy->next_node_id = orig->next_node_id;
    copy->next_constraint_id = orig->next_constraint_id;

    /* ---- 第一遍：深拷贝所有节点 ---- */
    if (orig->node_count > 0) {
        copy->nodes = lv00_malloc(orig->node_count * sizeof(GeomNode *));
        if (!copy->nodes) {
            graph_destroy(copy);
            return NULL;
        }
        for (int i = 0; i < orig->node_count; i++) {
            copy->nodes[i] = node_deep_copy_geom_node(orig->nodes[i], NULL);
            if (!copy->nodes[i] && orig->nodes[i]) {
                graph_destroy(copy);
                return NULL;
            }
        }
        copy->node_count = orig->node_count;
    }

    /* ---- 第二遍：更新内部指针到新图中的节点 ---- */
    for (int i = 0; i < copy->node_count; i++) {
        GeomNode *cn = copy->nodes[i];
        switch (cn->type) {
            case GEOM_REGION:
                for (int j = 0; j < cn->data.region.segment_count; j++) {
                    if (cn->data.region.boundary_segments[j]) {
                        int old_id = cn->data.region.boundary_segments[j]->id;
                        GeomNode *new_node = graph_get_node(copy, old_id);
                        if (new_node) {
                            cn->data.region.boundary_segments[j] = new_node;
                        }
                    }
                }
                break;
            case GEOM_FUNCTION_BLOCK:
                for (int j = 0; j < cn->data.func_block.internal_node_count; j++) {
                    if (cn->data.func_block.internal_nodes[j]) {
                        int old_id = cn->data.func_block.internal_nodes[j]->id;
                        GeomNode *new_node = graph_get_node(copy, old_id);
                        if (new_node) {
                            cn->data.func_block.internal_nodes[j] = new_node;
                        }
                    }
                }
                break;
            case GEOM_PORT:
                if (cn->data.port && orig->nodes[i]->data.port && orig->nodes[i]->data.port->connected_to) {
                    int old_id = orig->nodes[i]->data.port->connected_to->id;
                    GeomNode *new_node = graph_get_node(copy, old_id);
                    if (new_node) {
                        cn->data.port->connected_to = new_node;
                    }
                }
                break;
            default:
                break;
        }
    }

    /* ---- 深拷贝所有约束 ---- */
    if (orig->constraint_count > 0) {
        copy->constraints = lv00_malloc(orig->constraint_count * sizeof(Constraint *));
        if (!copy->constraints) {
            graph_destroy(copy);
            return NULL;
        }
        for (int i = 0; i < orig->constraint_count; i++) {
            Constraint *oc = orig->constraints[i];
            Constraint *cc = lv00_malloc(sizeof(Constraint));
            if (!cc) {
                graph_destroy(copy);
                return NULL;
            }
            cc->id = oc->id;
            cc->type = oc->type;
            cc->template_id = oc->template_id;
            cc->participant_count = oc->participant_count;
            if (oc->participants && oc->participant_count > 0) {
                cc->participants = lv00_malloc(oc->participant_count * sizeof(int));
                if (!cc->participants) {
                    lv00_free((void **) &cc);
                    graph_destroy(copy);
                    return NULL;
                }
                memcpy(cc->participants, oc->participants, oc->participant_count * sizeof(int));
            } else {
                cc->participants = NULL;
            }
            copy->constraints[i] = cc;
        }
        copy->constraint_count = orig->constraint_count;
    }

    return copy;
}

/* ============== 合一检查 ============== */

/**
 * @brief 尝试将命题模式与构造图进行统一匹配
 *
 * @param construction 构造图
 * @param proposition  命题
 * @param normalize_first 是否先规范化
 * @return 统一状态
 */
UnifyStatus proof_unify(const ConstraintGraph *construction, Proposition *proposition, bool normalize_first) {
    if (!construction || !proposition)
        return UNIFY_STATUS_FAILED;

    ConstraintGraph *pattern = proposition->pattern;

    if (!pattern)
        return UNIFY_STATUS_FAILED;

    /* 始终深拷贝构造图，避免 unify 过程修改调用者的原图 */
    ConstraintGraph *constr_copy = deep_copy_graph(construction);
    if (!constr_copy)
        return UNIFY_STATUS_FAILED;

    /* P-1 修复：深拷贝命题模式图，避免修改调用者的原始 pattern。
     * 之前的实现直接修改 pattern 中约束的 template_id，这是一个副作用。
     * 现在改为在副本上进行模板展开，保护调用者的数据不被修改。 */
    ConstraintGraph *pattern_copy = deep_copy_graph(pattern);
    if (!pattern_copy) {
        graph_destroy(constr_copy);
        return UNIFY_STATUS_FAILED;
    }

    /* 如果需要，先执行图规范化遍 */
    if (normalize_first) {
        /* 执行规范化 */
        NormalizationResult *norm_result = graph_normalize(constr_copy, false);
        if (!norm_result) {
            graph_destroy(pattern_copy);
            graph_destroy(constr_copy);
            return UNIFY_STATUS_FAILED;
        }
        normalization_result_destroy(norm_result);
    }

    /* 三层匹配 */

    /* 第零层：约束模板展开
     * 在约束匹配之前，将命题模式图副本中所有 template_id != -1 的约束
     * 展开为其基本约束形式（将 template_id 置为 -1，保留类型和参与者）。
     * 这确保后续的约束匹配只处理基本约束。
     * P-1 修复：现在在 pattern_copy 上操作，而非原始 pattern。
     */
    {
        for (int i = 0; i < pattern_copy->constraint_count; i++) {
            Constraint *c = pattern_copy->constraints[i];
            if (c && c->template_id != -1) {
                c->template_id = -1; /* 展开为基本约束 */
            }
        }
    }

    /* 第一层：端口类型匹配 */
    if (proposition->input_count > 0 || proposition->output_count > 0) {
        /* 检查构造图的端口数量是否匹配 */
        int constr_input_count = 0;
        int constr_output_count = 0;

        for (int i = 0; i < constr_copy->node_count; i++) {
            GeomNode *n = constr_copy->nodes[i];
            if (n->type == GEOM_PORT && n->data.port) {
                if (n->data.port->is_formal_param) {
                    constr_input_count++;
                } else {
                    constr_output_count++;
                }
            }
        }

        if (constr_input_count != proposition->input_count || constr_output_count != proposition->output_count) {
            graph_destroy(pattern_copy);
            graph_destroy(constr_copy);
            return UNIFY_STATUS_PORT_TYPE_MISMATCH;
        }
    }

    /* 第二层：约束类型匹配（使用 pattern_copy 而非原始 pattern） */
    UnifyStatus unify_status = unify_construction_with_proposition(constr_copy, pattern_copy);

    if (unify_status != UNIFY_STATUS_OK) {
        graph_destroy(pattern_copy);
        graph_destroy(constr_copy);
        return unify_status;
    }

    /* 第三层：坐标级等价验证
     * Per design_v2.9.md Section 10.2: after constraint matching passes,
     * verify that all coordinate parameters are algebraically equivalent. */
    UnifyStatus coord_status = unify_construction_with_proposition_coord(constr_copy, pattern_copy);

    graph_destroy(pattern_copy);
    graph_destroy(constr_copy);

    if (coord_status == UNIFY_STATUS_OK) {
        proposition->color = PROOF_COLOR_GREEN;
    }

    /* 流式事件：合一检查结果 */
    if (proof_stream_ctx != NULL) {
        const char *status_str = unify_result_to_string(coord_status);
        char buf[128];
        snprintf(buf, sizeof(buf), "证明合一检查: %s", status_str);
        stream_emit_simple(proof_stream_ctx, STREAM_EVENT_PROOF_UNIFY, buf, 0);
    }

    return coord_status;
}

/**
 * @brief 带详细信息的合一检查
 *
 * 执行 proof_unify 并在失败时通过 out_mismatch_info 返回人类可读的错误描述。
 * 调用者负责通过 lv00_free 释放 *out_mismatch_info。
 *
 * @param construction      构造图
 * @param proposition       命题
 * @param out_mismatch_info [输出] 不匹配的描述字符串（仅在合一失败时设置）
 * @return 合一状态
 */
UnifyStatus proof_unify_detailed(const ConstraintGraph *construction, Proposition *proposition, char **out_mismatch_info) {
    if (!out_mismatch_info) {
        return proof_unify(construction, proposition, true);
    }

    UnifyStatus result = proof_unify(construction, proposition, true);

    /* 释放旧值，避免内存泄漏 */
    if (result != UNIFY_STATUS_OK && out_mismatch_info) {
        if (*out_mismatch_info) {
            lv00_free((void **) out_mismatch_info);
            *out_mismatch_info = NULL;
        }
        switch (result) {
            case UNIFY_STATUS_PORT_TYPE_MISMATCH:
                *out_mismatch_info = lv00_strdup_safe("端口类型不匹配");
                break;
            case UNIFY_STATUS_CONSTRAINT_MISMATCH:
                *out_mismatch_info = lv00_strdup_safe("约束类型不匹配");
                break;
            case UNIFY_STATUS_COORD_MISMATCH:
                *out_mismatch_info = lv00_strdup_safe("符号坐标不匹配");
                break;
            case UNIFY_STATUS_STRUCTURE_MISMATCH:
                *out_mismatch_info = lv00_strdup_safe("结构不匹配");
                break;
            case UNIFY_STATUS_SCOPE_MISMATCH:
                *out_mismatch_info = lv00_strdup_safe("作用域不匹配");
                break;
            default:
                *out_mismatch_info = lv00_strdup_safe("未知错误");
                break;
        }
    }

    return result;
}

/* ============== 证明步骤管理 ============== */

/**
 * 自动更新后继依赖关系
 * 当 step_id 依赖 dep_id 时，将 step_id 添加到 dep_id 的 dependent_step_ids 中
 * 注意：此函数需要通过导航器查找 dep_id 对应的步骤，因此需要导航器上下文
 */
static void update_dependent_steps(ProofNavigator *nav, int step_id, int dep_id) {
    if (!nav || dep_id < 0 || dep_id >= nav->step_count)
        return;

    ProofStep *dep_step = nav->steps[dep_id];
    if (!dep_step)
        return;

    /* 检查是否已经记录过此后继关系（避免重复） */
    for (int i = 0; i < dep_step->dependent_count; i++) {
        if (dep_step->dependent_step_ids[i] == step_id) {
            return; /* 已存在，无需重复添加 */
        }
    }

    /* 将 step_id 添加到 dep_id 的 dependent_step_ids 中 */
    dep_step->dependent_count++;
    int *new_arr = lv00_realloc(dep_step->dependent_step_ids, dep_step->dependent_count * sizeof(int));
    if (!new_arr) {
        dep_step->dependent_count--;
        return;
    }

    dep_step->dependent_step_ids = new_arr;
    dep_step->dependent_step_ids[dep_step->dependent_count - 1] = step_id;
}

/**
 * @brief 创建证明步骤
 *
 * 分配并初始化一个 ProofStep 结构体，设置类型、默认颜色（绿色）和时间戳。
 *
 * @param type 证明步骤类型
 * @return 新分配的证明步骤指针，失败返回 NULL
 */
ProofStep *proof_step_create(ProofStepType type) {
    ProofStep *step = lv00_calloc(1, sizeof(ProofStep));
    if (!step)
        return NULL;

    step->type = type;
    step->color = PROOF_COLOR_GREEN; /* 默认绿色 */
    step->timestamp = (long) time(NULL);
    step->parent_step_id = -1; /* 根步骤，无父步骤 */
    step->depth = 0;           /* 根步骤深度为 0 */

    return step;
}

/**
 * @brief 销毁证明步骤并释放所有资源
 *
 * @param step 证明步骤指针（可为 NULL）
 */
void proof_step_destroy(ProofStep *step) {
    if (!step)
        return;

    lv00_free((void **) &step->merged_node_ids);
    lv00_free((void **) &step->dependency_step_ids);
    lv00_free((void **) &step->dependent_step_ids);
    lv00_free((void **) &step->note);
    lv00_free((void **) &step);
}

/**
 * @brief 为证明步骤添加依赖关系
 *
 * 将依赖步骤 ID 追加到步骤的依赖列表中，动态扩容。
 *
 * @param step        证明步骤指针
 * @param dep_step_id 依赖的步骤 ID
 * @return true 添加成功，false 参数无效或内存分配失败
 */
bool proof_step_add_dependency(ProofStep *step, int dep_step_id) {
    if (!step)
        return false;

    int new_count = step->dependency_count + 1;
    int *new_arr = lv00_realloc(step->dependency_step_ids, (size_t) new_count * sizeof(int));
    if (!new_arr)
        return false;

    step->dependency_step_ids = new_arr;
    step->dependency_step_ids[step->dependency_count] = dep_step_id;
    step->dependency_count = new_count;
    return true;
}

/**
 * @brief 设置证明步骤的断点标志
 *
 * @param step         证明步骤指针
 * @param is_breakpoint 是否设置为断点
 */
void proof_step_set_breakpoint(ProofStep *step, bool is_breakpoint) {
    if (step)
        step->is_breakpoint = is_breakpoint;
}

/* ============== 证明导航器 ============== */

/**
 * @brief 创建证明导航器
 *
 * 分配并初始化证明导航器，绑定目标命题和引擎实例。
 * 导航器用于管理证明步骤的执行、回退和状态追踪。
 *
 * @param target 目标命题指针
 * @param engine 引擎实例指针
 * @return 新分配的导航器指针，失败返回 NULL
 */
ProofNavigator *proof_navigator_create(Proposition *target, LV00Engine *engine) {
    if (proof_stream_ctx) {
        stream_emit_simple(proof_stream_ctx, STREAM_EVENT_PROOF_STEP_ADDED, "证明导航器创建", 0);
    }

    ProofNavigator *nav = lv00_calloc(1, sizeof(ProofNavigator)); /* 统一内存分配器 */
    if (!nav)
        return NULL;

    nav->target_prop = target;
    nav->engine = engine; /* 保存引擎上下文 */
    nav->current_step = -1;
    nav->is_complete = false;
    nav->final_color = PROOF_COLOR_BLUE_UNEXPLORED;
    nav->proof_state = PROOF_STATE_ONGOING;
    nav->strategy_note = NULL; /* LeanGeo风格：策略注释 */

    return nav;
}

/**
 * @brief 销毁证明导航器并释放所有资源
 *
 * 逐个销毁所有证明步骤，释放等价命题表、自底定义、
 * 引理视图状态和导航器自身。
 *
 * @param nav 证明导航器指针（可为 NULL）
 */
void proof_navigator_destroy(ProofNavigator *nav) {
    if (!nav)
        return;

    for (int i = 0; i < nav->step_count; i++) {
        proof_step_destroy(nav->steps[i]);
    }
    lv00_free((void **) &nav->steps);

    if (nav->dep_tree) {
        proof_dependency_destroy(nav->dep_tree);
    }

    lv00_free((void **) &nav->breakpoint_indices);

    /* 释放等价命题表 */
    for (int i = 0; i < nav->equivalence_count; i++) {
        if (nav->equivalences[i].transformation) {
            graph_destroy(nav->equivalences[i].transformation);
        }
    }
    lv00_free((void **) &nav->equivalences);

    /* 释放 ⊥ 定义 */
    lv00_free((void **) &nav->bottom_def);

    /* 释放引理视图状态 */
    lv00_free((void **) &nav->lemma_view_step_ids);
    lv00_free((void **) &nav->lemma_view_states);

    /* 释放策略注释 */
    lv00_free((void **) &nav->strategy_note);

    /* 流式输出: 证明导航器销毁 */
    if (proof_stream_ctx) {
        stream_emit_simple(proof_stream_ctx, STREAM_EVENT_PROOF_STEP_APPLIED, "证明导航器销毁", nav->step_count);
    }

    lv00_free((void **) &nav);
}

/**
 * @brief 向证明导航器添加证明步骤
 *
 * 将步骤追加到导航器的步骤数组中，自动设置步骤 ID，
 * 更新前驱-后继依赖关系，并记录断点信息。
 * 添加成功后导航器自动移动到新步骤。
 *
 * @param nav  证明导航器指针
 * @param step 证明步骤指针
 * @return true 添加成功，false 参数无效或内存分配失败
 */
bool proof_navigator_add_step(ProofNavigator *nav, ProofStep *step) {
    if (!nav || !step)
        return false;

    /* 流式输出：证明步骤添加 */
    if (proof_stream_ctx) {
        char desc_buf[128];
        snprintf(desc_buf, sizeof(desc_buf), "证明步骤添加: %s", proof_step_type_to_string(step->type));
        stream_emit_simple(proof_stream_ctx, STREAM_EVENT_PROOF_STEP_ADDED, desc_buf, nav->step_count);
    }

    step->id = nav->step_count;

    /* 设置父子关系和深度 */
    if (nav->step_count > 0 && nav->current_step >= 0 && nav->current_step < nav->step_count) {
        step->parent_step_id = nav->current_step;
        ProofStep *parent = nav->steps[nav->current_step];
        if (parent) {
            step->depth = parent->depth + 1;
        }
    }

    int new_count = nav->step_count + 1;
    ProofStep **new_arr = lv00_realloc(nav->steps, (size_t) new_count * sizeof(ProofStep *));
    if (!new_arr)
        return false;

    nav->steps = new_arr;
    nav->steps[nav->step_count] = step;
    nav->step_count = new_count;
    nav->current_step = new_count - 1;

    /* 自动更新后继依赖关系：遍历步骤的前驱依赖，将当前步骤添加到被依赖步骤的后继列表中 */
    for (int i = 0; i < step->dependency_count; i++) {
        update_dependent_steps(nav, step->id, step->dependency_step_ids[i]);
    }

    /* 如果是断点，添加到断点列表 */
    if (step->is_breakpoint) {
        nav->breakpoint_count++;
        int *new_bp = lv00_realloc(nav->breakpoint_indices, nav->breakpoint_count * sizeof(int));
        if (new_bp) {
            nav->breakpoint_indices = new_bp;
            nav->breakpoint_indices[nav->breakpoint_count - 1] = step->id;
        }
    }

    /* 逻辑互斥校验：检查新步骤是否产生了矛盾命题 */
    if (nav->target_prop && step->type == PROOF_STEP_UNIFY) {
        /* 遍历所有已存在的命题，检查是否与新推导结论矛盾 */
        /* 这里通过导航器的目标命题和已加载的引擎命题进行检测 */
        /* 自矛盾检测：检查目标命题的子命题之间是否存在矛盾 */
        bool has_contradiction = false;
        if (nav->target_prop->sub_props && nav->target_prop->sub_prop_count >= 2) {
            for (int i = 0; i < nav->target_prop->sub_prop_count && !has_contradiction; i++) {
                for (int j = i + 1; j < nav->target_prop->sub_prop_count && !has_contradiction; j++) {
                    if (proposition_contradicts(nav->target_prop->sub_props[i],
                                                nav->target_prop->sub_props[j])) {
                        has_contradiction = true;
                    }
                }
            }
        }
        if (has_contradiction) {
            if (proof_stream_ctx) {
                stream_emit_simple(proof_stream_ctx, STREAM_EVENT_WARNING,
                                   "逻辑互斥: 检测到自矛盾命题", step->id);
            }
            nav->proof_state = PROOF_STATE_CONTRADICTORY;
        }
    }

    return true;
}

/**
 * @brief 导航到下一个证明步骤
 *
 * @param nav 证明导航器指针
 * @return true 导航成功，false 已到达最后一步或参数无效
 */
bool proof_navigator_next(ProofNavigator *nav) {
    if (!nav || nav->current_step >= nav->step_count - 1)
        return false;
    nav->current_step++;

    if (proof_stream_ctx) {
        stream_emit_simple(proof_stream_ctx, STREAM_EVENT_PROOF_STEP_APPLIED, "执行下一步", nav->current_step);
    }

    return true;
}

/**
 * @brief 导航到上一个证明步骤
 *
 * @param nav 证明导航器指针
 * @return true 导航成功，false 已在第一步或参数无效
 */
bool proof_navigator_prev(ProofNavigator *nav) {
    if (!nav || nav->current_step <= 0)
        return false;
    nav->current_step--;

    /* 流式事件：回退到上一步 */
    if (proof_stream_ctx != NULL) {
        char buf[128];
        snprintf(buf, sizeof(buf), "证明导航: 回退到步骤 %d", nav->current_step);
        stream_emit_simple(proof_stream_ctx, STREAM_EVENT_PROOF_STEP_APPLIED, buf, 0);
    }

    return true;
}

/**
 * @brief 跳转到指定的证明步骤索引
 *
 * @param nav        证明导航器指针
 * @param step_index 目标步骤索引（0-based）
 * @return true 跳转成功，false 索引越界或参数无效
 */
bool proof_navigator_goto(ProofNavigator *nav, int step_index) {
    if (!nav || step_index < 0 || step_index >= nav->step_count)
        return false;
    nav->current_step = step_index;

    /* 流式事件：跳转到指定步骤 */
    if (proof_stream_ctx != NULL) {
        char buf[128];
        snprintf(buf, sizeof(buf), "证明导航: 跳转到步骤 %d", step_index);
        stream_emit_simple(proof_stream_ctx, STREAM_EVENT_PROOF_STEP_APPLIED, buf, 0);
    }

    return true;
}

/**
 * @brief 导航到下一个断点步骤
 *
 * 从当前位置向后搜索第一个标记为断点的步骤并跳转。
 *
 * @param nav 证明导航器指针
 * @return true 找到并跳转成功，false 无后续断点或参数无效
 */
bool proof_navigator_next_breakpoint(ProofNavigator *nav) {
    if (!nav || nav->breakpoint_count == 0)
        return false;

    /* 查找下一个断点 */
    for (int i = 0; i < nav->breakpoint_count; i++) {
        if (nav->breakpoint_indices[i] > nav->current_step) {
            nav->current_step = nav->breakpoint_indices[i];

            /* 流式事件：跳转到断点 */
            if (proof_stream_ctx != NULL) {
                char buf[128];
                snprintf(buf, sizeof(buf), "证明导航: 跳转到断点步骤 %d", nav->current_step);
                stream_emit_simple(proof_stream_ctx, STREAM_EVENT_PROOF_STEP_APPLIED, buf, 0);
            }

            return true;
        }
    }

    return false;
}

/**
 * @brief 获取导航器当前的证明步骤
 *
 * @param nav 证明导航器指针
 * @return 当前步骤指针，无效状态返回 NULL
 */
ProofStep *proof_navigator_current_step(ProofNavigator *nav) {
    if (!nav || nav->current_step < 0 || nav->current_step >= nav->step_count) {
        return NULL;
    }
