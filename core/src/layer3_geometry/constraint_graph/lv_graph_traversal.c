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
#include "lv/lv_utils.h"

#include "lv_internal.h"

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
 * 内部辅助函数
 * ============================================================ */

/**
 * @brief 获取节点 ID 的最大值（用于 visited 数组大小）
 */
static int get_max_node_id(const ConstraintGraph *graph) {
    if (!graph)
        return 0;
    /* next_node_id 是即将分配的下一个 ID，所以最大有效 ID 为 next_node_id - 1 */
    int max_id = graph->node_count;
    /* 也要考虑 next_node_id 的值 */
    if (graph->next_node_id > max_id)
        max_id = (int)graph->next_node_id;
    return max_id < 0 ? 0 : max_id;
}

/**
 * @brief 检查节点是否应被跳过（根据 config 和 is_active 状态）
 */
static bool should_skip_node(ConstraintGraph *graph, int node_id,
                              const lvGraphTraversalConfig *config) {
    GeomNode *node = graph_get_node(graph, node_id);
    if (!node)
        return true;
    if (config->skip_disabled && !node->is_active)
        return true;
    return false;
}

/**
 * @brief 查找节点的邻居（通过约束连接）
 *
 * 遍历所有涉及该节点的约束，收集其他参与者作为邻居。
 * 返回邻居数量，邻居 ID 写入 out_neighbors 数组。
 */
static int find_neighbors(ConstraintGraph *graph, int node_id,
                           int *out_neighbors, int max_neighbors,
                           const lvGraphTraversalConfig *config) {
    if (!graph || !out_neighbors || max_neighbors <= 0)
        return 0;

    /* 先找到涉及该节点的所有约束索引 */
    int constraint_indices[256];
    int con_count = graph_find_constraints_involving(graph, node_id,
                                                      constraint_indices, 256);

    int count = 0;
    for (int i = 0; i < con_count && count < max_neighbors; i++) {
        Constraint *c = graph->constraints[constraint_indices[i]];
        if (!c->is_active)
            continue;

        int start = 0;
        int end = c->participant_count;
        int step = 1;

        if (config->reverse_edges) {
            start = c->participant_count - 1;
            end = -1;
            step = -1;
        }

        for (int j = start; j != end; j += step) {
            int pid = c->participants[j];
            if (pid == node_id)
                continue;
            if (config->skip_disabled) {
                GeomNode *pn = graph_get_node(graph, pid);
                if (!pn || !pn->is_active)
                    continue;
            }
            /* 去重 */
            bool already = false;
            for (int k = 0; k < count; k++) {
                if (out_neighbors[k] == pid) {
                    already = true;
                    break;
                }
            }
            if (!already) {
                out_neighbors[count++] = pid;
                if (count >= max_neighbors)
                    return count;
            }
        }
    }
    return count;
}

/**
 * @brief 初始化 BFS 队列
 */
static BFSQueue *bfs_queue_create(int capacity) {
    BFSQueue *q = (BFSQueue *)lv_malloc(sizeof(BFSQueue));
    if (!q)
        return NULL;
    q->ids = (int *)lv_malloc((size_t)capacity * sizeof(int));
    q->depths = (int *)lv_malloc((size_t)capacity * sizeof(int));
    if (!q->ids || !q->depths) {
        lv_free((void **)&q->ids);
        lv_free((void **)&q->depths);
        lv_free((void **)&q);
        return NULL;
    }
    q->head = 0;
    q->tail = 0;
    q->capacity = capacity;
    return q;
}

static void bfs_queue_destroy(BFSQueue *q) {
    if (!q)
        return;
    lv_free((void **)&q->ids);
    lv_free((void **)&q->depths);
    lv_free((void **)&q);
}

static bool bfs_queue_push(BFSQueue *q, int id, int depth) {
    if (!q || q->tail >= q->capacity)
        return false;
    q->ids[q->tail] = id;
    q->depths[q->tail] = depth;
    q->tail++;
    return true;
}

static bool bfs_queue_pop(BFSQueue *q, int *out_id, int *out_depth) {
    if (!q || q->head >= q->tail)
        return false;
    if (out_id)
        *out_id = q->ids[q->head];
    if (out_depth)
        *out_depth = q->depths[q->head];
    q->head++;
    return true;
}

static bool bfs_queue_is_empty(const BFSQueue *q) {
    return !q || q->head >= q->tail;
}

/* ============================================================
 * 内部遍历实现
 * ============================================================ */

/**
 * @brief 从指定节点执行 DFS 遍历（非递归，栈实现）
 *
 * 支持前序和后序，使用 visited 集合避免重复访问。
 */
static int dfs_traverse_from(ConstraintGraph *graph, int start_id,
                              bool *visited, int visited_size,
                              lvGraphNodeVisitor visitor, void *user_data,
                              const lvGraphTraversalConfig *config,
                              int base_depth) {
    if (start_id < 0 || start_id >= visited_size)
        return lv_OK;
    if (visited[start_id])
        return lv_OK;
    if (should_skip_node(graph, start_id, config))
        return lv_OK;

    /* 栈 —— 使用动态数组 */
    int stack_cap = 64;
    DFSFrame *stack = (DFSFrame *)lv_malloc((size_t)stack_cap * sizeof(DFSFrame));
    if (!stack)
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "dfs_traverse_from: stack alloc failed");

    int stack_top = 0;
    stack[stack_top].node_id = start_id;
    stack[stack_top].depth = base_depth;
    stack[stack_top].is_exit = false;
    stack_top++;

    int result = lv_OK;

    while (stack_top > 0) {
        stack_top--;
        DFSFrame frame = stack[stack_top];

        if (frame.is_exit) {
            /* 后序阶段：访问节点 */
            if (config->order == lv_TRAVERSAL_DFS_POST) {
                GeomNode *node = graph_get_node(graph, frame.node_id);
                if (node) {
                    lvTraversalResult tr = visitor(node, frame.depth, user_data);
                    if (tr == lv_TRAVERSAL_STOP) {
                        result = lv_OK;
                        goto cleanup;
                    }
                }
            }
            continue;
        }

        if (visited[frame.node_id])
            continue;
        visited[frame.node_id] = true;

        /* 深度检查 */
        if (config->max_depth > 0 && frame.depth >= config->max_depth) {
            continue;
        }

        /* 前序：访问节点 */
        if (config->order != lv_TRAVERSAL_DFS_POST) {
            GeomNode *node = graph_get_node(graph, frame.node_id);
            if (node) {
                lvTraversalResult tr = visitor(node, frame.depth, user_data);
                if (tr == lv_TRAVERSAL_STOP) {
                    result = lv_OK;
                    goto cleanup;
                }
                if (tr == lv_TRAVERSAL_SKIP_CHILDREN)
                    continue;
            }
        }

        /* 对于后序：先压入退出帧，再压入子节点 */
        if (config->order == lv_TRAVERSAL_DFS_POST) {
            /* 扩展栈 */
            if (stack_top >= stack_cap) {
                if (!lv_ensure_capacity((void **)&stack, stack_top, &stack_cap, sizeof(DFSFrame), 0)) {
                    lv_free((void **)&stack);
                    lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "dfs_traverse_from: stack realloc failed");
                }
            }
            /* 压入退出帧 */
            stack[stack_top].node_id = frame.node_id;
            stack[stack_top].depth = frame.depth;
            stack[stack_top].is_exit = true;
            stack_top++;
        }

        /* 查找邻居并压入栈 */
        int neighbors[256];
        int ncount = find_neighbors(graph, frame.node_id, neighbors, 256, config);

        for (int i = ncount - 1; i >= 0; i--) {
            int nid = neighbors[i];
            if (nid < 0 || nid >= visited_size || visited[nid])
                continue;
            if (should_skip_node(graph, nid, config))
                continue;

            /* 扩展栈 */
            if (stack_top >= stack_cap) {
                if (!lv_ensure_capacity((void **)&stack, stack_top, &stack_cap, sizeof(DFSFrame), 0)) {
                    lv_free((void **)&stack);
                    lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "dfs_traverse_from: stack realloc failed");
                }
            }

            stack[stack_top].node_id = nid;
            stack[stack_top].depth = frame.depth + 1;
            stack[stack_top].is_exit = false;
            stack_top++;
        }
    }

cleanup:
    lv_free((void **)&stack);
    return result;
}

/**
 * @brief 从指定节点执行 BFS 遍历（队列实现）
 */
static int bfs_traverse_from(ConstraintGraph *graph, int start_id,
                              bool *visited, int visited_size,
                              lvGraphNodeVisitor visitor, void *user_data,
                              const lvGraphTraversalConfig *config,
                              int base_depth) {
    if (start_id < 0 || start_id >= visited_size)
        return lv_OK;
    if (visited[start_id])
        return lv_OK;
    if (should_skip_node(graph, start_id, config))
        return lv_OK;

    BFSQueue *q = bfs_queue_create(graph->node_count + 64);
    if (!q)
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "bfs_traverse_from: queue alloc failed");

    visited[start_id] = true;
    bfs_queue_push(q, start_id, base_depth);

    int result = lv_OK;

    while (!bfs_queue_is_empty(q)) {
        int node_id;
        int depth;
        bfs_queue_pop(q, &node_id, &depth);

        /* 深度检查 */
        if (config->max_depth > 0 && depth >= config->max_depth)
            continue;

        /* 访问节点 */
        GeomNode *node = graph_get_node(graph, node_id);
        if (node) {
            lvTraversalResult tr = visitor(node, depth, user_data);
            if (tr == lv_TRAVERSAL_STOP) {
                result = lv_OK;
                goto bfs_cleanup;
            }
            if (tr == lv_TRAVERSAL_SKIP_CHILDREN)
                continue;
        }

        /* 入队邻居 */
        int neighbors[256];
        int ncount = find_neighbors(graph, node_id, neighbors, 256, config);

        for (int i = 0; i < ncount; i++) {
            int nid = neighbors[i];
            if (nid < 0 || nid >= visited_size || visited[nid])
                continue;
            if (should_skip_node(graph, nid, config))
                continue;
            visited[nid] = true;
            bfs_queue_push(q, nid, depth + 1);
        }
    }

bfs_cleanup:
    bfs_queue_destroy(q);
    return result;
}

/**
 * @brief 遍历所有连通分量（visit_all = true 时使用）
 */
static int traverse_all_components(ConstraintGraph *graph,
                                    bool *visited, int visited_size,
                                    lvGraphNodeVisitor visitor, void *user_data,
                                    const lvGraphTraversalConfig *config) {
    int result = lv_OK;

    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node)
            continue;
        int nid = node->id;
        if (nid < 0 || nid >= visited_size || visited[nid])
            continue;
        if (config->skip_disabled && !node->is_active)
            continue;

        if (config->order == lv_TRAVERSAL_BFS) {
            result = bfs_traverse_from(graph, nid, visited, visited_size,
                                        visitor, user_data, config, 0);
        } else {
            result = dfs_traverse_from(graph, nid, visited, visited_size,
                                        visitor, user_data, config, 0);
        }
        if (result != lv_OK)
            return result;
    }
    return lv_OK;
}

/**
 * @brief 按拓扑序（或逆拓扑序）回调节点（拓扑遍历核心循环）
 *
 * 第一段按 lv_graph_topological_sort 输出的节点顺序（逆拓扑则反序）回调；
 * 第二段按数组序补访拓扑序未覆盖的节点 —— 拓扑排序仅覆盖活跃节点，
 * 当 skip_disabled=false 时 disabled 节点不会出现在拓扑序中，需补访以保证
 * 全图节点均被回调。拓扑遍历没有分层的概念，depth 统一为 0。
 * visitor 返回 lv_TRAVERSAL_STOP 时立即终止整个遍历。
 *
 * @param reachable 若非 NULL，仅回调该数组中标记为 true 的节点
 *                  （lv_graph_traverse_from 的起点可达性过滤）
 */
static int topological_traverse(ConstraintGraph *graph, bool *visited, int visited_size,
                                const int *order, int count, bool reverse,
                                const bool *reachable,
                                lvGraphNodeVisitor visitor, void *user_data,
                                const lvGraphTraversalConfig *config) {
    /* 第一段：按拓扑序（或逆拓扑序）回调 */
    for (int i = 0; i < count; i++) {
        int idx = reverse ? (count - 1 - i) : i;
        int nid = order[idx];
        if (nid < 0 || nid >= visited_size || visited[nid])
            continue;
        if (reachable && !reachable[nid])
            continue;
        if (should_skip_node(graph, nid, config))
            continue;
        visited[nid] = true;
        GeomNode *node = graph_get_node(graph, nid);
        if (!node)
            continue;
        lvTraversalResult tr = visitor(node, 0, user_data);
        if (tr == lv_TRAVERSAL_STOP)
            return lv_OK;
    }

    /* 第二段：补访拓扑序未覆盖的节点（skip_disabled=false 时的 disabled 节点等） */
    for (int j = 0; j < graph->node_count; j++) {
        GeomNode *node = graph->nodes[j];
        if (!node)
            continue;
        int nid = node->id;
        if (nid < 0 || nid >= visited_size || visited[nid])
            continue;
        if (reachable && !reachable[nid])
            continue;
        if (should_skip_node(graph, nid, config))
            continue;
        visited[nid] = true;
        lvTraversalResult tr = visitor(node, 0, user_data);
        if (tr == lv_TRAVERSAL_STOP)
            break;
    }
    return lv_OK;
}

/**
 * @brief 全图拓扑（或逆拓扑）遍历：lv_graph_traverse 的拓扑分支
 *
 * 拓扑语义说明：该图是约束超图 —— 每个约束连接其全部参与者（超边），
 * 拓扑排序基于约束参与者之间的"星型近似"依赖（参与者索引高的节点依赖
 * 索引低的节点，见 lv_graph_topological_sort），并非有向边意义的偏序。
 * 因此此处按 Kahn 算法输出的全图节点顺序依次回调，depth 统一为 0。
 * 若图含环，lv_graph_topological_sort 返回 lv_ERROR_INVALID_STATE，本函数
 * 原样向上传递该错误（拓扑序在含环图上无定义，不再降级为 DFS 前序）。
 */
static int topological_order_traverse(ConstraintGraph *graph, bool *visited, int visited_size,
                                      lvGraphNodeVisitor visitor, void *user_data,
                                      const lvGraphTraversalConfig *config) {
    int *order = NULL;
    int count = 0;
    int result = lv_graph_topological_sort(graph, &order, &count);
    if (result != lv_OK)
        return result;

    bool reverse = (config->order == lv_TRAVERSAL_REVERSE_TOPOLOGICAL);
    result = topological_traverse(graph, visited, visited_size, order, count, reverse,
                                  NULL, visitor, user_data, config);

    lv_free((void **)&order);
    return result;
}

/**
 * @brief 从 start_id 出发标记所有可达节点（无向超边邻居语义，与 BFS/DFS 一致）
 *
 * 用于 lv_graph_traverse_from 的拓扑分支：拓扑序是全局序，需要按"从起点可达"
 * 过滤，以保持与 BFS/DFS 相同的起点语义。
 * @return lv_OK 成功；内存不足返回错误码（此时 reachable 状态不可靠，调用方应中止）
 */
static int mark_reachable_from(ConstraintGraph *graph, int start_id,
                               bool *reachable, int reachable_size,
                               const lvGraphTraversalConfig *config) {
    if (start_id < 0 || start_id >= reachable_size)
        return lv_OK;
    if (reachable[start_id])
        return lv_OK;
    if (should_skip_node(graph, start_id, config))
        return lv_OK;

    int stack_cap = 64;
    int *stack = (int *)lv_malloc((size_t)stack_cap * sizeof(int));
    if (!stack)
        return lv_ERROR_OUT_OF_MEMORY;

    int top = 0;
    stack[top++] = start_id;
    reachable[start_id] = true;

    int result = lv_OK;
    while (top > 0) {
        int nid = stack[--top];
        int neighbors[256];
        int ncount = find_neighbors(graph, nid, neighbors, 256, config);
        for (int i = 0; i < ncount; i++) {
            int nb = neighbors[i];
            if (nb < 0 || nb >= reachable_size || reachable[nb])
                continue;
            if (should_skip_node(graph, nb, config))
                continue;
            reachable[nb] = true;
            if (top >= stack_cap) {
                if (!lv_ensure_capacity((void **)&stack, top, &stack_cap, sizeof(int), 0)) {
                    result = lv_ERROR_OUT_OF_MEMORY;
                    goto cleanup;
                }
            }
            stack[top++] = nb;
        }
    }

cleanup:
    lv_free((void **)&stack);
    return result;
}

/**
 * @brief lv_graph_traverse_from 的拓扑分支：从指定节点按拓扑（或逆拓扑）序遍历
 *
 * visit_all=true 时输出全图拓扑序（与 lv_graph_traverse 的拓扑分支一致）；
 * 否则仅输出从 start_node_id 可达的节点（按全局拓扑序过滤后的顺序回调）。
 */
static int traverse_from_topological(ConstraintGraph *graph, int start_node_id,
                                     bool *visited, int visited_size,
                                     lvGraphNodeVisitor visitor, void *user_data,
                                     const lvGraphTraversalConfig *config) {
    int *order = NULL;
    int count = 0;
    int result = lv_graph_topological_sort(graph, &order, &count);
    if (result != lv_OK)
        return result;

    bool reverse = (config->order == lv_TRAVERSAL_REVERSE_TOPOLOGICAL);

    if (config->visit_all) {
        result = topological_traverse(graph, visited, visited_size, order, count, reverse,
                                      NULL, visitor, user_data, config);
    } else {
        bool *reachable = (bool *)lv_calloc((size_t)visited_size, sizeof(bool));
        if (!reachable) {
            lv_free((void **)&order);
            return lv_ERROR_OUT_OF_MEMORY;
        }
        result = mark_reachable_from(graph, start_node_id, reachable, visited_size, config);
        if (result == lv_OK) {
            result = topological_traverse(graph, visited, visited_size, order, count, reverse,
                                          reachable, visitor, user_data, config);
        }
        lv_free((void **)&reachable);
    }

    lv_free((void **)&order);
    return result;
}

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

    int neighbors[256];
    int ncount = find_neighbors(graph, node_id, neighbors, 256, config);

    for (int i = 0; i < ncount; i++) {
        GeomNode *n = graph_get_node(graph, neighbors[i]);
        if (!n)
            continue;
        lvTraversalResult tr = visitor(n, 1, user_data);
        if (tr == lv_TRAVERSAL_STOP)
            break;
    }

    return lv_OK;
}

/* ============================================================
 * 树遍历 API 实现
 * ============================================================ */

int lv_tree_traverse(void *root,
                      lvTreeNodeVisitor visitor,
                      void *user_data,
                      lvGetChildrenFunc get_children,
                      const lvTreeTraversalConfig *config) {
    if (!root || !visitor || !get_children)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "lv_tree_traverse: NULL param");

    lvTreeTraversalConfig default_config = lv_TREE_TRAVERSAL_DEFAULT_CONFIG;
    if (!config)
        config = &default_config;

    /* 使用栈/队列进行迭代遍历 */
    if (config->order == lv_TRAVERSAL_BFS) {
        /* BFS 队列 */
        int cap = 64;
        void **queue = (void **)lv_malloc((size_t)cap * sizeof(void *));
        int *depths = (int *)lv_malloc((size_t)cap * sizeof(int));
        if (!queue || !depths) {
            lv_free((void **)&queue);
            lv_free((void **)&depths);
            lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "lv_tree_traverse: queue alloc failed");
        }

        int head = 0, tail = 0;
        queue[tail] = root;
        depths[tail] = 0;
        tail++;

        while (head < tail) {
            void *node = queue[head];
            int depth = depths[head];
            head++;

            /* 深度检查 */
            if (config->max_depth > 0 && depth >= config->max_depth)
                continue;

            lvTraversalResult tr = visitor(node, depth, user_data);
            if (tr == lv_TRAVERSAL_STOP)
                break;
            if (tr == lv_TRAVERSAL_SKIP_CHILDREN)
                continue;

            void **children = NULL;
            int child_count = get_children(node, &children);

            for (int i = 0; i < child_count; i++) {
                if (tail >= cap) {
                    if (!lv_ensure_capacity((void **)&queue, tail, &cap, sizeof(void *), 0) ||
                        !lv_ensure_capacity((void **)&depths, tail, &cap, sizeof(int), 0)) {
                        lv_free((void **)&queue);
                        lv_free((void **)&depths);
                        lv_free((void **)&children);
                        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "lv_tree_traverse: queue realloc failed");
                    }
                }
                queue[tail] = children[i];
                depths[tail] = depth + 1;
                tail++;
            }

            lv_free((void **)&children);
        }

        lv_free((void **)&queue);
        lv_free((void **)&depths);
    } else {
        /* DFS 栈（前序/后序） */
        int cap = 64;
        typedef struct {
            void *node;
            int depth;
            bool is_exit;
        } TreeFrame;

        TreeFrame *stack = (TreeFrame *)lv_malloc((size_t)cap * sizeof(TreeFrame));
        if (!stack)
            lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "lv_tree_traverse: stack alloc failed");

        int top = 0;
        stack[top].node = root;
        stack[top].depth = 0;
        stack[top].is_exit = false;
        top++;

        while (top > 0) {
            top--;
            TreeFrame frame = stack[top];

            if (frame.is_exit) {
                if (config->order == lv_TRAVERSAL_DFS_POST) {
                    lvTraversalResult tr = visitor(frame.node, frame.depth, user_data);
                    if (tr == lv_TRAVERSAL_STOP)
                        break;
                }
                continue;
            }

            /* 深度检查 */
            if (config->max_depth > 0 && frame.depth >= config->max_depth)
                continue;

            /* 前序 */
            if (config->order != lv_TRAVERSAL_DFS_POST) {
                lvTraversalResult tr = visitor(frame.node, frame.depth, user_data);
                if (tr == lv_TRAVERSAL_STOP)
                    break;
                if (tr == lv_TRAVERSAL_SKIP_CHILDREN)
                    continue;
            }

            void **children = NULL;
            int child_count = get_children(frame.node, &children);

            if (config->order == lv_TRAVERSAL_DFS_POST && child_count > 0) {
                /* 后序：先压入退出帧，再压入子节点 */
                if (top >= cap) {
                    if (!lv_ensure_capacity((void **)&stack, top, &cap, sizeof(TreeFrame), 0)) {
                        lv_free((void **)&stack);
                        lv_free((void **)&children);
                        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "lv_tree_traverse: stack realloc failed");
                    }
                }
                stack[top].node = frame.node;
                stack[top].depth = frame.depth;
                stack[top].is_exit = true;
                top++;
            }

            /* 逆序压入子节点（保证从左到右遍历） */
            for (int i = child_count - 1; i >= 0; i--) {
                if (top >= cap) {
                    if (!lv_ensure_capacity((void **)&stack, top, &cap, sizeof(TreeFrame), 0)) {
                        lv_free((void **)&stack);
                        lv_free((void **)&children);
                        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "lv_tree_traverse: stack realloc failed");
                    }
                }
                stack[top].node = children[i];
                stack[top].depth = frame.depth + 1;
                stack[top].is_exit = false;
                top++;
            }

            lv_free((void **)&children);
        }

        lv_free((void **)&stack);
    }

    return lv_OK;
}

/* ============================================================
 * 便利函数实现
 * ============================================================ */

int lv_graph_count_nodes(ConstraintGraph *graph) {
    if (!graph)
        return 0;
    int count = 0;
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (node && node->is_active)
            count++;
    }
    return count;
}

/**
 * @brief 三色标记法环检测
 *
 * 使用 DFS 对图进行三色标记：
 *   - 0 (WHITE): 未访问
 *   - 1 (GRAY):  正在访问（在当前 DFS 路径上）
 *   - 2 (BLACK): 已访问完成
 */
bool lv_graph_has_cycle(ConstraintGraph *graph) {
    if (!graph || graph->node_count <= 0)
        return false;

    int visited_size = get_max_node_id(graph);
    if (visited_size <= 0)
        return false;

    char *color = (char *)lv_calloc((size_t)visited_size, sizeof(char));
    /* 0 = WHITE, 1 = GRAY, 2 = BLACK */
    if (!color)
        return false;

    /* 栈实现非递归三色 DFS */
    int stack_cap = 256;
    typedef struct {
        int node_id;
        int state; /* 0 = enter, 1 = exit */
    } CycleFrame;
    CycleFrame *stack = (CycleFrame *)lv_malloc((size_t)stack_cap * sizeof(CycleFrame));
    if (!stack) {
        lv_free((void **)&color);
        return false;
    }

    bool has_cycle = false;

    for (int i = 0; i < graph->node_count && !has_cycle; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node || !node->is_active)
            continue;
        int nid = node->id;
        if (nid < 0 || nid >= visited_size)
            continue;
        if (color[nid] != 0) /* 非 WHITE */
            continue;

        int top = 0;
        stack[top].node_id = nid;
        stack[top].state = 0;
        top++;

        while (top > 0 && !has_cycle) {
            top--;
            CycleFrame f = stack[top];

            if (f.state == 1) {
                /* 退出节点 */
                color[f.node_id] = 2; /* BLACK */
                continue;
            }

            if (color[f.node_id] == 1) {
                /* 发现 GRAY 节点 → 环 */
                has_cycle = true;
                break;
            }

            if (color[f.node_id] != 0)
                continue;

            /* 标记为 GRAY */
            color[f.node_id] = 1;

            /* 压入退出帧 */
            if (top >= stack_cap) {
                if (!lv_ensure_capacity((void **)&stack, top, &stack_cap, sizeof(CycleFrame), 0)) {
                    lv_free((void **)&stack);
                    lv_free((void **)&color);
                    return false;
                }
            }
            stack[top].node_id = f.node_id;
            stack[top].state = 1;
            top++;

            /* 压入邻居 */
            int neighbors[256];
            int ncount = find_neighbors(graph, f.node_id, neighbors, 256,
                &(lvGraphTraversalConfig){lv_TRAVERSAL_DFS_PRE, 0, false, false, true});

            for (int j = 0; j < ncount; j++) {
                int nb = neighbors[j];
                if (nb < 0 || nb >= visited_size)
                    continue;
                if (color[nb] == 2) /* BLACK: 已处理 */
                    continue;

                if (top >= stack_cap) {
                    if (!lv_ensure_capacity((void **)&stack, top, &stack_cap, sizeof(CycleFrame), 0)) {
                        lv_free((void **)&stack);
                        lv_free((void **)&color);
                        return false;
                    }
                }
                stack[top].node_id = nb;
                stack[top].state = 0;
                top++;
            }
        }
    }

    lv_free((void **)&stack);
    lv_free((void **)&color);
    return has_cycle;
}

int lv_graph_topological_sort(ConstraintGraph *graph, int **out_nodes, int *out_count) {
    if (!graph || !out_nodes || !out_count)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "lv_graph_topological_sort: NULL param");

    *out_nodes = NULL;
    *out_count = 0;

    if (graph->node_count <= 0)
        return lv_OK;

    /* 检查环 */
    if (lv_graph_has_cycle(graph))
        lv_RETURN_ERROR(lv_ERROR_INVALID_STATE, "lv_graph_topological_sort: graph contains a cycle");

    int visited_size = get_max_node_id(graph);
    if (visited_size <= 0)
        return lv_OK;

    /* 计算入度 */
    int *in_degree = (int *)lv_calloc((size_t)visited_size, sizeof(int));
    if (!in_degree)
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "lv_graph_topological_sort: in_degree alloc failed");

    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *c = graph->constraints[i];
        if (!c || !c->is_active)
            continue;
        /* 对于每个约束，参与者索引高的节点依赖于索引低的节点 */
        for (int j = 1; j < c->participant_count; j++) {
            int pid = c->participants[j];
            if (pid >= 0 && pid < visited_size)
                in_degree[pid]++;
        }
    }

    /* Kahn 算法 */
    int *queue = (int *)lv_malloc((size_t)visited_size * sizeof(int));
    if (!queue) {
        lv_free((void **)&in_degree);
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "lv_graph_topological_sort: queue alloc failed");
    }

    int qhead = 0, qtail = 0;

    /* 入度为 0 的节点入队 */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node || !node->is_active)
            continue;
        int nid = node->id;
        if (nid >= 0 && nid < visited_size && in_degree[nid] == 0) {
            queue[qtail++] = nid;
        }
    }

    int *result = (int *)lv_malloc((size_t)graph->node_count * sizeof(int));
    if (!result) {
        lv_free((void **)&in_degree);
        lv_free((void **)&queue);
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "lv_graph_topological_sort: result alloc failed");
    }

    int count = 0;

    while (qhead < qtail) {
        int nid = queue[qhead++];
        result[count++] = nid;

        /* 减少邻居的入度 */
        int neighbors[256];
        int ncount = find_neighbors(graph, nid, neighbors, 256,
            &(lvGraphTraversalConfig){lv_TRAVERSAL_DFS_PRE, 0, false, false, true});

        for (int i = 0; i < ncount; i++) {
            int nb = neighbors[i];
            if (nb < 0 || nb >= visited_size)
                continue;
            in_degree[nb]--;
            if (in_degree[nb] == 0) {
                queue[qtail++] = nb;
            }
        }
    }

    lv_free((void **)&in_degree);
    lv_free((void **)&queue);

    *out_nodes = result;
    *out_count = count;
    return lv_OK;
}

/* ============================================================
 * 字符串转换函数
 * ============================================================ */

/* ================================================================
 * 枚举 -> 名称 静态字符串表（数据表化，替代 switch）
 * ================================================================ */

/** @brief 遍历顺序 -> 名称表（lvTraversalOrder 枚举值 0~4 连续，按下标索引） */
static const char *const s_traversal_order_names[] = {
    [lv_TRAVERSAL_DFS_PRE] = "DFS_PRE",
    [lv_TRAVERSAL_DFS_POST] = "DFS_POST",
    [lv_TRAVERSAL_BFS] = "BFS",
    [lv_TRAVERSAL_TOPOLOGICAL] = "TOPOLOGICAL",
    [lv_TRAVERSAL_REVERSE_TOPOLOGICAL] = "REVERSE_TOPOLOGICAL",
};

const char *lv_traversal_order_to_string(lvTraversalOrder order) {
    /* 按下标索引；未知/越界顺序回退到 "UNKNOWN"（原 default 分支） */
    if ((unsigned) order < lv_ARRAY_SIZE(s_traversal_order_names) && s_traversal_order_names[order])
        return s_traversal_order_names[order];
    return "UNKNOWN";
}

/** @brief 遍历回调结果 -> 名称表（lvTraversalResult 枚举值 0~2 连续，按下标索引） */
static const char *const s_traversal_result_names[] = {
    [lv_TRAVERSAL_CONTINUE] = "CONTINUE",
    [lv_TRAVERSAL_SKIP_CHILDREN] = "SKIP_CHILDREN",
    [lv_TRAVERSAL_STOP] = "STOP",
};

const char *lv_traversal_result_to_string(lvTraversalResult result) {
    /* 按下标索引；未知/越界结果回退到 "UNKNOWN"（原 default 分支） */
    if ((unsigned) result < lv_ARRAY_SIZE(s_traversal_result_names) && s_traversal_result_names[result])
        return s_traversal_result_names[result];
    return "UNKNOWN";
}
