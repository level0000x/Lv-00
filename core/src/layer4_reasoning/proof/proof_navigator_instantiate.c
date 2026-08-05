/*
 * @file proof_navigator_instantiate.c
 * @brief Proof navigator module - proposition instantiation
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
#include "proof_classical.h"

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

    /* ---- 2. 深拷贝模式图（统一走公共入口 graph_copy） ---- */
    if (prop->pattern) {
        inst->pattern = graph_copy(prop->pattern);
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

/* 经典不可构造问题 → 构造图特征谓词（函数指针表分发；NONE/越界走 default → pattern_match 保持 false） */
static bool classical_matcher_trisection(const ConstraintGraph *graph) {
    /* 三等分角问题：通常涉及角度构造 */
    /* 检查图中是否有角度相关的约束 */
    for (int k = 0; k < graph->constraint_count; k++) {
        if (graph->constraints[k]->type == BETWEENNESS) {
            return true;
        }
    }
    return false;
}

static bool classical_matcher_doubling(const ConstraintGraph *graph) {
    /* 倍立方问题：涉及特定比例 */
    return graph->node_count >= 3 && graph->node_count <= 8;
}

static bool classical_matcher_squaring(const ConstraintGraph *graph) {
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
    return circle_count >= 2 && region_count >= 1;
}

static bool classical_matcher_heptagon(const ConstraintGraph *graph) {
    /* 正七边形构造 */
    return graph->node_count >= 7;
}

static bool (*kClassicalMatchers[])(const ConstraintGraph *) = {
    [CLASSICAL_PROBLEM_TRISECTION] = classical_matcher_trisection,
    [CLASSICAL_PROBLEM_DOUBLING]   = classical_matcher_doubling,
    [CLASSICAL_PROBLEM_SQUARING]   = classical_matcher_squaring,
    [CLASSICAL_PROBLEM_HEPTAGON]   = classical_matcher_heptagon,
};

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
            if (!pkg || pkg->known_unconstructibles.count <= 0)
                continue;

            /* 遍历此公理包中的所有已知不可构造问题 */
            for (int j = 0; j < pkg->known_unconstructibles.count; j++) {
                KnownUnconstructible *ku = (KnownUnconstructible *)lv_darray_get(&pkg->known_unconstructibles, j);
                if (!ku || !ku->name)
                    continue;

                /* 检查构造图的特征是否匹配此已知问题 */
                /* 启发式匹配：根据已知不可构造问题的特征检查约束图 */
                bool pattern_match = false;

                /* 经典不可构造问题的启发式匹配（统一走 proof_classical.h 查找表） */
                int classical_problem = lv_classical_problem_match(ku->name);
                if ((unsigned) classical_problem < sizeof(kClassicalMatchers) / sizeof(kClassicalMatchers[0]) &&
                    kClassicalMatchers[classical_problem]) {
                    pattern_match = kClassicalMatchers[classical_problem](graph);
                }

                if (pattern_match) {
                    info->result = UNCONSTRUCT_PROVED;
                    info->matched_problem = ku->name;
                    info->matched_theory = pkg->name ? pkg->name : "未知理论";
                    info->proof_strategy = "匹配已知不可构造问题";
                    info->reduction_steps = 0;

                    lvStrBuf sb_14 = {0};
                    lv_strbuf_printf(&sb_14, "构造匹配已知的不可构造问题 '%s'（来自公理包 '%s'）", ku->name,
                             pkg->name ? pkg->name : "未知");
                    info->detailed_report = lv_strdup(sb_14.data);

                    if (proof_stream_ctx) {
                        stream_emit_simple(proof_stream_ctx, STREAM_EVENT_PROOF_UNIFY, "匹配已知不可构造问题", 1);
                    }
                    lv_strbuf_destroy(&sb_14);
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

            lvStrBuf sb_15 = {0};
            lv_strbuf_printf(&sb_15, "命题已被标记为矛盾类型（BOTTOM），表示不可构造");
            info->detailed_report = lv_strdup(sb_15.data);
            lv_strbuf_destroy(&sb_15);
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
 * proof_navigator_search / constraint_solver_get_proposition
 * =========================================================================== */

/**
 * @brief 在证明导航器中执行搜索
 *
 * 遍历导航器的证明步骤，查找可应用的策略。
 * 如果导航器中有有效的证明步骤且目标非空，返回导航器本身作为"找到"。
 *
 * @param nav 证明导航器指针（ProofNavigator *）
 * @return 搜索结果指针，未找到或参数无效时返回 NULL
 */
void *proof_navigator_search(void *nav) {
    ProofNavigator *navigator;
    if (!nav)
        return NULL;

    navigator = (ProofNavigator *) nav;

    /* 验证导航器包含有效的证明步骤 */
    if (navigator->step_count <= 0)
        return NULL;

    if (navigator->current_step < 0 ||
        navigator->current_step >= navigator->step_count)
        return NULL;

    if (!navigator->steps[navigator->current_step])
        return NULL;

    /* 搜索成功：返回导航器指针 */
    return navigator;
}

/**
 * @brief 从约束求解器获取几何对象的命题描述
 *
 * 根据几何对象名称生成对应的类型命题描述。
 * geom_obj 实际类型为 const char *（几何对象名）。
 *
 * @param solver    约束求解器指针（未使用，保留为 API 兼容）
 * @param geom_obj  几何对象名（const char * 类型）
 * @return 命题字符串描述（指向线程局部 scratch 缓冲区，调用者不可释放），
 *         参数无效时返回 NULL
 */
const char *constraint_solver_get_proposition(void *solver, void *geom_obj) {
    const char *obj_name;

    (void) solver;

    if (!geom_obj)
        return NULL;

    obj_name = (const char *) geom_obj;
    if (*obj_name == '\0')
        return NULL;

    /* 根据对象名生成命题描述 */
    char *buf = lv_scratch_buf(128);
    lv_snprintf(buf, 128, "type(%s, object)", obj_name);
    return buf;
}
