/**
 * @file groebner_engine_variety.c
 * @brief 数值求解与簇计算
 *
 * @details 从 groebner_engine.c 拆分的子模块（Lv-00 项目 v3.3.0+）。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "lv/groebner_engine.h"
#include "groebner_engine_internal.h"
#include "groebner_engine_guard.h"

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "lv/error_codes.h"
#include "lv/lv_internal.h"
#include "lv/lv_utils.h"

#include "lv/lv_thread.h"

/* ================================================================
 *  零维代数簇数值求解
 * ================================================================ */

/**
 * @brief 单变量多项式求值
 */
typedef struct {
    double *coeffs;
    int degree;
} UnivariatePolyCtx;

static double univar_eval(double x, void *ctx) {
    UnivariatePolyCtx *uc = (UnivariatePolyCtx *) ctx;
    double result = 0.0;
    double xpow = 1.0;
    for (int i = 0; i <= uc->degree; i++) {
        result += uc->coeffs[i] * xpow;
        xpow *= x;
    }
    return result;
}

/**
 * @brief 单变量多项式求导
 */
static double univar_deriv(double x, void *ctx) {
    UnivariatePolyCtx *uc = (UnivariatePolyCtx *) ctx;
    double result = 0.0;
    double xpow = 1.0;
    for (int i = 1; i <= uc->degree; i++) {
        result += i * uc->coeffs[i] * xpow;
        xpow *= x;
    }
    return result;
}

/**
 * @brief 牛顿法细化单变量根
 */
static double groebner_newton_refine(double (*eval)(double, void *), double (*deriv)(double, void *), void *ctx,
                                     double x0) {
    double x = x0;
    double prev_fx = fabs(eval(x, ctx));
    for (int iter = 0; iter < lv_config_get_int(LV_CFG_GROEBNER_NEWTON_MAX_ITER, GROEBNER_NEWTON_MAX_ITER); iter++) {
        double fx = eval(x, ctx);
        double fpx = deriv(x, ctx);
        if (fabs(fpx) < lv_config_get_double(LV_CFG_GROEBNER_ZERO_THRESHOLD, GROEBNER_ZERO_THRESHOLD)) {
            break;
        }
        double dx = fx / fpx;
        x = x - dx;
        if (fabs(dx) < lv_config_get_double(LV_CFG_GROEBNER_NEWTON_TOL, GROEBNER_NEWTON_TOL)) {
            break;
        }
        /* 发散检测：如果 |fx| 增长超过 10 倍，说明迭代发散，提前退出 */
        double abs_fx = fabs(fx);
        if (iter > 0 && abs_fx > prev_fx * 10.0) {
            break;
        }
        prev_fx = abs_fx;
    }
    return x;
}

/** @brief 解点池容量（回代笛卡尔积上限；超出部分丢弃并记录） */
#define MAX_SOL_POOL 64

/**
 * @brief 单变量多项式求根：区间扫描（符号变化/近零）+ 牛顿细化
 * @return 找到的根数量（<= max_roots）
 */
static int univar_find_roots(UnivariatePolyCtx *ctx, double *out_roots, int max_roots) {
    if (!ctx || !out_roots || max_roots <= 0 || ctx->degree <= 0)
        return 0;

    int root_count = 0;
    double a = -10.0, b = 10.0;
    int segments = lv_config_get_int(LV_CFG_GROEBNER_ROOT_SEARCH_SEGMENTS, GROEBNER_ROOT_SEARCH_SEGMENTS);
    double step = (b - a) / (double) segments;
    double prev_val = univar_eval(a, ctx);

    for (int seg = 1; seg <= segments && root_count < max_roots; seg++) {
        double x = a + step * seg;
        double curr_val = univar_eval(x, ctx);

        if (prev_val * curr_val < 0.0) {
            /* 符号变化：根存在于此区间 */
            double mid = (x + (x - step)) / 2.0;
            double root = groebner_newton_refine(univar_eval, univar_deriv, ctx, mid);
            if (fabs(univar_eval(root, ctx)) < lv_config_get_double(LV_CFG_GROEBNER_ZERO_THRESHOLD, GROEBNER_ZERO_THRESHOLD)) {
                out_roots[root_count++] = root;
            }
        } else if (fabs(curr_val) < lv_config_get_double(LV_CFG_GROEBNER_ZERO_THRESHOLD, GROEBNER_ZERO_THRESHOLD)) {
            out_roots[root_count++] = x;
        }
        prev_val = curr_val;
    }
    return root_count;
}

/**
 * @brief 线性系统快速求解：基中所有非零多项式总次数 <= 1 时，
 *        用高斯消元（部分主元）直接求解，支持任意变量数。
 *
 * 仅处理"恰好一个解"（满秩方阵）的零维情形；无解或欠定（无穷解）返回 NULL。
 *
 * @param basis           已计算的 Groebner 基（线性基元）
 * @param ring            多项式环
 * @param solution_count  输出解的数量（成功且唯一解时为 1）
 * @return 解点多项式数组（每个多项式 coeffs[0..vc-1] 为坐标值），失败返回 NULL
 */
static lvPolynomial **groebner_solve_linear_basis(const lvGroebnerBasis *basis, const lvPolynomialRing *ring,
                                                  int *solution_count) {
    *solution_count = 0;
    if (!basis || !ring) {
        return NULL;
    }

    int vc = ring->var_count;
    if (vc < 1) {
        return NULL;
    }
    double zero_thr = lv_config_get_double(LV_CFG_GROEBNER_ZERO_THRESHOLD, GROEBNER_ZERO_THRESHOLD);

    /* 第一遍：统计线性方程行数，检测常数矛盾方程（c = 0 且 c != 0 → 无解） */
    int m = 0;
    for (int i = 0; i < basis->bases_count; i++) {
        lvPolynomial *p = basis->basis_polys[i];
        if (!p || poly_internal_is_zero(p))
            continue;
        bool is_const = true;
        for (int ti = 0; ti < p->term_count; ti++) {
            for (int v = 0; v < vc; v++) {
                if (p->powers[ti * vc + v] != 0) {
                    is_const = false;
                    break;
                }
            }
            if (!is_const)
                break;
        }
        if (is_const) {
            if (fabs(((double *) p->coeffs)[0]) > zero_thr) {
                return NULL; /* 1 = 0 型矛盾方程：方程组无解 */
            }
            continue;
        }
        m++;
    }
    if (m == 0) {
        return NULL; /* 无有效方程：欠定 */
    }

    /* 构造增广矩阵 mat[m][vc+1]：每行系数 + 右端常数项 */
    double *mat = (double *) lv_calloc((size_t) m * (size_t) (vc + 1), sizeof(double));
    if (!mat) {
        return NULL;
    }
    int row = 0;
    for (int i = 0; i < basis->bases_count && row < m; i++) {
        lvPolynomial *p = basis->basis_polys[i];
        if (!p || poly_internal_is_zero(p))
            continue;
        bool is_const = true;
        for (int ti = 0; ti < p->term_count; ti++) {
            for (int v = 0; v < vc; v++) {
                if (p->powers[ti * vc + v] != 0) {
                    is_const = false;
                    break;
                }
            }
            if (!is_const)
                break;
        }
        if (is_const)
            continue;
        for (int ti = 0; ti < p->term_count; ti++) {
            double c = ((double *) p->coeffs)[ti];
            int var = -1;
            for (int v = 0; v < vc; v++) {
                if (p->powers[ti * vc + v] != 0) {
                    var = v;
                    break;
                }
            }
            if (var < 0) {
                mat[row * (vc + 1) + vc] = -c; /* 常数项移到右端 */
            } else {
                mat[row * (vc + 1) + var] = c;
            }
        }
        row++;
    }
    m = row;

    /* 高斯消元（部分主元，化为 RREF） */
    int rank = 0;
    for (int col = 0; col < vc && rank < m; col++) {
        int pivot = -1;
        for (int r = rank; r < m; r++) {
            if (fabs(mat[r * (vc + 1) + col]) > zero_thr) {
                pivot = r;
                break;
            }
        }
        if (pivot < 0)
            continue;
        if (pivot != rank) {
            for (int k = col; k <= vc; k++) {
                double t = mat[rank * (vc + 1) + k];
                mat[rank * (vc + 1) + k] = mat[pivot * (vc + 1) + k];
                mat[pivot * (vc + 1) + k] = t;
            }
        }
        double pv = mat[rank * (vc + 1) + col];
        for (int r = 0; r < m; r++) {
            if (r == rank)
                continue;
            double f = mat[r * (vc + 1) + col] / pv;
            if (fabs(f) < zero_thr)
                continue;
            for (int k = col; k <= vc; k++) {
                mat[r * (vc + 1) + k] -= f * mat[rank * (vc + 1) + k];
            }
        }
        rank++;
    }

    /* 一致性检查：消元后左侧全零但右端非零的行 → 无解 */
    for (int r = rank; r < m; r++) {
        bool all_zero = true;
        for (int c = 0; c < vc; c++) {
            if (fabs(mat[r * (vc + 1) + c]) > zero_thr) {
                all_zero = false;
                break;
            }
        }
        if (all_zero && fabs(mat[r * (vc + 1) + vc]) > zero_thr) {
            lv_free((void **) &mat);
            return NULL;
        }
    }

    /* 欠定（rank < vc）→ 无穷多解，非零维簇 */
    if (rank < vc) {
        lv_free((void **) &mat);
        return NULL;
    }

    /* 回代提取唯一解 */
    double *sol = (double *) lv_calloc((size_t) vc, sizeof(double));
    if (!sol) {
        lv_free((void **) &mat);
        return NULL;
    }
    for (int r = 0; r < rank; r++) {
        int pc = -1;
        for (int c = 0; c < vc; c++) {
            if (fabs(mat[r * (vc + 1) + c]) > zero_thr) {
                pc = c;
                break;
            }
        }
        if (pc >= 0) {
            sol[pc] = mat[r * (vc + 1) + vc] / mat[r * (vc + 1) + pc];
        }
    }
    lv_free((void **) &mat);

    /* 构造解点多项式：coeffs[0..vc-1] 即坐标值（与单变量路径格式一致） */
    lvPolynomial *sol_poly = poly_internal_create(ring, vc, NULL);
    if (!sol_poly) {
        lv_free((void **) &sol);
        return NULL;
    }
    sol_poly->term_count = vc;
    for (int v = 0; v < vc; v++) {
        ((double *) sol_poly->coeffs)[v] = sol[v];
    }
    sol_poly->total_degree = 1;
    lv_free((void **) &sol);

    lvPolynomial **solutions = (lvPolynomial **) lv_malloc(sizeof(lvPolynomial *));
    if (!solutions) {
        poly_internal_destroy(sol_poly);
        return NULL;
    }
    solutions[0] = sol_poly;
    *solution_count = 1;
    return solutions;
}

/**
 * @brief 从零维 Groebner 基求解多项式方程组
 *
 * 对于零维理想，Groebner 基（在 lex 序下）具有三角形形式：
 * g_n(x_n) = 0, g_{n-1}(x_{n-1}, x_n) = 0, ...
 *
 * 完整实现采用回代法：
 * 1. 分层：按"最高出现变量"把基元分组（仅含 {k..vc-1} 的基元进入第 k 层）；
 * 2. 逐层求根：从最高变量 x_{vc-1} 开始，解该层单变量方程；
 * 3. 回代：把已求得的 (x_{k+1}..x_{vc-1}) 代入第 k 层基元（poly_internal_substitute
 *    替换常量），得到 x_k 的单变量方程再求根；
 * 4. 笛卡尔积：逐层组合所有根的取值生成解点。
 * 任一层无单变量方程（缺三角分层）则回退线性/单变量近似路径。
 *
 * @return 解点坐标数组（调用者负责释放），*solution_count 输出解的数量
 */
lvPolynomial **groebner_solve_zero_dim(const lvGroebnerBasis *basis, const lvPolynomialRing *ring,
                                              int *solution_count) {
    *solution_count = 0;
    if (!basis || !ring || basis->bases_count == 0) {
        return NULL;
    }

    int vc = ring->var_count;
    if (vc < 1) {
        return NULL;
    }

    /* 线性系统快速路径：基中所有非零多项式总次数 <= 1 时，
     * 直接用高斯消元求解，支持任意变量数（几何约束系统多为线性）。 */
    {
        bool all_linear = true;
        for (int i = 0; i < basis->bases_count; i++) {
            lvPolynomial *p = basis->basis_polys[i];
            if (!p || poly_internal_is_zero(p))
                continue;
            if (p->total_degree > 1) {
                all_linear = false;
                break;
            }
        }
        if (all_linear) {
            return groebner_solve_linear_basis(basis, ring, solution_count);
        }
    }

    /* 仅支持变量数 <= 3 的零维求解 */
    if (vc > 3) {
        return NULL;
    }

    /* ── 完整回代：三角分层 + 逐变量求根 ──
     * 对每层 k（0..vc-1）收集"最高变量为 k"的基元：即出现在变量集合
     * {k..vc-1} 中且不出现 {0..k-1} 的基元。第 vc-1 层必须是单变量。 */
    lvPolynomial **layers[3] = {NULL, NULL, NULL};
    int layer_counts[3] = {0, 0, 0};
    int layer_caps[3] = {0, 0, 0};

    bool layers_ok = true;
    for (int k = 0; k < vc && layers_ok; k++) {
        int cap = basis->bases_count;
        layers[k] = (lvPolynomial **) lv_calloc((size_t) cap, sizeof(lvPolynomial *));
        if (!layers[k]) {
            layers_ok = false;
            break;
        }
        layer_caps[k] = cap;
        for (int i = 0; i < basis->bases_count; i++) {
            lvPolynomial *p = basis->basis_polys[i];
            if (!p || poly_internal_is_zero(p))
                continue;
            /* 出现在变量 k 上 */
            bool has_k = false;
            for (int ti = 0; ti < p->term_count; ti++) {
                if (p->powers[ti * vc + k] != 0) {
                    has_k = true;
                    break;
                }
            }
            if (!has_k)
                continue;
            /* 不出现 {0..k-1}（保证分层严格） */
            bool has_lower = false;
            for (int v = 0; v < k; v++) {
                for (int ti = 0; ti < p->term_count; ti++) {
                    if (p->powers[ti * vc + v] != 0) {
                        has_lower = true;
                        break;
                    }
                }
                if (has_lower)
                    break;
            }
            if (has_lower)
                continue;
            layers[k][layer_counts[k]++] = p;
        }
        /* 最高层（vc-1）必须含至少一个单变量基元 */
        if (k == vc - 1 && layer_counts[k] == 0)
            layers_ok = false;
    }
    if (!layers_ok) {
        for (int k = 0; k < vc; k++) {
            if (layers[k])
                lv_free((void **) &layers[k]);
        }
        return NULL;
    }

    /* 解点存储：容量上限（每个变量最多 GROEBNER_ROOT_MAX_SOLUTIONS 个根） */
    enum { GROEBNER_ROOT_MAX_SOLUTIONS = 16 };
    double *solutions[MAX_SOL_POOL];
    int sol_count = 0;

    /* 当前已回代的部分解：var_values[v] = 变量 v 的当前候选值（未确定层未写） */
    double var_values[MAX_SOL_POOL];

    /* 显式栈：state[k] = 第 k 层当前候选根下标 */
    int state[MAX_SOL_POOL];
    for (int k = 0; k < vc; k++)
        state[k] = 0;

    /* 各层根列表：stored_roots[k][j]，root_counts[k] */
    double stored_roots[3][GROEBNER_ROOT_MAX_SOLUTIONS];
    int root_counts[3] = {0, 0, 0};

    int k = vc - 1;
    bool done = false;
    while (!done) {
        if (k == vc - 1) {
            /* 最高层：直接解单变量方程（从该层基元提取系数） */
            if (root_counts[k] == 0) {
                lvPolynomial *univar = layers[k][0];
                int max_deg = 0;
                double *u_coeffs = (double *) univar->coeffs;
                for (int ti = 0; ti < univar->term_count; ti++) {
                    int deg = univar->powers[ti * vc + k];
                    if (deg > max_deg)
                        max_deg = deg;
                }
                if (max_deg <= 0) {
                    done = true;
                    break;
                }
                double *deg_coeffs = (double *) lv_calloc((size_t) (max_deg + 1), sizeof(double));
                if (!deg_coeffs) {
                    done = true;
                    break;
                }
                for (int ti = 0; ti < univar->term_count; ti++) {
                    int deg = univar->powers[ti * vc + k];
                    deg_coeffs[deg] = u_coeffs[ti];
                }
                UnivariatePolyCtx ctx;
                ctx.coeffs = deg_coeffs;
                ctx.degree = max_deg;
                int rc = univar_find_roots(&ctx, stored_roots[k], GROEBNER_ROOT_MAX_SOLUTIONS);
                root_counts[k] = rc;
                lv_free((void **) &deg_coeffs);
                if (rc == 0) {
                    done = true;
                    break;
                }
            }
            if (state[k] >= root_counts[k]) {
                /* 本层穷举完：回退到更高层 */
                k++;
                while (k < vc && state[k] >= root_counts[k] - 1)
                    k++;
                if (k >= vc) {
                    done = true;
                    break;
                }
                state[k]++;
                continue;
            }
            var_values[k] = stored_roots[k][state[k]];
            if (k - 1 >= 0) {
                k--;
                state[k] = 0;
                root_counts[k] = 0;
                continue;
            }
            /* 全部变量确定（vc==1 时直接到这里），记录解点 */
            {
                double *pt = (double *) lv_calloc((size_t) vc, sizeof(double));
                if (pt) {
                    for (int v = 0; v < vc; v++)
                        pt[v] = var_values[v];
                    if (sol_count < (int) lv_ARRAY_SIZE(solutions))
                        solutions[sol_count++] = pt;
                    else
                        lv_free((void **) &pt);
                }
            }
            /* 回溯：本层取下一个根，或回退更高层 */
            k++;
            while (k < vc && state[k] >= root_counts[k] - 1)
                k++;
            if (k >= vc) {
                done = true;
                break;
            }
            state[k]++;
            continue;
        } else {
            /* 中间层 k：把已确定的 (x_{k+1}..x_{vc-1}) 代入该层基元 → x_k 单变量方程 */
            if (root_counts[k] == 0) {
                bool found = false;
                for (int li = 0; li < layer_counts[k] && !found; li++) {
                    lvPolynomial *base = layers[k][li];
                    lvPolynomial *cur = poly_internal_copy(base, ring);
                    if (!cur)
                        break;
                    /* 代入已确定变量 vv = k+1..vc-1 */
                    for (int vv = k + 1; vv < vc && cur; vv++) {
                        double val = var_values[vv];
                        lvPolynomial *cst = poly_internal_create(ring, 1, NULL);
                        if (!cst) {
                            poly_internal_destroy(cur);
                            cur = NULL;
                            break;
                        }
                        cst->term_count = 1;
                        for (int v = 0; v < vc; v++)
                            cst->powers[v] = 0;
                        ((double *) cst->coeffs)[0] = val;
                        lvPolynomial *sub = poly_internal_substitute(cur, vv, cst, ring);
                        poly_internal_destroy(cur);
                        poly_internal_destroy(cst);
                        cur = sub;
                    }
                    if (!cur)
                        continue;
                    /* 代入后必须是仅含变量 k 的单变量 */
                    bool single_var = true;
                    for (int ti = 0; ti < cur->term_count; ti++) {
                        for (int v = 0; v < vc; v++) {
                            if (v != k && cur->powers[ti * vc + v] != 0) {
                                single_var = false;
                                break;
                            }
                        }
                        if (!single_var)
                            break;
                    }
                    if (!single_var || cur->term_count == 0) {
                        poly_internal_destroy(cur);
                        continue;
                    }
                    int md = 0;
                    double *uc = (double *) cur->coeffs;
                    for (int ti = 0; ti < cur->term_count; ti++) {
                        int deg = cur->powers[ti * vc + k];
                        if (deg > md)
                            md = deg;
                    }
                    if (md <= 0) {
                        /* 代入后常数：矛盾（无解）或恒等（任意值） */
                        if (fabs(uc[0]) > 1e-9) {
                            poly_internal_destroy(cur);
                            continue;
                        }
                        poly_internal_destroy(cur);
                        found = true;
                        root_counts[k] = 0; /* 恒等：任意值，取 0 代表 */
                        break;
                    }
                    double *dc = (double *) lv_calloc((size_t) (md + 1), sizeof(double));
                    if (!dc) {
                        poly_internal_destroy(cur);
                        break;
                    }
                    for (int ti = 0; ti < cur->term_count; ti++) {
                        int deg = cur->powers[ti * vc + k];
                        dc[deg] = uc[ti];
                    }
                    poly_internal_destroy(cur);
                    UnivariatePolyCtx ctx;
                    ctx.coeffs = dc;
                    ctx.degree = md;
                    int rc = univar_find_roots(&ctx, stored_roots[k], GROEBNER_ROOT_MAX_SOLUTIONS);
                    lv_free((void **) &dc);
                    root_counts[k] = rc;
                    found = true;
                }
                if (!found) {
                    /* 该分支无有效单变量方程：本层无解，回退更高层 */
                    k++;
                    while (k < vc && state[k] >= root_counts[k] - 1)
                        k++;
                    if (k >= vc) {
                        done = true;
                        break;
                    }
                    state[k]++;
                    continue;
                }
                if (root_counts[k] == 0) {
                    /* 恒等分支：任意值满足，取 0 作为代表解 */
                    stored_roots[k][0] = 0.0;
                    root_counts[k] = 1;
                }
            }
            if (state[k] >= root_counts[k]) {
                k++;
                while (k < vc && state[k] >= root_counts[k] - 1)
                    k++;
                if (k >= vc) {
                    done = true;
                    break;
                }
                state[k]++;
                continue;
            }
            var_values[k] = stored_roots[k][state[k]];
            if (k - 1 >= 0) {
                k--;
                state[k] = 0;
                root_counts[k] = 0;
                continue;
            }
            /* 全部变量确定，记录解点 */
            {
                double *pt = (double *) lv_calloc((size_t) vc, sizeof(double));
                if (pt) {
                    for (int v = 0; v < vc; v++)
                        pt[v] = var_values[v];
                    if (sol_count < (int) lv_ARRAY_SIZE(solutions))
                        solutions[sol_count++] = pt;
                    else
                        lv_free((void **) &pt);
                }
            }
            k++;
            while (k < vc && state[k] >= root_counts[k] - 1)
                k++;
            if (k >= vc) {
                done = true;
                break;
            }
            state[k]++;
            continue;
        }
    }

    for (int k = 0; k < vc; k++) {
        if (layers[k])
            lv_free((void **) &layers[k]);
    }

    if (sol_count == 0) {
        return NULL;
    }

    /* 构造解点多项式数组：每个解一个常量多项式，coeffs[0..vc-1] 为坐标 */
    lvPolynomial **solutions_out = (lvPolynomial **) lv_malloc((size_t) sol_count * sizeof(lvPolynomial *));
    if (!solutions_out) {
        for (int i = 0; i < sol_count; i++)
            lv_free((void **) &solutions[i]);
        return NULL;
    }
    for (int ri = 0; ri < sol_count; ri++) {
        lvPolynomial *sol = poly_internal_create(ring, vc, NULL);
        if (!sol) {
            for (int j = 0; j < ri; j++)
                poly_internal_destroy(solutions_out[j]);
            for (int j = ri; j < sol_count; j++)
                lv_free((void **) &solutions[j]);
            lv_free((void **) &solutions_out);
            return NULL;
        }
        sol->term_count = vc;
        for (int v = 0; v < vc; v++) {
            sol->powers[v] = 0;
            ((double *) sol->coeffs)[v] = solutions[ri][v];
        }
        sol->total_degree = 0;
        solutions_out[ri] = sol;
        lv_free((void **) &solutions[ri]);
    }
    *solution_count = sol_count;
    return solutions_out;
}


/* ================================================================
 *  第四部分：公共 API —— 代数簇
 * ================================================================ */

/**
 * @brief 计算代数簇
 */
int variety_compute(lvRingRegistry *registry, int ideal_id, const char *label) {
    /* exempt: 单指针 NULL 守卫（registry 非空），与 id 范围守卫不同构，保留 */
    if (!registry)
        return -1;

    int ret = -1;
    GROEBNER_LOCK_GUARD_BEGIN();
    if (!lv_index_in_range(ideal_id, g_data->ideal_count)) {
        goto _gcleanup;
    }

    lvIdeal *ideal = g_data->ideals[ideal_id];
    if (!ideal) {
        goto _gcleanup;
    }

    /* 确保 Groebner 基已计算（直接调用内部函数，已持有锁） */
    if (!ideal->basis_valid || !ideal->cached_basis) {
        lvPolynomialRing *ring_for_basis = registry->rings[ideal->ring_id];
        if (!ring_for_basis) {
            goto _gcleanup;
        }

        lvGroebnerBasis *basis =
            groebner_internal_compute(ring_for_basis, ideal->generators, ideal->generator_count, GROEBNER_BUCHBERGER);
        if (!basis) {
            goto _gcleanup;
        }

        /* 释放旧缓存 */
        ideal_clear_cached_basis(ideal);
        ideal->cached_basis = basis;
        ideal->basis_valid = true;
    }

    lvPolynomialRing *ring = registry->rings[ideal->ring_id];
    if (!ring) {
        goto _gcleanup;
    }

    lvVariety *variety = (lvVariety *) lv_calloc(1, sizeof(lvVariety));
    if (!variety) {
        goto _gcleanup;
    }

    variety->ideal_id = ideal_id;
    variety->label = lv_strdup_safe(label);

    /* 尝试零维求解 */
    int sol_count = 0;
    lvPolynomial **sol_polys = groebner_solve_zero_dim(ideal->cached_basis, ring, &sol_count);

    if (sol_polys && sol_count > 0) {
        variety->is_zero_dimensional = true;
        variety->solution_count = sol_count;
        variety->solution_points = (double **) lv_calloc((size_t) sol_count, sizeof(double *));
        if (variety->solution_points) {
            for (int i = 0; i < sol_count && sol_polys[i]; i++) {
                variety->solution_points[i] = (double *) lv_calloc((size_t) ring->var_count, sizeof(double));
                if (variety->solution_points[i]) {
                    for (int v = 0; v < ring->var_count && v < sol_polys[i]->term_count; v++) {
                        variety->solution_points[i][v] = ((double *) sol_polys[i]->coeffs)[v];
                    }
                }
            }
        }
        variety->solution_capacity = sol_count;
        variety->variety_dimension = 0;
        variety->degree_of_freedom = 0;

        for (int i = 0; i < sol_count; i++) {
            poly_internal_destroy(sol_polys[i]);
        }
        lv_free((void **) &sol_polys);
    } else {
        /* 非零维：估算维数 */
        variety->is_zero_dimensional = false;
        variety->variety_dimension = ring->var_count - ideal->cached_basis->bases_count;
        if (variety->variety_dimension < 0) {
            variety->variety_dimension = 0;
        }
        variety->degree_of_freedom = variety->variety_dimension;
    }

    lvRegistryData *data = registry_data_ensure();
    if (!data) {
        lv_free_many((void **) &variety->label, (void **) &variety, NULL);
        goto _gcleanup;
    }

    ret = variety_internal_store(data, variety);

GROEBNER_LOCK_GUARD_END();
    return ret;
}

/**
 * @brief 获取代数簇的维数
 */
int variety_dimension(lvRingRegistry *registry, int variety_id) {
    lv_UNUSED(registry);
    int ret = -1;
    GROEBNER_LOCK_GUARD_BEGIN();
    if (!lv_index_in_range(variety_id, g_data->variety_count)) {
        goto _gcleanup;
    }
    lvVariety *v = g_data->varieties[variety_id];
    ret = v ? v->variety_dimension : -1;

GROEBNER_LOCK_GUARD_END();
    return ret;
}

/**
 * @brief 检查是否为零维簇
 */
bool variety_is_zero_dimensional(lvRingRegistry *registry, int variety_id) {
    lv_UNUSED(registry);
    bool ok = false;
    GROEBNER_LOCK_GUARD_BEGIN();
    if (!lv_index_in_range(variety_id, g_data->variety_count)) {
        goto _gcleanup;
    }
    lvVariety *v = g_data->varieties[variety_id];
    ok = v ? v->is_zero_dimensional : false;

GROEBNER_LOCK_GUARD_END();
    return ok;
}

/**
 * @brief 从代数簇中获取指定索引的解点坐标
 */
bool variety_get_solution_point(lvRingRegistry *registry, int variety_id, int point_idx, double *out_coords,
                                int coord_count) {
    lv_UNUSED(registry);
    if (!out_coords || coord_count <= 0)
        return false;

    bool ok = false;
    GROEBNER_LOCK_GUARD_BEGIN();
    if (!lv_index_in_range(variety_id, g_data->variety_count)) {
        goto _gcleanup;
    }
    lvVariety *v = g_data->varieties[variety_id];
    if (!v || !v->solution_points || !v->is_zero_dimensional ||
        !lv_index_in_range(point_idx, v->solution_count)) {
        goto _gcleanup;
    }
    double *src = v->solution_points[point_idx];
    if (!src) {
        goto _gcleanup;
    }
    /* 复制坐标值到输出缓冲区 */
    for (int i = 0; i < coord_count; i++) {
        out_coords[i] = src[i];
    }
    ok = true;

GROEBNER_LOCK_GUARD_END();
    return ok;
}