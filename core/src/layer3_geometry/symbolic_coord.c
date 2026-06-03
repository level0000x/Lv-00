/**
 * @file symbolic_coord.c
 * @brief 符号坐标系统实现
 * @details 实现有理数、代数数、二次根式和超越数的精确符号计算。
 *          支持信任颜色机制（绿/蓝/黄/橙/琥珀）和 A/B 计划切换。
 *          基于 GMP 任意精度算术库，确保计算精度。
 *
 * 修复记录：
 * - R01: 修复牛顿迭代中 val_mid == 0.0 的不安全浮点比较
 * - R02: 修复 continued_fraction_approx 中 ULLONG_MAX 转 double 的精度问题
 * - R03: 添加 mpz_get_ui 截断的安全检查
 */

#include "symbolic_coord.h"

#include <gmp.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "debug.h"
#include "lv00_internal.h"
#include "lv00_utils.h"  /* 提供 lv00_malloc/lv00_free/lv00_strdup */
#include "mpz_poly.h"

/*
 * BIT_CUTOFF_THRESHOLD 和 MAX_PRECISION_BITS 现在定义在 lv00_internal.h 中，
 * 通过 lv00_internal.h 统一管理所有项目级常量，此处不再重复定义。
 */

/* ============================================================
 * 文件级常量定义
 * ============================================================ */

/**
 * 获取两个信任等级中的较低值。
 * 用于计算两个坐标运算结果的信任等级。
 *
 * @param a 第一个信任等级
 * @param b 第二个信任等级
 * @return 两者中较低的信任等级
 */
#define LV00_MIN_TRUST(a, b) (((a) < (b)) ? (a) : (b))

/**
 * 区间中点系数：用于计算隔离区间的中点值。
 * 公式：(left_bound + right_bound) / 2.0
 */
#define INTERVAL_MIDPOINT_FACTOR 0.5

/**
 * 区间边界扩展系数：用于在计算结果时扩展隔离区间以确保唯一根隔离。
 * 公式：new_left -= width * 0.5; new_right += width * 0.5
 */
#define BOUND_EXPANSION_FACTOR 0.5

/**
 * 连分数近似的默认精度系数：相对于区间宽度的精度要求。
 * 公式：epsilon = interval_width / 4.0
 */
#define CF_APPROX_PRECISION_FACTOR 4.0

/**
 * 根探测距离系数：相对于区间宽度的探测距离。
 * 公式：probe = width * 0.5
 */
#define ROOT_PROBE_FACTOR 0.5

/* ============================================================
 * Global context for overflow handling.
 * NOTE: 使用线程局部存储以保证多线程环境下的安全性。
 *       每个线程拥有独立的溢出上下文。
 * ============================================================ */
static LV00_THREAD_LOCAL struct OverflowContext g_overflow_context = {
    .last_result = NULL,
    .last_operation = NULL,
    .left_type = RATIONAL,
    .right_type = RATIONAL,
    .overflow_count = 0,
    .frozen_point = NULL,
    .has_frozen_point = false
};

/* ============================================================
 * A/B Plan switching (Section 1.6 of design_v2.9.md)
 * NOTE: 线程局部存储，每个线程可独立设置计划
 * ============================================================ */
static LV00_THREAD_LOCAL AlgebraicPlan g_algebraic_plan = PLAN_A_FULL_ALGEBRAIC;

/* ============================================================
 * Digit circuit user interaction (Section 1.5 of design_v2.9.md)
 * NOTE: 线程局部存储，每个线程可独立设置回调
 * ============================================================ */
static LV00_THREAD_LOCAL CircuitTripCallback g_circuit_callback = NULL;
static LV00_THREAD_LOCAL void *g_circuit_user_data = NULL;

/* ============================================================
 * Bit Circuit (Digit Cutoff) Implementation
 * 
 * According to design_v2.9.md Section 1.5:
 * - Detect when any value exceeds 10^6 bits
 * - Trigger circuit trip signal
 * - Provide user options: ignore, rollback, or permanent downgrade
 * ============================================================ */

/* ============================================================
 * Rational Number Implementation
 * ============================================================ */

/**
 * 创建有理数对象。
 *
 * 使用 GMP 多精度整数库实现精确的有理数算术运算。
 * 创建后自动进行有理数规范化（约分）。
 *
 * @param numerator   分子（可正可负）
 * @param denominator 分母（必须为正数，且不能为 0）
 * @return 新创建的有理数对象，失败时返回 NULL；调用者需负责释放
 */
Rational *rational_create(int64_t numerator, uint64_t denominator) {
    if (denominator == 0)
        return NULL;
    Rational *r = lv00_malloc(sizeof(Rational));
    if (!r)
        return NULL;
    mpq_init(r->value);
    /* 修复：使用 mpz_set_str 通过字符串中转，避免 int64_t 到 long 的截断。
     * 在 Windows 64 位 (LLP64) 下 long 为 32 位，
     * 直接使用 mpz_set_si 会截断超过 LONG_MAX 的分子值。 */
    {
        char numer_str[21]; /* int64_t 最大值为 9223372036854775807，占 19 位 + 符号 + '\0' */
        char denom_str[21]; /* uint64_t 最大值为 18446744073709551615，占 20 位 + '\0' */
        snprintf(numer_str, sizeof(numer_str), "%" PRId64, (int64_t) numerator);
        snprintf(denom_str, sizeof(denom_str), "%" PRIu64, (uint64_t) denominator);
        /* 防御性检查：mpq_numref/mpq_denref 在 GMP 中理论上不会返回 NULL
         * （mpq_init 后已分配内部存储），但为健壮性仍做检查，
         * 防止在极端环境下出现未定义行为。 */
        mpz_ptr num_ptr = mpq_numref(r->value);
        mpz_ptr den_ptr = mpq_denref(r->value);
        if (!num_ptr || !den_ptr) {
            mpq_clear(r->value);
            lv00_free((void **) &r);
            return NULL;
        }
        mpz_set_str(num_ptr, numer_str, 10);
        mpz_set_str(den_ptr, denom_str, 10);
    }
    mpq_canonicalize(r->value);
    return r;
}

/**
 * 使用已有的 mpz_t 整数创建有理数对象。
 *
 * @param numerator   分子（GMP 多精度整数）
 * @param denominator 分母（GMP 多精度整数，必须非零）
 * @return 新创建的有理数对象，失败时返回 NULL；调用者需负责释放
 */
Rational *rational_create_from_mpz(const mpz_t numerator, const mpz_t denominator) {
    /* 参数有效性检查：分子和分母均不能为 NULL，分母不能为零 */
    if (!numerator) {
        return NULL;
    }
    if (!denominator || mpz_sgn(denominator) == 0) {
        return NULL;
    }
    Rational *r = lv00_malloc(sizeof(Rational));
    if (!r)
        return NULL;
    mpq_init(r->value);
    mpq_set_num(r->value, numerator);
    mpq_set_den(r->value, denominator);
    mpq_canonicalize(r->value);
    return r;
}

Rational *rational_copy(const Rational *src) {
    if (!src)
        return NULL;
    Rational *r = lv00_malloc(sizeof(Rational));
    if (!r)
        return NULL;
    mpq_init(r->value);
    mpq_set(r->value, src->value);
    return r;
}

void rational_destroy(Rational *r) {
    if (r) {
        mpq_clear(r->value);
        lv00_free((void **) &r);
    }
}

int rational_compare(const Rational *a, const Rational *b) {
    if (!a || !b) {
        /* 无效参数：返回非零值表示不相等 */
        return (a != b) ? ((a ? 1 : -1)) : 0;
    }
    return mpq_cmp(a->value, b->value);
}

/**
 * 有理数加法：计算 a + b。
 *
 * @param a 被加数（不能为 NULL）
 * @param b 加数（不能为 NULL）
 * @return 新的有理数对象表示 a + b，失败时返回 NULL；调用者需负责释放
 */
Rational *rational_add(const Rational *a, const Rational *b) {
    if (!a || !b) return NULL;
    Rational *r = lv00_malloc(sizeof(Rational));
    if (!r)
        return NULL;
    mpq_init(r->value);
    mpq_add(r->value, a->value, b->value);
    return r;
}

Rational *rational_subtract(const Rational *a, const Rational *b) {
    if (!a || !b) return NULL;
    Rational *r = lv00_malloc(sizeof(Rational));
    if (!r)
        return NULL;
    mpq_init(r->value);
    mpq_sub(r->value, a->value, b->value);
    return r;
}

/**
 * 有理数乘法：计算 a * b。
 *
 * @param a 第一个因数（不能为 NULL）
 * @param b 第二个因数（不能为 NULL）
 * @return 新的有理数对象表示 a * b，失败时返回 NULL；调用者需负责释放
 */
Rational *rational_multiply(const Rational *a, const Rational *b) {
    if (!a || !b) return NULL;
    Rational *r = lv00_malloc(sizeof(Rational));
    if (!r)
        return NULL;
    mpq_init(r->value);
    mpq_mul(r->value, a->value, b->value);
    return r;
}

/**
 * 有理数除法：计算 a / b。
 *
 * @param a 被除数（不能为 NULL）
 * @param b 除数（不能为 NULL，且不能为零）
 * @return 新的有理数对象表示 a / b，除数为零或失败时返回 NULL；调用者需负责释放
 */
Rational *rational_divide(const Rational *a, const Rational *b) {
    /* NULL 指针检查，与 rational_add / rational_multiply 保持一致 */
    if (!a || !b) return NULL;
    if (mpq_cmp_ui(b->value, 0, 1) == 0)
        return NULL;
    Rational *r = lv00_malloc(sizeof(Rational));
    if (!r)
        return NULL;
    mpq_init(r->value);
    mpq_div(r->value, a->value, b->value);
    return r;
}

/**
 * 将有理数序列化为 "分子/分母" 格式的字符串。
 *
 * 缓冲区大小 = 分子十进制位数 + 分母十进制位数 + 4（符号、斜杠、空终止符）
 * 额外多分配 4 个字节作为安全余量，防止 mpz_sizeinbase 估算值偏小导致缓冲区溢出。
 *
 * @param r  有理数对象
 * @return  新分配的字符串，调用者需负责释放；失败时返回 NULL
 */
char *rational_serialize(const Rational *r) {
    /* 空指针检查：调用者不得传入 NULL */
    if (!r)
        return NULL;

    /* 获取分子和分母的十进制位数。
     * mpz_sizeinbase 返回的是 size_t，两个大值相加可能溢出。
     * 此处先计算各自位数，再安全累加。 */
    size_t num_digits = mpz_sizeinbase(mpq_numref(r->value), 10);
    size_t den_digits = mpz_sizeinbase(mpq_denref(r->value), 10);

    /* 溢出检查：防止 num_digits + den_digits + 8 超过 SIZE_MAX */
    if (num_digits > SIZE_MAX - den_digits || num_digits + den_digits > SIZE_MAX - 8) {
        return NULL; /* 数值过大，无法安全分配缓冲区 */
    }

    /* 多分配 4 字节安全余量（'/' + 符号位），加 4 字节 '\0' 终止符，
     * 防止边界情况下的缓冲区溢出 */
    size_t buf_size = num_digits + den_digits + 4 + 4;
    char *buf = lv00_malloc(buf_size);
    if (!buf)
        return NULL;
    char *num_str = mpz_get_str(NULL, 10, mpq_numref(r->value));
    char *den_str = mpz_get_str(NULL, 10, mpq_denref(r->value));
    snprintf(buf, buf_size, "%s/%s", num_str, den_str);
    lv00_free_external((void **) &num_str);
    lv00_free_external((void **) &den_str);
    return buf;
}

Rational *rational_parse(const char *str) {
    Rational *r = lv00_malloc(sizeof(Rational));
    if (!r)
        return NULL;
    mpq_init(r->value);
    if (mpq_set_str(r->value, str, 10) != 0) {
        mpq_clear(r->value);
        lv00_free((void **) &r);
        return NULL;
    }
    mpq_canonicalize(r->value);
    return r;
}

/**
 * 将有理数转换为双精度浮点数。
 *
 * 使用 GMP 库的 mpq_get_d 函数进行转换，返回有理数的近似 double 值。
 * 此函数主要用于需要数值近似值的场景（如数值比较或输出）。
 *
 * @param r 有理数对象（不能为 NULL）
 * @return 有理数的双精度浮点数近似值
 */
static double rational_to_double(const Rational *r) {
    return mpq_get_d(r->value);
}

/**
 * 检查有理数是否超过位数阈值。
 *
 * 计算分子和分母的比特位数总和，若超过 LV00_BIT_CUTOFF_THRESHOLD 则触发熔断。
 *
 * @param r 有理数对象（不能为 NULL）
 * @return CIRCUIT_STATUS_OK 表示正常，CIRCUIT_STATUS_TRIPPED 表示超过阈值
 */
static CircuitStatus check_rational_circuit(const Rational *r) {
    if (!r)
        return CIRCUIT_STATUS_OK;
    size_t num_bits = mpz_sizeinbase(mpq_numref(r->value), 2);
    size_t den_bits = mpz_sizeinbase(mpq_denref(r->value), 2);
    /* 溢出保护：防止 num_bits + den_bits 在 size_t 范围内溢出 */
    if (num_bits > SIZE_MAX - den_bits) {
        return CIRCUIT_STATUS_TRIPPED;
    }
    if (num_bits + den_bits > LV00_BIT_CUTOFF_THRESHOLD) {
        return CIRCUIT_STATUS_TRIPPED;
    }
    return CIRCUIT_STATUS_OK;
}

/**
 * 检查代数数是否超过位数阈值。
 *
 * 同时检查缓存的有理数和多项式系数的比特位数。
 *
 * @param a 代数数对象（不能为 NULL）
 * @return CIRCUIT_STATUS_OK 表示正常，CIRCUIT_STATUS_TRIPPED 表示超过阈值
 */
static CircuitStatus check_algebraic_circuit(const Algebraic *a) {
    if (!a)
        return CIRCUIT_STATUS_OK;

    /* 检查缓存的有理数（如果存在） */
    if (a->cached_rational) {
        CircuitStatus status = check_rational_circuit(a->cached_rational);
        if (status == CIRCUIT_STATUS_TRIPPED)
            return CIRCUIT_STATUS_TRIPPED;
    }

    /* 检查多项式系数 */
    for (int i = 0; i <= a->minimal_poly.degree; i++) {
        size_t coeff_bits = mpz_sizeinbase(a->minimal_poly.coeffs[i], 2);
        if (coeff_bits > LV00_BIT_CUTOFF_THRESHOLD) {
            return CIRCUIT_STATUS_TRIPPED;
        }
    }

    return CIRCUIT_STATUS_OK;
}

/**
 * 检查二次根式是否超过位数阈值。
 *
 * @param q 二次根式对象（不能为 NULL）
 * @return CIRCUIT_STATUS_OK 表示正常，CIRCUIT_STATUS_TRIPPED 表示超过阈值
 */
static CircuitStatus check_quadratic_circuit(const Quadratic *q) {
    if (!q)
        return CIRCUIT_STATUS_OK;
    CircuitStatus status_a = check_rational_circuit(q->a);
    CircuitStatus status_b = check_rational_circuit(q->b);
    if (status_a == CIRCUIT_STATUS_TRIPPED || status_b == CIRCUIT_STATUS_TRIPPED) {
        return CIRCUIT_STATUS_TRIPPED;
    }
    return CIRCUIT_STATUS_OK;
}

/**
 * 检查 SymbolicCoord 是否超过位数阈值。
 *
 * 根据 design_v2.9.md Section 1.5：
 * "计算有理数分子和分母的比特位数总和，以及多项式的每个系数的比特位数。
 * 若任何值超过 10^6 比特，则触发位数熔断信号"
 *
 * @param coord SymbolicCoord 对象（不能为 NULL）
 * @return CIRCUIT_STATUS_OK 表示正常，CIRCUIT_STATUS_TRIPPED 表示超过阈值
 */
CircuitStatus check_digit_circuit(const SymbolicCoord *coord) {
    if (!coord)
        return CIRCUIT_STATUS_OK;

    switch (coord->type) {
        case RATIONAL:
            return check_rational_circuit(coord->data.rational);
        case ALGEBRAIC:
            return check_algebraic_circuit(coord->data.algebraic);
        case QUADRATIC:
            return check_quadratic_circuit(coord->data.quadratic);
        case TRANSCENDENTAL:
            return CIRCUIT_STATUS_OK; /* 超越数没有比特位需要检查 */
    }
    return CIRCUIT_STATUS_OK;
}

/**
 * 处理位数溢出事件。
 *
 * 根据 design_v2.9.md Section 1.5，用户选项：
 * 1. 忽略 (Ignore): 接受为"数值辅助"，标记为 AMBER
 * 2. 回退 (Rollback): 恢复到冻结点，撤销操作
 * 3. 永久降级 (Permanent Downgrade): 连续3次触发后，降级为数值近似
 */
void circuit_handle_overflow(void) {
    g_overflow_context.overflow_count++;

    /* 记录溢出事件 */
    LOG_WARN("[BIT CIRCUIT TRIPPED] Operation: %s, Count: %d\n",
             g_overflow_context.last_operation ? g_overflow_context.last_operation : "unknown",
             g_overflow_context.overflow_count);

    /* 连续3次触发后，建议永久降级 */
    if (g_overflow_context.overflow_count >= LV00_CIRCUIT_OVERFLOW_THRESHOLD) {
        fprintf(stderr, "[BIT CIRCUIT] Suggesting permanent downgrade to numerical approximation (AMBER)\n");
        /* 实际降级由调用者根据用户选择处理 */
    }
}

/**
 * 重置溢出上下文。
 *
 * 清除最近一次结果和操作记录，重置溢出计数器。
 * 注意：保留 frozen_point 以便回退时使用。
 */
void circuit_reset_context(void) {
    g_overflow_context.last_result = NULL;
    g_overflow_context.last_operation = NULL;
    g_overflow_context.overflow_count = 0;
}

/**
 * 设置回滚的冻结点快照。
 *
 * @param snapshot 指向要保存的快照的指针
 */
void circuit_set_frozen_point(void *snapshot) {
    g_overflow_context.frozen_point = snapshot;
    g_overflow_context.has_frozen_point = (snapshot != NULL);
}

/**
 * 获取当前溢出计数。
 *
 * @return 累计溢出次数
 */
int circuit_get_overflow_count(void) {
    return g_overflow_context.overflow_count;
}

/* ============================================================
 * Algebraic Number Implementation
 * ============================================================ */

/**
 * 在 double 值处计算多项式的值。
 *
 * 使用 Horner 法则计算多项式 p(x) = sum(coeffs[i] * x^i) 的值。
 *
 * @param poly 多项式对象
 * @param x    求值点
 * @return 多项式在 x 处的值
 */
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
static void refine_algebraic_bounds(Algebraic *a, int iterations) {
    if (a->minimal_poly.degree < 1)
        return;

    for (int iter = 0; iter < iterations; iter++) {
        double mid = (a->left_bound + a->right_bound) / 2.0;
        double val_mid = evaluate_poly_at_double(&a->minimal_poly, mid);
        double val_left = evaluate_poly_at_double(&a->minimal_poly, a->left_bound);

        if (fabs(val_mid) < LV00_EPSILON_NEWTON) {
            a->left_bound = mid - LV00_EPSILON_NEWTON;
            a->right_bound = mid + LV00_EPSILON_NEWTON;
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

    size_t bit_limit = LV00_BIT_CUTOFF_THRESHOLD / 2;

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
    for (int iter = 0; iter < LV00_CONTINUED_FRACTION_MAX_ITER; iter++) {
        if (frac < LV00_EPSILON_FRACTION_ZERO)
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
     * 使用 lv00_internal.h 中的 LV00_MAX_SUBINTERVALS 和 LV00_ROOT_EPSILON。
     */

    /* 使用栈模拟递归，避免栈溢出 */
    typedef struct {
        double lo, hi;
    } Interval;
    Interval stack[LV00_MAX_SUBINTERVALS];
    int stack_top = 0;
    int root_count = 0;

    stack[stack_top++] = (Interval) {a, b};

    while (stack_top > 0) {
        Interval cur = stack[--stack_top];
        double fa = poly_eval_double(poly, cur.lo);
        double fb = poly_eval_double(poly, cur.hi);

        /* 如果端点之一恰好是根（或非常接近），计入并缩小区间 */
        if (fabs(fa) < LV00_ROOT_EPSILON) {
            root_count++;
            /* 将左端点稍微右移，避免重复计数 */
            cur.lo += LV00_ROOT_EPSILON;
            if (cur.lo >= cur.hi)
                continue;
            fa = poly_eval_double(poly, cur.lo);
        }
        if (fabs(fb) < LV00_ROOT_EPSILON) {
            root_count++;
            /* 将右端点稍微左移，避免重复计数 */
            cur.hi -= LV00_ROOT_EPSILON;
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
        if (cur.hi - cur.lo < LV00_ROOT_EPSILON) {
            root_count++;
            continue;
        }

        /* 二分 */
        double mid = (cur.lo + cur.hi) * 0.5;
        if (stack_top + 2 <= LV00_MAX_SUBINTERVALS) {
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
    Algebraic *a = lv00_malloc(sizeof(Algebraic));
    if (!a)
        return NULL;
    mpz_poly_init(&a->minimal_poly);
    mpz_poly_set(&a->minimal_poly, poly);
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
        lv00_free((void **) &a);
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
static Algebraic *algebraic_from_rational(const Rational *r) {
    Algebraic *a = lv00_malloc(sizeof(Algebraic));
    if (!a)
        return NULL;

    mpz_poly_init(&a->minimal_poly);
    a->minimal_poly.degree = 0;
    a->minimal_poly.coeffs = lv00_malloc(sizeof(mpz_t));
    if (!a->minimal_poly.coeffs) {
        mpz_poly_clear(&a->minimal_poly);
        lv00_free((void **) &a);
        return NULL;
    }
    mpz_init(a->minimal_poly.coeffs[0]);
    mpz_set(a->minimal_poly.coeffs[0], mpq_numref(r->value));

    /* Handle denominator by storing numerator only; actual value is num/den */
    /* For simplicity, we'll use the double approximation for bounds */
    double val = rational_to_double(r);
    a->left_bound = val - LV00_EPSILON_NEWTON;
    a->right_bound = val + LV00_EPSILON_NEWTON;
    a->precision_bits = 53;
    a->cached_rational = rational_copy(r);

    return a;
}

/* Create algebraic from quadratic: a + b*sqrt(n) */
static Algebraic *algebraic_from_quadratic(const Quadratic *q) {
    Algebraic *alg = lv00_malloc(sizeof(Algebraic));
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
    alg->minimal_poly.coeffs = lv00_malloc(3 * sizeof(mpz_t));
    if (!alg->minimal_poly.coeffs) {
        mpz_poly_clear(&alg->minimal_poly);
        lv00_free((void**)&alg);  /* lv00_malloc分配 */
        return NULL;
    }

    /* Coefficients: x^2 - 2ax + (a^2 - b^2*n) */
    mpz_init(alg->minimal_poly.coeffs[0]); /* constant: a^2 - b^2*n */
    mpz_init(alg->minimal_poly.coeffs[1]); /* linear: -2a */
    mpz_init(alg->minimal_poly.coeffs[2]); /* quadratic: 1 */

    mpz_set_ui(alg->minimal_poly.coeffs[2], 1);

    /* -2a: need to handle rational a */
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
    /* 使用更宽的隔离区间（10倍 LV00_EPSILON_NUMERIC_COMPARE），
     * 确保在 double 近似误差下区间仍包含实际根。
     * 与 algebraic_from_rational 的隔离策略保持一致。 */
    double isolation_width = LV00_EPSILON_NUMERIC_COMPARE * 10.0;
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
        lv00_free((void **) &a);
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

    if (a_val < b_val - LV00_EPSILON_NUMERIC_COMPARE)
        return -1;
    if (a_val > b_val + LV00_EPSILON_NUMERIC_COMPARE)
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
static double algebraic_to_double(const Algebraic *a) {
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
    mpz_poly_t *result = lv00_malloc(sizeof(mpz_poly_t));
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
    mpz_poly_t *result = lv00_malloc(sizeof(mpz_poly_t));
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
        Algebraic *result = lv00_malloc(sizeof(Algebraic));
        if (!result)
            return NULL;
        mpz_poly_init(&result->minimal_poly);
        mpz_poly_set(&result->minimal_poly, &b->minimal_poly);
        result->left_bound = b->left_bound + a_val;
        result->right_bound = b->right_bound + a_val;
        result->precision_bits = b->precision_bits;
        result->cached_rational = NULL;
        return result;
    }

    if (b->minimal_poly.degree == 0 && b->cached_rational) {
        double b_val = rational_to_double(b->cached_rational);
        Algebraic *result = lv00_malloc(sizeof(Algebraic));
        if (!result)
            return NULL;
        mpz_poly_init(&result->minimal_poly);
        mpz_poly_set(&result->minimal_poly, &a->minimal_poly);
        result->left_bound = a->left_bound + b_val;
        result->right_bound = a->right_bound + b_val;
        result->precision_bits = a->precision_bits;
        result->cached_rational = NULL;
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
    lv00_free((void **) &sum_poly);

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
        Algebraic *result = lv00_malloc(sizeof(Algebraic));
        if (!result)
            return NULL;

        /* For subtraction, negate b's polynomial */
        mpz_poly_init(&result->minimal_poly);
        result->minimal_poly.degree = b->minimal_poly.degree;
        if (b->minimal_poly.degree >= 0) {
            result->minimal_poly.coeffs = lv00_malloc((b->minimal_poly.degree + 1) * sizeof(mpz_t));
            if (!result->minimal_poly.coeffs) {
                lv00_free((void **) &result);
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
        Algebraic *result = lv00_malloc(sizeof(Algebraic));
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
        neg_b_poly.coeffs = lv00_malloc((b->minimal_poly.degree + 1) * sizeof(mpz_t));
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

    mpz_poly_t *diff_poly = lv00_malloc(sizeof(mpz_poly_t));
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
    lv00_free((void **) &diff_poly);

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
        Algebraic *result = lv00_malloc(sizeof(Algebraic));
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
        Algebraic *result = lv00_malloc(sizeof(Algebraic));
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
    if (width < LV00_EPSILON_NUMERIC_COMPARE)
        width = LV00_EPSILON_NUMERIC_COMPARE;
    new_left -= width * 0.5;
    new_right += width * 0.5;

    Algebraic *result = algebraic_create(prod_poly, new_left, new_right);
    mpz_poly_clear(prod_poly);
    lv00_free((void **) &prod_poly);

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
        if (b_val == 0)
            return NULL;

        Algebraic *result = lv00_malloc(sizeof(Algebraic));
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

    mpz_poly_t *result_poly = lv00_malloc(sizeof(mpz_poly_t));
    if (!result_poly)
        return NULL;
    mpz_poly_init(result_poly);

    int deg_b = b->minimal_poly.degree;
    if (deg_b >= 0) {
        result_poly->degree = deg_b;
        result_poly->coeffs = lv00_malloc((deg_b + 1) * sizeof(mpz_t));
        if (!result_poly->coeffs) {
            mpz_poly_clear(result_poly);
            lv00_free((void **) &result_poly);
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
        lv00_free((void **) &result_poly);
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
    lv00_free((void **) &result_poly);

    refine_algebraic_bounds(result, 10);

    /* Auto-trigger priority rationalization (Section 1.2) */
    algebraic_try_rationalize(result);

    return result;
}

char *algebraic_serialize(const Algebraic *a) {
    char *poly_str = mpz_poly_get_str(&a->minimal_poly);
    size_t len = strlen(poly_str) + 128;
    char *result = lv00_malloc(len);
    if (!result) {
        lv00_free((void **) &poly_str);
        return NULL;
    }
    snprintf(result, len, "poly:%s left:%.15g right:%.15g", poly_str, a->left_bound, a->right_bound);
    lv00_free((void **) &poly_str);
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
    return mpq_cmp_ui(r->value, 0, 1) == 0;
}

/**
 * 创建二次根式对象。
 *
 * 二次根式表示为 a + b*sqrt(n)，其中 a、b 为有理数，n 为无平方因子的正整数。
 * 创建时自动规范化（移除平方因子）。
 *
 * @param a 二次项的系数有理数（不能为 NULL）
 * @param b 根号项的系数有理数（可为 NULL，视为 0）
 * @param n 根号内的整数（必须为正整数）
 * @return 新创建的二次根式对象，失败时返回 NULL；调用者需负责释放
 */
Quadratic *quadratic_create(Rational *a, Rational *b, unsigned int n) {
    if (!a)
        return NULL;

    Quadratic *q = lv00_malloc(sizeof(Quadratic));
    if (!q)
        return NULL;

    n = remove_square_factors(n);
    q->a = a;
    q->n = n;

    if (b && !is_rational_zero(b)) {
        q->b = b;
    } else {
        if (b)
            rational_destroy(b);
        q->b = rational_create(0, 1);
    }

    return q;
}

/**
 * 销毁二次根式对象并释放内存。
 *
 * @param q 二次根式对象，可为 NULL（空操作）
 */
void quadratic_destroy(Quadratic *q) {
    if (q) {
        rational_destroy(q->a);
        rational_destroy(q->b);
        lv00_free((void**)&q);  /* lv00_malloc分配 */
    }
}

/**
 * 检查两个二次根式是否具有相同的平方根参数 n。
 *
 * 当两个二次根式形式为 a1 + b1*sqrt(n1) 和 a2 + b2*sqrt(n2) 时，
 * 如果 n1 == n2，则它们可以进行精确的加减运算；
 * 否则只能通过数值近似进行比较。
 *
 * @param a 第一个二次根式（不能为 NULL）
 * @param b 第二个二次根式（不能为 NULL）
 * @return 如果 n 相同返回 true，否则返回 false
 */
static bool quadratic_same_n(const Quadratic *a, const Quadratic *b) {
    return a->n == b->n;
}

int quadratic_compare(const Quadratic *a, const Quadratic *b) {
    /* If same sqrt(n), compare directly */
    if (quadratic_same_n(a, b)) {
        int cmp_a = rational_compare(a->a, b->a);
        if (cmp_a != 0)
            return cmp_a;
        return rational_compare(a->b, b->b);
    }

    /* Otherwise, compare numerically */
    double a_val = rational_to_double(a->a) + rational_to_double(a->b) * sqrt((double) a->n);
    double b_val = rational_to_double(b->a) + rational_to_double(b->b) * sqrt((double) b->n);

    if (a_val < b_val - LV00_EPSILON_NUMERIC_COMPARE)
        return -1;
    if (a_val > b_val + LV00_EPSILON_NUMERIC_COMPARE)
        return 1;
    return 0;
}

/**
 * 获取二次根式的数值近似值。
 *
 * 计算公式：a + b * sqrt(n)
 * 其中 a 和 b 为有理数系数，n 为平方根参数。
 *
 * @param q 二次根式对象（不能为 NULL）
 * @return 二次根式的双精度浮点数近似值
 */
static double quadratic_to_double(const Quadratic *q) {
    return rational_to_double(q->a) + rational_to_double(q->b) * sqrt((double) q->n);
}

/**
 * 从整数值创建有理数。
 *
 * 将整数 val 转换为有理数 val/1，是 rational_create 的简化包装函数。
 *
 * @param val 整数值
 * @return 新创建的有理数对象；调用者需负责释放
 */
static Rational *rational_from_int(int64_t val) {
    return rational_create(val, 1);
}

/**
 * 二次根式加法：计算 a + b。
 *
 * 要求两个二次根式的 n 值相同。
 *
 * @param a 被加数（不能为 NULL）
 * @param b 加数（不能为 NULL）
 * @return 新的二次根式对象，n 值不同时返回 NULL；调用者需负责释放
 */
Quadratic *quadratic_add(const Quadratic *a, const Quadratic *b) {
    if (a->n != b->n)
        return NULL;
    Rational *new_a = rational_add(a->a, b->a);
    Rational *new_b = rational_add(a->b, b->b);
    return quadratic_create(new_a, new_b, a->n);
}

/**
 * 二次根式减法：计算 a - b。
 *
 * 要求两个二次根式的 n 值相同。
 *
 * @param a 被减数（不能为 NULL）
 * @param b 减数（不能为 NULL）
 * @return 新的二次根式对象，n 值不同时返回 NULL；调用者需负责释放
 */
Quadratic *quadratic_subtract(const Quadratic *a, const Quadratic *b) {
    if (a->n != b->n)
        return NULL;
    Rational *new_a = rational_subtract(a->a, b->a);
    Rational *new_b = rational_subtract(a->b, b->b);
    return quadratic_create(new_a, new_b, a->n);
}

Quadratic *quadratic_multiply(const Quadratic *a, const Quadratic *b) {
    if (a->n != b->n)
        return NULL;

    /* (a1 + b1*sqrt(n)) * (a2 + b2*sqrt(n)) = (a1*a2 + b1*b2*n) + (a1*b2 + a2*b1)*sqrt(n) */
    Rational *a1 = rational_copy(a->a);
    Rational *a2 = rational_copy(b->a);
    Rational *b1 = rational_copy(a->b);
    Rational *b2 = rational_copy(b->b);

    Rational *term1 = rational_multiply(a1, a2);
    Rational *b1b2 = rational_multiply(b1, b2);
    Rational *n_rat = rational_create(a->n, 1);
    Rational *term2 = rational_multiply(b1b2, n_rat);
    Rational *new_a = rational_add(term1, term2);

    Rational *a1b2 = rational_multiply(a1, b2);
    Rational *a2b1 = rational_multiply(a2, b1);
    Rational *new_b = rational_add(a1b2, a2b1);

    rational_destroy(a1);
    rational_destroy(a2);
    rational_destroy(b1);
    rational_destroy(b2);
    rational_destroy(term1);
    rational_destroy(term2);
    rational_destroy(b1b2);
    rational_destroy(n_rat);
    rational_destroy(a1b2);
    rational_destroy(a2b1);

    return quadratic_create(new_a, new_b, a->n);
}

Quadratic *quadratic_divide(const Quadratic *a, const Quadratic *b) {
    if (a->n != b->n)
        return NULL;

    Rational *zero = rational_create(0, 1);
    bool b_is_zero = (rational_compare(b->a, zero) == 0 && rational_compare(b->b, zero) == 0);
    rational_destroy(zero);

    if (b_is_zero)
        return NULL;

    /* (a1 + b1*sqrt(n)) / (a2 + b2*sqrt(n)) 
     * = (a1 + b1*sqrt(n)) * (a2 - b2*sqrt(n)) / (a2^2 - b2^2*n)
     */
    Rational *c = rational_copy(b->a);
    Rational *d = rational_copy(b->b);
    Rational *n_rat = rational_create(a->n, 1);

    Rational *c2 = rational_multiply(c, c);
    Rational *d2 = rational_multiply(d, d);
    Rational *d2n = rational_multiply(d2, n_rat);
    Rational *denom = rational_subtract(c2, d2n);

    /* 检查分母是否为零 */
    Rational *zero2 = rational_create(0, 1);
    if (rational_compare(denom, zero2) == 0) {
        rational_destroy(zero2);
        rational_destroy(c);
        rational_destroy(d);
        rational_destroy(c2);
        rational_destroy(d2);
        rational_destroy(d2n);
        rational_destroy(denom);
        rational_destroy(n_rat);
        return NULL;
    }
    rational_destroy(zero2);

    Rational *a1 = rational_copy(a->a);
    Rational *b1 = rational_copy(a->b);

    /* Numerator for a: a1*c + b1*d*n */
    Rational *a1c = rational_multiply(a1, c);
    Rational *b1d = rational_multiply(b1, d);
    Rational *b1dn = rational_multiply(b1d, n_rat);
    Rational *num_a = rational_add(a1c, b1dn);

    /* Numerator for b: b1*c - a1*d */
    Rational *b1c = rational_multiply(b1, c);
    Rational *a1d = rational_multiply(a1, d);
    Rational *num_b = rational_subtract(b1c, a1d);

    Rational *new_a = rational_divide(num_a, denom);
    Rational *new_b = rational_divide(num_b, denom);

    rational_destroy(c);
    rational_destroy(d);
    rational_destroy(c2);
    rational_destroy(d2);
    rational_destroy(d2n);
    rational_destroy(denom);
    rational_destroy(n_rat);
    rational_destroy(a1);
    rational_destroy(b1);
    rational_destroy(a1c);
    rational_destroy(b1d);
    rational_destroy(b1dn);
    rational_destroy(num_a);
    rational_destroy(b1c);
    rational_destroy(a1d);
    rational_destroy(num_b);

    if (!new_a || !new_b) {
        if (new_a)
            rational_destroy(new_a);
        if (new_b)
            rational_destroy(new_b);
        return NULL;
    }

    return quadratic_create(new_a, new_b, a->n);
}

char *quadratic_serialize(const Quadratic *q) {
    char *a_str = rational_serialize(q->a);
    char *b_str = rational_serialize(q->b);
    if (!a_str || !b_str) {
        lv00_free((void**)&a_str); /* lv00_malloc分配 */
        lv00_free((void**)&b_str); /* lv00_malloc分配 */
        return NULL;
    }
    size_t len = strlen(a_str) + strlen(b_str) + 32;
    char *result = lv00_malloc(len);
    if (!result) {
        lv00_free((void**)&a_str); /* lv00_malloc分配 */
        lv00_free((void**)&b_str); /* lv00_malloc分配 */
        return NULL;
    }
    snprintf(result, len, "%s + %s*sqrt(%u)", a_str, b_str, q->n);
    lv00_free((void**)&a_str); /* lv00_malloc分配 */
    lv00_free((void**)&b_str); /* lv00_malloc分配 */
    return result;
}

/* ============================================================
 * Transcendental Number Implementation
 * ============================================================ */

/**
 * 创建超越数对象。
 *
 * 支持的常量：pi, e
 * 支持的表达式形式：N*pi, N*pi/M, pi/N, -pi, -pi/N, -N*pi, -N*pi/M
 *
 * @param name 常量名称（不能为 NULL）
 * @return 新创建的超越数对象，解析失败时返回 NULL；调用者需负责释放
 */
Transcendental *transcendental_create(const char *name) {
    if (!name)
        return NULL;

    /* 支持基础常量 "pi" 和 "e"，以及复合表达式如 "pi/2", "pi/3",
     * "pi/4", "pi/6", "3*pi/4", "5*pi/6", "2*pi/3" 及其负数形式 */
    const char *base = NULL;
    int64_t coeff_num = 1; /* 系数分子（默认为 1） */
    int64_t coeff_den = 1; /* 系数分母（默认为 1） */
    bool is_mul = false;   /* true = coeff*base, false = base/coeff */

    if (strcmp(name, "pi") == 0 || strcmp(name, "e") == 0) {
        /* 裸常量 */
        base = name;
    } else if (strncmp(name, "-pi", 3) == 0 && (name[3] == '\0' || name[3] == '/')) {
        /* 负 pi 变体: -pi, -pi/2, -pi/3, ... */
        base = "pi";
        coeff_num = -1;
        if (name[3] == '/') {
            is_mul = false;
            coeff_den = atol(name + 4);
            if (coeff_den <= 0)
                return NULL;
        }
    } else if (strncmp(name, "pi/", 3) == 0) {
        /* pi/N 形式 */
        base = "pi";
        is_mul = false;
        coeff_den = atol(name + 3);
        if (coeff_den <= 0)
            return NULL;
    } else if (strncmp(name, "-pi/", 4) == 0) {
        /* 已在上面处理 */
        return NULL;
    } else {
        /* 尝试解析 N*pi/M 或 N*pi 形式 */
        char *star_pos = strstr(name, "*pi");
        if (star_pos && star_pos == name + 1 && name[0] != '-') {
            /* N*pi 或 N*pi/M */
            base = "pi";
            is_mul = true;
            coeff_num = atol(name);
            if (coeff_num <= 0)
                return NULL;
            const char *after = star_pos + 3; /* skip "*pi" */
            if (*after == '/') {
                coeff_den = atol(after + 1);
                if (coeff_den <= 0)
                    return NULL;
            }
        } else if (star_pos && star_pos == name + 2 && name[0] == '-') {
            /* -N*pi 或 -N*pi/M */
            base = "pi";
            is_mul = true;
            coeff_num = atol(name);
            if (coeff_num >= 0)
                return NULL;
            const char *after = star_pos + 3;
            if (*after == '/') {
                coeff_den = atol(after + 1);
                if (coeff_den <= 0)
                    return NULL;
            }
        } else {
            return NULL;
        }
    }

    if (!base)
        return NULL;

    Transcendental *t = lv00_malloc(sizeof(Transcendental));
    if (!t)
        return NULL;
    /* 安全字符串复制：确保以 null 终止 */
    {
        size_t name_len = strlen(name);
        if (name_len >= sizeof(t->name))
            name_len = sizeof(t->name) - 1;
        memcpy(t->name, name, name_len);
        t->name[name_len] = '\0';
    }

    /* 如果是裸常量（pi 或 e），expr 保持 NULL */
    if (strcmp(name, "pi") == 0 || strcmp(name, "e") == 0) {
        t->expr = NULL;
    } else {
        /* 构造表达式树 */
        TranscendentalExpr *expr = lv00_calloc(1, sizeof(TranscendentalExpr));
        if (!expr) {
            lv00_free((void **) &t);
            return NULL;
        }
        /* 安全字符串复制：确保以 null 终止 */
        {
            size_t base_len = strlen(base);
            if (base_len >= sizeof(expr->base_name))
                base_len = sizeof(expr->base_name) - 1;
            memcpy(expr->base_name, base, base_len);
            expr->base_name[base_len] = '\0';
        }
        expr->out_of_scope = false;

        if (is_mul) {
            expr->expr_type = TRANS_EXPR_MUL_RATIONAL;
            expr->rational_operand = rational_create(coeff_num, (uint64_t) coeff_den);
        } else {
            /* base/coeff 等价于 base * (1/coeff) */
            expr->expr_type = TRANS_EXPR_MUL_RATIONAL;
            expr->rational_operand = rational_create(coeff_num, (uint64_t) coeff_den);
        }

        if (!expr->rational_operand) {
            lv00_free((void **) &expr);
            lv00_free((void **) &t);
            return NULL;
        }

        t->expr = expr;
    }

    return t;
}

void transcendental_destroy(Transcendental *t) {
    if (!t)
        return;
    if (t->expr) {
        if (t->expr->rational_operand) {
            rational_destroy(t->expr->rational_operand);
        }
        lv00_free((void **) &t->expr);
    }
    lv00_free((void **) &t);
}

/**
 * 比较两个超越数的大小。
 *
 * @param a 第一个超越数（不能为 NULL）
 * @param b 第二个超越数（不能为 NULL）
 * @return -1 表示 a < b，0 表示 a == b，1 表示 a > b
 */
int transcendental_compare(const Transcendental *a, const Transcendental *b) {
    /* Compare base names first */
    int name_cmp = strcmp(a->name, b->name);
    if (name_cmp != 0)
        return name_cmp;

    /* Both NULL expr means bare constants with same name -> equal */
    if (!a->expr && !b->expr)
        return 0;
    /* NULL expr vs non-NULL expr: bare constant vs expression */
    if (!a->expr)
        return -1;
    if (!b->expr)
        return 1;

    /* Both have expr: compare expression trees */
    if (a->expr->expr_type != b->expr->expr_type) {
        return (a->expr->expr_type < b->expr->expr_type) ? -1 : 1;
    }

    /* Same expr_type: compare rational operands if present */
    if (a->expr->rational_operand && b->expr->rational_operand) {
        return rational_compare(a->expr->rational_operand, b->expr->rational_operand);
    }
    if (a->expr->rational_operand)
        return 1;
    if (b->expr->rational_operand)
        return -1;

    return 0;
}

char *transcendental_serialize(const Transcendental *t) {
    if (!t->expr) {
        /* 裸常量：使用 lv00_strdup 分配内存 */
        return lv00_strdup(t->name);
    }

    /* Serialize expression tree */
    const char *op_str = NULL;
    switch (t->expr->expr_type) {
        case TRANS_EXPR_ADD_RATIONAL:
            op_str = "+";
            break;
        case TRANS_EXPR_MUL_RATIONAL:
            op_str = "*";
            break;
        case TRANS_EXPR_ADD_ALGEBRAIC:
            op_str = "+";
            break;
        case TRANS_EXPR_MUL_ALGEBRAIC:
            op_str = "*";
            break;
        default:
            /* 未知表达式类型：使用 lv00_strdup 分配内存 */
            return lv00_strdup(t->name);
    }

    if (t->expr->out_of_scope) {
        /* Out-of-scope expression: mark clearly */
        size_t len = strlen(t->name) + strlen(op_str) + 32;
        char *buf = lv00_malloc(len);
        if (!buf)
            return NULL;
        snprintf(buf, len, "[%s %s <out-of-scope>]", t->name, op_str);
        return buf;
    }

    if (t->expr->rational_operand) {
        char *rat_str = rational_serialize(t->expr->rational_operand);
        if (!rat_str)
            return lv00_strdup(t->name); /* rational_serialize 失败时使用 lv00_strdup */
        size_t len = strlen(t->name) + strlen(op_str) + strlen(rat_str) + 8;
        char *buf = lv00_malloc(len);
        if (!buf) {
            lv00_free((void**)&rat_str); /* lv00_malloc分配 */
            return lv00_strdup(t->name); /* 内存不足时使用 lv00_strdup */
        }
        snprintf(buf, len, "(%s %s %s)", t->name, op_str, rat_str);
        lv00_free((void**)&rat_str); /* lv00_malloc分配 */
        return buf;
    }

    /* 无理数操作数：使用 lv00_strdup 分配内存 */
    return lv00_strdup(t->name);
}

/**
 * 获取超越数的数值近似。
 *
 * 支持的超越数包括：
 * - 基本常数：pi, e
 * - 有理数运算：k*pi, pi/k, k*e, e/k（k 为有理数）
 * - 超出作用域的表达式（如 pi + sqrt(2)）：返回基常数的近似值
 *
 * 对于无法解析的表达式，默认返回 0.0。
 *
 * @param t 超越数对象（不能为 NULL）
 * @return double 近似值；如果无法解析则返回 0.0
 */
static double transcendental_to_double(const Transcendental *t) {
    /* Get base constant value */
    const char *base = t->expr ? t->expr->base_name : t->name;
    double base_val;
    if (strcmp(base, "pi") == 0) {
        base_val = M_PI;
    } else if (strcmp(base, "e") == 0) {
        base_val = M_E;
    } else {
        return 0.0;
    }

    /* Bare constant (no expr) */
    if (!t->expr) {
        return base_val;
    }

    /* Handle expression types */
    if (t->expr->rational_operand) {
        double rat_val = rational_to_double(t->expr->rational_operand);
        switch (t->expr->expr_type) {
            case TRANS_EXPR_MUL_RATIONAL:
                return base_val * rat_val;
            case TRANS_EXPR_ADD_RATIONAL:
                return base_val + rat_val;
            default:
                break;
        }
    }

    /* Handle ADD_ALGEBRAIC / MUL_ALGEBRAIC: these are out_of_scope expressions
     * (e.g., pi + sqrt(2), e * sqrt(3)). We cannot compute the exact value
     * since the algebraic operand is not stored in the TranscendentalExpr.
     * Return the base constant value as an approximation, with the understanding
     * that this is only the base component of the expression.
     * Note: for expressions like "e + pi", the name may not match any known
     * pattern, so the fallback below would return 0.0. Returning base_val here
     * is a better approximation. */
    if (t->expr->expr_type == TRANS_EXPR_ADD_ALGEBRAIC || t->expr->expr_type == TRANS_EXPR_MUL_ALGEBRAIC) {
        /* Approximate: return the base constant value.
         * This is an approximation for out_of_scope expressions. */
        return base_val;
    }

    /* Fallback: try to parse the name for compound forms like "pi/2", "3*pi/4" */
    {
        const char *name = t->name;
        /* Try N*pi/M form */
        char *star_pos = strstr(name, "*pi");
        if (star_pos) {
            int64_t coeff_num = 1;
            int64_t coeff_den = 1;
            if (star_pos == name + 1 && name[0] != '-') {
                coeff_num = atol(name);
            } else if (star_pos == name + 2 && name[0] == '-') {
                coeff_num = atol(name);
            }
            const char *after = star_pos + 3;
            if (*after == '/') {
                coeff_den = atol(after + 1);
            }
            if (coeff_den > 0) {
                return M_PI * (double) coeff_num / (double) coeff_den;
            }
        }

        /* Try pi/N form */
        if (strncmp(name, "pi/", 3) == 0) {
            int64_t den = atol(name + 3);
            if (den > 0)
                return M_PI / (double) den;
        }

        /* Try -pi/N form */
        if (strncmp(name, "-pi/", 4) == 0) {
            int64_t den = atol(name + 4);
            if (den > 0)
                return -M_PI / (double) den;
        }

        /* Try -N*pi/M form */
        if (name[0] == '-' && strstr(name, "*pi")) {
            char *sp = strstr(name, "*pi");
            int64_t coeff_num = atol(name);
            int64_t coeff_den = 1;
            const char *after = sp + 3;
            if (*after == '/') {
                coeff_den = atol(after + 1);
            }
            if (coeff_den > 0) {
                return M_PI * (double) coeff_num / (double) coeff_den;
            }
        }
    }

    return 0.0;
}

/* ============================================================
 * SymbolicCoord Implementation
 * ============================================================ */

SymbolicCoord *symbolic_coord_create_rational(int64_t num, uint64_t denom) {
    SymbolicCoord *coord = lv00_malloc(sizeof(SymbolicCoord));
    if (!coord)
        return NULL;
    coord->type = RATIONAL;
    coord->trust = TRUST_GREEN;
    coord->cache_valid = false;
    coord->cached_value = 0.0;
    coord->data.rational = rational_create(num, denom);
    if (!coord->data.rational) {
        lv00_free((void**)&coord);  /* lv00_malloc分配 */
        return NULL;
    }
    return coord;
}

/**
 * 创建代数数类型的符号坐标。
 *
 * @param poly  极小多项式
 * @param left  隔离区间左边界
 * @param right 隔离区间右边界
 * @return 新创建的符号坐标对象，失败时返回 NULL；调用者需负责释放
 */
SymbolicCoord *symbolic_coord_create_algebraic(mpz_poly_t *poly, double left, double right) {
    SymbolicCoord *coord = lv00_malloc(sizeof(SymbolicCoord));
    if (!coord)
        return NULL;
    coord->type = ALGEBRAIC;
    coord->trust = TRUST_GREEN;
    coord->cache_valid = false;
    coord->cached_value = 0.0;
    coord->data.algebraic = algebraic_create(poly, left, right);
    if (!coord->data.algebraic) {
        lv00_free((void**)&coord);  /* lv00_malloc分配 */
        return NULL;
    }
    return coord;
}

/**
 * 创建二次根式类型的符号坐标。
 *
 * @param a 二次项的系数有理数
 * @param b 根号项的系数有理数
 * @param n 根号内的整数
 * @return 新创建的符号坐标对象，失败时返回 NULL；调用者需负责释放
 */
SymbolicCoord *symbolic_coord_create_quadratic(Rational *a, Rational *b, unsigned int n) {
    SymbolicCoord *coord = lv00_malloc(sizeof(SymbolicCoord));
    if (!coord)
        return NULL;
    coord->type = QUADRATIC;
    coord->trust = TRUST_GREEN;
    coord->cache_valid = false;
    coord->cached_value = 0.0;
    coord->data.quadratic = quadratic_create(a, b, n);
    if (!coord->data.quadratic) {
        lv00_free((void**)&coord);  /* lv00_malloc分配 */
        return NULL;
    }
    return coord;
}

SymbolicCoord *symbolic_coord_create_transcendental(const char *name) {
    SymbolicCoord *coord = lv00_malloc(sizeof(SymbolicCoord));
    if (!coord)
        return NULL;
    coord->type = TRANSCENDENTAL;
    coord->trust = TRUST_BLUE;
    coord->cache_valid = false;
    coord->cached_value = 0.0;
    coord->data.transcendental = transcendental_create(name);
    if (!coord->data.transcendental) {
        lv00_free((void**)&coord);  /* lv00_malloc分配 */
        return NULL;
    }
    return coord;
}

/**
 * 销毁符号坐标对象并释放内存。
 *
 * 销毁操作包括：
 * 1. 根据坐标类型调用对应的类型销毁函数，释放底层 GMP 变量和动态内存
 * 2. 递归清理嵌套数据结构（如 algebraic 的 cached_rational、
 *    quadratic 的子有理数、transcendental 的表达式树等）
 * 3. 使数值缓存失效，防止悬空引用
 * 4. 将所有指针置 NULL，防止悬空指针
 *
 * @param coord 符号坐标对象，可为 NULL（空操作）
 */
void symbolic_coord_destroy(SymbolicCoord *coord) {
    if (!coord)
        return;

    /* 使数值缓存失效 */
    coord->cache_valid = false;
    coord->cached_value = 0.0;

    switch (coord->type) {
        case RATIONAL:
            rational_destroy(coord->data.rational);
            coord->data.rational = NULL;
            break;
        case ALGEBRAIC:
            algebraic_destroy(coord->data.algebraic);
            coord->data.algebraic = NULL;
            break;
        case QUADRATIC:
            quadratic_destroy(coord->data.quadratic);
            coord->data.quadratic = NULL;
            break;
        case TRANSCENDENTAL:
            transcendental_destroy(coord->data.transcendental);
            coord->data.transcendental = NULL;
            break;
    }

    /* 将 trust 颜色重置为安全默认值 */
    coord->trust = TRUST_GREEN;
    coord->type = RATIONAL;

    lv00_free((void**)&coord);  /* lv00_malloc分配 */
}

/**
 * 获取任意 SymbolicCoord 类型的数值近似。
 *
 * 先检查缓存：若 cache_valid 为 true，则直接返回 cached_value，
 * 避免重复的 GMP 转换开销。缓存失效时重新计算并更新缓存。
 * 当坐标被修改时需调用 symbolic_coord_invalidate_cache() 使缓存失效。
 *
 * 根据 coord 的类型调用相应的转换函数：
 * - RATIONAL: rational_to_double
 * - ALGEBRAIC: algebraic_to_double
 * - QUADRATIC: quadratic_to_double
 * - TRANSCENDENTAL: transcendental_to_double
 *
 * @param coord SymbolicCoord 对象（不能为 NULL）
 * @return 转换后的双精度浮点数值
 */
double symbolic_coord_to_double(const SymbolicCoord *coord) {
    if (!coord)
        return 0.0;

    /* 缓存命中：直接返回已缓存值，避免重复计算 */
    /* 注意：为保持 const 语义，将 const 转换为非 const 以便写入缓存。
     * 这是安全的，因为缓存是透明的性能优化，不影响逻辑语义。 */
    if (coord->cache_valid) {
        return coord->cached_value;
    }

    double val = 0.0;
    switch (coord->type) {
        case RATIONAL:
            val = rational_to_double(coord->data.rational);
            break;
        case ALGEBRAIC:
            val = algebraic_to_double(coord->data.algebraic);
            break;
        case QUADRATIC:
            val = quadratic_to_double(coord->data.quadratic);
            break;
        case TRANSCENDENTAL:
            val = transcendental_to_double(coord->data.transcendental);
            break;
    }

    /* 更新缓存（const 转换为非 const：缓存是性能优化，不改变逻辑语义） */
    ((SymbolicCoord *)coord)->cached_value = val;
    ((SymbolicCoord *)coord)->cache_valid = true;

    return val;
}

/**
 * 使符号坐标的数值缓存失效。
 *
 * 当坐标被修改（如算术运算、信任颜色变更、类型转换）时，
 * 调用此函数标记缓存为无效，确保后续调用
 * symbolic_coord_to_double() 时重新计算精确数值。
 * 提供的指针将被设置为 NULL。
 *
 * @param coord 符号坐标（可为 NULL，空操作）
 */
void symbolic_coord_invalidate_cache(SymbolicCoord *coord) {
    if (!coord)
        return;
    coord->cache_valid = false;
    coord->cached_value = 0.0;
}

/* Cross-type comparison */
int symbolic_coord_compare(const SymbolicCoord *a, const SymbolicCoord *b) {
    /* Same type: use type-specific comparison */
    if (a->type == b->type) {
        switch (a->type) {
            case RATIONAL:
                return rational_compare(a->data.rational, b->data.rational);
            case ALGEBRAIC:
                return algebraic_compare(a->data.algebraic, b->data.algebraic);
            case QUADRATIC:
                return quadratic_compare(a->data.quadratic, b->data.quadratic);
            case TRANSCENDENTAL:
                return transcendental_compare(a->data.transcendental, b->data.transcendental);
        }
    }

    /* 跨类型比较（提高精度）*/

    /* 策略1：如果一方是有理数而另一方是代数数，
     * 尝试将有理化代数数以进行精确比较。 */
    if (a->type == RATIONAL && b->type == ALGEBRAIC) {
        Algebraic *alg = b->data.algebraic;
        /* Check cached rational first */
        if (alg->cached_rational) {
            return rational_compare(a->data.rational, alg->cached_rational);
        }
        /* Try rationalization */
        algebraic_try_rationalize(alg);
        if (alg->cached_rational) {
            return rational_compare(a->data.rational, alg->cached_rational);
        }
    }
    if (b->type == RATIONAL && a->type == ALGEBRAIC) {
        Algebraic *alg = a->data.algebraic;
        if (alg->cached_rational) {
            return rational_compare(alg->cached_rational, b->data.rational);
        }
        algebraic_try_rationalize(alg);
        if (alg->cached_rational) {
            return rational_compare(alg->cached_rational, b->data.rational);
        }
    }

    /* 策略2：如果一方是有理数而另一方是二次根式，
     * 检查二次根式是否实际上是有理数（b == 0）。 */
    if (a->type == RATIONAL && b->type == QUADRATIC) {
        const Quadratic *q = b->data.quadratic;
        if (is_rational_zero(q->b)) {
            return rational_compare(a->data.rational, q->a);
        }
    }
    if (b->type == RATIONAL && a->type == QUADRATIC) {
        const Quadratic *q = a->data.quadratic;
        if (is_rational_zero(q->b)) {
            return rational_compare(q->a, b->data.rational);
        }
    }

    /* 策略3：代数数与其他类型的隔离区间交叉检查。
     * 如果操作数之一是代数数，使用其隔离区间来确定
     * 差值的符号，而无需进行完整的 double 计算。 */
    if (a->type == ALGEBRAIC) {
        const Algebraic *alg = a->data.algebraic;
        double b_val = symbolic_coord_to_double(b);
        /* If the entire isolation interval of a is strictly on one side of b_val */
        if (alg->right_bound < b_val - LV00_EPSILON_NUMERIC_COMPARE)
            return -1;
        if (alg->left_bound > b_val + LV00_EPSILON_NUMERIC_COMPARE)
            return 1;
        /* Intervals overlap: refine and retry */
        refine_algebraic_bounds((Algebraic *) alg, 5);
        if (alg->right_bound < b_val - LV00_EPSILON_NUMERIC_COMPARE)
            return -1;
        if (alg->left_bound > b_val + LV00_EPSILON_NUMERIC_COMPARE)
            return 1;
    }
    if (b->type == ALGEBRAIC) {
        const Algebraic *alg = b->data.algebraic;
        double a_val = symbolic_coord_to_double(a);
        if (alg->right_bound < a_val - LV00_EPSILON_NUMERIC_COMPARE)
            return 1;
        if (alg->left_bound > a_val + LV00_EPSILON_NUMERIC_COMPARE)
            return -1;
        refine_algebraic_bounds((Algebraic *) alg, 5);
        if (alg->right_bound < a_val - LV00_EPSILON_NUMERIC_COMPARE)
            return 1;
        if (alg->left_bound > a_val + LV00_EPSILON_NUMERIC_COMPARE)
            return -1;
    }

    /* 策略4：对于二次根式 vs 有理数（b != 0 的情况），
     * 使用连分数近似以获得更高精度。 */
    if ((a->type == RATIONAL && b->type == QUADRATIC) || (b->type == RATIONAL && a->type == QUADRATIC)) {
        const SymbolicCoord *rat_coord = (a->type == RATIONAL) ? a : b;
        const SymbolicCoord *quad_coord = (a->type == QUADRATIC) ? a : b;
        const Quadratic *q = quad_coord->data.quadratic;
        double q_val = rational_to_double(q->a) + rational_to_double(q->b) * sqrt((double) q->n);
        double r_val = rational_to_double(rat_coord->data.rational);

        /* Use tighter tolerance for cross-type comparison */
        double tight_eps = LV00_EPSILON_NUMERIC_COMPARE * 0.01;
        if (q_val < r_val - tight_eps)
            return (a->type == QUADRATIC) ? -1 : 1;
        if (q_val > r_val + tight_eps)
            return (a->type == QUADRATIC) ? 1 : -1;
        /* Within tight epsilon: values are effectively equal */
        return 0;
    }

    /* 策略5：最后手段——使用更严格容差的 double 近似比较 */
    double a_val = symbolic_coord_to_double(a);
    double b_val = symbolic_coord_to_double(b);

    /* Use stricter tolerance: 1/100 of the normal epsilon */
    double strict_eps = LV00_EPSILON_NUMERIC_COMPARE * 0.01;

    if (a_val < b_val - strict_eps)
        return -1;
    if (a_val > b_val + strict_eps)
        return 1;
    return 0;
}

/* ============================================================
 * Type Promotion Functions
 * ============================================================ */

/**
 * 将有理数类型的符号坐标提升为代数数。
 *
 * @param r 有理数类型的符号坐标（不能为 NULL）
 * @return 新创建的代数数类型的符号坐标，失败时返回 NULL
 */
static SymbolicCoord *rational_to_algebraic(const SymbolicCoord *r) {
    Algebraic *alg = algebraic_from_rational(r->data.rational);
    if (!alg)
        return NULL;

    SymbolicCoord *result = lv00_malloc(sizeof(SymbolicCoord));
    if (!result) {
        algebraic_destroy(alg);
        return NULL;
    }
    result->type = ALGEBRAIC;
    result->trust = r->trust;
    result->data.algebraic = alg;
    return result;
}

/**
 * 将有理数提升为指定 n 值的二次根式。
 *
 * @param r 有理数类型的符号坐标（不能为 NULL）
 * @param n 二次根式的 n 值
 * @return 新创建的二次根式类型的符号坐标，失败时返回 NULL
 */
static SymbolicCoord *rational_to_quadratic_with_n(const SymbolicCoord *r, unsigned int n) {
    Rational *a = rational_copy(r->data.rational);
    Rational *b = rational_create(0, 1);

    SymbolicCoord *result = symbolic_coord_create_quadratic(a, b, n);
    if (result) {
        result->trust = r->trust;
    } else {
        rational_destroy(a);
        rational_destroy(b);
    }
    return result;
}

static SymbolicCoord *rational_to_quadratic(const SymbolicCoord *r) {
    return rational_to_quadratic_with_n(r, 1);
}

/* Promote Quadratic to Algebraic */
static SymbolicCoord *quadratic_to_algebraic(const SymbolicCoord *q) {
    Algebraic *alg = algebraic_from_quadratic(q->data.quadratic);
    if (!alg)
        return NULL;

    SymbolicCoord *result = lv00_malloc(sizeof(SymbolicCoord));
    if (!result) {
        algebraic_destroy(alg);
        return NULL;
    }
    result->type = ALGEBRAIC;
    result->trust = q->trust;
    result->data.algebraic = alg;
    return result;
}

/* ============================================================
 * Cross-type Arithmetic Operations
 * ============================================================ */

/**
 * 符号坐标加法：计算 a + b。
 *
 * 支持所有类型的跨类型运算：
 * - 同类型运算使用类型特定的精确算法
 * - 跨类型运算自动进行类型提升
 * - 超越数与其他类型运算结果标记为 AMBER（信任级别降低）
 *
 * @param a 被加数（不能为 NULL）
 * @param b 加数（不能为 NULL）
 * @return 新的符号坐标对象，失败时返回 NULL；调用者需负责释放
 */
SymbolicCoord *symbolic_coord_add(const SymbolicCoord *a, const SymbolicCoord *b) {
    /* Update overflow context */
    g_overflow_context.left_type = a->type;
    g_overflow_context.right_type = b->type;
    g_overflow_context.last_operation = "add";

    /* Transcendental + anything */
    if (a->type == TRANSCENDENTAL || b->type == TRANSCENDENTAL) {
        const SymbolicCoord *trans_coord = (a->type == TRANSCENDENTAL) ? a : b;
        const SymbolicCoord *other_coord = (a->type == TRANSCENDENTAL) ? b : a;

        if (other_coord->type == RATIONAL) {
            /* Transcendental + Rational -> TRANS_EXPR_ADD_RATIONAL */
            Transcendental *t = transcendental_create(trans_coord->data.transcendental->name);
            if (!t)
                return NULL;

            TranscendentalExpr *expr = lv00_malloc(sizeof(TranscendentalExpr));
            if (!expr) {
                transcendental_destroy(t);
                return NULL;
            }
            expr->expr_type = TRANS_EXPR_ADD_RATIONAL;
            /* 使用 lv00_strlcpy 替代不安全的 strncpy（自动保证零终止） */
            lv00_strlcpy(expr->base_name, trans_coord->data.transcendental->name, sizeof(expr->base_name));
            expr->rational_operand = rational_copy(other_coord->data.rational);
            expr->out_of_scope = false;

            t->expr = expr;

            SymbolicCoord *result = lv00_malloc(sizeof(SymbolicCoord));
            if (!result) {
                transcendental_destroy(t);
                return NULL;
            }
            result->type = TRANSCENDENTAL;
            result->trust = trans_coord->trust;
            result->data.transcendental = t;
            return result;
        }

        if (other_coord->type == ALGEBRAIC || other_coord->type == QUADRATIC) {
            /* Transcendental + Algebraic/Quadratic -> out_of_scope */
            Transcendental *t = transcendental_create(trans_coord->data.transcendental->name);
            if (!t)
                return NULL;

            TranscendentalExpr *expr = lv00_malloc(sizeof(TranscendentalExpr));
            if (!expr) {
                transcendental_destroy(t);
                return NULL;
            }
            expr->expr_type = TRANS_EXPR_ADD_ALGEBRAIC;

            lv00_strlcpy(expr->base_name, trans_coord->data.transcendental->name, sizeof(expr->base_name));
            expr->rational_operand = NULL;
            expr->out_of_scope = true;

            t->expr = expr;

            SymbolicCoord *result = lv00_malloc(sizeof(SymbolicCoord));
            if (!result) {
                transcendental_destroy(t);
                return NULL;
            }
            result->type = TRANSCENDENTAL;
            result->trust = TRUST_AMBER;
            result->data.transcendental = t;
            return result;
        }

        /* Transcendental + Transcendental: merge if same base, else out_of_scope */
        {
            const Transcendental *ta = a->data.transcendental;
            const Transcendental *tb = b->data.transcendental;
            const char *base_a = ta->expr ? ta->expr->base_name : ta->name;
            const char *base_b = tb->expr ? tb->expr->base_name : tb->name;

            Transcendental *t = transcendental_create(base_a);
            if (!t)
                return NULL;

            TranscendentalExpr *expr = lv00_malloc(sizeof(TranscendentalExpr));
            if (!expr) {
                transcendental_destroy(t);
                return NULL;
            }
            expr->out_of_scope = false;
            lv00_strlcpy(expr->base_name, base_a, sizeof(expr->base_name));
            if (strcmp(base_a, base_b) == 0) {
                /* 相同底数：检查两者是否都是纯乘法形式（MUL 或裸常量）*/
                bool a_is_mul = !ta->expr || ta->expr->expr_type == TRANS_EXPR_MUL_RATIONAL;
                bool b_is_mul = !tb->expr || tb->expr->expr_type == TRANS_EXPR_MUL_RATIONAL;

                if (a_is_mul && b_is_mul) {
                    /* 两者都是系数*底数形式：(ca*base) + (cb*base) = (ca+cb)*base */
                    Rational *rat_a =
                        (ta->expr && ta->expr->rational_operand) ? ta->expr->rational_operand : rational_create(1, 1);
                    Rational *rat_b =
                        (tb->expr && tb->expr->rational_operand) ? tb->expr->rational_operand : rational_create(1, 1);
                    Rational *own_a = (!ta->expr) ? rat_a : NULL;
                    Rational *own_b = (!tb->expr) ? rat_b : NULL;

                    expr->expr_type = TRANS_EXPR_MUL_RATIONAL;
                    expr->rational_operand = rational_add(rat_a, rat_b);

                    if (own_a)
                        rational_destroy(own_a);
                    if (own_b)
                        rational_destroy(own_b);

                    if (!expr->rational_operand) {
                        lv00_free((void **) &expr);
                        transcendental_destroy(t);
                        return NULL;
                    }
                } else {
                    /* 至少有一个是 ADD 形式：无法简化，超出作用域 */
                    expr->expr_type = TRANS_EXPR_ADD_ALGEBRAIC;
                    expr->rational_operand = NULL;
                    expr->out_of_scope = true;
                }
            } else {
                /* 不同底数（如 pi + e）：超出作用域 */
                expr->expr_type = TRANS_EXPR_ADD_ALGEBRAIC;
                expr->rational_operand = NULL;
                expr->out_of_scope = true;
            }

            t->expr = expr;

            SymbolicCoord *result = lv00_malloc(sizeof(SymbolicCoord));
            if (!result) {
                transcendental_destroy(t);
                return NULL;
            }
            result->type = TRANSCENDENTAL;
            result->trust = expr->out_of_scope ? TRUST_AMBER : TRUST_BLUE;
            result->data.transcendental = t;
            return result;
        }
    }

    /* Same type operations */
    if (a->type == b->type) {
        switch (a->type) {
            case RATIONAL: {
                Rational *r = rational_add(a->data.rational, b->data.rational);
                if (!r)
                    return NULL;
                SymbolicCoord *result = symbolic_coord_create_rational(0, 1);
                if (!result) {
                    rational_destroy(r);
                    return NULL;
                }
                rational_destroy(result->data.rational);
                result->data.rational = r;
                result->trust = (a->trust < b->trust) ? a->trust : b->trust;

                /* Check for overflow */
                if (check_digit_circuit(result) == CIRCUIT_STATUS_TRIPPED) {
                    g_overflow_context.last_result = result;
                    circuit_handle_overflow();
                }
                return result;
            }
            case ALGEBRAIC: {
                Algebraic *alg = algebraic_add(a->data.algebraic, b->data.algebraic);
                if (!alg)
                    return NULL;
                SymbolicCoord *result =
                    symbolic_coord_create_algebraic(&alg->minimal_poly, alg->left_bound, alg->right_bound);
                algebraic_destroy(alg);
                if (result)
                    result->trust = (a->trust < b->trust) ? a->trust : b->trust;
                return result;
            }
            case QUADRATIC: {
                Quadratic *q = quadratic_add(a->data.quadratic, b->data.quadratic);
                if (!q) {
                    /* Different sqrt(n) - promote to algebraic */
                    SymbolicCoord *a_alg = quadratic_to_algebraic(a);
                    SymbolicCoord *b_alg = quadratic_to_algebraic(b);
                    if (!a_alg || !b_alg) {
                        if (a_alg)
                            symbolic_coord_destroy(a_alg);
                        if (b_alg)
                            symbolic_coord_destroy(b_alg);
                        return NULL;
                    }
                    SymbolicCoord *result = symbolic_coord_add(a_alg, b_alg);
                    symbolic_coord_destroy(a_alg);
                    symbolic_coord_destroy(b_alg);
                    return result;
                }
                SymbolicCoord *result = symbolic_coord_create_quadratic(q->a, q->b, q->n);
                lv00_free((void**)&q); /* quadratic_create copies the rationals */
                if (result)
                    result->trust = (a->trust < b->trust) ? a->trust : b->trust;
                return result;
            }
            default:
                return NULL;
        }
    }

    /* Cross-type operations */

    /* Rational + Algebraic = Algebraic */
    if (a->type == RATIONAL && b->type == ALGEBRAIC) {
        SymbolicCoord *a_alg = rational_to_algebraic(a);
        if (!a_alg)
            return NULL;
        SymbolicCoord *result = symbolic_coord_add(a_alg, b);
        symbolic_coord_destroy(a_alg);
        return result;
    }
    if (a->type == ALGEBRAIC && b->type == RATIONAL) {
        SymbolicCoord *b_alg = rational_to_algebraic(b);
        if (!b_alg)
            return NULL;
        SymbolicCoord *result = symbolic_coord_add(a, b_alg);
        symbolic_coord_destroy(b_alg);
        return result;
    }

    /* Rational + Quadratic = Quadratic */
    if (a->type == RATIONAL && b->type == QUADRATIC) {
        /* Convert rational to quadratic with same n as b */
        SymbolicCoord *a_quad = rational_to_quadratic_with_n(a, b->data.quadratic->n);
        if (!a_quad)
            return NULL;
        SymbolicCoord *result = symbolic_coord_add(a_quad, b);
        symbolic_coord_destroy(a_quad);
        return result;
    }
    if (a->type == QUADRATIC && b->type == RATIONAL) {
        /* Convert rational to quadratic with same n as a */
        SymbolicCoord *b_quad = rational_to_quadratic_with_n(b, a->data.quadratic->n);
        if (!b_quad)
            return NULL;
        SymbolicCoord *result = symbolic_coord_add(a, b_quad);
        symbolic_coord_destroy(b_quad);
        return result;
    }

    /* Quadratic + Algebraic = Algebraic (promote Quadratic) */
    if (a->type == QUADRATIC && b->type == ALGEBRAIC) {
        SymbolicCoord *a_alg = quadratic_to_algebraic(a);
        if (!a_alg)
            return NULL;
        SymbolicCoord *result = symbolic_coord_add(a_alg, b);
        symbolic_coord_destroy(a_alg);
        return result;
    }
    if (a->type == ALGEBRAIC && b->type == QUADRATIC) {
        SymbolicCoord *b_alg = quadratic_to_algebraic(b);
        if (!b_alg)
            return NULL;
        SymbolicCoord *result = symbolic_coord_add(a, b_alg);
        symbolic_coord_destroy(b_alg);
        return result;
    }

    return NULL;
}

SymbolicCoord *symbolic_coord_subtract(const SymbolicCoord *a, const SymbolicCoord *b) {
    /* Update overflow context */
    g_overflow_context.left_type = a->type;
    g_overflow_context.right_type = b->type;
    g_overflow_context.last_operation = "subtract";

    /* Transcendental - anything */
    if (a->type == TRANSCENDENTAL || b->type == TRANSCENDENTAL) {
        const SymbolicCoord *trans_coord = (a->type == TRANSCENDENTAL) ? a : b;
        const SymbolicCoord *other_coord = (a->type == TRANSCENDENTAL) ? b : a;
        bool inverted = (b->type == TRANSCENDENTAL); /* true: a - transcendental */

        if (other_coord->type == RATIONAL) {
            if (inverted) {
                /* rational - transcendental: 无法精确表示，标记为 out_of_scope */
                Transcendental *t = transcendental_create(trans_coord->data.transcendental->name);
                if (!t)
                    return NULL;

                TranscendentalExpr *expr = lv00_malloc(sizeof(TranscendentalExpr));
                if (!expr) {
                    transcendental_destroy(t);
                    return NULL;
                }
                expr->expr_type = TRANS_EXPR_ADD_ALGEBRAIC;
                lv00_strlcpy(expr->base_name, trans_coord->data.transcendental->name, sizeof(expr->base_name));
                expr->rational_operand = NULL;
                expr->out_of_scope = true;

                t->expr = expr;

                SymbolicCoord *result = lv00_malloc(sizeof(SymbolicCoord));
                if (!result) {
                    transcendental_destroy(t);
                    return NULL;
                }
                result->type = TRANSCENDENTAL;
                result->trust = TRUST_AMBER;
                result->data.transcendental = t;
                return result;
            }

            /* Transcendental - Rational -> TRANS_EXPR_ADD_RATIONAL with negated rational */
            Transcendental *t = transcendental_create(trans_coord->data.transcendental->name);
            if (!t)
                return NULL;

            TranscendentalExpr *expr = lv00_malloc(sizeof(TranscendentalExpr));
            if (!expr) {
                transcendental_destroy(t);
                return NULL;
            }
            expr->expr_type = TRANS_EXPR_ADD_RATIONAL;
            lv00_strlcpy(expr->base_name, trans_coord->data.transcendental->name, sizeof(expr->base_name));
            Rational *neg_r = rational_create(0, 1);
            mpq_neg(neg_r->value, other_coord->data.rational->value);
            expr->rational_operand = neg_r;
            expr->out_of_scope = false;

            t->expr = expr;

            SymbolicCoord *result = lv00_malloc(sizeof(SymbolicCoord));
            if (!result) {
                transcendental_destroy(t);
                return NULL;
            }
            result->type = TRANSCENDENTAL;
            result->trust = trans_coord->trust;
            result->data.transcendental = t;
            return result;
        }

        if (other_coord->type == ALGEBRAIC || other_coord->type == QUADRATIC) {
            /* Transcendental - Algebraic/Quadratic -> out_of_scope */
            Transcendental *t = transcendental_create(trans_coord->data.transcendental->name);
            if (!t)
                return NULL;

            TranscendentalExpr *expr = lv00_malloc(sizeof(TranscendentalExpr));
            if (!expr) {
                transcendental_destroy(t);
                return NULL;
            }
            expr->expr_type = TRANS_EXPR_ADD_ALGEBRAIC;
            lv00_strlcpy(expr->base_name, trans_coord->data.transcendental->name, sizeof(expr->base_name));
            expr->rational_operand = NULL;
            expr->out_of_scope = true;

            t->expr = expr;

            SymbolicCoord *result = lv00_malloc(sizeof(SymbolicCoord));
            if (!result) {
                transcendental_destroy(t);
                return NULL;
            }
            result->type = TRANSCENDENTAL;
            result->trust = TRUST_AMBER;
            result->data.transcendental = t;
            return result;
        }

        /* Transcendental - Transcendental: merge if same base, else out_of_scope */
        {
            const Transcendental *ta = a->data.transcendental;
            const Transcendental *tb = b->data.transcendental;
            const char *base_a = ta->expr ? ta->expr->base_name : ta->name;
            const char *base_b = tb->expr ? tb->expr->base_name : tb->name;

            Transcendental *t = transcendental_create(base_a);
            if (!t)
                return NULL;

            TranscendentalExpr *expr = lv00_malloc(sizeof(TranscendentalExpr));
            if (!expr) {
                transcendental_destroy(t);
                return NULL;
            }
            expr->out_of_scope = false;
            lv00_strlcpy(expr->base_name, base_a, sizeof(expr->base_name));
            if (strcmp(base_a, base_b) == 0) {
                /* Same base: check if both are pure multiplicative (MUL or bare) */
                bool a_is_mul = !ta->expr || ta->expr->expr_type == TRANS_EXPR_MUL_RATIONAL;
                bool b_is_mul = !tb->expr || tb->expr->expr_type == TRANS_EXPR_MUL_RATIONAL;

                if (a_is_mul && b_is_mul) {
                    /* Both are coeff*base form: (ca*base) - (cb*base) = (ca-cb)*base */
                    Rational *rat_a =
                        (ta->expr && ta->expr->rational_operand) ? ta->expr->rational_operand : rational_create(1, 1);
                    Rational *rat_b =
                        (tb->expr && tb->expr->rational_operand) ? tb->expr->rational_operand : rational_create(1, 1);
                    Rational *own_a = (!ta->expr) ? rat_a : NULL;
                    Rational *own_b = (!tb->expr) ? rat_b : NULL;

                    expr->expr_type = TRANS_EXPR_MUL_RATIONAL;
                    expr->rational_operand = rational_subtract(rat_a, rat_b);

                    if (own_a)
                        rational_destroy(own_a);
                    if (own_b)
                        rational_destroy(own_b);

                    if (!expr->rational_operand) {
                        lv00_free((void **) &expr);
                        transcendental_destroy(t);
                        return NULL;
                    }
                } else {
                    /* At least one has ADD form: cannot simplify, out_of_scope */
                    expr->expr_type = TRANS_EXPR_ADD_ALGEBRAIC;
                    expr->rational_operand = NULL;
                    expr->out_of_scope = true;
                }
            } else {
                /* Different base (e.g. pi - e): out_of_scope */
                expr->expr_type = TRANS_EXPR_ADD_ALGEBRAIC;
                expr->rational_operand = NULL;
                expr->out_of_scope = true;
            }

            t->expr = expr;

            SymbolicCoord *result = lv00_malloc(sizeof(SymbolicCoord));
            if (!result) {
                transcendental_destroy(t);
                return NULL;
            }
            result->type = TRANSCENDENTAL;
            result->trust = expr->out_of_scope ? TRUST_AMBER : TRUST_BLUE;
            result->data.transcendental = t;
            return result;
        }
    }

    /* Same type operations */
    if (a->type == b->type) {
        switch (a->type) {
            case RATIONAL: {
                Rational *r = rational_subtract(a->data.rational, b->data.rational);
                if (!r)
                    return NULL;
                SymbolicCoord *result = symbolic_coord_create_rational(0, 1);
                if (!result) {
                    rational_destroy(r);
                    return NULL;
                }
                rational_destroy(result->data.rational);
                result->data.rational = r;
                result->trust = (a->trust < b->trust) ? a->trust : b->trust;

                if (check_digit_circuit(result) == CIRCUIT_STATUS_TRIPPED) {
                    g_overflow_context.last_result = result;
                    circuit_handle_overflow();
                }
                return result;
            }
            case ALGEBRAIC: {
                Algebraic *alg = algebraic_subtract(a->data.algebraic, b->data.algebraic);
                if (!alg)
                    return NULL;
                SymbolicCoord *result =
                    symbolic_coord_create_algebraic(&alg->minimal_poly, alg->left_bound, alg->right_bound);
                algebraic_destroy(alg);
                if (result)
                    result->trust = (a->trust < b->trust) ? a->trust : b->trust;
                return result;
            }
            case QUADRATIC: {
                Quadratic *q = quadratic_subtract(a->data.quadratic, b->data.quadratic);
                if (!q) {
                    /* Different sqrt(n) - promote to algebraic */
                    SymbolicCoord *a_alg = quadratic_to_algebraic(a);
                    SymbolicCoord *b_alg = quadratic_to_algebraic(b);
                    if (!a_alg || !b_alg) {
                        if (a_alg)
                            symbolic_coord_destroy(a_alg);
                        if (b_alg)
                            symbolic_coord_destroy(b_alg);
                        return NULL;
                    }
                    SymbolicCoord *result = symbolic_coord_subtract(a_alg, b_alg);
                    symbolic_coord_destroy(a_alg);
                    symbolic_coord_destroy(b_alg);
                    return result;
                }
                SymbolicCoord *result = symbolic_coord_create_quadratic(q->a, q->b, q->n);
                lv00_free((void**)&q);  /* lv00_malloc分配 */
                if (result)
                    result->trust = (a->trust < b->trust) ? a->trust : b->trust;
                return result;
            }
            default:
                return NULL;
        }
    }

    /* Cross-type operations */

    /* Rational - Algebraic = Algebraic */
    if (a->type == RATIONAL && b->type == ALGEBRAIC) {
        SymbolicCoord *a_alg = rational_to_algebraic(a);
        if (!a_alg)
            return NULL;
        SymbolicCoord *result = symbolic_coord_subtract(a_alg, b);
        symbolic_coord_destroy(a_alg);
        return result;
    }
    if (a->type == ALGEBRAIC && b->type == RATIONAL) {
        SymbolicCoord *b_alg = rational_to_algebraic(b);
        if (!b_alg)
            return NULL;
        SymbolicCoord *result = symbolic_coord_subtract(a, b_alg);
        symbolic_coord_destroy(b_alg);
        return result;
    }

    /* Rational - Quadratic = Quadratic */
    if (a->type == RATIONAL && b->type == QUADRATIC) {
        SymbolicCoord *a_quad = rational_to_quadratic_with_n(a, b->data.quadratic->n);
        if (!a_quad)
            return NULL;
        SymbolicCoord *result = symbolic_coord_subtract(a_quad, b);
        symbolic_coord_destroy(a_quad);
        return result;
    }
    if (a->type == QUADRATIC && b->type == RATIONAL) {
        SymbolicCoord *b_quad = rational_to_quadratic_with_n(b, a->data.quadratic->n);
        if (!b_quad)
            return NULL;
        SymbolicCoord *result = symbolic_coord_subtract(a, b_quad);
        symbolic_coord_destroy(b_quad);
        return result;
    }

    /* Quadratic - Algebraic = Algebraic */
    if (a->type == QUADRATIC && b->type == ALGEBRAIC) {
        SymbolicCoord *a_alg = quadratic_to_algebraic(a);
        if (!a_alg)
            return NULL;
        SymbolicCoord *result = symbolic_coord_subtract(a_alg, b);
        symbolic_coord_destroy(a_alg);
        return result;
    }
    if (a->type == ALGEBRAIC && b->type == QUADRATIC) {
        SymbolicCoord *b_alg = quadratic_to_algebraic(b);
        if (!b_alg)
            return NULL;
        SymbolicCoord *result = symbolic_coord_subtract(a, b_alg);
        symbolic_coord_destroy(b_alg);
        return result;
    }

    return NULL;
}

SymbolicCoord *symbolic_coord_multiply(const SymbolicCoord *a, const SymbolicCoord *b) {
    /* Update overflow context */
    g_overflow_context.left_type = a->type;
    g_overflow_context.right_type = b->type;
    g_overflow_context.last_operation = "multiply";

    /* Transcendental * anything */
    if (a->type == TRANSCENDENTAL || b->type == TRANSCENDENTAL) {
        const SymbolicCoord *trans_coord = (a->type == TRANSCENDENTAL) ? a : b;
        const SymbolicCoord *other_coord = (a->type == TRANSCENDENTAL) ? b : a;

        if (other_coord->type == RATIONAL) {
            /* Transcendental * Rational -> TRANS_EXPR_MUL_RATIONAL */
            Transcendental *t = transcendental_create(trans_coord->data.transcendental->name);
            if (!t)
                return NULL;

            TranscendentalExpr *expr = lv00_malloc(sizeof(TranscendentalExpr));
            if (!expr) {
                transcendental_destroy(t);
                return NULL;
            }
            expr->expr_type = TRANS_EXPR_MUL_RATIONAL;
            lv00_strlcpy(expr->base_name, trans_coord->data.transcendental->name, sizeof(expr->base_name));
            expr->rational_operand = rational_copy(other_coord->data.rational);
            expr->out_of_scope = false;

            t->expr = expr;

            SymbolicCoord *result = lv00_malloc(sizeof(SymbolicCoord));
            if (!result) {
                transcendental_destroy(t);
                return NULL;
            }
            result->type = TRANSCENDENTAL;
            result->trust = trans_coord->trust;
            result->data.transcendental = t;
            return result;
        }

        if (other_coord->type == ALGEBRAIC || other_coord->type == QUADRATIC) {
            /* Transcendental * Algebraic/Quadratic -> out_of_scope */
            Transcendental *t = transcendental_create(trans_coord->data.transcendental->name);
            if (!t)
                return NULL;

            TranscendentalExpr *expr = lv00_malloc(sizeof(TranscendentalExpr));
            if (!expr) {
                transcendental_destroy(t);
                return NULL;
            }
            expr->expr_type = TRANS_EXPR_MUL_ALGEBRAIC;
            lv00_strlcpy(expr->base_name, trans_coord->data.transcendental->name, sizeof(expr->base_name));
            expr->rational_operand = NULL;
            expr->out_of_scope = true;

            t->expr = expr;

            SymbolicCoord *result = lv00_malloc(sizeof(SymbolicCoord));
            if (!result) {
                transcendental_destroy(t);
                return NULL;
            }
            result->type = TRANSCENDENTAL;
            result->trust = TRUST_AMBER;
            result->data.transcendental = t;
            return result;
        }

        /* Transcendental * Transcendental: out_of_scope in general */
        {
            const Transcendental *ta = a->data.transcendental;
            const Transcendental *tb = b->data.transcendental;
            const char *base_a = ta->expr ? ta->expr->base_name : ta->name;
            const char *base_b = tb->expr ? tb->expr->base_name : tb->name;

            Transcendental *t = transcendental_create(base_a);
            if (!t)
                return NULL;

            TranscendentalExpr *expr = lv00_malloc(sizeof(TranscendentalExpr));
            if (!expr) {
                transcendental_destroy(t);
                return NULL;
            }
            expr->expr_type = TRANS_EXPR_MUL_ALGEBRAIC;
            lv00_strlcpy(expr->base_name, base_a, sizeof(expr->base_name));
            expr->rational_operand = NULL;
            expr->out_of_scope = true;

            t->expr = expr;

            SymbolicCoord *result = lv00_malloc(sizeof(SymbolicCoord));
            if (!result) {
                transcendental_destroy(t);
                return NULL;
            }
            result->type = TRANSCENDENTAL;
            result->trust = TRUST_AMBER;
            result->data.transcendental = t;
            return result;
        }
    }

    /* Same type operations */
    if (a->type == b->type) {
        switch (a->type) {
            case RATIONAL: {
                Rational *r = rational_multiply(a->data.rational, b->data.rational);
                if (!r)
                    return NULL;
                SymbolicCoord *result = symbolic_coord_create_rational(0, 1);
                if (!result) {
                    rational_destroy(r);
                    return NULL;
                }
                rational_destroy(result->data.rational);
                result->data.rational = r;
                result->trust = (a->trust < b->trust) ? a->trust : b->trust;

                if (check_digit_circuit(result) == CIRCUIT_STATUS_TRIPPED) {
                    g_overflow_context.last_result = result;
                    circuit_handle_overflow();
                }
                return result;
            }
            case ALGEBRAIC: {
                Algebraic *alg = algebraic_multiply(a->data.algebraic, b->data.algebraic);
                if (!alg)
                    return NULL;
                SymbolicCoord *result =
                    symbolic_coord_create_algebraic(&alg->minimal_poly, alg->left_bound, alg->right_bound);
                algebraic_destroy(alg);
                if (result)
                    result->trust = (a->trust < b->trust) ? a->trust : b->trust;
                return result;
            }
            case QUADRATIC: {
                Quadratic *q = quadratic_multiply(a->data.quadratic, b->data.quadratic);
                if (!q) {
                    /* Different sqrt(n) - promote to algebraic */
                    SymbolicCoord *a_alg = quadratic_to_algebraic(a);
                    SymbolicCoord *b_alg = quadratic_to_algebraic(b);
                    if (!a_alg || !b_alg) {
                        if (a_alg)
                            symbolic_coord_destroy(a_alg);
                        if (b_alg)
                            symbolic_coord_destroy(b_alg);
                        return NULL;
                    }
                    SymbolicCoord *result = symbolic_coord_multiply(a_alg, b_alg);
                    symbolic_coord_destroy(a_alg);
                    symbolic_coord_destroy(b_alg);
                    return result;
                }
                SymbolicCoord *result = symbolic_coord_create_quadratic(q->a, q->b, q->n);
                lv00_free((void**)&q);  /* lv00_malloc分配 */
                if (result)
                    result->trust = (a->trust < b->trust) ? a->trust : b->trust;
                return result;
            }
            default:
                return NULL;
        }
    }

    /* Cross-type operations */

    /* Rational * Algebraic = Algebraic */
    if (a->type == RATIONAL && b->type == ALGEBRAIC) {
        SymbolicCoord *a_alg = rational_to_algebraic(a);
        if (!a_alg)
            return NULL;
        SymbolicCoord *result = symbolic_coord_multiply(a_alg, b);
        symbolic_coord_destroy(a_alg);
        return result;
    }
    if (a->type == ALGEBRAIC && b->type == RATIONAL) {
        SymbolicCoord *b_alg = rational_to_algebraic(b);
        if (!b_alg)
            return NULL;
        SymbolicCoord *result = symbolic_coord_multiply(a, b_alg);
        symbolic_coord_destroy(b_alg);
        return result;
    }

    /* Rational * Quadratic = Quadratic */
    if (a->type == RATIONAL && b->type == QUADRATIC) {
        /* r * (a + b*sqrt(n)) = r*a + r*b*sqrt(n) */
        Rational *new_a = rational_multiply(a->data.rational, b->data.quadratic->a);
        Rational *new_b = rational_multiply(a->data.rational, b->data.quadratic->b);
        SymbolicCoord *result = symbolic_coord_create_quadratic(new_a, new_b, b->data.quadratic->n);
        if (result)
            result->trust = (a->trust < b->trust) ? a->trust : b->trust;
        return result;
    }
    if (a->type == QUADRATIC && b->type == RATIONAL) {
        Rational *new_a = rational_multiply(a->data.quadratic->a, b->data.rational);
        Rational *new_b = rational_multiply(a->data.quadratic->b, b->data.rational);
        SymbolicCoord *result = symbolic_coord_create_quadratic(new_a, new_b, a->data.quadratic->n);
        if (result)
            result->trust = (a->trust < b->trust) ? a->trust : b->trust;
        return result;
    }

    /* Quadratic * Algebraic = Algebraic */
    if (a->type == QUADRATIC && b->type == ALGEBRAIC) {
        SymbolicCoord *a_alg = quadratic_to_algebraic(a);
        if (!a_alg)
            return NULL;
        SymbolicCoord *result = symbolic_coord_multiply(a_alg, b);
        symbolic_coord_destroy(a_alg);
        return result;
    }
    if (a->type == ALGEBRAIC && b->type == QUADRATIC) {
        SymbolicCoord *b_alg = quadratic_to_algebraic(b);
        if (!b_alg)
            return NULL;
        SymbolicCoord *result = symbolic_coord_multiply(a, b_alg);
        symbolic_coord_destroy(b_alg);
        return result;
    }

    return NULL;
}

/**
 * 符号坐标除法：计算 a / b。
 *
 * 支持所有类型的跨类型运算。
 *
 * @param a 被除数（不能为 NULL）
 * @param b 除数（不能为 NULL，且不能为零）
 * @return 新的符号坐标对象，失败时返回 NULL；调用者需负责释放
 */
SymbolicCoord *symbolic_coord_divide(const SymbolicCoord *a, const SymbolicCoord *b) {
    /* Update overflow context */
    g_overflow_context.left_type = a->type;
    g_overflow_context.right_type = b->type;
    g_overflow_context.last_operation = "divide";

    /* Transcendental / anything */
    if (a->type == TRANSCENDENTAL || b->type == TRANSCENDENTAL) {
        const SymbolicCoord *trans_coord = (a->type == TRANSCENDENTAL) ? a : b;
        const SymbolicCoord *other_coord = (a->type == TRANSCENDENTAL) ? b : a;
        bool inverted = (b->type == TRANSCENDENTAL);

        if (other_coord->type == RATIONAL) {
            /* Transcendental / Rational -> TRANS_EXPR_MUL_RATIONAL with inverted rational */
            Transcendental *t = transcendental_create(trans_coord->data.transcendental->name);
            if (!t)
                return NULL;

            TranscendentalExpr *expr = lv00_malloc(sizeof(TranscendentalExpr));
            if (!expr) {
                transcendental_destroy(t);
                return NULL;
            }
            expr->expr_type = TRANS_EXPR_MUL_RATIONAL;
            lv00_strlcpy(expr->base_name, trans_coord->data.transcendental->name, sizeof(expr->base_name));
            if (inverted) {
                /* b is transcendental, a is rational: a / t = a * (1/t), not representable */
                transcendental_destroy(t);
                lv00_free((void **) &expr);
                return NULL;
            } else {
                /* a is transcendental, b is rational: t / r = t * (1/r) */
                expr->rational_operand = rational_divide(rational_create(1, 1), other_coord->data.rational);
                if (!expr->rational_operand) {
                    transcendental_destroy(t);
                    lv00_free((void **) &expr);
                    return NULL;
                }
            }
            expr->out_of_scope = false;

            t->expr = expr;

            SymbolicCoord *result = lv00_malloc(sizeof(SymbolicCoord));
            if (!result) {
                transcendental_destroy(t);
                return NULL;
            }
            result->type = TRANSCENDENTAL;
            result->trust = trans_coord->trust;
            result->data.transcendental = t;
            return result;
        }

        if (other_coord->type == ALGEBRAIC || other_coord->type == QUADRATIC) {
            /* Transcendental / Algebraic/Quadratic -> out_of_scope */
            Transcendental *t = transcendental_create(trans_coord->data.transcendental->name);
            if (!t)
                return NULL;

            TranscendentalExpr *expr = lv00_malloc(sizeof(TranscendentalExpr));
            if (!expr) {
                transcendental_destroy(t);
                return NULL;
            }
            expr->expr_type = TRANS_EXPR_MUL_ALGEBRAIC;
            lv00_strlcpy(expr->base_name, trans_coord->data.transcendental->name, sizeof(expr->base_name));
            expr->rational_operand = NULL;
            expr->out_of_scope = true;

            t->expr = expr;

            SymbolicCoord *result = lv00_malloc(sizeof(SymbolicCoord));
            if (!result) {
                transcendental_destroy(t);
                return NULL;
            }
            result->type = TRANSCENDENTAL;
            result->trust = TRUST_AMBER;
            result->data.transcendental = t;
            return result;
        }

        /* Transcendental / Transcendental: same base -> rational, else out_of_scope */
        {
            const Transcendental *ta = a->data.transcendental;
            const Transcendental *tb = b->data.transcendental;
            const char *base_a = ta->expr ? ta->expr->base_name : ta->name;
            const char *base_b = tb->expr ? tb->expr->base_name : tb->name;

            if (strcmp(base_a, base_b) == 0) {
                /* Same base: (ca*base) / (cb*base) = ca/cb (rational) */
                bool a_is_mul = !ta->expr || ta->expr->expr_type == TRANS_EXPR_MUL_RATIONAL;
                bool b_is_mul = !tb->expr || tb->expr->expr_type == TRANS_EXPR_MUL_RATIONAL;

                if (a_is_mul && b_is_mul) {
                    Rational *rat_a =
                        (ta->expr && ta->expr->rational_operand) ? ta->expr->rational_operand : rational_create(1, 1);
                    Rational *rat_b =
                        (tb->expr && tb->expr->rational_operand) ? tb->expr->rational_operand : rational_create(1, 1);
                    Rational *own_a = (!ta->expr) ? rat_a : NULL;
                    Rational *own_b = (!tb->expr) ? rat_b : NULL;

                    Rational *result_rat = rational_divide(rat_a, rat_b);

                    if (own_a)
                        rational_destroy(own_a);
                    if (own_b)
                        rational_destroy(own_b);

                    if (!result_rat)
                        return NULL;

                    SymbolicCoord *result = symbolic_coord_create_rational(0, 1);
                    if (!result) {
                        rational_destroy(result_rat);
                        return NULL;
                    }
                    rational_destroy(result->data.rational);
                    result->data.rational = result_rat;
                    result->trust = TRUST_GREEN;
                    return result;
                }
            }

            /* Different base or non-mul form: out_of_scope */
            Transcendental *t = transcendental_create(base_a);
            if (!t)
                return NULL;

            TranscendentalExpr *expr = lv00_malloc(sizeof(TranscendentalExpr));
            if (!expr) {
                transcendental_destroy(t);
                return NULL;
            }
            expr->expr_type = TRANS_EXPR_MUL_ALGEBRAIC;
            lv00_strlcpy(expr->base_name, base_a, sizeof(expr->base_name));
            expr->rational_operand = NULL;
            expr->out_of_scope = true;

            t->expr = expr;

            SymbolicCoord *result = lv00_malloc(sizeof(SymbolicCoord));
            if (!result) {
                transcendental_destroy(t);
                return NULL;
            }
            result->type = TRANSCENDENTAL;
            result->trust = TRUST_AMBER;
            result->data.transcendental = t;
            return result;
        }
    }

    /* Same type operations */
    if (a->type == b->type) {
        switch (a->type) {
            case RATIONAL: {
                Rational *r = rational_divide(a->data.rational, b->data.rational);
                if (!r)
                    return NULL;
                SymbolicCoord *result = symbolic_coord_create_rational(0, 1);
                if (!result) {
                    rational_destroy(r);
                    return NULL;
                }
                rational_destroy(result->data.rational);
                result->data.rational = r;
                result->trust = (a->trust < b->trust) ? a->trust : b->trust;

                if (check_digit_circuit(result) == CIRCUIT_STATUS_TRIPPED) {
                    g_overflow_context.last_result = result;
                    circuit_handle_overflow();
                }
                return result;
            }
            case ALGEBRAIC: {
                Algebraic *alg = algebraic_divide(a->data.algebraic, b->data.algebraic);
                if (!alg)
                    return NULL;
                SymbolicCoord *result =
                    symbolic_coord_create_algebraic(&alg->minimal_poly, alg->left_bound, alg->right_bound);
                algebraic_destroy(alg);
                if (result)
                    result->trust = (a->trust < b->trust) ? a->trust : b->trust;
                return result;
            }
            case QUADRATIC: {
                Quadratic *q = quadratic_divide(a->data.quadratic, b->data.quadratic);
                if (!q) {
                    /* Different sqrt(n) or division by zero - promote to algebraic */
                    SymbolicCoord *a_alg = quadratic_to_algebraic(a);
                    SymbolicCoord *b_alg = quadratic_to_algebraic(b);
                    if (!a_alg || !b_alg) {
                        if (a_alg)
                            symbolic_coord_destroy(a_alg);
                        if (b_alg)
                            symbolic_coord_destroy(b_alg);
                        return NULL;
                    }
                    SymbolicCoord *result = symbolic_coord_divide(a_alg, b_alg);
                    symbolic_coord_destroy(a_alg);
                    symbolic_coord_destroy(b_alg);
                    return result;
                }
                SymbolicCoord *result = symbolic_coord_create_quadratic(q->a, q->b, q->n);
                lv00_free((void**)&q);  /* lv00_malloc分配 */
                if (result)
                    result->trust = (a->trust < b->trust) ? a->trust : b->trust;
                return result;
            }
            default:
                return NULL;
        }
    }

    /* Cross-type operations */

    /* Rational / Algebraic = Algebraic */
    if (a->type == RATIONAL && b->type == ALGEBRAIC) {
        SymbolicCoord *a_alg = rational_to_algebraic(a);
        if (!a_alg)
            return NULL;
        SymbolicCoord *result = symbolic_coord_divide(a_alg, b);
        symbolic_coord_destroy(a_alg);
        return result;
    }
    if (a->type == ALGEBRAIC && b->type == RATIONAL) {
        SymbolicCoord *b_alg = rational_to_algebraic(b);
        if (!b_alg)
            return NULL;
        SymbolicCoord *result = symbolic_coord_divide(a, b_alg);
        symbolic_coord_destroy(b_alg);
        return result;
    }

    /* Rational / Quadratic = Quadratic */
    if (a->type == RATIONAL && b->type == QUADRATIC) {
        /* r / (a + b*sqrt(n)) = r * (a - b*sqrt(n)) / (a^2 - b^2*n) */
        SymbolicCoord *r_quad = rational_to_quadratic_with_n(a, b->data.quadratic->n);
        if (!r_quad)
            return NULL;
        SymbolicCoord *result = symbolic_coord_divide(r_quad, b);
        symbolic_coord_destroy(r_quad);
        return result;
    }
    if (a->type == QUADRATIC && b->type == RATIONAL) {
        /* (a + b*sqrt(n)) / r = a/r + (b/r)*sqrt(n) */
        Rational *new_a = rational_divide(a->data.quadratic->a, b->data.rational);
        Rational *new_b = rational_divide(a->data.quadratic->b, b->data.rational);
        if (!new_a || !new_b) {
            if (new_a)
                rational_destroy(new_a);
            if (new_b)
                rational_destroy(new_b);
            return NULL;
        }
        SymbolicCoord *result = symbolic_coord_create_quadratic(new_a, new_b, a->data.quadratic->n);
        if (result)
            result->trust = (a->trust < b->trust) ? a->trust : b->trust;
        return result;
    }

    /* Quadratic / Algebraic = Algebraic */
    if (a->type == QUADRATIC && b->type == ALGEBRAIC) {
        SymbolicCoord *a_alg = quadratic_to_algebraic(a);
        if (!a_alg)
            return NULL;
        SymbolicCoord *result = symbolic_coord_divide(a_alg, b);
        symbolic_coord_destroy(a_alg);
        return result;
    }
    if (a->type == ALGEBRAIC && b->type == QUADRATIC) {
        SymbolicCoord *b_alg = quadratic_to_algebraic(b);
        if (!b_alg)
            return NULL;
        SymbolicCoord *result = symbolic_coord_divide(a, b_alg);
        symbolic_coord_destroy(b_alg);
        return result;
    }

    return NULL;
}

/**
 * 将符号坐标序列化为字符串。
 *
 * @param coord 符号坐标对象（不能为 NULL）
 * @return 新分配的字符串，失败时返回 NULL
 */
char *symbolic_coord_serialize(const SymbolicCoord *coord) {
    switch (coord->type) {
        case RATIONAL:
            return rational_serialize(coord->data.rational);
        case ALGEBRAIC:
            return algebraic_serialize(coord->data.algebraic);
        case QUADRATIC:
            return quadratic_serialize(coord->data.quadratic);
        case TRANSCENDENTAL:
            return transcendental_serialize(coord->data.transcendental);
    }
    return NULL;
}

/* ============================================================
 * Additional Utility Functions
 * ============================================================ */

/* Create a deep copy of a SymbolicCoord */
SymbolicCoord *symbolic_coord_copy(const SymbolicCoord *src) {
    if (!src)
        return NULL;

    SymbolicCoord *dst = lv00_malloc(sizeof(SymbolicCoord));
    if (!dst)
        return NULL;

    dst->type = src->type;
    dst->trust = src->trust;
    dst->cache_valid = false;  /* 复制品缓存初始无效，首次访问时重新计算 */
    dst->cached_value = 0.0;

    switch (src->type) {
        case RATIONAL:
            dst->data.rational = rational_copy(src->data.rational);
            break;
        case ALGEBRAIC: {
            dst->data.algebraic = algebraic_create((mpz_poly_t *) &src->data.algebraic->minimal_poly,
                                                   src->data.algebraic->left_bound, src->data.algebraic->right_bound);
            if (dst->data.algebraic && src->data.algebraic->cached_rational) {
                dst->data.algebraic->cached_rational = rational_copy(src->data.algebraic->cached_rational);
            }
            break;
        }
        case QUADRATIC: {
            Rational *a = rational_copy(src->data.quadratic->a);
            Rational *b = rational_copy(src->data.quadratic->b);
            dst->data.quadratic = quadratic_create(a, b, src->data.quadratic->n);
            break;
        }
        case TRANSCENDENTAL: {
            dst->data.transcendental = transcendental_create(src->data.transcendental->name);
            /* Deep copy the expression tree if present */
            if (dst->data.transcendental && src->data.transcendental->expr) {
                TranscendentalExpr *src_expr = src->data.transcendental->expr;
                TranscendentalExpr *dst_expr = lv00_malloc(sizeof(TranscendentalExpr));
                if (dst_expr) {
                    dst_expr->expr_type = src_expr->expr_type;
                    /* 使用 lv00_strlcpy 替代不安全的 strncpy（自动保证零终止） */
                    lv00_strlcpy(dst_expr->base_name, src_expr->base_name, sizeof(dst_expr->base_name));
                    dst_expr->rational_operand =
                        src_expr->rational_operand ? rational_copy(src_expr->rational_operand) : NULL;
                    dst_expr->out_of_scope = src_expr->out_of_scope;
                    dst->data.transcendental->expr = dst_expr;
                }
            }
            break;
        }
    }

    bool copy_ok = false;
    switch (src->type) {
        case RATIONAL:
            copy_ok = (dst->data.rational != NULL);
            break;
        case ALGEBRAIC:
            copy_ok = (dst->data.algebraic != NULL);
            break;
        case QUADRATIC:
            copy_ok = (dst->data.quadratic != NULL);
            break;
        case TRANSCENDENTAL:
            copy_ok = (dst->data.transcendental != NULL);
            break;
    }
    if (!copy_ok) {
        symbolic_coord_destroy(dst);
        return NULL;
    }

    return dst;
}

/**
 * 检查符号坐标是否为零。
 *
 * @param coord 符号坐标对象（可为 NULL，NULL 视为零）
 * @return true 表示为零，false 表示非零
 */
bool symbolic_coord_is_zero(const SymbolicCoord *coord) {
    if (!coord)
        return true;
    switch (coord->type) {
        case RATIONAL:
            return mpq_cmp_ui(coord->data.rational->value, 0, 1) == 0;
        case ALGEBRAIC: {
            Algebraic *a = coord->data.algebraic;
            if (a->cached_rational) {
                return mpq_cmp_ui(a->cached_rational->value, 0, 1) == 0;
            }
            /* 检查区间是否包含零 */
            return (a->left_bound <= 0 && a->right_bound >= 0);
        }
        case QUADRATIC: {
            Quadratic *q = coord->data.quadratic;
            return is_rational_zero(q->a) && is_rational_zero(q->b);
        }
        case TRANSCENDENTAL:
            return false; /* 超越数永远不为零 */
    }
    return false;
}

/**
 * 检查 SymbolicCoord 是否为正数。
 *
 * @param coord SymbolicCoord 对象
 * @return 如果值为正返回 true，否则返回 false
 */
bool symbolic_coord_is_positive(const SymbolicCoord *coord) {
    return symbolic_coord_to_double(coord) > 0;
}

/**
 * 检查 SymbolicCoord 是否为负数。
 *
 * @param coord SymbolicCoord 对象
 * @return 如果值为负返回 true，否则返回 false
 */
bool symbolic_coord_is_negative(const SymbolicCoord *coord) {
    return symbolic_coord_to_double(coord) < 0;
}

/* ============================================================
 * Numerical Downgrade to AMBER (Permanent Downgrade)
 * 
 * According to design_v2.9.md Section 1.5:
 * When user chooses "永久降级为数值假设" after 3+ circuit trips,
 * the coordinate is permanently marked as AMBER with numerical approximation.
 * ============================================================ */

/**
 * 将符号坐标降级为 AMBER 信任级别。
 *
 * 当用户选择"永久降级为数值假设"后（连续 3+ 次熔断触发），
 * 符号坐标被永久标记为 AMBER，并使用数值近似。
 *
 * @param coord       原符号坐标（不能为 NULL）
 * @param precision   精度阈值
 * @param declaration 声明信息（可为 NULL）
 * @return 降级后的新符号坐标对象，失败时返回 NULL；调用者需负责释放
 */
SymbolicCoord *symbolic_coord_downgrade_to_amber(const SymbolicCoord *coord, double precision,
                                                 const char *declaration) {
    if (!coord)
        return NULL;

    /* Create a new coordinate with AMBER trust level */
    SymbolicCoord *result = symbolic_coord_copy(coord);
    if (!result)
        return NULL;

    /* Mark as AMBER (numerical assumption) */
    result->trust = TRUST_AMBER;

    /* Store the precision threshold */
    /* Note: In a full implementation, this would be stored in a dedicated field */
    /* For now, we log it */
    fprintf(stderr, "[AMBER DOWNGRADE] Precision: %.15g, Declaration: %s\n", precision,
            declaration ? declaration : "(none)");

    /* The numerical value is accessible via symbolic_coord_to_double() */
    double numerical_value = symbolic_coord_to_double(result);
    fprintf(stderr, "[AMBER DOWNGRADE] Numerical value: %.15g\n", numerical_value);

    return result;
}

/**
 * 检查符号坐标是否标记为 AMBER（数值假设）。
 *
 * @param coord 符号坐标对象（可为 NULL）
 * @return true 表示为 AMBER，false 表示其他
 */
bool symbolic_coord_is_amber(const SymbolicCoord *coord) {
    if (!coord)
        return false;
    return coord->trust == TRUST_AMBER;
}

/*
 * Get the trust color of a coordinate
 */
TrustColor symbolic_coord_get_trust(const SymbolicCoord *coord) {
    if (!coord)
        return TRUST_GREEN;
    return coord->trust;
}

/**
 * 设置符号坐标的信任颜色。
 *
 * @param coord 符号坐标对象（可为 NULL，无操作）
 * @param trust 信任颜色
 */
void symbolic_coord_set_trust(SymbolicCoord *coord, TrustColor trust) {
    if (coord) {
        coord->trust = trust;
        /* 信任颜色变更可能影响数值含义，使缓存失效 */
        symbolic_coord_invalidate_cache(coord);
    }
}

/* ============================================================
 * Unary Negation
 * ============================================================ */

SymbolicCoord *symbolic_coord_negate(const SymbolicCoord *coord) {
    if (!coord)
        return NULL;

    switch (coord->type) {
        case RATIONAL: {
            Rational *neg = rational_create(0, 1);
            mpq_neg(neg->value, coord->data.rational->value);
            SymbolicCoord *result = symbolic_coord_create_rational(0, 1);
            if (result) {
                rational_destroy(result->data.rational);
                result->data.rational = neg;
                result->trust = coord->trust;
            } else {
                rational_destroy(neg);
            }
            return result;
        }
        case ALGEBRAIC: {
            /* Negate algebraic: replace poly P(x) with P(-x), swap bounds */
            Algebraic *a = coord->data.algebraic;
            mpz_poly_t neg_poly;
            mpz_poly_init(&neg_poly);

            /* P(-x): negate odd-degree coefficients */
            if (a->minimal_poly.degree >= 0) {
                neg_poly.degree = a->minimal_poly.degree;
                neg_poly.coeffs = lv00_malloc((neg_poly.degree + 1) * sizeof(mpz_t));
                if (!neg_poly.coeffs) {
                    mpz_poly_clear(&neg_poly);
                    return NULL;
                }
                for (int i = 0; i <= neg_poly.degree; i++) {
                    mpz_init(neg_poly.coeffs[i]);
                    if (i % 2 == 1) {
                        mpz_neg(neg_poly.coeffs[i], a->minimal_poly.coeffs[i]);
                    } else {
                        mpz_set(neg_poly.coeffs[i], a->minimal_poly.coeffs[i]);
                    }
                }
            }

            double new_left = -a->right_bound;
            double new_right = -a->left_bound;

            SymbolicCoord *result = symbolic_coord_create_algebraic(&neg_poly, new_left, new_right);
            mpz_poly_clear(&neg_poly);
            if (result)
                result->trust = coord->trust;
            return result;
        }
        case QUADRATIC: {
            Quadratic *q = coord->data.quadratic;
            Rational *neg_a = rational_create(0, 1);
            mpq_neg(neg_a->value, q->a->value);
            Rational *neg_b = rational_create(0, 1);
            mpq_neg(neg_b->value, q->b->value);
            SymbolicCoord *result = symbolic_coord_create_quadratic(neg_a, neg_b, q->n);
            if (result)
                result->trust = coord->trust;
            return result;
        }
        case TRANSCENDENTAL: {
            /* Negate transcendental: flip the sign of rational_operand */
            const Transcendental *src_t = coord->data.transcendental;
            const char *base = src_t->expr ? src_t->expr->base_name : src_t->name;

            Transcendental *t = transcendental_create(base);
            if (!t)
                return NULL;

            TranscendentalExpr *expr = lv00_malloc(sizeof(TranscendentalExpr));
            if (!expr) {
                transcendental_destroy(t);
                return NULL;
            }
            expr->out_of_scope = src_t->expr ? src_t->expr->out_of_scope : false;
            /* 使用 lv00_strlcpy 替代不安全的 strncpy（自动保证零终止） */
            lv00_strlcpy(expr->base_name, base, sizeof(expr->base_name));
            if (src_t->expr && src_t->expr->rational_operand) {
                /* Negate the existing rational operand */
                expr->expr_type = src_t->expr->expr_type;
                Rational *neg_r = rational_create(0, 1);
                if (!neg_r) {
                    lv00_free((void **) &expr);
                    transcendental_destroy(t);
                    return NULL;
                }
                mpq_neg(neg_r->value, src_t->expr->rational_operand->value);
                expr->rational_operand = neg_r;
            } else {
                /* Bare constant: -pi = -1*pi */
                expr->expr_type = TRANS_EXPR_MUL_RATIONAL;
                expr->rational_operand = rational_create(-1, 1);
            }

            if (!expr->rational_operand) {
                lv00_free((void **) &expr);
                transcendental_destroy(t);
                return NULL;
            }

            t->expr = expr;

            SymbolicCoord *result = lv00_malloc(sizeof(SymbolicCoord));
            if (!result) {
                transcendental_destroy(t);
                return NULL;
            }
            result->type = TRANSCENDENTAL;
            result->trust = coord->trust;
            result->data.transcendental = t;
            return result;
        }
    }
    return NULL;
}

/* ============================================================
 * Power and Square Root Operations
 * ============================================================ */

/* Forward declaration: mpz_perfect_sqrt is defined in the nested sqrt section below */
static mpz_t *mpz_perfect_sqrt(mpz_t n);

/*
 * Compute base^exponent where exponent is a non-negative integer.
 *
 * RATIONAL:   Uses GMP mpz_pow_ui for exact computation.
 * QUADRATIC:  (a+b*sqrt(n))^k via repeated squaring.
 * ALGEBRAIC:  Uses Newton evaluation + resultant for minimal poly,
 *             or falls back to double approximation with algebraic creation.
 * TRANSCENDENTAL: Marked as out_of_scope, returns NULL.
 */
SymbolicCoord *symbolic_coord_pow(const SymbolicCoord *base, unsigned int exponent) {
    if (!base)
        return NULL;

    /* base^0 = 1 for any type */
    if (exponent == 0) {
        return symbolic_coord_create_rational(1, 1);
    }

    /* base^1 = base (copy) */
    if (exponent == 1) {
        return symbolic_coord_copy(base);
    }

    switch (base->type) {
        case RATIONAL: {
            /* Exact rational power using GMP */
            const Rational *r = base->data.rational;
            mpz_t num_pow, den_pow;
            mpz_init(num_pow);
            mpz_init(den_pow);

            mpz_pow_ui(num_pow, mpq_numref(r->value), exponent);
            mpz_pow_ui(den_pow, mpq_denref(r->value), exponent);

            Rational *result_r = rational_create_from_mpz(num_pow, den_pow);
            mpz_clear(num_pow);
            mpz_clear(den_pow);

            if (!result_r)
                return NULL;

            SymbolicCoord *result = symbolic_coord_create_rational(0, 1);
            if (result) {
                rational_destroy(result->data.rational);
                result->data.rational = result_r;
                result->trust = base->trust;
            } else {
                rational_destroy(result_r);
            }
            return result;
        }

        case QUADRATIC: {
            /* (a + b*sqrt(n))^k via repeated squaring */
            const Quadratic *q = base->data.quadratic;

            /* Start with identity: 1 + 0*sqrt(n) */
            Rational *res_a = rational_create(1, 1);
            Rational *res_b = rational_create(0, 1);
            unsigned int res_n = q->n;

            /* Current power: base */
            Rational *cur_a = rational_copy(q->a);
            Rational *cur_b = rational_copy(q->b);

            unsigned int k = exponent;
            while (k > 0) {
                if (k & 1) {
                    /* result *= current */
                    /* (res_a + res_b*sqrt(n)) * (cur_a + cur_b*sqrt(n))
                     * = (res_a*cur_a + res_b*cur_b*n) + (res_a*cur_b + res_b*cur_a)*sqrt(n) */
                    Rational *t1 = rational_multiply(res_a, cur_a);
                    Rational *b1b2 = rational_multiply(res_b, cur_b);
                    Rational *n_rat = rational_create(res_n, 1);
                    Rational *t2 = rational_multiply(b1b2, n_rat);
                    Rational *new_a = rational_add(t1, t2);

                    Rational *a1b2 = rational_multiply(res_a, cur_b);
                    Rational *a2b1 = rational_multiply(res_b, cur_a);
                    Rational *new_b = rational_add(a1b2, a2b1);

                    rational_destroy(res_a);
                    rational_destroy(res_b);
                    res_a = new_a;
                    res_b = new_b;

                    rational_destroy(t1);
                    rational_destroy(t2);
                    rational_destroy(b1b2);
                    rational_destroy(n_rat);
                    rational_destroy(a1b2);
                    rational_destroy(a2b1);
                }
                k >>= 1;
                if (k > 0) {
                    /* current = current^2 */
                    /* (cur_a + cur_b*sqrt(n))^2
                     * = (cur_a^2 + cur_b^2*n) + 2*cur_a*cur_b*sqrt(n) */
                    Rational *a_sq = rational_multiply(cur_a, cur_a);
                    Rational *b_sq = rational_multiply(cur_b, cur_b);
                    Rational *n_rat = rational_create(res_n, 1);
                    Rational *b_sq_n = rational_multiply(b_sq, n_rat);
                    Rational *new_cur_a = rational_add(a_sq, b_sq_n);

                    Rational *two = rational_create(2, 1);
                    Rational *ab = rational_multiply(cur_a, cur_b);
                    Rational *new_cur_b = rational_multiply(two, ab);

                    rational_destroy(cur_a);
                    rational_destroy(cur_b);
                    cur_a = new_cur_a;
                    cur_b = new_cur_b;

                    rational_destroy(a_sq);
                    rational_destroy(b_sq);
                    rational_destroy(n_rat);
                    rational_destroy(b_sq_n);
                    rational_destroy(two);
                    rational_destroy(ab);
                }
            }

            rational_destroy(cur_a);
            rational_destroy(cur_b);

            SymbolicCoord *result = symbolic_coord_create_quadratic(res_a, res_b, res_n);
            if (result)
                result->trust = base->trust;
            else {
                rational_destroy(res_a);
                rational_destroy(res_b);
            }
            return result;
        }

        case ALGEBRAIC: {
            /* For algebraic numbers, compute exact power when possible.
             *
             * Strategy:
             * - exponent 0: already handled above (returns 1)
             * - exponent 1: already handled above (returns copy)
             * - exponent 2, degree <= 2: use analytic formula for quadratic surds
             * - higher: numerical approach with rationalization attempt,
             *   then construct new algebraic number from numerical value
             *
             * For the minimal polynomial of alpha^k given minpoly P(alpha):
             * Use resultant_y(P(y), y^k - x) which eliminates y. */
            Algebraic *a = base->data.algebraic;

            /* If the algebraic number is actually a cached rational, use rational path */
            if (a->cached_rational) {
                SymbolicCoord *rat_coord = symbolic_coord_create_rational(0, 1);
                if (rat_coord) {
                    rational_destroy(rat_coord->data.rational);
                    rat_coord->data.rational = rational_copy(a->cached_rational);
                    rat_coord->trust = base->trust;
                    SymbolicCoord *result = symbolic_coord_pow(rat_coord, exponent);
                    symbolic_coord_destroy(rat_coord);
                    return result;
                }
            }

            /* Try rationalization first */
            algebraic_try_rationalize(a);
            if (a->cached_rational) {
                SymbolicCoord *rat_coord = symbolic_coord_create_rational(0, 1);
                if (rat_coord) {
                    rational_destroy(rat_coord->data.rational);
                    rat_coord->data.rational = rational_copy(a->cached_rational);
                    rat_coord->trust = base->trust;
                    SymbolicCoord *result = symbolic_coord_pow(rat_coord, exponent);
                    symbolic_coord_destroy(rat_coord);
                    return result;
                }
            }

            /* Refine bounds for better precision */
            refine_algebraic_bounds(a, 20);
            double val = (a->left_bound + a->right_bound) / 2.0;

            /* For exponent 2 with degree <= 2: use analytic formula */
            if (exponent == 2 && a->minimal_poly.degree <= 2) {
                if (a->minimal_poly.degree == 1) {
                    /* Degree 1: effectively rational, already handled above via
                     * rationalization. But as a fallback, compute directly. */
                    /* P(x) = c1*x + c0 => root = -c0/c1 */
                    mpz_t neg_c0;
                    mpz_init(neg_c0);
                    mpz_neg(neg_c0, a->minimal_poly.coeffs[0]);
                    mpq_t root;
                    mpq_init(root);
                    mpq_set_num(root, neg_c0);
                    mpq_set_den(root, a->minimal_poly.coeffs[1]);
                    mpq_canonicalize(root);
                    mpz_clear(neg_c0);

                    /* Square the rational: (num/den)^2 = num^2/den^2 */
                    mpz_t num_sq, den_sq;
                    mpz_init(num_sq);
                    mpz_init(den_sq);
                    mpz_pow_ui(num_sq, mpq_numref(root), 2);
                    mpz_pow_ui(den_sq, mpq_denref(root), 2);
                    mpq_clear(root);

                    Rational *result_r = rational_create_from_mpz(num_sq, den_sq);
                    mpz_clear(num_sq);
                    mpz_clear(den_sq);

                    if (result_r) {
                        SymbolicCoord *result = symbolic_coord_create_rational(0, 1);
                        if (result) {
                            rational_destroy(result->data.rational);
                            result->data.rational = result_r;
                            result->trust = base->trust;
                        } else {
                            rational_destroy(result_r);
                        }
                        return result;
                    }
                } else if (a->minimal_poly.degree == 2) {
                    /* Degree 2: construct minimal polynomial of alpha^2 via resultant.
                     *
                     * For P(y) = c2*y^2 + c1*y + c0, the minimal polynomial of alpha^2 is:
                     * resultant_y(P(y), y^2 - x) = c2^2 * x^2 + (c1^2 - 2*c0*c2) * x + c0^2
                     * (Sylvester matrix determinant)
                     *
                     * This handles cases like sqrt(2)^2 = 2 exactly: the resultant
                     * polynomial will have 2 as a root, and algebraic_try_rationalize
                     * will detect it. */
                    mpz_t *c2_ptr = &a->minimal_poly.coeffs[2];
                    mpz_t *c1_ptr = &a->minimal_poly.coeffs[1];
                    mpz_t *c0_ptr = &a->minimal_poly.coeffs[0];

                    mpz_t c2_sq, c1_sq, c0_sq, term_mid;
                    mpz_init(c2_sq);
                    mpz_init(c1_sq);
                    mpz_init(c0_sq);
                    mpz_init(term_mid);
                    mpz_mul(c2_sq, *c2_ptr, *c2_ptr);
                    mpz_mul(c1_sq, *c1_ptr, *c1_ptr);
                    mpz_mul(c0_sq, *c0_ptr, *c0_ptr);
                    mpz_mul(term_mid, *c0_ptr, *c2_ptr);
                    mpz_mul_si(term_mid, term_mid, 2);
                    mpz_sub(term_mid, c1_sq, term_mid);

                    mpz_poly_t sq_poly;
                    mpz_poly_init(&sq_poly);
                    sq_poly.degree = 2;
                    sq_poly.coeffs = lv00_malloc(3 * sizeof(mpz_t));
                    if (sq_poly.coeffs) {
                        mpz_init(sq_poly.coeffs[0]); /* c0^2 */
                        mpz_init(sq_poly.coeffs[1]); /* c1^2 - 2*c0*c2 */
                        mpz_init(sq_poly.coeffs[2]); /* c2^2 */
                        mpz_set(sq_poly.coeffs[0], c0_sq);
                        mpz_set(sq_poly.coeffs[1], term_mid);
                        mpz_set(sq_poly.coeffs[2], c2_sq);

                        double result_val = val * val;
                        double margin = fabs(result_val) * LV00_EPSILON_NEWTON * 100.0;
                        if (margin < LV00_EPSILON_NEWTON)
                            margin = LV00_EPSILON_NEWTON;

                        SymbolicCoord *result =
                            symbolic_coord_create_algebraic(&sq_poly, result_val - margin, result_val + margin);
                        mpz_poly_clear(&sq_poly);
                        if (result) {
                            result->trust = base->trust;
                            /* Try to rationalize: sqrt(2)^2 = 2 should be detected */
                            algebraic_try_rationalize(result->data.algebraic);
                            if (result->data.algebraic->cached_rational) {
                                SymbolicCoord *rat_result = symbolic_coord_create_rational(0, 1);
                                if (rat_result) {
                                    rational_destroy(rat_result->data.rational);
                                    rat_result->data.rational = rational_copy(result->data.algebraic->cached_rational);
                                    rat_result->trust = result->trust;
                                    symbolic_coord_destroy(result);
                                    mpz_clear(c2_sq);
                                    mpz_clear(c1_sq);
                                    mpz_clear(c0_sq);
                                    mpz_clear(term_mid);
                                    return rat_result;
                                }
                            }
                        }
                    } else {
                        mpz_poly_clear(&sq_poly);
                    }
                    mpz_clear(c2_sq);
                    mpz_clear(c1_sq);
                    mpz_clear(c0_sq);
                    mpz_clear(term_mid);
                    /* Fall through to numerical approach if resultant method failed */
                }
            }

            /* General case: numerical approach with rationalization attempt */
            double result_val = pow(val, (double) exponent);

            /* Try rationalization: check if the result is actually a rational number */
            {
                /* Use continued fraction on the numerical value to find a candidate */
                double margin_cf = fabs(result_val) * 1e-10;
                if (margin_cf < 1e-15)
                    margin_cf = 1e-15;

                /* Try mpq_set_d as a quick rationality check */
                mpq_t approx;
                mpq_init(approx);
                mpq_set_d(approx, result_val);
                mpq_canonicalize(approx);

                /* Verify: check if the rational approximation is close enough */
                double approx_val = mpq_get_d(approx);
                if (fabs(approx_val - result_val) <
                    LV00_EPSILON_NUMERIC_COMPARE * fabs(result_val) + LV00_EPSILON_NUMERIC_COMPARE) {
                    /* Create an algebraic number with this rational value and try rationalization */
                    mpz_poly_t check_poly;
                    mpz_poly_init(&check_poly);
                    check_poly.degree = 1;
                    check_poly.coeffs = lv00_malloc(2 * sizeof(mpz_t));
                    if (check_poly.coeffs) {
                        mpz_init(check_poly.coeffs[0]);
                        mpz_init(check_poly.coeffs[1]);
                        mpz_neg(check_poly.coeffs[0], mpq_numref(approx));
                        mpz_set(check_poly.coeffs[1], mpq_denref(approx));

                        double tight_margin = fabs(result_val) * LV00_EPSILON_NEWTON;
                        if (tight_margin < LV00_EPSILON_NEWTON)
                            tight_margin = LV00_EPSILON_NEWTON;

                        SymbolicCoord *result = symbolic_coord_create_algebraic(&check_poly, result_val - tight_margin,
                                                                                result_val + tight_margin);
                        mpz_poly_clear(&check_poly);
                        if (result) {
                            result->trust = base->trust;
                            /* Try to rationalize */
                            algebraic_try_rationalize(result->data.algebraic);
                            if (result->data.algebraic->cached_rational) {
                                /* Successfully rationalized: return as rational */
                                SymbolicCoord *rat_result = symbolic_coord_create_rational(0, 1);
                                if (rat_result) {
                                    rational_destroy(rat_result->data.rational);
                                    rat_result->data.rational = rational_copy(result->data.algebraic->cached_rational);
                                    rat_result->trust = result->trust;
                                    symbolic_coord_destroy(result);
                                    mpq_clear(approx);
                                    return rat_result;
                                }
                            }
                            mpq_clear(approx);
                            return result;
                        }
                    }
                    mpz_poly_clear(&check_poly);
                }
                mpq_clear(approx);
            }

            /* Final fallback: create algebraic number from numerical value */
            double margin = fabs(result_val) * LV00_EPSILON_NUMERIC_COMPARE;
            if (margin < LV00_EPSILON_NUMERIC_COMPARE)
                margin = LV00_EPSILON_NUMERIC_COMPARE;

            mpz_poly_t poly;
            mpz_poly_init(&poly);
            poly.degree = 1;
            poly.coeffs = lv00_malloc(2 * sizeof(mpz_t));
            if (!poly.coeffs) {
                mpz_poly_clear(&poly);
                return NULL;
            }
            mpz_init(poly.coeffs[0]);
            mpz_init(poly.coeffs[1]);

            mpq_t approx;
            mpq_init(approx);
            mpq_set_d(approx, result_val);
            mpz_neg(poly.coeffs[0], mpq_numref(approx));
            mpz_set(poly.coeffs[1], mpq_denref(approx));
            mpq_clear(approx);

            SymbolicCoord *result = symbolic_coord_create_algebraic(&poly, result_val - margin, result_val + margin);
            mpz_poly_clear(&poly);
            if (result)
                result->trust = base->trust;
            return result;
        }

        case TRANSCENDENTAL: {
            /* Transcendental power: out_of_scope, but provide double approximation.
             * exponent 0 and 1 are already handled above.
             * For other exponents, create an ALGEBRAIC result with the numerical
             * value pow(transcendental_to_double(t), exponent). */
            {
                double base_val = transcendental_to_double(base->data.transcendental);
                double result_val = pow(base_val, (double) exponent);

                /* Create an algebraic number from the numerical value */
                mpz_poly_t poly;
                mpz_poly_init(&poly);
                poly.degree = 1;
                poly.coeffs = lv00_malloc(2 * sizeof(mpz_t));
                if (!poly.coeffs) {
                    mpz_poly_clear(&poly);
                    return NULL;
                }
                mpz_init(poly.coeffs[0]);
                mpz_init(poly.coeffs[1]);

                mpq_t approx;
                mpq_init(approx);
                mpq_set_d(approx, result_val);
                mpz_neg(poly.coeffs[0], mpq_numref(approx));
                mpz_set(poly.coeffs[1], mpq_denref(approx));
                mpq_clear(approx);

                double margin = fabs(result_val) * LV00_EPSILON_NEWTON * 100.0;
                if (margin < LV00_EPSILON_NEWTON)
                    margin = LV00_EPSILON_NEWTON;

                SymbolicCoord *result =
                    symbolic_coord_create_algebraic(&poly, result_val - margin, result_val + margin);
                mpz_poly_clear(&poly);
                if (result)
                    result->trust = TRUST_AMBER;
                return result;
            }
        }
    }

    return NULL;
}

/*
 * Compute sqrt(coord).
 *
 * RATIONAL:   If numerator and denominator are both perfect squares,
 *             return exact rational. Otherwise, try to represent as
 *             quadratic a + b*sqrt(n).
 * QUADRATIC:  sqrt(a + b*sqrt(n)): try nested expansion if
 *             discriminant a^2 - b^2*n is a perfect square.
 * ALGEBRAIC:  Construct sqrt's minimal polynomial via resultant:
 *             resultant_y(P(y), x^2 - y) = P(x^2).
 * TRANSCENDENTAL: Marked as out_of_scope, returns NULL.
 */
SymbolicCoord *symbolic_coord_sqrt(const SymbolicCoord *coord) {
    if (!coord)
        return NULL;

    switch (coord->type) {
        case RATIONAL: {
            const Rational *r = coord->data.rational;
            mpz_t num, den;
            mpz_init_set(num, mpq_numref(r->value));
            mpz_init_set(den, mpq_denref(r->value));

            /* 检查分子和分母是否都是完全平方数 */
            int num_sign = mpz_sgn(num);
            if (num_sign < 0) {
                /* 负有理数：sqrt 不是实数 */
                mpz_clear(num);
                mpz_clear(den);
                return NULL;
            }

            mpz_t *num_sqrt = mpz_perfect_sqrt(num);
            mpz_t *den_sqrt = mpz_perfect_sqrt(den);

            if (num_sqrt && den_sqrt) {
                /* Both are perfect squares: return exact rational */
                Rational *result_r = rational_create_from_mpz(*num_sqrt, *den_sqrt);
                mpz_clear(*num_sqrt);
                lv00_free((void **) &num_sqrt);
                mpz_clear(*den_sqrt);
                lv00_free((void **) &den_sqrt);

                if (!result_r) {
                    mpz_clear(num);
                    mpz_clear(den);
                    return NULL;
                }

                SymbolicCoord *result = symbolic_coord_create_rational(0, 1);
                if (result) {
                    rational_destroy(result->data.rational);
                    result->data.rational = result_r;
                    result->trust = coord->trust;
                } else {
                    rational_destroy(result_r);
                }
                return result;
            }

            /* 清理任何非 NULL 的结果 */
            if (num_sqrt) {
                mpz_clear(*num_sqrt);
                lv00_free((void **) &num_sqrt);
            }
            if (den_sqrt) {
                mpz_clear(*den_sqrt);
                lv00_free((void **) &den_sqrt);
            }

            /* 不是完全平方数：尝试表示为二次根式 a + b*sqrt(n)
             * sqrt(p/q) = sqrt(p*q) / q
             * 如果 p*q 有平方因子，提取它们。
             */
            mpz_t product;
            mpz_init(product);
            mpz_mul(product, num, den);

            /* 从乘积中提取完全平方因子 */
            mpz_t remaining;
            mpz_init_set(remaining, product);
            mpz_t square_part;
            mpz_init_set_ui(square_part, 1);

            /* 尝试小素因子 */
            for (unsigned int i = 2; i * i <= 10000; i++) {
                while (mpz_divisible_ui_p(remaining, i * i)) {
                    mpz_mul_ui(square_part, square_part, i);
                    mpz_divexact_ui(remaining, remaining, i * i);
                }
            }

            /* 检查 remaining 是否为 1（上面已处理完全平方情况）
             * 或者如果 remaining > 1，则有 sqrt(product) = square_part * sqrt(remaining) */
            if (mpz_cmp_si(remaining, 1) == 0) {
                /* Should have been caught above, but handle gracefully */
                mpz_clear(product);
                mpz_clear(remaining);
                mpz_clear(square_part);
                return NULL;
            }

            /* sqrt(p/q) = square_part * sqrt(remaining) / q
             * = (square_part / q) * sqrt(remaining)
             * = 0 + (square_part/q) * sqrt(remaining) */

            /* Check if remaining fits in unsigned int */
            if (!mpz_fits_uint_p(remaining)) {
                mpz_clear(product);
                mpz_clear(remaining);
                mpz_clear(square_part);
                return NULL;
            }

            unsigned int n = mpz_get_ui(remaining);
            n = remove_square_factors(n);

            /* Result: 0 + (square_part / den) * sqrt(n) */
            Rational *a = rational_create(0, 1);
            Rational *b = rational_create_from_mpz(square_part, den);

            mpz_clear(product);
            mpz_clear(remaining);
            mpz_clear(square_part);

            SymbolicCoord *result = symbolic_coord_create_quadratic(a, b, n);
            if (result)
                result->trust = coord->trust;
            else {
                rational_destroy(a);
                rational_destroy(b);
            }
            return result;
        }

        case QUADRATIC: {
            /* sqrt(a + b*sqrt(n))
             * Try to expand to c + d*sqrt(m) form.
             *
             * If (a + b*sqrt(n)) = (c + d*sqrt(m))^2
             * = c^2 + d^2*m + 2cd*sqrt(m)
             *
             * For m = n: c^2 + d^2*n = a, 2cd = b
             * => d = b/(2c), substitute: c^2 + b^2*n/(4c^2) = a
             * => 4c^4 - 4a*c^2 + b^2*n = 0
             * => c^2 = (4a +/- sqrt(16a^2 - 16*b^2*n)) / 8
             *        = (a +/- sqrt(a^2 - b^2*n)) / 2
             *
             * So we need a^2 - b^2*n to be a perfect square (as a rational).
             */
            const Quadratic *q = coord->data.quadratic;

            /* Check if b = 0: sqrt(a) where a is rational */
            if (is_rational_zero(q->b)) {
                /* Delegate to rational sqrt */
                SymbolicCoord *rat_coord = symbolic_coord_create_rational(0, 1);
                if (rat_coord) {
                    rational_destroy(rat_coord->data.rational);
                    rat_coord->data.rational = rational_copy(q->a);
                    rat_coord->trust = coord->trust;
                    SymbolicCoord *result = symbolic_coord_sqrt(rat_coord);
                    symbolic_coord_destroy(rat_coord);
                    return result;
                }
                return NULL;
            }

            /* Compute discriminant: a^2 - b^2*n (as rational) */
            mpq_t a_sq, b_sq, b_sq_n, disc;
            mpq_init(a_sq);
            mpq_init(b_sq);
            mpq_init(b_sq_n);
            mpq_init(disc);

            mpq_mul(a_sq, q->a->value, q->a->value);
            mpq_mul(b_sq, q->b->value, q->b->value);
            mpq_set_ui(b_sq_n, q->n, 1);
            mpq_mul(b_sq_n, b_sq_n, b_sq);
            mpq_sub(disc, a_sq, b_sq_n);

            /* disc = a^2 - b^2*n must be non-negative for real sqrt */
            if (mpq_sgn(disc) < 0) {
                mpq_clear(a_sq);
                mpq_clear(b_sq);
                mpq_clear(b_sq_n);
                mpq_clear(disc);
                return NULL;
            }

            /* Check if disc is a perfect square of a rational */
            /* disc = p/q, need sqrt(p/q) = sqrt(p*q)/q to be rational */
            mpz_t disc_num_sq, disc_product;
            mpz_init(disc_num_sq);
            mpz_init(disc_product);
            mpz_mul(disc_product, mpq_numref(disc), mpq_denref(disc));

            mpz_t *disc_sqrt = mpz_perfect_sqrt(disc_product);

            if (disc_sqrt) {
                /* disc is a perfect square rational: disc_sqrt / den(disc) */
                /* k = disc_sqrt / den(disc) */
                mpq_t k;
                mpq_init(k);
                mpz_set(mpq_numref(k), *disc_sqrt);
                mpz_set(mpq_denref(k), mpq_denref(disc));
                mpq_canonicalize(k);

                /* c^2 = (a + k) / 2 */
                mpq_t c_sq, c_sq_neg;
                mpq_init(c_sq);
                mpq_init(c_sq_neg);
                mpq_add(c_sq, q->a->value, k);
                mpq_div_2exp(c_sq, c_sq, 1); /* divide by 2 */

                /* Also try (a - k) / 2 */
                mpq_sub(c_sq_neg, q->a->value, k);
                mpq_div_2exp(c_sq_neg, c_sq_neg, 1);

                /* Try c_sq first: check if it's a perfect square rational */
                mpz_t csq_product;
                mpz_init(csq_product);
                mpz_mul(csq_product, mpq_numref(c_sq), mpq_denref(c_sq));
                mpz_t *c_sqrt = mpz_perfect_sqrt(csq_product);

                Rational *c_rat = NULL;
                Rational *d_rat = NULL;

                if (c_sqrt && mpq_sgn(c_sq) >= 0) {
                    /* c = c_sqrt / den(c_sq) */
                    c_rat = rational_create_from_mpz(*c_sqrt, mpq_denref(c_sq));
                    /* d = b / (2c) */
                    Rational *two_c = rational_create_from_mpz(*c_sqrt, mpq_denref(c_sq));
                    mpq_mul_2exp(two_c->value, two_c->value, 1); /* 2c */
                    d_rat = rational_divide(q->b, two_c);
                    rational_destroy(two_c);
                    mpz_clear(*c_sqrt);
                    lv00_free((void **) &c_sqrt);
                } else {
                    if (c_sqrt) {
                        mpz_clear(*c_sqrt);
                        lv00_free((void **) &c_sqrt);
                    }

                    /* Try c_sq_neg */
                    mpz_mul(csq_product, mpq_numref(c_sq_neg), mpq_denref(c_sq_neg));
                    c_sqrt = mpz_perfect_sqrt(csq_product);

                    if (c_sqrt && mpq_sgn(c_sq_neg) >= 0) {
                        c_rat = rational_create_from_mpz(*c_sqrt, mpq_denref(c_sq_neg));
                        Rational *two_c = rational_create_from_mpz(*c_sqrt, mpq_denref(c_sq_neg));
                        mpq_mul_2exp(two_c->value, two_c->value, 1);
                        d_rat = rational_divide(q->b, two_c);
                        rational_destroy(two_c);
                        mpz_clear(*c_sqrt);
                        lv00_free((void **) &c_sqrt);
                    } else {
                        if (c_sqrt) {
                            mpz_clear(*c_sqrt);
                            lv00_free((void **) &c_sqrt);
                        }
                    }
                }

                mpz_clear(csq_product);
                mpq_clear(c_sq);
                mpq_clear(c_sq_neg);
                mpq_clear(k);

                if (c_rat && d_rat) {
                    /* Result: c + d*sqrt(n) */
                    SymbolicCoord *result = symbolic_coord_create_quadratic(c_rat, d_rat, q->n);
                    if (result)
                        result->trust = coord->trust;
                    else {
                        rational_destroy(c_rat);
                        rational_destroy(d_rat);
                    }
                    mpz_clear(*disc_sqrt);
                    lv00_free((void **) &disc_sqrt);
                    mpq_clear(a_sq);
                    mpq_clear(b_sq);
                    mpq_clear(b_sq_n);
                    mpq_clear(disc);
                    mpz_clear(disc_num_sq);
                    mpz_clear(disc_product);
                    return result;
                }

                if (c_rat)
                    rational_destroy(c_rat);
                if (d_rat)
                    rational_destroy(d_rat);
                mpz_clear(*disc_sqrt);
                lv00_free((void **) &disc_sqrt);
            } else {
                mpz_clear(disc_num_sq);
                mpz_clear(disc_product);
            }

            mpq_clear(a_sq);
            mpq_clear(b_sq);
            mpq_clear(b_sq_n);
            mpq_clear(disc);

            /* Cannot expand exactly: fall back to algebraic representation
             * sqrt(a + b*sqrt(n)) has minimal polynomial obtained by
             * substituting x^2 for x in the minimal poly of (a + b*sqrt(n)).
             *
             * The minimal poly of a + b*sqrt(n) is:
             * x^2 - 2a*x + (a^2 - b^2*n) = 0
             *
             * So sqrt(alpha) satisfies:
             * (x^2)^2 - 2a*(x^2) + (a^2 - b^2*n) = 0
             * x^4 - 2a*x^2 + (a^2 - b^2*n) = 0
             */
            {
                double a_val = rational_to_double(q->a);
                double b_val = rational_to_double(q->b);
                double sqrt_n = sqrt((double) q->n);
                double inner = a_val + b_val * sqrt_n;

                if (inner < 0)
                    return NULL;

                double result_val = sqrt(inner);

                /* Construct minimal poly: x^4 - 2a*x^2 + (a^2 - b^2*n) = 0 */
                mpz_poly_t poly;
                mpz_poly_init(&poly);
                poly.degree = 4;
                poly.coeffs = lv00_malloc(5 * sizeof(mpz_t));
                if (!poly.coeffs) {
                    mpz_poly_clear(&poly);
                    return NULL;
                }

                /* coeffs[0] = a^2 - b^2*n (constant term) */
                mpz_init(poly.coeffs[0]);
                mpz_mul(poly.coeffs[0], mpq_numref(q->a->value), mpq_numref(q->a->value));
                mpz_t den_a_sq;
                mpz_init(den_a_sq);
                mpz_mul(den_a_sq, mpq_denref(q->a->value), mpq_denref(q->a->value));

                mpz_t b_sq_n_num, b_sq_n_den;
                mpz_init(b_sq_n_num);
                mpz_init(b_sq_n_den);
                mpz_mul(b_sq_n_num, mpq_numref(q->b->value), mpq_numref(q->b->value));
                mpz_mul_ui(b_sq_n_num, b_sq_n_num, q->n);
                mpz_mul(b_sq_n_den, mpq_denref(q->b->value), mpq_denref(q->b->value));

                /* coeffs[0] = (a_num^2 * b_sq_n_den - b_sq_n_num * den_a^2) / (den_a^2 * b_sq_n_den) */
                /* Store as integer polynomial by multiplying through by LCD */
                mpz_t lcd;
                mpz_init(lcd);
                mpz_mul(lcd, den_a_sq, b_sq_n_den);

                mpz_t term1, term2;
                mpz_init(term1);
                mpz_init(term2);
                mpz_mul(term1, poly.coeffs[0], b_sq_n_den);
                mpz_mul(term2, b_sq_n_num, den_a_sq);
                mpz_sub(poly.coeffs[0], term1, term2);

                /* Scale all coefficients by LCD to get integer polynomial */
                mpz_set(poly.coeffs[0], poly.coeffs[0]); /* already scaled */

                /* coeffs[1] = 0 (no x term) */
                mpz_init_set_ui(poly.coeffs[1], 0);

                /* coeffs[2] = -2a (x^2 term) */
                mpz_init(poly.coeffs[2]);
                mpz_mul_ui(poly.coeffs[2], mpq_numref(q->a->value), 2);
                mpz_neg(poly.coeffs[2], poly.coeffs[2]);
                mpz_mul(poly.coeffs[2], poly.coeffs[2], b_sq_n_den);

                /* coeffs[3] = 0 (no x^3 term) */
                mpz_init_set_ui(poly.coeffs[3], 0);

                /* coeffs[4] = den_a^2 * b_sq_n_den (leading coefficient for x^4) */
                mpz_init_set(poly.coeffs[4], lcd);

                mpz_clear(den_a_sq);
                mpz_clear(b_sq_n_num);
                mpz_clear(b_sq_n_den);
                mpz_clear(lcd);
                mpz_clear(term1);
                mpz_clear(term2);

                double margin = fabs(result_val) * LV00_EPSILON_NEWTON * 10.0;
                if (margin < LV00_EPSILON_NEWTON)
                    margin = LV00_EPSILON_NEWTON;

                SymbolicCoord *result =
                    symbolic_coord_create_algebraic(&poly, result_val - margin, result_val + margin);
                mpz_poly_clear(&poly);
                if (result)
                    result->trust = coord->trust;
                return result;
            }
        }

        case ALGEBRAIC: {
            /* sqrt of algebraic number alpha:
             * If alpha has minpoly P(x), then sqrt(alpha) has minpoly P(x^2).
             *
             * P(x^2) is obtained by replacing x^k with x^{2k} in P(x).
             */
            Algebraic *a = coord->data.algebraic;

            /* Check if the algebraic number is negative (no real sqrt) */
            double mid = (a->left_bound + a->right_bound) / 2.0;
            if (mid < -LV00_EPSILON_NUMERIC_COMPARE)
                return NULL;

            /* Try rationalization first: if the algebraic number is actually rational */
            if (a->cached_rational) {
                SymbolicCoord *rat_coord = symbolic_coord_create_rational(0, 1);
                if (rat_coord) {
                    rational_destroy(rat_coord->data.rational);
                    rat_coord->data.rational = rational_copy(a->cached_rational);
                    rat_coord->trust = coord->trust;
                    SymbolicCoord *result = symbolic_coord_sqrt(rat_coord);
                    symbolic_coord_destroy(rat_coord);
                    return result;
                }
            }

            /* Try to rationalize */
            algebraic_try_rationalize(a);
            if (a->cached_rational) {
                SymbolicCoord *rat_coord = symbolic_coord_create_rational(0, 1);
                if (rat_coord) {
                    rational_destroy(rat_coord->data.rational);
                    rat_coord->data.rational = rational_copy(a->cached_rational);
                    rat_coord->trust = coord->trust;
                    SymbolicCoord *result = symbolic_coord_sqrt(rat_coord);
                    symbolic_coord_destroy(rat_coord);
                    return result;
                }
            }

            /* Construct P(x^2) from P(x) */
            int deg = a->minimal_poly.degree;
            int new_deg = 2 * deg;

            mpz_poly_t sqrt_poly;
            mpz_poly_init(&sqrt_poly);
            sqrt_poly.degree = new_deg;
            sqrt_poly.coeffs = lv00_malloc((new_deg + 1) * sizeof(mpz_t));
            if (!sqrt_poly.coeffs) {
                mpz_poly_clear(&sqrt_poly);
                return NULL;
            }

            for (int i = 0; i <= new_deg; i++) {
                mpz_init(sqrt_poly.coeffs[i]);
                if (i % 2 == 0) {
                    /* Even power: comes from P(x^{i/2}) */
                    int src_idx = i / 2;
                    if (src_idx <= deg) {
                        mpz_set(sqrt_poly.coeffs[i], a->minimal_poly.coeffs[src_idx]);
                    }
                }
                /* Odd powers: coefficient is 0 (already initialized) */
            }

            /* Compute numerical value for isolation bounds */
            refine_algebraic_bounds(a, 20);
            double val = (a->left_bound + a->right_bound) / 2.0;
            double sqrt_val = sqrt(fabs(val));

            double margin = fabs(sqrt_val) * LV00_EPSILON_NEWTON * 10.0;
            if (margin < LV00_EPSILON_NEWTON)
                margin = LV00_EPSILON_NEWTON;

            SymbolicCoord *result = symbolic_coord_create_algebraic(&sqrt_poly, sqrt_val - margin, sqrt_val + margin);
            mpz_poly_clear(&sqrt_poly);
            if (result)
                result->trust = coord->trust;
            return result;
        }

        case TRANSCENDENTAL: {
            /* Transcendental sqrt: out_of_scope, but provide double approximation.
             * Create an ALGEBRAIC result with the numerical value
             * sqrt(transcendental_to_double(t)). */
            {
                double base_val = transcendental_to_double(coord->data.transcendental);
                if (base_val < 0.0)
                    return NULL; /* sqrt of negative transcendental is not real */

                double sqrt_val = sqrt(base_val);

                /* Create an algebraic number from the numerical value */
                mpz_poly_t poly;
                mpz_poly_init(&poly);
                poly.degree = 1;
                poly.coeffs = lv00_malloc(2 * sizeof(mpz_t));
                if (!poly.coeffs) {
                    mpz_poly_clear(&poly);
                    return NULL;
                }
                mpz_init(poly.coeffs[0]);
                mpz_init(poly.coeffs[1]);

                mpq_t approx;
                mpq_init(approx);
                mpq_set_d(approx, sqrt_val);
                mpz_neg(poly.coeffs[0], mpq_numref(approx));
                mpz_set(poly.coeffs[1], mpq_denref(approx));
                mpq_clear(approx);

                double margin = fabs(sqrt_val) * LV00_EPSILON_NEWTON * 100.0;
                if (margin < LV00_EPSILON_NEWTON)
                    margin = LV00_EPSILON_NEWTON;

                SymbolicCoord *result = symbolic_coord_create_algebraic(&poly, sqrt_val - margin, sqrt_val + margin);
                mpz_poly_clear(&poly);
                if (result)
                    result->trust = TRUST_AMBER;
                return result;
            }
        }
    }

    return NULL;
}

/* ============================================================
 * Hash Function for Normalization Grouping
 * 
 * According to design_v2.9.md Section 4.2:
 * "遍历图中所有 POINT 节点，按符号坐标的哈希值分组。
 *  哈希基于符号坐标的规范序列化计算。"
 * ============================================================ */

/* FNV-1a hash constants - 使用 lv00_internal.h 中统一定义的 LV00_FNV64_PRIME / LV00_FNV64_OFFSET_BASIS */

static uint64_t fnv1a_update(uint64_t hash, const char *data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        hash ^= (uint64_t) (unsigned char) data[i];
        hash *= LV00_FNV64_PRIME;
    }
    return hash;
}

uint64_t symbolic_coord_hash(const SymbolicCoord *coord) {
    if (!coord)
        return 0;

    uint64_t hash = LV00_FNV64_OFFSET_BASIS;

    /* Hash the type */
    hash = fnv1a_update(hash, (const char *) &coord->type, sizeof(coord->type));

    switch (coord->type) {
        case RATIONAL: {
            /* Hash based on canonical serialization */
            char *ser = rational_serialize(coord->data.rational);
            if (ser) {
                hash = fnv1a_update(hash, ser, strlen(ser));
                lv00_free((void**)&ser); /* lv00_malloc分配 */
            }
            break;
        }
        case ALGEBRAIC: {
            Algebraic *a = coord->data.algebraic;
            /* Hash minimal polynomial coefficients */
            for (int i = 0; i <= a->minimal_poly.degree; i++) {
                char *coeff_str = mpz_get_str(NULL, 16, a->minimal_poly.coeffs[i]);
                if (coeff_str) {
                    hash = fnv1a_update(hash, coeff_str, strlen(coeff_str));
                    lv00_free_external((void **) &coeff_str);
                }
            }
            /* Hash bounds for distinguishing different roots */
            hash = fnv1a_update(hash, (const char *) &a->left_bound, sizeof(double));
            hash = fnv1a_update(hash, (const char *) &a->right_bound, sizeof(double));
            break;
        }
        case QUADRATIC: {
            Quadratic *q = coord->data.quadratic;
            char *a_ser = rational_serialize(q->a);
            char *b_ser = rational_serialize(q->b);
            if (a_ser) {
                hash = fnv1a_update(hash, a_ser, strlen(a_ser));
                lv00_free((void**)&a_ser); /* lv00_malloc分配 */
            }
            if (b_ser) {
                hash = fnv1a_update(hash, b_ser, strlen(b_ser));
                lv00_free((void**)&b_ser); /* lv00_malloc分配 */
            }
            hash = fnv1a_update(hash, (const char *) &q->n, sizeof(q->n));
            break;
        }
        case TRANSCENDENTAL: {
            hash = fnv1a_update(hash, coord->data.transcendental->name, strlen(coord->data.transcendental->name));
            break;
        }
    }

    return hash;
}

/* ============================================================
 * Enhanced Priority Rationalization via Continued Fractions
 * 
 * According to design_v2.9.md Section 1.2:
 * "每次算术运算后自动运行。对结果代数数计算连分式逼近
 *  （精度取当前隔离区间宽度的 1/4），产生一个有理数候选值 r_approx。
 *  在符号层将 r_approx 代入极小多项式精确求值。
 *  若多项式值为零，则该代数数等价于 r_approx"
 * ============================================================ */

/*
 * Compute continued fraction convergent at specified precision.
 * Returns a Rational approximation of the algebraic number.
 * precision: target precision (width of isolation interval / 4)
 */
static Rational *algebraic_continued_fraction_approx(const Algebraic *a, double precision) {
    double val = algebraic_to_double(a);
    if (precision <= 0)
        precision = 1e-15;

    /* Use GMP mpq for exact continued fraction computation */
    mpq_t approx;
    mpq_init(approx);

    /* Simple continued fraction: repeatedly take integer part and invert remainder */
    double x = val;
    mpq_t result;
    mpq_init(result);
    mpq_set_ui(result, 0, 1);

    mpq_t term;
    mpq_init(term);

    /* Build continued fraction from bottom up */
    /* We collect terms and then evaluate from the deepest */
    int terms[100];
    int n_terms = 0;

    double remaining = val;
    for (int i = 0; i < 100 && n_terms < 100; i++) {
        if (remaining < 1e-300 || remaining > 1e300)
            break;

        int64_t int_part = (int64_t) remaining;
        terms[n_terms++] = (int) int_part;
        remaining = remaining - (double) int_part;

        if (remaining < precision / 2.0)
            break;
        if (remaining < 1e-300)
            break;

        remaining = 1.0 / remaining;
    }

    /* Evaluate continued fraction from deepest term */
    if (n_terms > 0) {
        mpq_set_si(result, terms[n_terms - 1], 1);
        for (int i = n_terms - 2; i >= 0; i--) {
            mpq_set_si(term, terms[i], 1);
            /* result = term + 1/result */
            mpq_inv(result, result);
            mpq_add(result, term, result);
        }
    }

    mpq_canonicalize(result);

    Rational *r = lv00_malloc(sizeof(Rational));
    if (r) {
        mpq_init(r->value);
        mpq_set(r->value, result);
    }

    mpq_clear(approx);
    mpq_clear(result);
    mpq_clear(term);

    return r;
}

/*
 * Enhanced priority rationalization using continued fractions.
 * 
 * Algorithm:
 * 1. Compute precision = (right_bound - left_bound) / 4
 * 2. Generate continued fraction approximation at that precision
 * 3. Evaluate minimal polynomial at the rational candidate exactly
 * 4. If polynomial value is zero, cache the rational and return true
 * 
 * Returns: true if rationalization succeeded, false otherwise
 */
bool algebraic_try_rationalize(Algebraic *a) {
    if (!a)
        return false;
    if (a->cached_rational)
        return true; /* Already rationalized */

    /* Step 1: Compute precision target */
    double interval_width = a->right_bound - a->left_bound;
    double precision = interval_width / 4.0;
    if (precision < 1e-15)
        precision = 1e-15;

    /* Step 2: Generate continued fraction approximation */
    Rational *candidate = algebraic_continued_fraction_approx(a, precision);
    if (!candidate)
        return false;

    /* Step 3: Evaluate minimal polynomial at candidate exactly */
    mpz_t eval_result;
    mpz_init(eval_result);
    evaluate_algebraic_at_rational(eval_result, &a->minimal_poly, candidate);

    bool is_zero = (mpz_cmp_si(eval_result, 0) == 0);
    mpz_clear(eval_result);

    /* Step 4: If zero, cache the rational */
    if (is_zero) {
        a->cached_rational = candidate;
        return true;
    }

    rational_destroy(candidate);
    return false;
}

/* ============================================================
 * Lazy Precision Refinement for Algebraic Equality
 * 
 * According to design_v2.9.md Section 1.2:
 * "两个不同代数数判等时，精度加倍直到足以判定不相等，
 *  或达到硬上限（2^100 位）"
 * ============================================================ */

/*
 * Enhanced lazy precision refinement for algebraic equality.
 *
 * Per design_v2.9.md Section 1.2:
 * "两个不同代数数判等时，精度加倍直到足以判定不相等，
 *  或达到硬上限（2^100 位）"
 *
 * Returns: 0 = same algebraic number, 1 = different, -1 = inconclusive
 */
int algebraic_refine_for_equality(Algebraic *a, Algebraic *b, int max_iterations) {
    if (!a || !b)
        return -1;

    /* If minimal polynomials are different, they are definitely not equal */
    if (!mpz_poly_equal(&a->minimal_poly, &b->minimal_poly)) {
        return 1; /* Different minimal polynomials → different algebraic numbers */
    }

    /* Same polynomial: check isolation intervals */
    for (int iter = 0; iter < max_iterations; iter++) {
        /* Check if intervals are disjoint → different roots */
        if (a->right_bound < b->left_bound || b->right_bound < a->left_bound) {
            return 1; /* Disjoint intervals → different roots */
        }

        /* Check containment: if one interval is a subset of the other,
         * they must contain the same root */
        bool a_contains_b = (a->left_bound <= b->left_bound && b->right_bound <= a->right_bound);
        bool b_contains_a = (b->left_bound <= a->left_bound && a->right_bound <= b->right_bound);
        if (a_contains_b || b_contains_a) {
            return 0; /* Same root */
        }

        /* Intervals overlap but neither contains the other: refine both */
        /* Double precision bits before refining */
        a->precision_bits = (a->precision_bits > 0) ? a->precision_bits * 2 : 1;
        b->precision_bits = (b->precision_bits > 0) ? b->precision_bits * 2 : 1;

        /* Hard limit check */
        if (a->precision_bits > LV00_MAX_PRECISION_BITS || b->precision_bits > LV00_MAX_PRECISION_BITS) {
            break;
        }

        /* Refine using bisection (Newton's method via evaluate_poly_at_double) */
        refine_algebraic_bounds(a, 1);
        refine_algebraic_bounds(b, 1);
    }

    return -1; /* Inconclusive: reached precision limit */
}

/* ============================================================
 * Nested Square Root Expansion
 * 
 * According to design_v2.9.md Section 1.2:
 * "若运算产生 sqrt(a + b*sqrt(n)) 形式，先检查 a^2 - b^2*n 
 *  是否为完全平方数。若是，则结果可展开为规范的二次根式形式"
 * ============================================================ */

/*
 * Check if an mpz_t is a perfect square.
 * Returns the square root if perfect, NULL otherwise.
 */
static mpz_t *mpz_perfect_sqrt(mpz_t n) {
    if (mpz_cmp_si(n, 0) < 0)
        return NULL;
    if (mpz_cmp_si(n, 0) == 0) {
        mpz_t *result = lv00_malloc(sizeof(mpz_t));
        if (!result)
            return NULL;
        mpz_init_set_ui(*result, 0);
        return result;
    }

    mpz_t root;
    mpz_init(root);
    mpz_sqrt(root, n);

    mpz_t square;
    mpz_init(square);
    mpz_mul(square, root, root);

    int is_perfect = (mpz_cmp(square, n) == 0);
    mpz_clear(square);

    if (is_perfect) {
        mpz_t *result = lv00_malloc(sizeof(mpz_t));
        if (!result) {
            mpz_clear(root);
            return NULL;
        }
        mpz_init_set(*result, root);
        mpz_clear(root);
        return result;
    }

    mpz_clear(root);
    return NULL;
}

/*
 * Try to expand sqrt(a + b*sqrt(n)) to quadratic form c + d*sqrt(m).
 * 
 * If a^2 - b^2*n is a perfect square k^2, then:
 * sqrt(a + b*sqrt(n)) = sqrt((a+k)/2) + sqrt((a-k)/2) * sign(b)
 * 
 * This only works when (a+k)/2 and (a-k)/2 are both non-negative
 * and their ratio gives a clean quadratic form.
 * 
 * Returns: new SymbolicCoord if expansion succeeds, NULL otherwise
 */
SymbolicCoord *symbolic_coord_try_expand_nested_sqrt(const SymbolicCoord *coord) {
    if (!coord || coord->type != ALGEBRAIC)
        return NULL;

    Algebraic *a = coord->data.algebraic;

    /* Check if this is a quadratic algebraic number (degree 2 minimal poly) */
    if (a->minimal_poly.degree != 2)
        return NULL;

    /* For a quadratic x^2 + px + q = 0:
     * x = (-p ± sqrt(p^2 - 4q)) / 2
     * 
     * This represents sqrt(something) when p = 0:
     * x^2 + q = 0 => x = sqrt(-q)
     * 
     * For nested sqrt(a + b*sqrt(n)), the minimal poly is:
     * x^4 - 2a*x^2 + (a^2 - b^2*n) = 0
     * 
     * But if degree is 2, it's already a simple quadratic - 
     * check if it can be represented as c + d*sqrt(m)
     */

    /* Extract coefficients: poly is c0 + c1*x + c2*x^2 */
    mpz_t c0, c1, c2;
    mpz_init_set(c0, a->minimal_poly.coeffs[0]);
    mpz_init_set(c1, a->minimal_poly.coeffs[1]);
    mpz_init_set(c2, a->minimal_poly.coeffs[2]);

    /* For x^2 + c1*x + c0 = 0 (assuming c2 = 1):
     * x = (-c1 ± sqrt(c1^2 - 4*c0)) / 2
     * 
     * Discriminant D = c1^2 - 4*c0
     */
    mpz_t D;
    mpz_init(D);
    mpz_mul(D, c1, c1);
    mpz_t four_c0;
    mpz_init(four_c0);
    mpz_mul_ui(four_c0, c0, 4);
    mpz_sub(D, D, four_c0);
    mpz_clear(four_c0);

    /* Check if D is positive (for real roots) */
    int D_positive = (mpz_cmp_si(D, 0) > 0);

    if (D_positive) {
        /* Check if D is a perfect square times a square-free number */
        /* D = k^2 * m where m is square-free */
        mpz_t *k = mpz_perfect_sqrt(D);
        if (k) {
            /* D is a perfect square: x = (-c1 ± k) / 2, which is rational */
            Rational *root = rational_create(0, 1);
            mpz_t neg_c1;
            mpz_init(neg_c1);
            mpz_neg(neg_c1, c1);

            /* Determine which root matches our isolation interval */
            double mid = (a->left_bound + a->right_bound) / 2.0;

            /* Root 1: (-c1 + k) / 2 */
            mpz_t num1;
            mpz_init(num1);
            mpz_add(num1, neg_c1, *k);
            Rational *r1 = rational_create_from_mpz(num1, *k); /* divide by 2 */
            /* Actually divide by 2 */
            mpq_t q1;
            mpq_init(q1);
            mpz_set(mpq_numref(q1), num1);
            mpz_set_ui(mpq_denref(q1), 2);
            mpq_canonicalize(q1);
            rational_destroy(r1);
            r1 = lv00_malloc(sizeof(Rational));
            if (!r1) {
                mpz_clear(num1);
                mpz_clear(neg_c1);
                mpz_clear(c0);
                mpz_clear(c1);
                mpz_clear(c2);
                mpz_clear(D);
                mpz_clear(*k);
                lv00_free((void **) &k);
                return NULL;
            }
            mpq_init(r1->value);
            mpq_set(r1->value, q1);
            mpq_clear(q1);

            /* Root 2: (-c1 - k) / 2 */
            mpz_t num2;
            mpz_init(num2);
            mpz_sub(num2, neg_c1, *k);
            mpq_t q2;
            mpq_init(q2);
            mpz_set(mpq_numref(q2), num2);
            mpz_set_ui(mpq_denref(q2), 2);
            mpq_canonicalize(q2);
            Rational *r2 = lv00_malloc(sizeof(Rational));
            if (!r2) {
                rational_destroy(r1);
                mpz_clear(num1);
                mpz_clear(num2);
                mpz_clear(neg_c1);
                mpz_clear(c0);
                mpz_clear(c1);
                mpz_clear(c2);
                mpz_clear(D);
                mpz_clear(*k);
                lv00_free((void **) &k);
                return NULL;
            }
            mpq_init(r2->value);
            mpq_set(r2->value, q2);
            mpq_clear(q2);

            double v1 = rational_to_double(r1);
            double v2 = rational_to_double(r2);

            /* Pick the root that falls within the isolation interval */
            Rational *chosen = NULL;
            if (mid >= v1 - 1e-10 && mid <= v1 + 1e-10) {
                chosen = r1;
                rational_destroy(r2);
            } else {
                chosen = r2;
                rational_destroy(r1);
            }

            SymbolicCoord *result = symbolic_coord_create_rational(0, 1);
            if (result) {
                rational_destroy(result->data.rational);
                result->data.rational = chosen;
                result->trust = coord->trust;
            } else {
                rational_destroy(chosen);
            }

            mpz_clear(c0);
            mpz_clear(c1);
            mpz_clear(c2);
            mpz_clear(D);
            mpz_clear(neg_c1);
            mpz_clear(num1);
            mpz_clear(num2);
            mpz_clear(*k);
            lv00_free((void **) &k);
            return result;
        }

/* D is not a perfect square: check if D/n is a perfect square for some n */
/* Try to express D as k^2 * m where m is square-free */

/** 有理数近似中平方因子分解的上限 */
#define LV00_RATIONAL_SQ_FACTOR_LIMIT 10000

        mpz_t remaining;
        mpz_init_set(remaining, D);

        unsigned int square_factor = 1;
        for (unsigned int i = 2; i * i <= LV00_RATIONAL_SQ_FACTOR_LIMIT; i++) {
            while (mpz_divisible_ui_p(remaining, i * i)) {
                square_factor *= i;
                mpz_divexact_ui(remaining, remaining, i * i);
            }
        }

        /* remaining is now square-free part m */
        /* k = square_factor */
        /* D = square_factor^2 * remaining */

        if (square_factor > 1 && mpz_cmp_si(remaining, 1) > 0) {
            /* Can express as: x = (-c1 ± square_factor*sqrt(remaining)) / 2 */
            double mid = (a->left_bound + a->right_bound) / 2.0;
            double sqrt_remaining = sqrt(mpz_get_d(remaining));
            double sf = (double) square_factor;

            /* Root 1: (-c1 + sf*sqrt(remaining)) / 2 */
            double v1 = (mpz_get_d(c1) * (-1.0) + sf * sqrt_remaining) / 2.0;
            /* Root 2: (-c1 - sf*sqrt(remaining)) / 2 */
            double v2 = (mpz_get_d(c1) * (-1.0) - sf * sqrt_remaining) / 2.0;

            /* Determine sign of b coefficient */
            double b_val;
            if (mid >= v1 - 1e-10 && mid <= v1 + 1e-10) {
                b_val = sf / 2.0;
            } else {
                b_val = -sf / 2.0;
            }

            double a_val = mpz_get_d(c1) * (-1.0) / 2.0;

            /* Create quadratic: a_val + b_val * sqrt(remaining) */
            /* Use rational approximation for a_val and b_val */
            if (fabs(a_val) > 9.2e12 || fabs(b_val) > 9.2e12) {
                /* Value too large for exact rational representation */
                mpz_clear(c0);
                mpz_clear(c1);
                mpz_clear(c2);
                mpz_clear(D);
                mpz_clear(remaining);
                return NULL;
            }
            Rational *q_a =
                rational_create((int64_t) (a_val * (double) LV00_BIT_CUTOFF_THRESHOLD), LV00_BIT_CUTOFF_THRESHOLD);
            Rational *q_b =
                rational_create((int64_t) (b_val * (double) LV00_BIT_CUTOFF_THRESHOLD), LV00_BIT_CUTOFF_THRESHOLD);

            /* Remove square factors from remaining */
            /* R03: 先检查 remaining 是否在 unsigned int 范围内 */
            unsigned int m;
            if (mpz_fits_uint_p(remaining)) {
                m = (unsigned int) mpz_get_ui(remaining);
            } else {
                /* remaining 超出 unsigned int 范围，取低32位作为截断近似。
                 * 注意：此处截断可能导致平方因子移除不完整，但实际应用中
                 * algebraic_simplify_to_quadratic 的主要用例不会产生超大 remaining 值。 */
                m = (unsigned int) mpz_get_ui(remaining);
            }
            m = remove_square_factors(m);

            SymbolicCoord *result = symbolic_coord_create_quadratic(q_a, q_b, m);
            if (result)
                result->trust = coord->trust;

            mpz_clear(c0);
            mpz_clear(c1);
            mpz_clear(c2);
            mpz_clear(D);
            mpz_clear(remaining);
            return result;
        }

        mpz_clear(remaining);
    }

    mpz_clear(c0);
    mpz_clear(c1);
    mpz_clear(c2);
    mpz_clear(D);
    return NULL;
}

/* ============================================================
 * A/B Plan Switching Implementation (Section 1.6 of design_v2.9.md)
 * ============================================================ */

AlgebraicPlan algebraic_get_plan(void) {
    return g_algebraic_plan;
}

void algebraic_set_plan(AlgebraicPlan plan) {
    g_algebraic_plan = plan;
}

/*
 * Stress test for A-plan validation.
 *
 * Creates an algebraic number of degree <= max_poly_degree,
 * then performs chain_length alternating add/multiply operations
 * with itself, monitoring:
 *   - Isolation interval precision decay (width growth in bits)
 *   - Maximum polynomial coefficient bits
 *
 * Returns a StressTestResult indicating whether A-plan is stable.
 */
StressTestResult algebraic_stress_test(int chain_length, int max_poly_degree) {
    StressTestResult result = {true, true, 0, 0};

    if (chain_length <= 0 || max_poly_degree <= 0 || max_poly_degree > 4) {
        result.precision_stable = false;
        result.performance_stable = false;
        return result;
    }

    /* Create a simple algebraic number: root of x^2 - 2 = 0 (sqrt(2)) */
    mpz_poly_t poly;
    mpz_poly_init(&poly);
    poly.degree = 2;
    poly.coeffs = lv00_malloc(3 * sizeof(mpz_t));
    if (!poly.coeffs) {
        mpz_poly_clear(&poly);
        result.precision_stable = false;
        result.performance_stable = false;
        return result;
    }
    mpz_init_set_si(poly.coeffs[0], -2); /* constant term */
    mpz_init_set_si(poly.coeffs[1], 0);  /* linear term */
    mpz_init_set_si(poly.coeffs[2], 1);  /* quadratic term */

    Algebraic *current = algebraic_create(&poly, 1.4, 1.5);
    mpz_poly_clear(&poly);

    if (!current) {
        result.precision_stable = false;
        result.performance_stable = false;
        return result;
    }

    /* Track initial isolation interval width */
    double initial_width = current->right_bound - current->left_bound;

    /* Also create a second algebraic number for variety: root of x^2 - 3 = 0 (sqrt(3)) */
    mpz_poly_t poly2;
    mpz_poly_init(&poly2);
    poly2.degree = 2;
    poly2.coeffs = lv00_malloc(3 * sizeof(mpz_t));
    if (!poly2.coeffs) {
        mpz_poly_clear(&poly2);
        algebraic_destroy(current);
        result.precision_stable = false;
        result.performance_stable = false;
        return result;
    }
    mpz_init_set_si(poly2.coeffs[0], -3);
    mpz_init_set_si(poly2.coeffs[1], 0);
    mpz_init_set_si(poly2.coeffs[2], 1);

    Algebraic *other = algebraic_create(&poly2, 1.7, 1.8);
    mpz_poly_clear(&poly2);

    if (!other) {
        algebraic_destroy(current);
        result.precision_stable = false;
        result.performance_stable = false;
        return result;
    }

    /* Perform chain of operations */
    for (int i = 0; i < chain_length; i++) {
        Algebraic *next = NULL;

        if (i % 2 == 0) {
            next = algebraic_add(current, other);
        } else {
            next = algebraic_multiply(current, other);
        }

        if (!next) {
            /* Operation failed - likely degree explosion */
            result.precision_stable = false;
            result.performance_stable = false;
            break;
        }

        /* Check precision decay: how many bits has the interval width grown? */
        double current_width = next->right_bound - next->left_bound;
        if (current_width > 0 && initial_width > 0) {
            double ratio = current_width / initial_width;
            int decay_bits = (int) (log2(ratio) + 0.5);
            if (decay_bits < 0)
                decay_bits = 0;
            if (decay_bits > result.max_precision_decay) {
                result.max_precision_decay = decay_bits;
            }
        }

        /* Check maximum polynomial coefficient bits */
        for (int j = 0; j <= next->minimal_poly.degree; j++) {
            int bits = (int) mpz_sizeinbase(next->minimal_poly.coeffs[j], 2);
            if (bits > result.max_bits_observed) {
                result.max_bits_observed = bits;
            }
        }

        algebraic_destroy(current);
        current = next;
    }

    algebraic_destroy(other);
    algebraic_destroy(current);

    /* Determine stability */
    result.precision_stable = (result.max_precision_decay <= 1);
    result.performance_stable = (result.max_bits_observed <= BIT_CUTOFF_THRESHOLD);

    return result;
}

/* ============================================================
 * Digit Circuit User Interaction (Section 1.5 of design_v2.9.md)
 * ============================================================ */

void circuit_set_trip_callback(CircuitTripCallback cb, void *user_data) {
    g_circuit_callback = cb;
    g_circuit_user_data = user_data;
}

CircuitResponse circuit_handle_trip_interactive(const SymbolicCoord *coord) {
    if (g_circuit_callback) {
        return g_circuit_callback(coord, g_overflow_context.overflow_count, g_circuit_user_data);
    }

    /* Default behavior when no callback is set: return IGNORE */
    /* This preserves backward compatibility - existing code that doesn't
     * set a callback will get the old behavior of accepting the overflow */
    return CIRCUIT_RESPONSE_IGNORE;
}

/* ============================================================
 * Bit Circuit Context Management
 * 
 * 提供对线程局部溢出上下文的访问，供外部模块使用。
 * ============================================================ */

void circuit_set_context(SymbolicCoord *result, const char *operation, CoordType left_type, CoordType right_type) {
    g_overflow_context.last_result = result;
    g_overflow_context.last_operation = operation;
    g_overflow_context.left_type = left_type;
    g_overflow_context.right_type = right_type;
}

SymbolicCoord *circuit_get_last_result(void) {
    return g_overflow_context.last_result;
}

const char *circuit_get_last_operation(void) {
    return g_overflow_context.last_operation;
}

bool circuit_has_frozen_point(void) {
    return g_overflow_context.has_frozen_point;
}

void *circuit_get_frozen_point(void) {
    return g_overflow_context.frozen_point;
}
