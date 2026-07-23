#ifndef lv_RATIONAL_H
#define lv_RATIONAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <gmp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include "lv/lv_utils.h"

/* ========================================================================
 * lvRational 结构体定义
 * ======================================================================== */

typedef struct lvRational {
    mpz_t num;
    mpz_t den;
} lvRational;

/* ========================================================================
 * 生命周期管理
 * ======================================================================== */

lvRational *lv_rational_create(void);
lvRational *lv_rational_create_from_mpz(const mpz_t num, const mpz_t den);
lvRational *lv_rational_create_from_si(long num, unsigned long den);
lvRational *lv_rational_create_from_i64(int64_t num, uint64_t den);
lvRational *lv_rational_clone(const lvRational *src);
void          lv_rational_destroy(lvRational **r);

/* ========================================================================
 * 赋值操作
 * ======================================================================== */

void lv_rational_set(lvRational *dst, const lvRational *src);
void lv_rational_set_zero(lvRational *r);
void lv_rational_set_one(lvRational *r);
bool lv_rational_set_mpz(lvRational *r, const mpz_t num, const mpz_t den);

/* ========================================================================
 * 规范化
 * ======================================================================== */

void lv_rational_simplify(lvRational *r);

/* ========================================================================
 * 算术运算（返回新分配的有理数）
 * ======================================================================== */

lvRational *lv_rational_add(const lvRational *a, const lvRational *b);
lvRational *lv_rational_sub(const lvRational *a, const lvRational *b);
lvRational *lv_rational_mul(const lvRational *a, const lvRational *b);
lvRational *lv_rational_div(const lvRational *a, const lvRational *b);
lvRational *lv_rational_neg(const lvRational *a);
lvRational *lv_rational_inv(const lvRational *a);
lvRational *lv_rational_abs(const lvRational *a);

/* ========================================================================
 * 原地算术运算
 * ======================================================================== */

void lv_rational_add_inplace(lvRational *a, const lvRational *b);
void lv_rational_sub_inplace(lvRational *a, const lvRational *b);
void lv_rational_mul_inplace(lvRational *a, const lvRational *b);
bool lv_rational_div_inplace(lvRational *a, const lvRational *b);
void lv_rational_neg_inplace(lvRational *a);

/* ========================================================================
 * 比较操作
 * ======================================================================== */

int  lv_rational_cmp(const lvRational *a, const lvRational *b);
bool lv_rational_equal(const lvRational *a, const lvRational *b);
bool lv_rational_is_zero(const lvRational *a);
bool lv_rational_is_one(const lvRational *a);
bool lv_rational_is_integer(const lvRational *a);
int  lv_rational_sgn(const lvRational *a);

/* ========================================================================
 * 与 double 的转换
 * ======================================================================== */

bool lv_rational_to_double(const lvRational *r, double *out_lossy, int *out_loss_bits);
int  lv_rational_estimate_loss(const lvRational *r);

/* ========================================================================
 * 防止分母溢出
 * ======================================================================== */

bool lv_rational_mul_is_safe(const lvRational *a, const lvRational *b, uint64_t max_bits);
bool lv_rational_den_is_safe(const mpz_t den);

/* ========================================================================
 * 格式化与调试
 * ======================================================================== */

char         *lv_rational_to_string(const lvRational *r);
lvRational *lv_rational_from_string(const char *s);

/* ========================================================================
 * 与 GMP mpq_t 的互操作
 * ======================================================================== */

lvRational *lv_rational_from_mpq(mpq_srcptr val);
void          lv_rational_to_mpq(const lvRational *r, mpq_t out);

/* ========================================================================
 * 兼容宏
 * ======================================================================== */

#ifndef SAFE_FREE_STR
#define SAFE_FREE_STR(p) do { if (p) { lv_free((void**)&(p)); } } while(0)
#endif

#ifdef __cplusplus
}
#endif
#endif
