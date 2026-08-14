/**
 * @file graph_node_emit.c
 * @brief 集中化图编辑流式事件发射（由 graph_node_alloc.c 拆分子模块）
 *
 * @details graph_emit_node_added/constraint_added/node_removed/
 *          constraint_removed：图编辑事件的流式通知。
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
#include "lv/determinism_state.h"
#include "lv/memory_pool.h"
#include "lv/symbolic_coord.h"
#include "lv/geo_utils.h" /* geo_point_on_segment / geo_segments_intersect / GEO_EPSILON */

#include "lv/config.h"
#include "lv/context.h"
#include "lv/debug.h"
#include "lv/error_codes.h"
#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/stream.h"
#include "lv/lv_json.h"
#include "lv/lv_strbuf.h"
#include "lv/lv_xmacro.h"

#include "graph_node_internal.h"

/* ========================================================================
 * 集中化图编辑流式事件发射（graph_node_internal.h 声明）
 *
 * 收敛自 graph_node_alloc.c / graph_node_conflict.c / graph_node_stub.c /
 * graph_index.c 各内联发射点；消息文案与各原发射点逐字一致，
 * step_number 与消息模板选择（use_generic_message）由调用点按原语义传入。
 * ======================================================================== */

void graph_emit_node_added(ConstraintGraph *graph, GeomNode *node, int step_number, bool use_generic_message) {
    (void) graph;
    if (!graph_stream_ctx || !node)
        return;
    lvStrBuf sb = {0};
    if (use_generic_message) {
        /* 通用模板：graph_add_node_with_id 反序列化路径 */
        lv_strbuf_printf(&sb, "添加节点 #%d (类型: %s)", node->id, lv_geom_type_name(node->type));
    } else {
        /* 具体模板：graph_add_point / graph_add_region / graph_add_circle /
         * graph_add_port / graph_add_function_block 各路径，文案与集中前逐字一致 */
        switch (node->type) {
        case GEOM_POINT:
            lv_strbuf_printf(&sb, "添加点节点: id=%d", node->id);
            break;
        case GEOM_REGION:
            lv_strbuf_printf(&sb, "添加区域节点: id=%d, segments=%d", node->id, node->data.region.segment_count);
            break;
        case GEOM_CIRCLE:
            lv_strbuf_printf(&sb, "添加圆节点: id=%d, center=%d, radius=%d", node->id,
                             node->data.circle.center_node_id, node->data.circle.radius_node_id);
            break;
        case GEOM_PORT:
            lv_strbuf_printf(&sb, "添加端口节点: id=%d, type=%d, depth=%d, parent=%d", node->id,
                             (int) node->data.port->type, node->namespace_depth, node->parent_block_id);
            break;
        case GEOM_FUNCTION_BLOCK:
            lv_strbuf_printf(&sb, "添加函数块节点: id=%d, internal=%d, in=%d, out=%d", node->id,
                             node->data.func_block.internal_node_count, node->data.func_block.input_count,
                             node->data.func_block.output_count);
            break;
        default:
            lv_strbuf_printf(&sb, "添加节点 #%d (类型: %d)", node->id, (int) node->type);
            break;
        }
    }
    stream_emit_simple(graph_stream_ctx, STREAM_EVENT_NODE_ADDED, sb.data, step_number);
    lv_strbuf_destroy(&sb);
}

void graph_emit_constraint_added(ConstraintGraph *graph, Constraint *con, int step_number, bool use_generic_message) {
    (void) graph;
    if (!graph_stream_ctx || !con)
        return;
    lvStrBuf sb = {0};
    if (use_generic_message) {
        /* 通用模板：graph_add_constraint_with_id 反序列化路径 */
        const char *cname = lv_constraint_type_name(con->type);
        if (!cname)
            cname = "UNKNOWN";
        lv_strbuf_printf(&sb, "添加约束 #%d (类型: %s, 参与者: %d个)", con->id, cname, con->participant_count);
    } else {
        /* 具体模板：graph_add_incidence 路径 */
        lv_strbuf_printf(&sb, "添加关联约束: id=%d, point=%d, target=%d", con->id, con->participants[0],
                         con->participants[1]);
    }
    stream_emit_simple(graph_stream_ctx, STREAM_EVENT_CONSTRAINT_ADDED, sb.data, step_number);
    lv_strbuf_destroy(&sb);
}

void graph_emit_node_removed(ConstraintGraph *graph, int node_id) {
    (void) graph;
    if (!graph_stream_ctx)
        return;
    lvStrBuf sb = {0};
    lv_strbuf_printf(&sb, "移除节点: id=%d", node_id);
    stream_emit_node_event(graph_stream_ctx, STREAM_EVENT_INFO, node_id, sb.data, 0);
    lv_strbuf_destroy(&sb);
}

void graph_emit_constraint_removed(ConstraintGraph *graph, int constraint_id, bool deactivated) {
    (void) graph;
    if (!graph_stream_ctx)
        return;
    lvStrBuf sb = {0};
    if (deactivated)
        lv_strbuf_printf(&sb, "废弃约束: id=%d (已停用，保留审计数据)", constraint_id);
    else
        lv_strbuf_printf(&sb, "移除约束: id=%d", constraint_id);
    stream_emit_constraint_event(graph_stream_ctx, STREAM_EVENT_INFO, constraint_id, sb.data, 0);
    lv_strbuf_destroy(&sb);
}
