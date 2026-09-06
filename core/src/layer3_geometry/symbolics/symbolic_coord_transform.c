/**
 * @file symbolic_coord_transform.c
 * @brief SymbolicCoord 一元变换操作：取负、幂、平方根
 *
 * @details 实现 RATIONAL / QUADRATIC / ALGEBRAIC / TRANSCENDENTAL
 *          四种符号坐标类型的一元变换操作。
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
#include "lv/lv_arith_safe.h" /* lv_squarefree_i64（K2/F33 收敛权威） */

#include "lv/debug.h"
#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/mpz_poly.h"

/* ── 多项式系数内存池（共享实现，见 lv/coeff_pool.h）── */
#include "lv/coeff_pool.h"

/* ── 前向声明（来自 symbolics 子目录其他模块）── */
void refine_algebraic_bounds(Algebraic *a, int iterations);
bool algebraic_try_rationalize(Algebraic *a);
bool is_rational_zero(const Rational *r);
/* K2/F33：remove_square_factors 已收敛为 lv_arith_safe.h lv_squarefree_i64
 * （原 extern int 版声明与实际 int64_t 定义签名不匹配 UB） */
double algebraic_to_double(const Algebraic *a);
double transcendental_to_double(const Transcendental *t);

/* ── 外部溢出上下文 ── */
extern lv_THREAD_LOCAL struct OverflowContext g_overflow_context;

/* 指数上限：防止极大指数导致内存耗尽或计算时间过长 */
#define SYMBOLIC_COORD_POW_MAX_EXPONENT 1000

/* ── VTable 类型定义：SymbolicCoord 一元变换操作 ── */

/**
 * @brief 统一函数指针类型，用于 VTable 分发表
 *
 * 对于 negate 和 sqrt 操作，exponent 参数未使用（传 0 即可）。
 * 对于 pow 操作，exponent 为幂指数。
 */
typedef SymbolicCoord *(*CoordTransformOpFn)(const SymbolicCoord *, unsigned int);

/**
 * @brief VTable 结构体：每个 SymbolicCoord 类型对应一个实例，
 *        包含 negate / pow / sqrt 三个操作的函数指针。
 */
typedef struct {
    CoordTransformOpFn negate; /**< 取负操作 */
    CoordTransformOpFn pow;    /**< 幂操作   */
    CoordTransformOpFn sqrt;   /**< 平方根操作 */
} SymbolicCoordTransformOps;

/* 操作索引枚举（用于 VTable 分发表列索引） */
typedef enum {
    OP_NEGATE = 0,
    OP_POW    = 1,
    OP_SQRT   = 2,
    OP_COUNT  = 3
} CoordTransformOp;

/* ── handler 函数前向声明 ── */
static SymbolicCoord *negate_rational(const SymbolicCoord *coord, unsigned int unused);
static SymbolicCoord *negate_algebraic(const SymbolicCoord *coord, unsigned int unused);
static SymbolicCoord *negate_quadratic(const SymbolicCoord *coord, unsigned int unused);
static SymbolicCoord *negate_transcendental(const SymbolicCoord *coord, unsigned int unused);
static SymbolicCoord *pow_rational(const SymbolicCoord *base, unsigned int exponent);
static SymbolicCoord *pow_quadratic(const SymbolicCoord *base, unsigned int exponent);
static SymbolicCoord *pow_algebraic(const SymbolicCoord *base, unsigned int exponent);
static SymbolicCoord *pow_transcendental(const SymbolicCoord *base, unsigned int exponent);
static SymbolicCoord *sqrt_rational(const SymbolicCoord *coord, unsigned int unused);
static SymbolicCoord *sqrt_quadratic(const SymbolicCoord *coord, unsigned int unused);
static SymbolicCoord *sqrt_algebraic(const SymbolicCoord *coord, unsigned int unused);
static SymbolicCoord *sqrt_transcendental(const SymbolicCoord *coord, unsigned int unused);

/* ── VTable 分发表：4 种类型 × 3 种操作 ── */
static const SymbolicCoordTransformOps kCoordTransformOps[4] = {
    [RATIONAL]       = { negate_rational,      pow_rational,      sqrt_rational      },
    [ALGEBRAIC]      = { negate_algebraic,     pow_algebraic,     sqrt_algebraic     },
    [QUADRATIC]      = { negate_quadratic,     pow_quadratic,     sqrt_quadratic     },
    [TRANSCENDENTAL] = { negate_transcendental,pow_transcendental,sqrt_transcendental },
};

/* ============================================================
 * Perfect Square Check
 * ============================================================ */

/**
 * @brief 计算大整数的精确平方根（如果它是完全平方数）
 *
 * @param n 输入大整数
 * @return 如果 n 是完全平方数，返回新分配的 mpz_t*（调用者负责释放）；
 *         否则返回 NULL
 */
mpz_t *mpz_perfect_sqrt(mpz_t n) {
    if (mpz_sgn(n) < 0)
        return NULL;
    if (!mpz_perfect_square_p(n))
        return NULL;
    mpz_t *result = (mpz_t *) lv_calloc(1, sizeof(mpz_t));
    if (!result)
        return NULL;
    mpz_init(*result);
    mpz_sqrt(*result, n);
    return result;
}

/* ============================================================
 * Unary Negation — handler functions
 * ============================================================ */

static SymbolicCoord *negate_rational(const SymbolicCoord *coord, unsigned int unused) {
    (void)unused;
    Rational *neg = rational_negate(coord->data.rational);
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

/** 计算隔离区间容差：fabs(v)*factor，下限 floor_eps（收敛散落 8 处的 margin 计算样板） */
static double margin_floor(double v, double factor, double floor_eps) {
    double margin = fabs(v) * factor;
    if (margin < floor_eps)
        margin = floor_eps;
    return margin;
}

static SymbolicCoord *negate_algebraic(const SymbolicCoord *coord, unsigned int unused) {
    (void)unused;
    /* Negate algebraic: replace poly P(x) with P(-x), swap bounds */
    Algebraic *a = coord->data.algebraic;
    mpz_poly_t neg_poly;
    mpz_poly_init(&neg_poly);

    /* P(-x): negate odd-degree coefficients */
    if (a->minimal_poly.degree >= 0) {
        neg_poly.degree = a->minimal_poly.degree;
        neg_poly.coeffs = coeff_pool_alloc(neg_poly.degree + 1);
        if (!neg_poly.coeffs) {
            coeff_pool_clear(&neg_poly);
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
    coeff_pool_clear(&neg_poly);
    if (result)
        result->trust = coord->trust;
    return result;
}

static SymbolicCoord *negate_quadratic(const SymbolicCoord *coord, unsigned int unused) {
    (void)unused;
    Quadratic *q = coord->data.quadratic;
    Rational *neg_a = rational_negate(q->a);
    Rational *neg_b = rational_negate(q->b);
    SymbolicCoord *result = symbolic_coord_create_quadratic(neg_a, neg_b, q->n);
    if (result)
        result->trust = coord->trust;
    return result;
}

static SymbolicCoord *negate_transcendental(const SymbolicCoord *coord, unsigned int unused) {
    (void)unused;
    /* Negate transcendental: flip the sign of rational_operand */
    const Transcendental *src_t = coord->data.transcendental;
    const char *base = src_t->expr ? src_t->expr->base_name : src_t->name;

    Transcendental *t = transcendental_create(base);
    if (!t)
        return NULL;

    TranscendentalExpr *expr = lv_calloc(1, sizeof(TranscendentalExpr));
    if (!expr) {
        transcendental_destroy(t);
        return NULL;
    }
    expr->out_of_scope = src_t->expr ? src_t->expr->out_of_scope : false;
    lv_strlcpy(expr->base_name, base, sizeof(expr->base_name));
    if (src_t->expr && src_t->expr->rational_operand) {
        /* Negate the existing rational operand */
        expr->expr_type = src_t->expr->expr_type;
        Rational *neg_r = rational_negate(src_t->expr->rational_operand);
        if (!neg_r) {
            lv_free((void **) &expr);
            transcendental_destroy(t);
            return NULL;
        }
        expr->rational_operand = neg_r;
    } else {
        /* Bare constant: -pi = -1*pi */
        expr->expr_type = TRANS_EXPR_MUL_RATIONAL;
        expr->rational_operand = rational_create(-1, 1);
    }

    if (!expr->rational_operand) {
        lv_free((void **) &expr);
        transcendental_destroy(t);
        return NULL;
    }

    t->expr = expr;

    SymbolicCoord *result = lv_calloc(1, sizeof(SymbolicCoord));
    if (!result) {
        transcendental_destroy(t);
        return NULL;
    }
    result->type = TRANSCENDENTAL;
    result->trust = coord->trust;
    result->data.transcendental = t;
    return result;
}

/* ============================================================
 * Unary Negation — public API
 * ============================================================ */

SymbolicCoord *symbolic_coord_negate(const SymbolicCoord *coord) {
    if (!coord)
        return NULL;
    return kCoordTransformOps[coord->type].negate(coord, 0);
}

/* ============================================================
 * Power Operation — handler functions
 * ============================================================ */

static SymbolicCoord *pow_rational(const SymbolicCoord *base, unsigned int exponent) {
    /* Exact rational power using GMP */
    const Rational *r = base->data.rational;
    mpz_t num_pow, den_pow;
    mpz_init(num_pow);
    mpz_init(den_pow);

    mpz_pow_ui(num_pow, mpq_numref(lv_rational_mpq(r)), exponent);
    mpz_pow_ui(den_pow, mpq_denref(lv_rational_mpq(r)), exponent);

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

static SymbolicCoord *pow_quadratic(const SymbolicCoord *base, unsigned int exponent) {
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

static SymbolicCoord *pow_algebraic(const SymbolicCoord *base, unsigned int exponent) {
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
            mpz_t neg_c0;
            mpz_init(neg_c0);
            mpz_neg(neg_c0, a->minimal_poly.coeffs[0]);
            mpq_t root;
            mpq_init(root);
            mpq_set_num(root, neg_c0);
            mpq_set_den(root, a->minimal_poly.coeffs[1]);
            mpq_canonicalize(root);
            mpz_clear(neg_c0);

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
            sq_poly.coeffs = coeff_pool_alloc(3);
            if (sq_poly.coeffs) {
                mpz_init(sq_poly.coeffs[0]);
                mpz_init(sq_poly.coeffs[1]);
                mpz_init(sq_poly.coeffs[2]);
                mpz_set(sq_poly.coeffs[0], c0_sq);
                mpz_set(sq_poly.coeffs[1], term_mid);
                mpz_set(sq_poly.coeffs[2], c2_sq);

                double result_val = val * val;
                double margin = margin_floor(result_val, lv_EPSILON_NEWTON * 100.0, lv_EPSILON_NEWTON);

                SymbolicCoord *result =
                    symbolic_coord_create_algebraic(&sq_poly, result_val - margin, result_val + margin);
                coeff_pool_clear(&sq_poly);
                if (result) {
                    result->trust = base->trust;
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
                coeff_pool_clear(&sq_poly);
            }
            mpz_clear(c2_sq);
            mpz_clear(c1_sq);
            mpz_clear(c0_sq);
            mpz_clear(term_mid);
        }
    }

    /* General case: numerical approach with rationalization attempt */
    double result_val = pow(val, (double) exponent);

    {
        double margin_cf = margin_floor(result_val, lv_EPSILON_NUMERIC_COMPARE, lv_EPSILON_SUPERTINY);

        mpq_t approx;
        mpq_init(approx);
        mpq_set_d(approx, result_val);
        mpq_canonicalize(approx);

        double approx_val = mpq_get_d(approx);
        if (fabs(approx_val - result_val) <
            lv_EPSILON_NUMERIC_COMPARE * fabs(result_val) + lv_EPSILON_NUMERIC_COMPARE) {
            mpz_poly_t check_poly;
            mpz_poly_init(&check_poly);
            check_poly.degree = 1;
            check_poly.coeffs = coeff_pool_alloc(2);
            if (check_poly.coeffs) {
                mpz_init(check_poly.coeffs[0]);
                mpz_init(check_poly.coeffs[1]);
                mpz_neg(check_poly.coeffs[0], mpq_numref(approx));
                mpz_set(check_poly.coeffs[1], mpq_denref(approx));

                double tight_margin = margin_floor(result_val, lv_EPSILON_NEWTON, lv_EPSILON_NEWTON);

                SymbolicCoord *result = symbolic_coord_create_algebraic(&check_poly, result_val - tight_margin,
                                                                        result_val + tight_margin);
                coeff_pool_clear(&check_poly);
                if (result) {
                    result->trust = base->trust;
                    algebraic_try_rationalize(result->data.algebraic);
                    if (result->data.algebraic->cached_rational) {
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
            coeff_pool_clear(&check_poly);
        }
        mpq_clear(approx);
    }

    /* Final fallback: create algebraic number from numerical value */
    double margin = margin_floor(result_val, lv_EPSILON_NUMERIC_COMPARE, lv_EPSILON_NUMERIC_COMPARE);

    mpz_poly_t poly;
    mpz_poly_init(&poly);
    poly.degree = 1;
    poly.coeffs = coeff_pool_alloc(2);
    if (!poly.coeffs) {
        coeff_pool_clear(&poly);
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
    coeff_pool_clear(&poly);
    if (result)
        result->trust = base->trust;
    return result;
}

static SymbolicCoord *pow_transcendental(const SymbolicCoord *base, unsigned int exponent) {
    double base_val = transcendental_to_double(base->data.transcendental);
    double result_val = pow(base_val, (double) exponent);

    mpz_poly_t poly;
    mpz_poly_init(&poly);
    poly.degree = 1;
    poly.coeffs = coeff_pool_alloc(2);
    if (!poly.coeffs) {
        coeff_pool_clear(&poly);
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

    double margin = margin_floor(result_val, lv_EPSILON_NEWTON * 100.0, lv_EPSILON_NEWTON);

    SymbolicCoord *result =
        symbolic_coord_create_algebraic(&poly, result_val - margin, result_val + margin);
    coeff_pool_clear(&poly);
    if (result)
        result->trust = TRUST_AMBER;
    return result;
}

/* ============================================================
 * Power Operation — public API
 * ============================================================ */

SymbolicCoord *symbolic_coord_pow(const SymbolicCoord *base, unsigned int exponent) {
    if (!base)
        return NULL;

    /* 指数上限检查：防止 DoS */
    if (exponent > SYMBOLIC_COORD_POW_MAX_EXPONENT) {
        return NULL;
    }

    /* base^0 = 1 for any type */
    if (exponent == 0) {
        return symbolic_coord_create_rational(1, 1);
    }

    /* base^1 = base (copy) */
    if (exponent == 1) {
        return symbolic_coord_copy(base);
    }

    return kCoordTransformOps[base->type].pow(base, exponent);
}

/* ============================================================
 * Square Root Operation — handler functions
 * ============================================================ */

static SymbolicCoord *sqrt_rational(const SymbolicCoord *coord, unsigned int unused) {
    (void)unused;
    const Rational *r = coord->data.rational;
    mpz_t num, den;
    mpz_init_set(num, mpq_numref(lv_rational_mpq(r)));
    mpz_init_set(den, mpq_denref(lv_rational_mpq(r)));

    /* 检查分子和分母是否都是完全平方数 */
    int num_sign = mpz_sgn(num);
    if (num_sign < 0) {
        mpz_clear(num);
        mpz_clear(den);
        return NULL;
    }

    mpz_t *num_sqrt = mpz_perfect_sqrt(num);
    mpz_t *den_sqrt = mpz_perfect_sqrt(den);

    if (num_sqrt && den_sqrt) {
        Rational *result_r = rational_create_from_mpz(*num_sqrt, *den_sqrt);
        mpz_clear(*num_sqrt);
        lv_free((void **) &num_sqrt);
        mpz_clear(*den_sqrt);
        lv_free((void **) &den_sqrt);

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

    if (num_sqrt) {
        mpz_clear(*num_sqrt);
        lv_free((void **) &num_sqrt);
    }
    if (den_sqrt) {
        mpz_clear(*den_sqrt);
        lv_free((void **) &den_sqrt);
    }

    mpz_t product;
    mpz_init(product);
    mpz_mul(product, num, den);

    mpz_t remaining;
    mpz_init_set(remaining, product);
    mpz_t square_part;
    mpz_init_set_ui(square_part, 1);

    for (unsigned int i = 2; i * i <= 10000; i++) {
        while (mpz_divisible_ui_p(remaining, i * i)) {
            mpz_mul_ui(square_part, square_part, i);
            mpz_divexact_ui(remaining, remaining, i * i);
        }
    }

    if (mpz_cmp_si(remaining, 1) == 0) {
        mpz_clear(product);
        mpz_clear(remaining);
        mpz_clear(square_part);
        return NULL;
    }

    if (!mpz_fits_uint_p(remaining)) {
        mpz_clear(product);
        mpz_clear(remaining);
        mpz_clear(square_part);
        return NULL;
    }

    unsigned int n = mpz_get_ui(remaining);
    n = (unsigned int) lv_squarefree_i64(n); /* K2/F33：统一走权威 */

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

static SymbolicCoord *sqrt_quadratic(const SymbolicCoord *coord, unsigned int unused) {
    (void)unused;
    const Quadratic *q = coord->data.quadratic;

    /* Check if b = 0: sqrt(a) where a is rational */
    if (is_rational_zero(q->b)) {
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

    mpq_mul(a_sq, lv_rational_mpq(q->a), lv_rational_mpq(q->a));
    mpq_mul(b_sq, lv_rational_mpq(q->b), lv_rational_mpq(q->b));
    mpq_set_ui(b_sq_n, q->n, 1);
    mpq_mul(b_sq_n, b_sq_n, b_sq);
    mpq_sub(disc, a_sq, b_sq_n);

    if (mpq_sgn(disc) < 0) {
        mpq_clear(a_sq);
        mpq_clear(b_sq);
        mpq_clear(b_sq_n);
        mpq_clear(disc);
        return NULL;
    }

    mpz_t disc_num_sq, disc_product;
    mpz_init(disc_num_sq);
    mpz_init(disc_product);
    mpz_mul(disc_product, mpq_numref(disc), mpq_denref(disc));

    mpz_t *disc_sqrt = mpz_perfect_sqrt(disc_product);

    if (disc_sqrt) {
        mpq_t k;
        mpq_init(k);
        mpz_set(mpq_numref(k), *disc_sqrt);
        mpz_set(mpq_denref(k), mpq_denref(disc));
        mpq_canonicalize(k);

        mpq_t c_sq, c_sq_neg;
        mpq_init(c_sq);
        mpq_init(c_sq_neg);
        mpq_add(c_sq, lv_rational_mpq(q->a), k);
        mpq_div_2exp(c_sq, c_sq, 1);

        mpq_sub(c_sq_neg, lv_rational_mpq(q->a), k);
        mpq_div_2exp(c_sq_neg, c_sq_neg, 1);

        mpz_t csq_product;
        mpz_init(csq_product);
        mpz_mul(csq_product, mpq_numref(c_sq), mpq_denref(c_sq));
        mpz_t *c_sqrt = mpz_perfect_sqrt(csq_product);

        Rational *c_rat = NULL;
        Rational *d_rat = NULL;

        if (c_sqrt && mpq_sgn(c_sq) >= 0) {
            c_rat = rational_create_from_mpz(*c_sqrt, mpq_denref(c_sq));
            Rational *two_c = rational_create_from_mpz(*c_sqrt, mpq_denref(c_sq));
            mpq_mul_2exp(two_c->value, two_c->value, 1);
            d_rat = rational_divide(q->b, two_c);
            rational_destroy(two_c);
            mpz_clear(*c_sqrt);
            lv_free((void **) &c_sqrt);
        } else {
            if (c_sqrt) {
                mpz_clear(*c_sqrt);
                lv_free((void **) &c_sqrt);
            }

            mpz_mul(csq_product, mpq_numref(c_sq_neg), mpq_denref(c_sq_neg));
            c_sqrt = mpz_perfect_sqrt(csq_product);

            if (c_sqrt && mpq_sgn(c_sq_neg) >= 0) {
                c_rat = rational_create_from_mpz(*c_sqrt, mpq_denref(c_sq_neg));
                Rational *two_c = rational_create_from_mpz(*c_sqrt, mpq_denref(c_sq_neg));
                mpq_mul_2exp(two_c->value, two_c->value, 1);
                d_rat = rational_divide(q->b, two_c);
                rational_destroy(two_c);
                mpz_clear(*c_sqrt);
                lv_free((void **) &c_sqrt);
            } else {
                if (c_sqrt) {
                    mpz_clear(*c_sqrt);
                    lv_free((void **) &c_sqrt);
                }
            }
        }

        mpz_clear(csq_product);
        mpq_clear(c_sq);
        mpq_clear(c_sq_neg);
        mpq_clear(k);

        if (c_rat && d_rat) {
            SymbolicCoord *result = symbolic_coord_create_quadratic(c_rat, d_rat, q->n);
            if (result)
                result->trust = coord->trust;
            else {
                rational_destroy(c_rat);
                rational_destroy(d_rat);
            }
            mpz_clear(*disc_sqrt);
            lv_free((void **) &disc_sqrt);
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
        lv_free((void **) &disc_sqrt);
        mpz_clear(disc_num_sq);
        mpz_clear(disc_product);
    } else {
        mpz_clear(disc_num_sq);
        mpz_clear(disc_product);
    }

    mpq_clear(a_sq);
    mpq_clear(b_sq);
    mpq_clear(b_sq_n);
    mpq_clear(disc);

    {
        double a_val = rational_to_double(q->a);
        double b_val = rational_to_double(q->b);
        double sqrt_n = sqrt((double) q->n);
        double inner = a_val + b_val * sqrt_n;

        if (inner < 0)
            return NULL;

        double result_val = sqrt(inner);

        mpz_poly_t poly;
        mpz_poly_init(&poly);
        poly.degree = 4;
        poly.coeffs = coeff_pool_alloc(5);
        if (!poly.coeffs) {
            coeff_pool_clear(&poly);
            return NULL;
        }

        mpz_init(poly.coeffs[0]);
        mpz_mul(poly.coeffs[0], mpq_numref(lv_rational_mpq(q->a)), mpq_numref(lv_rational_mpq(q->a)));
        mpz_t den_a_sq;
        mpz_init(den_a_sq);
        mpz_mul(den_a_sq, mpq_denref(lv_rational_mpq(q->a)), mpq_denref(lv_rational_mpq(q->a)));

        mpz_t b_sq_n_num, b_sq_n_den;
        mpz_init(b_sq_n_num);
        mpz_init(b_sq_n_den);
        mpz_mul(b_sq_n_num, mpq_numref(lv_rational_mpq(q->b)), mpq_numref(lv_rational_mpq(q->b)));
        mpz_mul_ui(b_sq_n_num, b_sq_n_num, q->n);
        mpz_mul(b_sq_n_den, mpq_denref(lv_rational_mpq(q->b)), mpq_denref(lv_rational_mpq(q->b)));

        mpz_t lcd;
        mpz_init(lcd);
        mpz_mul(lcd, den_a_sq, b_sq_n_den);

        mpz_t term1, term2;
        mpz_init(term1);
        mpz_init(term2);
        mpz_mul(term1, poly.coeffs[0], b_sq_n_den);
        mpz_mul(term2, b_sq_n_num, den_a_sq);
        mpz_sub(poly.coeffs[0], term1, term2);

        mpz_set(poly.coeffs[0], poly.coeffs[0]);

        mpz_init_set_ui(poly.coeffs[1], 0);

        mpz_init(poly.coeffs[2]);
        mpz_mul_ui(poly.coeffs[2], mpq_numref(lv_rational_mpq(q->a)), 2);
        mpz_neg(poly.coeffs[2], poly.coeffs[2]);
        mpz_mul(poly.coeffs[2], poly.coeffs[2], b_sq_n_den);

        mpz_init_set_ui(poly.coeffs[3], 0);

        mpz_init_set(poly.coeffs[4], lcd);

        mpz_clear(den_a_sq);
        mpz_clear(b_sq_n_num);
        mpz_clear(b_sq_n_den);
        mpz_clear(lcd);
        mpz_clear(term1);
        mpz_clear(term2);

        double margin = margin_floor(result_val, lv_EPSILON_NEWTON * 10.0, lv_EPSILON_NEWTON);

        SymbolicCoord *result =
            symbolic_coord_create_algebraic(&poly, result_val - margin, result_val + margin);
        coeff_pool_clear(&poly);
        if (result)
            result->trust = coord->trust;
        return result;
    }
}

static SymbolicCoord *sqrt_algebraic(const SymbolicCoord *coord, unsigned int unused) {
    (void)unused;
    Algebraic *a = coord->data.algebraic;

    double mid = (a->left_bound + a->right_bound) / 2.0;
    if (mid < -lv_EPSILON_NUMERIC_COMPARE)
        return NULL;

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

    int deg = a->minimal_poly.degree;
    int new_deg = 2 * deg;

    mpz_poly_t sqrt_poly;
    mpz_poly_init(&sqrt_poly);
    sqrt_poly.degree = new_deg;
    sqrt_poly.coeffs = coeff_pool_alloc(new_deg + 1);
    if (!sqrt_poly.coeffs) {
        coeff_pool_clear(&sqrt_poly);
        return NULL;
    }

    for (int i = 0; i <= new_deg; i++) {
        mpz_init(sqrt_poly.coeffs[i]);
        if (i % 2 == 0) {
            int src_idx = i / 2;
            if (src_idx <= deg) {
                mpz_set(sqrt_poly.coeffs[i], a->minimal_poly.coeffs[src_idx]);
            }
        }
    }

    refine_algebraic_bounds(a, 20);
    double val = (a->left_bound + a->right_bound) / 2.0;
    double sqrt_val = sqrt(fabs(val));

    double margin = margin_floor(sqrt_val, lv_EPSILON_NEWTON * 10.0, lv_EPSILON_NEWTON);

    SymbolicCoord *result = symbolic_coord_create_algebraic(&sqrt_poly, sqrt_val - margin, sqrt_val + margin);
    coeff_pool_clear(&sqrt_poly);
    if (result)
        result->trust = coord->trust;
    return result;
}

static SymbolicCoord *sqrt_transcendental(const SymbolicCoord *coord, unsigned int unused) {
    (void)unused;
    double base_val = transcendental_to_double(coord->data.transcendental);
    if (base_val < 0.0)
        return NULL;

    double sqrt_val = sqrt(base_val);

    mpz_poly_t poly;
    mpz_poly_init(&poly);
    poly.degree = 1;
    poly.coeffs = coeff_pool_alloc(2);
    if (!poly.coeffs) {
        coeff_pool_clear(&poly);
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

    double margin = margin_floor(sqrt_val, lv_EPSILON_NEWTON * 100.0, lv_EPSILON_NEWTON);

    SymbolicCoord *result = symbolic_coord_create_algebraic(&poly, sqrt_val - margin, sqrt_val + margin);
    coeff_pool_clear(&poly);
    if (result)
        result->trust = TRUST_AMBER;
    return result;
}

/* ============================================================
 * Square Root Operation — public API
 * ============================================================ */

SymbolicCoord *symbolic_coord_sqrt(const SymbolicCoord *coord) {
    if (!coord)
        return NULL;
    return kCoordTransformOps[coord->type].sqrt(coord, 0);
}
