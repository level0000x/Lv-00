/**
 * @file graph_traversal_bfs.c
 * @brief 通用图算法核心（由 lv_graph_traversal.c 拆分子模块）
 *
 * @details lv_bfs_run / lv_cycle_detect / lv_topo_run 与邻居批量收集。
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

