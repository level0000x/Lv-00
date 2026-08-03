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
#include "lv/lv_json.h"
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
    node->vtable = get_vtable_for_type(type);
    node->trust = TRUST_GREEN;
    node->is_active = true; /* v3.6.0: 新节点默认活跃 */
    node->namespace_depth = 0;
    node->parent_block_id = -1;

    /* 通过 vtable 调用类型特定的初始化 */
    if (node->vtable && node->vtable->alloc) {
        node->vtable->alloc(node, graph);
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
    node->vtable = get_vtable_for_type(type);
    node->trust = TRUST_GREEN;
    node->is_active = true; /* v3.6.0: 新节点默认活跃 */
    node->namespace_depth = 0;
    node->parent_block_id = -1;

    /* 通过 vtable 调用类型特定的初始化 */
    if (node->vtable && node->vtable->alloc) {
        node->vtable->alloc(node, graph);
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

/* ========================================================================
 * 几何节点虚函数表 (VTable) 实现
 * ======================================================================== */

/* ── 类型特定的 alloc（类型特定的节点初始化） ── */

static void point_alloc(GeomNode *node, ConstraintGraph *graph) {
    (void)node;
    (void)graph;
    /* GEOM_POINT: 无需额外的类型特定初始化 */
}

static void line_segment_alloc(GeomNode *node, ConstraintGraph *graph) {
    (void)node;
    (void)graph;
    /* GEOM_LINE_SEGMENT: 无需额外的类型特定初始化 */
}

static void region_alloc(GeomNode *node, ConstraintGraph *graph) {
    (void)node;
    (void)graph;
    /* GEOM_REGION: 边界线段在 graph_add_region 中设置 */
}

static void circle_alloc(GeomNode *node, ConstraintGraph *graph) {
    (void)node;
    (void)graph;
    /* GEOM_CIRCLE: 圆心和半径节点 ID 在 graph_add_circle 中设置 */
}

static void port_alloc(GeomNode *node, ConstraintGraph *graph) {
    (void)graph;
    /* GEOM_PORT: 同步分配 Port 结构体，避免后续 data.port 为 NULL */
    if (!node->data.port) {
        node->data.port = (Port *)lv_calloc(1, sizeof(Port));
        if (node->data.port) {
            node->data.port->type = PORT_INPUT;
            node->data.port->namespace_depth = 0;
            node->data.port->parent_block_id = -1;
            node->data.port->is_formal_param = false;
        }
    }
}

static void func_block_alloc(GeomNode *node, ConstraintGraph *graph) {
    (void)node;
    (void)graph;
    /* GEOM_FUNCTION_BLOCK: 内部节点和端口数组在 graph_add_function_block 中设置 */
}

/* ── 类型特定的 free（释放类型特定的数据） ── */

static void point_free(GeomNode *node) {
    (void)node;
    /* GEOM_POINT: 无类型特定数据需释放 */
}

static void line_segment_free(GeomNode *node) {
    (void)node;
    /* GEOM_LINE_SEGMENT: 无类型特定数据需释放 */
}

static void region_free(GeomNode *node) {
    lv_free((void **)&node->data.region.boundary_segments);
    node->data.region.boundary_segments = NULL;
    node->data.region.segment_count = 0;
}

static void circle_free(GeomNode *node) {
    (void)node;
    /* GEOM_CIRCLE: 无类型特定数据需释放 */
}

static void port_free(GeomNode *node) {
    lv_free((void **)&node->data.port);
    node->data.port = NULL;
}

static void func_block_free(GeomNode *node) {
    lv_free((void **)&node->data.func_block.internal_nodes);
    node->data.func_block.internal_nodes = NULL;
    lv_free((void **)&node->data.func_block.input_port_ids);
    node->data.func_block.input_port_ids = NULL;
    lv_free((void **)&node->data.func_block.output_port_ids);
    node->data.func_block.output_port_ids = NULL;
    node->data.func_block.internal_node_count = 0;
    node->data.func_block.input_count = 0;
    node->data.func_block.output_count = 0;
}

/* ── 类型特定的 clone（存根，完整实现需深拷贝类型特定数据） ── */

static GeomNode *point_clone(const GeomNode *node, ConstraintGraph *dst_graph) {
    (void)node;
    (void)dst_graph;
    return NULL; /* 存根 */
}

static GeomNode *line_segment_clone(const GeomNode *node, ConstraintGraph *dst_graph) {
    (void)node;
    (void)dst_graph;
    return NULL; /* 存根 */
}

static GeomNode *region_clone(const GeomNode *node, ConstraintGraph *dst_graph) {
    (void)node;
    (void)dst_graph;
    return NULL; /* 存根 */
}

static GeomNode *circle_clone(const GeomNode *node, ConstraintGraph *dst_graph) {
    (void)node;
    (void)dst_graph;
    return NULL; /* 存根 */
}

static GeomNode *port_clone(const GeomNode *node, ConstraintGraph *dst_graph) {
    (void)node;
    (void)dst_graph;
    return NULL; /* 存根 */
}

static GeomNode *func_block_clone(const GeomNode *node, ConstraintGraph *dst_graph) {
    (void)node;
    (void)dst_graph;
    return NULL; /* 存根 */
}

/* ── 类型特定的 type_name ── */

static const char *point_type_name(void) {
    return "POINT";
}

static const char *line_segment_type_name(void) {
    return "LINE_SEGMENT";
}

static const char *region_type_name(void) {
    return "REGION";
}

static const char *circle_type_name(void) {
    return "CIRCLE";
}

static const char *port_type_name(void) {
    return "PORT";
}

static const char *func_block_type_name(void) {
    return "FUNCTION_BLOCK";
}

/* ── 类型特定的 serialize（追加类型特定数据到 JSON 缓冲区） ── */

static bool point_serialize(const GeomNode *node, void *buf) {
    (void)node;
    (void)buf;
    /* GEOM_POINT: 只有通用数据，无需追加 */
    return true;
}

static bool line_segment_serialize(const GeomNode *node, void *buf) {
    lvJsonBuf *jb = (lvJsonBuf *)buf;
    char id_str[32];
    int half = node->coord_count / 2;
    if (half < 2) half = 2;
    lv_json_buf_append_raw(jb, "\"endpoint1_start\":0,");
    lv_json_buf_append_raw(jb, "\"endpoint2_start\":");
    snprintf(id_str, sizeof(id_str), "%d", half);
    lv_json_buf_append_raw(jb, id_str);
    lv_json_buf_append_raw(jb, ",");
    lv_json_buf_append_raw(jb, "\"coord_count\":");
    snprintf(id_str, sizeof(id_str), "%d", node->coord_count);
    lv_json_buf_append_raw(jb, id_str);
    return true;
}

static bool region_serialize(const GeomNode *node, void *buf) {
    lvJsonBuf *jb = (lvJsonBuf *)buf;
    char id_str[32];
    lv_json_buf_append_raw(jb, "\"boundary_segments\":[");
    for (int i = 0; i < node->data.region.segment_count; i++) {
        if (i > 0) lv_json_buf_append_char(jb, ',');
        snprintf(id_str, sizeof(id_str), "%d", node->data.region.boundary_segments[i]->id);
        lv_json_buf_append_raw(jb, id_str);
    }
    lv_json_buf_append_raw(jb, "],");
    lv_json_buf_append_raw(jb, "\"segment_count\":");
    snprintf(id_str, sizeof(id_str), "%d", node->data.region.segment_count);
    lv_json_buf_append_raw(jb, id_str);
    return true;
}

static bool circle_serialize(const GeomNode *node, void *buf) {
    lvJsonBuf *jb = (lvJsonBuf *)buf;
    char id_str[32];
    lv_json_buf_append_raw(jb, "\"center_node_id\":");
    snprintf(id_str, sizeof(id_str), "%d", node->data.circle.center_node_id);
    lv_json_buf_append_raw(jb, id_str);
    lv_json_buf_append_raw(jb, ",");
    lv_json_buf_append_raw(jb, "\"radius_node_id\":");
    snprintf(id_str, sizeof(id_str), "%d", node->data.circle.radius_node_id);
    lv_json_buf_append_raw(jb, id_str);
    return true;
}

static bool port_serialize(const GeomNode *node, void *buf) {
    lvJsonBuf *jb = (lvJsonBuf *)buf;
    if (node->data.port) {
        lv_json_buf_append_raw(jb, "\"port_type\":\"");
        lv_json_buf_append_raw(jb, node->data.port->type == PORT_INPUT ? "INPUT" : "OUTPUT");
        lv_json_buf_append_raw(jb, "\",");
        lv_json_buf_append_raw(jb, "\"is_formal_param\":");
        lv_json_buf_append_raw(jb, node->data.port->is_formal_param ? "true" : "false");
        lv_json_buf_append_raw(jb, ",");
        lv_json_buf_append_raw(jb, "\"is_polymorphic\":");
        lv_json_buf_append_raw(jb, node->data.port->is_polymorphic ? "true" : "false");
    }
    return true;
}

static bool func_block_serialize(const GeomNode *node, void *buf) {
    lvJsonBuf *jb = (lvJsonBuf *)buf;
    char id_str[32];

    lv_json_buf_append_raw(jb, "\"internal_nodes\":[");
    for (int i = 0; i < node->data.func_block.internal_node_count; i++) {
        if (i > 0) lv_json_buf_append_char(jb, ',');
        snprintf(id_str, sizeof(id_str), "%d", node->data.func_block.internal_nodes[i]->id);
        lv_json_buf_append_raw(jb, id_str);
    }
    lv_json_buf_append_raw(jb, "],");

    lv_json_buf_append_raw(jb, "\"input_port_ids\":[");
    for (int i = 0; i < node->data.func_block.input_count; i++) {
        if (i > 0) lv_json_buf_append_char(jb, ',');
        snprintf(id_str, sizeof(id_str), "%d", node->data.func_block.input_port_ids[i]);
        lv_json_buf_append_raw(jb, id_str);
    }
    lv_json_buf_append_raw(jb, "],");

    lv_json_buf_append_raw(jb, "\"output_port_ids\":[");
    for (int i = 0; i < node->data.func_block.output_count; i++) {
        if (i > 0) lv_json_buf_append_char(jb, ',');
        snprintf(id_str, sizeof(id_str), "%d", node->data.func_block.output_port_ids[i]);
        lv_json_buf_append_raw(jb, id_str);
    }
    lv_json_buf_append_raw(jb, "],");

    lv_json_buf_append_raw(jb, "\"determinism_state\":");
    switch (node->data.func_block.determinism_state) {
        case UNVERIFIED:
            lv_json_buf_append_raw(jb, "\"UNVERIFIED\"");
            break;
        case VERIFIED:
            lv_json_buf_append_raw(jb, "\"VERIFIED\"");
            break;
        case NON_DETERMINISTIC:
            lv_json_buf_append_raw(jb, "\"NON_DETERMINISTIC\"");
            break;
        case PARTIALLY_VERIFIED:
            lv_json_buf_append_raw(jb, "\"PARTIALLY_VERIFIED\"");
            break;
    }
    return true;
}

/* ── 类型特定的 detect_conflict（存根） ── */

static bool point_detect_conflict(const GeomNode *a, const GeomNode *b) {
    (void)a;
    (void)b;
    return false; /* 存根 */
}

static bool line_segment_detect_conflict(const GeomNode *a, const GeomNode *b) {
    (void)a;
    (void)b;
    return false; /* 存根 */
}

static bool region_detect_conflict(const GeomNode *a, const GeomNode *b) {
    (void)a;
    (void)b;
    return false; /* 存根 */
}

static bool circle_detect_conflict(const GeomNode *a, const GeomNode *b) {
    (void)a;
    (void)b;
    return false; /* 存根 */
}

static bool port_detect_conflict(const GeomNode *a, const GeomNode *b) {
    (void)a;
    (void)b;
    return false; /* 存根 */
}

static bool func_block_detect_conflict(const GeomNode *a, const GeomNode *b) {
    (void)a;
    (void)b;
    return false; /* 存根 */
}

/* ── 类型特定的 hash（计算类型特定数据的哈希值） ── */

static uint32_t point_hash(const GeomNode *node) {
    (void)node;
    return 0; /* GEOM_POINT: 无类型特定数据，哈希值固定为 0 */
}

static uint32_t line_segment_hash(const GeomNode *node) {
    (void)node;
    return 0; /* 存根 */
}

static uint32_t region_hash(const GeomNode *node) {
    (void)node;
    return 0; /* 存根 */
}

static uint32_t circle_hash(const GeomNode *node) {
    (void)node;
    return 0; /* 存根 */
}

static uint32_t port_hash(const GeomNode *node) {
    (void)node;
    return 0; /* 存根 */
}

static uint32_t func_block_hash(const GeomNode *node) {
    (void)node;
    return 0; /* 存根 */
}

/* ── 类型特定的 compare ── */

static int point_compare(const GeomNode *a, const GeomNode *b) {
    (void)a;
    (void)b;
    return 0; /* 存根：暂不比较类型特定数据 */
}

static int line_segment_compare(const GeomNode *a, const GeomNode *b) {
    (void)a;
    (void)b;
    return 0; /* 存根 */
}

static int region_compare(const GeomNode *a, const GeomNode *b) {
    (void)a;
    (void)b;
    return 0; /* 存根 */
}

static int circle_compare(const GeomNode *a, const GeomNode *b) {
    (void)a;
    (void)b;
    return 0; /* 存根 */
}

static int port_compare(const GeomNode *a, const GeomNode *b) {
    (void)a;
    (void)b;
    return 0; /* 存根 */
}

static int func_block_compare(const GeomNode *a, const GeomNode *b) {
    (void)a;
    (void)b;
    return 0; /* 存根 */
}

/* ── 类型特定的 deserialize（存根） ── */

static bool point_deserialize(GeomNode *node, const uint8_t *data, size_t size) {
    (void)node;
    (void)data;
    (void)size;
    return true; /* GEOM_POINT: 无类型特定数据 */
}

static bool line_segment_deserialize(GeomNode *node, const uint8_t *data, size_t size) {
    (void)node;
    (void)data;
    (void)size;
    return true; /* 存根 */
}

static bool region_deserialize(GeomNode *node, const uint8_t *data, size_t size) {
    (void)node;
    (void)data;
    (void)size;
    return true; /* 存根 */
}

static bool circle_deserialize(GeomNode *node, const uint8_t *data, size_t size) {
    (void)node;
    (void)data;
    (void)size;
    return true; /* 存根 */
}

static bool port_deserialize(GeomNode *node, const uint8_t *data, size_t size) {
    (void)node;
    (void)data;
    (void)size;
    return true; /* 存根 */
}

static bool func_block_deserialize(GeomNode *node, const uint8_t *data, size_t size) {
    (void)node;
    (void)data;
    (void)size;
    return true; /* 存根 */
}

/* ── 类型特定的 fixup_refs（深拷贝后修复交叉引用） ── */

static void point_fixup_refs(GeomNode *node, const int *id_map, int max_id, ConstraintGraph *dst_graph) {
    (void)node;
    (void)id_map;
    (void)max_id;
    (void)dst_graph;
    /* GEOM_POINT: 无内部指针引用需修复 */
}

static void line_segment_fixup_refs(GeomNode *node, const int *id_map, int max_id, ConstraintGraph *dst_graph) {
    (void)node;
    (void)id_map;
    (void)max_id;
    (void)dst_graph;
    /* GEOM_LINE_SEGMENT: 无内部指针引用需修复 */
}

static void region_fixup_refs(GeomNode *node, const int *id_map, int max_id, ConstraintGraph *dst_graph) {
    for (int j = 0; j < node->data.region.segment_count; j++) {
        if (node->data.region.boundary_segments[j]) {
            int old_sid = node->data.region.boundary_segments[j]->id;
            if (id_map && old_sid >= 0 && old_sid <= max_id && id_map[old_sid] >= 0) {
                node->data.region.boundary_segments[j] = graph_get_node(dst_graph, id_map[old_sid]);
            } else {
                node->data.region.boundary_segments[j] = NULL;
            }
        }
    }
}

static void circle_fixup_refs(GeomNode *node, const int *id_map, int max_id, ConstraintGraph *dst_graph) {
    (void)dst_graph;
    if (id_map) {
        if (node->data.circle.center_node_id >= 0 && node->data.circle.center_node_id <= max_id &&
            id_map[node->data.circle.center_node_id] >= 0) {
            node->data.circle.center_node_id = id_map[node->data.circle.center_node_id];
        }
        if (node->data.circle.radius_node_id >= 0 && node->data.circle.radius_node_id <= max_id &&
            id_map[node->data.circle.radius_node_id] >= 0) {
            node->data.circle.radius_node_id = id_map[node->data.circle.radius_node_id];
        }
    }
}

static void port_fixup_refs(GeomNode *node, const int *id_map, int max_id, ConstraintGraph *dst_graph) {
    if (node->data.port && node->data.port->connected_to) {
        int old_cid = node->data.port->connected_to->id;
        if (id_map && old_cid >= 0 && old_cid <= max_id && id_map[old_cid] >= 0) {
            node->data.port->connected_to = graph_get_node(dst_graph, id_map[old_cid]);
        } else {
            node->data.port->connected_to = NULL;
        }
    }
}

static void func_block_fixup_refs(GeomNode *node, const int *id_map, int max_id, ConstraintGraph *dst_graph) {
    for (int j = 0; j < node->data.func_block.internal_node_count; j++) {
        if (node->data.func_block.internal_nodes[j]) {
            int old_iid = node->data.func_block.internal_nodes[j]->id;
            if (id_map && old_iid >= 0 && old_iid <= max_id && id_map[old_iid] >= 0) {
                node->data.func_block.internal_nodes[j] = graph_get_node(dst_graph, id_map[old_iid]);
            } else {
                node->data.func_block.internal_nodes[j] = NULL;
            }
        }
    }
    for (int j = 0; j < node->data.func_block.input_count; j++) {
        int old_pid = node->data.func_block.input_port_ids[j];
        if (id_map && old_pid >= 0 && old_pid <= max_id && id_map[old_pid] >= 0) {
            node->data.func_block.input_port_ids[j] = id_map[old_pid];
        }
    }
    for (int j = 0; j < node->data.func_block.output_count; j++) {
        int old_pid = node->data.func_block.output_port_ids[j];
        if (id_map && old_pid >= 0 && old_pid <= max_id && id_map[old_pid] >= 0) {
            node->data.func_block.output_port_ids[j] = id_map[old_pid];
        }
    }
}

/* ── 类型特定的 get_trust_coord_count（信任颜色传播用的坐标计数） ── */

static int point_get_trust_coord_count(const GeomNode *node) {
    return node->coord_count;
}

static int line_segment_get_trust_coord_count(const GeomNode *node) {
    return node->coord_count;
}

static int region_get_trust_coord_count(const GeomNode *node) {
    (void)node;
    return 0; /* 区域节点不参与信任颜色传播 */
}

static int circle_get_trust_coord_count(const GeomNode *node) {
    (void)node;
    return 0; /* 圆节点不参与信任颜色传播 */
}

static int port_get_trust_coord_count(const GeomNode *node) {
    (void)node;
    return 0; /* 端口节点不参与信任颜色传播 */
}

static int func_block_get_trust_coord_count(const GeomNode *node) {
    (void)node;
    return 0; /* 函数块节点不参与信任颜色传播 */
}

/* ── VTable 实例 ── */

static const GeomNodeVTable kPointVTable = {
    .alloc = point_alloc,
    .free = point_free,
    .clone = point_clone,
    .type_name = point_type_name,
    .serialize = point_serialize,
    .detect_conflict = point_detect_conflict,
    .hash = point_hash,
    .compare = point_compare,
    .deserialize = point_deserialize,
    .fixup_refs = point_fixup_refs,
    .get_trust_coord_count = point_get_trust_coord_count,
};

static const GeomNodeVTable kLineSegmentVTable = {
    .alloc = line_segment_alloc,
    .free = line_segment_free,
    .clone = line_segment_clone,
    .type_name = line_segment_type_name,
    .serialize = line_segment_serialize,
    .detect_conflict = line_segment_detect_conflict,
    .hash = line_segment_hash,
    .compare = line_segment_compare,
    .deserialize = line_segment_deserialize,
    .fixup_refs = line_segment_fixup_refs,
    .get_trust_coord_count = line_segment_get_trust_coord_count,
};

static const GeomNodeVTable kRegionVTable = {
    .alloc = region_alloc,
    .free = region_free,
    .clone = region_clone,
    .type_name = region_type_name,
    .serialize = region_serialize,
    .detect_conflict = region_detect_conflict,
    .hash = region_hash,
    .compare = region_compare,
    .deserialize = region_deserialize,
    .fixup_refs = region_fixup_refs,
    .get_trust_coord_count = region_get_trust_coord_count,
};

static const GeomNodeVTable kCircleVTable = {
    .alloc = circle_alloc,
    .free = circle_free,
    .clone = circle_clone,
    .type_name = circle_type_name,
    .serialize = circle_serialize,
    .detect_conflict = circle_detect_conflict,
    .hash = circle_hash,
    .compare = circle_compare,
    .deserialize = circle_deserialize,
    .fixup_refs = circle_fixup_refs,
    .get_trust_coord_count = circle_get_trust_coord_count,
};

static const GeomNodeVTable kPortVTable = {
    .alloc = port_alloc,
    .free = port_free,
    .clone = port_clone,
    .type_name = port_type_name,
    .serialize = port_serialize,
    .detect_conflict = port_detect_conflict,
    .hash = port_hash,
    .compare = port_compare,
    .deserialize = port_deserialize,
    .fixup_refs = port_fixup_refs,
    .get_trust_coord_count = port_get_trust_coord_count,
};

static const GeomNodeVTable kFuncBlockVTable = {
    .alloc = func_block_alloc,
    .free = func_block_free,
    .clone = func_block_clone,
    .type_name = func_block_type_name,
    .serialize = func_block_serialize,
    .detect_conflict = func_block_detect_conflict,
    .hash = func_block_hash,
    .compare = func_block_compare,
    .deserialize = func_block_deserialize,
    .fixup_refs = func_block_fixup_refs,
    .get_trust_coord_count = func_block_get_trust_coord_count,
};

/* ── 根据 GeomType 获取 vtable ── */

const GeomNodeVTable *get_vtable_for_type(GeomType type) {
    switch (type) {
        case GEOM_POINT:
            return &kPointVTable;
        case GEOM_LINE_SEGMENT:
            return &kLineSegmentVTable;
        case GEOM_REGION:
            return &kRegionVTable;
        case GEOM_CIRCLE:
            return &kCircleVTable;
        case GEOM_PORT:
            return &kPortVTable;
        case GEOM_FUNCTION_BLOCK:
            return &kFuncBlockVTable;
        default:
            return NULL;
    }
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
