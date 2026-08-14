/**
 * @file lv_graph_traversal.c
 * @brief 图/树遍历抽象层实现
 *
 * 提供统一的 DFS/BFS/拓扑排序/树遍历实现，消除各模块重复的遍历逻辑。
 *
 * @author Lv-00 Project
 * @version 1.0.0
 */

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/constraint_graph.h"
#include "lv/lv_graph_traversal.h"
#include "lv/lv_lifecycle.h"
#include "lv/lv_utils.h"

#include "lv/lv_internal.h"
#include "graph_traversal_internal.h"

/* ============================================================
 * 内部辅助结构
 * ============================================================ */

/** @brief 栈帧：用于非递归 DFS */
typedef struct {
    int node_id;
    int depth;
    bool is_exit; /**< true = 后序退出阶段 */
} DFSFrame;

/** @brief 队列：用于 BFS */
typedef struct {
    int *ids;
    int *depths;
    int head;
    int tail;
    int capacity;
} BFSQueue;

/* ============================================================
 * 图遍历 API 实现
 * ============================================================ */

int lv_graph_traverse(ConstraintGraph *graph,
                       lvGraphNodeVisitor visitor,
                       void *user_data,
                       const lvGraphTraversalConfig *config) {
    if (!graph || !visitor)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "lv_graph_traverse: NULL param");

    lvGraphTraversalConfig default_config = lv_GRAPH_TRAVERSAL_DEFAULT_CONFIG;
    if (!config)
        config = &default_config;

    int visited_size = get_max_node_id(graph);
    if (visited_size <= 0)
        return lv_OK;

    bool *visited = (bool *)lv_calloc((size_t)visited_size, sizeof(bool));
    if (!visited)
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "lv_graph_traverse: visited alloc failed");

    int result = lv_OK;

    if (config->order == lv_TRAVERSAL_TOPOLOGICAL ||
        config->order == lv_TRAVERSAL_REVERSE_TOPOLOGICAL) {
        /* 拓扑序（或逆拓扑序）：全图序遍历。
         * 拓扑序是全局序，天然覆盖全部活跃节点，visit_all 不再有意义；
         * skip_disabled=false 时由补访逻辑兜底 disabled 节点。 */
        result = topological_order_traverse(graph, visited, visited_size,
                                            visitor, user_data, config);
    } else if (config->visit_all) {
        /* 遍历所有连通分量 */
        result = traverse_all_components(graph, visited, visited_size,
                                          visitor, user_data, config);
    } else {
        /* 从第一个活跃节点开始 */
        int start_id = -1;
        for (int i = 0; i < graph->node_count; i++) {
            GeomNode *node = graph->nodes[i];
            if (node && node->is_active) {
                start_id = node->id;
                break;
            }
        }
        if (start_id >= 0) {
            if (config->order == lv_TRAVERSAL_BFS) {
                result = bfs_traverse_from(graph, start_id, visited, visited_size,
                                            visitor, user_data, config, 0);
            } else {
                result = dfs_traverse_from(graph, start_id, visited, visited_size,
                                            visitor, user_data, config, 0);
            }
        }
    }

    lv_free((void **)&visited);
    return result;
}

int lv_graph_traverse_from(ConstraintGraph *graph, int start_node_id,
                            lvGraphNodeVisitor visitor,
                            void *user_data,
                            const lvGraphTraversalConfig *config) {
    if (!graph || !visitor)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "lv_graph_traverse_from: NULL param");

    /* 验证起始节点存在 */
    GeomNode *start = graph_get_node(graph, start_node_id);
    if (!start)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "lv_graph_traverse_from: start node %d not found", start_node_id);
    if (config && config->skip_disabled && !start->is_active)
        return lv_OK;

    lvGraphTraversalConfig default_config = lv_GRAPH_TRAVERSAL_DEFAULT_CONFIG;
    if (!config)
        config = &default_config;

    int visited_size = get_max_node_id(graph);
    if (visited_size <= 0)
        return lv_OK;

    bool *visited = (bool *)lv_calloc((size_t)visited_size, sizeof(bool));
    if (!visited)
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "lv_graph_traverse_from: visited alloc failed");

    int result;
    if (config->order == lv_TRAVERSAL_TOPOLOGICAL ||
        config->order == lv_TRAVERSAL_REVERSE_TOPOLOGICAL) {
        /* 拓扑分支：内部自行处理 visit_all 与起点可达性语义 */
        result = traverse_from_topological(graph, start_node_id, visited, visited_size,
                                           visitor, user_data, config);
    } else if (config->order == lv_TRAVERSAL_BFS) {
        result = bfs_traverse_from(graph, start_node_id, visited, visited_size,
                                    visitor, user_data, config, 0);
        /* 如果 visit_all，遍历其余未访问的节点 */
        if (result == lv_OK && config->visit_all) {
            result = traverse_all_components(graph, visited, visited_size,
                                              visitor, user_data, config);
        }
    } else {
        result = dfs_traverse_from(graph, start_node_id, visited, visited_size,
                                    visitor, user_data, config, 0);
        /* 如果 visit_all，遍历其余未访问的节点 */
        if (result == lv_OK && config->visit_all) {
            result = traverse_all_components(graph, visited, visited_size,
                                              visitor, user_data, config);
        }
    }

    lv_free((void **)&visited);
    return result;
}

int lv_graph_traverse_neighbors(ConstraintGraph *graph, int node_id,
                                 lvGraphNodeVisitor visitor,
                                 void *user_data,
                                 const lvGraphTraversalConfig *config) {
    if (!graph || !visitor)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "lv_graph_traverse_neighbors: NULL param");

    GeomNode *node = graph_get_node(graph, node_id);
    if (!node)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "lv_graph_traverse_neighbors: node %d not found", node_id);

    lvGraphTraversalConfig default_config = lv_GRAPH_TRAVERSAL_DEFAULT_CONFIG;
    if (!config)
        config = &default_config;

    lvDArray nbr;
    lv_darray_init(&nbr, sizeof(int));
    int ncount = find_neighbors(graph, node_id, &nbr, config);
    if (ncount < 0) {
        lv_darray_free(&nbr);
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "lv_graph_traverse_neighbors: find_neighbors OOM");
    }

    for (int i = 0; i < ncount; i++) {
        GeomNode *n = graph_get_node(graph, *(const int *)lv_darray_get(&nbr, i));
        if (!n)
            continue;
        lvTraversalResult tr = visitor(n, 1, user_data);
        if (tr == lv_TRAVERSAL_STOP)
            break;
    }
    lv_darray_free(&nbr);

    return lv_OK;
}

