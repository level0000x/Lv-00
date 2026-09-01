/**
 * @file algebraic_number_rational.c
 * @brief 代数数域封装 —— 第一层：有理数域 Q
 *
 * @details 由 algebraic_number.c 按数域类型拆分而来。
 *          所有运算基于 int64_t，不依赖 GMP 等外部库。
 *
 * @version 3.5.0
 * @copyright Copyright (c) 2024-2026 Lv-00 Project
 */

#include "lv/algebraic_number.h"
#include "lv/lv_arith_safe.h" /* lv_POW_SQUARING（K2/F33 快速幂单骨架） */
#include "lv/lv_str_utils.h"
#include "lv/lv_strbuf.h"
#include "lv/lv_xmacro.h"

#include <gmp.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "algebraic_number_internal.h"
#include "lv/lv_internal.h"

/** @brief 设置错误码（若 err 非空） */
static void alg_set_error_rational(AlgRationalError *err, AlgRationalError code) {
    if (err)
        *err = code;
}

/* ============================================================
 * 第一层：有理数域 Q —— 实现
 * ============================================================ */

AlgRational lv_alg_rational_create(int64_t p, int64_t q, AlgRationalError *err) {
    AlgRational result;
    if (q == 0) {
        alg_set_error_rational(err, lv_alg_rational_ERR_ZERO_DEN);
        result.num = 0;
        result.den = 1;
        return result;
    }
    lv_alg_rational_simplify(&p, &q);
    alg_set_error_rational(err, lv_alg_rational_OK);
    result.num = p;
    result.den = q;
    return result;
}

AlgRational lv_alg_rational_zero(void) {
    AlgRational r;
    r.num = 0;
    r.den = 1;
    return r;
}

AlgRational lv_alg_rational_one(void) {
    AlgRational r;
    r.num = 1;
    r.den = 1;
    return r;
}

AlgRational lv_alg_rational_from_int(int64_t n) {
    AlgRational r;
    r.num = n;
    r.den = 1;
    return r;
}

AlgRational lv_alg_rational_add(const AlgRational *a, const AlgRational *b, AlgRationalError *err) {
    if (!a || !b) {
        alg_set_error_rational(err, lv_alg_rational_ERR_NULL);
        return lv_alg_rational_zero();
    }

    /* a.num/b.den + b.num/a.den = (a.num * b.den + b.num * a.den) / (a.den * b.den) */
    int64_t num1, num2, denom;
    if (alg_mul_overflow(a->num, b->den, &num1) || alg_mul_overflow(b->num, a->den, &num2) ||
        alg_add_overflow(num1, num2, &num1) || alg_mul_overflow(a->den, b->den, &denom)) {
        alg_set_error_rational(err, lv_alg_rational_ERR_OVERFLOW);
        return lv_alg_rational_zero();
    }

    alg_set_error_rational(err, lv_alg_rational_OK);
    return lv_alg_rational_create(num1, denom, NULL);
}

AlgRational lv_alg_rational_sub(const AlgRational *a, const AlgRational *b, AlgRationalError *err) {
    if (!a || !b) {
        alg_set_error_rational(err, lv_alg_rational_ERR_NULL);
        return lv_alg_rational_zero();
    }

    int64_t num1, num2, denom;
    if (alg_mul_overflow(a->num, b->den, &num1) || alg_mul_overflow(b->num, a->den, &num2) ||
        alg_sub_overflow(num1, num2, &num1) || alg_mul_overflow(a->den, b->den, &denom)) {
        alg_set_error_rational(err, lv_alg_rational_ERR_OVERFLOW);
        return lv_alg_rational_zero();
    }

    alg_set_error_rational(err, lv_alg_rational_OK);
    return lv_alg_rational_create(num1, denom, NULL);
}

AlgRational lv_alg_rational_mul(const AlgRational *a, const AlgRational *b, AlgRationalError *err) {
    if (!a || !b) {
        alg_set_error_rational(err, lv_alg_rational_ERR_NULL);
        return lv_alg_rational_zero();
    }

    /* 先约分再乘，减少溢出风险 */
    int64_t g1 = alg_gcd(a->num, b->den);
    int64_t g2 = alg_gcd(b->num, a->den);
    int64_t p1 = a->num / g1;
    int64_t q1 = b->den / g1;
    int64_t p2 = b->num / g2;
    int64_t q2 = a->den / g2;

    int64_t num, denom;
    if (alg_mul_overflow(p1, p2, &num) || alg_mul_overflow(q2, q1, &denom)) {
        alg_set_error_rational(err, lv_alg_rational_ERR_OVERFLOW);
        return lv_alg_rational_zero();
    }

    alg_set_error_rational(err, lv_alg_rational_OK);
    AlgRational result;
    result.num = num;
    result.den = denom;
    lv_alg_rational_simplify(&result.num, &result.den);
    return result;
}

AlgRational lv_alg_rational_div(const AlgRational *a, const AlgRational *b, AlgRationalError *err) {
    if (!a || !b) {
        alg_set_error_rational(err, lv_alg_rational_ERR_NULL);
        return lv_alg_rational_zero();
    }
    if (b->num == 0) {
        alg_set_error_rational(err, lv_alg_rational_ERR_ZERO_DEN);
        return lv_alg_rational_zero();
    }

    /* a / b = a * (b.den / b.num) */
    AlgRational inv_b;
    inv_b.num = b->den;
    inv_b.den = b->num;
    if (inv_b.den < 0) {
        inv_b.num = -inv_b.num;
        inv_b.den = -inv_b.den;
    }

    return lv_alg_rational_mul(a, &inv_b, err);
}

AlgRational lv_alg_rational_neg(const AlgRational *a) {
    AlgRational r;
    r.num = -a->num;
    r.den = a->den;
    return r;
}

AlgRational lv_alg_rational_abs(const AlgRational *a) {
    AlgRational r;
    r.num = (a->num < 0) ? -a->num : a->num;
    r.den = a->den;
    return r;
}

AlgRational lv_alg_rational_inv(const AlgRational *a, AlgRationalError *err) {
    if (!a) {
        alg_set_error_rational(err, lv_alg_rational_ERR_NULL);
        return lv_alg_rational_zero();
    }
    if (a->num == 0) {
        alg_set_error_rational(err, lv_alg_rational_ERR_ZERO_DEN);
        return lv_alg_rational_zero();
    }
    alg_set_error_rational(err, lv_alg_rational_OK);
    return lv_alg_rational_create(a->den, a->num, NULL);
}

AlgRational lv_alg_rational_pow(const AlgRational *a, int n, AlgRationalError *err) {
    if (!a) {
        alg_set_error_rational(err, lv_alg_rational_ERR_NULL);
        return lv_alg_rational_zero();
    }
    if (n < 0) {
        /* 负指数：先取倒数再计算正幂 */
        /* 保护：n == INT_MIN 时 -n 溢出，直接报错返回 */
        if (n == INT_MIN) {
            alg_set_error_rational(err, lv_alg_rational_ERR_OVERFLOW);
            return lv_alg_rational_zero();
        }
        AlgRationalError inv_err;
        AlgRational inv = lv_alg_rational_inv(a, &inv_err);
        if (inv_err != lv_alg_rational_OK) {
            alg_set_error_rational(err, inv_err);
            return lv_alg_rational_zero();
        }
        return lv_alg_rational_pow(&inv, -n, err);
    }
    if (n == 0) {
        alg_set_error_rational(err, lv_alg_rational_OK);
        return lv_alg_rational_one();
    }

    /* 快速幂算法（K2/F33：骨架委托 lv_POW_SQUARING 单实现，mul 注入域内乘法） */
    AlgRational base = *a;
    AlgRational result = lv_alg_rational_one();
    int exp = n;

#define LV_ALG_POW_MUL(x, y)                                            \
    do {                                                                \
        AlgRationalError me;                                            \
        x = lv_alg_rational_mul(&x, &y, &me);                           \
        if (me != lv_alg_rational_OK) {                                 \
            alg_set_error_rational(err, me);                            \
            return lv_alg_rational_zero();                              \
        }                                                               \
    } while (0)
    lv_POW_SQUARING(base, exp, result, LV_ALG_POW_MUL);
#undef LV_ALG_POW_MUL

    alg_set_error_rational(err, lv_alg_rational_OK);
    return result;
}

int lv_alg_rational_cmp(const AlgRational *a, const AlgRational *b) {
    /* a.num/a.den - b.num/b.den = (a.num * b.den - b.num * a.den) / (a.den * b.den) */
    /* 由于 den > 0，只需比较分子 */
    int64_t lhs, rhs;
    /* 使用 __int128 避免溢出（如果编译器支持） */
#if defined(__SIZEOF_INT128__)
    __int128 l = (__int128) a->num * (__int128) b->den;
    __int128 r = (__int128) b->num * (__int128) a->den;
    if (l < r)
        return -1;
    if (l > r)
        return 1;
    return 0;
#else
    /* 回退到 double 近似比较 */
    double da = (double) a->num / (double) a->den;
    double db = (double) b->num / (double) b->den;
    if (da < db)
        return -1;
    if (da > db)
        return 1;
    return 0;
#endif
}

bool lv_alg_rational_eq(const AlgRational *a, const AlgRational *b) {
    return lv_alg_rational_cmp(a, b) == 0;
}

double lv_alg_rational_to_double(const AlgRational *r) {
    return (double) r->num / (double) r->den;
}

int lv_alg_rational_to_string(const AlgRational *r, char *buf, size_t size) {
    mpq_t q;
    char nb[24], db[24];
    mpq_init(q);
    lv_snprintf(nb, sizeof(nb), "%lld", (long long) r->num);
    lv_snprintf(db, sizeof(db), "%lld", (long long) r->den);
    mpq_set_str(q, nb, 10);
    mpz_set_str(mpq_denref(q), db, 10);
    mpq_canonicalize(q);
    char *s = lv_mpq_to_string(q, true);
    mpq_clear(q);
    if (!s)
        return -1;
    int len = (int) lv_strlcpy(buf, s, size);
    lv_free((void **) &s);
    return len;
}

bool lv_alg_rational_is_zero(const AlgRational *r) {
    return r->num == 0;
}

bool lv_alg_rational_is_positive(const AlgRational *r) {
    return r->num > 0;
}

bool lv_alg_rational_is_negative(const AlgRational *r) {
    return r->num < 0;
}

/* ================================================================
 * 枚举 -> 名称 映射表（数据表化，替代 switch）
 * ================================================================ */

/** @brief lv_alg_rational_error_string 名称表（按枚举值升序） */
static const lvStrToEnumEntry s_alg_rational_error_string_entries[] = {
    {"成功", lv_alg_rational_OK},
    {"分母为零", lv_alg_rational_ERR_ZERO_DEN},
    {"整数溢出", lv_alg_rational_ERR_OVERFLOW},
    {"空指针", lv_alg_rational_ERR_NULL},
    {"无效参数", lv_alg_rational_ERR_INVALID},
};

const char *lv_alg_rational_error_string(AlgRationalError err) {
    return lv_enum_to_str(s_alg_rational_error_string_entries, lv_ARRAY_SIZE(s_alg_rational_error_string_entries), (int) err, "未知错误");
}
