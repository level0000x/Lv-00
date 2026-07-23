/**
 * @file solver_dirty_set.c
 * @brief 脏变量追踪（增量求解支持）
 *
 * @details 从 solver.c 拆分出的子模块（Lv-00 项目 v3.3.0+）。
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "lv/constraint_graph.h"
#include "lv/solver.h"

#include "lv_internal.h"
#include "lv_utils.h"
#include "mpz_poly.h"

/* --- 共享宏 --- */
#define lv_SOLVER_DYNARRAY_INIT_CAP 16

/* ── PolyEquation + EquationSystem ── */
typedef struct {
    mpz_poly_t poly;
    int var_node_id;
    int coord_index;
} PolyEquation;

typedef struct EquationSystem {
    PolyEquation *eqs;
    int count;
    int capacity;
} EquationSystem;

/* 脏变量集合结构体 */
typedef struct {
    int *dirty_ids;  /* dirty variable IDs */
    int dirty_count; /* number of dirty variables */
    int capacity;    /* allocated capacity */
} DirtyVariableSet;

/* 前向声明 */
void equation_system_init(EquationSystem *sys);
int equation_system_push(EquationSystem *sys, mpz_poly_t poly, int var_node_id, int coord_index);

/* 初始化脏变量集合 */
static void dirty_set_init(DirtyVariableSet *ds) {
    ds->dirty_ids = NULL;
    ds->dirty_count = 0;
    ds->capacity = 0;
}

/* 检查变量 ID 是否在脏集合中 */
static bool dirty_set_contains(DirtyVariableSet *ds, int var_id) {
    for (int i = 0; i < ds->dirty_count; i++) {
        if (ds->dirty_ids[i] == var_id)
            return true;
    }
    return false;
}

/* 向脏集合中添加变量 ID */
static void dirty_set_add(DirtyVariableSet *ds, int var_id) {
    if (dirty_set_contains(ds, var_id))
        return;
    if (ds->dirty_count >= ds->capacity) {
        int new_cap = ds->capacity == 0 ? lv_SOLVER_DYNARRAY_INIT_CAP : ds->capacity * 2;
        int *new_ids = lv_realloc(ds->dirty_ids, (size_t) new_cap * sizeof(int));
        if (!new_ids)
            return; /* allocation failed, skip */
        ds->dirty_ids = new_ids;
        ds->capacity = new_cap;
    }
    ds->dirty_ids[ds->dirty_count++] = var_id;
}

/* 清空脏变量集合 */
static void dirty_set_clear(DirtyVariableSet *ds) {
    ds->dirty_count = 0;
    /* 不释放内存, 保留容量以供复用 */
}

/* 释放脏变量集合资源 */
static void dirty_set_free(DirtyVariableSet *ds) {
    lv_free((void **) &ds->dirty_ids);
    ds->dirty_ids = NULL;
    ds->dirty_count = 0;
    ds->capacity = 0;
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
static void filter_equations_for_dirty(EquationSystem *sys, DirtyVariableSet *ds, EquationSystem *filtered) {
    equation_system_init(filtered);

    if (!sys || !ds || ds->dirty_count == 0)
        return;

    /* 第一遍: 直接筛选涉及脏变量的方程 */
    for (int i = 0; i < sys->count; i++) {
        if (sys->eqs[i].poly.degree < 0)
            continue;
        if (dirty_set_contains(ds, sys->eqs[i].var_node_id)) {
            if (equation_system_push(filtered, sys->eqs[i].poly, sys->eqs[i].var_node_id, sys->eqs[i].coord_index) != 0) {
                lv_set_error(lv_ERROR_OUT_OF_MEMORY, "push failed (OOM)");
                return;
            }
        }
    }

    /* 第二遍: 传播 - 收集过滤后方程涉及的所有变量 */
    DirtyVariableSet related;
    dirty_set_init(&related);
    for (int i = 0; i < filtered->count; i++) {
        dirty_set_add(&related, filtered->eqs[i].var_node_id);
    }

    /* 第三遍: 添加与相关变量共享同一节点的方程
     * (同一节点的 x 和 y 坐标是耦合的) */
    for (int i = 0; i < sys->count; i++) {
        if (sys->eqs[i].poly.degree < 0)
            continue;
        if (dirty_set_contains(&related, sys->eqs[i].var_node_id)) {
            /* 检查是否已在 filtered 中 */
            bool found = false;
            for (int j = 0; j < filtered->count; j++) {
                if (filtered->eqs[j].var_node_id == sys->eqs[i].var_node_id &&
                    filtered->eqs[j].coord_index == sys->eqs[i].coord_index) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                if (equation_system_push(filtered, sys->eqs[i].poly, sys->eqs[i].var_node_id, sys->eqs[i].coord_index) != 0) {
                    lv_set_error(lv_ERROR_OUT_OF_MEMORY, "push failed (OOM)");
                    dirty_set_free(&related);
                    return;
                }
            }
        }
    }

    dirty_set_free(&related);
}
