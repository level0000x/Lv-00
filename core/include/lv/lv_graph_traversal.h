/**
 * @file lv_graph_traversal.h
 * @brief 图/树遍历抽象层 —— 统一 DFS/BFS/拓扑排序/树遍历 API
 *
 * 为 Lv-00 系统中的约束图、证明树、表达式树等结构提供统一的遍历抽象，
 * 消除各模块重复实现的遍历逻辑。
 *
 * @author Lv-00 Project
 * @version 1.0.0
 */

#ifndef lv_GRAPH_TRAVERSAL_H
#define lv_GRAPH_TRAVERSAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 前向声明 */
typedef struct ConstraintGraph ConstraintGraph;
typedef struct GeomNode GeomNode;

// 遍历顺序
typedef enum {
    lv_TRAVERSAL_DFS_PRE = 0,     // 深度优先-前序
    lv_TRAVERSAL_DFS_POST,        // 深度优先-后序
    lv_TRAVERSAL_BFS,             // 广度优先
    lv_TRAVERSAL_TOPOLOGICAL,     // 拓扑序
    lv_TRAVERSAL_REVERSE_TOPOLOGICAL, // 逆拓扑序
} lvTraversalOrder;

// 遍历操作结果
typedef enum {
    lv_TRAVERSAL_CONTINUE = 0,    // 继续遍历
    lv_TRAVERSAL_SKIP_CHILDREN,   // 跳过当前节点的子节点
    lv_TRAVERSAL_STOP,            // 停止遍历
} lvTraversalResult;

// 图节点访问器（每个节点被访问时调用）
typedef lvTraversalResult (*lvGraphNodeVisitor)(GeomNode *node, int depth,
                                                  void *user_data);

// 图遍历配置
typedef struct lvGraphTraversalConfig {
    lvTraversalOrder order;           // 遍历顺序
    int max_depth;                    // 最大深度（0 = 不限制）
    bool visit_all;                   // 是否访问所有节点（即使图不连通）
    bool reverse_edges;               // 是否按反向边遍历
    bool skip_disabled;               // 是否跳过禁用的节点
} lvGraphTraversalConfig;

#define lv_GRAPH_TRAVERSAL_DEFAULT_CONFIG { lv_TRAVERSAL_DFS_PRE, 0, true, false, true }

// 树节点访问器（用于树结构遍历）
typedef lvTraversalResult (*lvTreeNodeVisitor)(void *node, int depth,
                                                 void *user_data);

// 树遍历配置
typedef struct lvTreeTraversalConfig {
    lvTraversalOrder order;           // 遍历顺序
    int max_depth;                    // 最大深度（0 = 不限制）
} lvTreeTraversalConfig;

#define lv_TREE_TRAVERSAL_DEFAULT_CONFIG { lv_TRAVERSAL_DFS_PRE, 0 }

// ---- 图遍历 API ----

// 遍历约束图
int lv_graph_traverse(ConstraintGraph *graph,
                       lvGraphNodeVisitor visitor,
                       void *user_data,
                       const lvGraphTraversalConfig *config);

// 从指定节点开始遍历
int lv_graph_traverse_from(ConstraintGraph *graph, int start_node_id,
                            lvGraphNodeVisitor visitor,
                            void *user_data,
                            const lvGraphTraversalConfig *config);

// 遍历节点的邻居
int lv_graph_traverse_neighbors(ConstraintGraph *graph, int node_id,
                                 lvGraphNodeVisitor visitor,
                                 void *user_data,
                                 const lvGraphTraversalConfig *config);

// ---- 树遍历 API ----

// 遍历树结构（通过回调获取子节点）
typedef int (*lvGetChildrenFunc)(void *node, void ***out_children);
int lv_tree_traverse(void *root,
                      lvTreeNodeVisitor visitor,
                      void *user_data,
                      lvGetChildrenFunc get_children,
                      const lvTreeTraversalConfig *config);

// ---- 便利函数 ----

// 计算图节点数
int lv_graph_count_nodes(ConstraintGraph *graph);

// 检查图是否包含环
bool lv_graph_has_cycle(ConstraintGraph *graph);

// 获取拓扑排序
int lv_graph_topological_sort(ConstraintGraph *graph, int **out_nodes, int *out_count);

// 遍历状态转字符串
const char *lv_traversal_order_to_string(lvTraversalOrder order);
const char *lv_traversal_result_to_string(lvTraversalResult result);

// ---- 通用图算法核心（任意整数 id 图：回调驱动，供各模块复用） ----
//
// lv_bfs_run / lv_cycle_detect / lv_topo_run 面向"节点为 0..node_count-1 的
// 整数 id、出边由回调枚举"的任意图结构，消除各模块手写 BFS / 三色环检测 /
// Kahn 拓扑排序的重复实现（block_scheduler、solver_order、meta_verify、
// probabilistic_constraint、conflict_detector 等）。
// ConstraintGraph 专用遍历（lv_graph_traverse 等）保持原 API 不变。

/**
 * @brief 邻居枚举回调（BFS / 环检测核心共用，按"批次"枚举）
 *
 * 输出 node_id 的第 batch_index 个批次邻居（环检测中每个批次对应约束超图的
 * 一条超边/一个约束的全部参与者；BFS 使用方忽略 batch_index，把全部邻居作为
 * 批次 0 输出）。
 *
 * 将本批次邻居 id 写入 out_neighbors（容量 max_neighbors）。out_edge_infos
 * 非 NULL 时，可顺带写入每条边对应的边信息（如约束指针，用于环检测的报告；
 * 不需要时传 NULL）。
 *
 * 返回写入数（> 0）：本批次邻居数。返回 0：无更多批次（批次结束）。
 * 返回 -1：batch_index 槽位无效（环检测中用于跳过非活跃超边，调用方应推进
 * batch_index 后继续）。
 *
 * 本批次邻居数超过 max_neighbors 时写入前 max_neighbors 个并返回
 * max_neighbors —— 驱动层会把该返回值视为"可能截断"，扩容后以同一
 * batch_index 重试，直至返回 < max_neighbors。因此回调须可重复调用且结果
 * 稳定（无副作用）。out_neighbors 为 NULL 或 max_neighbors <= 0 时返回 0。
 *
 * 注意：回调应压缩空批次（含 0 个有效邻居的批次直接跳过），使返回 0 仅表示
 * "无更多批次"。
 */
typedef int (*lvGraphNeighborFunc)(void *ctx, int node_id, int batch_index,
                                   int *out_neighbors, void **out_edge_infos,
                                   int max_neighbors);

/**
 * @brief BFS 出队访问回调（在 visited 判定之前调用）
 *
 * 返回 lv_TRAVERSAL_STOP 终止整个 BFS；lv_TRAVERSAL_SKIP_CHILDREN 跳过该节点
 * 的出边扩展；其余继续。
 */
typedef lvTraversalResult (*lvBfsVisitFunc)(void *ctx, int node_id);

/** @brief 通用 BFS 驱动配置 */
typedef struct lvBfsSpec {
    int node_count;       /**< 节点 id 空间 0..node_count-1 */
    const int *seeds;     /**< 起点 id 列表（入队时不查 visited；mark_on_enqueue 时标记） */
    int seed_count;
    bool *visited;        /**< 可为 NULL（内部申请并清零）；否则长度须 >= node_count */
    bool mark_on_enqueue; /**< true=入队时查 visited 并标记（标准 BFS）；false=出队时查 visited 并标记 */
    int max_queue;        /**< 队列 tail 上限（0 = 不限；满则丢弃新元素，与原定长队列截断语义一致） */
    lvGraphNeighborFunc neighbors; /**< 出边枚举 */
    lvBfsVisitFunc visit;          /**< 出队回调，可为 NULL */
    void *ctx;
} lvBfsSpec;

/** @brief 通用 BFS：返回出队节点数；内存不足返回 -1（visited 状态不可靠，调用方应中止） */
int lv_bfs_run(const lvBfsSpec *spec);

/** @brief 通用 Kahn 拓扑排序配置（节点空间 0..node_count-1，后继由回调枚举） */
typedef struct lvTopoSpec {
    int node_count;              /**< 节点 id 空间 0..node_count-1 */
    const int *nodes;            /**< 待排序节点 id 数组（重复 id 去重，按首次出现顺序）；NULL 时使用 0..node_count-1 全部 */
    int nodes_count;             /**< nodes 数组长度（nodes 为 NULL 时忽略） */
    int *out_order;              /**< 输出：拓扑序（长度须 >= 去重后的节点数；可为 NULL 仅统计数量） */
    lvGraphNeighborFunc successors; /**< 后继（出边）枚举回调（语义同 lv_bfs_run.neighbors，全部后继按批次输出） */
    void *ctx;
} lvTopoSpec;

/**
 * @brief 通用 Kahn 拓扑排序：对 0..node_count-1 的整数 id 数组做拓扑排序
 *
 * 与 lv_bfs_run / lv_cycle_detect 同级的通用设施，消除各模块手写 Kahn 排序。
 * 入度在驱动内部由 successors 回调逐节点枚举后继统计（含重复边重复计数，
 * 与各调用方手写实现一致）；初始入队顺序 = nodes 数组序（NULL 时按 id 升序）。
 *
 * @return 已排序节点数；小于去重后的待排序节点数表示存在环（仅输出无环部分）；
 *         内存不足返回 -1
 */
int lv_topo_run(const lvTopoSpec *spec);

/**
 * @brief 三色环检测：发现 from_id → to_id 反向边（to_id 为 GRAY）时回调
 *
 * 返回 lv_TRAVERSAL_STOP 立即终止整个检测（detected = true）；返回
 * lv_TRAVERSAL_CONTINUE 继续遍历（可借此逐环报告并限量）。
 */
typedef lvTraversalResult (*lvCycleFoundFunc)(void *ctx, int from_id, int to_id,
                                              void *edge_info);

/** @brief 通用三色环检测配置 */
typedef struct lvCycleDetectSpec {
    int node_count;              /**< 节点 id 空间 0..node_count-1 */
    const int *seeds;            /**< DFS 森林根 id 列表；NULL 时使用 0..node_count-1 */
    int seed_count;
    lvGraphNeighborFunc neighbors; /**< 出边枚举（无向/有向语义由实现决定） */
    lvCycleFoundFunc on_cycle;     /**< 可为 NULL（发现即返回 true） */
    void *ctx;
} lvCycleDetectSpec;

/** @brief 通用三色环检测：返回是否存在环 */
bool lv_cycle_detect(const lvCycleDetectSpec *spec);

#ifdef __cplusplus
}
#endif

#endif /* lv_GRAPH_TRAVERSAL_H */
