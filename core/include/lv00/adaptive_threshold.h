/**
 * @file adaptive_threshold.h
 * @brief 自适应阈值框架 - 动态调整算法迭代限制
 * 
 * 本模块提供基于问题复杂度的动态阈值计算机制，
 * 替代原有的硬编码限制（VF2_MAX_DEPTH, BUCHBERGER_MAX_STEPS等）。
 * 
 * @version 1.0.0
 * @date 2026-05-26
 */

#ifndef LV00_ADAPTIVE_THRESHOLD_H
#define LV00_ADAPTIVE_THRESHOLD_H

#include "lv00.h"
#include "constraint_graph.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 问题复杂度评估指标
 * 
 * 用于量化约束图问题的规模，作为动态阈值计算的基础
 */
typedef struct {
    size_t node_count;           /**< 几何节点数量 */
    size_t constraint_count;     /**< 约束数量 */
    size_t edge_count;          /**< 约束图中的边数（用于图算法） */
    size_t max_degree;          /**< 多项式最高次数（用于代数算法） */
    size_t avg_node_degree;     /**< 平均节点度数 */
    size_t connected_components; /**< 连通分量数量 */
    double density;             /**< 图密度 [0, 1] */
} Lv00ProblemComplexity;

/**
 * @brief 算法类型枚举
 * 
 * 不同算法需要不同的阈值计算策略
 */
typedef enum {
    LV00_ALGO_VF2_MATCH,           /**< VF2子图同构匹配 */
    LV00_ALGO_BUCHBERGER,          /**< Buchberger Groebner基算法 */
    LV00_ALGO_POLY_REDUCE,         /**< 多项式约化 */
    LV00_ALGO_REWRITE_SOLVE,       /**< 重写-求解迭代 */
    LV00_ALGO_NORMALIZATION,       /**< 约束图归一化 */
    LV00_ALGO_UNIFICATION,         /**< 合一检查 */
    LV00_ALGO_COUNT                /**< 算法类型总数 */
} Lv00AlgorithmType;

/**
 * @brief 阈值配置参数
 * 
 * 允许用户自定义阈值计算的行为
 */
typedef struct {
    double base_threshold;       /**< 基础阈值 */
    double scale_factor;         /**< 规模缩放因子 */
    double time_budget_ms;       /**< 时间预算（毫秒） */
    double min_threshold;        /**< 最小阈值（防止过小） */
    double max_threshold;        /**< 最大阈值（防止过大） */
    bool enable_time_based;      /**< 是否启用基于时间的动态调整 */
    bool enable_progress_tracking; /**< 是否启用进度跟踪 */
} Lv00ThresholdConfig;

/**
 * @brief 搜索进度跟踪状态
 * 
 * 用于启发式剪枝决策
 */
typedef struct {
    double progress_estimate;    /**< 进度估计 [0, 1] */
    double solution_likelihood;  /**< 解存在概率估计 [0, 1] */
    double time_spent_ms;        /**< 已花费时间 */
    size_t iterations_done;      /**< 已完成迭代次数 */
    size_t backtracks;           /**< 回溯次数 */
} Lv00SearchProgress;

/**
 * @brief 自适应阈值上下文
 * 
 * 每个算法实例维护一个上下文，用于跟踪和调整阈值
 */
typedef struct {
    Lv00AlgorithmType algo_type;     /**< 算法类型 */
    Lv00ThresholdConfig config;      /**< 配置参数 */
    Lv00ProblemComplexity complexity; /**< 问题复杂度（缓存） */
    Lv00SearchProgress progress;     /**< 当前进度 */
    size_t current_threshold;        /**< 当前计算的阈值 */
    uint64_t start_time_us;          /**< 开始时间戳（微秒） */
} Lv00AdaptiveThresholdCtx;

/* ==================== 核心API ==================== */

/**
 * @brief 初始化自适应阈值框架
 * 
 * 必须在任何其他函数之前调用
 * 
 * @return LV00_OK 成功，其他错误码
 */
LV00_PUBLIC_API Lv00Error lv00_adaptive_threshold_init(void);

/**
 * @brief 清理自适应阈值框架资源
 */
LV00_PUBLIC_API void lv00_adaptive_threshold_cleanup(void);

/**
 * @brief 从约束图计算问题复杂度
 * 
 * @param graph 输入约束图
 * @param complexity 输出复杂度评估结果
 * @return LV00_OK 成功
 */
LV00_PUBLIC_API Lv00Error lv00_compute_complexity(
    const Lv00ConstraintGraph* graph,
    Lv00ProblemComplexity* complexity
);

/**
 * @brief 创建自适应阈值上下文
 * 
 * @param algo_type 算法类型
 * @param graph 输入约束图（用于初始复杂度评估）
 * @param config 阈值配置（可为NULL，使用默认值）
 * @param ctx 输出上下文指针
 * @return LV00_OK 成功
 */
LV00_PUBLIC_API Lv00Error lv00_adaptive_threshold_create(
    Lv00AlgorithmType algo_type,
    const Lv00ConstraintGraph* graph,
    const Lv00ThresholdConfig* config,
    Lv00AdaptiveThresholdCtx** ctx
);

/**
 * @brief 销毁自适应阈值上下文
 * 
 * @param ctx 上下文指针（会被置为NULL）
 */
LV00_PUBLIC_API void lv00_adaptive_threshold_destroy(
    Lv00AdaptiveThresholdCtx** ctx
);

/**
 * @brief 计算当前阈值
 * 
 * 基于问题复杂度和配置参数计算建议的迭代阈值
 * 
 * @param ctx 自适应阈值上下文
 * @return 计算得到的阈值（已应用min/max限制）
 */
LV00_PUBLIC_API size_t lv00_adaptive_threshold_compute(
    Lv00AdaptiveThresholdCtx* ctx
);

/**
 * @brief 更新搜索进度
 * 
 * 在算法执行过程中定期调用，用于启发式剪枝决策
 * 
 * @param ctx 自适应阈值上下文
 * @param iterations_done 已完成迭代次数
 * @param backtracks 回溯次数
 * @return LV00_OK 成功
 */
LV00_PUBLIC_API Lv00Error lv00_adaptive_threshold_update_progress(
    Lv00AdaptiveThresholdCtx* ctx,
    size_t iterations_done,
    size_t backtracks
);

/**
 * @brief 检查是否应该剪枝当前分支
 * 
 * 基于启发式规则判断是否应该提前终止当前搜索分支
 * 
 * @param ctx 自适应阈值上下文
 * @param should_prune 输出：是否应该剪枝
 * @return LV00_OK 成功
 */
LV00_PUBLIC_API Lv00Error lv00_adaptive_threshold_should_prune(
    Lv00AdaptiveThresholdCtx* ctx,
    bool* should_prune
);

/**
 * @brief 获取默认配置
 * 
 * @param algo_type 算法类型
 * @param config 输出默认配置
 * @return LV00_OK 成功
 */
LV00_PUBLIC_API Lv00Error lv00_adaptive_threshold_default_config(
    Lv00AlgorithmType algo_type,
    Lv00ThresholdConfig* config
);

/**
 * @brief 设置全局配置
 * 
 * 影响所有新创建的上下文
 * 
 * @param algo_type 算法类型
 * @param config 配置参数
 * @return LV00_OK 成功
 */
LV00_PUBLIC_API Lv00Error lv00_adaptive_threshold_set_global_config(
    Lv00AlgorithmType algo_type,
    const Lv00ThresholdConfig* config
);

/* ==================== 便捷宏 ==================== */

/**
 * @brief 便捷宏：获取VF2匹配的最大深度阈值
 */
#define LV00_ADAPTIVE_VF2_MAX_DEPTH(graph, ctx_ptr) \
    (lv00_adaptive_threshold_compute(ctx_ptr))

/**
 * @brief 便捷宏：获取Buchberger算法的最大步数阈值
 */
#define LV00_ADAPTIVE_BUCHBERGER_MAX_STEPS(graph, ctx_ptr) \
    (lv00_adaptive_threshold_compute(ctx_ptr))

/**
 * @brief 便捷宏：获取多项式约化的最大步数阈值
 */
#define LV00_ADAPTIVE_POLY_REDUCE_MAX_STEPS(graph, ctx_ptr) \
    (lv00_adaptive_threshold_compute(ctx_ptr))

/**
 * @brief 便捷宏：获取重写-求解迭代的最大次数阈值
 */
#define LV00_ADAPTIVE_REWRITE_SOLVE_MAX_ITER(graph, ctx_ptr) \
    (lv00_adaptive_threshold_compute(ctx_ptr))

#ifdef __cplusplus
}
#endif

#endif /* LV00_ADAPTIVE_THRESHOLD_H */
