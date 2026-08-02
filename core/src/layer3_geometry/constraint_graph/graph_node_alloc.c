/**
 * @file graph_node_alloc.c
 * @brief ConstraintGraph 节点与约束生命周期管理 —— 节点/约束分配与生命周期创建
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

/**
 * @brief 安全数组扩容辅助函数（委托给统一的 lv_ensure_capacity）
 * @param arr 当前数组指针
 * @param count 当前元素数量
 * @param capacity 当前容量指针
 * @param elem_size 单个元素大小
 * @param min_growth 最小增长量
 * @return 扩容后的数组指针，失败返回 NULL
 * @note 内部委托给 lv_ensure_capacity
 */
static void *graph_ensure_capacity(void *arr, int count, int *capacity, size_t elem_size, int min_growth) {
    void *arr_ptr = arr;
    if (!lv_ensure_capacity(&arr_ptr, count, capacity, elem_size, min_growth))
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "graph_ensure_capacity: lv_ensure_capacity failed");
    return arr_ptr;
}

/**
 * @brief 在约束图中分配并初始化一个新的几何节点
 *
 * 分配 GeomNode 内存（lv_malloc + memset），设置唯一 ID、类型、
 * 默认信任等级和命名空间深度。若节点数组容量不足，自动扩容
 * （按 lv_ARRAY_GROWTH_FACTOR 倍增长），扩容失败时释放已分配节点并返回 NULL。
 *
 * @param graph 约束图指针
 * @param type  几何节点类型（GEOM_POINT / GEOM_SEGMENT / GEOM_REGION / GEOM_PORT / GEOM_FUNCTION_BLOCK）
 * @return 新分配的 GeomNode 指针，失败返回 NULL
 */
GeomNode *graph_alloc_node(ConstraintGraph *graph, GeomType type) {
    GeomNode *node = lv_calloc(1, sizeof(GeomNode));
    if (!node)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "graph_alloc_node: calloc node failed");
    /* v3.4.1: 使用原子操作分配节点ID，确保多线程安全 */
    node->id = GRAPH_ATOMIC_NODE_ID_INCREMENT(graph);
    node->type = type;
    node->trust = TRUST_GREEN;
    node->is_active = true; /* v3.6.0: 新节点默认活跃 */
    node->namespace_depth = 0;
    node->parent_block_id = -1;

    /* v3.x: PORT 类型节点同步分配 Port 结构体，避免后续 data.port 为 NULL */
    if (type == GEOM_PORT) {
        node->data.port = lv_calloc(1, sizeof(Port));
        if (node->data.port) {
            node->data.port->type = PORT_INPUT;
            node->data.port->namespace_depth = 0;
            node->data.port->parent_block_id = -1;
            node->data.port->is_formal_param = false;
        }
    }
    GeomNode **new_nodes = (GeomNode **) graph_ensure_capacity(graph->nodes, graph->node_count, &graph->node_capacity,
                                                               sizeof(GeomNode *), 1);
    if (!new_nodes) {
        lv_free((void **) &node);
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "graph_alloc_node: ensure_capacity failed");
    }
    graph->nodes = new_nodes;
    graph->nodes[graph->node_count++] = node;
    graph_node_index_insert(graph, node);
    return node;
}

/**
 * @brief 在约束图中分配并初始化一个新的约束
 *
 * 分配 Constraint 内存（lv_malloc + memset），设置唯一 ID 和约束类型。
 * 若约束数组容量不足，自动按 lv_ARRAY_GROWTH_FACTOR 倍扩容，
 * 扩容失败时释放已分配约束并返回 NULL。
 *
 * @param graph 约束图指针
 * @param type  约束类型（INCIDENCE / BETWEENNESS / INTERSECTION / CONTAINMENT / CONNECTION）
 * @return 新分配的 Constraint 指针，失败返回 NULL
 */
Constraint *graph_alloc_constraint(ConstraintGraph *graph, ConstraintType type) {
    Constraint *con = lv_calloc(1, sizeof(Constraint));
    if (!con)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "graph_alloc_constraint: calloc con failed");
    /* v3.4.1: 使用原子操作分配约束ID，确保多线程安全 */
    con->id = GRAPH_ATOMIC_CONSTRAINT_ID_INCREMENT(graph);
    con->type = type;
    con->is_active = true; /* v3.5.0: 新约束默认活跃 */
    Constraint **new_constraints = (Constraint **) graph_ensure_capacity(
        graph->constraints, graph->constraint_count, &graph->constraint_capacity, sizeof(Constraint *), 1);
    if (!new_constraints) {
        lv_free((void **) &con);
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "graph_alloc_constraint: ensure_capacity failed");
    }
    graph->constraints = new_constraints;
    graph->constraints[graph->constraint_count++] = con;
    graph_constraint_index_insert(graph, con);
    return con;
}

/* 带指定ID添加节点（用于反序列化） */
GeomNode *graph_add_node_with_id(ConstraintGraph *graph, int node_id, GeomType type, SymbolicCoord **coords,
                                 int coord_count) {
    if (!graph)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "graph_add_node_with_id: graph is NULL");

    /* 检查ID是否已被使用 */
    if (graph_get_node(graph, node_id) != NULL) {
        lv_RETURN_ERROR_NULL(lv_ERROR_ALREADY_EXISTS, "graph_add_node_with_id: node ID %d already exists", node_id);
    }

    GeomNode *node = lv_calloc(1, sizeof(GeomNode));
    if (!node)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "graph_add_node_with_id: calloc node failed");

    node->id = node_id;
    node->type = type;
    node->trust = TRUST_GREEN;
    node->is_active = true; /* v3.6.0: 新节点默认活跃 */
    node->namespace_depth = 0;
    node->parent_block_id = -1;

    /* PORT 类型节点同步分配 Port 结构体 */
    if (type == GEOM_PORT) {
        node->data.port = lv_calloc(1, sizeof(Port));
        if (node->data.port) {
            node->data.port->type = PORT_INPUT;
            node->data.port->namespace_depth = 0;
            node->data.port->parent_block_id = -1;
            node->data.port->is_formal_param = false;
        }
    }

    /* 复制坐标 */
    if (coord_count > 0 && coords) {
        node->symbolic_coords = lv_malloc((size_t) coord_count * sizeof(SymbolicCoord *));
        if (!node->symbolic_coords) {
            lv_free((void **) &node);
            lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "graph_add_node_with_id: malloc symbolic_coords failed");
        }
        for (int i = 0; i < coord_count; i++) {
            node->symbolic_coords[i] = symbolic_coord_copy(coords[i]);
            if (!node->symbolic_coords[i]) {
                /* 坐标拷贝失败：清理已分配的坐标并返回 NULL */
                for (int j = 0; j < i; j++) {
                    symbolic_coord_destroy(node->symbolic_coords[j]);
                }
                lv_free((void **) &node->symbolic_coords);
                lv_free((void **) &node);
                lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "graph_add_node_with_id: symbolic_coord_copy failed");
            }
        }
        node->coord_count = coord_count;
    }

    /* 扩展数组 */
    if (graph->node_count >= graph->node_capacity) {
        if (graph->node_capacity > INT_MAX / lv_ARRAY_GROWTH_FACTOR) {
            lv_free((void **) &node);
            lv_RETURN_ERROR_NULL(lv_ERROR_OVERFLOW, "graph_add_node_with_id: node_capacity overflow");
        }
        int new_capacity =
            graph->node_capacity == 0 ? lv_INITIAL_ARRAY_CAPACITY : graph->node_capacity * lv_ARRAY_GROWTH_FACTOR;
        /* 检查 size_t 乘积溢出：new_capacity * sizeof(GeomNode *) 可能超过 SIZE_MAX */
        if ((size_t) new_capacity > SIZE_MAX / sizeof(GeomNode *)) {
            lv_free((void **) &node);
            lv_RETURN_ERROR_NULL(lv_ERROR_OVERFLOW, "graph_add_node_with_id: new_capacity overflow for node array");
        }
        GeomNode **new_nodes = lv_realloc(graph->nodes, (size_t) new_capacity * sizeof(GeomNode *));
        if (!new_nodes) {
            /* 清理已分配的坐标 */
            for (int i = 0; i < coord_count; i++) {
                if (node->symbolic_coords[i])
                    symbolic_coord_destroy(node->symbolic_coords[i]);
            }
            lv_free((void **) &node->symbolic_coords);
            lv_free((void **) &node);
            lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "graph_add_node_with_id: realloc nodes failed");
        }
        graph->nodes = new_nodes;
        graph->node_capacity = new_capacity;
    }

    graph->nodes[graph->node_count++] = node;
    graph_node_index_insert(graph, node);

    /* 更新 next_node_id 以确保新节点ID不会冲突（使用原子 CAS 循环保证线程安全） */
    if (node_id >= graph->next_node_id) {
        int expected = graph->next_node_id;
        int desired = node_id + 1;
        while (expected < desired) {
            desired = node_id + 1;
            if (atomic_compare_exchange_weak((atomic_int *) &graph->next_node_id, &expected, desired)) {
                break;
            }
            /* expected 已被更新为当前值，循环重试直到成功或当前值已 >= desired */
        }
    }

    /* 流式事件: 节点添加 */
    if (graph_stream_ctx) {
        static const char *type_names[] = {"POINT", "LINE", "REGION", "CIRCLE", "PORT", "FUNC_BLOCK"};
        lvStrBuf sb = {0};
        const char *tname = (type >= 0 && type <= 5) ? type_names[type] : "UNKNOWN";
        lv_strbuf_printf(&sb, "添加节点 #%d (类型: %s)", node_id, tname);
        stream_emit_simple(graph_stream_ctx, STREAM_EVENT_NODE_ADDED, sb.data, node_id);
        lv_strbuf_destroy(&sb);
    }

    return node;
}

/* 带指定ID添加约束（用于反序列化） */
Constraint *graph_add_constraint_with_id(ConstraintGraph *graph, int constraint_id, ConstraintType type,
                                         const int *participants, int participant_count) {
    if (!graph || !participants || participant_count <= 0)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "graph_add_constraint_with_id: NULL parameter or zero count");

    /* 检查ID是否已被使用 */
    if (graph_get_constraint(graph, constraint_id) != NULL) {
        lv_RETURN_ERROR_NULL(lv_ERROR_ALREADY_EXISTS, "graph_add_constraint_with_id: constraint ID %d already exists", constraint_id);
    }

    Constraint *con = lv_calloc(1, sizeof(Constraint));
    if (!con)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "graph_add_constraint_with_id: calloc con failed");

    con->id = constraint_id;
    con->type = type;
    con->is_active = true; /* v3.5.0: 新约束默认活跃 */
    /* 复用共享的参与者分配器（graph_index.c）：malloc + 复制 + 设置 participant_count */
    if (!graph_constraint_assign_participants(con, participants, participant_count)) {
        lv_free((void **) &con);
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "graph_add_constraint_with_id: malloc participants failed");
    }

    /* 扩展数组 */
    if (graph->constraint_count >= graph->constraint_capacity) {
        if (graph->constraint_capacity > INT_MAX / lv_ARRAY_GROWTH_FACTOR) {
            lv_free((void **) &con->participants);
            lv_free((void **) &con);
            lv_RETURN_ERROR_NULL(lv_ERROR_OVERFLOW, "graph_add_constraint_with_id: constraint_capacity overflow");
        }
        int new_capacity = graph->constraint_capacity == 0 ? lv_INITIAL_ARRAY_CAPACITY
                                                           : graph->constraint_capacity * lv_ARRAY_GROWTH_FACTOR;
        Constraint **new_constraints = lv_realloc(graph->constraints, (size_t) new_capacity * sizeof(Constraint *));
        if (!new_constraints) {
            lv_free((void **) &con->participants);
            lv_free((void **) &con);
            lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "graph_add_constraint_with_id: realloc constraints failed");
        }
        graph->constraints = new_constraints;
        graph->constraint_capacity = new_capacity;
    }

    graph->constraints[graph->constraint_count++] = con;
    graph_constraint_index_insert(graph, con);

    /* 更新 next_constraint_id 以确保新约束ID不会冲突 */
    if (constraint_id >= graph->next_constraint_id) {
        graph->next_constraint_id = constraint_id + 1;
    }

    /* 流式事件: 约束添加 */
    if (graph_stream_ctx) {
        /* 约束类型名称数组 —— 与 ConstraintType 枚举严格对齐 */
        static const char *ctype_names[] = {
            "INCIDENCE",    /* 关联约束 */
            "BETWEENNESS",  /* 介于约束 */
            "INTERSECTION", /* 相交约束 */
            "CONTAINMENT",  /* 包含约束 */
            "CONNECTION",   /* 连接约束 */
            "ANGLE"         /* 角度约束 */
        };
        lvStrBuf sb_2 = {0};
        const char *cname = (type >= 0 && type <= 5) ? ctype_names[type] : "UNKNOWN";
        lv_strbuf_printf(&sb_2, "添加约束 #%d (类型: %s, 参与者: %d个)", constraint_id, cname, participant_count);
        stream_emit_simple(graph_stream_ctx, STREAM_EVENT_CONSTRAINT_ADDED, sb_2.data, constraint_id);
        lv_strbuf_destroy(&sb_2);
    }

    return con;
}

/**
 * 检查约束是否已存在。
 *
 * @param graph       约束图指针
 * @param type        约束类型
 * @param participants 参与者节点 ID 数组
 * @param count       参与者数量
 * @return true 表示约束已存在，false 表示不存在
 */
bool constraint_exists(const ConstraintGraph *graph, ConstraintType type, const int *participants, int count) {
    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *c = graph->constraints[i];
        if (!c->is_active) /* v3.5.0: 跳过不活跃约束 */
            continue;
        if (c->type != type || c->participant_count != count)
            continue;
        bool same = true;
        for (int j = 0; j < count; j++) {
            if (c->participants[j] != participants[j]) {
                same = false;
                break;
            }
        }
        if (same)
            return true;
    }
    return false;
}
