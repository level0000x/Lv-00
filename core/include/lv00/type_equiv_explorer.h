/* ============================================================================
 * 模块名称：交互式类型等价探索器引擎 (type_equiv_explorer)
 *
 * 功能概述：
 *   当自动重写引擎无法将两个类型区域归一到同一范式时（即
 *   type_check_equivalence 返回 TYPE_EQUIV_NEEDS_INTERACTION），
 *   本模块提供双向 BFS 自动搜索——同时对左右两侧类型区域应用
 *   重写规则，寻找使两侧等价的变换路径。
 *
 *   与 PathExplorer 的关系：
 *     PathExplorer 负责单侧探索（从当前类型到目标类型），
 *     本模块在其上构建双向搜索：同时对左右两侧应用规则，
 *     直到两侧经 type_check_equivalence 判定为等价。
 *
 * 设计文档参考：§3.6 用户交互式路径探索 · 类型等价双向搜索
 *
 * 主要 API：
 *   - type_equiv_explore_create   创建双向探索会话
 *   - type_equiv_explore_search   BFS 自动搜索合一路径
 *   - type_equiv_explore_get_path 获取已找到的证明路径
 *   - type_equiv_explore_destroy  释放全部资源
 *   - type_equiv_explore_get_stats 获取搜索统计信息
 *
 * 使用示例：
 *   @code
 *   TypeEquivExplorer *exp = type_equiv_explore_create(ts, left, right);
 *   if (type_equiv_explore_search(exp, 50)) {
 *       const TypeRewritePath *path = type_equiv_explore_get_path(exp);
 *       // path 包含从原始类型到等价的完整变换步骤
 *   }
 *   type_equiv_explore_destroy(exp);
 *   @endcode
 *
 * ============================================================================ */

#ifndef LV00_TYPE_EQUIV_EXPLORER_H
#define LV00_TYPE_EQUIV_EXPLORER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include "constraint_graph.h"
#include "type_system.h"

/* ============================================================================
 * 数据结构
 * ============================================================================ */

/**
 * @brief BFS 搜索树节点——搜索空间中的一个状态
 *
 * 每个节点携带左右两侧类型区域的独立副本，以及指向父节点的索引。
 * 搜索成功时沿 parent_index 回溯可重建完整证明路径。
 */
typedef struct TypeEquivNode {
    /** 左侧类型区域（当前状态下的深拷贝） */
    TypeRegion *left;
    /** 右侧类型区域（当前状态下的深拷贝） */
    TypeRegion *right;
    /** 从根节点到当前节点的搜索深度 */
    int depth;
    /** 父节点在 BFS 队列中的索引（-1 表示根节点） */
    int parent_index;
    /** 从父节点到当前节点所应用规则的索引 */
    int applied_rule_index;
    /** 规则应用到左侧（true）还是右侧（false） */
    bool applied_to_left;
    /** 应用规则的名称（最大 63 字符，以 null 结尾） */
    char applied_rule_name[64];
} TypeEquivNode;

/**
 * @brief 双向类型等价探索会话
 *
 * 同时对左右两侧类型区域应用重写规则，BFS 搜索使两侧合一的路径。
 * 内部维护一个队列用于 BFS 遍历，支持步数上限和节点数上限。
 */
typedef struct {
    /** 类型系统上下文（提供重写规则集） */
    TypeSystem *ts;
    /** 左侧原始类型（深拷贝，搜索起点，销毁时释放） */
    TypeRegion *left_original;
    /** 右侧原始类型（深拷贝，搜索起点，销毁时释放） */
    TypeRegion *right_original;

    /** BFS 搜索队列（TypeEquivNode* 数组） */
    TypeEquivNode **queue;
    /** 队列头索引（下一个待出队的节点） */
    int queue_head;
    /** 队列尾索引（下一个入队位置） */
    int queue_tail;
    /** 队列当前容量（自动翻倍扩容） */
    int queue_capacity;

    /** 是否已找到等价证明 */
    bool found_equivalence;
    /** 解节点在队列中的索引（-1 表示未找到） */
    int solution_node_index;

    /** 最大搜索深度限制 */
    int max_depth;
    /** 最大探索节点数限制 */
    int max_nodes;
    /** 当前已探索的节点总数 */
    int nodes_explored;

    /** 若 found_equivalence == true，存储从根到解的完整证明路径 */
    TypeRewritePath *proved_path;

    /** 是否已穷尽搜索空间（队列为空） */
    bool exhausted;
} TypeEquivExplorer;

/* ============================================================================
 * 核心 API
 * ============================================================================ */

/**
 * @brief 创建双向类型等价探索器
 *
 * 深拷贝左右两侧的类型区域作为搜索起点，创建 BFS 队列并插入根节点。
 * 根节点的深度为 0、parent_index 为 -1、无规则信息。
 *
 * @param ts     类型系统（提供重写规则集）
 * @param left   左侧类型区域（将被深拷贝，调用者保持所有权）
 * @param right  右侧类型区域（将被深拷贝，调用者保持所有权）
 * @return 新分配的探索器，调用者负责 type_equiv_explore_destroy；
 *         任一参数为 NULL 返回 NULL
 */
TypeEquivExplorer *type_equiv_explore_create(TypeSystem *ts,
                                              const TypeRegion *left,
                                              const TypeRegion *right);

/**
 * @brief BFS 自动搜索合一路径
 *
 * 搜索策略：
 *   - BFS 按深度优先遍历搜索树
 *   - 每步对出队节点的左右两侧分别枚举所有匹配类型种类的规则
 *   - 每生成一个后继，深拷贝当前节点类型并对其对应侧应用规则
 *   - 每个后继入队时检查左右是否等价（type_check_equivalence）
 *   - 等价时回溯构造 TypeRewritePath 并返回成功
 *
 * 终止条件：
 *   - 找到等价 → 返回 true
 *   - 步数耗尽（steps >= max_steps）→ 返回 false
 *   - 节点数超限（queue 为空或超 max_nodes）→ exhausted = true，返回 false
 *
 * @param explorer   探索器
 * @param max_steps  最大步数限制（设得越小越快但越容易漏解）
 * @return true 找到合一路径，false 未能在限制内找到
 */
int type_equiv_explore_search(TypeEquivExplorer *explorer, int max_steps);

/**
 * @brief 获取已找到的证明路径（只读）
 *
 * @param explorer  探索器
 * @return 证明路径（只读，由探索器持有），未找到等价时返回 NULL
 */
const TypeRewritePath *type_equiv_explore_get_path(const TypeEquivExplorer *explorer);

/**
 * @brief 销毁探索器并释放所有资源
 *
 * 释放队列中所有剩余节点、原始类型副本、证明路径及探索器本身。
 * NULL 安全：传入 NULL 无操作。
 *
 * @param explorer  探索器
 */
void type_equiv_explore_destroy(TypeEquivExplorer *explorer);

/**
 * @brief 获取搜索统计信息
 *
 * 所有输出参数均可为 NULL（跳过不关心的统计）。
 *
 * @param explorer          探索器（可为 NULL）
 * @param out_nodes         输出：已探索节点总数
 * @param out_max_depth     输出：队列中达到的最大深度
 * @param out_found         输出：是否成功找到等价证明
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
