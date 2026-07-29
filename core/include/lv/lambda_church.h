/**
 * @file lambda_church.h
 * @brief Church 编码公共 API——λ-演算的标准数据编码
 *
 * 提供 Church 数字、Church 布尔值及其运算的 λ-项构造器。
 * 所有函数返回新分配的 LvLambdaTerm *（调用者负责销毁）。
 *
 * Church 编码参考：
 * - 数字 n: λf.λx.f^n x
 * - true:   λx.λy.x
 * - false:  λx.λy.y
 * - if:     λp.λt.λf.p t f
 */

#ifndef lv_LAMBDA_CHURCH_H
#define lv_LAMBDA_CHURCH_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lv/lambda_term.h"

/* ── Church 数字 ── */

/**
 * @brief Church 数字 0: λf.λx.x
 */
lv_PUBLIC_API LvLambdaTerm *lv_church_0(void);

/**
 * @brief Church 数字 1: λf.λx.f x
 */
lv_PUBLIC_API LvLambdaTerm *lv_church_1(void);

/**
 * @brief Church 数字 2: λf.λx.f (f x)
 */
lv_PUBLIC_API LvLambdaTerm *lv_church_2(void);

/**
 * @brief Church 数字 n: λf.λx.f^n x
 *
 * @param n 非负整数
 * @return 新分配的 Church 数字，失败返回 NULL
 */
lv_PUBLIC_API LvLambdaTerm *lv_church_n(int n);

/**
 * @brief Church 后继: λn.λf.λx.f (n f x)
 */
lv_PUBLIC_API LvLambdaTerm *lv_church_succ(void);

/**
 * @brief Church 乘法: λm.λn.λf.m (n f)
 */
lv_PUBLIC_API LvLambdaTerm *lv_church_mul(void);

/**
 * @brief Church 幂运算: λm.λn.n m
 */
lv_PUBLIC_API LvLambdaTerm *lv_church_pow(void);

/**
 * @brief Church 前驱: λn.λf.λx.n (λg.λh.h (g f)) (λu.x) (λu.u)
 */
lv_PUBLIC_API LvLambdaTerm *lv_church_pred(void);

/* ── Church 布尔值 ── */

/**
 * @brief Church true: λx.λy.x
 */
lv_PUBLIC_API LvLambdaTerm *lv_church_true(void);

/**
 * @brief Church false: λx.λy.y
 */
lv_PUBLIC_API LvLambdaTerm *lv_church_false(void);

/**
 * @brief Church if-then-else: λp.λt.λf.p t f
 */
lv_PUBLIC_API LvLambdaTerm *lv_church_if(void);

/**
 * @brief Church iszero 测试: λn.n (λx.false) true
 */
lv_PUBLIC_API LvLambdaTerm *lv_church_iszero(void);

/* ── 不动点组合子 ── */

/**
 * @brief Y 组合子: λf.(λx.f (x x)) (λx.f (x x))
 */
lv_PUBLIC_API LvLambdaTerm *lv_church_y_combinator(void);

#ifdef __cplusplus
}
#endif

#endif /* lv_LAMBDA_CHURCH_H */
