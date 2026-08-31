/**
 * @file depth_limits.h
 * @brief 递归/嵌套深度限制 —— 单一权威表（K28 收敛）
 *
 * @details 收敛代码库中散落的 30+ 深度常量/机制：
 *          - 「递归 128」四处重复（recursion.h:58 / config.h:793 /
 *            lv_RUNTIME_GUARD_MAX_RECURSE config.h:818 + runtime_guard.h:53）
 *          - 推理深度 100/1000/10000 三套并存
 *          - 销毁深度文档声称 10000 vs 代码 200 的 50 倍差
 *
 *          本头为唯一事实源；消费方一律引用 lv_DEPTH_LIMIT_* 宏，
 *          禁止散落 #define。仅含编译期常量，无任何依赖（除标准头）。
 *
 * @version 1.0.0
 * @copyright Copyright (c) 2024-2026 Lv-00 Project
 */

#ifndef lv_DEPTH_LIMITS_H
#define lv_DEPTH_LIMITS_H

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 单一权威深度限制表
 * ============================================================ */

/**
 * @brief 全局递归熔断硬限（K28：原 128 四处合一）
 *
 * 任何递归调用深度超过此值时触发熔断（circuit breaker）。
 * 默认 128 足够覆盖绝大多数合法递归场景。
 */
#define lv_DEPTH_LIMIT_GLOBAL_RECURSION 128

/**
 * @brief 单个 RecursionContext 的硬上限
 *
 * 原 lv_MAX_RECURSION_DEPTH_LIMIT（recursion.h:263，100000）。
 * 测度验证的极端上限，远高于全局熔断阈值。
 */
#define lv_DEPTH_LIMIT_CONTEXT_MAX 100000

/**
 * @brief 解析 AST 深度上限（K28：A1 接线，原 parser_max_ast_depth 默认 256）
 *
 * lv_check_ast_depth 的安全上限；接入主解析链后，
 * 纯括号嵌套等无法被节点数拦住的深递归由此闸门兜底。
 */
#define lv_DEPTH_LIMIT_PARSE_AST 256

/**
 * @brief 推理/证明域递归深度（K28：100/1000/10000 三套统一）
 *
 * 外层引擎迭代不递归（实际 C 栈安全），语义上取 1000：
 * 覆盖证明搜索/推理链的合法深度，同时防止失控递归。
 */
#define lv_DEPTH_LIMIT_REASONING 1000

/**
 * @brief λ/公式递归域（K28）
 *
 * lambda_unify 5 族子递归（occurs_check/apply_subs/is_pattern 等）
 * 与 formula 树递归的统一上限。
 */
#define lv_DEPTH_LIMIT_LAMBDA 1024

/**
 * @brief 销毁/复制域统一深度（K28：裁决 200）
 *
 * 文档声称 10000（PROOF_TREE_DESTROY_MAX_DEPTH）vs 代码 200
 * 差 50 倍——取保守值 200（现实现值），销毁/复制递归
 * 优先改显式栈（样板已存在），此处为兜底上限。
 */
#define lv_DEPTH_LIMIT_DESTROY 200

/* ============================================================
 * 兼容别名（K28 收敛：引用权威，禁止散落 #define）
 * ============================================================ */

/** @brief 旧名兼容：全局递归熔断（原 recursion.h / config.h 双处） */
#ifndef lv_MAX_RECURSION_DEPTH
#define lv_MAX_RECURSION_DEPTH lv_DEPTH_LIMIT_GLOBAL_RECURSION
#endif

/** @brief 旧名兼容：RecursionContext 硬上限（原 recursion.h:263） */
#ifndef lv_MAX_RECURSION_DEPTH_LIMIT
#define lv_MAX_RECURSION_DEPTH_LIMIT lv_DEPTH_LIMIT_CONTEXT_MAX
#endif

/** @brief 旧名兼容：运行时守护递归上限（原 runtime_guard.h:53 / config.h:818） */
#ifndef lv_RUNTIME_GUARD_MAX_RECURSE
#define lv_RUNTIME_GUARD_MAX_RECURSE lv_DEPTH_LIMIT_GLOBAL_RECURSION
#endif

#ifdef __cplusplus
}
#endif

#endif /* lv_DEPTH_LIMITS_H */
