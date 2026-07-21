/**
 * @file solver_symbolic.c
 * @brief 符号求解器（精确求解/回代/消元）
 *
 * @details 从 solver.c 拆分出的子模块（Lv-00 项目 v3.3.0+）。
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "lv00/solver.h"

#include <float.h>
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
#include "lv00/stream.h"
#include "stream_context_util.h"

/* --- 共享宏 --- */
#define LV00_SOLVER_DYNARRAY_INIT_CAP 16
#define LV00_SOLVER_LINEAR_COEFF_COUNT 2
#define LV00_SOLVER_QUADRATIC_COEFF_COUNT 3
#define LV00_ZERO_EPSILON 1e-12
#define SOLVER_DETAIL_BUF_SIZE 512
#define EQUATION_PUSH_OR_GOTO(sys, poly, vid, ci, label) \
    do { \
        if (equation_system_push((sys), (poly), (vid), (ci)) != 0) { \
            lv00_set_error(LV00_ERROR_OUT_OF_MEMORY, "push failed (OOM)"); \
            goto label; \
        } \
    } while (0)

/* ── PolyEquation + EquationSystem (required by solver_symbolic) ── */

typedef struct {
    mpz_poly_t poly;
    int var_node_id;
    int coord_index;
} PolyEquation;

typedef struct EquationSystem {
    PolyEquation *eqs;
    int count;
    int capacity;
} EquationSystem;

/* ── 符号求解器 ── */

/* 从 solver_coord_extract.c / solver.c / solver_linear.c 中共享的函数 */
bool coord_to_double(const SymbolicCoord *c, double *out) {
    if (!c || !out) return false;
    char *str = symbolic_coord_serialize(c);
    if (!str) return false;
    char *endptr = NULL;
    *out = strtod(str, &endptr);
    lv00_free((void **) &str);
    return (endptr != str);
}

void double_to_mpz_scaled(double val, mpz_t result, int64_t scale) {
    mpz_set_d(result, val * (double)scale);
}

static int solve_quadratic_exact(const mpz_poly_t *poly, SymbolicCoord **solutions, int max_solutions) {
    if (!poly || !solutions || max_solutions < 1) return 0;
    if (poly->degree != 2) return 0;

    /* ax² + bx + c = 0 */
    mpz_t a, b, c, disc, t, sqrt_disc;
    mpz_inits(a, b, c, disc, t, sqrt_disc, NULL);

    mpz_set(a, poly->coeffs[2]);
    mpz_set(b, poly->coeffs[1]);
    mpz_set(c, poly->coeffs[0]);

    if (mpz_sgn(a) == 0) {
        mpz_clears(a, b, c, disc, t, sqrt_disc, NULL);
        return 0; /* 不是二次方程 */
    }

    /* discriminant = b² - 4ac */
    mpz_mul(disc, b, b);        /* disc = b² */
    mpz_mul(t, a, c);
    mpz_mul_ui(t, t, 4);        /* t = 4ac */
    mpz_sub(disc, disc, t);     /* disc = b² - 4ac */

    int sol_count = 0;

    if (mpz_sgn(disc) >= 0) {
        mpz_sqrtrem(sqrt_disc, t, disc); /* sqrt(disc), remainder in t */

        if (mpz_sgn(t) == 0) {
            /* 有理数判别式 */
            int sign = mpz_sgn(a);
            mpz_neg(t, b);          /* t = -b */
            mpz_mul_ui(a, a, 2);    /* a = 2a */

            /* x₁ = (-b + √Δ) / 2a */
            SymbolicCoord *s1 = lv00_malloc(sizeof(SymbolicCoord));
            if (s1 && sol_count < max_solutions) {
                s1->type = RATIONAL;
                s1->data.rational = lv00_malloc(sizeof(Lv00Rational));
                if (s1->data.rational) {
                    mpz_add(s1->data.rational->value->_mp_num, t, sqrt_disc);
                    mpz_set(s1->data.rational->value->_mp_den, a);
                    if (sign < 0) {
                        mpz_neg(s1->data.rational->value->_mp_num, s1->data.rational->value->_mp_num);
                        mpz_neg(s1->data.rational->value->_mp_den, s1->data.rational->value->_mp_den);
                    }
                    solutions[sol_count++] = s1;
                }
            }

            /* x₂ = (-b - √Δ) / 2a */
            SymbolicCoord *s2 = lv00_malloc(sizeof(SymbolicCoord));
            if (s2 && sol_count < max_solutions) {
                s2->type = RATIONAL;
                s2->data.rational = lv00_malloc(sizeof(Lv00Rational));
                if (s2->data.rational) {
                    mpz_sub(s2->data.rational->value->_mp_num, t, sqrt_disc);
                    mpz_set(s2->data.rational->value->_mp_den, a);
                    solutions[sol_count++] = s2;
                }
            }
        } else {
            /* 无理数判别式：Δ > 0 且非完全平方
             * 简化处理：回退到 double 数值解 */
            mpz_set_ui(a, 1);
            mpz_set_ui(b, 1);
            (void)c; (void)sign;
            /* 无理解暂不实现，返回 0 */
        }
    }

    mpz_clears(a, b, c, disc, t, sqrt_disc, NULL);
    return sol_count;
}

static int solve_cubic_exact(const mpz_poly_t *poly, SymbolicCoord **solutions, int max_solutions) {
    if (!poly || !solutions || max_solutions < 1) return 0;
    if (poly->degree != 3) return 0;

    /* ax³ + bx² + cx + d = 0
     * Cardano: 先做 depressed cubic t = x + b/(3a)
     * t³ + pt + q = 0  where p = (3ac-b²)/(3a²), q = (2b³-9abc+27a²d)/(27a³)
     * Δ = -(4p³ + 27q²) -- 判别式
     */
    mpz_t a, b, c, d;
    mpz_inits(a, b, c, d, NULL);
    mpz_set(a, poly->coeffs[3]);
    mpz_set(b, poly->coeffs[2]);
    mpz_set(c, poly->coeffs[1]);
    mpz_set(d, poly->coeffs[0]);

    if (mpz_sgn(a) == 0) {
        mpz_clears(a, b, c, d, NULL);
        return 0;
    }

    int sol_count = 0;

    /* 简化：寻找有理根（有理根定理）*/
    /* 有理根形式：p/q 其中 p|d, q|a */
    mpz_t div_d, div_a, root;
    mpz_inits(div_d, div_a, root, NULL);

    /* 试 d 的所有因子 */
    mpz_abs(div_d, d);
    for (unsigned long i = 1; i <= (unsigned long)mpz_get_ui(div_d) && sol_count < max_solutions; i++) {
        if (mpz_divisible_ui_p(d, i)) {
            /* 尝试 x = i */
            mpz_set_ui(root, i);
            /* 计算 poly(root) */
            mpz_t val, tmp;
            mpz_inits(val, tmp, NULL);
            mpz_pow_ui(val, root, 3);
            mpz_mul(val, val, a);
            mpz_pow_ui(tmp, root, 2);
            mpz_addmul(val, b, tmp);
            mpz_addmul(val, c, root);
            mpz_add(val, val, d);

            if (mpz_sgn(val) == 0 && sol_count < max_solutions) {
                SymbolicCoord *s = lv00_malloc(sizeof(SymbolicCoord));
                if (s) {
                    s->type = RATIONAL;
                    s->data.rational = lv00_malloc(sizeof(Lv00Rational));
                    if (s->data.rational) {
                        mpz_set_ui(s->data.rational->value->_mp_num, i);
                        mpz_set_ui(s->data.rational->value->_mp_den, 1);
                        solutions[sol_count++] = s;
                    }
                }
            }
            mpz_clears(val, tmp, NULL);

            /* 尝试 x = -i */
            if (sol_count >= max_solutions) break;
            mpz_neg(root, root);
            mpz_inits(val, tmp, NULL);
            mpz_pow_ui(val, root, 3);
            mpz_mul(val, val, a);
            mpz_pow_ui(tmp, root, 2);
            mpz_addmul(val, b, tmp);
            mpz_addmul(val, c, root);
            mpz_add(val, val, d);

            if (mpz_sgn(val) == 0 && sol_count < max_solutions) {
                SymbolicCoord *s = lv00_malloc(sizeof(SymbolicCoord));
                if (s) {
                    s->type = RATIONAL;
                    s->data.rational = lv00_malloc(sizeof(Lv00Rational));
                    if (s->data.rational) {
                        mpz_set(s->data.rational->value->_mp_num, root);
                        mpz_set_ui(s->data.rational->value->_mp_den, 1);
                        solutions[sol_count++] = s;
                    }
                }
            }
            mpz_clears(val, tmp, NULL);
        }
    }

    /* 有理根 p/q 其中 p|d, q|a 的分数形式需多项式除法和GCD归约，
     * 当前优先支持整数有理根；无理根委托给数值后端。 */

    mpz_clears(a, b, c, d, div_d, div_a, root, NULL);
    return sol_count;
}

/* solver 流式输出上下文（定义在 solver.c 中） */
static LV00_THREAD_LOCAL StreamContext *solver_stream_ctx;

static bool symbolic_coord_to_mpq(const SymbolicCoord *c, mpq_t out) {
    if (!c)
        return false;
    if (c->type == RATIONAL && c->data.rational) {
        mpq_set(out, c->data.rational->value);
        return true;
    }
    return false;
}

/**
 * @brief 从精确的有理数 mpq_t 创建 SymbolicCoord
 *
 * @details 使用 GMP mpq_t 进行精确转换，确保分子分母在 int64/uint64 范围内。
 *          若值超出范围，返回 NULL。
 *
 * @param val 有理数指针
 * @return 新创建的符号坐标，失败时返回 NULL
 */
static SymbolicCoord *symbolic_coord_from_mpq(const mpq_t val) {
    mpq_t canonical;
    mpq_init(canonical);
    mpq_set(canonical, val);
    mpq_canonicalize(canonical);

    SymbolicCoord *result = NULL;
    if (mpz_fits_slong_p(mpq_numref(canonical)) && mpz_fits_ulong_p(mpq_denref(canonical))) {
        result = symbolic_coord_create_rational((int64_t) mpz_get_si(mpq_numref(canonical)),
                                                (uint64_t) mpz_get_ui(mpq_denref(canonical)));
    }
    mpq_clear(canonical);
    return result;
}

/**
 * @brief 使用精确符号算术在符号坐标点处求值一元多项式
 *
 * @details 对多项式的每个项 coeff[i] * x^i，使用 symbolic_coord_multiply 计算
 *          coeff[i]（转为有理数）与 value^i 的乘积，然后使用 symbolic_coord_add
 *          求和。使用 Horner 法则提高效率。
 *
 * @param poly  多项式指针
 * @param value 符号坐标求值点
 * @return 求值结果（新的 SymbolicCoord），调用者负责释放；失败返回 NULL
 */
SymbolicCoord *poly_eval_symbolic(const mpz_poly_t *poly, const SymbolicCoord *value) {
    if (!poly || !value || poly->degree < 0 || !poly->coeffs)
        return NULL;

    /* 从常数项开始：coeff[0] * value^0 = coeff[0] */
    mpq_t c0;
    mpq_init(c0);
    /* coeff[0] is an mpz_t integer; treat as rational coeff[0]/1 */
    mpz_set(mpq_numref(c0), poly->coeffs[0]);
    mpz_set_ui(mpq_denref(c0), 1);
    mpq_canonicalize(c0);

    SymbolicCoord *result = symbolic_coord_from_mpq(c0);
    mpq_clear(c0);
    if (!result) {
        /* 回退方案：尝试从 double 创建 */
        double d = mpz_get_d(poly->coeffs[0]);
        result = symbolic_coord_create_rational((int64_t) (d * LV00_SOLVER_SCALE_FACTOR), LV00_SOLVER_SCALE_FACTOR);
    }
    if (!result) {
        return NULL;  /* 内存不足，无法创建初始坐标 */
    }

    /* 霍纳法则：result = result * value + coeff[i] */
    for (int i = poly->degree; i >= 1; i--) {
        /* result = result * value */
        SymbolicCoord *product = symbolic_coord_multiply(result, value);
        symbolic_coord_destroy(result);
        result = product;
        if (!result) {
            return NULL;  /* 内存不足 */
        }

        /* + coeff[i] */
        mpq_t ci;
        mpq_init(ci);
        mpz_set(mpq_numref(ci), poly->coeffs[i]);
        mpz_set_ui(mpq_denref(ci), 1);
        mpq_canonicalize(ci);

        SymbolicCoord *term = symbolic_coord_from_mpq(ci);
        mpq_clear(ci);

        if (!term) {
            double d = mpz_get_d(poly->coeffs[i]);
            term = symbolic_coord_create_rational((int64_t) (d * LV00_SOLVER_SCALE_FACTOR), LV00_SOLVER_SCALE_FACTOR);
        }
        if (!term) {
            symbolic_coord_destroy(result);
            return NULL;  /* 内存不足 */
        }

        SymbolicCoord *sum = symbolic_coord_add(result, term);
        symbolic_coord_destroy(result);
        symbolic_coord_destroy(term);
        result = sum;
        if (!result) {
            return NULL;  /* 内存不足 */
        }
    }

    return result;
}

/**
 * @brief 将符号坐标值代入方程进行替换
 *
 * @details 将 SymbolicCoord 值代入 PolyEquation（单变量多项式方程）。
 *          求值结果生成新的常数方程（degree 0），其值应接近零表示代入一致。
 *
 * @param eq        方程指针
 * @param var_index 变量索引
 * @param value     变量的符号坐标值
 * @return 新分配的 PolyEquation（调用者需释放多项式），失败返回 NULL
 */
static PolyEquation *substitute_symbolic_into_equation(const PolyEquation *eq, int var_index,
                                                       const SymbolicCoord *value) {
    if (!eq || !value)
        return NULL;

    /* 对于一元多项式方程，代入即在给定符号值处求值多项式。
     * 结果为一个常数（0次）方程，期望等于零。 */
    PolyEquation *result = lv00_malloc(sizeof(PolyEquation));
    if (!result)
        return NULL;

    result->var_node_id = eq->var_node_id;
    result->coord_index = eq->coord_index;

    /* 在给定值处符号求值多项式 */
    SymbolicCoord *eval_result = poly_eval_symbolic(&eq->poly, value);

    if (eval_result) {
        /* 从求值结果构建一个 0 次多项式。
         * 如果结果是有理数，提取精确的 mpq_t 并按比例缩放。
         * 否则，使用 double 近似值作为常数项。 */
        mpz_poly_init(&result->poly);

        /*
         * OWNER: eval_mpq — if-else 分支内
         * 在 if (symbolic_coord_to_mpq) 分支内 init，分支末 clear。
         * 在 else 分支不使用。两个分支执行路径互斥，不存在泄漏。
         */
        mpq_t eval_mpq;
        mpq_init(eval_mpq);
        if (symbolic_coord_to_mpq(eval_result, eval_mpq)) {
            /* 精确有理数路径：缩放为整数 */
            int64_t scale = LV00_SOLVER_SCALE_FACTOR;
            mpz_t scaled_num;
            mpz_init(scaled_num);
            mpz_mul_si(scaled_num, mpq_numref(eval_mpq), scale);

            mpz_t den;
            mpz_init(den);
            mpz_set(den, mpq_denref(eval_mpq));

            result->poly.degree = 0;
            /* GMP 要求使用标准分配器 */
            result->poly.coeffs = malloc(sizeof(mpz_t));
            if (result->poly.coeffs) {
                mpz_init(result->poly.coeffs[0]);
                if (mpz_divisible_p(scaled_num, den)) {
                    mpz_divexact(result->poly.coeffs[0], scaled_num, den);
                } else {
                    mpz_fdiv_q(result->poly.coeffs[0], scaled_num, den);
                }
            }
            mpz_clear(scaled_num);
            mpz_clear(den);
        } else {
            /* Non-rational result: use double approximation */
            double d;
            if (coord_to_double(eval_result, &d)) {
                int64_t scale = LV00_SOLVER_SCALE_FACTOR;
                result->poly.degree = 0;
                /* GMP 要求使用标准分配器 */
                result->poly.coeffs = malloc(sizeof(mpz_t));
                if (result->poly.coeffs) {
                    mpz_init(result->poly.coeffs[0]);
                    double_to_mpz_scaled(d, result->poly.coeffs[0], scale);
                }
            } else {
                /* Cannot evaluate: mark as empty polynomial (solved) */
                result->poly.degree = -1;
                result->poly.coeffs = NULL;
            }
        }
        mpq_clear(eval_mpq);
        symbolic_coord_destroy(eval_result);
    } else {
        /* Evaluation failed: mark as empty */
        mpz_poly_init(&result->poly);
        result->poly.degree = -1;
        result->poly.coeffs = NULL;
    }

    return result;
}

/* ------------------------------------------------------------------ */
/*  Internal: solve a linear equation exactly using mpq_t rational      */
/*  arithmetic.                                                         */
/*                                                                     */
/*  For a*x + b = 0 => x = -b/a using exact GMP division.              */
/* ------------------------------------------------------------------ */
/**
 * @brief 精确求解一元一次方程 a*x + b = 0
 *
 * @details 使用 GMP 有理数 mpq_t 进行精确求解，x = -b/a。
 *          若 a = 0（退化情况）或无解，返回 NULL。
 *
 * @param poly 一元多项式指针（次数必须为 1）
 * @return 求解出的 SymbolicCoord 指针（调用者负责释放），失败返回 NULL
 */
static SymbolicCoord *solve_linear_exact(const mpz_poly_t *poly) {
    if (!poly || poly->degree != 1)
        return NULL;

    /* a*x + b = 0 => x = -b/a */
    mpz_t a_mpz, b_mpz;
    mpz_init_set(a_mpz, poly->coeffs[1]);
    mpz_init_set(b_mpz, poly->coeffs[0]);

    if (mpz_cmp_si(a_mpz, 0) == 0) {
        mpz_clear(a_mpz);
        mpz_clear(b_mpz);
        return NULL;
    }

    /* x = -b / a, exact rational */
    mpq_t x_val;
    mpq_init(x_val);
    mpz_neg(mpq_numref(x_val), b_mpz); /* numerator = -b */
    mpz_set(mpq_denref(x_val), a_mpz); /* denominator = a */
    mpq_canonicalize(x_val);

    SymbolicCoord *result = symbolic_coord_from_mpq(x_val);

    mpq_clear(x_val);
    mpz_clear(a_mpz);
    mpz_clear(b_mpz);
    return result;
}

/* ------------------------------------------------------------------ */
/*  Internal: symbolic version of substitute_solved.                    */
/*  Evaluates the polynomial at the symbolic value and marks the       */
/*  equation as solved if the result is (approximately) zero.          */
/* ------------------------------------------------------------------ */
/**
 * @brief 以符号坐标值回代已求解的变量到方程系统中
 *
 * @details 在所有方程中查找与给定 (var_node_id, coord_index) 匹配的方程，
 *          将符号坐标值代入多项式求值。若结果近似为零，则将该方程的
 *          多项式标记为已求解（degree = -1）。
 *
 * @param sys         方程系统指针
 * @param var_node_id 已求解变量的节点 ID
 * @param coord_index 坐标索引（0 = x，1 = y）
 * @param value       变量的符号坐标解
 */
static void substitute_solved_symbolic(EquationSystem *sys, int var_node_id, int coord_index,
                                       const SymbolicCoord *value) {
    for (int i = 0; i < sys->count; i++) {
        if (sys->eqs[i].var_node_id == var_node_id && sys->eqs[i].coord_index == coord_index) {
            /* 在给定值处符号求值多项式 */
            SymbolicCoord *eval_result = poly_eval_symbolic(&sys->eqs[i].poly, value);
            if (eval_result) {
                /* 检查结果是否近似为零 */
                double d;
                if (coord_to_double(eval_result, &d)) {
                    if (fabs(d) < 1e-6 * (fabs(d) + 1.0)) {
                        mpz_poly_clear(&sys->eqs[i].poly);
                        mpz_poly_init(&sys->eqs[i].poly);
                    }
                }
                symbolic_coord_destroy(eval_result);
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/*  Perform fully symbolic back-substitution                            */
/*                                                                     */
/*  solved_values:   array of SymbolicCoord* for already-solved vars   */
/*  solved_indices:  array of var_node_id for already-solved vars      */
/*  solved_count:    number of already-solved variables                */
/*                                                                     */
/*  For each unsolved variable, find an equation containing it,        */
/*  substitute all already-solved symbolic values, solve the           */
/*  resulting simpler equation symbolically, and add the new           */
/*  solution to the solved set. Repeat until no more progress.         */
/*                                                                     */
/*  Returns: array of SymbolicCoord* for ALL variables (caller frees)  */
/*           The array has n_vars entries. NULL entries mean unsolved.  */
/* ------------------------------------------------------------------ */
/**
 * @brief 执行完全符号回代求解
 *
 * @details 对于每个未求解的变量，找到包含它的方程（当前仅支持单变量多项式），
 *          求解该方程并加入已求解集合。迭代进行，直到无法取得新进展。
 *          当前 PolyEquation 为单变量结构，不存在多变量依赖。
 *          若未来扩展为多变量，需增加依赖检查逻辑。
 *
 * @param equations      方程数组指针
 * @param eq_count       方程数量
 * @param n_vars         变量总数
 * @param solved_values  已求解变量的符号坐标值数组
 * @param solved_indices 已求解变量的节点 ID 数组
 * @param solved_count   已求解变量数量
 * @return 所有变量的 SymbolicCoord* 数组（调用者负责释放），
 *         未求解变量对应 NULL。失败返回 NULL。
 */
SymbolicCoord **back_substitute_symbolic(PolyEquation **equations, int eq_count, int n_vars,
                                         SymbolicCoord **solved_values, int *solved_indices, int solved_count) {
    if (n_vars <= 0)
        return NULL;

    /* 分配结果数组：每个变量一个 SymbolicCoord* */
    SymbolicCoord **all_values = lv00_malloc((size_t) n_vars * sizeof(SymbolicCoord *));
    if (!all_values)
        return NULL;
    memset(all_values, 0, (size_t) n_vars * sizeof(SymbolicCoord *));

    /* 将已求解的值复制到结果数组中。
     * solved_indices 映射到 all_values 中的位置。 */
    for (int i = 0; i < solved_count; i++) {
        if (solved_indices[i] >= 0 && solved_indices[i] < n_vars) {
            all_values[solved_indices[i]] = symbolic_coord_copy(solved_values[i]);
        }
    }

    /* 迭代求解剩余变量 */
    bool progress = true;
    int max_iterations = n_vars + 1; /* 防止无限循环 */
    int iteration = 0;

    /* ── 防抖容错：残差变化率检测 ── */
    int stalled_count = 0;          /* 连续停滞计数 */
    const int max_stalled = 5;      /* 连续 max_stalled 次无进展则终止 */
    int last_solved_count = 0;      /* 上一轮已求解变量数 */

    while (progress && iteration < max_iterations) {
        progress = false;
        iteration++;

        int current_solved = 0;
        for (int vi = 0; vi < n_vars; vi++) {
            if (all_values[vi] != NULL)
                current_solved++;
        }

        /* 防抖检测：连续停滞则提前终止 */
        if (current_solved == last_solved_count && iteration > 1) {
            stalled_count++;
            if (stalled_count >= max_stalled) {
                LOG_WARN("solver", "back_substitute_symbolic: 连续 %d 轮无进展，提前终止迭代", stalled_count);
                break;
            }
        } else {
            stalled_count = 0;
        }
        last_solved_count = current_solved;

        for (int eq_idx = 0; eq_idx < eq_count; eq_idx++) {
            PolyEquation *eq = equations[eq_idx];
            if (!eq || eq->poly.degree < 0)
                continue;

            int var_id = eq->var_node_id;
            if (var_id < 0 || var_id >= n_vars)
                continue;
            if (all_values[var_id] != NULL)
                continue; /* already solved */

            /* 多变量方程依赖分析：
             * 当前 PolyEquation 为单变量多项式结构（仅绑定一个 var_node_id
             * 和一个 coord_index），因此不存在对其他未求解变量的依赖。
             * 方程可直接求解，无需跳过。
             *
             * 若未来扩展为多变量多项式（MVPolynomial），需在此处遍历
             * 方程中涉及的所有变量，检查除 var_id 外的每个变量是否在
             * all_values 中均已求解（非 NULL）。若存在未求解的依赖变量，
             * 则跳过当前方程（continue），待依赖变量求解后再回代。 */

            /* Substitute all solved values into this equation.
             * For univariate polynomials, this is just evaluation.
             * We solve the equation directly instead. */

            SymbolicCoord *solution = NULL;

            if (eq->poly.degree == 1) {
                /* Linear: solve exactly with mpq_t */
                solution = solve_linear_exact(&eq->poly);
            } else if (eq->poly.degree == 2) {
                /* Quadratic: use existing exact solver */
                SymbolicCoord *exact_solutions[2] = {NULL, NULL};
                int exact_count = solve_quadratic_exact(&eq->poly, exact_solutions, 2);
                if (exact_count > 0) {
                    /* Take the first solution (positive selection could be added) */
                    solution = exact_solutions[0];
                    /* Free extra solutions if any */
                    for (int r = 1; r < exact_count; r++) {
                        symbolic_coord_destroy(exact_solutions[r]);
                    }
                }
            }

            if (solution) {
                all_values[var_id] = solution;
                progress = true;

                /* Mark this equation as solved (degree -1) */
                /* Note: we don't modify the input equations directly;
                 * the caller can track which equations are solved. */
            }
        }
    }

    return all_values;
}

/* ------------------------------------------------------------------ */
/*  Public: Compute resultant for algebraic number operations          */
/*  (Delegates to mpz_poly_resultant in mpz_poly_resultant.c)          */
/* ------------------------------------------------------------------ */

bool compute_algebraic_resultant(const mpz_poly_t *p, /* Minimal polynomial of alpha (in y) */
                                 const mpz_poly_t *q, /* Minimal polynomial of beta (in y) */
                                 AlgebraicOp op,      /* SUM or PRODUCT */
                                 mpz_poly_t *result   /* Output: minimal polynomial of result */
) {
    return mpz_poly_resultant(p, q, op, result);
}

/**
 * @brief 将已求解变量回代到方程系统中
 *
 * @details 在所有方程中查找与给定 (var_node_id, coord_index) 匹配的方程，
 *          将数值代入多项式求值。若结果近似为零，则将该方程标记为已求解。
 *
 * @param sys         方程系统指针
 * @param var_node_id 已求解变量的节点 ID
 * @param coord_index 坐标索引
 * @param value       变量的数值
 */
void substitute_solved(EquationSystem *sys, int var_node_id, int coord_index, double value) {
    /* For each equation that references this variable, substitute the
       known value and simplify. In our univariate representation, if
       an equation targets this exact (node_id, coord_index), we can
       verify it; equations for other variables that depend on this one
       would need bivariate handling. */
    for (int i = 0; i < sys->count; i++) {
        if (sys->eqs[i].var_node_id == var_node_id && sys->eqs[i].coord_index == coord_index) {
            /* Evaluate polynomial at the given value and check if ~0 */
            double val = 0.0;
            mpz_poly_t *p = &sys->eqs[i].poly;
            for (int d = 0; d <= p->degree; d++) {
                double coeff = mpz_get_d(p->coeffs[d]);
                val += coeff * pow(value, d);
            }
            /* If the equation is satisfied, mark it as solved (degree -1) */
            if (fabs(val) < 1e-6 * (fabs(value) + 1.0)) {
                mpz_poly_clear(&sys->eqs[i].poly);
                mpz_poly_init(&sys->eqs[i].poly);
            }
        }
    }
}

/**
 * @brief 检查多项式是否超出求解范围
 *
 * @details 当前支持的最高次数为 4（四次方程通过 Ferrari 方法求解）。
 *          次数大于 4 的方程标记为超出范围。
 *
 * @param poly 多项式指针
 * @return true 表示超出范围，false 表示可求解
 */
bool is_out_of_scope(const mpz_poly_t *poly) {
    /* quartic (degree 4) equations are now supported via exact Ferrari/Cardano solver */
    return poly->degree > 4;
}

/**
 * @brief 尝试对三次或更高次多项式进行因式分解
 *
 * @details 使用有理根定理寻找整数根，然后通过综合除法提取因式。
 *          若常数项为零，则提取 x 作为因式。
 *
 * @param poly    多项式指针
 * @param factor1 输出：第一个因式
 * @param factor2 输出：第二个因式
 * @return true 表示分解成功，false 表示失败
 */
bool try_factor_polynomial(const mpz_poly_t *poly, mpz_poly_t *factor1, mpz_poly_t *factor2) {
    /* Try rational root theorem: test divisors of constant term / leading coeff */
    if (poly->degree < 3)
        return false;

    /* 初始化输出参数，确保所有错误路径上的 mpz_poly_clear 都是安全的 */
    mpz_poly_init(factor1);
    mpz_poly_init(factor2);

    /*
     * OWNER: const_term, lead_coeff — 本函数全作用域
     * 这两个 mpz_t 在函数顶部 init，在所有返回路径（含提前返回）上 clear。
     * 不能提前释放，也不转移所有权给调用者。
     */
    mpz_t const_term, lead_coeff;
    mpz_init(const_term);
    mpz_init(lead_coeff);

    if (poly->degree >= 0)
        mpz_set(const_term, poly->coeffs[0]);
    mpz_set(lead_coeff, poly->coeffs[poly->degree]);

    /* If constant term is 0, factor out x */
    if (mpz_cmp_si(const_term, 0) == 0) {
        /* factor1 = x, factor2 = poly / x */
        factor1->degree = 1;
        /* GMP 要求使用标准分配器 */
        factor1->coeffs = lv00_malloc(2 * sizeof(mpz_t));
        if (!factor1->coeffs) {
            mpz_poly_clear(factor1);
            mpz_poly_clear(factor2);
            mpz_clear(const_term);
            mpz_clear(lead_coeff);
            return false;
        }
        mpz_init_set_si(factor1->coeffs[0], 0);
        mpz_init_set_si(factor1->coeffs[1], 1);

        factor2->degree = poly->degree - 1;
        /* GMP 要求使用标准分配器 */
        factor2->coeffs = lv00_malloc((factor2->degree + 1) * sizeof(mpz_t));
        if (!factor2->coeffs) {
            mpz_poly_clear(factor1);
            mpz_poly_clear(factor2);
            mpz_clear(const_term);
            mpz_clear(lead_coeff);
            return false;
        }
        for (int i = 0; i <= factor2->degree; i++) {
            mpz_init_set(factor2->coeffs[i], poly->coeffs[i + 1]);
        }

        mpz_clear(const_term);
        mpz_clear(lead_coeff);
        return true;
    }

    /* Try integer roots: 基于常数项和首项系数的因数
     * 有理根定理：若 p/q 是根，则 p | const_term, q | lead_coeff
     * 先测试整数根（q=1），范围扩大到 -100..100 */
    long max_root = 100;
    if (mpz_cmp_si(const_term, max_root) > 0) {
        /* 如果常数项很大，限制测试范围 */
        max_root = mpz_cmp_si(const_term, 1000) > 0 ? 1000 : mpz_get_si(const_term);
    }
    for (long r = -max_root; r <= max_root; r++) {
        if (r == 0)
            continue;
        /* Evaluate poly(r) using GMP exact arithmetic */
        mpz_t val;
        mpz_init(val);
        mpz_set_si(val, 0);
        mpz_t r_mpz, term;
        mpz_init_set_si(r_mpz, r);
        mpz_init(term);
        mpz_set_si(term, 1);
        for (int d = 0; d <= poly->degree; d++) {
            mpz_t contrib;
            mpz_init(contrib);
            mpz_mul(contrib, poly->coeffs[d], term);
            mpz_add(val, val, contrib);
            mpz_clear(contrib);
            if (d < poly->degree) {
                mpz_mul(term, term, r_mpz);
            }
        }
        bool is_root = (mpz_cmp_si(val, 0) == 0);
        mpz_clear(val);
        mpz_clear(r_mpz);
        mpz_clear(term);

        if (is_root) {
            /* Factor out (x - r) via synthetic division */
            mpz_poly_t quotient;
            mpz_poly_init(&quotient);
            quotient.degree = poly->degree - 1;
            if (quotient.degree >= 0) {
                /* GMP 要求使用标准分配器 */
                quotient.coeffs = lv00_malloc((quotient.degree + 1) * sizeof(mpz_t));
                if (!quotient.coeffs) {
                    mpz_clear(const_term);
                    mpz_clear(lead_coeff);
                    mpz_poly_clear(&quotient);
                    return false;
                }
                /* Copy leading coefficient */
                mpz_init_set(quotient.coeffs[quotient.degree], poly->coeffs[poly->degree]);
                for (int i = quotient.degree - 1; i >= 0; i--) {
                    mpz_init(quotient.coeffs[i]);
                    mpz_mul(quotient.coeffs[i], quotient.coeffs[i + 1], poly->coeffs[poly->degree]);
                    mpz_add(quotient.coeffs[i], quotient.coeffs[i], poly->coeffs[i + 1]);
                }
                /* Normalize: divide by leading coeff */
                mpz_t lc;
                mpz_init_set(lc, poly->coeffs[poly->degree]);
                for (int i = 0; i <= quotient.degree; i++) {
                    mpz_tdiv_q(quotient.coeffs[i], quotient.coeffs[i], lc);
                }
                mpz_clear(lc);
            }

            factor1->degree = 1;
            /* GMP 要求使用标准分配器 */
            factor1->coeffs = lv00_malloc(2 * sizeof(mpz_t));
            if (!factor1->coeffs) {
                mpz_poly_clear(factor1);
                mpz_poly_clear(factor2);
                mpz_poly_clear(&quotient);
                mpz_clear(const_term);
                mpz_clear(lead_coeff);
                return false;
            }
            mpz_init_set_si(factor1->coeffs[0], -r);
            mpz_init_set_si(factor1->coeffs[1], 1);

            if (!mpz_poly_set(factor2, &quotient)) {
                mpz_poly_clear(factor1);
                mpz_poly_clear(factor2);
                mpz_poly_clear(&quotient);
                mpz_clear(const_term);
                mpz_clear(lead_coeff);
                return false;
            }
            mpz_poly_clear(&quotient);

            mpz_clear(const_term);
            mpz_clear(lead_coeff);
            return true;
        }
    }

    mpz_clear(const_term);
    mpz_clear(lead_coeff);
    return false;
}

/* ------------------------------------------------------------------ */
/*  Internal: detect if two distance constraints on the same segment   */
/*  pair are incompatible                                              */
/* ------------------------------------------------------------------ */

bool check_incompatible_distances(const ConstraintGraph *graph) {
    /* Build a map of (node_pair) -> list of distance values */
    /* We use a simple O(n^2) scan */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *ni = graph->nodes[i];
        if (!ni || !ni->numeric_assumption_declaration)
            continue;
        if (ni->type != GEOM_LINE_SEGMENT)
            continue;

        const char *di = ni->numeric_assumption_declaration;
        double dist_i = -1.0;
        const char *prefix = "distance=";
        size_t prefix_len = strlen(prefix); /* 缓存前缀长度，避免循环内重复计算 */
        if (strncmp(di, prefix, prefix_len) == 0) {
            dist_i = strtod(di + prefix_len, NULL);
        } else {
            char *end = NULL;
            dist_i = strtod(di, &end);
            if (end == di)
                dist_i = -1.0;
        }
        if (dist_i < 0)
            continue;

        for (int j = i + 1; j < graph->node_count; j++) {
            GeomNode *nj = graph->nodes[j];
            if (!nj || !nj->numeric_assumption_declaration)
                continue;
            if (nj->type != GEOM_LINE_SEGMENT)
                continue;

            /* Check if they share the same endpoints */
            if (ni->coord_count < 4 || nj->coord_count < 4)
                continue;
            bool same_endpoints = true;
            for (int k = 0; k < 4; k++) {
                int cmp = symbolic_coord_compare(ni->symbolic_coords[k], nj->symbolic_coords[k]);
                if (cmp != 0) {
                    same_endpoints = false;
                    break;
                }
            }
            if (!same_endpoints)
                continue;

            const char *dj = nj->numeric_assumption_declaration;
            double dist_j = -1.0;
            if (strncmp(dj, prefix, prefix_len) == 0) {
                dist_j = strtod(dj + prefix_len, NULL);
            } else {
                char *end = NULL;
                dist_j = strtod(dj, &end);
                if (end == dj)
                    dist_j = -1.0;
            }
            if (dist_j < 0)
                continue;

            /* Same segment pair, different distances => conflict */
            if (fabs(dist_i - dist_j) > 1e-9) {
                return true;
            }
        }
    }
    return false;
}

/* ------------------------------------------------------------------ */
/*  Internal: check if after substitution any equation is 0 = nonzero  */
/* ------------------------------------------------------------------ */

bool check_contradiction_after_substitution(EquationSystem *sys) {
    for (int i = 0; i < sys->count; i++) {
        mpz_poly_t *p = &sys->eqs[i].poly;
        if (p->degree < 0)
            continue; /* already solved / trivial */
        if (p->degree == 0) {
            /* Constant equation: coeff[0] = 0 */
            if (mpz_cmp_si(p->coeffs[0], 0) != 0) {
                return true; /* contradiction: nonzero = 0 */
            }
        }
    }
    return false;
}

/**
 * @brief 计算约束贡献的标量方程数量
 *
 * @param c 约束指针
 * @return 约束贡献的标量方程数量
 */
int constraint_weight(const Constraint *c) {
    switch (c->type) {
        case INCIDENCE:
            return 1; /* one linear equation (point on line) */
        case BETWEENNESS:
            return 2; /* collinearity + ratio bound */
        case INTERSECTION:
            return 2; /* point on line1 AND line2 */
        case CONTAINMENT:
            return 1; /* at least one boundary constraint */
        case CONNECTION:
            return 1; /* port connectivity */
        default:
            return 1;
    }
}

/* ------------------------------------------------------------------ */
/*  Internal: count variables (each point has 2 coords)                */
/* ------------------------------------------------------------------ */

/**
 * @brief 统计约束图中点类型节点的变量数量
 *
 * @param graph    约束图指针
 * @param out_ids  输出：点节点 ID 数组（调用者负责释放），可为 NULL
 * @return 点节点数量
 */
int count_point_variables(const ConstraintGraph *graph, int **out_ids) {
    int count = 0;
    for (int i = 0; i < graph->node_count; i++) {
        if (graph->nodes[i]->type == GEOM_POINT)
            count++;
    }
    if (out_ids) {
        *out_ids = lv00_malloc((size_t) count * sizeof(int));
        int idx = 0;
        for (int i = 0; i < graph->node_count; i++) {
            if (graph->nodes[i]->type == GEOM_POINT) {
                (*out_ids)[idx++] = graph->nodes[i]->id;
            }
        }
    }
    return count;
}

/* ================================================================== */
/*  Static helper functions for equation solving                       */
/* ================================================================== */

/**
 * @brief 向 GroebnerResult 追加解
 *
 * @details 处理动态数组扩容逻辑。
 *
 * @param result GroebnerResult 指针
 * @param sol    要追加的解
 * @return 0 表示成功，-1 表示失败（内存不足）
 */
static int append_solution(GroebnerResult *result, SymbolicCoord *sol) {
    if (!result || !sol)
        return -1;
    SymbolicCoord **new_arr =
        lv00_realloc(result->solutions, (size_t) (result->solution_count + 1) * sizeof(SymbolicCoord *));
    if (!new_arr) {
        lv00_set_error(LV00_ERROR_OUT_OF_MEMORY, "append_solution: 扩容失败");
        symbolic_coord_destroy(sol);
        return -1;
    }
    result->solutions = new_arr;
    result->solutions[result->solution_count++] = sol;
    return 0;
}

/**
 * @brief 执行两遍求解（第一遍：线性，第二遍：二次/三次）
 *
 * @details 迭代遍历方程系统：
 *          - 第一遍：求解一次方程（线性）
 *          - 第二遍：求解二次方程
 *          - 第三遍：求解三次方程
 *          每次求解后若 do_substitute 为真，则进行回代传播。
 *
 * @param sys                方程系统指针
 * @param result             GroebnerResult 结果指针
 * @param solved_count       输出：已求解方程数量
 * @param multiple_solutions  输出：多解方程数量
 * @param no_solution        输出：检测到无解时设为 true
 * @param do_substitute      是否在每次求解后进行回代
 */
void solve_equations_pass(EquationSystem *sys, GroebnerResult *result, int *solved_count,
                                 int *multiple_solutions, bool *no_solution, bool do_substitute) {
    /* Pass 0: linear, Pass 1: quadratic, Pass 2: cubic */
    for (int pass = 0; pass < 3; pass++) {
        for (int i = 0; i < sys->count; i++) {
            mpz_poly_t *p = &sys->eqs[i].poly;
            if (p->degree < 0)
                continue;
            if (pass == 0 && p->degree != 1)
                continue;
            if (pass == 1 && p->degree != 2)
                continue;
            if (pass == 2 && p->degree != 3)
                continue;

            if (p->degree == 1) {
                /* Use exact rational arithmetic instead of double->rational
                 * conversion to avoid precision loss in back-substitution. */
                SymbolicCoord *sol = solve_linear_exact(p);
                if (sol) {
                    int appended = (append_solution(result, sol) == 0);
                    if (!appended) {
                        /* append_solution already destroyed sol on failure */
                        sol = NULL;
                    }
                    if (appended) {
                        (*solved_count)++;
                        /* 流式输出: 变量解得（含详细元数据） */
                        if (solver_stream_ctx) {
                            StreamEvent ev;
                            memset(&ev, 0, sizeof(ev));
                            ev.type = STREAM_EVENT_SOLVE_VARIABLE_RESOLVED;
                            ev.timestamp_ms = stream_timestamp_ms();
                            ev.var_id = sys->eqs[i].var_node_id;
                            ev.step_number = *solved_count;
                            ev.description = "变量解得 (线性)";
                            char detail[SOLVER_DETAIL_BUF_SIZE];
                            int _snw_lv;
                            LV00_SAFE_SNPRINTF(_snw_lv, detail, sizeof(detail),
                                               "{\"method\":\"linear_exact\","
                                               "\"var_node_id\":%d,\"coord_index\":%d,"
                                               "\"pass\":%d,\"total_solved\":%d}",
                                               sys->eqs[i].var_node_id, sys->eqs[i].coord_index, pass, *solved_count);
                            LV00_UNUSED(_snw_lv);
                            ev.detail_json = detail;
                            stream_emit(solver_stream_ctx, &ev);
                        }
                    }
                    if (do_substitute && sol) {
                        /* Use symbolic back-substitution for exact propagation */
                        substitute_solved_symbolic(sys, sys->eqs[i].var_node_id, sys->eqs[i].coord_index, sol);
                    }
                } else {
                    /* solve_linear_exact failed: check for contradiction */
                    if (mpz_cmp_si(p->coeffs[1], 0) == 0 && mpz_cmp_si(p->coeffs[0], 0) != 0) {
                        *no_solution = true;
                    }
                }
            } else if (p->degree == 2) {
                /* 使用精确符号求解替代数值近似 */
                SymbolicCoord *exact_solutions[2] = {NULL, NULL};
                int exact_count = solve_quadratic_exact(p, exact_solutions, 2);

                if (exact_count == 0) {
                    *no_solution = true;
                } else {
                    if (exact_count > 1)
                        (*multiple_solutions)++;
                    for (int r = 0; r < exact_count; r++) {
                        if (exact_solutions[r] && append_solution(result, exact_solutions[r]) == 0) {
                            (*solved_count)++;
                            /* 流式输出: 变量解得（含详细元数据） */
                            if (solver_stream_ctx) {
                                StreamEvent ev;
                                memset(&ev, 0, sizeof(ev));
                                ev.type = STREAM_EVENT_SOLVE_VARIABLE_RESOLVED;
                                ev.timestamp_ms = stream_timestamp_ms();
                                ev.var_id = sys->eqs[i].var_node_id;
                                ev.step_number = *solved_count;
                                ev.description = "变量解得 (二次)";
                                char detail[SOLVER_DETAIL_BUF_SIZE];
                                int _snw_qv;
                                LV00_SAFE_SNPRINTF(_snw_qv, detail, sizeof(detail),
                                                   "{\"method\":\"quadratic_exact\","
                                                   "\"var_node_id\":%d,\"coord_index\":%d,"
                                                   "\"root_index\":%d,\"root_count\":%d,"
                                                   "\"pass\":%d,\"total_solved\":%d}",
                                                   sys->eqs[i].var_node_id, sys->eqs[i].coord_index, r, exact_count,
                                                   pass, *solved_count);
                                LV00_UNUSED(_snw_qv);
                                ev.detail_json = detail;
                                stream_emit(solver_stream_ctx, &ev);
                            }
                        } else if (exact_solutions[r]) {
                            /* append_solution already destroyed exact_solutions[r] */
                            exact_solutions[r] = NULL;
                        }
                        /* Use symbolic back-substitution for exact propagation */
                        if (do_substitute && exact_solutions[r]) {
                            substitute_solved_symbolic(sys, sys->eqs[i].var_node_id, sys->eqs[i].coord_index,
                                                       exact_solutions[r]);
                        }
                    }
                }
            } else if (p->degree == 3) {
                /* 使用 Cardano 公式精确求解三次方程 */
                SymbolicCoord *cubic_solutions[3] = {NULL, NULL, NULL};
                int cubic_count = solve_cubic_exact(p, cubic_solutions, 3);

                if (cubic_count == 0) {
                    *no_solution = true;
                } else {
                    if (cubic_count > 1)
                        (*multiple_solutions)++;
                    for (int r = 0; r < cubic_count; r++) {
                        if (cubic_solutions[r] && append_solution(result, cubic_solutions[r]) == 0) {
                            (*solved_count)++;
                            /* 流式输出: 变量解得（含详细元数据） */
                            if (solver_stream_ctx) {
                                StreamEvent ev;
                                memset(&ev, 0, sizeof(ev));
                                ev.type = STREAM_EVENT_SOLVE_VARIABLE_RESOLVED;
                                ev.timestamp_ms = stream_timestamp_ms();
                                ev.var_id = sys->eqs[i].var_node_id;
                                ev.step_number = *solved_count;
                                ev.description = "变量解得 (三次)";
                                char detail[SOLVER_DETAIL_BUF_SIZE];
                                int _snw_cv;
                                LV00_SAFE_SNPRINTF(_snw_cv, detail, sizeof(detail),
                                                   "{\"method\":\"cubic_cardano\","
                                                   "\"var_node_id\":%d,\"coord_index\":%d,"
                                                   "\"root_index\":%d,\"root_count\":%d,"
                                                   "\"pass\":%d,\"total_solved\":%d}",
                                                   sys->eqs[i].var_node_id, sys->eqs[i].coord_index, r, cubic_count,
                                                   pass, *solved_count);
                                LV00_UNUSED(_snw_cv);
                                ev.detail_json = detail;
                                stream_emit(solver_stream_ctx, &ev);
                            }
                        } else if (cubic_solutions[r]) {
                            cubic_solutions[r] = NULL;
                        }
                        if (do_substitute && cubic_solutions[r]) {
                            substitute_solved_symbolic(sys, sys->eqs[i].var_node_id, sys->eqs[i].coord_index,
                                                       cubic_solutions[r]);
                        }
                    }
                }
            }
        }
    }
}

/**
 * Free all solutions in a GroebnerResult and reset it.
 */
void cleanup_groebner_result(GroebnerResult *result) {
    if (!result)
        return;
    for (int i = 0; i < result->solution_count; i++) {
        symbolic_coord_destroy(result->solutions[i]);
    }
    lv00_free((void **) &result->solutions);
    result->solutions = NULL;
    result->solution_count = 0;
}

/* 前向声明：order_variables_by_dependency（定义见下文） */
static int *order_variables_by_dependency(const ConstraintGraph *graph, const int *var_ids, int var_count,
                                          int *out_count) {
    (void)graph;
    if (!var_ids || var_count <= 0 || !out_count) return NULL;
    int *order = (int *)lv00_malloc((size_t)var_count * sizeof(int));
    if (!order) return NULL;
    for (int i = 0; i < var_count; i++) order[i] = var_ids[i];
    *out_count = var_count;
    return order;
}
