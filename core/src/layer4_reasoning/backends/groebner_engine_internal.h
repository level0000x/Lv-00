/**
 * @file groebner_engine_internal.h
 * @brief Groebner 引擎内部共享声明（groebner_mono.c / groebner_poly.c 与 groebner_engine.c 共用）
 *
 * @details 从 groebner_engine.c 拆分单项式操作与多项式运算段后，
 *          将共享宏与跨文件函数声明集中于此，避免重复定义。
 */

#ifndef lv_GROEBNER_ENGINE_INTERNAL_H
#define lv_GROEBNER_ENGINE_INTERNAL_H

#include <stdbool.h>

#include "groebner_engine.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 数值零阈值 */
#define GROEBNER_ZERO_THRESHOLD 1e-15

/** @brief 多项式初始项容量 */
#define GROEBNER_POLY_INIT_CAPACITY 8

/** @brief 多项式扩容因子 */
#define GROEBNER_POLY_GROW_FACTOR 2

/* ================================================================
 *  单项式操作（groebner_mono.c 与 groebner_engine.c 共享）
 * ================================================================ */
int mono_compare(const lvPolynomialRing *ring, const int *powers_a, const int *powers_b);
int mono_total_degree(const int *powers, int var_count);
void mono_lcm(const lvPolynomialRing *ring, const int *powers_a, const int *powers_b, int *lcm_out);
bool mono_divides(const lvPolynomialRing *ring, const int *powers_d, const int *powers_e);
void mono_divide(const lvPolynomialRing *ring, const int *powers_dividend, const int *powers_divisor,
                 int *quotient_out);
bool mono_is_coprime(const lvPolynomialRing *ring, const int *powers_a, const int *powers_b);
void mono_copy(int *dest, const int *src, int var_count);

/* ================================================================
 *  多项式内部运算（groebner_poly.c 与 groebner_engine.c 共享）
 * ================================================================ */
int poly_sort_terms(lvPolynomial *poly, const lvPolynomialRing *ring);
lvPolynomial *poly_internal_create(const lvPolynomialRing *ring, int capacity, const char *label);
void poly_internal_destroy(lvPolynomial *poly);
bool poly_ensure_capacity(lvPolynomial *poly, int needed);
bool poly_ensure_capacity_ex(lvPolynomial *poly, int needed, int var_count);
lvPolynomial *poly_internal_copy(const lvPolynomial *src, const lvPolynomialRing *ring);
lvPolynomial *poly_internal_add(const lvPolynomial *f, const lvPolynomial *g, const lvPolynomialRing *ring);
lvPolynomial *poly_internal_multiply(const lvPolynomial *f, const lvPolynomial *g, const lvPolynomialRing *ring);
lvPolynomial *poly_internal_substitute(const lvPolynomial *f, int var_index, const lvPolynomial *subst,
                                       const lvPolynomialRing *ring);
lvPolynomial *poly_internal_s_polynomial(const lvPolynomial *f, const lvPolynomial *g,
                                         const lvPolynomialRing *ring);
lvPolynomial *poly_internal_reduce(const lvPolynomial *p, lvPolynomial **basis, int basis_count,
                                   const lvPolynomialRing *ring);
bool poly_internal_is_zero(const lvPolynomial *poly);
int poly_internal_total_degree(const lvPolynomial *poly, int var_count);
void poly_internal_scale(lvPolynomial *poly, double scalar);
int poly_leading_term(const lvPolynomial *poly, const lvPolynomialRing *ring, int *lt_out, double *lc_out);

/* ================================================================
 *  通用辅助
 * ================================================================ */
char *groebner_strdup_safe(const char *src);

#ifdef __cplusplus
}
#endif

#endif /* lv_GROEBNER_ENGINE_INTERNAL_H */
