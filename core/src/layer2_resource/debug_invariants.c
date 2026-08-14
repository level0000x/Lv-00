/**
 * @file debug_invariants.c
 * @brief port invariant assertions
 * @details Split from debug.c
 */

#include "lv/lv_file.h"
#include "lv/lv_platform.h"
#include "lv/lv_thread.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "lv/engine.h"
#include "lv/lv_json.h"

#include "lv/context.h"
#include "lv/debug.h"
#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/stream.h"
#include "lv/stream_context_util.h"
#include "lv/type_system.h"
#include "lv/lv_xmacro.h"
#include "lv/lv_strbuf.h"
#include "debug_internal.h"

/* ================================================================== */
/*  端口不变量断言（完整版）实现                                        */
/* ================================================================== */

/* 类型等价结果 → 端口兼容判定（0=兼容，1=不兼容；ERROR=-1 哨兵 → 错误路径） */
static const int kEquivCompat[] = {
    [TYPE_EQUIV_OK]                = 0,  /* 兼容 */
    [TYPE_EQUIV_NOT_EQUIV]         = 1,  /* 不兼容 */
    [TYPE_EQUIV_UNKNOWN]           = 0,  /* 无法证明不兼容，视为兼容 */
    [TYPE_EQUIV_NEEDS_INTERACTION] = 0,  /* 无法证明不兼容，视为兼容 */
    [TYPE_EQUIV_ERROR]             = -1, /* 检查出错 */
};

/**
 * @brief 端口与其连接节点之间的深度类型兼容性检查。
 *
 * 使用类型系统的 type_check_equivalence() 执行深度结构类型等价检查。
 * 当类型系统不可用（NULL）时，回退到基本指针比较。
 *
 * @param graph          约束图
 * @param port_node_id   端口节点 ID
 * @param connected_node_id  连接节点的 ID
 * @param ts             类型系统（可为 NULL，用于回退）
 * @return 0 = 兼容, 1 = 不兼容, -1 = 错误
 */
static int check_port_type_deep_compatible(const ConstraintGraph *graph, int port_node_id, int connected_node_id,
                                           TypeSystem *ts) {
    if (!graph)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "graph 为空");

    GeomNode *port_node = graph_get_node((ConstraintGraph *) graph, port_node_id);
    if (!port_node || port_node->type != GEOM_PORT || !port_node->data.port) {
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "端口节点无效");
    }

    GeomNode *connected_node = graph_get_node((ConstraintGraph *) graph, connected_node_id);
    if (!connected_node) {
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "连接节点为空");
    }

    Port *port = port_node->data.port;
    TypeRegion *port_type = port->type_region;

    /* 获取连接节点的类型区域 */
    TypeRegion *connected_type = NULL;
    if (connected_node->type == GEOM_PORT && connected_node->data.port) {
        connected_type = connected_node->data.port->type_region;
    }

    /* 如果任一方没有类型信息，返回 0（无类型信息 = 默认兼容） */
    if (!port_type || !connected_type) {
        return 0;
    }

    /* 如果类型系统不可用，回退到基本指针比较。
     * 指针不同意味着类型区域不是同一个对象，视为不兼容。 */
    if (!ts) {
        return (port_type == connected_type) ? 0 : 1;
    }

    /* 双方都有类型且类型系统可用：使用深度等价检查 */
    TypeEquivResult equiv = type_check_equivalence(ts, port_type, connected_type, true);

    /* 类型等价结果 → 端口兼容判定（0=兼容，1=不兼容；ERROR/越界走错误路径） */
    int compat = (unsigned) equiv < sizeof(kEquivCompat) / sizeof(kEquivCompat[0]) ? kEquivCompat[equiv] : -1;
    if (compat < 0) {
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "类型等价检查出错");
    }
    return compat;
}

PortInvariantResult *debug_check_port_invariants(const ConstraintGraph *graph) {
    PortInvariantResult *result = (PortInvariantResult *) lv_calloc(1, sizeof(PortInvariantResult));
    if (!result)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "分配 PortInvariantResult 失败");

    if (!graph) {
        result->all_valid = true;
        result->total_ports = 0;
        return result;
    }

    /* 第一遍：统计端口数量 */
    int total_ports = 0;
    for (int i = 0; i < graph->node_count; i++) {
        if (graph->nodes[i] && graph->nodes[i]->type == GEOM_PORT) {
            total_ports++;
        }
    }

    result->total_ports = total_ports;
    result->invalid_port_ids = (int *) lv_calloc((size_t) (total_ports > 0 ? total_ports : 1), sizeof(int));
    result->error_messages = (char **) lv_calloc((size_t) (total_ports > 0 ? total_ports : 1), sizeof(char *));
    result->invalid_ports = 0;
    result->all_valid = true;

    if (!result->invalid_port_ids || !result->error_messages) {
        lv_free((void **) &result->invalid_port_ids);
        lv_free((void **) &result->error_messages);
        lv_free((void **) &result);
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "分配不变量数组失败");
    }

    /* 第二遍：检查每个端口的不变量 */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node || node->type != GEOM_PORT)
            continue;

        Port *port = node->data.port;
        if (!port)
            continue;

        bool port_valid = true;

        /* 不变量 1 & 2: 端口的 namespace_depth <= 父函数块的 namespace_depth */
        if (port->parent_block_id >= 0) {
            GeomNode *parent = graph_get_node((ConstraintGraph *) graph, port->parent_block_id);
            if (parent && parent->type == GEOM_FUNCTION_BLOCK) {
                if (port->namespace_depth > parent->namespace_depth) {
                    /* 记录违规 */
                    int idx = result->invalid_ports;
                    result->invalid_port_ids[idx] = node->id;
                    const char *port_type_str = (port->type == PORT_INPUT) ? "INPUT" : "OUTPUT";
                    lvStrBuf sb = {0};
                    lv_strbuf_printf(&sb,
                             "Port %d (%s): namespace_depth (%d) > parent function block %d namespace_depth (%d)",
                             node->id, port_type_str, port->namespace_depth, port->parent_block_id,
                             parent->namespace_depth);
                    lv_free((void **) &result->error_messages[idx]);
                    result->error_messages[idx] = lv_strdup_safe(sb.data);
                    result->invalid_ports++;
                    port_valid = false;
                }
            }
        }

        /* 不变量 3: 端口连接的对方节点存在 */
        if (port->connected_to) {
            /* 检查 connected_to 节点是否在图的节点列表中 */
            bool found = false;
            for (int j = 0; j < graph->node_count; j++) {
                if (graph->nodes[j] == port->connected_to) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                int idx = result->invalid_ports;
                result->invalid_port_ids[idx] = node->id;
                lvStrBuf sb_2 = {0};
                lv_strbuf_printf(&sb_2, "Port %d: connected_to node does not exist in graph", node->id);
                lv_free((void **) &result->error_messages[idx]);
                result->error_messages[idx] = lv_strdup_safe(sb_2.data);
                result->invalid_ports++;
                port_valid = false;
            }
        }

        /* 不变量 4: 端口的类型区域与连接节点的类型兼容（深度类型等价检查） */
        if (port->type_region && port->connected_to) {
            int compat = check_port_type_deep_compatible(graph, node->id, port->connected_to->id, NULL);
            if (compat == 1) {
                /* 类型不兼容——记录违规 */
                int idx = result->invalid_ports;
                result->invalid_port_ids[idx] = node->id;
                lvStrBuf sb_3 = {0};
                lv_strbuf_printf(&sb_3, "Port %d: type incompatible with connected node %d", node->id,
                         port->connected_to->id);
                lv_free((void **) &result->error_messages[idx]);
                result->error_messages[idx] = lv_strdup_safe(sb_3.data);
                result->invalid_ports++;
                port_valid = false;
            }
        }

        if (!port_valid) {
            result->all_valid = false;
        }
    }

    return result;
}

void debug_port_invariant_result_destroy(PortInvariantResult *result) {
    if (!result)
        return;
    if (result->error_messages) {
        for (int i = 0; i < result->invalid_ports; i++) {
            lv_free((void **) &result->error_messages[i]);
        }
        lv_free((void **) &result->error_messages);
    }
    lv_free((void **) &result->invalid_port_ids);
    lv_free((void **) &result);
}
