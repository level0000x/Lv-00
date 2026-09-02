/**
 * @file solver_dirty_set.h
 * @brief 脏变量追踪 — 增量求解支持
 *
 * 从 solver.c 拆分出的独立模块（Lv-00 项目 v3.3.0+）。
 * 追踪在增量求解中发生变化的变量，只有涉及脏变量的方程需要重新求解，
 * 避免全局重新计算。solver_dirty_set.c 为唯一实现，
 * solver_incremental.c 通过本头文件共享该模块。
 */

#ifndef lv_SOLVER_DIRTY_SET_H
#define lv_SOLVER_DIRTY_SET_H

#include "lv_api_spec.h" /* lv_PUBLIC_API（K59） */
#include "lv/solver_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 脏变量集合结构体 */
typedef struct {
    lvDArray dirty_ids; /* dirty variable IDs (lvDArray of int) */
} DirtyVariableSet;

/* 初始化脏变量集合 */
lv_PUBLIC_API void dirty_set_init(DirtyVariableSet *ds);

/* 检查变量 ID 是否在脏集合中 */
lv_PUBLIC_API bool dirty_set_contains(DirtyVariableSet *ds, int var_id);

/* 向脏集合中添加变量 ID（已存在则忽略） */
lv_PUBLIC_API void dirty_set_add(DirtyVariableSet *ds, int var_id);

/* 清空脏变量集合（不释放内存, 保留容量以供复用） */
lv_PUBLIC_API void dirty_set_clear(DirtyVariableSet *ds);

/* 释放脏变量集合资源 */
lv_PUBLIC_API void dirty_set_free(DirtyVariableSet *ds);

/* 基于脏变量过滤方程（从完整方程系统中筛选出涉及脏变量的方程子集） */
lv_PUBLIC_API void filter_equations_for_dirty(EquationSystem *sys, DirtyVariableSet *ds, EquationSystem *filtered);

/* 判断两个 (var_node_id, coord_index) 键是否相同 */
lv_PUBLIC_API bool poly_eq_same_key(int a_var, int a_coord, int b_var, int b_coord);

#ifdef __cplusplus
}
#endif

#endif /* lv_SOLVER_DIRTY_SET_H */
