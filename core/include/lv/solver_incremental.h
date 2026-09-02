/**
 * @file solver_incremental.h
 * @brief 蓝图增量求解 API（TEN_LAYER_OPTIMIZED_PLAN §15.4 落地）
 *
 * lvIncrementalSolver 维护约束图 → lvSolverSystem 的映射与求解缓存；
 * solve_incremental 在缓存失效时重建/重解；invalidate/mark_changed 标记变更；
 * solve_parallel 对 lv_graph_decompose 的子图做相容性检查（并行粒度为
 * 子图检查，简化串行执行——见实现注释）。
 *
 * 所有权：create 返回 [take] 句柄，destroy 释放；graph 为 [borrow]。
 */

#ifndef lv_SOLVER_INCREMENTAL_H
#define lv_SOLVER_INCREMENTAL_H

#include <stdbool.h>
#include <stddef.h>

#include "constraint_graph.h"
#include "geo_constraint_solver.h"
#include "lv_api_spec.h" /* lv_PUBLIC_API（K59） */

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 增量求解上下文（蓝图 lvIncrementalSolver 的库内适配） */
typedef struct lvIncrementalSolver {
    ConstraintGraph *graph;      /**< 关联约束图（[borrow]） */
    void *solver_sys;            /**< 内部 lvSolverSystem*（懒创建） */
    lvSolveResult last_result;   /**< 上次求解结果缓存 */
    int *changed_nodes;          /**< 自上次求解后变更节点 ID 数组 */
    int changed_count;           /**< 变更节点数 */
    int changed_capacity;        /**< 变更数组容量 */
    bool is_valid;               /**< 缓存是否有效 */
} lvIncrementalSolver;

/**
 * @brief 创建增量求解器（[take]）
 *
 * @param graph 关联约束图（[borrow]，生命周期须长于 solver）
 * @return 增量求解器句柄；失败返回 NULL
 */
lv_PUBLIC_API lvIncrementalSolver *lv_solver_incremental_create(ConstraintGraph *graph);

/**
 * @brief 销毁增量求解器并释放内部求解系统与缓存
 * NULL 安全。
 */
lv_PUBLIC_API void lv_solver_incremental_destroy(lvIncrementalSolver *solver);

/**
 * @brief 增量求解（蓝图 lv_solve_incremental）
 *
 * 首次或缓存失效（is_valid=false 或 changed_count>0）时：将 graph 的
 * 点节点与约束映射到内部 lvSolverSystem 并求解，结果写入 *out_result，
 * 标记缓存有效并清空变更集；缓存有效且无变更时直接返回缓存结果。
 * 无法建立求解系统（如图仅含非点节点）时回退 graph_check_compatibility。
 *
 * @param solver     增量求解器（非 NULL）
 * @param out_result 输出求解结果（可为 NULL 表示仅执行不取结果）
 * @return true 执行成功（结果含一致/求解态）；false 参数无效或内部失败
 */
lv_PUBLIC_API bool lv_solve_incremental(lvIncrementalSolver *solver, lvSolveResult *out_result);

/**
 * @brief 标记图变更（蓝图 lv_incremental_solver_invalidate）
 *
 * 使增量缓存失效（is_valid=false），下次 solve 全量重建。
 *
 * @param solver 增量求解器（非 NULL）
 */
lv_PUBLIC_API void lv_incremental_solver_invalidate(lvIncrementalSolver *solver);

/**
 * @brief 记录节点变更（补充 API：changed_nodes 填充入口）
 *
 * 追加节点 ID 到变更集（去重）；下次 solve_incremental 据此重建。
 *
 * @param solver  增量求解器（非 NULL）
 * @param node_id 变更节点 ID
 * @return true 记录成功
 */
lv_PUBLIC_API bool lv_incremental_solver_mark_changed(lvIncrementalSolver *solver, int node_id);

/**
 * @brief 并行求解子图（蓝图 lv_solve_parallel）
 *
 * 对 lv_graph_decompose 的每个子图执行 graph_check_compatibility
 * （子图一致性与全图一致等价——相容性是图级性质，检查单子图即全图）。
 * 实现为串行循环（子图检查间无共享可变状态，线程化收益不显著），
 * out_result 填 lv_SOLVE_OK（全部一致）或 lv_SOLVE_INCONSISTENT。
 *
 * @param tasks       子图任务数组（来自 lv_graph_decompose，[borrow]）
 * @param task_count  子图数
 * @param max_threads 最大线程数（提示性，当前实现忽略）
 * @param out_result  输出结果（可为 NULL）
 * @return true 执行成功；false 参数无效
 */
lv_PUBLIC_API bool lv_solve_parallel(lvSubgraphTask *tasks, int task_count, int max_threads,
                                     lvSolveResult *out_result);

#ifdef __cplusplus
}
#endif

#endif /* lv_SOLVER_INCREMENTAL_H */
