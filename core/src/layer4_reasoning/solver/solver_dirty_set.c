/**
 * @file solver_dirty_set.c
 * @brief 脏变量追踪（增量求解支持）
 *
 * @details 从 solver.c 拆分出的子模块（Lv-00 项目 v3.3.0+）。
 *          本文件为 DirtyVariableSet 与 filter_equations_for_dirty 的唯一实现，
 *          solver_incremental.c 通过 lv/solver_dirty_set.h 共享。
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "solver_common.h"
#include "lv/solver_dirty_set.h"

/* ================================================================== */
/*  脏变量集合（共享实现，供增量求解模块使用）                          */
/* ================================================================== */

/* 初始化脏变量集合 */
void dirty_set_init(DirtyVariableSet *ds) {
    lv_darray_init(&ds->dirty_ids, sizeof(int));
}

/* 检查变量 ID 是否在脏集合中 */
bool dirty_set_contains(DirtyVariableSet *ds, int var_id) {
    for (int i = 0; i < ds->dirty_ids.count; i++) {
        int *p = (int *)lv_darray_get(&ds->dirty_ids, i);
        if (p && *p == var_id)
            return true;
    }
    return false;
}

/* 向脏集合中添加变量 ID */
void dirty_set_add(DirtyVariableSet *ds, int var_id) {
    if (dirty_set_contains(ds, var_id))
        return;
    lv_darray_push(&ds->dirty_ids, &var_id);
}

/* 清空脏变量集合 */
void dirty_set_clear(DirtyVariableSet *ds) {
    lv_darray_clear(&ds->dirty_ids);
    /* 不释放内存, 保留容量以供复用 */
}

/* 释放脏变量集合资源 */
void dirty_set_free(DirtyVariableSet *ds) {
    lv_darray_free(&ds->dirty_ids);
}

/* 判断两个 (var_node_id, coord_index) 键是否相同 */
bool poly_eq_same_key(int a_var, int a_coord, int b_var, int b_coord) {
    return a_var == b_var && a_coord == b_coord;
}

/* ================================================================== */
/*  内部: 基于脏变量过滤方程                                            */
/* ================================================================== */

/*
 * Filter equations to only those involving dirty variables.
 * 从完整方程系统中筛选出只涉及脏变量的方程子集,
 * 以及通过约束图传播涉及的间接相关方程。
 *
 * 传播规则: 如果一个方程涉及的变量与脏变量共享约束,
 * 则该方程也间接相关。
 */
void filter_equations_for_dirty(EquationSystem *sys, DirtyVariableSet *ds, EquationSystem *filtered) {
    equation_system_init(filtered);

    if (!sys || !ds || ds->dirty_ids.count == 0)
        return;

    /* 第一遍: 直接筛选涉及脏变量的方程 */
    for (int i = 0; i < sys->eqs.count; i++) {
        PolyEquation *pe = ((PolyEquation *)lv_darray_get(&sys->eqs, i));
        if (pe->poly.degree < 0)
            continue;
        if (dirty_set_contains(ds, pe->var_node_id)) {
            if (equation_system_push(filtered, pe->poly, pe->var_node_id, pe->coord_index) != 0) {
                lv_set_error(lv_ERROR_OUT_OF_MEMORY, "push failed (OOM)");
                return;
            }
        }
    }

    /* 第二遍: 传播 - 收集过滤后方程涉及的所有变量 */
    DirtyVariableSet related;
    dirty_set_init(&related);
    for (int i = 0; i < filtered->eqs.count; i++) {
        PolyEquation *pe = ((PolyEquation *)lv_darray_get(&filtered->eqs, i));
        dirty_set_add(&related, pe->var_node_id);
    }

    /* 第三遍: 添加与相关变量共享同一节点的方程
     * (同一节点的 x 和 y 坐标是耦合的) */
    for (int i = 0; i < sys->eqs.count; i++) {
        PolyEquation *pe = ((PolyEquation *)lv_darray_get(&sys->eqs, i));
        if (pe->poly.degree < 0)
            continue;
        if (dirty_set_contains(&related, pe->var_node_id)) {
            /* 检查是否已在 filtered 中 */
            bool found = false;
            for (int j = 0; j < filtered->eqs.count; j++) {
                PolyEquation *fj = ((PolyEquation *)lv_darray_get(&filtered->eqs, j));
                if (poly_eq_same_key(fj->var_node_id, fj->coord_index,
                                     pe->var_node_id, pe->coord_index)) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                if (equation_system_push(filtered, pe->poly, pe->var_node_id,
                                         pe->coord_index) != 0) {
                    lv_set_error(lv_ERROR_OUT_OF_MEMORY, "push failed (OOM)");
                    dirty_set_free(&related);
                    return;
                }
            }
        }
    }

    dirty_set_free(&related);
}
