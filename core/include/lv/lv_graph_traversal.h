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

#ifdef __cplusplus
}
#endif

#endif /* lv_GRAPH_TRAVERSAL_H */