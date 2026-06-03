/* ========================================================================
 * 模块名称：等价类管理器 (equiv_class)
 * 功能概述：将 Lv-00 的"坐标判等 → 并查集合并"推广为更一般的
 *          代数等价关系，支持约束推导等价、代数共轭等价、
 *          几何变换等价，并为每步合并生成可追溯的证明。
 *
 * 数学基础：
 *   等价关系 ~ 满足自反性、对称性、传递性
 *   商集 V/~ = {[v] : v ∈ V}
 *   关键定理：若 S 相容，则 S/~ 也相容
 *
 * 等价来源分类：
 *   - 坐标精确相等（COORD_EQUAL）
 *   - 约束链推导（CONSTRAINT_DERIVE）
 *   - 代数共轭（ALGEBRAIC_CONJ）—— 同一极小多项式的不同实根
 *   - 几何变换（GEOM_TRANSFORM）—— 合同变换下的等价
 *   - 语义模式匹配（SEMANTIC_PATTERN）
 *
 * 主要 API：
 *   - equiv_manager_create / destroy       — 创建/销毁等价类管理器
 *   - equiv_merge_by_coord                — 坐标等价合并
 *   - equiv_derive_from_constraints       — 约束推导等价
 *   - equiv_merge_algebraic_conjugates    — 代数共轭等价
 *   - equiv_merge_by_transform            — 几何变换等价
 *   - equiv_prove_merge_valid             — 合并合法性证明
 *   - equiv_get_class / equiv_find        — 查询等价类
 *
 * 使用示例：
 *   EquivClassManager *mgr = equiv_manager_create(graph);
 *   equiv_merge_by_coord(mgr);
 *   equiv_derive_from_constraints(mgr);
 *   equiv_manager_destroy(mgr);
 *
 * ======================================================================== */

/**
 * @file equiv_class.h
 * @brief 等价类管理器 —— 代数等价关系与合并合法性证明
 */

#ifndef LV00_EQUIV_CLASS_H
#define LV00_EQUIV_CLASS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "constraint_graph.h"
#include "stream.h"
#include "symbolic_coord.h"

/* ================================================================
 * 枚举类型
 * ================================================================ */

/**
 * @brief 等价来源类型
 */
typedef enum {
    EQUIV_SOURCE_COORD_EQUAL,       /**< 坐标精确相等 */
    EQUIV_SOURCE_CONSTRAINT_DERIVE, /**< 约束链推导 */
    EQUIV_SOURCE_ALGEBRAIC_CONJ,    /**< 代数共轭（同一极小多项式） */
    EQUIV_SOURCE_GEOM_TRANSFORM,    /**< 几何变换（合同变换） */
    EQUIV_SOURCE_SEMANTIC_PATTERN   /**< 语义模式匹配 */
} EquivSourceType;

/**
 * @brief 等价类合并结果
 */
typedef enum {
    EQUIV_MERGE_OK,             /**< 合并成功 */
    EQUIV_MERGE_ALREADY_EQUIV,  /**< 已经等价 */
    EQUIV_MERGE_INVALID,        /**< 非法合并（破坏相容性） */
    EQUIV_MERGE_SCOPE_REJECTED  /**< 跨作用域合并被拒绝 */
} EquivMergeResult;

/* ================================================================
 * 数据结构
 * ================================================================ */

/**
 * @brief 等价证明
 *
 * 记录一对节点为何被判定为等价的理由。
 */
typedef struct EquivProof {
    EquivSourceType source;         /**< 等价来源类型 */
    int node_a_id;                  /**< 节点 A 的 ID */
    int node_b_id;                  /**< 节点 B 的 ID */
    int deriving_constraint_id;     /**< 推导来源约束 ID（CONSTRAINT_DERIVE 时有效） */
    int proof_step_id;              /**< 证明步骤引用（-1 表示无） */
    TrustColor trust;               /**< 信任颜色 */
} EquivProof;

/**
 * @brief 等价类
 *
 * 表示一组互相等价的节点集合。
 */
typedef struct EquivClass {
    int representative_id;          /**< 代表节点 ID（集合中最小 ID） */
    int *member_ids;                /**< 成员节点 ID 数组 */
    int member_count;               /**< 成员数量 */
    int capacity;                   /**< 预分配容量 */

    EquivProof *proofs;             /**< 等价证明链 */
    int proof_count;                /**< 证明数量 */
    int proof_capacity;             /**< 证明预分配容量 */

    TrustColor min_trust;           /**< 类内最低信任颜色 */
} EquivClass;

/**
 * @brief 等价类管理器
 *
 * 管理约束图上所有节点的等价关系，提供合并、查询和合法性证明。
 */
typedef struct EquivClassManager {
    ConstraintGraph *graph;         /**< 关联约束图（只读引用，不拥有所有权） */

    EquivClass *classes;            /**< 等价类数组 */
    int class_count;                /**< 等价类数量 */
    int class_capacity;             /**< 预分配容量 */

    /* 节点 ID → 类索引的映射 */
    int *node_to_class;             /**< node_id → class_index（-1 表示无映射） */
    int node_to_class_capacity;     /**< 映射表容量 */

    /* 并查集（底层实现） */
    int *uf_parent;                 /**< 并查集父节点数组 */
    int *uf_rank;                   /**< 并查集秩数组 */
    int uf_capacity;                /**< 并查集容量 */

    /* 等价证明日志 */
    EquivProof *proof_log;          /**< 所有等价证明的日志 */
    int proof_log_count;            /**< 日志条目数 */
    int proof_log_capacity;         /**< 日志预分配容量 */

    /* 统计 */
    int64_t total_merges;           /**< 总合并次数 */
    int64_t coord_merges;           /**< 坐标等价合并次数 */
    int64_t constraint_derives;     /**< 约束推导次数 */
    int64_t algebraic_conjugates;   /**< 代数共轭合并次数 */
    int64_t transform_merges;       /**< 几何变换合并次数 */
    int64_t rejected_merges;        /**< 被拒绝的合并次数 */

    /* 流式事件 */
    StreamContext *stream_ctx;      /**< 流式输出上下文 */
} EquivClassManager;

/* ================================================================
 * 生命周期管理
 * ================================================================ */

/**
 * @brief 创建等价类管理器
 *
 * @param graph  约束图（必须非 NULL，管理期间保持有效）
 * @return 新创建的管理器，失败返回 NULL
 */
EquivClassManager *equiv_manager_create(ConstraintGraph *graph);

/**
 * @brief 销毁等价类管理器
 *
 * 释放所有等价类、并查集和证明日志。
 * 不销毁关联的约束图。
 *
 * @param mgr  等价类管理器
 */
void equiv_manager_destroy(EquivClassManager *mgr);

/* ================================================================
 * 等价合并操作
 * ================================================================ */

/**
 * @brief 坐标等价合并
 *
 * 封装现有的坐标判等逻辑，为每对坐标相等的节点生成
 * EQUIV_SOURCE_COORD_EQUAL 证明并合并。
 *
 * @param mgr  等价类管理器
 * @return 新合并的等价对数量
 */
int equiv_merge_by_coord(EquivClassManager *mgr);

/**
 * @brief 约束推导等价
 *
 * 从约束图推导隐式等价关系：
 *
 * 规则 1 - 中点等价：
 *   若 B 是 A,C 的中点，D 是 E,F 的中点，且 AC ≅ EF，则 B ~ D
 *
 * 规则 2 - 对称等价：
 *   若 l 是 A,B 的对称轴，C 关于 l 的对称点为 D，则 C ~ D
 *
 * 规则 3 - 交点等价：
 *   若 l1 ∩ l2 = {P}，l1' ∩ l2' = {P'}，且 l1 ~ l1'，l2 ~ l2'，则 P ~ P'
 *
 * @param mgr  等价类管理器
 * @return 新发现的等价对数量
 */
int equiv_derive_from_constraints(EquivClassManager *mgr);

/**
 * @brief 代数共轭等价合并
 *
 * 检测代数共轭等价：若两个节点的坐标是同一极小多项式的不同实根，
 * 则它们在代数意义上"等价"（可互换而不破坏约束）。
 *
 * 算法：
 * 1. 收集所有 ALGEBRAIC 类型坐标
 * 2. 按极小多项式哈希分组
 * 3. 组内比较极小多项式系数（精确 GMP 比较）
 * 4. 验证交换共轭对后约束仍然成立
 *
 * @param mgr  等价类管理器
 * @return 新发现的共轭等价对数量
 */
int equiv_merge_algebraic_conjugates(EquivClassManager *mgr);

/**
 * @brief 几何变换等价合并
 *
 * 检测合同变换下的等价：若存在旋转/平移/反射/组合变换 T
 * 使得 T(A) = B，且 T 保持所有邻域约束，则 A ~ B。
 *
 * @param mgr  等价类管理器
 * @return 新发现的变换等价对数量
 */
int equiv_merge_by_transform(EquivClassManager *mgr);

/**
 * @brief 运行全部等价合并
 *
 * 按顺序执行所有等价合并策略：
 * 1. equiv_merge_by_coord
 * 2. equiv_derive_from_constraints
 * 3. equiv_merge_algebraic_conjugates
 * 4. equiv_merge_by_transform
 *
 * @param mgr  等价类管理器
 * @return 总新合并等价对数量
 */
int equiv_merge_all(EquivClassManager *mgr);

/* ================================================================
 * 合法性证明
 * ================================================================ */

/**
 * @brief 证明合并操作的合法性
 *
 * 验证合并两个等价类后约束系统仍然相容。
 *
 * @param mgr          等价类管理器
 * @param class_a_idx  等价类 A 的索引
 * @param class_b_idx  等价类 B 的索引
 * @return true = 合法, false = 非法
 */
bool equiv_prove_merge_valid(EquivClassManager *mgr, int class_a_idx, int class_b_idx);

/* ================================================================
 * 查询接口
 * ================================================================ */

/**
 * @brief 查找节点所属的等价类
 *
 * @param mgr      等价类管理器
 * @param node_id  节点 ID
 * @return 等价类指针，未找到返回 NULL
 */
const EquivClass *equiv_get_class(const EquivClassManager *mgr, int node_id);

/**
 * @brief 查找节点的代表节点 ID
 *
 * @param mgr      等价类管理器
 * @param node_id  节点 ID
 * @return 代表节点 ID，未找到返回 -1
 */
int equiv_find(const EquivClassManager *mgr, int node_id);

/**
 * @brief 检查两个节点是否等价
 *
 * @param mgr      等价类管理器
 * @param node_a   节点 A 的 ID
 * @param node_b   节点 B 的 ID
 * @return true = 等价
 */
bool equiv_are_equivalent(const EquivClassManager *mgr, int node_a, int node_b);

/**
 * @brief 获取等价类数量
 * @param mgr  等价类管理器
 * @return 等价类数量
 */
int equiv_class_count(const EquivClassManager *mgr);

/* ================================================================
 * 配置与诊断
 * ================================================================ */

/**
 * @brief 设置流式输出上下文
 * @param mgr         等价类管理器
 * @param stream_ctx  流式上下文（可为 NULL）
 */
void equiv_set_stream_context(EquivClassManager *mgr, StreamContext *stream_ctx);

/**
 * @brief 获取统计信息
 *
 * @param mgr              等价类管理器
 * @param out_total        [out] 总合并次数
 * @param out_coord        [out] 坐标等价合并次数
 * @param out_derive       [out] 约束推导次数
 * @param out_conjugate    [out] 代数共轭合并次数
 * @param out_transform    [out] 几何变换合并次数
 * @param out_rejected     [out] 被拒绝的合并次数
 */
void equiv_get_statistics(const EquivClassManager *mgr,
                           int64_t *out_total,
                           int64_t *out_coord,
                           int64_t *out_derive,
                           int64_t *out_conjugate,
                           int64_t *out_transform,
                           int64_t *out_rejected);

/**
 * @brief 验证等价类幂等性
 *
 * 对当前等价关系再次运行合并，检查是否有新合并产生。
 *
 * @param mgr  等价类管理器
 * @return true = 幂等（无新合并）
 */
bool equiv_verify_idempotency(EquivClassManager *mgr);

#ifdef __cplusplus
}
#endif

#endif /* LV00_EQUIV_CLASS_H */
