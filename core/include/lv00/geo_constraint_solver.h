#ifndef LV00_GEO_CONSTRAINT_SOLVER_H
#define LV00_GEO_CONSTRAINT_SOLVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdbool.h>

#ifndef LV00_PUBLIC_API
#define LV00_PUBLIC_API
#endif

/* ========================================================================
 * 实体类型枚举
 * ======================================================================== */

typedef enum {
    LV00_ENTITY_POINT_2D = 0,
    LV00_ENTITY_POINT_3D,
    LV00_ENTITY_LINE_2D,
    LV00_ENTITY_CIRCLE_2D,
    LV00_ENTITY_SEGMENT_2D,
    LV00_ENTITY_ARC_2D
} Lv00EntityType;

/* ========================================================================
 * 约束类型枚举
 * ======================================================================== */

typedef enum {
    LV00_CONSTRAINT_POINTS_COINCIDENT = 0,
    LV00_CONSTRAINT_PT_PT_DISTANCE,
    LV00_CONSTRAINT_PT_ON_LINE,
    LV00_CONSTRAINT_PT_LINE_DISTANCE,
    LV00_CONSTRAINT_PT_ON_SEGMENT,
    LV00_CONSTRAINT_PT_ON_CIRCLE,
    LV00_CONSTRAINT_PT_PT_MIDPOINT,
    LV00_CONSTRAINT_PARALLEL,
    LV00_CONSTRAINT_PERPENDICULAR,
    LV00_CONSTRAINT_ANGLE,
    LV00_CONSTRAINT_EQUAL_LENGTH,
    LV00_CONSTRAINT_EQUAL_RADIUS,
    LV00_CONSTRAINT_CONCENTRIC,
    LV00_CONSTRAINT_TANGENT,
    LV00_CONSTRAINT_FIXED,
    LV00_CONSTRAINT_HORIZONTAL,
    LV00_CONSTRAINT_VERTICAL
} Lv00ConstraintType;

/* ========================================================================
 * 求解结果枚举
 * ======================================================================== */

typedef enum {
    LV00_SOLVE_OK = 0,
    LV00_SOLVE_FAILED,
    LV00_SOLVE_NOT_CONVERGED,
    LV00_SOLVE_INCONSISTENT
} Lv00SolveResult;

/* ========================================================================
 * 系统状态枚举
 * ======================================================================== */

typedef enum {
    LV00_SYSTEM_UNDER_CONSTRAINED = 0,
    LV00_SYSTEM_WELL_CONSTRAINED,
    LV00_SYSTEM_OVER_CONSTRAINED
} Lv00SystemStatus;

/* ========================================================================
 * 实体结构体
 * ======================================================================== */

#define LV00_MAX_ENTITY_PARAMS 8

typedef struct Lv00Entity {
    int id;
    Lv00EntityType type;
    double params[LV00_MAX_ENTITY_PARAMS];
    int param_count;
    bool is_fixed;
    bool is_dragged;
    double initial_params[LV00_MAX_ENTITY_PARAMS];
} Lv00Entity;

/* ========================================================================
 * 约束结构体
 * ======================================================================== */

typedef struct Lv00Constraint {
    int id;
    Lv00ConstraintType type;
    int entity_a;
    int entity_b;
    int entity_c;
    double value;
    bool is_active;
} Lv00Constraint;

/* ========================================================================
 * 求解器配置结构体
 * ======================================================================== */

typedef struct Lv00SolverConfig {
    int max_iterations;
    double convergence_tol;
    double damping_factor;
    double min_step;
    bool verbose;
} Lv00SolverConfig;

/* ========================================================================
 * 求解器系统结构体（前向声明+不透明指针风格）
 * ======================================================================== */

typedef struct Lv00SolverSystem Lv00SolverSystem;

/* ========================================================================
 * DOF 分析结果结构体
 * ======================================================================== */

typedef struct Lv00DOFAnalysis {
    int total_dof;
    int constraint_dof;
    int remaining_dof;
    int fixed_count;
    int *free_entity_ids;
    int free_entity_count;
    Lv00SystemStatus status;
} Lv00DOFAnalysis;

/* ========================================================================
 * 求解器系统结构体完整定义（因内部分配需要）
 * ======================================================================== */

struct Lv00SolverSystem {
    Lv00Entity *entities;
    int entity_count;
    int entity_capacity;
    Lv00Constraint *constraints;
    int constraint_count;
    int constraint_capacity;
    Lv00SolverConfig config;
    Lv00SolveResult last_result;
    int iteration_count;
};

/* ========================================================================
 * 公共 API 函数声明
 * ======================================================================== */

/* 实体自由度查询 */
LV00_PUBLIC_API int lv00_entity_dof(Lv00EntityType type);

/* 约束自由度查询 */
LV00_PUBLIC_API int lv00_constraint_dof(Lv00ConstraintType type);

/* 求解器配置 */
LV00_PUBLIC_API Lv00SolverConfig lv00_solver_default_config(void);

/* 求解器系统创建与释放 */
LV00_PUBLIC_API Lv00SolverSystem *lv00_solver_create(const Lv00SolverConfig *config);
LV00_PUBLIC_API void lv00_solver_destroy(Lv00SolverSystem *sys);

/* 实体管理 */
LV00_PUBLIC_API int lv00_solver_add_entity(Lv00SolverSystem *sys, const Lv00Entity *entity);
LV00_PUBLIC_API Lv00Entity *lv00_solver_get_entity(Lv00SolverSystem *sys, int id);

/* 约束管理 */
LV00_PUBLIC_API int lv00_solver_add_constraint(Lv00SolverSystem *sys, const Lv00Constraint *c);
LV00_PUBLIC_API Lv00Constraint *lv00_solver_get_constraint(Lv00SolverSystem *sys, int id);
LV00_PUBLIC_API bool lv00_solver_remove_constraint(Lv00SolverSystem *sys, int id);

/* 求解核心 */
LV00_PUBLIC_API Lv00SolveResult lv00_solver_solve(Lv00SolverSystem *sys);

/* DOF 分析 */
LV00_PUBLIC_API Lv00DOFAnalysis *lv00_solver_dof_analyze(const Lv00SolverSystem *sys);
LV00_PUBLIC_API void lv00_dof_analysis_destroy(Lv00DOFAnalysis *analysis);

/* 系统状态查询 */
LV00_PUBLIC_API Lv00SystemStatus lv00_solver_get_status(const Lv00SolverSystem *sys);
LV00_PUBLIC_API int lv00_solver_get_iteration_count(const Lv00SolverSystem *sys);

/* 交互支持 */
LV00_PUBLIC_API void lv00_solver_set_fixed(Lv00SolverSystem *sys, int entity_id, bool fixed);
LV00_PUBLIC_API void lv00_solver_set_dragged(Lv00SolverSystem *sys, int entity_id, bool dragged);
LV00_PUBLIC_API void lv00_solver_set_drag_position(
    Lv00SolverSystem *sys, int entity_id, double x, double y);

/* 便捷函数 —— 快速创建实体 */
LV00_PUBLIC_API Lv00Entity lv00_entity_point_2d(int id, double x, double y);
LV00_PUBLIC_API Lv00Entity lv00_entity_line_2d(int id, double x1, double y1, double x2, double y2);
LV00_PUBLIC_API Lv00Entity lv00_entity_circle_2d(int id, double cx, double cy, double r);
LV00_PUBLIC_API Lv00Entity lv00_entity_segment_2d(int id, double x1, double y1, double x2, double y2);

/* 便捷函数 —— 快速创建约束 */
LV00_PUBLIC_API Lv00Constraint lv00_constraint_coincident(int id, int entity_a, int entity_b);
LV00_PUBLIC_API Lv00Constraint lv00_constraint_distance(int id, int entity_a, int entity_b, double dist);
LV00_PUBLIC_API Lv00Constraint lv00_constraint_parallel(int id, int entity_a, int entity_b);
LV00_PUBLIC_API Lv00Constraint lv00_constraint_perpendicular(int id, int entity_a, int entity_b);
LV00_PUBLIC_API Lv00Constraint lv00_constraint_angle(int id, int entity_a, int entity_b, double angle_rad);
LV00_PUBLIC_API Lv00Constraint lv00_constraint_on_circle(int id, int point_entity, int circle_entity);
LV00_PUBLIC_API Lv00Constraint lv00_constraint_fixed(int id, int entity);
LV00_PUBLIC_API Lv00Constraint lv00_constraint_equal_length(int id, int entity_a, int entity_b);
LV00_PUBLIC_API Lv00Constraint lv00_constraint_horizontal(int id, int entity);
LV00_PUBLIC_API Lv00Constraint lv00_constraint_vertical(int id, int entity);

/* 兼容旧 API */
LV00_PUBLIC_API int lv00_solve_constraints(const Lv00Constraint *constraints, size_t count, double *points, size_t n_points);

#ifdef __cplusplus
}
#endif

#endif
