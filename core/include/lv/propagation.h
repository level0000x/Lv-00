/* ========================================================================
 * 模块名称：约束传播引擎 (propagation)
 * 功能概述：WFC（Wave Function Collapse）风格的约束传播引擎，
 *          通过动态约束传播、熵最小化节点选择和死路恢复机制，
 *          将"局部规则排除错误路径"的范式注入 Lv-00 约束求解。
 *
 * 核心算法：
 *   - AC-3 弧相容性约束传播
 *   - WFC 熵最小化节点选择与坍缩
 *   - 快照/回溯死路恢复
 *   - 与现有 engine_solve() 流水线集成
 *
 * 数学基础：
 *   约束系统 S = (V, C, Sigma, delta)
 *   传播函数 delta: C x Sigma -> P(Sigma)
 *   全局约束 Sigma_global = ∩_{c in C} delta(c, Sigma)
 *   合法解 sigma* ∈ Sigma_global
 *
 * 主要 API：
 *   - propagation_context_create / destroy      — 创建/销毁传播上下文
 *   - propagation_init_state_spaces             — 初始化节点状态空间
 *   - propagation_run                           — AC-3 约束传播
 *   - propagation_select_node                   — WFC 熵最小化选择
 *   - propagation_collapse                      — 坍缩节点状态
 *   - propagation_wfc_solve                     — 完整 WFC 求解循环
 *   - propagation_snapshot_save / restore       — 快照/恢复
 *
 * 使用示例：
 *   PropagationContext *ctx = propagation_context_create(graph);
 *   propagation_init_state_spaces(ctx);
 *   PropagationResult result = propagation_wfc_solve(ctx);
 *   propagation_context_destroy(ctx);
 *
 * ======================================================================== */
/**
 * @file propagation.h
 * @brief 约束传播引擎 —— WFC 风格的动态约束传播
 */
#ifndef lv_PROPAGATION_H
#define lv_PROPAGATION_H
#ifdef __cplusplus
extern "C" {
#endif
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "lv/lv_config.h"

#include "constraint_graph.h"
#include "stream.h"
#include "symbolic_coord.h"
/* ================================================================
 * 常量定义
 * ================================================================ */
/** @brief 传播队列默认容量 */
#define PROP_DEFAULT_QUEUE_CAPACITY 256
/** @brief 快照栈默认容量 */
#define PROP_DEFAULT_SNAPSHOT_CAPACITY 64
/** @brief 节点状态空间默认初始容量 */
#define PROP_DEFAULT_STATE_CAPACITY 8
/** @brief 无穷大熵标记（表示自由变量/无界） */
#define PROP_ENTROPY_UNBOUNDED (-1.0)

// Config defaults — values read from runtime lvConfig:
int propagation_default_max_iterations(void);
int propagation_default_max_backtracks(void);
int propagation_wfc_max_collaboration_iterations(void);
/* ================================================================
 * 枚举类型
 * ================================================================ */
/**
 * @brief 传播策略枚举
 *
 * 决定在 WFC 循环中选择下一个坍缩节点的方式。
 */
typedef enum {
    PROP_STRATEGY_MIN_ENTROPY, /**< WFC: 选择状态空间最小的节点（熵最小化） */
    PROP_STRATEGY_MRVS,        /**< MRV: 最少剩余值启发式（约束满足术语） */
    PROP_STRATEGY_DEGREE,      /**< Degree: 选择度数（邻接约束数）最高的节点 */
    PROP_STRATEGY_BFS,         /**< BFS: 从最近修改处广度优先传播 */
    PROP_STRATEGY_TOPOLOGICAL  /**< 拓扑序: 按依赖关系顺序选择 */
} PropagationStrategy;
/**
 * @brief 传播结果枚举
 */
typedef enum {
    PROP_RESULT_CONSISTENT,    /**< 相容，可继续传播 */
    PROP_RESULT_CONTRADICTION, /**< 矛盾，某节点状态空间为空 */
    PROP_RESULT_SATISFIED,     /**< 所有节点已坍缩为唯一值 */
    PROP_RESULT_STABLE,        /**< 传播收敛，但仍有未确定节点 */
    PROP_RESULT_TIMEOUT        /**< 超时 */
} PropagationResult;
/**
 * @brief 坍缩策略枚举
 */
typedef enum {
    PROP_COLLAPSE_FIRST,   /**< 确定性：选择第一个候选 */
    PROP_COLLAPSE_WEIGHTED /**< 加权随机：按约束兼容性加权 */
} CollapseStrategy;
/* ================================================================
 * 数据结构
 * ================================================================ */
/**
 * @brief 节点状态空间
 *
 * 记录约束图中每个节点的可能取值集合。
 * 当 is_collapsed 为 true 时，状态空间收缩为唯一值。
 */
typedef struct NodeStateSpace {
    int node_id;                     /**< 关联的节点 ID */
    SymbolicCoord **possible_coords; /**< 可能坐标列表（每个元素是一个 dim 维坐标数组） */
    int *coord_dims;                 /**< 每个候选坐标的维度 */
    int coord_count;                 /**< 候选坐标数量 */
    int capacity;                    /**< 预分配容量 */
    bool is_collapsed;               /**< 是否已坍缩为唯一值 */
    SymbolicCoord *collapsed_value;  /**< 坍缩后的唯一坐标值 */
    bool is_unbounded;               /**< 是否为无界自由变量 */
} NodeStateSpace;
/**
 * @brief 传播快照
 *
 * 保存传播上下文的完整状态，用于回溯。
 */
typedef struct PropagationSnapshot {
    NodeStateSpace *states;    /**< 所有节点状态空间的深拷贝 */
    int state_count;           /**< 状态空间数量 */
    int64_t propagation_steps; /**< 当时的传播步数 */
    int64_t collapse_count;    /**< 当时的坍缩次数 */
    int64_t backtrack_count;   /**< 当时的回溯次数 */
    int64_t prune_count;       /**< 当时的剪枝次数 */
    int decision_node_id;      /**< 本次决策的节点 ID */
    int decision_coord_index;  /**< 本次决策选择的坐标索引 */
} PropagationSnapshot;
/**
 * @brief 传播上下文
 *
 * 约束传播引擎的核心上下文，管理节点状态空间、传播队列和回溯栈。
 */
typedef struct PropagationContext {
    ConstraintGraph *graph;             /**< 关联约束图（只读引用，不拥有所有权） */
    NodeStateSpace *state_spaces;       /**< 每个节点的状态空间数组 */
    int state_count;                    /**< 状态空间数量（= graph->node_count） */
    PropagationStrategy strategy;       /**< 节点选择策略 */
    CollapseStrategy collapse_strategy; /**< 坍缩策略 */
    int max_iterations;                 /**< 最大传播轮次 */
    int max_backtracks;                 /**< 最大回溯次数 */
    /* 传播队列（环形缓冲区） */
    int *propagation_queue; /**< 待传播节点 ID 队列 */
    int queue_head, queue_tail;
    int queue_capacity;
    int queue_size;
    /* 回溯栈 */
    PropagationSnapshot **snapshot_stack; /**< 快照栈 */
    int snapshot_count;
    int snapshot_capacity;
    /* 统计信息 */
    int64_t propagation_steps; /**< 传播步数 */
    int64_t collapse_count;    /**< 坍缩次数 */
    int64_t backtrack_count;   /**< 回溯次数 */
    int64_t prune_count;       /**< 剪枝（状态移除）次数 */
    /* 流式事件 */
    StreamContext *stream_ctx; /**< 流式输出上下文（可为 NULL） */
} PropagationContext;
/* ================================================================
 * 生命周期管理
 * ================================================================ */
/**
 * @brief 创建传播上下文
 *
 * @param graph  约束图（必须非 NULL，传播期间保持有效）
 * @return 新创建的传播上下文，失败返回 NULL
 */
PropagationContext *propagation_context_create(ConstraintGraph *graph);
/**
 * @brief 销毁传播上下文
 *
 * 释放所有状态空间、传播队列和快照栈。
 * 不销毁关联的约束图。
 *
 * @param ctx  传播上下文
 */
void propagation_context_destroy(PropagationContext *ctx);
/* ================================================================
 * 状态空间初始化
 * ================================================================ */
/**
 * @brief 初始化所有节点的状态空间
 *
 * 为约束图中每个活跃节点生成初始候选坐标集：
 * - 已有精确坐标的节点 → 状态空间 = {当前坐标}（已坍缩）
 * - 有邻接约束但无坐标的节点 → 从约束推导候选集
 * - 完全自由的节点 → 标记为 unbounded
 *
 * @param ctx  传播上下文
 * @return PROP_RESULT_CONSISTENT 或 PROP_RESULT_CONTRADICTION
 */
PropagationResult propagation_init_state_spaces(PropagationContext *ctx);
/**
 * @brief 获取指定节点的状态空间
 *
 * @param ctx      传播上下文
 * @param node_id  节点 ID
 * @return 状态空间指针，节点不存在返回 NULL
 */
NodeStateSpace *propagation_get_state_space(PropagationContext *ctx, int node_id);
/* ================================================================
 * AC-3 约束传播
 * ================================================================ */
/**
 * @brief 对单个约束执行弧相容性检查
 *
 * 从参与者 A 的状态空间中移除所有"在 B 的当前状态下不可能"的值。
 *
 * 数学定义：
 *   对于约束 c = (A, B, type)，
 *   移除所有 a ∈ Σ(A) 使得 ¬∃ b ∈ Σ(B) : δ(c, (a, b)) 成立
 *
 * @param ctx             传播上下文
 * @param constraint_id   约束 ID
 * @return true = 状态空间被收缩（有值被移除），false = 无变化
 */
bool propagation_arc_reduce(PropagationContext *ctx, int constraint_id);
/**
 * @brief 运行 AC-3 约束传播
 *
 * AC-3 主循环：
 * 1. 将所有约束加入工作队列
 * 2. 取出约束，执行弧相容性检查
 * 3. 若某参与者状态空间收缩，将其所有邻接约束重新入队
 * 4. 若某参与者状态空间变空 → CONTRADICTION
 * 5. 队列为空 → 返回当前状态
 *
 * @param ctx  传播上下文
 * @return PropagationResult
 */
PropagationResult propagation_run(PropagationContext *ctx);
/* ================================================================
 * WFC 节点选择与坍缩
 * ================================================================ */
/**
 * @brief WFC 熵最小化节点选择
 *
 * 1. 计算每个未坍缩节点的"熵" H(v) = log₂|Σ(v)|
 * 2. 选择 H(v) 最小的节点（状态空间最小的）
 * 3. 若有多个节点熵相同，选择度数最高的（打破对称性）
 *
 * @param ctx  传播上下文
 * @return 节点 ID，无候选时返回 -1
 */
int propagation_select_node(PropagationContext *ctx);
/**
 * @brief 计算节点的熵
 *
 * @param state  节点状态空间
 * @return 熵值（log2），unbounded 返回 PROP_ENTROPY_UNBOUNDED
 */
double propagation_compute_entropy(const NodeStateSpace *state);
/**
 * @brief 坍缩节点状态
 *
 * 将选定节点的状态空间收缩为单一值。
 * 策略由 collapse_strategy 决定：
 * - PROP_COLLAPSE_FIRST: 选择第一个候选（确定性）
 * - PROP_COLLAPSE_WEIGHTED: 按约束兼容性加权随机选择
 *
 * @param ctx      传播上下文
 * @param node_id  要坍缩的节点 ID
 * @return true = 成功坍缩, false = 状态空间为空或已坍缩
 */
bool propagation_collapse(PropagationContext *ctx, int node_id);
/* ================================================================
 * 完整 WFC 求解循环
 * ================================================================ */
/**
 * @brief 完整的 WFC 求解循环
 *
 *   loop:
 *     1. propagation_run()           -- AC-3 约束传播
 *     2. if CONTRADICTION → backtrack or restart
 *     3. if SATISFIED → return SUCCESS
 *     4. if STABLE → select_node() + collapse() → goto 1
 *
 * @param ctx  传播上下文
 * @return PropagationResult
 */
PropagationResult propagation_wfc_solve(PropagationContext *ctx);
/* ================================================================
 * 快照与回溯
 * ================================================================ */
/**
 * @brief 保存传播上下文快照
 *
 * 深拷贝所有节点状态空间和统计信息。
 * 调用方负责在适当时机调用 propagation_snapshot_restore 或 destroy。
 *
 * @param ctx  传播上下文
 * @return 新创建的快照，失败返回 NULL
 */
PropagationSnapshot *propagation_snapshot_save(PropagationContext *ctx);
/**
 * @brief 恢复传播上下文到指定快照
 *
 * 销毁当前状态，用快照数据替换。恢复后快照被销毁。
 *
 * @param ctx   传播上下文
 * @param snap  要恢复的快照（所有权转移，调用后不可再使用）
 */
void propagation_snapshot_restore(PropagationContext *ctx, PropagationSnapshot *snap);
/**
 * @brief 销毁快照
 *
 * @param snap  要销毁的快照
 */
void propagation_snapshot_destroy(PropagationSnapshot *snap);
/* ================================================================
 * 配置
 * ================================================================ */
/**
 * @brief 设置传播策略
 * @param ctx       传播上下文
 * @param strategy  传播策略
 */
void propagation_set_strategy(PropagationContext *ctx, PropagationStrategy strategy);
/**
 * @brief 设置坍缩策略
 * @param ctx       传播上下文
 * @param strategy  坍缩策略
 */
void propagation_set_collapse_strategy(PropagationContext *ctx, CollapseStrategy strategy);
/**
 * @brief 设置流式输出上下文
 * @param ctx  传播上下文
 * @param stream_ctx  流式上下文（可为 NULL 以禁用）
 */
void propagation_set_stream_context(PropagationContext *ctx, StreamContext *stream_ctx);
/**
 * @brief 设置最大迭代次数
 * @param ctx           传播上下文
 * @param max_iterations  最大迭代次数
 */
void propagation_set_max_iterations(PropagationContext *ctx, int max_iterations);
/**
 * @brief 设置最大回溯次数
 * @param ctx            传播上下文
 * @param max_backtracks  最大回溯次数
 */
void propagation_set_max_backtracks(PropagationContext *ctx, int max_backtracks);
/* ================================================================
 * 诊断与查询
 * ================================================================ */
/**
 * @brief 获取传播统计信息
 *
 * @param ctx              传播上下文
 * @param out_steps        [out] 传播步数
 * @param out_collapses    [out] 坍缩次数
 * @param out_backtracks   [out] 回溯次数
 * @param out_prunes       [out] 剪枝次数
 */
void propagation_get_statistics(const PropagationContext *ctx, int64_t *out_steps, int64_t *out_collapses,
                                int64_t *out_backtracks, int64_t *out_prunes);
/**
 * @brief 统计未坍缩节点数量
 * @param ctx  传播上下文
 * @return 未坍缩且非 unbounded 的节点数量
 */
int propagation_count_uncollapsed(const PropagationContext *ctx);
/**
 * @brief 检查传播结果是否为 SATISFIED
 * @param ctx  传播上下文
 * @return true = 所有节点已坍缩
 */
bool propagation_is_fully_collapsed(const PropagationContext *ctx);
#ifdef __cplusplus
}
#endif
#endif /* lv_PROPAGATION_H */
