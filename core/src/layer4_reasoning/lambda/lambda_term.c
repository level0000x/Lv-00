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

/* ===========================================================================
 * β-归约求值器（规范序，最左最外）
 *
 * 编码约定（见 lambda_term.h）：
 *   - VAR.index 为标准 De Bruijn 相对索引（0 = 最近 binder）。
 *   - ABS.binder 恒为 0 占位，不参与求值。
 *
 * 实现：标准 TAPL shift/subst（De Bruijn 版）+ 迭代单步 β-归约。
 *   - shift(d, c, t)：把 t 中所有索引 ≥ c 的变量加 d（d 可为负；cutoff
 *     随 ABS 嵌套递增，绑定变量不受影响）。
 *   - subst(j, s, t)：把 t 中索引等于 j 的变量替换为 s（TAPL 完整版，
 *     进入 ABS 时 j+1；s 按 termSubstTop 约定预先 shift）。
 *   - 单步 β-归约（最左最外，规范序）：
 *       APP(ABS(_, body), arg) → shift(-1, subst(0, shift(1, arg), body))
 *     其余情形沿 left → right → ABS body 顺序递归寻找 redex。
 *   - lv_lambda_eval 循环调用单步归约直到无 redex（规范形）或
 *     β 步数超过上限（返回 NULL）。
 *
 * 开放项：自由变量（无法归约）原样保留，不报错。
 * 非终止项（如 Y 组合子）：β 步数超限安全返回 NULL。
 * =========================================================================== */

/* ── shift/subst 辅助 ── */

/** 对 t 做 shift：索引 ≥ cutoff 的变量加 d（d 可为负）；返回新项 */
static LvLambdaTerm *shift_internal(const LvLambdaTerm *t, int d, int cutoff) {
    LvLambdaTerm *body;
    LvLambdaTerm *l;
    LvLambdaTerm *r;

    if (!t)
        return NULL;
    switch (t->type) {
    case LV_LAMBDA_VAR:
        return lv_lambda_create_var(t->data.var.index >= cutoff ? t->data.var.index + d
                                                                : t->data.var.index);
    case LV_LAMBDA_ABS:
        body = shift_internal(t->data.abs.body, d, cutoff + 1);
        if (!body)
            return NULL;
        return lv_lambda_create_abs(t->data.abs.binder, body);
    case LV_LAMBDA_APP:
        l = shift_internal(t->data.app.left, d, cutoff);
        if (!l)
            return NULL;
        r = shift_internal(t->data.app.right, d, cutoff);
        if (!r) {
            lv_lambda_destroy(l);
            return NULL;
        }
        return lv_lambda_create_app(l, r);
    }
    return NULL;
}

/** 把 s 从替换点提升 j 层（TAPL termShift j s：cutoff 0，全部索引 +j） */
static LvLambdaTerm *shift_lift(const LvLambdaTerm *s, int j) {
    return shift_internal(s, j, 0);
}

/* β 步数上限（全局可配置，默认见头文件宏） */
static int s_eval_max_steps = LV_LAMBDA_EVAL_DEFAULT_MAX_STEPS;

/** 用 s 替换 t 中所有索引等于 j 的变量（进入 ABS 时 j+1，s 不变） */
static LvLambdaTerm *subst_internal(const LvLambdaTerm *t, int j, const LvLambdaTerm *s) {
    LvLambdaTerm *body;
    LvLambdaTerm *l;
    LvLambdaTerm *r;
    LvLambdaTerm *tmp;

    if (!t)
        return NULL;
    switch (t->type) {
    case LV_LAMBDA_VAR:
        if (t->data.var.index == j)
            return shift_lift(s, j);
        return lv_lambda_create_var(t->data.var.index);
    case LV_LAMBDA_ABS:
        body = subst_internal(t->data.abs.body, j + 1, s);
        if (!body)
            return NULL;
        return lv_lambda_create_abs(t->data.abs.binder, body);
    case LV_LAMBDA_APP:
        l = subst_internal(t->data.app.left, j, s);
        if (!l)
            return NULL;
        r = subst_internal(t->data.app.right, j, s);
        if (!r) {
            lv_lambda_destroy(l);
            return NULL;
        }
        tmp = lv_lambda_create_app(l, r);
        if (!tmp) {
            lv_lambda_destroy(l);
            lv_lambda_destroy(r);
            return NULL;
        }
        return tmp;
    }
    return NULL;
}

/* ── 单步 β-归约 ── */

/**
 * @brief 最左最外（规范序）单步 β-归约
 *
 * 找到最左最外 redex 并归约一步，返回新分配的项；无 redex 返回 NULL。
 * 输入 t 为借用；返回值由调用者负责 lv_lambda_destroy。
 */
static LvLambdaTerm *beta_step(const LvLambdaTerm *t) {
    LvLambdaTerm *mid;
    LvLambdaTerm *res;
    LvLambdaTerm *sub;
    LvLambdaTerm *lf;
    LvLambdaTerm *rt;

    if (!t)
        return NULL;

    switch (t->type) {
    case LV_LAMBDA_APP:
        if (t->data.app.left->type == LV_LAMBDA_ABS) {
            /* (λ. body) arg → shift(-1, subst(0, shift(1, arg), body)) */
            sub = shift_lift(t->data.app.right, 1);
            if (!sub)
                return NULL;
            mid = subst_internal(t->data.app.left->data.abs.body, 0, sub);
            lv_lambda_destroy(sub);
            if (!mid)
                return NULL;
            res = shift_internal(mid, -1, 0);
            lv_lambda_destroy(mid);
            return res;
        }
        /* 最左最外：先 left，后 right */
        lf = beta_step(t->data.app.left);
        if (lf)
            return lv_lambda_create_app(lf, lv_lambda_copy(t->data.app.right));
        rt = beta_step(t->data.app.right);
        if (rt)
            return lv_lambda_create_app(lv_lambda_copy(t->data.app.left), rt);
        return NULL;

    case LV_LAMBDA_ABS:
        res = beta_step(t->data.abs.body);
        if (res)
            return lv_lambda_create_abs(t->data.abs.binder, res);
        return NULL;

    case LV_LAMBDA_VAR:
    default:
        return NULL;
    }
}

/* ── 公共 API ── */

LvLambdaTerm *lv_lambda_eval(LvLambdaTerm *term) {
    LvLambdaTerm *cur;
    LvLambdaTerm *next;
    int steps = 0;

    if (!term)
        return NULL;
    cur = lv_lambda_copy(term);
    if (!cur)
        return NULL;
    for (;;) {
        next = beta_step(cur);
        if (!next)
            break;
        steps++;
        if (steps > s_eval_max_steps) {
            lv_lambda_destroy(cur);
            lv_lambda_destroy(next);
            return NULL;
        }
        lv_lambda_destroy(cur);
        cur = next;
    }
    return cur;
}

LvLambdaTerm *lv_lambda_eval_full(LvLambdaTerm *term) {
    LvLambdaTerm *r1;
    LvLambdaTerm *r2;

    if (!term)
        return NULL;
    r1 = lv_lambda_eval(term);
    if (!r1)
        return NULL;
    r2 = lv_lambda_eval(r1); /* 不动点：规范形再次求值不变 */
    lv_lambda_destroy(r1);
    return r2;
}

int lv_lambda_eval_steps(LvLambdaTerm *term) {
    LvLambdaTerm *cur;
    LvLambdaTerm *next;
    int steps = 0;

    if (!term)
        return 0;
    cur = lv_lambda_copy(term);
    if (!cur)
        return 0;
    for (;;) {
        next = beta_step(cur);
        if (!next)
            break;
        steps++;
        if (steps > s_eval_max_steps) {
            lv_lambda_destroy(cur);
            lv_lambda_destroy(next);
            return steps;
        }
        lv_lambda_destroy(cur);
        cur = next;
    }
    lv_lambda_destroy(cur);
    return steps;
}

void lv_lambda_eval_set_max_steps(int max_steps) {
    s_eval_max_steps = (max_steps > 0) ? max_steps : LV_LAMBDA_EVAL_DEFAULT_MAX_STEPS;
}
