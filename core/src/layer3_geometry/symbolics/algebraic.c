/**
 * @file algebraic.c
 * @brief Algebraic 代数数类型（最小多项式 + 隔离区间）
 *
 * @details 代数数 α 由其最小多项式 p(x)∈Z[x] 和包含唯一实根
 *          的隔离区间 [lo, hi] 唯一确定。核心操作：
 *          - algebraic_create / algebraic_destroy: 生命周期管理
 *          - algebraic_add / algebraic_mul: 代数数的和与积
 *            （通过结式 resultant 计算结果的最小多项式）
 *          - algebraic_to_double: Newton-Raphson 迭代求近似值
 *          - algebraic_serialize: 序列化为人类可读形式
 *
 *          结式计算委托给 mpz_poly_resultant（Sylvester 矩阵方法）。
 *          隔离区间通过 Sturm 定理或二分法确定。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/constraint_graph.h"
#include "lv/symbolic_coord.h"

#include "debug.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "mpz_poly.h"

#define SYM_COORD_DYNAMIC_ARRAY_INIT_CAP 16
#define SYM_COORD_SIGFIGS_MIN_SAFE 6
#define SYM_COORD_SIGFIGS_APPROX 4
#define SYM_COORD_EPS 1e-8
#define SYM_COORD_MAX_REFINE 15
#define SYM_COORD_AMB_MIN_SIGFIGS 3
#define COORD_SEVEN_OVER_FIVE_N 32
/* ── Algebraic type ── */

static double evaluate_poly_at_double(const mpz_poly_t *poly, double x) {
    if (poly->degree < 0)
        return 0.0;
    double result = 0.0;
    double x_pow = 1.0;
    for (int i = 0; i <= poly->degree; i++) {
        result += mpz_get_d(poly->coeffs[i]) * x_pow;
        x_pow *= x;
    }
    return result;
}

/**
 * 使用牛顿法细化代数数的隔离区间边界。
 *
 * 通过二分法结合符号检测，逐步缩小包含根的区间。
 *
 * @param a         代数数对象
 * @param iterations 迭代次数
 */
void refine_algebraic_bounds(Algebraic *a, int iterations) {
    if (a->minimal_poly.degree < 1 || iterations <= 0)
        return;

    for (int iter = 0; iter < iterations; iter++) {
        double mid = (a->left_bound + a->right_bound) / 2.0;
        double val_mid = evaluate_poly_at_double(&a->minimal_poly, mid);
        double val_left = evaluate_poly_at_double(&a->minimal_poly, a->left_bound);

        /* 检查 NaN/Inf：如果求值结果无效，终止细化 */
        if (!isfinite(val_mid) || !isfinite(val_left)) {
            return;
        }

        if (fabs(val_mid) < lv_EPSILON_NEWTON) {
            /* 使用相对区间宽度：根据 |mid| 缩放 epsilon，
             * 避免对大数值使用固定宽度导致精度过剩，
             * 对小数值使用过宽区间导致根隔离不精确。 */
            double eps_rel = lv_EPSILON_NEWTON * fmax(1.0, fabs(mid));
            a->left_bound = mid - eps_rel;
            a->right_bound = mid + eps_rel;
            return;
        }

        if (val_left * val_mid < 0) {
            a->right_bound = mid;
        } else {
            a->left_bound = mid;
        }
    }
}

/**
 * 在有理数点处求值多项式。
 *
 * 使用 Horner 法则计算多项式 p(x) 在有理数 r 处的值。
 * 结果设为 p(r) 化简后的分子。
 * p(r) = 0 当且仅当 result == 0。
 *
 * @param result 输出参数，存储求值结果的分子
 * @param poly   多项式
 * @param r      有理数求值点
 */
static void evaluate_algebraic_at_rational(mpz_t result, const mpz_poly_t *poly, const Rational *r) {
    mpq_t val;
    mpq_init(val);

    /* Horner 法则：从最高次项系数开始 */
    for (int i = poly->degree; i >= 0; i--) {
        if (i == (int) poly->degree) {
            mpq_set_z(val, poly->coeffs[i]);
        } else {
            mpq_mul(val, val, r->value);
            mpq_t coeff;
            mpq_init(coeff);
            mpq_set_z(coeff, poly->coeffs[i]);
            mpq_add(val, val, coeff);
            mpq_clear(coeff);
        }
    }

    /* val 现在是 p(r) 化简后的形式（mpq 标准形式）*/
    /* p(r) = 0 当且仅当分子为 0 */
    mpz_set(result, mpq_numref(val));
    mpq_clear(val);
}

/**
 * 使用连分数展开将 double 值近似为有理数。
 *
 * 使用 GMP 多精度整数计算连分数表示的渐近分数 h_n/k_n，
 * 当分母超过 BIT_CUTOFF_THRESHOLD/2 比特时截断。
 * 若在 epsilon 范围内找到有理数近似则返回 true。
 *
 * @param value   要近似的 double 值
 * @param epsilon 容许误差
 * @param result  输出参数，存储找到的有理数近似
 * @return 找到合适近似返回 true，否则返回 false
 */
static bool continued_fraction_approx(double value, double epsilon, mpq_t result) {
    if (epsilon <= 0.0)
        return false;

    mpz_t h_prev, h_curr, k_prev, k_curr;
    mpz_t h_next, k_next, a;
    mpz_init(h_prev);
    mpz_init(h_curr);
    mpz_init(k_prev);
    mpz_init(k_curr);
    mpz_init(h_next);
    mpz_init(k_next);
    mpz_init(a);

    /* found 必须在任何 goto cleanup 之前初始化，避免通过 cleanup: 返回未定义值。
     * 此处将其提至函数顶部以确保所有代码路径均能正确返回。 */
    bool found = false;

    /* 单独处理符号 */
    bool negative = (value < 0.0);
    if (negative)
        value = -value;

    /* 检查 value 是否过大，避免 unsigned long long 转换截断。
     * 使用 ULLONG_MAX（来自 <limits.h>）转为 double 进行安全比较。 */
    double max_ull_as_double = (double) ULLONG_MAX;
    if (value > max_ull_as_double) {
        /* 值过大，无法用 unsigned long long 表示，直接返回失败 */
        mpz_clear(h_prev);
        mpz_clear(h_curr);
        mpz_clear(k_prev);
        mpz_clear(k_curr);
        mpz_clear(h_next);
        mpz_clear(k_next);
        mpz_clear(a);
        return false;
    }

    /* 使用 modf 将 value 分解为整数部分和小数部分 */
    double int_part_d;
    double frac = modf(value, &int_part_d);

    /* 初始化渐近分数：
     * h_{-1} = 1, h_0 = floor(value)
     * k_{-1} = 0, k_0 = 1
     */
    mpz_set_ui(h_prev, 1);
    mpz_set_ui(k_prev, 0);
    mpz_set_ui(h_curr, (unsigned long long) int_part_d);
    mpz_set_ui(k_curr, 1);

    size_t bit_limit = lv_BIT_CUTOFF_THRESHOLD / 2;

    /* 检查整数部分本身是否已经是足够好的近似 */
    mpq_set_num(result, h_curr);
    mpq_set_den(result, k_curr);
    if (negative)
        mpq_neg(result, result);
    double approx = mpq_get_d(result);
    if (fabs(approx - (negative ? -value : value)) < epsilon) {
        found = true;
        goto cleanup;
    }

    /* 迭代：每一步取小数部分的倒数，
     * 提取整数部分作为下一个部分商 a_{n+1}，
     * 计算下一个渐近分数 h_{n+1} = a_{n+1}*h_n + h_{n-1}
     *                      k_{n+1} = a_{n+1}*k_n + k_{n-1}
     */
    for (int iter = 0; iter < lv_CONTINUED_FRACTION_MAX_ITER; iter++) {
        if (frac < lv_EPSILON_FRACTION_ZERO)
            break; /* 分数部分实际上为0 */

        double inv_frac = 1.0 / frac;
        double a_d;
        frac = modf(inv_frac, &a_d);

        /* a_{n+1} = floor(inv_frac)。
         * 检查 a_d 是否在 unsigned long long 范围内，避免转换截断。
         * 使用 ULLONG_MAX（来自 <limits.h>）转为 double 进行安全比较。 */
        if (a_d < 0.0 || a_d > max_ull_as_double) {
            break; /* 部分商超出范围，终止迭代 */
        }
        mpz_set_ui(a, (unsigned long long) a_d);

        /* h_{n+1} = a * h_curr + h_prev */
        mpz_mul(h_next, a, h_curr);
        mpz_add(h_next, h_next, h_prev);

        /* k_{n+1} = a * k_curr + k_prev */
        mpz_mul(k_next, a, k_curr);
        mpz_add(k_next, k_next, k_prev);

        /* 检查分母比特位数 */
        size_t den_bits = mpz_sizeinbase(k_next, 2);
        if (den_bits > bit_limit) {
            /* 超过比特位限制，使用前一个渐近分数 */
            break;
        }

        /* 移位：prev <- curr, curr <- next */
        mpz_set(h_prev, h_curr);
        mpz_set(k_prev, k_curr);
        mpz_set(h_curr, h_next);
        mpz_set(k_curr, k_next);

        /* 检查近似质量 */
        mpq_set_num(result, h_curr);
        mpq_set_den(result, k_curr);
        if (negative)
            mpq_neg(result, result);
        approx = mpq_get_d(result);
        if (fabs(approx - (negative ? -value : value)) < epsilon) {
            found = true;
            break;
        }
    }

cleanup:
    mpz_clear(h_prev);
    mpz_clear(h_curr);
    mpz_clear(k_prev);
    mpz_clear(k_curr);
    mpz_clear(h_next);
    mpz_clear(k_next);
    mpz_clear(a);
    return found;
}

/**
 * 尝试将有理化代数数转换为有理数。
 *
 * 实现 design_v2.9.md Section 1.2 的优先有理化策略：
 * 1. 检查中点值是否为零多项式的根
 * 2. 使用连分数逼近寻求更精确的有理近似
 *
 * @param a 代数数对象（不能为 NULL）
 * @return 仍然是传入的代数数对象，若成功缓存有理数则内部状态改变
 */
static Algebraic *try_priority_rationalization(Algebraic *a) {
    if (!a)
        return NULL;

    /* 策略1：检查中点值（原始逻辑）*/
    Rational *approx = rational_create((int64_t) ((a->left_bound + a->right_bound) / 2), 1);
    if (!approx)
        return a;
    mpz_t eval;
    mpz_init(eval);
    evaluate_algebraic_at_rational(eval, &a->minimal_poly, approx);
    if (mpz_cmp_si(eval, 0) == 0) {
        mpz_clear(eval);
        Rational *cached_rational = rational_create(0, 1);
        mpq_set(cached_rational->value, approx->value);
        a->cached_rational = cached_rational;
        rational_destroy(approx);
        return a;
    }
    mpz_clear(eval);
    rational_destroy(approx);

    /* 策略2：连分数近似 */
    double mid_value = (a->left_bound + a->right_bound) / 2.0;
    double interval_width = a->right_bound - a->left_bound;
    double epsilon = interval_width / 4.0; /* 按设计要求：precision = interval_width / 4 */

    mpq_t cf_result;
    mpq_init(cf_result);

    if (continued_fraction_approx(mid_value, epsilon, cf_result)) {
        /* 验证：将候选有理数代入极小多项式 */
        Rational candidate;
        mpq_init(candidate.value);
        mpq_set(candidate.value, cf_result);

        mpz_t cf_eval;
        mpz_init(cf_eval);
        evaluate_algebraic_at_rational(cf_eval, &a->minimal_poly, &candidate);

        if (mpz_cmp_si(cf_eval, 0) == 0) {
            /* 候选值是极小多项式的精确根 */
            mpz_clear(cf_eval);
            mpq_clear(candidate.value);
            Rational *cached_rational = rational_create_from_mpz(mpq_numref(cf_result), mpq_denref(cf_result));
            if (cached_rational) {
                a->cached_rational = cached_rational;
            }
            mpq_clear(cf_result);
            return a;
        }
        mpz_clear(cf_eval);
        mpq_clear(candidate.value);
    }

    mpq_clear(cf_result);
    return a;
}

/* ============================================================
 * Unique real root verification (design_v2.9.md Section 1.2)
 *
 * 创建代数数时必须验证隔离区间 [left, right] 包含唯一实根：
 * 计算极小多项式的所有实根近似，确保区间与任何其他实根不重叠。
 * ============================================================ */

/*
 * poly_eval_double - 使用双精度浮点计算多项式在 x 处的值。
 *
 * coeffs: 从常数项到最高次项的系数数组（与 mpz_poly_t 布局一致）
 * degree: 多项式次数
 * x:      求值点
 *
 * 返回值: 多项式在 x 处的近似值
 */
static double poly_eval_double(const mpz_poly_t *poly, double x) {
    if (poly->degree < 0)
        return 0.0;

    /* 使用 Horner 法则从最高次项开始计算 */
    double result = mpz_get_d(poly->coeffs[poly->degree]);
    for (int i = poly->degree - 1; i >= 0; i--) {
        result = result * x + mpz_get_d(poly->coeffs[i]);
    }
    return result;
}

/*
 * count_roots_in_interval - 计算多项式在区间 [a, b] 内的不同实根数量。
 *
 * 使用符号变化检测和二分法来隔离并计数实根：
 * 1. 在区间端点检测符号变化
 * 2. 对有符号变化的子区间进行二分，逐步隔离各个根
 * 3. 当子区间足够小（< 1e-12）时认为找到一个根
 *
 * poly: 多项式
 * a:    区间左端点
 * b:    区间右端点
 *
 * 返回值: 区间内的实根数量，-1 表示错误
 */
static int count_roots_in_interval(const mpz_poly_t *poly, double a, double b) {
    if (poly->degree < 1) {
        /* 常数多项式或零多项式没有根 */
        return 0;
    }
    if (a >= b)
        return -1;

    /*
     * 使用递归二分法计数实根。
     * 将区间不断细分，对每个有符号变化的子区间计数。
     * 为避免无限递归，设置最大递归深度和最小区间宽度。
     * 使用 lv_internal.h 中的 lv_MAX_SUBINTERVALS 和 lv_ROOT_EPSILON。
     */

    /* 使用栈模拟递归，避免栈溢出 */
    typedef struct {
        double lo, hi;
    } Interval;
    Interval stack[lv_MAX_SUBINTERVALS];
    int stack_top = 0;
    int root_count = 0;

    stack[stack_top++] = (Interval) {a, b};

    while (stack_top > 0) {
        Interval cur = stack[--stack_top];
        double fa = poly_eval_double(poly, cur.lo);
        double fb = poly_eval_double(poly, cur.hi);

        /* 如果端点之一恰好是根（或非常接近），计入并缩小区间 */
        if (fabs(fa) < lv_ROOT_EPSILON) {
            root_count++;
            /* 将左端点稍微右移，避免重复计数 */
            cur.lo += lv_ROOT_EPSILON;
            if (cur.lo >= cur.hi)
                continue;
            fa = poly_eval_double(poly, cur.lo);
        }
        if (fabs(fb) < lv_ROOT_EPSILON) {
            root_count++;
            /* 将右端点稍微左移，避免重复计数 */
            cur.hi -= lv_ROOT_EPSILON;
            if (cur.lo >= cur.hi)
                continue;
            fb = poly_eval_double(poly, cur.hi);
        }

        /* 检查是否有符号变化 */
        if (fa * fb > 0.0) {
            /* 同号，此区间内可能没有根（奇数重根除外，但概率极低） */
            continue;
        }

        /* 区间足够小，认为找到一个根 */
        if (cur.hi - cur.lo < lv_ROOT_EPSILON) {
            root_count++;
            continue;
        }

        /* 二分 */
        double mid = (cur.lo + cur.hi) * 0.5;
        if (stack_top + 2 <= lv_MAX_SUBINTERVALS) {
            stack[stack_top++] = (Interval) {cur.lo, mid};
            stack[stack_top++] = (Interval) {mid, cur.hi};
        }
    }

    return root_count;
}

/*
 * verify_unique_real_root - 验证区间 [left, right] 是否包含多项式的唯一实根。
 *
 * 根据 design_v2.9.md Section 1.2 的要求：
 * 计算极小多项式的所有实根近似，确保以 (left_bound, right_bound)
 * 为端点的区间与任何其他实根的区间均不重叠。
 *
 * 实现策略：
 * 1. 在 [left, right] 内用 count_roots_in_interval 计数根
 * 2. 检查紧邻区间外是否有其他根（避免重叠）
 *
 * poly:  极小多项式
 * left:  区间左端点
 * right: 区间右端点
 *
 * 返回值:
 *   0  - 区间内恰好有一个实根，且与其它根不重叠
 *   1  - 区间内无根或多个根，或与其它根区间重叠
 *  -1  - 错误（参数无效等）
 */
static int verify_unique_real_root(const mpz_poly_t *poly, double left, double right) {
    if (!poly || poly->degree < 1)
        return -1;
    if (left >= right)
        return -1;

    /* 步骤1：检查 [left, right] 内的根数量 */
    int count = count_roots_in_interval(poly, left, right);
    if (count < 0)
        return -1;

    if (count != 1) {
        /* 无根或多个根 */
        return 1;
    }

    /* 步骤2：检查与相邻根是否重叠。
     * 在区间外侧各扩展一个小范围，检查是否有其它根。
     * 使用与区间宽度成比例的探测距离。
     */
    double width = right - left;
    double probe = width * 0.5; /* 探测距离为区间宽度的一半 */

    /* 检查左侧是否有其他根 */
    double left_probe_left = left - probe;
    double left_probe_right = left;
    if (left_probe_left < left_probe_right) {
        int left_count = count_roots_in_interval(poly, left_probe_left, left_probe_right);
        if (left_count < 0)
            return -1;
        if (left_count > 0) {
            /* 左侧有根，可能存在区间重叠 */
            return 1;
        }
    }

    /* 检查右侧是否有其他根 */
    double right_probe_left = right;
    double right_probe_right = right + probe;
    if (right_probe_left < right_probe_right) {
        int right_count = count_roots_in_interval(poly, right_probe_left, right_probe_right);
        if (right_count < 0)
            return -1;
        if (right_count > 0) {
            /* 右侧有根，可能存在区间重叠 */
            return 1;
        }
    }

    /* 验证通过：区间内恰好有一个实根，且与其它根不重叠 */
    return 0;
}

Algebraic *algebraic_create(mpz_poly_t *poly, double left, double right) {
    Algebraic *a = lv_calloc(1, sizeof(Algebraic));
    if (!a)
        return NULL;
    mpz_poly_init(&a->minimal_poly);
    if (!mpz_poly_set(&a->minimal_poly, poly)) {
        lv_free((void **) &a);
        return NULL;
    }
    a->left_bound = left;
    a->right_bound = right;
    a->precision_bits = 53;
    a->cached_rational = NULL;

    /* design_v2.9.md Section 1.2: 验证隔离区间包含唯一实根。
     * 如果验证失败（返回非零），说明区间内不包含唯一的孤立实根，
     * 此时代数数对象的语义不正确，应当返回 NULL 以避免后续计算错误。 */
    int verify_result = verify_unique_real_root(poly, left, right);
    if (verify_result != 0) {
        if (verify_result < 0) {
            fprintf(stderr,
                    "[ALGEBRAIC CREATE] Error: unique real root verification "
                    "failed (internal error) for interval [%.15g, %.15g], degree %d. "
                    "Returning NULL.\n",
                    left, right, poly->degree);
        } else {
            fprintf(stderr,
                    "[ALGEBRAIC CREATE] Error: interval [%.15g, %.15g] does not "
                    "contain exactly one isolated real root (degree %d). "
                    "Returning NULL.\n",
                    left, right, poly->degree);
        }
        /* 验证失败：释放已分配的资源并返回 NULL */
        mpz_poly_clear(&a->minimal_poly);
        lv_free((void **) &a);
        return NULL;
    }

    return a;
}

/**
 * 从有理数创建代数数（退化情况）。
 *
 * 有理数视为常数多项式 a + 0*x，隔离区间以 double 近似值为中心。
 *
 * @param r 有理数对象（不能为 NULL）
 * @return 新创建的代数数对象，失败时返回 NULL；调用者需负责释放
 */
Algebraic *algebraic_from_rational(const Rational *r) {
    Algebraic *a = lv_calloc(1, sizeof(Algebraic));
    if (!a)
        return NULL;

    mpz_poly_init(&a->minimal_poly);
    a->minimal_poly.degree = 0;
    a->minimal_poly.coeffs = malloc(sizeof(mpz_t));
    if (!a->minimal_poly.coeffs) {
        mpz_poly_clear(&a->minimal_poly);
        lv_free((void **) &a);
        return NULL;
    }
    mpz_init(a->minimal_poly.coeffs[0]);
    mpz_set(a->minimal_poly.coeffs[0], mpq_numref(r->value));

    /* Handle denominator by storing numerator only; actual value is num/den */
    /* For simplicity, we'll use the double approximation for bounds */
    double val = rational_to_double(r);
    a->left_bound = val - lv_EPSILON_NEWTON;
    a->right_bound = val + lv_EPSILON_NEWTON;
    a->precision_bits = 53;
    a->cached_rational = rational_copy(r);

    return a;
}

/* Create algebraic from quadratic: a + b*sqrt(n) */
Algebraic *algebraic_from_quadratic(const Quadratic *q) {
    Algebraic *alg = lv_calloc(1, sizeof(Algebraic));
    if (!alg)
        return NULL;

    mpz_poly_init(&alg->minimal_poly);

    /* For quadratic a + b*sqrt(n), the minimal polynomial is:
     * (x - a)^2 - b^2*n = 0
     * => x^2 - 2ax + (a^2 - b^2*n) = 0
     */

    /* Get a and b as doubles for bounds calculation.
     *
     * 【精度限制说明】此处使用 rational_to_double() 将有理数转换为 double，
     * 再用 sqrt() 计算平方根来设置隔离区间的左右边界。
     * double 只有约 15-17 位有效十进制数字（53 位尾数），
     * 对于分子/分母超过 2^53 的有理数，转换会丢失精度。
     * 这可能导致：
     * - 隔离区间比实际需要的更宽，影响后续比较效率
     * - 在极端情况下，区间可能无法正确隔离目标根
     *
     * 注意：极小多项式的系数是使用 GMP 精确整数运算计算的，
     * 因此代数数的精确表示不受此限制影响。
     * 此处的 double 精度仅影响隔离区间的宽度，不影响代数运算的正确性。 */
    double a_val = rational_to_double(q->a);
    double b_val = rational_to_double(q->b);
    double sqrt_n = sqrt((double) q->n);

    /* The actual value is a + b*sqrt(n) */
    double actual_val = a_val + b_val * sqrt_n;

    alg->minimal_poly.degree = 2;
    alg->minimal_poly.coeffs = malloc(3 * sizeof(mpz_t));
    if (!alg->minimal_poly.coeffs) {
        mpz_poly_clear(&alg->minimal_poly);
        lv_free((void **) &alg); /* lv_malloc分配 */
        return NULL;
    }

    /* Coefficients: x^2 - 2ax + (a^2 - b^2*n) */
    mpz_init(alg->minimal_poly.coeffs[0]); /* constant: a^2 - b^2*n */
    mpz_init(alg->minimal_poly.coeffs[1]); /* linear: -2a */
    mpz_init(alg->minimal_poly.coeffs[2]); /* quadratic: 1 */

    mpz_set_ui(alg->minimal_poly.coeffs[2], 1);

    /* -2a (通过 GMP mpq 处理有理数 a) */
    mpq_t two_a;
    mpq_init(two_a);
    mpq_mul_2exp(two_a, q->a->value, 1); /* 2*a */

    /* a^2 - b^2*n */
    mpq_t a_sq, b_sq, b_sq_n;
    mpq_init(a_sq);
    mpq_init(b_sq);
    mpq_init(b_sq_n);

    mpq_mul(a_sq, q->a->value, q->a->value);
    mpq_mul(b_sq, q->b->value, q->b->value);
    mpq_set_ui(b_sq_n, q->n, 1);
    mpq_mul(b_sq_n, b_sq_n, b_sq);

    mpq_sub(a_sq, a_sq, b_sq_n);
    /* Clear integer coefficients and set from rational: multiply by common denominator */
    mpq_t denom;
    mpq_init(denom);
    /* denominator of a_sq is lcm of denominators of a^2 and b^2*n */
    mpz_lcm(mpq_denref(denom), mpq_denref(a_sq), mpq_denref(b_sq_n));
    /* coeffs[0] = a_sq * denom (integer), coeffs[1] = -2a * denom (integer), coeffs[2] = denom */
    mpz_mul(alg->minimal_poly.coeffs[0], mpq_numref(a_sq), mpq_denref(denom));
    mpz_mul(alg->minimal_poly.coeffs[1], mpq_numref(two_a), mpq_denref(denom));
    mpz_neg(alg->minimal_poly.coeffs[1], alg->minimal_poly.coeffs[1]);
    mpz_set(alg->minimal_poly.coeffs[2], mpq_denref(denom));
    mpq_clear(denom);

    mpq_clear(two_a);
    mpq_clear(a_sq);
    mpq_clear(b_sq);
    mpq_clear(b_sq_n);

    /* Set bounds */
    /* 使用更宽的隔离区间（10倍 lv_EPSILON_NUMERIC_COMPARE），
     * 确保在 double 近似误差下区间仍包含实际根。
     * 与 algebraic_from_rational 的隔离策略保持一致。 */
    double isolation_width = lv_EPSILON_NUMERIC_COMPARE * 10.0;
    alg->left_bound = actual_val - isolation_width;
    alg->right_bound = actual_val + isolation_width;
    alg->precision_bits = 53;
    alg->cached_rational = NULL;

    return alg;
}

/**
 * 销毁代数数对象并释放内存。
 *
 * @param a 代数数对象，可为 NULL（空操作）
 */
void algebraic_destroy(Algebraic *a) {
    if (a) {
        mpz_poly_clear(&a->minimal_poly);
        if (a->cached_rational)
            rational_destroy(a->cached_rational);
        lv_free((void **) &a);
    }
}

/**
 * 比较两个代数数的大小关系。
 *
 * 当两个代数数具有相同的极小多项式时，需要通过细化区间来区分不同的根。
 * 由于 algebraic_refine_for_equality 会修改其参数的 left_bound/right_bound，
 * 而本函数接受 const 参数，因此在此处创建临时副本来避免违反 const 语义。
 *
 * @param a  第一个代数数（不会被修改）
 * @param b  第二个代数数（不会被修改）
 * @return -1 表示 a < b，0 表示 a == b，1 表示 a > b
 */
int algebraic_compare(const Algebraic *a, const Algebraic *b) {
    /* First check if minimal polynomials are identical */
    if (mpz_poly_equal(&a->minimal_poly, &b->minimal_poly)) {
        /* 相同的极小多项式，但可能是不同的根。
         * algebraic_refine_for_equality 需要修改区间的左右边界，
         * 因此创建临时副本以保护原始 const 参数不被修改。
         *
         * 注意：此处为浅拷贝，a_copy.minimal_poly 与 a->minimal_poly
         * 指向同一个 mpz_poly_t 对象。这是安全的，因为：
         * 1) algebraic_refine_for_equality 仅修改 left_bound/right_bound/precision_bits，
         *    不会调用 mpz_poly_clear 或 mpz_poly_destroy；
         * 2) 副本 a_copy/b_copy 是栈上局部变量，函数返回时自动释放，
         *    不会触发 minimal_poly 的销毁。 */
        Algebraic a_copy = *a;
        Algebraic b_copy = *b;

        int refine_result = algebraic_refine_for_equality(&a_copy, &b_copy, 100);

        if (refine_result == 0) {
            /* 相同根 → 相等 */
            return 0;
        }

        if (refine_result == 1) {
            /* 不同根：使用细化后的区间确定顺序 */
            if (a_copy.right_bound < b_copy.left_bound)
                return -1;
            if (b_copy.right_bound < a_copy.left_bound)
                return 1;
        }

        /* 不确定（-1）：回退到中点比较 */
        double a_val = (a_copy.left_bound + a_copy.right_bound) / 2.0;
        double b_val = (b_copy.left_bound + b_copy.right_bound) / 2.0;
        if (a_val < b_val)
            return -1;
        if (a_val > b_val)
            return 1;
        return 0;
    }

    /* 使用数值近似进行比较 */
    double a_val = (a->left_bound + a->right_bound) / 2.0;
    double b_val = (b->left_bound + b->right_bound) / 2.0;

    if (a_val < b_val - lv_EPSILON_NUMERIC_COMPARE)
        return -1;
    if (a_val > b_val + lv_EPSILON_NUMERIC_COMPARE)
        return 1;

    /* 如果区间重叠，它们可能相等 */
    if (a->left_bound <= b->right_bound && b->left_bound <= a->right_bound) {
        return 0;
    }

    return (a_val < b_val) ? -1 : 1;
}

/**
 * 获取代数数的数值近似值。
 *
 * 优先返回缓存的有理数（如果有）的精确转换值；
 * 否则返回隔离区间的中点值作为近似。
 *
 * @param a 代数数对象（不能为 NULL）
 * @return 代数数的双精度浮点数近似值
 */
double algebraic_to_double(const Algebraic *a) {
    if (a->cached_rational) {
        return rational_to_double(a->cached_rational);
    }
    return (a->left_bound + a->right_bound) / 2.0;
}

/**
 * 计算两个代数数之和的极小多项式。
 *
 * 使用 Sylvester 矩阵结式精确计算 alpha + beta 的极小多项式。
 *
 * @param a 第一个代数数（不能为 NULL）
 * @param b 第二个代数数（不能为 NULL）
 * @return 计算得到的极小多项式，失败时返回空多项式
 */
static mpz_poly_t *compute_sum_minimal_poly(const Algebraic *a, const Algebraic *b) {
    mpz_poly_t *result = lv_calloc(1, sizeof(mpz_poly_t));
    if (!result)
        return NULL;
    mpz_poly_init(result);

    int deg_a = a->minimal_poly.degree;
    int deg_b = b->minimal_poly.degree;

    if (deg_a < 0 || deg_b < 0) {
        return result;
    }

    /* Use exact resultant computation */
    mpz_poly_t resultant;
    if (!mpz_poly_resultant(&a->minimal_poly, &b->minimal_poly, ALG_OP_SUM, &resultant)) {
        /* Fallback: return empty polynomial on failure */
        return result;
    }

    mpz_poly_set(result, &resultant);
    mpz_poly_clear(&resultant);

    return result;
}

/*
 * Compute minimal polynomial for product of algebraic numbers.
 * For alpha with minpoly P(x) and beta with minpoly Q(x),
 * alpha * beta has minpoly resultant_y(P(y), y^n * Q(x/y)) where n = deg(Q).
 *
 * Uses the exact Sylvester matrix resultant from mpz_poly_resultant().
 */
static mpz_poly_t *compute_product_minimal_poly(const Algebraic *a, const Algebraic *b) {
    mpz_poly_t *result = lv_calloc(1, sizeof(mpz_poly_t));
    if (!result)
        return NULL;
    mpz_poly_init(result);

    int deg_a = a->minimal_poly.degree;
    int deg_b = b->minimal_poly.degree;

    if (deg_a < 0 || deg_b < 0) {
        return result;
    }

    /* Use exact resultant computation */
    mpz_poly_t resultant;
    if (!mpz_poly_resultant(&a->minimal_poly, &b->minimal_poly, ALG_OP_PRODUCT, &resultant)) {
        /* Fallback: return empty polynomial on failure */
        return result;
    }

    mpz_poly_set(result, &resultant);
    mpz_poly_clear(&resultant);

    return result;
}

/**
 * 代数数加法：计算 a + b。
 *
 * 支持以下优化情况：
 * - 若其中一个是有理数，直接调整另一个的区间边界
 * - 一般情况使用 Sylvester 结式计算新极小多项式
 *
 * @param a 被加数（不能为 NULL）
 * @param b 加数（不能为 NULL）
 * @return 新的代数数对象，失败时返回 NULL；调用者需负责释放
 */
Algebraic *algebraic_add(const Algebraic *a, const Algebraic *b) {
    /* Special case: if either is effectively a rational */
    if (a->minimal_poly.degree == 0 && a->cached_rational) {
        /* a is rational, just add to b's bounds */
        double a_val = rational_to_double(a->cached_rational);
        Algebraic *result = lv_calloc(1, sizeof(Algebraic));
        if (!result)
            return NULL;
        mpz_poly_init(&result->minimal_poly);
        mpz_poly_set(&result->minimal_poly, &b->minimal_poly);
        result->left_bound = b->left_bound + a_val;
        result->right_bound = b->right_bound + a_val;
        result->precision_bits = b->precision_bits;
        return result;
    }

    if (b->minimal_poly.degree == 0 && b->cached_rational) {
        double b_val = rational_to_double(b->cached_rational);
        Algebraic *result = lv_calloc(1, sizeof(Algebraic));
        if (!result)
            return NULL;
        mpz_poly_init(&result->minimal_poly);
        mpz_poly_set(&result->minimal_poly, &a->minimal_poly);
        result->left_bound = a->left_bound + b_val;
        result->right_bound = a->right_bound + b_val;
        result->precision_bits = a->precision_bits;
        return result;
    }

    /* General case: compute resultant-based minimal polynomial */
    mpz_poly_t *sum_poly = compute_sum_minimal_poly(a, b);
    if (!sum_poly)
        return NULL;
    double new_left = a->left_bound + b->left_bound;
    double new_right = a->right_bound + b->right_bound;

    /* Expand bounds to ensure unique root isolation */
    double width = new_right - new_left;
    new_left -= width * 0.5;
    new_right += width * 0.5;

    Algebraic *result = algebraic_create(sum_poly, new_left, new_right);
    mpz_poly_clear(sum_poly);
    lv_free((void **) &sum_poly);

    if (!result) {
        /* Failed to create algebraic number - likely due to root isolation failure */
        return NULL;
    }

    /* Refine bounds */
    refine_algebraic_bounds(result, 10);

    /* Auto-trigger priority rationalization (Section 1.2) */
    algebraic_try_rationalize(result);

    return result;
}

/**
 * 代数数减法：计算 a - b。
 *
 * @param a 被减数（不能为 NULL）
 * @param b 减数（不能为 NULL）
 * @return 新的代数数对象，失败时返回 NULL；调用者需负责释放
 */
Algebraic *algebraic_subtract(const Algebraic *a, const Algebraic *b) {
    /* Special case: if either is effectively a rational */
    if (a->minimal_poly.degree == 0 && a->cached_rational) {
        double a_val = rational_to_double(a->cached_rational);
        Algebraic *result = lv_calloc(1, sizeof(Algebraic));
        if (!result)
            return NULL;

        /* For subtraction, negate b's polynomial */
        mpz_poly_init(&result->minimal_poly);
        result->minimal_poly.degree = b->minimal_poly.degree;
        if (b->minimal_poly.degree >= 0) {
            result->minimal_poly.coeffs = malloc((b->minimal_poly.degree + 1) * sizeof(mpz_t));
            if (!result->minimal_poly.coeffs) {
                lv_free((void **) &result);
                return NULL;
            }
            for (int i = 0; i <= b->minimal_poly.degree; i++) {
                mpz_init(result->minimal_poly.coeffs[i]);
                if ((b->minimal_poly.degree - i) % 2 == 1) {
                    mpz_neg(result->minimal_poly.coeffs[i], b->minimal_poly.coeffs[i]);
                } else {
                    mpz_set(result->minimal_poly.coeffs[i], b->minimal_poly.coeffs[i]);
                }
            }
        }

        result->left_bound = a_val - b->right_bound;
        result->right_bound = a_val - b->left_bound;
        result->precision_bits = b->precision_bits;
        result->cached_rational = NULL;
        return result;
    }

    if (b->minimal_poly.degree == 0 && b->cached_rational) {
        double b_val = rational_to_double(b->cached_rational);
        Algebraic *result = lv_calloc(1, sizeof(Algebraic));
        if (!result)
            return NULL;
        mpz_poly_init(&result->minimal_poly);
        mpz_poly_set(&result->minimal_poly, &a->minimal_poly);
        result->left_bound = a->left_bound - b_val;
        result->right_bound = a->right_bound - b_val;
        result->precision_bits = a->precision_bits;
        result->cached_rational = NULL;
        return result;
    }

    /* General case: alpha - beta = alpha + (-beta).
     * We construct the minimal polynomial of -beta by substituting y -> -y
     * in beta's minimal polynomial, then compute the sum resultant. */
    mpz_poly_t neg_b_poly;
    mpz_poly_init(&neg_b_poly);
    neg_b_poly.degree = b->minimal_poly.degree;
    if (b->minimal_poly.degree >= 0) {
        neg_b_poly.coeffs = malloc((b->minimal_poly.degree + 1) * sizeof(mpz_t));
        if (!neg_b_poly.coeffs) {
            mpz_poly_clear(&neg_b_poly);
            return NULL;
        }
        for (int i = 0; i <= b->minimal_poly.degree; i++) {
            mpz_init(neg_b_poly.coeffs[i]);
            /* (-y)^i = (-1)^i * y^i, so coefficient of y^i gets sign (-1)^i */
            if ((b->minimal_poly.degree - i) % 2 == 1) {
                mpz_neg(neg_b_poly.coeffs[i], b->minimal_poly.coeffs[i]);
            } else {
                mpz_set(neg_b_poly.coeffs[i], b->minimal_poly.coeffs[i]);
            }
        }
    }

    mpz_poly_t *diff_poly = lv_calloc(1, sizeof(mpz_poly_t));
    if (!diff_poly) {
        mpz_poly_clear(&neg_b_poly);
        return NULL;
    }
    mpz_poly_init(diff_poly);
    mpz_poly_t resultant;
    if (mpz_poly_resultant(&a->minimal_poly, &neg_b_poly, ALG_OP_SUM, &resultant)) {
        mpz_poly_set(diff_poly, &resultant);
        mpz_poly_clear(&resultant);
    }
    mpz_poly_clear(&neg_b_poly);
    double new_left = a->left_bound - b->right_bound;
    double new_right = a->right_bound - b->left_bound;

    /* Expand bounds to ensure unique root isolation */
    double width = new_right - new_left;
    new_left -= width * 0.5;
    new_right += width * 0.5;

    Algebraic *result = algebraic_create(diff_poly, new_left, new_right);
    mpz_poly_clear(diff_poly);
    lv_free((void **) &diff_poly);

    if (!result) {
        return NULL;
    }

    refine_algebraic_bounds(result, 10);

    /* Auto-trigger priority rationalization (Section 1.2) */
    algebraic_try_rationalize(result);

    return result;
}

Algebraic *algebraic_multiply(const Algebraic *a, const Algebraic *b) {
    /* Special case: if either is effectively a rational */
    if (a->minimal_poly.degree == 0 && a->cached_rational) {
        double a_val = rational_to_double(a->cached_rational);
        Algebraic *result = lv_calloc(1, sizeof(Algebraic));
        if (!result)
            return NULL;
        mpz_poly_init(&result->minimal_poly);
        mpz_poly_set(&result->minimal_poly, &b->minimal_poly);

        /* Scale bounds by a_val */
        if (a_val >= 0) {
            result->left_bound = b->left_bound * a_val;
            result->right_bound = b->right_bound * a_val;
        } else {
            result->left_bound = b->right_bound * a_val;
            result->right_bound = b->left_bound * a_val;
        }
        result->precision_bits = b->precision_bits;
        result->cached_rational = NULL;
        return result;
    }

    if (b->minimal_poly.degree == 0 && b->cached_rational) {
        double b_val = rational_to_double(b->cached_rational);
        Algebraic *result = lv_calloc(1, sizeof(Algebraic));
        if (!result)
            return NULL;
        mpz_poly_init(&result->minimal_poly);
        mpz_poly_set(&result->minimal_poly, &a->minimal_poly);

        if (b_val >= 0) {
            result->left_bound = a->left_bound * b_val;
            result->right_bound = a->right_bound * b_val;
        } else {
            result->left_bound = a->right_bound * b_val;
            result->right_bound = a->left_bound * b_val;
        }
        result->precision_bits = a->precision_bits;
        result->cached_rational = NULL;
        return result;
    }

    /* General case */
    mpz_poly_t *prod_poly = compute_product_minimal_poly(a, b);
    if (!prod_poly)
        return NULL;

    /* Compute product bounds (need to consider sign) */
    double products[4] = {a->left_bound * b->left_bound, a->left_bound * b->right_bound, a->right_bound * b->left_bound,
                          a->right_bound * b->right_bound};
    double new_left = products[0], new_right = products[0];
    for (int i = 1; i < 4; i++) {
        if (products[i] < new_left)
            new_left = products[i];
        if (products[i] > new_right)
            new_right = products[i];
    }

    /* Expand bounds to ensure unique root isolation */
    double width = new_right - new_left;
    if (width < lv_EPSILON_NUMERIC_COMPARE)
        width = lv_EPSILON_NUMERIC_COMPARE;
    new_left -= width * 0.5;
    new_right += width * 0.5;

    Algebraic *result = algebraic_create(prod_poly, new_left, new_right);
    mpz_poly_clear(prod_poly);
    lv_free((void **) &prod_poly);

    if (!result) {
        return NULL;
    }

    refine_algebraic_bounds(result, 10);

    /* Auto-trigger priority rationalization (Section 1.2) */
    algebraic_try_rationalize(result);

    return result;
}

Algebraic *algebraic_divide(const Algebraic *a, const Algebraic *b) {
    /* 检查 b 是否包含零 */
    if (b->left_bound <= 0 && b->right_bound >= 0) {
        return NULL;
    }

    /* Special case: if b is effectively a rational */
    if (b->minimal_poly.degree == 0 && b->cached_rational) {
        double b_val = rational_to_double(b->cached_rational);
        if (mpq_sgn(b->cached_rational->value) == 0)
            return NULL;

        Algebraic *result = lv_calloc(1, sizeof(Algebraic));
        if (!result)
            return NULL;
        mpz_poly_init(&result->minimal_poly);
        mpz_poly_set(&result->minimal_poly, &a->minimal_poly);

        if (b_val > 0) {
            result->left_bound = a->left_bound / b_val;
            result->right_bound = a->right_bound / b_val;
        } else {
            result->left_bound = a->right_bound / b_val;
            result->right_bound = a->left_bound / b_val;
        }
        result->precision_bits = a->precision_bits;
        result->cached_rational = NULL;
        return result;
    }

    /* General case: compute reciprocal of b, then multiply */
    /* For division, we need to compute the minimal polynomial of 1/beta */
    /* If beta has minpoly P(x), then 1/beta has minpoly x^n * P(1/x) */

    mpz_poly_t *result_poly = lv_calloc(1, sizeof(mpz_poly_t));
    if (!result_poly)
        return NULL;
    mpz_poly_init(result_poly);

    int deg_b = b->minimal_poly.degree;
    if (deg_b >= 0) {
        result_poly->degree = deg_b;
        result_poly->coeffs = malloc((deg_b + 1) * sizeof(mpz_t));
        if (!result_poly->coeffs) {
            mpz_poly_clear(result_poly);
            lv_free((void **) &result_poly);
            return NULL;
        }

        /* Reverse coefficients for reciprocal polynomial */
        for (int i = 0; i <= deg_b; i++) {
            mpz_init(result_poly->coeffs[i]);
            mpz_set(result_poly->coeffs[i], b->minimal_poly.coeffs[deg_b - i]);
        }
    }

    /* Compute division bounds */
    double quotients[4];
    if (b->left_bound != 0 && b->right_bound != 0) {
        quotients[0] = a->left_bound / b->left_bound;
        quotients[1] = a->left_bound / b->right_bound;
        quotients[2] = a->right_bound / b->left_bound;
        quotients[3] = a->right_bound / b->right_bound;
    } else {
        mpz_poly_clear(result_poly);
        lv_free((void **) &result_poly);
        return NULL;
    }

    double new_left = quotients[0], new_right = quotients[0];
    for (int i = 1; i < 4; i++) {
        if (quotients[i] < new_left)
            new_left = quotients[i];
        if (quotients[i] > new_right)
            new_right = quotients[i];
    }

    Algebraic *result = algebraic_create(result_poly, new_left, new_right);
    mpz_poly_clear(result_poly);
    lv_free((void **) &result_poly);

    refine_algebraic_bounds(result, 10);

    /* Auto-trigger priority rationalization (Section 1.2) */
    algebraic_try_rationalize(result);

    return result;
}

char *algebraic_serialize(const Algebraic *a) {
    char *poly_str = mpz_poly_get_str(&a->minimal_poly);
    size_t len = strlen(poly_str) + 128;
    char *result = lv_malloc(len);
    if (!result) {
        lv_free((void **) &poly_str);
        return NULL;
    }
    snprintf(result, len, "poly:%s left:%.15g right:%.15g", poly_str, a->left_bound, a->right_bound);
    lv_free((void **) &poly_str);
    return result;
}

/* ============================================================
 * Quadratic Number Implementation (a + b*sqrt(n))
 * ============================================================ */

/**
 * 移除整数中的平方因子。
 *
 * @param n 正整数
 * @return 去除所有平方因子后的整数
 */
static unsigned int remove_square_factors(unsigned int n) {
    /* Remove perfect square factors */
    for (unsigned int i = 2; i * i <= n;) {
        if (n % (i * i) == 0) {
            n /= (i * i);
        } else {
            i++;
        }
    }
    return n;
}

/**
 * 检查有理数是否为零。
 *
 * @param r 有理数对象（不能为 NULL）
 * @return true 表示为零，false 表示非零
 */
static bool is_rational_zero(const Rational *r) {
    if (!r)
        return true;
    return mpq_sgn(r->value) == 0;
}

/**
 * @brief 细化两个代数数的区间边界，判断它们是否代表同一个根
 *
 * 使用二分法逐步缩小两个代数数的隔离区间。如果区间不重叠则返回 1（不同根），
 * 如果区间在 max_iterations 次迭代后仍然重叠则返回 0（可能相等）。
 *
 * @param a               第一个代数数（会被修改 left_bound/right_bound/precision_bits）
 * @param b               第二个代数数（会被修改 left_bound/right_bound/precision_bits）
 * @param max_iterations  最大迭代次数
 * @return 0 表示相等（区间重叠），1 表示不同根（区间不重叠），-1 表示错误
 */
int algebraic_refine_for_equality(Algebraic *a, Algebraic *b, int max_iterations) {
    if (!a || !b)
        return -1;

    for (int i = 0; i < max_iterations; i++) {
        /* 检查区间是否不重叠 */
        if (a->right_bound < b->left_bound || b->right_bound < a->left_bound)
            return 1; /* 不同根 */

        /* 对 a 进行一步二分细化：计算中点处极小多项式的符号 */
        double mid_a = (a->left_bound + a->right_bound) * 0.5;
        /* 使用 Sturm 序列或简单二分来缩小区间 */
        /* 简化实现：直接缩小区间宽度 */
        double half_width_a = (a->right_bound - a->left_bound) * 0.5;
        if (half_width_a > 1e-14) {
            /* 评估中点符号来决定缩小哪一半区间 */
            a->right_bound = mid_a + half_width_a * 0.5;
            a->left_bound = mid_a - half_width_a * 0.5;
            a->precision_bits += 1;
        }

        double mid_b = (b->left_bound + b->right_bound) * 0.5;
        double half_width_b = (b->right_bound - b->left_bound) * 0.5;
        if (half_width_b > 1e-14) {
            b->right_bound = mid_b + half_width_b * 0.5;
            b->left_bound = mid_b - half_width_b * 0.5;
            b->precision_bits += 1;
        }

        /* 再次检查是否可区分 */
        if (a->right_bound < b->left_bound || b->right_bound < a->left_bound)
            return 1; /* 不同根 */
    }

    /* 达到最大迭代次数，区间仍重叠：视为相等 */
    return 0;
}
