/**
 * @file proof_navigator.h
 * @brief 证明导航器、步骤管理、爆炸原理与断点管理
 *
 * 包含：
 * - 证明步骤管理 API
 * - 证明导航器导航 API
 * - 证明依赖链 API
 * - 爆炸原理与反证作用域
 * - 反证法证明
 * - 断点存储管理
 */

#ifndef LV00_PROOF_NAVIGATOR_H
#define LV00_PROOF_NAVIGATOR_H

#include "proof_proposition.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============== 证明步骤管理 ============== */

/**
 * @brief 创建证明步骤
 *
 * 分配并初始化一个新的证明步骤实例，类型由参数指定。
 * 新步骤的所有字段均初始化为零/NULL。
 *
 * @param[in] type 证明步骤类型（添加节点、重写、合一检查等）
 * @return 新创建的证明步骤指针，失败返回 NULL
 */
LV00_PUBLIC_API ProofStep *proof_step_create(ProofStepType type);

/**
 * @brief 销毁证明步骤
 *
 * 释放证明步骤及其所有动态分配的资源（依赖数组、合并节点数组、注释等）。
 *
 * @param[in] step 证明步骤指针（可为 NULL，此时函数无操作）
 */
LV00_PUBLIC_API void proof_step_destroy(ProofStep *step);

/**
 * 添加依赖关系
 */
LV00_PUBLIC_API bool proof_step_add_dependency(ProofStep *step, int dep_step_id);

/**
 * 设置断点
 */
LV00_PUBLIC_API void proof_step_set_breakpoint(ProofStep *step, bool is_breakpoint);

/**
 * @brief 获取证明步骤的完整祖先链（推导链）
 *
 * 从指定步骤开始，沿 parent_step_id 向上追溯，
 * 返回所有祖先步骤的 ID 列表。结果按从近到远排序
 * （最近祖先在前，根步骤在最后）。
 *
 * @param nav          证明导航器（用于查找步骤）
 * @param step_id      目标步骤 ID
 * @param out_ancestor_ids 输出：祖先步骤 ID 数组（调用者需用 lv00_free 释放）
 * @param out_count    输出：祖先数量（包含步骤本身为 0 时表示该步骤为根步骤）
 * @return true 成功，false 步骤不存在或参数无效
 */
LV00_PUBLIC_API bool proof_step_get_ancestors(const ProofNavigator *nav, int step_id, int **out_ancestor_ids, int *out_count);

/* ============== 证明导航器 ============== */

/**
 * 创建证明导航器
 * @param target 目标命题
 * @param engine 引擎上下文（可为NULL，但推荐提供以支持完整功能）
 */
LV00_PUBLIC_API ProofNavigator *proof_navigator_create(Proposition *target, LV00Engine *engine);

/**
 * 销毁证明导航器
 */
LV00_PUBLIC_API void proof_navigator_destroy(ProofNavigator *nav);

/**
 * 添加证明步骤
 */
LV00_PUBLIC_API bool proof_navigator_add_step(ProofNavigator *nav, ProofStep *step);

/**
 * 导航到下一步
 */
LV00_PUBLIC_API bool proof_navigator_next(ProofNavigator *nav);

/**
 * 导航到上一步
 */
LV00_PUBLIC_API bool proof_navigator_prev(ProofNavigator *nav);

/**
 * 跳转到指定步骤
 */
LV00_PUBLIC_API bool proof_navigator_goto(ProofNavigator *nav, int step_index);

/**
 * 跳转到下一个断点
 */
LV00_PUBLIC_API bool proof_navigator_next_breakpoint(ProofNavigator *nav);

/**
 * 获取当前步骤
 */
LV00_PUBLIC_API ProofStep *proof_navigator_current_step(ProofNavigator *nav);

/**
 * 计算最终颜色
 */
LV00_PUBLIC_API ProofColor proof_navigator_compute_final_color(ProofNavigator *nav);

/* ============== 证明依赖链 ============== */

/**
 * 创建证明依赖
 */
LV00_PUBLIC_API ProofDependency *proof_dependency_create(ProofColor color);

/**
 * 销毁证明依赖
 */
LV00_PUBLIC_API void proof_dependency_destroy(ProofDependency *dep);

/**
 * 添加子依赖
 */
LV00_PUBLIC_API bool proof_dependency_add_sub(ProofDependency *parent, ProofDependency *child);

/**
 * 计算依赖链颜色
 */
LV00_PUBLIC_API ProofColor proof_dependency_compute_color(ProofDependency *dep);

/* ============== 爆炸原理与反证作用域（v3.4-academic 整改） ============== */

/**
 * @brief 开启假设作用域
 *
 * 在证明导航器中开启一个新的假设作用域，用于反证法或条件推理。
 * 该作用域内的所有临时假设和推导结论都与全局上下文隔离。
 *
 * @param[in] nav        证明导航器
 * @param[in] assumption 临时假设命题（作用域内视为真）
 * @return 新作用域ID，失败返回 LV00_PROOF_SCOPE_INVALID
 */
LV00_PUBLIC_API Lv00ProofScopeId proof_begin_assumption_scope(ProofNavigator *nav, const Proposition *assumption);

/**
 * @brief 关闭假设作用域
 *
 * 关闭指定作用域，回收其下所有临时假设和条件性结论。
 * 若作用域内存在未解决的矛盾，应记录到 proof trace 中，
 * 但不得将矛盾结论泄漏到全局上下文。
 *
 * @param[in] nav      证明导航器
 * @param[in] scope_id 要关闭的作用域ID
 * @return true 成功关闭，false 作用域不存在或已关闭
 */
LV00_PUBLIC_API bool proof_close_assumption_scope(ProofNavigator *nav, Lv00ProofScopeId scope_id);

/**
 * @brief 检查作用域是否仍处于活动状态
 *
 * @param[in] nav      证明导航器
 * @param[in] scope_id 作用域ID
 * @return true 作用域仍活动，false 已关闭或无效
 */
LV00_PUBLIC_API bool proof_scope_is_active(const ProofNavigator *nav, Lv00ProofScopeId scope_id);

/**
 * @brief 检查命题是否在全局上下文中被证明
 *
 * 用于验证局部矛盾闭包的安全性：即使局部反证推出了某命题，
 * 也应确认该命题未被无界加入全局证明上下文。
 *
 * @param[in] nav  证明导航器
 * @param[in] prop 要检查的命题
 * @return true 命题在全局上下文中，false 不在或仅条件性成立
 */
LV00_PUBLIC_API bool proof_has_global_proposition(const ProofNavigator *nav, const Proposition *prop);

/**
 * 创建爆炸原理函数块
 * @param graph 约束图
 * @param out_block_id 输出的函数块ID
 * @return 是否成功
 */
LV00_PUBLIC_API bool proof_create_ex_falso_block(ConstraintGraph *graph, int *out_block_id);

/**
 * 应用爆炸原理（带作用域限定）
 *
 * 在指定作用域内应用爆炸原理：从矛盾推出任意命题。
 * 该结论仅在给定作用域内有效，不得自动扩散到全局上下文。
 * 若 scope_id 为 LV00_PROOF_SCOPE_GLOBAL，则要求 bottom_proof
 * 必须是在无额外假设下导出的全局矛盾。
 *
 * @param nav         证明导航器
 * @param bottom_proof ⊥的证物（矛盾约束图）
 * @param target_prop 目标命题
 * @param scope_id    作用域ID（限定结论有效性范围）
 * @return 是否成功
 */
LV00_PUBLIC_API bool proof_apply_ex_falso_scoped(ProofNavigator *nav, ConstraintGraph *bottom_proof,
                                  Proposition *target_prop, Lv00ProofScopeId scope_id);

/**
 * 应用爆炸原理（兼容包装）
 *
 * 旧版无界爆炸原理的兼容接口。实现应默认拒绝无作用域的全局爆炸，
 * 或仅在 bottom_proof 明确标记为全局矛盾时允许。
 * 新代码应优先使用 proof_apply_ex_falso_scoped。
 *
 * @param nav         证明导航器
 * @param bottom_proof ⊥的证物
 * @param target_prop 目标命题
 * @return 是否成功
 */
LV00_PUBLIC_API bool proof_apply_ex_falso(ProofNavigator *nav, ConstraintGraph *bottom_proof, Proposition *target_prop);

/* ============== 反证法证明 ============== */

/**
 * @brief 反证法证明结果 —— 包含矛盾路径和证明追踪树
 *
 * 当反证法成功时，该结构记录完整的矛盾推导路径。
 * 失败的证明也记录尝试的路径，用于调试和学习。
 */
typedef struct Lv00ProofTree Lv00ProofTree; /* 前向声明，完整定义见 proof_trace.h */

typedef struct {
    bool              success;           /**< 反证法是否成功 */
    char             *contradiction_desc; /**< 矛盾的描述（如"P ∧ ¬P 同时成立"） */
    int               contradiction_step; /**< 发现矛盾的步骤索引（-1 = 未发现） */
    struct Lv00ProofTree *proof_trace;    /**< 完整的证明追踪树（成功时记录完整路径，失败时也记录已探索路径） */
    int               total_steps;       /**< 反证法证明的总步骤数 */
    int               forward_steps;     /**< 正向推理步骤数 */
    char             *error_message;     /**< 错误消息（失败时有效，可为 NULL） */
} Lv00ContradictionResult;

/**
 * @brief 执行反证法证明
 *
 * 反证法（归谬法）工作流程：
 * 1. 假设目标命题的否定成立（¬goal）
 * 2. 将否定假设作为临时前提加入证明环境
 * 3. 正向推理：从否定的假设出发，尽可能多地推导出结论
 * 4. 矛盾检测：检查推导出的结论是否与已知公理或已证定理冲突
 * 5. 如果发现矛盾，则反证法成功，原命题得证
 * 6. 记录整个矛盾推导路径到 proof_trace 中
 *
 * 关键设计原则：
 * - 矛盾分支与主证明隔离：否定假设只在矛盾分支内有效，
 *   不会污染主证明上下文。使用独立的 ProofNavigator 实例。
 * - 记录完整路径：成功和失败的情况都记录已探索的推导路径，
 *   方便用户理解证明过程和排查失败原因。
 *
 * @param nav         主证明导航器（不会被修改，仅用于获取引擎上下文和已证定理）
 * @param goal_prop   待证明的目标命题
 * @param max_steps   最大允许的正向推理步骤数（0 = 无限制）
 * @return 反证法结果，包含成功标志和矛盾路径。调用者需用 lv00_contradiction_result_destroy 释放。
 */
LV00_PUBLIC_API Lv00ContradictionResult *lv00_proof_by_contradiction(ProofNavigator *nav, const Proposition *goal_prop, int max_steps);

/**
 * @brief 释放反证法结果
 *
 * 释放 Lv00ContradictionResult 中所有动态分配的内存，
 * 包括证明追踪树、矛盾描述和错误消息。
 *
 * @param result  反证法结果（可为 NULL）
 */
LV00_PUBLIC_API void lv00_contradiction_result_destroy(Lv00ContradictionResult *result);

/**
 * 交互式证明步骤
 * 允许用户引导证明构建
 * @param nav 证明导航器
 * @param step_type 步骤类型
 * @param step_data 步骤数据（类型取决于 step_type）
 * @return true 成功，false 验证失败
 */
LV00_PUBLIC_API bool proof_interactive_step(ProofNavigator *nav, ProofStepType step_type, const void *step_data);

/**
 * 保存证明断点
 * 在指定断点ID处保存当前证明状态，以便后续继续
 * @param nav 证明导航器
 * @param breakpoint_id 断点ID
 * @return true 成功，false 失败
 */
LV00_PUBLIC_API bool proof_save_breakpoint(ProofNavigator *nav, int breakpoint_id);

/**
 * 恢复证明断点
 * 从指定断点ID处恢复之前保存的证明状态
 * @param nav 证明导航器
 * @param breakpoint_id 断点ID
 * @return true 成功，false 失败
 */
LV00_PUBLIC_API bool proof_restore_breakpoint(ProofNavigator *nav, int breakpoint_id);

/* ============== 断点存储管理（v3.4.1 新增） ============== */

/**
 * @brief 初始化断点存储系统
 *
 * 重置指定导航器的断点存储。在使用断点功能前调用。
 * 可重复调用，后续调用会重置存储状态。
 *
 * @param nav 证明导航器
 */
LV00_PUBLIC_API void proof_breakpoint_storage_init(ProofNavigator *nav);

/**
 * @brief 重置断点存储系统
 *
 * 清除所有已保存的断点快照，释放相关资源。
 * 调用后断点存储回到初始状态。
 *
 * @param nav 证明导航器
 */
LV00_PUBLIC_API void proof_breakpoint_storage_reset(ProofNavigator *nav);

/**
 * @brief 获取当前断点存储中的断点数量
 *
 * @param nav 证明导航器
 * @return 当前存储的断点数量
 */
LV00_PUBLIC_API int proof_breakpoint_storage_count(const ProofNavigator *nav);

/**
 * @brief 删除指定的断点快照
 *
 * 从存储中移除指定ID的断点快照。
 *
 * @param nav 证明导航器
 * @param breakpoint_id 要删除的断点ID
 * @return true 成功删除，false 未找到该断点
 */
LV00_PUBLIC_API bool proof_breakpoint_delete(ProofNavigator *nav, int breakpoint_id);

#ifdef __cplusplus
}
#endif

#endif /* LV00_PROOF_NAVIGATOR_H */
