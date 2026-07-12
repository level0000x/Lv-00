/**
 * @file rational.c
 * @brief Rational 有理数类型
 *
 * @details 拆分子模块（Lv-00 v3.3.0+）。
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "lv00/symbolic_coord.h"
#include <float.h>
#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lv00/constraint_graph.h"
#include "debug.h"
#include "lv00_internal.h"
#include "lv00_utils.h"
#include "mpz_poly.h"

#define SYM_COORD_DYNAMIC_ARRAY_INIT_CAP 16
#define SYM_COORD_SIGFIGS_MIN_SAFE 6
#define SYM_COORD_SIGFIGS_APPROX 4
#define SYM_COORD_EPS 1e-8
#define SYM_COORD_MAX_REFINE 15
#define SYM_COORD_AMB_MIN_SIGFIGS 3
#define COORD_SEVEN_OVER_FIVE_N 32
/* ── Rational type ── */

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
double rational_to_double(const Rational *r) {
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
        default:
            return CIRCUIT_STATUS_OK; /* 未知类型视为安全 */
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
/* ── Circuit (rational helpers) ── */

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
        default:
            return CIRCUIT_STATUS_OK; /* 未知类型视为安全 */
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
