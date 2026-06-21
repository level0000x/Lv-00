/**
 * @file rational.h
 * @brief 精确有理数类型 —— 基于 GMP mpz_t 的类型安全封装
 *
 * 所有 Lv-00 数值计算使用精确有理数，绝无浮点数舍入误差。
 * 本文件定义 Rational 类型及其完整 API。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#ifndef LV00_RATIONAL_H
#define LV00_RATIONAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <gmp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

/**
 * @brief 精确有理数类型（GMP mpz_t 分子/分母封装）
 *
 * 用法示例:
 *   Rational *r = rational_create(1, 2);  // r = 1/2
 *   Rational *s = rational_add(r, r);     // s = 1
 *   int cmp = rational_compare(r, s);     // cmp < 0
 *   rational_destroy(r);
 *   rational_destroy(s);
 */
typedef struct Rational {
    mpz_t num;
    mpz_t den;
} Rational;

/* Alias for compatibility with lv00_ prefix API */
typedef Rational Lv00Rational;

/* ============================================================
 * 生命周期
 * ============================================================ */

/** 创建有理数 num/den（自动约分）。若 den=0 返回 NULL。 */
Rational *rational_create(int num, int den);

/** 从字符串解析（支持 "3/4", "5", "-7/8"）。出错返回 NULL。 */
Rational *rational_parse(const char *str);

/** 深拷贝。 */
Rational *rational_copy(const Rational *r);

/** 释放内存。 */
void rational_destroy(Rational *r);

/** 化简（约分至最简形式）。 */
void rational_simplify(Rational *r);

/* ============================================================
 * 算术
 * ============================================================ */

Rational *rational_add(const Rational *r, const Rational *s);
Rational *rational_subtract(const Rational *r, const Rational *s);
Rational *rational_multiply(const Rational *r, const Rational *s);
Rational *rational_divide(const Rational *r, const Rational *s);
Rational *rational_negate(const Rational *r);
Rational *rational_inverse(const Rational *r);
Rational *rational_abs(const Rational *r);

/* ============================================================
 * 比较与谓词
 * ============================================================ */

int rational_compare(const Rational *r, const Rational *s);
bool rational_is_zero(const Rational *r);
bool rational_is_positive(const Rational *r);
bool rational_is_negative(const Rational *r);
bool rational_is_integer(const Rational *r);
int rational_sgn(const Rational *r);

/* ============================================================
 * 转换与序列化
 * ============================================================ */

double rational_to_double(const Rational *r);
char *rational_to_string(const Rational *r);
char *rational_serialize(const Rational *r);

/* ============================================================
 * Setters
 * ============================================================ */

void rational_set_one(Rational *r);
void rational_set_zero(Rational *r);

/* ============================================================
 * SAFE_FREE_STR helper (define only if not already)
 * ============================================================ */

/* ============================================================
 * Lv00Rational — 前缀版本 inline wrappers
 * ============================================================ */

static inline Lv00Rational *lv00_rational_create(void) {
    Rational *r = (Rational*)malloc(sizeof(Rational));
    if (r) { mpz_init(r->num); mpz_init(r->den); mpz_set_ui(r->num, 0); mpz_set_ui(r->den, 1); }
    return r;
}
static inline Lv00Rational *lv00_rational_create_from_si(int num, int den) {
    Rational *r = (Rational*)malloc(sizeof(Rational));
    if (r) { mpz_init_set_si(r->num, num); mpz_init_set_si(r->den, den); rational_simplify(r); }
    return r;
}
static inline Lv00Rational *lv00_rational_clone(const Lv00Rational *r) {
    return rational_copy(r);
}
static inline void lv00_rational_destroy(Lv00Rational **rp) {
    if (rp && *rp) { rational_destroy(*rp); free(*rp); *rp = NULL; }
}
static inline void lv00_rational_set_one(Lv00Rational *r) {
    rational_set_one(r);
}
static inline void lv00_rational_set_zero(Lv00Rational *r) {
    rational_set_zero(r);
}
static inline void lv00_rational_simplify(Lv00Rational *r) {
    rational_simplify(r);
}
static inline char *lv00_rational_to_string(const Lv00Rational *r) {
    return rational_to_string(r);
}
static inline bool lv00_rational_to_double(const Lv00Rational *r, double *out, int *loss_bits) {
    *out = rational_to_double(r);
    if (loss_bits) *loss_bits = 0;
    return true;
}
static inline int lv00_rational_cmp(const Lv00Rational *a, const Lv00Rational *b) {
    return rational_compare(a, b);
}
static inline Lv00Rational *lv00_rational_add(const Lv00Rational *a, const Lv00Rational *b) {
    return rational_add(a, b);
}
static inline Lv00Rational *lv00_rational_sub(const Lv00Rational *a, const Lv00Rational *b) {
    return rational_subtract(a, b);
}
static inline Lv00Rational *lv00_rational_mul(const Lv00Rational *a, const Lv00Rational *b) {
    return rational_multiply(a, b);
}
static inline Lv00Rational *lv00_rational_div(const Lv00Rational *a, const Lv00Rational *b) {
    return rational_divide(a, b);
}
static inline Lv00Rational *lv00_rational_neg(const Lv00Rational *r) {
    return rational_negate(r);
}
static inline Lv00Rational *lv00_rational_inv(const Lv00Rational *r) {
    return rational_inverse(r);
}
static inline Lv00Rational *lv00_rational_abs(const Lv00Rational *r) {
    return rational_abs(r);
}
static inline int lv00_rational_is_zero(const Lv00Rational *r) {
    return rational_is_zero(r);
}
static inline int lv00_rational_is_one(const Lv00Rational *r) {
    return mpz_cmp_ui(r->den, 1) == 0 && mpz_cmp_ui(r->num, 1) == 0;
}
static inline int lv00_rational_is_integer(const Lv00Rational *r) {
    return rational_is_integer(r);
}
static inline int lv00_rational_sgn(const Lv00Rational *r) {
    return rational_sgn(r);
}
static inline bool lv00_rational_add_inplace(Lv00Rational *a, const Lv00Rational *b) {
    Rational *sum = rational_add(a, b); if (!sum) return false;
    mpz_set(a->num, sum->num); mpz_set(a->den, sum->den);
    rational_destroy(sum); return true;
}
static inline bool lv00_rational_sub_inplace(Lv00Rational *a, const Lv00Rational *b) {
    Rational *diff = rational_subtract(a, b); if (!diff) return false;
    mpz_set(a->num, diff->num); mpz_set(a->den, diff->den);
    rational_destroy(diff); return true;
}
static inline bool lv00_rational_mul_inplace(Lv00Rational *a, const Lv00Rational *b) {
    Rational *prod = rational_multiply(a, b); if (!prod) return false;
    mpz_set(a->num, prod->num); mpz_set(a->den, prod->den);
    rational_destroy(prod); return true;
}
static inline bool lv00_rational_div_inplace(Lv00Rational *a, const Lv00Rational *b) {
    Rational *quot = rational_divide(a, b); if (!quot) return false;
    mpz_set(a->num, quot->num); mpz_set(a->den, quot->den);
    rational_destroy(quot); return true;
}
static inline bool lv00_rational_neg_inplace(Lv00Rational *a) {
    Rational *neg = rational_negate(a); if (!neg) return false;
    mpz_set(a->num, neg->num); mpz_set(a->den, neg->den);
    rational_destroy(neg); return true;
}

#ifdef __cplusplus
}
#endif

#endif /* LV00_RATIONAL_H */
