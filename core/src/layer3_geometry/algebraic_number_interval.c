/**
 * @file algebraic_number_interval.c
 * @brief 代数数域封装 —— 第三层：区间运算
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
static void alg_set_error_interval(AlgIntervalError *err, AlgIntervalError code) {
    if (err)
        *err = code;
}

/* ============================================================
 * 第三层：区间运算 —— 实现
 * ============================================================ */

AlgInterval lv_alg_interval_create(int64_t lo_val, int64_t lo_den, int64_t hi_val, int64_t hi_den,
                                              AlgIntervalError *err) {
    AlgInterval result;
    AlgRationalError r_err;

    result.lo = lv_alg_rational_create(lo_val, lo_den, &r_err);
    result.hi = lv_alg_rational_create(hi_val, hi_den, &r_err);

    /* 确保 lo <= hi */
    if (lv_alg_rational_cmp(&result.lo, &result.hi) > 0) {
        lv_SWAP(AlgRational, result.lo, result.hi);
    }

    alg_set_error_interval(err, lv_alg_interval_OK);
    return result;
}

AlgInterval lv_alg_interval_point(const AlgRational *r) {
    AlgInterval iv;
    iv.lo = *r;
    iv.hi = *r;
    return iv;
}

AlgInterval lv_alg_interval_from_quadratic(const AlgQuadratic *x, AlgIntervalError *err) {
    if (!x) {
        alg_set_error_interval(err, lv_alg_interval_ERR_NULL);
        AlgInterval iv;
        iv.lo = lv_alg_rational_zero();
        iv.hi = lv_alg_rational_zero();
        return iv;
    }

    /* 若 d 为完全平方数，可计算精确值 */
    if (x->d == 0 || alg_is_perfect_square(x->d)) {
        int64_t sqrt_d = (x->d == 0) ? 0 : alg_isqrt(x->d);
        AlgRationalError r_err;
        AlgRational sqrt_d_rat = lv_alg_rational_from_int(sqrt_d);
        AlgRational b_sqrt_d = lv_alg_rational_mul(&x->b, &sqrt_d_rat, &r_err);
        AlgRational exact = lv_alg_rational_add(&x->a, &b_sqrt_d, &r_err);
        alg_set_error_interval(err, lv_alg_interval_OK);
        return lv_alg_interval_point(&exact);
    }

    /* 否则使用包围区间 */
    double val = lv_alg_quadratic_to_double(x);
    /* 使用有理数近似包围 */
    /* lo = floor(val) - 1, hi = ceil(val) + 1 作为粗略包围 */
    /* 钳制到 int64 安全范围再转换，避免大值时 floor/ceil 结果的未定义行为 */
    double clamped_val = val;
    if (clamped_val > 9223372036854774784.0)
        clamped_val = 9223372036854774784.0;
    if (clamped_val < -9223372036854774784.0)
        clamped_val = -9223372036854774784.0;
    int64_t lo_int = (int64_t) floor(clamped_val) - 1;
    int64_t hi_int = (int64_t) ceil(clamped_val) + 1;

    alg_set_error_interval(err, lv_alg_interval_OK);
    return lv_alg_interval_create(lo_int, 1, hi_int, 1, NULL);
}

AlgInterval lv_alg_interval_add(const AlgInterval *x, const AlgInterval *y, AlgIntervalError *err) {
    if (!x || !y) {
        alg_set_error_interval(err, lv_alg_interval_ERR_NULL);
        AlgInterval iv;
        iv.lo = iv.hi = lv_alg_rational_zero();
        return iv;
    }

    AlgInterval result;
    result.lo = lv_alg_rational_add(&x->lo, &y->lo, NULL);
    result.hi = lv_alg_rational_add(&x->hi, &y->hi, NULL);
    alg_set_error_interval(err, lv_alg_interval_OK);
    return result;
}

AlgInterval lv_alg_interval_sub(const AlgInterval *x, const AlgInterval *y, AlgIntervalError *err) {
    if (!x || !y) {
        alg_set_error_interval(err, lv_alg_interval_ERR_NULL);
        AlgInterval iv;
        iv.lo = iv.hi = lv_alg_rational_zero();
        return iv;
    }

    AlgInterval result;
    result.lo = lv_alg_rational_sub(&x->lo, &y->hi, NULL);
    result.hi = lv_alg_rational_sub(&x->hi, &y->lo, NULL);
    alg_set_error_interval(err, lv_alg_interval_OK);
    return result;
}

AlgInterval lv_alg_interval_mul(const AlgInterval *x, const AlgInterval *y, AlgIntervalError *err) {
    if (!x || !y) {
        alg_set_error_interval(err, lv_alg_interval_ERR_NULL);
        AlgInterval iv;
        iv.lo = iv.hi = lv_alg_rational_zero();
        return iv;
    }

    /* 计算四个端点乘积，取最小和最大 */
    AlgRational p1 = lv_alg_rational_mul(&x->lo, &y->lo, NULL);
    AlgRational p2 = lv_alg_rational_mul(&x->lo, &y->hi, NULL);
    AlgRational p3 = lv_alg_rational_mul(&x->hi, &y->lo, NULL);
    AlgRational p4 = lv_alg_rational_mul(&x->hi, &y->hi, NULL);

    AlgRational lo = p1, hi = p1;
    if (lv_alg_rational_cmp(&p2, &lo) < 0)
        lo = p2;
    if (lv_alg_rational_cmp(&p3, &lo) < 0)
        lo = p3;
    if (lv_alg_rational_cmp(&p4, &lo) < 0)
        lo = p4;
    if (lv_alg_rational_cmp(&p2, &hi) > 0)
        hi = p2;
    if (lv_alg_rational_cmp(&p3, &hi) > 0)
        hi = p3;
    if (lv_alg_rational_cmp(&p4, &hi) > 0)
        hi = p4;

    AlgInterval result;
    result.lo = lo;
    result.hi = hi;
    alg_set_error_interval(err, lv_alg_interval_OK);
    return result;
}

AlgInterval lv_alg_interval_div(const AlgInterval *x, const AlgInterval *y, AlgIntervalError *err) {
    if (!x || !y) {
        alg_set_error_interval(err, lv_alg_interval_ERR_NULL);
        AlgInterval iv;
        iv.lo = iv.hi = lv_alg_rational_zero();
        return iv;
    }

    /* 检查除数区间是否包含零 */
    AlgRational _zero = lv_alg_rational_zero();
    bool lo_nonpos = lv_alg_rational_cmp(&y->lo, &_zero) <= 0;
    bool hi_nonneg = lv_alg_rational_cmp(&y->hi, &_zero) >= 0;
    if (lo_nonpos && hi_nonneg) {
        alg_set_error_interval(err, lv_alg_interval_ERR_DIV_BY_ZERO);
        AlgInterval iv;
        iv.lo = iv.hi = lv_alg_rational_zero();
        return iv;
    }

    /* x / y = x * (1/y) */
    AlgRational inv_lo, inv_hi;
    AlgRationalError r_err;
    inv_lo = lv_alg_rational_inv(&y->lo, &r_err);
    inv_hi = lv_alg_rational_inv(&y->hi, &r_err);

    AlgInterval inv_y;
    if (lv_alg_rational_cmp(&inv_lo, &inv_hi) <= 0) {
        inv_y.lo = inv_lo;
        inv_y.hi = inv_hi;
    } else {
        inv_y.lo = inv_hi;
        inv_y.hi = inv_lo;
    }

    return lv_alg_interval_mul(x, &inv_y, err);
}

AlgInterval lv_alg_interval_neg(const AlgInterval *x) {
    AlgInterval result;
    result.lo = lv_alg_rational_neg(&x->hi);
    result.hi = lv_alg_rational_neg(&x->lo);
    return result;
}

AlgInterval lv_alg_interval_intersect(const AlgInterval *x, const AlgInterval *y, AlgIntervalError *err) {
    if (!x || !y) {
        alg_set_error_interval(err, lv_alg_interval_ERR_NULL);
        AlgInterval iv;
        iv.lo = iv.hi = lv_alg_rational_zero();
        return iv;
    }

    AlgInterval result;
    if (lv_alg_rational_cmp(&x->lo, &y->lo) > 0) {
        result.lo = x->lo;
    } else {
        result.lo = y->lo;
    }
    if (lv_alg_rational_cmp(&x->hi, &y->hi) < 0) {
        result.hi = x->hi;
    } else {
        result.hi = y->hi;
    }

    if (lv_alg_rational_cmp(&result.lo, &result.hi) > 0) {
        alg_set_error_interval(err, lv_alg_interval_ERR_EMPTY);
    } else {
        alg_set_error_interval(err, lv_alg_interval_OK);
    }
    return result;
}

AlgInterval lv_alg_interval_hull(const AlgInterval *x, const AlgInterval *y, AlgIntervalError *err) {
    if (!x || !y) {
        alg_set_error_interval(err, lv_alg_interval_ERR_NULL);
        AlgInterval iv;
        iv.lo = iv.hi = lv_alg_rational_zero();
        return iv;
    }

    AlgInterval result;
    if (lv_alg_rational_cmp(&x->lo, &y->lo) < 0) {
        result.lo = x->lo;
    } else {
        result.lo = y->lo;
    }
    if (lv_alg_rational_cmp(&x->hi, &y->hi) > 0) {
        result.hi = x->hi;
    } else {
        result.hi = y->hi;
    }

    alg_set_error_interval(err, lv_alg_interval_OK);
    return result;
}

bool lv_alg_interval_contains(const AlgInterval *x, const AlgInterval *y) {
    return lv_alg_rational_cmp(&x->lo, &y->lo) <= 0 && lv_alg_rational_cmp(&x->hi, &y->hi) >= 0;
}

bool lv_alg_interval_contains_rational(const AlgInterval *x, const AlgRational *r) {
    return lv_alg_rational_cmp(&x->lo, r) <= 0 && lv_alg_rational_cmp(&x->hi, r) >= 0;
}

bool lv_alg_interval_is_empty(const AlgInterval *x) {
    return lv_alg_rational_cmp(&x->lo, &x->hi) > 0;
}

bool lv_alg_interval_is_point(const AlgInterval *x) {
    return lv_alg_rational_eq(&x->lo, &x->hi);
}

AlgRational lv_alg_interval_width(const AlgInterval *x, AlgIntervalError *err) {
    if (!x) {
        alg_set_error_interval(err, lv_alg_interval_ERR_NULL);
        return lv_alg_rational_zero();
    }
    alg_set_error_interval(err, lv_alg_interval_OK);
    return lv_alg_rational_sub(&x->hi, &x->lo, NULL);
}

AlgRational lv_alg_interval_midpoint(const AlgInterval *x, AlgIntervalError *err) {
    if (!x) {
        alg_set_error_interval(err, lv_alg_interval_ERR_NULL);
        return lv_alg_rational_zero();
    }
    AlgRational sum = lv_alg_rational_add(&x->lo, &x->hi, NULL);
    AlgRational two = lv_alg_rational_from_int(2);
    alg_set_error_interval(err, lv_alg_interval_OK);
    return lv_alg_rational_div(&sum, &two, NULL);
}

void lv_alg_interval_bisect(const AlgInterval *x, AlgInterval *lower, AlgInterval *upper,
                                       AlgIntervalError *err) {
    if (!x) {
        alg_set_error_interval(err, lv_alg_interval_ERR_NULL);
        return;
    }

    AlgIntervalError i_err;
    AlgRational mid = lv_alg_interval_midpoint(x, &i_err);

    if (lower) {
        lower->lo = x->lo;
        lower->hi = mid;
    }
    if (upper) {
        upper->lo = mid;
        upper->hi = x->hi;
    }

    alg_set_error_interval(err, lv_alg_interval_OK);
}

int lv_alg_interval_to_string(const AlgInterval *x, char *buf, size_t size) {
    lvStrBuf sb = {0};

    lv_strbuf_printf(&sb, "[");

    lv_strbuf_printf(&sb, "%lld", (long long) x->lo.num);
    if (x->lo.den != 1) {
        lv_strbuf_printf(&sb, "/%lld", (long long) x->lo.den);
    }

    lv_strbuf_printf(&sb, ", ");

    lv_strbuf_printf(&sb, "%lld", (long long) x->hi.num);
    if (x->hi.den != 1) {
        lv_strbuf_printf(&sb, "/%lld", (long long) x->hi.den);
    }

    lv_strbuf_printf(&sb, "]");

    int len = (int) sb.len;
    if (buf && size > 0) {
        lv_strlcpy(buf, lv_strbuf_cstr(&sb), size);
    }
    lv_strbuf_destroy(&sb);
    return len;
}

/** @brief lv_alg_interval_error_string 名称表（按枚举值升序） */
static const lvStrToEnumEntry s_alg_interval_error_string_entries[] = {
    {"成功", lv_alg_interval_OK},
    {"空区间", lv_alg_interval_ERR_EMPTY},
    {"整数溢出", lv_alg_interval_ERR_OVERFLOW},
    {"空指针", lv_alg_interval_ERR_NULL},
    {"无效参数（lo > hi）", lv_alg_interval_ERR_INVALID},
    {"除以包含零的区间", lv_alg_interval_ERR_DIV_BY_ZERO},
};

const char *lv_alg_interval_error_string(AlgIntervalError err) {
    return lv_enum_to_str(s_alg_interval_error_string_entries, lv_ARRAY_SIZE(s_alg_interval_error_string_entries), (int) err, "未知错误");
}

