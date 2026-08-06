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
#include "lv/memory_pool.h"
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
 * @brief 节点分配内部函数 —— 所有节点创建的统一底层分配路径
 *
 * 统一封装节点内存分配、字段初始化（ID/类型/vtable/信任等级/活跃标志/
 * 命名空间深度/父块ID）、vtable->alloc 类型特定初始化、坐标复制（with_id 路径）、
 * 节点数组扩容与索引插入。graph_alloc_node 与 graph_add_node_with_id
 * 均为本函数的薄包装，保证两路径字段初始化、ID 分配、错误处理与返回值
 * 语义完全一致（节点池化前置条件：所有节点分配唯一入口）。
 *
 * @param graph       约束图指针
 * @param type        几何节点类型
 * @param id          指定 ID（with_id 时生效；否则忽略）
 * @param with_id     是否为带指定 ID 的添加路径（graph_add_node_with_id）
 * @param coords      坐标数组（with_id 且 coord_count > 0 时深拷贝）
 * @param coord_count 坐标数量
 * @return 新分配的 GeomNode 指针，失败返回 NULL
 */
static GeomNode *node_alloc_internal(ConstraintGraph *graph, GeomType type, int id, bool with_id,
                                     SymbolicCoord **coords, int coord_count) {
    /* 错误消息前缀：与两个公开入口的原错误文本保持一致 */
    const char *fn = with_id ? "graph_add_node_with_id" : "graph_alloc_node";
    /* 节点外壳池化分配：优先从 ConstraintNode 预设池取（lv_pool_alloc 内部按
     * object_size(128) 清零，覆盖 sizeof(GeomNode)=104，与 lv_calloc 零初始化语义一致）；
     * 池不可用（lv_init 未调用 / preset pools 初始化失败）或池扩展失败时回退 lv_calloc。
     * 回退对象由 lv_pool_free 的归属校验自动按普通分配释放，不进入池空闲链表。 */
    GeomNode *node = (GeomNode *) lv_pool_alloc(lv_get_node_pool());
    if (!node) {
        node = (GeomNode *) lv_calloc(1, sizeof(GeomNode));
        if (!node) {
            lv_set_error_ctx(lv_ERROR_OUT_OF_MEMORY, __FILE__, __LINE__, fn, "%s: calloc node failed", fn);
            return NULL;
        }
    }
    /* v3.4.1: 使用原子操作分配节点ID，确保多线程安全（with_id 路径使用指定 ID） */
    node->id = with_id ? id : GRAPH_ATOMIC_NODE_ID_INCREMENT(graph);
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

    /* 复制坐标（graph_add_node_with_id 路径） */
    if (with_id && coord_count > 0 && coords) {
        node->symbolic_coords = lv_malloc((size_t) coord_count * sizeof(SymbolicCoord *));
        if (!node->symbolic_coords) {
            lv_pool_free(lv_get_node_pool(), node);
            lv_set_error_ctx(lv_ERROR_OUT_OF_MEMORY, __FILE__, __LINE__, fn, "%s: malloc symbolic_coords failed", fn);
            return NULL;
        }
        for (int i = 0; i < coord_count; i++) {
            node->symbolic_coords[i] = symbolic_coord_copy(coords[i]);
            if (!node->symbolic_coords[i]) {
                /* 坐标拷贝失败：清理已分配的坐标并返回 NULL */
                for (int j = 0; j < i; j++) {
                    symbolic_coord_destroy(node->symbolic_coords[j]);
                }
                lv_free((void **) &node->symbolic_coords);
                lv_pool_free(lv_get_node_pool(), node);
                lv_set_error_ctx(lv_ERROR_OUT_OF_MEMORY, __FILE__, __LINE__, fn, "%s: symbolic_coord_copy failed", fn);
                return NULL;
            }
        }
        node->coord_count = coord_count;
    }

    /* 扩展数组（统一委托 lv_ensure_capacity，内部含 INT_MAX/SIZE_MAX 溢出检查与倍增；
     * graph_alloc_node 原走 graph_ensure_capacity（min_growth=1），
     * graph_add_node_with_id 原直接 lv_ensure_capacity（min_growth=0），保持原扩容行为） */
    if (!lv_ensure_capacity((void **) &graph->nodes, graph->node_count, &graph->node_capacity, sizeof(GeomNode *),
                            with_id ? 0 : 1)) {
        /* 清理已分配的坐标 */
        if (with_id && coord_count > 0 && coords) {
            for (int i = 0; i < coord_count; i++) {
                if (node->symbolic_coords[i])
                    symbolic_coord_destroy(node->symbolic_coords[i]);
            }
            lv_free((void **) &node->symbolic_coords);
        }
        lv_pool_free(lv_get_node_pool(), node);
        lv_set_error_ctx(lv_ERROR_OUT_OF_MEMORY, __FILE__, __LINE__, fn,
                         with_id ? "%s: realloc nodes failed" : "%s: ensure_capacity failed", fn);
        return NULL;
    }

    graph->nodes[graph->node_count++] = node;
    graph_node_index_insert(graph, node);
    return node;
}

/**
 * @brief 在约束图中分配并初始化一个新的几何节点（统一分配路径的薄包装）
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
    return node_alloc_internal(graph, type, 0, false, NULL, 0);
}

/**
 * @brief 约束分配内部函数 —— 所有约束创建的统一底层分配路径
 *
 * 统一封装约束内存分配、字段初始化（ID/类型/活跃标志）、参与者分配
 * （with_id 路径）、约束数组扩容与索引插入。graph_alloc_constraint 与
 * graph_add_constraint_with_id 均为本函数的薄包装，保证两路径字段初始化、
 * ID 分配、错误处理与返回值语义完全一致（节点池化前置条件：所有约束分配唯一入口）。
 *
 * @param graph            约束图指针
 * @param type             约束类型
 * @param id               指定 ID（with_id 时生效；否则忽略）
 * @param with_id          是否为带指定 ID 的添加路径（graph_add_constraint_with_id）
 * @param participants     参与者节点 ID 数组（with_id 时复制）
 * @param participant_count 参与者数量
 * @return 新分配的 Constraint 指针，失败返回 NULL
 */
static Constraint *constraint_alloc_internal(ConstraintGraph *graph, ConstraintType type, int id, bool with_id,
                                             const int *participants, int participant_count) {
    /* 错误消息前缀：与两个公开入口的原错误文本保持一致 */
    const char *fn = with_id ? "graph_add_constraint_with_id" : "graph_alloc_constraint";
    /* 约束外壳池化分配：优先从 Constraint 预设池取（lv_pool_alloc 内部按
     * object_size(96) 清零，覆盖 sizeof(Constraint)=48，与 lv_calloc 零初始化语义一致）；
     * 池不可用或池扩展失败时回退 lv_calloc。回退对象由 lv_pool_free 的归属校验
     * 自动按普通分配释放，不进入池空闲链表。 */
    Constraint *con = (Constraint *) lv_pool_alloc(lv_get_constraint_pool());
    if (!con) {
        con = (Constraint *) lv_calloc(1, sizeof(Constraint));
        if (!con) {
            lv_set_error_ctx(lv_ERROR_OUT_OF_MEMORY, __FILE__, __LINE__, fn, "%s: calloc con failed", fn);
            return NULL;
        }
    }
    /* v3.4.1: 使用原子操作分配约束ID，确保多线程安全（with_id 路径使用指定 ID） */
    con->id = with_id ? id : GRAPH_ATOMIC_CONSTRAINT_ID_INCREMENT(graph);
    con->type = type;
    con->is_active = true; /* v3.5.0: 新约束默认活跃 */
    if (with_id) {
        /* 复用共享的参与者分配器（graph_index.c）：malloc + 复制 + 设置 participant_count */
        if (!graph_constraint_assign_participants(con, participants, participant_count)) {
            lv_pool_free(lv_get_constraint_pool(), con);
            lv_set_error_ctx(lv_ERROR_OUT_OF_MEMORY, __FILE__, __LINE__, fn, "%s: malloc participants failed", fn);
            return NULL;
        }
    }

    /* 扩展数组（统一委托 lv_ensure_capacity，内部含 INT_MAX/SIZE_MAX 溢出检查与倍增；
     * graph_alloc_constraint 原走 graph_ensure_capacity（min_growth=1），
     * graph_add_constraint_with_id 原直接 lv_ensure_capacity（min_growth=0），保持原扩容行为） */
    if (!lv_ensure_capacity((void **) &graph->constraints, graph->constraint_count, &graph->constraint_capacity,
                            sizeof(Constraint *), with_id ? 0 : 1)) {
        if (with_id)
            lv_free((void **) &con->participants);
        lv_pool_free(lv_get_constraint_pool(), con);
        lv_set_error_ctx(lv_ERROR_OUT_OF_MEMORY, __FILE__, __LINE__, fn,
                         with_id ? "%s: realloc constraints failed" : "%s: ensure_capacity failed", fn);
        return NULL;
    }

    graph->constraints[graph->constraint_count++] = con;
    graph_constraint_index_insert(graph, con);
    return con;
}

/**
 * @brief 在约束图中分配并初始化一个新的约束（统一分配路径的薄包装）
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
    return constraint_alloc_internal(graph, type, 0, false, NULL, 0);
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

    /* 统一分配路径：字段初始化 / 坐标复制 / 数组挂接 / 索引插入均在内部完成 */
    GeomNode *node = node_alloc_internal(graph, type, node_id, true, coords, coord_count);
    if (!node)
        return NULL;

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

    /* 流式事件: 节点添加（通用 with_id 模板） */
    graph_emit_node_added(graph, node, node_id, true);

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

    /* 统一分配路径：字段初始化 / 参与者复制 / 数组挂接 / 索引插入均在内部完成 */
    Constraint *con = constraint_alloc_internal(graph, type, constraint_id, true, participants, participant_count);
    if (!con)
        return NULL;

    /* 更新 next_constraint_id 以确保新约束ID不会冲突 */
    if (constraint_id >= graph->next_constraint_id) {
        graph->next_constraint_id = constraint_id + 1;
    }

    /* 流式事件: 约束添加（通用 with_id 模板） */
    graph_emit_constraint_added(graph, con, constraint_id, true);

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

/* ── 类型特定的 clone（深拷贝类型特定数据到目标图中的对应节点） ──
 * 契约：调用方须先在 dst_graph 中创建 id 与 node->id 相同的节点
 * （如 graph_copy 通过 graph_add_node_with_id 完成），clone 将源节点的
 * union data 深拷贝到目标节点上并返回目标节点；失败返回 NULL。
 * 指针类字段（region.boundary_segments / func_block.internal_nodes /
 * port.connected_to）先拷贝源图引用，随后由调用方调用 vtable->fixup_refs
 * 通过 id_map 重映射到 dst_graph 中的节点。
 * 内存所有权：clone 分配的数组归目标节点所有，随图销毁走 vtable->free
 * （region_free / func_block_free / port_free）释放。 */

static GeomNode *point_clone(const GeomNode *node, ConstraintGraph *dst_graph) {
    /* GEOM_POINT: 无类型特定数据，返回目标节点表示成功 */
    return graph_get_node(dst_graph, node->id);
}

static GeomNode *line_segment_clone(const GeomNode *node, ConstraintGraph *dst_graph) {
    /* GEOM_LINE_SEGMENT: 无类型特定数据，返回目标节点表示成功 */
    return graph_get_node(dst_graph, node->id);
}

static GeomNode *region_clone(const GeomNode *node, ConstraintGraph *dst_graph) {
    GeomNode *dst = graph_get_node(dst_graph, node->id);
    if (!dst)
        return NULL;
    dst->data.region.segment_count = node->data.region.segment_count;
    dst->data.region.boundary_segments = NULL;
    if (node->data.region.boundary_segments && node->data.region.segment_count > 0) {
        dst->data.region.boundary_segments =
            (GeomNode **) lv_malloc((size_t) node->data.region.segment_count * sizeof(GeomNode *));
        if (!dst->data.region.boundary_segments) {
            /* 分配失败：保持数据一致，避免 segment_count 非零而数组为 NULL */
            dst->data.region.segment_count = 0;
            return NULL;
        }
        /* 先拷贝源图引用，后续由 fixup_refs 重映射到新图 */
        memcpy(dst->data.region.boundary_segments, node->data.region.boundary_segments,
               (size_t) node->data.region.segment_count * sizeof(GeomNode *));
    }
    return dst;
}

static GeomNode *circle_clone(const GeomNode *node, ConstraintGraph *dst_graph) {
    GeomNode *dst = graph_get_node(dst_graph, node->id);
    if (!dst)
        return NULL;
    /* 纯标量字段：圆心/半径端点节点 ID（ID 一致，无需 fixup 也可直接复制） */
    dst->data.circle.center_node_id = node->data.circle.center_node_id;
    dst->data.circle.radius_node_id = node->data.circle.radius_node_id;
    return dst;
}

static GeomNode *port_clone(const GeomNode *node, ConstraintGraph *dst_graph) {
    GeomNode *dst = graph_get_node(dst_graph, node->id);
    if (!dst)
        return NULL;
    /* 复用 graph_add_node_with_id -> port_alloc 已分配的 Port 结构体，避免泄漏 */
    if (!dst->data.port) {
        dst->data.port = (Port *) lv_calloc(1, sizeof(Port));
        if (!dst->data.port)
            return NULL;
    }
    if (!node->data.port)
        return dst; /* 源无端口数据，保留目标默认值 */
    Port *src_p = node->data.port;
    Port *dst_p = dst->data.port;
    dst_p->id = src_p->id;
    dst_p->type = src_p->type;
    dst_p->namespace_depth = src_p->namespace_depth;
    dst_p->parent_block_id = src_p->parent_block_id;
    dst_p->is_formal_param = src_p->is_formal_param;
    dst_p->is_polymorphic = src_p->is_polymorphic;
    /* type_region 浅拷贝：所有权由 TypeSystem 统一管理（同 node_deep_copy_port） */
    dst_p->type_region = src_p->type_region;
    /* connected_to 先拷贝源图引用，由 fixup_refs 重映射到新图 */
    dst_p->connected_to = src_p->connected_to;
    return dst;
}

static GeomNode *func_block_clone(const GeomNode *node, ConstraintGraph *dst_graph) {
    GeomNode *dst = graph_get_node(dst_graph, node->id);
    if (!dst)
        return NULL;
    dst->data.func_block.internal_node_count = node->data.func_block.internal_node_count;
    dst->data.func_block.input_count = node->data.func_block.input_count;
    dst->data.func_block.output_count = node->data.func_block.output_count;
    dst->data.func_block.determinism_state = node->data.func_block.determinism_state;
    dst->data.func_block.internal_nodes = NULL;
    dst->data.func_block.input_port_ids = NULL;
    dst->data.func_block.output_port_ids = NULL;

    if (node->data.func_block.internal_nodes && node->data.func_block.internal_node_count > 0) {
        dst->data.func_block.internal_nodes =
            (GeomNode **) lv_malloc((size_t) node->data.func_block.internal_node_count * sizeof(GeomNode *));
        if (!dst->data.func_block.internal_nodes) {
            dst->data.func_block.internal_node_count = 0;
            return NULL;
        }
        /* 先拷贝源图引用，后续由 fixup_refs 重映射到新图 */
        memcpy(dst->data.func_block.internal_nodes, node->data.func_block.internal_nodes,
               (size_t) node->data.func_block.internal_node_count * sizeof(GeomNode *));
    }
    if (node->data.func_block.input_port_ids && node->data.func_block.input_count > 0) {
        dst->data.func_block.input_port_ids = (int *) lv_malloc((size_t) node->data.func_block.input_count * sizeof(int));
        if (!dst->data.func_block.input_port_ids) {
            /* 分配失败：释放已分配数据并保持一致性，随图销毁走 func_block_free */
            dst->data.func_block.input_count = 0;
            lv_free((void **) &dst->data.func_block.internal_nodes);
            dst->data.func_block.internal_node_count = 0;
            return NULL;
        }
        memcpy(dst->data.func_block.input_port_ids, node->data.func_block.input_port_ids,
               (size_t) node->data.func_block.input_count * sizeof(int));
    }
    if (node->data.func_block.output_port_ids && node->data.func_block.output_count > 0) {
        dst->data.func_block.output_port_ids =
            (int *) lv_malloc((size_t) node->data.func_block.output_count * sizeof(int));
        if (!dst->data.func_block.output_port_ids) {
            /* 分配失败：释放已分配数据并保持一致性，随图销毁走 func_block_free */
            dst->data.func_block.output_count = 0;
            lv_free((void **) &dst->data.func_block.input_port_ids);
            dst->data.func_block.input_count = 0;
            lv_free((void **) &dst->data.func_block.internal_nodes);
            dst->data.func_block.internal_node_count = 0;
            return NULL;
        }
        memcpy(dst->data.func_block.output_port_ids, node->data.func_block.output_port_ids,
               (size_t) node->data.func_block.output_count * sizeof(int));
    }
    return dst;
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
    /* 确定性状态 -> JSON 字符串 查找表（按下标索引，与 func_block 匿名枚举严格对齐） */
    static const char *const s_determinism_state_names[] = {
        [UNVERIFIED] = "UNVERIFIED",
        [VERIFIED] = "VERIFIED",
        [NON_DETERMINISTIC] = "NON_DETERMINISTIC",
        [PARTIALLY_VERIFIED] = "PARTIALLY_VERIFIED",
    };
    /* 原 switch 无 default：未知状态不输出字符串，保持行为一致 */
    if ((unsigned) node->data.func_block.determinism_state < lv_ARRAY_SIZE(s_determinism_state_names)) {
        const char *ds_name = s_determinism_state_names[node->data.func_block.determinism_state];
        if (ds_name) {
            lv_json_buf_append_raw(jb, ds_name);
        }
    }
    return true;
}

/* ── 类型特定的 detect_conflict ──
 * 未实现存根：代码库当前没有任何 vtable->detect_conflict 调用方
 * （冲突检测由 graph_conflict.c 的 graph_detect_conflicts 基于约束关系独立实现，不走 vtable），
 * 因此存根不会触发。若未来启用该路径，须先补齐类型特定冲突检测；当前返回 false（无冲突）为保守默认。 ── */

static bool point_detect_conflict(const GeomNode *a, const GeomNode *b) {
    (void)a;
    (void)b;
    /* 未实现：当前无 vtable->detect_conflict 调用方（冲突检测由 graph_conflict.c 独立实现），
     * 启用前须实现类型特定冲突检测；返回 false（无冲突）为保守默认 */
    return false;
}

static bool line_segment_detect_conflict(const GeomNode *a, const GeomNode *b) {
    (void)a;
    (void)b;
    /* 未实现：当前无 vtable->detect_conflict 调用方（冲突检测由 graph_conflict.c 独立实现），
     * 启用前须实现类型特定冲突检测；返回 false（无冲突）为保守默认 */
    return false;
}

static bool region_detect_conflict(const GeomNode *a, const GeomNode *b) {
    (void)a;
    (void)b;
    /* 未实现：当前无 vtable->detect_conflict 调用方（冲突检测由 graph_conflict.c 独立实现），
     * 启用前须实现类型特定冲突检测；返回 false（无冲突）为保守默认 */
    return false;
}

static bool circle_detect_conflict(const GeomNode *a, const GeomNode *b) {
    (void)a;
    (void)b;
    /* 未实现：当前无 vtable->detect_conflict 调用方（冲突检测由 graph_conflict.c 独立实现），
     * 启用前须实现类型特定冲突检测；返回 false（无冲突）为保守默认 */
    return false;
}

static bool port_detect_conflict(const GeomNode *a, const GeomNode *b) {
    (void)a;
    (void)b;
    /* 未实现：当前无 vtable->detect_conflict 调用方（冲突检测由 graph_conflict.c 独立实现），
     * 启用前须实现类型特定冲突检测；返回 false（无冲突）为保守默认 */
    return false;
}

static bool func_block_detect_conflict(const GeomNode *a, const GeomNode *b) {
    (void)a;
    (void)b;
    /* 未实现：当前无 vtable->detect_conflict 调用方（冲突检测由 graph_conflict.c 独立实现），
     * 启用前须实现类型特定冲突检测；返回 false（无冲突）为保守默认 */
    return false;
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

/* ── 根据 GeomType 获取 vtable（GeomType 连续 0..5，按下标索引） ── */

/** GeomType → 节点 VTable 映射表 */
static const GeomNodeVTable *kVTables[] = {
    [GEOM_POINT] = &kPointVTable,
    [GEOM_LINE_SEGMENT] = &kLineSegmentVTable,
    [GEOM_REGION] = &kRegionVTable,
    [GEOM_CIRCLE] = &kCircleVTable,
    [GEOM_PORT] = &kPortVTable,
    [GEOM_FUNCTION_BLOCK] = &kFuncBlockVTable,
};

const GeomNodeVTable *get_vtable_for_type(GeomType type) {
    if ((unsigned) type < sizeof(kVTables) / sizeof(kVTables[0]) && kVTables[type]) {
        return kVTables[type];
    }
    return NULL; /* 非法类型 */
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

/* ============== graph_copy：约束图深拷贝（唯一公共图级复制入口） ============== */

/**
 * @brief 深拷贝约束图
 *
 * 遍历源图中的所有节点和约束，在新图中创建完全独立的副本。
 * 高级类型（Region/Circle/Port/FunctionBlock）的类型特定数据
 * （boundary_segments、center/radius_node_id、data.port、
 * internal_nodes/input/output_port_ids）通过 vtable->clone 深拷贝，
 * 内部指针引用通过 vtable->fixup_refs 重映射到新图
 * （graph_add_node_with_id 保证新图节点 ID 与源图一致，故使用恒等 id_map）。
 *
 * 该实现是 ConstraintGraph 的唯一公共深拷贝入口。engine_frozen 的引擎
 * 冻结点与 critical_pair 的工作图复制此前各有独立的深拷贝实现（新 ID
 * 方案 / GraphSnapshot 序列化-恢复方案），均已收敛至此函数。
 */
ConstraintGraph *graph_copy(const ConstraintGraph *graph) {
    if (!graph)
        return NULL;

    ConstraintGraph *new_graph = graph_create();
    if (!new_graph)
        return NULL;

    int max_id = -1; /* 源图最大节点 ID，用于构建恒等 id_map */

    /* 复制所有节点 */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *src = graph->nodes[i];
        if (!src)
            continue;

        /* 使用带ID接口添加节点，保持ID一致 */
        GeomNode *dst = graph_add_node_with_id(new_graph, src->id, src->type, src->symbolic_coords, src->coord_count);
        if (!dst) {
            graph_destroy(new_graph);
            return NULL;
        }

        /* 复制增强字段 */
        dst->trust = src->trust;
        dst->is_active = src->is_active;
        dst->lo_subtype = src->lo_subtype;
        dst->namespace_depth = src->namespace_depth;
        dst->parent_block_id = src->parent_block_id;
        dst->numeric_precision = src->numeric_precision;

        /* 深拷贝 numeric_assumption_declaration */
        if (src->numeric_assumption_declaration) {
            dst->numeric_assumption_declaration = lv_strdup_safe(src->numeric_assumption_declaration);
        }

        /* 高级类型：通过 vtable->clone 深拷贝类型特定数据（union data）到 dst，
         * 修复 graph_copy 之前丢失 Region/Circle/Port/FunctionBlock 类型数据的缺陷 */
        if (src->vtable && src->vtable->clone) {
            if (!src->vtable->clone(src, new_graph)) {
                graph_destroy(new_graph);
                return NULL;
            }
        }

        if (src->id > max_id)
            max_id = src->id;
    }

    /* 第二遍：修复类型特定数据中的交叉引用（此时所有节点均已就绪）。
     * 由于 graph_add_node_with_id 保证新图节点 ID 与源图一致，
     * id_map 为恒等映射（old_id -> 同一 ID）。 */
    if (max_id >= 0) {
        int *id_map = (int *) lv_calloc((size_t) (max_id + 1), sizeof(int));
        if (!id_map) {
            graph_destroy(new_graph);
            return NULL;
        }
        for (int i = 0; i <= max_id; i++) {
            id_map[i] = i;
        }
        for (int i = 0; i < graph->node_count; i++) {
            GeomNode *src = graph->nodes[i];
            if (!src)
                continue;
            if (src->vtable && src->vtable->fixup_refs) {
                GeomNode *dst = graph_get_node(new_graph, src->id);
                if (dst) {
                    src->vtable->fixup_refs(dst, id_map, max_id, new_graph);
                }
            }
        }
        lv_free((void **) &id_map);
    }

    /* 复制所有约束 */
    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *src = graph->constraints[i];
        if (!src)
            continue;

        Constraint *dst =
            graph_add_constraint_with_id(new_graph, src->id, src->type, src->participants, src->participant_count);
        if (!dst) {
            graph_destroy(new_graph);
            return NULL;
        }

        /* 复制增强字段 */
        dst->template_id = src->template_id;
        dst->is_active = src->is_active;
        dst->numeric_value = src->numeric_value;
        dst->satisfaction = src->satisfaction;
    }

    /* 复制高级图属性 */
    new_graph->dirty = graph->dirty;

    return new_graph;
}

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
        static const char *type_names[] = {"POINT", "LINE", "REGION", "CIRCLE", "PORT", "FUNC_BLOCK"};
        const char *tname = (node->type >= 0 && node->type <= 5) ? type_names[node->type] : "UNKNOWN";
        lv_strbuf_printf(&sb, "添加节点 #%d (类型: %s)", node->id, tname);
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
