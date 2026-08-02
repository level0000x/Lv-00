/**
 * @file algebraic_number_internal.h
 * @brief 代数数内部整数工具共享声明
 *
 * @details 供 algebraic_number_util.c / algebraic_number_rational.c /
 *          algebraic_number_quadratic.c / algebraic_number_interval.c /
 *          algebraic_number_poly.c / algebraic_number_io.c 共享。
 */

#ifndef lv_ALGEBRAIC_NUMBER_INTERNAL_H
#define lv_ALGEBRAIC_NUMBER_INTERNAL_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 内部整数工具（algebraic_number_util.c 实现，跨模块共享） */
int64_t alg_gcd(int64_t a, int64_t b);
int64_t alg_lcm(int64_t a, int64_t b);
bool alg_mul_overflow(int64_t a, int64_t b, int64_t *result);
bool alg_add_overflow(int64_t a, int64_t b, int64_t *result);
bool alg_sub_overflow(int64_t a, int64_t b, int64_t *result);
void lv_alg_rational_simplify(int64_t *p, int64_t *q);
bool alg_is_perfect_square(int64_t n);
int64_t alg_isqrt(int64_t n);

#ifdef __cplusplus
}
#endif

#endif /* lv_ALGEBRAIC_NUMBER_INTERNAL_H */
