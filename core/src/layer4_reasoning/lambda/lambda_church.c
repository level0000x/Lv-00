/**
 * @file lambda_church.c
 * @brief Church 编码公共 API 实现
 *
 * 提供 Church 数字/布尔值及其运算的 λ-项构造器。
 * 所有函数使用 De Bruijn 索引表示变量绑定。
 */

#include "lv/lambda_church.h"

/* ================================================================
 *  Church 数字
 * ================================================================ */

LvLambdaTerm *lv_church_0(void) {
    return lv_lambda_create_abs(0, lv_lambda_create_abs(0, lv_lambda_create_var(0)));
}

LvLambdaTerm *lv_church_1(void) {
    return lv_lambda_create_abs(
        0, lv_lambda_create_abs(0, lv_lambda_create_app(lv_lambda_create_var(1), lv_lambda_create_var(0))));
}

LvLambdaTerm *lv_church_2(void) {
    return lv_lambda_create_abs(
        0, lv_lambda_create_abs(
               0, lv_lambda_create_app(lv_lambda_create_var(1),
                                       lv_lambda_create_app(lv_lambda_create_var(1), lv_lambda_create_var(0)))));
}

LvLambdaTerm *lv_church_n(int n) {
    LvLambdaTerm *body;
    int i;

    if (n < 0)
        return NULL;

    body = lv_lambda_create_var(0);
    for (i = 0; i < n; i++) {
        body = lv_lambda_create_app(lv_lambda_create_var(1), body);
    }
    return lv_lambda_create_abs(0, lv_lambda_create_abs(0, body));
}

LvLambdaTerm *lv_church_succ(void) {
    LvLambdaTerm *body = lv_lambda_create_app(
        lv_lambda_create_var(1),
        lv_lambda_create_app(lv_lambda_create_app(lv_lambda_create_var(2), lv_lambda_create_var(1)),
                             lv_lambda_create_var(0)));
    return lv_lambda_create_abs(0, lv_lambda_create_abs(0, lv_lambda_create_abs(0, body)));
}

LvLambdaTerm *lv_church_mul(void) {
    LvLambdaTerm *body = lv_lambda_create_app(lv_lambda_create_var(2),
                                              lv_lambda_create_app(lv_lambda_create_var(1), lv_lambda_create_var(0)));
    return lv_lambda_create_abs(0, lv_lambda_create_abs(0, lv_lambda_create_abs(0, body)));
}

LvLambdaTerm *lv_church_pow(void) {
    LvLambdaTerm *body = lv_lambda_create_app(lv_lambda_create_var(0), lv_lambda_create_var(1));
    return lv_lambda_create_abs(0, lv_lambda_create_abs(0, body));
}

LvLambdaTerm *lv_church_pred(void) {
    /* λg.λh.h (g f) — inside λn.λf.λx: f=Var(3) from λh */
    LvLambdaTerm *inner1 = lv_lambda_create_abs(
        0, lv_lambda_create_app(lv_lambda_create_var(0),
                                lv_lambda_create_app(lv_lambda_create_var(1), lv_lambda_create_var(3))));
    LvLambdaTerm *pair_fn = lv_lambda_create_abs(0, inner1);

    /* λu.x — x is at depth 1（scope=[n(3),f(2),x(1),u(0)]，De Bruijn 相对索引） */
    LvLambdaTerm *const_x = lv_lambda_create_abs(0, lv_lambda_create_var(1));

    /* λu.u */
    LvLambdaTerm *const_u = lv_lambda_create_abs(0, lv_lambda_create_var(0));

    /* n pair_fn const_x const_u — n 在 λn.λf.λx 内索引 2（x=0, f=1, n=2） */
    LvLambdaTerm *body = lv_lambda_create_app(
        lv_lambda_create_app(lv_lambda_create_app(lv_lambda_create_var(2), pair_fn), const_x), const_u);

    return lv_lambda_create_abs(0, lv_lambda_create_abs(0, lv_lambda_create_abs(0, body)));
}

/* ================================================================
 *  Church 布尔值
 * ================================================================ */

LvLambdaTerm *lv_church_true(void) {
    return lv_lambda_create_abs(0, lv_lambda_create_abs(0, lv_lambda_create_var(1)));
}

LvLambdaTerm *lv_church_false(void) {
    return lv_lambda_create_abs(0, lv_lambda_create_abs(0, lv_lambda_create_var(0)));
}

LvLambdaTerm *lv_church_if(void) {
    return lv_lambda_create_abs(
        0, lv_lambda_create_abs(
               0, lv_lambda_create_abs(
                      0, lv_lambda_create_app(lv_lambda_create_app(lv_lambda_create_var(2), lv_lambda_create_var(1)),
                                              lv_lambda_create_var(0)))));
}

LvLambdaTerm *lv_church_iszero(void) {
    LvLambdaTerm *false_term = lv_church_false();
    LvLambdaTerm *true_term = lv_church_true();
    LvLambdaTerm *body = lv_lambda_create_app(
        lv_lambda_create_app(lv_lambda_create_var(0), lv_lambda_create_abs(0, false_term)), true_term);
    return lv_lambda_create_abs(0, body);
}

/* ================================================================
 *  Church 算术运算
 * ================================================================ */

/**
 * λm.λn.λf.λx.m f (n f x)
 * De Bruijn: m=3, n=2, f=1, x=0
 */
LvLambdaTerm *lv_church_add(void) {
    /* m f (n f x) */
    LvLambdaTerm *body = lv_lambda_create_app(
        lv_lambda_create_app(lv_lambda_create_var(3), lv_lambda_create_var(1)),
        lv_lambda_create_app(lv_lambda_create_app(lv_lambda_create_var(2), lv_lambda_create_var(1)),
                             lv_lambda_create_var(0)));
    return lv_lambda_create_abs(0, lv_lambda_create_abs(0, lv_lambda_create_abs(0, lv_lambda_create_abs(0, body))));
}

/**
 * λm.λn.n pred m
 * De Bruijn: m=1, n=0
 */
LvLambdaTerm *lv_church_sub(void) {
    LvLambdaTerm *pred_term = lv_church_pred();
    /* n pred m */
    LvLambdaTerm *body = lv_lambda_create_app(
        lv_lambda_create_app(lv_lambda_create_var(0), pred_term),
        lv_lambda_create_var(1));
    return lv_lambda_create_abs(0, lv_lambda_create_abs(0, body));
}

/* ================================================================
 *  Church 对（pair）
 * ================================================================ */

/**
 * λx.λy.λf.f x y
 * De Bruijn: x=2, y=1, f=0
 */
LvLambdaTerm *lv_church_pair(void) {
    LvLambdaTerm *body = lv_lambda_create_app(
        lv_lambda_create_app(lv_lambda_create_var(0), lv_lambda_create_var(2)),
        lv_lambda_create_var(1));
    return lv_lambda_create_abs(0, lv_lambda_create_abs(0, lv_lambda_create_abs(0, body)));
}

/**
 * λp.p true
 * De Bruijn: p=0
 */
LvLambdaTerm *lv_church_first(void) {
    LvLambdaTerm *true_term = lv_church_true();
    LvLambdaTerm *body = lv_lambda_create_app(lv_lambda_create_var(0), true_term);
    return lv_lambda_create_abs(0, body);
}

/**
 * λp.p false
 * De Bruijn: p=0
 */
LvLambdaTerm *lv_church_second(void) {
    LvLambdaTerm *false_term = lv_church_false();
    LvLambdaTerm *body = lv_lambda_create_app(lv_lambda_create_var(0), false_term);
    return lv_lambda_create_abs(0, body);
}

/* ================================================================
 *  Church 列表
 * ================================================================ */

/**
 * 空列表 (nil): λc.λn.n
 * De Bruijn: c=1, n=0
 */
LvLambdaTerm *lv_church_nil(void) {
    return lv_lambda_create_abs(0, lv_lambda_create_abs(0, lv_lambda_create_var(0)));
}

/**
 * cons: λh.λt.λc.λn.c h (t c n)
 * De Bruijn: h=3, t=2, c=1, n=0
 */
LvLambdaTerm *lv_church_cons(void) {
    /* c h */
    LvLambdaTerm *ch = lv_lambda_create_app(lv_lambda_create_var(1), lv_lambda_create_var(3));
    /* t c n */
    LvLambdaTerm *tcn = lv_lambda_create_app(
        lv_lambda_create_app(lv_lambda_create_var(2), lv_lambda_create_var(1)),
        lv_lambda_create_var(0));
    /* c h (t c n) */
    LvLambdaTerm *body = lv_lambda_create_app(ch, tcn);
    return lv_lambda_create_abs(0, lv_lambda_create_abs(0, lv_lambda_create_abs(0, lv_lambda_create_abs(0, body))));
}

/* ================================================================
 *  Church 布尔运算
 * ================================================================ */

/**
 * not: λp.λa.λb.p b a
 * De Bruijn: p=2, a=1, b=0
 */
LvLambdaTerm *lv_church_not(void) {
    LvLambdaTerm *body = lv_lambda_create_app(
        lv_lambda_create_app(lv_lambda_create_var(2), lv_lambda_create_var(0)),
        lv_lambda_create_var(1));
    return lv_lambda_create_abs(0, lv_lambda_create_abs(0, lv_lambda_create_abs(0, body)));
}

/**
 * and: λp.λq.p q p
 * De Bruijn: p=1, q=0
 */
LvLambdaTerm *lv_church_and(void) {
    LvLambdaTerm *body = lv_lambda_create_app(
        lv_lambda_create_app(lv_lambda_create_var(1), lv_lambda_create_var(0)),
        lv_lambda_create_var(1));
    return lv_lambda_create_abs(0, lv_lambda_create_abs(0, body));
}

/**
 * or: λp.λq.p p q
 * De Bruijn: p=1, q=0
 */
LvLambdaTerm *lv_church_or(void) {
    LvLambdaTerm *body = lv_lambda_create_app(
        lv_lambda_create_app(lv_lambda_create_var(1), lv_lambda_create_var(1)),
        lv_lambda_create_var(0));
    return lv_lambda_create_abs(0, lv_lambda_create_abs(0, body));
}

/**
 * xor: λp.λq.p (not q) q
 * De Bruijn: p=1, q=0
 * 使用 lv_church_not() 构造 not q，然后应用 p。
 */
LvLambdaTerm *lv_church_xor(void) {
    LvLambdaTerm *not_term = lv_church_not();
    LvLambdaTerm *not_q = lv_lambda_create_app(not_term, lv_lambda_create_var(0));
    LvLambdaTerm *body = lv_lambda_create_app(
        lv_lambda_create_app(lv_lambda_create_var(1), not_q),
        lv_lambda_create_var(0));
    return lv_lambda_create_abs(0, lv_lambda_create_abs(0, body));
}

/* ================================================================
 *  Church 列表操作
 * ================================================================ */

/**
 * isnil: λl.l (λh.λt.λx.false) true
 * De Bruijn: l=0, h=2, t=1, x=0
 * 应用列表到 (\h.\t.\x.false) 和 true：
 *   - nil 返回 true（选择了第二个参数）
 *   - cons h t 返回 (\h.\t.\x.false) h t = \x.false，应用任意参数得 false
 */
LvLambdaTerm *lv_church_isnil(void) {
    LvLambdaTerm *false_term = lv_church_false();
    LvLambdaTerm *true_term = lv_church_true();
    /* λh.λt.λx.false */
    LvLambdaTerm *step = lv_lambda_create_abs(
        0, lv_lambda_create_abs(0, lv_lambda_create_abs(0, false_term)));
    /* l step true */
    LvLambdaTerm *body = lv_lambda_create_app(
        lv_lambda_create_app(lv_lambda_create_var(0), step), true_term);
    return lv_lambda_create_abs(0, body);
}

/**
 * head: λl.l (λh.λt.h) (error)
 * De Bruijn: l=0, h=1, t=0
 * 应用列表到 (\h.\t.h) 和 error 项：
 *   - nil 返回 error（空列表无头部）
 *   - cons h t 返回 λh.λt.h 应用到 h 和 t，结果为 h
 */
LvLambdaTerm *lv_church_head(void) {
    /* λh.λt.h */
    LvLambdaTerm *step = lv_lambda_create_abs(
        0, lv_lambda_create_abs(0, lv_lambda_create_var(1)));
    /* 使用 id 函数作为 error 占位 */
    LvLambdaTerm *error_term = lv_lambda_create_abs(0, lv_lambda_create_var(0));
    /* l step error */
    LvLambdaTerm *body = lv_lambda_create_app(
        lv_lambda_create_app(lv_lambda_create_var(0), step), error_term);
    return lv_lambda_create_abs(0, body);
}

/**
 * tail: λl.l (λh.λt.t) (error)
 * De Bruijn: l=0, h=1, t=0
 * 应用列表到 (\h.\t.t) 和 error 项：
 *   - nil 返回 error（空列表无尾部）
 *   - cons h t 返回 (\h.\t.t) h t = t
 */
LvLambdaTerm *lv_church_tail(void) {
    /* λh.λt.t */
    LvLambdaTerm *step = lv_lambda_create_abs(
        0, lv_lambda_create_abs(0, lv_lambda_create_var(0)));
    /* 使用 id 函数作为 error 占位（与 head 一致） */
    LvLambdaTerm *error_term = lv_lambda_create_abs(0, lv_lambda_create_var(0));
    /* l step error */
    LvLambdaTerm *body = lv_lambda_create_app(
        lv_lambda_create_app(lv_lambda_create_var(0), step), error_term);
    return lv_lambda_create_abs(0, body);
}

/**
 * map: λf.λl.l (λh.λt.cons (f h) t) nil
 * De Bruijn: f=1, l=0；step(λh.λt) 内 f=3, h=1, t=0
 * 通过 Church 列表折叠语义实现：
 *   map f nil = nil step nil = nil
 *   map f (cons h t) = step h (t step nil) = cons (f h) (map f t)
 */
LvLambdaTerm *lv_church_map(void) {
    LvLambdaTerm *cons_term = lv_church_cons();
    LvLambdaTerm *nil_term = lv_church_nil();
    /* f h */
    LvLambdaTerm *fh = lv_lambda_create_app(lv_lambda_create_var(3), lv_lambda_create_var(1));
    /* cons (f h) */
    LvLambdaTerm *cons_fh = lv_lambda_create_app(cons_term, fh);
    /* cons (f h) t */
    LvLambdaTerm *cons_fh_t = lv_lambda_create_app(cons_fh, lv_lambda_create_var(0));
    /* λh.λt.cons (f h) t */
    LvLambdaTerm *step = lv_lambda_create_abs(0, lv_lambda_create_abs(0, cons_fh_t));
    /* l step nil */
    LvLambdaTerm *body = lv_lambda_create_app(
        lv_lambda_create_app(lv_lambda_create_var(0), step), nil_term);
    return lv_lambda_create_abs(0, lv_lambda_create_abs(0, body));
}

/**
 * filter: λp.λl.l (λh.λt.if (p h) (cons h t) t) nil
 * De Bruijn: p=1, l=0；step(λh.λt) 内 p=3, h=1, t=0
 * 通过 Church 列表折叠语义实现：
 *   filter p nil = nil
 *   filter p (cons h t) = if (p h) (cons h (filter p t)) (filter p t)
 */
LvLambdaTerm *lv_church_filter(void) {
    LvLambdaTerm *if_term = lv_church_if();
    LvLambdaTerm *cons_term = lv_church_cons();
    LvLambdaTerm *nil_term = lv_church_nil();
    /* p h */
    LvLambdaTerm *ph = lv_lambda_create_app(lv_lambda_create_var(3), lv_lambda_create_var(1));
    /* cons h */
    LvLambdaTerm *cons_h = lv_lambda_create_app(cons_term, lv_lambda_create_var(1));
    /* cons h t */
    LvLambdaTerm *cons_h_t = lv_lambda_create_app(cons_h, lv_lambda_create_var(0));
    /* if (p h) (cons h t) t */
    LvLambdaTerm *branch = lv_lambda_create_app(
        lv_lambda_create_app(lv_lambda_create_app(if_term, ph), cons_h_t),
        lv_lambda_create_var(0));
    /* λh.λt.if (p h) (cons h t) t */
    LvLambdaTerm *step = lv_lambda_create_abs(0, lv_lambda_create_abs(0, branch));
    /* l step nil */
    LvLambdaTerm *body = lv_lambda_create_app(
        lv_lambda_create_app(lv_lambda_create_var(0), step), nil_term);
    return lv_lambda_create_abs(0, lv_lambda_create_abs(0, body));
}

/**
 * foldr: λf.λz.λl.l f z
 * De Bruijn: f=2, z=1, l=0
 * 利用 Church 列表编码自身实现折叠：
 *   foldr f z nil = nil f z = z
 *   foldr f z (cons h t) = cons f z = f h (t f z) = f h (foldr f z t)
 */
LvLambdaTerm *lv_church_foldr(void) {
    /* l f z */
    LvLambdaTerm *body = lv_lambda_create_app(
        lv_lambda_create_app(lv_lambda_create_var(0), lv_lambda_create_var(2)),
        lv_lambda_create_var(1));
    return lv_lambda_create_abs(0, lv_lambda_create_abs(0, lv_lambda_create_abs(0, body)));
}

/**
 * foldl: λf.λz.λl.l (λx.λg.λa.g (f a x)) (λa.a) z
 * De Bruijn: f=2, z=1, l=0；step(λx.λg.λa) 内 f=5, x=2, g=1, a=0
 * 累积器传递技巧：
 *   foldl f z nil = nil step id z = z
 *   foldl f z (cons h t) = step h (t step id) z = (t step id) (f z h) = foldl f (f z h) t
 */
LvLambdaTerm *lv_church_foldl(void) {
    /* f a x = App(App(f, a), x) */
    LvLambdaTerm *f_a_x = lv_lambda_create_app(
        lv_lambda_create_app(lv_lambda_create_var(5), lv_lambda_create_var(0)),
        lv_lambda_create_var(2));
    /* g (f a x) */
    LvLambdaTerm *g_fax = lv_lambda_create_app(lv_lambda_create_var(1), f_a_x);
    /* λx.λg.λa.g (f a x) */
    LvLambdaTerm *step = lv_lambda_create_abs(0, lv_lambda_create_abs(0, lv_lambda_create_abs(0, g_fax)));
    /* id = λa.a */
    LvLambdaTerm *id_term = lv_lambda_create_abs(0, lv_lambda_create_var(0));
    /* l step id z */
    LvLambdaTerm *body = lv_lambda_create_app(
        lv_lambda_create_app(lv_lambda_create_app(lv_lambda_create_var(0), step), id_term),
        lv_lambda_create_var(1));
    return lv_lambda_create_abs(0, lv_lambda_create_abs(0, lv_lambda_create_abs(0, body)));
}

/**
 * length: λl.l (λh.λt.succ t) zero
 * De Bruijn: l=0, h=1, t=0
 * 从右向左遍历，每一步将 succ 应用于累加器：
 *   length nil = nil succ zero = zero
 *   length (cons h t) = cons succ zero = succ (t succ zero) = succ (length t)
 */
LvLambdaTerm *lv_church_length(void) {
    LvLambdaTerm *zero_term = lv_church_0();
    LvLambdaTerm *succ_term = lv_church_succ();
    /* λh.λt.succ t：忽略元素 h，对累加器 t 应用 succ */
    LvLambdaTerm *step = lv_lambda_create_abs(
        0, lv_lambda_create_abs(0, lv_lambda_create_app(succ_term, lv_lambda_create_var(0))));
    /* l step zero */
    LvLambdaTerm *body = lv_lambda_create_app(
        lv_lambda_create_app(lv_lambda_create_var(0), step), zero_term);
    return lv_lambda_create_abs(0, body);
}

/**
 * append: λl1.λl2.l1 cons l2
 * De Bruijn: l1=1, l2=0
 * 在 l1 后拼接 l2：将 cons 作为折叠函数应用于 l1，以 l2 为初始累加器。
 *   append nil l2 = nil cons l2 = l2
 *   append (cons h t) l2 = cons h (t cons l2) = cons h (append t l2)
 */
LvLambdaTerm *lv_church_append(void) {
    LvLambdaTerm *cons_term = lv_church_cons();
    /* l1 cons l2 */
    LvLambdaTerm *body = lv_lambda_create_app(
        lv_lambda_create_app(lv_lambda_create_var(1), cons_term),
        lv_lambda_create_var(0));
    return lv_lambda_create_abs(0, lv_lambda_create_abs(0, body));
}

/* ================================================================
 *  Church 比较运算
 * ================================================================ */

/**
 * leq: λm.λn.iszero (sub m n)
 * De Bruijn: m=1, n=0
 * 若 m ≤ n 则 sub m n = 0（或 0 裁剪），iszero 返回 true。
 */
LvLambdaTerm *lv_church_leq(void) {
    LvLambdaTerm *sub_term = lv_church_sub();
    LvLambdaTerm *iszero_term = lv_church_iszero();
    /* sub m n */
    LvLambdaTerm *sub_mn = lv_lambda_create_app(
        lv_lambda_create_app(sub_term, lv_lambda_create_var(1)),
        lv_lambda_create_var(0));
    /* iszero (sub m n) */
    LvLambdaTerm *body = lv_lambda_create_app(iszero_term, sub_mn);
    return lv_lambda_create_abs(0, lv_lambda_create_abs(0, body));
}

/**
 * eq: λm.λn.and (iszero (sub m n)) (iszero (sub n m))
 * De Bruijn: m=1, n=0
 * 双向减法均为零则两数相等。
 */
LvLambdaTerm *lv_church_eq(void) {
    LvLambdaTerm *sub_term = lv_church_sub();
    LvLambdaTerm *iszero_term = lv_church_iszero();
    LvLambdaTerm *and_term = lv_church_and();
    /* iszero (sub m n) — 第一次使用，sub_term/iszero_term 所有权转移给子树 */
    LvLambdaTerm *sub_mn = lv_lambda_create_app(
        lv_lambda_create_app(sub_term, lv_lambda_create_var(1)),
        lv_lambda_create_var(0));
    LvLambdaTerm *iszero_mn = lv_lambda_create_app(iszero_term, sub_mn);
    /* iszero (sub n m) — 第二次使用，必须复制 term 避免 double-free */
    LvLambdaTerm *sub_nm = lv_lambda_create_app(
        lv_lambda_create_app(lv_lambda_copy(lv_church_sub()), lv_lambda_create_var(0)),
        lv_lambda_create_var(1));
    LvLambdaTerm *iszero_nm = lv_lambda_create_app(lv_lambda_copy(lv_church_iszero()), sub_nm);
    /* and */
    LvLambdaTerm *body = lv_lambda_create_app(
        lv_lambda_create_app(and_term, iszero_mn), iszero_nm);
    return lv_lambda_create_abs(0, lv_lambda_create_abs(0, body));
}

/**
 * gt: λm.λn.not (leq m n)
 * De Bruijn: m=1, n=0
 * m > n 当且仅当 ¬(m ≤ n)。
 */
LvLambdaTerm *lv_church_gt(void) {
    LvLambdaTerm *leq_term = lv_church_leq();
    LvLambdaTerm *not_term = lv_church_not();
    /* leq m n */
    LvLambdaTerm *leq_mn = lv_lambda_create_app(
        lv_lambda_create_app(leq_term, lv_lambda_create_var(1)),
        lv_lambda_create_var(0));
    /* not (leq m n) */
    LvLambdaTerm *body = lv_lambda_create_app(not_term, leq_mn);
    return lv_lambda_create_abs(0, lv_lambda_create_abs(0, body));
}

/* ================================================================
 *  不动点组合子
 * ================================================================ */

LvLambdaTerm *lv_church_y_combinator(void) {
    LvLambdaTerm *inner = lv_lambda_create_abs(
        0, lv_lambda_create_app(lv_lambda_create_var(1),
                                lv_lambda_create_app(lv_lambda_create_var(0), lv_lambda_create_var(0))));
    LvLambdaTerm *body = lv_lambda_create_app(lv_lambda_copy(inner), inner);
    return lv_lambda_create_abs(0, body);
}

/* ================================================================
 *  扩展 Church 运算
 * ================================================================ */

/**
 * div = Y (λf.λm.λn.if (leq n m) (succ (f (sub m n) n)) 0)
 * De Bruijn: f=2, m=1, n=0
 */
LvLambdaTerm *lv_church_div(void) {
    LvLambdaTerm *y = lv_church_y_combinator();

    /* n=0: sub m n */
    LvLambdaTerm *sub_mn = lv_lambda_create_app(
        lv_lambda_create_app(lv_church_sub(), lv_lambda_create_var(1)),
        lv_lambda_create_var(0));

    /* f (sub m n) n = APP(APP(2, sub_mn), 0) */
    LvLambdaTerm *rec_call = lv_lambda_create_app(
        lv_lambda_create_app(lv_lambda_create_var(2), sub_mn),
        lv_lambda_create_var(0));

    /* succ (rec_call) */
    LvLambdaTerm *then_branch = lv_lambda_create_app(lv_church_succ(), rec_call);

    /* leq n m = APP(APP(leq, 0), 1) */
    LvLambdaTerm *leq_nm = lv_lambda_create_app(
        lv_lambda_create_app(lv_church_leq(), lv_lambda_create_var(0)),
        lv_lambda_create_var(1));

    /* if (leq n m) then_branch 0 */
    LvLambdaTerm *body = lv_lambda_create_app(
        lv_lambda_create_app(lv_lambda_create_app(lv_church_if(), leq_nm), then_branch),
        lv_church_0());

    LvLambdaTerm *core = lv_lambda_create_abs(0, lv_lambda_create_abs(0, lv_lambda_create_abs(0, body)));
    return lv_lambda_create_app(y, core);
}

/**
 * fact = Y (λf.λn.if (leq n 1) 1 (mul n (f (pred n))))
 * De Bruijn: f=1, n=0
 */
LvLambdaTerm *lv_church_factorial(void) {
    LvLambdaTerm *y = lv_church_y_combinator();

    /* leq n 1 */
    LvLambdaTerm *leq_n1 = lv_lambda_create_app(
        lv_lambda_create_app(lv_church_leq(), lv_lambda_create_var(0)),
        lv_church_1());

    /* pred n */
    LvLambdaTerm *pred_n = lv_lambda_create_app(lv_church_pred(), lv_lambda_create_var(0));

    /* f (pred n) */
    LvLambdaTerm *f_pred = lv_lambda_create_app(lv_lambda_create_var(1), pred_n);

    /* mul n (f (pred n)) */
    LvLambdaTerm *mul_n = lv_lambda_create_app(
        lv_lambda_create_app(lv_church_mul(), lv_lambda_create_var(0)), f_pred);

    /* if (leq n 1) 1 (mul n (f (pred n))) */
    LvLambdaTerm *body = lv_lambda_create_app(
        lv_lambda_create_app(lv_lambda_create_app(lv_church_if(), leq_n1), lv_church_1()),
        mul_n);

    LvLambdaTerm *core = lv_lambda_create_abs(0, lv_lambda_create_abs(0, body));
    return lv_lambda_create_app(y, core);
}

/**
 * fib = Y (λf.λn.if (leq n 1) n (add (f (sub n 1)) (f (sub n 2))))
 * De Bruijn: f=1, n=0
 */
LvLambdaTerm *lv_church_fib(void) {
    LvLambdaTerm *y = lv_church_y_combinator();

    /* leq n 1 */
    LvLambdaTerm *leq_n1 = lv_lambda_create_app(
        lv_lambda_create_app(lv_church_leq(), lv_lambda_create_var(0)),
        lv_church_1());

    /* sub n 1 => APP(APP(sub, 0), 1) */
    LvLambdaTerm *sub_n1 = lv_lambda_create_app(
        lv_lambda_create_app(lv_church_sub(), lv_lambda_create_var(0)),
        lv_church_1());

    /* f (sub n 1) */
    LvLambdaTerm *f_sub_n1 = lv_lambda_create_app(lv_lambda_create_var(1), sub_n1);

    /* sub n 2 => APP(APP(sub, 0), 2) */
    LvLambdaTerm *sub_n2 = lv_lambda_create_app(
        lv_lambda_create_app(lv_church_sub(), lv_lambda_create_var(0)),
        lv_church_2());

    /* f (sub n 2) */
    LvLambdaTerm *f_sub_n2 = lv_lambda_create_app(lv_lambda_create_var(1), sub_n2);

    /* add (f (sub n 1)) (f (sub n 2)) */
    LvLambdaTerm *rec_call = lv_lambda_create_app(
        lv_lambda_create_app(lv_church_add(), f_sub_n1), f_sub_n2);

    /* if (leq n 1) n rec_call */
    LvLambdaTerm *body = lv_lambda_create_app(
        lv_lambda_create_app(lv_lambda_create_app(lv_church_if(), leq_n1),
                             lv_lambda_create_var(0)),
        rec_call);

    LvLambdaTerm *core = lv_lambda_create_abs(0, lv_lambda_create_abs(0, body));
    return lv_lambda_create_app(y, core);
}
