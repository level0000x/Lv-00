/**
 * @file interval_arith.c
 * @brief Lv-00 公共区间算术库实现 —— 数值验证策略的统一区间底座
 *
 * @details 基础运算（add/sub/mul/div/sqrt/sin/cos/exp/log/abs/neg）的语义
 *          以 float_error.c 的 float_interval_* 为准（侦察确认其最完整）：
 *          - 端点采用 nextafter 方向舍入（round_down/round_up）保证保守性，
 *            且 round_down(0.0) = -0.0、round_up(0.0) = +0.0，与 float_error
 *            原实现逐位一致（interval_arithmetic.c 的 round_down(0.0) 会返回
 *            -DBL_TRUE_MIN，故不复用）。
 *          - 定义域外（除数跨零、sqrt/log 负下界）返回保守的全实数区间
 *            [-HUGE_VAL, HUGE_VAL]，而非 IEEE 1788 空区间：空区间 (lo>hi)
 *            会让 FPTaylor 的 half_width 为负，进而把 TrustColor 误判为
 *            TRUST_GREEN，破坏 fptaylor_verify_safety 语义。
 *          - sin/cos 采用 float_error 的极值点检测算法（ceil/floor 循环 +
 *            fmod 判定），保证 float_error 侧输出不变。
 *
 *          扩展函数（tan/atan/pow/asin/acos/floor/ceil）采用保守端点法
 *          （naive endpoint method），奇点/定义域处理在各函数注释中说明。
 *
 * @version 1.0.0
 * @date 2026-08-06
 */

#include "lv/interval_arith.h"

#include "lv/lv_platform.h"

#include <float.h>
#include <math.h>

/* ========================================================================
 * 内部工具：方向舍入（与 float_error.c 原实现逐位一致）
 * ======================================================================== */

/**
 * @brief 向下舍入（向 -∞ 方向取下一个可表示值）
 *
 * 与 float_error.c 的 round_down 一致：0.0 特判为 -0.0（等价于 0），
 * NaN/Inf 原样返回。注意与 interval_arithmetic.c 的 round_down 不同
 * （后者对 0.0 返回 -DBL_TRUE_MIN），这里保持 float_error 原语义。
 */
static double ia_round_down(double x) {
    if (isnan(x) || isinf(x))
        return x;
    if (x == 0.0)
        return -0.0; /* 零方向的下一个可表示值（数值上等于 0） */
    return nextafter(x, -INFINITY);
}

/**
 * @brief 向上舍入（向 +∞ 方向取下一个可表示值）
 */
static double ia_round_up(double x) {
    if (isnan(x) || isinf(x))
        return x;
    if (x == 0.0)
        return +0.0;
    return nextafter(x, INFINITY);
}

/* ========================================================================
 * 构造
 * ======================================================================== */

lvInterval lv_interval_make(double lo, double hi, int is_exact) {
    lvInterval iv;
    iv.lo = lo;
    iv.hi = hi;
    iv.is_exact = is_exact;
    return iv;
}

/* ========================================================================
 * 基础运算（语义基准：float_error.c 的 float_interval_*）
 * ======================================================================== */

lvInterval lv_interval_add(lvInterval a, lvInterval b) {
    lvInterval result;
    result.lo = ia_round_down(a.lo + b.lo);
    result.hi = ia_round_up(a.hi + b.hi);
    result.is_exact = a.is_exact && b.is_exact;
    return result;
}

lvInterval lv_interval_sub(lvInterval a, lvInterval b) {
    lvInterval result;
    /* a - b: 下界 = a.lo - b.hi, 上界 = a.hi - b.lo */
    result.lo = ia_round_down(a.lo - b.hi);
    result.hi = ia_round_up(a.hi - b.lo);
    result.is_exact = a.is_exact && b.is_exact;
    return result;
}

lvInterval lv_interval_mul(lvInterval a, lvInterval b) {
    /* 四个角点最小/最大原理 */
    double p1 = a.lo * b.lo;
    double p2 = a.lo * b.hi;
    double p3 = a.hi * b.lo;
    double p4 = a.hi * b.hi;

    double min_val = fmin(fmin(p1, p2), fmin(p3, p4));
    double max_val = fmax(fmax(p1, p2), fmax(p3, p4));

    lvInterval result;
    result.lo = ia_round_down(min_val);
    result.hi = ia_round_up(max_val);
    result.is_exact = a.is_exact && b.is_exact;
    return result;
}

lvInterval lv_interval_div(lvInterval a, lvInterval b) {
    lvInterval result;

    /* 分母跨越零点：返回全实数区间（float_error 语义，保持 fptaylor 误差界为正） */
    if (b.lo <= 0.0 && b.hi >= 0.0) {
        result.lo = -HUGE_VAL;
        result.hi = HUGE_VAL;
        result.is_exact = 0;
        return result;
    }

    /* 通过倒数 + 乘法实现除法：分母全负或全正时倒数范围均为 [1/b.hi, 1/b.lo] */
    double inv_lo = 1.0 / b.hi;
    double inv_hi = 1.0 / b.lo;
    lvInterval inv_b = lv_interval_make(inv_lo, inv_hi, b.is_exact);
    result = lv_interval_mul(a, inv_b);
    return result;
}

lvInterval lv_interval_sqrt(lvInterval a) {
    lvInterval result;
    if (a.lo < 0.0) {
        /* 负数部分无实数定义，截断到 0（与 float_error 一致） */
        result.lo = 0.0;
    } else {
        result.lo = ia_round_down(sqrt(a.lo));
    }
    result.hi = ia_round_up(sqrt(a.hi));
    result.is_exact = a.is_exact && (a.lo == a.hi);
    return result;
}

lvInterval lv_interval_sin(lvInterval a) {
    /* sin 在 [-1, 1] 之间，需处理非单调区间与极值点（float_error 算法） */
    double sin_lo = sin(a.lo);
    double sin_hi = sin(a.hi);

    double width = a.hi - a.lo;
    double min_val = fmin(sin_lo, sin_hi);
    double max_val = fmax(sin_lo, sin_hi);

    if (width >= 2.0 * M_PI) {
        /* 区间超过一个完整周期 -> 覆盖全范围 */
        min_val = -1.0;
        max_val = 1.0;
    } else {
        /* 检查 pi/2 + 2k*pi（sin 峰值点）是否在区间内。
         * 与 float_error 原实现保持一致（含 fmod(k,2) 的保守分支），
         * 保证 float_error 侧输出不变。 */
        double pi_half = M_PI / 2.0;
        double k_start = ceil((a.lo - pi_half) / (2.0 * M_PI));
        double k_end = floor((a.hi - pi_half) / (2.0 * M_PI));
        for (double k = k_start; k <= k_end; k += 1.0) {
            double peak = pi_half + k * 2.0 * M_PI;
            if (peak >= a.lo && peak <= a.hi) {
                if (fmod(k, 2.0) == 0.0) {
                    max_val = 1.0; /* sin(pi/2 + 2k*pi) = 1 */
                } else {
                    min_val = -1.0; /* 保守分支：拉低下界 */
                }
            }
        }
    }

    lvInterval result;
    result.lo = ia_round_down(min_val);
    result.hi = ia_round_up(max_val);
    result.is_exact = a.is_exact && (a.lo == a.hi);
    return result;
}

lvInterval lv_interval_cos(lvInterval a) {
    /* cos 性质类似 sin，偏移 pi/2（float_error 算法） */
    double cos_lo = cos(a.lo);
    double cos_hi = cos(a.hi);
    double width = a.hi - a.lo;
    double min_val = fmin(cos_lo, cos_hi);
    double max_val = fmax(cos_lo, cos_hi);

    if (width >= 2.0 * M_PI) {
        min_val = -1.0;
        max_val = 1.0;
    } else {
        /* 检查 k*pi（cos 极值点）是否在区间内 */
        double k_start = ceil(a.lo / M_PI);
        double k_end = floor(a.hi / M_PI);
        for (double k = k_start; k <= k_end; k += 1.0) {
            double peak = k * M_PI;
            if (peak >= a.lo && peak <= a.hi) {
                /* cos(k*pi) = (-1)^k */
                if (fmod(k, 2.0) == 0.0) {
                    max_val = 1.0;
                } else {
                    min_val = -1.0;
                }
            }
        }
    }

    lvInterval result;
    result.lo = ia_round_down(min_val);
    result.hi = ia_round_up(max_val);
    result.is_exact = a.is_exact && (a.lo == a.hi);
    return result;
}

lvInterval lv_interval_exp(lvInterval a) {
    /* exp 单调递增 */
    lvInterval result;
    result.lo = ia_round_down(exp(a.lo));
    result.hi = ia_round_up(exp(a.hi));
    result.is_exact = a.is_exact && (a.lo == a.hi);
    return result;
}

lvInterval lv_interval_log(lvInterval a) {
    lvInterval result;
    if (a.lo <= 0.0) {
        /* log 在非正区间无定义：下界置 -HUGE_VAL（float_error 语义） */
        result.lo = -HUGE_VAL;
        result.hi = (a.hi > 0.0) ? ia_round_up(log(a.hi)) : -HUGE_VAL;
        result.is_exact = 0;
        return result;
    }
    result.lo = ia_round_down(log(a.lo));
    result.hi = ia_round_up(log(a.hi));
    result.is_exact = a.is_exact && (a.lo == a.hi);
    return result;
}

lvInterval lv_interval_abs(lvInterval a) {
    lvInterval r;
    if (a.lo >= 0.0) {
        /* 全非负 */
        r.lo = a.lo;
        r.hi = a.hi;
    } else if (a.hi <= 0.0) {
        /* 全非正 */
        r.lo = -a.hi;
        r.hi = -a.lo;
    } else {
        /* 跨越零 */
        r.lo = 0.0;
        r.hi = fmax(-a.lo, a.hi);
    }
    r.is_exact = a.is_exact;
    return r;
}

lvInterval lv_interval_neg(lvInterval a) {
    lvInterval r;
    r.lo = -a.hi;
    r.hi = -a.lo;
    r.is_exact = a.is_exact;
    return r;
}

/* ========================================================================
 * 扩展函数 —— 保守端点法（naive endpoint method）
 *
 * 说明：以下函数遵循现有区间求值风格（端点 min/max + 向外舍入），
 * 并在注释中说明各函数的奇点/定义域处理：
 *  - tan：π/2+kπ 处奇点，区间含奇点则返回全实数
 *  - atan：全实数域单调递增
 *  - pow：a>0 时 f(a,b)=a^b 在矩形上无内部极值，4 角点法保守；
 *         a 跨零时仅整数幂可定义（偶/奇次分别处理），否则全实数
 *  - asin/acos：定义域 [-1,1]，与定义域求交后求值；交空则全实数
 *  - floor/ceil：单调不减，结果本身精确整数，无需舍入扩展
 * ======================================================================== */

lvInterval lv_interval_tan(lvInterval a) {
    lvInterval r;
    /* tan 周期 π，奇点在 π/2 + kπ（k 为整数）。
     * 若区间包含任一奇点，tan 可趋向 ±∞，保守返回全实数。 */
    double k_lo = ceil((a.lo - M_PI / 2.0) / M_PI);
    double k_hi = floor((a.hi - M_PI / 2.0) / M_PI);
    if (k_lo <= k_hi) {
        r.lo = -HUGE_VAL;
        r.hi = HUGE_VAL;
        r.is_exact = 0;
        return r;
    }
    /* 不含奇点：tan 在相邻奇点之间单调，端点法保守 */
    double t_lo = tan(a.lo);
    double t_hi = tan(a.hi);
    r.lo = ia_round_down(fmin(t_lo, t_hi));
    r.hi = ia_round_up(fmax(t_lo, t_hi));
    r.is_exact = a.is_exact && (a.lo == a.hi);
    return r;
}

lvInterval lv_interval_atan(lvInterval a) {
    /* atan 全实数域单调递增 */
    lvInterval r;
    r.lo = ia_round_down(atan(a.lo));
    r.hi = ia_round_up(atan(a.hi));
    r.is_exact = a.is_exact && (a.lo == a.hi);
    return r;
}

lvInterval lv_interval_pow(lvInterval a, lvInterval b) {
    lvInterval r;
    if (a.lo >= 0.0) {
        /* 正底（含 0）：f(a,b)=a^b 对 a 单调（b>0 增 / b<0 减），
         * 对 b 的单调性由 ln(a) 决定（a=1 或 b=0 为退化线）。
         * f 在矩形 [a.lo,a.hi]x[b.lo,b.hi] 上无内部极值，4 角点法保守。
         * 但 b 区间跨 0 时 a^0=1 是可达值（不在角点上），需显式并入。 */
        double p1 = pow(a.lo, b.lo);
        double p2 = pow(a.lo, b.hi);
        double p3 = pow(a.hi, b.lo);
        double p4 = pow(a.hi, b.hi);
        double mn = fmin(fmin(p1, p2), fmin(p3, p4));
        double mx = fmax(fmax(p1, p2), fmax(p3, p4));
        if (b.lo <= 0.0 && b.hi >= 0.0) {
            mn = fmin(mn, 1.0);
            mx = fmax(mx, 1.0);
        }
        r.lo = ia_round_down(mn);
        r.hi = ia_round_up(mx);
        r.is_exact = a.is_exact && b.is_exact && (a.lo == a.hi) && (b.lo == b.hi);
        return r;
    }

    /* 底区间跨零：仅当指数为单个整数时实数幂可定义 */
    if (b.lo == b.hi && b.lo == floor(b.lo)) {
        double n = b.lo;
        if (fmod(n, 2.0) == 0.0) {
            /* 偶次幂：f(a)=a^n 为偶函数，跨零时最小值出现在 0 或端点 */
            double m1 = pow(a.lo, n);
            double m2 = pow(a.hi, n);
            double mn = fmin(m1, m2);
            double mx = fmax(m1, m2);
            if (mn > 0.0 && a.lo < 0.0 && a.hi > 0.0)
                mn = 0.0; /* 跨零偶幂包含 0 */
            r.lo = ia_round_down(mn);
            r.hi = ia_round_up(mx);
        } else {
            /* 奇次幂：单调递增 */
            r.lo = ia_round_down(pow(a.lo, n));
            r.hi = ia_round_up(pow(a.hi, n));
        }
        r.is_exact = a.is_exact && b.is_exact && (a.lo == a.hi) && (b.lo == b.hi);
        return r;
    }

    /* 负数底 + 非整数幂：无实数定义，保守返回全实数 */
    r.lo = -HUGE_VAL;
    r.hi = HUGE_VAL;
    r.is_exact = 0;
    return r;
}

lvInterval lv_interval_asin(lvInterval a) {
    lvInterval r;
    /* asin 定义域 [-1,1]：与定义域求交，交空则无实数定义 -> 全实数 */
    double lo = (a.lo < -1.0) ? -1.0 : a.lo;
    double hi = (a.hi > 1.0) ? 1.0 : a.hi;
    if (lo > hi) {
        r.lo = -HUGE_VAL;
        r.hi = HUGE_VAL;
        r.is_exact = 0;
        return r;
    }
    /* asin 单调递增 */
    r.lo = ia_round_down(asin(lo));
    r.hi = ia_round_up(asin(hi));
    r.is_exact = a.is_exact && (a.lo == a.hi) && (lo == a.lo) && (hi == a.hi);
    return r;
}

lvInterval lv_interval_acos(lvInterval a) {
    lvInterval r;
    /* acos 定义域 [-1,1]：与定义域求交，交空则无实数定义 -> 全实数 */
    double lo = (a.lo < -1.0) ? -1.0 : a.lo;
    double hi = (a.hi > 1.0) ? 1.0 : a.hi;
    if (lo > hi) {
        r.lo = -HUGE_VAL;
        r.hi = HUGE_VAL;
        r.is_exact = 0;
        return r;
    }
    /* acos 单调递减：下界取 hi 端、上界取 lo 端 */
    r.lo = ia_round_down(acos(hi));
    r.hi = ia_round_up(acos(lo));
    r.is_exact = a.is_exact && (a.lo == a.hi) && (lo == a.lo) && (hi == a.hi);
    return r;
}

lvInterval lv_interval_floor(lvInterval a) {
    /* floor 单调不减；结果本身为精确整数，无需向外舍入 */
    lvInterval r;
    r.lo = floor(a.lo);
    r.hi = floor(a.hi);
    r.is_exact = a.is_exact && (a.lo == a.hi);
    return r;
}

lvInterval lv_interval_ceil(lvInterval a) {
    /* ceil 单调不减；结果本身为精确整数，无需向外舍入 */
    lvInterval r;
    r.lo = ceil(a.lo);
    r.hi = ceil(a.hi);
    r.is_exact = a.is_exact && (a.lo == a.hi);
    return r;
}
