/**
 * @file graph_node.c
 * @brief ConstraintGraph 节点与约束生命周期管理
 *
 * @details 实现节点和约束的完整生命周期：
 *          - 节点创建：graph_create_point / graph_create_line /
 *            graph_create_circle 等各类型节点的构造与坐标初始化
 *          - 节点删除：graph_delete_node（级联删除关联约束）
 *          - 约束创建：graph_add_constraint（带类型验证与参与者兼容性检查）
 *          - 约束删除：graph_delete_constraint（更新邻接矩阵）
 *          - 安全扩容：动态数组的容量管理与 realloc 原子性保证
 *
 *          内存安全策略：
 *          - 所有数组扩容使用临时变量保存 realloc 结果，
 *            失败时不破坏原有数组状态
 *          - 节点删除前检查所有约束的参与者引用，级联清理
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

/* Forward declarations for hash index functions
 * graph_node_index_insert 和 graph_constraint_index_insert 已公开为公共接口，
 * 供 func_block.c 在例化时将新节点/约束注册到哈希索引 */
void graph_node_index_insert(ConstraintGraph *graph, GeomNode *node);
void node_index_remove(ConstraintGraph *graph, int node_id);
void graph_constraint_index_insert(ConstraintGraph *graph, Constraint *con);

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
static GeomNode *graph_alloc_node(ConstraintGraph *graph, GeomType type) {
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
    con->participant_count = participant_count;
    con->participants = lv_malloc((size_t) participant_count * sizeof(int));
    if (!con->participants) {
        lv_free((void **) &con);
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "graph_add_constraint_with_id: malloc participants failed");
    }
    memcpy(con->participants, participants, participant_count * sizeof(int));

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

/* ============================================================================
 * 节点哈希索引：用于 O(1) 节点查找
 * 初始大小和负载因子定义在 lv_internal.h 中
 * ============================================================================ */

/**
 * @brief 计算节点ID的哈希值
 * @param id 节点ID
 * @param capacity 哈希表容量
 * @return 哈希值
 */
unsigned node_id_hash(int id, int capacity) {
    /* FNV-1a-like hash，乘数定义在 lv_internal.h 中 */
    unsigned h = (unsigned) id * lv_FNV_HASH_MULTIPLIER;
    /*
     * 运行时检查：哈希表容量通常为 2 的幂，此时使用高效的位掩码取模。
     * 若容量不是 2 的幂（防御性场景），回退到安全的取模操作，
     * 避免在 Release 构建中因 assert 被移除而导致未定义行为。
     */
    if (capacity > 0 && (capacity & (capacity - 1)) == 0) {
        return h & (unsigned) (capacity - 1);
    } else {
        return h % (unsigned) (capacity > 0 ? capacity : 1);
    }
}

/**
 * @brief 确保节点哈希索引有足够容量
 * @param graph 约束图指针
 * @return 成功返回 true，失败返回 false
 */
static bool node_index_ensure_capacity(ConstraintGraph *graph) {
    if (!graph->node_index) {
        int cap = lv_NODE_INDEX_INITIAL_SIZE;
        graph->node_index = lv_calloc(cap, sizeof(GeomNode *));
        if (!graph->node_index)
            lv_RETURN_ERROR_BOOL(lv_ERROR_OUT_OF_MEMORY, "node_index_ensure_capacity: calloc node_index failed");
        graph->node_index_capacity = cap;
        /* 重新插入所有现有节点 */
        for (int i = 0; i < graph->node_count; i++) {
            unsigned idx = node_id_hash(graph->nodes[i]->id, cap);
            while (graph->node_index[idx] != NULL) {
                idx = (idx + 1) & (unsigned) (cap - 1);
            }
            graph->node_index[idx] = graph->nodes[i];
        }
        return true;
    }

    /* 检查负载因子 */
    if (graph->node_count < (int) (graph->node_index_capacity * lv_INDEX_LOAD_FACTOR)) {
        return true;
    }

    /* 重新哈希到更大的表 */
    if (graph->node_index_capacity > INT_MAX / 2)
        lv_RETURN_ERROR_BOOL(lv_ERROR_OVERFLOW, "node_index_ensure_capacity: capacity overflow");
    int new_cap = graph->node_index_capacity * 2;
    GeomNode **new_index = lv_calloc(new_cap, sizeof(GeomNode *));
    if (!new_index)
        lv_RETURN_ERROR_BOOL(lv_ERROR_OUT_OF_MEMORY, "node_index_ensure_capacity: calloc new_index failed");

    for (int i = 0; i < graph->node_count; i++) {
        unsigned idx = node_id_hash(graph->nodes[i]->id, new_cap);
        while (new_index[idx] != NULL) {
            idx = (idx + 1) & (unsigned) (new_cap - 1);
        }
        new_index[idx] = graph->nodes[i];
    }

    lv_free((void **) &graph->node_index);
    graph->node_index = new_index;
    graph->node_index_capacity = new_cap;
    return true;
}

/**
 * @brief 将节点插入哈希索引
 *
 * 确保哈希索引有足够容量后，使用开放寻址法将节点插入到
 * 对应的哈希槽中。若哈希冲突则线性探测下一个可用槽位。
 *
 * @param graph 约束图指针
 * @param node  要插入的几何节点指针
 */
void graph_node_index_insert(ConstraintGraph *graph, GeomNode *node) {
    if (!node_index_ensure_capacity(graph))
        return;
    unsigned idx = node_id_hash(node->id, graph->node_index_capacity);
    while (graph->node_index[idx] != NULL) {
        idx = (idx + 1) & (unsigned) (graph->node_index_capacity - 1);
    }
    graph->node_index[idx] = node;
}

/**
 * @brief 从节点哈希索引中移除节点
 * @param graph 约束图指针
 * @param node_id 要移除的节点ID
 */
void node_index_remove(ConstraintGraph *graph, int node_id) {
    if (!graph->node_index)
        return;
    unsigned idx = node_id_hash(node_id, graph->node_index_capacity);
    while (graph->node_index[idx] != NULL && graph->node_index[idx]->id != node_id) {
        idx = (idx + 1) & (unsigned) (graph->node_index_capacity - 1);
    }
    if (graph->node_index[idx] == NULL)
        return;

    /* 开放寻址删除：重新插入此槽之后的所有条目 */
    graph->node_index[idx] = NULL;
    unsigned i = (idx + 1) & (unsigned) (graph->node_index_capacity - 1);
    while (graph->node_index[i] != NULL) {
        GeomNode *entry = graph->node_index[i];
        graph->node_index[i] = NULL;
        unsigned ideal = node_id_hash(entry->id, graph->node_index_capacity);
        /* 判断理想位置是否在被删除槽的影响范围内 */
        unsigned cap = graph->node_index_capacity;
        unsigned range = (unsigned) ((i - idx + cap) % cap);
        unsigned ideal_dist = (unsigned) ((ideal - idx + cap) % cap);
        bool in_range = (ideal_dist <= range);
        if (in_range) {
            /* 条目属于被删除槽的影响范围，需要重新插入 */
            unsigned j = ideal;
            while (graph->node_index[j] != NULL) {
                j = (j + 1) & (unsigned) (graph->node_index_capacity - 1);
                if (j == ideal) {
                    /* 哈希表探测绕回起点，条目无法重新插入 —— 记录错误 */
                    lv_set_error(lv_ERROR_INTERNAL, "哈希表已满，无法重新插入条目（ID=%d）", entry->id);
                    break;
                }
            }
            if (graph->node_index[j] == NULL)
                graph->node_index[j] = entry;
        } else {
            /* 条目不在影响范围内，保持原位 */
            graph->node_index[i] = entry;
        }
        i = (i + 1) & (unsigned) (graph->node_index_capacity - 1);
    }
}

/* ================================================================
 *  约束哈希索引：用于 O(1) 按ID查找约束
 *  初始大小和负载因子定义在 lv_internal.h 中
 * ================================================================ */

/**
 * @brief 计算约束ID的哈希值
 * @param id 约束ID
 * @param capacity 哈希表容量
 * @return 哈希值
 */
unsigned constraint_id_hash(int id, int capacity) {
    unsigned h = (unsigned) id * lv_FNV_HASH_MULTIPLIER;
    return h & (unsigned) (capacity - 1);
}

/**
 * @brief 确保约束哈希索引有足够容量
 * @param graph 约束图指针
 * @return 成功返回 true，失败返回 false
 */
static bool constraint_index_ensure_capacity(ConstraintGraph *graph) {
    if (!graph->constraint_index) {
        int cap = lv_CONSTRAINT_INDEX_INITIAL_SIZE;
        graph->constraint_index = lv_calloc(cap, sizeof(Constraint *));
        if (!graph->constraint_index)
            lv_RETURN_ERROR_BOOL(lv_ERROR_OUT_OF_MEMORY, "constraint_index_ensure_capacity: calloc constraint_index failed");
        graph->constraint_index_capacity = cap;
        for (int i = 0; i < graph->constraint_count; i++) {
            unsigned idx = constraint_id_hash(graph->constraints[i]->id, cap);
            while (graph->constraint_index[idx] != NULL) {
                idx = (idx + 1) & (unsigned) (cap - 1);
            }
            graph->constraint_index[idx] = graph->constraints[i];
        }
        return true;
    }

    if (graph->constraint_count < (int) (graph->constraint_index_capacity * lv_INDEX_LOAD_FACTOR)) {
        return true;
    }

    if (graph->constraint_index_capacity > INT_MAX / 2)
        lv_RETURN_ERROR_BOOL(lv_ERROR_OVERFLOW, "constraint_index_ensure_capacity: capacity overflow");
    int new_cap = graph->constraint_index_capacity * 2;
    Constraint **new_index = lv_calloc(new_cap, sizeof(Constraint *));
    if (!new_index)
        lv_RETURN_ERROR_BOOL(lv_ERROR_OUT_OF_MEMORY, "constraint_index_ensure_capacity: calloc new_index failed");

    for (int i = 0; i < graph->constraint_count; i++) {
        unsigned idx = constraint_id_hash(graph->constraints[i]->id, new_cap);
        while (new_index[idx] != NULL) {
            idx = (idx + 1) & (unsigned) (new_cap - 1);
        }
        new_index[idx] = graph->constraints[i];
    }

    lv_free((void **) &graph->constraint_index);
    graph->constraint_index = new_index;
    graph->constraint_index_capacity = new_cap;
    return true;
}

/**
 * @brief 将约束插入哈希索引
 *
 * 确保哈希索引有足够容量后，使用开放寻址法将约束插入到
 * 对应的哈希槽中。若哈希冲突则线性探测下一个可用槽位。
 *
 * @param graph 约束图指针
 * @param con   要插入的约束指针
 */
void graph_constraint_index_insert(ConstraintGraph *graph, Constraint *con) {
    if (!constraint_index_ensure_capacity(graph))
        return;
    unsigned idx = constraint_id_hash(con->id, graph->constraint_index_capacity);
    while (graph->constraint_index[idx] != NULL) {
        idx = (idx + 1) & (unsigned) (graph->constraint_index_capacity - 1);
    }
    graph->constraint_index[idx] = con;
}

/**
 * @brief 从约束哈希索引中移除约束
 * @param graph 约束图指针
 * @param constraint_id 要移除的约束ID
 */
void constraint_index_remove(ConstraintGraph *graph, int constraint_id) {
    if (!graph || !graph->constraint_index || graph->constraint_index_capacity == 0)
        return;

    unsigned capacity = (unsigned) graph->constraint_index_capacity;
    unsigned idx = constraint_id_hash(constraint_id, capacity);
    unsigned i = idx;

    /* 查找要删除的 entry */
    while (graph->constraint_index[i] != NULL) {
        if (graph->constraint_index[i]->id == constraint_id)
            break;
        i = (i + 1) & (capacity - 1);
        if (i == idx)
            return; /* 未找到 */
    }
    if (graph->constraint_index[i] == NULL)
        return; /* 未找到 */

    /* 向后移位删除 */
    unsigned j = i;
    unsigned next = (i + 1) & (capacity - 1);
    while (graph->constraint_index[next] != NULL) {
        unsigned k = constraint_id_hash(graph->constraint_index[next]->id, capacity);
        /* 检查 k 是否不在 (j, next] 的开区间内（循环意义上） */
        bool in_range;
        if (j <= next) {
            in_range = (k > j && k <= next);
        } else {
            in_range = (k > j || k <= next);
        }
        if (!in_range) {
            graph->constraint_index[j] = graph->constraint_index[next];
            j = next;
        }
        next = (next + 1) & (capacity - 1);
    }
    graph->constraint_index[j] = NULL;
}

/* ================================================================
 *  线段相交测试（参数化）
 * ================================================================ */

/**
 * @brief 检查两条线段是否在其内部相交
 * @param seg_a 第一个线段节点
 * @param seg_b 第二个线段节点
 * @return 线段内部相交（t,u 都在 (0,1) 区间内）返回 true
 *
 * 使用参数化线段相交算法：
 *   线段 A: P + t*(Q-P), t in [0,1]
 *   线段 B: R + u*(S-R), u in [0,1]
 */
static bool segments_intersect(const GeomNode *seg_a, const GeomNode *seg_b) {
    if (!seg_a || !seg_b)
        lv_RETURN_ERROR_BOOL(lv_ERROR_NULL_POINTER, "segments_intersect: NULL segment");
    if (seg_a->type != GEOM_LINE_SEGMENT || seg_b->type != GEOM_LINE_SEGMENT)
        return false;
    if (seg_a->coord_count < 4 || seg_b->coord_count < 4)
        return false;
    if (!seg_a->symbolic_coords || !seg_b->symbolic_coords)
        return false;

    /* 提取端点坐标为双精度浮点数 */
    double ax1 = symbolic_coord_to_double(seg_a->symbolic_coords[0]);
    double ay1 = symbolic_coord_to_double(seg_a->symbolic_coords[1]);
    double ax2 = symbolic_coord_to_double(seg_a->symbolic_coords[2]);
    double ay2 = symbolic_coord_to_double(seg_a->symbolic_coords[3]);
    double bx1 = symbolic_coord_to_double(seg_b->symbolic_coords[0]);
    double by1 = symbolic_coord_to_double(seg_b->symbolic_coords[1]);
    double bx2 = symbolic_coord_to_double(seg_b->symbolic_coords[2]);
    double by2 = symbolic_coord_to_double(seg_b->symbolic_coords[3]);

    double dx_a = ax2 - ax1, dy_a = ay2 - ay1;
    double dx_b = bx2 - bx1, dy_b = by2 - by1;
    double denom = dx_a * dy_b - dy_a * dx_b;

    if (fabs(denom) < lv_EPSILON_DOUBLE)
        return false; /* 平行或共线 */

    double t = ((bx1 - ax1) * dy_b - (by1 - ay1) * dx_b) / denom;
    double u = ((bx1 - ax1) * dy_a - (by1 - ay1) * dx_a) / denom;

    /* 严格内部相交检查 */
    return (t > lv_EPSILON_SEGMENT_INTERIOR && t < 1.0 - lv_EPSILON_SEGMENT_INTERIOR &&
            u > lv_EPSILON_SEGMENT_INTERIOR && u < 1.0 - lv_EPSILON_SEGMENT_INTERIOR);
}

/* ================================================================
 *  增量代数冲突检测
 * ================================================================ */

/**
 * 检查新约束是否与现有约束代数冲突。
 *
 * 使用 GMP mpq_t 进行精确有理数运算：
 * - INCIDENCE：检查点是否已约束在另一条不相交线上
 * - BETWEENNESS：检查三点是否已约束为非共线
 * - INTERSECTION：检查两线是否已声明为平行
 * - CONTAINMENT：检查内部节点是否已在外部
 * - CONNECTION：无代数冲突可能
 *
 * @param graph         约束图指针
 * @param new_constraint 新约束指针
 * @return true 表示存在冲突，false 表示无冲突
 */
bool check_incremental_conflict(const ConstraintGraph *graph, const Constraint *new_constraint) {
    if (!graph || !new_constraint)
        return false;

    switch (new_constraint->type) {
        case INCIDENCE: {
            /* 点不应已被约束在两条不相交线上（这在2D中会过度约束） */
            int point_id = new_constraint->participants[0];
            int line_id = new_constraint->participants[1];
            int incident_count = 0;
            for (int i = 0; i < graph->constraint_count; i++) {
                Constraint *c = graph->constraints[i];
                if (c->type != INCIDENCE)
                    continue;
                if (c->participants[0] == point_id && c->participants[1] != line_id) {
                    incident_count++;
                }
            }
            /* 在2D中，点在2条不同线上是可以的（构成交点）
         * 但3条或更多条线且没有相交约束则是冲突 */
            if (incident_count >= 2) {
                /* 检查是否有任意一对关联线之间有 INTERSECTION 约束 */
                /* v3.4.2: 使用动态分配替代固定大小数组，避免缓冲区溢出风险 */
                int *lines = NULL;
                int line_count = 0;
                int lines_capacity = 8; /* 初始容量 */

                lines = (int *) lv_malloc(sizeof(int) * lines_capacity);
                if (!lines) {
                    lv_LOG_ERROR("check_incremental_conflict: 内存分配失败");
                    return false; /* 内存不足，保守返回无冲突 */
                }

                for (int i = 0; i < graph->constraint_count; i++) {
                    Constraint *c = graph->constraints[i];
                    if (c->type == INCIDENCE && c->participants[0] == point_id) {
                        /* v3.4.2: 动态扩容 */
                        if (line_count >= lines_capacity) {
                            int new_cap = lines_capacity * 2;
                            if (new_cap < lines_capacity) { /* 溢出检查 */
                                lv_free((void **) &lines);
                                return false;
                            }
                            int *new_lines = (int *) lv_realloc(lines, sizeof(int) * new_cap);
                            if (!new_lines) {
                                lv_free((void **) &lines);
                                return false;
                            }
                            lines = new_lines;
                            lines_capacity = new_cap;
                        }
                        lines[line_count++] = c->participants[1];
                    }
                }
                /* 包含新添加的线 */
                if (line_count >= lines_capacity) {
                    int new_cap = lines_capacity * 2;
                    if (new_cap < lines_capacity) {
                        lv_free((void **) &lines);
                        return false;
                    }
                    int *new_lines = (int *) lv_realloc(lines, sizeof(int) * new_cap);
                    if (!new_lines) {
                        lv_free((void **) &lines);
                        return false;
                    }
                    lines = new_lines;
                    lines_capacity = new_cap;
                }
                lines[line_count++] = line_id;

                /* 检查所有线对的相交约束 */
                bool conflict_found = false;
                for (int a = 0; a < line_count && !conflict_found; a++) {
                    for (int b = a + 1; b < line_count && !conflict_found; b++) {
                        bool has_intersection = false;
                        for (int i = 0; i < graph->constraint_count; i++) {
                            Constraint *c = graph->constraints[i];
                            if (c->type == INTERSECTION && c->participant_count == 3) {
                                if ((c->participants[0] == lines[a] && c->participants[1] == lines[b]) ||
                                    (c->participants[0] == lines[b] && c->participants[1] == lines[a])) {
                                    has_intersection = true;
                                    break;
                                }
                            }
                        }
                        if (!has_intersection) {
                            /* 两条线共点但没有相交约束 - 如果线平行则是潜在冲突 */
                            conflict_found = true;
                        }
                    }
                }

                /* v3.4.2: 释放动态分配的数组 */
                lv_free((void **) &lines);

                if (conflict_found) {
                    return true;
                }
            }
            break;
        }
        case BETWEENNESS: {
            /* 检查三个点是否已被约束为非共线（如，各自位于不同线上） */
            int p1 = new_constraint->participants[0];
            int p2 = new_constraint->participants[1];
            int p3 = new_constraint->participants[2];

            /* 收集每个点所在的线 */
            int p1_lines[64], p1_lc = 0;
            int p2_lines[64], p2_lc = 0;
            int p3_lines[64], p3_lc = 0;
            for (int i = 0; i < graph->constraint_count; i++) {
                Constraint *c = graph->constraints[i];
                if (c->type != INCIDENCE)
                    continue;
                int pid = c->participants[0], lid = c->participants[1];
                if (pid == p1 && p1_lc < 64)
                    p1_lines[p1_lc++] = lid;
                if (pid == p2 && p2_lc < 64)
                    p2_lines[p2_lc++] = lid;
                if (pid == p3 && p3_lc < 64)
                    p3_lines[p3_lc++] = lid;
            }
            /* 如果没有两个点共享一条线，则它们不可能共线 */
            bool any_shared = false;
            for (int i = 0; i < p1_lc && !any_shared; i++) {
                for (int j = 0; j < p2_lc && !any_shared; j++) {
                    if (p1_lines[i] == p2_lines[j])
                        any_shared = true;
                }
            }
            if (!any_shared) {
                for (int i = 0; i < p2_lc && !any_shared; i++) {
                    for (int j = 0; j < p3_lc && !any_shared; j++) {
                        if (p2_lines[i] == p3_lines[j])
                            any_shared = true;
                    }
                }
            }
            if (!any_shared) {
                for (int i = 0; i < p1_lc && !any_shared; i++) {
                    for (int j = 0; j < p3_lc && !any_shared; j++) {
                        if (p1_lines[i] == p3_lines[j])
                            any_shared = true;
                    }
                }
            }
            if (!any_shared)
                return true; /* 冲突：不可能共线 */
            break;
        }
        case INTERSECTION: {
            /* Check if the two lines are already known to be parallel
         * (no intersection possible). For now, we check if there's
         * already an INTERSECTION constraint for the same pair with
         * a different result point. */
            int l1 = new_constraint->participants[0];
            int l2 = new_constraint->participants[1];
            for (int i = 0; i < graph->constraint_count; i++) {
                Constraint *c = graph->constraints[i];
                if (c->type == INTERSECTION && c->participant_count == 3) {
                    if ((c->participants[0] == l1 && c->participants[1] == l2) ||
                        (c->participants[0] == l2 && c->participants[1] == l1)) {
                        /* Already have an intersection for this pair */
                        if (c->participants[2] != new_constraint->participants[2]) {
                            return true; /* Different result point = conflict */
                        }
                    }
                }
            }
            break;
        }
        case CONTAINMENT:
        case CONNECTION:
        case ANGLE:
            /* No simple algebraic conflict check for these types */
            break;
        default:
            /* v3.5.0: 未知约束类型，记录错误 */
            lv_set_error(lv_ERROR_UNKNOWN, "check_incremental_conflict: 未知约束类型 %d (constraint id=%d)",
                         (int) new_constraint->type, new_constraint->id);
            break;
    }
    return false;
}

/**
 * 在约束图中添加点节点。
 *
 * @param graph       约束图指针
 * @param coords      点的坐标数组（每个坐标对应一个自由度）
 * @param coord_count 坐标数量
 * @return 添加结果状态
 */
AddNodeResult graph_add_point(ConstraintGraph *graph, SymbolicCoord *const *coords, int coord_count) {
    if (!graph || (coord_count > 0 && !coords))
        return ADD_NODE_CONFLICT;
    GeomNode *node = graph_alloc_node(graph, GEOM_POINT);
    if (!node)
        return ADD_NODE_CONFLICT;
    node->symbolic_coords = lv_malloc((size_t) coord_count * sizeof(SymbolicCoord *));
    if (!node->symbolic_coords) {
        graph->node_count--;
        node_index_remove(graph, node->id);
        lv_free((void **) &node);
        return ADD_NODE_CONFLICT;
    }
    /* 深拷贝坐标，使节点拥有这些坐标 */
    for (int i = 0; i < coord_count; i++) {
        node->symbolic_coords[i] = coords[i] ? symbolic_coord_copy(coords[i]) : NULL;
        if (coords[i] && !node->symbolic_coords[i]) {
            /* 坐标拷贝失败：清理已分配的坐标并回滚节点添加 */
            for (int j = 0; j < i; j++) {
                symbolic_coord_destroy(node->symbolic_coords[j]);
            }
            lv_free((void **) &node->symbolic_coords);
            node->symbolic_coords = NULL;
            node->coord_count = 0;
            graph->node_count--;
            node_index_remove(graph, node->id);
            lv_free((void **) &node);
            return ADD_NODE_CONFLICT;
        }
    }
    node->coord_count = coord_count;
    if (graph_stream_ctx) {
        lvStrBuf sb_3 = {0};
        lv_strbuf_printf(&sb_3, "添加点节点: id=%d", node->id);
        stream_emit_simple(graph_stream_ctx, STREAM_EVENT_NODE_ADDED, sb_3.data, 0);
        lv_strbuf_destroy(&sb_3);
    }
    return ADD_NODE_OK;
}

/**
 * @brief 添加线段节点（由两个端点定义）
 *
 * 创建线段类型的几何节点，验证端点存在性，并自动添加端点关联约束。
 *
 * @param graph         约束图指针
 * @param endpoint1_id 第一个端点节点 ID
 * @param endpoint2_id 第二个端点节点 ID
 * @return 操作结果枚举
 */
AddNodeResult graph_add_line_segment(ConstraintGraph *graph, int endpoint1_id, int endpoint2_id) {
    GeomNode *n1 = graph_get_node(graph, endpoint1_id);
    GeomNode *n2 = graph_get_node(graph, endpoint2_id);
    if (!n1 || !n2)
        return ADD_NODE_CONFLICT;
    if (n1->type != GEOM_POINT || n2->type != GEOM_POINT)
        return ADD_NODE_CONFLICT;
    GeomNode *node = graph_alloc_node(graph, GEOM_LINE_SEGMENT);
    if (!node)
        return ADD_NODE_CONFLICT;

    /* 计算线段需要的坐标总数：两个端点的坐标之和，至少为4个 */
    int total_coords = n1->coord_count + n2->coord_count;
    /* 如果端点没有坐标，至少保留4个位置以容纳 (x1,y1,x2,y2) */
    if (total_coords < 4)
        total_coords = 4;

    node->symbolic_coords = lv_malloc((size_t) total_coords * sizeof(SymbolicCoord *));
    if (!node->symbolic_coords) {
        graph->node_count--;
        node_index_remove(graph, node->id);
        lv_free((void **) &node);
        return ADD_NODE_CONFLICT;
    }

    /* 拷贝端点1的所有坐标 */
    int coord_idx = 0;
    if (n1->symbolic_coords && n1->coord_count > 0) {
        for (int i = 0; i < n1->coord_count; i++) {
            node->symbolic_coords[coord_idx++] = symbolic_coord_copy(n1->symbolic_coords[i]);
        }
    } else {
        /* 端点1无坐标时填充NULL（保持 x1 位置） */
        node->symbolic_coords[coord_idx++] = NULL;
    }

    /* 拷贝端点2的所有坐标 */
    if (n2->symbolic_coords && n2->coord_count > 0) {
        for (int i = 0; i < n2->coord_count; i++) {
            node->symbolic_coords[coord_idx++] = symbolic_coord_copy(n2->symbolic_coords[i]);
        }
    } else {
        /* 端点2无坐标时填充NULL（保持 x2 位置） */
        node->symbolic_coords[coord_idx++] = NULL;
    }

    /* 将剩余位置初始化为 NULL */
    for (int i = coord_idx; i < total_coords; i++) {
        node->symbolic_coords[i] = NULL;
    }

    node->coord_count = coord_idx;
    return ADD_NODE_OK;
}

/**
 * 在约束图中添区域节点。
 *
 * 区域由边界线段围成，边界线段必须已存在于图中。
 *
 * @param graph                约束图指针
 * @param boundary_segment_ids  边界线段节点 ID 数组
 * @param segment_count        边界线段数量
 * @return 添加结果状态
 */
static bool graph_coord_equal_for_compatibility(const SymbolicCoord *a, const SymbolicCoord *b) {
    if (!a || !b)
        return false;
    if (a->type != b->type)
        return false;
    if (a->type == RATIONAL)
        return a->data.rational && b->data.rational && rational_compare(a->data.rational, b->data.rational) == 0;
    return symbolic_coord_compare(a, b) == 0;
}

static bool graph_segment_is_degenerate(const GeomNode *segment) {
    if (!segment || segment->type != GEOM_LINE_SEGMENT)
        return false;
    if (!segment->symbolic_coords || segment->coord_count < 4)
        return true;
    if (!segment->symbolic_coords[0] || !segment->symbolic_coords[1] || !segment->symbolic_coords[2] ||
        !segment->symbolic_coords[3])
        return true;
    return graph_coord_equal_for_compatibility(segment->symbolic_coords[0], segment->symbolic_coords[2]) &&
           graph_coord_equal_for_compatibility(segment->symbolic_coords[1], segment->symbolic_coords[3]);
}

static bool graph_segments_have_same_endpoints(const GeomNode *a, const GeomNode *b) {
    if (!a || !b || a->type != GEOM_LINE_SEGMENT || b->type != GEOM_LINE_SEGMENT)
        return false;
    if (!a->symbolic_coords || !b->symbolic_coords || a->coord_count < 4 || b->coord_count < 4)
        return false;
    for (int i = 0; i < 4; i++) {
        if (!a->symbolic_coords[i] || !b->symbolic_coords[i])
            return false;
    }
    bool same_direction = graph_coord_equal_for_compatibility(a->symbolic_coords[0], b->symbolic_coords[0]) &&
                          graph_coord_equal_for_compatibility(a->symbolic_coords[1], b->symbolic_coords[1]) &&
                          graph_coord_equal_for_compatibility(a->symbolic_coords[2], b->symbolic_coords[2]) &&
                          graph_coord_equal_for_compatibility(a->symbolic_coords[3], b->symbolic_coords[3]);
    bool reverse_direction = graph_coord_equal_for_compatibility(a->symbolic_coords[0], b->symbolic_coords[2]) &&
                             graph_coord_equal_for_compatibility(a->symbolic_coords[1], b->symbolic_coords[3]) &&
                             graph_coord_equal_for_compatibility(a->symbolic_coords[2], b->symbolic_coords[0]) &&
                             graph_coord_equal_for_compatibility(a->symbolic_coords[3], b->symbolic_coords[1]);
    return same_direction || reverse_direction;
}

/**
 * @brief 计算约束的自由度 (DOF) 消耗量
 *
 * 根据约束类型返回该约束消耗的自由度数：
 *   - INCIDENCE (关联): 点在线段/区域上 → 1 DOF
 *   - BETWEENNESS (介于): 点在两点之间 → 1 DOF
 *   - INTERSECTION (相交): 两线在交点相交 → 1 DOF
 *   - CONTAINMENT (包含): 对象在另一对象内 → 1 DOF
 *   - CONNECTION (连接): 端口间数据流 → 0 DOF（非几何约束）
 *
 * @param con 约束指针
 * @return 该约束消耗的自由度数
 */
static int constraint_dof_cost(const Constraint *con) {
    if (!con)
        return 0;
    switch (con->type) {
        case INCIDENCE:    /* 关联约束：点在线段/区域上 */
        case BETWEENNESS:  /* 之间约束：三点共线有序 */
        case INTERSECTION: /* 相交约束：两线交于一点 */
        case CONTAINMENT:  /* 包含约束：对象在另一对象内 */
            return 1;
        case CONNECTION: /* 连接约束：数据流连接，非几何约束 */
            return 0;
        case ANGLE:      /* 角度约束：两条线段之间的夹角 */
            return 1;
        default:
            /* 未知约束类型：保守按 1 DOF 消耗计 */
            return 1;
    }
}

bool graph_check_compatibility(const ConstraintGraph *graph, lvConstraintCompatibilityResult *out_result) {
    if (out_result) {
        out_result->status = lv_CONSTRAINT_STATUS_INVALID;
        out_result->conflicting_constraint_id = -1;
        out_result->redundant_constraint_count = 0;
        out_result->free_degree_count = 0;
        out_result->diagnostic = "输入无效";
    }
    if (!graph || !out_result)
        lv_RETURN_ERROR_BOOL(lv_ERROR_NULL_POINTER, "graph_check_compatibility: NULL graph or out_result");

    if (graph->node_count == 0) {
        out_result->status = lv_CONSTRAINT_STATUS_UNDER_CONSTRAINED;
        out_result->free_degree_count = 1;
        out_result->diagnostic = "空约束图缺少几何事实";
        return true;
    }

    /* ================================================================
     * 阶段 1: 统计活跃几何节点并检测退化线段
     *
     * DOF 模型：
     *   - 每个点节点 (GEOM_POINT):   2 DOF (x, y)
     *   - 每个区域节点 (GEOM_REGION): 2 DOF (位置)
     *   - 每条线段 (GEOM_LINE_SEGMENT): 不增加 DOF，但提供 4 DOF 约束
     *     （两个端点 x 2 坐标，固定线段后两端点相对位置完全确定）
     *   - 每条线段的长度是固有属性（已隐含），线段体可平移（2 DOF 刚体运动）
     *     但端点坐标作为约束消耗 4 DOF
     * ================================================================ */
    int active_geometry_nodes = 0;
    int active_segment_count = 0;
    int segment_constraint_bonus = 0;
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node)
            continue;
        /* v3.6.0: 跳过已废弃（不活跃）的节点 */
        if (!node->is_active)
            continue;
        if (node->type == GEOM_LINE_SEGMENT) {
            if (graph_segment_is_degenerate(node)) {
                out_result->status = lv_CONSTRAINT_STATUS_INCONSISTENT;
                out_result->conflicting_constraint_id = node->id;
                out_result->diagnostic = "检测到由重合端点构成的退化线段";
                return true;
            }
            active_segment_count++;
            /* 每条线段连接两个端点，完全确定其相对位置（4 DOF 约束） */
            segment_constraint_bonus += 4;
        } else if (node->type == GEOM_POINT || node->type == GEOM_REGION || node->type == GEOM_CIRCLE) {
            active_geometry_nodes++;
        }
    }

    /* 边界情况：所有节点均不活跃 */
    if (active_geometry_nodes == 0 && active_segment_count == 0) {
        out_result->status = lv_CONSTRAINT_STATUS_UNDER_CONSTRAINED;
        out_result->free_degree_count = 1;
        out_result->diagnostic = "约束图中无活跃几何节点";
        return true;
    }

    /* ================================================================
     * 阶段 2: 检测重复线段（退化冗余）
     * ================================================================ */
    int redundant_count = 0;
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *left = graph->nodes[i];
        if (!left || !left->is_active || left->type != GEOM_LINE_SEGMENT)
            continue;
        for (int j = i + 1; j < graph->node_count; j++) {
            GeomNode *right = graph->nodes[j];
            if (!right || !right->is_active)
                continue;
            if (graph_segments_have_same_endpoints(left, right))
                redundant_count++;
        }
    }
    out_result->redundant_constraint_count = redundant_count;
    if (redundant_count > 0) {
        out_result->status = lv_CONSTRAINT_STATUS_OVER_CONSTRAINED;
        out_result->diagnostic = "检测到重复线段约束";
        return true;
    }

    /* ================================================================
     * 阶段 3: 自由度 (DOF) 计算
     *
     * 总自由度 = 活跃点/区域节点 × 2
     * 约束消耗  = 线段约束奖励 + 各类型几何约束消耗
     * 剩余自由度 = 总自由度 - 约束消耗
     *
     * 约束类型 DOF 消耗：
     *   INCIDENCE (关联):    1 DOF  -- 点在线上
     *   BETWEENNESS (介于):  1 DOF  -- 三点共线
     *   INTERSECTION (相交): 1 DOF  -- 两线交于一点
     *   CONTAINMENT (包含):  1 DOF  -- 对象在内
     *   CONNECTION (连接):   0 DOF  -- 数据流，非几何
     *   DISTANCE (距离):     1 DOF  -- (通过 template_id >= 100 识别)
     *   ANGLE (角度):        1 DOF  -- (通过 template_id >= 100 识别)
     *   PERPENDICULAR (垂直): 1 DOF -- (通过 template_id >= 100 识别)
     *   PARALLEL (平行):     1 DOF  -- (通过 template_id >= 100 识别)
     * ================================================================ */
    int total_dof = active_geometry_nodes * 2;
    int active_constraint_count = segment_constraint_bonus;
    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *con = graph->constraints[i];
        if (!con || !con->is_active)
            continue;
        active_constraint_count += constraint_dof_cost(con);
    }
    int free_dof = total_dof - active_constraint_count;

    /* ================================================================
     * 阶段 4: 根据剩余自由度判定约束状态
     *   - free_dof <  0  → 过约束 (OVER_CONSTRAINED)
     *   - free_dof == 0  → 恰好约束 (CONSISTENT)
     *   - free_dof >  0  → 欠约束 (UNDER_CONSTRAINED)
     * ================================================================ */
    if (free_dof < 0) {
        out_result->status = lv_CONSTRAINT_STATUS_OVER_CONSTRAINED;
        out_result->free_degree_count = free_dof;
        out_result->diagnostic = "约束过多导致过约束";
        return true;
    }

    if (free_dof > 0) {
        out_result->status = lv_CONSTRAINT_STATUS_UNDER_CONSTRAINED;
        out_result->free_degree_count = free_dof;
        out_result->diagnostic = "约束不足，存在自由度";
        return true;
    }

    /* free_dof == 0: 恰好约束 */
    out_result->status = lv_CONSTRAINT_STATUS_CONSISTENT;
    out_result->free_degree_count = 0;
    out_result->diagnostic = "约束图状态良好";
    return true;
}

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
            if (found >= capacity) {
                capacity *= 2;
                CrossBoundaryConstraint *new_results =
                    lv_realloc(results, (size_t) capacity * sizeof(CrossBoundaryConstraint));
                if (!new_results) {
                    lv_free((void **) &results);
                    lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "find_cross_boundary_constraints: realloc results failed");
                }
                results = new_results;
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
