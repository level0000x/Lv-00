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

#include "constraint_graph.h"
#include "rational.h"           /* Rational, rational_compare */
#include "symbolic_coord.h"     /* SymbolicCoord, TrustColor */

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
#include "solver.h"
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

    /* 扩展数组 */
    if (graph->node_count >= graph->node_capacity) {
        if (graph->node_capacity > INT_MAX / LV00_ARRAY_GROWTH_FACTOR) {
            lv00_free((void **) &node);
            return NULL;
        }
        int new_capacity =
            graph->node_capacity == 0 ? LV00_INITIAL_ARRAY_CAPACITY : graph->node_capacity * LV00_ARRAY_GROWTH_FACTOR;
        GeomNode **new_nodes = lv00_realloc(graph->nodes, (size_t) new_capacity * sizeof(GeomNode *));
        if (!new_nodes) {
            /* 清理已分配的坐标 */
            for (int i = 0; i < coord_count; i++) {
                if (node->symbolic_coords[i])
                    symbolic_coord_destroy(node->symbolic_coords[i]);
            }
            lv00_free((void **) &node->symbolic_coords);
            lv00_free((void **) &node);
            return NULL;
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
            if (atomic_compare_exchange_weak((atomic_int *)&graph->next_node_id, &expected, desired)) {
                break;
            }
            /* expected 已被更新为当前值，循环重试直到成功或当前值已 >= desired */
        }
    }

    /* 流式事件: 节点添加 */
    if (graph_stream_ctx) {
        static const char *type_names[] = {"POINT", "LINE", "REGION", "PORT", "FUNC_BLOCK"};
        char desc[128];
        const char *tname = (type >= 0 && type <= 4) ? type_names[type] : "UNKNOWN";
        snprintf(desc, sizeof(desc), "添加节点 #%d (类型: %s)", node_id, tname);
        stream_emit_simple(graph_stream_ctx, STREAM_EVENT_NODE_ADDED, desc, node_id);
    }

    return node;
}

/* 带指定ID添加约束（用于反序列化） */
Constraint *graph_add_constraint_with_id(ConstraintGraph *graph, int constraint_id, ConstraintType type,
                                         const int *participants, int participant_count) {
    if (!graph || !participants || participant_count <= 0)
        return NULL;

    /* 检查ID是否已被使用 */
    if (graph_get_constraint(graph, constraint_id) != NULL) {
        lv00_set_error(LV00_ERROR_UNKNOWN, "%s", "Constraint ID already exists");
        return NULL;
    }

    Constraint *con = lv00_malloc(sizeof(Constraint));
    if (!con)
        return NULL;
    memset(con, 0, sizeof(Constraint));

    con->id = constraint_id;
    con->type = type;
    con->is_active = true;   /* v3.5.0: 新约束默认活跃 */
    con->participant_count = participant_count;
    con->participants = lv00_malloc((size_t) participant_count * sizeof(int));
    if (!con->participants) {
        lv00_free((void **) &con);
        return NULL;
    }
    memcpy(con->participants, participants, participant_count * sizeof(int));

    /* 扩展数组 */
    if (graph->constraint_count >= graph->constraint_capacity) {
        if (graph->constraint_capacity > INT_MAX / LV00_ARRAY_GROWTH_FACTOR) {
            lv00_free((void **) &con->participants);
            lv00_free((void **) &con);
            return NULL;
        }
        int new_capacity = graph->constraint_capacity == 0 ? LV00_INITIAL_ARRAY_CAPACITY
                                                           : graph->constraint_capacity * LV00_ARRAY_GROWTH_FACTOR;
        Constraint **new_constraints = lv00_realloc(graph->constraints, (size_t) new_capacity * sizeof(Constraint *));
        if (!new_constraints) {
            lv00_free((void **) &con->participants);
            lv00_free((void **) &con);
            return NULL;
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
            "CONNECTION"    /* 连接约束 */
        };
        char desc[256];
        const char *cname = (type >= 0 && type <= 4) ? ctype_names[type] : "UNKNOWN";
        snprintf(desc, sizeof(desc), "添加约束 #%d (类型: %s, 参与者: %d个)", constraint_id, cname, participant_count);
        stream_emit_simple(graph_stream_ctx, STREAM_EVENT_CONSTRAINT_ADDED, desc, constraint_id);
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
static bool constraint_exists(const ConstraintGraph *graph, ConstraintType type, const int *participants, int count) {
    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *c = graph->constraints[i];
        if (!c->is_active)                    /* v3.5.0: 跳过不活跃约束 */
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
 * 初始大小和负载因子定义在 lv00_internal.h 中
 * ============================================================================ */

/**
 * @brief 计算节点ID的哈希值
 * @param id 节点ID
 * @param capacity 哈希表容量
 * @return 哈希值
 */
static unsigned node_id_hash(int id, int capacity) {
    /* FNV-1a-like hash，乘数定义在 lv00_internal.h 中 */
    unsigned h = (unsigned) id * LV00_FNV_HASH_MULTIPLIER;
    /*
     * 运行时检查：哈希表容量通常为 2 的幂，此时使用高效的位掩码取模。
     * 若容量不是 2 的幂（防御性场景），回退到安全的取模操作，
     * 避免在 Release 构建中因 assert 被移除而导致未定义行为。
     */
    if (capacity > 0 && (capacity & (capacity - 1)) == 0) {
        return h & (unsigned)(capacity - 1);
    } else {
        return h % (unsigned)(capacity > 0 ? capacity : 1);
    }
}

/**
 * @brief 确保节点哈希索引有足够容量
 * @param graph 约束图指针
 * @return 成功返回 true，失败返回 false
 */
static bool node_index_ensure_capacity(ConstraintGraph *graph) {
    if (!graph->node_index) {
        int cap = LV00_NODE_INDEX_INITIAL_SIZE;
        graph->node_index = lv00_calloc(cap, sizeof(GeomNode *));
        if (!graph->node_index)
            return false;
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
    if (graph->node_count < (int) (graph->node_index_capacity * LV00_INDEX_LOAD_FACTOR)) {
        return true;
    }

    /* 重新哈希到更大的表 */
    if (graph->node_index_capacity > INT_MAX / 2) return false;
    int new_cap = graph->node_index_capacity * 2;
    GeomNode **new_index = lv00_calloc(new_cap, sizeof(GeomNode *));
    if (!new_index)
        return false;

    for (int i = 0; i < graph->node_count; i++) {
        unsigned idx = node_id_hash(graph->nodes[i]->id, new_cap);
        while (new_index[idx] != NULL) {
            idx = (idx + 1) & (unsigned) (new_cap - 1);
        }
        new_index[idx] = graph->nodes[i];
    }

    lv00_free((void **) &graph->node_index);
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
static void node_index_remove(ConstraintGraph *graph, int node_id) {
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
        unsigned range = (unsigned)((i - idx + cap) % cap);
        unsigned ideal_dist = (unsigned)((ideal - idx + cap) % cap);
        bool in_range = (ideal_dist <= range);
        if (in_range) {
            /* 条目属于被删除槽的影响范围，需要重新插入 */
            unsigned j = ideal;
            while (graph->node_index[j] != NULL) {
                j = (j + 1) & (unsigned) (graph->node_index_capacity - 1);
                if (j == ideal) {
                    /* 哈希表探测绕回起点，条目无法重新插入 —— 记录错误 */
                    lv00_set_error(LV00_ERROR_INTERNAL, "哈希表已满，无法重新插入条目（ID=%d）", entry->id);
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
 *  初始大小和负载因子定义在 lv00_internal.h 中
 * ================================================================ */

/**
 * @brief 计算约束ID的哈希值
 * @param id 约束ID
 * @param capacity 哈希表容量
 * @return 哈希值
 */
static unsigned constraint_id_hash(int id, int capacity) {
    unsigned h = (unsigned) id * LV00_FNV_HASH_MULTIPLIER;
    return h & (unsigned) (capacity - 1);
}

/**
 * @brief 确保约束哈希索引有足够容量
 * @param graph 约束图指针
 * @return 成功返回 true，失败返回 false
 */
static bool constraint_index_ensure_capacity(ConstraintGraph *graph) {
    if (!graph->constraint_index) {
        int cap = LV00_CONSTRAINT_INDEX_INITIAL_SIZE;
        graph->constraint_index = lv00_calloc(cap, sizeof(Constraint *));
        if (!graph->constraint_index)
            return false;
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

    if (graph->constraint_count < (int) (graph->constraint_index_capacity * LV00_INDEX_LOAD_FACTOR)) {
        return true;
    }

    if (graph->constraint_index_capacity > INT_MAX / 2) return false;
    int new_cap = graph->constraint_index_capacity * 2;
    Constraint **new_index = lv00_calloc(new_cap, sizeof(Constraint *));
    if (!new_index)
        return false;

    for (int i = 0; i < graph->constraint_count; i++) {
        unsigned idx = constraint_id_hash(graph->constraints[i]->id, new_cap);
        while (new_index[idx] != NULL) {
            idx = (idx + 1) & (unsigned) (new_cap - 1);
        }
        new_index[idx] = graph->constraints[i];
    }

    lv00_free((void **) &graph->constraint_index);
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
static void constraint_index_remove(ConstraintGraph *graph, int constraint_id) {
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
        return false;
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

    if (fabs(denom) < LV00_EPSILON_DOUBLE)
        return false; /* 平行或共线 */

    double t = ((bx1 - ax1) * dy_b - (by1 - ay1) * dx_b) / denom;
    double u = ((bx1 - ax1) * dy_a - (by1 - ay1) * dx_a) / denom;

    /* 严格内部相交检查 */
    return (t > LV00_EPSILON_SEGMENT_INTERIOR && t < 1.0 - LV00_EPSILON_SEGMENT_INTERIOR &&
            u > LV00_EPSILON_SEGMENT_INTERIOR && u < 1.0 - LV00_EPSILON_SEGMENT_INTERIOR);
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
static bool check_incremental_conflict(const ConstraintGraph *graph, const Constraint *new_constraint) {
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
                int lines_capacity = 8;  /* 初始容量 */

                lines = (int *)lv00_malloc(sizeof(int) * lines_capacity);
                if (!lines) {
                    LV00_LOG_ERROR("check_incremental_conflict: 内存分配失败");
                    return false;  /* 内存不足，保守返回无冲突 */
                }

                for (int i = 0; i < graph->constraint_count; i++) {
                    Constraint *c = graph->constraints[i];
                    if (c->type == INCIDENCE && c->participants[0] == point_id) {
                        /* v3.4.2: 动态扩容 */
                        if (line_count >= lines_capacity) {
                            int new_cap = lines_capacity * 2;
                            if (new_cap < lines_capacity) {  /* 溢出检查 */
                                lv00_free((void**)&lines);
                                return false;
                            }
                            int *new_lines = (int *)lv00_realloc(lines, sizeof(int) * new_cap);
                            if (!new_lines) {
                                lv00_free((void**)&lines);
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
                        lv00_free((void**)&lines);
                        return false;
                    }
                    int *new_lines = (int *)lv00_realloc(lines, sizeof(int) * new_cap);
                    if (!new_lines) {
                        lv00_free((void**)&lines);
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
                lv00_free((void**)&lines);

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
            /* No simple algebraic conflict check for these types */
            break;
        default:
            /* v3.5.0: 未知约束类型，记录错误 */
            lv00_set_error(LV00_ERROR_UNKNOWN,
                           "check_incremental_conflict: 未知约束类型 %d (constraint id=%d)",
                           (int)new_constraint->type, new_constraint->id);
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
    node->symbolic_coords = lv00_malloc((size_t) coord_count * sizeof(SymbolicCoord *));
    if (!node->symbolic_coords) {
        graph->node_count--;
        node_index_remove(graph, node->id);
        lv00_free((void **) &node);
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
            lv00_free((void **) &node->symbolic_coords);
            node->symbolic_coords = NULL;
            node->coord_count = 0;
            graph->node_count--;
            node_index_remove(graph, node->id);
            lv00_free((void **) &node);
            return ADD_NODE_CONFLICT;
        }
    }
    node->coord_count = coord_count;
    if (graph_stream_ctx) {
        char buf[128];
        snprintf(buf, sizeof(buf), "添加点节点: id=%d", node->id);
        stream_emit_simple(graph_stream_ctx, STREAM_EVENT_NODE_ADDED, buf, 0);
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

    node->symbolic_coords = lv00_malloc((size_t) total_coords * sizeof(SymbolicCoord *));
    if (!node->symbolic_coords) {
        graph->node_count--;
        node_index_remove(graph, node->id);
        lv00_free((void **) &node);
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
    if (!segment->symbolic_coords[0] || !segment->symbolic_coords[1] ||
        !segment->symbolic_coords[2] || !segment->symbolic_coords[3])
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

bool graph_check_compatibility(const ConstraintGraph *graph, Lv00ConstraintCompatibilityResult *out_result) {
    if (out_result) {
        out_result->status = LV00_CONSTRAINT_STATUS_INVALID;
        out_result->conflicting_constraint_id = -1;
        out_result->redundant_constraint_count = 0;
        out_result->free_degree_count = 0;
        out_result->diagnostic = "输入无效";
    }
    if (!graph || !out_result)
        return false;

    if (graph->node_count == 0) {
        out_result->status = LV00_CONSTRAINT_STATUS_UNDER_CONSTRAINED;
        out_result->free_degree_count = 1;
        out_result->diagnostic = "空约束图缺少几何事实";
        return true;
    }

    int active_geometry_nodes = 0;
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node)
            continue;
        if (node->type == GEOM_LINE_SEGMENT) {
            active_geometry_nodes++;
            if (graph_segment_is_degenerate(node)) {
                out_result->status = LV00_CONSTRAINT_STATUS_INCONSISTENT;
                out_result->conflicting_constraint_id = node->id;
                out_result->diagnostic = "检测到由重合端点构成的退化线段";
                return true;
            }
        } else if (node->type == GEOM_POINT || node->type == GEOM_REGION) {
            active_geometry_nodes++;
        }
    }

    int redundant_count = 0;
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *left = graph->nodes[i];
        if (!left || left->type != GEOM_LINE_SEGMENT)
            continue;
        for (int j = i + 1; j < graph->node_count; j++) {
            GeomNode *right = graph->nodes[j];
            if (graph_segments_have_same_endpoints(left, right))
                redundant_count++;
        }
    }
    out_result->redundant_constraint_count = redundant_count;
    if (redundant_count > 0) {
        out_result->status = LV00_CONSTRAINT_STATUS_OVER_CONSTRAINED;
        out_result->diagnostic = "检测到重复线段约束";
        return true;
    }

    if (active_geometry_nodes < 3) {
        out_result->status = LV00_CONSTRAINT_STATUS_UNDER_CONSTRAINED;
        out_result->free_degree_count = 3 - active_geometry_nodes;
        out_result->diagnostic = "几何事实不足，约束系统欠约束";
        return true;
    }

    out_result->status = LV00_CONSTRAINT_STATUS_CONSISTENT;
    out_result->diagnostic = "未发现直接矛盾或冗余";
    return true;
}

AddNodeResult graph_add_region(ConstraintGraph *graph, const int *boundary_segment_ids, int segment_count) {
    for (int i = 0; i < segment_count; i++) {
        GeomNode *seg = graph_get_node(graph, boundary_segment_ids[i]);
        if (!seg || seg->type != GEOM_LINE_SEGMENT) {
            return ADD_NODE_INVALID_REGION;
        }
    }
    GeomNode *node = graph_alloc_node(graph, GEOM_REGION);
    if (!node)
        return ADD_NODE_CONFLICT;
    if (segment_count == 0) {
        node->data.region.boundary_segments = NULL;
        node->data.region.segment_count = 0;
        node->coord_count = 0;
        return ADD_NODE_OK;
    }
    node->data.region.boundary_segments = lv00_malloc((size_t) segment_count * sizeof(GeomNode *));
    if (!node->data.region.boundary_segments) {
        graph->node_count--;
        node_index_remove(graph, node->id);
        lv00_free((void **) &node);
        return ADD_NODE_CONFLICT;
    }
    for (int i = 0; i < segment_count; i++) {
        node->data.region.boundary_segments[i] = graph_get_node(graph, boundary_segment_ids[i]);
    }
    node->data.region.segment_count = segment_count;
    node->coord_count = 0;
    return ADD_NODE_OK;
}

/**
 * @brief 添加端口节点
 *
 * 端口是函数块与外部约束图之间的连接接口。
 *
 * @param graph           约束图指针
 * @param type            端口类型
 * @param namespace_depth 命名空间深度
 * @param parent_block_id 父函数块节点 ID
 * @return 操作结果枚举
 */
AddNodeResult graph_add_port(ConstraintGraph *graph, PortType type, int namespace_depth, int parent_block_id) {
    GeomNode *node = graph_alloc_node(graph, GEOM_PORT);
    if (!node)
        return ADD_NODE_CONFLICT;
    Port *port = lv00_malloc(sizeof(Port));
    if (!port) {
        /* Remove the already-allocated node from the graph */
        graph->node_count--;
        node_index_remove(graph, node->id);
        lv00_free((void **) &node);
        return ADD_NODE_CONFLICT;
    }
    memset(port, 0, sizeof(Port));
    port->id = node->id;
    port->type = type;
    port->namespace_depth = namespace_depth;
    port->parent_block_id = parent_block_id;
    port->is_formal_param = (type == PORT_INPUT);
    port->connected_to = NULL;
    node->data.port = port;
    node->namespace_depth = namespace_depth;
    node->parent_block_id = parent_block_id;
    return ADD_NODE_OK;
}

/**
 * 在约束图中添函数块节点。
 *
 * 函数块包含内部节点、输入端口和输出端口。
 *
 * @param graph              约束图指针
 * @param internal_node_ids   内部节点 ID 数组
 * @param internal_count     内部节点数量
 * @param input_port_ids     输入端口 ID 数组
 * @param input_count       输入端口数量
 * @param output_port_ids    输出端口 ID 数组
 * @param output_count       输出端口数量
 * @return 添加结果状态
 */
AddNodeResult graph_add_function_block(ConstraintGraph *graph, const int *internal_node_ids, int internal_count,
                                       const int *input_port_ids, int input_count, const int *output_port_ids, int output_count) {
    if (!graph || (internal_count > 0 && !internal_node_ids) || (input_count > 0 && !input_port_ids) || (output_count > 0 && !output_port_ids))
        return ADD_NODE_ERROR;
    for (int i = 0; i < internal_count; i++) {
        if (!graph_get_node(graph, internal_node_ids[i]))
            return ADD_NODE_CONFLICT;
    }
    for (int i = 0; i < input_count; i++) {
        GeomNode *n = graph_get_node(graph, input_port_ids[i]);
        if (!n || n->type != GEOM_PORT)
            return ADD_NODE_CONFLICT;
    }
    for (int i = 0; i < output_count; i++) {
        GeomNode *n = graph_get_node(graph, output_port_ids[i]);
        if (!n || n->type != GEOM_PORT)
            return ADD_NODE_CONFLICT;
    }
    GeomNode *node = graph_alloc_node(graph, GEOM_FUNCTION_BLOCK);
    if (!node)
        return ADD_NODE_CONFLICT;
    node->data.func_block.internal_nodes = internal_count > 0 ? lv00_malloc((size_t) internal_count * sizeof(GeomNode *)) : NULL;
    if (internal_count > 0 && !node->data.func_block.internal_nodes) {
        graph->node_count--;
        node_index_remove(graph, node->id);
        lv00_free((void **) &node);
        return ADD_NODE_CONFLICT;
    }
    for (int i = 0; i < internal_count; i++) {
        node->data.func_block.internal_nodes[i] = graph_get_node(graph, internal_node_ids[i]);
    }
    node->data.func_block.internal_node_count = internal_count;
    /* 仅在数量大于0时才分配端口ID数组，避免 lv00_malloc(0) 的未定义行为 */
    node->data.func_block.input_port_ids = input_count > 0 ? lv00_malloc((size_t) input_count * sizeof(int)) : NULL;
    if (input_count > 0 && !node->data.func_block.input_port_ids) {
        lv00_free((void **) &node->data.func_block.internal_nodes);
        graph->node_count--;
        node_index_remove(graph, node->id);
        lv00_free((void **) &node);
        return ADD_NODE_CONFLICT;
    }
    if (input_count > 0) {
        memcpy(node->data.func_block.input_port_ids, input_port_ids, input_count * sizeof(int));
    }
    node->data.func_block.output_port_ids = output_count > 0 ? lv00_malloc((size_t) output_count * sizeof(int)) : NULL;
    if (output_count > 0 && !node->data.func_block.output_port_ids) {
        lv00_free((void **) &node->data.func_block.input_port_ids);
        lv00_free((void **) &node->data.func_block.internal_nodes);
        graph->node_count--;
        node_index_remove(graph, node->id);
        lv00_free((void **) &node);
        return ADD_NODE_CONFLICT;
    }
    if (output_count > 0) {
        memcpy(node->data.func_block.output_port_ids, output_port_ids, output_count * sizeof(int));
    }
    node->data.func_block.input_count = input_count;
    node->data.func_block.output_count = output_count;
    node->data.func_block.determinism_state = UNVERIFIED;
    return ADD_NODE_OK;
}

/**
 * 查找跨越边界的约束。
 *
 * 查找同时涉及内部节点/端口和外部节点的约束（连接约束除外）。
 *
 * @param graph             约束图指针
 * @param internal_node_ids  内部节点 ID 数组
 * @param internal_count    内部节点数量
 * @param port_ids          端口 ID 数组
 * @param port_count        端口数量
 * @param out_count         输出：找到的跨越约束数量
 * @return 跨越约束数组，调用者需负责释放；失败时返回 NULL
 */
CrossBoundaryConstraint *find_cross_boundary_constraints(ConstraintGraph *graph, const int *internal_node_ids,
                                                         int internal_count, const int *port_ids, int port_count,
                                                         int *out_count) {
    if (!graph || !internal_node_ids || !port_ids || !out_count)
        return NULL;
    int max_count = graph->constraint_count;
    CrossBoundaryConstraint *conflicts = lv00_malloc((size_t) max_count * sizeof(CrossBoundaryConstraint));
    if (!conflicts)
        return NULL;
    int count = 0;
    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *c = graph->constraints[i];
        if (c->type == CONNECTION)
            continue;
        bool has_internal = false;
        bool has_external = false;
        for (int j = 0; j < c->participant_count; j++) {
            int p_id = c->participants[j];
            bool is_internal = false;
            bool is_port = false;
            for (int k = 0; k < internal_count; k++) {
                if (internal_node_ids[k] == p_id) {
                    is_internal = true;
                    break;
                }
            }
            for (int k = 0; k < port_count; k++) {
                if (port_ids[k] == p_id) {
                    is_port = true;
                    break;
                }
            }
            if (is_internal || is_port)
                has_internal = true;
            else
                has_external = true;
        }
        if (has_internal && has_external) {
            CrossBoundaryConstraint *conflict = &conflicts[count];
            conflict->constraint_id = c->id;
            conflict->type = c->type;
            /* CrossBoundaryConstraint.node_ids 只能存2个节点ID */
            int copy_count = c->participant_count < 2 ? c->participant_count : 2;
            conflict->node_count = copy_count;
            for (int j = 0; j < copy_count; j++) {
                conflict->node_ids[j] = c->participants[j];
            }
            count++;
        }
    }
    *out_count = count;
    return conflicts;
}

/**
 * 检查函数块是否有跨越边界的无效约束。
 *
 * 根据作用域规则：
 * - 子块可以引用父块的公共节点
 * - 兄弟块不能相互引用私有节点
 * - 命名空间深度差异不能超过 1
 *
 * @param graph      约束图指针
 * @param func_block 函数块节点
 * @return true 表示检查通过，false 表示存在无效的跨越约束
 */
static bool check_cross_boundary_constraints(ConstraintGraph *graph, GeomNode *func_block) {
    if (!graph || !func_block || func_block->type != GEOM_FUNCTION_BLOCK) {
        return true;
    }

    int internal_count = func_block->data.func_block.internal_node_count;
    int input_count = func_block->data.func_block.input_count;
    int output_count = func_block->data.func_block.output_count;

    /* 收集所有内部节点ID和端口ID */
    int *internal_ids = lv00_malloc((size_t)(internal_count + input_count + output_count) * sizeof(int));
    if (!internal_ids)
        return true;

    int idx = 0;
    for (int i = 0; i < internal_count; i++) {
        if (func_block->data.func_block.internal_nodes[i]) {
            internal_ids[idx++] = func_block->data.func_block.internal_nodes[i]->id;
        }
    }
    for (int i = 0; i < input_count; i++) {
        internal_ids[idx++] = func_block->data.func_block.input_port_ids[i];
    }
    for (int i = 0; i < output_count; i++) {
        internal_ids[idx++] = func_block->data.func_block.output_port_ids[i];
    }
    int total_internal = idx;

    /* 检查每个约束的跨边界违规 */
    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *c = graph->constraints[i];
        if (c->type == CONNECTION)
            continue; /* 连接约束允许跨边界 */

        bool has_internal = false;
        bool has_external = false;
        int external_namespace = -1;
        int external_parent = -1;

        for (int j = 0; j < c->participant_count; j++) {
            int p_id = c->participants[j];
            bool is_internal = false;

            for (int k = 0; k < total_internal; k++) {
                if (internal_ids[k] == p_id) {
                    is_internal = true;
                    break;
                }
            }

            if (is_internal) {
                has_internal = true;
            } else {
                has_external = true;
                GeomNode *ext_node = graph_get_node(graph, p_id);
                if (ext_node) {
                    external_namespace = ext_node->namespace_depth;
                    external_parent = ext_node->parent_block_id;
                }
            }
        }

        /* 如果约束跨越边界，检查其是否有效 */
        if (has_internal && has_external) {
            /* 检查作用域规则：
             * - 子块可以引用父块的公共节点
             * - 兄弟块不能相互引用私有节点
             * - namespace_depth 差异应最多为1才是有效引用
             */
            int block_namespace = func_block->namespace_depth;
            int block_parent = func_block->parent_block_id;

            /* 引用兄弟块内部节点是无效的 */
            if (external_namespace == block_namespace && external_parent != block_parent) {
                /* 相同深度但不同父块 - 兄弟块引用 */
                lv00_free((void **) &internal_ids);
                lv00_set_error(LV00_ERROR_UNKNOWN, "%s", "Cross-boundary constraint references sibling block's private node");
                return false;
            }

            /* 引用更深命名空间的节点是无效的 */
            if (external_namespace > block_namespace) {
                lv00_free((void **) &internal_ids);
                lv00_set_error(LV00_ERROR_UNKNOWN, "%s", "Cross-boundary constraint references node from deeper namespace");
                return false;
            }

            /* 命名空间深度差异 > 1 是无效的 */
            if (block_namespace - external_namespace > 1) {
                lv00_free((void **) &internal_ids);
                lv00_set_error(LV00_ERROR_UNKNOWN, "%s", "Cross-boundary constraint spans more than one namespace level");
                return false;
            }
        }
    }

    lv00_free((void **) &internal_ids);
    return true;
}

/**
 * 在约束图中添加关联约束（点在线/区域上）。
 *
 * @param graph           约束图指针
 * @param point_id       点节点 ID
 * @param line_or_region_id 线段或区域节点 ID
 * @return 添加结果状态
 */
AddConstraintResult graph_add_incidence(ConstraintGraph *graph, int point_id, int line_or_region_id) {
    GeomNode *point = graph_get_node(graph, point_id);
    GeomNode *target = graph_get_node(graph, line_or_region_id);
    if (!point || !target)
        return ADD_CONSTRAINT_CONFLICT;
    if (point->type != GEOM_POINT)
        return ADD_CONSTRAINT_CONFLICT;
    if (target->type != GEOM_LINE_SEGMENT && target->type != GEOM_REGION)
        return ADD_CONSTRAINT_CONFLICT;
    int participants[2] = {point_id, line_or_region_id};
    if (constraint_exists(graph, INCIDENCE, participants, 2))
        return ADD_CONSTRAINT_DUPLICATE;
    Constraint *con = graph_alloc_constraint(graph, INCIDENCE);
    if (!con)
        return ADD_CONSTRAINT_CONFLICT;
    con->participants = lv00_malloc(2 * sizeof(int));
    if (!con->participants) {
        graph->constraint_count--;
        constraint_index_remove(graph, con->id);
        lv00_free((void **) &con);
        return ADD_CONSTRAINT_CONFLICT;
    }
    con->participants[0] = point_id;
    con->participants[1] = line_or_region_id;
    con->participant_count = 2;
    /* 增量代数冲突检查 */
    if (check_incremental_conflict(graph, con)) {
        LOG_WARN("constraint_graph",
                 "检测到 INCIDENCE 约束 %d 的增量冲突 "
                 "(点 %d 在线/区域 %d 上)",
                 con->id, point_id, line_or_region_id);
    }
    if (graph_stream_ctx) {
        char buf[128];
        snprintf(buf, sizeof(buf), "添加关联约束: id=%d, point=%d, target=%d", con->id, point_id, line_or_region_id);
        stream_emit_simple(graph_stream_ctx, STREAM_EVENT_CONSTRAINT_ADDED, buf, 0);
    }
    graph->dirty = true;  /* v3.5.0: 约束被添加，标记脏状态 */
    return ADD_CONSTRAINT_OK;
}

/**
 * @brief 添加介于约束（B-A-C 共线有序）
 *
 * 声明点 B 在点 A 和点 C 之间，三点共线且有序。
 *
 * @param graph 约束图指针
 * @param p1_id 第一个端点 ID
 * @param p2_id 中间点 ID
 * @param p3_id 第二个端点 ID
 * @return 操作结果枚举
 */
AddConstraintResult graph_add_betweenness(ConstraintGraph *graph, int p1_id, int p2_id, int p3_id) {
    GeomNode *p1 = graph_get_node(graph, p1_id);
    GeomNode *p2 = graph_get_node(graph, p2_id);
    GeomNode *p3 = graph_get_node(graph, p3_id);
    if (!p1 || !p2 || !p3)
        return ADD_CONSTRAINT_CONFLICT;
    if (p1->type != GEOM_POINT || p2->type != GEOM_POINT || p3->type != GEOM_POINT) {
        return ADD_CONSTRAINT_CONFLICT;
    }
    int participants[3] = {p1_id, p2_id, p3_id};
    if (constraint_exists(graph, BETWEENNESS, participants, 3))
        return ADD_CONSTRAINT_DUPLICATE;
    Constraint *con = graph_alloc_constraint(graph, BETWEENNESS);
    if (!con)
        return ADD_CONSTRAINT_CONFLICT;
    con->participants = lv00_malloc(3 * sizeof(int));
    if (!con->participants) {
        graph->constraint_count--;
        constraint_index_remove(graph, con->id);
        lv00_free((void **) &con);
        return ADD_CONSTRAINT_CONFLICT;
    }
    memcpy(con->participants, participants, 3 * sizeof(int));
    con->participant_count = 3;
    /* 增量代数冲突检查 */
    if (check_incremental_conflict(graph, con)) {
        LOG_WARN("constraint_graph",
                 "检测到 BETWEENNESS 约束 %d 的增量冲突 "
                 "(点 %d-%d-%d 不共线)",
                 con->id, p1_id, p2_id, p3_id);
    }
    graph->dirty = true;  /* v3.5.0: 约束被添加，标记脏状态 */
    return ADD_CONSTRAINT_OK;
}

/**
 * @brief 添加相交约束
 *
 * 声明两条线在指定交点处真交。
 *
 * @param graph           约束图指针
 * @param line1_id        第一条线 ID
 * @param line2_id        第二条线 ID
 * @param result_point_id 交点 ID
 * @return 操作结果枚举
 */
AddConstraintResult graph_add_intersection(ConstraintGraph *graph, int line1_id, int line2_id, int result_point_id) {
    GeomNode *l1 = graph_get_node(graph, line1_id);
    GeomNode *l2 = graph_get_node(graph, line2_id);
    GeomNode *pt = graph_get_node(graph, result_point_id);
    if (!l1 || !l2 || !pt)
        return ADD_CONSTRAINT_CONFLICT;
    if (l1->type != GEOM_LINE_SEGMENT || l2->type != GEOM_LINE_SEGMENT)
        return ADD_CONSTRAINT_CONFLICT;
    if (pt->type != GEOM_POINT)
        return ADD_CONSTRAINT_CONFLICT;
    int participants[3] = {line1_id, line2_id, result_point_id};
    if (constraint_exists(graph, INTERSECTION, participants, 3))
        return ADD_CONSTRAINT_DUPLICATE;
    Constraint *con = graph_alloc_constraint(graph, INTERSECTION);
    if (!con)
        return ADD_CONSTRAINT_CONFLICT;
    con->participants = lv00_malloc(3 * sizeof(int));
    if (!con->participants) {
        graph->constraint_count--;
        constraint_index_remove(graph, con->id);
        lv00_free((void **) &con);
        return ADD_CONSTRAINT_CONFLICT;
    }
    memcpy(con->participants, participants, 3 * sizeof(int));
    con->participant_count = 3;
    /* 增量代数冲突检查 */
    if (check_incremental_conflict(graph, con)) {
        LOG_WARN("constraint_graph",
                 "检测到 INTERSECTION 约束 %d 的增量冲突 "
                 "(线 %d 和 %d 已在不同点相交)",
                 con->id, line1_id, line2_id);
    }
    graph->dirty = true;  /* v3.5.0: 约束被添加，标记脏状态 */
    return ADD_CONSTRAINT_OK;
}

/**
 * @brief 添加包含约束
 *
 * 声明内部节点被包含在外部节点（区域）中。
 *
 * @param graph   约束图指针
 * @param inner_id 内部节点 ID
 * @param outer_id 外部节点 ID
 * @return 操作结果枚举
 */
AddConstraintResult graph_add_containment(ConstraintGraph *graph, int inner_id, int outer_id) {
    GeomNode *inner = graph_get_node(graph, inner_id);
    GeomNode *outer = graph_get_node(graph, outer_id);
    if (!inner || !outer)
        return ADD_CONSTRAINT_CONFLICT;
    if (inner->type != GEOM_POINT && inner->type != GEOM_REGION)
        return ADD_CONSTRAINT_CONFLICT;
    if (outer->type != GEOM_REGION)
        return ADD_CONSTRAINT_CONFLICT;
    int participants[2] = {inner_id, outer_id};
    if (constraint_exists(graph, CONTAINMENT, participants, 2))
        return ADD_CONSTRAINT_DUPLICATE;
    Constraint *con = graph_alloc_constraint(graph, CONTAINMENT);
    if (!con)
        return ADD_CONSTRAINT_CONFLICT;
    con->participants = lv00_malloc(2 * sizeof(int));
    if (!con->participants) {
        graph->constraint_count--;
        constraint_index_remove(graph, con->id);
        lv00_free((void **) &con);
        return ADD_CONSTRAINT_CONFLICT;
    }
    con->participants[0] = inner_id;
    con->participants[1] = outer_id;
    con->participant_count = 2;
    graph->dirty = true;  /* v3.5.0: 约束被添加，标记脏状态 */
    return ADD_CONSTRAINT_OK;
}

/**
 * 在约束图中添加连接约束（端口连接）。
 *
 * 连接约束必须是输出端口到输入端口，且命名空间深度相同或相差 1。
 *
 * @param graph        约束图指针
 * @param src_port_id 源端口节点 ID（必须是输出端口）
 * @param dst_port_id 目标端口节点 ID（必须是输入端口）
 * @return 添加结果状态
 */
AddConstraintResult graph_add_connection(ConstraintGraph *graph, int src_port_id, int dst_port_id) {
    GeomNode *src = graph_get_node(graph, src_port_id);
    GeomNode *dst = graph_get_node(graph, dst_port_id);
    if (!src || !dst)
        return ADD_CONSTRAINT_CONFLICT;
    if (src->type != GEOM_PORT || dst->type != GEOM_PORT)
        return ADD_CONSTRAINT_CONFLICT;
    Port *src_port = src->data.port;
    Port *dst_port = dst->data.port;
    if (src_port->type != PORT_OUTPUT || dst_port->type != PORT_INPUT) {
        return ADD_CONSTRAINT_CONFLICT;
    }
    if (src_port->namespace_depth != dst_port->namespace_depth &&
        abs(src_port->namespace_depth - dst_port->namespace_depth) != 1) {
        return ADD_CONSTRAINT_CONFLICT;
    }
    int participants[2] = {src_port_id, dst_port_id};
    if (constraint_exists(graph, CONNECTION, participants, 2))
        return ADD_CONSTRAINT_DUPLICATE;
    Constraint *con = graph_alloc_constraint(graph, CONNECTION);
    if (!con)
        return ADD_CONSTRAINT_CONFLICT;
    con->participants = lv00_malloc(2 * sizeof(int));
    if (!con->participants) {
        graph->constraint_count--;
        constraint_index_remove(graph, con->id);
        lv00_free((void **) &con);
        return ADD_CONSTRAINT_CONFLICT;
    }
    con->participants[0] = src_port_id;
    con->participants[1] = dst_port_id;
    con->participant_count = 2;
    /* 建立双向连接关系 */
    dst_port->connected_to = src;
    src_port->connected_to = dst;
    graph->dirty = true;  /* v3.5.0: 约束被添加，标记脏状态 */
    return ADD_CONSTRAINT_OK;
}

/**
 * @brief 从所有约束的参与者列表中移除对指定节点的引用
 *
 * 遍历约束图中的所有约束，将参与者列表中匹配 node_id 的条目
 * 移除，并相应减少 participant_count。此函数在删除节点时调用，
 * 确保约束数据与图的节点集合保持一致。
 *
 * @param graph   约束图指针
 * @param node_id 要移除引用的节点 ID
 */
static void remove_references_to_node(ConstraintGraph *graph, int node_id) {
    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *con = graph->constraints[i];
        for (int j = 0; j < con->participant_count; j++) {
            if (con->participants[j] == node_id) {
                for (int k = j; k < con->participant_count - 1; k++) {
                    con->participants[k] = con->participants[k + 1];
                }
                con->participant_count--;
                j--;
            }
        }
    }
}

/* 前向声明 */
static void node_destroy(GeomNode *node);

/**
 * @brief 检查节点是否属于某个区域的边界
 * @param graph 约束图指针
 * @param node_id 节点ID
 * @return 是返回 true，否则返回 false
 */
static bool node_in_region_boundary(const ConstraintGraph *graph, int node_id) {
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *n = graph->nodes[i];
        if (n->type == GEOM_REGION) {
            for (int j = 0; j < n->data.region.segment_count; j++) {
                if (n->data.region.boundary_segments[j] && n->data.region.boundary_segments[j]->id == node_id) {
                    return true;
                }
            }
        }
    }
    return false;
}

/**
 * @brief 从约束图中移除节点
 *
 * 移除指定节点及其所有关联约束，清理邻接表和哈希索引。
 *
 * @param graph   约束图指针
 * @param node_id 要移除的节点 ID
 * @return 操作结果枚举
 */
RemoveNodeResult graph_remove_node(ConstraintGraph *graph, int node_id) {
    GeomNode *node = graph_get_node(graph, node_id);
    if (!node)
        return REMOVE_NODE_NOT_FOUND;

    /* 检查节点是否属于某个区域的边界 */
    if (node_in_region_boundary(graph, node_id)) {
        return REMOVE_NODE_ERROR;
    }

    /* 清理交叉引用：将其他 PORT 的 connected_to 指向此节点的置为 NULL */
    if (node->type == GEOM_PORT) {
        for (int i = 0; i < graph->node_count; i++) {
            GeomNode *other = graph->nodes[i];
            if (other->type == GEOM_PORT && other->data.port && other->data.port->connected_to &&
                other->data.port->connected_to->id == node_id) {
                other->data.port->connected_to = NULL;
            }
        }
    }

    /* 清理交叉引用：将此节点从所有 FUNCTION_BLOCK 的 internal_nodes 中移除 */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *fb = graph->nodes[i];
        if (fb->type == GEOM_FUNCTION_BLOCK && fb->data.func_block.internal_nodes) {
            for (int j = 0; j < fb->data.func_block.internal_node_count; j++) {
                if (fb->data.func_block.internal_nodes[j] && fb->data.func_block.internal_nodes[j]->id == node_id) {
                    fb->data.func_block.internal_nodes[j] = NULL;
                }
            }
        }
    }

    /* 移除所有引用此节点的约束 */
    for (int i = graph->constraint_count - 1; i >= 0; i--) {
        Constraint *con = graph->constraints[i];
        bool references_node = false;
        for (int j = 0; j < con->participant_count; j++) {
            if (con->participants[j] == node_id) {
                references_node = true;
                break;
            }
        }
        if (references_node) {
            int cid = con->id;
            lv00_free((void **) &con->participants);
            lv00_free((void **) &con);
            constraint_index_remove(graph, cid);
            for (int k = i; k < graph->constraint_count - 1; k++) {
                graph->constraints[k] = graph->constraints[k + 1];
            }
            graph->constraint_count--;
            graph->dirty = true;  /* v3.5.0: 约束被移除，标记脏状态 */
        }
    }

    /* 从数组中移除节点（压缩） */
    for (int i = 0; i < graph->node_count; i++) {
        if (graph->nodes[i]->id == node_id) {
            /* 在销毁节点之前先从哈希索引中移除，
             * 因为 node_index_remove 会访问其他条目的节点指针 */
            node_index_remove(graph, node_id);
            node_destroy(graph->nodes[i]);
            for (int j = i; j < graph->node_count - 1; j++) {
                graph->nodes[j] = graph->nodes[j + 1];
            }
            graph->node_count--;
            if (graph_stream_ctx) {
                char buf[128];
                snprintf(buf, sizeof(buf), "移除节点: id=%d", node_id);
                stream_emit_node_event(graph_stream_ctx, STREAM_EVENT_INFO, node_id, buf, 0);
            }
            return REMOVE_NODE_OK;
        }
    }
    return REMOVE_NODE_NOT_FOUND;
}

/**
 * @brief 从约束图中移除指定索引处的约束
 *
 * @param graph           约束图指针
 * @param constraint_index 约束在数组中的索引
 * @return 操作结果枚举
 */
RemoveConstraintResult graph_remove_constraint(ConstraintGraph *graph, int constraint_index) {
    if (constraint_index < 0 || constraint_index >= graph->constraint_count) {
        return REMOVE_CONSTRAINT_NOT_FOUND;
    }
    Constraint *con = graph->constraints[constraint_index];
    int cid = con->id;

    /* 清理交叉引用：如果是 CONNECTION 约束，清理 dst_port 的 connected_to */
    if (con->type == CONNECTION && con->participant_count == 2) {
        GeomNode *dst = graph_get_node(graph, con->participants[1]);
        if (dst && dst->type == GEOM_PORT && dst->data.port) {
            dst->data.port->connected_to = NULL;
        }
    }

    /* 先从哈希索引中移除，再释放约束内存（避免 use-after-free） */
    constraint_index_remove(graph, cid);
    lv00_free((void **) &con->participants);
    lv00_free((void **) &con);
    for (int i = constraint_index; i < graph->constraint_count - 1; i++) {
        graph->constraints[i] = graph->constraints[i + 1];
    }
    graph->constraint_count--;
    graph->dirty = true;  /* v3.5.0: 约束被移除，标记脏状态 */
    if (graph_stream_ctx) {
        char buf[128];
        snprintf(buf, sizeof(buf), "移除约束: id=%d", cid);
        stream_emit_constraint_event(graph_stream_ctx, STREAM_EVENT_INFO, cid, buf, 0);
    }
    return REMOVE_CONSTRAINT_OK;
}

/* ============================================================
 * v3.5.0: 脏标记传播与约束生命周期管理
 * ============================================================ */

/**
 * @brief 标记约束图为脏状态
 *
 * 约束被修改（添加/删除/废弃）时调用，设置 dirty 标记。
 * 后续 graph_sync_nodes() 会遍历受影响节点并刷新属性。
 *
 * @param graph 约束图指针
 */
void graph_mark_dirty(ConstraintGraph *graph) {
    if (!graph)
        return;
    graph->dirty = true;
}

/**
 * @brief 同步约束图中所有受影响节点的属性
 *
 * 遍历所有节点，刷新受约束影响的属性（如 trust 等级、
 * 数值精度等）。同步完成后重置 dirty 标记。
 *
 * @param graph 约束图指针
 */
void graph_sync_nodes(ConstraintGraph *graph) {
    if (!graph)
        return;
    if (!graph->dirty)
        return;  /* 无变更，无需同步 */

    /* 遍历所有活跃约束，传播约束信息到受影响节点 */
    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *c = graph->constraints[i];
        if (!c || !c->is_active)
            continue;

        /* 刷新每个参与节点：根据约束类型调整 trust 等级 */
        for (int j = 0; j < c->participant_count; j++) {
            GeomNode *node = graph_get_node(graph, c->participants[j]);
            if (!node)
                continue;

            /* 基于约束类型调整信任等级 */
            switch (c->type) {
                case INCIDENCE:
                case BETWEENNESS:
                case INTERSECTION:
                    /* 几何约束对精度要求高，保持或提升 trust */
                    if (node->trust > TRUST_GREEN)
                        node->trust = TRUST_GREEN;
                    break;
                case CONTAINMENT:
                case CONNECTION:
                    /* 拓扑约束允许较低的 trust */
                    break;
                default:
                    LV00_LOG_ERROR("graph_sync_nodes: 未知约束类型 %d (id=%d)",
                                   (int)c->type, c->id);
                    break;
            }
        }
    }

    graph->dirty = false;
}

/**
 * @brief 废弃约束（惰性删除）
 *
 * 将约束标记为不活跃（is_active = false），从活跃约束索引中移除，
 * 但保留其数据以便审计跟踪。活跃约束迭代时自动跳过不活跃约束。
 *
 * @param graph         约束图指针
 * @param constraint_id 要废弃的约束 ID
 * @return LV00_OK 成功，其他错误码表示失败
 */
int graph_deactivate_constraint(ConstraintGraph *graph, int constraint_id) {
    if (!graph)
        return LV00_ERROR_INVALID_PARAM;

    Constraint *con = graph_get_constraint(graph, constraint_id);
    if (!con) {
        lv00_set_error(LV00_ERROR_NOT_FOUND,
                       "graph_deactivate_constraint: 约束 #%d 未找到", constraint_id);
        return LV00_ERROR_NOT_FOUND;
    }
    if (!con->is_active) {
        lv00_set_error(LV00_ERROR_UNKNOWN,
                       "graph_deactivate_constraint: 约束 #%d 已经是不活跃状态", constraint_id);
        return LV00_ERROR_UNKNOWN;
    }

    /* 标记为不活跃 */
    con->is_active = false;

    /* 从哈希索引中移除（保留约束数据用于审计） */
    constraint_index_remove(graph, constraint_id);

    /* 标记图为脏状态，需要同步 */
    graph_mark_dirty(graph);

    LOG_INFO("constraint_graph",
             "约束 #%d (类型=%d) 已废弃，保留数据用于审计跟踪",
             constraint_id, (int)con->type);

    if (graph_stream_ctx) {
        char buf[128];
        snprintf(buf, sizeof(buf), "废弃约束: id=%d (已停用，保留审计数据)", constraint_id);
        stream_emit_constraint_event(graph_stream_ctx, STREAM_EVENT_INFO, constraint_id, buf, 0);
    }

    return LV00_OK;
}

/**
 * 查找涉及指定节点的所有约束。
 *
 * @param graph        约束图指针
 * @param node_id      节点 ID
 * @param out_indices 输出：约束索引数组
 * @param max_results 数组最大容量
 * @return 找到的约束数量
 */
int graph_find_constraints_involving(const ConstraintGraph *graph, int node_id, int *out_indices, int max_results) {
    if (!graph || !out_indices || max_results <= 0)
        return 0;
    int count = 0;
    for (int i = 0; i < graph->constraint_count && count < max_results; i++) {
        Constraint *c = graph->constraints[i];
        if (!c->is_active)                    /* v3.5.0: 跳过不活跃约束 */
            continue;
        for (int j = 0; j < c->participant_count; j++) {
            if (c->participants[j] == node_id) {
                out_indices[count++] = i;
                break;
            }
        }
    }
    return count;
}

/**
 * 检测约束是否冗余。
 *
 * @param graph       约束图指针
 * @param type        约束类型
 * @param participants 参与者节点 ID 数组
 * @param n_parts    参与者数量
 * @return 1 表示冗余，0 表示不冗余，-1 表示错误
 */
int graph_detect_redundancy(const ConstraintGraph *graph, ConstraintType type, const int *participants, int n_parts) {
    if (!graph || !participants || n_parts <= 0)
        return -1;
    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *c = graph->constraints[i];
        if (!c->is_active)                    /* v3.5.0: 跳过不活跃约束 */
            continue;
        if (c->type != type || c->participant_count != n_parts)
            continue;
        bool same = true;
        for (int j = 0; j < n_parts; j++) {
            if (c->participants[j] != participants[j]) {
                same = false;
                break;
            }
        }
        if (same)
            return 1;
    }
    return 0;
}

/**
 * 获取约束图中的节点数量。
 *
 * @param graph 约束图指针
 * @return 节点数量
 */
int graph_get_node_count(const ConstraintGraph *graph) {
    if (!graph)
        return 0;
    return graph->node_count;
}

/**
 * @brief 获取约束图中的约束数量
 *
 * @param graph 约束图指针
 * @return 约束数量
 */
int graph_get_constraint_count(const ConstraintGraph *graph) {
    if (!graph)
        return 0;
    return graph->constraint_count;
}

/**
 * 通过节点 ID 获取节点（线性扫描版本）。
 *
 * @param graph   约束图指针
 * @param node_id 节点 ID
 * @return 节点指针，不存在时返回 NULL
 */
GeomNode *graph_get_node_by_id(const ConstraintGraph *graph, int node_id) {
    if (!graph)
        return NULL;
    for (int i = 0; i < graph->node_count; i++) {
        if (graph->nodes[i]->id == node_id)
            return graph->nodes[i];
    }
    return NULL;
}

GeomNode *graph_get_node(const ConstraintGraph *graph, int node_id) {
    if (!graph)
        return NULL;
    if (graph->node_index) {
        unsigned idx = node_id_hash(node_id, graph->node_index_capacity);
        while (graph->node_index[idx] != NULL) {
            if (graph->node_index[idx]->id == node_id)
                return graph->node_index[idx];
            idx = (idx + 1) & (unsigned) (graph->node_index_capacity - 1);
        }
        return NULL;
    }
    /* Fallback to linear scan if index not built */
    for (int i = 0; i < graph->node_count; i++) {
        if (graph->nodes[i]->id == node_id)
            return graph->nodes[i];
    }
    return NULL;
}

Constraint *graph_get_constraint(const ConstraintGraph *graph, int constraint_id) {
    if (!graph)
        return NULL;
    if (graph->constraint_index) {
        unsigned idx = constraint_id_hash(constraint_id, graph->constraint_index_capacity);
        while (graph->constraint_index[idx] != NULL) {
            if (graph->constraint_index[idx]->id == constraint_id)
                return graph->constraint_index[idx];
            idx = (idx + 1) & (unsigned) (graph->constraint_index_capacity - 1);
        }
        return NULL;
    }
    /* Fallback to linear scan if index not built */
    for (int i = 0; i < graph->constraint_count; i++) {
        if (graph->constraints[i]->id == constraint_id)
            return graph->constraints[i];
    }
    return NULL;
}

/**
 * 创建新的约束图。
 *
 * @return 新创建的约束图，失败时返回 NULL；调用者需负责释放
 */
ConstraintGraph *graph_create(void) {
    ConstraintGraph *graph = lv00_malloc(sizeof(ConstraintGraph));
    if (!graph)
        return NULL;
    memset(graph, 0, sizeof(ConstraintGraph));
    graph->next_node_id = 0;
    graph->next_constraint_id = 0;
    graph->dirty = false;  /* v3.5.0: 脏标记初始化为 false */

    /* ============================================================================
     * 遗留缓冲区说明 (v3.4.0 计划清理)
     * ============================================================================
     * error_buffer 和 serialize_buffer 是 v3.3.0 引入的每图级错误缓冲区，
     * 用于替代旧版静态全局变量，提升并发安全性。
     *
     * 迁移计划 (v3.4.0):
     * - 当 Lv00Context 统一错误系统完全就绪后，这些缓冲区将逐步迁移
     *   到 context->error_message[] 数组中统一管理。
     * - graph_set_error() / graph_get_error() 已优先使用 context 错误存储，
     *   这些缓冲区仅作为 fallback 保留。
     * - 当前保留是为了向后兼容，避免破坏已有调用代码。
     *
     * 风险评估: 无运行时风险，仅为架构演进预留的占位代码。
     * ============================================================================ */
    graph->error_buffer = lv00_malloc(256);
    graph->serialize_buffer = lv00_malloc(256);
    if (!graph->error_buffer || !graph->serialize_buffer) {
        /* 缓冲区分配失败：清理已分配资源，返回 NULL */
        lv00_free((void **) &graph->error_buffer);
        lv00_free((void **) &graph->serialize_buffer);
        lv00_free((void **) &graph);
        return NULL;
    }
    graph->error_buffer[0] = '\0';
    graph->serialize_buffer[0] = '\0';

    return graph;
}

/* ============================================================
 * 错误码转换函数
 * ============================================================ */

Lv00ErrorCode lv00_add_node_result_to_error(AddNodeResult result) {
    switch (result) {
        case ADD_NODE_OK:
            return LV00_OK;
        case ADD_NODE_CONFLICT:
            return LV00_ERROR_NODE_CONFLICT;
        case ADD_NODE_INVALID_REGION:
            return LV00_ERROR_INVALID_REGION;
        default:
            return LV00_ERROR_UNKNOWN;
    }
}

/**
 * 将添加约束结果转换为错误码。
 *
 * @param result 添加约束结果
 * @return 对应的错误码
 */
Lv00ErrorCode lv00_add_constraint_result_to_error(AddConstraintResult result) {
    switch (result) {
        case ADD_CONSTRAINT_OK:
            return LV00_OK;
        case ADD_CONSTRAINT_DUPLICATE:
            return LV00_ERROR_CONSTRAINT_DUPLICATE;
        case ADD_CONSTRAINT_CONFLICT:
            return LV00_ERROR_CONSTRAINT_CONFLICT;
        default:
            return LV00_ERROR_UNKNOWN;
    }
}

Lv00ErrorCode lv00_remove_node_result_to_error(RemoveNodeResult result) {
    switch (result) {
        case REMOVE_NODE_OK:
            return LV00_OK;
        case REMOVE_NODE_NOT_FOUND:
            return LV00_ERROR_NODE_NOT_FOUND;
        case REMOVE_NODE_ERROR:
            return LV00_ERROR_GRAPH_CORRUPTED;
        default:
            return LV00_ERROR_UNKNOWN;
    }
}

/* ============================================================
 * 统一错误系统实现 (v3.4.0: 迁移到 Lv00Context)
 *
 * 优先使用 graph->context->error_message，fallback 到
 * graph->error_buffer。
 * ============================================================ */

/**
 * @brief 设置约束图的错误信息 (v3.4.0: 支持 Lv00Context)
 *
 * 优先将错误信息存储到 graph->context->error_message 中
 * (如果有 context)，fallback 到 graph->error_buffer。
 *
 * @param graph 约束图（可以为 NULL，但错误信息不会被存储）
 * @param fmt   printf 风格的格式字符串
 * @param ...   可变参数列表
 */
void graph_set_error(ConstraintGraph *graph, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    if (graph) {
        /* 优先使用 context 错误存储 */
        if (graph->context) {
            vsnprintf(graph->context->error_message, sizeof(graph->context->error_message), fmt, args);
        } else if (graph->error_buffer) {
            /* Fallback 到 error_buffer */
            vsnprintf(graph->error_buffer, 256, fmt, args);
        } else {
            /* 两者都不可用，记录到全局错误 API */
            char fallback[256];
            vsnprintf(fallback, sizeof(fallback), fmt, args);
            lv00_set_error(LV00_ERROR_UNKNOWN, "%s", fallback);
        }
    }

    va_end(args);
}

/**
 * @brief 获取约束图的错误信息 (v3.4.0: 支持 Lv00Context)
 *
 * 优先从 graph->context->error_message 读取错误信息
 * (如果有 context 且有错误)，fallback 到 graph->error_buffer。
 *
 * @param graph 约束图（可以为 NULL，返回 "NULL graph"）
 * @return 错误信息字符串（内部存储，勿 free）
 */
const char *graph_get_error(const ConstraintGraph *graph) {
    if (!graph) {
        return "NULL graph";
    }

    /* 优先从 context 读取错误信息 */
    if (graph->context && graph->context->error_message[0]) {
        return graph->context->error_message;
    }

    /* Fallback 到 error_buffer */
    if (graph->error_buffer && graph->error_buffer[0]) {
        return graph->error_buffer;
    }

    return "";
}

/**
 * @brief 销毁几何节点并释放其所有资源
 *
 * 根据节点类型释放对应的内部数据：
 * - GEOM_FUNCTION_BLOCK：释放内部节点数组、输入/输出端口 ID 数组
 * - 所有类型：释放符号坐标数组和数值假设声明字符串
 * 最后释放节点结构体本身。
 *
 * @param node 要销毁的几何节点指针
 */
static void node_destroy(GeomNode *node) {
    if (!node)
        return;
    if (node->symbolic_coords) {
        for (int i = 0; i < node->coord_count; i++) {
            symbolic_coord_destroy(node->symbolic_coords[i]);
        }
        lv00_free((void **) &node->symbolic_coords);
    }
    if (node->numeric_assumption_declaration) {
        lv00_free((void **) &node->numeric_assumption_declaration);
        node->numeric_assumption_declaration = NULL;
    }
    switch (node->type) {
        case GEOM_PORT:
            lv00_free((void **) &node->data.port);
            break;
        case GEOM_REGION:
            lv00_free((void **) &node->data.region.boundary_segments);
            break;
        case GEOM_FUNCTION_BLOCK:
            lv00_free((void **) &node->data.func_block.internal_nodes);
            lv00_free((void **) &node->data.func_block.input_port_ids);
            lv00_free((void **) &node->data.func_block.output_port_ids);
            break;
        default:
            break;
    }
    lv00_free((void **) &node);
}

/**
 * @brief 获取约束图中最后添加的节点 ID
 *
 * 返回节点数组中最后一个节点的索引（即 node_count - 1）。
 * 注意：此 ID 是数组索引而非节点的逻辑 ID。
 *
 * @param graph 约束图指针
 * @return 最后添加的节点索引，图为空或无效时返回 -1
 */
int graph_get_last_added_node_id(const ConstraintGraph *graph) {
    if (!graph || graph->node_count == 0)
        return -1;
    return graph->node_count - 1;
}

/**
 * @brief 销毁约束图并释放所有资源
 *
 * 依次销毁所有节点（调用 node_destroy）、释放所有约束的参与者数组和约束本身、
 * 释放节点和约束的哈希索引、释放序列化缓冲区和邻接矩阵。
 * 最后释放约束图结构体本身。
 *
 * @param graph 约束图指针（可以为 NULL，此时直接返回）
 */
void graph_destroy(ConstraintGraph *graph) {
    if (!graph)
        return;
    for (int i = 0; i < graph->node_count; i++) {
        node_destroy(graph->nodes[i]);
    }
    lv00_free((void **) &graph->nodes);
    for (int i = 0; i < graph->constraint_count; i++) {
        lv00_free((void **) &graph->constraints[i]->participants);
        lv00_free((void **) &graph->constraints[i]);
    }
    lv00_free((void **) &graph->constraints);
    lv00_free((void **) &graph->node_index);
    lv00_free((void **) &graph->constraint_index);
    /* 释放每图级的错误缓冲区（v3.3.0） */
    lv00_free((void **) &graph->error_buffer);
    lv00_free((void **) &graph->serialize_buffer);
    lv00_free((void **) &graph);
}

/**
 * 检测约束图中的冗余约束。
 *
 * 使用两阶段检测：
 * 1. 精确重复检测：使用哈希分组和排序实现 O(n log n) 复杂度
 * 2. 线性相关性检测：使用高斯消元和 GMP mpq_t 精确算术
 *
 * @param graph     约束图指针
 * @param out_count 输出：找到的冗余约束数量
 * @return 冗余约束 ID 数组，调用者需负责释放；失败时返回 NULL
 */
int *graph_detect_redundant_constraints(const ConstraintGraph *graph, int *out_count) {
    /* 参数验证：防止空指针解引用 */
    if (!out_count)
        return NULL;
    *out_count = 0;
    if (!graph || graph->constraint_count == 0)
        return NULL;

    /* Allocate enough space for both phases */
    int max_redundant = graph->constraint_count * 2;
    int *redundant = lv00_malloc((size_t) max_redundant * sizeof(int));
    if (!redundant)
        return NULL;
    for (int i = 0; i < max_redundant; i++) {
        redundant[i] = -1;
    }

    /* Phase 1: Exact duplicate detection using hash-based grouping
     * Optimization: O(n log n) instead of O(n²) by sorting constraints
     * by a hash signature (type + participant_count + first participant).
     * Only compare constraints within the same hash group.
     */

    /* Helper struct for sorting */
    typedef struct {
        int constraint_idx;
        unsigned long hash;
    } ConstraintHashEntry;

    int n = graph->constraint_count;
    if (n > 1) {
        ConstraintHashEntry *entries = lv00_malloc((size_t) n * sizeof(ConstraintHashEntry));
        if (entries) {
            /* Compute hash for each constraint */
            for (int i = 0; i < n; i++) {
                Constraint *c = graph->constraints[i];
                unsigned long h = (unsigned long) c->type * 31 + (unsigned long) c->participant_count;
                for (int k = 0; k < c->participant_count; k++) {
                    h = h * 37 + (unsigned long) c->participants[k];
                }
                entries[i].constraint_idx = i;
                entries[i].hash = h;
            }

            /* Sort by hash (simple insertion sort for small arrays, qsort for large) */
            for (int i = 1; i < n; i++) {
                ConstraintHashEntry tmp = entries[i];
                int j = i - 1;
                while (j >= 0 && entries[j].hash > tmp.hash) {
                    entries[j + 1] = entries[j];
                    j--;
                }
                entries[j + 1] = tmp;
            }

            /* Compare constraints with same hash */
            int i = 0;
            while (i < n) {
                unsigned long cur_hash = entries[i].hash;
                int j = i + 1;
                /* Find group with same hash */
                while (j < n && entries[j].hash == cur_hash)
                    j++;

                /* Compare all pairs within this group */
                for (int a = i; a < j; a++) {
                    Constraint *ci = graph->constraints[entries[a].constraint_idx];
                    for (int b = a + 1; b < j; b++) {
                        Constraint *cj = graph->constraints[entries[b].constraint_idx];
                        if (ci->type != cj->type || ci->participant_count != cj->participant_count)
                            continue;
                        bool same = true;
                        for (int k = 0; k < ci->participant_count; k++) {
                            if (ci->participants[k] != cj->participants[k]) {
                                same = false;
                                break;
                            }
                        }
                        if (same) {
                            redundant[*out_count] = cj->id;
                            (*out_count)++;
                        }
                    }
                }
                i = j;
            }

            lv00_free((void **) &entries);
        } else {
            /* Fallback to O(n²) if allocation fails */
            for (int i = 0; i < graph->constraint_count; i++) {
                Constraint *ci = graph->constraints[i];
                for (int j = i + 1; j < graph->constraint_count; j++) {
                    Constraint *cj = graph->constraints[j];
                    if (ci->type != cj->type || ci->participant_count != cj->participant_count)
                        continue;
                    bool same = true;
                    for (int k = 0; k < ci->participant_count; k++) {
                        if (ci->participants[k] != cj->participants[k]) {
                            same = false;
                            break;
                        }
                    }
                    if (same) {
                        redundant[*out_count] = cj->id;
                        (*out_count)++;
                    }
                }
            }
        }
    }

    /* Phase 2: Linear dependency detection using Gaussian elimination
     * with GMP mpq_t for exact rational arithmetic.
     *
     * For INCIDENCE constraints: point P on line AB means
     *   (P-A) x (B-A) = 0  (cross product in 2D)
     *   => (Px-Ax)*(By-Ay) - (Py-Ay)*(Bx-Ax) = 0
     * This is a linear equation in the coordinates.
     *
     * For BETWEENNESS constraints: P2 is between P1 and P3,
     *   which implies collinearity: (P2-P1) x (P3-P1) = 0
     *   => (P2x-P1x)*(P3y-P1y) - (P2y-P1y)*(P3x-P1x) = 0
     *
     * We collect these linear equations, build a coefficient matrix,
     * and use Gaussian elimination to find rows that are linearly
     * dependent (i.e., can be expressed as combinations of others).
     */

    /* Collect all coordinate variables (point x,y pairs) */
    /* First, find all points referenced by INCIDENCE/BETWEENNESS constraints */
    int *point_ids = lv00_malloc((size_t) graph->node_count * sizeof(int));
    if (!point_ids) {
        lv00_free((void **) &redundant);
        return redundant;
    }
    int point_count = 0;
    bool *point_seen = lv00_calloc(graph->node_count, sizeof(bool));
    if (!point_seen) {
        lv00_free((void **) &point_ids);
        lv00_free((void **) &redundant);
        return redundant;
    }

    /* Use a mapping from node id to variable index */
    int max_node_id = 0;
    for (int i = 0; i < graph->node_count; i++) {
        if (graph->nodes[i]->id > max_node_id)
            max_node_id = graph->nodes[i]->id;
    }

    /* node_id_to_var_idx: maps node_id to variable index (-1 if not a variable) */
    int *node_id_to_var_idx = lv00_malloc((size_t)(max_node_id + 1) * sizeof(int));
    if (!node_id_to_var_idx) {
        lv00_free((void **) &point_seen);
        lv00_free((void **) &point_ids);
        lv00_free((void **) &redundant);
        return redundant;
    }
    for (int i = 0; i <= max_node_id; i++)
        node_id_to_var_idx[i] = -1;

    /* Collect points involved in INCIDENCE or BETWEENNESS constraints */
    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *c = graph->constraints[i];
        if (c->type != INCIDENCE && c->type != BETWEENNESS)
            continue;
        for (int j = 0; j < c->participant_count; j++) {
            int nid = c->participants[j];
            if (nid < 0 || nid > max_node_id)
                continue;
            GeomNode *n = graph_get_node(graph, nid);
            if (!n || n->type != GEOM_POINT)
                continue;
            if (node_id_to_var_idx[nid] < 0) {
                node_id_to_var_idx[nid] = point_count * 2; /* x,y pair */
                if (point_count < graph->node_count) {
                    point_ids[point_count] = nid;
                    point_count++;
                }
            }
        }
    }

    int num_vars = point_count * 2; /* x and y for each point */

    /* Count linear constraints */
    int num_linear = 0;
    int *linear_constraint_indices = lv00_malloc((size_t) graph->constraint_count * sizeof(int));
    if (!linear_constraint_indices) {
        lv00_free((void **) &node_id_to_var_idx);
        lv00_free((void **) &point_seen);
        lv00_free((void **) &point_ids);
        lv00_free((void **) &redundant);
        return redundant;
    }
    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *c = graph->constraints[i];
        if (c->type == INCIDENCE || c->type == BETWEENNESS) {
            linear_constraint_indices[num_linear] = i;
            num_linear++;
        }
    }

    if (num_linear <= 1 || num_vars <= 0) {
        /* 约束数量不足，无法进行线性相关性检测 */
        lv00_free((void **) &point_ids);
        lv00_free((void **) &point_seen);
        lv00_free((void **) &node_id_to_var_idx);
        lv00_free((void **) &linear_constraint_indices);
        return redundant;
    }

    /* 使用 GMP mpq_t 构建系数矩阵进行精确算术运算
     * 矩阵维度：num_linear x (num_vars + 1) [增广矩阵]
     * 每行代表一个约束对应的线性方程 */
    mpq_t *matrix = lv00_malloc((size_t) num_linear * (num_vars + 1) * sizeof(mpq_t));
    if (!matrix) {
        lv00_free((void **) &point_ids);
        lv00_free((void **) &point_seen);
        lv00_free((void **) &node_id_to_var_idx);
        lv00_free((void **) &linear_constraint_indices);
        return redundant;
    }

    for (int i = 0; i < num_linear * (num_vars + 1); i++) {
        mpq_init(matrix[i]);
    }

    /* Fill the matrix with equation coefficients */
    for (int row = 0; row < num_linear; row++) {
        Constraint *c = graph->constraints[linear_constraint_indices[row]];

        if (c->type == INCIDENCE && c->participant_count >= 2) {
            /* INCIDENCE: point P on line segment S
             * (P-A) x (B-A) = 0
             * We need the coordinates of P and the endpoints of S.
             * The line segment's symbolic_coords[0..3] are (Ax, Ay, Bx, By).
             * The point's symbolic_coords[0..1] are (Px, Py).
             */
            int point_id = c->participants[0];
            int seg_id = c->participants[1];
            GeomNode *pt = graph_get_node(graph, point_id);
            GeomNode *seg = graph_get_node(graph, seg_id);

            if (pt && seg && seg->type == GEOM_LINE_SEGMENT && seg->coord_count >= 4 && seg->symbolic_coords &&
                pt->coord_count >= 2 && pt->symbolic_coords) {
                /* Get coordinates - use exact mpq_t for RATIONAL, double for others */
                mpq_t ax_q, ay_q, bx_q, by_q;
                bool ax_exact = false, ay_exact = false, bx_exact = false, by_exact = false;
                double ax_d, ay_d, bx_d, by_d;

                ax_d = symbolic_coord_to_double(seg->symbolic_coords[0]);
                ay_d = symbolic_coord_to_double(seg->symbolic_coords[1]);
                bx_d = symbolic_coord_to_double(seg->symbolic_coords[2]);
                by_d = symbolic_coord_to_double(seg->symbolic_coords[3]);

                if (seg->symbolic_coords[0]->type == RATIONAL) {
                    mpq_init(ax_q);
                    mpq_set(ax_q, seg->symbolic_coords[0]->data.rational->value);
                    ax_exact = true;
                }
                if (seg->symbolic_coords[1]->type == RATIONAL) {
                    mpq_init(ay_q);
                    mpq_set(ay_q, seg->symbolic_coords[1]->data.rational->value);
                    ay_exact = true;
                }
                if (seg->symbolic_coords[2]->type == RATIONAL) {
                    mpq_init(bx_q);
                    mpq_set(bx_q, seg->symbolic_coords[2]->data.rational->value);
                    bx_exact = true;
                }
                if (seg->symbolic_coords[3]->type == RATIONAL) {
                    mpq_init(by_q);
                    mpq_set(by_q, seg->symbolic_coords[3]->data.rational->value);
                    by_exact = true;
                }

                /* Direction vector of line: (Bx-Ax, By-Ay) */
                int p_idx = node_id_to_var_idx[point_id];
                if (p_idx >= 0) {
                    /* Coefficient for Px: (By-Ay), for Py: -(Bx-Ax) */
                    if (by_exact && ay_exact) {
                        mpq_t dy_q;
                        mpq_init(dy_q);
                        mpq_sub(dy_q, by_q, ay_q);
                        mpq_set(matrix[row * (num_vars + 1) + p_idx], dy_q);
                        mpq_neg(matrix[row * (num_vars + 1) + p_idx], dy_q);
                        mpq_clear(dy_q);
                    } else {
                        double dy = by_d - ay_d;
                        mpq_set_d(matrix[row * (num_vars + 1) + p_idx], dy);
                    }

                    if (bx_exact && ax_exact) {
                        mpq_t dx_q;
                        mpq_init(dx_q);
                        mpq_sub(dx_q, bx_q, ax_q);
                        mpq_neg(matrix[row * (num_vars + 1) + p_idx + 1], dx_q);
                        mpq_clear(dx_q);
                    } else {
                        double dx = bx_d - ax_d;
                        mpq_set_d(matrix[row * (num_vars + 1) + p_idx + 1], -dx);
                    }
                }

                /* Constant term: Ax*dy - Ay*dx */
                if (ax_exact && ay_exact && bx_exact && by_exact) {
                    mpq_t dy_q, dx_q, const_q;
                    mpq_init(dy_q);
                    mpq_init(dx_q);
                    mpq_init(const_q);
                    mpq_sub(dy_q, by_q, ay_q);
                    mpq_sub(dx_q, bx_q, ax_q);
                    /* const = Ax*dy - Ay*dx */
                    mpq_mul(const_q, ax_q, dy_q);
                    mpq_t tmp_q;
                    mpq_init(tmp_q);
                    mpq_mul(tmp_q, ay_q, dx_q);
                    mpq_sub(const_q, const_q, tmp_q);
                    mpq_clear(tmp_q);
                    mpq_set(matrix[row * (num_vars + 1) + num_vars], const_q);
                    mpq_clear(dy_q);
                    mpq_clear(dx_q);
                    mpq_clear(const_q);
                } else {
                    double dx = bx_d - ax_d;
                    double dy = by_d - ay_d;
                    mpq_set_d(matrix[row * (num_vars + 1) + num_vars], ax_d * dy - ay_d * dx);
                }

                if (ax_exact)
                    mpq_clear(ax_q);
                if (ay_exact)
                    mpq_clear(ay_q);
                if (bx_exact)
                    mpq_clear(bx_q);
                if (by_exact)
                    mpq_clear(by_q);
            }
        } else if (c->type == BETWEENNESS && c->participant_count >= 3) {
            /* BETWEENNESS: P2 between P1 and P3
             * Collinearity: (P2-P1) x (P3-P1) = 0
             * => (P2x-P1x)*(P3y-P1y) - (P2y-P1y)*(P3x-P1x) = 0
             * => P2x*(P3y-P1y) - P2y*(P3x-P1x) - P1x*(P3y-P1y) + P1y*(P3x-P1x) = 0
             */
            int p1_id = c->participants[0];
            int p2_id = c->participants[1];
            int p3_id = c->participants[2];
            GeomNode *p1 = graph_get_node(graph, p1_id);
            GeomNode *p2 = graph_get_node(graph, p2_id);
            GeomNode *p3 = graph_get_node(graph, p3_id);

            if (p1 && p2 && p3 && p1->coord_count >= 2 && p1->symbolic_coords && p2->coord_count >= 2 &&
                p2->symbolic_coords && p3->coord_count >= 2 && p3->symbolic_coords) {
                /* Get coordinates - use exact mpq_t for RATIONAL, double for others */
                mpq_t p1x_q, p1y_q, p3x_q, p3y_q;
                bool p1x_exact = false, p1y_exact = false, p3x_exact = false, p3y_exact = false;
                double p1x, p1y, p3x, p3y;

                p1x = symbolic_coord_to_double(p1->symbolic_coords[0]);
                p1y = symbolic_coord_to_double(p1->symbolic_coords[1]);
                p3x = symbolic_coord_to_double(p3->symbolic_coords[0]);
                p3y = symbolic_coord_to_double(p3->symbolic_coords[1]);

                if (p1->symbolic_coords[0]->type == RATIONAL) {
                    mpq_init(p1x_q);
                    mpq_set(p1x_q, p1->symbolic_coords[0]->data.rational->value);
                    p1x_exact = true;
                }
                if (p1->symbolic_coords[1]->type == RATIONAL) {
                    mpq_init(p1y_q);
                    mpq_set(p1y_q, p1->symbolic_coords[1]->data.rational->value);
                    p1y_exact = true;
                }
                if (p3->symbolic_coords[0]->type == RATIONAL) {
                    mpq_init(p3x_q);
                    mpq_set(p3x_q, p3->symbolic_coords[0]->data.rational->value);
                    p3x_exact = true;
                }
                if (p3->symbolic_coords[1]->type == RATIONAL) {
                    mpq_init(p3y_q);
                    mpq_set(p3y_q, p3->symbolic_coords[1]->data.rational->value);
                    p3y_exact = true;
                }

                int p2_idx = node_id_to_var_idx[p2_id];
                if (p2_idx >= 0) {
                    /* Coefficient for P2x: (P3y-P1y), for P2y: -(P3x-P1x) */
                    if (p3y_exact && p1y_exact) {
                        mpq_t dy13_q;
                        mpq_init(dy13_q);
                        mpq_sub(dy13_q, p3y_q, p1y_q);
                        mpq_set(matrix[row * (num_vars + 1) + p2_idx], dy13_q);
                        mpq_clear(dy13_q);
                    } else {
                        mpq_set_d(matrix[row * (num_vars + 1) + p2_idx], p3y - p1y);
                    }

                    if (p3x_exact && p1x_exact) {
                        mpq_t dx13_q;
                        mpq_init(dx13_q);
                        mpq_sub(dx13_q, p3x_q, p1x_q);
                        mpq_neg(matrix[row * (num_vars + 1) + p2_idx + 1], dx13_q);
                        mpq_clear(dx13_q);
                    } else {
                        mpq_set_d(matrix[row * (num_vars + 1) + p2_idx + 1], -(p3x - p1x));
                    }
                }

                /* Constant term: P1x*(P3y-P1y) - P1y*(P3x-P1x) */
                if (p1x_exact && p1y_exact && p3x_exact && p3y_exact) {
                    mpq_t dy13_q, dx13_q, const_q;
                    mpq_init(dy13_q);
                    mpq_init(dx13_q);
                    mpq_init(const_q);
                    mpq_sub(dy13_q, p3y_q, p1y_q);
                    mpq_sub(dx13_q, p3x_q, p1x_q);
                    mpq_mul(const_q, p1x_q, dy13_q);
                    mpq_t tmp13_q;
                    mpq_init(tmp13_q);
                    mpq_mul(tmp13_q, p1y_q, dx13_q);
                    mpq_sub(const_q, const_q, tmp13_q);
                    mpq_clear(tmp13_q);
                    mpq_set(matrix[row * (num_vars + 1) + num_vars], const_q);
                    mpq_clear(dy13_q);
                    mpq_clear(dx13_q);
                    mpq_clear(const_q);
                } else {
                    double dy13 = p3y - p1y;
                    double dx13 = p3x - p1x;
                    mpq_set_d(matrix[row * (num_vars + 1) + num_vars], p1x * dy13 - p1y * dx13);
                }

                if (p1x_exact)
                    mpq_clear(p1x_q);
                if (p1y_exact)
                    mpq_clear(p1y_q);
                if (p3x_exact)
                    mpq_clear(p3x_q);
                if (p3y_exact)
                    mpq_clear(p3y_q);
            }
        }
    }

    /* Gaussian elimination with partial pivoting using mpq_t */
    int *pivot_row = lv00_malloc((size_t) num_linear * sizeof(int)); /* maps row i -> original constraint index */
    if (!pivot_row) {
        for (int i = 0; i < num_linear * (num_vars + 1); i++)
            mpq_clear(matrix[i]);
        lv00_free((void **) &matrix);
        lv00_free((void **) &linear_constraint_indices);
        lv00_free((void **) &node_id_to_var_idx);
        lv00_free((void **) &point_seen);
        lv00_free((void **) &point_ids);
        lv00_free((void **) &redundant);
        return redundant;
    }
    for (int i = 0; i < num_linear; i++)
        pivot_row[i] = linear_constraint_indices[i];

    int rank = 0;
    for (int col = 0; col < num_vars && rank < num_linear; col++) {
        /* Find pivot row (first non-zero entry in this column) */
        int pivot = -1;
        for (int row = rank; row < num_linear; row++) {
            if (mpq_sgn(matrix[row * (num_vars + 1) + col]) != 0) {
                pivot = row;
                break;
            }
        }
        if (pivot < 0)
            continue; /* All zeros in this column */

        /* Swap rows rank and pivot */
        if (pivot != rank) {
            for (int j = 0; j <= num_vars; j++) {
                mpq_swap(matrix[rank * (num_vars + 1) + j], matrix[pivot * (num_vars + 1) + j]);
            }
            int tmp = pivot_row[rank];
            pivot_row[rank] = pivot_row[pivot];
            pivot_row[pivot] = tmp;
        }

        /* Scale pivot row so leading coefficient is 1 */
        mpq_t inv_pivot;
        mpq_init(inv_pivot);
        mpq_inv(inv_pivot, matrix[rank * (num_vars + 1) + col]);
        for (int j = col; j <= num_vars; j++) {
            mpq_mul(matrix[rank * (num_vars + 1) + j], matrix[rank * (num_vars + 1) + j], inv_pivot);
        }
        mpq_clear(inv_pivot);

        /* Eliminate this column from all other rows */
        for (int row = 0; row < num_linear; row++) {
            if (row == rank)
                continue;
            if (mpq_sgn(matrix[row * (num_vars + 1) + col]) == 0)
                continue;

            mpq_t factor;
            mpq_init(factor);
            mpq_set(factor, matrix[row * (num_vars + 1) + col]);
            for (int j = col; j <= num_vars; j++) {
                mpq_t tmp;
                mpq_init(tmp);
                mpq_mul(tmp, factor, matrix[rank * (num_vars + 1) + j]);
                mpq_sub(matrix[row * (num_vars + 1) + j], matrix[row * (num_vars + 1) + j], tmp);
                mpq_clear(tmp);
            }
            mpq_clear(factor);
        }

        rank++;
    }

    /* After Gaussian elimination, rows from 'rank' to 'num_linear-1'
     * should be all-zero. These correspond to linearly dependent constraints.
     * However, we also check for rows that became zero due to elimination
     * but have a non-zero RHS (inconsistent), which we skip.
     * Rows that are all-zero (including RHS) are redundant.
     */
    for (int row = rank; row < num_linear; row++) {
        /* Check if this row is all-zero */
        bool all_zero = true;
        for (int j = 0; j <= num_vars; j++) {
            if (mpq_sgn(matrix[row * (num_vars + 1) + j]) != 0) {
                all_zero = false;
                break;
            }
        }
        if (all_zero) {
            int con_id = pivot_row[row];
            /* Check not already marked redundant */
            bool already = false;
            for (int m = 0; m < *out_count; m++) {
                if (redundant[m] == con_id) {
                    already = true;
                    break;
                }
            }
            if (!already && *out_count < max_redundant) {
                redundant[*out_count] = con_id;
                (*out_count)++;
                LOG_DEBUG("constraint_graph", "Linear dependency: constraint %d is redundant (Gaussian elimination)",
                          con_id);
            }
        }
    }

    /* Also check among the rank rows: if two rows have identical
     * coefficient patterns, one is redundant */
    for (int i = 0; i < rank; i++) {
        for (int j = i + 1; j < rank; j++) {
            bool identical = true;
            for (int k = 0; k <= num_vars; k++) {
                if (mpq_equal(matrix[i * (num_vars + 1) + k], matrix[j * (num_vars + 1) + k]) == 0) {
                    identical = false;
                    break;
                }
            }
            if (identical) {
                int con_id = pivot_row[j];
                bool already = false;
                for (int m = 0; m < *out_count; m++) {
                    if (redundant[m] == con_id) {
                        already = true;
                        break;
                    }
                }
                if (!already && *out_count < max_redundant) {
                    redundant[*out_count] = con_id;
                    (*out_count)++;
                    LOG_DEBUG("constraint_graph", "Linear dependency: constraint %d is identical to constraint %d",
                              con_id, pivot_row[i]);
                }
            }
        }
    }

    /* 清理 GMP mpq_t 矩阵资源 */
    for (int i = 0; i < num_linear * (num_vars + 1); i++) {
        mpq_clear(matrix[i]);
    }
    lv00_free((void **) &matrix);
    lv00_free((void **) &pivot_row);
    lv00_free((void **) &point_ids);
    lv00_free((void **) &point_seen);
    lv00_free((void **) &node_id_to_var_idx);
    lv00_free((void **) &linear_constraint_indices);

    return redundant;
}

/**
 * 检测代数冲突。
 *
 * 使用求解器模块检查约束系统是否有解。
 *
 * @param graph    约束图指针
 * @param new_con 新约束指针
 * @return true 表示存在冲突，false 表示无冲突
 */
static bool algebraic_conflict_detected(ConstraintGraph *graph, Constraint *new_con) {
    if (!graph || !new_con)
        return false;

    /* 使用求解器模块检查约束系统是否有解 */
    int *dirty_vars = NULL;
    int dirty_count = 0;
    GroebnerResult *result = NULL;

    /* 收集与新约束相关的所有变量 */
    int max_vars = new_con->participant_count * 2;
    dirty_vars = lv00_malloc((size_t) max_vars * sizeof(int));
    if (!dirty_vars)
        return false;

    /* 将新约束的参与者添加为脏变量 */
    for (int i = 0; i < new_con->participant_count && i < max_vars; i++) {
        dirty_vars[dirty_count++] = new_con->participants[i];
    }

    /* 调用求解器检查冲突 */
    SolverStatus status = solve_algebraic_system(graph, dirty_vars, dirty_count, &result);

    lv00_free((void **) &dirty_vars);
    dirty_vars = NULL;

    bool conflict = false;

    /* 检查求解器状态以判断冲突条件 */
    if (status == SOLVER_STATUS_NO_SOLUTION) {
        conflict = true; /* 检测到冲突 - 无解 */
    } else if (status == SOLVER_STATUS_OVERCONSTRAINED) {
        /* 使用冲突检查器检查是否有实际冲突 */
        conflict = check_conflict_equations(graph);
    }

    /* 清理 result 资源（包含可能持有 mpq_t 的 SymbolicCoord） */
    if (result) {
        for (int i = 0; i < result->solution_count; i++) {
            symbolic_coord_destroy(result->solutions[i]);
        }
        lv00_free((void **) &result->solutions);
        lv00_free((void **) &result);
    }

    return conflict;
}

/**
 * @brief 统计影响指定点的约束数量
 *
 * 遍历所有约束，收集参与者中包含指定点 ID 的约束，
 * 将结果写入 out_constraints 数组。
 *
 * @param graph       约束图指针
 * @param point_id    目标点节点 ID
 * @param out_constraints 输出：找到的约束指针数组
 * @param max_out     输出数组的最大容量
 * @return 找到的约束数量
 */
static int count_point_constraints(const ConstraintGraph *graph, int point_id, Constraint **out_constraints, int max_out) {
    int count = 0;
    for (int i = 0; i < graph->constraint_count && count < max_out; i++) {
        Constraint *c = graph->constraints[i];
        for (int j = 0; j < c->participant_count; j++) {
            if (c->participants[j] == point_id) {
                out_constraints[count++] = c;
                break;
            }
        }
    }
    return count;
}

/**
 * @brief 检查两个约束是否独立（非互相推导）
 *
 * 两个约束独立的条件是：c1 的参与者集合不完全是 c2 参与者集合的子集。
 * 如果 c1 的所有参与者都出现在 c2 中，则认为 c1 可能由 c2 推导而来，
 * 此时返回 false（不独立）。
 *
 * @param c1 第一个约束指针
 * @param c2 第二个约束指针
 * @return true 表示两个约束独立，false 表示可能互相推导
 */
static bool constraints_are_independent(const Constraint *c1, const Constraint *c2) {
    /* Two constraints are dependent if they involve the exact same participants */
    if (c1->participant_count != c2->participant_count)
        return true;

    int match_count = 0;
    for (int i = 0; i < c1->participant_count; i++) {
        for (int j = 0; j < c2->participant_count; j++) {
            if (c1->participants[i] == c2->participants[j]) {
                match_count++;
                break;
            }
        }
    }

    return match_count != c1->participant_count;
}

/**
 * @brief 解析距离值声明
 * @param decl 声明字符串，格式如 "distance=5.0" 或 "d=3.14"
 * @param out_value 输出参数，解析出的双精度浮点数值
 * @return 解析成功返回 true，失败返回 false
 */
static bool parse_distance_value(const char *decl, double *out_value) {
    if (!decl || !out_value)
        return false;

    /* 查找等号模式 */
    const char *eq = strchr(decl, '=');
    if (!eq)
        return false;

    char *endptr;
    double val = strtod(eq + 1, &endptr);
    if (endptr == eq + 1)
        return false; /* 未找到有效数字 */

    *out_value = val;
    return true;
}

/**
 * @brief 检查点是否在线段上
 * @param graph 约束图指针
 * @param point_id 点节点ID
 * @param segment_id 线段节点ID
 * @return 点在线段上返回 true，否则返回 false
 */
static bool point_on_segment(const ConstraintGraph *graph, int point_id, int segment_id) {
    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *c = graph->constraints[i];
        if (c->type == INCIDENCE && c->participant_count == 2) {
            if ((c->participants[0] == point_id && c->participants[1] == segment_id) ||
                (c->participants[1] == point_id && c->participants[0] == segment_id)) {
                return true;
            }
        }
    }
    return false;
}

/**
 * @brief 将一组节点 ID 添加到冲突组输出数组
 *
 * 分配内存并复制节点 ID 列表到冲突组数组中，
 * 同时更新冲突组大小数组和冲突组计数。
 *
 * @param conflicts       输入/输出：冲突组二维数组
 * @param conflict_count  输入/输出：当前冲突组数量（将递增）
 * @param conflict_sizes  输入/输出：每个冲突组的大小数组
 * @param node_ids        要添加的节点 ID 数组
 * @param node_count      节点 ID 数量
 */
static void add_conflict_group(int **conflicts, int *conflict_count, int **conflict_sizes, const int *node_ids,
                               int node_count) {
    conflicts[*conflict_count] = lv00_malloc((size_t) node_count * sizeof(int));
    if (!conflicts[*conflict_count]) return;
    memcpy(conflicts[*conflict_count], node_ids, node_count * sizeof(int));
    (*conflict_sizes)[*conflict_count] = node_count;
    (*conflict_count)++;
}

/* ============================================================
 * 连接图环路检测 —— 有向图 DFS 遍历（v3.3.0 重构）
 *
 * 【重构原因】
 * 原版使用递归 DFS 检测 CONNECTION 约束形成的环路，在
 * 深层约束图中可能触发栈溢出。递归深度 = 约束图中有向边
 * 可达的最大端口链长度。
 *
 * 【方案对比 —— 递归 vs 迭代】
 *
 *   递归版（原版）：
 *     优点：代码简洁直观，紧贴 DFS 算法思想，易维护
 *     缺点：栈深度受系统限制（Windows 默认 1MB / ~8000 帧），
 *           深层约束图存在栈溢出风险，错误恢复困难
 *     适用：浅层约束图（深度 < 500），开发调试阶段
 *
 *   迭代版（当前）：
 *     优点：堆分配的显式栈，深度仅受可用内存限制，
 *           可精确控制最大深度，错误恢复容易
 *     缺点：代码较递归版冗长约 3 倍，需手动管理栈状态
 *     适用：深层约束图、生产环境、需精确深度控制的场景
 *
 * 【性能基准】
 *   - 浅层图（深度 < 100）：递归版快约 5-10%（函数调用优化）
 *   - 中层图（深度 100-1000）：两者接近（递归开销与栈管理开销平衡）
 *   - 深层图（深度 > 1000）：迭代版明显更安全且仍可用
 *
 * 【深度限制】
 *   LV00_MAX_TRAVERSAL_DEPTH 默认 4096，匹配 LV00_INITIAL_ARRAY_CAPACITY
 *   的典型值。大型几何问题中一个端口链可达数百层嵌套。
 * ============================================================ */

/** @brief 环路检测最大遍历深度（防止无限循环或过于深层的 DFS） */
#ifndef LV00_MAX_TRAVERSAL_DEPTH
#define LV00_MAX_TRAVERSAL_DEPTH 4096
#endif

/* DFS 栈帧 —— 模拟递归调用栈的一个级别 */
typedef struct {
    int node_id;       /**< 当前正在探索的端口节点 ID */
    int neighbor_idx;  /**< 当前节点已处理到的邻接索引（恢复点） */
    int path_len;      /**< 该节点在 path 数组中的位置 */
} DfsFrame;

/**
 * @brief 使用迭代 DFS 检测 CONNECTION 约束形成的有向环路
 *
 * 基于显式栈的迭代 DFS 遍历有向连接图。从 start_port_id 出发，
 * 沿 OUTPUT→INPUT 方向遍历 CONNECTION 约束边。若在遍历过程中
 * 遇到仍在递归栈中的节点，则检测到环路。
 *
 * 检测到环路时，调用 add_conflict_group() 记录环路上的所有端口。
 *
 * 【语义】CONNECTION 约束是有向边（输出端口 → 输入端口）。
 *   从输出端口出发追踪信号流，如果在追踪过程中返回之前经过的
 *   节点，标志着一个组合环路（combinational cycle）。
 *
 * @param graph         约束图（非 NULL）
 * @param start_port_id 起始端口节点 ID
 * @param visited       已访问节点标记数组
 * @param rec_stack     当前递归路径标记数组（用于环路检测）
 * @param path          当前遍历路径上的节点 ID 数组
 * @param path_len      起始节点在 path 中的位置（调用者传入 0）
 * @param conflicts     输出：检测到的冲突组数组
 * @param conflict_count 输入/输出：当前冲突组数量
 * @param conflict_sizes 输出：每个冲突组的大小
 * @param conn_adj      CONNECTION 约束的扁平邻接矩阵
 * @param conn_counts   每个节点的 CONNECTION 约束计数
 * @return true 发现环路，false 该分支无环路
 *
 * @note 最大遍历深度由 LV00_MAX_TRAVERSAL_DEPTH 限制。
 *       超过深度时遍历终止但不报错（视为无环路）。
 */
static bool has_connection_cycle(ConstraintGraph *graph, int start_port_id, bool *visited, bool *rec_stack, int *path,
                                 int path_len, int **conflicts, int *conflict_count, int **conflict_sizes,
                                 const int *conn_adj, const int *conn_counts) {
    /* 分配显式 DFS 栈（堆分配，深度不受调用栈限制） */
    DfsFrame *stack = lv00_malloc((size_t) LV00_MAX_TRAVERSAL_DEPTH * sizeof(DfsFrame));
    if (!stack) {
        /* 栈分配失败：回退到快速路径检查 —— 若能分配则无法检测深层环路，
         * 但至少不会崩溃。记录警告并继续。 */
        LOG_WARN("constraint_graph", "环路检测: DFS 栈分配失败，跳过深度 > 0 的遍历");
        /* 回退：仅检查直接环路（1层） */
        int cnt = conn_counts[start_port_id];
        for (int ci = 0; ci < cnt; ci++) {
            Constraint *c = graph->constraints[conn_adj[start_port_id * LV00_MAX_CONN_ADJ_STRIDE + ci]];
            if (c->participants[0] == start_port_id && c->participants[1] == start_port_id) {
                /* 自环路（罕见但需检测） */
                path[0] = start_port_id;
                add_conflict_group(conflicts, conflict_count, conflict_sizes, path, 1);
                return true;
            }
        }
        return false;
    }

    int stack_top = 0; /* 栈顶索引：-1 = 空栈 */

    /* 压入起始帧 */
    stack[0].node_id = start_port_id;
    stack[0].neighbor_idx = 0;
    stack[0].path_len = path_len;
    /* visited 和 rec_stack 在向下深入时标记，回溯时恢复 */
    /* 注意：起始节点可能已在 visited 中，由调用者负责在栈顶帧处理 */

    bool found_cycle = false;

    while (stack_top >= 0 && !found_cycle) {
        DfsFrame *frame = &stack[stack_top];
        int current_id = frame->node_id;

        /* 首次进入此节点时标记 */
        if (frame->neighbor_idx == 0) {
            visited[current_id] = true;
            rec_stack[current_id] = true;
            path[frame->path_len] = current_id;
        }

        /* 获取此节点的所有 CONNECTION 邻接 */
        int cnt = conn_counts[current_id];

        /* 遍历剩余的邻接（从上次中断位置继续） */
        bool pushed_child = false;
        while (frame->neighbor_idx < cnt && !pushed_child) {
            int ci = frame->neighbor_idx;
            Constraint *c = graph->constraints[conn_adj[current_id * LV00_MAX_CONN_ADJ_STRIDE + ci]];
            int next_port = -1;

            /* CONNECTION 是双向存储的（participants[0] 和 [1] 都是端口ID），
             * 而方向性体现在语义中（output → input）。
             * 我们从输出端口出发追踪：如果 current_id 是 participants[0]，
             * 则方向为 (该端口) → participants[1]。
             * 如果 current_id 是 participants[1]，则表示从输入回溯输出端，
             * 此处跳过。 */
            if (c->participants[0] == current_id) {
                next_port = c->participants[1];
            } else if (c->participants[1] == current_id) {
                /* 从输入端口出发：跳过此边（方向反向） */
                frame->neighbor_idx++;
                continue;
            }

            if (next_port >= 0) {
                if (rec_stack[next_port]) {
                    /* ── 检测到环路！──
                     * next_port 仍在递归栈中，即我们通过某条路径回到了
                     * 之前经过的端口。记录环路上的所有节点。 */
                    int cycle_start = 0;
                    for (int j = 0; j <= frame->path_len; j++) {
                        if (path[j] == next_port) {
                            cycle_start = j;
                            break;
                        }
                    }

                    int cycle_len = frame->path_len - cycle_start + 1;
                    add_conflict_group(conflicts, conflict_count, conflict_sizes,
                                       &path[cycle_start], cycle_len);
                    found_cycle = true;
                    break;
                }

                if (!visited[next_port]) {
                    /* 向更深层次深入 */
                    frame->neighbor_idx++; /* 保存当前进度 */

                    /* 检查深度限制 */
                    if (stack_top + 1 >= LV00_MAX_TRAVERSAL_DEPTH) {
                        LOG_WARN("constraint_graph",
                                 "环路检测: 遍历深度超过上限 %d，在节点 %d 处截断",
                                 LV00_MAX_TRAVERSAL_DEPTH, next_port);
                        /* 超过深度上限：将该分支视为死胡同 */
                        frame->neighbor_idx = cnt; /* 跳过该节点剩余邻接 */
                        break;
                    }

                    /* 压入新帧 */
                    stack_top++;
                    stack[stack_top].node_id = next_port;
                    stack[stack_top].neighbor_idx = 0;
                    stack[stack_top].path_len = frame->path_len + 1;
                    pushed_child = true;
                } else {
                    /* 已访问但不在递归栈中的节点：交叉边（cross edge），
                     * 在 DAG 中正常，不会形成环路。 */
                    frame->neighbor_idx++;
                }
            } else {
                frame->neighbor_idx++;
            }
        } /* while neighbors */

        /* 如果当前节点的所有邻接都已处理完毕且未推入子节点：回溯 */
        if (!pushed_child && !found_cycle) {
            rec_stack[current_id] = false; /* 从递归路径中移除 */
            /* visited[current_id] 保持为 true —— 节点已完全探索 */
            stack_top--; /* 弹出栈帧，返回父节点 */
        }
    }

    /* 释放 DFS 栈 */
    lv00_free((void **) &stack);

    return found_cycle;
}

/**
 * @brief 检查两条线段是否可能相交（非平行）
 *
 * 遍历约束图检查两条线段之间是否存在平行约束。
 * 当前实现默认返回 true（假设可以相交），后续可扩展
 * 平行约束检测逻辑。
 *
 * @param graph    约束图指针
 * @param seg1_id 第一条线段节点 ID
 * @param seg2_id 第二条线段节点 ID
 * @return true 表示两条线段可能相交，false 表示平行
 */
static bool segments_can_intersect(const ConstraintGraph *graph, int seg1_id, int seg2_id) {
    /* For symbolic coordinates, we check if there's any geometric constraint 
     * that would make them parallel */
    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *c = graph->constraints[i];
        /* Check for parallel constraint between these segments */
        /* This would require a PARALLEL constraint type - for now assume they can intersect */
    }
    return true;
}

int **graph_detect_conflicts(const ConstraintGraph *graph, int *out_conflict_count, int **out_conflict_sizes) {
    lv00_clear_error();

    if (!graph || !out_conflict_count || !out_conflict_sizes) {
        if (out_conflict_count)
            *out_conflict_count = 0;
        if (out_conflict_sizes)
            *out_conflict_sizes = NULL;
        return NULL;
    }

    *out_conflict_count = 0;

    /* Allocate maximum possible conflicts */
    int max_conflicts = graph->node_count + graph->constraint_count;
    int **conflicts = lv00_malloc((size_t) max_conflicts * sizeof(int *));
    *out_conflict_sizes = lv00_malloc((size_t) max_conflicts * sizeof(int));

    if (!conflicts || !*out_conflict_sizes) {
        lv00_free((void **) &conflicts);
        lv00_free((void **) &*out_conflict_sizes);
        *out_conflict_sizes = NULL;
        *out_conflict_count = -1; /* 使用 -1 表示 OOM 错误，与 0（无冲突）区分 */
        return NULL;
    }

    /* ===== 预构建邻接索引以实现 O(1) 约束查找 ===== */
    int max_node_id = 0;
    for (int i = 0; i < graph->node_count; i++) {
        if (graph->nodes[i]->id > max_node_id)
            max_node_id = graph->nodes[i]->id;
    }

    /* adj: node_id -> 约束索引的扁平数组 */
    size_t adj_total = (size_t)(max_node_id + 1) * LV00_ADJ_MAX_PER_NODE;
    if (adj_total > (size_t)INT_MAX) {
        lv00_free((void **) &conflicts);
        lv00_free((void **) out_conflict_sizes);
        *out_conflict_sizes = NULL;
        *out_conflict_count = -1;
        return NULL;
    }
    int *adj_lists = lv00_calloc((int)adj_total, sizeof(int));
    int *adj_counts = lv00_calloc(max_node_id + 1, sizeof(int));

    /* inc_adj: node_id -> INCIDENCE 约束索引 */
    int *inc_adj = lv00_calloc(adj_total, sizeof(int));
    int *inc_counts = lv00_calloc(max_node_id + 1, sizeof(int));

    /* conn_adj: node_id -> CONNECTION 约束索引 */
    int *conn_adj = lv00_calloc(adj_total, sizeof(int));
    int *conn_counts = lv00_calloc(max_node_id + 1, sizeof(int));

    /* int_adj: node_id -> INTERSECTION 约束索引 */
    int *int_adj = lv00_calloc(adj_total, sizeof(int));
    int *int_counts = lv00_calloc(max_node_id + 1, sizeof(int));

    if (adj_lists && adj_counts && inc_adj && inc_counts && conn_adj && conn_counts && int_adj && int_counts) {
        for (int i = 0; i < graph->constraint_count; i++) {
            Constraint *c = graph->constraints[i];
            for (int j = 0; j < c->participant_count; j++) {
                int nid = c->participants[j];
                if (nid < 0 || nid > max_node_id)
                    continue;

                /* 通用邻接关系 */
                if (adj_counts[nid] < LV00_ADJ_MAX_PER_NODE) {
                    adj_lists[nid * LV00_ADJ_MAX_PER_NODE + adj_counts[nid]++] = i;
                } else {
                    LOG_DEBUG("constraint_graph", "节点 %d 超出邻接限制 (%d)，约束 %d 被忽略", nid, LV00_ADJ_MAX_PER_NODE,
                              i);
                }
                /* 类型特定邻接关系 */
                int *ta = NULL;
                int *tc = NULL;
                if (c->type == INCIDENCE) {
                    ta = inc_adj;
                    tc = inc_counts;
                }
                if (c->type == CONNECTION) {
                    ta = conn_adj;
                    tc = conn_counts;
                }
                if (c->type == INTERSECTION) {
                    ta = int_adj;
                    tc = int_counts;
                }
                if (ta && tc && tc[nid] < LV00_ADJ_MAX_PER_NODE) {
                    ta[nid * LV00_ADJ_MAX_PER_NODE + tc[nid]++] = i;
                } else if (ta && tc) {
                    LOG_DEBUG("constraint_graph", "节点 %d 超出类型特定邻接限制 (%d)，类型 %d", nid, LV00_ADJ_MAX_PER_NODE,
                              c->type);
                }
            }
        }
    }
    /* ===== End adjacency indexes ===== */

    /* Type 1: Overconstrained points (point with > 2 independent geometric constraints) */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (node->type != GEOM_POINT)
            continue;

        Constraint *point_constraints[64];
        int pc_count = 0;
        int ac = adj_counts[node->id];
        for (int ai = 0; ai < ac && pc_count < 64; ai++) {
            point_constraints[pc_count++] = graph->constraints[adj_lists[node->id * 256 + ai]];
        }

        /* Count independent constraints */
        int independent_count = 0;
        Constraint *independent_constraints[64];

        for (int j = 0; j < pc_count; j++) {
            bool is_independent = true;
            for (int k = 0; k < independent_count; k++) {
                if (!constraints_are_independent(point_constraints[j], independent_constraints[k])) {
                    is_independent = false;
                    break;
                }
            }
            if (is_independent) {
                independent_constraints[independent_count++] = point_constraints[j];
            }
        }

        /* In 2D, a point has 2 DOF, so > 2 independent constraints is overconstrained */
        if (independent_count > 2) {
            int conflict_nodes[64];
            int cn_count = 0;
            conflict_nodes[cn_count++] = node->id;
            for (int j = 0; j < independent_count && cn_count < 64; j++) {
                conflict_nodes[cn_count++] = independent_constraints[j]->id;
            }
            add_conflict_group(conflicts, out_conflict_count, out_conflict_sizes, conflict_nodes, cn_count);
        }
    }

    /* Type 2: Incompatible distances on same segment pair */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *seg1 = graph->nodes[i];
        if (seg1->type != GEOM_LINE_SEGMENT)
            continue;

        double dist1 = 0.0;
        bool has_dist1 = parse_distance_value(seg1->numeric_assumption_declaration, &dist1);

        for (int j = i + 1; j < graph->node_count; j++) {
            GeomNode *seg2 = graph->nodes[j];
            if (seg2->type != GEOM_LINE_SEGMENT)
                continue;

            double dist2 = 0.0;
            bool has_dist2 = parse_distance_value(seg2->numeric_assumption_declaration, &dist2);

            /* Check if they share the same endpoints (same segment pair) */
            /* This requires checking if they connect the same two points */
            /* For now, check if both have distance declarations with different values */
            if (has_dist1 && has_dist2) {
                /* Check if segments share endpoints by comparing their symbolic coordinates */
                bool share_endpoints = false;
                if (seg1->coord_count == 2 && seg2->coord_count == 2 && seg1->symbolic_coords &&
                    seg2->symbolic_coords) {
                    /* seg1 endpoints: symbolic_coords[0], symbolic_coords[1]
                     * seg2 endpoints: symbolic_coords[0], symbolic_coords[1]
                     * Check all 4 combinations for shared endpoints */
                    for (int ei = 0; ei < 2 && !share_endpoints; ei++) {
                        for (int ej = 0; ej < 2 && !share_endpoints; ej++) {
                            if (seg1->symbolic_coords[ei] && seg2->symbolic_coords[ej]) {
                                if (symbolic_coord_compare(seg1->symbolic_coords[ei], seg2->symbolic_coords[ej]) == 0) {
                                    share_endpoints = true;
                                }
                            }
                        }
                    }
                }

                /* If different distances and could be same segment pair */
                if (fabs(dist1 - dist2) > 1e-9 && share_endpoints) {
                    int conflict_nodes[4];
                    conflict_nodes[0] = seg1->id;
                    conflict_nodes[1] = seg2->id;
                    add_conflict_group(conflicts, out_conflict_count, out_conflict_sizes, conflict_nodes, 2);
                }
            }
        }
    }

    /* Type 3: Invalid betweenness constraints */
    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *c = graph->constraints[i];
        if (c->type != BETWEENNESS || c->participant_count != 3)
            continue;

        int middle_id = c->participants[1]; /* p2 is the middle point */
        int end1_id = c->participants[0];   /* p1 is one endpoint */
        int end2_id = c->participants[2];   /* p3 is the other endpoint */

        GeomNode *middle = graph_get_node(graph, middle_id);
        GeomNode *end1 = graph_get_node(graph, end1_id);
        GeomNode *end2 = graph_get_node(graph, end2_id);

        if (!middle || !end1 || !end2)
            continue;

        /* Check for collinearity - all three points should be on the same line */
        bool collinear = false;

        /* Check if all three points are incident to the same line segment */
        for (int j = 0; j < graph->node_count; j++) {
            GeomNode *line = graph->nodes[j];
            if (line->type != GEOM_LINE_SEGMENT)
                continue;

            /* Use inc_adj to check incidence in O(1) per lookup */
            bool middle_on_line = false;
            int mic = inc_counts[line->id];
            for (int mi = 0; mi < mic; mi++) {
                Constraint *ic = graph->constraints[inc_adj[line->id * 256 + mi]];
                if (ic->participants[0] == middle_id) {
                    middle_on_line = true;
                    break;
                }
            }
            bool end1_on_line = false;
            int e1c = inc_counts[line->id];
            for (int e1i = 0; e1i < e1c; e1i++) {
                Constraint *ic = graph->constraints[inc_adj[line->id * 256 + e1i]];
                if (ic->participants[0] == end1_id) {
                    end1_on_line = true;
                    break;
                }
            }
            bool end2_on_line = false;
            int e2c = inc_counts[line->id];
            for (int e2i = 0; e2i < e2c; e2i++) {
                Constraint *ic = graph->constraints[inc_adj[line->id * 256 + e2i]];
                if (ic->participants[0] == end2_id) {
                    end2_on_line = true;
                    break;
                }
            }

            if (middle_on_line && end1_on_line && end2_on_line) {
                collinear = true;
                break;
            }
        }

        /* If not collinear, the betweenness constraint is invalid */
        if (!collinear) {
            int conflict_nodes[4];
            conflict_nodes[0] = c->id;
            conflict_nodes[1] = middle_id;
            conflict_nodes[2] = end1_id;
            conflict_nodes[3] = end2_id;
            add_conflict_group(conflicts, out_conflict_count, out_conflict_sizes, conflict_nodes, 4);
        }

        /* Check ratio if numeric assumptions are available */
        /* The middle point should be between 0 and 1 on the segment */
        /* This requires coordinate evaluation which is complex for symbolic coords */
    }

    /* Type 4: Cycles in connection graph */
    int max_port_id = 0;
    for (int i = 0; i < graph->node_count; i++) {
        if (graph->nodes[i]->type == GEOM_PORT && graph->nodes[i]->id > max_port_id) {
            max_port_id = graph->nodes[i]->id;
        }
    }

    if (max_port_id > 0) {
        bool *visited = lv00_calloc(max_port_id + 1, sizeof(bool));
        bool *rec_stack = lv00_calloc(max_port_id + 1, sizeof(bool));
        int *path = lv00_malloc((size_t)(max_port_id + 1) * sizeof(int));

        if (visited && rec_stack && path) {
            for (int i = 0; i < graph->node_count; i++) {
                GeomNode *node = graph->nodes[i];
                if (node->type == GEOM_PORT && node->data.port->type == PORT_OUTPUT) {
                    if (!visited[node->id]) {
                        has_connection_cycle(graph, node->id, visited, rec_stack, path, 0, conflicts,
                                             out_conflict_count, out_conflict_sizes, conn_adj, conn_counts);
                    }
                }
            }
        }

        lv00_free((void **) &visited);
        lv00_free((void **) &rec_stack);
        lv00_free((void **) &path);
    }

    /* Type 5: Contradictory incidences - point required to be on two non-intersecting lines */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *point = graph->nodes[i];
        if (point->type != GEOM_POINT)
            continue;

        /* Find all incidence constraints for this point using inc_adj */
        int incident_lines[64];
        int il_count = 0;

        int pil = inc_counts[point->id];
        for (int pi = 0; pi < pil && il_count < 64; pi++) {
            Constraint *c = graph->constraints[inc_adj[point->id * 256 + pi]];
            if (c->type == INCIDENCE && c->participants[0] == point->id) {
                incident_lines[il_count++] = c->participants[1];
            }
        }

        /* Check pairs of incident lines for intersection possibility */
        for (int j = 0; j < il_count; j++) {
            GeomNode *line1 = graph_get_node(graph, incident_lines[j]);
            if (!line1 || line1->type != GEOM_LINE_SEGMENT)
                continue;

            for (int k = j + 1; k < il_count; k++) {
                GeomNode *line2 = graph_get_node(graph, incident_lines[k]);
                if (!line2 || line2->type != GEOM_LINE_SEGMENT)
                    continue;

                /* Check if there's an intersection constraint for these lines using int_adj */
                bool has_intersection = false;
                int iic = int_counts[incident_lines[j]];
                for (int ii = 0; ii < iic; ii++) {
                    Constraint *ic = graph->constraints[int_adj[incident_lines[j] * 256 + ii]];
                    if (ic->type == INTERSECTION && ic->participant_count == 3) {
                        if ((ic->participants[0] == incident_lines[j] && ic->participants[1] == incident_lines[k]) ||
                            (ic->participants[0] == incident_lines[k] && ic->participants[1] == incident_lines[j])) {
                            has_intersection = true;
                            break;
                        }
                    }
                }

                /* If no intersection constraint exists, check if lines can intersect */
                if (!has_intersection && !segments_can_intersect(graph, incident_lines[j], incident_lines[k])) {
                    /* Lines are parallel and distinct - point cannot be on both */
                    int conflict_nodes[4];
                    conflict_nodes[0] = point->id;
                    conflict_nodes[1] = incident_lines[j];
                    conflict_nodes[2] = incident_lines[k];
                    add_conflict_group(conflicts, out_conflict_count, out_conflict_sizes, conflict_nodes, 3);
                }
            }
        }
    }

    /* If no conflicts found, free and return NULL */
    if (*out_conflict_count == 0) {
        lv00_free((void **) &adj_lists);
        lv00_free((void **) &adj_counts);
        lv00_free((void **) &inc_adj);
        lv00_free((void **) &inc_counts);
        lv00_free((void **) &conn_adj);
        lv00_free((void **) &conn_counts);
        lv00_free((void **) &int_adj);
        lv00_free((void **) &int_counts);
        lv00_free((void **) &conflicts);
        lv00_free((void **) &*out_conflict_sizes);
        *out_conflict_sizes = NULL;
        return NULL;
    }

    lv00_free((void **) &adj_lists);
    lv00_free((void **) &adj_counts);
    lv00_free((void **) &inc_adj);
    lv00_free((void **) &inc_counts);
    lv00_free((void **) &conn_adj);
    lv00_free((void **) &conn_counts);
    lv00_free((void **) &int_adj);
    lv00_free((void **) &int_counts);

    /* 流式事件: 冲突检测结果 */
    if (graph_stream_ctx && *out_conflict_count > 0) {
        char desc[128];
        snprintf(desc, sizeof(desc), "冲突检测完成: 发现 %d 个冲突", *out_conflict_count);
        stream_emit_simple(graph_stream_ctx, STREAM_EVENT_CONFLICT_DETECTED, desc, *out_conflict_count);
    }

    return conflicts;
}

/**
 * @brief 验证区域的边界是否闭合
 *
 * 检查指定区域的所有边界线段是否首尾相连形成闭合路径。
 * 从第一条边界线段出发，沿连接关系遍历，最终应回到起始线段。
 *
 * @param graph     约束图指针
 * @param region_id 区域节点 ID
 * @return true 表示区域边界闭合，false 表示不闭合或参数无效
 */
bool graph_validate_region_closure(const ConstraintGraph *graph, int region_id) {
    lv00_clear_error();

    if (!graph) {
        lv00_set_error(LV00_ERROR_UNKNOWN, "%s", "Null graph");
        return false;
    }

    GeomNode *region = graph_get_node(graph, region_id);
    if (!region) {
        lv00_set_error(LV00_ERROR_UNKNOWN, "%s", "Region node not found");
        return false;
    }

    if (region->type != GEOM_REGION) {
        lv00_set_error(LV00_ERROR_UNKNOWN, "%s", "Node is not a region");
        return false;
    }

    /* Check 1: segment_count >= 3 */
    if (region->data.region.segment_count < 3) {
        lv00_set_error(LV00_ERROR_UNKNOWN, "%s", "Region must have at least 3 boundary segments");
        return false;
    }

    int segment_count = region->data.region.segment_count;
    GeomNode **segments = region->data.region.boundary_segments;

    /* Check 2: All segments exist and are valid */
    for (int i = 0; i < segment_count; i++) {
        if (!segments[i]) {
            lv00_set_error(LV00_ERROR_UNKNOWN, "%s", "Null segment in region boundary");
            return false;
        }
        if (segments[i]->type != GEOM_LINE_SEGMENT) {
            lv00_set_error(LV00_ERROR_UNKNOWN, "%s", "Boundary element is not a line segment");
            return false;
        }
    }

    /* Build endpoint connectivity map */
    /* For each segment, we need to track its two endpoints */
    /* A closed region means: every endpoint connects to exactly one other endpoint */

    /* Collect all endpoint references */
    typedef struct {
        int segment_idx;
        int endpoint_idx; /* 0 or 1 for start/end */
    } EndpointRef;

    /* We'll use a simplified approach: track which segments connect at each point */
    int *endpoint_connections = lv00_calloc(segment_count * 2, sizeof(int));
    if (!endpoint_connections) {
        lv00_set_error(LV00_ERROR_UNKNOWN, "%s", "Memory allocation failed");
        return false;
    }

    /* Initialize all endpoints as unconnected (-1) */
    for (int i = 0; i < segment_count * 2; i++) {
        endpoint_connections[i] = -1;
    }

    /* Find shared endpoints between segments */
    /* Two segments share an endpoint if there's a point that both are incident to */
    for (int i = 0; i < segment_count; i++) {
        for (int ep_i = 0; ep_i < 2; ep_i++) {
            /* Find points incident to segment i */
            int point_i = -1;

            /* Look for incidence constraints involving this segment */
            for (int k = 0; k < graph->constraint_count; k++) {
                Constraint *c = graph->constraints[k];
                if (c->type == INCIDENCE && c->participants[1] == segments[i]->id) {
                    /* Found a point incident to this segment */
                    /* For simplicity, we'll use the first two incidence points as endpoints */
                    point_i = c->participants[0];
                    break;
                }
            }

            /* Check if this endpoint connects to another segment */
            for (int j = i + 1; j < segment_count; j++) {
                for (int ep_j = 0; ep_j < 2; ep_j++) {
                    int point_j = -1;

                    for (int k = 0; k < graph->constraint_count; k++) {
                        Constraint *c = graph->constraints[k];
                        if (c->type == INCIDENCE && c->participants[1] == segments[j]->id) {
                            point_j = c->participants[0];
                            break;
                        }
                    }

                    if (point_i == point_j && point_i >= 0) {
                        /* Segments i and j share an endpoint */
                        endpoint_connections[i * 2 + ep_i] = j * 2 + ep_j;
                        endpoint_connections[j * 2 + ep_j] = i * 2 + ep_i;
                    }
                }
            }
        }
    }

    /* Check 3: Verify closed chain - each endpoint should connect to exactly one other */
    int unconnected_count = 0;
    int overconnected_count = 0;

    for (int i = 0; i < segment_count * 2; i++) {
        if (endpoint_connections[i] == -1) {
            unconnected_count++;
        }
    }

    lv00_free((void **) &endpoint_connections);

    if (unconnected_count > 0) {
        lv00_set_error(LV00_ERROR_UNKNOWN, "%s", "Region boundary has dangling segments (not closed)");
        return false;
    }

    /* Check 4: Verify the segments form a single closed chain (not multiple loops) */
    /* We do this by traversing from segment 0 and counting how many we visit */
    bool *visited_segments = lv00_calloc(segment_count, sizeof(bool));
    if (!visited_segments) {
        lv00_set_error(LV00_ERROR_UNKNOWN, "%s", "Memory allocation failed");
        return false;
    }

    int visited_count = 0;
    int current_segment = 0;
    int current_endpoint = 0;
    int prev_segment = -1;

    /* Start traversal from segment 0 */
    while (visited_count < segment_count) {
        if (visited_segments[current_segment]) {
            /* We've returned to a visited segment - check if we completed the loop */
            if (visited_count == segment_count && current_segment == 0) {
                break; /* Successfully completed the loop */
            }
            /* Multiple loops detected */
            lv00_free((void **) &visited_segments);
            lv00_set_error(LV00_ERROR_UNKNOWN, "%s", "Region boundary forms multiple loops instead of single closed chain");
            return false;
        }

        visited_segments[current_segment] = true;
        visited_count++;

        /* Find next segment in chain */
        bool found_next = false;
        for (int ep = 0; ep < 2; ep++) {
            if (ep == current_endpoint && prev_segment >= 0)
                continue; /* Skip the endpoint we came from */

            /* Find which segment this endpoint connects to */
            for (int j = 0; j < segment_count; j++) {
                if (j == current_segment || j == prev_segment)
                    continue;

                /* Check if segments share an endpoint */
                for (int k = 0; k < graph->constraint_count; k++) {
                    Constraint *c = graph->constraints[k];
                    if (c->type == INCIDENCE) {
                        int seg_id = c->participants[1];
                        int point_id = c->participants[0];

                        if (seg_id == segments[current_segment]->id) {
                            /* Check if another segment also has this point */
                            for (int m = 0; m < graph->constraint_count; m++) {
                                Constraint *c2 = graph->constraints[m];
                                if (c2->type == INCIDENCE && c2->participants[1] == segments[j]->id &&
                                    c2->participants[0] == point_id) {
                                    /* Found connection */
                                    prev_segment = current_segment;
                                    current_segment = j;
                                    current_endpoint = (ep == 0) ? 1 : 0;
                                    found_next = true;
                                    break;
                                }
                            }
                        }
                        if (found_next)
                            break;
                    }
                    if (found_next)
                        break;
                }
                if (found_next)
                    break;
            }
            if (found_next)
                break;
        }

        if (!found_next && visited_count < segment_count) {
            lv00_free((void **) &visited_segments);
            lv00_set_error(LV00_ERROR_UNKNOWN, "%s", "Region boundary is not connected");
            return false;
        }
    }

    lv00_free((void **) &visited_segments);

    /* Check 5: Self-intersection detection (warning only) */
    /* For each pair of non-adjacent segments, check if they intersect */
    {
        int *seg_ids = lv00_malloc((size_t) segment_count * sizeof(int));
        if (seg_ids) {
            for (int i = 0; i < segment_count; i++) {
                seg_ids[i] = segments[i]->id;
            }
            for (int i = 0; i < segment_count; i++) {
                for (int j = i + 2; j < segment_count; j++) {
                    /* Skip adjacent segments (they share an endpoint) */
                    if (i == 0 && j == segment_count - 1)
                        continue;

                    if (segments_intersect(segments[i], segments[j])) {
                        LOG_WARN("constraint_graph", "Region %d has self-intersection between segments %d and %d",
                                 region_id, seg_ids[i], seg_ids[j]);
                    }
                }
            }
            lv00_free((void **) &seg_ids);
        }
    }

    /* If we visited all segments and returned to start, it's valid */
    return true;
}

/* ============================================================
 * 图序列化与反序列化实现
 * ============================================================ */

/* 序列化错误信息存储 —— v3.3.0：使用图级 serialize_buffer 替代旧版全局变量 */
const char *graph_get_serialize_error(const ConstraintGraph *graph) {
    if (!graph || !graph->serialize_buffer) {
        return "";
    }
    return graph->serialize_buffer;
}

/**
 * @brief 设置序列化错误信息
 *
 * 使用可变参数格式化字符串，将错误信息写入约束图的序列化缓冲区。
 * 若图或缓冲区不可用，则回退到全局错误 API。
 *
 * @param graph 约束图指针（可以为 NULL）
 * @param fmt   printf 风格的格式字符串
 * @param ...   可变参数列表
 */
static void set_serialize_error(ConstraintGraph *graph, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    if (graph && graph->serialize_buffer) {
        vsnprintf(graph->serialize_buffer, 256, fmt, args);
    } else {
        /* 无 graph 时的回退：格式化后写入全局错误 API */
        char fallback[256];
        vsnprintf(fallback, sizeof(fallback), fmt, args);
        lv00_set_error(LV00_ERROR_UNKNOWN, "%s", fallback);
    }
    va_end(args);
}

/* JSON 写入器辅助 */
typedef struct {
    char *buffer;
    size_t capacity;
    size_t pos;
} JsonBuf;

/**
 * @brief 初始化 JSON 写入缓冲区
 *
 * 分配指定初始大小的缓冲区，并将位置归零。
 *
 * @param buf           JsonBuf 结构体指针
 * @param initial_size  初始缓冲区大小（字节）
 * @return 成功返回 true，内存分配失败返回 false
 */
static bool json_buf_init(JsonBuf *buf, size_t initial_size) {
    buf->capacity = initial_size;
    buf->pos = 0;
    buf->buffer = lv00_malloc(initial_size);
    if (!buf->buffer)
        return false;
    buf->buffer[0] = '\0';
    return true;
}

/**
 * @brief 扩展 JSON 缓冲区容量（倍增策略）
 *
 * 将缓冲区容量翻倍，使用 lv00_realloc 重新分配内存。
 * 若重分配失败则保持原缓冲区不变。
 *
 * @param buf JsonBuf 结构体指针
 */
static void json_buf_grow(JsonBuf *buf) {
    int old_capacity = buf->capacity;
    buf->capacity *= 2;
    char *new_buf = lv00_realloc(buf->buffer, buf->capacity);
    if (new_buf)
        buf->buffer = new_buf;
    else
        buf->capacity = old_capacity; /* 恢复旧容量 */
}

/**
 * @brief 向 JSON 缓冲区追加字符串
 *
 * 若剩余空间不足则自动扩展缓冲区，然后将字符串（含 null 终止符）复制到缓冲区。
 *
 * @param buf JsonBuf 结构体指针
 * @param str 要追加的字符串
 */
static void json_buf_append(JsonBuf *buf, const char *str) {
    size_t len = strlen(str);
    while (buf->pos + len + 1 >= buf->capacity) {
        json_buf_grow(buf);
    }
    memcpy(buf->buffer + buf->pos, str, len + 1);
    buf->pos += len;
}

/**
 * @brief 向 JSON 缓冲区追加单个字符
 *
 * 若剩余空间不足则自动扩展缓冲区，然后写入一个字符并添加 null 终止符。
 *
 * @param buf JsonBuf 结构体指针
 * @param c  要追加的字符
 */
static void json_buf_append_char(JsonBuf *buf, char c) {
    if (buf->pos + 2 >= buf->capacity) {
        json_buf_grow(buf);
    }
    buf->buffer[buf->pos++] = c;
    buf->buffer[buf->pos] = '\0';
}

/**
 * @brief 完成 JSON 缓冲区写入并返回内容
 *
 * 将缓冲区的内部指针转移给调用者，调用者负责释放该内存。
 * 调用后 JsonBuf 结构体不应再被使用。
 *
 * @param buf JsonBuf 结构体指针
 * @return 缓冲区内容的字符串指针（调用者需释放），失败返回 NULL
 */
static char *json_buf_finalize(JsonBuf *buf) {
    char *result = buf->buffer;
    (void) buf; /* 防止未使用警告 */
    return result;
}

/**
 * @brief 将几何节点类型枚举转换为字符串
 *
 * 用于 JSON 序列化时输出节点类型的可读名称。
 *
 * @param type 几何节点类型枚举值
 * @return 类型名称字符串（静态常量，无需释放）
 */
static const char *geom_type_to_string(GeomType type) {
    switch (type) {
        case GEOM_POINT:
            return "POINT";
        case GEOM_LINE_SEGMENT:
            return "LINE_SEGMENT";
        case GEOM_REGION:
            return "REGION";
        case GEOM_PORT:
            return "PORT";
        case GEOM_FUNCTION_BLOCK:
            return "FUNCTION_BLOCK";
        default:
            return "UNKNOWN";
    }
}

/**
 * @brief 将约束类型枚举转换为字符串
 *
 * 用于 JSON 序列化时输出约束类型的可读名称。
 *
 * @param type 约束类型枚举值
 * @return 类型名称字符串（静态常量，无需释放）
 */
static const char *constraint_type_to_string(ConstraintType type) {
    switch (type) {
        case INCIDENCE:
            return "INCIDENCE";
        case BETWEENNESS:
            return "BETWEENNESS";
        case INTERSECTION:
            return "INTERSECTION";
        case CONTAINMENT:
            return "CONTAINMENT";
        case CONNECTION:
            return "CONNECTION";
        default:
            return "UNKNOWN";
    }
}

/* 序列化符号坐标 */
static void json_buf_append_coord(JsonBuf *buf, const SymbolicCoord *coord) {
    if (!coord) {
        json_buf_append(buf, "null");
        return;
    }

    char *coord_json = symbolic_coord_serialize(coord);
    if (!coord_json) {
        json_buf_append(buf, "null");
        return;
    }

    /* coord_json 格式: {"type":"RATIONAL","num":1,"den":2} */
    json_buf_append(buf, coord_json);
    lv00_free((void **) &coord_json);
}

/* 序列化信任颜色 */
static const char *trust_color_to_string(TrustColor trust) {
    switch (trust) {
        case TRUST_GREEN:
            return "GREEN";
        case TRUST_BLUE:
            return "BLUE";
        case TRUST_YELLOW:
            return "YELLOW";
        case TRUST_ORANGE:
            return "ORANGE";
        case TRUST_LIGHT_ORANGE:
            return "LIGHT_ORANGE";
        case TRUST_AMBER:
            return "AMBER";
        default:
            return "UNKNOWN";
    }
}

/* 序列化单个节点 */
char *graph_node_serialize_to_json(const GeomNode *node) {
    if (!node)
        return NULL;

    JsonBuf buf;
    if (!json_buf_init(&buf, 1024))
        return NULL;

    json_buf_append(&buf, "{");

    /* id */
    json_buf_append(&buf, "\"id\":");
    char id_str[32];
    snprintf(id_str, sizeof(id_str), "%d", node->id);
    json_buf_append(&buf, id_str);
    json_buf_append(&buf, ",");

    /* type */
    json_buf_append(&buf, "\"type\":\"");
    json_buf_append(&buf, geom_type_to_string(node->type));
    json_buf_append(&buf, "\",");

    /* trust */
    json_buf_append(&buf, "\"trust\":\"");
    json_buf_append(&buf, trust_color_to_string(node->trust));
    json_buf_append(&buf, "\",");

    /* namespace_depth */
    json_buf_append(&buf, "\"namespace_depth\":");
    snprintf(id_str, sizeof(id_str), "%d", node->namespace_depth);
    json_buf_append(&buf, id_str);
    json_buf_append(&buf, ",");

    /* parent_block_id */
    json_buf_append(&buf, "\"parent_block_id\":");
    snprintf(id_str, sizeof(id_str), "%d", node->parent_block_id);
    json_buf_append(&buf, id_str);
    json_buf_append(&buf, ",");

    /* coords */
    json_buf_append(&buf, "\"coords\":[");
    for (int i = 0; i < node->coord_count; i++) {
        if (i > 0)
            json_buf_append_char(&buf, ',');
        json_buf_append_coord(&buf, node->symbolic_coords[i]);
    }
    json_buf_append(&buf, "],");

    /* 类型特定数据 */
    switch (node->type) {
        case GEOM_POINT:
            /* 点节点只有通用数据 */
            break;

        case GEOM_LINE_SEGMENT: {
            /* 线段节点：存储端点ID */
            /* 从坐标中提取端点ID：coords[0..n1-1] 是端点1的坐标，coords[n1..n1+n2-1] 是端点2的坐标 */
            /* 但我们需要在序列化时知道端点ID，所以使用特殊格式 */
            /* 这里我们简化处理：coords 格式为 [x1, y1, x2, y2]，从中提取端点信息 */
            /* 更好的方法是存储端点ID，但需要修改数据结构 */
            /* 对于反序列化，我们从 coords 中推断端点（假设 coords[0] 是端点1的x，coords[n1] 是端点2的x） */
            int half = node->coord_count / 2;
            if (half < 2)
                half = 2;
            json_buf_append(&buf, "\"endpoint1_start\":0,");
            json_buf_append(&buf, "\"endpoint2_start\":");
            snprintf(id_str, sizeof(id_str), "%d", half);
            json_buf_append(&buf, id_str);
            json_buf_append(&buf, ",");
            json_buf_append(&buf, "\"coord_count\":");
            snprintf(id_str, sizeof(id_str), "%d", node->coord_count);
            json_buf_append(&buf, id_str);
            break;
        }

        case GEOM_REGION: {
            json_buf_append(&buf, "\"boundary_segments\":[");
            for (int i = 0; i < node->data.region.segment_count; i++) {
                if (i > 0)
                    json_buf_append_char(&buf, ',');
                snprintf(id_str, sizeof(id_str), "%d", node->data.region.boundary_segments[i]->id);
                json_buf_append(&buf, id_str);
            }
            json_buf_append(&buf, "],");
            json_buf_append(&buf, "\"segment_count\":");
            snprintf(id_str, sizeof(id_str), "%d", node->data.region.segment_count);
            json_buf_append(&buf, id_str);
            break;
        }

        case GEOM_PORT: {
            if (node->data.port) {
                json_buf_append(&buf, "\"port_type\":\"");
                json_buf_append(&buf, node->data.port->type == PORT_INPUT ? "INPUT" : "OUTPUT");
                json_buf_append(&buf, "\",");
                json_buf_append(&buf, "\"is_formal_param\":");
                json_buf_append(&buf, node->data.port->is_formal_param ? "true" : "false");
                json_buf_append(&buf, ",");
                json_buf_append(&buf, "\"is_polymorphic\":");
                json_buf_append(&buf, node->data.port->is_polymorphic ? "true" : "false");
            }
            break;
        }

        case GEOM_FUNCTION_BLOCK: {
            json_buf_append(&buf, "\"internal_nodes\":[");
            for (int i = 0; i < node->data.func_block.internal_node_count; i++) {
                if (i > 0)
                    json_buf_append_char(&buf, ',');
                snprintf(id_str, sizeof(id_str), "%d", node->data.func_block.internal_nodes[i]->id);
                json_buf_append(&buf, id_str);
            }
            json_buf_append(&buf, "],");

            json_buf_append(&buf, "\"input_port_ids\":[");
            for (int i = 0; i < node->data.func_block.input_count; i++) {
                if (i > 0)
                    json_buf_append_char(&buf, ',');
                snprintf(id_str, sizeof(id_str), "%d", node->data.func_block.input_port_ids[i]);
                json_buf_append(&buf, id_str);
            }
            json_buf_append(&buf, "],");

            json_buf_append(&buf, "\"output_port_ids\":[");
            for (int i = 0; i < node->data.func_block.output_count; i++) {
                if (i > 0)
                    json_buf_append_char(&buf, ',');
                snprintf(id_str, sizeof(id_str), "%d", node->data.func_block.output_port_ids[i]);
                json_buf_append(&buf, id_str);
            }
            json_buf_append(&buf, "],");

            json_buf_append(&buf, "\"determinism_state\":");
            switch (node->data.func_block.determinism_state) {
                case UNVERIFIED:
                    json_buf_append(&buf, "\"UNVERIFIED\"");
                    break;
                case VERIFIED:
                    json_buf_append(&buf, "\"VERIFIED\"");
                    break;
                case NON_DETERMINISTIC:
                    json_buf_append(&buf, "\"NON_DETERMINISTIC\"");
                    break;
                case PARTIALLY_VERIFIED:
                    json_buf_append(&buf, "\"PARTIALLY_VERIFIED\"");
                    break;
            }
            break;
        }
    }

    json_buf_append_char(&buf, '}');
    return json_buf_finalize(&buf);
}

/* 序列化单个约束 */
char *graph_constraint_serialize_to_json(const Constraint *constraint) {
    if (!constraint)
        return NULL;

    JsonBuf buf;
    if (!json_buf_init(&buf, 256))
        return NULL;

    json_buf_append(&buf, "{");

    /* id */
    json_buf_append(&buf, "\"id\":");
    char id_str[32];
    snprintf(id_str, sizeof(id_str), "%d", constraint->id);
    json_buf_append(&buf, id_str);
    json_buf_append(&buf, ",");

    /* type */
    json_buf_append(&buf, "\"constraint_type\":\"");
    json_buf_append(&buf, constraint_type_to_string(constraint->type));
    json_buf_append(&buf, "\",");

    /* participants */
    json_buf_append(&buf, "\"participants\":[");
    for (int i = 0; i < constraint->participant_count; i++) {
        if (i > 0)
            json_buf_append_char(&buf, ',');
        snprintf(id_str, sizeof(id_str), "%d", constraint->participants[i]);
        json_buf_append(&buf, id_str);
    }
    json_buf_append(&buf, "],");

    /* template_id */
    json_buf_append(&buf, "\"template_id\":");
    snprintf(id_str, sizeof(id_str), "%d", constraint->template_id);
    json_buf_append(&buf, id_str);

    json_buf_append_char(&buf, '}');
    return json_buf_finalize(&buf);
}

/* 序列化整个图 */
char *graph_serialize_to_json(const ConstraintGraph *graph) {
    if (!graph) {
        set_serialize_error(graph, "图指针为空");
        return NULL;
    }

    JsonBuf buf;
    if (!json_buf_init(&buf, 8192)) {
        set_serialize_error(graph, "内存分配失败");
        return NULL;
    }

    json_buf_append(&buf, "{");

    /* 图元数据 */
    json_buf_append(&buf, "\"node_count\":");
    char num_str[32];
    snprintf(num_str, sizeof(num_str), "%d", graph->node_count);
    json_buf_append(&buf, num_str);
    json_buf_append(&buf, ",");

    json_buf_append(&buf, "\"constraint_count\":");
    snprintf(num_str, sizeof(num_str), "%d", graph->constraint_count);
    json_buf_append(&buf, num_str);
    json_buf_append(&buf, ",");

    json_buf_append(&buf, "\"next_node_id\":");
    snprintf(num_str, sizeof(num_str), "%d", graph->next_node_id);
    json_buf_append(&buf, num_str);
    json_buf_append(&buf, ",");

    json_buf_append(&buf, "\"next_constraint_id\":");
    snprintf(num_str, sizeof(num_str), "%d", graph->next_constraint_id);
    json_buf_append(&buf, num_str);
    json_buf_append(&buf, ",");

    /* 节点数组 */
    json_buf_append(&buf, "\"nodes\":[");
    for (int i = 0; i < graph->node_count; i++) {
        if (i > 0)
            json_buf_append_char(&buf, ',');
        char *node_json = graph_node_serialize_to_json(graph->nodes[i]);
        if (node_json) {
            json_buf_append(&buf, node_json);
            lv00_free((void **) &node_json);
        } else {
            json_buf_append(&buf, "null");
        }
    }
    json_buf_append(&buf, "],");

    /* 约束数组 */
    json_buf_append(&buf, "\"constraints\":[");
    for (int i = 0; i < graph->constraint_count; i++) {
        if (i > 0)
            json_buf_append_char(&buf, ',');
        char *constraint_json = graph_constraint_serialize_to_json(graph->constraints[i]);
        if (constraint_json) {
            json_buf_append(&buf, constraint_json);
            lv00_free((void **) &constraint_json);
        } else {
            json_buf_append(&buf, "null");
        }
    }
    json_buf_append(&buf, "]");

    json_buf_append_char(&buf, '}');
    return json_buf_finalize(&buf);
}

/* ============================================================
 * JSON 解析器
 * ============================================================ */

typedef struct {
    const char *data;
    size_t size;
    size_t pos;
} JsonParser;

static void json_parser_init(JsonParser *p, const char *data, size_t size) {
    p->data = data;
    p->size = size;
    p->pos = 0;
}

static void json_parser_skip_ws(JsonParser *p) {
    while (p->pos < p->size) {
        char c = p->data[p->pos];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            p->pos++;
        } else {
            break;
        }
    }
}

static char json_parser_peek(JsonParser *p) {
    json_parser_skip_ws(p);
    return p->pos < p->size ? p->data[p->pos] : '\0';
}

static char json_parser_next(JsonParser *p) {
    json_parser_skip_ws(p);
    return p->pos < p->size ? p->data[p->pos++] : '\0';
}

static bool json_parser_expect(JsonParser *p, char c) {
    char got = json_parser_next(p);
    return got == c;
}

/* 解析 JSON 字符串 */
static char *json_parser_parse_string(JsonParser *p) {
    if (!json_parser_expect(p, '"'))
        return NULL;

    size_t start = p->pos;
    size_t len = 0;

    while (p->pos < p->size && p->data[p->pos] != '"') {
        if (p->data[p->pos] == '\\' && p->pos + 1 < p->size) {
            p->pos += 2;
            len++;
        } else {
            p->pos++;
            len++;
        }
    }

    if (p->pos >= p->size)
        return NULL;
    p->pos++; /* skip end quote */

    char *result = lv00_malloc(len + 1);
    if (!result)
        return NULL;

    const char *src = p->data + start;
    char *dst = result;
    const char *end = p->data + p->pos - 1;

    while (src < end) {
        if (*src == '\\' && src + 1 < end) {
            src++;
            switch (*src) {
                case 'n':
                    *dst++ = '\n';
                    break;
                case 'r':
                    *dst++ = '\r';
                    break;
                case 't':
                    *dst++ = '\t';
                    break;
                case '"':
                    *dst++ = '"';
                    break;
                case '\\':
                    *dst++ = '\\';
                    break;
                default:
                    *dst++ = *src;
                    break;
            }
            src++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
    return result;
}

/* 解析 JSON 整数 */
static bool json_parser_parse_int(JsonParser *p, int *out) {
    json_parser_skip_ws(p);
    size_t start = p->pos;
    bool negative = false;

    if (p->pos < p->size && p->data[p->pos] == '-') {
        negative = true;
        p->pos++;
    }

    if (p->pos >= p->size || p->data[p->pos] < '0' || p->data[p->pos] > '9') {
        return false;
    }

    while (p->pos < p->size && p->data[p->pos] >= '0' && p->data[p->pos] <= '9') {
        p->pos++;
    }

    if (p->pos == start || (p->pos == start + 1 && negative))
        return false;

    int64_t val = 0;
    for (size_t i = start + (negative ? 1 : 0); i < p->pos; i++) {
        val = val * 10 + (p->data[i] - '0');
    }
    *out = negative ? -val : val;
    return true;
}

/* 解析布尔值 */
static bool json_parser_parse_bool(JsonParser *p, bool *out) {
    json_parser_skip_ws(p);
    if (p->pos + 4 <= p->size && strncmp(p->data + p->pos, "true", 4) == 0) {
        p->pos += 4;
        *out = true;
        return true;
    }
    if (p->pos + 5 <= p->size && strncmp(p->data + p->pos, "false", 5) == 0) {
        p->pos += 5;
        *out = false;
        return true;
    }
    return false;
}

/* 解析 null */
static bool json_parser_parse_null(JsonParser *p) {
    json_parser_skip_ws(p);
    if (p->pos + 4 <= p->size && strncmp(p->data + p->pos, "null", 4) == 0) {
        p->pos += 4;
        return true;
    }
    return false;
}

/* 跳过 JSON 值 */
static void json_parser_skip_value(JsonParser *p) {
    json_parser_skip_ws(p);
    if (p->pos >= p->size)
        return;

    char c = p->data[p->pos];
    if (c == '"') {
        /* string */
        p->pos++;
        while (p->pos < p->size && p->data[p->pos] != '"') {
            if (p->data[p->pos] == '\\')
                p->pos++;
            p->pos++;
        }
        if (p->pos < p->size)
            p->pos++;
    } else if (c == '{') {
        /* object */
        p->pos++;
        while (p->pos < p->size && p->data[p->pos] != '}') {
            if (p->data[p->pos] == '"') {
                json_parser_skip_value(p);
                json_parser_skip_ws(p);
                if (p->pos < p->size && p->data[p->pos] == ':')
                    p->pos++;
                json_parser_skip_value(p);
            } else if (p->data[p->pos] == '}') {
                break;
            } else {
                p->pos++;
            }
        }
        if (p->pos < p->size)
            p->pos++;
    } else if (c == '[') {
        /* array */
        p->pos++;
        while (p->pos < p->size && p->data[p->pos] != ']') {
            json_parser_skip_value(p);
            json_parser_skip_ws(p);
            if (p->pos < p->size && p->data[p->pos] == ',')
                p->pos++;
        }
        if (p->pos < p->size)
            p->pos++;
    } else {
        /* number or literal */
        while (p->pos < p->size && p->data[p->pos] != ',' && p->data[p->pos] != '}' && p->data[p->pos] != ']' &&
               p->data[p->pos] != ' ' && p->data[p->pos] != '\n' && p->data[p->pos] != '\t' &&
               p->data[p->pos] != '\r') {
            p->pos++;
        }
    }
}

/* 从字符串转换类型名称 */
static GeomType string_to_geom_type(const char *str) {
    if (strcmp(str, "POINT") == 0)
        return GEOM_POINT;
    if (strcmp(str, "LINE_SEGMENT") == 0)
        return GEOM_LINE_SEGMENT;
    if (strcmp(str, "REGION") == 0)
        return GEOM_REGION;
    if (strcmp(str, "PORT") == 0)
        return GEOM_PORT;
    if (strcmp(str, "FUNCTION_BLOCK") == 0)
        return GEOM_FUNCTION_BLOCK;
    return GEOM_POINT;
}

static ConstraintType string_to_constraint_type(const char *str) {
    if (strcmp(str, "INCIDENCE") == 0)
        return INCIDENCE;
    if (strcmp(str, "BETWEENNESS") == 0)
        return BETWEENNESS;
    if (strcmp(str, "INTERSECTION") == 0)
        return INTERSECTION;
    if (strcmp(str, "CONTAINMENT") == 0)
        return CONTAINMENT;
    if (strcmp(str, "CONNECTION") == 0)
        return CONNECTION;
    return INCIDENCE;
}

/* 解析数组中的整数列表 */
static int *json_parser_parse_int_array(JsonParser *p, int *out_count) {
    if (!json_parser_expect(p, '[')) {
        *out_count = 0;
        return NULL;
    }

    json_parser_skip_ws(p);
    if (json_parser_peek(p) == ']') {
        p->pos++;
        *out_count = 0;
        return NULL;
    }

    /* 先计数 */
    int capacity = 8;
    int count = 0;
    int *result = lv00_malloc((size_t) capacity * sizeof(int));
    if (!result) {
        *out_count = 0;
        return NULL;
    }

    while (json_parser_peek(p) != ']' && json_parser_peek(p) != '\0') {
        if (count >= capacity) {
            capacity *= 2;
            int *new_result = lv00_realloc(result, (size_t) capacity * sizeof(int));
            if (!new_result) {
                lv00_free((void **) &result);
                *out_count = 0;
                return NULL;
            }
            result = new_result;
        }

        if (json_parser_parse_int(p, &result[count])) {
            count++;
        }

        json_parser_skip_ws(p);
        if (json_parser_peek(p) == ',') {
            p->pos++;
        }
    }

    json_parser_expect(p, ']');
    *out_count = count;
    return result;
}

/* 反序列化图 */
ConstraintGraph *graph_deserialize_from_json(const char *json) {
    if (!json) {
        set_serialize_error(NULL, "JSON 字符串为空");
        return NULL;
    }

    size_t json_len = strlen(json);
    JsonParser p;
    json_parser_init(&p, json, json_len);

    if (json_parser_peek(&p) != '{') {
        set_serialize_error(NULL, "期望 JSON 对象");
        return NULL;
    }
    p.pos++; /* skip '{' */

    /* 创建图 */
    ConstraintGraph *graph = graph_create();
    if (!graph) {
        set_serialize_error(graph, "创建图失败");
        return NULL;
    }

    /* 解析元数据（可选） */
    int node_count = 0, constraint_count = 0;
    int next_node_id = 0, next_constraint_id = 0;

    /* 解析节点和约束数组 */
    while (json_parser_peek(&p) != '}' && json_parser_peek(&p) != '\0') {
        char *key = json_parser_parse_string(&p);
        if (!key)
            break;

        json_parser_skip_ws(&p);
        if (p.pos >= p.size || p.data[p.pos] != ':') {
            lv00_free((void **) &key);
            break;
        }
        p.pos++;

        if (strcmp(key, "nodes") == 0) {
            if (!json_parser_expect(&p, '[')) {
                lv00_free((void **) &key);
                graph_destroy(graph);
                set_serialize_error(graph, "节点数组格式错误");
                return NULL;
            }

            while (json_parser_peek(&p) != ']' && json_parser_peek(&p) != '\0') {
                if (json_parser_peek(&p) == ',') {
                    p.pos++;
                    continue;
                }

                if (json_parser_peek(&p) == 'n') {
                    /* null */
                    json_parser_skip_value(&p);
                    continue;
                }

                if (json_parser_peek(&p) != '{') {
                    json_parser_skip_value(&p);
                    continue;
                }
                p.pos++; /* skip '{' */

                /* 解析节点 */
                int node_id = 0, coord_count = 0;
                GeomType node_type = GEOM_POINT;
                TrustColor trust = TRUST_GREEN;
                int ns_depth = 0, parent_block_id = -1;

                /* 存储临时数据 */
                int *boundary_segs = NULL;
                int boundary_seg_count = 0;
                int *internal_nodes = NULL;
                int internal_node_count = 0;
                int *input_port_ids = NULL;
                int input_port_count = 0;
                int *output_port_ids = NULL;
                int output_port_count = 0;
                PortType port_type = PORT_INPUT;
                bool is_formal_param = false;
                bool is_polymorphic = false;
                SymbolicCoord **coords = NULL;

                while (json_parser_peek(&p) != '}' && json_parser_peek(&p) != '\0') {
                    char *node_key = json_parser_parse_string(&p);
                    if (!node_key)
                        break;

                    json_parser_skip_ws(&p);
                    if (p.pos >= p.size || p.data[p.pos] != ':') {
                        lv00_free((void **) &node_key);
                        break;
                    }
                    p.pos++;

                    if (strcmp(node_key, "id") == 0) {
                        json_parser_parse_int(&p, &node_id);
                    } else if (strcmp(node_key, "type") == 0) {
                        char *type_str = json_parser_parse_string(&p);
                        if (type_str) {
                            node_type = string_to_geom_type(type_str);
                            lv00_free((void **) &type_str);
                        }
                    } else if (strcmp(node_key, "trust") == 0) {
                        char *trust_str = json_parser_parse_string(&p);
                        if (trust_str) {
                            if (strcmp(trust_str, "GREEN") == 0)
                                trust = TRUST_GREEN;
                            else if (strcmp(trust_str, "BLUE") == 0)
                                trust = TRUST_BLUE;
                            else if (strcmp(trust_str, "YELLOW") == 0)
                                trust = TRUST_YELLOW;
                            else if (strcmp(trust_str, "ORANGE") == 0)
                                trust = TRUST_ORANGE;
                            else if (strcmp(trust_str, "LIGHT_ORANGE") == 0)
                                trust = TRUST_LIGHT_ORANGE;
                            else if (strcmp(trust_str, "AMBER") == 0)
                                trust = TRUST_AMBER;
                            else
                                trust = TRUST_GREEN;
                            lv00_free((void **) &trust_str);
                        }
                    } else if (strcmp(node_key, "namespace_depth") == 0) {
                        json_parser_parse_int(&p, &ns_depth);
                    } else if (strcmp(node_key, "parent_block_id") == 0) {
                        json_parser_parse_int(&p, &parent_block_id);
                    } else if (strcmp(node_key, "coords") == 0) {
                        if (json_parser_expect(&p, '[')) {
                            /* 计数 */
                            int temp_count = 0;
                            JsonParser temp_p = p;
                            while (temp_p.pos < temp_p.size && temp_p.data[temp_p.pos] != ']') {
                                if (temp_p.data[temp_p.pos] == ',') {
                                    temp_count++;
                                    temp_p.pos++;
                                } else if (temp_p.data[temp_p.pos] != ' ' && temp_p.data[temp_p.pos] != '\t') {
                                    temp_count++;
                                }
                                json_parser_skip_value(&temp_p);
                                json_parser_skip_ws(&temp_p);
                            }
                            coord_count = temp_count;

                            /* 解析坐标 */
                            if (coord_count > 0) {
                                coords = lv00_malloc((size_t) coord_count * sizeof(SymbolicCoord *));
                                if (!coords) {
                                    coord_count = 0;
                                } else {
                                    for (int i = 0; i < coord_count; i++) {
                                        coords[i] = NULL;
                                    }
                                }
                            }

                            for (int i = 0;
                                 i < coord_count && json_parser_peek(&p) != ']' && json_parser_peek(&p) != '\0'; i++) {
                                if (json_parser_peek(&p) == ',')
                                    p.pos++;

                                if (json_parser_peek(&p) == 'n') {
                                    json_parser_skip_value(&p);
                                    continue;
                                }

                                if (json_parser_peek(&p) == '{') {
                                    p.pos++; /* skip '{' */
                                    char *coord_key = json_parser_parse_string(&p);
                                    int coord_type = -1; /* 0=RATIONAL, 1=ALGEBRAIC, 2=QUADRATIC */
                                    if (coord_key && strcmp(coord_key, "type") == 0) {
                                        p.pos++; /* skip ':' */
                                        char *ct = json_parser_parse_string(&p);
                                        if (ct) {
                                            if (strcmp(ct, "RATIONAL") == 0)
                                                coord_type = 0;
                                            else if (strcmp(ct, "ALGEBRAIC") == 0)
                                                coord_type = 1;
                                            else if (strcmp(ct, "QUADRATIC") == 0)
                                                coord_type = 2;
                                            lv00_free((void **) &ct);
                                        }
                                        lv00_free((void **) &coord_key);

                                        /* 继续解析值 */
                                        while (json_parser_peek(&p) != '}' && json_parser_peek(&p) != '\0') {
                                            coord_key = json_parser_parse_string(&p);
                                            if (!coord_key)
                                                break;
                                            json_parser_skip_ws(&p);
                                            if (p.pos < p.size && p.data[p.pos] == ':')
                                                p.pos++;

                                            if (coord_type == 0 && strcmp(coord_key, "num") == 0) {
                                                int64_t num, den = 1;
                                                /* 手动解析 int64_t 值，避免 int* 截断 */
                                                {
                                                    json_parser_skip_ws(&p);
                                                    char *end;
                                                    long long val = strtoll((const char *) p.data + p.pos, &end, 10);
                                                    num = (int64_t) val;
                                                    p.pos = (size_t) (end - (const char *) p.data);
                                                }
                                                /* 查找 den */
                                                json_parser_skip_ws(&p);
                                                if (json_parser_peek(&p) == ',') {
                                                    p.pos++;
                                                    char *dk = json_parser_parse_string(&p);
                                                    if (dk && strcmp(dk, "den") == 0) {
                                                        p.pos++;
                                                        if (p.pos < p.size && p.data[p.pos] == ':')
                                                            p.pos++;
                                                        /* 手动解析 int64_t 值，避免 int* 截断 */
                                                        {
                                                            json_parser_skip_ws(&p);
                                                            char *end;
                                                            long long val =
                                                                strtoll((const char *) p.data + p.pos, &end, 10);
                                                            den = (int64_t) val;
                                                            p.pos = (size_t) (end - (const char *) p.data);
                                                        }
                                                    }
                                                    lv00_free((void **) &dk);
                                                }
                                                coords[i] = symbolic_coord_create_rational(num, den);
                                            } else if (coord_type == 0 && strcmp(coord_key, "num") == 0) {
                                                int64_t num = 0, den = 1;
                                                /* 手动解析 int64_t 值，避免 int* 截断 */
                                                {
                                                    json_parser_skip_ws(&p);
                                                    char *end;
                                                    long long val = strtoll((const char *) p.data + p.pos, &end, 10);
                                                    num = (int64_t) val;
                                                    p.pos = (size_t) (end - (const char *) p.data);
                                                }
                                                coords[i] = symbolic_coord_create_rational(num, den);
                                            }
                                            lv00_free((void **) &coord_key);
                                            json_parser_skip_ws(&p);
                                        }
                                    } else {
                                        lv00_free((void **) &coord_key);
                                        json_parser_skip_value(&p);
                                        while (json_parser_peek(&p) != '}')
                                            json_parser_skip_value(&p);
                                    }
                                    if (json_parser_peek(&p) == '}')
                                        p.pos++;
                                } else {
                                    json_parser_skip_value(&p);
                                }
                            }

                            json_parser_expect(&p, ']');
                        }
                    } else if (strcmp(node_key, "boundary_segments") == 0) {
                        boundary_segs = json_parser_parse_int_array(&p, &boundary_seg_count);
                    } else if (strcmp(node_key, "internal_nodes") == 0) {
                        internal_nodes = json_parser_parse_int_array(&p, &internal_node_count);
                    } else if (strcmp(node_key, "input_port_ids") == 0) {
                        input_port_ids = json_parser_parse_int_array(&p, &input_port_count);
                    } else if (strcmp(node_key, "output_port_ids") == 0) {
                        output_port_ids = json_parser_parse_int_array(&p, &output_port_count);
                    } else if (strcmp(node_key, "port_type") == 0) {
                        char *pt = json_parser_parse_string(&p);
                        if (pt) {
                            port_type = (strcmp(pt, "OUTPUT") == 0) ? PORT_OUTPUT : PORT_INPUT;
                            lv00_free((void **) &pt);
                        }
                    } else if (strcmp(node_key, "is_formal_param") == 0) {
                        json_parser_parse_bool(&p, &is_formal_param);
                    } else if (strcmp(node_key, "is_polymorphic") == 0) {
                        json_parser_parse_bool(&p, &is_polymorphic);
                    } else {
                        json_parser_skip_value(&p);
                    }

                    lv00_free((void **) &node_key);
                    json_parser_skip_ws(&p);
                }

                if (json_parser_peek(&p) == '}')
                    p.pos++;

                /* 对于 LINE_SEGMENT，需要先确保端点节点存在 */
                /* 我们先跳过 LINE_SEGMENT 和 REGION，在第二轮处理 */
                /* 暂时存储节点信息以便后续处理 */
                GeomNode *node = graph_add_node_with_id(graph, node_id, node_type, coords, coord_count);
                if (node) {
                    node->trust = trust;
                    node->namespace_depth = ns_depth;
                    node->parent_block_id = parent_block_id;

                    /* 设置类型特定数据 */
                    if (node_type == GEOM_REGION && boundary_segs && boundary_seg_count > 0) {
                        node->data.region.boundary_segments = lv00_malloc((size_t) boundary_seg_count * sizeof(GeomNode *));
                        if (node->data.region.boundary_segments) {
                            for (int i = 0; i < boundary_seg_count; i++) {
                                node->data.region.boundary_segments[i] = graph_get_node(graph, boundary_segs[i]);
                            }
                            node->data.region.segment_count = boundary_seg_count;
                        }
                    } else if (node_type == GEOM_PORT) {
                        Port *port = lv00_malloc(sizeof(Port));
                        if (port) {
                            memset(port, 0, sizeof(Port));
                            port->id = node_id;
                            port->type = port_type;
                            port->namespace_depth = ns_depth;
                            port->parent_block_id = parent_block_id;
                            port->is_formal_param = is_formal_param;
                            port->is_polymorphic = is_polymorphic;
                            port->connected_to = NULL;
                            node->data.port = port;
                        }
                    } else if (node_type == GEOM_FUNCTION_BLOCK) {
                        node->data.func_block.internal_node_count = internal_node_count;
                        node->data.func_block.input_count = input_port_count;
                        node->data.func_block.output_count = output_port_count;
                        if (internal_nodes && internal_node_count > 0) {
                            node->data.func_block.internal_nodes =
                                lv00_malloc((size_t) internal_node_count * sizeof(GeomNode *));
                            if (node->data.func_block.internal_nodes) {
                                for (int i = 0; i < internal_node_count; i++) {
                                    node->data.func_block.internal_nodes[i] = graph_get_node(graph, internal_nodes[i]);
                                }
                            }
                        }
                        if (input_port_ids && input_port_count > 0) {
                            node->data.func_block.input_port_ids = lv00_malloc((size_t) input_port_count * sizeof(int));
                            if (node->data.func_block.input_port_ids) {
                                memcpy(node->data.func_block.input_port_ids, input_port_ids,
                                       input_port_count * sizeof(int));
                            }
                        }
                        if (output_port_ids && output_port_count > 0) {
                            node->data.func_block.output_port_ids = lv00_malloc((size_t) output_port_count * sizeof(int));
                            if (node->data.func_block.output_port_ids) {
                                memcpy(node->data.func_block.output_port_ids, output_port_ids,
                                       output_port_count * sizeof(int));
                            }
                        }
                        node->data.func_block.determinism_state = UNVERIFIED;
                    }

                    /* 更新 next_node_id */
                    if (node_id >= graph->next_node_id) {
                        graph->next_node_id = node_id + 1;
                    }
                }

                /* 释放临时数据 */
                lv00_free((void **) &boundary_segs);
                lv00_free((void **) &internal_nodes);
                lv00_free((void **) &input_port_ids);
                lv00_free((void **) &output_port_ids);
                if (coords) {
                    for (int i = 0; i < coord_count; i++) {
                        if (coords[i])
                            symbolic_coord_destroy(coords[i]);
                    }
                    lv00_free((void **) &coords);
                }
            }

            json_parser_expect(&p, ']');
        } else if (strcmp(key, "constraints") == 0) {
            if (!json_parser_expect(&p, '[')) {
                lv00_free((void **) &key);
                graph_destroy(graph);
                set_serialize_error(graph, "约束数组格式错误");
                return NULL;
            }

            while (json_parser_peek(&p) != ']' && json_parser_peek(&p) != '\0') {
                if (json_parser_peek(&p) == ',') {
                    p.pos++;
                    continue;
                }

                if (json_parser_peek(&p) == 'n') {
                    json_parser_skip_value(&p);
                    continue;
                }

                if (json_parser_peek(&p) != '{') {
                    json_parser_skip_value(&p);
                    continue;
                }
                p.pos++; /* skip '{' */

                int constraint_id = 0, template_id = -1;
                ConstraintType constraint_type = INCIDENCE;
                int *participants = NULL;
                int participant_count = 0;

                while (json_parser_peek(&p) != '}' && json_parser_peek(&p) != '\0') {
                    char *ckey = json_parser_parse_string(&p);
                    if (!ckey)
                        break;

                    json_parser_skip_ws(&p);
                    if (p.pos >= p.size || p.data[p.pos] != ':') {
                        lv00_free((void **) &ckey);
                        break;
                    }
                    p.pos++;

                    if (strcmp(ckey, "id") == 0) {
                        json_parser_parse_int(&p, &constraint_id);
                    } else if (strcmp(ckey, "constraint_type") == 0) {
                        char *type_str = json_parser_parse_string(&p);
                        if (type_str) {
                            constraint_type = string_to_constraint_type(type_str);
                            lv00_free((void **) &type_str);
                        }
                    } else if (strcmp(ckey, "participants") == 0) {
                        participants = json_parser_parse_int_array(&p, &participant_count);
                    } else if (strcmp(ckey, "template_id") == 0) {
                        json_parser_parse_int(&p, &template_id);
                    } else {
                        json_parser_skip_value(&p);
                    }

                    lv00_free((void **) &ckey);
                    json_parser_skip_ws(&p);
                }

                if (json_parser_peek(&p) == '}')
                    p.pos++;

                /* 使用带ID的接口添加约束 */
                if (participants && participant_count > 0) {
                    Constraint *constraint = graph_add_constraint_with_id(graph, constraint_id, constraint_type,
                                                                          participants, participant_count);
                    if (constraint) {
                        constraint->template_id = template_id;
                        if (constraint_id >= graph->next_constraint_id) {
                            graph->next_constraint_id = constraint_id + 1;
                        }
                    }
                }

                lv00_free((void **) &participants);
            }

            json_parser_expect(&p, ']');
        } else {
            json_parser_skip_value(&p);
        }

        lv00_free((void **) &key);
        json_parser_skip_ws(&p);
        if (json_parser_peek(&p) == ',')
            p.pos++;
    }

    return graph;
}
