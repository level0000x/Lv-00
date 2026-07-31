#include <stdlib.h>
#include <string.h>

#include "lv/block_scheduler.h"
#include "lv/lv_internal.h"
#include "lv/lv_utils.h"

/* Incremental execution engine */
/* Tracks which blocks need re-execution based on port value changes */

typedef struct lvIncrementalExec {
    void *value_cache;
    int *dependency_graph;
    int node_count;

    /* 有效性位图：每个int的32位代表32个block的有效性，0=无效(脏), 1=有效 */
    unsigned int *validity_bitmap;
    int bitmap_count; /* bitmap数组长度 */
} lvIncrementalExec;

lvIncrementalExec *lv_incremental_exec_create(int node_count) {
    lvIncrementalExec *exec = lv_calloc(1, sizeof(lvIncrementalExec));
    if (!exec)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "failed to allocate incremental exec");
    exec->node_count = node_count;
    /* 分配位图，每个unsigned int追踪32个block */
    if (node_count > 0) {
        exec->bitmap_count = (node_count + 31) / 32;
        exec->validity_bitmap = lv_calloc(exec->bitmap_count, sizeof(unsigned int));
        if (!exec->validity_bitmap) {
            lv_free((void **) &exec);
            lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "failed to allocate validity bitmap");
        }
        /* 初始状态：所有block都有效 */
        memset(exec->validity_bitmap, 0xFF, exec->bitmap_count * sizeof(unsigned int));
    }
    return exec;
}

void lv_incremental_exec_destroy(lvIncrementalExec *exec) {
    if (!exec)
        return;
    lv_free((void **) &exec->dependency_graph);
    lv_free((void **) &exec->validity_bitmap);
    lv_free((void **) &exec);
}

/* 将指定block标记为无效（脏） */
int lv_incremental_exec_invalidate(lvIncrementalExec *exec, int block_id) {
    if (!exec || block_id < 0 || block_id >= exec->node_count)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "NULL exec or invalid block_id");
    int word_idx = block_id / 32;
    int bit_idx = block_id % 32;
    exec->validity_bitmap[word_idx] &= ~(1u << bit_idx);
    return 0;
}

/* 检查指定block的输出是否有效 */
int lv_incremental_exec_is_valid(lvIncrementalExec *exec, int block_id) {
    if (!exec || block_id < 0 || block_id >= exec->node_count)
        return 0;
    int word_idx = block_id / 32;
    int bit_idx = block_id % 32;
    return (exec->validity_bitmap[word_idx] & (1u << bit_idx)) != 0;
}
