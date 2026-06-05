#include "lv00/block_scheduler.h"
#include <stdlib.h>
#include <string.h>

/* Incremental execution engine */
/* Tracks which blocks need re-execution based on port value changes */

typedef struct Lv00IncrementalExec {
    void *value_cache;
    int *dependency_graph;
    int node_count;

    /* 有效性位图：每个int的32位代表32个block的有效性，0=无效(脏), 1=有效 */
    unsigned int *validity_bitmap;
    int bitmap_count;  /* bitmap数组长度 */
} Lv00IncrementalExec;

Lv00IncrementalExec *lv00_incremental_exec_create(int node_count) {
    Lv00IncrementalExec *exec = calloc(1, sizeof(Lv00IncrementalExec));
    if (!exec) return NULL;
    exec->node_count = node_count;
    /* 分配位图，每个unsigned int追踪32个block */
    if (node_count > 0) {
        exec->bitmap_count = (node_count + 31) / 32;
        exec->validity_bitmap = calloc(exec->bitmap_count, sizeof(unsigned int));
        if (!exec->validity_bitmap) {
            free(exec);
            return NULL;
        }
        /* 初始状态：所有block都有效 */
        memset(exec->validity_bitmap, 0xFF,
               exec->bitmap_count * sizeof(unsigned int));
    }
    return exec;
}

void lv00_incremental_exec_destroy(Lv00IncrementalExec *exec) {
    if (!exec) return;
    free(exec->dependency_graph);
    free(exec->validity_bitmap);
    free(exec);
}

/* 将指定block标记为无效（脏） */
int lv00_incremental_exec_invalidate(Lv00IncrementalExec *exec, int block_id) {
    if (!exec || block_id < 0 || block_id >= exec->node_count) return -1;
    int word_idx = block_id / 32;
    int bit_idx = block_id % 32;
    exec->validity_bitmap[word_idx] &= ~(1u << bit_idx);
    return 0;
}

/* 检查指定block的输出是否有效 */
int lv00_incremental_exec_is_valid(Lv00IncrementalExec *exec, int block_id) {
    if (!exec || block_id < 0 || block_id >= exec->node_count) return 0;
    int word_idx = block_id / 32;
    int bit_idx = block_id % 32;
    return (exec->validity_bitmap[word_idx] & (1u << bit_idx)) != 0;
}
