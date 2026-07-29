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

    /* λu.x — x is at depth 2: scope=[n(3),f(2),x(1),u(0)] */
    LvLambdaTerm *const_x = lv_lambda_create_abs(0, lv_lambda_create_var(2));

    /* λu.u */
    LvLambdaTerm *const_u = lv_lambda_create_abs(0, lv_lambda_create_var(0));

    /* n pair_fn const_x const_u */
    LvLambdaTerm *body = lv_lambda_create_app(
        lv_lambda_create_app(lv_lambda_create_app(lv_lambda_create_var(0), pair_fn), const_x), const_u);

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
 *  不动点组合子
 * ================================================================ */

LvLambdaTerm *lv_church_y_combinator(void) {
    LvLambdaTerm *inner = lv_lambda_create_abs(
        0, lv_lambda_create_app(lv_lambda_create_var(1),
                                lv_lambda_create_app(lv_lambda_create_var(0), lv_lambda_create_var(0))));
    LvLambdaTerm *body = lv_lambda_create_app(lv_lambda_copy(inner), inner);
    return lv_lambda_create_abs(0, body);
}
