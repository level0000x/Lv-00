#ifndef lv_GEO_CONSTRAINT_SOLVER_H
#define lv_GEO_CONSTRAINT_SOLVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include "lv_api_spec.h" /* lv_PUBLIC_API（K59） */

#ifndef lv_PUBLIC_API
#define lv_PUBLIC_API
#endif

/* ========================================================================
 * 实体类型枚举
 * ======================================================================== */

typedef enum {
    lv_ENTITY_POINT_2D = 0,
    lv_ENTITY_POINT_3D,
    lv_ENTITY_LINE_2D,
    lv_ENTITY_CIRCLE_2D,
    lv_ENTITY_SEGMENT_2D,
    lv_ENTITY_ARC_2D
} lvEntityType;

/* ========================================================================
 * 约束类型枚举
 * ======================================================================== */

typedef enum {
    lv_CONSTRAINT_POINTS_COINCIDENT = 0,
    lv_CONSTRAINT_PT_PT_DISTANCE,
    lv_CONSTRAINT_PT_ON_LINE,
    lv_CONSTRAINT_PT_LINE_DISTANCE,
    lv_CONSTRAINT_PT_ON_SEGMENT,
    lv_CONSTRAINT_PT_ON_CIRCLE,
    lv_CONSTRAINT_PT_PT_MIDPOINT,
    lv_CONSTRAINT_PARALLEL,
    lv_CONSTRAINT_PERPENDICULAR,
    lv_CONSTRAINT_ANGLE,
    lv_CONSTRAINT_EQUAL_LENGTH,
    lv_CONSTRAINT_EQUAL_RADIUS,
    lv_CONSTRAINT_CONCENTRIC,
    lv_CONSTRAINT_TANGENT,
    lv_CONSTRAINT_FIXED,
    lv_CONSTRAINT_HORIZONTAL,
    lv_CONSTRAINT_VERTICAL
} lvConstraintType;

/* ========================================================================
 * 求解结果枚举
 * ======================================================================== */

typedef enum { lv_SOLVE_OK = 0, lv_SOLVE_FAILED, lv_SOLVE_NOT_CONVERGED, lv_SOLVE_INCONSISTENT } lvSolveResult;

/* ========================================================================
 * 系统状态枚举
 * ======================================================================== */

typedef enum { lv_SYSTEM_UNDER_CONSTRAINED = 0, lv_SYSTEM_WELL_CONSTRAINED, lv_SYSTEM_OVER_CONSTRAINED } lvSystemStatus;

/* ========================================================================
 * 实体结构体
 * ======================================================================== */

#define lv_MAX_ENTITY_PARAMS 8

typedef struct lvEntity {
    int id;
    lvEntityType type;
    double params[lv_MAX_ENTITY_PARAMS];
    int param_count;
    bool is_fixed;
    bool is_dragged;
    double initial_params[lv_MAX_ENTITY_PARAMS];
} lvEntity;

/* ========================================================================
 * 约束结构体
 * ======================================================================== */

typedef struct lvConstraint {
    int id;
    lvConstraintType type;
    int entity_a;
    int entity_b;
    int entity_c;
    double value;
    bool is_active;
} lvConstraint;

/* ========================================================================
 * 求解器配置结构体
 * ======================================================================== */

typedef struct lvSolverConfig {
    int max_iterations;
    double convergence_tol;
    double damping_factor;
    double min_step;
    bool verbose;
} lvSolverConfig;

/* ========================================================================
 * 求解器系统结构体（前向声明+不透明指针风格）
 * ======================================================================== */

typedef struct lvSolverSystem lvSolverSystem;

/* ========================================================================
 * DOF 分析结果结构体
 * ======================================================================== */

typedef struct lvDOFAnalysis {
    int total_dof;
    int constraint_dof;
    int remaining_dof;
    int fixed_count;
    int *free_entity_ids;
    int free_entity_count;
    lvSystemStatus status;
} lvDOFAnalysis;

/* ========================================================================
 * 求解器系统结构体完整定义（因内部分配需要）
 * ======================================================================== */

struct lvSolverSystem {
    lvEntity *entities;
    int entity_count;
    int entity_capacity;
    lvConstraint *constraints;
    int constraint_count;
    int constraint_capacity;
    lvSolverConfig config;
    lvSolveResult last_result;
    int iteration_count;
};

/* ========================================================================
 * 公共 API 函数声明
 * ======================================================================== */

/* 实体自由度查询 */
lv_PUBLIC_API int lv_entity_dof(lvEntityType type);

/* 约束自由度查询 */
lv_PUBLIC_API int lv_constraint_dof(lvConstraintType type);

/* 求解器配置 */
lv_PUBLIC_API lvSolverConfig lv_solver_default_config(void);

/* 求解器系统创建与释放 */
lv_PUBLIC_API lvSolverSystem *lv_geo_solver_create(const lvSolverConfig *config);
lv_PUBLIC_API void lv_geo_solver_destroy(lvSolverSystem *sys);

/* 实体管理 */
lv_PUBLIC_API int lv_solver_add_entity(lvSolverSystem *sys, const lvEntity *entity);
lv_PUBLIC_API lvEntity *lv_solver_get_entity(lvSolverSystem *sys, int id);

/* 约束管理 */
lv_PUBLIC_API int lv_geo_solver_add_constraint(lvSolverSystem *sys, const lvConstraint *c);
lv_PUBLIC_API lvConstraint *lv_solver_get_constraint(lvSolverSystem *sys, int id);
lv_PUBLIC_API bool lv_geo_solver_remove_constraint(lvSolverSystem *sys, int id);

/* 求解核心 */
lv_PUBLIC_API lvSolveResult lv_geo_solver_solve(lvSolverSystem *sys);

/* DOF 分析 */
lv_PUBLIC_API lvDOFAnalysis *lv_solver_dof_analyze(const lvSolverSystem *sys);
lv_PUBLIC_API void lv_dof_analysis_destroy(lvDOFAnalysis *analysis);

/* 系统状态查询 */
lv_PUBLIC_API lvSystemStatus lv_solver_get_status(const lvSolverSystem *sys);
lv_PUBLIC_API int lv_solver_get_iteration_count(const lvSolverSystem *sys);

/* 交互支持 */
lv_PUBLIC_API void lv_solver_set_fixed(lvSolverSystem *sys, int entity_id, bool fixed);
lv_PUBLIC_API void lv_solver_set_dragged(lvSolverSystem *sys, int entity_id, bool dragged);
lv_PUBLIC_API void lv_solver_set_drag_position(lvSolverSystem *sys, int entity_id, double x, double y);

/* 便捷函数 —— 快速创建实体 */
lv_PUBLIC_API lvEntity lv_entity_point_2d(int id, double x, double y);
lv_PUBLIC_API lvEntity lv_entity_line_2d(int id, double x1, double y1, double x2, double y2);
lv_PUBLIC_API lvEntity lv_entity_circle_2d(int id, double cx, double cy, double r);
lv_PUBLIC_API lvEntity lv_entity_segment_2d(int id, double x1, double y1, double x2, double y2);

/* 便捷函数 —— 快速创建约束 */
lv_PUBLIC_API lvConstraint lv_constraint_coincident(int id, int entity_a, int entity_b);
lv_PUBLIC_API lvConstraint lv_constraint_distance(int id, int entity_a, int entity_b, double dist);
lv_PUBLIC_API lvConstraint lv_constraint_parallel(int id, int entity_a, int entity_b);
lv_PUBLIC_API lvConstraint lv_constraint_perpendicular(int id, int entity_a, int entity_b);
lv_PUBLIC_API lvConstraint lv_constraint_angle(int id, int entity_a, int entity_b, double angle_rad);
lv_PUBLIC_API lvConstraint lv_constraint_on_circle(int id, int point_entity, int circle_entity);
lv_PUBLIC_API lvConstraint lv_constraint_fixed(int id, int entity);
lv_PUBLIC_API lvConstraint lv_constraint_equal_length(int id, int entity_a, int entity_b);
lv_PUBLIC_API lvConstraint lv_constraint_horizontal(int id, int entity);
lv_PUBLIC_API lvConstraint lv_constraint_vertical(int id, int entity);

/* 兼容旧 API */
lv_PUBLIC_API int lv_solve_constraints(const lvConstraint *constraints, size_t count, double *points, size_t n_points);

#ifdef __cplusplus
}
#endif

#endif
