/**
 * @file lambda_term.c
 * @brief λ-项数据结构的实现
 *
 * 提供 LvLambdaTerm 类型的创建、销毁、拷贝和字符串化功能。
 * 使用 tagged union 表示变量（Var）、抽象（Abs）和应用（App）。
 */

#include "lv/lambda_term.h"
#include "lv/lv_xmacro.h"

#include "lv/lv_strbuf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv_internal.h"

/* ===========================================================================
 * 创建函数
 * =========================================================================== */

LvLambdaTerm *lv_lambda_create_var(int index) {
    LvLambdaTerm *term = lv_calloc(1, sizeof(LvLambdaTerm));
    if (!term)
        return NULL;
    term->type = LV_LAMBDA_VAR;
    term->data.var.index = index;
    return term;
}

LvLambdaTerm *lv_lambda_create_abs(int binder, LvLambdaTerm *body) {
    if (!body)
        return NULL;
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
        if (left)
            lv_lambda_destroy(left);
        if (right)
            lv_lambda_destroy(right);
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
 * 销毁函数 - 查找表
 * =========================================================================== */

typedef void (*DestroyHandler)(LvLambdaTerm *term);

static void destroy_var(LvLambdaTerm *term) {
    (void)term;
    /* Var: 没有子项需要递归销毁 */
}

static void destroy_abs(LvLambdaTerm *term) {
    lv_lambda_destroy(term->data.abs.body);
}

static void destroy_app(LvLambdaTerm *term) {
    lv_lambda_destroy(term->data.app.left);
    lv_lambda_destroy(term->data.app.right);
}

static const DestroyHandler destroy_table[LV_LAMBDA_APP + 1] = {
    [LV_LAMBDA_VAR] = destroy_var,
    [LV_LAMBDA_ABS] = destroy_abs,
    [LV_LAMBDA_APP] = destroy_app,
};

void lv_lambda_destroy(LvLambdaTerm *term) {
    if (!term)
        return;

    /* 统一分发：边界/NULL 检查由 LV_DISPATCH_VOID 完成（void 返回表） */
    LV_DISPATCH_VOID(destroy_table, term->type, term);

    lv_free((void **) &term);
}

/* ===========================================================================
 * 拷贝函数 - 查找表
 * =========================================================================== */

typedef LvLambdaTerm *(*CopyHandler)(LvLambdaTerm *term);

static LvLambdaTerm *copy_var(LvLambdaTerm *term) {
    return lv_lambda_create_var(term->data.var.index);
}

static LvLambdaTerm *copy_abs(LvLambdaTerm *term) {
    LvLambdaTerm *body_copy = lv_lambda_copy(term->data.abs.body);
    if (!body_copy)
        return NULL;
    return lv_lambda_create_abs(term->data.abs.binder, body_copy);
}

static LvLambdaTerm *copy_app(LvLambdaTerm *term) {
    LvLambdaTerm *left_copy = lv_lambda_copy(term->data.app.left);
    LvLambdaTerm *right_copy = lv_lambda_copy(term->data.app.right);
    if (!left_copy || !right_copy) {
        lv_lambda_destroy(left_copy);
        lv_lambda_destroy(right_copy);
        return NULL;
    }
    return lv_lambda_create_app(left_copy, right_copy);
}

static const CopyHandler copy_table[LV_LAMBDA_APP + 1] = {
    [LV_LAMBDA_VAR] = copy_var,
    [LV_LAMBDA_ABS] = copy_abs,
    [LV_LAMBDA_APP] = copy_app,
};

LvLambdaTerm *lv_lambda_copy(LvLambdaTerm *term) {
    if (!term)
        return NULL;

    return LV_DISPATCH(copy_table, term->type, NULL, term);
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
/* ── 字符串化查找表 ── */
static char *lambda_to_string_internal(const LvLambdaTerm *term, size_t *out_len);
typedef char *(*ToStringHandler)(const LvLambdaTerm *term, size_t *out_len);

static char *to_string_var(const LvLambdaTerm *term, size_t *out_len) {
    lvStrBuf sb = {0};
    lv_strbuf_printf(&sb, "#%d", term->data.var.index);
    char *result = lv_strbuf_to_string(&sb);
    if (!result)
        return NULL;
    if (out_len)
        *out_len = strlen(result);
    return result;
}

static char *to_string_abs(const LvLambdaTerm *term, size_t *out_len) {
    size_t body_len = 0;
    char *body_str = lambda_to_string_internal(term->data.abs.body, &body_len);
    if (!body_str)
        return NULL;

    size_t prefix_len = 2;
    char *tmp = lv_malloc(prefix_len + body_len + 1);
    if (!tmp) {
        lv_free((void **) &body_str);
        return NULL;
    }
    memcpy(tmp, "λ.", prefix_len);
    memcpy(tmp + prefix_len, body_str, body_len + 1);
    lv_free((void **) &body_str);
    if (out_len)
        *out_len = prefix_len + body_len;
    return tmp;
}

static char *to_string_app(const LvLambdaTerm *term, size_t *out_len) {
    size_t left_len = 0, right_len = 0;
    char *left_str = lambda_to_string_internal(term->data.app.left, &left_len);
    char *right_str = lambda_to_string_internal(term->data.app.right, &right_len);

    if (!left_str || !right_str) {
        lv_free((void **) &left_str);
        lv_free((void **) &right_str);
        return NULL;
    }

    size_t total = 1 + left_len + 1 + right_len + 1;
    char *tmp = lv_malloc(total + 1);
    if (!tmp) {
        lv_free((void **) &left_str);
        lv_free((void **) &right_str);
        return NULL;
    }
    tmp[0] = '(';
    memcpy(tmp + 1, left_str, left_len);
    tmp[1 + left_len] = ' ';
    memcpy(tmp + 1 + left_len + 1, right_str, right_len);
    tmp[total - 1] = ')';
    tmp[total] = '\0';
    lv_free((void **) &left_str);
    lv_free((void **) &right_str);
    if (out_len)
        *out_len = total;
    return tmp;
}

static const ToStringHandler to_string_table[LV_LAMBDA_APP + 1] = {
    [LV_LAMBDA_VAR] = to_string_var,
    [LV_LAMBDA_ABS] = to_string_abs,
    [LV_LAMBDA_APP] = to_string_app,
};

static char *lambda_to_string_internal(const LvLambdaTerm *term, size_t *out_len) {
    if (!term) {
        if (out_len)
            *out_len = 0;
        return NULL;
    }

    /* 统一分发：越界/NULL 槽时回退并清零 out_len（与旧行为一致） */
    return LV_DISPATCH(to_string_table, term->type, (out_len ? (*out_len = 0, NULL) : NULL), term, out_len);
}

char *lv_lambda_to_string(LvLambdaTerm *term) {
    return lambda_to_string_internal(term, NULL);
}
