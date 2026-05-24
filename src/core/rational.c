/**
 * @file rational.c
 * @brief 精确有理数运算实现 —— 基于 GMP mpz_t
 *
 * @details Lv00Rational 的有理数运算的完整实现。
 *          所有运算保证精确，无浮点舍入。与 symbolic_coord.h 中的
 *          Rational* (mpq_t 型) 可互操作。
 *
 * @author Lv-00 Project
 * @version v1.0.0
 * @date 2026-05-24
 */

#include "rational.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv00_utils.h"

/* 用于 lv00_rational_den_is_safe 的安全比特阈值 */
#define RATIONAL_SAFE_BITS_DEFAULT 65536 /* 2^16 比特 */

/* 内部辅助: 计算 mpz 的近似比特数 */
static uint64_t mpz_bit_size(const mpz_t x) {
    if (mpz_sgn(x) == 0)
        return 0;
    return (uint64_t) mpz_sizeinbase(x, 2);
}

/* ========================================================================
 * 生命周期管理
 * ======================================================================== */

Lv00Rational *lv00_rational_create(void) {
    Lv00Rational *r = (Lv00Rational *) lv00_malloc(sizeof(Lv00Rational));
    if (!r)
        return NULL;
    mpz_init_set_si(r->num, 0);
    mpz_init_set_si(r->den, 1);
    return r;
}

Lv00Rational *lv00_rational_create_from_mpz(const mpz_t num, const mpz_t den) {
    if (mpz_sgn(den) == 0)
        return NULL; /* 分母不得为 0 */

    Lv00Rational *r = (Lv00Rational *) lv00_malloc(sizeof(Lv00Rational));
    if (!r)
        return NULL;

    mpz_init(r->num);
    mpz_init(r->den);

    mpz_set(r->num, num);
    mpz_set(r->den, den);

    /* 规范化: 化简 + 确保分母为正 */
    lv00_rational_simplify(r);

    return r;
}

Lv00Rational *lv00_rational_create_from_si(long num, unsigned long den) {
    if (den == 0)
        return NULL;

    Lv00Rational *r = (Lv00Rational *) lv00_malloc(sizeof(Lv00Rational));
    if (!r)
        return NULL;

    mpz_init(r->num);
    mpz_init(r->den);
    mpz_set_si(r->num, num);
    mpz_set_ui(r->den, den);

    lv00_rational_simplify(r);
    return r;
}

Lv00Rational *lv00_rational_create_from_i64(int64_t num, uint64_t den) {
    if (den == 0)
        return NULL;

    Lv00Rational *r = (Lv00Rational *) lv00_malloc(sizeof(Lv00Rational));
    if (!r)
        return NULL;

    mpz_init(r->num);
    mpz_init(r->den);

    /* 将 int64_t 转为 mpz_t */
    if (num >= 0) {
        mpz_set_ui(r->num, (unsigned long) num);
    } else {
        /* num < 0: 取绝对值后设负号 */
        uint64_t abs_num = (uint64_t)(-num);
        mpz_set_ui(r->num, (unsigned long) abs_num);
        mpz_neg(r->num, r->num);
    }
    mpz_set_ui(r->den, (unsigned long) den);

    lv00_rational_simplify(r);
    return r;
}

Lv00Rational *lv00_rational_clone(const Lv00Rational *src) {
    if (!src)
        return NULL;

    Lv00Rational *r = (Lv00Rational *) lv00_malloc(sizeof(Lv00Rational));
    if (!r)
        return NULL;

    mpz_init_set(r->num, src->num);
    mpz_init_set(r->den, src->den);

    return r;
}

void lv00_rational_destroy(Lv00Rational **r) {
    if (r && *r) {
        mpz_clear((*r)->num);
        mpz_clear((*r)->den);
        lv00_free((void **) r);
    }
}

/* ========================================================================
 * 赋值操作
 * ======================================================================== */

void lv00_rational_set(Lv00Rational *dst, const Lv00Rational *src) {
    if (!dst || !src)
        return;
    mpz_set(dst->num, src->num);
    mpz_set(dst->den, src->den);
}

void lv00_rational_set_zero(Lv00Rational *r) {
    if (!r)
        return;
    mpz_set_si(r->num, 0);
    mpz_set_si(r->den, 1);
}

void lv00_rational_set_one(Lv00Rational *r) {
    if (!r)
        return;
    mpz_set_si(r->num, 1);
    mpz_set_si(r->den, 1);
}

bool lv00_rational_set_mpz(Lv00Rational *r, const mpz_t num, const mpz_t den) {
    if (!r)
        return false;
    if (mpz_sgn(den) == 0)
        return false;

    mpz_set(r->num, num);
    mpz_set(r->den, den);
    lv00_rational_simplify(r);
    return true;
}

/* ========================================================================
 * 规范化
 * ======================================================================== */

void lv00_rational_simplify(Lv00Rational *r) {
    if (!r)
        return;

    /* 处理分子为 0 的情况: 立即设为 0/1 */
    if (mpz_sgn(r->num) == 0) {
        mpz_set_si(r->den, 1);
        return;
    }

    /* 确保分母为正 */
    if (mpz_sgn(r->den) < 0) {
        mpz_neg(r->num, r->num);
        mpz_neg(r->den, r->den);
    }

    /* 计算 gcd 并化简 */
    mpz_t g;
    mpz_init(g);
    mpz_gcd(g, r->num, r->den);

    /* 仅当 gcd > 1 时才执行化简 */
    if (mpz_cmp_si(g, 1) > 0) {
        mpz_divexact(r->num, r->num, g);
        mpz_divexact(r->den, r->den, g);
    }

    mpz_clear(g);
}

/* ========================================================================
 * 算术运算
 * ======================================================================== */

Lv00Rational *lv00_rational_add(const Lv00Rational *a, const Lv00Rational *b) {
    if (!a || !b)
        return NULL;

    Lv00Rational *r = lv00_rational_create();
    if (!r)
        return NULL;

    /* result = (a.num*b.den + b.num*a.den) / (a.den * b.den) */
    mpz_t t1, t2;
    mpz_init(t1);
    mpz_init(t2);

    mpz_mul(t1, a->num, b->den); /* a.num * b.den */
    mpz_mul(t2, b->num, a->den); /* b.num * a.den */
    mpz_add(r->num, t1, t2);

    mpz_mul(r->den, a->den, b->den);

    mpz_clear(t1);
    mpz_clear(t2);

    lv00_rational_simplify(r);
    return r;
}

Lv00Rational *lv00_rational_sub(const Lv00Rational *a, const Lv00Rational *b) {
    if (!a || !b)
        return NULL;

    Lv00Rational *r = lv00_rational_create();
    if (!r)
        return NULL;

    /* result = (a.num*b.den - b.num*a.den) / (a.den * b.den) */
    mpz_t t1, t2;
    mpz_init(t1);
    mpz_init(t2);

    mpz_mul(t1, a->num, b->den);
    mpz_mul(t2, b->num, a->den);
    mpz_sub(r->num, t1, t2);

    mpz_mul(r->den, a->den, b->den);

    mpz_clear(t1);
    mpz_clear(t2);

    lv00_rational_simplify(r);
    return r;
}

Lv00Rational *lv00_rational_mul(const Lv00Rational *a, const Lv00Rational *b) {
    if (!a || !b)
        return NULL;

    Lv00Rational *r = lv00_rational_create();
    if (!r)
        return NULL;

    mpz_mul(r->num, a->num, b->num);
    mpz_mul(r->den, a->den, b->den);

    lv00_rational_simplify(r);
    return r;
}

Lv00Rational *lv00_rational_div(const Lv00Rational *a, const Lv00Rational *b) {
    if (!a || !b)
        return NULL;
    if (lv00_rational_is_zero(b))
        return NULL;

    Lv00Rational *r = lv00_rational_create();
    if (!r)
        return NULL;

    mpz_mul(r->num, a->num, b->den);
    mpz_mul(r->den, a->den, b->num);

    lv00_rational_simplify(r);
    return r;
}

Lv00Rational *lv00_rational_neg(const Lv00Rational *a) {
    if (!a)
        return NULL;

    Lv00Rational *r = lv00_rational_clone(a);
    if (r) {
        mpz_neg(r->num, r->num);
    }
    return r;
}

Lv00Rational *lv00_rational_inv(const Lv00Rational *a) {
    if (!a)
        return NULL;
    if (lv00_rational_is_zero(a))
        return NULL;

    Lv00Rational *r = lv00_rational_create();
    if (!r)
        return NULL;

    mpz_set(r->num, a->den);
    mpz_set(r->den, a->num);

    /* 确保分母为正 */
    if (mpz_sgn(r->den) < 0) {
        mpz_neg(r->num, r->num);
        mpz_neg(r->den, r->den);
    }

    return r;
}

Lv00Rational *lv00_rational_abs(const Lv00Rational *a) {
    if (!a)
        return NULL;

    Lv00Rational *r = lv00_rational_clone(a);
    if (r) {
        if (mpz_sgn(r->num) < 0) {
            mpz_neg(r->num, r->num);
        }
    }
    return r;
}

/* ========================================================================
 * 原地算术运算
 * ======================================================================== */

void lv00_rational_add_inplace(Lv00Rational *a, const Lv00Rational *b) {
    if (!a || !b)
        return;

    /* a = a + b:
     * a.num = a.num*b.den + b.num*a.den
     * a.den = a.den * b.den
     */
    mpz_t t1, t2;
    mpz_init(t1);
    mpz_init(t2);

    mpz_mul(t1, a->num, b->den);
    mpz_mul(t2, b->num, a->den);
    mpz_add(a->num, t1, t2);

    mpz_mul(a->den, a->den, b->den);

    mpz_clear(t1);
    mpz_clear(t2);

    lv00_rational_simplify(a);
}

void lv00_rational_sub_inplace(Lv00Rational *a, const Lv00Rational *b) {
    if (!a || !b)
        return;

    mpz_t t1, t2;
    mpz_init(t1);
    mpz_init(t2);

    mpz_mul(t1, a->num, b->den);
    mpz_mul(t2, b->num, a->den);
    mpz_sub(a->num, t1, t2);

    mpz_mul(a->den, a->den, b->den);

    mpz_clear(t1);
    mpz_clear(t2);

    lv00_rational_simplify(a);
}

void lv00_rational_mul_inplace(Lv00Rational *a, const Lv00Rational *b) {
    if (!a || !b)
        return;

    mpz_mul(a->num, a->num, b->num);
    mpz_mul(a->den, a->den, b->den);

    lv00_rational_simplify(a);
}

bool lv00_rational_div_inplace(Lv00Rational *a, const Lv00Rational *b) {
    if (!a || !b)
        return false;
    if (lv00_rational_is_zero(b))
        return false;

    mpz_mul(a->num, a->num, b->den);
    mpz_mul(a->den, a->den, b->num);

    lv00_rational_simplify(a);
    return true;
}

void lv00_rational_neg_inplace(Lv00Rational *a) {
    if (!a)
        return;
    mpz_neg(a->num, a->num);
}

/* ========================================================================
 * 比较操作
 * ======================================================================== */

int lv00_rational_cmp(const Lv00Rational *a, const Lv00Rational *b) {
    if (!a || !b)
        return 0;

    /* 交叉乘法比较: a.num/a.den ? b.num/b.den <=> a.num*b.den ? b.num*a.den */
    mpz_t left, right;
    mpz_init(left);
    mpz_init(right);

    mpz_mul(left, a->num, b->den);
    mpz_mul(right, b->num, a->den);

    int result = mpz_cmp(left, right);

    mpz_clear(left);
    mpz_clear(right);

    return result;
}

bool lv00_rational_equal(const Lv00Rational *a, const Lv00Rational *b) {
    return lv00_rational_cmp(a, b) == 0;
}

bool lv00_rational_is_zero(const Lv00Rational *a) {
    if (!a)
        return true;
    return mpz_sgn(a->num) == 0;
}

bool lv00_rational_is_one(const Lv00Rational *a) {
    if (!a)
        return false;
    return mpz_cmp_si(a->num, 1) == 0 && mpz_cmp_si(a->den, 1) == 0;
}

bool lv00_rational_is_integer(const Lv00Rational *a) {
    if (!a)
        return false;
    return mpz_cmp_si(a->den, 1) == 0;
}

int lv00_rational_sgn(const Lv00Rational *a) {
    if (!a)
        return 0;
    return mpz_sgn(a->num);
}

/* ========================================================================
 * 与 double 的转换（显式标注精度损失）
 * ======================================================================== */

bool lv00_rational_to_double(const Lv00Rational *r, double *out_lossy, int *out_loss_bits) {
    if (!r || !out_lossy)
        return false;

    /* 使用 GMP 的 mpz_get_d 获取最佳近似 */
    double num_d = mpz_get_d(r->num);
    double den_d = mpz_get_d(r->den);

    if (den_d == 0.0) {
        /* 分母为 0（不应发生，但此处防御） */
        return false;
    }

    *out_lossy = num_d / den_d;

    if (out_loss_bits) {
        *out_loss_bits = lv00_rational_estimate_loss(r);
    }

    return true;
}

int lv00_rational_estimate_loss(const Lv00Rational *r) {
    if (!r || lv00_rational_is_zero(r))
        return 0;

    /* 预估精度损失：有效位的比特数是否超出 double 的 53 位尾数 */
    uint64_t num_bits = mpz_bit_size(r->num);
    uint64_t den_bits = mpz_bit_size(r->den);
    uint64_t total_bits = (num_bits > den_bits) ? num_bits : den_bits;

    /* double 的尾数为 53 位（含隐藏位） */
    if (total_bits <= 53) {
        return 0; /* double 可精确表示 */
    }

    /* 否则估算损失 */
    int loss = (int)(total_bits - 53);
    return loss > 0 ? loss : 0;
}

/* ========================================================================
 * 防止分母溢出
 * ======================================================================== */

bool lv00_rational_mul_is_safe(const Lv00Rational *a, const Lv00Rational *b, uint64_t max_bits) {
    if (!a || !b)
        return false;

    if (max_bits == 0) {
        max_bits = RATIONAL_SAFE_BITS_DEFAULT;
    }

    /* 乘法后分子/分母的比特数约为各操作数比特数之和 */
    uint64_t num_bits_a = mpz_bit_size(a->num);
    uint64_t den_bits_a = mpz_bit_size(a->den);
    uint64_t num_bits_b = mpz_bit_size(b->num);
    uint64_t den_bits_b = mpz_bit_size(b->den);

    /* 检查分子和分母各自是否会溢出阈值 */
    if (num_bits_a + num_bits_b > max_bits)
        return false;
    if (den_bits_a + den_bits_b > max_bits)
        return false;

    return true;
}

bool lv00_rational_den_is_safe(const mpz_t den) {
    if (!den)
        return false;
    uint64_t bits = mpz_bit_size(den);
    return bits <= RATIONAL_SAFE_BITS_DEFAULT;
}

/* ========================================================================
 * 格式化与调试
 * ======================================================================== */

char *lv00_rational_to_string(const Lv00Rational *r) {
    if (!r)
        return NULL;

    /* 检查分母是否为 1 —— 即使简单整数也统一走 lv00_malloc */
    if (mpz_cmp_si(r->den, 1) == 0) {
        char *gmp_str = mpz_get_str(NULL, 10, r->num);
        if (!gmp_str) return NULL;
        size_t slen = strlen(gmp_str);
        char *result = (char *) lv00_malloc(slen + 1);
        if (!result) { free(gmp_str); return NULL; }
        memcpy(result, gmp_str, slen + 1);
        free(gmp_str);  /* GMP分配，用标准free */
        return result;
    }

    /* 输出 "num/den" 格式 */
    char *num_str = mpz_get_str(NULL, 10, r->num);
    char *den_str = mpz_get_str(NULL, 10, r->den);

    if (!num_str || !den_str) {
        free(num_str); num_str = NULL;  /* GMP分配，用标准free */
        free(den_str); den_str = NULL;  /* GMP分配，用标准free */
        return NULL;
    }

    size_t len = strlen(num_str) + strlen(den_str) + 2; /* num + '/' + den + '\0' */
    char *result = (char *) lv00_malloc(len);
    if (!result) {
        free(num_str); num_str = NULL;  /* GMP分配，用标准free */
        free(den_str); den_str = NULL;  /* GMP分配，用标准free */
        return NULL;
    }

    snprintf(result, len, "%s/%s", num_str, den_str);

    free(num_str);   /* GMP分配，用标准free */
    free(den_str);   /* GMP分配，用标准free */

    return result;
}

Lv00Rational *lv00_rational_from_string(const char *s) {
    if (!s || !*s)
        return NULL;

    const char *slash = strchr(s, '/');

    if (slash) {
        /* 格式: "num/den" */
        size_t num_len = (size_t)(slash - s);
        char *num_str = (char *) lv00_malloc(num_len + 1);
        if (!num_str)
            return NULL;
        memcpy(num_str, s, num_len);
        num_str[num_len] = '\0';

        mpz_t num, den;
        mpz_init(num);
        mpz_init(den);

        if (mpz_set_str(num, num_str, 10) != 0) {
            lv00_free((void **) &num_str);
            mpz_clear(num);
            mpz_clear(den);
            return NULL;
        }

        if (mpz_set_str(den, slash + 1, 10) != 0) {
            lv00_free((void **) &num_str);
            mpz_clear(num);
            mpz_clear(den);
            return NULL;
        }

        lv00_free((void **) &num_str);

        if (mpz_sgn(den) == 0) {
            mpz_clear(num);
            mpz_clear(den);
            return NULL;
        }

        Lv00Rational *r = lv00_rational_create_from_mpz(num, den);
        mpz_clear(num);
        mpz_clear(den);
        return r;
    } else {
        /* 格式: "integer" */
        mpz_t num, den;
        mpz_init(num);
        mpz_init(den);

        if (mpz_set_str(num, s, 10) != 0) {
            mpz_clear(num);
            mpz_clear(den);
            return NULL;
        }

        mpz_set_si(den, 1);

        Lv00Rational *r = lv00_rational_create_from_mpz(num, den);
        mpz_clear(num);
        mpz_clear(den);
        return r;
    }
}

/* ========================================================================
 * 与现有 Rational* (mpq_t) 类型的互操作
 * ======================================================================== */

Lv00Rational *lv00_rational_from_mpq(mpq_srcptr val) {
    if (!val)
        return NULL;

    mpz_t num, den;
    mpz_init(num);
    mpz_init(den);

    mpz_set(num, mpq_numref(val));
    mpz_set(den, mpq_denref(val));

    Lv00Rational *r = lv00_rational_create_from_mpz(num, den);

    mpz_clear(num);
    mpz_clear(den);

    return r;
}

void lv00_rational_to_mpq(const Lv00Rational *r, mpq_t out) {
    if (!r || !out)
        return;

    mpq_set_num(out, r->num);
    mpq_set_den(out, r->den);
    mpq_canonicalize(out);
}
