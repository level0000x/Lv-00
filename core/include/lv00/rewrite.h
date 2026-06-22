/* ========================================================================
 * 模块名称：重写引擎 (rewrite)
 * 功能概述：提供约束图的规则匹配、替换与循环检测功能。支持 VF2 子图
 *          同构匹配、规则热加载/卸载、图快照事务回滚、WL 图核哈希
 *          循环检测、多非重叠匹配批量应用以及归约度量验证。
 *          同时借鉴 Maude 的策略组合子和 Herbie 的数值精度优化。
 *
 * 主要 API：
 *   - rewrite_rule_create / rewrite_rule_destroy  — 创建/销毁重写规则
 *   - find_rewrite_match / apply_rewrite           — 查找/应用匹配
 *   - rewrite_with_rules                           — 多规则重写
 *   - rewrite_strategy_apply                       — Maude 风格策略执行
 *   - rewrite_search_backward                      — BFS/DFS 逆向证明搜索
 *   - graph_snapshot_create / restore / destroy    — 图快照事务
 *   - rewrite_num_optimize                         — Herbie 风格数值优化
 *
 * 使用示例：
 LV00_PUBLIC_API *   RewriteRule *rule = rewrite_rule_create("simplify", pattern, replacement, 1);
 LV00_PUBLIC_API *   RewriteMatch *match = find_rewrite_match(graph, rule, false);
 *   if (match) { apply_rewrite(graph, rule, match); }
 *
 * ======================================================================== */

/**
 * @file rewrite.h
 * @brief 重写引擎 —— 规则匹配、替换与循环检测
 */

#ifndef LV00_REWRITE_H
#define LV00_REWRITE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "constraint_graph.h"
#include "graph_hash.h"
#include "stream.h"

/**
 * @brief 设置重写引擎的流式输出上下文
 * @param ctx  流式上下文（可为 NULL 以禁用流式输出）
 */
LV00_PUBLIC_API void rewrite_set_stream_context(StreamContext *ctx);

typedef struct RewritePattern {
    int kind;           /* type kind (TypeKind) — used by type_system.c */
    int *variable_node_ids;
    int var_count;
    Constraint **pattern_constraints;
    int pattern_constraint_count;
} RewritePattern;

typedef struct RewriteReplacement {
    int **node_bindings;
    int binding_count;
    Constraint **replacement_constraints;
    int replacement_constraint_count;
    int *new_nodes;
    int new_node_count;
    GeomType *new_node_types; /* 新节点的几何类型（与 new_nodes 一一对应） */
} RewriteReplacement;

typedef struct RewriteMatch {
    int *node_bindings;
    int *constraint_bindings;
    int binding_count;
} RewriteMatch;

/* 前置条件评估回调类型 */
typedef bool (*RewritePrecondition)(ConstraintGraph *graph, RewriteMatch *match, void *user_data);

/* 扩展的重写规则，支持前置条件 */
typedef struct RewriteRule {
    RewritePattern *pattern;
    RewriteReplacement *replacement;
    int reduction_measure;
    char *name;
    /* 前置条件系统 */
    RewritePrecondition condition_func;
    void *condition_data;
} RewriteRule;

/**
 * @brief 重写状态枚举
 * 【枚举值命名规范】所有枚举值使用 UPPER_SNAKE_CASE
 */
typedef enum {
    REWRITE_STATUS_OK,               /**< 重写成功（无操作） */
    REWRITE_STATUS_NO_MATCH,         /**< 未找到匹配 */
    REWRITE_STATUS_APPLIED,          /**< 规则已应用 */
    REWRITE_STATUS_CONFLUENCE_ISSUE, /**< 汇流性问题 */
    REWRITE_STATUS_TERMINATED        /**< 重写终止 */
} RewriteStatus;

/* VF2 子图同构匹配状态 */
typedef struct {
    int *core_1;      /* core_1[pattern_node] = target_node 或 -1 */
    int *core_2;      /* core_2[target_node] = pattern_node 或 -1 */
    int core_count;   /* 已匹配对数 */
    int *in_1;        /* in_1[pattern_node]: 1 表示在 M_1 中（前驱已映射） */
    int *out_1;       /* out_1[pattern_node]: 1 表示在 T_1 中（后继未映射） */
    int *in_2;        /* in_2[target_node]: 1 表示在 M_2 中 */
    int *out_2;       /* out_2[target_node]: 1 表示在 T_2 中 */
    int pattern_size; /* 模式图节点数 */
    int target_size;  /* 目标图节点数 */
    /* in/out 集合：用于 VF2 剪枝优化 */
    int *in_set;      /* 已匹配的目标节点索引集合 */
    int in_count;     /* in_set 中的元素数量 */
    int *out_set;     /* 已排除的目标节点索引集合 */
    int out_count;    /* out_set 中的元素数量 */
    int in_capacity;  /* in_set 的容量 */
    int out_capacity; /* out_set 的容量 */
} VF2State;

/* WL (Weisfeiler-Lehman) 图核哈希历史 */
#ifndef WL_ITERATIONS
#define WL_ITERATIONS 3
#endif
#ifndef WL_HISTORY_SIZE
#define WL_HISTORY_SIZE 64
#endif

typedef struct {
    uint64_t *hash_history;       /* 最近图哈希的环形缓冲区（完整 WL 哈希） */
    int history_count;            /* 缓冲区中的哈希数量 */
    int history_pos;              /* 环形缓冲区当前位置 */
    uint32_t *light_hash_history; /* 轻量哈希环形缓冲区（用于快速预筛选） */
    int light_history_count;      /* 轻量哈希缓冲区中的哈希数量 */
    int light_history_pos;        /* 轻量哈希环形缓冲区当前位置 */
} WLHashHistory;

/* ---- 图快照——用于重写替换操作的事务性回滚 ---- */

/**
 * @brief 图快照——用于重写替换操作的事务性回滚
 *
 * 深拷贝约束图的所有节点和约束，包括符号坐标、端口信息、
 * 函数块内部数据等。用于在替换操作产生冲突时恢复到替换前状态。
 *
 * port_refs / region_refs / fb_refs 保存深拷贝时被清零的交叉引用信息
 *（节点 ID），以便在 graph_snapshot_restore 时根据 ID 重新绑定指针。
 */

/* PORT 节点的 connected_to 交叉引用（保存为节点 ID） */
typedef struct {
    int port_node_index; /* 快照中 PORT 节点的索引 */
    int connected_to_id; /* 原始 connected_to 目标的节点 ID（-1 表示无连接） */
} PortRef;

/* REGION 节点的 boundary_segments 交叉引用（保存为节点 ID） */
typedef struct {
    int region_node_index; /* 快照中 REGION 节点的索引 */
    int *segment_ids;      /* boundary_segments 中每个元素的节点 ID 数组 */
    int segment_count;     /* boundary_segments 数量 */
} RegionRef;

/* FUNCTION_BLOCK 节点的 internal_nodes 交叉引用（保存为节点 ID） */
typedef struct {
    int fb_node_index;       /* 快照中 FUNCTION_BLOCK 节点的索引 */
    int *internal_node_ids;  /* internal_nodes 中每个元素的节点 ID 数组 */
    int internal_node_count; /* internal_nodes 数量 */
} FBRef;

typedef struct GraphSnapshot {
    GeomNode **nodes; /* 节点数组的深拷贝 */
    int node_count;
    int node_capacity;
    Constraint **constraints; /* 约束数组的深拷贝 */
    int constraint_count;
    int constraint_capacity;
    int next_node_id;
    int next_constraint_id;
    PortRef *port_refs; /* PORT 节点的 connected_to ID 信息 */
    int port_ref_count;
    RegionRef *region_refs; /* REGION 节点的 boundary_segments ID 信息 */
    int region_ref_count;
    FBRef *fb_refs; /* FUNCTION_BLOCK 节点的 internal_nodes ID 信息 */
    int fb_ref_count;
} GraphSnapshot;

/**
 * @brief 创建约束图的快照
 *
 * 深拷贝所有节点（包括符号坐标、端口、区域边界、函数块数据）
 * 和约束（包括参与者数组）。不拷贝哈希索引，恢复时重建。
 *
 * @param[in] graph  要快照的约束图
 * @return 新分配的 GraphSnapshot，调用者需用 graph_snapshot_destroy 释放
 */
LV00_PUBLIC_API GraphSnapshot *graph_snapshot_create(const ConstraintGraph *graph);

/**
 * @brief 从快照恢复约束图
 *
 * 1. 销毁当前图中的所有节点和约束
 * 2. 从快照深拷贝恢复所有节点和约束
 * 3. 重建哈希索引
 *
 * @param[in] snapshot  快照
 * @param[in,out] graph  要恢复的约束图
 * @return true 恢复成功，false 失败
 */
LV00_PUBLIC_API bool graph_snapshot_restore(GraphSnapshot *snapshot, ConstraintGraph *graph);

/**
 * @brief 销毁快照，释放所有深拷贝的数据
 */
LV00_PUBLIC_API void graph_snapshot_destroy(GraphSnapshot *snapshot);

/* ---- 规则热加载/卸载 ---- */

/**
 * @brief 从文件加载重写规则
 *
 * 解析 .lvz 格式的规则定义文件，创建 RewriteRule 数组。
 * 每条规则包含名称、模式、替换和优先级。
 *
 * @param[in]  filepath   规则定义文件路径
 * @param[out] out_rules  接收分配的规则数组（调用者需释放每个规则和数组本身）
 * @param[out] out_count  接收加载的规则数量
 * @return 加载的规则数量（>=0），或 -1 表示错误
 */
LV00_PUBLIC_API int rewrite_rules_load_from_file(const char *filepath, RewriteRule ***out_rules, int *out_count);

/**
 * @brief 卸载指定名称的重写规则
 *
 * 从规则数组中移除指定名称的规则。
 * 不影响已应用该规则的历史步骤。
 *
 * @param[in,out] rules  规则数组的指针（会被更新）
 * @param[in,out] count  规则数量的指针（会被更新）
 * @param[in]     rule_name  要卸载的规则名称
 * @return true 卸载成功，false 未找到或失败
 */
LV00_PUBLIC_API bool rewrite_rule_unload(RewriteRule ***rules, int *count, const char *rule_name);

/* ---- 基础重写 API ---- */

/**
 * @brief 创建重写规则
 *
 * 创建一条包含模式、替换和归约度量的重写规则。
 *
 * @param[in] name       规则名称
 * @param[in] pattern    匹配模式
 * @param[in] replacement 替换内容
 * @param[in] measure    归约度量值（用于循环检测）
 * @return 新创建的重写规则（调用者负责释放），失败返回 NULL
 */
LV00_PUBLIC_API RewriteRule *rewrite_rule_create(const char *name, RewritePattern *pattern, RewriteReplacement *replacement,
                                 int measure);

/**
 * @brief 销毁重写规则
 * @param rule 要销毁的重写规则
 */
LV00_PUBLIC_API void rewrite_rule_destroy(RewriteRule *rule);

/**
 * @brief 查找匹配的重写规则
 *
 * 在约束图中查找与给定规则模式匹配的位置。
 *
 * @param[in] graph                        约束图
 * @param[in] rule                        重写规则
 * @param[in] local_equivalence_tolerant  是否容忍局部等价
 * @return 匹配的节点绑定信息（调用者负责释放），失败返回 NULL
 */
LV00_PUBLIC_API RewriteMatch *find_rewrite_match(ConstraintGraph *graph, RewriteRule *rule, bool local_equivalence_tolerant);

/**
 * @brief 应用重写规则
 *
 * @param[in,out] graph 约束图
 * @param[in] rule        重写规则
 * @param[in] match       匹配信息
 * @return 重写状态
 */
LV00_PUBLIC_API RewriteStatus apply_rewrite(ConstraintGraph *graph, RewriteRule *rule, RewriteMatch *match);

/**
 * @brief 使用多个规则重写约束图
 *
 * @param[in,out] graph                约束图
 * @param[in] rules                    规则数组
 * @param[in] rule_count               规则数量
 * @param[in] step_limit               最大步数限制
 * @param[in] normalize_between_steps   是否在每步之间规范化
 * @return 重写状态
 */
LV00_PUBLIC_API RewriteStatus rewrite_with_rules(ConstraintGraph *graph, RewriteRule **rules, int rule_count, int step_limit,
                                 bool normalize_between_steps);

/* ---- VF2 子图同构匹配 ---- */

/**
 * @brief 使用 VF2 算法查找匹配
 *
 * @param[in] target_graph             目标约束图
 * @param[in] pattern                  匹配模式
 * @param[in] local_equivalence_tolerant 是否容忍局部等价
 * @return 匹配的节点绑定信息（调用者负责释放），失败返回 NULL
 */
LV00_PUBLIC_API RewriteMatch *vf2_find_match(ConstraintGraph *target_graph, RewritePattern *pattern, bool local_equivalence_tolerant);

/* ---- WL 图核哈希循环检测 ---- */

/**
 * @brief 使用 WL 图核哈希检测重写循环
 *
 * @param[in,out] graph 约束图
 * @param[in] hist       WL 哈希历史记录
 * @return 重写状态
 */
LV00_PUBLIC_API RewriteStatus detect_rewrite_loop_wl(ConstraintGraph *graph, WLHashHistory *hist);

/* ---- WL 哈希历史管理 ---- */

/**
 * @brief 初始化 WL 哈希历史
 * @param[out] hist WL 哈希历史记录
 */
LV00_PUBLIC_API void wl_history_init(WLHashHistory *hist);

/**
 * @brief 销毁 WL 哈希历史
 * @param[in,out] hist WL 哈希历史记录
 */
LV00_PUBLIC_API void wl_history_destroy(WLHashHistory *hist);

/* ---- 最佳匹配选择 ---- */

/**
 * @brief 查找最佳匹配
 *
 * 在约束图中查找与给定规则模式最佳匹配的子图。
 *
 * @param[in] graph                        约束图
 * @param[in] rule                        重写规则
 * @param[in] local_equivalence_tolerant  是否容忍局部等价
 * @return 最佳匹配（调用者负责释放），失败返回 NULL
 */
LV00_PUBLIC_API RewriteMatch *find_best_match(ConstraintGraph *graph, RewriteRule *rule, bool local_equivalence_tolerant);

/* ---- 重写度量验证 ---- */

/**
 * @brief 验证重写度量
 *
 * 在应用重写规则后，验证归约度量是否确实减少了。
 *
 * @param[in] graph          重写后的约束图
 * @param[in] rule           所应用的重写规则（提供 reduction_measure）
 * @param[in] graph_before   重写前的图哈希快照
 * @return true 如果度量满足归约/扩展要求，false 如果没有
 */
LV00_PUBLIC_API bool rewrite_validate_measure(const ConstraintGraph *graph, const RewriteRule *rule, const GraphHash *graph_before);

/* ---- WL 图核哈希（公开接口） ---- */

/**
 * @brief 计算约束图的 Weisfeiler-Lehman 图核哈希
 *
 * 使用 WL 迭代算法计算图的拓扑哈希值。
 * 初始标签基于节点类型和约束拓扑（忽略坐标值），
 * 每轮迭代根据邻居标签更新当前节点标签，
 * 最终通过全局哈希聚合得到图的 WL 哈希。
 *
 * WL 哈希主要用于：
 * - 重写循环检测：在 detec_rewrite_loop_wl 中使用
 * - 图同构快速过滤：两个图同构的必要条件是 WL 哈希相等
 * - 图去重：避免对相同拓扑结构的图重复处理
 *
 * @param[in] graph  约束图
 * @return 64位 WL 图哈希值，失败返回 0
 */
LV00_PUBLIC_API uint64_t rewrite_compute_wl_hash(const ConstraintGraph *graph);

/* ---- 多非重叠匹配查找与批量应用 (design_v2.9.md Section 6.4) ---- */

/**
 * @brief 查找所有非重叠的子图同构匹配
 *
 * 在约束图中反复使用 VF2 算法查找与给定规则模式匹配的子图同构。
 * 每找到一个匹配后，将其匹配的节点标记为已使用，继续搜索直到
 * 无法找到新的非重叠匹配。最终返回按匹配质量（匹配约束数降序）排序的数组。
 *
 * @param[in]  graph           约束图
 * @param[in]  rule            重写规则（提供模式）
 * @param[in]  used_node_ids   外部传入的已使用节点 ID 数组（可为 NULL）
 * @param[in]  used_count      已使用节点数量
 * @param[out] out_matches     接收分配的匹配数组（调用者需释放每个匹配和数组本身）
 * @param[out] out_match_count 接收找到的匹配数量
 * @return 0 成功，-1 参数错误或内存不足
 */
LV00_PUBLIC_API int find_all_non_overlapping_matches(ConstraintGraph *graph, RewriteRule *rule, const int *used_node_ids,
                                     int used_count, RewriteMatch ***out_matches, int *out_match_count);

/**
 * @brief 批量应用非重叠匹配
 *
 * 对一组非重叠匹配依次应用重写规则。对每个匹配创建图快照，
 * 尝试应用替换。如果替换产生冲突或节点已被前序替换修改，则跳过。
 *
 * @param[in]  graph            约束图
 * @param[in]  rule             重写规则
 * @param[in]  matches          匹配数组（由 find_all_non_overlapping_matches 返回）
 * @param[in]  match_count      匹配数量
 * @param[out] out_applied_count 接收成功应用的替换数量
 * @return 0 成功，-1 参数错误或内存不足
 */
LV00_PUBLIC_API int rewrite_apply_all_matches(ConstraintGraph *graph, RewriteRule *rule, RewriteMatch *matches, int match_count,
                              int *out_applied_count);

/* ================================================================
 * === 第六梯队参考项目落地 (P1) — Maude 重写策略引擎 ==============
 * === 2026-05-24 ==================================================
 *
 * 借鉴 Maude (github.com/maude-team/maude) 的重写逻辑：
 *   - sort → Lv-00 GeomType  + TypeRegion
 *   - op   → Lv-00 FuncBlock
 *   - eq   → Lv-00 RewriteRule（单向/双向）
 *   - rl   → Lv-00 图重写规则
 *   - strat → rewrite_strategy_apply() 策略组合子
 *   - search → rewrite_search_backward() 反向证明搜索
 * ================================================================ */

/** @brief Maude 风格重写策略组合子（10 种）
 * 【枚举值命名规范】所有枚举值使用 UPPER_SNAKE_CASE
 */
typedef enum {
    REWRITE_STRATEGY_KIND_IDLE,          /**< idle:  不执行任何操作 */
    REWRITE_STRATEGY_KIND_FAIL,          /**< fail:  总是失败 */
    REWRITE_STRATEGY_KIND_APPLY_RULE,    /**< apply rule_id: 应用指定规则 */
    REWRITE_STRATEGY_KIND_MATCH_PATTERN, /**< match pattern: 匹配模式不替换 */
    REWRITE_STRATEGY_KIND_TEST_COND,     /**< test condition: 条件检查 */
    REWRITE_STRATEGY_KIND_SEQUENCE,      /**< s1 ; s2: 顺序组合 */
    REWRITE_STRATEGY_KIND_ORELSE,        /**< s1 or-else s2: 回退组合 */
    REWRITE_STRATEGY_KIND_REPEAT,        /**< repeat s: 重复直到不动点 */
    REWRITE_STRATEGY_KIND_NORMALIZE,     /**< normalize s: 规范化（等价于 repeat(s ; s)） */
    REWRITE_STRATEGY_KIND_TRY            /**< try s: 尝试，失败则保持原状 */
} RewriteStrategyKind;

/** @brief 可执行重写策略树节点 */
typedef struct RewriteStrategy {
    RewriteStrategyKind kind;
    /* --- 叶节点数据（用于 APPLY_RULE / MATCH_PATTERN / TEST_COND）--- */
    int rule_id;              /* APPLY_RULE: 规则索引 */
    char *pattern_expr;       /* MATCH_PATTERN: 模式表达式 */
    int (*test_func)(void *); /* TEST_COND: 条件测试函数 */
    void *test_ctx;           /* TEST_COND: 上下文 */
    /* --- 内部节点数据（用于 SEQUENCE / ORELSE / REPEAT 等）--- */
    struct RewriteStrategy *left;
    struct RewriteStrategy *right;
    int max_iterations; /* REPEAT: 最大迭代次数（0 = 不限） */
} RewriteStrategy;

/* ---- 策略树构造与销毁 ---- */
LV00_PUBLIC_API RewriteStrategy *rewrite_strategy_create_idle(void);
LV00_PUBLIC_API RewriteStrategy *rewrite_strategy_create_fail(void);
LV00_PUBLIC_API RewriteStrategy *rewrite_strategy_create_apply_rule(int rule_id);
LV00_PUBLIC_API RewriteStrategy *rewrite_strategy_create_match(const char *pattern);
RewriteStrategy *rewrite_strategy_create_test(int (*test)(void *), void *ctx);
LV00_PUBLIC_API RewriteStrategy *rewrite_strategy_sequence(RewriteStrategy *left, RewriteStrategy *right);
LV00_PUBLIC_API RewriteStrategy *rewrite_strategy_orelse(RewriteStrategy *left, RewriteStrategy *right);
LV00_PUBLIC_API RewriteStrategy *rewrite_strategy_repeat(RewriteStrategy *child, int max_iter);
LV00_PUBLIC_API RewriteStrategy *rewrite_strategy_normalize(RewriteStrategy *child);
LV00_PUBLIC_API RewriteStrategy *rewrite_strategy_try(RewriteStrategy *child);
LV00_PUBLIC_API void rewrite_strategy_destroy(RewriteStrategy *s);

/**
 * @brief 按策略表达式在约束图上递归执行重写
 *
 * 这是 Maude `srewrite` 命令的 Lv-00 对应：
 *   给定策略树和约束图，按策略定义的顺序尝试所有规则，
 *   每次成功匹配后返回新的约束图状态。
 *
 * @param graph          输入约束图（调用者持有所有权，不会被修改）
 * @param strategy       策略树根节点
 * @param rules          可用重写规则数组
 * @param rule_count     规则数量
 * @param out_graph      输出：重写后的约束图（调用者负责释放）
 * @param out_steps      输出：实际执行的重写步数
 * @return 是否至少成功了一步
 */
LV00_PUBLIC_API bool rewrite_strategy_apply(const ConstraintGraph *graph, const RewriteStrategy *strategy, const RewriteRule *rules,
                            int rule_count, ConstraintGraph **out_graph, int *out_steps);

/**
 * @brief BFS/DFS 逆向证明搜索（Maude `search =>*` 的 Lv-00 对应）
 *
 * 从目标几何命题（目标约束图）出发，逆向应用公理/规则，
 * 搜索能归约到已知公理的重写路径。
 *
 * @param target_graph  目标约束图（要证明的几何命题）
 * @param rules         可用重写规则（公理+派生规则）
 * @param rule_count    规则数量
 * @param max_depth     搜索深度上限
 * @param use_bfs       true=BFS（找最短证明），false=DFS（找任意证明）
 * @param out_path      输出：重写步骤序列（调用者释放，每个元素是 rule_id）
 * @param out_path_len  输出：路径长度
 * @return 是否找到证明路径
 */
LV00_PUBLIC_API bool rewrite_search_backward(const ConstraintGraph *target_graph, const RewriteRule *rules, int rule_count,
                             int max_depth, bool use_bfs, int **out_path, int *out_path_len);

/* ================================================================
 * === 第六梯队参考项目落地 (P1) — Herbie 数值精度优化 ==============
 * === 2026-05-24 ==================================================
 *
 * 借鉴 Herbie (herbie.uwplse.org) 的数值精度优化方法：
 *   - Herbie 使用 Pareto 最优搜索发现数值更稳定的等价表达式
 *   - 通过随机采样检测灾难性抵消、条件数恶化等数值问题
 *   - Lv-00 将 Herbie 的浮点优化思想适配到精确有理数/代数数领域
 * ================================================================ */

/* ============== 数值精度优化规则（Herbie 风格） ============== */

/** 数值精度优化规则优先级（借鉴 Herbie Pareto 最优重写搜索） */
typedef enum {
    REWRITE_NUM_CRITICAL = 0, /**< 关键：消除灾难性抵消 */
    REWRITE_NUM_HIGH = 1,     /**< 高：改善条件数 */
    REWRITE_NUM_MEDIUM = 2,   /**< 中：重组表达式 */
    REWRITE_NUM_LOW = 3       /**< 低：微调不影响正确性 */
} RewriteNumPriority;

/** 数值重写规则（Herbie 风格 — 自动发现数值更稳定的等价表达式） */
typedef struct RewriteNumRule {
    char *name;                             /**< 规则名称（如 "sqrt-diff-recip"） */
    char *pattern_expr;                     /**< 模式表达式（如 "sqrt(x+1) - sqrt(x)"） */
    char *replacement_expr;                 /**< 替换表达式（如 "1/(sqrt(x+1)+sqrt(x))"） */
    RewriteNumPriority priority;            /**< 优先级 */
    double accuracy_improvement;            /**< 精度改进倍数（估计值） */
    char *condition_desc;                   /**< 触发条件描述（如 "x > 100 时有效"） */
    bool (*condition)(double *vars, int n); /**< 条件检测函数 */
} RewriteNumRule;

/** 创建数值重写规则 */
LV00_PUBLIC_API RewriteNumRule *rewrite_num_rule_create(const char *name, const char *pattern, const char *replacement,
                                        RewriteNumPriority pri, double improvement);

/** 销毁数值重写规则 */
LV00_PUBLIC_API void rewrite_num_rule_destroy(RewriteNumRule *rule);

/** 在表达式上应用数值优化规则（返回优化后的表达式字符串，调用者释放） */
LV00_PUBLIC_API char *rewrite_num_optimize(const char *expr, RewriteNumRule **rules, int rule_count, double *out_improvement);

/** 注册内置数值优化规则集（含 sqrt-diff-recip, quadratic-formula-avoid-cancel 等 6 条） */
LV00_PUBLIC_API int rewrite_num_register_builtins(void);

/** 获取已注册的数值规则数量 */
LV00_PUBLIC_API int rewrite_num_rule_count(void);

#ifdef __cplusplus
}
#endif

#endif /* LV00_REWRITE_H */