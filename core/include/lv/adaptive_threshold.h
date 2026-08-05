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

#ifndef lv_ADAPTIVE_THRESHOLD_H
#define lv_ADAPTIVE_THRESHOLD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#include "lv/constraint_graph.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct ConstraintGraph lvConstraintGraph;

/** @brief lvError 是 lvErrorCode 的别名，用于统一 API 风格 */
typedef lvErrorCode lvError;

/* ============================================================
 * 类型定义
 * ============================================================ */

/**
 * @brief 算法类型枚举
 */
typedef enum {
    lv_ALGO_VF2_MATCH = 0,    /**< VF2 子图同构匹配 */
    lv_ALGO_BUCHBERGER = 1,   /**< Buchberger Gröbner 基算法 */
    lv_ALGO_REWRITE_SOLVE = 2 /**< 重写求解算法 */
} lvAlgorithmType;

/**
 * @brief 问题复杂度描述
 */
typedef struct {
    int32_t node_count;           /**< 节点数量 */
    int32_t constraint_count;     /**< 约束数量 */
    int32_t edge_count;           /**< 边数量（从约束元数计算） */
    double density;               /**< 图密度 */
    int32_t connected_components; /**< 连通分量数量 */
} lvProblemComplexity;

/**
 * @brief 阈值配置
 */
typedef struct {
    double base_threshold;         /**< 基础阈值 */
    double scale_factor;           /**< 缩放因子 */
    double time_budget_ms;         /**< 时间预算（毫秒） */
    double min_threshold;          /**< 最小阈值 */
    double max_threshold;          /**< 最大阈值 */
    bool enable_time_based;        /**< 是否启用基于时间的剪枝 */
    bool enable_progress_tracking; /**< 是否启用进度跟踪 */
} lvThresholdConfig;

/**
 * @brief 自适应阈值上下文
 */
typedef struct lvAdaptiveThresholdCtx {
    lvAlgorithmType algo;           /**< 关联的算法类型 */
    lvProblemComplexity complexity; /**< 问题复杂度 */
    lvThresholdConfig config;       /**< 阈值配置 */
    size_t current_progress;        /**< 当前进度 */
    size_t backtrack_count;         /**< 回溯计数 */
    struct timespec start_time;     /**< 开始时间 */
    bool initialized;               /**< 是否已初始化 */
} lvAdaptiveThresholdCtx;

/* ============================================================
 * API 声明
 * ============================================================ */

/**
 * @brief 初始化自适应阈值模块（幂等操作）
 * @return lv_OK 成功
 */
lv_PUBLIC_API lvError lv_adaptive_threshold_init(void);

/**
 * @brief 清理自适应阈值模块
 */
lv_PUBLIC_API void lv_adaptive_threshold_cleanup(void);

/**
 * @brief 分析约束图的问题复杂度
 * @param graph      约束图（不可为 NULL）
 * @param complexity 输出复杂度信息（不可为 NULL）
 * @return lv_OK 成功，否则返回错误码
 */
lv_PUBLIC_API lvError lv_compute_complexity(const lvConstraintGraph *graph, lvProblemComplexity *complexity);

/**
 * @brief 创建自适应阈值上下文
 * @param algo   算法类型
 * @param graph  约束图（不可为 NULL）
 * @param config 阈值配置（NULL 时使用默认配置）
 * @param ctx    输出上下文指针（不可为 NULL）
 * @return lv_OK 成功，否则返回错误码
 */
lv_PUBLIC_API lvError lv_adaptive_threshold_create(lvAlgorithmType algo, const lvConstraintGraph *graph,
                                                   const lvThresholdConfig *config, lvAdaptiveThresholdCtx **ctx);

/**
 * @brief 计算自适应阈值
 * @param ctx 阈值上下文
 * @return 计算出的阈值，ctx 为 NULL 时返回 0
 */
lv_PUBLIC_API size_t lv_adaptive_threshold_compute(lvAdaptiveThresholdCtx *ctx);

/**
 * @brief 获取算法的默认阈值配置
 * @param algo   算法类型
 * @param config 输出配置（不可为 NULL）
 * @return lv_OK 成功，否则返回错误码
 */
lv_PUBLIC_API lvError lv_adaptive_threshold_default_config(lvAlgorithmType algo, lvThresholdConfig *config);

/**
 * @brief 销毁阈值上下文
 * @param ctx 上下文指针的指针（销毁后 *ctx 置 NULL）
 * @note 签名采用双指针（lvAdaptiveThresholdCtx **ctx），与主流约定
 *       void xxx_destroy(T*) 不同：这是有意设计 —— 销毁后把调用方持有的
 *       指针置 NULL，避免悬挂指针。销毁语义等同主流约定（NULL 安全）。
 */
lv_PUBLIC_API void lv_adaptive_threshold_destroy(lvAdaptiveThresholdCtx **ctx);

/**
 * @brief 更新进度跟踪
 * @param ctx             阈值上下文
 * @param current         当前进度值
 * @param backtrack_count 回溯次数
 */
lv_PUBLIC_API void lv_adaptive_threshold_update_progress(lvAdaptiveThresholdCtx *ctx, size_t current,
                                                         size_t backtrack_count);

/**
 * @brief 判断是否应该执行剪枝
 * @param ctx          阈值上下文
 * @param should_prune 输出：是否应该剪枝
 */
lv_PUBLIC_API void lv_adaptive_threshold_should_prune(lvAdaptiveThresholdCtx *ctx, bool *should_prune);

/**
 * @brief 设置算法的全局默认配置
 * @param algo   算法类型
 * @param config 配置（不可为 NULL）
 * @return lv_OK 成功，否则返回错误码
 */
lv_PUBLIC_API lvError lv_adaptive_threshold_set_global_config(lvAlgorithmType algo, const lvThresholdConfig *config);

/**
 * @brief 便捷函数：获取 VF2 匹配的最大搜索深度
 * @param graph 约束图
 * @return 最大深度阈值（范围 50-1000）
 */
lv_PUBLIC_API size_t lv_get_vf2_max_depth(const lvConstraintGraph *graph);

/**
 * @brief 便捷函数：获取 Buchberger 算法的最大迭代步数
 * @param graph 约束图
 * @return 最大步数阈值（范围 10000-200000）
 */
lv_PUBLIC_API size_t lv_get_buchberger_max_steps(const lvConstraintGraph *graph);

/**
 * @brief 便捷函数：获取重写求解的最大迭代次数
 * @param graph 约束图
 * @return 最大迭代次数阈值（范围 5000-50000）
 */
lv_PUBLIC_API size_t lv_get_rewrite_solve_max_iterations(const lvConstraintGraph *graph);

#ifdef __cplusplus
}
#endif

#endif /* lv_ADAPTIVE_THRESHOLD_H */
