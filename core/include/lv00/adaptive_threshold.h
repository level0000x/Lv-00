/**
 * @file adaptive_threshold.h
 * @brief Lv-00 自适应阈值框架 —— 动态阈值计算与启发式剪枝
 *
 * 提供基于问题复杂度的动态阈值计算、进度跟踪和启发式剪枝机制，
 * 支持 VF2 图匹配、Buchberger 算法和重写求解三种核心算法。
 *
 * @version 1.0.0
 * @date 2026-07-21
 */

#ifndef LV00_ADAPTIVE_THRESHOLD_H
#define LV00_ADAPTIVE_THRESHOLD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#include "lv00/constraint_graph.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct ConstraintGraph Lv00ConstraintGraph;

/** @brief Lv00Error 是 Lv00ErrorCode 的别名，用于统一 API 风格 */
typedef Lv00ErrorCode Lv00Error;

/* ============================================================
 * 类型定义
 * ============================================================ */

/**
 * @brief 算法类型枚举
 */
typedef enum {
    LV00_ALGO_VF2_MATCH = 0,    /**< VF2 子图同构匹配 */
    LV00_ALGO_BUCHBERGER = 1,   /**< Buchberger Gröbner 基算法 */
    LV00_ALGO_REWRITE_SOLVE = 2 /**< 重写求解算法 */
} Lv00AlgorithmType;

/**
 * @brief 问题复杂度描述
 */
typedef struct {
    int32_t node_count;           /**< 节点数量 */
    int32_t constraint_count;     /**< 约束数量 */
    int32_t edge_count;           /**< 边数量（从约束元数计算） */
    double  density;              /**< 图密度 */
    int32_t connected_components; /**< 连通分量数量 */
} Lv00ProblemComplexity;

/**
 * @brief 阈值配置
 */
typedef struct {
    double base_threshold;          /**< 基础阈值 */
    double scale_factor;            /**< 缩放因子 */
    double time_budget_ms;          /**< 时间预算（毫秒） */
    double min_threshold;           /**< 最小阈值 */
    double max_threshold;           /**< 最大阈值 */
    bool   enable_time_based;       /**< 是否启用基于时间的剪枝 */
    bool   enable_progress_tracking;/**< 是否启用进度跟踪 */
} Lv00ThresholdConfig;

/**
 * @brief 自适应阈值上下文
 */
typedef struct Lv00AdaptiveThresholdCtx {
    Lv00AlgorithmType     algo;              /**< 关联的算法类型 */
    Lv00ProblemComplexity complexity;        /**< 问题复杂度 */
    Lv00ThresholdConfig   config;            /**< 阈值配置 */
    size_t                current_progress;  /**< 当前进度 */
    size_t                backtrack_count;   /**< 回溯计数 */
    struct timespec       start_time;        /**< 开始时间 */
    bool                  initialized;       /**< 是否已初始化 */
} Lv00AdaptiveThresholdCtx;

/* ============================================================
 * API 声明
 * ============================================================ */

/**
 * @brief 初始化自适应阈值模块（幂等操作）
 * @return LV00_OK 成功
 */
LV00_PUBLIC_API Lv00Error lv00_adaptive_threshold_init(void);

/**
 * @brief 清理自适应阈值模块
 */
LV00_PUBLIC_API void lv00_adaptive_threshold_cleanup(void);

/**
 * @brief 分析约束图的问题复杂度
 * @param graph      约束图（不可为 NULL）
 * @param complexity 输出复杂度信息（不可为 NULL）
 * @return LV00_OK 成功，否则返回错误码
 */
LV00_PUBLIC_API Lv00Error lv00_compute_complexity(
    const Lv00ConstraintGraph *graph,
    Lv00ProblemComplexity *complexity);

/**
 * @brief 创建自适应阈值上下文
 * @param algo   算法类型
 * @param graph  约束图（不可为 NULL）
 * @param config 阈值配置（NULL 时使用默认配置）
 * @param ctx    输出上下文指针（不可为 NULL）
 * @return LV00_OK 成功，否则返回错误码
 */
LV00_PUBLIC_API Lv00Error lv00_adaptive_threshold_create(
    Lv00AlgorithmType algo,
    const Lv00ConstraintGraph *graph,
    const Lv00ThresholdConfig *config,
    Lv00AdaptiveThresholdCtx **ctx);

/**
 * @brief 计算自适应阈值
 * @param ctx 阈值上下文
 * @return 计算出的阈值，ctx 为 NULL 时返回 0
 */
LV00_PUBLIC_API size_t lv00_adaptive_threshold_compute(
    Lv00AdaptiveThresholdCtx *ctx);

/**
 * @brief 获取算法的默认阈值配置
 * @param algo   算法类型
 * @param config 输出配置（不可为 NULL）
 * @return LV00_OK 成功，否则返回错误码
 */
LV00_PUBLIC_API Lv00Error lv00_adaptive_threshold_default_config(
    Lv00AlgorithmType algo,
    Lv00ThresholdConfig *config);

/**
 * @brief 销毁阈值上下文
 * @param ctx 上下文指针的指针（销毁后 *ctx 置 NULL）
 */
LV00_PUBLIC_API void lv00_adaptive_threshold_destroy(
    Lv00AdaptiveThresholdCtx **ctx);

/**
 * @brief 更新进度跟踪
 * @param ctx             阈值上下文
 * @param current         当前进度值
 * @param backtrack_count 回溯次数
 */
LV00_PUBLIC_API void lv00_adaptive_threshold_update_progress(
    Lv00AdaptiveThresholdCtx *ctx,
    size_t current,
    size_t backtrack_count);

/**
 * @brief 判断是否应该执行剪枝
 * @param ctx          阈值上下文
 * @param should_prune 输出：是否应该剪枝
 */
LV00_PUBLIC_API void lv00_adaptive_threshold_should_prune(
    Lv00AdaptiveThresholdCtx *ctx,
    bool *should_prune);

/**
 * @brief 设置算法的全局默认配置
 * @param algo   算法类型
 * @param config 配置（不可为 NULL）
 * @return LV00_OK 成功，否则返回错误码
 */
LV00_PUBLIC_API Lv00Error lv00_adaptive_threshold_set_global_config(
    Lv00AlgorithmType algo,
    const Lv00ThresholdConfig *config);

/**
 * @brief 便捷函数：获取 VF2 匹配的最大搜索深度
 * @param graph 约束图
 * @return 最大深度阈值（范围 50-1000）
 */
LV00_PUBLIC_API size_t lv00_get_vf2_max_depth(
    const Lv00ConstraintGraph *graph);

/**
 * @brief 便捷函数：获取 Buchberger 算法的最大迭代步数
 * @param graph 约束图
 * @return 最大步数阈值（范围 10000-200000）
 */
LV00_PUBLIC_API size_t lv00_get_buchberger_max_steps(
    const Lv00ConstraintGraph *graph);

/**
 * @brief 便捷函数：获取重写求解的最大迭代次数
 * @param graph 约束图
 * @return 最大迭代次数阈值（范围 5000-50000）
 */
LV00_PUBLIC_API size_t lv00_get_rewrite_solve_max_iterations(
    const Lv00ConstraintGraph *graph);

#ifdef __cplusplus
}
#endif

#endif /* LV00_ADAPTIVE_THRESHOLD_H */
