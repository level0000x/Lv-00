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
 * 遍历所有涉及该节点的约束，收集其他参与者作为邻居，
 * 写入调用方提供的动态数组 out_neighbors（无固定容量上限）。
 *
 * @return 本次追加到 out_neighbors 的邻居数量；内存不足返回 -1
 */
static int find_neighbors(ConstraintGraph *graph, int node_id,
                           lvDArray *out_neighbors,
                           const lvGraphTraversalConfig *config) {
    if (!graph || !out_neighbors)
        return 0;

    /* 先收集涉及该节点的所有活跃约束索引（动态扩容，消除原 256 上限） */
    lvDArray con_indices;
    lv_darray_init(&con_indices, sizeof(int));
    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *c = graph->constraints[i];
        if (!c->is_active)
            continue;
        for (int j = 0; j < c->participant_count; j++) {
            if (c->participants[j] == node_id) {
                if (lv_darray_push(&con_indices, &i) < 0) {
                    lv_darray_free(&con_indices);
                    return -1;
                }
                break;
            }
        }
    }

    int added = 0;
    int base = out_neighbors->count;
    for (int i = 0; i < con_indices.count; i++) {
        Constraint *c = graph->constraints[*(const int *)lv_darray_get(&con_indices, i)];
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
            for (int k = base; k < out_neighbors->count; k++) {
                if (*(const int *)lv_darray_get(out_neighbors, k) == pid) {
                    already = true;
                    break;
                }
            }
            if (!already) {
                if (lv_darray_push(out_neighbors, &pid) < 0) {
                    lv_darray_free(&con_indices);
                    return -1;
                }
                added++;
            }
        }
    }
    lv_darray_free(&con_indices);
    return added;
}

/* ---- lv_DEFER 作用域守卫回调（本文件 goto cleanup 样板的统一替代） ---- */

/* BFS 队列部分构建守卫：guard 持有队列值拷贝，清理时按原失败路径语义
 * 释放 ids/depths/外壳（lv_free NULL 安全），避免字段级注册的 detach 失效问题 */
typedef struct {
    BFSQueue *q;
} BfsQueueGuard;

static void bfs_queue_guard_cleanup(void *p) {
    BfsQueueGuard *g = (BfsQueueGuard *) p;
    if (g->q) {
        lv_free((void **) &g->q->ids);
        lv_free((void **) &g->q->depths);
        lv_free((void **) &g->q);
    }
}

/**
 * @brief 初始化 BFS 队列
 */
static BFSQueue *bfs_queue_create(int capacity) {
    BFSQueue *q = (BFSQueue *)lv_malloc(sizeof(BFSQueue));
    if (!q)
        return NULL;
    /* 注册 lv_DEFER 守卫：ids/depths 任一失败自动释放已分配的成员与外壳；
     * 成功路径 guard.q = NULL 解除守卫 */
    BfsQueueGuard guard = {q};
    lv_DEFER(bfs_queue_guard_cleanup, &guard);
    q->ids = (int *)lv_malloc((size_t)capacity * sizeof(int));
    q->depths = (int *)lv_malloc((size_t)capacity * sizeof(int));
    if (!q->ids || !q->depths)
        return NULL;
    q->head = 0;
    q->tail = 0;
    q->capacity = capacity;
    guard.q = NULL; /* 守卫解除：结果移交调用方 */
    return q;
}

static void bfs_queue_destroy(BFSQueue *q) {
    if (!q)
        return;
    lv_free((void **)&q->ids);
    lv_free((void **)&q->depths);
    lv_free((void **)&q);
}

/* ---- lv_DEFER 作用域守卫回调（本文件 goto cleanup 样板的统一替代） ---- */

/* lvDArray 清理回调（栈上结构体，仅释放内部堆数据；配合 lv_DEFER(cleanup, &arr)） */
static void traversal_darray_cleanup(void *p) {
    lvDArray *d = (lvDArray *)p;
    lv_darray_free(d);
}

/* BFS 队列清理回调（变量置 NULL 即解除守卫；配合 lv_DEFER(cleanup, &q)） */
static void traversal_bfs_queue_cleanup(void *p) {
    BFSQueue **pp = (BFSQueue **)p;
    if (*pp)
        bfs_queue_destroy(*pp);
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
    lv_DEFER_FREE(stack);

    int stack_top = 0;
    stack[stack_top].node_id = start_id;
    stack[stack_top].depth = base_depth;
    stack[stack_top].is_exit = false;
    stack_top++;

    /* 邻居缓冲（lvDArray 动态收集，消除原 256 邻居截断上限） */
    lvDArray nbr;
    lv_darray_init(&nbr, sizeof(int));
    lv_DEFER(traversal_darray_cleanup, &nbr);

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
                    if (tr == lv_TRAVERSAL_STOP)
                        return result;
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
                if (tr == lv_TRAVERSAL_STOP)
                    return result;
                if (tr == lv_TRAVERSAL_SKIP_CHILDREN)
                    continue;
            }
        }

        /* 对于后序：先压入退出帧，再压入子节点 */
        if (config->order == lv_TRAVERSAL_DFS_POST) {
            /* 扩展栈 */
            if (stack_top >= stack_cap) {
                if (!lv_ensure_capacity((void **)&stack, stack_top, &stack_cap, sizeof(DFSFrame), 0))
                    lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "dfs_traverse_from: stack realloc failed");
            }
            /* 压入退出帧 */
            stack[stack_top].node_id = frame.node_id;
            stack[stack_top].depth = frame.depth;
            stack[stack_top].is_exit = true;
            stack_top++;
        }

        /* 查找邻居并压入栈 */
        lv_darray_clear(&nbr);
        int ncount = find_neighbors(graph, frame.node_id, &nbr, config);
        if (ncount < 0)
            return lv_ERROR_OUT_OF_MEMORY;

        for (int i = ncount - 1; i >= 0; i--) {
            int nid = *(const int *)lv_darray_get(&nbr, i);
            if (nid < 0 || nid >= visited_size || visited[nid])
                continue;
            if (should_skip_node(graph, nid, config))
                continue;

            /* 扩展栈 */
            if (stack_top >= stack_cap) {
                if (!lv_ensure_capacity((void **)&stack, stack_top, &stack_cap, sizeof(DFSFrame), 0))
                    lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "dfs_traverse_from: stack realloc failed");
            }

            stack[stack_top].node_id = nid;
            stack[stack_top].depth = frame.depth + 1;
            stack[stack_top].is_exit = false;
            stack_top++;
        }
    }

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
    lv_DEFER(traversal_bfs_queue_cleanup, &q);

    visited[start_id] = true;
    bfs_queue_push(q, start_id, base_depth);

    int result = lv_OK;

    /* 邻居缓冲（lvDArray 动态收集，消除原 256 邻居截断上限） */
    lvDArray nbr;
    lv_darray_init(&nbr, sizeof(int));
    lv_DEFER(traversal_darray_cleanup, &nbr);

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
            if (tr == lv_TRAVERSAL_STOP)
                return result;
            if (tr == lv_TRAVERSAL_SKIP_CHILDREN)
                continue;
        }

        /* 入队邻居 */
        lv_darray_clear(&nbr);
        int ncount = find_neighbors(graph, node_id, &nbr, config);
        if (ncount < 0)
            return lv_ERROR_OUT_OF_MEMORY;

        for (int i = 0; i < ncount; i++) {
            int nid = *(const int *)lv_darray_get(&nbr, i);
            if (nid < 0 || nid >= visited_size || visited[nid])
                continue;
            if (should_skip_node(graph, nid, config))
                continue;
            visited[nid] = true;
            bfs_queue_push(q, nid, depth + 1);
        }
    }

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
    lv_DEFER_FREE(stack);

    int top = 0;
    stack[top++] = start_id;
    reachable[start_id] = true;

    /* 邻居缓冲（lvDArray 动态收集，消除原 256 邻居截断上限） */
    lvDArray nbr;
    lv_darray_init(&nbr, sizeof(int));
    lv_DEFER(traversal_darray_cleanup, &nbr);

    int result = lv_OK;
    while (top > 0) {
        int nid = stack[--top];
        lv_darray_clear(&nbr);
        int ncount = find_neighbors(graph, nid, &nbr, config);
        if (ncount < 0)
            return lv_ERROR_OUT_OF_MEMORY;
        for (int i = 0; i < ncount; i++) {
            int nb = *(const int *)lv_darray_get(&nbr, i);
            if (nb < 0 || nb >= reachable_size || reachable[nb])
                continue;
            if (should_skip_node(graph, nb, config))
                continue;
            reachable[nb] = true;
            if (top >= stack_cap) {
                if (!lv_ensure_capacity((void **)&stack, top, &stack_cap, sizeof(int), 0))
                    return lv_ERROR_OUT_OF_MEMORY;
            }
            stack[top++] = nb;
        }
    }

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
        lv_DEFER_FREE(queue);
        lv_DEFER_FREE(depths);

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
        /* BFS 分支结束：lv_DEFER 守卫自动释放 queue/depths */
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
        lv_DEFER_FREE(stack);

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
        /* DFS 分支结束：lv_DEFER 守卫自动释放 stack */
    }

    return lv_OK;
}

/* ============================================================
 * 通用图算法核心（lv_bfs_run / lv_cycle_detect）
 * ============================================================ */

/**
 * @brief 收集节点 node_id 的第 batch_index 批邻居（对 lvGraphNeighborFunc
 * 自动扩容重试，直至返回 < 容量；返回 -1 表示该批次槽位无效）
 *
 * 回调返回 == 容量时视为"可能截断"，扩容后以同一 batch_index 重试，保证结果完整。
 * @param out_edges 是否同时收集边信息数组（环检测报告用）
 * @return 批次邻居数；0 = 无更多批次；-1 = 槽位无效（回调返回 -1）；-2 = 内存不足
 */
static int collect_neighbor_batch(lvGraphNeighborFunc fn, void *ctx, int node_id, int batch_index,
                                  int **out_ids, void ***out_edges, int *buf_cap,
                                  bool with_edges) {
    if (*buf_cap <= 0) {
        *buf_cap = 256;
        *out_ids = (int *)lv_malloc((size_t)*buf_cap * sizeof(int));
        if (!*out_ids)
            return -2;
        if (with_edges) {
            *out_edges = (void **)lv_malloc((size_t)*buf_cap * sizeof(void *));
            if (!*out_edges)
                return -2;
        }
    }
    for (;;) {
        int cnt = fn(ctx, node_id, batch_index, *out_ids,
                     with_edges ? *out_edges : NULL, *buf_cap);
        if (cnt < 0) /* 槽位无效（回调返回 -1） */
            return cnt;
        if (cnt < *buf_cap)
            return cnt;
        /* 可能截断：扩容重试 */
        int new_cap = *buf_cap * 2;
        if (new_cap <= *buf_cap) /* 溢出保护 */
            return -2;
        int *new_ids = (int *)lv_realloc(*out_ids, (size_t)new_cap * sizeof(int));
        if (!new_ids)
            return -2;
        *out_ids = new_ids;
        if (with_edges) {
            void **new_edges = (void **)lv_realloc(*out_edges, (size_t)new_cap * sizeof(void *));
            if (!new_edges)
                return -2;
            *out_edges = new_edges;
        }
        *buf_cap = new_cap;
    }
}

/**
 * @brief 通用 BFS 驱动：对任意整数 id 图（0..node_count-1）做广度优先遍历
 *
 * - seeds 起点直接入队（不查 visited；mark_on_enqueue 时标记 visited）；
 * - 出队顺序：范围检查 → visit 回调（STOP 终止 / SKIP_CHILDREN 跳过扩展）
 *   →（mark_on_enqueue=false 时）visited 检查与标记 → 出边扩展；
 * - mark_on_enqueue=true 时扩展入队前做范围 + visited 检查并标记（标准 BFS）；
 * - max_queue > 0 时队列 tail 达到上限即丢弃新元素（定长截断语义）。
 */
int lv_bfs_run(const lvBfsSpec *spec) {
    if (!spec || !spec->neighbors || spec->node_count <= 0)
        return -1;

    int n = spec->node_count;
    bool owned_visited = false;
    bool *visited = spec->visited;
    if (!visited) {
        visited = (bool *)lv_calloc((size_t)n, sizeof(bool));
        if (!visited)
            return -1;
        owned_visited = true;
    }

    /* 队列（非环形：head/tail 单调递增，与原各调用方手写队列一致） */
    int qcap = 64;
    if (n > qcap)
        qcap = n;
    if (spec->max_queue > 0 && spec->max_queue < qcap)
        qcap = spec->max_queue < 64 ? 64 : spec->max_queue;
    int *queue = (int *)lv_malloc((size_t)qcap * sizeof(int));
    if (!queue) {
        if (owned_visited)
            lv_free((void **)&visited);
        return -1;
    }

    int head = 0, tail = 0;

    /* 邻居缓冲（惰性分配） */
    int buf_cap = 0;
    int *nbr_ids = NULL;
    void **nbr_edges = NULL;

    /* 起点入队 */
    for (int i = 0; i < spec->seed_count; i++) {
        if (spec->max_queue > 0 && tail >= spec->max_queue)
            break;
        int s = spec->seeds[i];
        if (s < 0 || s >= n) {
            /* 越界起点：原手写实现中由出队处范围检查兜底（meta_verify 语义），
             * 直接跳过与"入队后出队跳过"等价 */
            continue;
        }
        if (spec->mark_on_enqueue)
            visited[s] = true;
        if (tail >= qcap) {
            if (!lv_ensure_capacity((void **)&queue, tail, &qcap, sizeof(int), 1)) {
                if (owned_visited)
                    lv_free((void **)&visited);
                lv_free((void **)&nbr_ids);
                lv_free((void **)&nbr_edges);
                lv_free((void **)&queue);
                return -1;
            }
        }
        queue[tail++] = s;
    }

    int processed = 0;

    while (head < tail) {
        int cur = queue[head++];
        processed++;

        /* 范围检查（先于 visit：与 meta_verify 原"越界前提出队即跳过"语义一致） */
        if (cur < 0 || cur >= n)
            continue;

        /* visit 回调（在 visited 判定之前：meta_verify 依赖"起点已标记仍可检测 cur==self"） */
        bool skip_children = false;
        if (spec->visit) {
            lvTraversalResult tr = spec->visit(spec->ctx, cur);
            if (tr == lv_TRAVERSAL_STOP)
                break;
            if (tr == lv_TRAVERSAL_SKIP_CHILDREN)
                skip_children = true;
        }

        if (!spec->mark_on_enqueue) {
            if (visited[cur])
                continue;
            visited[cur] = true;
        }

        if (skip_children)
            continue;

        /* 出边扩展（BFS：全部邻居作为批次 0；返回 0 表示无邻居） */
        int cnt = collect_neighbor_batch(spec->neighbors, spec->ctx, cur, 0,
                                         &nbr_ids, &nbr_edges, &buf_cap, false);
        if (cnt < 0) {
            if (owned_visited)
                lv_free((void **)&visited);
            lv_free((void **)&nbr_ids);
            lv_free((void **)&nbr_edges);
            lv_free((void **)&queue);
            return -1;
        }
        for (int j = 0; j < cnt; j++) {
            int nb = nbr_ids[j];
            if (spec->mark_on_enqueue) {
                if (nb < 0 || nb >= n)
                    continue;
                if (visited[nb])
                    continue;
                visited[nb] = true;
            }
            if (spec->max_queue > 0 && tail >= spec->max_queue)
                continue; /* 定长截断：丢弃新元素 */
            if (tail >= qcap) {
                if (!lv_ensure_capacity((void **)&queue, tail, &qcap, sizeof(int), 1)) {
                    if (owned_visited)
                        lv_free((void **)&visited);
                    lv_free((void **)&nbr_ids);
                    lv_free((void **)&nbr_edges);
                    lv_free((void **)&queue);
                    return -1;
                }
            }
            queue[tail++] = nb;
        }
    }

    lv_free((void **)&nbr_ids);
    lv_free((void **)&nbr_edges);
    lv_free((void **)&queue);
    if (owned_visited)
        lv_free((void **)&visited);
    return processed;
}

/**
 * @brief 通用三色环检测核心（非递归三色 DFS）
 *
 * 语义与手写实现（conflict_detector 的 detect_cyclic_dependency_conflicts、
 * 原 lv_graph_has_cycle）等价：
 *  - 0=WHITE 未访问, 1=GRAY 栈中, 2=BLACK 已完成；
 *  - 起点按 seeds 顺序（NULL 时 0..node_count-1），仅从 WHITE 节点开新根；
 *  - 压栈时标灰；枚举出边时遇到 GRAY 邻居 → on_cycle 回调（CONTINUE 继续枚举）；
 *  - 遇到 WHITE 邻居 → 标灰、压入"恢复帧 + 子帧"后下潜（回溯时从下一批次继续，
 *    与手写 iter 按约束恢复的语义一致：当前批次剩余项跳过）；批次耗尽 → 标黑。
 * on_cycle 返回 lv_TRAVERSAL_STOP 或未提供回调时，发现首个环即终止并返回 true。
 */
bool lv_cycle_detect(const lvCycleDetectSpec *spec) {
    if (!spec || !spec->neighbors || spec->node_count <= 0)
        return false;

    int n = spec->node_count;
    char *color = (char *)lv_calloc((size_t)n, sizeof(char)); /* 0 WHITE 1 GRAY 2 BLACK */
    if (!color)
        return false;
    lv_DEFER_FREE(color);

    typedef struct {
        int node_id;
        int iter; /* 批次恢复位置（下一批次索引） */
    } CycleFrame;

    int stack_cap = 64;
    CycleFrame *stack = (CycleFrame *)lv_malloc((size_t)stack_cap * sizeof(CycleFrame));
    if (!stack)
        return false; /* 守卫自动释放 color */
    lv_DEFER_FREE(stack);

    int buf_cap = 0;
    int *nbr_ids = NULL;
    void **nbr_edges = NULL;
    lv_DEFER_FREE(nbr_ids);
    lv_DEFER_FREE(nbr_edges);

    bool detected = false;

    int seed_total = spec->seeds ? spec->seed_count : n;
    for (int si = 0; si < seed_total && !detected; si++) {
        int s = spec->seeds ? spec->seeds[si] : si;
        if (s < 0 || s >= n)
            continue;
        if (color[s] != 0) /* 非 WHITE */
            continue;

        int top = 0;
        stack[top].node_id = s;
        stack[top].iter = 0;
        top++;
        color[s] = 1; /* GRAY：压栈时标灰 */

        while (top > 0 && !detected) {
            top--;
            CycleFrame f = stack[top];
            if (f.node_id < 0 || f.node_id >= n)
                continue;
            if (color[f.node_id] == 2) /* BLACK：已完成（防御，正常不会入栈） */
                continue;

            /* 按批次枚举（f.iter = 下一批次索引），与手写 iter 按约束恢复语义一致 */
            int bi = f.iter;
            bool descended = false;
            while (!descended && !detected) {
                int cnt = collect_neighbor_batch(spec->neighbors, spec->ctx, f.node_id, bi,
                                                 &nbr_ids, &nbr_edges, &buf_cap, true);
                if (cnt == -1) {
                    /* 槽位无效（非活跃超边）：推进批次继续 */
                    bi++;
                    continue;
                }
                if (cnt < 0) {
                    /* 内存不足：按"无环"返回（与原实现 OOM 返回 false 一致），
                     * 守卫自动释放 color/stack/nbr 缓冲 */
                    return false;
                }
                if (cnt == 0) {
                    /* 无更多批次 → 枚举耗尽 → 标黑 */
                    color[f.node_id] = 2;
                    break;
                }

                /* 处理本批次参与者 */
                int j = 0;
                while (j < cnt && !detected) {
                    int nb = nbr_ids[j];
                    if (nb < 0 || nb >= n) {
                        j++;
                        continue;
                    }
                    if (color[nb] == 1) {
                        /* GRAY 邻居 → 反向边（环） */
                        if (spec->on_cycle) {
                            lvTraversalResult tr = spec->on_cycle(spec->ctx, f.node_id, nb,
                                                                  nbr_edges ? nbr_edges[j] : NULL);
                            if (tr == lv_TRAVERSAL_STOP) {
                                detected = true;
                                break;
                            }
                        } else {
                            detected = true;
                            break;
                        }
                        j++;
                    } else if (color[nb] == 0) {
                        /* WHITE：标灰并下潜（恢复帧 = 下一批次，跳过本批次剩余项，
                         * 与原实现 iter 按约束推进的恢复语义一致） */
                        color[nb] = 1;
                        if (top + 2 > stack_cap) {
                            if (!lv_ensure_capacity((void **)&stack, top, &stack_cap,
                                                    sizeof(CycleFrame), 1))
                                return false; /* 守卫自动释放 color/stack/nbr 缓冲 */
                        }
                        stack[top].node_id = f.node_id;
                        stack[top].iter = bi + 1;
                        top++;
                        stack[top].node_id = nb;
                        stack[top].iter = 0;
                        top++;
                        descended = true;
                        break;
                    } else {
                        j++; /* BLACK */
                    }
                }

                if (!descended && !detected)
                    bi++; /* 本批次处理完（未下潜），继续下一批次 */
            }
        }
    }

    return detected;
}

/**
 * @brief 通用 Kahn 拓扑排序（任意整数 id 图：回调提供后继）
 *
 * 与 lv_bfs_run / lv_cycle_detect 同级的通用设施：
 *  - 节点空间 0..node_count-1，待排序集合由 nodes 数组指定（NULL = 全部，按 id 升序）；
 *  - 入度在驱动内部由 successors 回调逐节点枚举全部批次后继统计（含重复边
 *    重复计数，与各调用方手写实现一致）；
 *  - 初始入队顺序 = nodes 数组序（NULL 时按 id 升序）；队列 FIFO 出队，输出到
 *    out_order；返回已排序节点数，小于去重节点数表示存在环（仅输出无环部分，
 *    由调用方据返回值判定环）。
 */
int lv_topo_run(const lvTopoSpec *spec) {
    if (!spec || !spec->successors || spec->node_count <= 0)
        return -1;

    int n = spec->node_count;

    /* 待排序集合（in_set 后半程复用为"已入队"标记） */
    bool *in_set = (bool *)lv_calloc((size_t)n, sizeof(bool));
    if (!in_set)
        return -1;
    int set_size = 0;
    if (spec->nodes) {
        for (int i = 0; i < spec->nodes_count; i++) {
            int id = spec->nodes[i];
            if (id < 0 || id >= n)
                continue;
            if (!in_set[id]) {
                in_set[id] = true;
                set_size++;
            }
        }
    } else {
        for (int i = 0; i < n; i++) {
            in_set[i] = true;
        }
        set_size = n;
    }
    if (set_size == 0) {
        lv_free((void **)&in_set);
        return 0;
    }

    /* 入度统计（逐节点枚举全部批次后继；重复后继重复计数） */
    int *in_degree = (int *)lv_calloc((size_t)n, sizeof(int));
    if (!in_degree) {
        lv_free((void **)&in_set);
        return -1;
    }
    int buf_cap = 0;
    int *succ_ids = NULL;
    for (int i = 0; i < n; i++) {
        if (!in_set[i])
            continue;
        int bi = 0;
        for (;;) {
            int cnt = collect_neighbor_batch(spec->successors, spec->ctx, i, bi,
                                             &succ_ids, NULL, &buf_cap, false);
            if (cnt == 0)
                break;
            if (cnt < 0)
                goto lv_topo_run_oom;
            for (int j = 0; j < cnt; j++) {
                int nb = succ_ids[j];
                if (nb >= 0 && nb < n && in_set[nb])
                    in_degree[nb]++;
            }
            bi++;
        }
    }

    /* 初始入队：入度为 0 的待排序节点（按 nodes 序 / id 升序） */
    int *queue = (int *)lv_malloc((size_t)set_size * sizeof(int));
    if (!queue)
        goto lv_topo_run_oom;
    int head = 0, tail = 0;
    if (spec->nodes) {
        for (int i = 0; i < spec->nodes_count; i++) {
            int id = spec->nodes[i];
            if (id < 0 || id >= n)
                continue;
            if (in_set[id] && in_degree[id] == 0) {
                in_set[id] = false; /* 标记已入队 */
                queue[tail++] = id;
            }
        }
    } else {
        for (int i = 0; i < n; i++) {
            if (in_set[i] && in_degree[i] == 0) {
                in_set[i] = false;
                queue[tail++] = i;
            }
        }
    }

    /* Kahn 主循环（FIFO 队列） */
    int topo_count = 0;
    while (head < tail) {
        int cur = queue[head++];
        if (spec->out_order)
            spec->out_order[topo_count] = cur;
        topo_count++;

        int bi = 0;
        for (;;) {
            int cnt = collect_neighbor_batch(spec->successors, spec->ctx, cur, bi,
                                             &succ_ids, NULL, &buf_cap, false);
            if (cnt == 0)
                break;
            if (cnt < 0)
                goto lv_topo_run_oom;
            for (int j = 0; j < cnt; j++) {
                int nb = succ_ids[j];
                if (nb < 0 || nb >= n || !in_set[nb])
                    continue;
                in_degree[nb]--;
                if (in_degree[nb] == 0) {
                    in_set[nb] = false;
                    queue[tail++] = nb;
                }
            }
            bi++;
        }
    }

    lv_free((void **)&in_set);
    lv_free((void **)&in_degree);
    lv_free((void **)&succ_ids);
    lv_free((void **)&queue);
    return topo_count;

lv_topo_run_oom:
    lv_free((void **)&in_set);
    lv_free((void **)&in_degree);
    lv_free((void **)&succ_ids);
    lv_free((void **)&queue);
    return -1;
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
 * @brief lv_graph_has_cycle 的邻居枚举回调
 *
 * 保持原实现语义：约束超图无向邻居（find_neighbors，skip_disabled=true）。
 * 邻居经 lvDArray 动态收集（无固定上限），再按槽位容量写出；
 * 返回 == max_neighbors 时 collect_neighbor_batch 扩容重试，直至完整。
 */
static int has_cycle_neighbors_cb(void *ctx, int node_id, int batch_index,
                                  int *out_neighbors, void **out_edge_infos,
                                  int max_neighbors) {
    ConstraintGraph *graph = (ConstraintGraph *)ctx;
    (void)node_id;
    (void)out_edge_infos;
    if (!out_neighbors || max_neighbors <= 0)
        return 0;
    if (batch_index != 0)
        return 0; /* 单批次（全部邻居合并为一个批次），批次 1 起为空 */

    lvGraphTraversalConfig cfg = lv_GRAPH_TRAVERSAL_DEFAULT_CONFIG;
    cfg.skip_disabled = true;

    lvDArray nbr;
    lv_darray_init(&nbr, sizeof(int));
    int cnt = find_neighbors(graph, node_id, &nbr, &cfg);
    if (cnt < 0) {
        lv_darray_free(&nbr);
        return -1; /* 内存不足 */
    }
    int out_cnt = cnt < max_neighbors ? cnt : max_neighbors;
    for (int i = 0; i < out_cnt; i++) {
        out_neighbors[i] = *(const int *)lv_darray_get(&nbr, i);
    }
    lv_darray_free(&nbr);
    return out_cnt;
}

/**
 * @brief 三色标记法环检测
 *
 * 使用 DFS 对图进行三色标记：
 *   - 0 (WHITE): 未访问
 *   - 1 (GRAY):  正在访问（在当前 DFS 路径上）
 *   - 2 (BLACK): 已访问完成
 *
 * 委托给通用核心 lv_cycle_detect（无 on_cycle 回调，发现首个环即返回 true），
 * 语义与原手写实现等价：起点为 nodes 数组序的活跃节点、邻居为约束超图
 * 无向邻居（跳过 disabled）。
 */
bool lv_graph_has_cycle(ConstraintGraph *graph) {
    if (!graph || graph->node_count <= 0)
        return false;

    int visited_size = get_max_node_id(graph);
    if (visited_size <= 0)
        return false;

    /* 起点：nodes 数组序的活跃节点（与原实现一致） */
    int *seeds = (int *)lv_malloc((size_t)graph->node_count * sizeof(int));
    if (!seeds)
        return false;
    int seed_count = 0;
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node || !node->is_active)
            continue;
        if (node->id >= 0 && node->id < visited_size)
            seeds[seed_count++] = node->id;
    }

    lvCycleDetectSpec spec = {
        visited_size, seeds, seed_count,
        has_cycle_neighbors_cb, NULL, graph
    };
    bool has_cycle = lv_cycle_detect(&spec);
    lv_free((void **)&seeds);
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
        lvDArray nbr;
        lv_darray_init(&nbr, sizeof(int));
        int ncount = find_neighbors(graph, nid, &nbr,
            &(lvGraphTraversalConfig){lv_TRAVERSAL_DFS_PRE, 0, false, false, true});
        if (ncount < 0) {
            lv_darray_free(&nbr);
            lv_free((void **)&in_degree);
            lv_free((void **)&queue);
            lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "lv_graph_topological_sort: find_neighbors OOM");
        }

        for (int i = 0; i < ncount; i++) {
            int nb = *(const int *)lv_darray_get(&nbr, i);
            if (nb < 0 || nb >= visited_size)
                continue;
            in_degree[nb]--;
            if (in_degree[nb] == 0) {
                queue[qtail++] = nb;
            }
        }
        lv_darray_free(&nbr);
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
