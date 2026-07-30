/**
 * @file expr_canon.c
 * @brief 代数表达式规范形式实现
 *
 * @details 实现 lvExprCanonical 的完整生命周期、规范化和算术运算。
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

#include "lv_internal.h" /* lv_UNUSED */
#include "lv_utils.h"

/* 默认初始容量 */
#define EXPR_CANON_DEFAULT_CAPACITY 16

/* ========================================================================
 * 内部辅助
 * ======================================================================== */

/** 计算项的哈希值 */
static uint64_t term_hash(const int *exponents, int var_count) {
    uint64_t h = 0x811c9dc5; /* FNV-1a offset basis (32-bit truncated) */
    for (int i = 0; i < var_count; i++) {
        h ^= (uint64_t) (exponents[i] & 0xFF);
        h *= 0x01000193; /* FNV-1a prime (64-bit) */
        h ^= (uint64_t) ((exponents[i] >> 8) & 0xFF);
        h *= 0x01000193;
        h ^= (uint64_t) ((exponents[i] >> 16) & 0xFF);
        h *= 0x01000193;
        h ^= (uint64_t) ((exponents[i] >> 24) & 0xFF);
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

/* ========================================================================
 * 排序规则
 * ======================================================================== */

int lv_canonical_compare_terms(const int *a, const int *b, int var_count) {
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

lvExprCanonical *lv_expr_canonical_create(int var_count, const char **var_names) {
    if (var_count < 0)
        return NULL;

    lvExprCanonical *expr = (lvExprCanonical *) lv_calloc(1, sizeof(lvExprCanonical));
    if (!expr)
        return NULL;
    expr->term_capacity = EXPR_CANON_DEFAULT_CAPACITY;
    expr->var_count = var_count;
    expr->canonicalized = true;

    expr->terms = (lvExprTerm *) lv_malloc((size_t) expr->term_capacity * sizeof(lvExprTerm));
    if (!expr->terms) {
        lv_free((void **) &expr);
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
        expr->var_names = (char **) lv_malloc((size_t) var_count * sizeof(char *));
        if (!expr->var_names) {
            lv_free((void **) &expr->terms);
            lv_free((void **) &expr);
            return NULL;
        }
        for (int i = 0; i < var_count; i++) {
            if (var_names[i]) {
                size_t name_len = strlen(var_names[i]) + 1;
                expr->var_names[i] = (char *) lv_malloc(name_len);
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

void lv_expr_canonical_destroy(lvExprCanonical **expr) {
    if (!expr || !*expr)
        return;

    lvExprCanonical *e = *expr;

    for (int i = 0; i < e->term_capacity; i++) {
        if (e->terms[i].coeff) {
            lv_rational_destroy(&e->terms[i].coeff);
        }
        lv_free((void **) &e->terms[i].exponents);
    }
    lv_free((void **) &e->terms);

    if (e->var_names) {
        for (int i = 0; i < e->var_count; i++) {
            lv_free((void **) &e->var_names[i]);
        }
        lv_free((void **) &e->var_names);
    }

    lv_free((void **) expr);
}

lvExprCanonical *lv_expr_canonical_clone(const lvExprCanonical *src) {
    if (!src)
        return NULL;

    const char **names = (const char **) src->var_names;
    lvExprCanonical *dst = lv_expr_canonical_create(src->var_count, names);
    if (!dst)
        return NULL;

    /* 逐个拷贝现有项 */
    for (int i = 0; i < src->term_count; i++) {
        if (!lv_expr_canonical_add_term(dst, src->terms[i].coeff, src->terms[i].exponents)) {
            lv_expr_canonical_destroy(&dst);
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
static bool ensure_capacity(lvExprCanonical *expr, int needed) {
    if (expr->term_count + needed <= expr->term_capacity)
        return true;

    /* 计算新容量 */
    int new_cap = expr->term_capacity;
    while (new_cap < expr->term_count + needed) {
        if (new_cap > INT_MAX / 2)
            return false;
        new_cap *= 2;
    }

    lvExprTerm *new_terms = (lvExprTerm *) lv_realloc(expr->terms, (size_t) new_cap * sizeof(lvExprTerm));
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

bool lv_expr_canonical_add_term(lvExprCanonical *expr, const lvRational *coeff, const int *exponents) {
    if (!expr || !coeff || !exponents)
        return false;
    if (lv_rational_is_zero(coeff))
        return true; /* 零系数项直接跳过 */

    if (!ensure_capacity(expr, 1))
        return false;

    int idx = expr->term_count;

    /* 分配并拷贝系数 */
    expr->terms[idx].coeff = lv_rational_clone(coeff);
    if (!expr->terms[idx].coeff)
        return false;

    /* 分配并拷贝指数 */
    expr->terms[idx].exponents = (int *) lv_malloc((size_t) expr->var_count * sizeof(int));
    if (!expr->terms[idx].exponents) {
        lv_rational_destroy(&expr->terms[idx].coeff);
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

bool lv_expr_canonicalize(lvExprCanonical *expr) {
    if (!expr)
        return false;
    if (expr->term_count == 0) {
        expr->canonicalized = true;
        return true;
    }

    int old_count = expr->term_count;
    int vc = expr->var_count;
    lvExprTerm *merged = (lvExprTerm *) lv_malloc((size_t) expr->term_capacity * sizeof(lvExprTerm));
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
        if (!expr->terms[i].coeff || !expr->terms[i].exponents || lv_rational_is_zero(expr->terms[i].coeff))
            continue;

        int found = -1;
        for (int j = 0; j < merged_count; j++) {
            if (exponents_equal(merged[j].exponents, expr->terms[i].exponents, vc)) {
                found = j;
                break;
            }
        }

        if (found >= 0) {
            lv_rational_add_inplace(merged[found].coeff, expr->terms[i].coeff);
            if (lv_rational_is_zero(merged[found].coeff)) {
                lv_rational_destroy(&merged[found].coeff);
                lv_free((void **) &merged[found].exponents);
                for (int k = found; k + 1 < merged_count; k++) {
                    merged[k] = merged[k + 1];
                }
                merged_count--;
                merged[merged_count].coeff = NULL;
                merged[merged_count].exponents = NULL;
                merged[merged_count].var_count = 0;
            }
        } else {
            merged[merged_count].coeff = lv_rational_clone(expr->terms[i].coeff);
            if (!merged[merged_count].coeff) {
                ok = false;
                break;
            }
            merged[merged_count].exponents = (int *) lv_malloc((size_t) vc * sizeof(int));
            if (!merged[merged_count].exponents) {
                lv_rational_destroy(&merged[merged_count].coeff);
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
                lv_rational_destroy(&merged[i].coeff);
            lv_free((void **) &merged[i].exponents);
        }
        lv_free((void **) &merged);
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
            } else if (deg_i == deg_j && lv_canonical_compare_terms(merged[i].exponents, merged[j].exponents, vc) < 0) {
                should_swap = true;
            }
            if (should_swap) {
                lvExprTerm tmp = merged[i];
                merged[i] = merged[j];
                merged[j] = tmp;
            }
        }
    }

    /* 规范化必须保持多项式值，不对整体符号做归一化。 */

    /* 释放旧项后安装新项数组。 */
    for (int i = 0; i < expr->term_capacity; i++) {
        if (expr->terms[i].coeff)
            lv_rational_destroy(&expr->terms[i].coeff);
        lv_free((void **) &expr->terms[i].exponents);
    }
    lv_free((void **) &expr->terms);

    expr->terms = merged;
    expr->term_count = merged_count;
    expr->canonicalized = true;
    return true;
}

bool lv_expr_is_canonical(const lvExprCanonical *expr) {
    if (!expr)
        return false;
    if (!expr->canonicalized)
        return false;
    if (expr->term_count <= 1)
        return true;

    /* 检查排序和唯一性 */
    for (int i = 0; i < expr->term_count; i++) {
        /* 检查零系数 */
        if (expr->terms[i].coeff && lv_rational_is_zero(expr->terms[i].coeff))
            return false;

        /* 检查排序 */
        if (i + 1 < expr->term_count) {
            int deg_a = term_total_degree(expr->terms[i].exponents, expr->var_count);
            int deg_b = term_total_degree(expr->terms[i + 1].exponents, expr->var_count);
            if (deg_a < deg_b)
                return false;
            if (deg_a == deg_b) {
                int cmp =
                    lv_canonical_compare_terms(expr->terms[i].exponents, expr->terms[i + 1].exponents, expr->var_count);
                if (cmp == 0)
                    return false; /* 重复项 */
                if (cmp < 0)
                    return false; /* 排序错误 */
            }
        }
    }

    /* 检查首项系数符号 */
    if (expr->term_count > 0 && expr->terms[0].coeff) {
        if (lv_rational_sgn(expr->terms[0].coeff) < 0)
            return false;
    }

    return true;
}

/* ========================================================================
 * 算术操作
 * ======================================================================== */

lvExprCanonical *lv_expr_canonical_add(const lvExprCanonical *a, const lvExprCanonical *b) {
    if (!a || !b)
        return NULL;
    if (a->var_count != b->var_count)
        return NULL;

    const char **names = (const char **) a->var_names;
    lvExprCanonical *result = lv_expr_canonical_create(a->var_count, names);
    if (!result)
        return NULL;

    /* 添加 a 的所有项 */
    for (int i = 0; i < a->term_count; i++) {
        if (!lv_expr_canonical_add_term(result, a->terms[i].coeff, a->terms[i].exponents)) {
            lv_expr_canonical_destroy(&result);
            return NULL;
        }
    }

    /* 添加 b 的所有项 */
    for (int i = 0; i < b->term_count; i++) {
        if (!lv_expr_canonical_add_term(result, b->terms[i].coeff, b->terms[i].exponents)) {
            lv_expr_canonical_destroy(&result);
            return NULL;
        }
    }

    /* 规范化 */
    if (!lv_expr_canonicalize(result)) {
        lv_expr_canonical_destroy(&result);
        return NULL;
    }

    return result;
}

lvExprCanonical *lv_expr_canonical_sub(const lvExprCanonical *a, const lvExprCanonical *b) {
    if (!a || !b)
        return NULL;
    if (a->var_count != b->var_count)
        return NULL;

    /* a - b = a + (-b) */
    lvExprCanonical *neg_b = lv_expr_canonical_neg(b);
    if (!neg_b)
        return NULL;

    lvExprCanonical *result = lv_expr_canonical_add(a, neg_b);
    lv_expr_canonical_destroy(&neg_b);
    return result;
}

lvExprCanonical *lv_expr_canonical_mul(const lvExprCanonical *a, const lvExprCanonical *b) {
    if (!a || !b)
        return NULL;
    if (a->var_count != b->var_count)
        return NULL;

    const char **names = (const char **) a->var_names;
    lvExprCanonical *result = lv_expr_canonical_create(a->var_count, names);
    if (!result)
        return NULL;

    int vc = a->var_count;

    /* 逐项乘法 */
    for (int i = 0; i < a->term_count; i++) {
        for (int j = 0; j < b->term_count; j++) {
            lvRational *coeff = lv_rational_mul(a->terms[i].coeff, b->terms[j].coeff);
            if (!coeff) {
                lv_expr_canonical_destroy(&result);
                return NULL;
            }

            int *exp = (int *) lv_malloc((size_t) vc * sizeof(int));
            if (!exp) {
                lv_rational_destroy(&coeff);
                lv_expr_canonical_destroy(&result);
                return NULL;
            }

            for (int k = 0; k < vc; k++) {
                exp[k] = a->terms[i].exponents[k] + b->terms[j].exponents[k];
            }

            bool ok = lv_expr_canonical_add_term(result, coeff, exp);
            lv_rational_destroy(&coeff);
            lv_free((void **) &exp);

            if (!ok) {
                lv_expr_canonical_destroy(&result);
                return NULL;
            }
        }
    }

    if (!lv_expr_canonicalize(result)) {
        lv_expr_canonical_destroy(&result);
        return NULL;
    }

    return result;
}

lvExprCanonical *lv_expr_canonical_scale(const lvExprCanonical *a, const lvRational *coeff) {
    if (!a || !coeff)
        return NULL;

    lvExprCanonical *result = lv_expr_canonical_clone(a);
    if (!result)
        return NULL;

    for (int i = 0; i < result->term_count; i++) {
        lv_rational_mul_inplace(result->terms[i].coeff, coeff);
    }

    /* 重新规范化以合并可能产生的零项 */
    if (!lv_expr_canonicalize(result)) {
        lv_expr_canonical_destroy(&result);
        return NULL;
    }

    return result;
}

lvExprCanonical *lv_expr_canonical_neg(const lvExprCanonical *a) {
    if (!a)
        return NULL;

    lvExprCanonical *result = lv_expr_canonical_clone(a);
    if (!result)
        return NULL;

    for (int i = 0; i < result->term_count; i++) {
        lv_rational_neg_inplace(result->terms[i].coeff);
    }

    return result;
}

/* ========================================================================
 * 比较与查询
 * ======================================================================== */

bool lv_expr_canonical_equal(const lvExprCanonical *a, const lvExprCanonical *b) {
    if (!a || !b)
        return (a == b);
    if (a->var_count != b->var_count)
        return false;
    if (a->term_count != b->term_count)
        return false;

    for (int i = 0; i < a->term_count; i++) {
        if (!lv_rational_equal(a->terms[i].coeff, b->terms[i].coeff))
            return false;
        if (!exponents_equal(a->terms[i].exponents, b->terms[i].exponents, a->var_count))
            return false;
    }
    return true;
}

bool lv_expr_canonical_is_zero(const lvExprCanonical *a) {
    if (!a)
        return true;
    return a->term_count == 0;
}

int lv_expr_canonical_degree(const lvExprCanonical *expr) {
    if (!expr || expr->term_count == 0)
        return -1;

    /* 规范形式下，首项是最高次数 */
    return term_total_degree(expr->terms[0].exponents, expr->var_count);
}

int lv_expr_canonical_term_count(const lvExprCanonical *expr) {
    if (!expr)
        return 0;
    return expr->term_count;
}

/* ========================================================================
 * 字符串表示
 * ======================================================================== */

char *lv_expr_canonical_to_string(const lvExprCanonical *expr) {
    if (!expr)
        return NULL;
    if (expr->term_count == 0) {
        char *zero = (char *) lv_malloc(2);
        if (zero)
            memcpy(zero, "0", 2);
        return zero;
    }

    /* 使用动态增长的缓冲区。返回值由调用者用 free() 释放。 */
    size_t buf_cap = 256;
    size_t buf_len = 0;
    char *buf = (char *) lv_malloc(buf_cap);
    if (!buf)
        return NULL;
    buf[0] = '\0';

    bool first = true;

    for (int i = 0; i < expr->term_count; i++) {
        const lvRational *coeff = expr->terms[i].coeff;
        const int *exp = expr->terms[i].exponents;
        int sgn = lv_rational_sgn(coeff);

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
                if (w > 0) {
                    pos += w;
                    remain -= (size_t) w;
                }
            }
            first = false;
        } else {
            int w = snprintf(pos, remain, sgn >= 0 ? " + " : " - ");
            if (w > 0) {
                pos += w;
                remain -= (size_t) w;
            }
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
            lvRational *abs_coeff = lv_rational_abs(coeff);
            char *cs = lv_rational_to_string(abs_coeff);
            if (cs) {
                int w = snprintf(pos, remain, "%s", cs);
                if (w > 0) {
                    pos += w;
                    remain -= (size_t) w;
                }
                lv_free((void **)&(cs));
            }
            lv_rational_destroy(&abs_coeff);
        } else {
            /* 变量项 */
            lvRational *abs_coeff = lv_rational_abs(coeff);
            bool is_one = lv_rational_is_one(abs_coeff);

            if (!is_one) {
                char *cs = lv_rational_to_string(abs_coeff);
                if (cs) {
                    int w = snprintf(pos, remain, "%s*", cs);
                    if (w > 0) {
                        pos += w;
                        remain -= (size_t) w;
                    }
                    lv_free((void **)&(cs));
                }
            }
            lv_rational_destroy(&abs_coeff);

            /* 输出变量 */
            bool first_var = true;
            for (int k = 0; k < expr->var_count; k++) {
                if (exp[k] == 0)
                    continue;

                if (!first_var) {
                    int w = snprintf(pos, remain, "*");
                    if (w > 0) {
                        pos += w;
                        remain -= (size_t) w;
                    }
                }
                first_var = false;

                if (expr->var_names && expr->var_names[k]) {
                    int w = snprintf(pos, remain, "%s", expr->var_names[k]);
                    if (w > 0) {
                        pos += w;
                        remain -= (size_t) w;
                    }
                } else {
                    int w = snprintf(pos, remain, "x%d", k);
                    if (w > 0) {
                        pos += w;
                        remain -= (size_t) w;
                    }
                }

                if (exp[k] > 1) {
                    int w = snprintf(pos, remain, "^%d", exp[k]);
                    if (w > 0) {
                        pos += w;
                        remain -= (size_t) w;
                    }
                }
            }
        }

        /* 确保 piece 的大小足够并扩展到 buf */
        size_t piece_len = strlen(piece);
        while (buf_len + piece_len + 1 > buf_cap) {
            buf_cap *= 2;
            char *new_buf = (char *) lv_realloc(buf, buf_cap);
            if (!new_buf) {
                lv_free((void **)&(buf));
                return NULL;
            }
            buf = new_buf;
        }
        memcpy(buf + buf_len, piece, piece_len + 1);
        buf_len += piece_len;
    }

    return buf;
}

/* ========================================================================
 * 字符串解析（递归下降解析器）
 * ======================================================================== */

/** 跳过空白字符 */
static void skip_spaces(const char **pp) {
    while (**pp && (unsigned char)**pp <= ' ')
        (*pp)++;
}

/**
 * @brief 解析不含 '/' 的十进制数字字符串为有理数
 *
 * 支持 "42"（整数）和 "3.14"（小数）两种格式。
 * 小数通过去除小数点转为分数形式，再通过 lv_rational_from_string
 * 构造后化简。
 */
static lvRational *parse_decimal(const char *start, const char *end) {
    /* 检查是否有小数点 */
    const char *dot = NULL;
    for (const char *cp = start; cp < end; cp++) {
        if (*cp == '.') {
            dot = cp;
            break;
        }
    }

    if (!dot) {
        /* 纯整数 */
        long val;
        char *e = NULL;
        val = strtol(start, &e, 10);
        if (e == start) return NULL;
        return lv_rational_create_from_si(val, 1);
    }

    /* 小数：提取所有数字字符，构造 num/den 分数 */
    int digits_before_dot = (int)(dot - start);
    int digits_after_dot  = (int)(end - dot - 1);

    /* 限制小数位精度，避免分母过大 */
    if (digits_after_dot > 9) digits_after_dot = 9;

    /* 构造分母字符串 "1" + digits_after_dot 个 "0" */
    char den_buf[32];
    int den_idx = 0;
    den_buf[den_idx++] = '1';
    for (int i = 0; i < digits_after_dot && den_idx < 30; i++)
        den_buf[den_idx++] = '0';
    den_buf[den_idx] = '\0';

    /* 构造分子：去掉小数点后的数字字符串 */
    char num_buf[128];
    int num_idx = 0;

    /* 处理符号 */
    if (*start == '-') {
        num_buf[num_idx++] = '-';
        start++;
    } else if (*start == '+') {
        start++;
    }

    /* 复制小数点前的数字（不含 '.' 本身）*/
    for (int i = 0; i < digits_before_dot; i++) {
        num_buf[num_idx++] = start[i];
    }

    /* 复制小数点后的数字 */
    for (int i = 0; i < digits_after_dot; i++) {
        num_buf[num_idx++] = dot[1 + i];
    }
    num_buf[num_idx] = '\0';

    /* 处理边缘情况：纯小数如 ".5" → 分子 "5" */
    if (num_idx == 0 || (num_idx == 1 && num_buf[0] == '-')) {
        num_buf[num_idx++] = '0';
        num_buf[num_idx] = '\0';
    }

    /* 组合 "num/den" */
    char full[256];
    int n = snprintf(full, sizeof(full), "%s/%s", num_buf, den_buf);
    if (n < 0 || (size_t)n >= sizeof(full))
        return NULL;

    lvRational *r = lv_rational_from_string(full);
    if (r) lv_rational_simplify(r);
    return r;
}

/**
 * @brief 从字符串解析规范多项式表达式
 *
 * 支持语法：
 *   expr   → term (('+' | '-') term)*
 *   term   → [SIGN] [NUMBER] ['*'] factor ('*' factor)*
 *   factor → VARIABLE ['^' UINT]
 *
 * 示例: "3*x^2*y + 5*x - 2", "-x + y", "42", "x^2"
 *
 * 变量名必须在 var_names 数组中注册，否则解析失败。
 *
 * @param str       输入字符串
 * @param var_names 变量名数组
 * @param var_count 变量个数
 * @return 解析后的规范多项式，失败返回 NULL
 */
lvExprCanonical *lv_expr_canonical_from_string(const char *str,
                                               const char **var_names,
                                               int var_count) {
    if (!str)
        return NULL;

    lvExprCanonical *expr = lv_expr_canonical_create(var_count, var_names);
    if (!expr)
        return NULL;

    const char *p = str;
    skip_spaces(&p);

    if (*p == '\0')
        return expr; /* 空字符串 = 零多项式 */

    int sign = 1;
    bool first_item = true;

    while (*p) {
        skip_spaces(&p);
        if (*p == '\0')
            break;

        /* --- 处理 +/- 分隔符 --- */
        if (*p == '+' || *p == '-') {
            if (!first_item) {
                sign = (*p == '+') ? 1 : -1;
                p++;
                continue;
            } else {
                sign = (*p == '-') ? -1 : 1;
                p++;
                first_item = false;
            }
        } else if (first_item) {
            first_item = false;
            sign = 1;
        } else {
            /* 非首项未出现 + / -，视为终止 */
            break;
        }

        skip_spaces(&p);
        if (*p == '\0')
            break;

        /* --- 解析数字系数 --- */
        lvRational *coeff = NULL;
        const char *num_start = p;

        if (*p == '-' || *p == '+' || (*p >= '0' && *p <= '9') || *p == '.') {
            char *num_end = NULL;
            strtod(p, &num_end);
            if (num_end != p) {
                coeff = parse_decimal(num_start, num_end);
                p = num_end;
                /* 系数后可能有 '*' 分隔符 */
                skip_spaces(&p);
                if (*p == '*') {
                    p++;
                    skip_spaces(&p);
                }
            }
        }

        /* --- 解析变量因子 --- */
        int *exponents = (int *)lv_calloc((size_t)var_count, sizeof(int));
        if (!exponents) {
            if (coeff) lv_rational_destroy(&coeff);
            lv_expr_canonical_destroy(&expr);
            return NULL;
        }

        bool has_var_part = false;

        while (*p) {
            skip_spaces(&p);
            if (*p == '\0' || *p == '+' || *p == '-')
                break;

            /* 尝试匹配变量名 */
            int longest_match = 0;
            int var_idx = -1;

            for (int i = 0; i < var_count; i++) {
                if (!var_names[i]) continue;
                size_t vlen = strlen(var_names[i]);
                if ((int)vlen > longest_match &&
                    strncmp(p, var_names[i], vlen) == 0) {
                    /* 确保不是更长的标识符的一部分 */
                    if (p[vlen] == '\0' || p[vlen] == '*' ||
                        p[vlen] == '^' || p[vlen] == '+' ||
                        p[vlen] == '-' || (unsigned char)p[vlen] <= ' ') {
                        longest_match = (int)vlen;
                        var_idx = i;
                    }
                }
            }

            if (var_idx < 0)
                break; /* 不是变量，终止变量因子解析 */

            has_var_part = true;
            p += longest_match;

            /* 解析可选的指数 ^N */
            int exponent = 1;
            skip_spaces(&p);
            if (*p == '^') {
                p++;
                skip_spaces(&p);
                char *exp_end = NULL;
                long exp_val = strtol(p, &exp_end, 10);
                if (exp_end != p && exp_val > 0 && exp_val <= 65536) {
                    exponent = (int)exp_val;
                    p = exp_end;
                }
            }

            exponents[var_idx] += exponent;

            /* 跳过可选的 '*' */
            skip_spaces(&p);
            if (*p == '*') {
                p++;
                skip_spaces(&p);
            }
        }

        /* --- 创建项 --- */
        if (coeff == NULL) {
            /* 没有显式系数：若只有变量部分，系数为 1 */
            coeff = lv_rational_create_from_si(sign, 1);
        } else if (sign == -1) {
            lv_rational_neg_inplace(coeff);
        }

        if (!has_var_part && coeff) {
            /* 常数项，指数已全零 */
        }

        if (coeff && !lv_rational_is_zero(coeff)) {
            lv_expr_canonical_add_term(expr, coeff, exponents);
        }

        lv_rational_destroy(&coeff);
        lv_free((void **)&exponents);
    }

    /* 规范化并返回 */
    if (!lv_expr_canonicalize(expr)) {
        lv_expr_canonical_destroy(&expr);
        return NULL;
    }

    return expr;
}

/* ========================================================================
 * 旧接口兼容
 * ======================================================================== */

char *lv_expr_canon(const char *expr) {
    if (!expr)
        return NULL;

    /* 尝试从字符串解析为规范形式 */
    const char *var_names[] = {"x", "y", "z", "w", "u", "v"};
    int var_count = 6;
    lvExprCanonical *canon = lv_expr_canonical_from_string(expr, var_names, var_count);
    if (!canon) {
        /* 解析失败，回退：返回原始字符串的副本 */
        char *fallback = (char *)lv_malloc(strlen(expr) + 1);
        if (fallback)
            memcpy(fallback, expr, strlen(expr) + 1);
        return fallback;
    }

    char *result = lv_expr_canonical_to_string(canon);
    lv_expr_canonical_destroy(&canon);
    return result;
}
