/* ========================================================================
 * 模块名称：剪枝合法性元证明 (meta_proof)
 * 功能概述：在证明系统之上增加元证明层，证明"被排除的状态空间
 *          确实不包含合法解"。这是 WFC 范式数学严格化的基础。
 *
 * 数学基础：
 *   剪枝操作 π = (v, R, φ)，其中 R ⊂ Σ(v) 为被移除的状态子集
 *   合法性条件：∀ r ∈ R, ∀ σ* ∈ Σ_global : σ*(v) ≠ r
 *   完备性定理：若每步剪枝合法且 Σ_global ≠ ∅，
 *              则 Σ_global 中的所有解都是原问题的合法解
 *
 * 证明策略（三层）：
 *   L1: 直接矛盾 —— r 与某约束直接矛盾
 *   L2: 传播矛盾 —— 选择 r 后约束传播导致矛盾
 *   L3: 代数排除 —— r 不满足多项式方程组的解集
 *
 * 主要 API：
 *   - meta_proof_context_create / destroy    — 创建/销毁元证明上下文
 *   - meta_prove_direct_contradiction       — L1 直接矛盾证明
 *   - meta_prove_propagation_contradiction  — L2 传播矛盾证明
 *   - meta_prove_algebraic_exclusion        — L3 代数排除证明
 *   - meta_prove_pruning                    — 自动选择策略证明剪枝合法性
 *   - meta_prove_completeness               — 完备性验证
 *
 * 使用示例：
 *   MetaProofContext *ctx = meta_proof_context_create(nav, graph);
 *   bool valid = meta_prove_pruning(ctx, node_id, candidate);
 *   ProofColor color = meta_prove_completeness(ctx);
 *   meta_proof_context_destroy(ctx);
 *
 * ======================================================================== */
/**
 * @file meta_proof.h
 * @brief 剪枝合法性元证明 —— WFC 范式的数学严格化
 */
#ifndef lv_META_PROOF_H
#define lv_META_PROOF_H
#ifdef __cplusplus
extern "C" {
#endif
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "constraint_graph.h"
#include "propagation.h"
#include "stream.h"
#include "symbolic_coord.h"
/* 前向声明 */
typedef struct ProofNavigator ProofNavigator;
typedef struct EquivClassManager EquivClassManager;
/* ================================================================
 * 枚举类型
 * ================================================================ */
/**
 * @brief 剪枝策略枚举
 */
typedef enum {
    PRUNE_DIRECT_CONTRADICTION,        /**< L1: 直接矛盾（候选与约束直接冲突） */
    PRUNE_PROPAGATION_CONTRADICTION,   /**< L2: 传播矛盾（选择后传播导致死路） */
    PRUNE_ALGEBRAIC_EXCLUSION          /**< L3: 代数排除（不在 Groebner 基解集中） */
} PruneStrategy;
/**
 * @brief 元证明结果枚举
 */
typedef enum {
    META_PROVE_VALID,          /**< 剪枝合法（已证明） */
    META_PROVE_INVALID,        /**< 剪枝非法（候选可能是合法解） */
    META_PROVE_INCONCLUSIVE,   /**< 无法确定（超出证明能力） */
    META_PROVE_TIMEOUT         /**< 证明超时 */
} MetaProofResult;
/* ================================================================
 * 数据结构
 * ================================================================ */
/**
 * @brief 剪枝操作
 *
 * 记录一次状态空间剪枝的详细信息。
 */
typedef struct PruningOperation {
    int node_id;                       /**< 被剪枝的节点 ID */
    SymbolicCoord **removed_states;    /**< 被移除的状态列表 */
    int removed_count;                 /**< 被移除的状态数量 */
    PruneStrategy strategy;            /**< 使用的证明策略 */
    TrustColor trust;                  /**< 剪枝信任颜色 */
    /* L1: 直接矛盾 */
    int conflicting_constraint_id;     /**< 矛盾约束 ID（L1 时有效，-1 表示无） */
    /* L2: 传播矛盾 */
    int propagation_steps;             /**< 传播到矛盾所需的步数（L2 时有效） */
    int *propagation_trace;            /**< 传播路径节点 ID 序列（L2 时有效） */
    int propagation_trace_count;       /**< 传播路径长度 */
    /* L3: 代数排除 */
    int poly_violation_count;          /**< 违反的多项式数量（L3 时有效） */
} PruningOperation;
/**
 * @brief 剪枝记录
 *
 * 记录整个求解过程中的所有剪枝操作。
 */
typedef struct PruningRecord {
    PruningOperation *operations;      /**< 剪枝操作数组 */
    int operation_count;               /**< 操作数量 */
    int capacity;                      /**< 预分配容量 */
    int64_t total_states_removed;      /**< 总移除状态数 */
    int64_t total_states_remaining;    /**< 总剩余状态数 */
} PruningRecord;
/**
 * @brief 完备性报告
 */
typedef struct CompletenessReport {
    int total_prunings;                /**< 总剪枝次数 */
    int proven_prunings;               /**< 已证明合法的剪枝次数 */
    int unproven_prunings;             /**< 未证明的剪枝次数 */
    int invalid_prunings;              /**< 非法剪枝次数 */
    TrustColor overall_color;          /**< 总体信任颜色 */
    char summary[256];                 /**< 人类可读摘要 */
} CompletenessReport;
/**
 * @brief 元证明上下文
 *
 * 管理剪枝合法性证明的上下文，关联传播引擎和等价类管理器。
 */
typedef struct MetaProofContext {
    ConstraintGraph *graph;            /**< 关联约束图（只读引用） */
    PropagationContext *prop_ctx;      /**< 关联传播上下文（可为 NULL） */
    EquivClassManager *equiv_mgr;      /**< 关联等价类管理器（可为 NULL） */
    ProofNavigator *navigator;         /**< 关联证明导航器（可为 NULL） */
    PruningRecord *record;             /**< 剪枝记录 */
    /* 配置 */
    int max_propagation_steps;         /**< L2 传播矛盾最大步数 */
    int timeout_ms;                    /**< 单次证明超时（毫秒） */
    bool enable_l1;                    /**< 启用 L1 直接矛盾 */
    bool enable_l2;                    /**< 启用 L2 传播矛盾 */
    bool enable_l3;                    /**< 启用 L3 代数排除 */
    /* 统计 */
    int64_t l1_proofs;                 /**< L1 证明次数 */
    int64_t l2_proofs;                 /**< L2 证明次数 */
    int64_t l3_proofs;                 /**< L3 证明次数 */
    int64_t inconclusive_count;        /**< 无法确定次数 */
    /* 流式事件 */
    StreamContext *stream_ctx;         /**< 流式输出上下文 */
} MetaProofContext;
/* ================================================================
 * 生命周期管理
 * ================================================================ */
/**
 * @brief 创建元证明上下文
 *
 * @param graph     约束图（必须非 NULL）
 * @param prop_ctx  传播上下文（可为 NULL，L2 策略需要）
 * @return 新创建的元证明上下文，失败返回 NULL
 */
MetaProofContext *meta_proof_context_create(ConstraintGraph *graph,
                                             PropagationContext *prop_ctx);
/**
 * @brief 销毁元证明上下文
 *
 * @param ctx  元证明上下文
 */
void meta_proof_context_destroy(MetaProofContext *ctx);
/* ================================================================
 * 剪枝合法性证明
 * ================================================================ */
/**
 * @brief L1: 直接矛盾证明
 *
 * 证明候选状态 r 对节点 v 是非法的，因为 r 与某约束直接矛盾。
 * 将 r 代入约束的代数表达式，若结果非零则矛盾成立。
 *
 * @param ctx                          元证明上下文
 * @param node_id                      节点 ID
 * @param candidate                    候选坐标
 * @param out_conflicting_constraint   [out] 矛盾约束 ID（可为 NULL）
 * @return META_PROVE_VALID / INVALID / INCONCLUSIVE / TIMEOUT
 */
MetaProofResult meta_prove_direct_contradiction(MetaProofContext *ctx,
                                                  int node_id,
                                                  const SymbolicCoord *candidate,
                                                  int *out_conflicting_constraint);
/**
 * @brief L2: 传播矛盾证明
 *
 * 证明候选状态 r 对节点 v 是非法的，因为选择 r 后约束传播导致矛盾。
 * 临时坍缩 v 为 r，运行传播，若 CONTRADICTION 则证明成立。
 *
 * @param ctx        元证明上下文
 * @param node_id    节点 ID
 * @param candidate  候选坐标
 * @return META_PROVE_VALID / INVALID / INCONCLUSIVE / TIMEOUT
 */
MetaProofResult meta_prove_propagation_contradiction(MetaProofContext *ctx,
                                                      int node_id,
                                                      const SymbolicCoord *candidate);
/**
 * @brief L3: 代数排除证明
 *
 * 证明候选状态 r 对节点 v 是非法的，因为 r 不满足多项式方程组的解集。
 * 从约束图提取 Groebner 基，将 r 代入验证。
 *
 * @param ctx        元证明上下文
 * @param node_id    节点 ID
 * @param candidate  候选坐标
 * @return META_PROVE_VALID / INVALID / INCONCLUSIVE / TIMEOUT
 */
MetaProofResult meta_prove_algebraic_exclusion(MetaProofContext *ctx,
                                                int node_id,
                                                const SymbolicCoord *candidate);
/**
 * @brief 自动选择策略证明剪枝合法性
 *
 * 按优先级尝试 L1 → L2 → L3，返回第一个成功的证明。
 *
 * @param ctx        元证明上下文
 * @param node_id    节点 ID
 * @param candidate  候选坐标
 * @return META_PROVE_VALID / INVALID / INCONCLUSIVE / TIMEOUT
 */
MetaProofResult meta_prove_pruning(MetaProofContext *ctx,
                                    int node_id,
                                    const SymbolicCoord *candidate);
/* ================================================================
 * 完备性验证
 * ================================================================ */
/**
 * @brief 验证整个剪枝序列的完备性
 *
 * 检查所有被移除的状态是否都有合法的剪枝证明。
 *
 * @param ctx  元证明上下文
 * @return CompletenessReport（调用方负责释放）
 */
CompletenessReport *meta_prove_completeness(MetaProofContext *ctx);
/**
 * @brief 销毁完备性报告
 * @param report  完备性报告
 */
void meta_proof_completeness_report_destroy(CompletenessReport *report);
/* ================================================================
 * 剪枝记录管理
 * ================================================================ */
/**
 * @brief 记录一次剪枝操作
 *
 * @param ctx         元证明上下文
 * @param node_id     节点 ID
 * @param removed     被移除的状态数组
 * @param count       被移除的状态数量
 * @param strategy    证明策略
 * @param trust       信任颜色
 */
void meta_proof_record_pruning(MetaProofContext *ctx,
                                int node_id,
                                SymbolicCoord **removed,
                                int count,
                                PruneStrategy strategy,
                                TrustColor trust);
/**
 * @brief 获取剪枝记录（只读）
 * @param ctx  元证明上下文
 * @return 剪枝记录指针
 */
const PruningRecord *meta_proof_get_record(const MetaProofContext *ctx);
/* ================================================================
 * 配置
 * ================================================================ */
/**
 * @brief 设置关联的证明导航器
 * @param ctx        元证明上下文
 * @param navigator  证明导航器（可为 NULL）
 */
void meta_proof_set_navigator(MetaProofContext *ctx, ProofNavigator *navigator);
/**
 * @brief 设置关联的等价类管理器
 * @param ctx      元证明上下文
 * @param mgr      等价类管理器（可为 NULL）
 */
void meta_proof_set_equiv_manager(MetaProofContext *ctx, EquivClassManager *mgr);
/**
 * @brief 设置流式输出上下文
 * @param ctx         元证明上下文
 * @param stream_ctx  流式上下文（可为 NULL）
 */
void meta_proof_set_stream_context(MetaProofContext *ctx, StreamContext *stream_ctx);
/**
 * @brief 启用/禁用证明策略
 * @param ctx      元证明上下文
 * @param strategy 策略类型
 * @param enable   true 启用, false 禁用
 */
void meta_proof_set_strategy_enabled(MetaProofContext *ctx,
                                      PruneStrategy strategy,
                                      bool enable);
/**
 * @brief 设置 L2 传播矛盾最大步数
 * @param ctx       元证明上下文
 * @param max_steps 最大步数
 */
void meta_proof_set_max_propagation_steps(MetaProofContext *ctx, int max_steps);
/**
 * @brief 设置单次证明超时
 * @param ctx        元证明上下文
 * @param timeout_ms 超时毫秒数
 */
void meta_proof_set_timeout(MetaProofContext *ctx, int timeout_ms);
/* ================================================================
 * 诊断与查询
 * ================================================================ */
/**
 * @brief 获取统计信息
 *
 * @param ctx              元证明上下文
 * @param out_l1           [out] L1 证明次数
 * @param out_l2           [out] L2 证明次数
 * @param out_l3           [out] L3 证明次数
 * @param out_inconclusive [out] 无法确定次数
 */
void meta_proof_get_statistics(const MetaProofContext *ctx,
                                int64_t *out_l1,
                                int64_t *out_l2,
                                int64_t *out_l3,
                                int64_t *out_inconclusive);
#ifdef __cplusplus
}
#endif
#endif /* lv_META_PROOF_H */