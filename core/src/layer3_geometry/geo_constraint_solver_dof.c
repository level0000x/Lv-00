/**
 * @file geo_constraint_solver_dof.c
 * @brief 几何约束求解器 —— 自由度分析与系统状态查询
 */

#include "geo_constraint_solver_internal.h"

#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ========================================================================
 * 第二部分：实体自由度查询
 * ======================================================================== */

/**
 * @brief 获取实体类型的自由度数量
 *
 * 返回值说明：
 *   - POINT_2D: 2 (x, y)
 *   - POINT_3D: 3 (x, y, z)
 *   - LINE_2D:  4 (ax, ay, bx, by)
 *   - CIRCLE_2D: 3 (cx, cy, r)
 *   - SEGMENT_2D: 4 (x1, y1, x2, y2)
 *   - ARC_2D: 5 (cx, cy, r, start_angle, sweep)
 */
lv_PUBLIC_API int lv_entity_dof(lvEntityType type) {
    static const int dof_map[] = {
        [lv_ENTITY_POINT_2D]   = 2,
        [lv_ENTITY_POINT_3D]   = 3,
        [lv_ENTITY_LINE_2D]    = 4,
        [lv_ENTITY_CIRCLE_2D]  = 3,
        [lv_ENTITY_SEGMENT_2D] = 4,
        [lv_ENTITY_ARC_2D]     = 5,
    };
    if (type >= 0 && type < (int)(sizeof(dof_map)/sizeof(dof_map[0])))
        return dof_map[type];
    return 0;
}

/* ========================================================================
 * 第二部分：约束自由度查询
 * ======================================================================== */

/**
 * @brief 获取约束类型消耗的自由度数量
 *
 * 返回值说明：
 *   - POINTS_COINCIDENT: 2（消除两个平移自由度）
 *   - PT_PT_DISTANCE: 1（消除一个距离自由度）
 *   - PT_ON_LINE: 1
 *   - PT_LINE_DISTANCE: 1
 *   - PT_ON_SEGMENT: 1
 *   - PT_ON_CIRCLE: 1
 *   - PT_PT_MIDPOINT: 2
 *   - PARALLEL: 1
 *   - PERPENDICULAR: 1
 *   - ANGLE: 1
 *   - EQUAL_LENGTH: 1
 *   - EQUAL_RADIUS: 1
 *   - CONCENTRIC: 2
 *   - TANGENT: 1
 *   - FIXED: 消除实体全部自由度（此处返回 -1 表示特殊处理）
 *   - HORIZONTAL: 1
 *   - VERTICAL: 1
 */
lv_PUBLIC_API int lv_constraint_dof(lvConstraintType type) {
    static const int dof_map[] = {
        [lv_CONSTRAINT_POINTS_COINCIDENT]  = 2,
        [lv_CONSTRAINT_PT_PT_DISTANCE]     = 1,
        [lv_CONSTRAINT_PT_ON_LINE]         = 1,
        [lv_CONSTRAINT_PT_LINE_DISTANCE]   = 1,
        [lv_CONSTRAINT_PT_ON_SEGMENT]      = 1,
        [lv_CONSTRAINT_PT_ON_CIRCLE]       = 1,
        [lv_CONSTRAINT_PT_PT_MIDPOINT]     = 2,
        [lv_CONSTRAINT_PARALLEL]           = 1,
        [lv_CONSTRAINT_PERPENDICULAR]      = 1,
        [lv_CONSTRAINT_ANGLE]              = 1,
        [lv_CONSTRAINT_EQUAL_LENGTH]       = 1,
        [lv_CONSTRAINT_EQUAL_RADIUS]       = 1,
        [lv_CONSTRAINT_CONCENTRIC]         = 2,
        [lv_CONSTRAINT_TANGENT]            = 1,
        [lv_CONSTRAINT_FIXED]              = -1, /* 特殊：消除全部自由度 */
        [lv_CONSTRAINT_HORIZONTAL]         = 1,
        [lv_CONSTRAINT_VERTICAL]           = 1,
    };
    if (type >= 0 && type < (int)(sizeof(dof_map)/sizeof(dof_map[0])))
        return dof_map[type];
    return 0;
}


/* ========================================================================
 * 第十一部分：DOF 分析
 * ======================================================================== */

/**
 * @brief DOF（自由度）分析
 *
 * 统计所有实体的自由度之和，减去约束消耗的自由度。
 * FIXED 约束特殊处理：消除对应实体的全部自由度。
 *
 * @return DOF 分析结果（需用 lv_dof_analysis_destroy 释放）
 */
lv_PUBLIC_API lvDOFAnalysis *lv_solver_dof_analyze(const lvSolverSystem *sys) {
    if (!sys)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "lv_solver_dof_analyze: NULL sys");

    lvDOFAnalysis *analysis = (lvDOFAnalysis *) lv_calloc(1, sizeof(lvDOFAnalysis));
    if (!analysis)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "lv_solver_dof_analyze: calloc failed");

    /* 统计总自由度 */
    int total_dof = 0;
    int fixed_count = 0;

    for (int i = 0; i < sys->entity_count; i++) {
        const lvEntity *e = &sys->entities[i];
        if (e->is_fixed) {
            fixed_count++;
        } else {
            total_dof += lv_entity_dof(e->type);
        }
    }

    /* 统计约束消耗的自由度 */
    int constraint_dof = 0;
    for (int i = 0; i < sys->constraint_count; i++) {
        const lvConstraint *c = &sys->constraints[i];
        if (!c->is_active)
            continue;

        int cdof = lv_constraint_dof(c->type);
        if (cdof == -1) {
            /* FIXED 约束：目标实体的 DOF 已通过 is_fixed 在 total_dof 中排除，跳过 */
            continue;
        } else {
            constraint_dof += cdof;
        }
    }

    analysis->total_dof = total_dof;
    analysis->constraint_dof = constraint_dof;
    analysis->remaining_dof = total_dof - constraint_dof;
    analysis->fixed_count = fixed_count;

    /* 确定自由实体 */
    int free_cap = sys->entity_count;
    analysis->free_entity_ids = (int *) lv_malloc(free_cap * sizeof(int));
    analysis->free_entity_count = 0;

    if (analysis->free_entity_ids) {
        for (int i = 0; i < sys->entity_count; i++) {
            if (!sys->entities[i].is_fixed) {
                analysis->free_entity_ids[analysis->free_entity_count] = sys->entities[i].id;
                analysis->free_entity_count++;
            }
        }
    }

    /* 确定系统状态 */
    if (analysis->remaining_dof > 0) {
        analysis->status = lv_SYSTEM_UNDER_CONSTRAINED;
    } else if (analysis->remaining_dof == 0) {
        analysis->status = lv_SYSTEM_WELL_CONSTRAINED;
    } else {
        analysis->status = lv_SYSTEM_OVER_CONSTRAINED;
    }

    return analysis;
}

/**
 * @brief 释放 DOF 分析结果
 */
lv_PUBLIC_API void lv_dof_analysis_destroy(lvDOFAnalysis *analysis) {
    if (!analysis)
        return;
    lv_free((void **) &(analysis->free_entity_ids));
    lv_free((void **) &(analysis));
}

/* ========================================================================
 * 第十二部分：系统状态查询
 * ======================================================================== */

/**
 * @brief 获取系统约束状态
 */
lv_PUBLIC_API lvSystemStatus lv_solver_get_status(const lvSolverSystem *sys) {
    if (!sys)
        return lv_SYSTEM_UNDER_CONSTRAINED;

    lvDOFAnalysis *analysis = lv_solver_dof_analyze(sys);
    if (!analysis)
        return lv_SYSTEM_UNDER_CONSTRAINED;

    lvSystemStatus status = analysis->status;
    lv_dof_analysis_destroy(analysis);
    return status;
}

/**
 * @brief 获取上次求解的迭代次数
 */
lv_PUBLIC_API int lv_solver_get_iteration_count(const lvSolverSystem *sys) {
    if (!sys)
        return 0;
    return sys->iteration_count;
}

