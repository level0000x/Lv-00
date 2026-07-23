/**
 * @file solver_dirty_set.c
 * @brief 脏变量追踪实现 — 从 solver.c 拆分
 *
 * 原位置: solver.c L6375-L6453
 */

#include "../solver_dirty_set.h"
#include "lv/lv.h"
#include <limits.h>

/* 数组增长因子（与 solver.c 原实现保持一致） */
#ifndef lv_ARRAY_GROWTH_FACTOR
#define lv_ARRAY_GROWTH_FACTOR 2
#endif

void dirty_set_init(DirtyVariableSet *ds) {
    ds->dirty_ids = NULL;
    ds->dirty_count = 0;
    ds->capacity = 0;
}

bool dirty_set_contains(DirtyVariableSet *ds, int var_id) {
    for (int i = 0; i < ds->dirty_count; i++) {
        if (ds->dirty_ids[i] == var_id)
            return true;
    }
    return false;
}

void dirty_set_add(DirtyVariableSet *ds, int var_id) {
    /* 检查是否已存在 */
    if (dirty_set_contains(ds, var_id))
        return;

    if (ds->dirty_count >= ds->capacity) {
        /* 内存安全修复：检查整数溢出 */
        int new_cap = ds->capacity == 0 ? 16 : ds->capacity;
        if (new_cap > 0 && new_cap > INT_MAX / lv_ARRAY_GROWTH_FACTOR)
            return; /* 溢出，跳过 */
        new_cap = new_cap == 0 ? 16 : new_cap * lv_ARRAY_GROWTH_FACTOR;
        ds->capacity = new_cap;
        int *new_ids = lv_realloc(ds->dirty_ids, ds->capacity * sizeof(int));
        if (!new_ids)
            return; /* allocation failed, skip */
        ds->dirty_ids = new_ids;
    }
    ds->dirty_ids[ds->dirty_count++] = var_id;
}

void dirty_set_clear(DirtyVariableSet *ds) {
    ds->dirty_count = 0;
    /* 不释放内存，保留容量以供复用 */
}

void dirty_set_free(DirtyVariableSet *ds) {
    lv_free((void **) &ds->dirty_ids);
    ds->dirty_ids = NULL;
    ds->dirty_count = 0;
    ds->capacity = 0;
}