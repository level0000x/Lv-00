/**
 * @file graph_traversal_util.c
 * @brief 图便利函数与枚举映射（由 lv_graph_traversal.c 拆分子模块）
 *
 * @details lv_graph_count_nodes / lv_graph_has_cycle /
 *          lv_graph_topological_sort 与遍历序/结果枚举字符串映射。
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
