/**
 * @file geo_constraint_solver.h
 * @brief 几何约束求解器 —— 借鉴 SolveSpace Newton-Raphson 求解架构
 *
 * 借鉴来源：
 *   - SolveSpace (github.com/solvespace/solvespace)
 *     Newton-Raphson 迭代求解、DOF 自由度分析、实时拖拽反馈
 *
 * 设计目标：
 *   - 轻量级约束求解器（借鉴 SolveSpace 的 5000 行核心设计）
 *   - 支持点、线、圆等几何实体之间的约束关系
 *   - DOF（自由度）分析自动检测欠约束/过约束状态
 *   - 实时求解反馈支持交互式拖拽
 *   - 与 Lv-00 约束图系统无缝集成
 *
 * 版本：v3.6.0（第十三梯队 SolveSpace 落地）
 */

#ifndef LV00_GEO_CONSTRAINT_SOLVER_H
#define LV00_GEO_CONSTRAINT_SOLVER_H

#include <stdbool.h>
#include <stdint.h>

#ifndef LV00_PUBLIC_API
#define LV00_PUBLIC_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 第一部分：几何实体类型（借鉴 SolveSpace Entity 系统）
 * ======================================================================== */

/**
 * @brief 几何实体类型
 */
typedef enum {
    LV00_ENTITY_POINT_2D,      /**< 2D 点（2 个自由度：x, y） */
    LV00_ENTITY_POINT_3D,      /**< 3D 点（3 个自由度：x, y, z） */
    LV00_ENTITY_LINE_2D,       /**< 2D 直线（4 个参数：ax, ay, bx, by） */
    LV00_ENTITY_CIRCLE_2D,     /**< 2D 圆（3 个参数：cx, cy, r） */
    LV00_ENTITY_SEGMENT_2D,    /**< 2D 线段（4 个参数：x1, y1, x2, y2） */
    LV00_ENTITY_ARC_2D,        /**< 2D 弧（5 个参数：cx, cy, r, start_angle, sweep） */
    LV00_ENTITY_NONE           /**< 无效实体 */
} Lv00EntityType;

/**
 * @brief 几何实体
 */
typedef struct {
    Lv00EntityType type;        /**< 实体类型 */
    int id;                     /**< 实体 ID（唯一标识） */
    double params[8];           /**< 参数数组（含义取决于类型） */
    double initial_params[8];   /**< 初始参数（FIXED 约束保存初始位置） */
    int param_count;            /**< 参数数量 */
    bool is_fixed;              /**< 是否固定（固定实体不参与求解） */
    bool is_dragged;            /**< 是否正在被拖拽 */
} Lv00Entity;

/**
 * @brief 获取实体类型的自由度数量
 */
LV00_PUBLIC_API int lv00_entity_dof(Lv00EntityType type);

/* ========================================================================
 * 第二部分：约束类型（借鉴 SolveSpace Constraint 系统）
 * ======================================================================== */

/**
 * @brief 约束类型
 */
typedef enum {
    /* 点-点约束 */
    LV00_CONSTRAINT_POINTS_COINCIDENT,   /**< 两点重合 */
    LV00_CONSTRAINT_PT_PT_DISTANCE,      /**< 点点距离 */

    /* 点-线约束 */
    LV00_CONSTRAINT_PT_ON_LINE,          /**< 点在直线上 */
    LV00_CONSTRAINT_PT_LINE_DISTANCE,    /**< 点线距离 */
    LV00_CONSTRAINT_PT_ON_SEGMENT,       /**< 点在线段上 */

    /* 点-圆约束 */
    LV00_CONSTRAINT_PT_ON_CIRCLE,        /**< 点在圆上 */
    LV00_CONSTRAINT_PT_PT_MIDPOINT,      /**< 第三点为前两点中点 */

    /* 线-线约束 */
    LV00_CONSTRAINT_PARALLEL,            /**< 两线平行 */
    LV00_CONSTRAINT_PERPENDICULAR,       /**< 两线垂直 */
    LV00_CONSTRAINT_ANGLE,               /**< 两线角度 */
    LV00_CONSTRAINT_EQUAL_LENGTH,        /**< 两线段等长 */

    /* 圆约束 */
    LV00_CONSTRAINT_EQUAL_RADIUS,        /**< 两圆等半径 */
    LV00_CONSTRAINT_CONCENTRIC,          /**< 两圆同心 */
    LV00_CONSTRAINT_TANGENT,             /**< 线与圆相切 */

    /* 通用约束 */
    LV00_CONSTRAINT_FIXED,               /**< 固定实体位置 */
    LV00_CONSTRAINT_HORIZONTAL,          /**< 线段水平 */
    LV00_CONSTRAINT_VERTICAL,            /**< 线段垂直 */

    LV00_CONSTRAINT_NONE                 /**< 无效约束 */
} Lv00ConstraintType;

/**
 * @brief 约束
 */
typedef struct {
    Lv00ConstraintType type;    /**< 约束类型 */
    int id;                     /**< 约束 ID */
    int entity_a;               /**< 第一个实体 ID */
    int entity_b;               /**< 第二个实体 ID（-1 表示无） */
    int entity_c;               /**< 第三个实体 ID（-1 表示无） */
    double value;               /**< 约束值（距离/角度/半径等） */
    bool is_active;             /**< 是否激活 */
} Lv00Constraint;

/**
 * @brief 获取约束类型消耗的自由度数量
 */
LV00_PUBLIC_API int lv00_constraint_dof(Lv00ConstraintType type);

/* ========================================================================
 * 第三部分：求解结果与状态（借鉴 SolveSpace SolveResult）
 * ======================================================================== */

/**
 * @brief 求解结果状态
 */
typedef enum {
    LV00_SOLVE_OK,                  /**< 成功求解 */
    LV00_SOLVE_INCONSISTENT,        /**< 约束矛盾（无解） */
    LV00_SOLVE_REDUNDANT_OK,        /**< 约束冗余但可解 */
    LV00_SOLVE_REDUNDANT_FAIL,      /**< 约束冗余且失败 */
    LV00_SOLVE_FAILED,              /**< 求解失败（数值问题） */
    LV00_SOLVE_NOT_CONVERGED        /**< 未收敛（达到最大迭代次数） */
} Lv00SolveResult;

/**
 * @brief 系统约束状态
 */
typedef enum {
    LV00_SYSTEM_UNDER_CONSTRAINED,  /**< 欠约束（DOF > 0） */
    LV00_SYSTEM_WELL_CONSTRAINED,   /**< 恰好约束（DOF = 0） */
    LV00_SYSTEM_OVER_CONSTRAINED,   /**< 过约束（DOF < 0） */
    LV00_SYSTEM_INCONSISTENT        /**< 约束矛盾 */
} Lv00SystemStatus;

/**
 * @brief DOF（自由度）分析结果
 */
typedef struct {
    int total_dof;              /**< 总自由度（所有实体自由度之和） */
    int constraint_dof;         /**< 约束消耗的自由度 */
    int remaining_dof;          /**< 剩余自由度 */
    int fixed_count;            /**< 固定实体数量 */
    int free_entity_count;      /**< 自由实体数量 */
    int *free_entity_ids;       /**< 自由实体 ID 数组 */
    Lv00SystemStatus status;    /**< 系统状态 */
} Lv00DOFAnalysis;

/* ========================================================================
 * 第四部分：求解器配置
 * ======================================================================== */

/**
 * @brief 求解器配置
 */
typedef struct {
    int max_iterations;         /**< 最大迭代次数（默认 50） */
    double convergence_tol;     /**< 收敛容差（默认 1e-10） */
    double damping_factor;      /**< 阻尼因子（默认 0.8） */
    double min_step;            /**< 最小步长（默认 1e-15） */
    bool verbose;               /**< 是否输出调试信息 */
} Lv00SolverConfig;

/**
 * @brief 获取默认求解器配置
 */
LV00_PUBLIC_API Lv00SolverConfig lv00_solver_default_config(void);

/* ========================================================================
 * 第五部分：求解器系统 API
 * ======================================================================== */

/**
 * @brief 约束求解系统
 */
typedef struct Lv00SolverSystem {
    Lv00Entity *entities;           /**< 实体数组 */
    int entity_count;               /**< 实体数量 */
    int entity_capacity;            /**< 实体容量 */

    Lv00Constraint *constraints;    /**< 约束数组 */
    int constraint_count;           /**< 约束数量 */
    int constraint_capacity;        /**< 约束容量 */

    Lv00SolverConfig config;        /**< 求解器配置 */
    Lv00SolveResult last_result;    /**< 上次求解结果 */
    int iteration_count;            /**< 上次求解迭代次数 */
} Lv00SolverSystem;

/**
 * @brief 创建约束求解系统
 * @param config 配置（NULL 使用默认配置）
 * @return 求解系统指针
 */
LV00_PUBLIC_API Lv00SolverSystem *lv00_solver_create(const Lv00SolverConfig *config);

/**
 * @brief 释放约束求解系统
 */
LV00_PUBLIC_API void lv00_solver_free(Lv00SolverSystem *sys);

/**
 * @brief 添加几何实体
 * @return 新实体的 ID
 */
LV00_PUBLIC_API int lv00_solver_add_entity(Lv00SolverSystem *sys, const Lv00Entity *entity);

/**
 * @brief 获取实体指针
 * @return 实体指针（NULL 表示不存在）
 */
LV00_PUBLIC_API Lv00Entity *lv00_solver_get_entity(Lv00SolverSystem *sys, int id);

/**
 * @brief 添加约束
 * @return 新约束的 ID
 */
LV00_PUBLIC_API int lv00_solver_add_constraint(Lv00SolverSystem *sys, const Lv00Constraint *c);

/**
 * @brief 获取约束指针
 */
LV00_PUBLIC_API Lv00Constraint *lv00_solver_get_constraint(Lv00SolverSystem *sys, int id);

/**
 * @brief 移除约束
 */
LV00_PUBLIC_API bool lv00_solver_remove_constraint(Lv00SolverSystem *sys, int id);

/**
 * @brief 执行求解
 * @return 求解结果
 */
LV00_PUBLIC_API Lv00SolveResult lv00_solver_solve(Lv00SolverSystem *sys);

/**
 * @brief DOF（自由度）分析
 * @return DOF 分析结果（需用 lv00_dof_analysis_free 释放）
 */
LV00_PUBLIC_API Lv00DOFAnalysis *lv00_solver_dof_analyze(const Lv00SolverSystem *sys);

/**
 * @brief 释放 DOF 分析结果
 */
LV00_PUBLIC_API void lv00_dof_analysis_free(Lv00DOFAnalysis *analysis);

/**
 * @brief 获取系统约束状态
 */
LV00_PUBLIC_API Lv00SystemStatus lv00_solver_get_status(const Lv00SolverSystem *sys);

/**
 * @brief 设置实体固定状态
 */
LV00_PUBLIC_API void lv00_solver_set_fixed(Lv00SolverSystem *sys, int entity_id, bool fixed);

/**
 * @brief 设置实体拖拽状态（用于实时反馈）
 */
LV00_PUBLIC_API void lv00_solver_set_dragged(Lv00SolverSystem *sys, int entity_id, bool dragged);

/**
 * @brief 设置实体拖拽位置（用于实时反馈）
 */
LV00_PUBLIC_API void lv00_solver_set_drag_position(
    Lv00SolverSystem *sys, int entity_id, double x, double y);

/**
 * @brief 获取上次求解的迭代次数
 */
LV00_PUBLIC_API int lv00_solver_get_iteration_count(const Lv00SolverSystem *sys);

/* ========================================================================
 * 第六部分：便捷函数 —— 快速创建实体和约束
 * ======================================================================== */

/**
 * @brief 创建 2D 点实体
 */
LV00_PUBLIC_API Lv00Entity lv00_entity_point_2d(int id, double x, double y);

/**
 * @brief 创建 2D 直线实体（过两点的直线）
 */
LV00_PUBLIC_API Lv00Entity lv00_entity_line_2d(int id, double x1, double y1, double x2, double y2);

/**
 * @brief 创建 2D 圆实体
 */
LV00_PUBLIC_API Lv00Entity lv00_entity_circle_2d(int id, double cx, double cy, double r);

/**
 * @brief 创建 2D 线段实体
 */
LV00_PUBLIC_API Lv00Entity lv00_entity_segment_2d(int id, double x1, double y1, double x2, double y2);

/**
 * @brief 创建两点重合约束
 */
LV00_PUBLIC_API Lv00Constraint lv00_constraint_coincident(int id, int entity_a, int entity_b);

/**
 * @brief 创建点点距离约束
 */
LV00_PUBLIC_API Lv00Constraint lv00_constraint_distance(int id, int entity_a, int entity_b, double dist);

/**
 * @brief 创建平行约束
 */
LV00_PUBLIC_API Lv00Constraint lv00_constraint_parallel(int id, int entity_a, int entity_b);

/**
 * @brief 创建垂直约束
 */
LV00_PUBLIC_API Lv00Constraint lv00_constraint_perpendicular(int id, int entity_a, int entity_b);

/**
 * @brief 创建角度约束
 */
LV00_PUBLIC_API Lv00Constraint lv00_constraint_angle(int id, int entity_a, int entity_b, double angle_rad);

/**
 * @brief 创建点在圆上约束
 */
LV00_PUBLIC_API Lv00Constraint lv00_constraint_on_circle(int id, int point_entity, int circle_entity);

/**
 * @brief 创建固定约束
 */
LV00_PUBLIC_API Lv00Constraint lv00_constraint_fixed(int id, int entity);

/**
 * @brief 创建等长约束
 */
LV00_PUBLIC_API Lv00Constraint lv00_constraint_equal_length(int id, int entity_a, int entity_b);

/**
 * @brief 创建水平约束
 */
LV00_PUBLIC_API Lv00Constraint lv00_constraint_horizontal(int id, int entity);

/**
 * @brief 创建垂直约束（线段垂直方向）
 */
LV00_PUBLIC_API Lv00Constraint lv00_constraint_vertical(int id, int entity);

#ifdef __cplusplus
}
#endif

#endif /* LV00_GEO_CONSTRAINT_SOLVER_H */
