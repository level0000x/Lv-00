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

/* ── Church 布尔运算 ── */

/**
 * @brief Church not (逻辑非): λp.λa.λb.p b a
 *
 * 翻转条件分支：若 p 为 true 返回 a，否则返回 b（即选择第二个参数）。
 */
lv_PUBLIC_API LvLambdaTerm *lv_church_not(void);

/**
 * @brief Church and (逻辑与): λp.λq.p q p
 *
 * 若 p 为 true 则返回 q，否则返回 false。
 */
lv_PUBLIC_API LvLambdaTerm *lv_church_and(void);

/**
 * @brief Church or (逻辑或): λp.λq.p p q
 *
 * 若 p 为 true 则返回 true，否则返回 q。
 */
lv_PUBLIC_API LvLambdaTerm *lv_church_or(void);

/**
 * @brief Church xor (逻辑异或): λp.λq.p (not q) q
 *
 * 若 p 为 true 则返回 not q，否则返回 q。
 */
lv_PUBLIC_API LvLambdaTerm *lv_church_xor(void);

/* ── Church 算术运算 ── */

/**
 * @brief Church 加法: λm.λn.λf.λx.m f (n f x)
 *
 * 对两个 Church 数字求和。
 */
lv_PUBLIC_API LvLambdaTerm *lv_church_add(void);

/**
 * @brief Church 减法: λm.λn.n pred m
 *
 * 使用前驱函数重复 n 次作用于 m 实现减法。
 * 注：当 m < n 时结果为零（Church 前驱在零上继续返回零）。
 */
lv_PUBLIC_API LvLambdaTerm *lv_church_sub(void);

/* ── Church 对（pair）── */

/**
 * @brief Church 对: λx.λy.λf.f x y
 *
 * 将两个值编码为一个有序对。
 * 实际构造时 x 和 y 作为 Church 数字/布尔值传入。
 */
lv_PUBLIC_API LvLambdaTerm *lv_church_pair(void);

/**
 * @brief 取对的第一个元素: λp.p true
 *
 * 应用 pair 到 true，true 选择第一个参数。
 */
lv_PUBLIC_API LvLambdaTerm *lv_church_first(void);

/**
 * @brief 取对的第二个元素: λp.p false
 *
 * 应用 pair 到 false，false 选择第二个参数。
 */
lv_PUBLIC_API LvLambdaTerm *lv_church_second(void);

/* ── Church 列表 ── */

/**
 * @brief Church 空列表 (nil): λc.λn.n
 *
 * 列表的编码：list = λcons.λnil.(...)
 * 空列表直接返回 nil。
 */
lv_PUBLIC_API LvLambdaTerm *lv_church_nil(void);

/**
 * @brief Church 列表构造 (cons): λh.λt.λc.λn.c h (t c n)
 *
 * 将元素 h 添加到列表 t 的头部。
 */
lv_PUBLIC_API LvLambdaTerm *lv_church_cons(void);

/**
 * @brief 检查列表是否为空: λl.l (λh.λt.λx.false) true
 *
 * 若列表为空返回 true，否则返回 false。
 */
lv_PUBLIC_API LvLambdaTerm *lv_church_isnil(void);

/**
 * @brief 取列表头部: λl.l (λh.λt.h) (error)
 *
 * 返回列表的第一个元素。空列表上调用返回 error 项。
 */
lv_PUBLIC_API LvLambdaTerm *lv_church_head(void);

/**
 * @brief Church 右折叠 (foldr): λf.λz.λl.l f z
 *
 * 从右向左折叠列表：foldr f z [x1,x2,...,xn] = f x1 (f x2 (... (f xn z)...))
 * 利用 Church 列表的编码本身实现折叠。
 */
lv_PUBLIC_API LvLambdaTerm *lv_church_foldr(void);

/**
 * @brief Church 求列表长度: λl.l (λh.λt.succ t) zero
 *
 * 计算 Church 编码列表的长度。
 */
lv_PUBLIC_API LvLambdaTerm *lv_church_length(void);

/**
 * @brief Church 列表拼接: λl1.λl2.l1 cons l2
 *
 * 将两个 Church 列表拼接为一个。
 */
lv_PUBLIC_API LvLambdaTerm *lv_church_append(void);

/* ── Church 比较运算 ── */

/**
 * @brief Church 小于等于: λm.λn.iszero (sub m n)
 *
 * 判断 m ≤ n。利用减法后检查是否为零实现。
 */
lv_PUBLIC_API LvLambdaTerm *lv_church_leq(void);

/**
 * @brief Church 相等: λm.λn.and (iszero (sub m n)) (iszero (sub n m))
 *
 * 判断两个 Church 数字相等。
 */
lv_PUBLIC_API LvLambdaTerm *lv_church_eq(void);

/**
 * @brief Church 大于: λm.λn.not (leq m n)
 *
 * 判断 m > n。
 */
lv_PUBLIC_API LvLambdaTerm *lv_church_gt(void);

/* ── 不动点组合子 ── */

/**
 * @brief Y 组合子: λf.(λx.f (x x)) (λx.f (x x))
 */
lv_PUBLIC_API LvLambdaTerm *lv_church_y_combinator(void);

#ifdef __cplusplus
}
#endif

#endif /* lv_LAMBDA_CHURCH_H */
