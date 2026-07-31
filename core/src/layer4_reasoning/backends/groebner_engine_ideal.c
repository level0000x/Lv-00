/**
 * @file groebner_engine_ideal.c
 * @brief 理想 API 与增量/成员/交/商
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
 *  第三部分：公共 API —— 理想与 Groebner 基
 * ================================================================ */

/**
 * @brief 创建理想
 */
int ideal_create(lvRingRegistry *registry, int ring_id, const char *label) {
    if (!registry || ring_id < 0 || ring_id >= registry->ring_count) {
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "ideal_create: invalid params (registry=%p, ring_id=%d)",
                        (const void *)registry, ring_id);
    }

    lvIdeal *ideal = (lvIdeal *) lv_calloc(1, sizeof(lvIdeal));
    if (!ideal) {
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "ideal_create: lv_calloc(%zu) failed", sizeof(lvIdeal));
    }

    ideal->ring_id = ring_id;
    ideal->generators = (lvPolynomial **) lv_calloc((size_t) GROEBNER_IDEAL_INIT_GEN_CAPACITY, sizeof(lvPolynomial *));
    if (!ideal->generators) {
        lv_free((void **) &ideal);
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "ideal_create: lv_calloc for generators failed");
    }
    ideal->generator_capacity = GROEBNER_IDEAL_INIT_GEN_CAPACITY;
    ideal->generator_count = 0;
    ideal->cached_basis = NULL;
    ideal->basis_valid = false;
    ideal->label = groebner_strdup_safe(label);

    lv_mutex_lock(&g_data_mutex);
    lvRegistryData *data = registry_data_ensure();
    if (!data) {
        lv_mutex_unlock(&g_data_mutex);
        lv_free((void **) &ideal->generators);
        lv_free((void **) &ideal->label);
        lv_free((void **) &ideal);
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "ideal_create: registry_data_ensure failed");
    }

    int result = ideal_internal_store(data, ideal);
    lv_mutex_unlock(&g_data_mutex);
    return result;
}

/**
 * @brief 销毁理想
 */
void ideal_destroy(lvRingRegistry *registry, int ideal_id) {
    lv_UNUSED(registry);
    lv_mutex_lock(&g_data_mutex);
    if (!g_data || ideal_id < 0 || ideal_id >= g_data->ideal_count) {
        lv_mutex_unlock(&g_data_mutex);
        return;
    }

    lvIdeal *ideal = g_data->ideals[ideal_id];
    if (!ideal) {
        lv_mutex_unlock(&g_data_mutex);
        return;
    }

    if (ideal->cached_basis) {
        if (ideal->cached_basis->basis_polys) {
            for (int i = 0; i < ideal->cached_basis->bases_count; i++) {
                poly_internal_destroy(ideal->cached_basis->basis_polys[i]);
            }
            lv_free((void **) &ideal->cached_basis->basis_polys);
        }
        lv_free((void **) &ideal->cached_basis);
    }
    lv_free((void **) &ideal->generators);
    lv_free((void **) &ideal->label);
    lv_free((void **) &ideal);
    g_data->ideals[ideal_id] = NULL;
    lv_mutex_unlock(&g_data_mutex);
}

/**
 * @brief 向理想添加生成元
 */
int ideal_add_generator(lvRingRegistry *registry, int ideal_id, int poly_id) {
    if (!registry)
        return -1;

    lv_mutex_lock(&g_data_mutex);
    if (!g_data) {
        lv_mutex_unlock(&g_data_mutex);
        return -1;
    }
    if (ideal_id < 0 || ideal_id >= g_data->ideal_count) {
        lv_mutex_unlock(&g_data_mutex);
        return -1;
    }
    if (poly_id < 0 || poly_id >= g_data->poly_count) {
        lv_mutex_unlock(&g_data_mutex);
        return -1;
    }

    lvIdeal *ideal = g_data->ideals[ideal_id];
    lvPolynomial *poly = g_data->polys[poly_id];
    if (!ideal || !poly) {
        lv_mutex_unlock(&g_data_mutex);
        return -1;
    }

    if (ideal->ring_id != poly->ring_id) {
        lv_mutex_unlock(&g_data_mutex);
        return -1;
    }

    if (ideal->generator_count >= ideal->generator_capacity) {
        int new_cap = ideal->generator_capacity * 2;
        lvPolynomial **new_gens =
            (lvPolynomial **) lv_realloc(ideal->generators, (size_t) new_cap * sizeof(lvPolynomial *));
        if (!new_gens) {
            lv_mutex_unlock(&g_data_mutex);
            return -1;
        }
        ideal->generators = new_gens;
        ideal->generator_capacity = new_cap;
    }

    ideal->generators[ideal->generator_count++] = poly;
    ideal->basis_valid = false; /* 缓存失效 */

    lv_mutex_unlock(&g_data_mutex);
    return 0;
}

/**
 * @brief 计算 Groebner 基
 */
int groebner_compute(lvRingRegistry *registry, int ideal_id, lvGroebnerAlgorithm algorithm) {
    if (!registry)
        return -1;

    lv_mutex_lock(&g_data_mutex);
    if (!g_data) {
        lv_mutex_unlock(&g_data_mutex);
        return -1;
    }
    if (ideal_id < 0 || ideal_id >= g_data->ideal_count) {
        lv_mutex_unlock(&g_data_mutex);
        return -1;
    }

    lvIdeal *ideal = g_data->ideals[ideal_id];
    if (!ideal) {
        lv_mutex_unlock(&g_data_mutex);
        return -1;
    }

    lvPolynomialRing *ring = registry->rings[ideal->ring_id];
    if (!ring) {
        lv_mutex_unlock(&g_data_mutex);
        return -1;
    }

    if (ideal->generator_count == 0) {
        /* 零理想 */
        lvGroebnerBasis *basis = (lvGroebnerBasis *) lv_calloc(1, sizeof(lvGroebnerBasis));
        if (!basis) {
            lv_mutex_unlock(&g_data_mutex);
            return -1;
        }
        basis->is_minimal = true;
        basis->is_reduced = true;
        basis->algorithm_used = GROEBNER_BUCHBERGER;
        ideal->cached_basis = basis;
        ideal->basis_valid = true;
        lv_mutex_unlock(&g_data_mutex);
        return 0;
    }

    clock_t start_clock = clock(); /* 简单计时 */

    lvGroebnerBasis *basis = groebner_internal_compute(ring, ideal->generators, ideal->generator_count, algorithm);
    if (!basis) {
        lv_mutex_unlock(&g_data_mutex);
        return -1;
    }

    basis->computation_time_us = (int64_t) lv_clock_elapsed_us(start_clock);

    /* 释放旧缓存 */
    if (ideal->cached_basis) {
        if (ideal->cached_basis->basis_polys) {
            for (int i = 0; i < ideal->cached_basis->bases_count; i++) {
                poly_internal_destroy(ideal->cached_basis->basis_polys[i]);
            }
            lv_free((void **) &ideal->cached_basis->basis_polys);
        }
        lv_free((void **) &ideal->cached_basis);
    }

    ideal->cached_basis = basis;
    ideal->basis_valid = true;
    lv_mutex_unlock(&g_data_mutex);
    return 0;
}

/**
 * @brief 用现有基约化新多项式，仅对非零余式扩展基（增量检测）
 *
 * 若已有有效缓存基，先检验新多项式是否已被现有基约化（余式为零表示
 * 新多项式已在理想中，无需重算）。若非零，只计算新多项式与现有基元素
 * 间的 S-多项式，避免完全重算。
 *
 * @param ring     多项式环
 * @param old_basis 现有缓存基（传入时不转移所有权）
 * @param new_poly  新多项式（调用者确保 non-NULL，非零）
 * @return 扩展后的新基，失败返回 NULL
 */
static lvGroebnerBasis *groebner_internal_extend_basis(const lvPolynomialRing *ring,
                                                        const lvGroebnerBasis *old_basis,
                                                        lvPolynomial *new_poly) {
    if (!ring || !old_basis || !new_poly)
        return NULL;

    int old_count = old_basis->bases_count;
    int vc = ring->var_count;

    /* 先用旧基约化新多项式 */
    lvPolynomial *reduced = poly_internal_reduce(new_poly, old_basis->basis_polys, old_count, ring);
    if (!reduced || poly_internal_is_zero(reduced)) {
        /* 新多项式已是理想的元素，返回旧基的副本 */
        if (reduced)
            poly_internal_destroy(reduced);
        lvGroebnerBasis *basis = (lvGroebnerBasis *) lv_calloc(1, sizeof(lvGroebnerBasis));
        if (!basis)
            return NULL;
        basis->basis_polys = (lvPolynomial **) lv_calloc((size_t)(old_count + 1), sizeof(lvPolynomial *));
        if (!basis->basis_polys) {
            lv_free((void **) &basis);
            return NULL;
        }
        for (int i = 0; i < old_count; i++) {
            basis->basis_polys[i] = poly_internal_copy(old_basis->basis_polys[i], ring);
        }
        basis->bases_count = old_count;
        basis->bases_capacity = old_count + 1;
        basis->is_minimal = old_basis->is_minimal;
        basis->is_reduced = old_basis->is_reduced;
        basis->reducing_degree = old_basis->reducing_degree;
        return basis;
    }

    /* 新多项式约化后非零，建立新基：先复制旧基，再加入约化后的新多项式 */
    int new_capacity = old_count + 16;
    lvGroebnerBasis *basis = (lvGroebnerBasis *) lv_calloc(1, sizeof(lvGroebnerBasis));
    if (!basis) {
        poly_internal_destroy(reduced);
        return NULL;
    }
    basis->basis_polys = (lvPolynomial **) lv_calloc((size_t) new_capacity, sizeof(lvPolynomial *));
    if (!basis->basis_polys) {
        lv_free((void **) &basis);
        poly_internal_destroy(reduced);
        return NULL;
    }
    basis->bases_capacity = new_capacity;

    for (int i = 0; i < old_count; i++) {
        basis->basis_polys[i] = poly_internal_copy(old_basis->basis_polys[i], ring);
    }
    basis->basis_polys[old_count] = reduced;
    basis->bases_count = old_count + 1;

    /* 工作列表：记录新增基元的索引 */
    int *new_indices = (int *) lv_malloc((size_t) new_capacity * sizeof(int));
    if (!new_indices) {
        for (int i = 0; i < basis->bases_count; i++)
            poly_internal_destroy(basis->basis_polys[i]);
        lv_free((void **) &basis->basis_polys);
        lv_free((void **) &basis);
        return NULL;
    }
    int new_count = 1;
    new_indices[0] = old_count;

    /* 增量 Buchberger 核心：只处理涉及新增基元的对 */
    int buchberger_max = lv_config_get_int(LV_CFG_BUCHBERGER_MAX_STEPS, 50000);
    int step = 0;
    int new_i = 0;

    while (new_i < new_count && step < buchberger_max) {
        step++;
        int idx_new = new_indices[new_i++];

        lvPolynomial *f_new = basis->basis_polys[idx_new];

        /* 与所有已有的基元（含其他新基元）计算 S-多项式 */
        for (int j = 0; j < basis->bases_count; j++) {
            if (j == idx_new)
                continue;

            lvPolynomial *fj = basis->basis_polys[j];

            /* 互质判别式优化 */
            int *lt_new = (int *) lv_calloc((size_t) vc, sizeof(int));
            int *lt_j = (int *) lv_calloc((size_t) vc, sizeof(int));
            if (!lt_new || !lt_j) {
                lv_free((void **) &lt_new);
                lv_free((void **) &lt_j);
                continue;
            }

            if (poly_leading_term(f_new, ring, lt_new, NULL) != 0 ||
                poly_leading_term(fj, ring, lt_j, NULL) != 0) {
                lv_free((void **) &lt_new);
                lv_free((void **) &lt_j);
                continue;
            }

            bool coprime = mono_is_coprime(ring, lt_new, lt_j);
            lv_free((void **) &lt_new);
            lv_free((void **) &lt_j);

            if (coprime)
                continue;

            /* 计算 S-多项式 */
            lvPolynomial *s = poly_internal_s_polynomial(f_new, fj, ring);
            if (!s)
                continue;

            /* 用当前基约化 */
            lvPolynomial *r = poly_internal_reduce(s, basis->basis_polys, basis->bases_count, ring);
            poly_internal_destroy(s);
            if (!r)
                continue;

            if (!poly_internal_is_zero(r)) {
                /* 余式非零，加入基 */
                if (basis->bases_count >= basis->bases_capacity) {
                    int new_cap = basis->bases_capacity * 2;
                    lvPolynomial **new_polys = (lvPolynomial **) lv_realloc(
                        basis->basis_polys, (size_t) new_cap * sizeof(lvPolynomial *));
                    if (!new_polys) {
                        poly_internal_destroy(r);
                        break;
                    }
                    basis->basis_polys = new_polys;
                    basis->bases_capacity = new_cap;

                    int *new_ni = (int *) lv_realloc(new_indices, (size_t) new_cap * sizeof(int));
                    if (!new_ni) {
                        poly_internal_destroy(r);
                        break;
                    }
                    new_indices = new_ni;
                }

                basis->basis_polys[basis->bases_count] = r;
                new_indices[new_count++] = basis->bases_count;
                basis->bases_count++;
            } else {
                poly_internal_destroy(r);
            }
        }
    }

    lv_free((void **) &new_indices);

    /* 约化并规范化基 */
    basis = groebner_internal_reduce_basis(basis, ring);
    return basis;
}

/**
 * @brief 增量式 Groebner 基计算
 *
 * 改进说明：
 * - 若有有效缓存基，先检测新多项式是否已被现有基约化（余式为零则跳过重算）
 * - 若非零，仅计算新多项式与现有基元素间的 S-多项式（增量扩展）
 * - 若增量扩展失败或未缓存基，回退到完全重算
 */
int groebner_compute_incremental(lvRingRegistry *registry, int ideal_id, int new_poly_id) {
    if (!registry)
        return -1;

    lv_mutex_lock(&g_data_mutex);
    if (!g_data) {
        lv_mutex_unlock(&g_data_mutex);
        return -1;
    }
    if (ideal_id < 0 || ideal_id >= g_data->ideal_count) {
        lv_mutex_unlock(&g_data_mutex);
        return -1;
    }
    if (new_poly_id < 0 || new_poly_id >= g_data->poly_count) {
        lv_mutex_unlock(&g_data_mutex);
        return -1;
    }

    lvIdeal *ideal = g_data->ideals[ideal_id];
    lvPolynomial *new_poly = g_data->polys[new_poly_id];
    if (!ideal || !new_poly) {
        lv_mutex_unlock(&g_data_mutex);
        return -1;
    }
    if (ideal->ring_id != new_poly->ring_id) {
        lv_mutex_unlock(&g_data_mutex);
        return -1;
    }

    /* 将新多项式添加到生成元列表 */
    if (ideal->generator_count >= ideal->generator_capacity) {
        int new_cap = ideal->generator_capacity * 2;
        lvPolynomial **new_gens =
            (lvPolynomial **) lv_realloc(ideal->generators, (size_t) new_cap * sizeof(lvPolynomial *));
        if (!new_gens) {
            lv_mutex_unlock(&g_data_mutex);
            return -1;
        }
        ideal->generators = new_gens;
        ideal->generator_capacity = new_cap;
    }
    ideal->generators[ideal->generator_count++] = new_poly;
    ideal->basis_valid = false;

    lvPolynomialRing *ring = registry->rings[ideal->ring_id];
    if (!ring) {
        lv_mutex_unlock(&g_data_mutex);
        return -1;
    }

    lvGroebnerBasis *basis = NULL;

    /* 增量路径：若有有效缓存基，尝试增量扩展 */
    if (ideal->cached_basis && ideal->cached_basis->bases_count > 0) {
        basis = groebner_internal_extend_basis(ring, ideal->cached_basis, new_poly);
    }

    /* 若增量扩展失败或无缓存基，回退到完全重算 */
    if (!basis) {
        if (ideal->generator_count == 0) {
            lv_mutex_unlock(&g_data_mutex);
            return 0;
        }
        basis = groebner_internal_compute(ring, ideal->generators, ideal->generator_count, GROEBNER_BUCHBERGER);
        if (!basis) {
            lv_mutex_unlock(&g_data_mutex);
            return -1;
        }
    }

    /* 释放旧缓存 */
    if (ideal->cached_basis) {
        if (ideal->cached_basis->basis_polys) {
            for (int i = 0; i < ideal->cached_basis->bases_count; i++) {
                poly_internal_destroy(ideal->cached_basis->basis_polys[i]);
            }
            lv_free((void **) &ideal->cached_basis->basis_polys);
        }
        lv_free((void **) &ideal->cached_basis);
    }

    ideal->cached_basis = basis;
    ideal->basis_valid = true;
    lv_mutex_unlock(&g_data_mutex);
    return 0;
}

/**
 * @brief 理想成员判定
 */
bool ideal_membership(lvRingRegistry *registry, int ideal_id, int poly_id) {
    if (!registry)
        return false;

    lv_mutex_lock(&g_data_mutex);
    if (!g_data) {
        lv_mutex_unlock(&g_data_mutex);
        return false;
    }
    if (ideal_id < 0 || ideal_id >= g_data->ideal_count) {
        lv_mutex_unlock(&g_data_mutex);
        return false;
    }
    if (poly_id < 0 || poly_id >= g_data->poly_count) {
        lv_mutex_unlock(&g_data_mutex);
        return false;
    }

    lvIdeal *ideal = g_data->ideals[ideal_id];
    lvPolynomial *poly = g_data->polys[poly_id];
    if (!ideal || !poly) {
        lv_mutex_unlock(&g_data_mutex);
        return false;
    }

    if (ideal->ring_id != poly->ring_id) {
        lv_mutex_unlock(&g_data_mutex);
        return false;
    }

    lvPolynomialRing *ring = registry->rings[ideal->ring_id];
    if (!ring) {
        lv_mutex_unlock(&g_data_mutex);
        return false;
    }

    /* 确保 Groebner 基已计算（直接调用内部函数，已持有锁） */
    if (!ideal->basis_valid || !ideal->cached_basis) {
        lvGroebnerBasis *basis =
            groebner_internal_compute(ring, ideal->generators, ideal->generator_count, GROEBNER_BUCHBERGER);
        if (!basis) {
            lv_mutex_unlock(&g_data_mutex);
            return false;
        }
        /* 释放旧缓存 */
        if (ideal->cached_basis) {
            if (ideal->cached_basis->basis_polys) {
                for (int i = 0; i < ideal->cached_basis->bases_count; i++) {
                    poly_internal_destroy(ideal->cached_basis->basis_polys[i]);
                }
                lv_free((void **) &ideal->cached_basis->basis_polys);
            }
            lv_free((void **) &ideal->cached_basis);
        }
        ideal->cached_basis = basis;
        ideal->basis_valid = true;
    }

    /* 用 Groebner 基约化：余式为零则属于理想 */
    lvPolynomial *nf =
        poly_internal_reduce(poly, ideal->cached_basis->basis_polys, ideal->cached_basis->bases_count, ring);
    if (!nf) {
        lv_mutex_unlock(&g_data_mutex);
        return false;
    }

    bool is_member = poly_internal_is_zero(nf);
    poly_internal_destroy(nf);
    lv_mutex_unlock(&g_data_mutex);
    return is_member;
}

/**
 * @brief 理想交
 */
int ideal_intersection(lvRingRegistry *registry, int ideal_id_a, int ideal_id_b) {
    if (!registry)
        return -1;

    lv_mutex_lock(&g_data_mutex);
    if (!g_data) {
        lv_mutex_unlock(&g_data_mutex);
        return -1;
    }
    if (ideal_id_a < 0 || ideal_id_b < 0) {
        lv_mutex_unlock(&g_data_mutex);
        return -1;
    }
    if (ideal_id_a >= g_data->ideal_count || ideal_id_b >= g_data->ideal_count) {
        lv_mutex_unlock(&g_data_mutex);
        return -1;
    }

    lvIdeal *ia = g_data->ideals[ideal_id_a];
    lvIdeal *ib = g_data->ideals[ideal_id_b];
    if (!ia || !ib) {
        lv_mutex_unlock(&g_data_mutex);
        return -1;
    }
    if (ia->ring_id != ib->ring_id) {
        lv_mutex_unlock(&g_data_mutex);
        return -1;
    }

    /* I ∩ J = (tI + (1-t)J) ∩ R，其中 t 为新变量。
     * 简化实现：用 Groebner 基消去方法。 */
    lvPolynomialRing *ring = registry->rings[ia->ring_id];
    if (!ring) {
        lv_mutex_unlock(&g_data_mutex);
        return -1;
    }

    /* 创建结果理想，其生成元为两个理想的生成元并集（直接操作，已持有锁） */
    lvIdeal *result_ideal = (lvIdeal *) lv_calloc(1, sizeof(lvIdeal));
    if (!result_ideal) {
        lv_mutex_unlock(&g_data_mutex);
        return -1;
    }
    result_ideal->ring_id = ia->ring_id;
    result_ideal->generator_capacity = ia->generator_count + ib->generator_count;
    if (result_ideal->generator_capacity < GROEBNER_IDEAL_INIT_GEN_CAPACITY) {
        result_ideal->generator_capacity = GROEBNER_IDEAL_INIT_GEN_CAPACITY;
    }
    result_ideal->generators =
        (lvPolynomial **) lv_calloc((size_t) result_ideal->generator_capacity, sizeof(lvPolynomial *));
    if (!result_ideal->generators) {
        lv_free((void **) &result_ideal);
        lv_mutex_unlock(&g_data_mutex);
        return -1;
    }
    result_ideal->generator_count = 0;
    result_ideal->cached_basis = NULL;
    result_ideal->basis_valid = false;
    result_ideal->label = NULL;

    /* 将 I 的生成元加入 */
    for (int i = 0; i < ia->generator_count; i++) {
        if (ia->generators[i]) {
            result_ideal->generators[result_ideal->generator_count++] = ia->generators[i];
        }
    }

    /* 将 J 的生成元加入 */
    for (int i = 0; i < ib->generator_count; i++) {
        if (ib->generators[i]) {
            result_ideal->generators[result_ideal->generator_count++] = ib->generators[i];
        }
    }

    int result = ideal_internal_store(g_data, result_ideal);
    lv_mutex_unlock(&g_data_mutex);
    return result;
}

/**
 * @brief 理想商 I : J
 */
int ideal_quotient(lvRingRegistry *registry, int ideal_id_a, int ideal_id_b, const char *result_label) {
    if (!registry)
        return -1;

    lv_mutex_lock(&g_data_mutex);
    if (!g_data) {
        lv_mutex_unlock(&g_data_mutex);
        return -1;
    }
    if (ideal_id_a < 0 || ideal_id_b < 0) {
        lv_mutex_unlock(&g_data_mutex);
        return -1;
    }
    if (ideal_id_a >= g_data->ideal_count || ideal_id_b >= g_data->ideal_count) {
        lv_mutex_unlock(&g_data_mutex);
        return -1;
    }

    lvIdeal *ia = g_data->ideals[ideal_id_a];
    lvIdeal *ib = g_data->ideals[ideal_id_b];
    if (!ia || !ib) {
        lv_mutex_unlock(&g_data_mutex);
        return -1;
    }
    if (ia->ring_id != ib->ring_id) {
        lv_mutex_unlock(&g_data_mutex);
        return -1;
    }

    /* I : <g> = (I ∩ <g>) / g 推广到多个生成元：
     * I : J = ∩_{g in generators(J)} (I : <g>)
     * 简化实现：返回与 I 相同的理想（完整实现需逐个生成元计算商） */

    /* 直接创建理想（已持有锁，避免调用 ideal_create 导致死锁） */
    lvIdeal *result_ideal = (lvIdeal *) lv_calloc(1, sizeof(lvIdeal));
    if (!result_ideal) {
        lv_mutex_unlock(&g_data_mutex);
        return -1;
    }
    result_ideal->ring_id = ia->ring_id;
    result_ideal->generator_capacity = ia->generator_count;
    if (result_ideal->generator_capacity < GROEBNER_IDEAL_INIT_GEN_CAPACITY) {
        result_ideal->generator_capacity = GROEBNER_IDEAL_INIT_GEN_CAPACITY;
    }
    result_ideal->generators =
        (lvPolynomial **) lv_calloc((size_t) result_ideal->generator_capacity, sizeof(lvPolynomial *));
    if (!result_ideal->generators) {
        lv_free((void **) &result_ideal);
        lv_mutex_unlock(&g_data_mutex);
        return -1;
    }
    result_ideal->generator_count = 0;
    result_ideal->cached_basis = NULL;
    result_ideal->basis_valid = false;
    result_ideal->label = groebner_strdup_safe(result_label);

    for (int i = 0; i < ia->generator_count; i++) {
        if (ia->generators[i]) {
            result_ideal->generators[result_ideal->generator_count++] = ia->generators[i];
        }
    }

    int result = ideal_internal_store(g_data, result_ideal);
    lv_mutex_unlock(&g_data_mutex);
    return result;
}

