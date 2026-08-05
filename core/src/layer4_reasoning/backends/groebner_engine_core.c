/**
 * @file groebner_engine_core.c
 * @brief Buchberger 算法核心
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
 *  Buchberger 算法 —— 核心 Gröbner 基计算
 * ================================================================ */

/**
 * @brief Buchberger 算法的标准实现（带优化）
 *
 * 算法流程：
 * 1. 初始化 G = 生成元集合
 * 2. 构建所有生成元对的集合 B
 * 3. 重复直到 B 为空：
 *    a. 选择一对 (fi, fj) 从 B 中移除
 *    b. 应用 Buchberger 互质判别式：若 gcd(LT(fi), LT(fj)) = 1，跳过
 *    c. 计算 S(fi, fj)，并用 G 约化得到 r
 *    d. 若 r != 0，则将 r 加入 G，并添加新对 (gi, r) 到 B
 * 4. 返回约化的 Gröbner 基
 *
 * @param ring        多项式环
 * @param generators  生成元多项式数组
 * @param gen_count   生成元数量
 * @param algorithm   算法选择（当前仅实现 BUCHBERGER）
 * @return Gröbner 基结构体，失败返回 NULL
 */
lvGroebnerBasis *groebner_internal_compute(const lvPolynomialRing *ring, lvPolynomial **generators,
                                                  int gen_count, lvGroebnerAlgorithm algorithm) {
    if (!ring || !generators || gen_count <= 0) {
        return NULL;
    }

    lv_UNUSED(algorithm); /* 当前仅实现 Buchberger */

    lvGroebnerBasis *basis = basis_alloc(gen_count * 2 + GROEBNER_BASIS_INIT_CAPACITY);
    if (!basis) {
        return NULL;
    }
    basis->algorithm_used = GROEBNER_BUCHBERGER;

    /* 将生成元复制到基中（去除非零的） */
    for (int i = 0; i < gen_count; i++) {
        if (generators[i] && !poly_internal_is_zero(generators[i])) {
            if (basis->bases_count >= basis->bases_capacity) {
                if (!lv_ensure_capacity((void **) &basis->basis_polys, basis->bases_count, &basis->bases_capacity,
                                        sizeof(lvPolynomial *), 1)) {
                    /* 清理已分配的内存（失败时 basis->basis_polys 保持不变） */
                    for (int j = 0; j < basis->bases_count; j++) {
                        poly_internal_destroy(basis->basis_polys[j]);
                    }
                    lv_free((void **) &basis->basis_polys);
                    lv_free((void **) &basis);
                    return NULL;
                }
            }
            basis->basis_polys[basis->bases_count] = poly_internal_copy(generators[i], ring);
            if (!basis->basis_polys[basis->bases_count]) {
                /* 清理 */
                for (int j = 0; j < basis->bases_count; j++) {
                    poly_internal_destroy(basis->basis_polys[j]);
                }
                lv_free((void **) &basis->basis_polys);
                lv_free((void **) &basis);
                return NULL;
            }
            basis->bases_count++;
        }
    }

    if (basis->bases_count == 0) {
        /* 理想是零理想 */
        basis->is_minimal = true;
        basis->is_reduced = true;
        basis->reducing_degree = 0;
        return basis;
    }

    int vc = ring->var_count;

    /* 构建对集合 B：用二维数组标记哪些对已被处理 */
    /* 使用简单方法：维护一个增长的对列表 */
    int pair_capacity = 4096;
    int pair_count = 0;
    int *pairs_i = (int *) lv_malloc((size_t) pair_capacity * sizeof(int));
    int *pairs_j = (int *) lv_malloc((size_t) pair_capacity * sizeof(int));
    if (!pairs_i || !pairs_j) {
        lv_free((void **) &pairs_i);
        lv_free((void **) &pairs_j);
        for (int i = 0; i < basis->bases_count; i++) {
            poly_internal_destroy(basis->basis_polys[i]);
        }
        lv_free((void **) &basis->basis_polys);
        lv_free((void **) &basis);
        return NULL;
    }

    /* 初始化：所有 (i, j) 对，i < j */
    for (int i = 0; i < basis->bases_count; i++) {
        for (int j = i + 1; j < basis->bases_count; j++) {
            if (pair_count >= pair_capacity) {
                int cap_i = pair_capacity, cap_j = pair_capacity;
                if (!lv_ensure_capacity((void **) &pairs_i, pair_count, &cap_i, sizeof(int), 1) ||
                    !lv_ensure_capacity((void **) &pairs_j, pair_count, &cap_j, sizeof(int), 1)) {
                    /* 失败时各指针保持有效（成功的已更新、失败的未动） */
                    lv_free((void **) &pairs_i);
                    lv_free((void **) &pairs_j);
                    for (int k = 0; k < basis->bases_count; k++) {
                        poly_internal_destroy(basis->basis_polys[k]);
                    }
                    lv_free((void **) &basis->basis_polys);
                    lv_free((void **) &basis);
                    return NULL;
                }
                pair_capacity = (cap_i > cap_j) ? cap_i : cap_j;
            }
            pairs_i[pair_count] = i;
            pairs_j[pair_count] = j;
            pair_count++;
        }
    }

    int step = 0;
    int buchberger_max = lv_config_get_int(LV_CFG_BUCHBERGER_MAX_STEPS, 50000);

    while (pair_count > 0 && step < buchberger_max) {
        step++;

        /* 取一对 */
        pair_count--;
        int idx_i = pairs_i[pair_count];
        int idx_j = pairs_j[pair_count];

        lvPolynomial *fi = basis->basis_polys[idx_i];
        lvPolynomial *fj = basis->basis_polys[idx_j];

        /* 优化 1：互质判别式 —— 若前导项互质，则 S(fi, fj) 一定约化为 0 */
        int *lt_i = (int *) lv_calloc((size_t) vc, sizeof(int));
        int *lt_j = (int *) lv_calloc((size_t) vc, sizeof(int));
        if (!lt_i || !lt_j) {
            lv_free((void **) &lt_i);
            lv_free((void **) &lt_j);
            continue;
        }

        if (poly_leading_term(fi, ring, lt_i, NULL) != 0 || poly_leading_term(fj, ring, lt_j, NULL) != 0) {
            lv_free((void **) &lt_i);
            lv_free((void **) &lt_j);
            continue;
        }

        bool coprime = mono_is_coprime(ring, lt_i, lt_j);
        lv_free((void **) &lt_i);
        lv_free((void **) &lt_j);

        if (coprime) {
            /* 互质 => S(fi, fj) 约化为 0，跳过 */
            continue;
        }

        /* 计算 S-多项式 */
        lvPolynomial *s = poly_internal_s_polynomial(fi, fj, ring);
        if (!s) {
            continue;
        }

        /* 用当前基约化 S-多项式 */
        lvPolynomial *r = poly_internal_reduce(s, basis->basis_polys, basis->bases_count, ring);
        poly_internal_destroy(s);
        if (!r) {
            continue;
        }

        /* 如果约化结果非零，加入基 */
        if (!poly_internal_is_zero(r)) {
            /* 扩容基数组 */
            if (basis->bases_count >= basis->bases_capacity) {
                if (!lv_ensure_capacity((void **) &basis->basis_polys, basis->bases_count, &basis->bases_capacity,
                                        sizeof(lvPolynomial *), 1)) {
                    poly_internal_destroy(r);
                    break;
                }
            }

            int new_idx = basis->bases_count;
            basis->basis_polys[new_idx] = r;
            basis->bases_count++;

            /* 添加新对 (existing_i, new_idx) 到 B */
            for (int i = 0; i < new_idx; i++) {
                if (pair_count >= pair_capacity) {
                    int cap_i = pair_capacity, cap_j = pair_capacity;
                    if (!lv_ensure_capacity((void **) &pairs_i, pair_count, &cap_i, sizeof(int), 1) ||
                        !lv_ensure_capacity((void **) &pairs_j, pair_count, &cap_j, sizeof(int), 1)) {
                        /* 失败时各指针保持有效，退出主循环（末尾统一释放） */
                        pair_count = 0;
                        break;
                    }
                    pair_capacity = (cap_i > cap_j) ? cap_i : cap_j;
                }
                pairs_i[pair_count] = i;
                pairs_j[pair_count] = new_idx;
                pair_count++;
            }
        } else {
            poly_internal_destroy(r);
        }
    }

    lv_free((void **) &pairs_i);
    lv_free((void **) &pairs_j);

    /* 计算约化 Groebner 基 */
    basis = groebner_internal_reduce_basis(basis, ring);

    return basis;
}

/**
 * @brief 约化 Groebner 基 —— 使基满足最小且约化的属性
 *
 * 1. 最小化：移除前导项可被其他元素前导项整除的元素
 * 2. 约化：每个基元素的前导系数规一化，并用其他基元素约化降低其余项
 *
 * @param basis 原始基
 * @param ring  环
 * @return 约化后的基（原地修改）
 */
lvGroebnerBasis *groebner_internal_reduce_basis(lvGroebnerBasis *basis, const lvPolynomialRing *ring) {
    if (!basis || !ring || basis->bases_count == 0) {
        if (basis) {
            basis->is_minimal = true;
            basis->is_reduced = true;
        }
        return basis;
    }

    int vc = ring->var_count;

    /* 规一化所有基多项式的前导系数 */
    for (int i = 0; i < basis->bases_count; i++) {
        lvPolynomial *p = basis->basis_polys[i];
        if (poly_internal_is_zero(p))
            continue;
        double lc;
        if (poly_leading_term(p, ring, NULL, &lc) == 0 && fabs(lc) > lv_config_get_double(LV_CFG_GROEBNER_ZERO_THRESHOLD, GROEBNER_ZERO_THRESHOLD)) {
            poly_internal_scale(p, 1.0 / lc);
        }
    }

    /* 最小化：删除前导项可被其他基元前导项整除的元素 */
    int write_pos = 0;
    for (int i = 0; i < basis->bases_count; i++) {
        lvPolynomial *pi = basis->basis_polys[i];
        if (poly_internal_is_zero(pi)) {
            poly_internal_destroy(pi);
            continue;
        }
        int *lt_pi = (int *) lv_calloc((size_t) vc, sizeof(int));
        if (!lt_pi)
            continue;
        if (poly_leading_term(pi, ring, lt_pi, NULL) != 0) {
            lv_free((void **) &lt_pi);
            poly_internal_destroy(pi);
            continue;
        }

        bool redundant = false;
        for (int j = 0; j < basis->bases_count; j++) {
            if (i == j)
                continue;
            lvPolynomial *pj = basis->basis_polys[j];
            if (poly_internal_is_zero(pj))
                continue;
            int *lt_pj = (int *) lv_calloc((size_t) vc, sizeof(int));
            if (!lt_pj)
                continue;
            if (poly_leading_term(pj, ring, lt_pj, NULL) == 0) {
                if (mono_divides(ring, lt_pi, lt_pj)) {
                    redundant = true;
                    lv_free((void **) &lt_pj);
                    break;
                }
            }
            lv_free((void **) &lt_pj);
        }

        lv_free((void **) &lt_pi);

        if (!redundant) {
            basis->basis_polys[write_pos] = pi;
            write_pos++;
        } else {
            poly_internal_destroy(pi);
        }
    }
    basis->bases_count = write_pos;

    /* 互相约化：每个基元素用其余基元素约化 */
    for (int i = 0; i < basis->bases_count; i++) {
        /* 构建不含第 i 个元素的基数组 */
        lvPolynomial **others = (lvPolynomial **) lv_malloc((size_t) (basis->bases_count - 1) * sizeof(lvPolynomial *));
        if (!others)
            continue;
        int o_count = 0;
        for (int j = 0; j < basis->bases_count; j++) {
            if (j != i) {
                others[o_count++] = basis->basis_polys[j];
            }
        }
        lvPolynomial *reduced = poly_internal_reduce(basis->basis_polys[i], others, o_count, ring);
        lv_free((void **) &others);
        if (reduced) {
            poly_internal_destroy(basis->basis_polys[i]);
            basis->basis_polys[i] = reduced;
        }
    }

    /* 再次规一化 */
    for (int i = 0; i < basis->bases_count; i++) {
        lvPolynomial *p = basis->basis_polys[i];
        if (poly_internal_is_zero(p))
            continue;
        double lc;
        if (poly_leading_term(p, ring, NULL, &lc) == 0 && fabs(lc) > lv_config_get_double(LV_CFG_GROEBNER_ZERO_THRESHOLD, GROEBNER_ZERO_THRESHOLD)) {
            poly_internal_scale(p, 1.0 / lc);
        }
    }

    /* 计算约化后的最大次数 */
    int max_deg = 0;
    for (int i = 0; i < basis->bases_count; i++) {
        int deg = poly_internal_total_degree(basis->basis_polys[i], vc);
        if (deg > max_deg) {
            max_deg = deg;
        }
    }
    basis->reducing_degree = max_deg;
    basis->is_minimal = true;
    basis->is_reduced = true;

    return basis;
}

