/**
 * @file symbolic_coord_trust.c
 * @brief SymbolicCoord 信任颜色管理、哈希、A/B 自动降级系统
 *
 * @details 实现：
 *          - 信任颜色查询与设置 (get_trust / set_trust / is_amber / downgrade_to_amber)
 *          - 信任颜色组合规则 (trust_color_combine)
 *          - FNV-1a 哈希 (symbolic_coord_hash)
 *          - 连分数有理化 (algebraic_try_rationalize)
 *          - A/B 计划管理与自动降级 (set_plan / get_plan / auto_degrade / create_with_plan)
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "lv/lv_platform.h"
#include "lv/lv_mempool_utils.h"

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/bit_burning.h"
#include "lv/constraint_graph.h"
#include "lv/symbolic_coord.h"

#include "debug.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "mpz_poly.h"

/* ── 前向声明（来自 symbolics 子目录其他模块）── */
double algebraic_to_double(const Algebraic *a);
double rational_to_double(const Rational *r);
bool is_rational_zero(const Rational *r);

/* ── 降级统计（线程局部）── */
#define SYM_COORD_DEGRADE_THRESHOLD 3
#define SYM_COORD_ALGEBRAIC_BIT_LIMIT_FACTOR 10

static lv_THREAD_LOCAL int g_degrade_fail_count = 0;
static lv_THREAD_LOCAL int g_degrade_total = 0;

/* ============================================================
 * Trust Color Functions
 * ============================================================ */

/**
 * 将符号坐标降级为 AMBER 信任级别。
 */
SymbolicCoord *symbolic_coord_downgrade_to_amber(const SymbolicCoord *coord, double precision,
                                                 const char *declaration) {
    if (!coord)
        return NULL;

    SymbolicCoord *result = symbolic_coord_copy(coord);
    if (!result)
        return NULL;

    result->trust = TRUST_AMBER;

    fprintf(stderr, "[AMBER DOWNGRADE] Precision: %.15g, Declaration: %s\n", precision,
            declaration ? declaration : "(none)");

    double numerical_value = symbolic_coord_to_double(result);
    fprintf(stderr, "[AMBER DOWNGRADE] Numerical value: %.15g\n", numerical_value);

    return result;
}

/**
 * 检查符号坐标是否标记为 AMBER。
 */
bool symbolic_coord_is_amber(const SymbolicCoord *coord) {
    if (!coord)
        return false;
    return coord->trust == TRUST_AMBER;
}

/**
 * 获取符号坐标的信任颜色。
 */
TrustColor symbolic_coord_get_trust(const SymbolicCoord *coord) {
    if (!coord)
        return TRUST_GREEN;
    return coord->trust;
}

/**
 * 设置符号坐标的信任颜色。
 */
void symbolic_coord_set_trust(SymbolicCoord *coord, TrustColor trust) {
    if (coord) {
        coord->trust = trust;
        symbolic_coord_invalidate_cache(coord);
    }
}

/* ============================================================
 * Trust Color Combination
 * ============================================================ */

TrustColor trust_color_combine(TrustColor a, TrustColor b) {
    int a_val = (int) a;
    int b_val = (int) b;

    int is_a_lo = (a_val == TRUST_LIGHT_ORANGE_ORACLE || a_val == TRUST_LIGHT_ORANGE_EXPLOSION);
    int is_b_lo = (b_val == TRUST_LIGHT_ORANGE_ORACLE || b_val == TRUST_LIGHT_ORANGE_EXPLOSION);

    if ((is_a_lo && b_val == TRUST_AMBER) || (is_b_lo && a_val == TRUST_AMBER)) {
        return TRUST_DEEP_ORANGE;
    }

    return (a_val > b_val) ? a : b;
}

/* ============================================================
 * Hash Function for Normalization Grouping
 * ============================================================ */

uint64_t symbolic_coord_hash(const SymbolicCoord *coord) {
    if (!coord)
        return 0;

    uint64_t hash = lv_FNV64_OFFSET_BASIS;

    /* Hash the type */
    hash = lv_fnv1a_update(hash, (const char *) &coord->type, sizeof(coord->type));

    switch (coord->type) {
        case RATIONAL: {
            char *ser = rational_serialize(coord->data.rational);
            if (ser) {
                hash = lv_fnv1a_update(hash, ser, strlen(ser));
                lv_free((void **) &ser);
            }
            break;
        }
        case ALGEBRAIC: {
            Algebraic *a = coord->data.algebraic;
            for (int i = 0; i <= a->minimal_poly.degree; i++) {
                char *coeff_str = mpz_get_str(NULL, 16, a->minimal_poly.coeffs[i]);
                if (coeff_str) {
                    hash = lv_fnv1a_update(hash, coeff_str, strlen(coeff_str));
                    lv_free_external((void **) &coeff_str);
                }
            }
            hash = lv_fnv1a_update(hash, (const char *) &a->left_bound, sizeof(double));
            hash = lv_fnv1a_update(hash, (const char *) &a->right_bound, sizeof(double));
            break;
        }
        case QUADRATIC: {
            Quadratic *q = coord->data.quadratic;
            char *a_ser = rational_serialize(q->a);
            char *b_ser = rational_serialize(q->b);
            if (a_ser) {
                hash = lv_fnv1a_update(hash, a_ser, strlen(a_ser));
                lv_free((void **) &a_ser);
            }
            if (b_ser) {
                hash = lv_fnv1a_update(hash, b_ser, strlen(b_ser));
                lv_free((void **) &b_ser);
            }
            hash = lv_fnv1a_update(hash, (const char *) &q->n, sizeof(q->n));
            break;
        }
        case TRANSCENDENTAL: {
            hash = lv_fnv1a_update(hash, coord->data.transcendental->name, strlen(coord->data.transcendental->name));
            break;
        }
    }

    return hash;
}

/* ============================================================
 * Continued Fraction Rationalization
 * ============================================================ */

/*
 * Compute continued fraction convergent at specified precision.
 */
static Rational *algebraic_continued_fraction_approx(const Algebraic *a, double precision) {
    double val = algebraic_to_double(a);
    if (precision <= 0)
        precision = 1e-15;

    mpq_t approx;
    mpq_init(approx);

    double x = val;
    mpq_t result;
    mpq_init(result);
    mpq_set_ui(result, 0, 1);

    mpq_t term;
    mpq_init(term);

    int terms[100];
    int n_terms = 0;

    double remaining = val;
    for (int i = 0; i < 100 && n_terms < 100; i++) {
        if (remaining < 1e-300 || remaining > 1e300)
            break;

        int64_t int_part = (int64_t) remaining;
        if (n_terms >= 100)
            break;
        terms[n_terms++] = (int) int_part;
        remaining = remaining - (double) int_part;

        if (remaining < precision / 2.0)
            break;
        if (remaining < 1e-300)
            break;

        remaining = 1.0 / remaining;
    }

    if (n_terms > 0) {
        mpq_set_si(result, terms[n_terms - 1], 1);
        for (int i = n_terms - 2; i >= 0; i--) {
            mpq_set_si(term, terms[i], 1);
            if (mpq_sgn(result) == 0) {
                mpq_set(result, term);
            } else {
                mpq_inv(result, result);
                mpq_add(result, term, result);
            }
        }
    }

    mpq_canonicalize(result);

    Rational *r = lv_calloc(1, sizeof(Rational));
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
 */
bool algebraic_try_rationalize(Algebraic *a) {
    if (!a)
        return false;

    double span = a->right_bound - a->left_bound;
    if (span <= 0.0)
        return false;

    double precision = span / 4.0;

    Rational *candidate = algebraic_continued_fraction_approx(a, precision);
    if (!candidate)
        return false;

    int deg = a->minimal_poly.degree;

    mpz_t mpz_one;
    mpz_init_set_ui(mpz_one, 1);

    Rational *result = rational_create_from_mpz(a->minimal_poly.coeffs[0], mpz_one);
    if (!result) {
        mpz_clear(mpz_one);
        rational_destroy(candidate);
        return false;
    }

    Rational *power = rational_create(1, 1);
    if (!power) {
        mpz_clear(mpz_one);
        rational_destroy(result);
        rational_destroy(candidate);
        return false;
    }

    for (int i = 1; i <= deg; i++) {
        Rational *new_power = rational_multiply(power, candidate);
        rational_destroy(power);
        power = new_power;

        Rational *coeff_r = rational_create_from_mpz(a->minimal_poly.coeffs[i], mpz_one);
        Rational *term = rational_multiply(coeff_r, power);
        rational_destroy(coeff_r);

        Rational *new_result = rational_add(result, term);
        rational_destroy(result);
        rational_destroy(term);
        result = new_result;
    }

    mpz_clear(mpz_one);
    rational_destroy(power);

    if (is_rational_zero(result)) {
        if (a->cached_rational)
            rational_destroy(a->cached_rational);
        a->cached_rational = candidate;
        rational_destroy(result);
        return true;
    }

    rational_destroy(result);
    rational_destroy(candidate);
    return false;
}

/* ============================================================
 * A/B 自动降级系统
 * ============================================================ */

/**
 * @brief 降级辅助函数：检查代数数运算结果是否需要降级
 */
SymbolicCoord *_symbolic_coord_degrade_check_algebraic(SymbolicCoord *result) {
    if (!result || result->type != ALGEBRAIC)
        return result;

    if (algebraic_get_plan() != PLAN_A_FULL_ALGEBRAIC)
        return result;

    Algebraic *a = result->data.algebraic;
    if (!a)
        return result;

    int deg = a->minimal_poly.degree;

    bool needs_check = false;
    if (deg > 2) {
        needs_check = true;
    } else {
        size_t max_bits = 0;
        for (int i = 0; i <= deg; i++) {
            size_t bits = mpz_sizeinbase(a->minimal_poly.coeffs[i], 2);
            if (bits > max_bits)
                max_bits = bits;
        }
        if (max_bits > (size_t) (lv_BIT_CUTOFF_THRESHOLD / SYM_COORD_ALGEBRAIC_BIT_LIMIT_FACTOR)) {
            needs_check = true;
        }
    }

    if (!needs_check)
        return result;

    /* 1. 尝试有理化 */
    algebraic_try_rationalize(a);
    if (a->cached_rational) {
        SymbolicCoord *new_result = symbolic_coord_create_rational(0, 1);
        if (new_result) {
            rational_destroy(new_result->data.rational);
            new_result->data.rational = rational_copy(a->cached_rational);
            new_result->trust = result->trust;
            symbolic_coord_destroy(result);
            return new_result;
        }
    }

    /* 2. 如果 degree == 2，尝试转换为二次根式形式 */
    if (deg == 2) {
        mpz_t *c0 = &a->minimal_poly.coeffs[0];
        mpz_t *c1 = &a->minimal_poly.coeffs[1];
        mpz_t *c2 = &a->minimal_poly.coeffs[2];

        if (mpz_cmp_si(*c1, 0) == 0 && mpz_sgn(*c0) < 0 && mpz_sgn(*c2) > 0) {
            mpz_t n_num;
            mpz_init(n_num);
            mpz_neg(n_num, *c0);
            if (mpz_divisible_p(n_num, *c2) && mpz_sgn(n_num) > 0) {
                mpz_divexact(n_num, n_num, *c2);
                if (mpz_fits_uint_p(n_num)) {
                    unsigned long n_val = mpz_get_ui(n_num);
                    if (n_val > 0) {
                        double a_val = algebraic_to_double(a);
                        Rational *zero = rational_create(0, 1);
                        Rational *one = rational_create(1, 1);
                        unsigned int n_ui = (unsigned int) n_val;

                        SymbolicCoord *new_result = symbolic_coord_create_quadratic(zero, one, n_ui);
                        if (new_result) {
                            if (a_val < 0) {
                                Rational *neg_one = rational_create(-1, 1);
                                rational_destroy(new_result->data.quadratic->b);
                                new_result->data.quadratic->b = neg_one;
                            }
                            new_result->trust = result->trust;
                            symbolic_coord_destroy(result);
                            mpz_clear(n_num);
                            return new_result;
                        }
                        rational_destroy(zero);
                        rational_destroy(one);
                    }
                }
            }
            mpz_clear(n_num);
        }

        {
            g_degrade_fail_count++;
        }
        return result;
    }

    /* 3. 对于 degree > 2：无法化简，触发自动降级 */
    symbolic_coord_auto_degrade("algebraic result degree > 2, exceeds A-plan threshold");

    return result;
}

/**
 * @brief 设置坐标系统当前代数计划
 */
void symbolic_coord_set_plan(AlgebraicPlan plan) {
    algebraic_set_plan(plan);
    g_degrade_fail_count = 0;
}

/**
 * @brief 获取当前代数计划
 */
AlgebraicPlan symbolic_coord_get_plan(void) {
    return algebraic_get_plan();
}

/**
 * @brief 自动降级决策
 */
bool symbolic_coord_auto_degrade(const char *reason) {
    AlgebraicPlan current = algebraic_get_plan();
    (void) reason;

    if (current == PLAN_C_RATIONAL_ONLY) {
        g_degrade_fail_count = 0;
        return false;
    }

    g_degrade_fail_count++;

    if (g_degrade_fail_count >= SYM_COORD_DEGRADE_THRESHOLD) {
        AlgebraicPlan new_plan;
        switch (current) {
            case PLAN_A_FULL_ALGEBRAIC:
                new_plan = PLAN_B_QUADRATIC_ONLY;
                break;
            case PLAN_B_QUADRATIC_ONLY:
                new_plan = PLAN_C_RATIONAL_ONLY;
                break;
            default:
                new_plan = current;
                break;
        }

        if (new_plan != current) {
            algebraic_set_plan(new_plan);
            g_degrade_total++;
        }

        g_degrade_fail_count = 0;
        return true;
    }

    return false;
}

/**
 * @brief 创建符号坐标（带计划感知）
 */
SymbolicCoord *symbolic_coord_create_with_plan(long num, long den) {
    AlgebraicPlan plan = algebraic_get_plan();

    switch (plan) {
        case PLAN_A_FULL_ALGEBRAIC: {
            Rational *r = rational_create((int64_t) num, (uint64_t) den);
            if (!r)
                return NULL;

            Algebraic *alg = algebraic_from_rational(r);
            rational_destroy(r);

            if (alg) {
                SymbolicCoord *result = lv_calloc(1, sizeof(SymbolicCoord));
                if (!result) {
                    algebraic_destroy(alg);
                    return NULL;
                }
                result->type = ALGEBRAIC;
                result->trust = TRUST_GREEN;
                result->cache_valid = false;
                result->cached_value = 0.0;
                result->algebraic_info = NULL;
                result->data.algebraic = alg;
                return result;
            }

            symbolic_coord_auto_degrade("algebraic_from_rational failed");
            __attribute__((fallthrough));
        }
        case PLAN_B_QUADRATIC_ONLY: {
            Rational *a = rational_create((int64_t) num, (uint64_t) den);
            Rational *b = rational_create(0, 1);
            SymbolicCoord *result = symbolic_coord_create_quadratic(a, b, 1);
            if (!result) {
                rational_destroy(a);
                rational_destroy(b);
            }
            return result;
        }
        case PLAN_C_RATIONAL_ONLY:
        default:
            return symbolic_coord_create_rational((int64_t) num, (uint64_t) den);
    }
}

/**
 * @brief 检测代数表达式是否为二次根式形式
 */
bool symbolic_coord_is_quadratic_form(const char *expr) {
    if (!expr || expr[0] == '\0')
        return false;

    bool has_sqrt = (strstr(expr, "sqrt") != NULL);
    bool has_radical = (strstr(expr, "√") != NULL);

    if (!has_sqrt && !has_radical)
        return false;

    const char *first;
    if (has_sqrt) {
        first = strstr(expr, "sqrt");
        if (first) {
            const char *second = strstr(first + 4, "sqrt");
            if (second)
                return false;
        }
    } else {
        first = strstr(expr, "√");
        if (first) {
            const char *second = strstr(first + 3, "√");
            if (second)
                return false;
        }
    }

    if (strstr(expr, "^3") || strstr(expr, "^4") || strstr(expr, "cbrt") || strstr(expr, "cubic"))
        return false;

    return true;
}

/**
 * @brief 获取降级统计信息
 */
void symbolic_coord_plan_stats(int *out_total, AlgebraicPlan *out_current) {
    if (out_total)
        *out_total = g_degrade_total;
    if (out_current)
        *out_current = algebraic_get_plan();
}
