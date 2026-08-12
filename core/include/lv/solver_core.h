/**
 * @file solver_core.h
 * @brief CDCL SAT 求解器核心 —— 不透明句柄与公共 API
 *
 * @details 提供 CDCL（冲突驱动子句学习）SAT 求解器的公共接口：
 * - 不透明句柄 lvSolver 的生命周期管理
 * - 变量管理和约束（子句）管理
 * - CDCL 求解、假设求解、代数求解
 * - 冲突追踪（失败约束/假设、冲突集）
 * - 赋值查询和坐标解码
 * - CDCL 状态机访问（状态查询、统计信息）
 * - 求解器克隆和重置
 *
 * @version 1.1.0
 */

#ifndef lv_SOLVER_CORE_H
#define lv_SOLVER_CORE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "lv/lv_utils.h"

#include "constraint_graph.h"

#ifndef lv_PUBLIC_API
#define lv_PUBLIC_API
#endif

/* ================================================================
 * 常量
 * ================================================================ */

/* lv_SOLVER_SCALE_FACTOR 统一定义于 lv_internal.h（solver 各文件经
 * solver_common.h 引入；此处历史重复定义已移除，避免双头失步）。 */

/* ================================================================
 * 类型别名
 * ================================================================ */

/** @brief 求解器变量 ID（正整数，从 1 开始） */
typedef int lvSolverVar;

/** @brief 求解器文字（正=变量赋真，负=变量赋假） */
typedef int lvSolverLit;

/** @brief 求解器约束 ID */
typedef int lvConstraintId;

/** @brief 无效约束 ID 标记 */
#define lv_CONSTRAINT_ID_INVALID (-1)

/* ================================================================
 * 求解结果枚举
 * ================================================================ */

/**
 * @brief 求解结果枚举
 */
typedef enum {
    lv_SOLVER_UNKNOWN = 0, /**< 未知（资源耗尽或未求解） */
    lv_SOLVER_SAT = 1,     /**< 可满足 */
    lv_SOLVER_UNSAT = 2    /**< 不可满足 */
} lvSolverResult;

/* ================================================================
 * CDCL 状态枚举
 * ================================================================ */

/**
 * @brief CDCL 状态机状态枚举（10 状态）
 *
 * CDCL 求解器状态机包含以下状态：
 * - IDLE: 空闲/未初始化
 * - PROPAGATING: 单元传播中
 * - CONFLICT: 冲突检测
 * - ANALYZING: 冲突分析（1-UIP）
 * - BACKJUMPING: 非时序回溯
 * - LEARNING: 子句学习
 * - DECIDING: 变量决策
 * - RESTARTING: 重启
 * - SATISFIED: 所有子句满足
 * - UNSAT: 不可满足
 */
typedef enum {
    CDCL_IDLE = 0,        /**< 空闲 */
    CDCL_PROPAGATING = 1, /**< 传播中 */
    CDCL_CONFLICT = 2,    /**< 冲突 */
    CDCL_ANALYZING = 3,   /**< 分析中 */
    CDCL_BACKJUMPING = 4, /**< 回跳中 */
    CDCL_LEARNING = 5,    /**< 学习中 */
    CDCL_DECIDING = 6,    /**< 决策中 */
    CDCL_RESTARTING = 7,  /**< 重启中 */
    CDCL_SATISFIED = 8,   /**< 已满足 */
    CDCL_UNSAT = 9        /**< 不可满足 */
} CDCLState;

/* ================================================================
 * CDCL 上下文结构
 * ================================================================ */

/**
 * @brief CDCL 上下文 —— 存储求解器的内部状态
 *
 * 包含赋值数组、决策层级、子句库、trail、监视文字等 CDCL 算法
 * 所需的全部运行时数据。
 */
typedef struct {
    /* 赋值状态 */
    int *assigns;     /**< 变量赋值数组：0=未赋值, >0=正文字, <0=负文字 */
    int *levels;      /**< 每个变量的决策层级 */
    int *reasons;     /**< 每个变量的原因子句索引（-1=决策赋值） */
    int var_count;    /**< 变量总数 */
    int var_capacity; /**< 赋值数组容量 */

    /* Trail（赋值序列） */
    lvDArray trail;         /**< 动态数组，元素类型：int（文字列表） */
    int *trail_lim;         /**< 决策层起始位置数组 */
    int trail_lim_capacity; /**< trail_lim 数组容量 */

    /* 子句库 */
    int **clauses;          /**< 子句数组（文字序列） */
    int *clause_sizes;      /**< 每子句的文字数 */
    int orig_clause_count;  /**< 原始子句数 */
    int learn_clause_count; /**< 学习子句数 */
    int clause_capacity;    /**< 子句数组容量 */

    /* 冲突分析 */
    int *conflict_clause;  /**< 冲突/学习子句缓冲区 */
    int conflict_size;     /**< 冲突子句文字数 */
    int conflict_capacity; /**< 冲突缓冲区容量 */
    int backtrack_level;   /**< 回跳目标层级 */

    /* 监视文字 */
    int **watches;         /**< 监视文字数组 */
    int *watch_sizes;      /**< 每个变量的监视列表大小 */
    int *watch_capacities; /**< 每个变量的监视列表容量 */

    /* 状态机 */
    CDCLState state;    /**< 当前 CDCL 状态 */
    int decision_level; /**< 当前决策层级 */

    /* 统计 */
    int64_t conflicts;        /**< 冲突计数 */
    int64_t decisions;        /**< 决策计数 */
    int64_t propagations;     /**< 传播计数 */
    int64_t restarts;         /**< 重启计数 */
    int64_t learned_literals; /**< 学习文字总数 */
    double time_ms;           /**< 求解耗时（毫秒） */
} CDCLContext;

/* ================================================================
 * 求解器配置
 * ================================================================ */

/**
 * @brief 求解器配置
 */
typedef struct {
    bool enable_restarts; /**< 启用 Luby 序列重启 */
    int restart_interval; /**< 重启间隔（冲突数） */
    double max_time_sec;  /**< 最大求解时间（秒，0=无限制） */
} lvSolverConfig;

/* ================================================================
 * 求解器不透明句柄（前向声明）
 * ================================================================ */

/**
 * @brief CDCL SAT 求解器（不透明句柄）
 *
 * 内部结构在 solver_core.c 中定义，外部仅通过 API 函数操作。
 */
typedef struct lvSolver lvSolver;

/* ================================================================
 * 默认配置
 * ================================================================ */

/**
 * @brief 获取默认求解器配置
 * @return 默认配置
 */
lv_PUBLIC_API lvSolverConfig lv_solver_config_default(void);

/* ================================================================
 * 生命周期 API
 * ================================================================ */

/**
 * @brief 使用默认配置创建求解器
 * @return 求解器实例，失败返回 NULL
 */
lv_PUBLIC_API lvSolver *lv_solver_create(void);

/**
 * @brief 使用指定配置创建求解器
 * @param config 求解器配置
 * @return 求解器实例，失败返回 NULL
 */
lv_PUBLIC_API lvSolver *lv_solver_create_with_config(const lvSolverConfig *config);

/**
 * @brief 销毁求解器及所有关联资源
 * @param solver 求解器实例
 */
lv_PUBLIC_API void lv_solver_destroy(lvSolver *solver);

/* ================================================================
 * 变量管理 API
 * ================================================================ */

/**
 * @brief 创建一个新变量
 * @param solver 求解器实例
 * @return 新变量 ID（>=1），失败返回 -1
 */
lv_PUBLIC_API lvSolverVar lv_solver_new_var(lvSolver *solver);

/**
 * @brief 批量创建新变量
 * @param solver 求解器实例
 * @param count  变量数量（>=1）
 * @return 第一个新变量 ID，失败返回 -1
 */
lv_PUBLIC_API lvSolverVar lv_solver_new_vars(lvSolver *solver, int count);

/**
 * @brief 获取当前变量总数
 * @param solver 求解器实例
 * @return 变量数量
 */
lv_PUBLIC_API int lv_solver_var_count(const lvSolver *solver);

/* ================================================================
 * 约束管理 API
 * ================================================================ */

/**
 * @brief 添加约束（子句）
 * @param solver   求解器实例
 * @param literals 文字数组
 * @param count    文字数量
 * @return 约束 ID，失败返回 lv_CONSTRAINT_ID_INVALID
 */
lv_PUBLIC_API lvConstraintId lv_solver_add_constraint(lvSolver *solver, const lvSolverLit *literals, int count);

/**
 * @brief 移除约束
 * @param solver        求解器实例
 * @param constraint_id 约束 ID
 * @return true 成功
 */
lv_PUBLIC_API bool lv_solver_remove_constraint(lvSolver *solver, lvConstraintId constraint_id);

/* ================================================================
 * 求解 API
 * ================================================================ */

/**
 * @brief 执行 SAT 求解
 * @param solver 求解器实例
 * @return 求解结果
 */
lv_PUBLIC_API lvSolverResult lv_solver_solve(lvSolver *solver);

/**
 * @brief 在假设下求解
 * @param solver      求解器实例
 * @param assumptions 假设文字数组
 * @param count       假设数量
 * @return 求解结果
 */
lv_PUBLIC_API lvSolverResult lv_solver_solve_under_assumptions(lvSolver *solver, const lvSolverLit *assumptions,
                                                               int count);

/**
 * @brief 使用 Groebner 基代数方法求解
 * @param solver 求解器实例
 * @return 求解结果
 */
lv_PUBLIC_API lvSolverResult lv_solver_solve_algebraic(lvSolver *solver);

/* ================================================================
 * 冲突追踪 API
 * ================================================================ */

/**
 * @brief 检查约束是否在最近求解中失败
 * @param solver        求解器实例
 * @param constraint_id 约束 ID
 * @return true 表示该约束失败
 */
lv_PUBLIC_API bool lv_solver_failed_constraint(const lvSolver *solver, lvConstraintId constraint_id);

/**
 * @brief 检查假设文字是否在最近求解中失败
 * @param solver     求解器实例
 * @param assumption 假设文字
 * @return true 表示该假设失败
 */
lv_PUBLIC_API bool lv_solver_failed_assumption(const lvSolver *solver, lvSolverLit assumption);

/**
 * @brief 获取冲突集（最近学习子句）
 * @param solver    求解器实例
 * @param out_count 输出冲突集文字数量
 * @return 冲突集文字数组（调用者需 lv_free 释放），失败返回 NULL
 */
lv_PUBLIC_API lvSolverLit *lv_solver_conflict_set(const lvSolver *solver, int *out_count);

/* ================================================================
 * 查询赋值 API
 * ================================================================ */

/**
 * @brief 获取变量的赋值
 * @param solver 求解器实例
 * @param var    变量 ID
 * @return 赋值：0=未赋值, >0=真, <0=假
 */
lv_PUBLIC_API int lv_solver_get_value(const lvSolver *solver, lvSolverVar var);

/**
 * @brief 获取变量的符号坐标
 *
 * 从 SAT 赋值中解码坐标值，优先使用约束图中的精确符号坐标。
 *
 * @param solver    求解器实例
 * @param var_base  基础变量 ID（x 坐标对应变量）
 * @param coord     输出符号坐标
 * @return true 成功获取坐标
 */
lv_PUBLIC_API bool lv_solver_get_coord(const lvSolver *solver, lvSolverVar var_base, SymbolicCoord *coord);

/* ================================================================
 * CDCL 状态机访问 API
 * ================================================================ */

/**
 * @brief 获取当前 CDCL 状态
 * @param solver 求解器实例
 * @return CDCL 状态
 */
lv_PUBLIC_API CDCLState lv_solver_cdcl_state(const lvSolver *solver);

/**
 * @brief 获取 CDCL 统计信息
 * @param solver        求解器实例
 * @param out_conflicts   输出冲突计数（可为 NULL）
 * @param out_decisions  输出决策计数（可为 NULL）
 * @param out_propagations 输出传播计数（可为 NULL）
 * @param out_restarts    输出重启计数（可为 NULL）
 */
lv_PUBLIC_API void lv_solver_cdcl_stats(const lvSolver *solver, int64_t *out_conflicts, int64_t *out_decisions,
                                        int64_t *out_propagations, int64_t *out_restarts);

/**
 * @brief 获取 CDCL 上下文（只读访问）
 * @param solver 求解器实例
 * @return CDCL 上下文指针（求解器生命周期内有效）
 */
lv_PUBLIC_API const CDCLContext *lv_solver_cdcl_context(const lvSolver *solver);

/* ================================================================
 * 约束图关联 API
 * ================================================================ */

/**
 * @brief 设置关联的约束图（用于坐标解码）
 * @param solver 求解器实例
 * @param graph  约束图（引用，不转移所有权）
 */
lv_PUBLIC_API void lv_solver_set_constraint_graph(lvSolver *solver, const struct ConstraintGraph *graph);

/* ================================================================
 * 导入/导出 API
 * ================================================================ */

/**
 * @brief 克隆求解器（深拷贝）
 * @param solver 源求解器
 * @return 新求解器实例，失败返回 NULL
 */
lv_PUBLIC_API lvSolver *lv_solver_clone(const lvSolver *solver);

/**
 * @brief 重置求解器（清除所有变量、子句和状态）
 * @param solver 求解器实例
 */
lv_PUBLIC_API void lv_solver_reset(lvSolver *solver);

#ifdef __cplusplus
}
#endif

#endif /* lv_SOLVER_CORE_H */
