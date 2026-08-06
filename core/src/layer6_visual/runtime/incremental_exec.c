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

    /* 脏块集合（替换原"有效性位图"：位图语义 0=脏/1=有效，初始全有效；
     * 脏集合语义 空=全有效，invalidate 即 add，is_valid 即 !contains，完全等价） */
    lv_dirty_set dirty_set;
} lvIncrementalExec;

lvIncrementalExec *lv_incremental_exec_create(int node_count) {
    lvIncrementalExec *exec = lv_calloc(1, sizeof(lvIncrementalExec));
    if (!exec)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "failed to allocate incremental exec");
    exec->node_count = node_count;
    /* 初始状态：所有 block 都有效（空脏集合等价原全 1 位图，元素按需懒分配） */
    lv_dirty_set_init(&exec->dirty_set);
    return exec;
}

void lv_incremental_exec_destroy(lvIncrementalExec *exec) {
    if (!exec)
        return;
    lv_free((void **) &exec->dependency_graph);
    lv_dirty_set_free(&exec->dirty_set);
    lv_free((void **) &exec);
}

/* 将指定block标记为无效（脏） */
int lv_incremental_exec_invalidate(lvIncrementalExec *exec, int block_id) {
    if (!exec || block_id < 0 || block_id >= exec->node_count)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "NULL exec or invalid block_id");
    lv_dirty_set_add(&exec->dirty_set, block_id);
    return 0;
}

/* 检查指定block的输出是否有效 */
int lv_incremental_exec_is_valid(lvIncrementalExec *exec, int block_id) {
    if (!exec || block_id < 0 || block_id >= exec->node_count)
        return 0;
    return !lv_dirty_set_contains(&exec->dirty_set, block_id);
}
