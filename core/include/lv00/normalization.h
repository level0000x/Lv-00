/**
 * @file normalization.h
 * @brief 归一化引擎 —— 节点合并、冗余消除与拓扑排序
 * @details 提供约束图的归一化流程，包括共线线段合并、重叠区域合并、
 * 合并候选检测与批量应用、拓扑排序、重写历史与循环检测等功能。
 */

#ifndef LV00_NORMALIZATION_H
#define LV00_NORMALIZATION_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

#include "constraint_graph.h"
#include "graph_hash.h"
#include "stream.h"
#include "symbolic_coord.h"

/**
 * @brief 设置规范化模块的流式上下文
 *
 * 设置后，graph_normalize() / merge_line_segments() / merge_regions()
 * 等函数在执行过程中会向该上下文发射规范化事件。
 * 设为 NULL 可禁用流式输出。
 *
 * @param ctx  流式上下文（可为 NULL）
 */
void normalization_set_stream_context(StreamContext *ctx);

/**
 * @brief 合并确认回调函数类型
 *
 * 当归一化引擎发现跨作用域的合并候选时调用。
 * 返回 true 接受合并，false 拒绝。
 *
 * @param node_a_id       节点 A 的 ID
 * @param node_b_id       节点 B 的 ID
 * @param scope_a_depth   节点 A 的作用域深度
 * @param scope_b_depth   节点 B 的作用域深度
 * @param parent_a        节点 A 的父节点 ID
 * @param parent_b        节点 B 的父节点 ID
 * @param user_data       用户数据
 * @return true 接受合并，false 拒绝
 */
typedef bool (*MergeConfirmCallback)(int node_a_id, int node_b_id, int scope_a_depth, int scope_b_depth, int parent_a,
                                     int parent_b, void *user_data);

/**
 * @brief 设置合并确认回调及其用户数据
 * @param[in] cb        回调函数
 * @param[in] user_data 用户数据
 */
void normalization_set_merge_callback(MergeConfirmCallback cb, void *user_data);

/**
 * @brief 获取当前的合并确认回调
 * @return 回调函数指针，未设置返回 NULL
 */
MergeConfirmCallback normalization_get_merge_callback(void);

/**
 * @brief 归一化日志条目
 *
 * 记录单次合并事件。
 */
typedef struct NormalizationLogEntry {
    int old_id;       /**< 被合并的节点 ID */
    int new_id;       /**< 保留的代表节点 ID */
    bool auto_merged; /**< true 表示自动合并，false 表示用户确认 */
} NormalizationLogEntry;

/**
 * @brief 归一化日志
 *
 * 记录归一化过程中执行的所有合并操作。
 */
typedef struct NormalizationLog {
    NormalizationLogEntry *entries; /**< 日志条目数组 */
    int count;                      /**< 条目数量 */
    int capacity;                   /**< 数组容量 */
} NormalizationLog;

/**
 * @brief 创建归一化日志
 * @param[in] initial_capacity 初始容量
 * @return 新创建的归一化日志，失败返回 NULL
 */
NormalizationLog *normalization_log_create(int initial_capacity);

/**
 * @brief 销毁归一化日志
 * @param[in,out] log 要销毁的日志
 */
void normalization_log_destroy(NormalizationLog *log);

/**
 * @brief 记录合并事件
 * @param[in,out] log        归一化日志
 * @param[in] old_id         被合并的节点 ID
 * @param[in] new_id         保留的节点 ID
 * @param[in] auto_merged    是否自动合并
 */
void normalization_log_record(NormalizationLog *log, int old_id, int new_id, bool auto_merged);

typedef struct NormalizationResult {
    int *merged_node_ids;
    int merged_count;
    int merged_capacity; /**< 预分配的合并记录数组容量（用于边界检查） */
    int *original_ids;
    int *representative_ids;
    bool user_confirmed;
    NormalizationLog *log; /**< 详细合并日志（结果拥有所有权） */
} NormalizationResult;

/**
 * @brief 对约束图执行归一化
 * @param[in,out] graph        约束图
 * @param[in] scope_aware     是否考虑作用域
 * @return 归一化结果，失败返回 NULL
 */
NormalizationResult *graph_normalize(ConstraintGraph *graph, bool scope_aware);

/**
 * @brief 销毁归一化结果
 * @param[in,out] result 要销毁的结果
 */
void normalization_result_destroy(NormalizationResult *result);

/**
 * @brief 验证归一化的幂等性
 *
 * 验证运行两次归一化是否产生相同结果。
 *
 * @param[in] graph 约束图
 * @return true 幂等，false 非幂等
 */
bool normalization_verify_idempotency(ConstraintGraph *graph);

/**
 * @brief 合并图中共线的线段
 *
 * @param[in,out] graph 约束图
 * @param[in,out] log   合并日志（可为 NULL）
 * @return 执行的合并数量，出错返回 -1
 */
int merge_line_segments(ConstraintGraph *graph, NormalizationLog *log);

/**
 * @brief 合并图中重叠或相邻的区域
 *
 * @param[in,out] graph 约束图
 * @param[in,out] log   合并日志（可为 NULL）
 * @return 执行的合并数量，出错返回 -1
 */
int merge_regions(ConstraintGraph *graph, NormalizationLog *log);

/**
 * @brief 节点合并候选 —— 描述两个可能需要合并的节点及其上下文信息
 *
 * 【scope_a / scope_b 的类型选择原因 —— 为什么使用 long long 而非 int】
 *   scope_a 和 scope_b 表示节点所在的作用域深度，类型为 long long 而非 int，
 *   原因如下：
 *   1. 深度嵌套场景：在复杂的几何构造中，模块嵌套深度可能很大（如多层子图
 *      嵌套、递归模块展开等），int 的 32 位范围（约 21 亿）虽然看似足够，
 *      但使用 long long（64 位）提供了更大的安全裕度。
 *   2. 特殊值编码：long long 允许使用 -1 等特殊值表示"无作用域"或"全局作用域"，
 *      而不会与正常的深度值冲突。int 虽然也能表示 -1，但在混合使用无符号深度
 *      计数器时可能产生隐式转换问题。
 *   3. 与约束图内部表示一致：ConstraintGraph 中节点的作用域深度字段也使用
 *      long long 类型，此处保持一致可避免类型转换和截断风险。
 *   4. 跨平台一致性：long long 在所有主流平台上保证至少 64 位宽度，
 *      而 int 的宽度可能因平台而异（尽管通常为 32 位）。
 */
typedef struct NodeMergeCandidate {
    int node_a_id;          /* 候选节点 A 的 ID */
    int node_b_id;          /* 候选节点 B 的 ID */
    SymbolicCoord *coord_a; /* 节点 A 的符号坐标 */
    SymbolicCoord *coord_b; /* 节点 B 的符号坐标 */
    long long scope_a;      /* 节点 A 的作用域深度 */
    long long scope_b;      /* 节点 B 的作用域深度 */
} NodeMergeCandidate;

NodeMergeCandidate *find_merge_candidates(const ConstraintGraph *graph, int *out_count);

/**
 * @brief 销毁合并候选数组
 * @param[in,out] candidates 合并候选数组
 * @param[in] count 数组长度
 */
void merge_candidates_destroy(NodeMergeCandidate *candidates, int count);

/**
 * @brief 应用合并候选列表
 *
 * @param[in,out] graph            约束图
 * @param[in] candidates            合并候选数组
 * @param[in] count                候选数量
 * @param[out] user_confirmed      用户是否确认了任何合并
 * @return 执行的合并数量，出错返回 -1
 */
int apply_merges(ConstraintGraph *graph, NodeMergeCandidate *candidates, int count, bool *user_confirmed);

/**
 * @brief 对图进行稳定的拓扑排序
 * @param[in,out] graph 约束图
 */
void graph_topological_sort_stable(ConstraintGraph *graph);

/* GraphHash, compute_complete_graph_hash, graph_hash_equal, graph_hash_destroy
 * are defined in graph_hash.h (included above). */

/**
 * @brief 重写历史记录
 *
 * 记录图哈希的历史，用于循环检测。
 */
typedef struct RewriteHistory {
    GraphHash **history; /**< 历史哈希数组 */
    int count;           /**< 历史记录数量 */
    int capacity;        /**< 数组容量 */
} RewriteHistory;

/**
 * @brief 创建重写历史记录
 * @param[in] capacity 初始容量
 * @return 新创建的重写历史记录，失败返回 NULL
 */
RewriteHistory *rewrite_history_create(int capacity);

/**
 * @brief 销毁重写历史记录
 * @param[in,out] history 要销毁的历史记录
 */
void rewrite_history_destroy(RewriteHistory *history);

/**
 * @brief 检查图哈希是否已存在于历史记录中（循环检测）
 *
 * @param[in] history 历史记录
 * @param[in] graph   约束图
 * @return true 检测到循环，false 无循环
 */
bool rewrite_history_check_cycle(const RewriteHistory *history, const ConstraintGraph *graph);

/**
 * @brief 添加图到历史记录
 * @param[in,out] history 历史记录
 * @param[in] graph   约束图
 */
void rewrite_history_add(RewriteHistory *history, ConstraintGraph *graph);

#ifdef __cplusplus
}
#endif

#endif /* LV00_NORMALIZATION_H */
