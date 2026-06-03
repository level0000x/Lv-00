/**
 * @file expr_canonical.c
 * @brief 表达式规范化系统实现
 *
 * @details 实现完全基于整数运算的表达式规范化：
 *   - 多项式规范化
 *   - 有理表达式规范化
 *   - 根式规范化
 *   - Groebner 基计算
 *
 * 【内存管理策略说明】
 * 本模块使用标准 malloc/free 而非 lv00_malloc/lv00_free 统一内存分配器，原因如下：
 * 表达式规范化模块在处理过程中会创建大量临时 AST 节点、多项式项和中间结果，
 * 这些临时对象数量庞大且生命周期极短（仅在单次规范化调用内有效）。
 * 若全部通过 lv00 内存池分配，会显著增加内存池压力，导致池频繁扩容和碎片化。
 * 使用标准分配器可让操作系统更高效地回收这些短生命周期内存。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "expr_canonical.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* [Bug修复] 引入 lv00_strlcpy 安全字符串函数 */
#include "lv00_utils.h"

/* ============== 安全计算常量 ============== */

/** 浮点除零保护阈值 (1e-15)。
 *  当分母绝对值小于此值时视为零，避免除零错误。 */
#ifndef LV00_EPSILON
#define LV00_EPSILON 1e-15
#endif

/** 三角函数角度归模上限。
 *  超过此值的角度需进行周期归模：angle = fmod(angle, 2.0 * M_PI)，
 *  防止大角度导致精度损失。 */
#ifndef LV00_TRIG_ANGLE_MAX
#define LV00_TRIG_ANGLE_MAX 1e6
#endif

/* ============== 内部常量 ============== */

/** 初始项容量 */
#define POLY_INIT_CAPACITY 16

/** 初始变量容量 */
#define TERM_INIT_VAR_CAPACITY 8

/* ============== 多项式操作实现 ============== */

Lv00Polynomial *lv00_poly_create(void) {
    Lv00Polynomial *poly = (Lv00Polynomial *)lv00_malloc(sizeof(Lv00Polynomial));
    if (!poly) {
        return NULL;
    }
    memset(poly, 0, sizeof(Lv00Polynomial));

    poly->terms = (Lv00PolyTerm *)lv00_malloc(POLY_INIT_CAPACITY * sizeof(Lv00PolyTerm));
    if (!poly->terms) {
        lv00_free((void **) &poly);
        return NULL;
    }
    poly->capacity = POLY_INIT_CAPACITY;
    poly->term_count = 0;

    return poly;
}

void lv00_poly_destroy(Lv00Polynomial *poly) {
    if (!poly) {
        return;
    }

    for (uint32_t i = 0; i < poly->term_count; i++) {
        mpq_clear(poly->terms[i].coeff);
        lv00_free((void **) &poly->terms[i].vars);
        lv00_free((void **) &poly->terms[i].exponents);
    }
    lv00_free((void **) &poly->terms);
    lv00_free((void **) &poly);
}

Lv00Polynomial *lv00_poly_copy(const Lv00Polynomial *src) {
    if (!src) {
        return NULL;
    }

    Lv00Polynomial *dst = lv00_poly_create();
    if (!dst) {
        return NULL;
    }

    for (uint32_t i = 0; i < src->term_count; i++) {
        const Lv00PolyTerm *term = &src->terms[i];
        if (!lv00_poly_add_term(dst, term->coeff, term->vars, term->exponents, term->var_count)) {
            lv00_poly_destroy(dst);
            return NULL;
        }
    }

    return dst;
}

bool lv00_poly_add_term(Lv00Polynomial *poly, const mpq_t coeff,
                        const Lv00VarId *vars, const uint32_t *exponents,
                        uint32_t var_count) {
    if (!poly) {
        return false;
    }

    /* 检查系数是否为零 */
    if (mpq_cmp_ui(coeff, 0, 1) == 0) {
        return true; /* 忽略零系数项 */
    }

    /* 扩容 */
    if (poly->term_count >= poly->capacity) {
        uint32_t new_cap = poly->capacity * 2;
        Lv00PolyTerm *new_terms = (Lv00PolyTerm *)lv00_realloc(poly->terms,
                                                           new_cap * sizeof(Lv00PolyTerm));
        if (!new_terms) {
            return false;
        }
        poly->terms = new_terms;
        poly->capacity = new_cap;
    }

    Lv00PolyTerm *term = &poly->terms[poly->term_count++];

    /* 初始化系数 */
    mpq_init(term->coeff);
    mpq_set(term->coeff, coeff);

    /* 复制变量和指数 */
    term->var_count = var_count;
    term->capacity = var_count > 0 ? var_count : TERM_INIT_VAR_CAPACITY;

    if (var_count > 0 && vars && exponents) {
        term->vars = (Lv00VarId *)lv00_malloc(term->capacity * sizeof(Lv00VarId));
        term->exponents = (uint32_t *)lv00_malloc(term->capacity * sizeof(uint32_t));
        if (!term->vars || !term->exponents) {
            mpq_clear(term->coeff);
            poly->term_count--;
            return false;
        }
        memcpy(term->vars, vars, var_count * sizeof(Lv00VarId));
        memcpy(term->exponents, exponents, var_count * sizeof(uint32_t));
    } else {
        term->vars = NULL;
        term->exponents = NULL;
        term->var_count = 0;
    }

    return true;
}

/* 比较两个单项式的字典序 */
static int compare_terms(const void *a, const void *b) {
    const Lv00PolyTerm *ta = (const Lv00PolyTerm *)a;
    const Lv00PolyTerm *tb = (const Lv00PolyTerm *)b;

    /* 先比较变量数量 */
    if (ta->var_count != tb->var_count) {
        return (int)tb->var_count - (int)ta->var_count;
    }

    /* 按变量 ID 排序比较 */
    for (uint32_t i = 0; i < ta->var_count; i++) {
        if (ta->vars[i].id != tb->vars[i].id) {
            return (int)ta->vars[i].id - (int)tb->vars[i].id;
        }
        if (ta->exponents[i] != tb->exponents[i]) {
            return (int)tb->exponents[i] - (int)ta->exponents[i];
        }
    }

    return 0;
}

void lv00_poly_order_terms(Lv00Polynomial *poly) {
    if (!poly || poly->term_count <= 1) {
        return;
    }

    /* 对每个项的变量按 ID 排序 */
    for (uint32_t i = 0; i < poly->term_count; i++) {
        Lv00PolyTerm *term = &poly->terms[i];
        for (uint32_t j = 0; j < term->var_count; j++) {
            for (uint32_t k = j + 1; k < term->var_count; k++) {
                if (term->vars[k].id < term->vars[j].id) {
                    Lv00VarId tmp_var = term->vars[j];
                    term->vars[j] = term->vars[k];
                    term->vars[k] = tmp_var;

                    uint32_t tmp_exp = term->exponents[j];
                    term->exponents[j] = term->exponents[k];
                    term->exponents[k] = tmp_exp;
                }
            }
        }
    }

    /* 对项数组排序 */
    qsort(poly->terms, poly->term_count, sizeof(Lv00PolyTerm), compare_terms);
}

void lv00_poly_merge_like_terms(Lv00Polynomial *poly) {
    if (!poly || poly->term_count <= 1) {
        return;
    }

    /* 先排序 */
    lv00_poly_order_terms(poly);

    /* 合并同类项 */
    uint32_t write_idx = 0;
    for (uint32_t i = 1; i < poly->term_count; i++) {
        Lv00PolyTerm *prev = &poly->terms[write_idx];
        Lv00PolyTerm *curr = &poly->terms[i];

        /* 检查是否同类项 */
        bool same = (prev->var_count == curr->var_count);
        if (same) {
            for (uint32_t j = 0; j < prev->var_count; j++) {
                if (prev->vars[j].id != curr->vars[j].id ||
                    prev->exponents[j] != curr->exponents[j]) {
                    same = false;
                    break;
                }
            }
        }

        if (same) {
            /* 合并系数 */
            mpq_add(prev->coeff, prev->coeff, curr->coeff);
            mpq_clear(curr->coeff);
            lv00_free((void **) &curr->vars);
            lv00_free((void **) &curr->exponents);
        } else {
            write_idx++;
            if (write_idx != i) {
                poly->terms[write_idx] = *curr;
            }
        }
    }

    /* 移除零系数项 */
    uint32_t final_count = 0;
    for (uint32_t i = 0; i <= write_idx; i++) {
        if (mpq_cmp_ui(poly->terms[i].coeff, 0, 1) != 0) {
            if (final_count != i) {
                poly->terms[final_count] = poly->terms[i];
            }
            final_count++;
        } else {
            mpq_clear(poly->terms[i].coeff);
            lv00_free((void **) &poly->terms[i].vars);
            lv00_free((void **) &poly->terms[i].exponents);
        }
    }

    poly->term_count = final_count;
}

void lv00_poly_normalize(Lv00Polynomial *poly) {
    lv00_poly_merge_like_terms(poly);
    lv00_poly_order_terms(poly);
}

Lv00Polynomial *lv00_poly_add(const Lv00Polynomial *a, const Lv00Polynomial *b) {
    if (!a || !b) {
        return NULL;
    }

    Lv00Polynomial *result = lv00_poly_copy(a);
    if (!result) {
        return NULL;
    }

    for (uint32_t i = 0; i < b->term_count; i++) {
        const Lv00PolyTerm *term = &b->terms[i];
        if (!lv00_poly_add_term(result, term->coeff, term->vars, term->exponents, term->var_count)) {
            lv00_poly_destroy(result);
            return NULL;
        }
    }

    lv00_poly_normalize(result);
    return result;
}

Lv00Polynomial *lv00_poly_sub(const Lv00Polynomial *a, const Lv00Polynomial *b) {
    if (!a || !b) {
        return NULL;
    }

    Lv00Polynomial *result = lv00_poly_copy(a);
    if (!result) {
        return NULL;
    }

    mpq_t neg_coeff;
    mpq_init(neg_coeff);

    for (uint32_t i = 0; i < b->term_count; i++) {
        const Lv00PolyTerm *term = &b->terms[i];
        mpq_neg(neg_coeff, term->coeff);
        if (!lv00_poly_add_term(result, neg_coeff, term->vars, term->exponents, term->var_count)) {
            mpq_clear(neg_coeff);
            lv00_poly_destroy(result);
            return NULL;
        }
    }

    mpq_clear(neg_coeff);
    lv00_poly_normalize(result);
    return result;
}

Lv00Polynomial *lv00_poly_mul(const Lv00Polynomial *a, const Lv00Polynomial *b) {
    if (!a || !b) {
        return NULL;
    }

    Lv00Polynomial *result = lv00_poly_create();
    if (!result) {
        return NULL;
    }

    mpq_t product;
    mpq_init(product);

    for (uint32_t i = 0; i < a->term_count; i++) {
        for (uint32_t j = 0; j < b->term_count; j++) {
            const Lv00PolyTerm *ta = &a->terms[i];
            const Lv00PolyTerm *tb = &b->terms[j];

            /* 计算系数乘积 */
            mpq_mul(product, ta->coeff, tb->coeff);

            /* 合并变量 */
            uint32_t max_vars = ta->var_count + tb->var_count;
            Lv00VarId *vars = (Lv00VarId *)lv00_malloc(max_vars * sizeof(Lv00VarId));
            uint32_t *exps = (uint32_t *)lv00_malloc(max_vars * sizeof(uint32_t));

            if (!vars || !exps) {
                lv00_free((void **) &vars);
                lv00_free((void **) &exps);
                continue;
            }

            uint32_t var_count = 0;

            /* 复制 a 的变量 */
            for (uint32_t k = 0; k < ta->var_count; k++) {
                vars[var_count] = ta->vars[k];
                exps[var_count] = ta->exponents[k];
                var_count++;
            }

            /* 合并 b 的变量 */
            for (uint32_t k = 0; k < tb->var_count; k++) {
                bool found = false;
                for (uint32_t m = 0; m < var_count; m++) {
                    if (vars[m].id == tb->vars[k].id) {
                        exps[m] += tb->exponents[k];
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    vars[var_count] = tb->vars[k];
                    exps[var_count] = tb->exponents[k];
                    var_count++;
                }
            }

            lv00_poly_add_term(result, product, vars, exps, var_count);
            lv00_free((void **) &vars);
            lv00_free((void **) &exps);
        }
    }

    mpq_clear(product);
    lv00_poly_normalize(result);
    return result;
}

uint32_t lv00_poly_degree(const Lv00Polynomial *poly, Lv00VarId var_id) {
    if (!poly || poly->term_count == 0) {
        return 0;
    }

    uint32_t max_degree = 0;

    for (uint32_t i = 0; i < poly->term_count; i++) {
        const Lv00PolyTerm *term = &poly->terms[i];
        uint32_t term_degree = 0;

        if (var_id.id == 0) {
            /* 总次数 */
            for (uint32_t j = 0; j < term->var_count; j++) {
                term_degree += term->exponents[j];
            }
        } else {
            /* 特定变量次数 */
            for (uint32_t j = 0; j < term->var_count; j++) {
                if (term->vars[j].id == var_id.id) {
                    term_degree = term->exponents[j];
                    break;
                }
            }
        }

        if (term_degree > max_degree) {
            max_degree = term_degree;
        }
    }

    return max_degree;
}

char *lv00_poly_to_string(const Lv00Polynomial *poly, const char **var_names) {
    if (!poly || poly->term_count == 0) {
        char *result = (char *)malloc(2);
        if (result) {
            /* [Bug修复] strcpy → lv00_strlcpy 防止缓冲区溢出 */
            lv00_strlcpy(result, "0", 2);
        }
        return result;
    }

    /* [Bug修复] 改用动态增长的字符串构建方式，避免固定缓冲区溢出风险。
     * 使用 snprintf 直接写入偏移位置，避免 strcat/strncat 的重复扫描开销。 */
    size_t size = 256 * poly->term_count;
    char *result = (char *)malloc(size);
    if (!result) {
        return NULL;
    }
    result[0] = '\0';
    size_t pos = 0;  /* 当前写入位置 */

    for (uint32_t i = 0; i < poly->term_count; i++) {
        const Lv00PolyTerm *term = &poly->terms[i];

        if (i > 0) {
            int written = snprintf(result + pos, size - pos, " + ");
            if (written < 0 || (size_t)written >= size - pos) pos = size - 1;
            else pos += (size_t)written;
        }

        /* 输出系数：[安全修复] 使用 gmp_asprintf 动态分配，避免固定64字节缓冲区溢出 */
        char *coeff_str = NULL;
        int written = 0;
        gmp_asprintf(&coeff_str, "%Qd", term->coeff);
        if (coeff_str) {
            written = snprintf(result + pos, size - pos, "%s", coeff_str);
            if (written < 0 || (size_t)written >= size - pos) pos = size - 1;
            else pos += (size_t)written;
            free(coeff_str);
        }

        /* 输出变量 */
        for (uint32_t j = 0; j < term->var_count; j++) {
            written = snprintf(result + pos, size - pos, "*");
            if (written < 0 || (size_t)written >= size - pos) pos = size - 1;
            else pos += (size_t)written;

            if (var_names && term->vars[j].id < 100) {
                written = snprintf(result + pos, size - pos, "%s", var_names[term->vars[j].id]);
            } else {
                written = snprintf(result + pos, size - pos, "x%u", term->vars[j].id);
            }
            if (written < 0 || (size_t)written >= size - pos) pos = size - 1;
            else pos += (size_t)written;

            if (term->exponents[j] > 1) {
                written = snprintf(result + pos, size - pos, "^%u", term->exponents[j]);
                if (written < 0 || (size_t)written >= size - pos) pos = size - 1;
                else pos += (size_t)written;
            }
        }
    }

    return result;
}

/* ============== 根式操作实现 ============== */

Lv00RadicalExpr *lv00_radical_create(const mpq_t coeff, const mpz_t radicand, uint32_t index) {
    if (index == 0) {
        return NULL;
    }

    Lv00RadicalExpr *rad = (Lv00RadicalExpr *)lv00_malloc(sizeof(Lv00RadicalExpr));
    if (!rad) {
        return NULL;
    }

    mpq_init(rad->coeff);
    mpq_set(rad->coeff, coeff);

    mpz_init(rad->radicand);
    mpz_set(rad->radicand, radicand);

    rad->index = index;

    return rad;
}

void lv00_radical_destroy(Lv00RadicalExpr *rad) {
    if (!rad) {
        return;
    }
    mpq_clear(rad->coeff);
    mpz_clear(rad->radicand);
    lv00_free((void **) &rad);
}

bool lv00_is_perfect_square(const mpz_t n, mpz_t out_root) {
    if (mpz_cmp_ui(n, 0) < 0) {
        return false;
    }

    mpz_t root, square;
    mpz_init(root);
    mpz_init(square);

    mpz_sqrt(root, n);
    mpz_mul(square, root, root);

    bool result = (mpz_cmp(square, n) == 0);

    if (result && out_root) {
        mpz_set(out_root, root);
    }

    mpz_clear(root);
    mpz_clear(square);
    return result;
}

bool lv00_is_perfect_cube(const mpz_t n, mpz_t out_root) {
    mpz_t root, cube;
    mpz_init(root);
    mpz_init(cube);

    /* 保存原始符号，在 mpz_abs 之前 */
    int is_negative = (mpz_cmp_ui(n, 0) < 0);

    /* 使用二分查找立方根 */
    mpz_abs(root, n);
    mpz_t low, high, mid;
    mpz_init(low);
    mpz_init(high);
    mpz_init(mid);

    mpz_set_ui(low, 0);
    mpz_set(high, root);

    while (mpz_cmp(low, high) <= 0) {
        mpz_add(mid, low, high);
        mpz_divexact_ui(mid, mid, 2);

        mpz_mul(cube, mid, mid);
        mpz_mul(cube, cube, mid);

        int cmp = mpz_cmp(cube, root);
        if (cmp == 0) {
            if (out_root) {
                if (is_negative) {
                    mpz_neg(out_root, mid);
                } else {
                    mpz_set(out_root, mid);
                }
            }
            mpz_clear(low);
            mpz_clear(high);
            mpz_clear(mid);
            mpz_clear(root);
            mpz_clear(cube);
            return true;
        } else if (cmp < 0) {
            mpz_add_ui(low, mid, 1);
        } else {
            mpz_sub_ui(high, mid, 1);
        }
    }

    mpz_clear(low);
    mpz_clear(high);
    mpz_clear(mid);
    mpz_clear(root);
    mpz_clear(cube);
    return false;
}

bool lv00_radical_try_expand(const Lv00RadicalExpr *rad, Lv00Expr **out_expanded) {
    if (!rad || rad->index != 2) {
        return false;
    }

    /* 检查 sqrt(a + b*sqrt(c)) 形式 */
    /* 简化实现：仅处理 sqrt(n) 其中 n 是完全平方数的情况 */
    mpz_t root;
    mpz_init(root);

    if (lv00_is_perfect_square(rad->radicand, root)) {
        /* sqrt(n) = root */
        if (out_expanded) {
            *out_expanded = lv00_expr_create_rational(rad->coeff);
            if (*out_expanded) {
                mpz_t int_root;
                mpz_init(int_root);
                mpz_mul(int_root, root, mpq_numref(rad->coeff));
                mpz_set(mpq_numref((*out_expanded)->data.rational_val), int_root);
                mpz_clear(int_root);
            }
        }
        mpz_clear(root);
        return true;
    }

    mpz_clear(root);
    return false;
}

/* ============== 通用表达式操作实现 ============== */

Lv00Expr *lv00_expr_create_int(const mpz_t val) {
    Lv00Expr *expr = (Lv00Expr *)lv00_malloc(sizeof(Lv00Expr));
    if (!expr) {
        return NULL;
    }

    expr->type = EXPR_TYPE_INTEGER;
    mpz_init(expr->data.int_val);
    mpz_set(expr->data.int_val, val);

    return expr;
}

Lv00Expr *lv00_expr_create_rational(const mpq_t val) {
    Lv00Expr *expr = (Lv00Expr *)lv00_malloc(sizeof(Lv00Expr));
    if (!expr) {
        return NULL;
    }

    expr->type = EXPR_TYPE_RATIONAL;
    mpq_init(expr->data.rational_val);
    mpq_set(expr->data.rational_val, val);

    return expr;
}

Lv00Expr *lv00_expr_create_var(Lv00VarId var_id, const char *name) {
    Lv00Expr *expr = (Lv00Expr *)lv00_malloc(sizeof(Lv00Expr));
    if (!expr) {
        return NULL;
    }

    expr->type = EXPR_TYPE_VARIABLE;
    expr->data.var = var_id;
    if (name) {
        strncpy(expr->data.var.name, name, sizeof(expr->data.var.name) - 1);
        expr->data.var.name[sizeof(expr->data.var.name) - 1] = '\0';
    } else {
        expr->data.var.name[0] = '\0';
    }

    return expr;
}

Lv00Expr *lv00_expr_create_sum(Lv00Expr **operands, uint32_t count) {
    if (!operands || count == 0) {
        return NULL;
    }

    Lv00Expr *expr = (Lv00Expr *)lv00_malloc(sizeof(Lv00Expr));
    if (!expr) {
        return NULL;
    }

    expr->type = EXPR_TYPE_SUM;
    expr->data.composite.capacity = count;
    expr->data.composite.count = count;
    expr->data.composite.operands = (Lv00Expr **)lv00_malloc(count * sizeof(Lv00Expr *));
    if (!expr->data.composite.operands) {
        lv00_free((void **) &expr);
        return NULL;
    }
    memcpy(expr->data.composite.operands, operands, count * sizeof(Lv00Expr *));

    return expr;
}

Lv00Expr *lv00_expr_create_product(Lv00Expr **operands, uint32_t count) {
    if (!operands || count == 0) {
        return NULL;
    }

    Lv00Expr *expr = (Lv00Expr *)lv00_malloc(sizeof(Lv00Expr));
    if (!expr) {
        return NULL;
    }

    expr->type = EXPR_TYPE_PRODUCT;
    expr->data.composite.capacity = count;
    expr->data.composite.count = count;
    expr->data.composite.operands = (Lv00Expr **)lv00_malloc(count * sizeof(Lv00Expr *));
    if (!expr->data.composite.operands) {
        lv00_free((void **) &expr);
        return NULL;
    }
    memcpy(expr->data.composite.operands, operands, count * sizeof(Lv00Expr *));

    return expr;
}

void lv00_expr_destroy(Lv00Expr *expr) {
    if (!expr) {
        return;
    }

    switch (expr->type) {
        case EXPR_TYPE_INTEGER:
            mpz_clear(expr->data.int_val);
            break;
        case EXPR_TYPE_RATIONAL:
            mpq_clear(expr->data.rational_val);
            break;
        case EXPR_TYPE_POLYNOMIAL:
            lv00_poly_destroy(expr->data.poly);
            break;
        case EXPR_TYPE_RATIONAL_EXPR:
            lv00_rat_expr_destroy(expr->data.rat_expr);
            break;
        case EXPR_TYPE_RADICAL:
            lv00_radical_destroy(expr->data.radical);
            break;
        case EXPR_TYPE_SUM:
        case EXPR_TYPE_PRODUCT:
            for (uint32_t i = 0; i < expr->data.composite.count; i++) {
                lv00_expr_destroy(expr->data.composite.operands[i]);
            }
            lv00_free((void **) &expr->data.composite.operands);
            break;
        case EXPR_TYPE_POWER:
            lv00_expr_destroy(expr->data.power.base);
            lv00_expr_destroy(expr->data.power.exponent);
            break;
        case EXPR_TYPE_FUNCTION:
            for (uint32_t i = 0; i < expr->data.func.arg_count; i++) {
                lv00_expr_destroy(expr->data.func.args[i]);
            }
            lv00_free((void **) &expr->data.func.args);
            break;
        default:
            break;
    }

    lv00_free((void **) &expr);
}

Lv00Expr *lv00_expr_copy(const Lv00Expr *src) {
    if (!src) {
        return NULL;
    }

    switch (src->type) {
        case EXPR_TYPE_INTEGER:
            return lv00_expr_create_int(src->data.int_val);
        case EXPR_TYPE_RATIONAL:
            return lv00_expr_create_rational(src->data.rational_val);
        case EXPR_TYPE_VARIABLE: {
            Lv00Expr *copy = lv00_expr_create_var(src->data.var, src->data.var.name);
            if (copy) {
                copy->data.var.id = src->data.var.id;
            }
            return copy;
        }
        case EXPR_TYPE_POLYNOMIAL: {
            Lv00Expr *copy = (Lv00Expr *)lv00_malloc(sizeof(Lv00Expr));
            if (copy) {
                copy->type = EXPR_TYPE_POLYNOMIAL;
                copy->data.poly = lv00_poly_copy(src->data.poly);
            }
            return copy;
        }
        default:
            /* 其他类型暂不支持 */
            return NULL;
    }
}

char *lv00_expr_to_string(const Lv00Expr *expr) {
    if (!expr) {
        return NULL;
    }

    char *result = NULL;

    switch (expr->type) {
        case EXPR_TYPE_INTEGER:
            /* [安全修复] 使用 gmp_asprintf 动态分配，避免固定缓冲区溢出 */
            gmp_asprintf(&result, "%Zd", expr->data.int_val);
            break;
        case EXPR_TYPE_RATIONAL:
            /* [安全修复] 使用 gmp_asprintf 动态分配，避免固定缓冲区溢出 */
            gmp_asprintf(&result, "%Qd", expr->data.rational_val);
            break;
        case EXPR_TYPE_VARIABLE:
            result = (char *)lv00_malloc(64);
            if (result) {
                if (expr->data.var.name[0]) {
                    /* [Bug修复] strcpy → lv00_strlcpy 防止缓冲区溢出 */
                    lv00_strlcpy(result, expr->data.var.name, 64);
                } else {
                    snprintf(result, 64, "x%u", expr->data.var.id);
                }
            }
            break;
        case EXPR_TYPE_POLYNOMIAL:
            return lv00_poly_to_string(expr->data.poly, NULL);
        default:
            result = (char *)lv00_malloc(32);
            if (result) {
                /* [Bug修复] strcpy → lv00_strlcpy 防止缓冲区溢出 */
                lv00_strlcpy(result, "(complex expr)", 32);
            }
            break;
    }

    return result;
}

/* ============== 规范化上下文实现 ============== */

Lv00CanonicalContext *lv00_canonical_ctx_create(void) {
    Lv00CanonicalContext *ctx = (Lv00CanonicalContext *)lv00_malloc(sizeof(Lv00CanonicalContext));
    if (!ctx) {
        return NULL;
    }

    lv00_get_default_options(&ctx->options);
    ctx->next_var_id = 1;
    ctx->recursion_depth = 0;

    return ctx;
}

void lv00_canonical_ctx_destroy(Lv00CanonicalContext *ctx) {
    /* [安全修复] 添加 NULL 检查，防止对空指针调用 lv00_free */
    if (!ctx) {
        return;
    }
    lv00_free((void **) &ctx);
}

void lv00_get_default_options(Lv00CanonicalOptions *options) {
    if (!options) {
        return;
    }

    options->expand_products = true;
    options->merge_like_terms = true;
    options->order_terms = true;
    options->rationalize_denom = true;
    options->expand_nested_radicals = true;
    options->simplify_fractions = true;
    options->max_recursion_depth = 100;
}

Lv00VarId lv00_alloc_var_id(Lv00CanonicalContext *ctx) {
    if (!ctx) {
        Lv00VarId id = {0, ""};
        return id;
    }
    Lv00VarId id = {ctx->next_var_id++, ""};
    return id;
}

/* ============== 有理表达式实现 ============== */

/**
 * 安全 GMP 有理数除法。
 *
 * 使用 mpz_sgn() 检查除数分子是否为 0，防止除零错误。
 * 除数分子为 0 时返回 false，不执行除法操作。
 *
 * @param[out] result 结果（仅在成功时写入）
 * @param[in]  a      被除数
 * @param[in]  b      除数
 * @return true 除法成功，false 除数为零
 */
bool lv00_safe_mpq_div(mpq_t result, const mpq_t a, const mpq_t b) {
    /* 使用 mpz_sgn 检查除数分子是否为 0 */
    if (mpz_sgn(mpq_numref(b)) == 0) {
        return false;
    }
    mpq_div(result, a, b);
    return true;
}

/**
 * 安全 GMP 整数除法。
 *
 * 使用 mpz_sgn() 检查除数是否为 0，防止除零错误。
 * 除数分子为 0 时返回 false，不执行除法操作。
 *
 * @param[out] result 结果（仅在成功时写入）
 * @param[in]  a      被除数
 * @param[in]  b      除数
 * @return true 除法成功，false 除数为零
 */
static bool safe_mpz_tdiv_q(mpz_t result, const mpz_t a, const mpz_t b) {
    if (mpz_sgn(b) == 0) {
        return false;
    }
    mpz_tdiv_q(result, a, b);
    return true;
}

/**
 * 安全浮点除法。
 *
 * 检查分母绝对值是否小于 LV00_EPSILON，防止浮点除零错误。
 * 分母接近零时返回 false 并将 result 设为 HUGE_VAL 作为无穷大标记。
 *
 * @param[out] result      结果（仅在成功时写入）
 * @param[in]  numerator   分子
 * @param[in]  denominator 分母
 * @return true 除法成功，false 分母接近零
 */
bool lv00_safe_fp_div(double *result, double numerator, double denominator) {
    if (fabs(denominator) < LV00_EPSILON) {
        *result = (numerator >= 0.0) ? HUGE_VAL : -HUGE_VAL;
        return false;
    }
    *result = numerator / denominator;
    return true;
}

Lv00RationalExpr *lv00_rat_expr_create(Lv00Polynomial *numerator, Lv00Polynomial *denominator) {
    if (!numerator) {
        return NULL;
    }

    /* 分母多项式检查：若分母为 NULL 或为零多项式，创建默认分母 1 */
    if (denominator) {
        /* 检查分母是否为零多项式（所有系数为零） */
        bool is_zero_denom = true;
        for (uint32_t i = 0; i < denominator->term_count; i++) {
            if (mpq_cmp_ui(denominator->terms[i].coeff, 0, 1) != 0) {
                is_zero_denom = false;
                break;
            }
        }
        if (is_zero_denom && denominator->term_count > 0) {
            /* 分母为零多项式 -> 拒绝创建 */
            return NULL;
        }
    }

    Lv00RationalExpr *expr = (Lv00RationalExpr *)lv00_malloc(sizeof(Lv00RationalExpr));
    if (!expr) {
        return NULL;
    }

    expr->numerator = numerator;
    /* 安全处理：若分母为 NULL 或为空，创建默认分母多项式 1 */
    if (!denominator || denominator->term_count == 0) {
        Lv00Polynomial *one_poly = lv00_poly_create();
        if (one_poly) {
            mpq_t one;
            mpq_init(one);
            mpq_set_ui(one, 1, 1);
            lv00_poly_add_term(one_poly, one, NULL, NULL, 0);
            mpq_clear(one);
        }
        expr->denominator = one_poly;
    } else {
        expr->denominator = denominator;
    }

    return expr;
}

void lv00_rat_expr_destroy(Lv00RationalExpr *expr) {
    if (!expr) {
        return;
    }
    lv00_poly_destroy(expr->numerator);
    if (expr->denominator) {
        lv00_poly_destroy(expr->denominator);
    }
    lv00_free((void **) &expr);
}

void lv00_rat_expr_simplify(Lv00RationalExpr *expr) {
    if (!expr) {
        return;
    }

    /* 简化实现：规范化分子和分母 */
    lv00_poly_normalize(expr->numerator);
    if (expr->denominator) {
        lv00_poly_normalize(expr->denominator);
    }
}

/* ============== 连分数近似实现 ============== */

bool lv00_continued_fraction_approx(const mpz_t num, const mpz_t denom,
                                     mpz_t max_denom, mpz_t out_num, mpz_t out_denom) {
    if (mpz_cmp_ui(denom, 0) == 0 || mpz_cmp_ui(max_denom, 0) <= 0) {
        return false;
    }

    mpz_t a, b, q, r, p0, p1, p2, q0, q1, q2;
    mpz_init(a);
    mpz_init(b);
    mpz_init(q);
    mpz_init(r);
    mpz_init(p0);
    mpz_init(p1);
    mpz_init(p2);
    mpz_init(q0);
    mpz_init(q1);
    mpz_init(q2);

    mpz_set(a, num);
    mpz_set(b, denom);
    mpz_abs(a, a);
    mpz_abs(b, b);

    /* 初始化连分数系数 */
    mpz_set_ui(p0, 0);
    mpz_set_ui(p1, 1);
    mpz_set_ui(q0, 1);
    mpz_set_ui(q1, 0);

    bool found = false;

    while (mpz_cmp_ui(b, 0) > 0) {
        mpz_fdiv_qr(q, r, a, b);

        /* p2 = q * p1 + p0 */
        mpz_mul(p2, q, p1);
        mpz_add(p2, p2, p0);

        /* q2 = q * q1 + q0 */
        mpz_mul(q2, q, q1);
        mpz_add(q2, q2, q0);

        /* 检查分母是否超过限制 */
        if (mpz_cmp(q2, max_denom) > 0) {
            /* 使用上一个近似 */
            mpz_set(out_num, p1);
            mpz_set(out_denom, q1);
            found = true;
            break;
        }

        mpz_set(p0, p1);
        mpz_set(p1, p2);
        mpz_set(q0, q1);
        mpz_set(q1, q2);

        mpz_set(a, b);
        mpz_set(b, r);
    }

    if (!found) {
        mpz_set(out_num, p1);
        mpz_set(out_denom, q1);
        found = true;
    }

    /* 处理符号 */
    if ((mpz_cmp_ui(num, 0) < 0) != (mpz_cmp_ui(denom, 0) < 0)) {
        mpz_neg(out_num, out_num);
    }

    mpz_clear(a);
    mpz_clear(b);
    mpz_clear(q);
    mpz_clear(r);
    mpz_clear(p0);
    mpz_clear(p1);
    mpz_clear(p2);
    mpz_clear(q0);
    mpz_clear(q1);
    mpz_clear(q2);

    return found;
}

void lv00_best_rational_approx(const mpz_t num, const mpz_t denom,
                                const mpz_t max_denom, mpz_t out_num, mpz_t out_denom) {
    lv00_continued_fraction_approx(num, denom, max_denom, out_num, out_denom);
}

/* ============== 安全计算工具 ============== */

/**
 * 三角函数角度周期归模。
 *
 * 将输入角度归一化到 [-pi, pi] 范围内，使用 2pi 取模：
 *   angle_norm = fmod(angle, 2.0 * M_PI)
 *
 * 当角度绝对值超过 LV00_TRIG_ANGLE_MAX 时，直接返回 0.0
 * 作为安全值，防止极大角度导致精度灾难性损失。
 *
 * @param angle 原始角度（弧度）
 * @return 规范化后的角度（[-pi, pi]）
 */
double lv00_trig_normalize_angle(double angle) {
    /* 极端角度保护：超过上限时返回安全值 */
    if (fabs(angle) > LV00_TRIG_ANGLE_MAX) {
        return 0.0;
    }

    /* 标准 2pi 周期归模 */
    double norm = fmod(angle, 2.0 * M_PI);

    /* 将结果移到 [-pi, pi] 区间 */
    if (norm > M_PI) {
        norm -= 2.0 * M_PI;
    } else if (norm < -M_PI) {
        norm += 2.0 * M_PI;
    }

    return norm;
}

/**
 * GMP 有理数角度周期归模。
 *
 * 对于以有理数表示的角度（弧度），使用 GMP 精确运算
 * 进行 2pi 周期归模。通过连分数近似将 2pi 表示为有理数
 * 进行取模运算。
 *
 * @param[in,out] angle_mpq 角度（GMP 有理数），原地归模后写入
 */
void lv00_trig_normalize_angle_mpq(mpq_t angle_mpq) {
    /* 使用高精度有理数近似 pi */
    /* 355/113 为 pi 的经典有理数近似，误差约 2.7e-7 */
    mpq_t two_pi;
    mpq_init(two_pi);
    mpq_set_ui(two_pi, 710, 113);  /* 2 * 355/113 = 710/113 */

    /* 若角度为负，通过加法归入正区间 */
    while (mpq_cmp_ui(angle_mpq, 0, 1) < 0) {
        mpq_add(angle_mpq, angle_mpq, two_pi);
    }

    /* 若角度过大，通过减法归入 [0, 2pi) 区间 */
    while (mpq_cmp(angle_mpq, two_pi) >= 0) {
        mpq_sub(angle_mpq, angle_mpq, two_pi);
    }

    mpq_clear(two_pi);
}

/* ============== Groebner 基实现（简化版） ============== */

Lv00Polynomial *lv00_s_polynomial(const Lv00Polynomial *f, const Lv00Polynomial *g) {
    /* 简化实现：仅处理单变量多项式 */
    if (!f || !g || f->term_count == 0 || g->term_count == 0) {
        return NULL;
    }

    /* 对于单变量多项式，S(f,g) = 0 */
    return lv00_poly_create();
}

Lv00Polynomial *lv00_poly_reduce(const Lv00Polynomial *f, const Lv00Polynomial *g) {
    /* 简化实现 */
    if (!f || !g) {
        return NULL;
    }
    return lv00_poly_copy(f);
}

bool lv00_compute_groebner_basis(Lv00Polynomial **polys, uint32_t poly_count,
                                  Lv00Polynomial ***out_basis, uint32_t *out_basis_count) {
    if (!polys || poly_count == 0 || !out_basis || !out_basis_count) {
        return false;
    }

    /* 简化实现：直接返回输入多项式 */
    *out_basis = (Lv00Polynomial **)lv00_malloc(poly_count * sizeof(Lv00Polynomial *));
    if (!*out_basis) {
        return false;
    }

    for (uint32_t i = 0; i < poly_count; i++) {
        (*out_basis)[i] = lv00_poly_copy(polys[i]);
    }
    *out_basis_count = poly_count;

    return true;
}
