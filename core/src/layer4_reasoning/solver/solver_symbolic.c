/**
 * @file solver_symbolic.c
 * @brief 符号求解器（精确求解/回代/消元）
 *
 * @details 从 solver.c 拆分出的子模块（Lv-00 项目 v3.3.0+）。
 *          包含实际求解逻辑：二次方程精确求解（GMP 判别式）、
 *          三次方程 Cardano 公式求解、Groebner 基回代与结果清理。
 * @author Lv-00 Project
 * @version 3.4.0
 */

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "lv/constraint_graph.h"
#include "lv/solver.h"
#include "lv/stream.h"
#include "lv/symbolic_coord.h"

#include "debug.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "mpz_poly.h"
#include "stream_context_util.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* --- 共享宏 --- */
#define lv_SOLVER_DYNARRAY_INIT_CAP 16
#define lv_ZERO_EPSILON 1e-12

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

/* ── 符号求解器桩实现 ── */

int coord_to_double(const SymbolicCoord *c, double *out) {
    if (!c || !out)
        return 0;
    switch (c->type) {
        case RATIONAL:
            *out = mpq_get_d(c->data.rational->value);
            return 1;
        case ALGEBRAIC:
            *out = algebraic_to_double(c->data.algebraic);
            return 1;
        case QUADRATIC:
            *out = quadratic_to_double(c->data.quadratic);
            return 1;
        case TRANSCENDENTAL:
            *out = transcendental_to_double(c->data.transcendental);
            return 1;
        default:
            return 0;
    }
}

/**
 * @brief 将 double 值按 scale 缩放后精确转换为 mpz_t 整数
 *
 * @details 使用 mpq_t 中间类型进行缩放计算，避免 double * int64 相乘后
 *          mpz_set_d 截断（向零舍入）导致的精度损失。
 *
 *          计算过程：
 *          1. val → mpq_t（精确的 double→有理数转换）
 *          2. mpq_numref(q) * scale → 缩放后的分子
 *          3. floor(分子 / 分母) → mpz_t 结果
 *
 *          举例：val = 1.0/3.0 ≈ 0.333..., scale = 1000
 *          旧版本：0.333... * 1000 ≈ 333.33..., mpz_set_d → 333（截断，错误）
 *          新版本：6004799503160661/18014398509481984 * 1000 → 精确 floor → 333
 *
 * @param val    输入浮点数值
 * @param result 输出 mpz_t 整数（floor(val * scale)）
 * @param scale  缩放因子（正数）
 */
void double_to_mpz_scaled(double val, mpz_t result, int64_t scale) {
    mpq_t q;
    mpq_init(q);
    mpq_set_d(q, val);                                      /* val → 精确有理数 */
    mpz_mul_si(mpq_numref(q), mpq_numref(q), (long) scale); /* 分子 * scale */
    mpz_fdiv_q(result, mpq_numref(q), mpq_denref(q));       /* floor(分子/分母) */
    mpq_clear(q);
}

/**
 * @brief 在符号坐标上求值多项式
 * @details 将多项式系数在 SymbolicCoord 值上逐项求值并累加。
 *          多项式 p(x) = Σ coeffs[i] * x^i，每项用 symbolic_coord_pow 和
 *          symbolic_coord_multiply 计算，最后用 symbolic_coord_add 累加。
 * @param poly  整数系数多项式
 * @param value 求值点（符号坐标）
 * @return 求值结果（新分配的 SymbolicCoord），失败返回 NULL
 */
SymbolicCoord *poly_eval_symbolic(const mpz_poly_t *poly, const SymbolicCoord *value) {
    if (!poly || !value || poly->degree < 0)
        return NULL;

    SymbolicCoord *result = symbolic_coord_create_rational(0, 1);
    if (!result)
        return NULL;

    for (int i = 0; i <= poly->degree; i++) {
        if (mpz_cmp_si(poly->coeffs[i], 0) == 0)
            continue;

        /* coeff_i * value^i */
        SymbolicCoord *coeff = symbolic_coord_create_rational(mpz_get_si(poly->coeffs[i]), 1);
        if (!coeff) {
            symbolic_coord_destroy(result);
            return NULL;
        }

        SymbolicCoord *power = symbolic_coord_pow(value, (unsigned int) i);
        if (!power) {
            symbolic_coord_destroy(coeff);
            symbolic_coord_destroy(result);
            return NULL;
        }

        SymbolicCoord *term = symbolic_coord_multiply(coeff, power);
        symbolic_coord_destroy(coeff);
        symbolic_coord_destroy(power);
        if (!term) {
            symbolic_coord_destroy(result);
            return NULL;
        }

        SymbolicCoord *new_result = symbolic_coord_add(result, term);
        symbolic_coord_destroy(term);
        if (!new_result) {
            symbolic_coord_destroy(result);
            return NULL;
        }

        symbolic_coord_destroy(result);
        result = new_result;
    }
    return result;
}

/**
 * @brief 计算两个多项式的代数结式
 * @details 委托给 mpz_poly_resultant 执行 Sylvester 结式计算。
 *          支持的运算类型：
 *          - ALG_OP_SUM：计算 Res_y(p(y), q(x-y)) 得到 α+β 的最小多项式
 *          - ALG_OP_PRODUCT：计算 Res_y(p(y), y^n*q(x/y)) 得到 α·β 的最小多项式
 * @param p      α 的最小多项式
 * @param q      β 的最小多项式
 * @param op     运算类型（和/积）
 * @param result 输出：结果多项式
 * @return true 成功，false 失败
 */
bool compute_algebraic_resultant(const mpz_poly_t *p, const mpz_poly_t *q, AlgebraicOp op, mpz_poly_t *result) {
    return mpz_poly_resultant(p, q, op, result);
}

/**
 * @brief 回代求解：将已知值代入方程系统
 * @param sys          方程系统
 * @param var_node_id  变量节点 ID
 * @param coord_index  坐标索引
 * @param value        已知数值
 */
void substitute_solved(EquationSystem *sys, int var_node_id, int coord_index, double value) {
    for (int i = 0; i < sys->count; i++) {
        if (sys->eqs[i].var_node_id == var_node_id && sys->eqs[i].coord_index == coord_index) {
            double val = 0.0;
            mpz_poly_t *p = &sys->eqs[i].poly;
            for (int d = 0; d <= p->degree; d++) {
                double coeff = mpz_get_d(p->coeffs[d]);
                val += coeff * pow(value, d);
            }
            if (fabs(val) < 1e-6 * (fabs(value) + 1.0)) {
                mpz_poly_clear(&sys->eqs[i].poly);
                mpz_poly_init(&sys->eqs[i].poly);
            }
        }
    }
}

/**
 * @brief 检查多项式是否超出求解范围
 * @details 当前支持的最高次数为 4（四次方程通过 Ferrari 方法求解）。
 *          次数大于 4 的方程标记为超出范围。
 * @param poly 多项式指针
 * @return true 表示超出范围，false 表示可求解
 */
bool is_out_of_scope(const mpz_poly_t *poly) {
    return poly->degree > 4;
}

/**
 * @brief 尝试对三次或更高次多项式进行因式分解
 * @details 使用有理根定理寻找整数根，然后通过综合除法提取因式。
 *          若常数项为零，则提取 x 作为因式。
 * @param poly    多项式指针
 * @param factor1 输出：第一个因式
 * @param factor2 输出：第二个因式
 * @return true 表示分解成功，false 表示失败
 */
/**
 * @brief 使用有理根定理（Rational Root Theorem）对多项式进行因式分解
 *
 * @details 有理根定理：若整系数多项式 a_n*x^n + ... + a_1*x + a_0 有
 *          有理根 p/q（最简形式），则 p | a_0 且 q | a_n。
 *          本函数仅搜索整数根（q=1），因为几何构造中只需要整数分解。
 *
 *          搜索算法：
 *          1. 若常数项 a_0 = 0，直接提取因式 x
 *          2. 遍历 a_0 的所有正除数 d，使用 Horner 法检测 poly(d) == 0
 *             和 poly(-d) == 0
 *          3. 找到根后，通过综合除法（synthetic division）提取因式 (x - r)
 *          4. 若未找到任何根，返回 false
 *
 *          时间复杂度：O(|a_0|^{1/2} * degree)，因为正除数数量受 sqrt(|a_0|) 限制。
 *          由于 a_0 可能很大（GMP 大整数），当 |a_0| > 10^9 时提前终止以避免性能问题。
 *
 * @param poly    原始多项式（degree >= 3 的整系数多项式）
 * @param factor1 输出：线性因式 (x - r)，其中 r 是找到的整数根
 * @param factor2 输出：商多项式 poly(x) / (x - r)，次数为 degree-1
 * @return true 表示成功分解，false 表示未找到有理根或因式分解失败
 */
bool try_factor_polynomial(const mpz_poly_t *poly, mpz_poly_t *factor1, mpz_poly_t *factor2) {
    if (poly->degree < 3)
        return false;

    mpz_poly_init(factor1);
    mpz_poly_init(factor2);

    mpz_t const_term, lead_coeff;
    mpz_init(const_term);
    mpz_init(lead_coeff);

    if (poly->degree >= 0)
        mpz_set(const_term, poly->coeffs[0]);
    mpz_set(lead_coeff, poly->coeffs[poly->degree]);

    /* 常数项为 0 时提取 x 作为因式 */
    if (mpz_cmp_si(const_term, 0) == 0) {
        factor1->degree = 1;
        factor1->coeffs = lv_malloc(2 * sizeof(mpz_t));
        if (!factor1->coeffs) {
            goto fail;
        }
        mpz_init_set_si(factor1->coeffs[0], 0);
        mpz_init_set_si(factor1->coeffs[1], 1);

        factor2->degree = poly->degree - 1;
        factor2->coeffs = lv_malloc((size_t) (factor2->degree + 1) * sizeof(mpz_t));
        if (!factor2->coeffs) {
            mpz_poly_clear(factor1);
            goto fail;
        }
        for (int i = 0; i <= factor2->degree; i++) {
            mpz_init_set(factor2->coeffs[i], poly->coeffs[i + 1]);
        }
        mpz_clear(const_term);
        mpz_clear(lead_coeff);
        return true;
    }

    /* 有理根定理：搜索整数根（const_term 的正负除数） */
    /* 性能保护：若 |a_0| > 10^9，除数的遍历代价过高，提前终止 */
    mpz_t abs_ct;
    mpz_init(abs_ct);
    mpz_abs(abs_ct, const_term);

    /* 10^9 作为终止阈值 */
    mpz_t limit;
    mpz_init_set_str(limit, "1000000000", 10);
    if (mpz_cmp(abs_ct, limit) > 0) {
        mpz_clear(abs_ct);
        mpz_clear(limit);
        mpz_clear(const_term);
        mpz_clear(lead_coeff);
        mpz_poly_clear(factor1);
        mpz_poly_clear(factor2);
        return false;
    }
    mpz_clear(limit);

    /* 将 const_term 转为 int64_t 以进行遍历（已通过 10^9 检查，安全） */
    int64_t ct_val = mpz_get_si(const_term);
    int64_t abs_ct_val = (ct_val < 0) ? -ct_val : ct_val;

    /* 遍历正除数并测试 ±d */
    int64_t found_root = 0;
    bool root_found = false;

    /* 临时变量用于 Horner 法求值 */
    mpz_t val, term, d_mpz;
    mpz_init(val);
    mpz_init(term);
    mpz_init(d_mpz);

    for (int64_t d = 1; d * d <= abs_ct_val; d++) {
        if (abs_ct_val % d != 0)
            continue;

        /* 测试两个候选除数：d 和 abs_ct_val / d */
        int64_t candidates[2] = {d, abs_ct_val / d};
        /* 对每个候选除数，测试 ±candidate */
        for (int ci = 0; ci < 2 && !root_found; ci++) {
            int64_t cand = candidates[ci];
            if (ci == 1 && cand == d)
                continue; /* 避免重复测试（完全平方数） */

            /* 测试正根：多项式在 d 处的值 */
            mpz_set_ui(val, 0);
            for (int i = poly->degree; i >= 0; i--) {
                mpz_mul_si(val, val, cand);         /* val *= d */
                mpz_add(val, val, poly->coeffs[i]); /* val += coeff[i] */
            }
            if (mpz_cmp_si(val, 0) == 0) {
                found_root = cand;
                root_found = true;
                break;
            }

            /* 跳过零测试（d > 0 时，-d != 0） */
            if (cand != 0) {
                int64_t neg_cand = -cand;
                mpz_set_ui(val, 0);
                for (int i = poly->degree; i >= 0; i--) {
                    mpz_mul_si(val, val, neg_cand);
                    mpz_add(val, val, poly->coeffs[i]);
                }
                if (mpz_cmp_si(val, 0) == 0) {
                    found_root = neg_cand;
                    root_found = true;
                    break;
                }
            }
        }
    }

    mpz_clear(val);
    mpz_clear(term);
    mpz_clear(d_mpz);
    mpz_clear(abs_ct);

    if (!root_found) {
        mpz_clear(const_term);
        mpz_clear(lead_coeff);
        mpz_poly_clear(factor1);
        mpz_poly_clear(factor2);
        return false;
    }

    /* 找到整数根 r = found_root，通过综合除法提取因式 (x - r) */
    /* factor1 = (x - found_root) 即系数 [ -found_root, 1 ] */
    factor1->degree = 1;
    factor1->coeffs = lv_malloc(2 * sizeof(mpz_t));
    if (!factor1->coeffs) {
        goto fail;
    }
    mpz_init_set_si(factor1->coeffs[0], -found_root);
    mpz_init_set_si(factor1->coeffs[1], 1);

    /* factor2 = poly / (x - r) 通过综合除法计算 */
    factor2->degree = poly->degree - 1;
    factor2->coeffs = lv_malloc((size_t) (factor2->degree + 1) * sizeof(mpz_t));
    if (!factor2->coeffs) {
        mpz_poly_clear(factor1);
        goto fail;
    }

    /* 综合除法：从高次项开始
     * factor2[degree-1] = poly->coeffs[degree]                     (最高次系数)
     * factor2[k] = poly->coeffs[k+1] + factor2[k+1] * r           (k = degree-2 ... 0)
     * 余数 = poly->coeffs[0] + factor2[0] * r（应当为 0，已通过 Horner 验证）
     */
    {
        mpz_t accum, root_mpz, tmp;
        mpz_init(accum);
        mpz_init_set_si(root_mpz, found_root);
        mpz_init(tmp);
        mpz_set(accum, poly->coeffs[poly->degree]);

        for (int i = factor2->degree; i >= 0; i--) {
            mpz_init(factor2->coeffs[i]);
            mpz_set(factor2->coeffs[i], accum);
            if (i > 0) {
                /* accum = accum * r + poly->coeffs[i] */
                mpz_mul(tmp, accum, root_mpz);
                mpz_add(accum, tmp, poly->coeffs[i]);
            }
        }
        mpz_clear(accum);
        mpz_clear(root_mpz);
        mpz_clear(tmp);
    }

    mpz_clear(const_term);
    mpz_clear(lead_coeff);
    return true;

fail:
    mpz_clear(const_term);
    mpz_clear(lead_coeff);
    return false;
}

/**
 * @brief 检测约束图中是否存在矛盾的距离声明
 * @details 扫描图中所有线段节点，比较相同端点但不同距离声明的节点对。
 * @param graph 约束图指针
 * @return true 表示检测到矛盾
 */
bool check_incompatible_distances(const ConstraintGraph *graph) {
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *ni = graph->nodes[i];
        if (!ni || !ni->numeric_assumption_declaration)
            continue;
        if (ni->type != GEOM_LINE_SEGMENT)
            continue;

        const char *di = ni->numeric_assumption_declaration;
        double dist_i = -1.0;
        const char *prefix = "distance=";
        size_t prefix_len = strlen(prefix);
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

            if (ni->coord_count < 4 || nj->coord_count < 4)
                continue;
            bool same_endpoints = true;
            for (int k = 0; k < 4; k++) {
                if (symbolic_coord_compare(ni->symbolic_coords[k], nj->symbolic_coords[k]) != 0) {
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

            if (fabs(dist_i - dist_j) > 1e-9) {
                return true;
            }
        }
    }
    return false;
}

/**
 * @brief 检查代入后是否存在矛盾（非零常数 = 0）
 * @param sys 方程系统指针
 * @return true 表示检测到矛盾
 */
bool check_contradiction_after_substitution(EquationSystem *sys) {
    for (int i = 0; i < sys->count; i++) {
        mpz_poly_t *p = &sys->eqs[i].poly;
        if (p->degree < 0)
            continue;
        if (p->degree == 0) {
            if (mpz_cmp_si(p->coeffs[0], 0) != 0) {
                return true;
            }
        }
    }
    return false;
}

/**
 * @brief 计算约束贡献的标量方程数量
 * @param c 约束指针
 * @return 约束贡献的标量方程数量
 */
int constraint_weight(const Constraint *c) {
    switch (c->type) {
        case INCIDENCE:
            return 1; /* 一个线性方程（点在线上） */
        case BETWEENNESS:
            return 2; /* 共线性 + 比值约束 */
        case INTERSECTION:
            return 2; /* 点在两条线上 */
        case CONTAINMENT:
            return 1; /* 至少一个边界约束 */
        case CONNECTION:
            return 1; /* 端口连接性 */
        default:
            return 1;
    }
}

/**
 * @brief 统计约束图中点类型节点的变量数量
 * @param graph   约束图指针
 * @param out_ids 输出：点节点 ID 数组（调用者负责释放），可为 NULL
 * @return 点节点数量
 */
int count_point_variables(const ConstraintGraph *graph, int **out_ids) {
    int count = 0;
    for (int i = 0; i < graph->node_count; i++) {
        if (graph->nodes[i]->type == GEOM_POINT)
            count++;
    }
    if (out_ids) {
        *out_ids = lv_malloc((size_t) count * sizeof(int));
        int idx = 0;
        for (int i = 0; i < graph->node_count; i++) {
            if (graph->nodes[i]->type == GEOM_POINT) {
                (*out_ids)[idx++] = graph->nodes[i]->id;
            }
        }
    }
    return count;
}

/* ================================================================
 * 方程求解核心实现
 * ================================================================ */

/**
 * @brief 向 GroebnerResult 追加解
 */
static int append_solution(GroebnerResult *result, SymbolicCoord *sol) {
    if (!result || !sol)
        return -1;
    SymbolicCoord **new_arr =
        lv_realloc(result->solutions, (size_t) (result->solution_count + 1) * sizeof(SymbolicCoord *));
    if (!new_arr) {
        lv_set_error(lv_ERROR_OUT_OF_MEMORY, "append_solution: 扩容失败");
        symbolic_coord_destroy(sol);
        return -1;
    }
    result->solutions = new_arr;
    result->solutions[result->solution_count++] = sol;
    return 0;
}

/**
 * @brief 精确求解一元线性方程 ax + b = 0
 * @return x = -b/a 的有理数值，失败返回 NULL
 */
static SymbolicCoord *solve_linear_exact(const mpz_poly_t *poly) {
    if (!poly || poly->degree != 1)
        return NULL;

    mpz_t a_mpz, b_mpz;
    mpz_init_set(a_mpz, poly->coeffs[1]);
    mpz_init_set(b_mpz, poly->coeffs[0]);

    if (mpz_cmp_si(a_mpz, 0) == 0) {
        mpz_clear(a_mpz);
        mpz_clear(b_mpz);
        return NULL;
    }

    /* x = -b / a, convert to int64_t/uint64_t for symbolic_coord_create_rational */
    mpz_t num_mpz;
    mpz_init(num_mpz);
    mpz_neg(num_mpz, b_mpz); /* numerator = -b */

    /* Check for overflow in int64_t/uint64_t conversion */
    int64_t num = 0;
    uint64_t denom = 0;

    if (mpz_fits_sint_p(num_mpz)) {
        num = (int64_t) mpz_get_si(num_mpz);
    } else {
        /* Use double fallback for large values */
        num = (int64_t) (mpz_get_d(num_mpz));
    }

    if (mpz_fits_uint_p(a_mpz)) {
        denom = (uint64_t) mpz_get_ui(a_mpz);
    } else {
        denom = (uint64_t) (mpz_get_d(a_mpz));
        if (denom == 0)
            denom = 1;
    }

    /* Adjust sign: rational should have positive denominator */
    if ((int64_t) denom < 0) {
        denom = (uint64_t) (-(int64_t) denom);
        num = -num;
    }

    SymbolicCoord *result = symbolic_coord_create_rational(num, denom);

    mpz_clear(num_mpz);
    mpz_clear(a_mpz);
    mpz_clear(b_mpz);
    return result;
}

/**
 * @brief 将已求解变量的值代入方程系统
 */
static void substitute_solved_symbolic(EquationSystem *sys, int var_node_id, int coord_index,
                                       const SymbolicCoord *value) {
    for (int i = 0; i < sys->count; i++) {
        if (sys->eqs[i].var_node_id == var_node_id && sys->eqs[i].coord_index == coord_index) {
            SymbolicCoord *eval_result = poly_eval_symbolic(&sys->eqs[i].poly, value);
            if (eval_result) {
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

/* 前向声明：精确二次/三次求解器（定义见下文） */
int solve_quadratic_exact(const mpz_poly_t *poly, SymbolicCoord **solutions, int max_solutions);
int solve_cubic_exact(const mpz_poly_t *poly, SymbolicCoord **solutions, int max_solutions);

/**
 * @brief 执行多遍求解（线性 → 二次 → 三次）
 */
void solve_equations_pass(EquationSystem *sys, GroebnerResult *result, int *solved_count, int *multiple_solutions,
                          bool *no_solution, bool do_substitute) {
    if (solved_count)
        *solved_count = 0;
    if (multiple_solutions)
        *multiple_solutions = 0;
    if (no_solution)
        *no_solution = false;

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
                SymbolicCoord *sol = solve_linear_exact(p);
                if (sol) {
                    if (append_solution(result, sol) == 0) {
                        if (solved_count)
                            (*solved_count)++;
                    }
                    if (do_substitute && sol) {
                        substitute_solved_symbolic(sys, sys->eqs[i].var_node_id, sys->eqs[i].coord_index, sol);
                    }
                } else {
                    if (mpz_cmp_si(p->coeffs[1], 0) == 0 && mpz_cmp_si(p->coeffs[0], 0) != 0) {
                        if (no_solution)
                            *no_solution = true;
                    }
                }
            } else if (p->degree == 2) {
                SymbolicCoord *exact_solutions[2] = {NULL, NULL};
                int exact_count = solve_quadratic_exact(p, exact_solutions, 2);
                if (exact_count == 0) {
                    if (no_solution)
                        *no_solution = true;
                } else {
                    if (exact_count > 1 && multiple_solutions)
                        (*multiple_solutions)++;
                    for (int r = 0; r < exact_count; r++) {
                        if (exact_solutions[r] && append_solution(result, exact_solutions[r]) == 0) {
                            if (solved_count)
                                (*solved_count)++;
                        }
                        if (do_substitute && exact_solutions[r]) {
                            substitute_solved_symbolic(sys, sys->eqs[i].var_node_id, sys->eqs[i].coord_index,
                                                       exact_solutions[r]);
                        }
                    }
                }
            } else if (p->degree == 3) {
                SymbolicCoord *cubic_solutions[3] = {NULL, NULL, NULL};
                int cubic_count = solve_cubic_exact(p, cubic_solutions, 3);
                if (cubic_count == 0) {
                    if (no_solution)
                        *no_solution = true;
                } else {
                    if (cubic_count > 1 && multiple_solutions)
                        (*multiple_solutions)++;
                    for (int r = 0; r < cubic_count; r++) {
                        if (cubic_solutions[r] && append_solution(result, cubic_solutions[r]) == 0) {
                            if (solved_count)
                                (*solved_count)++;
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

/* ================================================================
 * 精确二次 / 三次方程求解（仅使用 GMP 公共 API，不访问内部成员）
 * ================================================================ */

/**
 * @brief 使用 mpz_gcd 约分有理数 (num/den)
 *
 * @details 原地修改 num 和 den，确保 gcd=1 且 den > 0。
 *          仅使用 mpz_t 公共函数，不访问 _mp_size / _mp_d / _mp_alloc。
 */
static void mpz_reduce_fraction(mpz_t *num, mpz_t *den) {
    mpz_t g;
    mpz_init(g);
    mpz_gcd(g, *num, *den);
    if (mpz_cmp_si(g, 0) != 0 && mpz_cmp_si(g, 1) != 0) {
        mpz_divexact(*num, *num, g);
        mpz_divexact(*den, *den, g);
    }
    mpz_clear(g);
    /* 确保分母为正 */
    if (mpz_cmp_si(*den, 0) < 0) {
        mpz_neg(*num, *num);
        mpz_neg(*den, *den);
    }
}

/**
 * @brief 从 mpz_t 分子/分母创建 RATIONAL 类型的 SymbolicCoord
 *
 * @details 先约分，再检查是否适配 int64/uint64，最后调用
 *          symbolic_coord_create_rational。失败时返回 NULL。
 *          不访问 GMP 内部成员。
 */
static SymbolicCoord *coord_from_mpz_pair(const mpz_t tnum, const mpz_t tden) {
    mpz_t num, den;
    mpz_init_set(num, tnum);
    mpz_init_set(den, tden);
    mpz_reduce_fraction(&num, &den);

    SymbolicCoord *result = NULL;
    if (mpz_fits_slong_p(num) && mpz_fits_ulong_p(den)) {
        result = symbolic_coord_create_rational((int64_t) mpz_get_si(num), (uint64_t) mpz_get_ui(den));
    }
    mpz_clear(num);
    mpz_clear(den);
    return result;
}

/**
 * @brief 使用 GMP 精确整数运算求解一元二次方程 a*x^2 + b*x + c = 0
 *
 * @details 完全使用 mpz_t 公共函数（mpz_init、mpz_mul、mpz_sub、
 *          mpz_sqrtrem、mpz_gcd 等），不访问 _mp_size / _mp_d / _mp_alloc。
 *
 * 算法：
 *   1. 提取系数 a = coeffs[2], b = coeffs[1], c = coeffs[0]
 *   2. 计算判别式 D = b^2 - 4ac（使用 mpz_mul / mpz_sub）
 *   3. D < 0: 无实数解，返回 0
 *   4. D == 0: 唯一解 x = -b / (2a)（RATIONAL 类型）
 *   5. D > 0 且为完全平方数: 两个有理解 x = (-b ± √D) / (2a)（RATIONAL）
 *   6. D > 0 且非完全平方数: 两个二次根式解（QUADRATIC 类型 a + b*√n）
 *      对超出 int64/uint64 范围的解回退到 double 近似值
 *
 * @param poly         二次多项式（degree 必须为 2）
 * @param solutions    输出解数组（调用者负责释放每个 SymbolicCoord）
 * @param max_solutions 最大解数量（通常为 2）
 * @return 实际解数量（0、1 或 2），出错返回 0
 */
int solve_quadratic_exact(const mpz_poly_t *poly, SymbolicCoord **solutions, int max_solutions) {
    if (!poly || poly->degree != 2 || !solutions || max_solutions <= 0)
        return 0;

    /* 提取系数 a, b, c */
    mpz_t a_mpz, b_mpz, c_mpz;
    mpz_init_set(a_mpz, poly->coeffs[2]);
    mpz_init_set(b_mpz, poly->coeffs[1]);
    mpz_init_set(c_mpz, poly->coeffs[0]);

    /* 检查 a 是否为零 */
    if (mpz_cmp_si(a_mpz, 0) == 0) {
        /* 退化为线性: b*x + c = 0 */
        if (mpz_cmp_si(b_mpz, 0) == 0) {
            mpz_clear(a_mpz);
            mpz_clear(b_mpz);
            mpz_clear(c_mpz);
            return 0;
        }
        /* x = -c / b */
        mpz_t neg_c;
        mpz_init(neg_c);
        mpz_neg(neg_c, c_mpz);
        solutions[0] = coord_from_mpz_pair(neg_c, b_mpz);
        mpz_clear(neg_c);
        mpz_clear(a_mpz);
        mpz_clear(b_mpz);
        mpz_clear(c_mpz);
        return solutions[0] ? 1 : 0;
    }

    /* 判别式 D = b^2 - 4ac */
    mpz_t D;
    mpz_init(D);
    mpz_mul(D, b_mpz, b_mpz); /* D = b^2 */

    mpz_t four_ac;
    mpz_init(four_ac);
    mpz_mul_si(four_ac, a_mpz, 4);    /* 4a */
    mpz_mul(four_ac, four_ac, c_mpz); /* 4ac */
    mpz_sub(D, D, four_ac);           /* D = b^2 - 4ac */
    mpz_clear(four_ac);

    int D_sign = mpz_cmp_si(D, 0);

    if (D_sign < 0) {
        /* 无实数解 */
        mpz_clear(a_mpz);
        mpz_clear(b_mpz);
        mpz_clear(c_mpz);
        mpz_clear(D);
        return 0;
    }

    if (D_sign == 0) {
        /* 唯一解 x = -b / (2a) */
        mpz_t neg_b, two_a;
        mpz_init(neg_b);
        mpz_neg(neg_b, b_mpz);
        mpz_init(two_a);
        mpz_mul_si(two_a, a_mpz, 2);
        solutions[0] = coord_from_mpz_pair(neg_b, two_a);
        mpz_clear(neg_b);
        mpz_clear(two_a);
        mpz_clear(a_mpz);
        mpz_clear(b_mpz);
        mpz_clear(c_mpz);
        mpz_clear(D);
        return solutions[0] ? 1 : 0;
    }

    /* D > 0: 两个实数解 */
    /* 检查 D 是否为完全平方数（使用 mpz_sqrtrem） */
    mpz_t sqrt_D, rem;
    mpz_init(sqrt_D);
    mpz_init(rem);
    mpz_sqrtrem(sqrt_D, rem, D);
    bool is_perfect_square = (mpz_cmp_si(rem, 0) == 0);

    mpz_t two_a;
    mpz_init(two_a);
    mpz_mul_si(two_a, a_mpz, 2); /* denom = 2a */

    if (is_perfect_square) {
        /* 两个有理解: (-b ∓ √D) / (2a) */
        int count = 0;
        /* 解1: (-b - √D) / (2a) */
        {
            mpz_t num1;
            mpz_init(num1);
            mpz_neg(num1, b_mpz);
            mpz_sub(num1, num1, sqrt_D);
            solutions[count] = coord_from_mpz_pair(num1, two_a);
            mpz_clear(num1);
            if (solutions[count])
                count++;
        }
        /* 解2: (-b + √D) / (2a) */
        if (count < max_solutions) {
            mpz_t num2;
            mpz_init(num2);
            mpz_neg(num2, b_mpz);
            mpz_add(num2, num2, sqrt_D);
            solutions[count] = coord_from_mpz_pair(num2, two_a);
            mpz_clear(num2);
            if (solutions[count])
                count++;
        }
        mpz_clear(two_a);
        mpz_clear(sqrt_D);
        mpz_clear(rem);
        mpz_clear(a_mpz);
        mpz_clear(b_mpz);
        mpz_clear(c_mpz);
        mpz_clear(D);
        return count;
    }

    /* D 不是完全平方数 → 两个 QUADRATIC 类型的解 */
    /* 提取 D 的无平方因子部分: D = k^2 * n */
    {
        mpz_t n_part, k_part;
        mpz_init_set(n_part, D);
        mpz_init_set_ui(k_part, 1);

        /* 试除平方因子 */
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
            mpz_init_set_si(p2_check, p);
            mpz_mul(p2_check, p2_check, p2_check);
            if (mpz_cmp(p2_check, n_part) > 0) {
                mpz_clear(p2_check);
                break;
            }
            mpz_clear(p2_check);
        }

        /* 获取 n 的 unsigned int 值 */
        unsigned int n_val = 0;
        bool n_in_range = mpz_fits_uint_p(n_part);
        if (n_in_range) {
            n_val = mpz_get_ui(n_part);
        }

        /* 有理部分: a_part = -b / (2a); sqrt 系数: b_part = ±k / (2a) */
        mpz_t neg_b;
        mpz_init(neg_b);
        mpz_neg(neg_b, b_mpz);

        /* 约分后的有理部分 */
        mpz_t a_num, a_den;
        mpz_init_set(a_num, neg_b);
        mpz_init_set(a_den, two_a);
        mpz_reduce_fraction(&a_num, &a_den);

        /* 约分后的 sqrt 系数 */
        mpz_t b_num, b_den;
        mpz_init_set(b_num, k_part);
        mpz_init_set(b_den, two_a);
        mpz_reduce_fraction(&b_num, &b_den);

        mpz_clear(neg_b);

        if (!n_in_range) {
            /* n 超出 unsigned int 范围 → 回退到 double 近似 */
            double a_num_d = mpz_get_d(a_num);
            double a_den_d = mpz_get_d(a_den);
            double b_num_d = mpz_get_d(b_num);
            double b_den_d = mpz_get_d(b_den);
            double n_d = mpz_get_d(n_part);

            /* 检查 double 转换是否产生 Inf/NaN */
            if (!isfinite(a_num_d) || !isfinite(a_den_d) || !isfinite(b_num_d) || !isfinite(b_den_d) ||
                !isfinite(n_d)) {
                mpz_clear(a_num);
                mpz_clear(a_den);
                mpz_clear(b_num);
                mpz_clear(b_den);
                mpz_clear(n_part);
                lv_free((void **) &solutions);
                return 0;
            }

            double a_val = a_num_d / a_den_d;
            double b_val = b_num_d / b_den_d;
            double approx1 = a_val - b_val * sqrt(n_d);
            double approx2 = a_val + b_val * sqrt(n_d);

            solutions[0] = symbolic_coord_create_rational((int64_t) (approx1 * lv_SOLVER_SCALE_FACTOR),
                                                          (uint64_t) lv_SOLVER_SCALE_FACTOR);
            int count = solutions[0] ? 1 : 0;
            if (count < max_solutions) {
                solutions[count] = symbolic_coord_create_rational((int64_t) (approx2 * lv_SOLVER_SCALE_FACTOR),
                                                                  (uint64_t) lv_SOLVER_SCALE_FACTOR);
                if (solutions[count])
                    count++;
            }
            mpz_clear(a_num);
            mpz_clear(a_den);
            mpz_clear(b_num);
            mpz_clear(b_den);
            mpz_clear(n_part);
            mpz_clear(k_part);
            mpz_clear(two_a);
            mpz_clear(sqrt_D);
            mpz_clear(rem);
            mpz_clear(a_mpz);
            mpz_clear(b_mpz);
            mpz_clear(c_mpz);
            mpz_clear(D);
            return count;
        }

        /* 正常路径：创建 QUADRATIC 类型解 */
        Rational *qa = rational_create_from_mpz(a_num, a_den);
        if (!qa) {
            mpz_clear(a_num);
            mpz_clear(a_den);
            mpz_clear(b_num);
            mpz_clear(b_den);
            mpz_clear(n_part);
            mpz_clear(k_part);
            mpz_clear(two_a);
            mpz_clear(sqrt_D);
            mpz_clear(rem);
            mpz_clear(a_mpz);
            mpz_clear(b_mpz);
            mpz_clear(c_mpz);
            mpz_clear(D);
            return 0;
        }

        int count = 0;
        /* 解1: a + (-b_val)*√n（负 sqrt 系数） */
        {
            mpz_t neg_bn;
            mpz_init(neg_bn);
            mpz_neg(neg_bn, b_num);
            Rational *qb1 = rational_create_from_mpz(neg_bn, b_den);
            mpz_clear(neg_bn);
            if (qb1) {
                solutions[0] = symbolic_coord_create_quadratic(qa, qb1, n_val);
                if (solutions[0]) {
                    count++;
                } else {
                    /* 所有权未转移，需手动释放 */
                    rational_destroy(qa);
                    rational_destroy(qb1);
                }
            } else {
                rational_destroy(qa);
            }
        }

        /* 解2: a + b_val*√n（正 sqrt 系数） */
        if (count >= 1 && count < max_solutions) {
            /* 为 qa 创建独立拷贝 */
            Rational *qa_copy = rational_create_from_mpz(a_num, a_den);
            if (!qa_copy) {
                mpz_clear(a_num);
                mpz_clear(a_den);
                mpz_clear(b_num);
                mpz_clear(b_den);
                mpz_clear(n_part);
                mpz_clear(k_part);
                mpz_clear(two_a);
                mpz_clear(sqrt_D);
                mpz_clear(rem);
                mpz_clear(a_mpz);
                mpz_clear(b_mpz);
                mpz_clear(c_mpz);
                mpz_clear(D);
                return count;
            }
            Rational *qb2 = rational_create_from_mpz(b_num, b_den);
            if (qb2) {
                solutions[count] = symbolic_coord_create_quadratic(qa_copy, qb2, n_val);
                if (solutions[count]) {
                    count++;
                } else {
                    rational_destroy(qa_copy);
                    rational_destroy(qb2);
                }
            } else {
                rational_destroy(qa_copy);
            }
        }

        mpz_clear(a_num);
        mpz_clear(a_den);
        mpz_clear(b_num);
        mpz_clear(b_den);
        mpz_clear(n_part);
        mpz_clear(k_part);
        mpz_clear(two_a);
        mpz_clear(sqrt_D);
        mpz_clear(rem);
        mpz_clear(a_mpz);
        mpz_clear(b_mpz);
        mpz_clear(c_mpz);
        mpz_clear(D);
        return count;
    }
}

/**
 * @brief 使用 Cardano 公式数值求解一元三次方程
 *
 * @details 对 a*x^3 + b*x^2 + c*x + d = 0，使用 double 近似计算
 *          Cardano 判别式和根。由于三次方程精确求解需要任意精度立方根
 *          和反三角函数的 GMP 实现（极为复杂），本实现采用 double 精度。
 *
 * 算法：
 *   1. 归一化: x^3 + px^2 + qx + r = 0
 *   2. 消去二次项: y = x + p/3，得到 y^3 + Py + Q = 0
 *   3. 判别式 D = (Q/2)^2 + (P/3)^3
 *      - D > 0: 一个实根
 *      - D = 0: 三个实根（至少两个相等）
 *      - D < 0: 三个不等实根（三角函数 casus irreducibilis）
 *   4. 反代 x = y - p/3
 *
 * @warning 此为 double 精度实现，可能对极大系数有精度损失。
 *
 * @param poly         三次多项式（degree 必须为 3）
 * @param solutions    输出解数组
 * @param max_solutions 最大解数量（通常为 3）
 * @return 实际解数量（1 到 3），失败返回 0
 */
int solve_cubic_exact(const mpz_poly_t *poly, SymbolicCoord **solutions, int max_solutions) {
    if (!poly || poly->degree != 3 || !solutions || max_solutions <= 0)
        return 0;

    /* 提取系数（使用 mpz_get_d 转为 double，仅使用公共 API） */
    double a_val = mpz_get_d(poly->coeffs[3]);
    double b_val = mpz_get_d(poly->coeffs[2]);
    double c_val = mpz_get_d(poly->coeffs[1]);
    double d_val = mpz_get_d(poly->coeffs[0]);

    /* 归一化: x^3 + px^2 + qx + r = 0 */
    if (fabs(a_val) < lv_EPSILON_NEWTON) {
        return 0; /* 非三次方程 */
    }
    double p_val = b_val / a_val;
    double q_val = c_val / a_val;
    double r_val = d_val / a_val;

    /* 消去二次项: y = x + p/3 → y^3 + Py + Q = 0 */
    double p_over_3 = p_val / 3.0;
    double P = q_val - p_val * p_over_3;
    double Q = r_val - p_over_3 * (q_val - 2.0 * p_val * p_over_3 / 3.0);

    /* Cardano 判别式 */
    double half_Q = Q / 2.0;
    double P_over_3 = P / 3.0;
    double D = half_Q * half_Q + P_over_3 * P_over_3 * P_over_3;

    int sol_count = 0;

    if (D > lv_EPSILON_DOUBLE) {
        /* 一个实根: y = cbrt(-Q/2 + √D) + cbrt(-Q/2 - √D) */
        double sqrt_D = sqrt(D);
        double u = -half_Q + sqrt_D;
        double v = -half_Q - sqrt_D;
        double y_root = cbrt(u) + cbrt(v);
        double x_root = y_root - p_over_3;
        solutions[0] = symbolic_coord_create_rational((int64_t) (x_root * lv_SOLVER_SCALE_FACTOR),
                                                      (uint64_t) lv_SOLVER_SCALE_FACTOR);
        if (solutions[0])
            sol_count = 1;
    } else if (fabs(D) < lv_EPSILON_DOUBLE) {
        /* 三个实根（至少两个相等）:
         * y1 = 2*cbrt(-Q/2), y2 = y3 = -cbrt(-Q/2) */
        double cbrt_val = cbrt(-half_Q);
        double x1 = 2.0 * cbrt_val - p_over_3;
        double x2 = -cbrt_val - p_over_3;

        solutions[sol_count] =
            symbolic_coord_create_rational((int64_t) (x1 * lv_SOLVER_SCALE_FACTOR), (uint64_t) lv_SOLVER_SCALE_FACTOR);
        if (solutions[sol_count])
            sol_count++;

        if (sol_count < max_solutions && fabs(x1 - x2) > lv_EPSILON_DOUBLE) {
            solutions[sol_count] = symbolic_coord_create_rational((int64_t) (x2 * lv_SOLVER_SCALE_FACTOR),
                                                                  (uint64_t) lv_SOLVER_SCALE_FACTOR);
            if (solutions[sol_count])
                sol_count++;
        }
    } else {
        /* 三个不等实根 (casus irreducibilis): 三角函数公式
         * y_k = 2√(-P/3) * cos((acos(3Q/(2P)*√(-3/P)) + 2πk)/3) */
        /* 数值稳定性：casus irreducibilis 要求 P < 0，浮点误差可能导致 P >= 0 */
        if (P >= 0)
            return sol_count;
        double sqrt_term = sqrt(-P / 3.0);
        double acos_arg = 3.0 * Q / (2.0 * P) * sqrt(-3.0 / P);
        if (acos_arg > 1.0)
            acos_arg = 1.0;
        if (acos_arg < -1.0)
            acos_arg = -1.0;
        double phi = acos(acos_arg);
        for (int k = 0; k < 3 && sol_count < max_solutions; k++) {
            double angle = (phi + 2.0 * M_PI * k) / 3.0;
            double x_k = 2.0 * sqrt_term * cos(angle) - p_over_3;
            solutions[sol_count] = symbolic_coord_create_rational((int64_t) (x_k * lv_SOLVER_SCALE_FACTOR),
                                                                  (uint64_t) lv_SOLVER_SCALE_FACTOR);
            if (solutions[sol_count])
                sol_count++;
        }
    }

    return sol_count;
}

/**
 * @brief 释放 GroebnerResult 中的所有解并重置
 * @param result GroebnerResult 指针
 */
void cleanup_groebner_result(GroebnerResult *result) {
    if (!result)
        return;
    for (int i = 0; i < result->solution_count; i++) {
        symbolic_coord_destroy(result->solutions[i]);
    }
    lv_free((void **) &result->solutions);
    result->solutions = NULL;
    result->solution_count = 0;
}
