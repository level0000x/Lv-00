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
#include <stdio.h>
#include <string.h>

#include "lv/error_codes.h"
#include "lv/lv_str_utils.h"
#include "lv/lv_utils.h"

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

/* ========================================================================
 * 【废弃兼容层】interval_* API（IEEE 1788 空区间语义）
 *
 * 收敛自 interval_arithmetic.c（该文件已删除，见 CMakeLists.txt）：区间算术
 * 双实现已收口为 interval_arith.c 一份实现文件。geo_predicate.c 已迁移到
 * lv_interval_*；本兼容层保留 interval_* 符号（含 interval_from_symbolic /
 * interval_verify_solution / interval_verify_adaptive / intersect / union 等
 * lv_interval_* 没有的独有能力），供 test_interval_arithmetic 与既有调用点
 * 继续使用，语义与原先逐位一致：
 *   - 定义域外返回 interval_empty()（lo=1, hi=-1）而非 [-HUGE_VAL, HUGE_VAL]
 *   - round_down(0.0) = -DBL_TRUE_MIN（nextafter 严格方向），与 lv_interval_*
 *     （round_down(0.0) = -0.0）不同
 * 新代码请使用 lv_interval_*（interval_arith.h）。
 * ======================================================================== */
/* ========================================================================
 * Internal helpers
 * ======================================================================== */

/**
 * @brief Round a double downward (toward -infinity) to ensure containment.
 *
 * For rigorous interval arithmetic, outward rounding is essential to
 * guarantee that the true mathematical result lies within the computed
 * interval. nextafter(x, -INFINITY) returns the next representable
 * double value less than x, providing a strict lower bound.
 *
 * For infinity/NaN inputs, returns the input unchanged.
 *
 * @param x  The value to round downward
 * @return   The next representable double less than or equal to x
 */
static double round_down(double x) {
    if (isfinite(x)) {
        return nextafter(x, -INFINITY);
    }
    return x;
}

/**
 * @brief Round a double upward (toward +infinity) to ensure containment.
 *
 * Symmetric to round_down. nextafter(x, INFINITY) returns the next
 * representable double value greater than x, providing a strict upper bound.
 *
 * For infinity/NaN inputs, returns the input unchanged.
 *
 * @param x  The value to round upward
 * @return   The next representable double greater than or equal to x
 */
static double round_up(double x) {
    if (isfinite(x)) {
        return nextafter(x, INFINITY);
    }
    return x;
}

/**
 * @brief Minimum of four doubles.
 */
static double min4(double a, double b, double c, double d) {
    double m = a;
    if (b < m)
        m = b;
    if (c < m)
        m = c;
    if (d < m)
        m = d;
    return m;
}

/**
 * @brief Maximum of four doubles.
 */
static double max4(double a, double b, double c, double d) {
    double m = a;
    if (b > m)
        m = b;
    if (c > m)
        m = c;
    if (d > m)
        m = d;
    return m;
}

/* ========================================================================
 * Factory functions
 * ======================================================================== */

lvInterval interval_create(double lo, double hi, int is_exact) {
    lvInterval iv;
    iv.lo = lo;
    iv.hi = hi;
    iv.is_exact = is_exact;
    return iv;
}

lvInterval interval_point(double val) {
    lvInterval iv;
    iv.lo = val;
    iv.hi = val;
    iv.is_exact = 1;
    return iv;
}

lvInterval interval_empty(void) {
    lvInterval iv;
    iv.lo = 1.0;
    iv.hi = -1.0;
    iv.is_exact = 0;
    return iv;
}

lvInterval interval_entire(void) {
    lvInterval iv;
    iv.lo = -INFINITY;
    iv.hi = INFINITY;
    iv.is_exact = 0;
    return iv;
}

lvIntervalConfig interval_config_default(void) {
    lvIntervalConfig cfg;
    cfg.precision = 53;     /* double precision */
    cfg.rounding_eps = 0.0; /* no extra rounding in double mode */
    return cfg;
}

/* ========================================================================
 * Arithmetic operations
 * ======================================================================== */

lvInterval interval_add(lvInterval a, lvInterval b) {
    if (interval_is_empty(a) || interval_is_empty(b)) {
        return interval_empty();
    }
    lvInterval r;
    r.lo = round_down(a.lo + b.lo);
    r.hi = round_up(a.hi + b.hi);
    r.is_exact = a.is_exact && b.is_exact;
    return r;
}

lvInterval interval_sub(lvInterval a, lvInterval b) {
    if (interval_is_empty(a) || interval_is_empty(b)) {
        return interval_empty();
    }
    lvInterval r;
    r.lo = round_down(a.lo - b.hi);
    r.hi = round_up(a.hi - b.lo);
    r.is_exact = a.is_exact && b.is_exact;
    return r;
}

lvInterval interval_mul(lvInterval a, lvInterval b) {
    if (interval_is_empty(a) || interval_is_empty(b)) {
        return interval_empty();
    }
    lvInterval r;
    double p1 = a.lo * b.lo;
    double p2 = a.lo * b.hi;
    double p3 = a.hi * b.lo;
    double p4 = a.hi * b.hi;
    r.lo = round_down(min4(p1, p2, p3, p4));
    r.hi = round_up(max4(p1, p2, p3, p4));
    r.is_exact = a.is_exact && b.is_exact;
    return r;
}

lvInterval interval_div(lvInterval a, lvInterval b) {
    if (interval_is_empty(a) || interval_is_empty(b)) {
        return interval_empty();
    }
    /* Check if divisor contains zero */
    if (b.lo <= 0.0 && b.hi >= 0.0) {
        return interval_empty();
    }
    lvInterval r;
    /* Reciprocal of b: [1/b.hi, 1/b.lo] (signs handled by min/max) */
    double inv_lo = 1.0 / b.hi;
    double inv_hi = 1.0 / b.lo;
    lvInterval inv_b;
    inv_b.lo = round_down((inv_lo < inv_hi) ? inv_lo : inv_hi);
    inv_b.hi = round_up((inv_lo < inv_hi) ? inv_hi : inv_lo);
    inv_b.is_exact = 0;
    /* a / b = a * (1/b) */
    r = interval_mul(a, inv_b);
    r.is_exact = 0; /* Division always introduces potential rounding */
    return r;
}

lvInterval interval_sqrt(lvInterval a) {
    if (interval_is_empty(a)) {
        return interval_empty();
    }
    if (a.lo < 0.0) {
        return interval_empty();
    }
    lvInterval r;
    r.lo = round_down(sqrt(a.lo));
    r.hi = round_up(sqrt(a.hi));
    r.is_exact = a.is_exact;
    return r;
}

lvInterval interval_sin(lvInterval a) {
    if (interval_is_empty(a)) {
        return interval_empty();
    }
    lvInterval r;

    /* Normalize the interval to [0, 2*pi) range */
    double lo = a.lo;
    double hi = a.hi;
    double two_pi = 2.0 * M_PI;

    /* If the interval spans more than 2*pi, the result is [-1, 1] */
    if (hi - lo >= two_pi) {
        r.lo = -1.0;
        r.hi = 1.0;
        r.is_exact = 0;
        return r;
    }

    /* Compute sin at endpoints */
    double s_lo = sin(lo);
    double s_hi = sin(hi);

    /* Check if the interval contains any critical points (pi/2 + k*pi) */
    /* where sin reaches its maximum of 1 or minimum of -1 */
    r.lo = round_down((s_lo < s_hi) ? s_lo : s_hi);
    r.hi = round_up((s_lo < s_hi) ? s_hi : s_lo);

    /* 计算极值点覆盖范围：使用区间端点计算所需的 k 范围而非固定 [-100,100] */
    /* 最大值在 pi/2 + k*pi 处，最小值在 -pi/2 + k*pi 处 */
    double k_min_lo = floor((lo - M_PI / 2.0) / M_PI);
    double k_max_hi = ceil((hi - M_PI / 2.0) / M_PI);
    /* 限制搜索范围避免过大循环（区间跨度已在上方保证 < 2*pi，因此最多覆盖 3 个极值点） */
    if (k_min_lo < -2.0)
        k_min_lo = -2.0;
    if (k_max_hi > 2.0)
        k_max_hi = 2.0;
    for (double k = k_min_lo; k <= k_max_hi; k += 1.0) {
        double crit = M_PI / 2.0 + k * M_PI;
        if (crit >= lo && crit <= hi) {
            double sc = sin(crit);
            if (sc > r.hi)
                r.hi = round_up(sc);
            if (sc < r.lo)
                r.lo = round_down(sc);
        }
    }

    for (double k = k_min_lo; k <= k_max_hi; k += 1.0) {
        double crit = -M_PI / 2.0 + k * M_PI;
        if (crit >= lo && crit <= hi) {
            double sc = sin(crit);
            if (sc < r.lo)
                r.lo = round_down(sc);
            if (sc > r.hi)
                r.hi = round_up(sc);
        }
    }

    r.is_exact = 0;
    return r;
}

lvInterval interval_cos(lvInterval a) {
    if (interval_is_empty(a)) {
        return interval_empty();
    }
    lvInterval r;

    double lo = a.lo;
    double hi = a.hi;
    double two_pi = 2.0 * M_PI;

    /* If the interval spans more than 2*pi, the result is [-1, 1] */
    if (hi - lo >= two_pi) {
        r.lo = -1.0;
        r.hi = 1.0;
        r.is_exact = 0;
        return r;
    }

    /* Compute cos at endpoints */
    double c_lo = cos(lo);
    double c_hi = cos(hi);

    r.lo = round_down((c_lo < c_hi) ? c_lo : c_hi);
    r.hi = round_up((c_lo < c_hi) ? c_hi : c_lo);

    /* 使用区间端点动态计算极值点范围 */
    double k_min = floor(lo / M_PI);
    double k_max = ceil(hi / M_PI);
    if (k_min < -2.0)
        k_min = -2.0;
    if (k_max > 2.0)
        k_max = 2.0;
    for (double k = k_min; k <= k_max; k += 1.0) {
        double crit = k * M_PI;
        if (crit >= lo && crit <= hi) {
            double cc = cos(crit);
            if (cc > r.hi)
                r.hi = round_up(cc);
            if (cc < r.lo)
                r.lo = round_down(cc);
        }
    }

    r.is_exact = 0;
    return r;
}

lvInterval interval_exp(lvInterval a) {
    if (interval_is_empty(a)) {
        return interval_empty();
    }
    lvInterval r;
    r.lo = round_down(exp(a.lo));
    r.hi = round_up(exp(a.hi));
    r.is_exact = 0;
    return r;
}

lvInterval interval_log(lvInterval a) {
    if (interval_is_empty(a)) {
        return interval_empty();
    }
    if (a.lo <= 0.0) {
        return interval_empty();
    }
    lvInterval r;
    r.lo = round_down(log(a.lo));
    r.hi = round_up(log(a.hi));
    r.is_exact = 0;
    return r;
}

lvInterval interval_abs(lvInterval a) {
    if (interval_is_empty(a)) {
        return interval_empty();
    }
    lvInterval r;
    if (a.lo >= 0.0) {
        /* Entirely non-negative */
        r.lo = a.lo;
        r.hi = a.hi;
    } else if (a.hi <= 0.0) {
        /* Entirely non-positive */
        r.lo = -a.hi;
        r.hi = -a.lo;
    } else {
        /* Spans zero */
        r.lo = 0.0;
        r.hi = (a.hi > -a.lo) ? a.hi : -a.lo;
    }
    r.is_exact = a.is_exact;
    return r;
}

lvInterval interval_neg(lvInterval a) {
    if (interval_is_empty(a)) {
        return interval_empty();
    }
    lvInterval r;
    r.lo = -a.hi;
    r.hi = -a.lo;
    r.is_exact = a.is_exact;
    return r;
}

/* ========================================================================
 * Properties
 * ======================================================================== */

double interval_diam(lvInterval a) {
    if (interval_is_empty(a)) {
        return 0.0;
    }
    return a.hi - a.lo;
}

double interval_mid(lvInterval a) {
    if (interval_is_empty(a)) {
        return NAN;
    }
    return (a.lo + a.hi) / 2.0;
}

int interval_is_empty(lvInterval a) {
    return a.lo > a.hi;
}

int interval_contains(lvInterval a, double val) {
    if (interval_is_empty(a)) {
        return 0;
    }
    return val >= a.lo && val <= a.hi;
}

int interval_is_subset(lvInterval a, lvInterval b) {
    if (interval_is_empty(a)) {
        return 1; /* Empty set is a subset of any set */
    }
    if (interval_is_empty(b)) {
        return 0;
    }
    return a.lo >= b.lo && a.hi <= b.hi;
}

int interval_equals(lvInterval a, lvInterval b) {
    /* Two empty intervals are equal */
    if (interval_is_empty(a) && interval_is_empty(b)) {
        return 1;
    }
    if (interval_is_empty(a) || interval_is_empty(b)) {
        return 0;
    }
    return a.lo == b.lo && a.hi == b.hi;
}

/* ========================================================================
 * Set operations
 * ======================================================================== */

lvInterval interval_intersect(lvInterval a, lvInterval b) {
    if (interval_is_empty(a) || interval_is_empty(b)) {
        return interval_empty();
    }
    lvInterval r;
    r.lo = (a.lo > b.lo) ? a.lo : b.lo;
    r.hi = (a.hi < b.hi) ? a.hi : b.hi;
    if (r.lo > r.hi) {
        return interval_empty();
    }
    r.is_exact = a.is_exact && b.is_exact && a.lo == b.lo && a.hi == b.hi;
    return r;
}

lvInterval interval_union(lvInterval a, lvInterval b) {
    if (interval_is_empty(a))
        return b;
    if (interval_is_empty(b))
        return a;
    lvInterval r;
    r.lo = (a.lo < b.lo) ? a.lo : b.lo;
    r.hi = (a.hi > b.hi) ? a.hi : b.hi;
    r.is_exact = a.is_exact && b.is_exact && r.lo == r.hi;
    return r;
}

/* ========================================================================
 * Symbolic coordinate integration
 * ======================================================================== */

/**
 * @brief Simple expression evaluator for interval_from_symbolic.
 *
 * Supports: numeric literals, variable references, +, -, *, /, sqrt, sin, cos.
 * This is a basic recursive descent parser.
 */

typedef struct {
    const char *pos;                 /**< Current position in the expression string */
    const char **var_names;          /**< Variable names */
    const lvInterval *var_intervals; /**< Variable intervals */
    int var_count;                   /**< Number of variables */
    int error;                       /**< Nonzero if parsing error occurred */
} ExprParser;

static void skip_whitespace(ExprParser *p) {
    while (*p->pos == ' ' || *p->pos == '\t' || *p->pos == '\n' || *p->pos == '\r') {
        p->pos++;
    }
}

static lvInterval parse_expr(ExprParser *p);

/**
 * @brief 一元数学函数查找表
 *
 * 函数名 → 区间运算函数映射，线性扫描匹配，替代手写 strcmp 分支链。
 */
static const struct {
    const char *name;
    lvInterval (*fn)(lvInterval);
} kIntervalFuncs[] = {
    {"sqrt", interval_sqrt},
    {"sin", interval_sin},
    {"cos", interval_cos},
    {"exp", interval_exp},
    {"log", interval_log},
    {"abs", interval_abs},
};

static lvInterval parse_primary(ExprParser *p) {
    skip_whitespace(p);

    /* Number literal */
    if ((*p->pos >= '0' && *p->pos <= '9') || *p->pos == '.') {
        char *end;
        double val = strtod(p->pos, &end);
        p->pos = end;
        return interval_point(val);
    }

    /* Parenthesized expression */
    if (*p->pos == '(') {
        p->pos++;
        lvInterval r = parse_expr(p);
        skip_whitespace(p);
        if (*p->pos == ')') {
            p->pos++;
        } else {
            p->error = 1;
        }
        return r;
    }

    /* Unary minus */
    if (*p->pos == '-') {
        p->pos++;
        lvInterval r = parse_primary(p);
        return interval_neg(r);
    }

    /* Function call or variable */
    if ((*p->pos >= 'a' && *p->pos <= 'z') || (*p->pos >= 'A' && *p->pos <= 'Z') || *p->pos == '_') {
        char name[64];
        int len = 0;
        while (len < 63 && ((*p->pos >= 'a' && *p->pos <= 'z') || (*p->pos >= 'A' && *p->pos <= 'Z') ||
                            (*p->pos >= '0' && *p->pos <= '9') || *p->pos == '_')) {
            name[len++] = *p->pos;
            p->pos++;
        }
        name[len] = '\0';

        skip_whitespace(p);

        /* Check for function call */
        if (*p->pos == '(') {
            p->pos++;
            lvInterval arg = parse_expr(p);
            skip_whitespace(p);
            if (*p->pos == ')')
                p->pos++;
            else
                p->error = 1;

            /* 查表匹配一元数学函数 */
            for (size_t i = 0; i < lv_ARRAY_SIZE(kIntervalFuncs); i++) {
                if (lv_str_eq(name, kIntervalFuncs[i].name))
                    return kIntervalFuncs[i].fn(arg);
            }

            /* Unknown function */
            p->error = 1;
            return interval_empty();
        }

        /* Variable lookup */
        for (int i = 0; i < p->var_count; i++) {
            if (lv_str_eq(name, p->var_names[i])) {
                return p->var_intervals[i];
            }
        }

        /* Unknown variable */
        p->error = 1;
        return interval_empty();
    }

    p->error = 1;
    return interval_empty();
}

static lvInterval parse_mul_div(ExprParser *p) {
    lvInterval left = parse_primary(p);
    while (!p->error) {
        skip_whitespace(p);
        if (*p->pos == '*') {
            p->pos++;
            lvInterval right = parse_primary(p);
            left = interval_mul(left, right);
        } else if (*p->pos == '/') {
            p->pos++;
            lvInterval right = parse_primary(p);
            left = interval_div(left, right);
        } else {
            break;
        }
    }
    return left;
}

static lvInterval parse_expr(ExprParser *p) {
    lvInterval left = parse_mul_div(p);
    while (!p->error) {
        skip_whitespace(p);
        if (*p->pos == '+') {
            p->pos++;
            lvInterval right = parse_mul_div(p);
            left = interval_add(left, right);
        } else if (*p->pos == '-') {
            p->pos++;
            lvInterval right = parse_mul_div(p);
            left = interval_sub(left, right);
        } else {
            break;
        }
    }
    return left;
}

lvInterval interval_from_symbolic(const char *expr_str, const char **var_names, const lvInterval *var_intervals,
                                  int var_count) {
    if (!expr_str || !var_names || !var_intervals || var_count <= 0) {
        return interval_empty();
    }

    ExprParser p;
    p.pos = expr_str;
    p.var_names = var_names;
    p.var_intervals = var_intervals;
    p.var_count = var_count;
    p.error = 0;

    lvInterval result = parse_expr(&p);

    if (p.error) {
        return interval_empty();
    }
    return result;
}

int interval_to_symbolic(lvInterval a, char *buf, size_t buf_size) {
    if (!buf || buf_size == 0) {
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "interval_to_symbolic: buf is NULL or buf_size is 0");
    }
    if (interval_is_empty(a)) {
        return snprintf(buf, buf_size, "[empty]");
    }
    if (a.is_exact && a.lo == a.hi) {
        return snprintf(buf, buf_size, "%.17g", a.lo);
    }
    return snprintf(buf, buf_size, "[%.17g, %.17g]", a.lo, a.hi);
}

/* ========================================================================
 * Verification functions
 * ======================================================================== */

int interval_verify_solution(lvInterval f_interval, double tolerance) {
    if (interval_is_empty(f_interval)) {
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "interval_verify_solution: empty interval");
    }
    /* Check if 0 is contained in the interval, considering tolerance */
    if (f_interval.lo <= tolerance && f_interval.hi >= -tolerance) {
        return 1;
    }
    return 0;
}

int interval_verify_adaptive(const char *expr_str, const char **var_names, lvInterval *var_intervals, int var_count,
                             int max_depth, double tolerance) {
    if (!expr_str || !var_names || !var_intervals || var_count <= 0) {
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "interval_verify_adaptive: NULL parameter or invalid var_count");
    }

    /* Evaluate the expression on the current intervals */
    lvInterval result = interval_from_symbolic(expr_str, var_names, var_intervals, var_count);

    /* Check if 0 is in the result interval */
    if (interval_verify_solution(result, tolerance) == 1) {
        return 1;
    }

    /* If max depth reached, give up */
    if (max_depth <= 0) {
        return 0;
    }

    /* Find the widest interval to bisect */
    int widest = 0;
    double max_diam = 0.0;
    for (int i = 0; i < var_count; i++) {
        double d = interval_diam(var_intervals[i]);
        if (d > max_diam) {
            max_diam = d;
            widest = i;
        }
    }

    if (max_diam <= 0.0) {
        /* All intervals are points, cannot refine further */
        return 0;
    }

    /* Bisect the widest interval */
    double mid = interval_mid(var_intervals[widest]);
    lvInterval saved = var_intervals[widest];

    /* Try lower half */
    var_intervals[widest] = interval_create(saved.lo, mid, 0);
    int lower = interval_verify_adaptive(expr_str, var_names, var_intervals, var_count, max_depth - 1, tolerance);
    if (lower == 1) {
        var_intervals[widest] = saved;
        return 1;
    }

    /* Try upper half */
    var_intervals[widest] = interval_create(mid, saved.hi, 0);
    int upper = interval_verify_adaptive(expr_str, var_names, var_intervals, var_count, max_depth - 1, tolerance);
    if (upper == 1) {
        var_intervals[widest] = saved;
        return 1;
    }

    /* Restore original interval */
    var_intervals[widest] = saved;
    return 0;
}
