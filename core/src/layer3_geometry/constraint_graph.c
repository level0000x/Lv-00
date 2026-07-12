/**
 * @file constraint_graph.c
 * @brief 约束图核心实现
 * @details 实现几何约束图的数据结构和操作，包括点、线段、区域、端口和函数块节点。
 *          支持 5 种约束类型（关联、中间、等距、角度、正交），
 *          提供哈希索引、冗余检测和冲突检测功能。
 *
 * 【错误系统迁移说明】
 * 本文件已完成从旧版 g_internal_error[256] 全局字符数组 + set_error()/clear_error()
 * 兼容层到统一错误系统（lv00_set_error / lv00_clear_error / lv00_get_error）的迁移。
 * 所有错误报告均已直接调用 lv00_set_error()，错误清除调用 lv00_clear_error()。
 * 旧的双轨错误系统已被完全移除，不再保留兼容层。
 */

/* ============================================================================
 * 魔法数字常量定义
 * ============================================================================ */

/**
 * @brief 每个节点的最大邻接约束数量
 * @details 用于邻接表的内存分配，超过此限制的约束将被静默忽略并记录警告
 */
#define LV00_ADJ_MAX_PER_NODE 256

/**
 * @brief 冲突检测中每个点的最大约束数量
 */
#define LV00_POINT_CONSTRAINT_ARRAY_SIZE 64

/**
 * @brief 连接图邻接矩阵的列步长
 */
#define LV00_MAX_CONN_ADJ_STRIDE 256

/**
 * @brief JSON 序列化缓冲区初始大小
 */
#define LV00_JSON_BUFFER_INITIAL_SIZE 1024

/**
 * @brief 节点/约束描述字符串缓冲区大小
 */
#define LV00_DESC_BUFFER_SIZE 128

#include "lv00/constraint_graph.h"
#include "symbolic_coord.h"     /* SymbolicCoord, TrustColor (brings rational.h) */

#include <assert.h>
#include <gmp.h>
#include <math.h>
#include <stdatomic.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "debug.h"
#include "error_codes.h"
#include "config.h"          /* LV00_ARRAY_GROWTH_FACTOR etc. */
#include "context.h"      /* v3.4.0: Lv00Context 用于统一错误系统 */
#include "lv00_internal.h"
#include "lv00_utils.h" /* lv00_malloc / lv00_free —— 统一内存分配器 */
#include "lv00/solver.h"
#include "stream.h"
#include "stream_context_util.h"

LV00_DECLARE_STREAM_CTX(graph);

/* Forward declarations for hash index functions
 * graph_node_index_insert 和 graph_constraint_index_insert 已公开为公共接口，
 * 供 func_block.c 在例化时将新节点/约束注册到哈希索引 */
void graph_node_index_insert(ConstraintGraph *graph, GeomNode *node);
static void node_index_remove(ConstraintGraph *graph, int node_id);
void graph_constraint_index_insert(ConstraintGraph *graph, Constraint *con);

/**
 * @brief 安全数组扩容辅助函数（委托给统一的 lv00_ensure_capacity）
 * @param arr 当前数组指针
 * @param count 当前元素数量
 * @param capacity 当前容量指针
 * @param elem_size 单个元素大小
 * @param min_growth 最小增长量
 * @return 扩容后的数组指针，失败返回 NULL
 * @note 内部委托给 lv00_ensure_capacity
 */
static void *graph_ensure_capacity(void *arr, int count, int *capacity,
                                    size_t elem_size, int min_growth) {
    void *arr_ptr = arr;
    if (!lv00_ensure_capacity(&arr_ptr, count, capacity, elem_size, min_growth))
        return NULL;
    return arr_ptr;
}

/**
 * @brief 在约束图中分配并初始化一个新的几何节点
 *
 * 分配 GeomNode 内存（lv00_malloc + memset），设置唯一 ID、类型、
 * 默认信任等级和命名空间深度。若节点数组容量不足，自动扩容
 * （按 LV00_ARRAY_GROWTH_FACTOR 倍增长），扩容失败时释放已分配节点并返回 NULL。
 *
 * @param graph 约束图指针
 * @param type  几何节点类型（GEOM_POINT / GEOM_SEGMENT / GEOM_REGION / GEOM_PORT / GEOM_FUNCTION_BLOCK）
 * @return 新分配的 GeomNode 指针，失败返回 NULL
 */
static GeomNode *graph_alloc_node(ConstraintGraph *graph, GeomType type) {
    GeomNode *node = lv00_malloc(sizeof(GeomNode));
    if (!node)
        return NULL;
    memset(node, 0, sizeof(GeomNode));
    /* v3.4.1: 使用原子操作分配节点ID，确保多线程安全 */
    node->id = GRAPH_ATOMIC_NODE_ID_INCREMENT(graph);
    node->type = type;
    node->trust = TRUST_GREEN;
    node->is_active = true;  /* v3.6.0: 新节点默认活跃 */
    node->namespace_depth = 0;
    node->parent_block_id = -1;
    GeomNode **new_nodes = (GeomNode **)graph_ensure_capacity(
        graph->nodes, graph->node_count, &graph->node_capacity,
        sizeof(GeomNode *), 1);
    if (!new_nodes) {
        lv00_free((void **) &node);
        return NULL;
    }
    graph->nodes = new_nodes;
    graph->nodes[graph->node_count++] = node;
    graph_node_index_insert(graph, node);
    return node;
}

/**
 * @brief 在约束图中分配并初始化一个新的约束
 *
 * 分配 Constraint 内存（lv00_malloc + memset），设置唯一 ID 和约束类型。
 * 若约束数组容量不足，自动按 LV00_ARRAY_GROWTH_FACTOR 倍扩容，
 * 扩容失败时释放已分配约束并返回 NULL。
 *
 * @param graph 约束图指针
 * @param type  约束类型（INCIDENCE / BETWEENNESS / INTERSECTION / CONTAINMENT / CONNECTION）
 * @return 新分配的 Constraint 指针，失败返回 NULL
 */
static Constraint *graph_alloc_constraint(ConstraintGraph *graph, ConstraintType type) {
    Constraint *con = lv00_malloc(sizeof(Constraint));
    if (!con)
        return NULL;
    memset(con, 0, sizeof(Constraint));
    /* v3.4.1: 使用原子操作分配约束ID，确保多线程安全 */
    con->id = GRAPH_ATOMIC_CONSTRAINT_ID_INCREMENT(graph);
    con->type = type;
    con->is_active = true;   /* v3.5.0: 新约束默认活跃 */
    Constraint **new_constraints = (Constraint **)graph_ensure_capacity(
        graph->constraints, graph->constraint_count, &graph->constraint_capacity,
        sizeof(Constraint *), 1);
    if (!new_constraints) {
        lv00_free((void **) &con);
        return NULL;
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
        return NULL;

    /* 检查ID是否已被使用 */
    if (graph_get_node(graph, node_id) != NULL) {
        lv00_set_error(LV00_ERROR_UNKNOWN, "%s", "Node ID already exists");
        return NULL;
    }

    GeomNode *node = lv00_malloc(sizeof(GeomNode));
    if (!node)
        return NULL;
    memset(node, 0, sizeof(GeomNode));

    node->id = node_id;
    node->type = type;
    node->trust = TRUST_GREEN;
    node->is_active = true;  /* v3.6.0: 新节点默认活跃 */
    node->namespace_depth = 0;
    node->parent_block_id = -1;

    /* 复制坐标 */
    if (coord_count > 0 && coords) {
        node->symbolic_coords = lv00_malloc((size_t) coord_count * sizeof(SymbolicCoord *));
        if (!node->symbolic_coords) {
            lv00_free((void **) &node);
            return NULL;
        }
        for (int i = 0; i < coord_count; i++) {
            node->symbolic_coords[i] = symbolic_coord_copy(coords[i]);
            if (!node->symbolic_coords[i]) {
                /* 坐标拷贝失败：清理已分配的坐标并返回 NULL */
                for (int j = 0; j < i; j++) {
                    symbolic_coord_destroy(node->symbolic_coords[j]);
                }
                lv00_free((void **) &node->symbolic_coords);
                lv00_free((void **) &node);
                return NULL;
            }
        }
        node->coord_count = coord_count;
    }

    /* 将节点添加到图中 */
    GeomNode **new_nodes = (GeomNode **)graph_ensure_capacity(
        graph->nodes, graph->node_count, &graph->node_capacity,
        sizeof(GeomNode *), 1);
    if (!new_nodes) {
        if (node->symbolic_coords) {
            for (int i = 0; i < coord_count; i++) {
                symbolic_coord_destroy(node->symbolic_coords[i]);
            }
            lv00_free((void **) &node->symbolic_coords);
        }
        lv00_free((void **) &node);
        return NULL;
    }
    graph->nodes = new_nodes;
    graph->nodes[graph->node_count++] = node;
    graph_node_index_insert(graph, node);
    return node;
}

/* ── 子模块已拆分至 constraint_graph/ ── */
