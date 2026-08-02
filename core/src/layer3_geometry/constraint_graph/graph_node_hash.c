/**
 * @file graph_node_hash.c
 * @brief ConstraintGraph 节点与约束生命周期管理 —— 节点/约束哈希索引
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
