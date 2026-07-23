/**
 * @file lambda_term.c
 * @brief λ-项数据结构的实现
 *
 * 提供 LvLambdaTerm 类型的创建、销毁、拷贝和字符串化功能。
 * 使用 tagged union 表示变量（Var）、抽象（Abs）和应用（App）。
 */

#include "lv/lambda_term.h"
#include "lv_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ===========================================================================
 * 创建函数
 * =========================================================================== */

LvLambdaTerm *lv_lambda_create_var(int index) {
    LvLambdaTerm *term = lv_calloc(1, sizeof(LvLambdaTerm));
    if (!term) return NULL;
    term->type = LV_LAMBDA_VAR;
    term->data.var.index = index;
    return term;
}

LvLambdaTerm *lv_lambda_create_abs(int binder, LvLambdaTerm *body) {
    if (!body) return NULL;
    LvLambdaTerm *term = lv_calloc(1, sizeof(LvLambdaTerm));
    if (!term) {
        lv_lambda_destroy(body);
        return NULL;
    }
    term->type = LV_LAMBDA_ABS;
    term->data.abs.binder = binder;
    term->data.abs.body = body;
    return term;
}

LvLambdaTerm *lv_lambda_create_app(LvLambdaTerm *left, LvLambdaTerm *right) {
    if (!left || !right) {
        if (left)  lv_lambda_destroy(left);
        if (right) lv_lambda_destroy(right);
        return NULL;
    }
    LvLambdaTerm *term = lv_calloc(1, sizeof(LvLambdaTerm));
    if (!term) {
        lv_lambda_destroy(left);
        lv_lambda_destroy(right);
        return NULL;
    }
    term->type = LV_LAMBDA_APP;
    term->data.app.left = left;
    term->data.app.right = right;
    return term;
}

/* ===========================================================================
 * 销毁函数
 * =========================================================================== */

void lv_lambda_destroy(LvLambdaTerm *term) {
    if (!term) return;

    switch (term->type) {
    case LV_LAMBDA_VAR:
        /* Var: 没有子项需要递归销毁 */
        break;

    case LV_LAMBDA_ABS:
        lv_lambda_destroy(term->data.abs.body);
        break;

    case LV_LAMBDA_APP:
        lv_lambda_destroy(term->data.app.left);
        lv_lambda_destroy(term->data.app.right);
        break;
    }

    lv_free((void**)&term);
}

/* ===========================================================================
 * 拷贝函数
 * =========================================================================== */

LvLambdaTerm *lv_lambda_copy(LvLambdaTerm *term) {
    if (!term) return NULL;

    switch (term->type) {
    case LV_LAMBDA_VAR:
        return lv_lambda_create_var(term->data.var.index);

    case LV_LAMBDA_ABS: {
        LvLambdaTerm *body_copy = lv_lambda_copy(term->data.abs.body);
        if (!body_copy) return NULL;
        return lv_lambda_create_abs(term->data.abs.binder, body_copy);
    }

    case LV_LAMBDA_APP: {
        LvLambdaTerm *left_copy = lv_lambda_copy(term->data.app.left);
        LvLambdaTerm *right_copy = lv_lambda_copy(term->data.app.right);
        if (!left_copy || !right_copy) {
            lv_lambda_destroy(left_copy);
            lv_lambda_destroy(right_copy);
            return NULL;
        }
        return lv_lambda_create_app(left_copy, right_copy);
    }
    }

    return NULL;
}

/* ===========================================================================
 * 字符串化
 * =========================================================================== */

/**
 * @brief 递归构建 λ-项的字符串表示
 *
 * 格式：
 *   - Var:  "#<index>"
 *   - Abs:  "λ.<body>"
 *   - App:  "(<left> <right>)"
 *
 * @param term     λ-项
 * @param out_len  输出：结果字符串长度
 * @return 新分配的字符串（调用者通过 lv_free 释放），失败返回 NULL
 */
static char *lambda_to_string_internal(const LvLambdaTerm *term, size_t *out_len) {
    if (!term) {
        if (out_len) *out_len = 0;
        return NULL;
    }

    char buf[256];
    char *result = NULL;
    size_t len = 0;

    switch (term->type) {
    case LV_LAMBDA_VAR: {
        int n = snprintf(buf, sizeof(buf), "#%d", term->data.var.index);
        if (n < 0 || (size_t)n >= sizeof(buf)) {
            /* 输出截断，动态分配 */
            int needed = snprintf(NULL, 0, "#%d", term->data.var.index);
            if (needed < 0) return NULL;
            result = lv_malloc((size_t)needed + 1);
            if (!result) return NULL;
            snprintf(result, (size_t)needed + 1, "#%d", term->data.var.index);
            len = (size_t)needed;
        } else {
            result = lv_strdup(buf);
            if (!result) return NULL;
            len = (size_t)n;
        }
        break;
    }

    case LV_LAMBDA_ABS: {
        size_t body_len = 0;
        char *body_str = lambda_to_string_internal(term->data.abs.body, &body_len);
        if (!body_str) return NULL;

        /* "λ." + body_str */
        size_t prefix_len = 2; /* "λ." */
        char *tmp = lv_malloc(prefix_len + body_len + 1);
        if (!tmp) {
            lv_free((void**)&body_str);
            return NULL;
        }
        memcpy(tmp, "λ.", prefix_len);
        memcpy(tmp + prefix_len, body_str, body_len + 1);
        lv_free((void**)&body_str);
        result = tmp;
        len = prefix_len + body_len;
        break;
    }

    case LV_LAMBDA_APP: {
        size_t left_len = 0, right_len = 0;
        char *left_str = lambda_to_string_internal(term->data.app.left, &left_len);
        char *right_str = lambda_to_string_internal(term->data.app.right, &right_len);

        if (!left_str || !right_str) {
            lv_free((void**)&left_str);
            lv_free((void**)&right_str);
            return NULL;
        }

        /* "(<left> <right>)" */
        size_t total = 1 + left_len + 1 + right_len + 1; /* '(' + left + ' ' + right + ')' */
        char *tmp = lv_malloc(total + 1);
        if (!tmp) {
            lv_free((void**)&left_str);
            lv_free((void**)&right_str);
            return NULL;
        }
        tmp[0] = '(';
        memcpy(tmp + 1, left_str, left_len);
        tmp[1 + left_len] = ' ';
        memcpy(tmp + 1 + left_len + 1, right_str, right_len);
        tmp[total - 1] = ')';
        tmp[total] = '\0';
        lv_free((void**)&left_str);
        lv_free((void**)&right_str);
        result = tmp;
        len = total;
        break;
    }
    }

    if (out_len) *out_len = len;
    return result;
}

char *lv_lambda_to_string(LvLambdaTerm *term) {
    return lambda_to_string_internal(term, NULL);
}
