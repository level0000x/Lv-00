/**
 * @file expr_canon.c
 * @brief 代数表达式规范形式实现
 *
 * @details 实现 Lv00ExprCanonical 的完整生命周期、规范化和算术运算。
 *          规范形式确保代数等价的表达式产生相同的序列化形式。
 *
 *          核心算法:
 *          - 合并同类项: O(n) 哈希分组后逐组合并
 *          - 排序: 按总次数降序 + 字典序
 *          - 符号归一化: 首项系数为正
 *
 * @author Lv-00 Project
 * @version v1.0.0
 * @date 2026-05-24
 */

#include "expr_canon.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv00_utils.h"
#include "lv00_internal.h" /* LV00_UNUSED */

/* 默认初始容量 */
#define EXPR_CANON_DEFAULT_CAPACITY 16

/* ========================================================================
 * 内部辅助
 * ======================================================================== */

/** 计算项的哈希值 */
static uint64_t term_hash(const int *exponents, int var_count) {
    uint64_t h = 0x811c9dc5; /* FNV-1a offset basis (32-bit truncated) */
    for (int i = 0; i < var_count; i++) {
        h ^= (uint64_t)(exponents[i] & 0xFF);
        h *= 0x01000193; /* FNV-1a prime (64-bit) */
        h ^= (uint64_t)((exponents[i] >> 8) & 0xFF);
        h *= 0x01000193;
        h ^= (uint64_t)((exponents[i] >> 16) & 0xFF);
        h *= 0x01000193;
        h ^= (uint64_t)((exponents[i] >> 24) & 0xFF);
        h *= 0x01000193;
    }
    return h;
}

/** 判断两个指数数组是否相等 */
static bool exponents_equal(const int *a, const int *b, int var_count) {
    for (int i = 0; i < var_count; i++) {
        if (a[i] != b[i])
            return false;
    }
    return true;
}

/** 计算项的总次数 */
static int term_total_degree(const int *exponents, int var_count) {
    int total = 0;
    for (int i = 0; i < var_count; i++) {
        total += exponents[i];
    }
    return total;
}

/** 哈希-索引对（用于分组） */
typedef struct {
    uint64_t hash;
    int idx;
    bool merged; /* 是否已被合并 */
} TermHashIdx;

/** 按整数升序比较 — 用于 qsort */
static int cmp_int(const void *a, const void *b) {
    int ia = *(const int *) a;
    int ib = *(const int *) b;
    return (ia > ib) - (ia < ib);
}

/** 按 TermHashIdx 的 hash 升序排列 */
static int cmp_hashidx(const void *a, const void *b) {
    uint64_t ha = ((const TermHashIdx *) a)->hash;
    uint64_t hb = ((const TermHashIdx *) b)->hash;
    if (ha < hb)
        return -1;
    if (ha > hb)
        return 1;
    return 0;
}

/** 按规范比较顺序排列 Lv00ExprTerm*: 次数降序，同次数字典序 */
static int cmp_term_canonical(const void *a, const void *b) {
    const Lv00ExprTerm *ta = *(const Lv00ExprTerm **) a;
    const Lv00ExprTerm *tb = *(const Lv00ExprTerm **) b;

    if (!ta || !tb || !ta->exponents || !tb->exponents) {
        return 0;
    }

    int deg_a = term_total_degree(ta->exponents, ta->var_count);
    int deg_b = term_total_degree(tb->exponents, tb->var_count);

    if (deg_a != deg_b)
        return deg_b - deg_a; /* 降序 */

    /* 同次数按字典序比较指数数组 */
    return -lv00_canonical_compare_terms(ta->exponents, tb->exponents, ta->var_count);
}

/* ========================================================================
 * 排序规则
 * ======================================================================== */

int lv00_canonical_compare_terms(const int *a, const int *b, int var_count) {
    if (!a || !b)
        return 0;

    /* 字典序从最后一个变量开始比较（grlex 偏序） */
    for (int i = var_count - 1; i >= 0; i--) {
        if (a[i] != b[i]) {
            return a[i] - b[i]; /* 正数表示 a 的此项指数更大 */
        }
    }
    return 0; /* 完全相同 */
}

/* ========================================================================
 * 生命周期
 * ======================================================================== */

Lv00ExprCanonical *lv00_expr_canonical_create(int var_count, const char **var_names) {
    if (var_count < 0)
        return NULL;

    Lv00ExprCanonical *expr = (Lv00ExprCanonical *) lv00_malloc(sizeof(Lv00ExprCanonical));
    if (!expr)
        return NULL;

    expr->term_count = 0;
    expr->term_capacity = EXPR_CANON_DEFAULT_CAPACITY;
    expr->var_count = var_count;
    expr->canonicalized = true;

    expr->terms = (Lv00ExprTerm *) lv00_malloc((size_t) expr->term_capacity * sizeof(Lv00ExprTerm));
    if (!expr->terms) {
        lv00_free((void **) &expr);
        return NULL;
    }

    /* 初始化所有项 */
    for (int i = 0; i < expr->term_capacity; i++) {
        expr->terms[i].coeff = NULL;
        expr->terms[i].exponents = NULL;
        expr->terms[i].var_count = 0;
    }

    /* 拷贝变量名 */
    if (var_names && var_count > 0) {
        expr->var_names = (char **) lv00_malloc((size_t) var_count * sizeof(char *));
        if (!expr->var_names) {
            lv00_free((void **) &expr->terms);
            lv00_free((void **) &expr);
            return NULL;
        }
        for (int i = 0; i < var_count; i++) {
            if (var_names[i]) {
                size_t name_len = strlen(var_names[i]) + 1;
                expr->var_names[i] = (char *) lv00_malloc(name_len);
                if (expr->var_names[i])
                    memcpy(expr->var_names[i], var_names[i], name_len);
            } else {
                expr->var_names[i] = NULL;
            }
        }
    } else {
        expr->var_names = NULL;
    }

    return expr;
}

void lv00_expr_canonical_destroy(Lv00ExprCanonical **expr) {
    if (!expr || !*expr)
        return;

    Lv00ExprCanonical *e = *expr;

    for (int i = 0; i < e->term_capacity; i++) {
        if (e->terms[i].coeff) {
            lv00_rational_destroy(&e->terms[i].coeff);
        }
        lv00_free((void **) &e->terms[i].exponents);
    }
    lv00_free((void **) &e->terms);

    if (e->var_names) {
        for (int i = 0; i < e->var_count; i++) {
            lv00_free((void **) &e->var_names[i]);
        }
        lv00_free((void **) &e->var_names);
    }

    lv00_free((void **) expr);
}

Lv00ExprCanonical *lv00_expr_canonical_clone(const Lv00ExprCanonical *src) {
    if (!src)
        return NULL;

    const char **names = (const char **) src->var_names;
    Lv00ExprCanonical *dst = lv00_expr_canonical_create(src->var_count, names);
    if (!dst)
        return NULL;

    /* 逐个拷贝现有项 */
    for (int i = 0; i < src->term_count; i++) {
        if (!lv00_expr_canonical_add_term(dst, src->terms[i].coeff, src->terms[i].exponents)) {
            lv00_expr_canonical_destroy(&dst);
            return NULL;
        }
    }

    /* 继承源的规范化状态 */
    dst->canonicalized = src->canonicalized;

    return dst;
}

/* ========================================================================
 * 项操作
 * ======================================================================== */

/**
 * @brief 确保有空间容纳一个新项
 *
 * @return true 成功扩容或空间足够，false 分配失败
 */
static bool ensure_capacity(Lv00ExprCanonical *expr, int needed) {
    if (expr->term_count + needed <= expr->term_capacity)
        return true;

    /* 计算新容量 */
    int new_cap = expr->term_capacity;
    while (new_cap < expr->term_count + needed) {
        if (new_cap > INT_MAX / 2)
            return false;
        new_cap *= 2;
    }

    Lv00ExprTerm *new_terms = (Lv00ExprTerm *) lv00_realloc(expr->terms, (size_t) new_cap * sizeof(Lv00ExprTerm));
    if (!new_terms)
        return false;

    /* 初始化新槽位 */
    for (int i = expr->term_capacity; i < new_cap; i++) {
        new_terms[i].coeff = NULL;
        new_terms[i].exponents = NULL;
        new_terms[i].var_count = 0;
    }

    expr->terms = new_terms;
    expr->term_capacity = new_cap;
    return true;
}

bool lv00_expr_canonical_add_term(Lv00ExprCanonical *expr, const Lv00Rational *coeff, const int *exponents) {
    if (!expr || !coeff || !exponents)
        return false;
    if (lv00_rational_is_zero(coeff))
        return true; /* 零系数项直接跳过 */

    if (!ensure_capacity(expr, 1))
        return false;

    int idx = expr->term_count;

    /* 分配并拷贝系数 */
    expr->terms[idx].coeff = lv00_rational_clone(coeff);
    if (!expr->terms[idx].coeff)
        return false;

    /* 分配并拷贝指数 */
    expr->terms[idx].exponents = (int *) lv00_malloc((size_t) expr->var_count * sizeof(int));
    if (!expr->terms[idx].exponents) {
        lv00_rational_destroy(&expr->terms[idx].coeff);
        return false;
    }
    memcpy(expr->terms[idx].exponents, exponents, (size_t) expr->var_count * sizeof(int));
    expr->terms[idx].var_count = expr->var_count;

    expr->term_count++;
    expr->canonicalized = false;
    return true;
}

/* ========================================================================
 * 规范化
 * ======================================================================== */

bool lv00_expr_canonicalize(Lv00ExprCanonical *expr) {
    if (!expr)
        return false;
    if (expr->term_count == 0) {
        expr->canonicalized = true;
        return true;
    }

    int old_count = expr->term_count;
    int vc = expr->var_count;
    Lv00ExprTerm *merged = (Lv00ExprTerm *) lv00_malloc((size_t) expr->term_capacity * sizeof(Lv00ExprTerm));
    if (!merged)
        return false;

    for (int i = 0; i < expr->term_capacity; i++) {
        merged[i].coeff = NULL;
        merged[i].exponents = NULL;
        merged[i].var_count = 0;
    }

    int merged_count = 0;
    bool ok = true;

    /* 深拷贝并合并同类项，避免浅拷贝导致指针所有权混乱。 */
    for (int i = 0; i < old_count && ok; i++) {
        if (!expr->terms[i].coeff || !expr->terms[i].exponents || lv00_rational_is_zero(expr->terms[i].coeff))
            continue;

        int found = -1;
        for (int j = 0; j < merged_count; j++) {
            if (exponents_equal(merged[j].exponents, expr->terms[i].exponents, vc)) {
                found = j;
                break;
            }
        }

        if (found >= 0) {
            lv00_rational_add_inplace(merged[found].coeff, expr->terms[i].coeff);
            if (lv00_rational_is_zero(merged[found].coeff)) {
                lv00_rational_destroy(&merged[found].coeff);
                lv00_free((void **) &merged[found].exponents);
                for (int k = found; k + 1 < merged_count; k++) {
                    merged[k] = merged[k + 1];
                }
                merged_count--;
                merged[merged_count].coeff = NULL;
                merged[merged_count].exponents = NULL;
                merged[merged_count].var_count = 0;
            }
        } else {
            merged[merged_count].coeff = lv00_rational_clone(expr->terms[i].coeff);
            if (!merged[merged_count].coeff) {
                ok = false;
                break;
            }
            merged[merged_count].exponents = (int *) lv00_malloc((size_t) vc * sizeof(int));
            if (!merged[merged_count].exponents) {
                lv00_rational_destroy(&merged[merged_count].coeff);
                ok = false;
                break;
            }
            memcpy(merged[merged_count].exponents, expr->terms[i].exponents, (size_t) vc * sizeof(int));
            merged[merged_count].var_count = vc;
            merged_count++;
        }
    }

    if (!ok) {
        for (int i = 0; i < expr->term_capacity; i++) {
            if (merged[i].coeff)
                lv00_rational_destroy(&merged[i].coeff);
            lv00_free((void **) &merged[i].exponents);
        }
        lv00_free((void **) &merged);
        return false;
    }

    /* 排序：总次数降序，同次数按规范字典序。直接交换结构体，不复制内部指针。 */
    for (int i = 0; i < merged_count; i++) {
        for (int j = i + 1; j < merged_count; j++) {
            int deg_i = term_total_degree(merged[i].exponents, merged[i].var_count);
            int deg_j = term_total_degree(merged[j].exponents, merged[j].var_count);
            bool should_swap = false;
            if (deg_i < deg_j) {
                should_swap = true;
            } else if (deg_i == deg_j &&
                       lv00_canonical_compare_terms(merged[i].exponents, merged[j].exponents, vc) < 0) {
                should_swap = true;
            }
            if (should_swap) {
                Lv00ExprTerm tmp = merged[i];
                merged[i] = merged[j];
                merged[j] = tmp;
            }
        }
    }

    /* 规范化必须保持多项式值，不对整体符号做归一化。 */

    /* 释放旧项后安装新项数组。 */
    for (int i = 0; i < expr->term_capacity; i++) {
        if (expr->terms[i].coeff)
            lv00_rational_destroy(&expr->terms[i].coeff);
        lv00_free((void **) &expr->terms[i].exponents);
    }
    lv00_free((void **) &expr->terms);

    expr->terms = merged;
    expr->term_count = merged_count;
    expr->canonicalized = true;
    return true;
}

bool lv00_expr_is_canonical(const Lv00ExprCanonical *expr) {
    if (!expr)
        return false;
    if (!expr->canonicalized)
        return false;
    if (expr->term_count <= 1)
        return true;

    /* 检查排序和唯一性 */
    for (int i = 0; i < expr->term_count; i++) {
        /* 检查零系数 */
        if (expr->terms[i].coeff && lv00_rational_is_zero(expr->terms[i].coeff))
            return false;

        /* 检查排序 */
        if (i + 1 < expr->term_count) {
            int deg_a = term_total_degree(expr->terms[i].exponents, expr->var_count);
            int deg_b = term_total_degree(expr->terms[i + 1].exponents, expr->var_count);
            if (deg_a < deg_b)
                return false;
            if (deg_a == deg_b) {
                int cmp = lv00_canonical_compare_terms(expr->terms[i].exponents,
                                                        expr->terms[i + 1].exponents,
                                                        expr->var_count);
                if (cmp == 0)
                    return false; /* 重复项 */
                if (cmp < 0)
                    return false; /* 排序错误 */
            }
        }
    }

    /* 检查首项系数符号 */
    if (expr->term_count > 0 && expr->terms[0].coeff) {
        if (lv00_rational_sgn(expr->terms[0].coeff) < 0)
            return false;
    }

    return true;
}

/* ========================================================================
 * 算术操作
 * ======================================================================== */

Lv00ExprCanonical *lv00_expr_canonical_add(const Lv00ExprCanonical *a, const Lv00ExprCanonical *b) {
    if (!a || !b)
        return NULL;
    if (a->var_count != b->var_count)
        return NULL;

    const char **names = (const char **) a->var_names;
    Lv00ExprCanonical *result = lv00_expr_canonical_create(a->var_count, names);
    if (!result)
        return NULL;

    /* 添加 a 的所有项 */
    for (int i = 0; i < a->term_count; i++) {
        if (!lv00_expr_canonical_add_term(result, a->terms[i].coeff, a->terms[i].exponents)) {
            lv00_expr_canonical_destroy(&result);
            return NULL;
        }
    }

    /* 添加 b 的所有项 */
    for (int i = 0; i < b->term_count; i++) {
        if (!lv00_expr_canonical_add_term(result, b->terms[i].coeff, b->terms[i].exponents)) {
            lv00_expr_canonical_destroy(&result);
            return NULL;
        }
    }

    /* 规范化 */
    if (!lv00_expr_canonicalize(result)) {
        lv00_expr_canonical_destroy(&result);
        return NULL;
    }

    return result;
}

Lv00ExprCanonical *lv00_expr_canonical_sub(const Lv00ExprCanonical *a, const Lv00ExprCanonical *b) {
    if (!a || !b)
        return NULL;
    if (a->var_count != b->var_count)
        return NULL;

    /* a - b = a + (-b) */
    Lv00ExprCanonical *neg_b = lv00_expr_canonical_neg(b);
    if (!neg_b)
        return NULL;

    Lv00ExprCanonical *result = lv00_expr_canonical_add(a, neg_b);
    lv00_expr_canonical_destroy(&neg_b);
    return result;
}

Lv00ExprCanonical *lv00_expr_canonical_mul(const Lv00ExprCanonical *a, const Lv00ExprCanonical *b) {
    if (!a || !b)
        return NULL;
    if (a->var_count != b->var_count)
        return NULL;

    const char **names = (const char **) a->var_names;
    Lv00ExprCanonical *result = lv00_expr_canonical_create(a->var_count, names);
    if (!result)
        return NULL;

    int vc = a->var_count;

    /* 逐项乘法 */
    for (int i = 0; i < a->term_count; i++) {
        for (int j = 0; j < b->term_count; j++) {
            Lv00Rational *coeff = lv00_rational_mul(a->terms[i].coeff, b->terms[j].coeff);
            if (!coeff) {
                lv00_expr_canonical_destroy(&result);
                return NULL;
            }

            int *exp = (int *) lv00_malloc((size_t) vc * sizeof(int));
            if (!exp) {
                lv00_rational_destroy(&coeff);
                lv00_expr_canonical_destroy(&result);
                return NULL;
            }

            for (int k = 0; k < vc; k++) {
                exp[k] = a->terms[i].exponents[k] + b->terms[j].exponents[k];
            }

            bool ok = lv00_expr_canonical_add_term(result, coeff, exp);
            lv00_rational_destroy(&coeff);
            lv00_free((void **) &exp);

            if (!ok) {
                lv00_expr_canonical_destroy(&result);
                return NULL;
            }
        }
    }

    if (!lv00_expr_canonicalize(result)) {
        lv00_expr_canonical_destroy(&result);
        return NULL;
    }

    return result;
}

Lv00ExprCanonical *lv00_expr_canonical_scale(const Lv00ExprCanonical *a, const Lv00Rational *coeff) {
    if (!a || !coeff)
        return NULL;

    Lv00ExprCanonical *result = lv00_expr_canonical_clone(a);
    if (!result)
        return NULL;

    for (int i = 0; i < result->term_count; i++) {
        lv00_rational_mul_inplace(result->terms[i].coeff, coeff);
    }

    /* 重新规范化以合并可能产生的零项 */
    if (!lv00_expr_canonicalize(result)) {
        lv00_expr_canonical_destroy(&result);
        return NULL;
    }

    return result;
}

Lv00ExprCanonical *lv00_expr_canonical_neg(const Lv00ExprCanonical *a) {
    if (!a)
        return NULL;

    Lv00ExprCanonical *result = lv00_expr_canonical_clone(a);
    if (!result)
        return NULL;

    for (int i = 0; i < result->term_count; i++) {
        lv00_rational_neg_inplace(result->terms[i].coeff);
    }

    return result;
}

/* ========================================================================
 * 比较与查询
 * ======================================================================== */

bool lv00_expr_canonical_equal(const Lv00ExprCanonical *a, const Lv00ExprCanonical *b) {
    if (!a || !b)
        return (a == b);
    if (a->var_count != b->var_count)
        return false;
    if (a->term_count != b->term_count)
        return false;

    for (int i = 0; i < a->term_count; i++) {
        if (!lv00_rational_equal(a->terms[i].coeff, b->terms[i].coeff))
            return false;
        if (!exponents_equal(a->terms[i].exponents, b->terms[i].exponents, a->var_count))
            return false;
    }
    return true;
}

bool lv00_expr_canonical_is_zero(const Lv00ExprCanonical *a) {
    if (!a)
        return true;
    return a->term_count == 0;
}

int lv00_expr_canonical_degree(const Lv00ExprCanonical *expr) {
    if (!expr || expr->term_count == 0)
        return -1;

    /* 规范形式下，首项是最高次数 */
    return term_total_degree(expr->terms[0].exponents, expr->var_count);
}

int lv00_expr_canonical_term_count(const Lv00ExprCanonical *expr) {
    if (!expr)
        return 0;
    return expr->term_count;
}

/* ========================================================================
 * 字符串表示
 * ======================================================================== */

char *lv00_expr_canonical_to_string(const Lv00ExprCanonical *expr) {
    if (!expr)
        return NULL;
    if (expr->term_count == 0) {
        char *zero = (char *) malloc(2);
        if (zero)
            memcpy(zero, "0", 2);
        return zero;
    }

    /* 使用动态增长的缓冲区。返回值由调用者用 free() 释放。 */
    size_t buf_cap = 256;
    size_t buf_len = 0;
    char *buf = (char *) malloc(buf_cap);
    if (!buf)
        return NULL;
    buf[0] = '\0';

    bool first = true;

    for (int i = 0; i < expr->term_count; i++) {
        const Lv00Rational *coeff = expr->terms[i].coeff;
        const int *exp = expr->terms[i].exponents;
        int sgn = lv00_rational_sgn(coeff);

        if (sgn == 0)
            continue;

        /* 构建项的字符串片段 */
        char piece[512];
        char *pos = piece;
        size_t remain = sizeof(piece);

        /* 符号前缀 */
        if (first) {
            if (sgn < 0) {
                int w = snprintf(pos, remain, "-");
                if (w > 0) { pos += w; remain -= (size_t)w; }
            }
            first = false;
        } else {
            int w = snprintf(pos, remain, sgn >= 0 ? " + " : " - ");
            if (w > 0) { pos += w; remain -= (size_t)w; }
        }

        /* 判断是否常数项 */
        bool is_constant = true;
        for (int k = 0; k < expr->var_count; k++) {
            if (exp[k] > 0) {
                is_constant = false;
                break;
            }
        }

        if (is_constant) {
            Lv00Rational *abs_coeff = lv00_rational_abs(coeff);
            char *cs = lv00_rational_to_string(abs_coeff);
            if (cs) {
                int w = snprintf(pos, remain, "%s", cs);
                if (w > 0) { pos += w; remain -= (size_t)w; }
                free(cs);
            }
            lv00_rational_destroy(&abs_coeff);
        } else {
            /* 变量项 */
            Lv00Rational *abs_coeff = lv00_rational_abs(coeff);
            bool is_one = lv00_rational_is_one(abs_coeff);

            if (!is_one) {
                char *cs = lv00_rational_to_string(abs_coeff);
                if (cs) {
                    int w = snprintf(pos, remain, "%s*", cs);
                    if (w > 0) { pos += w; remain -= (size_t)w; }
                    free(cs);
                }
            }
            lv00_rational_destroy(&abs_coeff);

            /* 输出变量 */
            bool first_var = true;
            for (int k = 0; k < expr->var_count; k++) {
                if (exp[k] == 0)
                    continue;

                if (!first_var) {
                    int w = snprintf(pos, remain, "*");
                    if (w > 0) { pos += w; remain -= (size_t)w; }
                }
                first_var = false;

                if (expr->var_names && expr->var_names[k]) {
                    int w = snprintf(pos, remain, "%s", expr->var_names[k]);
                    if (w > 0) { pos += w; remain -= (size_t)w; }
                } else {
                    int w = snprintf(pos, remain, "x%d", k);
                    if (w > 0) { pos += w; remain -= (size_t)w; }
                }

                if (exp[k] > 1) {
                    int w = snprintf(pos, remain, "^%d", exp[k]);
                    if (w > 0) { pos += w; remain -= (size_t)w; }
                }
            }
        }

        /* 确保 piece 的大小足够并扩展到 buf */
        size_t piece_len = strlen(piece);
        while (buf_len + piece_len + 1 > buf_cap) {
            buf_cap *= 2;
            char *new_buf = (char *) realloc(buf, buf_cap);
            if (!new_buf) {
                free(buf);
                return NULL;
            }
            buf = new_buf;
        }
        memcpy(buf + buf_len, piece, piece_len + 1);
        buf_len += piece_len;
    }

    return buf;
}

/**
 * @brief 简单解析器: 极其基本的解析，仅用于测试
 *
 * 格式: "coeff*x^e*y^e + coeff*x^e + ..."
 *
 * 完整解析应由 DSL 编译器完成。此函数仅提供基础功能。
 */
Lv00ExprCanonical *lv00_expr_canonical_from_string(const char *str, const char **var_names, int var_count) {
    LV00_UNUSED(str);
    LV00_UNUSED(var_names);
    LV00_UNUSED(var_count);

    /* 桩实现: 返回零多项式
     * 完整解析器应在 DSL 编译器模块中实现。
     * 此函数保留接口但不在本文件中实现复杂的递归下降解析。 */

    Lv00ExprCanonical *expr = lv00_expr_canonical_create(var_count, var_names);
    return expr; /* 空多项式 = 0 */
}
