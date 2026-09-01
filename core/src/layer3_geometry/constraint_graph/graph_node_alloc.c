/**
 * @file graph_node_alloc.c
 * @brief ConstraintGraph 节点与约束生命周期管理 —— 节点/约束分配与生命周期创建
 *
 * @details 由 graph_node.c 按功能域拆分而来。
 *          共享内部函数声明见 graph_node_internal.h。
 *          分配核心段；VTable/深拷贝/事件发射见 graph_node_vtable.c / graph_node_copy.c / graph_node_emit.c。
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

/* K66/F96 P0 防御：活性预设池尺寸对拍——池块按 object_size 清零，结构体
 * 增字段超过池尺寸即池块越界写（K43 coeff_pool 尺寸失配同族）。
 * GeomNode/Constraint 为活性池（graph_node_alloc 池化分配）：
 *   sizeof(GeomNode)    <= lv_CONFIG_POOL_CONSTRAINT_NODE_SIZE (128)
 *   sizeof(Constraint)  <= lv_CONFIG_POOL_CONSTRAINT_SIZE      (96)
 * symbolic_coord / proof_step 池为死池（全库 0 使用，仅创建），
 * 对拍不适用（删除需评审，登记于 memory_pool.c）。 */
_Static_assert(sizeof(GeomNode) <= lv_CONFIG_POOL_CONSTRAINT_NODE_SIZE,
               "K66: sizeof(GeomNode) exceeds node pool block size (128)");
_Static_assert(sizeof(Constraint) <= lv_CONFIG_POOL_CONSTRAINT_SIZE,
               "K66: sizeof(Constraint) exceeds constraint pool block size (96)");

GeomNode *node_alloc_internal(ConstraintGraph *graph, GeomType type, int id, bool with_id,
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

    /* 复制坐标（graph_add_node_with_id 路径）
     * [修复] 允许 NULL 坐标元素：graph_add_point_xy(graph, NULL, NULL)
     * 表示未约束自由度，源图含 NULL 元素时（如 graph_copy 复制此类图）
     * 原实现对 NULL 调 symbolic_coord_copy(NULL) 返回 NULL 被误判为拷贝
     * 失败，导致 graph_copy 无法复制含未约束自由度的图。NULL 元素保持
     * NULL（等价于 graph_add_point_xy 的未约束语义），仅非 NULL 源坐标
     * 拷贝失败才报错。 */
    if (with_id && coord_count > 0 && coords) {
        node->symbolic_coords = lv_malloc((size_t) coord_count * sizeof(SymbolicCoord *));
        if (!node->symbolic_coords) {
            lv_pool_free(lv_get_node_pool(), node);
            lv_set_error_ctx(lv_ERROR_OUT_OF_MEMORY, __FILE__, __LINE__, fn, "%s: malloc symbolic_coords failed", fn);
            return NULL;
        }
        for (int i = 0; i < coord_count; i++) {
            if (coords[i]) {
                node->symbolic_coords[i] = symbolic_coord_copy(coords[i]);
                if (!node->symbolic_coords[i]) {
                    /* 坐标拷贝失败：清理已分配的坐标并返回 NULL */
                    for (int j = 0; j < i; j++) {
                        if (node->symbolic_coords[j])
                            symbolic_coord_destroy(node->symbolic_coords[j]);
                    }
                    lv_free((void **) &node->symbolic_coords);
                    lv_pool_free(lv_get_node_pool(), node);
                    lv_set_error_ctx(lv_ERROR_OUT_OF_MEMORY, __FILE__, __LINE__, fn, "%s: symbolic_coord_copy failed", fn);
                    return NULL;
                }
            } else {
                /* 未约束自由度：NULL 坐标合法（与 graph_add_point_xy NULL 语义一致） */
                node->symbolic_coords[i] = NULL;
            }
        }
        node->coord_count = coord_count;
    }

    /* 扩展数组（统一委托 lv_ensure_capacity，内部含 INT_MAX/SIZE_MAX 溢出检查与倍增；
     * graph_alloc_node 原走 graph_ensure_capacity（min_growth=1），
     * graph_add_node_with_id 原直接 lv_ensure_capacity（min_growth=0），保持原扩容行为） */
    if (!lv_ensure_capacity((void **) &graph->nodes, graph->node_count, &graph->node_capacity, sizeof(GeomNode *),
                            with_id ? 0 : 1)) {
        /* 清理已分配的坐标（NULL 元素跳过） */
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
    /* 反向索引版本失效：约束集合已变更，下次查询时惰性重建 */
    graph->constraints_version++;
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
    if (node_id >= lv_ATOMIC_LOAD(&graph->next_node_id)) {
        int expected = lv_ATOMIC_LOAD(&graph->next_node_id);
        int desired = node_id + 1;
        while (expected < desired) {
            desired = node_id + 1;
            if (lv_ATOMIC_CAS_BOOL(&graph->next_node_id, desired, &expected)) {
                break;
            }
            /* 失败后重读当前值（Windows 端 lv_ATOMIC_CAS_BOOL 不更新 expected） */
            expected = lv_ATOMIC_LOAD(&graph->next_node_id);
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

