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

#include "lv/symbolic_coord.h"

#include <gmp.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/debug.h"
#include "lv/lv_internal.h"
#include "lv/lv_arith_safe.h" /* lv_squarefree_i64（K2/F33 收敛权威） */
#include "lv/lv_numeric.h" /* lv_mpz_bit_size（K63/F89 位数检查权威） */
#include "lv/lv_utils.h" /* 提供 lv_malloc/lv_free/lv_strdup */
#include "lv/mpz_poly.h"

/*
 * K63/F89：位数熔断与精度上限为项目级常量——权威定义于 config.h
 * 「根/位/降级」段：lv_BIT_CUTOFF_THRESHOLD（1000000）、lv_MAX_PRECISION_BITS（100）。
 * 早期注释声称定义于 lv_internal.h 已过期（事实核查不在），此处不再重复定义。
 */

/* ============================================================
 * 文件级常量定义
 * ============================================================ */

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
lv_THREAD_LOCAL struct OverflowContext g_overflow_context = {.last_result = NULL,
                                                             .last_operation = NULL,
                                                             .left_type = RATIONAL,
                                                             .right_type = RATIONAL,
                                                             .overflow_count = 0,
                                                             .frozen_point = NULL,
                                                             .has_frozen_point = false};

/* ============================================================
 * A/B Plan switching (Section 1.6 of design_v2.9.md)
 * NOTE: 线程局部存储，每个线程可独立设置计划
 * ============================================================ */
static lv_THREAD_LOCAL AlgebraicPlan g_algebraic_plan = PLAN_A_FULL_ALGEBRAIC;

/* ============================================================
 * Digit circuit user interaction (Section 1.5 of design_v2.9.md)
 * NOTE: 线程局部存储，每个线程可独立设置回调
 * ============================================================ */
static lv_THREAD_LOCAL CircuitTripCallback g_circuit_callback = NULL;
static lv_THREAD_LOCAL void *g_circuit_user_data = NULL;

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

/* ── 四类型操作已拆分至 symbolics/rational.c, algebraic.c, quadratic.c, transcendental.c, symbolic_coord_ops.c ── */
/* ── 电路与高级操作 ── */

/**
 * 获取当前线程的代数计划（A/B-Plan）。
 *
 * A/B 计划机制（design_v2.9.md Section 1.6）：
 * - PLAN_A_FULL_ALGEBRAIC：全程使用符号代数精确计算
 * - PLAN_B_MIXED：允许在精度可接受时回退到数值近似
 *
 * @return 当前代数计划枚举值
 */
AlgebraicPlan algebraic_get_plan(void) {
    return g_algebraic_plan;
}

/**
 * 设置当前线程的代数计划。
 *
 * 线程局部存储，每个线程可独立设置计划。
 * 通常在求解器入口处根据问题规模和用户配置选择计划。
 *
 * @param plan 要设置的代数计划
 */
void algebraic_set_plan(AlgebraicPlan plan) {
    g_algebraic_plan = plan;
}

/*
 * Stress test for A-plan validation.
 *
 * Creates algebraic numbers of varying degrees up to max_poly_degree,
 * then performs chain_length alternating add/multiply operations
 * with them, monitoring:
 *   - Isolation interval precision decay (width growth in bits)
 *   - Maximum polynomial coefficient bits
 *
 * The test uses 2 algebraic numbers:
 *   - degree=2: sqrt(2) and sqrt(3) (always created)
 *   - degree=3: root of x³-2 and root of x³-3 (if max_poly_degree >= 3)
 *   - degree=4: root of x⁴-2 and root of x⁴-3 (if max_poly_degree >= 4)
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

    /*
     * 根据 max_poly_degree 选择测试多项式的次数。
     * 默认使用 degree=2（sqrt(2), sqrt(3)），
     * 若 max_poly_degree >= 3 则增加 cubic，>= 4 则增加 quartic。
     * 测试始终使用 sqrt(2) 和 sqrt(3) 作为基准操作数（common case），
     * 因为它们是代数几何中最常见的二次无理数类型。
     */
    int test_degree = (max_poly_degree >= 4) ? 4 : (max_poly_degree >= 3) ? 3 : 2;

    /* Create test polynomials */
    /* 主操作数: root of x² - 2 = 0 (sqrt(2)) */
    mpz_poly_t poly;
    mpz_poly_init(&poly);
    poly.degree = test_degree;
    int coeff_count = test_degree + 1;
    poly.coeffs = lv_malloc((size_t) coeff_count * sizeof(mpz_t));
    if (!poly.coeffs) {
        mpz_poly_clear(&poly);
        result.precision_stable = false;
        result.performance_stable = false;
        return result;
    }
    for (int k = 0; k < coeff_count; k++)
        mpz_init(poly.coeffs[k]);
    mpz_set_si(poly.coeffs[0], -2); /* constant term: -2 */
    /* intermediate coefficients = 0 (already initialized) */
    mpz_set_si(poly.coeffs[test_degree], 1); /* leading coefficient: 1 */

    Algebraic *current = algebraic_create(&poly, 1.4, 1.5);
    mpz_poly_clear(&poly);

    if (!current) {
        result.precision_stable = false;
        result.performance_stable = false;
        return result;
    }

    /* Track initial isolation interval width */
    double initial_width = current->right_bound - current->left_bound;

    /* 辅助操作数: root of x^test_degree - 3 = 0 */
    mpz_poly_t poly2;
    mpz_poly_init(&poly2);
    poly2.degree = test_degree;
    poly2.coeffs = lv_malloc((size_t) coeff_count * sizeof(mpz_t));
    if (!poly2.coeffs) {
        mpz_poly_clear(&poly2);
        algebraic_destroy(current);
        result.precision_stable = false;
        result.performance_stable = false;
        return result;
    }
    for (int k = 0; k < coeff_count; k++)
        mpz_init(poly2.coeffs[k]);
    mpz_set_si(poly2.coeffs[0], -3);
    mpz_set_si(poly2.coeffs[test_degree], 1);

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
            int decay_bits = (int) round(log2(ratio));
            if (decay_bits < 0)
                decay_bits = 0;
            if (decay_bits > result.max_precision_decay) {
                result.max_precision_decay = decay_bits;
            }
        }

        /* Check maximum polynomial coefficient bits */
        for (int j = 0; j <= next->minimal_poly.degree; j++) {
            int bits = (int) lv_mpz_bit_size(next->minimal_poly.coeffs[j]);
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
    result.performance_stable = (result.max_bits_observed <= lv_BIT_CUTOFF_THRESHOLD);

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

/* symbolics/ 子目录函数由 algebraic.c / quadratic.c / transcendental.c 提供 */

bool is_rational_zero(const Rational *r) {
    if (!r)
        return true;
    return mpq_sgn(r->value) == 0;
}

/**
 * 移除整数 n 中的所有完全平方因子。
 *
 * 例如：remove_square_factors(72) = remove_square_factors(2³×3²) = 2
 *       因为 72 = (2×3)² × 2，去掉 6² 剩下 2。
 *
 * 使用 int64_t 避免大整数溢出：n 可能来自代数化简中的系数，
 * 在极端情况下可达 10⁹ 量级，int 在 32 位平台上仅有 2×10⁹ 范围。
 *
 * 【K2/F33 收敛】本函数改为委托 lv_arith_safe.h 的 lv_squarefree_i64
 * 单一权威（合并 symbolic_coord.c / quadratic.c / algebraic.c 三处实现）。
 *
 * @param n 要处理的整数
 * @return 移除所有平方因子后的结果（n 的无平方部分）
 */
int64_t remove_square_factors(int64_t n) {
    return lv_squarefree_i64(n);
}
