/**
 * @file proof_engine_enhanced.h
 * @brief 增强证明引擎 —— 反证法完善与逻辑溯源树
 *
 * @details 提供增强的证明引擎功能：
 *   1. 反证法证明：完整的矛盾推导路径
 *   2. 逻辑溯源树：记录证明的完整依赖链
 *   3. 证明策略调度：自动选择最优证明策略
 *   4. 证明验证：独立验证证明正确性
 *   5. 证明优化：简化证明步骤
 *
 * @author Lv-00 Project
 * @version 1.1.0
 */

#ifndef LV00_PROOF_ENGINE_ENHANCED_H
#define LV00_PROOF_ENGINE_ENHANCED_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "axiom_rule_engine.h"
#include "constraint_graph.h"
#include "proof.h"

/* ============== 配置常量 ============== */

/** 最大证明深度 */
#define LV00_PROOF_MAX_DEPTH 100

/** 最大分支数 */
#define LV00_PROOF_MAX_BRANCHES 64

/** 最大策略数 */
#define LV00_PROOF_MAX_STRATEGIES 16

/** 溯源树最大深度 */
#define LV00_TRACE_TREE_MAX_DEPTH 50

/* ============== 前向声明 ============== */

typedef struct Lv00ProofTraceNode Lv00ProofTraceNode;
typedef struct Lv00ProofTraceTree Lv00ProofTraceTree;
typedef struct Lv00ContradictionPath Lv00ContradictionPath;
typedef struct Lv00ProofStrategy Lv00ProofStrategy;
typedef struct Lv00ProofEngine Lv00ProofEngine;

/* ============== 溯源树节点 ============== */

/**
 * @brief 溯源节点类型
 */
typedef enum {
    TRACE_NODE_AXIOM,           /**< 公理 */
    TRACE_NODE_DEFINITION,      /**< 定义 */
    TRACE_NODE_THEOREM,         /**< 定理 */
    TRACE_NODE_LEMMA,           /**< 引理 */
    TRACE_NODE_HYPOTHESIS,      /**< 假设 */
    TRACE_NODE_DERIVATION,      /**< 推导 */
    TRACE_NODE_CONTRADICTION,   /**< 矛盾 */
    TRACE_NODE_GOAL             /**< 目标 */
} Lv00TraceNodeType;

/**
 * @brief 溯源节点状态
 */
typedef enum {
    TRACE_STATUS_UNEXPLORED,    /**< 未探索 */
    TRACE_STATUS_EXPLORING,     /**< 探索中 */
    TRACE_STATUS_PROVED,        /**< 已证明 */
    TRACE_STATUS_DISPROVED,     /**< 已证伪 */
    TRACE_STATUS_BLOCKED        /**< 阻塞 */
} Lv00TraceNodeStatus;

/**
 * @brief 溯源树节点
 */
struct Lv00ProofTraceNode {
    /* 基本信息 */
    uint32_t id;                        /**< 节点 ID */
    Lv00TraceNodeType type;             /**< 节点类型 */
    Lv00TraceNodeStatus status;         /**< 节点状态 */
    char label[256];                    /**< 节点标签 */
    char description[512];              /**< 详细描述 */

    /* 证明内容 */
    Proposition *proposition;           /**< 关联命题 */
    ProofStep *step;                    /**< 关联证明步骤 */
    Lv00Rule *rule;                     /**< 使用的规则 */

    /* 信任颜色 */
    TrustColor trust_color;             /**< 信任颜色 */

    /* 树结构 */
    Lv00ProofTraceNode *parent;         /**< 父节点 */
    Lv00ProofTraceNode **children;      /**< 子节点数组 */
    uint32_t child_count;               /**< 子节点数量 */
    uint32_t child_capacity;            /**< 子节点容量 */

    /* 依赖关系 */
    uint32_t *dependency_ids;           /**< 依赖节点 ID */
    uint32_t dependency_count;          /**< 依赖数量 */

    /* 元数据 */
    int depth;                          /**< 树深度 */
    int64_t create_time_ns;             /**< 创建时间 */
    int64_t complete_time_ns;           /**< 完成时间 */
    double elapsed_ms;                  /**< 耗时（毫秒） */
};

/**
 * @brief 溯源树
 */
struct Lv00ProofTraceTree {
    Lv00ProofTraceNode *root;           /**< 根节点 */
    Lv00ProofTraceNode **all_nodes;     /**< 所有节点（用于遍历） */
    uint32_t node_count;                /**< 节点总数 */
    uint32_t node_capacity;             /**< 节点容量 */

    /* 统计信息 */
    uint32_t proved_count;              /**< 已证明节点数 */
    uint32_t disproved_count;           /**< 已证伪节点数 */
    uint32_t max_depth;                 /**< 最大深度 */

    /* 状态 */
    bool is_complete;                   /**< 是否完成 */
    TrustColor final_color;             /**< 最终信任颜色 */
};

/* ============== 反证法路径 ============== */

/**
 * @brief 矛盾类型
 */
typedef enum {
    CONTRADICTION_TYPE_P_AND_NOT_P,     /**< P ∧ ¬P */
    CONTRADICTION_TYPE_FALSE_DERIVED,   /**< 推导出假 */
    CONTRADICTION_TYPE_CYCLE,           /**< 循环依赖 */
    CONTRADICTION_TYPE_TYPE_MISMATCH,   /**< 类型不匹配 */
    CONTRADICTION_TYPE_ARITHMETIC,      /**< 算术矛盾 */
    CONTRADICTION_TYPE_GEOMETRIC        /**< 几何矛盾 */
} Lv00ContradictionType;

/**
 * @brief 反证法路径节点
 */
typedef struct {
    uint32_t id;                        /**< 节点 ID */
    char statement[512];                /**< 陈述 */
    char justification[256];            /**< 理由 */
    bool is_assumption;                 /**< 是否为假设 */
    bool leads_to_contradiction;        /**< 是否导致矛盾 */
} Lv00ContradictionPathNode;

/**
 * @brief 反证法路径
 */
struct Lv00ContradictionPath {
    Lv00ContradictionPathNode *nodes;   /**< 节点数组 */
    uint32_t node_count;                /**< 节点数量 */
    uint32_t node_capacity;             /**< 节点容量 */

    Lv00ContradictionType type;         /**< 矛盾类型 */
    char contradiction_desc[512];       /**< 矛盾描述 */

    Lv00ProofTraceTree *trace_tree;     /**< 完整溯源树 */
    bool is_valid;                      /**< 是否有效 */
};

/* ============== 证明策略 ============== */

/**
 * @brief 策略类型
 */
typedef enum {
    STRATEGY_DIRECT,            /**< 直接证明 */
    STRATEGY_CONTRADICTION,     /**< 反证法 */
    STRATEGY_CONTRAPOSITIVE,    /**< 逆否证明 */
    STRATEGY_INDUCTION,         /**< 数学归纳法 */
    STRATEGY_CASES,             /**< 分情况讨论 */
    STRATEGY_CONSTRUCTION,      /**< 构造性证明 */
    STRATEGY_UNFOLDING,         /**< 定义展开 */
    STRATEGY_BACKWARD,          /**< 逆向推理 */
    STRATEGY_FORWARD,           /**< 正向推理 */
    STRATEGY_HYBRID             /**< 混合策略 */
} Lv00StrategyType;

/**
 * @brief 策略状态
 */
typedef enum {
    STRATEGY_STATUS_PENDING,    /**< 待执行 */
    STRATEGY_STATUS_RUNNING,    /**< 执行中 */
    STRATEGY_STATUS_SUCCESS,    /**< 成功 */
    STRATEGY_STATUS_FAILED,     /**< 失败 */
    STRATEGY_STATUS_TIMEOUT     /**< 超时 */
} Lv00StrategyStatus;

/**
 * @brief 证明策略
 */
struct Lv00ProofStrategy {
    Lv00StrategyType type;              /**< 策略类型 */
    char name[64];                      /**< 策略名称 */
    char description[256];              /**< 策略描述 */

    Lv00StrategyStatus status;          /**< 策略状态 */
    double priority;                    /**< 优先级（越高越优先） */

    /* 执行信息 */
    int64_t start_time_ns;              /**< 开始时间 */
    int64_t end_time_ns;                /**< 结束时间 */
    double elapsed_ms;                  /**< 耗时 */

    /* 结果 */
    Lv00ProofTraceTree *trace_tree;     /**< 生成的溯源树 */
    uint32_t step_count;                /**< 步骤数 */
    char error_message[512];            /**< 错误消息 */

    /* 适用性检查 */
    bool (*is_applicable)(const Proposition *prop, const ConstraintGraph *graph);
    bool (*execute)(Lv00ProofEngine *engine, const Proposition *prop);
};

/* ============== 证明引擎 ============== */

/**
 * @brief 证明引擎配置
 */
typedef struct {
    uint32_t max_depth;                 /**< 最大证明深度 */
    uint32_t max_branches;              /**< 最大分支数 */
    uint32_t timeout_ms;                /**< 超时时间（毫秒） */
    bool enable_parallel;               /**< 启用并行证明 */
    bool enable_cache;                  /**< 启用结果缓存 */
    bool verify_proofs;                 /**< 验证证明 */
    bool optimize_proofs;               /**< 优化证明 */
} Lv00ProofEngineConfig;

/**
 * @brief 证明引擎
 */
struct Lv00ProofEngine {
    /* 配置 */
    Lv00ProofEngineConfig config;

    /* 规则库 */
    Lv00RuleLibrary *rule_library;

    /* 策略 */
    Lv00ProofStrategy strategies[LV00_PROOF_MAX_STRATEGIES];
    uint32_t strategy_count;

    /* 当前状态 */
    ConstraintGraph *graph;
    ProofNavigator *navigator;
    Lv00ProofTraceTree *current_trace;

    /* 统计 */
    uint64_t total_proofs;              /**< 总证明次数 */
    uint64_t success_proofs;            /**< 成功次数 */
    double avg_proof_time_ms;           /**< 平均证明时间 */

    /* 缓存 */
    void *proof_cache;
};

/* ============== 溯源树操作 ============== */

/**
 * @brief 创建溯源树
 * @param root_prop 根命题
 * @return 新溯源树
 */
LV00_PUBLIC_API Lv00ProofTraceTree *lv00_trace_tree_create(Proposition *root_prop);

/**
 * @brief 销毁溯源树
 * @param tree 树指针
 */
LV00_PUBLIC_API void lv00_trace_tree_destroy(Lv00ProofTraceTree *tree);

/**
 * @brief 创建溯源节点
 * @param type 节点类型
 * @param label 节点标签
 * @return 新节点
 */
LV00_PUBLIC_API Lv00ProofTraceNode *lv00_trace_node_create(Lv00TraceNodeType type, const char *label);

/**
 * @brief 销毁溯源节点
 * @param node 节点指针
 */
LV00_PUBLIC_API void lv00_trace_node_destroy(Lv00ProofTraceNode *node);

/**
 * @brief 添加子节点
 * @param parent 父节点
 * @param child 子节点
 * @return 是否成功
 */
LV00_PUBLIC_API int lv00_trace_node_add_child(Lv00ProofTraceNode *parent, Lv00ProofTraceNode *child);

/**
 * @brief 设置节点状态
 * @param node 节点
 * @param status 状态
 */
LV00_PUBLIC_API void lv00_trace_node_set_status(Lv00ProofTraceNode *node, Lv00TraceNodeStatus status);

/**
 * @brief 计算节点信任颜色
 * @param node 节点
 * @return 信任颜色
 */
LV00_PUBLIC_API TrustColor lv00_trace_node_compute_color(Lv00ProofTraceNode *node);

/**
 * @brief 查找路径
 * @param tree 溯源树
 * @param from_id 起始节点 ID
 * @param to_id 目标节点 ID
 * @param out_path 输出路径节点数组
 * @param max_length 最大路径长度
 * @return 实际路径长度
 */
LV00_PUBLIC_API uint32_t lv00_trace_tree_find_path(const Lv00ProofTraceTree *tree,
                                    uint32_t from_id, uint32_t to_id,
                                    Lv00ProofTraceNode **out_path,
                                    uint32_t max_length);

/**
 * @brief 导出溯源树为 DOT 格式
 * @param tree 溯源树
 * @param path 输出文件路径
 * @return 是否成功
 */
LV00_PUBLIC_API int lv00_trace_tree_export_dot(const Lv00ProofTraceTree *tree, const char *path);

/**
 * @brief 导出溯源树为 JSON
 * @param tree 溯源树
 * @return JSON 字符串
 */
LV00_PUBLIC_API char *lv00_trace_tree_to_json(const Lv00ProofTraceTree *tree);

/* ============== 反证法 ============== */

/**
 * @brief 执行反证法证明
 * @param engine 证明引擎
 * @param goal 目标命题
 * @param max_steps 最大步骤数
 * @param out_path 输出矛盾路径
 * @return 是否成功
 */
LV00_PUBLIC_API int lv00_engine_proof_by_contradiction(Lv00ProofEngine *engine,
                                  const Proposition *goal,
                                  uint32_t max_steps,
                                  Lv00ContradictionPath **out_path);

/**
 * @brief 创建矛盾路径
 * @return 新路径
 */
LV00_PUBLIC_API Lv00ContradictionPath *lv00_contradiction_path_create(void);

/**
 * @brief 销毁矛盾路径
 * @param path 路径指针
 */
LV00_PUBLIC_API void lv00_contradiction_path_destroy(Lv00ContradictionPath *path);

/**
 * @brief 添加节点到矛盾路径
 * @param path 路径
 * @param statement 陈述
 * @param justification 理由
 * @param is_assumption 是否为假设
 * @return 节点 ID
 */
LV00_PUBLIC_API uint32_t lv00_contradiction_path_add_node(Lv00ContradictionPath *path,
                                           const char *statement,
                                           const char *justification,
                                           bool is_assumption);

/**
 * @brief 检测矛盾
 * @param graph 约束图
 * @param nav 证明导航器
 * @param out_type 输出矛盾类型
 * @param out_desc 输出矛盾描述
 * @return 是否检测到矛盾
 */
LV00_PUBLIC_API int lv00_detect_contradiction(const ConstraintGraph *graph,
                                const ProofNavigator *nav,
                                Lv00ContradictionType *out_type,
                                char *out_desc);

/**
 * @brief 验证反证法证明
 * @param path 矛盾路径
 * @return 是否有效
 */
LV00_PUBLIC_API int lv00_contradiction_path_validate(Lv00ContradictionPath *path);

/* ============== 证明引擎 ============== */

/**
 * @brief 创建证明引擎
 * @param config 配置
 * @return 新引擎
 */
LV00_PUBLIC_API Lv00ProofEngine *lv00_proof_engine_create(const Lv00ProofEngineConfig *config);

/**
 * @brief 销毁证明引擎
 * @param engine 引擎指针
 */
LV00_PUBLIC_API void lv00_proof_engine_destroy(Lv00ProofEngine *engine);

/**
 * @brief 设置规则库
 * @param engine 引擎
 * @param library 规则库
 */
LV00_PUBLIC_API void lv00_proof_engine_set_rule_library(Lv00ProofEngine *engine,
                                         Lv00RuleLibrary *library);

/**
 * @brief 注册证明策略
 * @param engine 引擎
 * @param strategy 策略
 * @return 是否成功
 */
LV00_PUBLIC_API int lv00_proof_engine_register_strategy(Lv00ProofEngine *engine,
                                          const Lv00ProofStrategy *strategy);

/**
 * @brief 执行证明
 * @param engine 引擎
 * @param goal 目标命题
 * @param graph 约束图
 * @param out_trace 输出溯源树
 * @return 是否成功
 */
LV00_PUBLIC_API int lv00_proof_engine_prove(Lv00ProofEngine *engine,
                              const Proposition *goal,
                              ConstraintGraph *graph,
                              Lv00ProofTraceTree **out_trace);

/**
 * @brief 自动选择策略并证明
 * @param engine 引擎
 * @param goal 目标命题
 * @param graph 约束图
 * @param out_trace 输出溯源树
 * @param out_strategy 输出使用的策略
 * @return 是否成功
 */
LV00_PUBLIC_API int lv00_proof_engine_auto_prove(Lv00ProofEngine *engine,
                                   const Proposition *goal,
                                   ConstraintGraph *graph,
                                   Lv00ProofTraceTree **out_trace,
                                   Lv00StrategyType *out_strategy);

/**
 * @brief 使用指定策略证明
 * @param engine 引擎
 * @param goal 目标命题
 * @param graph 约束图
 * @param strategy_type 策略类型
 * @param out_trace 输出溯源树
 * @return 是否成功
 */
LV00_PUBLIC_API int lv00_proof_engine_prove_with_strategy(Lv00ProofEngine *engine,
                                            const Proposition *goal,
                                            ConstraintGraph *graph,
                                            Lv00StrategyType strategy_type,
                                            Lv00ProofTraceTree **out_trace);

/**
 * @brief 获取引擎统计信息
 * @param engine 引擎
 * @param out_total 输出总证明次数
 * @param out_success 输出成功次数
 * @param out_avg_time 输出平均时间
 */
LV00_PUBLIC_API void lv00_proof_engine_get_stats(const Lv00ProofEngine *engine,
                                  uint64_t *out_total,
                                  uint64_t *out_success,
                                  double *out_avg_time);

/* ============== 证明验证 ============== */

/**
 * @brief 验证结果
 */
typedef enum {
    LV00_VERIFY_VALID,                /**< 有效 */
    LV00_VERIFY_INVALID,              /**< 无效 */
    LV00_VERIFY_INCOMPLETE,           /**< 不完整 */
    LV00_VERIFY_ERROR                 /**< 验证错误 */
} Lv00VerifyResult;

/**
 * @brief 验证证明
 * @param trace 溯源树
 * @param out_error 输出错误消息
 * @return 验证结果
 */
LV00_PUBLIC_API Lv00VerifyResult lv00_verify_proof(const Lv00ProofTraceTree *trace,
                                    char *out_error);

/**
 * @brief 验证证明步骤
 * @param step 证明步骤
 * @param graph 约束图
 * @param out_error 输出错误消息
 * @return 验证结果
 */
LV00_PUBLIC_API Lv00VerifyResult lv00_verify_proof_step(const ProofStep *step,
                                         const ConstraintGraph *graph,
                                         char *out_error);

/* ============== 证明优化 ============== */

/**
 * @brief 优化证明
 * @param trace 溯源树
 * @param out_optimized 输出优化后的溯源树
 * @return 是否成功优化
 */
LV00_PUBLIC_API int lv00_optimize_proof(const Lv00ProofTraceTree *trace,
                          Lv00ProofTraceTree **out_optimized);

/**
 * @brief 计算证明复杂度
 * @param trace 溯源树
 * @return 复杂度分数
 */
LV00_PUBLIC_API uint32_t lv00_compute_proof_complexity(const Lv00ProofTraceTree *trace);

/**
 * @brief 简化证明
 * @param trace 溯源树
 * @return 简化后的步骤数
 */
LV00_PUBLIC_API uint32_t lv00_simplify_proof(Lv00ProofTraceTree *trace);

/* ============== 证明导出 ============== */

/**
 * @brief 导出证明为自然语言
 * @param trace 溯源树
 * @param lang 语言
 * @return 自然语言文本
 */
LV00_PUBLIC_API char *lv00_proof_to_natural_language(const Lv00ProofTraceTree *trace,
                                      ProofNaturalLanguage lang);

/**
 * @brief 导出证明为 LaTeX
 * @param trace 溯源树
 * @return LaTeX 文本
 */
LV00_PUBLIC_API char *lv00_proof_to_latex(const Lv00ProofTraceTree *trace);

/**
 * @brief 导出证明为 Coq 脚本
 * @param trace 溯源树
 * @return Coq 脚本
 */
LV00_PUBLIC_API char *lv00_proof_to_coq(const Lv00ProofTraceTree *trace);

/**
 * @brief 导出证明为 Isar 脚本
 * @param trace 溯源树
 * @return Isar 脚本
 */
LV00_PUBLIC_API char *lv00_proof_to_isar(const Lv00ProofTraceTree *trace);

#ifdef __cplusplus
}
#endif

#endif /* LV00_PROOF_ENGINE_ENHANCED_H */
