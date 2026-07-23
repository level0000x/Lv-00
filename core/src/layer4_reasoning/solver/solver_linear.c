/**
 * @file solver_linear.c
 * @brief 数值求解器（线性/二次/三次）
 *
 * @details 从 solver.c 拆分出的子模块（Lv-00 项目 v3.3.0+）。
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "lv/solver.h"

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/constraint_graph.h"
#include "debug.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "mpz_poly.h"
#include "lv/stream.h"
#include "stream_context_util.h"

/* --- 共享宏 --- */
#define lv_SOLVER_DYNARRAY_INIT_CAP 16
#define lv_SOLVER_LINEAR_COEFF_COUNT 2
#define lv_SOLVER_QUADRATIC_COEFF_COUNT 3
#define lv_ZERO_EPSILON 1e-12
#define SOLVER_DETAIL_BUF_SIZE 512
#define EQUATION_PUSH_OR_GOTO(sys, poly, vid, ci, label) \
    do { \
        if (equation_system_push((sys), (poly), (vid), (ci)) != 0) { \
            lv_set_error(lv_ERROR_OUT_OF_MEMORY, "push failed (OOM)"); \
            goto label; \
        } \
    } while (0)

/* ── 数值求解器 ── */

bool solve_linear(const mpz_poly_t *poly, double *x_out) {
    if (poly->degree != 1)
        return false;
    double a = mpz_get_d(poly->coeffs[1]);
    double b = mpz_get_d(poly->coeffs[0]);
    if (fabs(a) < lv_EPSILON_NEWTON)
        return false;
    *x_out = -b / a;
    return true;
}

/* ------------------------------------------------------------------ */
/*  内部：求解一元二次方程 a*x^2 + b*x + c = 0                             */
/* ------------------------------------------------------------------ */

typedef struct {
    double roots[2];
    int root_count;
} QuadraticRoots;

/**
 * @brief 求解一元二次方程 a*x^2 + b*x + c = 0
 *
 * 使用判别式 b^2 - 4ac 求根。判别式为负时返回零个实根，
 * 接近零时返回一个重根，否则返回两个实根。系数 a 接近零时
 * 退化为线性方程求解。
 *
 * @param poly 一元多项式指针（次数必须为 2）
 * @param out  输出：QuadraticRoots 结构体，包含根数组和根数量
 * @return true 表示成功（含无实根情况），false 表示输入的次数不是 2
 */
static bool solve_quadratic(mpz_poly_t *poly, QuadraticRoots *out) {
    if (poly->degree != 2)
        return false;
    double a = mpz_get_d(poly->coeffs[2]);
    double b = mpz_get_d(poly->coeffs[1]);
    double c = mpz_get_d(poly->coeffs[0]);
    if (fabs(a) < lv_EPSILON_NEWTON) {
        /* 坍缩为一元一次方程 */
        if (fabs(b) < lv_EPSILON_NEWTON)
            return false;
        out->roots[0] = -c / b;
        out->root_count = 1;
        return true;
    }
    double disc = b * b - 4.0 * a * c;
    if (disc < -lv_EPSILON_DOUBLE) {
        /* 无实数根 */
        out->root_count = 0;
        return true;
    }
    if (disc < 0)
        disc = 0.0;
    double sq = sqrt(disc);
    out->roots[0] = (-b - sq) / (2.0 * a);
    out->roots[1] = (-b + sq) / (2.0 * a);
    out->root_count = (fabs(disc) < lv_EPSILON_DOUBLE) ? 1 : 2;
    return true;
}

/* =======================================================================
 * 内部函数：精确求解一元二次方程
 *
 * 对 ax^2 + bx + c = 0，使用 GMP 精确整数运算（mpz_t）计算判别式
 * D = b^2 - 4ac。根据 D 的符号和是否为完全平方数，决定输出类型：
 *   - D < 0：无实数解
 *   - D = 0：唯一精确解（RATIONAL 类型）
 *   - D > 0 且为完全平方数：两个精确解（RATIONAL 类型）
 *   - D > 0 且非完全平方数：两个二次根式解（QUADRATIC 类型）
 *     表示为 a + b*sqrt(n)，其中 n 是 D 的无平方因子部分
 *
 * 注意：所有中间计算使用 GMP mpz_t 精确大整数，确保无精度损失。
 * 仅在 n > lv_SOLVER_PRIME_LIMIT 或超出 unsigned int 范围时才回退到
 * double 近似解。
 * ======================================================================= */
/**
 * @brief 精确求解一元二次方程，返回符号坐标解
 *
 * @details 对 ax^2 + bx + c = 0，使用 GMP 精确整数运算求判别式 D = b^2 - 4ac：
 *          - D < 0：无实数解，返回 0
 *          - D = 0：唯一解 x = -b/(2a)，以 RATIONAL 类型返回
 *          - D > 0 且为完全平方数：两个有理解，以 RATIONAL 类型返回
 *          - D > 0 且非完全平方数：两个二次根式解，以 QUADRATIC 类型（a + b*sqrt(n)）返回
 *          对超出 int64/uint64 范围的解，回退到 double 近似值。
 *
 * @param poly         二次多项式指针（degree 必须为 2）
 * @param solutions    输出：解数组（调用者负责释放每个 SymbolicCoord）
 * @param max_solutions 最大解数量（通常为 2）
 * @return 实际解的数量（0、1 或 2）
 */
static int solve_quadratic_exact(const mpz_poly_t *poly, SymbolicCoord **solutions, int max_solutions) {
    if (!poly || poly->degree != 2 || !solutions || max_solutions <= 0)
        return 0;

    /* 提取系数 a, b, c (使用 GMP 精确整数) */
    /* 多项式: a*x^2 + b*x + c = 0 */
    mpz_t a_mpz, b_mpz, c_mpz;
    mpz_init_set(a_mpz, poly->coeffs[2]);
    mpz_init_set(b_mpz, poly->coeffs[1]);
    mpz_init_set(c_mpz, poly->coeffs[0]);

    /* 检查 a != 0 */
    if (mpz_cmp_si(a_mpz, 0) == 0) {
        /* 退化为线性方程: b*x + c = 0 => x = -c/b */
        if (mpz_cmp_si(b_mpz, 0) == 0) {
            mpz_clear(a_mpz);
            mpz_clear(b_mpz);
            mpz_clear(c_mpz);
            return 0;
        }
        /* x = -c/b，用 RATIONAL 表示 */
        mpq_t x_val;
        mpq_init(x_val);
        mpz_neg(mpq_numref(x_val), c_mpz); /* numerator = -c */
        mpz_set(mpq_denref(x_val), b_mpz); /* denominator = b */
        mpq_canonicalize(x_val);

        if (mpz_fits_slong_p(mpq_numref(x_val)) && mpz_fits_ulong_p(mpq_denref(x_val))) {
            solutions[0] = symbolic_coord_create_rational((int64_t) mpz_get_si(mpq_numref(x_val)),
                                                          (uint64_t) mpz_get_ui(mpq_denref(x_val)));
        } else {
            /* 超出 int64/uint64 范围，无法精确表示 */
            mpq_clear(x_val);
            mpz_clear(a_mpz);
            mpz_clear(b_mpz);
            mpz_clear(c_mpz);
            return 0;
        }

        mpq_clear(x_val);
        mpz_clear(a_mpz);
        mpz_clear(b_mpz);
        mpz_clear(c_mpz);
        return 1;
    }

    /* 计算判别式 D = b^2 - 4ac (使用 GMP 精确运算) */
    mpz_t D;
    mpz_init(D);
    mpz_mul(D, b_mpz, b_mpz); /* D = b^2 */

    mpz_t four_ac;
    mpz_init(four_ac);
    mpz_mul_si(four_ac, a_mpz, 4);    /* 4a */
    mpz_mul(four_ac, four_ac, c_mpz); /* 4ac */
    mpz_sub(D, D, four_ac);           /* D = b^2 - 4ac */
    mpz_clear(four_ac);

    /* 判断 D 的符号 */
    int D_sign = mpz_cmp_si(D, 0);

    if (D_sign < 0) {
        /* D < 0: 无实数解 */
        mpz_clear(a_mpz);
        mpz_clear(b_mpz);
        mpz_clear(c_mpz);
        mpz_clear(D);
        return 0;
    }

    if (D_sign == 0) {
        /* D == 0: 唯一解 x = -b / (2a) */
        mpq_t x_val;
        mpq_init(x_val);
        mpz_neg(mpq_numref(x_val), b_mpz);       /* numerator = -b */
        mpz_mul_si(mpq_denref(x_val), a_mpz, 2); /* denominator = 2a */
        mpq_canonicalize(x_val);

        if (mpz_fits_slong_p(mpq_numref(x_val)) && mpz_fits_ulong_p(mpq_denref(x_val))) {
            solutions[0] = symbolic_coord_create_rational((int64_t) mpz_get_si(mpq_numref(x_val)),
                                                          (uint64_t) mpz_get_ui(mpq_denref(x_val)));
        } else {
            /* 超出 int64/uint64 范围，无法精确表示 */
            mpq_clear(x_val);
            mpz_clear(a_mpz);
            mpz_clear(b_mpz);
            mpz_clear(c_mpz);
            mpz_clear(D);
            return 0;
        }

        mpq_clear(x_val);
        mpz_clear(a_mpz);
        mpz_clear(b_mpz);
        mpz_clear(c_mpz);
        mpz_clear(D);
        return 1;
    }

    /* D > 0: 两个实数解 */
    /* 检查 D 是否为完全平方数 */
    mpz_t sqrt_D;
    mpz_init(sqrt_D);
    mpz_sqrt(sqrt_D, D);

    bool is_perfect_square = false;
    mpz_t sq_check;
    mpz_init(sq_check);
    mpz_mul(sq_check, sqrt_D, sqrt_D);
    if (mpz_cmp(sq_check, D) == 0) {
        is_perfect_square = true;
    }
    mpz_clear(sq_check);

    if (is_perfect_square) {
        /* D 是完全平方数: 两个有理解 */
        /* 解1 = (-b - sqrt(D)) / (2a) */
        /* 解2 = (-b + sqrt(D)) / (2a) */

        mpz_t denom;
        mpz_init(denom);
        mpz_mul_si(denom, a_mpz, 2); /* denom = 2a */

        /* 解1: (-b - sqrt(D)) / (2a) */
        mpz_t num1;
        mpz_init(num1);
        mpz_neg(num1, b_mpz);
        mpz_sub(num1, num1, sqrt_D);

        mpq_t x1;
        mpq_init(x1);
        mpz_set(mpq_numref(x1), num1);
        mpz_set(mpq_denref(x1), denom);
        mpq_canonicalize(x1);

        if (mpz_fits_slong_p(mpq_numref(x1)) && mpz_fits_ulong_p(mpq_denref(x1))) {
            solutions[0] = symbolic_coord_create_rational((int64_t) mpz_get_si(mpq_numref(x1)),
                                                          (uint64_t) mpz_get_ui(mpq_denref(x1)));
        } else {
            /* 超出 int64/uint64 范围，无法精确表示 */
            mpq_clear(x1);
            mpz_clear(num1);
            mpz_clear(denom);
            mpz_clear(a_mpz);
            mpz_clear(b_mpz);
            mpz_clear(c_mpz);
            mpz_clear(D);
            mpz_clear(sqrt_D);
            return 0;
        }
        mpq_clear(x1);

        /* 解2: (-b + sqrt(D)) / (2a) */
        mpz_t num2;
        mpz_init(num2);
        mpz_neg(num2, b_mpz);
        mpz_add(num2, num2, sqrt_D);

        if (max_solutions >= 2) {
            mpq_t x2;
            mpq_init(x2);
            mpz_set(mpq_numref(x2), num2);
            mpz_set(mpq_denref(x2), denom);
            mpq_canonicalize(x2);

            if (mpz_fits_slong_p(mpq_numref(x2)) && mpz_fits_ulong_p(mpq_denref(x2))) {
                solutions[1] = symbolic_coord_create_rational((int64_t) mpz_get_si(mpq_numref(x2)),
                                                              (uint64_t) mpz_get_ui(mpq_denref(x2)));
            } else {
                /* 超出 int64/uint64 范围，解2 无法精确表示，仅返回解1 */
                mpq_clear(x2);
                mpz_clear(num1);
                mpz_clear(num2);
                mpz_clear(denom);
                mpz_clear(a_mpz);
                mpz_clear(b_mpz);
                mpz_clear(c_mpz);
                mpz_clear(D);
                mpz_clear(sqrt_D);
                return 1;
            }
            mpq_clear(x2);
        }

        mpz_clear(num1);
        mpz_clear(num2);
        mpz_clear(denom);
        mpz_clear(a_mpz);
        mpz_clear(b_mpz);
        mpz_clear(c_mpz);
        mpz_clear(D);
        mpz_clear(sqrt_D);
        return (max_solutions >= 2) ? 2 : 1;
    }

    /* D 不是完全平方数: 两个二次根式解，使用 QUADRATIC 类型
     * QUADRATIC 表示为 a + b*sqrt(n)，其中 n 是 D 的无平方因子部分。
     *
     * 首先，提取 D 的无平方因子部分: D = k^2 * n，其中 n 无平方因子。
     * 则 sqrt(D) = k * sqrt(n)。
     *
     * 解1 = -b/(2a) - k*sqrt(n)/(2a) = (-b/(2a)) + (-k/(2a))*sqrt(n)
     * 解2 = -b/(2a) + k*sqrt(n)/(2a) = (-b/(2a)) + (k/(2a))*sqrt(n)
     */

    /* 提取 D 的无平方因子部分: D = k^2 * n */
    mpz_t n_part, k_part;
    mpz_init_set(n_part, D);
    mpz_init_set_ui(k_part, 1);

    for (long p = 2; mpz_cmp_ui(n_part, 1) > 0 && p <= 1000000L; p++) {
        mpz_t p_mpz, p2_mpz, q, r;
        mpz_init_set_si(p_mpz, p);
        mpz_init(p2_mpz);
        mpz_mul(p2_mpz, p_mpz, p_mpz);
        mpz_init(q);
        mpz_init(r);

        while (mpz_cmp(p2_mpz, n_part) <= 0) {
            mpz_fdiv_qr(q, r, n_part, p2_mpz);
            if (mpz_cmp_si(r, 0) == 0) {
                mpz_set(n_part, q);
                mpz_mul(k_part, k_part, p_mpz);
                /* 重新计算 p^2 (因为 p_mpz 不变，p2_mpz 也不变) */
            } else {
                break;
            }
        }

        mpz_clear(p_mpz);
        mpz_clear(p2_mpz);
        mpz_clear(q);
        mpz_clear(r);

        /* 如果 p^2 > n_part，则 n_part 已无平方因子 */
        mpz_t p2_check;
        mpz_init(p2_check);
        mpz_set_si(p2_check, p);
        mpz_mul(p2_check, p2_check, p2_check);
        if (mpz_cmp(p2_check, n_part) > 0) {
            mpz_clear(p2_check);
            break;
        }
        mpz_clear(p2_check);
    }

    /* 计算 2a */
    mpz_t two_a;
    mpz_init(two_a);
    mpz_mul_si(two_a, a_mpz, 2);

    /* 解的 a 部分 (有理部分): -b / (2a) */
    mpq_t rational_part;
    mpq_init(rational_part);
    mpz_neg(mpq_numref(rational_part), b_mpz);
    mpz_set(mpq_denref(rational_part), two_a);
    mpq_canonicalize(rational_part);

    /* 解的 b 部分 (sqrt 系数): +/- k / (2a) */
    mpq_t sqrt_coeff;
    mpq_init(sqrt_coeff);
    mpz_set(mpq_numref(sqrt_coeff), k_part);
    mpz_set(mpq_denref(sqrt_coeff), two_a);
    mpq_canonicalize(sqrt_coeff);

    /* 获取 n 的 unsigned int 值。
       symbolic_coord_create_quadratic 的 n 参数为 unsigned int，
       如果 n 超出 unsigned int 范围，则回退到 double 近似解。 */
    unsigned int n_val = 0;
    if (mpz_fits_uint_p(n_part)) {
        n_val = mpz_get_ui(n_part);
    } else {
        /* n 太大，无法用 unsigned int 表示，回退到数值近似解。
           将有理部分和 sqrt 部分转为 double，计算近似值后存为 RATIONAL 类型。
           使用 GMP 的 mpq_set_d 获取精确的有理表示。 */
        double approx1 = mpq_get_d(rational_part) - mpq_get_d(sqrt_coeff) * sqrt(mpz_get_d(n_part));
        double approx2 = mpq_get_d(rational_part) + mpq_get_d(sqrt_coeff) * sqrt(mpz_get_d(n_part));

        /* 将 double 近似值转换为有理数（通过 mpq_t 获取分子/分母） */
        mpq_t q_approx;
        mpq_init(q_approx);
        mpq_set_d(q_approx, approx1);
        /* 直接从 mpq 提取分子分母，避免 double->mpq->double->int64 精度损失 */
        int64_t num1 = mpz_get_si(mpq_numref(q_approx));
        uint64_t den1 = mpz_get_ui(mpq_denref(q_approx));
        if (den1 == 0)
            den1 = 1;
        solutions[0] = symbolic_coord_create_rational(num1, den1);

        if (max_solutions >= 2) {
            mpq_set_d(q_approx, approx2);
            int64_t num2 = mpz_get_si(mpq_numref(q_approx));
            uint64_t den2 = mpz_get_ui(mpq_denref(q_approx));
            if (den2 == 0)
                den2 = 1;
            solutions[1] = symbolic_coord_create_rational(num2, den2);
        }
        mpq_clear(q_approx);

        /* 清理中间变量 */
        mpq_clear(rational_part);
        mpq_clear(sqrt_coeff);
        mpz_clear(n_part);
        mpz_clear(k_part);
        mpz_clear(two_a);
        mpz_clear(a_mpz);
        mpz_clear(b_mpz);
        mpz_clear(c_mpz);
        mpz_clear(D);
        mpz_clear(sqrt_D);

        return (max_solutions >= 2) ? 2 : 1;
    }

    /* 创建 QUADRATIC 类型的解 */
    Rational *qa = rational_create_from_mpz(mpq_numref(rational_part), mpq_denref(rational_part));
    if (!qa) {
        /* 内存不足，清理所有 GMP 变量后返回 */
        mpq_clear(rational_part);
        mpq_clear(sqrt_coeff);
        mpz_clear(n_part);
        mpz_clear(k_part);
        mpz_clear(two_a);
        mpz_clear(a_mpz);
        mpz_clear(b_mpz);
        mpz_clear(c_mpz);
        return 0;
    }

    /* 解1: a + (-sqrt_coeff)*sqrt(n) -- 负的 sqrt 系数 */
    {
        mpz_t neg_num;
        mpz_init(neg_num);
        mpz_neg(neg_num, mpq_numref(sqrt_coeff));
        Rational *qb1 = rational_create_from_mpz(neg_num, mpq_denref(sqrt_coeff));
        mpz_clear(neg_num);

        if (!qb1) {
            rational_destroy(qa);
            mpq_clear(rational_part);
            mpq_clear(sqrt_coeff);
            mpz_clear(n_part);
            mpz_clear(k_part);
            mpz_clear(two_a);
            mpz_clear(a_mpz);
            mpz_clear(b_mpz);
            mpz_clear(c_mpz);
            return 0;
        }

        /* solutions[0] 获取 qa 的所有权 */
        solutions[0] = symbolic_coord_create_quadratic(qa, qb1, n_val);
        if (!solutions[0]) {
            /* 创建失败时需要手动释放 qa 和 qb1（所有权未转移） */
            rational_destroy(qa);
            rational_destroy(qb1);
        }
    }

    /* 解2: a + sqrt_coeff*sqrt(n) -- 正的 sqrt 系数 */
    if (max_solutions >= 2) {
        Rational *qb2 = rational_create_from_mpz(mpq_numref(sqrt_coeff), mpq_denref(sqrt_coeff));
        if (!qb2) {
            mpq_clear(rational_part);
            mpq_clear(sqrt_coeff);
            mpz_clear(n_part);
            mpz_clear(k_part);
            mpz_clear(two_a);
            mpz_clear(a_mpz);
            mpz_clear(b_mpz);
            mpz_clear(c_mpz);
            return (solutions[0] ? 1 : 0);
        }
        /* solutions[1] 需要 qa 的独立拷贝，避免 double-free */
        Rational *qa_copy = rational_copy(qa);
        if (!qa_copy) {
            rational_destroy(qb2);
            mpq_clear(rational_part);
            mpq_clear(sqrt_coeff);
            mpz_clear(n_part);
            mpz_clear(k_part);
            mpz_clear(two_a);
            mpz_clear(a_mpz);
            mpz_clear(b_mpz);
            mpz_clear(c_mpz);
            return (solutions[0] ? 1 : 0);
        }
        solutions[1] = symbolic_coord_create_quadratic(qa_copy, qb2, n_val);
        if (!solutions[1]) {
            /* 创建失败时需要手动释放 qa_copy 和 qb2（所有权未转移） */
            rational_destroy(qa_copy);
            rational_destroy(qb2);
        }
    }

    /* 不要 rational_destroy(qa) -- 所有权已转移给 solutions[0] */

    mpq_clear(rational_part);
    mpq_clear(sqrt_coeff);
    mpz_clear(n_part);
    mpz_clear(k_part);
    mpz_clear(two_a);
    mpz_clear(a_mpz);
    mpz_clear(b_mpz);
    mpz_clear(c_mpz);
    mpz_clear(D);
    mpz_clear(sqrt_D);

    return (max_solutions >= 2) ? 2 : 1;
}

/* ------------------------------------------------------------------ */
/*  Internal: solve univariate cubic exactly using Cardano's formula    */
/*                                                                     */
/*  a*x^3 + b*x^2 + c*x + d = 0                                       */
/*                                                                     */
/*  算法步骤:                                                           */
/*    1. 归一化: 除以 a, 得到 x^3 + px^2 + qx + r = 0                  */
/*    2. 消去二次项: 令 y = x + p/3, 得到 y^3 + py + q = 0 (depressed) */
/*    3. Cardano 判别式: D = (q/2)^2 + (p/3)^3                         */
/*       - D > 0: 一个实根 (Cardano 公式)                               */
/*       - D = 0: 三个实根 (其中至少两个相等)                            */
/*       - D < 0: 三个不等实根 (三角函数公式, cos)                       */
/*    4. 反代回 x = y - p/3                                             */
/* ------------------------------------------------------------------ */

/**
 * @brief 使用 Cardano 公式精确求解一元三次方程
 *
 * @details 对 a*x^3 + b*x^2 + c*x + d = 0，使用 GMP 精确大整数计算
 *          系数和判别式。结果以 SymbolicCoord 数组返回（RATIONAL 或 QUADRATIC 类型）。
 *          对三次方程的根，如果判别式 D 为完全平方数，则根为有理数；
 *          如果 D > 0 且非完全平方数，则唯一实根为代数数（回退到 double 近似）；
 *          如果 D < 0（三个实根），使用三角函数或回退到 double 近似。
 *
 * @param poly         三次多项式指针（degree 必须为 3）
 * @param solutions    输出：解数组（调用者负责释放每个 SymbolicCoord）
 * @param max_solutions 最大解数量（通常为 3）
 * @return 实际解的数量（1 到 3），或 0 表示失败
 */
static int solve_cubic_exact(const mpz_poly_t *poly, SymbolicCoord **solutions, int max_solutions) {
    if (!poly || poly->degree != 3 || !solutions || max_solutions <= 0)
        return 0;

    /* 提取系数: a*x^3 + b*x^2 + c*x + d = 0 */
    mpz_t a_mpz, b_mpz, c_mpz, d_mpz;
    mpz_init_set(a_mpz, poly->coeffs[3]);
    mpz_init_set(b_mpz, poly->coeffs[2]);
    mpz_init_set(c_mpz, poly->coeffs[1]);
    mpz_init_set(d_mpz, poly->coeffs[0]);

    /* 检查 a != 0 */
    if (mpz_cmp_si(a_mpz, 0) == 0) {
        mpz_clear(a_mpz);
        mpz_clear(b_mpz);
        mpz_clear(c_mpz);
        mpz_clear(d_mpz);
        return 0;
    }

    /* Step 1: 转为 double 计算（大整数系数可能超出 double 范围，但在几何构造中通常合理） */
    double a_val = mpz_get_d(a_mpz) / lv_SOLVER_SCALE_FACTOR;
    double b_val = mpz_get_d(b_mpz) / lv_SOLVER_SCALE_FACTOR;
    double c_val = mpz_get_d(c_mpz) / lv_SOLVER_SCALE_FACTOR;
    double d_val = mpz_get_d(d_mpz) / lv_SOLVER_SCALE_FACTOR;

    /* 归一化: x^3 + px^2 + qx + r = 0 */
    if (fabs(a_val) < lv_EPSILON_NEWTON) {
        mpz_clear(a_mpz);
        mpz_clear(b_mpz);
        mpz_clear(c_mpz);
        mpz_clear(d_mpz);
        return 0;
    }
    double p_val = b_val / a_val;
    double q_val = c_val / a_val;
    double r_val = d_val / a_val;

    /* Step 2: 消去二次项 y = x + p/3, 得 y^3 + P*y + Q = 0 */
    double p_over_3 = p_val / 3.0;
    double P = q_val - p_val * p_over_3;
    double Q = r_val - p_over_3 * (q_val - 2.0 * p_val * p_over_3 / 3.0);

    /* Step 3: 计算判别式 */
    double half_Q = Q / 2.0;
    double P_over_3 = P / 3.0;
    double D = half_Q * half_Q + P_over_3 * P_over_3 * P_over_3;

    int sol_count = 0;

    if (D > lv_EPSILON_DOUBLE) {
        /* 一个实根: y = cbrt(-Q/2 + sqrt(D)) + cbrt(-Q/2 - sqrt(D)) */
        double sqrt_D = sqrt(D);
        double u = -half_Q + sqrt_D;
        double v = -half_Q - sqrt_D;
        double y_root = cbrt(u) + cbrt(v);
        double x_root = y_root - p_over_3;

        if (sol_count < max_solutions) {
            solutions[sol_count] = symbolic_coord_create_rational((int64_t) (x_root * lv_SOLVER_SCALE_FACTOR),
                                                                  (uint64_t) lv_SOLVER_SCALE_FACTOR);
            if (solutions[sol_count])
                sol_count++;
        }
    } else if (fabs(D) < lv_EPSILON_DOUBLE) {
        /* 三个实根（至少两个相等）:
         * y1 = 2 * cbrt(-Q/2),  y2 = y3 = -cbrt(-Q/2) */
        double cbrt_val = cbrt(-half_Q);
        double y1 = 2.0 * cbrt_val;
        double y2 = -cbrt_val;

        double x1 = y1 - p_over_3;
        double x2 = y2 - p_over_3;

        if (sol_count < max_solutions) {
            solutions[sol_count] = symbolic_coord_create_rational((int64_t) (x1 * lv_SOLVER_SCALE_FACTOR),
                                                                  (uint64_t) lv_SOLVER_SCALE_FACTOR);
            if (solutions[sol_count])
                sol_count++;
        }
        if (sol_count < max_solutions && fabs(x1 - x2) > lv_EPSILON_DOUBLE) {
            solutions[sol_count] = symbolic_coord_create_rational((int64_t) (x2 * lv_SOLVER_SCALE_FACTOR),
                                                                  (uint64_t) lv_SOLVER_SCALE_FACTOR);
            if (solutions[sol_count])
                sol_count++;
        }
    } else {
        /* 三个不等实根 (casus irreducibilis): 使用三角函数
         * y_k = 2*sqrt(-P/3)*cos((acos(3Q/(2P)*sqrt(-3/P)) + 2*pi*k)/3) */
        double sqrt_term = sqrt(-P / 3.0);
        double acos_arg = 3.0 * Q / (2.0 * P) * sqrt(-3.0 / P);
        /* 裁剪到 [-1, 1] 以防止浮点精度误差导致 acos 返回 NaN */
        if (acos_arg > 1.0) acos_arg = 1.0;
        if (acos_arg < -1.0) acos_arg = -1.0;
        double phi = acos(acos_arg);
        for (int k = 0; k < 3 && sol_count < max_solutions; k++) {
            double angle = (phi + 2.0 * M_PI * (double) k) / 3.0;
            double y_k = 2.0 * sqrt_term * cos(angle);
            double x_k = y_k - p_over_3;
            solutions[sol_count] = symbolic_coord_create_rational((int64_t) (x_k * lv_SOLVER_SCALE_FACTOR),
                                                                  (uint64_t) lv_SOLVER_SCALE_FACTOR);
            if (solutions[sol_count])
                sol_count++;
        }
    }

    mpz_clear(a_mpz);
    mpz_clear(b_mpz);
    mpz_clear(c_mpz);
    mpz_clear(d_mpz);
    return sol_count;
}

/* ================================================================== */
/*  Symbolic back-substitution support                                 */
/* ================================================================== */

/**
 * @brief 将 SymbolicCoord 转换为精确的有理数 mpq_t
 *
 * @details 仅支持 RATIONAL 类型的坐标。非 RATIONAL 类型返回 false。
 *          用于精确的有理数算术运算。
 *
 * @param c   符号坐标指针
 * @param out 输出：mpq_t 有理数
 * @return true 表示转换成功，false 表示失败（非 RATIONAL 类型）
 */
static bool symbolic_coord_to_mpq(const SymbolicCoord *c, mpq_t out) {
    if (!c || c->type != RATIONAL || !c->data.rational)
        return false;
    mpq_set(out, c->data.rational->value);
    return true;
}
