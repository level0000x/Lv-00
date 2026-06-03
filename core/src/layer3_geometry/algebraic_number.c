/**
 * @file algebraic_number.c
 * @brief 代数数域封装 —— 有理数、二次代数数、区间运算、多项式系统
 *
 * 实现文件，提供 algebraic_number.h 中声明的所有函数的实现。
 * 所有运算基于 int64_t，不依赖 GMP 等外部库。
 *
 * @version 3.5.0
 * @copyright Copyright (c) 2024-2026 Lv-00 Project
 */

#include "lv00/algebraic_number.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/* ============================================================
 * 内部辅助函数
 * ============================================================ */

/**
 * @brief 计算 int64_t 的最大公约数（GCD）
 *
 * 使用欧几里得算法。对负数取绝对值后计算。
 *
 * @param a 整数
 * @param b 整数
 * @return |a| 和 |b| 的最大公约数（>= 1）
 */
static int64_t alg_gcd(int64_t a, int64_t b)
{
    if (a < 0) a = -a;
    if (b < 0) b = -b;
    if (a == 0) return b;
    if (b == 0) return a;
    while (b != 0) {
        int64_t t = b;
        b = a % b;
        a = t;
    }
    return a;
}

/**
 * @brief 计算 int64_t 的最小公倍数（LCM）
 *
 * @param a 整数（非零）
 * @param b 整数（非零）
 * @return |a| 和 |b| 的最小公倍数
 */
static int64_t alg_lcm(int64_t a, int64_t b)
{
    if (a == 0 || b == 0) return 0;
    int64_t g = alg_gcd(a, b);
    /* 防止溢出：先除后乘 */
    int64_t aa = (a < 0) ? -a : a;
    int64_t bb = (b < 0) ? -b : b;
    return (aa / g) * bb;
}

/**
 * @brief 检测 int64_t 乘法是否溢出
 *
 * @param a 乘数
 * @param b 乘数
 * @param[out] result 乘积（无溢出时有效）
 * @return true 溢出，false 无溢出
 */
static bool alg_mul_overflow(int64_t a, int64_t b, int64_t *result)
{
    if (a == 0 || b == 0) {
        *result = 0;
        return false;
    }
    if (a > 0) {
        if (b > 0) {
            if (a > INT64_MAX / b) return true;
        } else {
            if (b < INT64_MIN / a) return true;
        }
    } else {
        if (b > 0) {
            if (a < INT64_MIN / b) return true;
        } else {
            if (a < INT64_MAX / b) return true;  /* 注意：两个负数相乘 */
            /* 修正：|a| * |b|，但 a < 0, b < 0 */
            if ((-a) > INT64_MAX / (-b)) return true;
        }
    }
    *result = a * b;
    return false;
}

/**
 * @brief 检测 int64_t 加法是否溢出
 */
static bool alg_add_overflow(int64_t a, int64_t b, int64_t *result)
{
    if ((b > 0 && a > INT64_MAX - b) || (b < 0 && a < INT64_MIN - b)) {
        return true;
    }
    *result = a + b;
    return false;
}

/**
 * @brief 检测 int64_t 减法是否溢出
 */
static bool alg_sub_overflow(int64_t a, int64_t b, int64_t *result)
{
    if ((b < 0 && a > INT64_MAX + b) || (b > 0 && a < INT64_MIN + b)) {
        return true;
    }
    *result = a - b;
    return false;
}

/**
 * @brief 约分有理数（内部使用）
 *
 * 将 p/q 约分为最简形式，确保 q > 0。
 * 调用者需确保 q != 0。
 */
static void alg_rational_simplify(int64_t *p, int64_t *q)
{
    if (*q < 0) {
        *p = -*p;
        *q = -*q;
    }
    int64_t g = alg_gcd(*p, *q);
    if (g > 1) {
        *p /= g;
        *q /= g;
    }
}

/**
 * @brief 判断整数是否为完全平方数
 */
static bool alg_is_perfect_square(int64_t n)
{
    if (n < 0) return false;
    if (n == 0 || n == 1) return true;
    int64_t lo = 1, hi = n < 46341 ? n : 46341; /* sqrt(INT64_MAX) ≈ 3037000499，但 46341^2 < INT64_MAX */
    /* 使用更安全的上界 */
    hi = (n < 3037000500LL) ? n : 3037000500LL;
    while (lo <= hi) {
        int64_t mid = lo + (hi - lo) / 2;
        int64_t sq;
        if (alg_mul_overflow(mid, mid, &sq)) {
            hi = mid - 1;
            continue;
        }
        if (sq == n) return true;
        if (sq < n) lo = mid + 1;
        else hi = mid - 1;
    }
    return false;
}

/**
 * @brief 计算整数平方根（向下取整）
 */
static int64_t alg_isqrt(int64_t n)
{
    if (n < 0) return 0;
    if (n == 0) return 0;
    int64_t x = (int64_t)sqrt((double)n);
    /* 修正浮点误差 */
    while (x > 0) {
        int64_t sq;
        if (alg_mul_overflow(x, x, &sq)) { x--; continue; }
        if (sq <= n) break;
        x--;
    }
    while (true) {
        int64_t sq;
        if (alg_mul_overflow(x + 1, x + 1, &sq)) break;
        if (sq > n) break;
        x++;
    }
    return x;
}

/**
 * @brief 设置错误码（若 err 非空）
 */
static void alg_set_error_rational(AlgRationalError *err, AlgRationalError code)
{
    if (err) *err = code;
}

static void alg_set_error_quadratic(AlgQuadraticError *err, AlgQuadraticError code)
{
    if (err) *err = code;
}

static void alg_set_error_interval(AlgIntervalError *err, AlgIntervalError code)
{
    if (err) *err = code;
}

static void alg_set_error_poly(AlgPolyError *err, AlgPolyError code)
{
    if (err) *err = code;
}

/* ============================================================
 * 第一层：有理数域 Q —— 实现
 * ============================================================ */

LV00_PUBLIC_API AlgRational alg_rational_create(int64_t p, int64_t q, AlgRationalError *err)
{
    AlgRational result;
    if (q == 0) {
        alg_set_error_rational(err, ALG_RATIONAL_ERR_ZERO_DEN);
        result.num = 0;
        result.den = 1;
        return result;
    }
    alg_rational_simplify(&p, &q);
    alg_set_error_rational(err, ALG_RATIONAL_OK);
    result.num = p;
    result.den = q;
    return result;
}

LV00_PUBLIC_API AlgRational alg_rational_zero(void)
{
    AlgRational r;
    r.num = 0;
    r.den = 1;
    return r;
}

LV00_PUBLIC_API AlgRational alg_rational_one(void)
{
    AlgRational r;
    r.num = 1;
    r.den = 1;
    return r;
}

LV00_PUBLIC_API AlgRational alg_rational_from_int(int64_t n)
{
    AlgRational r;
    r.num = n;
    r.den = 1;
    return r;
}

LV00_PUBLIC_API AlgRational alg_rational_add(const AlgRational *a, const AlgRational *b,
                                             AlgRationalError *err)
{
    if (!a || !b) {
        alg_set_error_rational(err, ALG_RATIONAL_ERR_NULL);
        return alg_rational_zero();
    }

    /* a.num/b.den + b.num/a.den = (a.num * b.den + b.num * a.den) / (a.den * b.den) */
    int64_t num1, num2, denom;
    if (alg_mul_overflow(a->num, b->den, &num1) ||
        alg_mul_overflow(b->num, a->den, &num2) ||
        alg_add_overflow(num1, num2, &num1) ||
        alg_mul_overflow(a->den, b->den, &denom)) {
        alg_set_error_rational(err, ALG_RATIONAL_ERR_OVERFLOW);
        return alg_rational_zero();
    }

    alg_set_error_rational(err, ALG_RATIONAL_OK);
    return alg_rational_create(num1, denom, NULL);
}

LV00_PUBLIC_API AlgRational alg_rational_sub(const AlgRational *a, const AlgRational *b,
                                             AlgRationalError *err)
{
    if (!a || !b) {
        alg_set_error_rational(err, ALG_RATIONAL_ERR_NULL);
        return alg_rational_zero();
    }

    int64_t num1, num2, denom;
    if (alg_mul_overflow(a->num, b->den, &num1) ||
        alg_mul_overflow(b->num, a->den, &num2) ||
        alg_sub_overflow(num1, num2, &num1) ||
        alg_mul_overflow(a->den, b->den, &denom)) {
        alg_set_error_rational(err, ALG_RATIONAL_ERR_OVERFLOW);
        return alg_rational_zero();
    }

    alg_set_error_rational(err, ALG_RATIONAL_OK);
    return alg_rational_create(num1, denom, NULL);
}

LV00_PUBLIC_API AlgRational alg_rational_mul(const AlgRational *a, const AlgRational *b,
                                             AlgRationalError *err)
{
    if (!a || !b) {
        alg_set_error_rational(err, ALG_RATIONAL_ERR_NULL);
        return alg_rational_zero();
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
        alg_set_error_rational(err, ALG_RATIONAL_ERR_OVERFLOW);
        return alg_rational_zero();
    }

    alg_set_error_rational(err, ALG_RATIONAL_OK);
    AlgRational result;
    result.num = num;
    result.den = denom;
    alg_rational_simplify(&result.num, &result.den);
    return result;
}

LV00_PUBLIC_API AlgRational alg_rational_div(const AlgRational *a, const AlgRational *b,
                                             AlgRationalError *err)
{
    if (!a || !b) {
        alg_set_error_rational(err, ALG_RATIONAL_ERR_NULL);
        return alg_rational_zero();
    }
    if (b->num == 0) {
        alg_set_error_rational(err, ALG_RATIONAL_ERR_ZERO_DEN);
        return alg_rational_zero();
    }

    /* a / b = a * (b.den / b.num) */
    AlgRational inv_b;
    inv_b.num = b->den;
    inv_b.den = b->num;
    if (inv_b.den < 0) {
        inv_b.num = -inv_b.num;
        inv_b.den = -inv_b.den;
    }

    return alg_rational_mul(a, &inv_b, err);
}

LV00_PUBLIC_API AlgRational alg_rational_neg(const AlgRational *a)
{
    AlgRational r;
    r.num = -a->num;
    r.den = a->den;
    return r;
}

LV00_PUBLIC_API AlgRational alg_rational_abs(const AlgRational *a)
{
    AlgRational r;
    r.num = (a->num < 0) ? -a->num : a->num;
    r.den = a->den;
    return r;
}

LV00_PUBLIC_API AlgRational alg_rational_inv(const AlgRational *a, AlgRationalError *err)
{
    if (!a) {
        alg_set_error_rational(err, ALG_RATIONAL_ERR_NULL);
        return alg_rational_zero();
    }
    if (a->num == 0) {
        alg_set_error_rational(err, ALG_RATIONAL_ERR_ZERO_DEN);
        return alg_rational_zero();
    }
    alg_set_error_rational(err, ALG_RATIONAL_OK);
    return alg_rational_create(a->den, a->num, NULL);
}

LV00_PUBLIC_API AlgRational alg_rational_pow(const AlgRational *a, int n,
                                             AlgRationalError *err)
{
    if (!a) {
        alg_set_error_rational(err, ALG_RATIONAL_ERR_NULL);
        return alg_rational_zero();
    }
    if (n < 0) {
        /* 负指数：先取倒数再计算正幂 */
        AlgRationalError inv_err;
        AlgRational inv = alg_rational_inv(a, &inv_err);
        if (inv_err != ALG_RATIONAL_OK) {
            alg_set_error_rational(err, inv_err);
            return alg_rational_zero();
        }
        return alg_rational_pow(&inv, -n, err);
    }
    if (n == 0) {
        alg_set_error_rational(err, ALG_RATIONAL_OK);
        return alg_rational_one();
    }

    /* 快速幂算法 */
    AlgRational base = *a;
    AlgRational result = alg_rational_one();
    int exp = n;

    while (exp > 0) {
        if (exp & 1) {
            AlgRationalError mul_err;
            result = alg_rational_mul(&result, &base, &mul_err);
            if (mul_err != ALG_RATIONAL_OK) {
                alg_set_error_rational(err, mul_err);
                return alg_rational_zero();
            }
        }
        exp >>= 1;
        if (exp > 0) {
            AlgRationalError sq_err;
            base = alg_rational_mul(&base, &base, &sq_err);
            if (sq_err != ALG_RATIONAL_OK) {
                alg_set_error_rational(err, sq_err);
                return alg_rational_zero();
            }
        }
    }

    alg_set_error_rational(err, ALG_RATIONAL_OK);
    return result;
}

LV00_PUBLIC_API int alg_rational_cmp(const AlgRational *a, const AlgRational *b)
{
    /* a.num/a.den - b.num/b.den = (a.num * b.den - b.num * a.den) / (a.den * b.den) */
    /* 由于 den > 0，只需比较分子 */
    int64_t lhs, rhs;
    /* 使用 __int128 避免溢出（如果编译器支持） */
#if defined(__SIZEOF_INT128__)
    __int128 l = (__int128)a->num * (__int128)b->den;
    __int128 r = (__int128)b->num * (__int128)a->den;
    if (l < r) return -1;
    if (l > r) return 1;
    return 0;
#else
    /* 回退到 double 近似比较 */
    double da = (double)a->num / (double)a->den;
    double db = (double)b->num / (double)b->den;
    if (da < db) return -1;
    if (da > db) return 1;
    return 0;
#endif
}

LV00_PUBLIC_API bool alg_rational_eq(const AlgRational *a, const AlgRational *b)
{
    return alg_rational_cmp(a, b) == 0;
}

LV00_PUBLIC_API double alg_rational_to_double(const AlgRational *r)
{
    return (double)r->num / (double)r->den;
}

LV00_PUBLIC_API int alg_rational_to_string(const AlgRational *r, char *buf, size_t size)
{
    if (r->den == 1) {
        return snprintf(buf, size, "%lld", (long long)r->num);
    }
    return snprintf(buf, size, "%lld/%lld", (long long)r->num, (long long)r->den);
}

LV00_PUBLIC_API bool alg_rational_is_zero(const AlgRational *r)
{
    return r->num == 0;
}

LV00_PUBLIC_API bool alg_rational_is_positive(const AlgRational *r)
{
    return r->num > 0;
}

LV00_PUBLIC_API bool alg_rational_is_negative(const AlgRational *r)
{
    return r->num < 0;
}

LV00_PUBLIC_API const char *alg_rational_error_string(AlgRationalError err)
{
    switch (err) {
        case ALG_RATIONAL_OK:          return "成功";
        case ALG_RATIONAL_ERR_ZERO_DEN: return "分母为零";
        case ALG_RATIONAL_ERR_OVERFLOW: return "整数溢出";
        case ALG_RATIONAL_ERR_NULL:     return "空指针";
        case ALG_RATIONAL_ERR_INVALID:  return "无效参数";
        default:                        return "未知错误";
    }
}

/* ============================================================
 * 第二层：二次代数数域 Q(sqrt(d)) —— 实现
 * ============================================================ */

LV00_PUBLIC_API AlgQuadratic alg_quadratic_create(int64_t a_val, int64_t a_den,
                                                    int64_t b_val, int64_t b_den,
                                                    int64_t d,
                                                    AlgQuadraticError *err)
{
    AlgQuadratic result;
    memset(&result, 0, sizeof(result));

    if (d < 0) {
        alg_set_error_quadratic(err, ALG_QUADRATIC_ERR_INVALID);
        result.d = 0;
        return result;
    }

    AlgRationalError r_err;
    result.a = alg_rational_create(a_val, a_den, &r_err);
    result.b = alg_rational_create(b_val, b_den, &r_err);
    result.d = d;

    alg_set_error_quadratic(err, ALG_QUADRATIC_OK);
    return result;
}

LV00_PUBLIC_API AlgQuadratic alg_quadratic_from_rational(const AlgRational *r, int64_t d)
{
    AlgQuadratic q;
    q.a = *r;
    q.b = alg_rational_zero();
    q.d = d;
    return q;
}

LV00_PUBLIC_API AlgQuadratic alg_quadratic_sqrt(int64_t b_val, int64_t b_den,
                                                 int64_t d,
                                                 AlgQuadraticError *err)
{
    return alg_quadratic_create(0, 1, b_val, b_den, d, err);
}

LV00_PUBLIC_API AlgQuadratic alg_quadratic_add(const AlgQuadratic *x, const AlgQuadratic *y,
                                                AlgQuadraticError *err)
{
    if (!x || !y) {
        alg_set_error_quadratic(err, ALG_QUADRATIC_ERR_NULL);
        AlgQuadratic z;
        memset(&z, 0, sizeof(z));
        return z;
    }
    if (x->d != y->d) {
        alg_set_error_quadratic(err, ALG_QUADRATIC_ERR_DOMAIN);
        AlgQuadratic z;
        memset(&z, 0, sizeof(z));
        return z;
    }

    AlgQuadratic result;
    result.a = alg_rational_add(&x->a, &y->a, NULL);
    result.b = alg_rational_add(&x->b, &y->b, NULL);
    result.d = x->d;
    alg_set_error_quadratic(err, ALG_QUADRATIC_OK);
    return result;
}

LV00_PUBLIC_API AlgQuadratic alg_quadratic_sub(const AlgQuadratic *x, const AlgQuadratic *y,
                                                AlgQuadraticError *err)
{
    if (!x || !y) {
        alg_set_error_quadratic(err, ALG_QUADRATIC_ERR_NULL);
        AlgQuadratic z;
        memset(&z, 0, sizeof(z));
        return z;
    }
    if (x->d != y->d) {
        alg_set_error_quadratic(err, ALG_QUADRATIC_ERR_DOMAIN);
        AlgQuadratic z;
        memset(&z, 0, sizeof(z));
        return z;
    }

    AlgQuadratic result;
    result.a = alg_rational_sub(&x->a, &y->a, NULL);
    result.b = alg_rational_sub(&x->b, &y->b, NULL);
    result.d = x->d;
    alg_set_error_quadratic(err, ALG_QUADRATIC_OK);
    return result;
}

LV00_PUBLIC_API AlgQuadratic alg_quadratic_mul(const AlgQuadratic *x, const AlgQuadratic *y,
                                                AlgQuadraticError *err)
{
    if (!x || !y) {
        alg_set_error_quadratic(err, ALG_QUADRATIC_ERR_NULL);
        AlgQuadratic z;
        memset(&z, 0, sizeof(z));
        return z;
    }
    if (x->d != y->d) {
        alg_set_error_quadratic(err, ALG_QUADRATIC_ERR_DOMAIN);
        AlgQuadratic z;
        memset(&z, 0, sizeof(z));
        return z;
    }

    /*
     * (a1 + b1*sqrt(d)) * (a2 + b2*sqrt(d))
     *   = (a1*a2 + b1*b2*d) + (a1*b2 + a2*b1)*sqrt(d)
     */
    AlgRational d_rat = alg_rational_from_int(x->d);
    AlgRationalError r_err;

    AlgRational _t1 = alg_rational_mul(&x->a, &y->a, &r_err);
    AlgRational _t2 = alg_rational_mul(&x->b, &y->b, &r_err);
    AlgRational _t3 = alg_rational_mul(&_t2, &d_rat, &r_err);
    AlgRational new_a = alg_rational_add(&_t1, &_t3, NULL);

    AlgRational _t4 = alg_rational_mul(&x->a, &y->b, &r_err);
    AlgRational _t5 = alg_rational_mul(&x->b, &y->a, &r_err);
    AlgRational new_b = alg_rational_add(&_t4, &_t5, NULL);

    AlgQuadratic result;
    result.a = new_a;
    result.b = new_b;
    result.d = x->d;
    alg_set_error_quadratic(err, ALG_QUADRATIC_OK);
    return result;
}

LV00_PUBLIC_API AlgQuadratic alg_quadratic_div(const AlgQuadratic *x, const AlgQuadratic *y,
                                                AlgQuadraticError *err)
{
    if (!x || !y) {
        alg_set_error_quadratic(err, ALG_QUADRATIC_ERR_NULL);
        AlgQuadratic z;
        memset(&z, 0, sizeof(z));
        return z;
    }
    if (x->d != y->d) {
        alg_set_error_quadratic(err, ALG_QUADRATIC_ERR_DOMAIN);
        AlgQuadratic z;
        memset(&z, 0, sizeof(z));
        return z;
    }

    /*
     * x / y = x * conj(y) / norm(y)
     * 其中 conj(y) = a - b*sqrt(d)
     *       norm(y) = a^2 - b^2*d
     */
    AlgQuadratic conj_y = alg_quadratic_conj(y);
    AlgRational norm_y = alg_quadratic_norm(y, err);
    if (err && *err != ALG_QUADRATIC_OK) {
        AlgQuadratic z;
        memset(&z, 0, sizeof(z));
        return z;
    }
    if (alg_rational_is_zero(&norm_y)) {
        alg_set_error_quadratic(err, ALG_QUADRATIC_ERR_INVALID);
        AlgQuadratic z;
        memset(&z, 0, sizeof(z));
        return z;
    }

    AlgQuadratic numerator = alg_quadratic_mul(x, &conj_y, err);
    if (err && *err != ALG_QUADRATIC_OK) {
        AlgQuadratic z;
        memset(&z, 0, sizeof(z));
        return z;
    }

    /* 将结果除以范数（有理数） */
    AlgRationalError r_err;
    numerator.a = alg_rational_div(&numerator.a, &norm_y, &r_err);
    numerator.b = alg_rational_div(&numerator.b, &norm_y, &r_err);

    if (r_err != ALG_RATIONAL_OK) {
        alg_set_error_quadratic(err, ALG_QUADRATIC_ERR_OVERFLOW);
        AlgQuadratic z;
        memset(&z, 0, sizeof(z));
        return z;
    }

    alg_set_error_quadratic(err, ALG_QUADRATIC_OK);
    return numerator;
}

LV00_PUBLIC_API AlgQuadratic alg_quadratic_neg(const AlgQuadratic *x)
{
    AlgQuadratic result;
    result.a = alg_rational_neg(&x->a);
    result.b = alg_rational_neg(&x->b);
    result.d = x->d;
    return result;
}

LV00_PUBLIC_API AlgQuadratic alg_quadratic_conj(const AlgQuadratic *x)
{
    AlgQuadratic result;
    result.a = x->a;
    result.b = alg_rational_neg(&x->b);
    result.d = x->d;
    return result;
}

LV00_PUBLIC_API AlgRational alg_quadratic_norm(const AlgQuadratic *x,
                                                AlgQuadraticError *err)
{
    if (!x) {
        alg_set_error_quadratic(err, ALG_QUADRATIC_ERR_NULL);
        return alg_rational_zero();
    }

    /*
     * N(x) = x * conj(x) = (a + b*sqrt(d)) * (a - b*sqrt(d))
     *      = a^2 - b^2 * d
     */
    AlgRationalError r_err;
    AlgRational a_sq = alg_rational_mul(&x->a, &x->a, &r_err);
    AlgRational b_sq = alg_rational_mul(&x->b, &x->b, &r_err);
    AlgRational d_rat = alg_rational_from_int(x->d);
    AlgRational b_sq_d = alg_rational_mul(&b_sq, &d_rat, &r_err);
    AlgRational norm = alg_rational_sub(&a_sq, &b_sq_d, &r_err);

    alg_set_error_quadratic(err, ALG_QUADRATIC_OK);
    return norm;
}

LV00_PUBLIC_API int alg_quadratic_cmp(const AlgQuadratic *x, const AlgQuadratic *y)
{
    double dx = alg_quadratic_to_double(x);
    double dy = alg_quadratic_to_double(y);
    if (dx < dy) return -1;
    if (dx > dy) return 1;
    return 0;
}

LV00_PUBLIC_API int alg_quadratic_cmp_exact(const AlgQuadratic *x, const AlgQuadratic *y,
                                             AlgQuadraticError *err)
{
    if (!x || !y) {
        alg_set_error_quadratic(err, ALG_QUADRATIC_ERR_NULL);
        return 0;
    }
    if (x->d != y->d) {
        alg_set_error_quadratic(err, ALG_QUADRATIC_ERR_DOMAIN);
        return 0;
    }

    /* 精确比较：x - y = (a1-a2) + (b1-b2)*sqrt(d) */
    /* 若 b1-b2 == 0，则直接比较有理部分 */
    /* 若 b1-b2 != 0，则 x == y 当且仅当 a1==a2 且 b1==b2 */
    AlgRational diff_a = alg_rational_sub(&x->a, &y->a, NULL);
    AlgRational diff_b = alg_rational_sub(&x->b, &y->b, NULL);

    if (alg_rational_is_zero(&diff_b)) {
        /* 纯有理数比较 */
        AlgRational _zero = alg_rational_zero();
        alg_set_error_quadratic(err, ALG_QUADRATIC_OK);
        return alg_rational_cmp(&diff_a, &_zero);
    }

    if (alg_rational_is_zero(&diff_a) && alg_rational_is_zero(&diff_b)) {
        alg_set_error_quadratic(err, ALG_QUADRATIC_OK);
        return 0;
    }

    /* 含 sqrt(d) 分量的精确比较：
     * diff = diff_a + diff_b * sqrt(d)
     * 若 diff_b > 0：diff_a + diff_b*sqrt(d) > 0 当且仅当 diff_a > -diff_b*sqrt(d)
     *   即 diff_a^2 > diff_b^2 * d（当 diff_a > 0 时）
     *   或 diff_a^2 < diff_b^2 * d（当 diff_a < 0 时）
     * 使用近似值进行判断 */
    alg_set_error_quadratic(err, ALG_QUADRATIC_OK);
    return alg_quadratic_cmp(x, y);
}

LV00_PUBLIC_API double alg_quadratic_to_double(const AlgQuadratic *x)
{
    double a = alg_rational_to_double(&x->a);
    double b = alg_rational_to_double(&x->b);
    double sqrt_d = (x->d == 0) ? 0.0 : sqrt((double)x->d);
    return a + b * sqrt_d;
}

LV00_PUBLIC_API int alg_quadratic_to_string(const AlgQuadratic *x, char *buf, size_t size)
{
    int written = 0;
    int n;

    /* 有理部分 */
    n = alg_rational_to_string(&x->a, buf, size);
    written += n;
    if (written < (int)size) buf += n;

    /* sqrt(d) 部分 */
    if (!alg_rational_is_zero(&x->b)) {
        if (alg_rational_is_positive(&x->b)) {
            n = snprintf(buf, size > (size_t)written ? size - (size_t)written : 0,
                         " + ");
            written += n;
            if (written < (int)size) buf += n;
        } else {
            n = snprintf(buf, size > (size_t)written ? size - (size_t)written : 0,
                         " - ");
            written += n;
            if (written < (int)size) buf += n;
        }

        /* 系数绝对值 */
        AlgRational abs_b = alg_rational_abs(&x->b);
        AlgRational _one = alg_rational_one();
        if (!alg_rational_eq(&abs_b, &_one)) {
            n = alg_rational_to_string(&abs_b, buf,
                                       size > (size_t)written ? size - (size_t)written : 0);
            written += n;
            if (written < (int)size) buf += n;
            n = snprintf(buf, size > (size_t)written ? size - (size_t)written : 0, "*");
            written += n;
            if (written < (int)size) buf += n;
        }

        /* sqrt(d) */
        if (x->d == 0) {
            /* 退化为有理数 */
        } else {
            n = snprintf(buf, size > (size_t)written ? size - (size_t)written : 0,
                         "sqrt(%lld)", (long long)x->d);
            written += n;
        }
    }

    return written;
}

LV00_PUBLIC_API bool alg_quadratic_is_rational(const AlgQuadratic *x)
{
    return alg_rational_is_zero(&x->b);
}

LV00_PUBLIC_API AlgRational alg_quadratic_rational_part(const AlgQuadratic *x)
{
    return x->a;
}

LV00_PUBLIC_API const char *alg_quadratic_error_string(AlgQuadraticError err)
{
    switch (err) {
        case ALG_QUADRATIC_OK:        return "成功";
        case ALG_QUADRATIC_ERR_DOMAIN: return "域不匹配（d 不同）";
        case ALG_QUADRATIC_ERR_OVERFLOW: return "整数溢出";
        case ALG_QUADRATIC_ERR_NULL:   return "空指针";
        case ALG_QUADRATIC_ERR_INVALID: return "无效参数";
        default:                       return "未知错误";
    }
}

/* ============================================================
 * 第三层：区间运算 —— 实现
 * ============================================================ */

LV00_PUBLIC_API AlgInterval alg_interval_create(int64_t lo_val, int64_t lo_den,
                                                int64_t hi_val, int64_t hi_den,
                                                AlgIntervalError *err)
{
    AlgInterval result;
    AlgRationalError r_err;

    result.lo = alg_rational_create(lo_val, lo_den, &r_err);
    result.hi = alg_rational_create(hi_val, hi_den, &r_err);

    /* 确保 lo <= hi */
    if (alg_rational_cmp(&result.lo, &result.hi) > 0) {
        AlgRational tmp = result.lo;
        result.lo = result.hi;
        result.hi = tmp;
    }

    alg_set_error_interval(err, ALG_INTERVAL_OK);
    return result;
}

LV00_PUBLIC_API AlgInterval alg_interval_point(const AlgRational *r)
{
    AlgInterval iv;
    iv.lo = *r;
    iv.hi = *r;
    return iv;
}

LV00_PUBLIC_API AlgInterval alg_interval_from_quadratic(const AlgQuadratic *x,
                                                         AlgIntervalError *err)
{
    if (!x) {
        alg_set_error_interval(err, ALG_INTERVAL_ERR_NULL);
        AlgInterval iv;
        iv.lo = alg_rational_zero();
        iv.hi = alg_rational_zero();
        return iv;
    }

    /* 若 d 为完全平方数，可计算精确值 */
    if (x->d == 0 || alg_is_perfect_square(x->d)) {
        int64_t sqrt_d = (x->d == 0) ? 0 : alg_isqrt(x->d);
        AlgRationalError r_err;
        AlgRational sqrt_d_rat = alg_rational_from_int(sqrt_d);
        AlgRational b_sqrt_d = alg_rational_mul(&x->b, &sqrt_d_rat, &r_err);
        AlgRational exact = alg_rational_add(&x->a, &b_sqrt_d, &r_err);
        alg_set_error_interval(err, ALG_INTERVAL_OK);
        return alg_interval_point(&exact);
    }

    /* 否则使用包围区间 */
    double val = alg_quadratic_to_double(x);
    /* 使用有理数近似包围 */
    /* lo = floor(val) - 1, hi = ceil(val) + 1 作为粗略包围 */
    int64_t lo_int = (int64_t)floor(val) - 1;
    int64_t hi_int = (int64_t)ceil(val) + 1;

    alg_set_error_interval(err, ALG_INTERVAL_OK);
    return alg_interval_create(lo_int, 1, hi_int, 1, NULL);
}

LV00_PUBLIC_API AlgInterval alg_interval_add(const AlgInterval *x, const AlgInterval *y,
                                             AlgIntervalError *err)
{
    if (!x || !y) {
        alg_set_error_interval(err, ALG_INTERVAL_ERR_NULL);
        AlgInterval iv;
        iv.lo = iv.hi = alg_rational_zero();
        return iv;
    }

    AlgInterval result;
    result.lo = alg_rational_add(&x->lo, &y->lo, NULL);
    result.hi = alg_rational_add(&x->hi, &y->hi, NULL);
    alg_set_error_interval(err, ALG_INTERVAL_OK);
    return result;
}

LV00_PUBLIC_API AlgInterval alg_interval_sub(const AlgInterval *x, const AlgInterval *y,
                                             AlgIntervalError *err)
{
    if (!x || !y) {
        alg_set_error_interval(err, ALG_INTERVAL_ERR_NULL);
        AlgInterval iv;
        iv.lo = iv.hi = alg_rational_zero();
        return iv;
    }

    AlgInterval result;
    result.lo = alg_rational_sub(&x->lo, &y->hi, NULL);
    result.hi = alg_rational_sub(&x->hi, &y->lo, NULL);
    alg_set_error_interval(err, ALG_INTERVAL_OK);
    return result;
}

LV00_PUBLIC_API AlgInterval alg_interval_mul(const AlgInterval *x, const AlgInterval *y,
                                             AlgIntervalError *err)
{
    if (!x || !y) {
        alg_set_error_interval(err, ALG_INTERVAL_ERR_NULL);
        AlgInterval iv;
        iv.lo = iv.hi = alg_rational_zero();
        return iv;
    }

    /* 计算四个端点乘积，取最小和最大 */
    AlgRational p1 = alg_rational_mul(&x->lo, &y->lo, NULL);
    AlgRational p2 = alg_rational_mul(&x->lo, &y->hi, NULL);
    AlgRational p3 = alg_rational_mul(&x->hi, &y->lo, NULL);
    AlgRational p4 = alg_rational_mul(&x->hi, &y->hi, NULL);

    AlgRational lo = p1, hi = p1;
    if (alg_rational_cmp(&p2, &lo) < 0) lo = p2;
    if (alg_rational_cmp(&p3, &lo) < 0) lo = p3;
    if (alg_rational_cmp(&p4, &lo) < 0) lo = p4;
    if (alg_rational_cmp(&p2, &hi) > 0) hi = p2;
    if (alg_rational_cmp(&p3, &hi) > 0) hi = p3;
    if (alg_rational_cmp(&p4, &hi) > 0) hi = p4;

    AlgInterval result;
    result.lo = lo;
    result.hi = hi;
    alg_set_error_interval(err, ALG_INTERVAL_OK);
    return result;
}

LV00_PUBLIC_API AlgInterval alg_interval_div(const AlgInterval *x, const AlgInterval *y,
                                             AlgIntervalError *err)
{
    if (!x || !y) {
        alg_set_error_interval(err, ALG_INTERVAL_ERR_NULL);
        AlgInterval iv;
        iv.lo = iv.hi = alg_rational_zero();
        return iv;
    }

    /* 检查除数区间是否包含零 */
    AlgRational _zero = alg_rational_zero();
    bool lo_nonpos = alg_rational_cmp(&y->lo, &_zero) <= 0;
    bool hi_nonneg = alg_rational_cmp(&y->hi, &_zero) >= 0;
    if (lo_nonpos && hi_nonneg) {
        alg_set_error_interval(err, ALG_INTERVAL_ERR_DIV_BY_ZERO);
        AlgInterval iv;
        iv.lo = iv.hi = alg_rational_zero();
        return iv;
    }

    /* x / y = x * (1/y) */
    AlgRational inv_lo, inv_hi;
    AlgRationalError r_err;
    inv_lo = alg_rational_inv(&y->lo, &r_err);
    inv_hi = alg_rational_inv(&y->hi, &r_err);

    AlgInterval inv_y;
    if (alg_rational_cmp(&inv_lo, &inv_hi) <= 0) {
        inv_y.lo = inv_lo;
        inv_y.hi = inv_hi;
    } else {
        inv_y.lo = inv_hi;
        inv_y.hi = inv_lo;
    }

    return alg_interval_mul(x, &inv_y, err);
}

LV00_PUBLIC_API AlgInterval alg_interval_neg(const AlgInterval *x)
{
    AlgInterval result;
    result.lo = alg_rational_neg(&x->hi);
    result.hi = alg_rational_neg(&x->lo);
    return result;
}

LV00_PUBLIC_API AlgInterval alg_interval_intersect(const AlgInterval *x, const AlgInterval *y,
                                                    AlgIntervalError *err)
{
    if (!x || !y) {
        alg_set_error_interval(err, ALG_INTERVAL_ERR_NULL);
        AlgInterval iv;
        iv.lo = iv.hi = alg_rational_zero();
        return iv;
    }

    AlgInterval result;
    if (alg_rational_cmp(&x->lo, &y->lo) > 0) {
        result.lo = x->lo;
    } else {
        result.lo = y->lo;
    }
    if (alg_rational_cmp(&x->hi, &y->hi) < 0) {
        result.hi = x->hi;
    } else {
        result.hi = y->hi;
    }

    if (alg_rational_cmp(&result.lo, &result.hi) > 0) {
        alg_set_error_interval(err, ALG_INTERVAL_ERR_EMPTY);
    } else {
        alg_set_error_interval(err, ALG_INTERVAL_OK);
    }
    return result;
}

LV00_PUBLIC_API AlgInterval alg_interval_hull(const AlgInterval *x, const AlgInterval *y,
                                              AlgIntervalError *err)
{
    if (!x || !y) {
        alg_set_error_interval(err, ALG_INTERVAL_ERR_NULL);
        AlgInterval iv;
        iv.lo = iv.hi = alg_rational_zero();
        return iv;
    }

    AlgInterval result;
    if (alg_rational_cmp(&x->lo, &y->lo) < 0) {
        result.lo = x->lo;
    } else {
        result.lo = y->lo;
    }
    if (alg_rational_cmp(&x->hi, &y->hi) > 0) {
        result.hi = x->hi;
    } else {
        result.hi = y->hi;
    }

    alg_set_error_interval(err, ALG_INTERVAL_OK);
    return result;
}

LV00_PUBLIC_API bool alg_interval_contains(const AlgInterval *x, const AlgInterval *y)
{
    return alg_rational_cmp(&x->lo, &y->lo) <= 0 &&
           alg_rational_cmp(&x->hi, &y->hi) >= 0;
}

LV00_PUBLIC_API bool alg_interval_contains_rational(const AlgInterval *x,
                                                     const AlgRational *r)
{
    return alg_rational_cmp(&x->lo, r) <= 0 &&
           alg_rational_cmp(&x->hi, r) >= 0;
}

LV00_PUBLIC_API bool alg_interval_is_empty(const AlgInterval *x)
{
    return alg_rational_cmp(&x->lo, &x->hi) > 0;
}

LV00_PUBLIC_API bool alg_interval_is_point(const AlgInterval *x)
{
    return alg_rational_eq(&x->lo, &x->hi);
}

LV00_PUBLIC_API AlgRational alg_interval_width(const AlgInterval *x,
                                                AlgIntervalError *err)
{
    if (!x) {
        alg_set_error_interval(err, ALG_INTERVAL_ERR_NULL);
        return alg_rational_zero();
    }
    alg_set_error_interval(err, ALG_INTERVAL_OK);
    return alg_rational_sub(&x->hi, &x->lo, NULL);
}

LV00_PUBLIC_API AlgRational alg_interval_midpoint(const AlgInterval *x,
                                                   AlgIntervalError *err)
{
    if (!x) {
        alg_set_error_interval(err, ALG_INTERVAL_ERR_NULL);
        return alg_rational_zero();
    }
    AlgRational sum = alg_rational_add(&x->lo, &x->hi, NULL);
    AlgRational two = alg_rational_from_int(2);
    alg_set_error_interval(err, ALG_INTERVAL_OK);
    return alg_rational_div(&sum, &two, NULL);
}

LV00_PUBLIC_API void alg_interval_bisect(const AlgInterval *x,
                                          AlgInterval *lower, AlgInterval *upper,
                                          AlgIntervalError *err)
{
    if (!x) {
        alg_set_error_interval(err, ALG_INTERVAL_ERR_NULL);
        return;
    }

    AlgIntervalError i_err;
    AlgRational mid = alg_interval_midpoint(x, &i_err);

    if (lower) {
        lower->lo = x->lo;
        lower->hi = mid;
    }
    if (upper) {
        upper->lo = mid;
        upper->hi = x->hi;
    }

    alg_set_error_interval(err, ALG_INTERVAL_OK);
}

LV00_PUBLIC_API int alg_interval_to_string(const AlgInterval *x, char *buf, size_t size)
{
    int written = 0;
    int n;

    n = snprintf(buf, size, "[");
    written += n;
    if (written < (int)size) buf += n;

    n = alg_rational_to_string(&x->lo, buf, size > (size_t)written ? size - (size_t)written : 0);
    written += n;
    if (written < (int)size) buf += n;

    n = snprintf(buf, size > (size_t)written ? size - (size_t)written : 0, ", ");
    written += n;
    if (written < (int)size) buf += n;

    n = alg_rational_to_string(&x->hi, buf, size > (size_t)written ? size - (size_t)written : 0);
    written += n;
    if (written < (int)size) buf += n;

    n = snprintf(buf, size > (size_t)written ? size - (size_t)written : 0, "]");
    written += n;

    return written;
}

LV00_PUBLIC_API const char *alg_interval_error_string(AlgIntervalError err)
{
    switch (err) {
        case ALG_INTERVAL_OK:            return "成功";
        case ALG_INTERVAL_ERR_EMPTY:     return "空区间";
        case ALG_INTERVAL_ERR_OVERFLOW:   return "整数溢出";
        case ALG_INTERVAL_ERR_NULL:      return "空指针";
        case ALG_INTERVAL_ERR_INVALID:    return "无效参数（lo > hi）";
        case ALG_INTERVAL_ERR_DIV_BY_ZERO: return "除以包含零的区间";
        default:                          return "未知错误";
    }
}

/* ============================================================
 * 第四层：多项式系统（简化版） —— 实现
 * ============================================================ */

/**
 * @brief 规范化多项式（去除高次零系数）
 */
static void alg_poly_normalize(AlgPoly *p)
{
    while (p->degree > 0 && p->coef[p->degree] == 0) {
        p->degree--;
    }
}

LV00_PUBLIC_API AlgPoly alg_poly_zero(void)
{
    AlgPoly p;
    memset(p.coef, 0, sizeof(p.coef));
    p.degree = 0;
    return p;
}

LV00_PUBLIC_API AlgPoly alg_poly_const(int64_t c)
{
    AlgPoly p;
    memset(p.coef, 0, sizeof(p.coef));
    p.coef[0] = c;
    p.degree = 0;
    alg_poly_normalize(&p);
    return p;
}

LV00_PUBLIC_API AlgPoly alg_poly_linear(int64_t a, int64_t b)
{
    AlgPoly p;
    memset(p.coef, 0, sizeof(p.coef));
    p.coef[1] = a;
    p.coef[0] = b;
    p.degree = (a != 0) ? 1 : 0;
    alg_poly_normalize(&p);
    return p;
}

LV00_PUBLIC_API AlgPoly alg_poly_quadratic(int64_t a, int64_t b, int64_t c)
{
    AlgPoly p;
    memset(p.coef, 0, sizeof(p.coef));
    p.coef[2] = a;
    p.coef[1] = b;
    p.coef[0] = c;
    p.degree = (a != 0) ? 2 : ((b != 0) ? 1 : 0);
    alg_poly_normalize(&p);
    return p;
}

LV00_PUBLIC_API AlgPoly alg_poly_x(void)
{
    return alg_poly_linear(1, 0);
}

LV00_PUBLIC_API int64_t alg_poly_eval_int(const AlgPoly *p, int64_t n,
                                           AlgPolyError *err)
{
    if (!p) {
        alg_set_error_poly(err, ALG_POLY_ERR_NULL);
        return 0;
    }

    /* Horner 方法 */
    int64_t result = 0;
    int64_t overflow_flag = 0; /* 简单溢出检测标志 */
    for (int i = p->degree; i >= 0; i--) {
        int64_t tmp;
        if (alg_mul_overflow(result, n, &tmp) || alg_add_overflow(tmp, p->coef[i], &result)) {
            /* 溢出时回退到 double 计算 */
            alg_set_error_poly(err, ALG_POLY_ERR_OVERFLOW);
            double d_result = 0.0;
            for (int j = p->degree; j >= 0; j--) {
                d_result = d_result * (double)n + (double)p->coef[j];
            }
            return (int64_t)d_result;
        }
    }

    alg_set_error_poly(err, ALG_POLY_OK);
    (void)overflow_flag;
    return result;
}

LV00_PUBLIC_API AlgRational alg_poly_eval_rational(const AlgPoly *p, const AlgRational *r,
                                                   AlgPolyError *err)
{
    if (!p || !r) {
        alg_set_error_poly(err, ALG_POLY_ERR_NULL);
        return alg_rational_zero();
    }

    /* Horner 方法（有理数版本） */
    AlgRational result = alg_rational_zero();
    AlgRationalError r_err;

    for (int i = p->degree; i >= 0; i--) {
        result = alg_rational_mul(&result, r, &r_err);
        AlgRational coef_r = alg_rational_from_int(p->coef[i]);
        result = alg_rational_add(&result, &coef_r, &r_err);
    }

    alg_set_error_poly(err, ALG_POLY_OK);
    return result;
}

LV00_PUBLIC_API AlgPoly alg_poly_add(const AlgPoly *p, const AlgPoly *q,
                                      AlgPolyError *err)
{
    if (!p || !q) {
        alg_set_error_poly(err, ALG_POLY_ERR_NULL);
        return alg_poly_zero();
    }

    AlgPoly result;
    memset(result.coef, 0, sizeof(result.coef));

    int max_deg = (p->degree > q->degree) ? p->degree : q->degree;
    for (int i = 0; i <= max_deg; i++) {
        int64_t a = (i <= p->degree) ? p->coef[i] : 0;
        int64_t b = (i <= q->degree) ? q->coef[i] : 0;
        if (alg_add_overflow(a, b, &result.coef[i])) {
            alg_set_error_poly(err, ALG_POLY_ERR_OVERFLOW);
            return alg_poly_zero();
        }
    }
    result.degree = max_deg;
    alg_poly_normalize(&result);

    alg_set_error_poly(err, ALG_POLY_OK);
    return result;
}

LV00_PUBLIC_API AlgPoly alg_poly_sub(const AlgPoly *p, const AlgPoly *q,
                                      AlgPolyError *err)
{
    if (!p || !q) {
        alg_set_error_poly(err, ALG_POLY_ERR_NULL);
        return alg_poly_zero();
    }

    AlgPoly result;
    memset(result.coef, 0, sizeof(result.coef));

    int max_deg = (p->degree > q->degree) ? p->degree : q->degree;
    for (int i = 0; i <= max_deg; i++) {
        int64_t a = (i <= p->degree) ? p->coef[i] : 0;
        int64_t b = (i <= q->degree) ? q->coef[i] : 0;
        if (alg_sub_overflow(a, b, &result.coef[i])) {
            alg_set_error_poly(err, ALG_POLY_ERR_OVERFLOW);
            return alg_poly_zero();
        }
    }
    result.degree = max_deg;
    alg_poly_normalize(&result);

    alg_set_error_poly(err, ALG_POLY_OK);
    return result;
}

LV00_PUBLIC_API AlgPoly alg_poly_mul(const AlgPoly *p, const AlgPoly *q,
                                      AlgPolyError *err)
{
    if (!p || !q) {
        alg_set_error_poly(err, ALG_POLY_ERR_NULL);
        return alg_poly_zero();
    }

    int new_deg = p->degree + q->degree;
    if (new_deg > ALG_POLY_MAX_DEGREE) {
        alg_set_error_poly(err, ALG_POLY_ERR_DEGREE);
        return alg_poly_zero();
    }

    AlgPoly result;
    memset(result.coef, 0, sizeof(result.coef));
    result.degree = new_deg;

    for (int i = 0; i <= p->degree; i++) {
        for (int j = 0; j <= q->degree; j++) {
            int64_t prod;
            if (alg_mul_overflow(p->coef[i], q->coef[j], &prod)) {
                alg_set_error_poly(err, ALG_POLY_ERR_OVERFLOW);
                return alg_poly_zero();
            }
            int64_t sum;
            if (alg_add_overflow(result.coef[i + j], prod, &sum)) {
                alg_set_error_poly(err, ALG_POLY_ERR_OVERFLOW);
                return alg_poly_zero();
            }
            result.coef[i + j] = sum;
        }
    }

    alg_poly_normalize(&result);
    alg_set_error_poly(err, ALG_POLY_OK);
    return result;
}

LV00_PUBLIC_API AlgPoly alg_poly_neg(const AlgPoly *p)
{
    AlgPoly result;
    for (int i = 0; i <= ALG_POLY_MAX_DEGREE; i++) {
        result.coef[i] = -p->coef[i];
    }
    result.degree = p->degree;
    return result;
}

LV00_PUBLIC_API int64_t alg_poly_lead_coef(const AlgPoly *p)
{
    return p->coef[p->degree];
}

LV00_PUBLIC_API int64_t alg_poly_const_coef(const AlgPoly *p)
{
    return p->coef[0];
}

LV00_PUBLIC_API bool alg_poly_is_zero(const AlgPoly *p)
{
    return p->degree == 0 && p->coef[0] == 0;
}

LV00_PUBLIC_API bool alg_poly_is_const(const AlgPoly *p)
{
    return p->degree == 0;
}

LV00_PUBLIC_API int64_t alg_poly_discriminant(const AlgPoly *p, AlgPolyError *err)
{
    if (!p) {
        alg_set_error_poly(err, ALG_POLY_ERR_NULL);
        return 0;
    }

    switch (p->degree) {
        case 0:
            alg_set_error_poly(err, ALG_POLY_OK);
            return 0;
        case 1:
            /* ax + b: 判别式为 1 */
            alg_set_error_poly(err, ALG_POLY_OK);
            return 1;
        case 2: {
            /* ax^2 + bx + c: 判别式为 b^2 - 4ac */
            int64_t b_sq, four_ac, disc;
            if (alg_mul_overflow(p->coef[1], p->coef[1], &b_sq) ||
                alg_mul_overflow(4, p->coef[2], &four_ac) ||
                alg_mul_overflow(four_ac, p->coef[0], &four_ac) ||
                alg_sub_overflow(b_sq, four_ac, &disc)) {
                alg_set_error_poly(err, ALG_POLY_ERR_OVERFLOW);
                return 0;
            }
            alg_set_error_poly(err, ALG_POLY_OK);
            return disc;
        }
        default:
            alg_set_error_poly(err, ALG_POLY_ERR_DEGREE);
            return 0;
    }
}

LV00_PUBLIC_API int alg_poly_rational_roots(const AlgPoly *p, AlgRational *roots,
                                             int max_roots, AlgPolyError *err)
{
    if (!p || !roots || max_roots <= 0) {
        alg_set_error_poly(err, ALG_POLY_ERR_NULL);
        return 0;
    }

    int found = 0;

    if (p->degree == 0) {
        alg_set_error_poly(err, ALG_POLY_OK);
        return 0;
    }

    if (p->degree == 1) {
        /* ax + b = 0 => x = -b/a */
        AlgRationalError r_err;
        roots[found] = alg_rational_create(-p->coef[0], p->coef[1], &r_err);
        found++;
        alg_set_error_poly(err, ALG_POLY_OK);
        return found;
    }

    if (p->degree == 2) {
        /* ax^2 + bx + c = 0 */
        int64_t disc = alg_poly_discriminant(p, err);
        if (err && *err != ALG_POLY_OK) return 0;

        if (disc < 0) {
            /* 无实根 */
            alg_set_error_poly(err, ALG_POLY_OK);
            return 0;
        }

        if (disc == 0) {
            /* 重根 x = -b/(2a) */
            AlgRationalError r_err;
            roots[found] = alg_rational_create(-p->coef[1], 2 * p->coef[2], &r_err);
            found++;
            alg_set_error_poly(err, ALG_POLY_OK);
            return found;
        }

        /* 两个不同的实根 */
        /* x = (-b +/- sqrt(disc)) / (2a) */
        /* 先检查 disc 是否为完全平方数 */
        if (alg_is_perfect_square(disc)) {
            int64_t sqrt_disc = alg_isqrt(disc);
            AlgRationalError r_err;
            if (found < max_roots) {
                roots[found] = alg_rational_create(-p->coef[1] - sqrt_disc,
                                                    2 * p->coef[2], &r_err);
                found++;
            }
            if (found < max_roots) {
                roots[found] = alg_rational_create(-p->coef[1] + sqrt_disc,
                                                    2 * p->coef[2], &r_err);
                found++;
            }
        }
        /* 若 disc 不是完全平方数，则无有理根 */

        alg_set_error_poly(err, ALG_POLY_OK);
        return found;
    }

    /* 高次多项式：使用有理根定理枚举候选 */
    if (p->coef[0] == 0) {
        /* x=0 是一个根，降次后递归 */
        if (found < max_roots) {
            roots[found] = alg_rational_zero();
            found++;
        }
        /* 降次：p(x) / x */
        AlgPoly reduced;
        memset(reduced.coef, 0, sizeof(reduced.coef));
        reduced.degree = p->degree - 1;
        for (int i = 1; i <= p->degree; i++) {
            reduced.coef[i - 1] = p->coef[i];
        }
        alg_poly_normalize(&reduced);
        int sub_found = alg_poly_rational_roots(&reduced, roots + found,
                                                 max_roots - found, err);
        return found + sub_found;
    }

    /* 有理根定理：候选 p/q 满足 p | a_0, q | a_n */
    int64_t a0 = (p->coef[0] < 0) ? -p->coef[0] : p->coef[0];
    int64_t an = (p->coef[p->degree] < 0) ? -p->coef[p->degree] : p->coef[p->degree];

    /* 收集 a0 的所有因子 */
    int64_t p_factors[64];
    int p_count = 0;
    for (int64_t d = 1; d * d <= a0 && p_count < 64; d++) {
        if (a0 % d == 0) {
            p_factors[p_count++] = d;
            if (d != a0 / d && p_count < 64) {
                p_factors[p_count++] = a0 / d;
            }
        }
    }

    /* 收集 an 的所有因子 */
    int64_t q_factors[64];
    int q_count = 0;
    for (int64_t d = 1; d * d <= an && q_count < 64; d++) {
        if (an % d == 0) {
            q_factors[q_count++] = d;
            if (d != an / d && q_count < 64) {
                q_factors[q_count++] = an / d;
            }
        }
    }

    /* 枚举所有候选 p/q 和 -p/q */
    for (int i = 0; i < p_count && found < max_roots; i++) {
        for (int j = 0; j < q_count && found < max_roots; j++) {
            /* 正候选 */
            AlgRationalError r_err;
            AlgRational candidate = alg_rational_create(p_factors[i], q_factors[j], &r_err);
            AlgRational val = alg_poly_eval_rational(p, &candidate, NULL);
            if (alg_rational_is_zero(&val)) {
                roots[found++] = candidate;
            }
            /* 负候选 */
            if (found < max_roots) {
                candidate = alg_rational_create(-p_factors[i], q_factors[j], &r_err);
                val = alg_poly_eval_rational(p, &candidate, NULL);
                if (alg_rational_is_zero(&val)) {
                    roots[found++] = candidate;
                }
            }
        }
    }

    alg_set_error_poly(err, ALG_POLY_OK);
    return found;
}

LV00_PUBLIC_API AlgPoly alg_poly_derivative(const AlgPoly *p, AlgPolyError *err)
{
    if (!p) {
        alg_set_error_poly(err, ALG_POLY_ERR_NULL);
        return alg_poly_zero();
    }

    if (p->degree == 0) {
        /* 常数的导数为零 */
        alg_set_error_poly(err, ALG_POLY_OK);
        return alg_poly_zero();
    }

    AlgPoly result;
    memset(result.coef, 0, sizeof(result.coef));
    result.degree = p->degree - 1;

    for (int i = 1; i <= p->degree; i++) {
        int64_t new_coef;
        if (alg_mul_overflow((int64_t)i, p->coef[i], &new_coef)) {
            alg_set_error_poly(err, ALG_POLY_ERR_OVERFLOW);
            return alg_poly_zero();
        }
        result.coef[i - 1] = new_coef;
    }

    alg_poly_normalize(&result);
    alg_set_error_poly(err, ALG_POLY_OK);
    return result;
}

LV00_PUBLIC_API int alg_poly_to_string(const AlgPoly *p, char *buf, size_t size)
{
    if (!p) {
        return snprintf(buf, size, "(null)");
    }

    int written = 0;
    bool first = true;

    for (int i = p->degree; i >= 0; i--) {
        if (p->coef[i] == 0) continue;

        int n;
        size_t remaining = (size > (size_t)written) ? size - (size_t)written : 0;

        if (!first && remaining > 0) {
            if (p->coef[i] > 0) {
                n = snprintf(buf, remaining, " + ");
            } else {
                n = snprintf(buf, remaining, " - ");
            }
            written += n;
            if (written < (int)size) buf += n;
            remaining = (size > (size_t)written) ? size - (size_t)written : 0;
        }

        int64_t coef = (p->coef[i] < 0 && !first) ? -p->coef[i] : p->coef[i];

        if (i == 0) {
            n = snprintf(buf, remaining, "%lld", (long long)coef);
        } else if (i == 1) {
            if (coef == 1) {
                n = snprintf(buf, remaining, "x");
            } else {
                n = snprintf(buf, remaining, "%lld*x", (long long)coef);
            }
        } else {
            if (coef == 1) {
                n = snprintf(buf, remaining, "x^%d", i);
            } else {
                n = snprintf(buf, remaining, "%lld*x^%d", (long long)coef, i);
            }
        }

        written += n;
        if (written < (int)size) buf += n;
        first = false;
    }

    if (first) {
        /* 零多项式 */
        return snprintf(buf, size, "0");
    }

    return written;
}

LV00_PUBLIC_API const char *alg_poly_error_string(AlgPolyError err)
{
    switch (err) {
        case ALG_POLY_OK:          return "成功";
        case ALG_POLY_ERR_DEGREE:  return "次数超限";
        case ALG_POLY_ERR_OVERFLOW: return "整数溢出";
        case ALG_POLY_ERR_NULL:     return "空指针";
        case ALG_POLY_ERR_INVALID:  return "无效参数";
        case ALG_POLY_ERR_DIV_BY_ZERO: return "除以零多项式";
        default:                    return "未知错误";
    }
}

/* ============================================================
 * 跨层数域转换工具 —— 实现
 * ============================================================ */

LV00_PUBLIC_API AlgInterval alg_quadratic_to_interval(const AlgQuadratic *x,
                                                       AlgIntervalError *err)
{
    return alg_interval_from_quadratic(x, err);
}

LV00_PUBLIC_API AlgInterval alg_rational_to_interval(const AlgRational *r)
{
    return alg_interval_point(r);
}

LV00_PUBLIC_API bool alg_has_real_roots(int64_t a, int64_t b, int64_t c)
{
    if (a == 0) {
        /* 退化为一次方程 bx + c = 0，总有实根（若 b != 0） */
        return b != 0;
    }
    /* 判别式 b^2 - 4ac >= 0 */
    int64_t b_sq, four_ac, disc;
    if (alg_mul_overflow(b, b, &b_sq) ||
        alg_mul_overflow(4, a, &four_ac) ||
        alg_mul_overflow(four_ac, c, &four_ac) ||
        alg_sub_overflow(b_sq, four_ac, &disc)) {
        /* 溢出时使用 double 近似 */
        double d_disc = (double)b * (double)b - 4.0 * (double)a * (double)c;
        return d_disc >= 0.0;
    }
    return disc >= 0;
}
