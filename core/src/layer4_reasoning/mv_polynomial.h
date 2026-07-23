/**
 * @file mv_polynomial.h
 * @brief 多变量多项式 — Groebner 基计算基础数据结构
 *
 * 从 solver.c 拆分出的独立模块。提供多变量单项式/多项式的
 * 创建、销毁、运算、排序等基础操作，用于 Buchberger 算法。
 *
 * 原位置: solver.c L4703-L4950
 */

#ifndef lv_MV_POLYNOMIAL_H
#define lv_MV_POLYNOMIAL_H

#include <gmp.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 多变量单项式: 用整数数组表示每个变量的幂次 */
typedef struct {
    int *exponents; /* 每个变量的幂次数组, 长度 = var_count */
    mpz_t coeff;    /* GMP 精确系数 */
} MVMonomial;

/* 多变量多项式 */
typedef struct {
    MVMonomial *terms; /* 单项式数组 */
    int term_count;    /* 单项式数量 */
    int capacity;      /* 分配容量 */
    int var_count;     /* 变量个数 */
} MVPolynomial;

/* ---- 生命周期 ---- */
void mv_poly_init(MVPolynomial *p, int var_count);
void mv_poly_clear(MVPolynomial *p);

/* ---- 项操作 ---- */
int mv_poly_add_term(MVPolynomial *p, const mpz_t coeff, const int *exponents);
void mv_poly_sort(MVPolynomial *p);
void mv_poly_remove_zeros(MVPolynomial *p);
void mv_poly_mul_monomial(MVPolynomial *result, const MVPolynomial *p, const int *mono_exp, const mpz_t mono_coeff,
                          int var_count);

/* ---- 算术 ---- */
void mv_poly_sub(MVPolynomial *result, const MVPolynomial *a, const MVPolynomial *b);
void mv_poly_copy(MVPolynomial *dst, const MVPolynomial *src);

/* ---- 查询 ---- */
bool mv_poly_is_zero(const MVPolynomial *p);
int mv_poly_leading_term(const MVPolynomial *p, MVMonomial *out);

/* ---- 单项式工具 ---- */
int mv_monomial_total_degree(const MVMonomial *m, int var_count);
int mv_monomial_compare_grlex(const MVMonomial *a, const MVMonomial *b, int var_count);
void mv_monomial_lcm(const MVMonomial *a, const MVMonomial *b, int var_count, int *out_lcm);
bool mv_monomial_divisible(const MVMonomial *m, const MVMonomial *d, int var_count);
bool mv_monomial_divisible_lcm(const MVMonomial *d, const int *lcm_exp, int var_count);

#ifdef __cplusplus
}
#endif

#endif /* lv_MV_POLYNOMIAL_H */