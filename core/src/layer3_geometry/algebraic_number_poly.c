/**
 * @file algebraic_number_poly.c
 * @brief 代数数域封装 —— 第四层：多项式系统
 *
 * @details 由 algebraic_number.c 按数域类型拆分而来。
 *          所有运算基于 int64_t，不依赖 GMP 等外部库。
 *
 * @version 3.5.0
 * @copyright Copyright (c) 2024-2026 Lv-00 Project
 */

#include "lv/algebraic_number.h"
#include "lv/lv_strbuf.h"
#include "lv/lv_xmacro.h"

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "algebraic_number_internal.h"

#include "lv_internal.h"

/** @brief 设置错误码（若 err 非空） */
static void alg_set_error_poly(AlgPolyError *err, AlgPolyError code) {
    if (err)
        *err = code;
}

/* ============================================================
 * 第四层：多项式系统（简化版） —— 实现
 * ============================================================ */

/**
 * @brief 规范化多项式（去除高次零系数）
 */
static void lv_alg_poly_normalize(AlgPoly *p) {
    while (p->degree > 0 && p->coef[p->degree] == 0) {
        p->degree--;
    }
}

lv_PUBLIC_API AlgPoly lv_alg_poly_zero(void) {
    AlgPoly p;
    memset(p.coef, 0, sizeof(p.coef));
    p.degree = 0;
    return p;
}

lv_PUBLIC_API AlgPoly lv_alg_poly_const(int64_t c) {
    AlgPoly p;
    memset(p.coef, 0, sizeof(p.coef));
    p.coef[0] = c;
    p.degree = 0;
    lv_alg_poly_normalize(&p);
    return p;
}

lv_PUBLIC_API AlgPoly lv_alg_poly_linear(int64_t a, int64_t b) {
    AlgPoly p;
    memset(p.coef, 0, sizeof(p.coef));
    p.coef[1] = a;
    p.coef[0] = b;
    p.degree = (a != 0) ? 1 : 0;
    lv_alg_poly_normalize(&p);
    return p;
}

lv_PUBLIC_API AlgPoly lv_alg_poly_quadratic(int64_t a, int64_t b, int64_t c) {
    AlgPoly p;
    memset(p.coef, 0, sizeof(p.coef));
    p.coef[2] = a;
    p.coef[1] = b;
    p.coef[0] = c;
    p.degree = (a != 0) ? 2 : ((b != 0) ? 1 : 0);
    lv_alg_poly_normalize(&p);
    return p;
}

lv_PUBLIC_API AlgPoly lv_alg_poly_x(void) {
    return lv_alg_poly_linear(1, 0);
}

lv_PUBLIC_API int64_t lv_alg_poly_eval_int(const AlgPoly *p, int64_t n, AlgPolyError *err) {
    if (!p) {
        alg_set_error_poly(err, lv_alg_poly_ERR_NULL);
        return 0;
    }

    /* Horner 方法 */
    int64_t result = 0;
    int64_t overflow_flag = 0; /* 简单溢出检测标志 */
    for (int i = p->degree; i >= 0; i--) {
        int64_t tmp;
        if (alg_mul_overflow(result, n, &tmp) || alg_add_overflow(tmp, p->coef[i], &result)) {
            /* 溢出时回退到 double 计算 */
            alg_set_error_poly(err, lv_alg_poly_ERR_OVERFLOW);
            double d_result = 0.0;
            for (int j = p->degree; j >= 0; j--) {
                d_result = d_result * (double) n + (double) p->coef[j];
            }
            /* 钳制到 int64 安全范围再转换，避免大值时未定义行为 */
            if (d_result > 9223372036854774784.0)
                d_result = 9223372036854774784.0;
            if (d_result < -9223372036854774784.0)
                d_result = -9223372036854774784.0;
            return (int64_t) d_result;
        }
    }

    alg_set_error_poly(err, lv_alg_poly_OK);
    (void) overflow_flag;
    return result;
}

lv_PUBLIC_API AlgRational lv_alg_poly_eval_rational(const AlgPoly *p, const AlgRational *r, AlgPolyError *err) {
    if (!p || !r) {
        alg_set_error_poly(err, lv_alg_poly_ERR_NULL);
        return lv_alg_rational_zero();
    }

    /* Horner 方法（有理数版本） */
    AlgRational result = lv_alg_rational_zero();
    AlgRationalError r_err;

    for (int i = p->degree; i >= 0; i--) {
        result = lv_alg_rational_mul(&result, r, &r_err);
        AlgRational coef_r = lv_alg_rational_from_int(p->coef[i]);
        result = lv_alg_rational_add(&result, &coef_r, &r_err);
    }

    alg_set_error_poly(err, lv_alg_poly_OK);
    return result;
}

lv_PUBLIC_API AlgPoly lv_alg_poly_add(const AlgPoly *p, const AlgPoly *q, AlgPolyError *err) {
    if (!p || !q) {
        alg_set_error_poly(err, lv_alg_poly_ERR_NULL);
        return lv_alg_poly_zero();
    }

    AlgPoly result;
    memset(result.coef, 0, sizeof(result.coef));

    int max_deg = (p->degree > q->degree) ? p->degree : q->degree;
    for (int i = 0; i <= max_deg; i++) {
        int64_t a = (i <= p->degree) ? p->coef[i] : 0;
        int64_t b = (i <= q->degree) ? q->coef[i] : 0;
        if (alg_add_overflow(a, b, &result.coef[i])) {
            alg_set_error_poly(err, lv_alg_poly_ERR_OVERFLOW);
            return lv_alg_poly_zero();
        }
    }
    result.degree = max_deg;
    lv_alg_poly_normalize(&result);

    alg_set_error_poly(err, lv_alg_poly_OK);
    return result;
}

lv_PUBLIC_API AlgPoly lv_alg_poly_sub(const AlgPoly *p, const AlgPoly *q, AlgPolyError *err) {
    if (!p || !q) {
        alg_set_error_poly(err, lv_alg_poly_ERR_NULL);
        return lv_alg_poly_zero();
    }

    AlgPoly result;
    memset(result.coef, 0, sizeof(result.coef));

    int max_deg = (p->degree > q->degree) ? p->degree : q->degree;
    for (int i = 0; i <= max_deg; i++) {
        int64_t a = (i <= p->degree) ? p->coef[i] : 0;
        int64_t b = (i <= q->degree) ? q->coef[i] : 0;
        if (alg_sub_overflow(a, b, &result.coef[i])) {
            alg_set_error_poly(err, lv_alg_poly_ERR_OVERFLOW);
            return lv_alg_poly_zero();
        }
    }
    result.degree = max_deg;
    lv_alg_poly_normalize(&result);

    alg_set_error_poly(err, lv_alg_poly_OK);
    return result;
}

lv_PUBLIC_API AlgPoly lv_alg_poly_mul(const AlgPoly *p, const AlgPoly *q, AlgPolyError *err) {
    if (!p || !q) {
        alg_set_error_poly(err, lv_alg_poly_ERR_NULL);
        return lv_alg_poly_zero();
    }

    int new_deg = p->degree + q->degree;
    if (new_deg > lv_alg_poly_MAX_DEGREE) {
        alg_set_error_poly(err, lv_alg_poly_ERR_DEGREE);
        return lv_alg_poly_zero();
    }

    AlgPoly result;
    memset(result.coef, 0, sizeof(result.coef));
    result.degree = new_deg;

    for (int i = 0; i <= p->degree; i++) {
        for (int j = 0; j <= q->degree; j++) {
            int64_t prod;
            if (alg_mul_overflow(p->coef[i], q->coef[j], &prod)) {
                alg_set_error_poly(err, lv_alg_poly_ERR_OVERFLOW);
                return lv_alg_poly_zero();
            }
            int64_t sum;
            if (alg_add_overflow(result.coef[i + j], prod, &sum)) {
                alg_set_error_poly(err, lv_alg_poly_ERR_OVERFLOW);
                return lv_alg_poly_zero();
            }
            result.coef[i + j] = sum;
        }
    }

    lv_alg_poly_normalize(&result);
    alg_set_error_poly(err, lv_alg_poly_OK);
    return result;
}

lv_PUBLIC_API AlgPoly lv_alg_poly_neg(const AlgPoly *p) {
    AlgPoly result;
    for (int i = 0; i <= lv_alg_poly_MAX_DEGREE; i++) {
        result.coef[i] = -p->coef[i];
    }
    result.degree = p->degree;
    return result;
}

lv_PUBLIC_API int64_t lv_alg_poly_lead_coef(const AlgPoly *p) {
    return p->coef[p->degree];
}

lv_PUBLIC_API int64_t lv_alg_poly_const_coef(const AlgPoly *p) {
    return p->coef[0];
}

lv_PUBLIC_API bool lv_alg_poly_is_zero(const AlgPoly *p) {
    return p->degree == 0 && p->coef[0] == 0;
}

lv_PUBLIC_API bool lv_alg_poly_is_const(const AlgPoly *p) {
    return p->degree == 0;
}

lv_PUBLIC_API int64_t lv_alg_poly_discriminant(const AlgPoly *p, AlgPolyError *err) {
    if (!p) {
        alg_set_error_poly(err, lv_alg_poly_ERR_NULL);
        return 0;
    }

    switch (p->degree) {
        case 0:
            alg_set_error_poly(err, lv_alg_poly_OK);
            return 0;
        case 1:
            /* ax + b: 判别式为 1 */
            alg_set_error_poly(err, lv_alg_poly_OK);
            return 1;
        case 2: {
            /* ax^2 + bx + c: 判别式为 b^2 - 4ac */
            int64_t b_sq, four_ac, disc;
            if (alg_mul_overflow(p->coef[1], p->coef[1], &b_sq) || alg_mul_overflow(4, p->coef[2], &four_ac) ||
                alg_mul_overflow(four_ac, p->coef[0], &four_ac) || alg_sub_overflow(b_sq, four_ac, &disc)) {
                alg_set_error_poly(err, lv_alg_poly_ERR_OVERFLOW);
                return 0;
            }
            alg_set_error_poly(err, lv_alg_poly_OK);
            return disc;
        }
        default:
            alg_set_error_poly(err, lv_alg_poly_ERR_DEGREE);
            return 0;
    }
}

lv_PUBLIC_API int lv_alg_poly_rational_roots(const AlgPoly *p, AlgRational *roots, int max_roots, AlgPolyError *err) {
    if (!p || !roots || max_roots <= 0) {
        alg_set_error_poly(err, lv_alg_poly_ERR_NULL);
        return 0;
    }

    int found = 0;

    if (p->degree == 0) {
        alg_set_error_poly(err, lv_alg_poly_OK);
        return 0;
    }

    if (p->degree == 1) {
        /* ax + b = 0 => x = -b/a */
        AlgRationalError r_err;
        roots[found] = lv_alg_rational_create(-p->coef[0], p->coef[1], &r_err);
        found++;
        alg_set_error_poly(err, lv_alg_poly_OK);
        return found;
    }

    if (p->degree == 2) {
        /* ax^2 + bx + c = 0 */
        int64_t disc = lv_alg_poly_discriminant(p, err);
        if (err && *err != lv_alg_poly_OK)
            return 0;

        if (disc < 0) {
            /* 无实根 */
            alg_set_error_poly(err, lv_alg_poly_OK);
            return 0;
        }

        if (disc == 0) {
            /* 重根 x = -b/(2a) */
            AlgRationalError r_err;
            roots[found] = lv_alg_rational_create(-p->coef[1], 2 * p->coef[2], &r_err);
            found++;
            alg_set_error_poly(err, lv_alg_poly_OK);
            return found;
        }

        /* 两个不同的实根 */
        /* x = (-b +/- sqrt(disc)) / (2a) */
        /* 先检查 disc 是否为完全平方数 */
        if (alg_is_perfect_square(disc)) {
            int64_t sqrt_disc = alg_isqrt(disc);
            AlgRationalError r_err;
            if (found < max_roots) {
                roots[found] = lv_alg_rational_create(-p->coef[1] - sqrt_disc, 2 * p->coef[2], &r_err);
                found++;
            }
            if (found < max_roots) {
                roots[found] = lv_alg_rational_create(-p->coef[1] + sqrt_disc, 2 * p->coef[2], &r_err);
                found++;
            }
        }
        /* 若 disc 不是完全平方数，则无有理根 */

        alg_set_error_poly(err, lv_alg_poly_OK);
        return found;
    }

    /* 高次多项式：使用有理根定理枚举候选 */
    if (p->coef[0] == 0) {
        /* x=0 是一个根，降次后递归 */
        if (found < max_roots) {
            roots[found] = lv_alg_rational_zero();
            found++;
        }
        /* 降次：p(x) / x */
        AlgPoly reduced;
        memset(reduced.coef, 0, sizeof(reduced.coef));
        reduced.degree = p->degree - 1;
        for (int i = 1; i <= p->degree; i++) {
            reduced.coef[i - 1] = p->coef[i];
        }
        lv_alg_poly_normalize(&reduced);
        int sub_found = lv_alg_poly_rational_roots(&reduced, roots + found, max_roots - found, err);
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
            AlgRational candidate = lv_alg_rational_create(p_factors[i], q_factors[j], &r_err);
            AlgRational val = lv_alg_poly_eval_rational(p, &candidate, NULL);
            if (lv_alg_rational_is_zero(&val)) {
                roots[found++] = candidate;
            }
            /* 负候选 */
            if (found < max_roots) {
                candidate = lv_alg_rational_create(-p_factors[i], q_factors[j], &r_err);
                val = lv_alg_poly_eval_rational(p, &candidate, NULL);
                if (lv_alg_rational_is_zero(&val)) {
                    roots[found++] = candidate;
                }
            }
        }
    }

    alg_set_error_poly(err, lv_alg_poly_OK);
    return found;
}

lv_PUBLIC_API AlgPoly lv_alg_poly_derivative(const AlgPoly *p, AlgPolyError *err) {
    if (!p) {
        alg_set_error_poly(err, lv_alg_poly_ERR_NULL);
        return lv_alg_poly_zero();
    }

    if (p->degree == 0) {
        /* 常数的导数为零 */
        alg_set_error_poly(err, lv_alg_poly_OK);
        return lv_alg_poly_zero();
    }

    AlgPoly result;
    memset(result.coef, 0, sizeof(result.coef));
    result.degree = p->degree - 1;

    for (int i = 1; i <= p->degree; i++) {
        int64_t new_coef;
        if (alg_mul_overflow((int64_t) i, p->coef[i], &new_coef)) {
            alg_set_error_poly(err, lv_alg_poly_ERR_OVERFLOW);
            return lv_alg_poly_zero();
        }
        result.coef[i - 1] = new_coef;
    }

    lv_alg_poly_normalize(&result);
    alg_set_error_poly(err, lv_alg_poly_OK);
    return result;
}

lv_PUBLIC_API int lv_alg_poly_to_string(const AlgPoly *p, char *buf, size_t size) {
    if (!p) {
        if (buf && size > 0) {
            snprintf(buf, size, "(null)");
        }
        return 6; /* strlen("(null)") */
    }

    lvStrBuf sb = {0};
    bool first = true;

    for (int i = p->degree; i >= 0; i--) {
        if (p->coef[i] == 0)
            continue;

        int64_t coef = p->coef[i];

        if (!first) {
            if (coef > 0) {
                lv_strbuf_printf(&sb, " + ");
            } else {
                lv_strbuf_printf(&sb, " - ");
                coef = -coef;
            }
        } else if (coef < 0) {
            lv_strbuf_printf(&sb, "-");
            coef = -coef;
        }

        if (i == 0) {
            lv_strbuf_printf(&sb, "%lld", (long long) coef);
        } else if (i == 1) {
            if (coef == 1) {
                lv_strbuf_printf(&sb, "x");
            } else {
                lv_strbuf_printf(&sb, "%lld*x", (long long) coef);
            }
        } else {
            if (coef == 1) {
                lv_strbuf_printf(&sb, "x^%d", i);
            } else {
                lv_strbuf_printf(&sb, "%lld*x^%d", (long long) coef, i);
            }
        }

        first = false;
    }

    if (first) {
        /* 零多项式 */
        int len = 1;
        if (buf && size > 0) {
            snprintf(buf, size, "0");
        }
        return len;
    }

    int len = (int) sb.len;
    if (buf && size > 0) {
        size_t copy = (sb.len < size - 1) ? sb.len : size - 1;
        memcpy(buf, lv_strbuf_cstr(&sb), copy);
        buf[copy] = '\0';
    }
    lv_strbuf_destroy(&sb);
    return len;
}

/** @brief lv_alg_poly_error_string 名称表（按枚举值升序） */
static const lvStrToEnumEntry s_alg_poly_error_string_entries[] = {
    {"成功", lv_alg_poly_OK},
    {"次数超限", lv_alg_poly_ERR_DEGREE},
    {"整数溢出", lv_alg_poly_ERR_OVERFLOW},
    {"空指针", lv_alg_poly_ERR_NULL},
    {"无效参数", lv_alg_poly_ERR_INVALID},
    {"除以零多项式", lv_alg_poly_ERR_DIV_BY_ZERO},
};

lv_PUBLIC_API const char *lv_alg_poly_error_string(AlgPolyError err) {
    return lv_enum_to_str(s_alg_poly_error_string_entries, lv_ARRAY_SIZE(s_alg_poly_error_string_entries), (int) err, "未知错误");
}

