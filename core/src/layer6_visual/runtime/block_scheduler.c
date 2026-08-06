#include "lv/block_scheduler.h"

#include <stdlib.h>
#include <string.h>

#include "lv/block_graph_view.h"
#include "lv/func_block.h"
#include "lv/lv_internal.h"
#include "lv/lv_utils.h"

/* 构建邻接表：根据输出端口和输入端口的连接关系确定依赖 */
/* 通过端口 ID 匹配确定块间依赖（run 与 run_incremental 共用；in_degree 可为 NULL） */
static int build_block_adjacency(BlockGraphView *bg, int n, int *in_degree, int ***out_adj, int **out_adj_count) {
    int **adj = *out_adj;
    int *adj_count = *out_adj_count;

    for (int i = 0; i < n; i++) {
        FuncBlock *fb = bg->blocks[i];
        if (!fb)
            continue;
        int out_count = func_block_get_output_count(fb);
        for (int oi = 0; oi < out_count; oi++) {
            int out_port = fb->output_port_ids ? fb->output_port_ids[oi] : -1;
            if (out_port < 0)
                continue;
            /* 查找哪些块的输入端口连接到此输出端口 */
            for (int j = 0; j < n; j++) {
                if (i == j)
                    continue;
                FuncBlock *other = bg->blocks[j];
                if (!other)
                    continue;
                int in_count = func_block_get_input_count(other);
                for (int ii = 0; ii < in_count; ii++) {
                    int in_port = other->input_port_ids ? other->input_port_ids[ii] : -1;
                    if (in_port == out_port) {
                        /* 存在连接：i -> j */
                        /* 线性增长：每次 +1 个元素直接 realloc（adj 无 cap 字段，不引入 lv_ensure_capacity） */
                        adj_count[i]++;
                        int *new_adj = lv_realloc(adj[i], adj_count[i] * sizeof(int));
                        if (new_adj) {
                            adj[i] = new_adj;
                            adj[i][adj_count[i] - 1] = j;
                        }
                        if (in_degree)
                            in_degree[j]++;
                    }
                }
            }
        }
    }
    return 0;
}

lvBlockScheduler *lv_block_scheduler_create(void *graph) {
    lvBlockScheduler *sched = lv_calloc(1, sizeof(lvBlockScheduler));
    if (!sched)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "failed to allocate block scheduler");
    sched->graph = graph;
    sched->strategy = lv_SCHED_FULL;
    sched->effect_tracker = lv_effect_tracker_create();
    lv_dirty_set_init(&sched->incremental.dirty_set);
    return sched;
}

void lv_block_scheduler_destroy(lvBlockScheduler *sched) {
    if (!sched)
        return;
    lv_free((void **) &sched->queue);
    lv_dirty_set_free(&sched->incremental.dirty_set);
    if (sched->effect_tracker)
        lv_effect_tracker_destroy(sched->effect_tracker);
    lv_free((void **) &sched);
}

void lv_block_scheduler_set_strategy(lvBlockScheduler *sched, lvSchedStrategy strategy) {
    if (sched)
        sched->strategy = strategy;
}

/* 拓扑排序执行：Kahn 算法 */
lvExecResult lv_block_scheduler_run(lvBlockScheduler *sched) {
    lvExecResult result = {0};
    if (!sched) {
        result.success = 0;
        strncpy(result.error_msg, "NULL scheduler", sizeof(result.error_msg));
        return result;
    }

    BlockGraphView *bg = (BlockGraphView *) sched->graph;
    if (!bg || !bg->blocks || bg->count <= 0) {
        result.success = 0;
        strncpy(result.error_msg, "Invalid or empty block graph", sizeof(result.error_msg));
        return result;
    }

    int n = bg->count;

    /* 分配工作数组 */
    int *in_degree = lv_calloc(n, sizeof(int));
    int *adj_count = lv_calloc(n, sizeof(int));  /* 每个块的下游邻居数 */
    int **adj = lv_calloc(n, sizeof(int *));     /* 邻接表 */
    int *queue_buf = lv_calloc(n, sizeof(int));  /* 拓扑排序队列 */
    int *topo_order = lv_calloc(n, sizeof(int)); /* 拓扑排序结果 */

    if (!in_degree || !adj_count || !adj || !queue_buf || !topo_order) {
        lv_free_many(&in_degree, &adj_count, &adj, &queue_buf, &topo_order, NULL);
        result.success = 0;
        strncpy(result.error_msg, "Out of memory", sizeof(result.error_msg));
        return result;
    }

    /* 构建邻接表：根据输出端口和输入端口的连接关系确定依赖 */
    /* 通过端口 ID 匹配确定块间依赖 */
    build_block_adjacency(bg, n, in_degree, &adj, &adj_count);

    /* Kahn 算法：将入度为0的节点入队 */
    int front = 0, back = 0;
    for (int i = 0; i < n; i++) {
        if (in_degree[i] == 0) {
            queue_buf[back++] = i;
        }
    }

    int topo_count = 0;
    while (front < back) {
        int cur = queue_buf[front++];
        topo_order[topo_count++] = cur;

        /* 遍历下游邻居，减少入度 */
        for (int k = 0; k < adj_count[cur]; k++) {
            int next = adj[cur][k];
            in_degree[next]--;
            if (in_degree[next] == 0) {
                queue_buf[back++] = next;
            }
        }
    }

    /* 检测环 */
    if (topo_count < n) {
        result.success = 0;
        snprintf(result.error_msg, sizeof(result.error_msg), "Cycle detected in block graph: %d/%d blocks sorted",
                 topo_count, n);
    } else {
        /* 按拓扑序执行所有块 */
        result.success = 1;
        for (int i = 0; i < topo_count; i++) {
            int idx = topo_order[i];
            FuncBlock *fb = bg->blocks[idx];
            if (!fb)
                continue;

            /* 记录效果（当前假设纯计算，完整版需分析副作用） */
            if (sched->effect_tracker) {
                lv_effect_tracker_record(sched->effect_tracker, lv_EFFECT_PURE, fb->id, "block executed");
            }
            result.blocks_executed++;
        }

        /* 保存拓扑排序队列 */
        lv_free((void **) &sched->queue);
        sched->queue = topo_order;
        sched->queue_count = topo_count;
        topo_order = NULL; /* 防止下面释放 */
    }

    /* 清理 */
    for (int i = 0; i < n; i++)
        lv_free((void **) &adj[i]);
    lv_free_many(&adj, &adj_count, &in_degree, &queue_buf, &topo_order, NULL);

    return result;
}

/* 增量执行：仅执行脏块及其下游依赖 */
lvExecResult lv_block_scheduler_run_incremental(lvBlockScheduler *sched, int *dirty, int count) {
    lvExecResult result = {0};
    if (!sched) {
        result.success = 0;
        strncpy(result.error_msg, "NULL scheduler", sizeof(result.error_msg));
        return result;
    }

    BlockGraphView *bg = (BlockGraphView *) sched->graph;
    if (!bg || !bg->blocks || bg->count <= 0) {
        result.success = 0;
        strncpy(result.error_msg, "Invalid or empty block graph", sizeof(result.error_msg));
        return result;
    }

    int n = bg->count;

    /* 确定脏块来源：优先使用参数传入的数组，否则使用调度器内部的脏集合 */
    int use_internal_dirty = 0;
    if (dirty && count > 0) {
        use_internal_dirty = 0; /* 使用外部数组 dirty/count */
    } else if (lv_dirty_set_count(&sched->incremental.dirty_set) > 0) {
        use_internal_dirty = 1;
    } else {
        /* 没有脏块，无需执行 */
        result.success = 1;
        result.blocks_skipped = n;
        return result;
    }

    /* 构建邻接表（同 run 函数） */
    int *adj_count = lv_calloc(n, sizeof(int));
    int **adj = lv_calloc(n, sizeof(int *));
    if (!adj_count || !adj) {
        lv_free_many(&adj_count, &adj, NULL);
        result.success = 0;
        strncpy(result.error_msg, "Out of memory", sizeof(result.error_msg));
        return result;
    }

    build_block_adjacency(bg, n, NULL, &adj, &adj_count);

    /* 计算脏块的传递闭包（包括所有下游依赖） */
    int *need_exec = lv_calloc(n, sizeof(int));
    if (!need_exec) {
        for (int i = 0; i < n; i++)
            lv_free((void **) &adj[i]);
        lv_free_many(&adj, &adj_count, NULL);
        result.success = 0;
        strncpy(result.error_msg, "Out of memory", sizeof(result.error_msg));
        return result;
    }

    /* 标记初始脏块（外部数组按传入顺序；内部集合按插入顺序，即块索引升序） */
    if (use_internal_dirty) {
        for (int d = 0; d < lv_dirty_set_count(&sched->incremental.dirty_set); d++) {
            int block_id = lv_dirty_set_at(&sched->incremental.dirty_set, d);
            /* 查找块索引 */
            for (int i = 0; i < n; i++) {
                if (bg->blocks[i] && bg->blocks[i]->id == block_id) {
                    need_exec[i] = 1;
                    break;
                }
            }
        }
    } else {
        for (int d = 0; d < count; d++) {
            int block_id = dirty[d];
            /* 查找块索引 */
            for (int i = 0; i < n; i++) {
                if (bg->blocks[i] && bg->blocks[i]->id == block_id) {
                    need_exec[i] = 1;
                    break;
                }
            }
        }
    }

    /* BFS 扩展：标记所有下游依赖 */
    int *bfs_queue = lv_calloc(n, sizeof(int));
    if (bfs_queue) {
        int front = 0, back = 0;
        for (int i = 0; i < n; i++) {
            if (need_exec[i])
                bfs_queue[back++] = i;
        }
        while (front < back) {
            int cur = bfs_queue[front++];
            for (int k = 0; k < adj_count[cur]; k++) {
                int next = adj[cur][k];
                if (!need_exec[next]) {
                    need_exec[next] = 1;
                    bfs_queue[back++] = next;
                }
            }
        }
        lv_free((void **) &bfs_queue);
    }

    /* 执行需要执行的块（按原始数组顺序，即拓扑序） */
    result.success = 1;
    for (int i = 0; i < n; i++) {
        if (!need_exec[i]) {
            result.blocks_skipped++;
            continue;
        }
        FuncBlock *fb = bg->blocks[i];
        if (!fb)
            continue;

        if (sched->effect_tracker) {
            lv_effect_tracker_record(sched->effect_tracker, lv_EFFECT_PURE, fb->id, "block executed (incremental)");
        }
        result.blocks_executed++;
    }

    /* 清理 */
    lv_free((void **) &need_exec);
    for (int i = 0; i < n; i++)
        lv_free((void **) &adj[i]);
    lv_free((void **) &adj);
    lv_free((void **) &adj_count);

    return result;
}

/* 标记单个块为脏 */
void lv_block_scheduler_mark_dirty(lvBlockScheduler *sched, int block_id) {
    if (!sched || block_id <= 0)
        return;

    /* add 内部去重（已存在则忽略），与原有线性查重语义一致 */
    lv_dirty_set_add(&sched->incremental.dirty_set, block_id);
}

/* 标记所有块为脏 */
void lv_block_scheduler_mark_all_dirty(lvBlockScheduler *sched) {
    if (!sched)
        return;

    BlockGraphView *bg = (BlockGraphView *) sched->graph;
    if (!bg || !bg->blocks)
        return;

    /* 清空旧脏集合（保留容量供复用），再按块索引升序逐个加入 */
    lv_dirty_set_clear(&sched->incremental.dirty_set);
    int n = bg->count;
    for (int i = 0; i < n; i++) {
        if (bg->blocks[i]) {
            lv_dirty_set_add(&sched->incremental.dirty_set, bg->blocks[i]->id);
        }
    }
}
