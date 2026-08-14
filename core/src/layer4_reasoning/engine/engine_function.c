/**
 * @file engine_function.c
 * @brief 引擎函数块操作（从 engine.c 拆分）
 *
 * @details 负责函数块打包（pack）、实例化（instantiate）与合一检查（unify）。
 *          包含端口节点命名空间深度调整等内部辅助逻辑。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "lv/engine.h"

#include <stdio.h>
#include <string.h>

#include "lv/constraint_graph.h"
#include "lv/func_block.h"
#include "lv/unify.h"

#include "lv/debug.h"
#include "engine_internal.h"
#include "lv/lv_internal.h"

/** 全局画布的上下文深度（用于引擎初始化时的默认值） */
#define ENGINE_GLOBAL_CANVAS_DEPTH 0

/**
 * @brief 更新端口节点的命名空间深度
 * @param n 目标节点
 * @param new_func_block_id 新函数块ID
 * @param context_depth 上下文深度
 * @param is_input 是否为输入端口（影响 is_formal_param 设置）
 */
static void update_port_namespace_depth(GeomNode *n, int new_func_block_id, int context_depth, bool is_input) {
    if (n && n->type == GEOM_PORT && n->data.port != NULL) {
        n->data.port->parent_block_id = new_func_block_id;
        n->data.port->is_formal_param = is_input;
        n->data.port->namespace_depth = n->data.port->namespace_depth - context_depth + 1;
        n->parent_block_id = new_func_block_id;
        n->namespace_depth = n->namespace_depth - context_depth + 1;
    }
}

/**
 * engine_pack_function - 将一组内部节点和端口打包为函数块。
 *
 * 将指定的内部节点、输入端口和输出端口封装为一个函数块(FunctionBlock)，
 * 并重新调整命名空间深度(namespace_depth)和父块ID(parent_block_id)。
 *
 * @param engine           引擎实例
 * @param internal_node_ids 内部节点ID数组
 * @param internal_count   内部节点数量
 * @param input_port_ids   输入端口ID数组
 * @param input_count      输入端口数量
 * @param output_port_ids  输出端口ID数组
 * @param output_count     输出端口数量
 * @param out_func_block_id [可选] 输出参数，用于接收新创建的函数块ID。
 *                         若为 NULL 则跳过赋值。
 * @return true 成功，false 失败（错误信息存入 last_error）
 */
bool engine_pack_function(lvEngine *engine, const int *internal_node_ids, int internal_count, const int *input_port_ids,
                          int input_count, const int *output_port_ids, int output_count, int *out_func_block_id) {
    if (!engine || !engine->main_graph || (internal_count > 0 && !internal_node_ids) ||
        (input_count > 0 && !input_port_ids) || (output_count > 0 && !output_port_ids)) {
        engine_set_error(engine, ENGINE_STATUS_INVALID_ARGUMENT, "引擎或主图为空");
        lv_RETURN_ERROR_BOOL(lv_ERROR_NULL_POINTER, "engine_pack_function: NULL parameter");
    }
    for (int i = 0; i < internal_count; i++) {
        GeomNode *n = graph_get_node(engine->main_graph, internal_node_ids[i]);
        if (!n) {
            engine_set_error(engine, ENGINE_STATUS_INVALID_STATE, "打包函数块失败: 内部节点 %d 不存在",
                             internal_node_ids[i]);
            return false;
        }
    }
    for (int i = 0; i < input_count; i++) {
        GeomNode *n = graph_get_node(engine->main_graph, input_port_ids[i]);
        if (!n || n->type != GEOM_PORT) {
            engine_set_error(engine, ENGINE_STATUS_INVALID_STATE, "打包函数块失败: 输入端口 %d 不存在或不是端口类型",
                             input_port_ids[i]);
            return false;
        }
    }
    for (int i = 0; i < output_count; i++) {
        GeomNode *n = graph_get_node(engine->main_graph, output_port_ids[i]);
        if (!n || n->type != GEOM_PORT) {
            engine_set_error(engine, ENGINE_STATUS_INVALID_STATE, "打包函数块失败: 输出端口 %d 不存在或不是端口类型",
                             output_port_ids[i]);
            return false;
        }
    }
    /* v3.4.1 改进：使用 graph_get_last_added_node_id() 安全获取新节点 ID，
     * 避免依赖 next_node_id - 1 的推断方式，消除悬空指针风险。
     * graph_add_function_block 成功后会更新最后添加节点 ID。 */
    AddNodeResult result = graph_add_function_block(engine->main_graph, internal_node_ids, internal_count,
                                                    input_port_ids, input_count, output_port_ids, output_count);
    if (result != ADD_NODE_OK) {
        engine_set_error(engine, ENGINE_STATUS_INVALID_STATE, "创建函数块失败（图操作返回错误: %d）", result);
        return false;
    }

    /* 安全获取新创建的函数块 ID */
    int new_func_block_id = graph_get_last_added_node_id(engine->main_graph);

    /* 验证获取的 ID 有效 */
    if (new_func_block_id < 0) {
        /* lv_LOG_ERROR("engine_add_function_block: 无法获取有效的函数块 ID"); */
        engine_set_error(engine, ENGINE_STATUS_ERROR_INTERNAL, "无法获取有效的函数块 ID");
        return false;
    }

    /* 二次验证：确保 ID 对应的节点存在且类型正确 */
    GeomNode *new_fb_node = graph_get_node(engine->main_graph, new_func_block_id);
    if (!new_fb_node || new_fb_node->type != GEOM_FUNCTION_BLOCK) {
        lv_LOG_ERROR("engine_add_function_block: ID=%d 对应的节点不存在或类型不是函数块", new_func_block_id);
        engine_set_error(engine, ENGINE_STATUS_ERROR_INTERNAL, "函数块节点验证失败");
        return false;
    }
    if (out_func_block_id) {
        *out_func_block_id = new_func_block_id;
    }
    /* context_depth is the current canvas depth (0 for global canvas) */
    int context_depth = ENGINE_GLOBAL_CANVAS_DEPTH;
    for (int i = 0; i < internal_count; i++) {
        GeomNode *n = graph_get_node(engine->main_graph, internal_node_ids[i]);
        if (n) {
            n->parent_block_id = new_func_block_id;
            /* 重新设定 namespace_depth：new_depth = original_depth - context_depth + 1 */
            n->namespace_depth = n->namespace_depth - context_depth + 1;
        }
    }
    for (int i = 0; i < input_count; i++) {
        GeomNode *n = graph_get_node(engine->main_graph, input_port_ids[i]);
        update_port_namespace_depth(n, new_func_block_id, context_depth, true);
    }
    for (int i = 0; i < output_count; i++) {
        GeomNode *n = graph_get_node(engine->main_graph, output_port_ids[i]);
        update_port_namespace_depth(n, new_func_block_id, context_depth, false);
    }
    return true;
}

int *engine_instantiate_function(lvEngine *engine, int func_block_id, const int *arg_mappings, int arg_count,
                                 int *out_result_count) {
    if (!out_result_count) {
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "out_result_count 不能为 NULL");
    }
    if (!engine || !engine->main_graph) {
        *out_result_count = 0;
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "引擎或主图为空");
    }
    *out_result_count = 0;

    GeomNode *func_block = graph_get_node(engine->main_graph, func_block_id);
    if (!func_block || func_block->type != GEOM_FUNCTION_BLOCK) {
        lv_RETURN_ERROR_NULL(lv_ERROR_INVALID_PARAM, "函数块 %d 不存在或类型不是函数块", func_block_id);
    }

    /*
     * 构建独立的 FuncBlock 描述符，从 GeomNode 中提取函数块数据，
     * 以便调用 func_block_instantiate()，实现完整的 beta-归约和变量捕获解析
     * （设计文档 Section 3.3 中的 A/B/C 三种情况）。
     */
    FuncBlock *fb = func_block_create(func_block_id);
    if (!fb) {
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "func_block_create 分配失败");
    }

    /* 拷贝内部节点ID */
    if (func_block->data.func_block.internal_node_count > 0) {
        int ic = func_block->data.func_block.internal_node_count;
        int *ids = lv_calloc((size_t) ic, sizeof(int));
        if (!ids) {
            func_block_destroy(fb);
            lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "内部节点ID数组分配失败");
        }
        for (int i = 0; i < ic; i++) {
            if (!func_block->data.func_block.internal_nodes[i]) {
                func_block_destroy(fb);
                lv_free((void **) &ids);
                lv_RETURN_ERROR_NULL(lv_ERROR_INVALID_STATE, "内部节点 %d 为空", i);
            }
            ids[i] = func_block->data.func_block.internal_nodes[i]->id;
        }
        func_block_set_internal_nodes(fb, ids, ic);
        lv_free((void **) &ids);
    }

    /* 拷贝输入端口ID */
    if (func_block->data.func_block.input_count > 0) {
        int ic = func_block->data.func_block.input_count;
        func_block_set_input_ports(fb, func_block->data.func_block.input_port_ids, ic);
    }

    /* 拷贝输出端口ID */
    if (func_block->data.func_block.output_count > 0) {
        int oc = func_block->data.func_block.output_count;
        func_block_set_output_ports(fb, func_block->data.func_block.output_port_ids, oc);
    }

    /* 调用 func_block_instantiate：完整 beta-归约（A/B/C 三种情况） */
    int *new_node_ids = NULL;
    int new_node_count = 0;
    InstantiateResult inst_result =
        func_block_instantiate(fb, engine->main_graph, (int *) arg_mappings, arg_count, &new_node_ids, &new_node_count);

    func_block_destroy(fb);

    if (inst_result != INSTANTIATE_OK) {
        lv_RETURN_ERROR_NULL(lv_ERROR_INTERNAL,
                         "engine_instantiate_function: instantiation failed (code %d)", inst_result);
    }

    *out_result_count = new_node_count;
    return new_node_ids;
}

UnifyStatus engine_unify(lvEngine *engine, ConstraintGraph *construction, ConstraintGraph *proposition) {
    /* 参数校验：任一参数为 NULL 视为调用方错误 */
    if (!engine || !construction || !proposition) {
        if (engine) {
            engine->last_unify_status = lv_ERROR_INVALID_PARAM;
            engine_set_error(engine, ENGINE_STATUS_INVALID_ARGUMENT,
                             "engine_unify: 空指针参数 (engine=%p, construction=%p, proposition=%p)", (void *) engine,
                             (void *) construction, (void *) proposition);
        }
        lv_set_error(lv_ERROR_INVALID_PARAM, "engine_unify: 空指针参数");
        return UNIFY_STATUS_FAILED;
    }

    /* 执行合一操作：委托给合一检查器模块 */
    UnifyStatus status = unify_construction_with_proposition(construction, proposition);

    /* 同步错误状态到引擎实例 */
    engine->last_unify_status = status;

    if (status == UNIFY_STATUS_OK) {
        /* 合一成功：构造图满足命题模式 */
        engine_set_error(engine, ENGINE_STATUS_OK, "");
    } else {
        /* 合一失败：中文文案统一取自 unify 公共模块（unify_status_reason_zh） */
        const char *reason = unify_status_reason_zh(status);
        engine_set_error(engine, ENGINE_STATUS_CONSTRAINT_CONFLICT, "合一失败 [状态码=%d]: %s", (int) status, reason);
        lv_set_error(lv_ERROR_UNIFY_FAILED, reason);
    }

    return status;
}
