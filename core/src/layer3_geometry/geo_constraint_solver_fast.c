/**
 * @file geo_constraint_solver_fast.c
 * @brief 几何约束求解器 —— 便捷函数 —— 快速创建实体与约束
 */

#include "geo_constraint_solver_internal.h"

#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ========================================================================
 * 第十四部分：便捷函数 —— 快速创建实体
 * ======================================================================== */

/**
 * @brief 创建 2D 点实体
 */
lv_PUBLIC_API lvEntity lv_entity_point_2d(int id, double x, double y) {
    lvEntity e;
    memset(&e, 0, sizeof(e));
    e.type = lv_ENTITY_POINT_2D;
    e.id = id;
    e.params[0] = x;
    e.params[1] = y;
    e.param_count = 2;
    e.is_fixed = false;
    e.is_dragged = false;
    return e;
}

/**
 * @brief 创建 2D 直线实体（过两点的直线）
 */
lv_PUBLIC_API lvEntity lv_entity_line_2d(int id, double x1, double y1, double x2, double y2) {
    lvEntity e;
    memset(&e, 0, sizeof(e));
    e.type = lv_ENTITY_LINE_2D;
    e.id = id;
    e.params[0] = x1;
    e.params[1] = y1;
    e.params[2] = x2;
    e.params[3] = y2;
    e.param_count = 4;
    e.is_fixed = false;
    e.is_dragged = false;
    return e;
}

/**
 * @brief 创建 2D 圆实体
 */
lv_PUBLIC_API lvEntity lv_entity_circle_2d(int id, double cx, double cy, double r) {
    lvEntity e;
    memset(&e, 0, sizeof(e));
    e.type = lv_ENTITY_CIRCLE_2D;
    e.id = id;
    e.params[0] = cx;
    e.params[1] = cy;
    e.params[2] = r;
    e.param_count = 3;
    e.is_fixed = false;
    e.is_dragged = false;
    return e;
}

/**
 * @brief 创建 2D 线段实体
 */
lv_PUBLIC_API lvEntity lv_entity_segment_2d(int id, double x1, double y1, double x2, double y2) {
    lvEntity e;
    memset(&e, 0, sizeof(e));
    e.type = lv_ENTITY_SEGMENT_2D;
    e.id = id;
    e.params[0] = x1;
    e.params[1] = y1;
    e.params[2] = x2;
    e.params[3] = y2;
    e.param_count = 4;
    e.is_fixed = false;
    e.is_dragged = false;
    return e;
}

/* ========================================================================
 * 第十五部分：便捷函数 —— 快速创建约束
 * ======================================================================== */

/**
 * @brief 创建两点重合约束
 */
lv_PUBLIC_API lvConstraint lv_constraint_coincident(int id, int entity_a, int entity_b) {
    lvConstraint c;
    memset(&c, 0, sizeof(c));
    c.type = lv_CONSTRAINT_POINTS_COINCIDENT;
    c.id = id;
    c.entity_a = entity_a;
    c.entity_b = entity_b;
    c.entity_c = -1;
    c.value = 0.0;
    c.is_active = true;
    return c;
}

/* ========================================================================
 * 兼容旧 API：lv_solve_constraints
 * ======================================================================== */

lv_PUBLIC_API int lv_solve_constraints(const lvConstraint *constraints, size_t count, double *points, size_t n_points) {
    if (!constraints || count == 0 || !points || n_points == 0) {
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "lv_solve_constraints: NULL or empty params");
    }

    lvSolverConfig config = lv_solver_default_config();
    lvSolverSystem *sys = lv_geo_solver_create(&config);
    if (!sys) {
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "lv_solve_constraints: solver creation failed");
    }

    /* 为每个点创建实体 */
    for (size_t i = 0; i < n_points; i++) {
        lvEntity e = lv_entity_point_2d((int) i, points[i * 2], points[i * 2 + 1]);
        if (lv_solver_add_entity(sys, &e) < 0) {
            lv_geo_solver_destroy(sys);
            lv_RETURN_ERROR(lv_ERROR_NODE_CONFLICT, "lv_solve_constraints: add entity failed");
        }
    }

    /* 添加所有约束 */
    for (size_t i = 0; i < count; i++) {
        if (lv_geo_solver_add_constraint(sys, &constraints[i]) < 0) {
            lv_geo_solver_destroy(sys);
            lv_RETURN_ERROR(lv_ERROR_NODE_CONFLICT, "lv_solve_constraints: add constraint failed");
        }
    }

    /* 求解 */
    lvSolveResult result = lv_geo_solver_solve(sys);
    if (result != lv_SOLVE_OK) {
        lv_geo_solver_destroy(sys);
        return -2;
    }

    /* 提取求解后的坐标 */
    for (size_t i = 0; i < n_points; i++) {
        lvEntity *entity = lv_solver_get_entity(sys, (int) i);
        if (entity && entity->param_count >= 2) {
            points[i * 2] = entity->params[0];
            points[i * 2 + 1] = entity->params[1];
        }
    }

    lv_geo_solver_destroy(sys);
    return (int) n_points;
}

/**
 * @brief 创建点点距离约束
 */
lv_PUBLIC_API lvConstraint lv_constraint_distance(int id, int entity_a, int entity_b, double dist) {
    lvConstraint c;
    memset(&c, 0, sizeof(c));
    c.type = lv_CONSTRAINT_PT_PT_DISTANCE;
    c.id = id;
    c.entity_a = entity_a;
    c.entity_b = entity_b;
    c.entity_c = -1;
    c.value = dist;
    c.is_active = true;
    return c;
}

/**
 * @brief 创建平行约束
 */
lv_PUBLIC_API lvConstraint lv_constraint_parallel(int id, int entity_a, int entity_b) {
    lvConstraint c;
    memset(&c, 0, sizeof(c));
    c.type = lv_CONSTRAINT_PARALLEL;
    c.id = id;
    c.entity_a = entity_a;
    c.entity_b = entity_b;
    c.entity_c = -1;
    c.value = 0.0;
    c.is_active = true;
    return c;
}

/**
 * @brief 创建垂直约束
 */
lv_PUBLIC_API lvConstraint lv_constraint_perpendicular(int id, int entity_a, int entity_b) {
    lvConstraint c;
    memset(&c, 0, sizeof(c));
    c.type = lv_CONSTRAINT_PERPENDICULAR;
    c.id = id;
    c.entity_a = entity_a;
    c.entity_b = entity_b;
    c.entity_c = -1;
    c.value = 0.0;
    c.is_active = true;
    return c;
}

/**
 * @brief 创建角度约束
 */
lv_PUBLIC_API lvConstraint lv_constraint_angle(int id, int entity_a, int entity_b, double angle_rad) {
    lvConstraint c;
    memset(&c, 0, sizeof(c));
    c.type = lv_CONSTRAINT_ANGLE;
    c.id = id;
    c.entity_a = entity_a;
    c.entity_b = entity_b;
    c.entity_c = -1;
    c.value = angle_rad;
    c.is_active = true;
    return c;
}

/**
 * @brief 创建点在圆上约束
 */
lv_PUBLIC_API lvConstraint lv_constraint_on_circle(int id, int point_entity, int circle_entity) {
    lvConstraint c;
    memset(&c, 0, sizeof(c));
    c.type = lv_CONSTRAINT_PT_ON_CIRCLE;
    c.id = id;
    c.entity_a = point_entity;
    c.entity_b = circle_entity;
    c.entity_c = -1;
    c.value = 0.0;
    c.is_active = true;
    return c;
}

/**
 * @brief 创建固定约束
 */
lv_PUBLIC_API lvConstraint lv_constraint_fixed(int id, int entity) {
    lvConstraint c;
    memset(&c, 0, sizeof(c));
    c.type = lv_CONSTRAINT_FIXED;
    c.id = id;
    c.entity_a = entity;
    c.entity_b = -1;
    c.entity_c = -1;
    c.value = 0.0;
    c.is_active = true;
    return c;
}

/**
 * @brief 创建等长约束
 */
lv_PUBLIC_API lvConstraint lv_constraint_equal_length(int id, int entity_a, int entity_b) {
    lvConstraint c;
    memset(&c, 0, sizeof(c));
    c.type = lv_CONSTRAINT_EQUAL_LENGTH;
    c.id = id;
    c.entity_a = entity_a;
    c.entity_b = entity_b;
    c.entity_c = -1;
    c.value = 0.0;
    c.is_active = true;
    return c;
}

/**
 * @brief 创建水平约束
 */
lv_PUBLIC_API lvConstraint lv_constraint_horizontal(int id, int entity) {
    lvConstraint c;
    memset(&c, 0, sizeof(c));
    c.type = lv_CONSTRAINT_HORIZONTAL;
    c.id = id;
    c.entity_a = entity;
    c.entity_b = -1;
    c.entity_c = -1;
    c.value = 0.0;
    c.is_active = true;
    return c;
}

/**
 * @brief 创建垂直约束（线段垂直方向）
 */
lv_PUBLIC_API lvConstraint lv_constraint_vertical(int id, int entity) {
    lvConstraint c;
    memset(&c, 0, sizeof(c));
    c.type = lv_CONSTRAINT_VERTICAL;
    c.id = id;
    c.entity_a = entity;
    c.entity_b = -1;
    c.entity_c = -1;
    c.value = 0.0;
    c.is_active = true;
    return c;
}
