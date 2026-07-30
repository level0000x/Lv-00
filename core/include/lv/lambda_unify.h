/**
 * @file lambda_unify.h
 * @brief λ-演算合一 API：句法合一 + Miller 模式合一
 *
 * 实现了 λ-项之间的合一匹配算法：
 * - 句法合一（syntactic unification）：处理 LV_LAMBDA_VAR/ABS/APP 的树结构匹配
 * - 模式合一（pattern unification）：Miller 可判定高阶合一子集
 * - 替换应用与合一结果集成
 */

#ifndef lv_LAMBDA_UNIFY_H
#define lv_LAMBDA_UNIFY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "lv/lambda_term.h"

/* 前向声明 */
struct ConstraintGraph;

/* ── 合一结果状态 ── */

typedef enum {
    LAMBDA_UNIFY_OK,           /**< 合一成功 */
    LAMBDA_UNIFY_FAIL,         /**< 合一失败（无法匹配） */
    LAMBDA_UNIFY_OCCURS_CHECK, /**< 变量出现在自身中 */
    LAMBDA_UNIFY_ERROR         /**< 内部错误（如最大深度超限） */
} LambdaUnifyStatus;

/* ── 合一替换：将 De Bruijn 索引对应的自由变量替换为项 ── */

typedef struct LambdaSubstitution {
    int index;                         /**< De Bruijn 索引 */
    struct LvLambdaTerm *replacement; /**< 替换项 */
    struct LambdaSubstitution *next;   /**< 链表下一节点 */
} LambdaSubstitution;

/* ================================================================
 * 句法合一 API
 * ================================================================ */

/**
 * @brief 对两个 λ-项执行句法合一
 *
 * 基于 Martelli-Montanari 风格的一阶合一算法，处理
 * LV_LAMBDA_VAR、LV_LAMBDA_ABS、LV_LAMBDA_APP 三种节点类型。
 *
 * @param t1, t2     待合一的两个 λ-项
 * @param out_subs   输出：合一替换链表（调用者通过 lambda_substitution_list_destroy 释放）
 * @param max_depth  最大递归深度（建议 1024，防止无限递归）
 * @return LambdaUnifyStatus
 */
lv_PUBLIC_API LambdaUnifyStatus lambda_unify(LvLambdaTerm *t1, LvLambdaTerm *t2,
                                              LambdaSubstitution **out_subs, int max_depth);

/**
 * @brief 将替换应用于 λ-项
 *
 * 遍历替换链表，将 term 中匹配的 De Bruijn 索引变量替换为对应的 replacement。
 * 返回新分配的 λ-项（调用者负责通过 lv_lambda_destroy 释放）。
 *
 * @param term  原始 λ-项
 * @param subs  替换链表
 * @return LvLambdaTerm*  应用替换后的新项
 */
lv_PUBLIC_API LvLambdaTerm *lambda_unify_apply(LvLambdaTerm *term, LambdaSubstitution *subs);

/**
 * @brief 销毁替换链表
 *
 * 递归释放所有 replacement 项和节点自身。
 *
 * @param subs 替换链表头（可为 NULL）
 */
lv_PUBLIC_API void lambda_substitution_list_destroy(LambdaSubstitution *subs);

/**
 * @brief 将合一替换输出为可读字符串（调试用）
 *
 * @param subs 替换链表
 * @param buf  输出缓冲区
 * @param size 缓冲区大小
 */
lv_PUBLIC_API void lambda_substitution_snprint(LambdaSubstitution *subs, char *buf, size_t size);

/**
 * @brief 将合一替换应用于约束图
 *
 * 遍历替换链表，对每个替换的 λ-项：
 * 1. 通过 lambda_to_graph 编译为约束图子图
 * 2. 将编译后的节点和约束合并到目标图
 * 3. 将图中匹配 De Bruijn 索引的 PORT 节点重连到替换子图的输出
 *
 * @param graph  目标约束图（会被修改）
 * @param subs   合一替换链表
 * @param binder_depth  当前 binder 深度（通常为 0）
 * @return int  成功返回 0，失败返回 -1
 */
lv_PUBLIC_API int lambda_unify_apply_to_graph(struct ConstraintGraph *graph,
                                               LambdaSubstitution *subs,
                                               int binder_depth);

/* ================================================================
 * 模式合一 API（Miller 可判定高阶合一子集）
 * ================================================================ */

/**
 * @brief 检查 λ-项是否符合模式合一条件
 *
 * 模式条件：所有高阶变量（函数位置的自由变量）的参数必须是不同的 bound 变量。
 * 例如：F x y 是模式形式，F (f x) 不是。
 *
 * @param term 待检查的 λ-项
 * @return true 如果是合法模式形式
 */
lv_PUBLIC_API bool lambda_is_pattern(LvLambdaTerm *term);

/**
 * @brief 对两个 λ-项执行 Miller 模式合一
 *
 * 限制：合一变量必须是模式形式。这是高阶合一的可判定实用子集。
 * 通过 Imitation（模仿目标结构）和 Projection（投影到参数）规则求解。
 *
 * @param t1, t2     待合一的 λ-项
 * @param out_subs   输出替换链表
 * @param max_depth  最大递归深度
 * @return LambdaUnifyStatus
 */
lv_PUBLIC_API LambdaUnifyStatus lambda_pattern_unify(LvLambdaTerm *t1, LvLambdaTerm *t2,
                                                      LambdaSubstitution **out_subs, int max_depth);

#ifdef __cplusplus
}
#endif

#endif /* lv_LAMBDA_UNIFY_H */
