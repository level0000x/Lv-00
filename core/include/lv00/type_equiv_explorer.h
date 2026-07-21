/* ========================================================================
 * 模块名称：交互式类型等价探索器引擎 (type_equiv_explorer)
 * 功能概述：当自动重写无法将两个类型区域归一到同一范式时，
 *          提供双向路径搜索、回溯、替代规则尝试的自动探索引擎。
 *          设计文档参考：§3.6 "用户交互式路径探索"。
 *
 * 与 PathExplorer 的关系：
 *   PathExplorer 负责单侧探索（从当前到目标），本模块在其上
 *   构建双向搜索——同时对左右两侧应用重写规则，寻找汇合点。
 *
 * 主要 API：
 *   - type_equiv_explore_create   — 创建双向探索会话
 *   - type_equiv_explore_search   — BFS 自动搜索合一路径
 *   - type_equiv_explore_get_path — 导出证明路径
 *   - type_equiv_explore_destroy  — 释放资源
 *
 * ======================================================================== */

#ifndef LV00_TYPE_EQUIV_EXPLORER_H
#define LV00_TYPE_EQUIV_EXPLORER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include "constraint_graph.h"
#include "type_system.h"

/* ========================================================================
 * 数据结构
 * ======================================================================== */

/**
 * @brief 探索节点——BFS 搜索树中的一个状态
 */
typedef struct TypeEquivNode {
    TypeRegion *left;             /**< 左侧类型（简化后的） */
    TypeRegion *right;            /**< 右侧类型（简化后的） */
    int depth;                    /**< 搜索深度 */
    int parent_index;             /**< 父节点索引（-1 为根） */
    int applied_rule_index;      /**< 应用的规则索引 */
    bool applied_to_left;        /**< 规则应用到左侧（true）还是右侧（false） */
    char applied_rule_name[64];  /**< 应用的规则名称 */
} TypeEquivNode;

/**
 * @brief 双向类型等价探索会话
 *
 * 同时对左右两侧类型区域应用重写规则，搜索使两侧合一的路径。
 */
typedef struct {
    TypeSystem *ts;              /**< 类型系统上下文 */
    TypeRegion *left_original;   /**< 左侧原始类型（探索起点） */
    TypeRegion *right_original;  /**< 右侧原始类型（探索起点） */

    /* BFS 搜索状态 */
    TypeEquivNode **queue;       /**< BFS 队列 */
    int queue_head;              /**< 队列头索引 */
    int queue_tail;              /**< 队列尾索引 */
    int queue_capacity;          /**< 队列容量 */

    bool found_equivalence;      /**< 是否已找到等价证明 */
    int solution_node_index;     /**< 解节点在队列中的索引（-1 表示未找到） */

    int max_depth;               /**< 最大搜索深度 */
    int max_nodes;               /**< 最大探索节点数 */
    int nodes_explored;          /**< 已探索节点数 */

    TypeRewritePath *proved_path; /**< 若 found_equivalence=true，存储证明路径 */

    bool exhausted;              /**< 是否已穷尽搜索空间 */
} TypeEquivExplorer;

/* ========================================================================
 * 核心 API
 * ======================================================================== */

/**
 * @brief 创建双向类型等价探索器
 *
 * @param ts     类型系统（提供重写规则）
 * @param left   左侧类型区域（将被深拷贝）
 * @param right  右侧类型区域（将被深拷贝）
 * @return 新分配的探索器，调用者负责 type_equiv_explore_destroy
 */
TypeEquivExplorer *type_equiv_explore_create(TypeSystem *ts,
                                              const TypeRegion *left,
                                              const TypeRegion *right);

/**
 * @brief BFS 自动搜索合一路径
 *
 * 对左右两侧类型区域同时应用重写规则，尝试找到使两侧
 * 经图规范化遍 + 合一检查后匹配的路径。
 *
 * 搜索策略：
 *   - BFS 按深度优先遍历
 *   - 每步尝试所有可用规则，分别应用到左侧或右侧
 *   - 每步后检查当前左右两侧是否通过 type_check_equivalence
 *   - 到达 max_steps 或 max_nodes 时停止
 *
 * @param explorer   探索器
 * @param max_steps  最大步数限制
 * @return true 找到合一路径，false 未能在限制内找到
 */
bool type_equiv_explore_search(TypeEquivExplorer *explorer, int max_steps);

/**
 * @brief 获取已找到的证明路径
 *
 * @param explorer  探索器
 * @return 证明路径（只读），未找到等价时返回 NULL
 */
const TypeRewritePath *type_equiv_explore_get_path(const TypeEquivExplorer *explorer);

/**
 * @brief 销毁探索器并释放所有资源
 *
 * @param explorer  探索器
 */
void type_equiv_explore_destroy(TypeEquivExplorer *explorer);

/**
 * @brief 获取探索统计
 *
 * @param explorer          探索器
 * @param out_nodes         输出：已探索节点数
 * @param out_max_depth     输出：达到的最大深度
 * @param out_found         输出：是否找到等价
 * @param out_exhausted     输出：是否已穷尽搜索空间
 */
void type_equiv_explore_get_stats(const TypeEquivExplorer *explorer,
                                  int *out_nodes,
                                  int *out_max_depth,
                                  bool *out_found,
                                  bool *out_exhausted);

#ifdef __cplusplus
}
#endif

#endif /* LV00_TYPE_EQUIV_EXPLORER_H */
