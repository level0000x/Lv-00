#ifndef LV00_RATIONAL_H
#define LV00_RATIONAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <gmp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include "lv00/lv00_utils.h"

/* ========================================================================
 * Lv00Rational 结构体定义
 * ======================================================================== */

typedef struct Lv00Rational {
    mpz_t num;
    mpz_t den;
} Lv00Rational;

/* ========================================================================
 * 生命周期管理
 * ======================================================================== */

Lv00Rational *lv00_rational_create(void);
Lv00Rational *lv00_rational_create_from_mpz(const mpz_t num, const mpz_t den);
Lv00Rational *lv00_rational_create_from_si(long num, unsigned long den);
Lv00Rational *lv00_rational_create_from_i64(int64_t num, uint64_t den);
Lv00Rational *lv00_rational_clone(const Lv00Rational *src);
void          lv00_rational_destroy(Lv00Rational **r);

/* ========================================================================
 * 赋值操作
 * ======================================================================== */

void lv00_rational_set(Lv00Rational *dst, const Lv00Rational *src);
void lv00_rational_set_zero(Lv00Rational *r);
void lv00_rational_set_one(Lv00Rational *r);
int lv00_rational_set_mpz(Lv00Rational *r, const mpz_t num, const mpz_t den);

/* ========================================================================
 * 规范化
 * ======================================================================== */

void lv00_rational_simplify(Lv00Rational *r);

/* ========================================================================
 * 算术运算（返回新分配的有理数）
 * ======================================================================== */

Lv00Rational *lv00_rational_add(const Lv00Rational *a, const Lv00Rational *b);
Lv00Rational *lv00_rational_sub(const Lv00Rational *a, const Lv00Rational *b);
Lv00Rational *lv00_rational_mul(const Lv00Rational *a, const Lv00Rational *b);
Lv00Rational *lv00_rational_div(const Lv00Rational *a, const Lv00Rational *b);
Lv00Rational *lv00_rational_neg(const Lv00Rational *a);
Lv00Rational *lv00_rational_inv(const Lv00Rational *a);
Lv00Rational *lv00_rational_abs(const Lv00Rational *a);

/* ========================================================================
 * 原地算术运算
 * ======================================================================== */

void lv00_rational_add_inplace(Lv00Rational *a, const Lv00Rational *b);
void lv00_rational_sub_inplace(Lv00Rational *a, const Lv00Rational *b);
void lv00_rational_mul_inplace(Lv00Rational *a, const Lv00Rational *b);
int lv00_rational_div_inplace(Lv00Rational *a, const Lv00Rational *b);
void lv00_rational_neg_inplace(Lv00Rational *a);

/* ========================================================================
 * 比较操作
 * ======================================================================== */

int  lv00_rational_cmp(const Lv00Rational *a, const Lv00Rational *b);
bool lv00_rational_equal(const Lv00Rational *a, const Lv00Rational *b);
bool lv00_rational_is_zero(const Lv00Rational *a);
bool lv00_rational_is_one(const Lv00Rational *a);
bool lv00_rational_is_integer(const Lv00Rational *a);
int  lv00_rational_sgn(const Lv00Rational *a);

/* ========================================================================
 * 与 double 的转换
 * ======================================================================== */

int lv00_rational_to_double(const Lv00Rational *r, double *out_lossy, int *out_loss_bits);
int  lv00_rational_estimate_loss(const Lv00Rational *r);

/* ========================================================================
 * 防止分母溢出
 * ======================================================================== */

bool lv00_rational_mul_is_safe(const Lv00Rational *a, const Lv00Rational *b, uint64_t max_bits);
bool lv00_rational_den_is_safe(const mpz_t den);

/* ========================================================================
 * 格式化与调试
 * ======================================================================== */

char         *lv00_rational_to_string(const Lv00Rational *r);
Lv00Rational *lv00_rational_from_string(const char *s);

/* ========================================================================
 * 与 GMP mpq_t 的互操作
 * ======================================================================== */

Lv00Rational *lv00_rational_from_mpq(mpq_srcptr val);
void          lv00_rational_to_mpq(const Lv00Rational *r, mpq_t out);

/* ========================================================================
 * 兼容宏
 * ======================================================================== */

#ifndef SAFE_FREE_STR
#define SAFE_FREE_STR(p) do { if (p) { lv00_free((void**)&(p)); } } while(0)
#endif

#ifdef __cplusplus
}
#endif
#endif
