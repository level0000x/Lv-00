/**
 * @file engine_scheduler.h
 * @brief 多引擎调度框架 —— 后端注册、自动路由与分发求解
 *
 * @details 本模块实现 Lv-00 的多求解引擎调度层，参考 polymake 的
 *          多后端架构。核心功能包括：
 *
 *          1. 后端注册表管理 —— 运行时注册/注销/查询可用的求解后端
 *          2. 约束图特征分析 —— 提取变量数、非线性度、量词存在性等特征
 *          3. 自动路由决策 —— 基于启发式规则为给定约束图选择最佳后端
 *          4. 分发求解 —— 透明地将求解请求路由到选定的后端
 *          5. 回退链 —— 当首选后端不可用/失败时，自动尝试次选后端
 *
 *          自动路由规则（按优先级降序）：
 *          - 含量词约束（CONTAINMENT/卷绕数）且 cvc5 可用 → SMT_CVC5
 *          - 非线性约束占比 > 30% 且 SMT 可用       → 最佳可用 SMT
 *          - 变量数 < 50                              → GROEBNER
 *          - 变量数 >= 50 且非线性约束 > 0            → 最佳可用 SMT
 *          - 默认                                     → GROEBNER（最稳定）
 *
 *          调度器工作流：
 *          1. scheduler_analyze_graph()     —— 提取图特征
 *          2. scheduler_select_backend()    —— 基于特征自动选后端
 *          3. scheduler_solve()             —— 创建求解器并求解
 *          4. 结果转换                       —— SMTSolverResult → GroebnerResult（向后兼容）
 *
 * @note 调度器本身不实现求解逻辑，它纯粹是路由和编排层。
 *       所有求解能力由注册的后端提供。
 *
 * @author Lv-00 Project
 * @version 3.2.0
 */

#ifndef LV00_ENGINE_SCHEDULER_H
#define LV00_ENGINE_SCHEDULER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "smt_backend.h"

/* ============================================================
 * 前向声明
 * ============================================================ */

/** @brief 约束图（来自 constraint_graph.h） */
struct ConstraintGraph;
typedef struct ConstraintGraph ConstraintGraph;

/** @brief Lv-00 主引擎（来自 engine.h） */
struct LV00Engine;
typedef struct LV00Engine LV00Engine;

/** @brief Gröbner 求解结果（来自 solver.h） */
struct GroebnerResult;
typedef struct GroebnerResult GroebnerResult;

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 调度器可注册的最大路由规则数 */
#define SCHEDULER_MAX_ROUTING_RULES 32

/** @brief 调度器可管理的最大后端实例数 */
#define SCHEDULER_MAX_BACKEND_INSTANCES 8

/** @brief 回退链的最大深度 */
#define SCHEDULER_MAX_FALLBACK_DEPTH 4

/* ============================================================
 * 图特征分析
 * ============================================================ */

/**
 * @brief 约束图的特征向量
 *
 * 从约束图中提取的结构化特征，用于路由决策。
 * 所有计数值均不含被标记为已删除的节点/约束。
 */
typedef struct GraphFeatures {
    int total_nodes;       /**< 节点总数 */
    int total_constraints; /**< 约束总数 */

    /* 变量相关 */
    int variable_nodes; /**< 可求解变量节点数（含符号坐标的点节点） */
    int fixed_nodes;    /**< 已确定坐标的点节点数 */
    int port_nodes;     /**< 端口节点数 */
    int block_nodes;    /**< 函数块节点数 */

    /* 约束类型分布 */
    int incidence_constraints;    /**< INCIDENCE 约束数 */
    int betweenness_constraints;  /**< BETWEENNESS 约束数 */
    int intersection_constraints; /**< INTERSECTION 约束数 */
    int containment_constraints;  /**< CONTAINMENT 约束数 */
    int connection_constraints;   /**< CONNECTION 约束数 */

    /* 非线性特征 */
    int nonlinear_constraints; /**< 含非线性项（degree > 1）的约束数 */
    double nonlinear_ratio;    /**< 非线性约束占比（0.0 ~ 1.0） */

    /* 量词特征 */
    bool has_quantifier_like;   /**< 含类量词约束（卷绕数、区域包含等） */
    bool has_boolean_variables; /**< 含布尔变量的约束 */

    /* 规模特征 */
    int estimated_equation_count; /**< 估计的方程数量 */
    int estimated_degree_max;     /**< 估计的最高多项式度数 */

    /* 元数据 */
    int64_t analysis_time_us; /**< 特征分析耗时（微秒） */
} GraphFeatures;

/**
 * @brief 分析约束图并提取特征向量
 *
 * 遍历约束图中的所有节点和约束，计算结构化特征。
 * 该函数设计为轻量级，时间复杂度 O(N + C)（N=节点数, C=约束数）。
 *
 * @param[in]  graph      约束图
 * @param[out] features   输出的特征向量（调用者分配）
 * @return 成功返回 0，失败返回 -1（graph 或 features 为 NULL）。
 */
int scheduler_analyze_graph(const ConstraintGraph *graph, GraphFeatures *features);

/**
 * @brief 初始化一个归零的 GraphFeatures 结构
 *
 * @param[out] features  要初始化的特征向量
 */
void scheduler_features_init(GraphFeatures *features);

/* ============================================================
 * 路由规则
 * ============================================================ */

/**
 * @brief 路由规则的匹配条件类型
 */
typedef enum {
    ROUTE_COND_NONE = 0,           /**< 无条件（始终匹配，作为默认/兜底规则） */
    ROUTE_COND_VAR_COUNT_LE,       /**< 变量数 <= 阈值 */
    ROUTE_COND_VAR_COUNT_GE,       /**< 变量数 >= 阈值 */
    ROUTE_COND_NONLINEAR_RATIO_GE, /**< 非线性约束占比 >= 阈值 */
    ROUTE_COND_HAS_QUANTIFIER,     /**< 含类量词约束 */
    ROUTE_COND_HAS_BOOLEAN,        /**< 含布尔变量 */
    ROUTE_COND_DEGREE_GE,          /**< 最高度数 >= 阈值 */
    ROUTE_COND_BACKEND_AVAILABLE,  /**< 指定后端可用 */
    ROUTE_COND_ALWAYS              /**< 始终匹配（无条件规则的另一名称） */
} RouteConditionType;

/**
 * @brief 路由规则的逻辑组合方式
 *
 * 当一条规则有多个条件时，指定条件之间的组合方式。
 */
typedef enum {
    ROUTE_COMBINE_AND, /**< 所有条件必须同时满足 */
    ROUTE_COMBINE_OR   /**< 满足任意一个条件即可 */
} RouteCombineMode;

/**
 * @brief 单个路由条件
 *
 * 描述一个匹配谓词，如 "变量数 <= 50" 或 "非线性占比 >= 0.3"。
 */
typedef struct RouteCondition {
    RouteConditionType type;   /**< 条件类型 */
    double threshold;          /**< 阈值（用于数值比较条件） */
    SolverBackendType backend; /**< 后端类型（用于 BACKEND_AVAILABLE 条件） */
} RouteCondition;

/**
 * @brief 路由规则
 *
 * 一条路由规则由名称、优先级、一组条件和目标后端组成。
 * 当所有条件满足时，请求被路由到目标后端。
 *
 * 规则评估顺序：
 * - 所有规则按 priority 降序排列
 * - 同优先级按注册顺序排列
 * - 第一个完全匹配的规则被选中
 */
typedef struct RoutingRule {
    char name[64];                    /**< 规则名称（用于调试和日志） */
    int priority;                     /**< 优先级（数值越低越优先） */
    bool enabled;                     /**< 是否启用此规则 */
    RouteCondition conditions[4];     /**< 匹配条件数组 */
    int condition_count;              /**< 条件数量 */
    RouteCombineMode combine_mode;    /**< 条件组合方式 */
    SolverBackendType target_backend; /**< 满足条件时路由到的目标后端 */
} RoutingRule;

/**
 * @brief 创建一条默认路由规则（名称、条件、后端均为空）
 *
 * 调用者应在创建后手动填充字段。
 *
 * @return 新分配的 RoutingRule，失败返回 NULL。调用者需用 scheduler_rule_destroy() 释放。
 */
RoutingRule *scheduler_rule_create(void);

/**
 * @brief 销毁路由规则
 *
 * @param[in,out] rule  要销毁的规则（可为 NULL）
 */
void scheduler_rule_destroy(RoutingRule *rule);

/**
 * @brief 评估路由规则是否匹配给定图特征
 *
 * @param[in] rule      路由规则
 * @param[in] features  图特征向量
 * @return true 匹配，false 不匹配或 rule 被禁用。
 */
bool scheduler_rule_matches(const RoutingRule *rule, const GraphFeatures *features);

/* ============================================================
 * 引擎调度器
 * ============================================================ */

/**
 * @brief 调度器统计信息
 *
 * 记录调度器的运行统计，用于性能监控和调试。
 */
typedef struct SchedulerStats {
    int64_t total_solves;            /**< 总求解次数 */
    int64_t total_solve_time_us;     /**< 总求解时间（微秒） */
    int64_t max_solve_time_us;       /**< 单次最大求解时间 */
    int64_t fallback_count;          /**< 回退次数 */
    int64_t selection_miss;          /**< 路由选择不理想次数 */
    int backend_solve_counts[COUNT]; /**< 各后端的求解次数分布 */
} SchedulerStats;

/**
 * @brief 引擎调度器
 *
 * 多求解引擎的中央调度单元，管理后端注册表、路由规则和调度逻辑。
 *
 * 典型用法：
 * ```
 * EngineScheduler *sch = scheduler_create();
 * // 注册可用后端
 * scheduler_register_backend(sch, GROEBNER, NULL);
 * scheduler_register_backend(sch, SMT_Z3, NULL);
 * // 选择并求解
 * SMTSolverResult result;
 * scheduler_solve(sch, graph, &result);
 * // 清理
 * scheduler_destroy(sch);
 * ```
 */
typedef struct EngineScheduler {
    /* 后端注册 */
    SMTBackendRegistry *registry;                              /**< 后端注册表 */
    SMTSolver *backend_cache[SCHEDULER_MAX_BACKEND_INSTANCES]; /**< 后端实例缓存 */
    int backend_cache_count;                                   /**< 当前缓存的后端实例数 */

    /* 路由规则 */
    RoutingRule *routing_rules[SCHEDULER_MAX_ROUTING_RULES]; /**< 路由规则表 */
    int routing_rule_count;                                  /**< 当前规则数 */
    SolverBackendType default_backend;                       /**< 兜底后端（规则全不匹配时使用） */

    /* 回退策略 */
    SolverBackendType fallback_chain[SCHEDULER_MAX_FALLBACK_DEPTH]; /**< 回退链 */
    int fallback_depth;                                             /**< 回退链深度 */
    bool enable_fallback;                                           /**< 是否启用失败回退 */

    /* 配置 */
    bool auto_create_backends; /**< 自动按需创建后端实例 */

    /* 统计 */
    SchedulerStats stats; /**< 运行统计 */
} EngineScheduler;

/* ============================================================
 * 调度器生命周期
 * ============================================================ */

/**
 * @brief 创建引擎调度器实例
 *
 * 自动初始化后端注册表并注册所有已链接的后端。
 * 默认配置：
 * - default_backend = GROEBNER
 * - enable_fallback  = true
 * - auto_create_backends = true
 * - fallback_chain   = [GROEBNER]（深度 1）
 * - 预置标准路由规则
 *
 * @return 新分配的 EngineScheduler，失败返回 NULL。
 *         调用者需用 scheduler_destroy() 释放。
 */
EngineScheduler *scheduler_create(void);

/**
 * @brief 销毁引擎调度器并释放所有资源
 *
 * 销毁所有缓存的后端实例、路由规则和注册表。
 * 安全接受 NULL 输入。
 *
 * @param[in,out] scheduler  要销毁的调度器（可为 NULL）
 */
void scheduler_destroy(EngineScheduler *scheduler);

/**
 * @brief 重置调度器到刚创建的初始状态
 *
 * 清除所有后端缓存、自定义路由规则和统计信息。
 * 保留注册表和后端可用性信息。
 *
 * @param[in,out] scheduler  调度器
 */
void scheduler_reset(EngineScheduler *scheduler);

/* ============================================================
 * 后端注册管理
 * ============================================================ */

/**
 * @brief 向调度器注册一个后端
 *
 * 将后端类型及其工厂函数注册到调度器的注册表中。
 * 如果后端已经被编译链接（通过 smtsolver_is_backend_available 检测），
 * 则 available 自动设为 true。
 *
 * @param[in,out] scheduler    调度器
 * @param[in]     type         后端类型
 * @param[in]     create_func  工厂创建函数（可为 NULL，表示使用默认工厂）
 * @param[in]     priority     调度优先级（数值越低越优先）
 * @param[in]     description  后端描述文本（可为 NULL）
 * @return 成功返回 0，失败返回 -1（type 无效或注册表已满）。
 */
int scheduler_register_backend(EngineScheduler *scheduler, SolverBackendType type, SMTSolverCreateFunc create_func,
                               int priority, const char *description);

/**
 * @brief 从调度器注销一个后端
 *
 * 从注册表中移除后端条目，并销毁对应的缓存实例（如果有）。
 *
 * @param[in,out] scheduler  调度器
 * @param[in]     type       要注销的后端类型
 * @return 成功返回 0，后端未注册返回 -1。
 */
int scheduler_unregister_backend(EngineScheduler *scheduler, SolverBackendType type);

/**
 * @brief 获取调度器中已注册且可用的后端列表
 *
 * @param[in]  scheduler   调度器
 * @param[out] out_types   输出的后端类型数组（调用者分配，大小至少为 COUNT）
 * @param[in]  max_count   数组容量
 * @return 实际输出的后端数量，失败返回 -1。
 */
int scheduler_list_available_backends(const EngineScheduler *scheduler, SolverBackendType *out_types, int max_count);

/* ============================================================
 * 路由规则管理
 * ============================================================ */

/**
 * @brief 向调度器添加一条路由规则
 *
 * 规则按优先级排序插入。如果规则名已存在，则替换旧规则。
 *
 * @param[in,out] scheduler  调度器
 * @param[in]     rule       路由规则（调度器取得所有权，调用者不应再修改/释放）
 * @return 成功返回 0，规则表已满返回 -1。
 */
int scheduler_add_routing_rule(EngineScheduler *scheduler, RoutingRule *rule);

/**
 * @brief 从调度器移除指定名称的路由规则
 *
 * @param[in,out] scheduler  调度器
 * @param[in]     name       规则名称
 * @return 成功返回 0，未找到返回 -1。
 */
int scheduler_remove_routing_rule(EngineScheduler *scheduler, const char *name);

/**
 * @brief 加载预置的标准路由规则集
 *
 * 标准规则（按优先级升序，即数值越小越优先）：
 * 1. "quantifier-cvc5":    含类量词约束 + cvc5可用 → SMT_CVC5    (priority=0)
 * 2. "nonlinear-smt":      非线性占比>=0.3 + 有SMT可用 → 最佳SMT (priority=10)
 * 3. "small-groebner":     变量数<50                        → GROEBNER  (priority=20)
 * 4. "large-smt":          变量数>=50 + 非线性>0              → 最佳SMT   (priority=30)
 * 5. "default-groebner":   无条件                            → GROEBNER  (priority=100)
 *
 * @param[in,out] scheduler  调度器（现有规则不会被清除）
 * @return 加载的规则数量，失败返回 -1。
 */
int scheduler_load_preset_rules(EngineScheduler *scheduler);

/* ============================================================
 * 后端选择
 * ============================================================ */

/**
 * @brief 为给定的约束图自动选择最佳后端
 *
 * 执行步骤：
 * 1. 分析约束图特征（scheduler_analyze_graph）
 * 2. 按优先级顺序评估所有启用的路由规则
 * 3. 返回第一个完全匹配规则的目标后端
 * 4. 若无规则匹配，返回 default_backend
 *
 * @param[in] scheduler    调度器
 * @param[in] graph        约束图
 * @param[out] out_reason  输出选择原因的字符串（可为 NULL）
 *                         格式如 "匹配规则 'small-groebner': 变量数=12 < 50"
 * @param[in] reason_size  原因缓冲区大小
 * @return 选定的后端类型。如果所有后端均不可用或图无效，返回 COUNT。
 */
SolverBackendType scheduler_select_backend(const EngineScheduler *scheduler, const ConstraintGraph *graph,
                                           char *out_reason, size_t reason_size);

/* ============================================================
 * 分发求解
 * ============================================================ */

/**
 * @brief 分发求解 —— 自动选择后端并执行完整求解管线
 *
 * 这是调度器的主入口函数，封装了完整的求解流程：
 * 1. scheduler_select_backend() —— 自动选择最佳后端
 * 2. smtsolver_create()         —— 创建/获取后端实例
 * 3. smtsolver_solve()           —— 执行求解管线
 * 4. 结果转换                     —— SMTSolverResult → 结构化输出
 *
 * 故障处理：
 * - 如果首选后端不可用，按 enable_fallback 决定是否尝试回退链
 * - 回退链按优先级依次尝试，直到找到可用的后端
 * - 如果所有回退均失败，返回错误
 *
 * @param[in]  scheduler    调度器
 * @param[in]  graph        约束图
 * @param[out] out_result   输出求解结果（调用者负责用 smtsolver_result_free 释放）
 * @return 求解器状态（参考 SolverStatus 中的值）：
 *         - 0 (SOLVER_OK)：求解成功
 *         - 非零：各种错误状态
 *         此返回值兼容现有 solve_algebraic_system() 的 SolverStatus 语义。
 */
int scheduler_solve(EngineScheduler *scheduler, const ConstraintGraph *graph, SMTSolverResult *out_result);

/**
 * @brief 使用指定后端分发求解
 *
 * 跳过自动路由选择，直接使用指定后端。如果指定后端不可用且
 * enable_fallback 为 true，则回退到回退链。
 *
 * @param[in]  scheduler    调度器
 * @param[in]  graph        约束图
 * @param[in]  backend_type 显式指定的后端类型
 * @param[out] out_result   输出求解结果
 * @return 求解器状态。
 */
int scheduler_solve_with_backend(EngineScheduler *scheduler, const ConstraintGraph *graph,
                                 SolverBackendType backend_type, SMTSolverResult *out_result);

/* ============================================================
 * 结果转换（向后兼容）
 * ============================================================ */

/**
 * @brief 将 SMTSolverResult 转换为 Gröbner 兼容的 GroebnerResult
 *
 * 用于桥接新的 SMT 后端和现有的求解器接口。
 * 将 SMT 变量赋值映射回符号坐标的 GroebnerResult 格式。
 *
 * 转换规则：
 * - SMT_RESULT_SAT 且 assignments 非空 → GroebnerResult（含解）
 * - SMT_RESULT_UNSAT                   → GroebnerResult（空解，overdetermined）
 * - 其他                                → NULL（转换失败）
 *
 * @param[in]  smt_result  SMT 求解结果
 * @param[in]  graph       约束图（用于查找变量节点和坐标信息）
 * @return 新分配的 GroebnerResult（调用者需用 groebner_result_free 释放）。
 *         转换失败返回 NULL。
 */
GroebnerResult *scheduler_convert_smt_to_groebner(const SMTSolverResult *smt_result, const ConstraintGraph *graph);

/**
 * @brief 自动选择后端、求解并返回 Gröbner 兼容结果
 *
 * 便捷函数，组合了 scheduler_solve() 和 scheduler_convert_smt_to_groebner()。
 * 适合逐步从 Gröbner 迁移到 SMT 后端的过渡期使用。
 *
 * @param[in]  scheduler    调度器
 * @param[in]  graph        约束图
 * @param[out] out_result   输出的 Gröbner 兼容结果
 * @return 求解器状态（兼容 SolverStatus）。
 */
int scheduler_solve_groebner_compat(EngineScheduler *scheduler, const ConstraintGraph *graph,
                                    GroebnerResult **out_result);

/* ============================================================
 * 调度器配置
 * ============================================================ */

/**
 * @brief 设置默认后端
 *
 * 当所有路由规则都不匹配时，使用此前端。
 *
 * @param[in,out] scheduler  调度器
 * @param[in]     type       默认后端类型
 */
void scheduler_set_default_backend(EngineScheduler *scheduler, SolverBackendType type);

/**
 * @brief 设置回退策略
 *
 * @param[in,out] scheduler      调度器
 * @param[in]     enable         是否启用失败回退
 * @param[in]     fallback_types  回退链（按优先级排序的数组）
 * @param[in]     depth          回退链深度
 */
void scheduler_set_fallback_policy(EngineScheduler *scheduler, bool enable, const SolverBackendType *fallback_types,
                                   int depth);

/**
 * @brief 设置是否自动按需创建后端实例
 *
 * 当为 true 时，scheduler_solve() 自动创建需要的后端实例；
 * 当为 false 时，调用者需通过 scheduler_register_backend() 预先注册。
 *
 * @param[in,out] scheduler      调度器
 * @param[in]     auto_create    是否自动创建
 */
void scheduler_set_auto_create(EngineScheduler *scheduler, bool auto_create);

/* ============================================================
 * 统计与诊断
 * ============================================================ */

/**
 * @brief 获取调度器统计信息的快照
 *
 * @param[in]  scheduler  调度器
 * @param[out] stats      输出统计快照
 */
void scheduler_get_stats(const EngineScheduler *scheduler, SchedulerStats *stats);

/**
 * @brief 重置调度器统计信息
 *
 * @param[in,out] scheduler  调度器
 */
void scheduler_reset_stats(EngineScheduler *scheduler);

/**
 * @brief 打印调度器诊断报告到字符串缓冲区
 *
 * 包括：注册的后端、路由规则表、统计摘要、缓存状态。
 *
 * @param[in]  scheduler  调度器
 * @param[out] buf        输出缓冲区
 * @param[in]  buf_size   缓冲区大小
 * @return 实际写入的字符数（不含终止符）。缓冲区不足返回所需大小。
 */
int scheduler_diagnose(const EngineScheduler *scheduler, char *buf, size_t buf_size);

#ifdef __cplusplus
}
#endif

#endif /* LV00_ENGINE_SCHEDULER_H */
