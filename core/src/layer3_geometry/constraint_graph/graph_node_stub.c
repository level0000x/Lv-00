/**
 * @file graph_node_stub.c
 * @brief ConstraintGraph 节点与约束生命周期管理 —— 区域/端口/函数块节点存根实现
 *
 * @details 由 graph_node.c 按功能域拆分而来。
 *          共享内部函数声明见 graph_node_internal.h。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include <assert.h>
#include <float.h>
#include <gmp.h>
#include <math.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/constraint_graph.h"
#include "lv/solver.h"
#include "lv/symbolic_coord.h"

#include "config.h"
#include "context.h"
#include "debug.h"
#include "error_codes.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "stream.h"
#include "stream_context_util.h"
#include "lv/lv_strbuf.h"

#include "graph_node_internal.h"

/* ===========================================================================
 * 存根实现：graph_add_region / graph_add_function_block
 *
 * graph_add_port 已完全实现（被 27+ 处调用），此处仅保留声明兼容。
 * =========================================================================== */

/**
 * @brief 向约束图添加区域节点（存根实现）
 *
 * @param graph                约束图
 * @param boundary_segment_ids 边界线段 ID 数组
 * @param segment_count        边界线段数量
 * @return 添加结果状态
 */
AddNodeResult graph_add_region(ConstraintGraph *graph, const int *boundary_segment_ids, int segment_count) {
    if (!graph || segment_count <= 0)
        return ADD_NODE_CONFLICT;
    GeomNode *node = graph_alloc_node(graph, GEOM_REGION);
    if (!node)
        return ADD_NODE_CONFLICT;
    /* 存储边界线段引用 */
    node->coord_count = 0;
    node->symbolic_coords = NULL;
    node->data.region.boundary_segments = (GeomNode **) lv_calloc((size_t) segment_count, sizeof(GeomNode *));
    if (!node->data.region.boundary_segments)
        return ADD_NODE_CONFLICT;
    node->data.region.segment_count = segment_count;
    for (int i = 0; i < segment_count; i++) {
        node->data.region.boundary_segments[i] = graph_get_node(graph, boundary_segment_ids[i]);
    }
    if (graph_stream_ctx) {
        lvStrBuf sb_4 = {0};
        lv_strbuf_printf(&sb_4, "添加区域节点: id=%d, segments=%d", node->id, segment_count);
        stream_emit_simple(graph_stream_ctx, STREAM_EVENT_NODE_ADDED, sb_4.data, 0);
        lv_strbuf_destroy(&sb_4);
    }
    return ADD_NODE_OK;
}

/**
 * @brief 向约束图添加圆节点
 *
 * 创建 GEOM_CIRCLE 节点并设置 data.circle 的圆心/半径端点节点 ID，
 * 与反序列化路径（graph_serialize.c 的 GEOM_CIRCLE 分支）创建的
 * circle 语义保持一致。
 *
 * @param graph          约束图
 * @param center_node_id 圆心节点 ID
 * @param radius_node_id 半径端点节点 ID（圆心到此点的距离为半径）
 * @return 添加结果状态
 */
AddNodeResult graph_add_circle(ConstraintGraph *graph, int center_node_id, int radius_node_id) {
    if (!graph || center_node_id < 0 || radius_node_id < 0)
        return ADD_NODE_CONFLICT;
    GeomNode *node = graph_alloc_node(graph, GEOM_CIRCLE);
    if (!node)
        return ADD_NODE_CONFLICT;
    node->coord_count = 0;
    node->symbolic_coords = NULL;
    /* 与反序列化创建的 circle 语义一致：设置圆心与半径端点节点 ID */
    node->data.circle.center_node_id = center_node_id;
    node->data.circle.radius_node_id = radius_node_id;
    if (graph_stream_ctx) {
        lvStrBuf sb_7 = {0};
        lv_strbuf_printf(&sb_7, "添加圆节点: id=%d, center=%d, radius=%d", node->id, center_node_id, radius_node_id);
        stream_emit_simple(graph_stream_ctx, STREAM_EVENT_NODE_ADDED, sb_7.data, 0);
        lv_strbuf_destroy(&sb_7);
    }
    return ADD_NODE_OK;
}

/**
 * @brief 向约束图添加端口节点（存根实现）
 *
 * @param graph            约束图
 * @param type             端口方向（输入/输出）
 * @param namespace_depth  命名空间深度
 * @param parent_block_id  父函数块 ID
 * @return 添加结果状态
 */
AddNodeResult graph_add_port(ConstraintGraph *graph, PortType type, int namespace_depth, int parent_block_id) {
    if (!graph)
        return ADD_NODE_CONFLICT;
    GeomNode *node = graph_alloc_node(graph, GEOM_PORT);
    if (!node)
        return ADD_NODE_CONFLICT;
    node->namespace_depth = namespace_depth;
    node->parent_block_id = parent_block_id;
    node->coord_count = 0;
    node->symbolic_coords = NULL;
    /* 设置端口类型 */
    if (!node->data.port) {
        node->data.port = lv_calloc(1, sizeof(Port));
    }
    if (node->data.port) {
        node->data.port->type = type;
        /* 同步 Port 层面的 namespace_depth/parent_block_id
         * 避免 GeomNode 与 Port 双存储不一致 */
        node->data.port->namespace_depth = namespace_depth;
        node->data.port->parent_block_id = parent_block_id;
        /* is_formal_param 默认为 false（calloc 零初始化），
         * 后续由调用方通过 update_port_namespace_depth 或打包函数设置 */
    }
    if (graph_stream_ctx) {
        lvStrBuf sb_5 = {0};
        lv_strbuf_printf(&sb_5, "添加端口节点: id=%d, type=%d, depth=%d, parent=%d", node->id, (int) type,
                 namespace_depth, parent_block_id);
        stream_emit_simple(graph_stream_ctx, STREAM_EVENT_NODE_ADDED, sb_5.data, 0);
        lv_strbuf_destroy(&sb_5);
    }
    return ADD_NODE_OK;
}

/**
 * @brief 向约束图添加函数块节点（存根实现）
 *
 * @param graph             约束图
 * @param internal_node_ids 内部节点 ID 数组
 * @param internal_count    内部节点数量
 * @param input_port_ids    输入端口 ID 数组
 * @param input_count       输入端口数量
 * @param output_port_ids   输出端口 ID 数组
 * @param output_count      输出端口数量
 * @return 添加结果状态
 */
AddNodeResult graph_add_function_block(ConstraintGraph *graph, const int *internal_node_ids, int internal_count,
                                       const int *input_port_ids, int input_count, const int *output_port_ids,
                                       int output_count) {
    if (!graph)
        return ADD_NODE_CONFLICT;
    GeomNode *node = graph_alloc_node(graph, GEOM_FUNCTION_BLOCK);
    if (!node)
        return ADD_NODE_CONFLICT;
    node->coord_count = 0;
    node->symbolic_coords = NULL;
    /* 初始化函数块数据 */
    if (!node->data.func_block.internal_nodes && internal_count > 0 && internal_node_ids) {
        node->data.func_block.internal_nodes = lv_malloc((size_t) internal_count * sizeof(GeomNode *));
        if (node->data.func_block.internal_nodes) {
            memset(node->data.func_block.internal_nodes, 0, (size_t) internal_count * sizeof(GeomNode *));
        }
        node->data.func_block.internal_node_count = internal_count;
    }
    if (input_count > 0 && input_port_ids) {
        node->data.func_block.input_port_ids = lv_malloc((size_t) input_count * sizeof(int));
        if (node->data.func_block.input_port_ids) {
            memcpy(node->data.func_block.input_port_ids, input_port_ids, (size_t) input_count * sizeof(int));
        }
        node->data.func_block.input_count = input_count;
    }
    if (output_count > 0 && output_port_ids) {
        node->data.func_block.output_port_ids = lv_malloc((size_t) output_count * sizeof(int));
        if (node->data.func_block.output_port_ids) {
            memcpy(node->data.func_block.output_port_ids, output_port_ids, (size_t) output_count * sizeof(int));
        }
        node->data.func_block.output_count = output_count;
    }
    (void) internal_node_ids; /* 已在上方处理 */
    if (graph_stream_ctx) {
        lvStrBuf sb_6 = {0};
        lv_strbuf_printf(&sb_6, "添加函数块节点: id=%d, internal=%d, in=%d, out=%d", node->id, internal_count,
                 input_count, output_count);
        stream_emit_simple(graph_stream_ctx, STREAM_EVENT_NODE_ADDED, sb_6.data, 0);
        lv_strbuf_destroy(&sb_6);
    }
    return ADD_NODE_OK;
}

/**
 * @brief 获取最近一次通过 graph_add_* 成功添加的节点 ID
 *
 * @param graph 约束图指针
 * @return 最近添加的节点 ID；如果尚未添加任何节点则返回 -1
 */
int graph_get_last_added_node_id(const ConstraintGraph *graph) {
    if (!graph || graph->node_count <= 0)
        lv_RETURN_ERROR(lv_ERROR_INVALID_STATE, "graph_get_last_added_node_id: graph is NULL or empty");
    return graph->nodes[graph->node_count - 1]->id;
}

/**
 * @brief 检测跨函数块边界的约束（存根实现）
 *
 * @param graph             约束图
 * @param internal_node_ids 内部节点 ID 数组
 * @param internal_count    内部节点数量
 * @param port_ids          端口 ID 数组（可为 NULL）
 * @param port_count        端口数量
 * @param out_count         输出：找到的跨边界约束数量
 * @return 跨边界约束数组（调用者需 free），无跨边界约束返回 NULL
 */
CrossBoundaryConstraint *find_cross_boundary_constraints(ConstraintGraph *graph, const int *internal_node_ids,
                                                         int internal_count, const int *port_ids, int port_count,
                                                         int *out_count) {
    if (out_count)
        *out_count = 0;
    if (!graph || !internal_node_ids || internal_count <= 0)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "find_cross_boundary_constraints: NULL parameter or empty internal");

    /* 存根实现：遍历所有约束，检查是否引用了内部节点和外部节点 */
    int capacity = 16;
    int found = 0;
    CrossBoundaryConstraint *results = lv_malloc((size_t) capacity * sizeof(CrossBoundaryConstraint));
    if (!results)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "find_cross_boundary_constraints: malloc results failed");

    for (int c = 0; c < graph->constraint_count; c++) {
        Constraint *con = graph->constraints[c];
        if (!con || !con->is_active)
            continue;
        /* 检查约束涉及的参与者节点是否跨越边界 */
        bool has_internal = false;
        bool has_external = false;
        for (int n = 0; n < con->participant_count && n < 2; n++) {
            bool is_internal = false;
            for (int k = 0; k < internal_count; k++) {
                if (con->participants[n] == internal_node_ids[k]) {
                    is_internal = true;
                    break;
                }
            }
            if (is_internal)
                has_internal = true;
            else
                has_external = true;
        }
        if (has_internal && has_external) {
            if (!lv_ensure_capacity((void **) &results, found, &capacity, sizeof(CrossBoundaryConstraint), 0)) {
                lv_free((void **) &results);
                lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "find_cross_boundary_constraints: realloc results failed");
            }
            results[found].constraint_id = con->id;
            results[found].type = con->type;
            results[found].node_ids[0] = con->participants[0];
            results[found].node_ids[1] = con->participant_count > 1 ? con->participants[1] : -1;
            results[found].node_count = con->participant_count > 2 ? 2 : con->participant_count;
            found++;
        }
    }

    (void) port_ids;
    (void) port_count;

    if (found == 0) {
        lv_free((void **) &results);
        if (out_count)
            *out_count = 0;
        lv_RETURN_ERROR_NULL(lv_ERROR_NOT_FOUND, "find_cross_boundary_constraints: no cross-boundary constraints found");
    }
    if (out_count)
        *out_count = found;
    return results;
}
