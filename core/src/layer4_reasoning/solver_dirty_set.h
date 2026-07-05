/**
 * @file solver_dirty_set.h
 * @brief 脏变量追踪 — 增量求解支持
 *
 * 从 solver.c 拆分出的独立模块。追踪在增量求解中发生变化的变量，
 * 只有涉及脏变量的方程需要重新求解，避免全局重新计算。
 *
 * 原位置: solver.c L6375-L6453
 */

#ifndef LV00_SOLVER_DIRTY_SET_H
#define LV00_SOLVER_DIRTY_SET_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 脏变量集合结构体 */
typedef struct {
    int *dirty_ids;  /* dirty variable IDs */
    int dirty_count; /* number of dirty variables */
    int capacity;    /* allocated capacity */
} DirtyVariableSet;

/* 初始化脏变量集合 */
void dirty_set_init(DirtyVariableSet *ds);

/* 检查变量 ID 是否在脏集合中 */
bool dirty_set_contains(DirtyVariableSet *ds, int var_id);

/* 添加一个脏变量 ID */
void dirty_set_add(DirtyVariableSet *ds, int var_id);

/* 清空脏变量集合（不释放内存，保留容量以供复用） */
void dirty_set_clear(DirtyVariableSet *ds);

/* 释放脏变量集合资源 */
void dirty_set_free(DirtyVariableSet *ds);

#ifdef __cplusplus
}
#endif

#endif /* LV00_SOLVER_DIRTY_SET_H */