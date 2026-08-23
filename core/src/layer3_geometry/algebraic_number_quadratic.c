/**
 * @file algebraic_number_quadratic.c
 * @brief 代数数域封装 —— 第二层：二次代数数域 Q(sqrt(d))
 *
 * @details 由 algebraic_number.c 按数域类型拆分而来。
 *          所有运算基于 int64_t，不依赖 GMP 等外部库。
 *
 * @section layering 分层边界（与 symbolics/quadratic.c 的关系）
 *
 * 本文件（lv_alg_quadratic_*）与 symbolics/quadratic.c（quadratic_*）实现同一
 * 数学域 Q(√d)，但精度载体不同，属分层设计，**不合并实现**：
 * - 本文件：AlgQuadratic 为 int64_t 栈值类型（AlgRational = num/den int64），
 *   轻量、无堆分配、无外部依赖，供代数数域链（algebraic_number.c 的
 *   lv_alg_* 系列，消费方见 lv/algebraic_number.h）使用；
 * - symbolics/quadratic.c：Quadratic 为 mpq_t 堆值类型（GMP 有理数），
 *   精确无溢出，供符号坐标链（symbolic_coord.*，消费方见 lv/symbolic_coord.h）
 *   使用。
 *
 * 两处 mul/div 的数学公式编码一致（mul: (a1a2+b1b2d)+(a1b2+a2b1)√d；
 * div: 乘共轭/范数），仅载体不同，属合理重复，不强行合并。
 * to_double 转换（lv_alg_quadratic_to_double vs quadratic_to_double）签名与
 * 载体不兼容（值类型 vs 堆类型、d 可含 0/负值 vs n 必须正无平方因子），
 * 不可互委派。
 *
 * @version 3.5.0
 * @copyright Copyright (c) 2024-2026 Lv-00 Project
 */

#include "lv/algebraic_number.h"
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
static void alg_set_error_quadratic(AlgQuadraticError *err, AlgQuadraticError code) {
    if (err)
        *err = code;
}

/* ============================================================
 * 第二层：二次代数数域 Q(sqrt(d)) —— 实现
 * ============================================================ */

AlgQuadratic lv_alg_quadratic_create(int64_t a_val, int64_t a_den, int64_t b_val, int64_t b_den, int64_t d,
                                                AlgQuadraticError *err) {
    AlgQuadratic result;
    memset(&result, 0, sizeof(result));

    if (d < 0) {
        alg_set_error_quadratic(err, lv_alg_quadratic_ERR_INVALID);
        result.d = 0;
        return result;
    }

    AlgRationalError r_err;
    result.a = lv_alg_rational_create(a_val, a_den, &r_err);
    if (r_err != lv_alg_rational_OK) {
        alg_set_error_quadratic(err, (r_err == lv_alg_rational_ERR_ZERO_DEN)
                                        ? lv_alg_quadratic_ERR_INVALID
                                        : lv_alg_quadratic_ERR_OVERFLOW);
        result.d = 0;
        return result;
    }
    result.b = lv_alg_rational_create(b_val, b_den, &r_err);
    if (r_err != lv_alg_rational_OK) {
        alg_set_error_quadratic(err, (r_err == lv_alg_rational_ERR_ZERO_DEN)
                                        ? lv_alg_quadratic_ERR_INVALID
                                        : lv_alg_quadratic_ERR_OVERFLOW);
        result.d = 0;
        return result;
    }
    result.d = d;

    alg_set_error_quadratic(err, lv_alg_quadratic_OK);
    return result;
}

AlgQuadratic lv_alg_quadratic_from_rational(const AlgRational *r, int64_t d) {
    AlgQuadratic q;
    q.a = *r;
    q.b = lv_alg_rational_zero();
    q.d = d;
    return q;
}

AlgQuadratic lv_alg_quadratic_sqrt(int64_t b_val, int64_t b_den, int64_t d, AlgQuadraticError *err) {
    return lv_alg_quadratic_create(0, 1, b_val, b_den, d, err);
}

AlgQuadratic lv_alg_quadratic_add(const AlgQuadratic *x, const AlgQuadratic *y, AlgQuadraticError *err) {
    if (!x || !y) {
        alg_set_error_quadratic(err, lv_alg_quadratic_ERR_NULL);
        AlgQuadratic z;
        memset(&z, 0, sizeof(z));
        return z;
    }
    if (x->d != y->d) {
        alg_set_error_quadratic(err, lv_alg_quadratic_ERR_DOMAIN);
        AlgQuadratic z;
        memset(&z, 0, sizeof(z));
        return z;
    }

    AlgQuadratic result;
    result.a = lv_alg_rational_add(&x->a, &y->a, NULL);
    result.b = lv_alg_rational_add(&x->b, &y->b, NULL);
    result.d = x->d;
    alg_set_error_quadratic(err, lv_alg_quadratic_OK);
    return result;
}

AlgQuadratic lv_alg_quadratic_sub(const AlgQuadratic *x, const AlgQuadratic *y, AlgQuadraticError *err) {
    if (!x || !y) {
        alg_set_error_quadratic(err, lv_alg_quadratic_ERR_NULL);
        AlgQuadratic z;
        memset(&z, 0, sizeof(z));
        return z;
    }
    if (x->d != y->d) {
        alg_set_error_quadratic(err, lv_alg_quadratic_ERR_DOMAIN);
        AlgQuadratic z;
        memset(&z, 0, sizeof(z));
        return z;
    }

    AlgQuadratic result;
    result.a = lv_alg_rational_sub(&x->a, &y->a, NULL);
    result.b = lv_alg_rational_sub(&x->b, &y->b, NULL);
    result.d = x->d;
    alg_set_error_quadratic(err, lv_alg_quadratic_OK);
    return result;
}

AlgQuadratic lv_alg_quadratic_mul(const AlgQuadratic *x, const AlgQuadratic *y, AlgQuadraticError *err) {
    if (!x || !y) {
        alg_set_error_quadratic(err, lv_alg_quadratic_ERR_NULL);
        AlgQuadratic z;
        memset(&z, 0, sizeof(z));
        return z;
    }
    if (x->d != y->d) {
        alg_set_error_quadratic(err, lv_alg_quadratic_ERR_DOMAIN);
        AlgQuadratic z;
        memset(&z, 0, sizeof(z));
        return z;
    }

    /*
     * (a1 + b1*sqrt(d)) * (a2 + b2*sqrt(d))
     *   = (a1*a2 + b1*b2*d) + (a1*b2 + a2*b1)*sqrt(d)
     */
    AlgRational d_rat = lv_alg_rational_from_int(x->d);
    AlgRationalError r_err;

    /* 溢出时统一返回零元 + ERR_OVERFLOW（局部宏收敛 7 处重复错误块） */
#define lv_QUAD_MUL_CHECK()                                    \
    do {                                                       \
        if (r_err != lv_alg_rational_OK) {                     \
            alg_set_error_quadratic(err, lv_alg_quadratic_ERR_OVERFLOW); \
            AlgRational zz = {0, 1};                           \
            AlgQuadratic ret = {zz, zz, 0};                    \
            return ret;                                        \
        }                                                      \
    } while (0)

    AlgRational _t1 = lv_alg_rational_mul(&x->a, &y->a, &r_err);
    lv_QUAD_MUL_CHECK();
    AlgRational _t2 = lv_alg_rational_mul(&x->b, &y->b, &r_err);
    lv_QUAD_MUL_CHECK();
    AlgRational _t3 = lv_alg_rational_mul(&_t2, &d_rat, &r_err);
    lv_QUAD_MUL_CHECK();
    AlgRational new_a = lv_alg_rational_add(&_t1, &_t3, &r_err);
    lv_QUAD_MUL_CHECK();

    AlgRational _t4 = lv_alg_rational_mul(&x->a, &y->b, &r_err);
    lv_QUAD_MUL_CHECK();
    AlgRational _t5 = lv_alg_rational_mul(&x->b, &y->a, &r_err);
    lv_QUAD_MUL_CHECK();
    AlgRational new_b = lv_alg_rational_add(&_t4, &_t5, &r_err);
    lv_QUAD_MUL_CHECK();

#undef lv_QUAD_MUL_CHECK

    AlgQuadratic result;
    result.a = new_a;
    result.b = new_b;
    result.d = x->d;
    alg_set_error_quadratic(err, lv_alg_quadratic_OK);
    return result;
}

AlgQuadratic lv_alg_quadratic_div(const AlgQuadratic *x, const AlgQuadratic *y, AlgQuadraticError *err) {
    if (!x || !y) {
        alg_set_error_quadratic(err, lv_alg_quadratic_ERR_NULL);
        AlgQuadratic z;
        memset(&z, 0, sizeof(z));
        return z;
    }
    if (x->d != y->d) {
        alg_set_error_quadratic(err, lv_alg_quadratic_ERR_DOMAIN);
        AlgQuadratic z;
        memset(&z, 0, sizeof(z));
        return z;
    }

    /*
     * x / y = x * conj(y) / norm(y)
     * 其中 conj(y) = a - b*sqrt(d)
     *       norm(y) = a^2 - b^2*d
     */
    AlgQuadraticError local_err = lv_alg_quadratic_OK;
    AlgQuadratic conj_y = lv_alg_quadratic_conj(y);
    AlgRational norm_y = lv_alg_quadratic_norm(y, &local_err);
    if (local_err != lv_alg_quadratic_OK) {
        alg_set_error_quadratic(err, local_err);
        AlgQuadratic z;
        memset(&z, 0, sizeof(z));
        return z;
    }
    if (lv_alg_rational_is_zero(&norm_y)) {
        alg_set_error_quadratic(err, lv_alg_quadratic_ERR_INVALID);
        AlgQuadratic z;
        memset(&z, 0, sizeof(z));
        return z;
    }

    AlgQuadratic numerator = lv_alg_quadratic_mul(x, &conj_y, &local_err);
    if (local_err != lv_alg_quadratic_OK) {
        alg_set_error_quadratic(err, local_err);
        AlgQuadratic z;
        memset(&z, 0, sizeof(z));
        return z;
    }

    /* 将结果除以范数（有理数） */
    AlgRationalError r_err;
    numerator.a = lv_alg_rational_div(&numerator.a, &norm_y, &r_err);
    if (r_err != lv_alg_rational_OK) {
        alg_set_error_quadratic(err, lv_alg_quadratic_ERR_OVERFLOW);
        AlgQuadratic z;
        memset(&z, 0, sizeof(z));
        return z;
    }
    numerator.b = lv_alg_rational_div(&numerator.b, &norm_y, &r_err);
    if (r_err != lv_alg_rational_OK) {
        alg_set_error_quadratic(err, lv_alg_quadratic_ERR_OVERFLOW);
        AlgQuadratic z;
        memset(&z, 0, sizeof(z));
        return z;
    }

    alg_set_error_quadratic(err, lv_alg_quadratic_OK);
    return numerator;
}

AlgQuadratic lv_alg_quadratic_neg(const AlgQuadratic *x) {
    AlgQuadratic result;
    result.a = lv_alg_rational_neg(&x->a);
    result.b = lv_alg_rational_neg(&x->b);
    result.d = x->d;
    return result;
}

AlgQuadratic lv_alg_quadratic_conj(const AlgQuadratic *x) {
    AlgQuadratic result;
    result.a = x->a;
    result.b = lv_alg_rational_neg(&x->b);
    result.d = x->d;
    return result;
}

AlgRational lv_alg_quadratic_norm(const AlgQuadratic *x, AlgQuadraticError *err) {
    if (!x) {
        alg_set_error_quadratic(err, lv_alg_quadratic_ERR_NULL);
        return lv_alg_rational_zero();
    }

    /*
     * N(x) = x * conj(x) = (a + b*sqrt(d)) * (a - b*sqrt(d))
     *      = a^2 - b^2 * d
     */
    AlgRationalError r_err;
    AlgRational a_sq = lv_alg_rational_mul(&x->a, &x->a, &r_err);
    if (r_err != lv_alg_rational_OK) {
        alg_set_error_quadratic(err, lv_alg_quadratic_ERR_OVERFLOW);
        return lv_alg_rational_zero();
    }
    AlgRational b_sq = lv_alg_rational_mul(&x->b, &x->b, &r_err);
    if (r_err != lv_alg_rational_OK) {
        alg_set_error_quadratic(err, lv_alg_quadratic_ERR_OVERFLOW);
        return lv_alg_rational_zero();
    }
    AlgRational d_rat = lv_alg_rational_from_int(x->d);
    AlgRational b_sq_d = lv_alg_rational_mul(&b_sq, &d_rat, &r_err);
    if (r_err != lv_alg_rational_OK) {
        alg_set_error_quadratic(err, lv_alg_quadratic_ERR_OVERFLOW);
        return lv_alg_rational_zero();
    }
    AlgRational norm = lv_alg_rational_sub(&a_sq, &b_sq_d, &r_err);
    if (r_err != lv_alg_rational_OK) {
        alg_set_error_quadratic(err, lv_alg_quadratic_ERR_OVERFLOW);
        return lv_alg_rational_zero();
    }

    alg_set_error_quadratic(err, lv_alg_quadratic_OK);
    return norm;
}

int lv_alg_quadratic_cmp(const AlgQuadratic *x, const AlgQuadratic *y) {
    double dx = lv_alg_quadratic_to_double(x);
    double dy = lv_alg_quadratic_to_double(y);
    if (dx < dy)
        return -1;
    if (dx > dy)
        return 1;
    return 0;
}

int lv_alg_quadratic_cmp_exact(const AlgQuadratic *x, const AlgQuadratic *y, AlgQuadraticError *err) {
    if (!x || !y) {
        alg_set_error_quadratic(err, lv_alg_quadratic_ERR_NULL);
        return 0;
    }
    if (x->d != y->d) {
        alg_set_error_quadratic(err, lv_alg_quadratic_ERR_DOMAIN);
        return 0;
    }
    if (x->d < 0) {
        /* d < 0 无法构成实数域，按无效参数处理 */
        alg_set_error_quadratic(err, lv_alg_quadratic_ERR_INVALID);
        return 0;
    }

    /* 精确比较：x - y = (a1-a2) + (b1-b2)*sqrt(d) = da + db*sqrt(d) */
    AlgRational diff_a = lv_alg_rational_sub(&x->a, &y->a, NULL);
    AlgRational diff_b = lv_alg_rational_sub(&x->b, &y->b, NULL);

    /* 若 db == 0 或 d == 0（sqrt(0)=0），退化为纯有理数比较 */
    if (lv_alg_rational_is_zero(&diff_b) || x->d == 0) {
        AlgRational _zero = lv_alg_rational_zero();
        alg_set_error_quadratic(err, lv_alg_quadratic_OK);
        return lv_alg_rational_cmp(&diff_a, &_zero);
    }

    /* 若 da == 0：diff = db*sqrt(d)，d > 0 时符号 = sign(db) */
    if (lv_alg_rational_is_zero(&diff_a)) {
        alg_set_error_quadratic(err, lv_alg_quadratic_OK);
        return diff_b.num > 0 ? 1 : -1;
    }

    /* 若 da 与 db 同号：diff 符号即该号（sqrt(d) >= 0 不改变符号） */
    if ((diff_a.num > 0) == (diff_b.num > 0)) {
        alg_set_error_quadratic(err, lv_alg_quadratic_OK);
        return diff_a.num > 0 ? 1 : -1;
    }

    /* 异号：diff 符号 = sign(da² - db²*d)（两侧非负，平方保持序） */
    /* 使用 GMP 任意精度有理数精确比较，避免 int64 溢出（先例：rational.c） */
    {
        char buf[32];
        mpq_t qa2, qb2, qd;
        mpq_init(qa2);
        mpq_init(qb2);
        mpq_init(qd);

        /* qa2 = diff_a² */
        lv_snprintf(buf, sizeof(buf), "%lld", (long long) diff_a.num);
        mpq_set_str(qa2, buf, 10);
        lv_snprintf(buf, sizeof(buf), "%lld", (long long) diff_a.den);
        mpz_set_str(mpq_denref(qa2), buf, 10);
        mpq_canonicalize(qa2);
        mpq_mul(qa2, qa2, qa2);

        /* qb2 = diff_b² * d */
        lv_snprintf(buf, sizeof(buf), "%lld", (long long) diff_b.num);
        mpq_set_str(qb2, buf, 10);
        lv_snprintf(buf, sizeof(buf), "%lld", (long long) diff_b.den);
        mpz_set_str(mpq_denref(qb2), buf, 10);
        mpq_canonicalize(qb2);
        mpq_mul(qb2, qb2, qb2);
        lv_snprintf(buf, sizeof(buf), "%lld", (long long) x->d);
        mpq_set_str(qd, buf, 10);
        mpq_mul(qb2, qb2, qd);

        int s = mpq_cmp(qa2, qb2); /* da² vs db²*d */
        mpq_clear(qa2);
        mpq_clear(qb2);
        mpq_clear(qd);

        alg_set_error_quadratic(err, lv_alg_quadratic_OK);
        /* da > 0, db < 0：diff > 0 ⟺ da² > db²*d ⟺ s > 0 */
        /* da < 0, db > 0：diff > 0 ⟺ da² < db²*d ⟺ s < 0 */
        if (diff_a.num > 0)
            return (s > 0) ? 1 : (s < 0) ? -1 : 0;
        return (s < 0) ? 1 : (s > 0) ? -1 : 0;
    }
}

double lv_alg_quadratic_to_double(const AlgQuadratic *x) {
    double a = lv_alg_rational_to_double(&x->a);
    double b = lv_alg_rational_to_double(&x->b);
    double sqrt_d;
    if (x->d == 0) {
        sqrt_d = 0.0;
    } else if (x->d < 0) {
        /* 负判别式：二次无理数无法表示为实数，只返回有理部分 */
        return a;
    } else {
        sqrt_d = sqrt((double) x->d);
    }
    return a + b * sqrt_d;
}

int lv_alg_quadratic_to_string(const AlgQuadratic *x, char *buf, size_t size) {
    lvStrBuf sb = {0};

    /* 有理部分 a */
    lv_strbuf_printf(&sb, "%lld", (long long) x->a.num);
    if (x->a.den != 1) {
        lv_strbuf_printf(&sb, "/%lld", (long long) x->a.den);
    }

    /* sqrt(d) 部分：d==0 时 sqrt(0)=0，b 分量无意义，忽略 */
    if (!lv_alg_rational_is_zero(&x->b) && x->d != 0) {
        if (lv_alg_rational_is_positive(&x->b)) {
            lv_strbuf_printf(&sb, " + ");
        } else {
            lv_strbuf_printf(&sb, " - ");
        }

        /* 系数绝对值 */
        AlgRational abs_b = lv_alg_rational_abs(&x->b);
        AlgRational _one = lv_alg_rational_one();
        if (!lv_alg_rational_eq(&abs_b, &_one)) {
            lv_strbuf_printf(&sb, "%lld", (long long) abs_b.num);
            if (abs_b.den != 1) {
                lv_strbuf_printf(&sb, "/%lld", (long long) abs_b.den);
            }
            lv_strbuf_printf(&sb, "*");
        }

        /* sqrt(d) */
        if (x->d != 0) {
            lv_strbuf_printf(&sb, "sqrt(%lld)", (long long) x->d);
        }
    }

    int len = (int) sb.len;
    if (buf && size > 0) {
        lv_strlcpy(buf, lv_strbuf_cstr(&sb), size);
    }
    lv_strbuf_destroy(&sb);
    return len;
}

bool lv_alg_quadratic_is_rational(const AlgQuadratic *x) {
    return lv_alg_rational_is_zero(&x->b);
}

AlgRational lv_alg_quadratic_rational_part(const AlgQuadratic *x) {
    return x->a;
}

/** @brief lv_alg_quadratic_error_string 名称表（按枚举值升序） */
static const lvStrToEnumEntry s_alg_quadratic_error_string_entries[] = {
    {"成功", lv_alg_quadratic_OK},
    {"域不匹配（d 不同）", lv_alg_quadratic_ERR_DOMAIN},
    {"整数溢出", lv_alg_quadratic_ERR_OVERFLOW},
    {"空指针", lv_alg_quadratic_ERR_NULL},
    {"无效参数", lv_alg_quadratic_ERR_INVALID},
};

const char *lv_alg_quadratic_error_string(AlgQuadraticError err) {
    return lv_enum_to_str(s_alg_quadratic_error_string_entries, lv_ARRAY_SIZE(s_alg_quadratic_error_string_entries), (int) err, "未知错误");
}
