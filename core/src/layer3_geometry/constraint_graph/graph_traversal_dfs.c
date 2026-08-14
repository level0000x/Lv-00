/**
 * @file graph_traversal_dfs.c
 * @brief 图遍历内部辅助与 DFS/BFS/拓扑内部实现（由 lv_graph_traversal.c 拆分子模块）
 *
 * @details 邻居收集、BFS 队列、DFS/BFS/拓扑遍历核心（图遍历 API 面
 *          与便利函数段复用，声明见 graph_traversal_internal.h）。
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
 * 内部辅助函数
 * ============================================================ */

/**
 * @brief 获取节点 ID 的最大值（用于 visited 数组大小）
 */
int get_max_node_id(const ConstraintGraph *graph) {
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
bool should_skip_node(ConstraintGraph *graph, int node_id,
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
int find_neighbors(ConstraintGraph *graph, int node_id,
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
int dfs_traverse_from(ConstraintGraph *graph, int start_id,
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
int bfs_traverse_from(ConstraintGraph *graph, int start_id,
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
int traverse_all_components(ConstraintGraph *graph,
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
int topological_order_traverse(ConstraintGraph *graph, bool *visited, int visited_size,
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
int mark_reachable_from(ConstraintGraph *graph, int start_id,
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
int traverse_from_topological(ConstraintGraph *graph, int start_node_id,
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

