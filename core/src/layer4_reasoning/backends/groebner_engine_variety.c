/**
 * @file groebner_engine_variety.c
 * @brief 数值求解与簇计算
 *
 * @details 从 groebner_engine.c 拆分的子模块（Lv-00 项目 v3.3.0+）。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "groebner_engine.h"
#include "lv/lv.h"
#include "groebner_engine_internal.h"

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "error_codes.h"
#include "lv_internal.h"
#include "lv_utils.h"

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

/**
 * @brief 从零维 Groebner 基求解多项式方程组
 *
 * 对于零维理想，Groebner 基（在 lex 序下）具有三角形形式：
 * g_n(x_n) = 0, g_{n-1}(x_{n-1}, x_n) = 0, ...
 *
 * 采用回代法：先解单变量方程，再逐次回代。
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

    /* 仅支持变量数 <= 3 的简单零维求解 */
    if (vc > 3) {
        return NULL;
    }

    /* 尝试从基中提取单变量多项式 */
    /* 寻找仅含最后一个变量的基元 */
    lvPolynomial *univar = NULL;
    for (int i = 0; i < basis->bases_count; i++) {
        lvPolynomial *p = basis->basis_polys[i];
        if (poly_internal_is_zero(p))
            continue;
        bool single_var = true;
        for (int ti = 0; ti < p->term_count; ti++) {
            for (int v = 0; v < vc - 1; v++) {
                if (p->powers[ti * vc + v] != 0) {
                    single_var = false;
                    break;
                }
            }
            if (!single_var)
                break;
        }
        if (single_var) {
            univar = p;
            break;
        }
    }

    if (!univar) {
        return NULL;
    }

    /* 提取最高次数用于构造单变量多项式上下文 */
    int max_deg = 0;
    double *deg_coeffs = NULL;
    double *u_coeffs = (double *) univar->coeffs;
    for (int ti = 0; ti < univar->term_count; ti++) {
        int deg = univar->powers[ti * vc + vc - 1];
        if (deg > max_deg) {
            max_deg = deg;
        }
    }

    deg_coeffs = (double *) lv_calloc((size_t) (max_deg + 1), sizeof(double));
    if (!deg_coeffs) {
        return NULL;
    }
    for (int ti = 0; ti < univar->term_count; ti++) {
        int deg = univar->powers[ti * vc + vc - 1];
        deg_coeffs[deg] = u_coeffs[ti];
    }

    UnivariatePolyCtx ctx;
    ctx.coeffs = deg_coeffs;
    ctx.degree = max_deg;

    /* 简单的根搜索：在区间 [-10, 10] 上分段查找符号变化 */
    int max_solutions = 16;
    double *roots = (double *) lv_malloc((size_t) max_solutions * sizeof(double));
    int root_count = 0;
    if (!roots) {
        lv_free((void **) &deg_coeffs);
        return NULL;
    }

    double a = -10.0, b = 10.0;
    double step = (b - a) / (double) lv_config_get_int(LV_CFG_GROEBNER_ROOT_SEARCH_SEGMENTS, GROEBNER_ROOT_SEARCH_SEGMENTS);
    double prev_val = univar_eval(a, &ctx);

    for (int seg = 1; seg <= lv_config_get_int(LV_CFG_GROEBNER_ROOT_SEARCH_SEGMENTS, GROEBNER_ROOT_SEARCH_SEGMENTS) && root_count < max_solutions; seg++) {
        double x = a + step * seg;
        double curr_val = univar_eval(x, &ctx);

        if (prev_val * curr_val < 0.0) {
            /* 符号变化：根存在于此区间 */
            double mid = (x + (x - step)) / 2.0;
            double root = groebner_newton_refine(univar_eval, univar_deriv, &ctx, mid);
            if (fabs(univar_eval(root, &ctx)) < lv_config_get_double(LV_CFG_GROEBNER_ZERO_THRESHOLD, GROEBNER_ZERO_THRESHOLD)) {
                roots[root_count++] = root;
            }
        } else if (fabs(curr_val) < lv_config_get_double(LV_CFG_GROEBNER_ZERO_THRESHOLD, GROEBNER_ZERO_THRESHOLD)) {
            roots[root_count++] = x;
        }
        prev_val = curr_val;
    }

    lv_free((void **) &deg_coeffs);

    if (root_count == 0) {
        lv_free((void **) &roots);
        return NULL;
    }

    /* 为每个根构造解点坐标（此处简化：仅一维） */
    lvPolynomial **solutions = (lvPolynomial **) lv_malloc((size_t) root_count * sizeof(lvPolynomial *));
    if (!solutions) {
        lv_free((void **) &roots);
        return NULL;
    }

    for (int ri = 0; ri < root_count; ri++) {
        lvPolynomial *sol = poly_internal_create(ring, 1, NULL);
        if (!sol) {
            for (int j = 0; j < ri; j++) {
                poly_internal_destroy(solutions[j]);
            }
            lv_free((void **) &solutions);
            lv_free((void **) &roots);
            return NULL;
        }
        sol->term_count = 1;
        sol->term_capacity = 1;
        lv_free((void **) &sol->powers);
        lv_free((void **) &sol->coeffs);
        sol->powers = (int *) lv_calloc((size_t) vc, sizeof(int));
        sol->coeffs = (double *) lv_calloc(1, sizeof(double));
        if (!sol->powers || !sol->coeffs) {
            poly_internal_destroy(sol);
            for (int j = 0; j < ri; j++) {
                poly_internal_destroy(solutions[j]);
            }
            lv_free((void **) &solutions);
            lv_free((void **) &roots);
            return NULL;
        }
        sol->term_capacity = 1;
        /* 常量多项式表示点坐标 */
        ((double *) sol->coeffs)[0] = roots[ri];
        solutions[ri] = sol;
    }

    *solution_count = root_count;
    lv_free((void **) &roots);
    return solutions;
}


/* ================================================================
 *  第四部分：公共 API —— 代数簇
 * ================================================================ */

/**
 * @brief 计算代数簇
 */
int variety_compute(lvRingRegistry *registry, int ideal_id, const char *label) {
    if (!registry)
        return -1;

    lvLockGuard _lg;
    groebner_lock_guard_init(&_lg);
    int ret = -1;

    if (!g_data) {
        goto cleanup;
    }
    if (ideal_id < 0 || ideal_id >= g_data->ideal_count) {
        goto cleanup;
    }

    lvIdeal *ideal = g_data->ideals[ideal_id];
    if (!ideal) {
        goto cleanup;
    }

    /* 确保 Groebner 基已计算（直接调用内部函数，已持有锁） */
    if (!ideal->basis_valid || !ideal->cached_basis) {
        lvPolynomialRing *ring_for_basis = registry->rings[ideal->ring_id];
        if (!ring_for_basis) {
            goto cleanup;
        }

        lvGroebnerBasis *basis =
            groebner_internal_compute(ring_for_basis, ideal->generators, ideal->generator_count, GROEBNER_BUCHBERGER);
        if (!basis) {
            goto cleanup;
        }

        /* 释放旧缓存 */
        ideal_clear_cached_basis(ideal);
        ideal->cached_basis = basis;
        ideal->basis_valid = true;
    }

    lvPolynomialRing *ring = registry->rings[ideal->ring_id];
    if (!ring) {
        goto cleanup;
    }

    lvVariety *variety = (lvVariety *) lv_calloc(1, sizeof(lvVariety));
    if (!variety) {
        goto cleanup;
    }

    variety->ideal_id = ideal_id;
    variety->label = groebner_strdup_safe(label);

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
        goto cleanup;
    }

    ret = variety_internal_store(data, variety);

cleanup:
    lv_lock_guard_destroy(&_lg);
    return ret;
}

/**
 * @brief 获取代数簇的维数
 */
int variety_dimension(lvRingRegistry *registry, int variety_id) {
    lv_UNUSED(registry);
    lvLockGuard _lg;
    groebner_lock_guard_init(&_lg);
    int ret = -1;

    if (!g_data || variety_id < 0 || variety_id >= g_data->variety_count) {
        goto cleanup;
    }
    lvVariety *v = g_data->varieties[variety_id];
    ret = v ? v->variety_dimension : -1;

cleanup:
    lv_lock_guard_destroy(&_lg);
    return ret;
}

/**
 * @brief 检查是否为零维簇
 */
bool variety_is_zero_dimensional(lvRingRegistry *registry, int variety_id) {
    lv_UNUSED(registry);
    lvLockGuard _lg;
    groebner_lock_guard_init(&_lg);
    bool ok = false;

    if (!g_data || variety_id < 0 || variety_id >= g_data->variety_count) {
        goto cleanup;
    }
    lvVariety *v = g_data->varieties[variety_id];
    ok = v ? v->is_zero_dimensional : false;

cleanup:
    lv_lock_guard_destroy(&_lg);
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

    lvLockGuard _lg;
    groebner_lock_guard_init(&_lg);
    bool ok = false;

    if (!g_data || variety_id < 0 || variety_id >= g_data->variety_count) {
        goto cleanup;
    }
    lvVariety *v = g_data->varieties[variety_id];
    if (!v || !v->solution_points || !v->is_zero_dimensional || point_idx < 0 || point_idx >= v->solution_count) {
        goto cleanup;
    }
    double *src = v->solution_points[point_idx];
    if (!src) {
        goto cleanup;
    }
    /* 复制坐标值到输出缓冲区 */
    for (int i = 0; i < coord_count; i++) {
        out_coords[i] = src[i];
    }
    ok = true;

cleanup:
    lv_lock_guard_destroy(&_lg);
    return ok;
}