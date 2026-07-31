/**
 * @file mv_polynomial.c
 * @brief 多变量多项式（稀疏多元形式）实现
 *
 * @details 支持任意数量变量的稀疏多项式表示与操作。
 *          每个多项式由一组 MVTerm 构成，每个项包含：
 *          - GMP 大整数系数（mpz_t）
 *          - 各变量的指数数组（int *exponents，长度为 var_count）
 *
 *          主要操作：
 *          - mv_poly_init / mv_poly_clear: 生命周期管理
 *          - mv_poly_add_term: 添加项（同类项自动合并系数）
 *          - mv_poly_copy / mv_poly_negate: 拷贝与取反
 *          - mv_poly_add / mv_poly_sub / mv_poly_mul: 算术运算
 *          - mv_poly_eval: 在指定点求值
 *
 *          存储采用动态数组，初始容量 lv_SOLVER_DYNARRAY_INIT_CAP (16)，
 *          扩容因子 lv_ARRAY_GROWTH_FACTOR (2)。
 *
 *          原位置: solver.c L4703-L4950，于 v3.3.0 拆分为独立模块。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 * @date 2026-05-24
 */

#include "../mv_polynomial.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "lv/lv.h"
#include "lv_internal.h"

/* 动态数组初始容量 */
#ifndef lv_SOLVER_DYNARRAY_INIT_CAP
#define lv_SOLVER_DYNARRAY_INIT_CAP 16
#endif

#ifndef lv_ARRAY_GROWTH_FACTOR
#define lv_ARRAY_GROWTH_FACTOR 2
#endif

/* ---- 生命周期 ---- */

void mv_poly_init(MVPolynomial *p, int var_count) {
    p->terms = NULL;
    p->term_count = 0;
    p->capacity = 0;
    p->var_count = var_count;
}

void mv_poly_clear(MVPolynomial *p) {
    for (int i = 0; i < p->term_count; i++) {
        lv_free((void **) &p->terms[i].exponents);
        mpz_clear(p->terms[i].coeff);
    }
    lv_free((void **) &p->terms);
    p->terms = NULL;
    p->term_count = 0;
    p->capacity = 0;
}

/* ---- 项操作 ---- */

int mv_poly_add_term(MVPolynomial *p, const mpz_t coeff, const int *exponents) {
    if (mpz_cmp_si(coeff, 0) == 0)
        return 0;

    /* 检查是否已有同类项 */
    for (int i = 0; i < p->term_count; i++) {
        bool same = true;
        for (int v = 0; v < p->var_count; v++) {
            if (p->terms[i].exponents[v] != exponents[v]) {
                same = false;
                break;
            }
        }
        if (same) {
            mpz_add(p->terms[i].coeff, p->terms[i].coeff, coeff);
            /* 如果合并后系数为零，移除该项防止 Groebner 基计算错误 */
            if (mpz_cmp_si(p->terms[i].coeff, 0) == 0) {
                mpz_clear(p->terms[i].coeff);
                int last = p->term_count - 1;
                if (i < last) {
                    p->terms[i] = p->terms[last];
                }
                p->term_count--;
            }
            return 0;
        }
    }

    /* 新单项式 */
    if (p->term_count >= p->capacity) {
        int new_cap = p->capacity == 0 ? lv_SOLVER_DYNARRAY_INIT_CAP : p->capacity;
        if (new_cap > 0 && new_cap > INT_MAX / lv_ARRAY_GROWTH_FACTOR) {
            lv_RETURN_ERROR(lv_ERROR_OVERFLOW, "mv_poly_add_term: 容量溢出");
        }
        new_cap = new_cap == 0 ? lv_SOLVER_DYNARRAY_INIT_CAP : new_cap * lv_ARRAY_GROWTH_FACTOR;
        /* 检查 size_t 乘积溢出：new_cap * sizeof(MVMonomial) 可能超过 SIZE_MAX */
        if ((size_t) new_cap > SIZE_MAX / sizeof(MVMonomial)) {
            lv_RETURN_ERROR(lv_ERROR_OVERFLOW, "mv_poly_add_term: 容量溢出");
        }
        p->capacity = new_cap;
        MVMonomial *new_terms = lv_realloc(p->terms, p->capacity * sizeof(MVMonomial));
        if (!new_terms) {
            lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "mv_poly_add_term: 扩容失败");
        }
        p->terms = new_terms;
    }
    MVMonomial *m = &p->terms[p->term_count];
    m->exponents = lv_calloc((size_t) p->var_count, sizeof(int));
    if (!m->exponents) {
        lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "mv_poly_add_term: 指数数组分配失败");
    }
    for (int v = 0; v < p->var_count; v++) {
        m->exponents[v] = exponents[v];
    }
    mpz_init_set(m->coeff, coeff);
    p->term_count++;
    return 0;
}

/* 按 grlex 字典序比较单项式（ctx 指向变量数 int） */
static int cmp_mv_monomial_grlex(const void *a, const void *b, void *ctx) {
    int var_count = *(const int *) ctx;
    return mv_monomial_compare_grlex((const MVMonomial *) a, (const MVMonomial *) b, var_count);
}

void mv_poly_sort(MVPolynomial *p) {
    /* 简单插入排序 (单项式数量通常不大) */
    lv_insertion_sort(p->terms, (size_t) p->term_count, sizeof(MVMonomial), cmp_mv_monomial_grlex,
                      &p->var_count);
}

void mv_poly_remove_zeros(MVPolynomial *p) {
    int write = 0;
    for (int i = 0; i < p->term_count; i++) {
        if (mpz_cmp_si(p->terms[i].coeff, 0) != 0) {
            if (write != i)
                p->terms[write] = p->terms[i];
            write++;
        } else {
            lv_free((void **) &p->terms[i].exponents);
            mpz_clear(p->terms[i].coeff);
        }
    }
    p->term_count = write;
}

void mv_poly_mul_monomial(MVPolynomial *result, const MVPolynomial *p, const int *mono_exp, const mpz_t mono_coeff,
                          int var_count) {
    mv_poly_clear(result);
    mv_poly_init(result, var_count);
    for (int i = 0; i < p->term_count; i++) {
        mpz_t new_coeff;
        mpz_init(new_coeff);
        mpz_mul(new_coeff, p->terms[i].coeff, mono_coeff);

        int *new_exp = lv_calloc((size_t) var_count, sizeof(int));
        if (!new_exp) {
            mpz_clear(new_coeff);
            continue;
        }
        for (int v = 0; v < var_count; v++) {
            new_exp[v] = p->terms[i].exponents[v] + mono_exp[v];
        }
        mv_poly_add_term(result, new_coeff, new_exp);
        mpz_clear(new_coeff);
        lv_free((void **) &new_exp);
    }
    mv_poly_sort(result);
}

/* ---- 算术 ---- */

void mv_poly_sub(MVPolynomial *result, const MVPolynomial *a, const MVPolynomial *b) {
    mv_poly_clear(result);
    mv_poly_init(result, a->var_count);
    for (int i = 0; i < a->term_count; i++) {
        mv_poly_add_term(result, a->terms[i].coeff, a->terms[i].exponents);
    }
    for (int i = 0; i < b->term_count; i++) {
        mpz_t neg_coeff;
        mpz_init(neg_coeff);
        mpz_neg(neg_coeff, b->terms[i].coeff);
        mv_poly_add_term(result, neg_coeff, b->terms[i].exponents);
        mpz_clear(neg_coeff);
    }
    mv_poly_sort(result);
    mv_poly_remove_zeros(result);
}

void mv_poly_copy(MVPolynomial *dst, const MVPolynomial *src) {
    mv_poly_clear(dst);
    mv_poly_init(dst, src->var_count);
    for (int i = 0; i < src->term_count; i++) {
        mv_poly_add_term(dst, src->terms[i].coeff, src->terms[i].exponents);
    }
}

/* ---- 查询 ---- */

bool mv_poly_is_zero(const MVPolynomial *p) {
    return p->term_count == 0;
}

int mv_poly_leading_term(const MVPolynomial *p, MVMonomial *out) {
    if (p->term_count == 0)
        lv_RETURN_ERROR(lv_ERROR_NOT_FOUND, "mv_poly_leading_term: polynomial is empty");
    /* 已排序，第一个即为首项 */
    if (out) {
        *out = p->terms[0];
    }
    return 0;
}

/* ---- 单项式工具 ---- */

int mv_monomial_total_degree(const MVMonomial *m, int var_count) {
    int deg = 0;
    for (int v = 0; v < var_count; v++) {
        deg += m->exponents[v];
    }
    return deg;
}

int mv_monomial_compare_grlex(const MVMonomial *a, const MVMonomial *b, int var_count) {
    int deg_a = mv_monomial_total_degree(a, var_count);
    int deg_b = mv_monomial_total_degree(b, var_count);
    if (deg_a != deg_b)
        return deg_b - deg_a; /* 降序: 高次在前 */
    /* 字典序比较 (从第一个变量开始) */
    for (int v = 0; v < var_count; v++) {
        if (a->exponents[v] != b->exponents[v]) {
            return b->exponents[v] - a->exponents[v]; /* 降序 */
        }
    }
    return 0;
}

void mv_monomial_lcm(const MVMonomial *a, const MVMonomial *b, int var_count, int *out_lcm) {
    for (int v = 0; v < var_count; v++) {
        out_lcm[v] = (a->exponents[v] > b->exponents[v]) ? a->exponents[v] : b->exponents[v];
    }
}

bool mv_monomial_divisible(const MVMonomial *m, const MVMonomial *d, int var_count) {
    for (int v = 0; v < var_count; v++) {
        if (m->exponents[v] < d->exponents[v])
            return false;
    }
    return true;
}

bool mv_monomial_divisible_lcm(const MVMonomial *d, const int *lcm_exp, int var_count) {
    for (int v = 0; v < var_count; v++) {
        if (d->exponents[v] > lcm_exp[v])
            return false;
    }
    return true;
}