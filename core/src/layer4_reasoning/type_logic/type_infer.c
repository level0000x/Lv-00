/**
 * @file type_infer.c
 * @brief 类型推断实现 —— 节点/端口类型推断、变量实例化与替换
 *
 * @details 本文件从 type_system.c 中拆分而来，封装所有类型推断相关操作。
 *          通过 extern 引用 type_system.c 中定义的流式上下文。
 *
 *          功能包括：
 *          - 节点类型推断（基于几何类型和约束关系）
 *          - 端口类型推断（基于连接关系）
 *          - 类型变量实例化
 *          - 类型变量替换
 *
 * @author Lv-00 Project
 */

#include "lv/type_system.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/lv.h"
#include "lv/lv_xmacro.h"
#include "lv/stream.h"

#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/rewrite.h"
#include "lv/lv_strbuf.h"

/* 访问 type_system.c 中定义的流式上下文变量 */
extern lv_THREAD_LOCAL StreamContext *type_system_stream_ctx;

/* ============== 前向声明 ============== */

/* ================================================================
 * 查找表：GeomType → 类型推断处理函数
 * ================================================================ */

/** @brief 节点类型推断处理函数类型 */
typedef bool (*GeomTypeInferHandler)(TypeSystem *ts, ConstraintGraph *graph, GeomNode *node, TypeRegion **out_type);

/** @brief 推断 GEOM_POINT 类型 */
static bool infer_geom_point(TypeSystem *ts, ConstraintGraph *graph, GeomNode *node, TypeRegion **out_type) {
    (void) graph;
    (void) node;
    *out_type = type_create_point(ts);
    return true;
}
/** @brief 推断 GEOM_LINE_SEGMENT 类型 */
static bool infer_geom_line_segment(TypeSystem *ts, ConstraintGraph *graph, GeomNode *node, TypeRegion **out_type) {
    (void) graph;
    (void) node;
    *out_type = type_create_line_segment(ts);
    return true;
}
/** @brief 推断 GEOM_REGION 类型 */
static bool infer_geom_region(TypeSystem *ts, ConstraintGraph *graph, GeomNode *node, TypeRegion **out_type) {
    (void) graph;
    (void) node;
    *out_type = type_create_region(ts, NULL, 0);
    return true;
}
/** @brief 推断 GEOM_CIRCLE 类型 */
static bool infer_geom_circle(TypeSystem *ts, ConstraintGraph *graph, GeomNode *node, TypeRegion **out_type) {
    (void) graph;
    (void) node;
    *out_type = type_create_region(ts, NULL, 0);
    return true;
}
/** @brief 推断 GEOM_PORT 类型 */
static bool infer_geom_port(TypeSystem *ts, ConstraintGraph *graph, GeomNode *node, TypeRegion **out_type) {
    (void) node;
    return type_infer_port(ts, graph, node->id, out_type);
}
/** @brief 推断 GEOM_FUNCTION_BLOCK 类型 */
static bool infer_geom_function_block(TypeSystem *ts, ConstraintGraph *graph, GeomNode *node, TypeRegion **out_type) {
    (void) graph;
    TypeRegion *input_type = NULL;
    TypeRegion *output_type = NULL;

    if (node->data.func_block.input_port_ids && node->data.func_block.input_count > 0) {
        for (int i = 0; i < node->data.func_block.input_count; i++) {
            TypeRegion *port_type = NULL;
            if (type_infer_port(ts, graph, node->data.func_block.input_port_ids[i], &port_type)) {
                if (input_type == NULL) {
                    input_type = port_type;
                } else {
                    input_type = type_create_product(ts, input_type, port_type);
                }
            }
        }
    }

    if (node->data.func_block.output_port_ids && node->data.func_block.output_count > 0) {
        for (int i = 0; i < node->data.func_block.output_count; i++) {
            TypeRegion *port_type = NULL;
            if (type_infer_port(ts, graph, node->data.func_block.output_port_ids[i], &port_type)) {
                if (output_type == NULL) {
                    output_type = port_type;
                } else {
                    output_type = type_create_product(ts, output_type, port_type);
                }
            }
        }
    }

    *out_type = type_create_function(ts, input_type, output_type);
    return true;
}

/**
 * @brief GeomType 推断处理函数查找表（按枚举值升序）
 *
 * 索引：GEOM_POINT=0, GEOM_LINE_SEGMENT=1, GEOM_REGION=2,
 *       GEOM_CIRCLE=3, GEOM_PORT=4, GEOM_FUNCTION_BLOCK=5
 */
static const GeomTypeInferHandler s_geom_type_infer_handlers[] = {
    infer_geom_point,          /* GEOM_POINT */
    infer_geom_line_segment,   /* GEOM_LINE_SEGMENT */
    infer_geom_region,         /* GEOM_REGION */
    infer_geom_circle,         /* GEOM_CIRCLE */
    infer_geom_port,           /* GEOM_PORT */
    infer_geom_function_block  /* GEOM_FUNCTION_BLOCK */
};
#define lv_GEOM_TYPE_INFER_HANDLER_COUNT lv_ARRAY_SIZE(s_geom_type_infer_handlers)

/**
 * @brief 类型推断的内部递归实现
 *
 * 根据约束图中节点的几何类型（GEOM_POINT / GEOM_LINE_SEGMENT / GEOM_REGION /
 * GEOM_PORT / GEOM_FUNCTION_BLOCK）递归推断其类型区域（TypeRegion）。
 * 对于基本几何类型直接创建对应类型；对于端口类型通过 type_infer_port 从连接关系推断；
 * 对于函数块类型则递归推断所有输入/输出端口类型，并组合为乘积类型。
 *
 * @param ts        类型系统上下文，用于创建和管理类型对象，不可为 NULL
 * @param graph     约束图，提供节点和连接信息，不可为 NULL
 * @param node_id   待推断节点的 ID（在 graph 中的索引）
 * @param out_type  输出参数，成功时指向新创建的 TypeRegion 对象；
 *                  调用者需通过类型系统的释放接口管理其生命周期，不可为 NULL
 * @param depth     当前递归深度，初始调用应传 0；每次递归调用时递增
 *
 * @return true  类型推断成功，*out_type 已设置
 * @return false 参数无效、节点不存在、递归深度超过 TYPE_INFER_MAX_DEPTH 或推断失败
 *
 * @note 递归深度限制（TYPE_INFER_MAX_DEPTH）用于防止在循环依赖的约束图中无限递归，
 *       例如函数块 A 的输出连接到函数块 B 的输入，而 B 的输出又连接到 A 的输入。
 *       多个输入/输出端口会组合为乘积类型（type_create_product）。
 */
static bool type_infer_node_internal(TypeSystem *ts, ConstraintGraph *graph, int node_id, TypeRegion **out_type,
                                     int depth);

/* ============== 类型推断 ============== */

/**
 * @brief 类型推断的内部递归实现
 *
 * 根据约束图中节点的几何类型（GEOM_POINT / GEOM_LINE_SEGMENT / GEOM_REGION /
 * GEOM_PORT / GEOM_FUNCTION_BLOCK）递归推断其类型区域（TypeRegion）。
 * 对于基本几何类型直接创建对应类型；对于端口类型通过 type_infer_port 从连接关系推断；
 * 对于函数块类型则递归推断所有输入/输出端口类型，并组合为乘积类型。
 *
 * @param ts        类型系统上下文，用于创建和管理类型对象，不可为 NULL
 * @param graph     约束图，提供节点和连接信息，不可为 NULL
 * @param node_id   待推断节点的 ID（在 graph 中的索引）
 * @param out_type  输出参数，成功时指向新创建的 TypeRegion 对象；
 *                  调用者需通过类型系统的释放接口管理其生命周期，不可为 NULL
 * @param depth     当前递归深度，初始调用应传 0；每次递归调用时递增
 *
 * @return true  类型推断成功，*out_type 已设置
 * @return false 参数无效、节点不存在、递归深度超过 TYPE_INFER_MAX_DEPTH 或推断失败
 *
 * @note 递归深度限制（TYPE_INFER_MAX_DEPTH）用于防止在循环依赖的约束图中无限递归，
 *       例如函数块 A 的输出连接到函数块 B 的输入，而 B 的输出又连接到 A 的输入。
 *       多个输入/输出端口会组合为乘积类型（type_create_product）。
 */
static bool type_infer_node_internal(TypeSystem *ts, ConstraintGraph *graph, int node_id, TypeRegion **out_type,
                                     int depth) {
    if (!ts || !graph || !out_type)
        return false;

    /* 递归深度限制检查。
     * 超过最大深度时返回 false，避免在循环依赖的约束图中无限递归。 */
    int infer_max = lv_config_get_int(LV_CFG_TYPE_INFER_MAX_DEPTH, 100);
    if (depth >= infer_max) {
        return false;
    }

    GeomNode *node = graph_get_node(graph, node_id);
    if (!node)
        return false;

    if ((unsigned) node->type < lv_GEOM_TYPE_INFER_HANDLER_COUNT) {
        if (s_geom_type_infer_handlers[node->type](ts, graph, node, out_type)) {
            return true;
        }
    }

    /* ===== 新增推断规则：沿集合包含链推断 ===== */
    /*
     * 若节点 A 包含在区域 R 中，且 R 的类型已知，
     * 则 A 的类型可从 R 的元素类型推断。
     *
     * 遍历约束图中的 CONTAINMENT 约束，查找包含当前节点的区域。
     */
    {
        int constraint_indices[64];
        int found_count = graph_find_constraints_involving(graph, node_id, constraint_indices, 64);

        for (int i = 0; i < found_count; i++) {
            Constraint *c = graph_get_constraint(graph, constraint_indices[i]);
            if (!c)
                continue;

            if (c->type == CONTAINMENT && c->participant_count >= 2) {
                /* 查找包含此节点的区域 */
                int container_id = -1;
                for (int j = 0; j < c->participant_count; j++) {
                    if (c->participants[j] != node_id) {
                        GeomNode *other = graph_get_node(graph, c->participants[j]);
                        if (other && other->type == GEOM_REGION) {
                            container_id = c->participants[j];
                            break;
                        }
                    }
                }

                if (container_id > 0) {
                    /* 尝试从外部映射获取容器区域的类型 */
                    TypeRegion *container_type = type_get_node_type(ts, container_id);
                    if (container_type) {
                        /*
                         * 容器类型已知，推断节点类型为容器类型的元素类型。
                         * 对于区域类型，元素类型即区域所包含的几何体类型。
                         * 这里创建一个与容器层级兼容的类型变量作为推断结果。
                         */
                        *out_type = type_create_variable(ts, NULL);
                        if (*out_type) {
                            (*out_type)->level =
                                container_type->level > UNIVERSE_BASE ? container_type->level - 1 : UNIVERSE_BASE;
                        }
                        return (*out_type) != NULL;
                    }
                }
            }
        }
    }

    /* ===== 新增推断规则：沿函数块输入输出关系推断 ===== */
    /*
     * 若函数块 F 的输出连接到节点 N，且 F 的输出类型已知，
     * 则 N 的类型可推断为 F 的输出类型的分量。
     *
     * 遍历约束图中的 CONNECTION 约束，查找连接到此节点的端口。
     */
    {
        int constraint_indices[64];
        int found_count = graph_find_constraints_involving(graph, node_id, constraint_indices, 64);

        for (int i = 0; i < found_count; i++) {
            Constraint *c = graph_get_constraint(graph, constraint_indices[i]);
            if (!c)
                continue;

            if (c->type == CONNECTION && c->participant_count >= 2) {
                /* 查找连接到此节点的源端口 */
                int src_port_id = -1;
                for (int j = 0; j < c->participant_count; j++) {
                    if (c->participants[j] != node_id) {
                        src_port_id = c->participants[j];
                        break;
                    }
                }

                if (src_port_id > 0) {
                    GeomNode *src_port = graph_get_node(graph, src_port_id);
                    if (src_port && src_port->type == GEOM_PORT && src_port->data.port) {
                        /* 查找源端口所属的函数块 */
                        int parent_block_id = src_port->data.port->parent_block_id;
                        if (parent_block_id > 0) {
                            /* 尝试推断函数块的类型，从中提取输出类型 */
                            TypeRegion *func_type = NULL;
                            if (type_infer_node_internal(ts, graph, parent_block_id, &func_type, depth + 1) &&
                                func_type && func_type->kind == TYPE_KIND_FUNCTION) {
                                /* 函数块的输出类型即为连接目标节点的推断类型 */
                                *out_type = func_type->output_type;
                                return (*out_type) != NULL;
                            }
                        }
                    }
                }
            }
        }
    }

    /* 所有推断规则均未匹配 */
    return false;
}

/**
 * @brief 公共 API 包装器，调用内部实现并传入初始深度 0
 *
 * @param ts      类型系统指针
 * @param graph   约束图指针
 * @param node_id 节点 ID
 * @param out_type 输出参数，接收推断的类型
 * @return true 推断成功，false 推断失败
 */
bool type_infer_node(TypeSystem *ts, ConstraintGraph *graph, int node_id, TypeRegion **out_type) {
    /* 流式事件：入口 */
    if (type_system_stream_ctx != NULL) {
        stream_emit_simple(type_system_stream_ctx, STREAM_EVENT_INFO, "类型推断开始", 0);
    }

    bool result = type_infer_node_internal(ts, graph, node_id, out_type, 0);

    /* 流式事件：结果 */
    if (type_system_stream_ctx != NULL) {
        if (result && out_type && *out_type) {
            lvStrBuf sb_3 = {0};
            lv_strbuf_printf(&sb_3, "类型推断完成: %s", type_kind_to_string((*out_type)->kind));
            stream_emit_simple(type_system_stream_ctx, STREAM_EVENT_INFO, sb_3.data, 0);
            lv_strbuf_destroy(&sb_3);
        } else {
            stream_emit_simple(type_system_stream_ctx, STREAM_EVENT_WARNING, "类型推断失败", 0);
        }
    }

    return result;
}

bool type_infer_port(TypeSystem *ts, ConstraintGraph *graph, int port_id, TypeRegion **out_type) {
    /* 流式事件：入口 */
    if (type_system_stream_ctx != NULL) {
        stream_emit_simple(type_system_stream_ctx, STREAM_EVENT_INFO, "端口类型推断开始", 0);
    }

    if (!ts || !graph || !out_type)
        return false;

    GeomNode *port = graph_get_node(graph, port_id);
    if (!port || port->type != GEOM_PORT)
        return false;

    bool result;

    /* 如果端口有连接，从连接的另一端推断类型 */
    if (port->data.port && port->data.port->connected_to) {
        /* 使用内部版本并传入 depth + 1 以跟踪递归深度。
         * 注意：type_infer_port 自身不直接跟踪深度，但通过调用
         * type_infer_node_internal 间接利用深度限制机制。 */
        result = type_infer_node_internal(ts, graph, port->data.port->connected_to->id, out_type, 1);
    } else {
        /* 否则，创建类型变量 */
        *out_type = type_create_variable(ts, NULL);
        result = true;
    }

    /* 流式事件：完成 */
    if (type_system_stream_ctx != NULL && result) {
        stream_emit_simple(type_system_stream_ctx, STREAM_EVENT_INFO, "端口类型推断完成", 0);
    }

    return result;
}

/* ============== 类型变量实例化 ============== */

bool type_instantiate_variable(TypeSystem *ts, int var_id, TypeRegion *concrete_type) {
    if (!ts || !concrete_type)
        return false;

    /* 查找类型变量 */
    TypeVariable *var = NULL;
    for (int i = 0; i < ts->type_var_count; i++) {
        if (ts->type_vars[i] && ts->type_vars[i]->id == var_id) {
            var = ts->type_vars[i];
            break;
        }
    }

    if (!var)
        return false;

    var->bound_type = concrete_type;
    var->is_polymorphic = false;

    /* 流式事件：变量实例化完成 */
    if (type_system_stream_ctx != NULL) {
        lvStrBuf sb_4 = {0};
        lv_strbuf_printf(&sb_4, "类型变量实例化: var_id=%d -> %s", var_id, type_kind_to_string(concrete_type->kind));
        stream_emit_simple(type_system_stream_ctx, STREAM_EVENT_INFO, sb_4.data, 0);
        lv_strbuf_destroy(&sb_4);
    }

    return true;
}

/* ================================================================
 * 查找表：TypeKind → 类型变量替换处理函数
 * ================================================================ */

/** @brief 类型变量替换处理函数类型 */
typedef bool (*TypeKindSubstHandler)(TypeSystem *ts, TypeRegion *type, int var_id, TypeRegion *replacement,
                                     TypeRegion **out_result);

/** @brief 替换 FUNCTION 类型中的变量 */
static bool subst_kind_function(TypeSystem *ts, TypeRegion *type, int var_id, TypeRegion *replacement,
                                TypeRegion **out_result) {
    TypeRegion *new_input = NULL;
    TypeRegion *new_output = NULL;

    if (type->input_type) {
        type_substitute_variable(ts, type->input_type, var_id, replacement, &new_input);
    }
    if (type->output_type) {
        type_substitute_variable(ts, type->output_type, var_id, replacement, &new_output);
    }

    *out_result = type_create_function(ts, new_input, new_output);
    return true;
}

/** @brief 替换 PRODUCT 类型中的变量 */
static bool subst_kind_product(TypeSystem *ts, TypeRegion *type, int var_id, TypeRegion *replacement,
                               TypeRegion **out_result) {
    TypeRegion *new_left = NULL;
    TypeRegion *new_right = NULL;

    if (type->left_type) {
        type_substitute_variable(ts, type->left_type, var_id, replacement, &new_left);
    }
    if (type->right_type) {
        type_substitute_variable(ts, type->right_type, var_id, replacement, &new_right);
    }

    *out_result = type_create_product(ts, new_left, new_right);
    return true;
}

/** @brief 替换 SUM 类型中的变量 */
static bool subst_kind_sum(TypeSystem *ts, TypeRegion *type, int var_id, TypeRegion *replacement,
                           TypeRegion **out_result) {
    TypeRegion *new_first = NULL;
    TypeRegion *new_second = NULL;

    if (type->first_type) {
        type_substitute_variable(ts, type->first_type, var_id, replacement, &new_first);
    }
    if (type->second_type) {
        type_substitute_variable(ts, type->second_type, var_id, replacement, &new_second);
    }

    *out_result = type_create_sum(ts, new_first, new_second);
    return true;
}

/** @brief 替换 DEPENDENT 类型中的变量 */
static bool subst_kind_dependent(TypeSystem *ts, TypeRegion *type, int var_id, TypeRegion *replacement,
                                 TypeRegion **out_result) {
    TypeRegion *new_body = NULL;

    if (type->body_type) {
        type_substitute_variable(ts, type->body_type, var_id, replacement, &new_body);
    }

    *out_result = type_create_dependent(ts, type->param_node_id, new_body ? new_body : type->body_type);
    return true;
}

/** @brief 替换 REGION 类型中的变量 */
static bool subst_kind_region(TypeSystem *ts, TypeRegion *type, int var_id, TypeRegion *replacement,
                              TypeRegion **out_result) {
    if (type->aliased_type) {
        TypeRegion *new_aliased = NULL;
        if (type_substitute_variable(ts, type->aliased_type, var_id, replacement, &new_aliased)) {
            if (new_aliased != type->aliased_type) {
                TypeRegion *new_region = type_create_region(ts, type->contained_node_ids, type->contained_count);
                if (new_region) {
                    new_region->aliased_type = new_aliased;
                    if (type->alias_name) {
                        new_region->alias_name = lv_strdup(type->alias_name);
                    }
                    *out_result = new_region;
                    return true;
                }
            }
        }
    }
    *out_result = type;
    return true;
}

/** @brief 无替换操作的默认处理：原样返回 */
static bool subst_kind_identity(TypeSystem *ts, TypeRegion *type, int var_id, TypeRegion *replacement,
                                TypeRegion **out_result) {
    (void) ts;
    (void) var_id;
    (void) replacement;
    *out_result = type;
    return true;
}

/**
 * @brief TypeKind 替换处理函数查找表（按枚举值升序）
 *
 * 索引：TYPE_KIND_POINT=0, TYPE_KIND_LINE_SEGMENT=1, TYPE_KIND_REGION=2,
 *       TYPE_KIND_FUNCTION=3, TYPE_KIND_PRODUCT=4, TYPE_KIND_SUM=5,
 *       TYPE_KIND_VARIABLE=6, TYPE_KIND_DEPENDENT=7, TYPE_KIND_BOTTOM=8,
 *       TYPE_KIND_PREDICATE_SUBTYPE=9
 */
static const TypeKindSubstHandler s_type_kind_subst_handlers[] = {
    subst_kind_identity,   /* TYPE_KIND_POINT */
    subst_kind_identity,   /* TYPE_KIND_LINE_SEGMENT */
    subst_kind_region,     /* TYPE_KIND_REGION */
    subst_kind_function,   /* TYPE_KIND_FUNCTION */
    subst_kind_product,    /* TYPE_KIND_PRODUCT */
    subst_kind_sum,        /* TYPE_KIND_SUM */
    subst_kind_identity,   /* TYPE_KIND_VARIABLE */
    subst_kind_dependent,  /* TYPE_KIND_DEPENDENT */
    subst_kind_identity,   /* TYPE_KIND_BOTTOM */
    subst_kind_identity,   /* TYPE_KIND_PREDICATE_SUBTYPE */
};
#define lv_TYPE_KIND_SUBST_HANDLER_COUNT lv_ARRAY_SIZE(s_type_kind_subst_handlers)

bool type_substitute_variable(TypeSystem *ts, TypeRegion *type, int var_id, TypeRegion *replacement,
                              TypeRegion **out_result) {
    if (!ts || !type || !replacement || !out_result)
        return false;

    /* 引用语义说明。
     * 当 type 是目标类型变量时，此函数直接将 replacement 指针赋值给
     * *out_result，而不创建 replacement 的深拷贝。这意味着：
     * 1. 调用者不能在返回后释放 replacement，否则 out_result 将成为悬垂指针。
     * 2. 多次调用可能返回指向同一 TypeRegion 的指针，修改一处会影响所有引用。
     * 3. 调用者需要自行管理 replacement 的生命周期。
     * 对于复合类型（FUNCTION、PRODUCT、SUM），此函数会创建新的 TypeRegion
     * 容器，但其中的子类型仍可能共享引用。 */

    /* 如果是类型变量 */
    if (type->kind == TYPE_KIND_VARIABLE && type->variable_id == var_id) {
        *out_result = replacement;
        return true;
    }

    /* 通过查找表递归替换 */
    if ((unsigned) type->kind < lv_TYPE_KIND_SUBST_HANDLER_COUNT) {
        return s_type_kind_subst_handlers[type->kind](ts, type, var_id, replacement, out_result);
    }
    *out_result = type;
    return true;
}
