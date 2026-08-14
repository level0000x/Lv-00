/**
 * @file groebner_engine_ideal.c
 * @brief 理想 API 与增量/成员/交/商
 *
 * @details 从 groebner_engine.c 拆分的子模块（Lv-00 项目 v3.3.0+）。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "lv/groebner_engine.h"
#include "lv/lv_lifecycle.h"
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
 *  第三部分：公共 API —— 理想与 Groebner 基
 * ================================================================ */

/* ================================================================
 *  内部辅助 —— 理想 / Gröbner 基构造与释放
 * ================================================================ */

/* ---- lv_DEFER 作用域守卫族（本文件 goto cleanup 样板的统一替代） ---- */

/* 锁守卫 defer 化：加锁后注册，函数出口（含任意 return）自动解锁 */
static void groebner_lock_guard_defer_cleanup(void *p) {
    lv_lock_guard_destroy((lvLockGuard *) p);
}

/* lvIdeal 守卫：释放 generators/label/外壳（生成元元素归全局多项式池所有，不在此释放）；
 * ideal 置 NULL 即解除守卫（注册成功时移交注册表） */
typedef struct {
    lvIdeal *ideal;
} GroebnerIdealGuard;

static void groebner_ideal_guard_cleanup(void *p) {
    GroebnerIdealGuard *g = (GroebnerIdealGuard *) p;
    if (g->ideal)
        lv_free_many((void **) &g->ideal->generators, (void **) &g->ideal->label, (void **) &g->ideal, NULL);
}

/* 单个 lvPolynomial* 变量的 defer 清理回调（置 NULL 即解除守卫） */
static void groebner_poly_defer_destroy(void *p) {
    lvPolynomial **pp = (lvPolynomial **) p;
    if (*pp)
        poly_internal_destroy(*pp);
}

/* 单个 lvGroebnerBasis* 变量的 defer 清理回调（置 NULL 即解除守卫） */
static void groebner_basis_defer_destroy(void *p) {
    lvGroebnerBasis **pp = (lvGroebnerBasis **) p;
    if (*pp)
        basis_destroy(*pp);
}

/* 多项式数组守卫：清理时读取（arr 变量当前值 / count 变量当前值），逐元素销毁后释放数组；
 * 用于池归属逐段变化时仍能按最新状态清理；置 NULL（arr 变量）即解除守卫 */
typedef struct {
    lvPolynomial ***arr; /* 指向 lvPolynomial ** 指针变量的地址 */
    int *count;          /* 指向元素个数变量的地址 */
} GroebnerPolyArrayGuard;

static void groebner_poly_array_guard_cleanup(void *p) {
    GroebnerPolyArrayGuard *g = (GroebnerPolyArrayGuard *) p;
    lvPolynomial **arr = g->arr ? *g->arr : NULL;
    int n = g->count ? *g->count : 0;
    if (arr) {
        for (int i = 0; i < n; i++)
            poly_internal_destroy(arr[i]);
        lv_free((void **) g->arr);
    }
}

/* 辅助环守卫：释放 elim_vars 与环外壳 */
typedef struct {
    lvPolynomialRing *ring;
} GroebnerRingGuard;

static void groebner_ring_guard_cleanup(void *p) {
    GroebnerRingGuard *g = (GroebnerRingGuard *) p;
    if (g->ring) {
        lv_free((void **) &g->ring->elim_vars);
        lv_free((void **) &g->ring);
    }
}

lvGroebnerBasis *basis_alloc(int capacity) {
    lvGroebnerBasis *basis = (lvGroebnerBasis *) lv_calloc(1, sizeof(lvGroebnerBasis));
    if (!basis) {
        return NULL;
    }
    /* 注册作用域守卫：basis_polys 分配失败时自动释放外壳 */
    lv_DEFER(lv_defer_free_ptr, &basis);
    if (capacity > 0) {
        basis->basis_polys = (lvPolynomial **) lv_calloc((size_t) capacity, sizeof(lvPolynomial *));
        if (!basis->basis_polys) {
            return NULL;
        }
    }
    basis->bases_capacity = capacity;
    basis->bases_count = 0;
    lvGroebnerBasis *out = basis;
    basis = NULL; /* 守卫解除：结果移交调用方 */
    return out;
}

void basis_destroy(lvGroebnerBasis *basis) {
    if (!basis) {
        return;
    }
    if (basis->basis_polys) {
        for (int i = 0; i < basis->bases_count; i++) {
            poly_internal_destroy(basis->basis_polys[i]);
        }
        lv_free((void **) &basis->basis_polys);
    }
    lv_free((void **) &basis);
}

void ideal_clear_cached_basis(lvIdeal *ideal) {
    if (!ideal || !ideal->cached_basis) {
        return;
    }
    basis_destroy(ideal->cached_basis);
    ideal->cached_basis = NULL;
}

static lvIdeal *ideal_alloc_locked(int ring_id, int capacity, const char *label) {
    if (capacity < GROEBNER_IDEAL_INIT_GEN_CAPACITY) {
        capacity = GROEBNER_IDEAL_INIT_GEN_CAPACITY;
    }
    lvIdeal *ideal = (lvIdeal *) lv_calloc(1, sizeof(lvIdeal));
    if (!ideal) {
        return NULL;
    }
    /* 注册作用域守卫：generators 分配失败时自动释放外壳 */
    lv_DEFER(lv_defer_free_ptr, &ideal);
    ideal->ring_id = ring_id;
    ideal->generators = (lvPolynomial **) lv_calloc((size_t) capacity, sizeof(lvPolynomial *));
    if (!ideal->generators) {
        return NULL;
    }
    ideal->generator_capacity = capacity;
    ideal->generator_count = 0;
    ideal->cached_basis = NULL;
    ideal->basis_valid = false;
    ideal->label = lv_strdup_safe(label);
    lvIdeal *out = ideal;
    ideal = NULL; /* 守卫解除：结果移交调用方 */
    return out;
}

/**
 * @brief 创建理想
 */
int ideal_create(lvRingRegistry *registry, int ring_id, const char *label) {
    if (!groebner_registry_has_ring(registry, ring_id)) {
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "ideal_create: invalid params (registry=%p, ring_id=%d)",
                        (const void *)registry, ring_id);
    }

    lvIdeal *ideal = ideal_alloc_locked(ring_id, GROEBNER_IDEAL_INIT_GEN_CAPACITY, label);
    if (!ideal) {
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "ideal_create: ideal_alloc_locked failed");
    }

    /* 注册守卫：registry_data_ensure 失败时自动释放理想结构 */
    GroebnerIdealGuard ideal_guard = {ideal};
    lv_DEFER(groebner_ideal_guard_cleanup, &ideal_guard);

    lvLockGuard _lg;
    groebner_lock_guard_init(&_lg);
    lv_DEFER(groebner_lock_guard_defer_cleanup, &_lg);

    lvRegistryData *data = registry_data_ensure();
    if (!data) {
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "ideal_create: registry_data_ensure failed");
    }

    int result = ideal_internal_store(data, ideal);
    if (result >= 0)
        ideal_guard.ideal = NULL; /* 注册成功，理想归注册表所有 */
    return result;
}

/**
 * @brief 销毁理想
 */
void ideal_destroy(lvRingRegistry *registry, int ideal_id) {
    lv_UNUSED(registry);
    lvLockGuard _lg;
    groebner_lock_guard_init(&_lg);
    lv_DEFER(groebner_lock_guard_defer_cleanup, &_lg);
    if (!g_data || !lv_index_in_range(ideal_id, g_data->ideal_count)) {
        return;
    }

    lvIdeal *ideal = g_data->ideals[ideal_id];
    if (!ideal) {
        return;
    }

    ideal_clear_cached_basis(ideal);
    lv_free((void **) &ideal->generators);
    lv_free((void **) &ideal->label);
    lv_free((void **) &ideal);
    g_data->ideals[ideal_id] = NULL;
}

/**
 * @brief 向理想添加生成元
 */
int ideal_add_generator(lvRingRegistry *registry, int ideal_id, int poly_id) {
    /* exempt: 单指针 NULL 守卫（registry 非空），与 id 范围守卫不同构，保留 */
    if (!registry)
        return -1;

    lvLockGuard _lg;
    groebner_lock_guard_init(&_lg);
    lv_DEFER(groebner_lock_guard_defer_cleanup, &_lg);
    /* exempt: 单指针 NULL 守卫（g_data 非空），与 id 范围守卫不同构，保留 */
    if (!g_data) {
        return -1;
    }
    if (!lv_index_in_range(ideal_id, g_data->ideal_count)) {
        return -1;
    }
    if (!lv_index_in_range(poly_id, g_data->poly_count)) {
        return -1;
    }

    lvIdeal *ideal = g_data->ideals[ideal_id];
    lvPolynomial *poly = g_data->polys[poly_id];
    if (!ideal || !poly) {
        return -1;
    }

    if (ideal->ring_id != poly->ring_id) {
        return -1;
    }

    if (ideal->generator_count >= ideal->generator_capacity) {
        if (!lv_ensure_capacity((void **) &ideal->generators, ideal->generator_count, &ideal->generator_capacity,
                                sizeof(lvPolynomial *), 1)) {
            return -1;
        }
    }

    ideal->generators[ideal->generator_count++] = poly;
    ideal->basis_valid = false; /* 缓存失效 */

    return 0;
}

/**
 * @brief 计算 Groebner 基
 */
int groebner_compute(lvRingRegistry *registry, int ideal_id, lvGroebnerAlgorithm algorithm) {
    /* exempt: 单指针 NULL 守卫（registry 非空），与 id 范围守卫不同构，保留 */
    if (!registry)
        return -1;

    lvLockGuard _lg;
    groebner_lock_guard_init(&_lg);
    lv_DEFER(groebner_lock_guard_defer_cleanup, &_lg);
    /* exempt: 单指针 NULL 守卫（g_data 非空），与 id 范围守卫不同构，保留 */
    if (!g_data) {
        return -1;
    }
    if (!lv_index_in_range(ideal_id, g_data->ideal_count)) {
        return -1;
    }

    lvIdeal *ideal = g_data->ideals[ideal_id];
    if (!ideal) {
        return -1;
    }

    lvPolynomialRing *ring = registry->rings[ideal->ring_id];
    if (!ring) {
        return -1;
    }

    if (ideal->generator_count == 0) {
        /* 零理想 */
        lvGroebnerBasis *basis = basis_alloc(0);
        if (!basis) {
            return -1;
        }
        basis->is_minimal = true;
        basis->is_reduced = true;
        basis->algorithm_used = GROEBNER_BUCHBERGER;
        ideal->cached_basis = basis;
        ideal->basis_valid = true;
        return 0;
    }

    clock_t start_clock = clock(); /* 简单计时 */

    lvGroebnerBasis *basis = groebner_internal_compute(ring, ideal->generators, ideal->generator_count, algorithm);
    if (!basis) {
        return -1;
    }

    basis->computation_time_us = (int64_t) lv_clock_elapsed_us(start_clock);

    /* 释放旧缓存 */
    ideal_clear_cached_basis(ideal);

    ideal->cached_basis = basis;
    ideal->basis_valid = true;
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
        lvGroebnerBasis *basis = basis_alloc(old_count + 1);
        if (!basis)
            return NULL;
        for (int i = 0; i < old_count; i++) {
            basis->basis_polys[i] = poly_internal_copy(old_basis->basis_polys[i], ring);
        }
        basis->bases_count = old_count;
        basis->is_minimal = old_basis->is_minimal;
        basis->is_reduced = old_basis->is_reduced;
        basis->reducing_degree = old_basis->reducing_degree;
        return basis;
    }

    /* 新多项式约化后非零，建立新基：先复制旧基，再加入约化后的新多项式 */
    int new_capacity = old_count + 16;
    lvGroebnerBasis *basis = basis_alloc(new_capacity);
    if (!basis) {
        poly_internal_destroy(reduced);
        return NULL;
    }

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
    int buchberger_max = lv_config_get_int(LV_CFG_BUCHBERGER_MAX_STEPS, BUCHBERGER_MAX_STEPS_DEFAULT);
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
                    int old_cap = basis->bases_capacity;
                    if (!lv_ensure_capacity((void **) &basis->basis_polys, basis->bases_count, &basis->bases_capacity,
                                            sizeof(lvPolynomial *), 1)) {
                        poly_internal_destroy(r);
                        break;
                    }
                    /* new_indices 与 basis 同步扩容（此前容量恒等于 old_cap） */
                    int ni_cap = old_cap;
                    if (!lv_ensure_capacity((void **) &new_indices, new_count, &ni_cap, sizeof(int), 1)) {
                        poly_internal_destroy(r);
                        break;
                    }
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
    /* exempt: 单指针 NULL 守卫（registry 非空），与 id 范围守卫不同构，保留 */
    if (!registry)
        return -1;

    lvLockGuard _lg;
    groebner_lock_guard_init(&_lg);
    lv_DEFER(groebner_lock_guard_defer_cleanup, &_lg);
    /* exempt: 单指针 NULL 守卫（g_data 非空），与 id 范围守卫不同构，保留 */
    if (!g_data) {
        return -1;
    }
    if (!lv_index_in_range(ideal_id, g_data->ideal_count)) {
        return -1;
    }
    if (!lv_index_in_range(new_poly_id, g_data->poly_count)) {
        return -1;
    }

    lvIdeal *ideal = g_data->ideals[ideal_id];
    lvPolynomial *new_poly = g_data->polys[new_poly_id];
    if (!ideal || !new_poly) {
        return -1;
    }
    if (ideal->ring_id != new_poly->ring_id) {
        return -1;
    }

    /* 将新多项式添加到生成元列表 */
    if (ideal->generator_count >= ideal->generator_capacity) {
        if (!lv_ensure_capacity((void **) &ideal->generators, ideal->generator_count, &ideal->generator_capacity,
                                sizeof(lvPolynomial *), 1)) {
            return -1;
        }
    }
    ideal->generators[ideal->generator_count++] = new_poly;
    ideal->basis_valid = false;

    lvPolynomialRing *ring = registry->rings[ideal->ring_id];
    if (!ring) {
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
            return 0;
        }
        basis = groebner_internal_compute(ring, ideal->generators, ideal->generator_count, GROEBNER_BUCHBERGER);
        if (!basis) {
            return -1;
        }
    }

    /* 释放旧缓存 */
    ideal_clear_cached_basis(ideal);

    ideal->cached_basis = basis;
    ideal->basis_valid = true;
    return 0;
}

/**
 * @brief 理想成员判定
 */
bool ideal_membership(lvRingRegistry *registry, int ideal_id, int poly_id) {
    /* exempt: 单指针 NULL 守卫（registry 非空），与 id 范围守卫不同构，保留 */
    if (!registry)
        return false;

    bool result = false;
    lvLockGuard _lg;
    groebner_lock_guard_init(&_lg);
    lv_DEFER(groebner_lock_guard_defer_cleanup, &_lg);
    /* exempt: 单指针 NULL 守卫（g_data 非空），与 id 范围守卫不同构，保留 */
    if (!g_data) {
        return false;
    }
    if (!lv_index_in_range(ideal_id, g_data->ideal_count)) {
        return false;
    }
    if (!lv_index_in_range(poly_id, g_data->poly_count)) {
        return false;
    }

    lvIdeal *ideal = g_data->ideals[ideal_id];
    lvPolynomial *poly = g_data->polys[poly_id];
    if (!ideal || !poly) {
        return false;
    }

    if (ideal->ring_id != poly->ring_id) {
        return false;
    }

    lvPolynomialRing *ring = registry->rings[ideal->ring_id];
    if (!ring) {
        return false;
    }

    /* 确保 Groebner 基已计算（直接调用内部函数，已持有锁） */
    if (!ideal->basis_valid || !ideal->cached_basis) {
        lvGroebnerBasis *basis =
            groebner_internal_compute(ring, ideal->generators, ideal->generator_count, GROEBNER_BUCHBERGER);
        if (!basis) {
            return false;
        }
        /* 释放旧缓存 */
        ideal_clear_cached_basis(ideal);
        ideal->cached_basis = basis;
        ideal->basis_valid = true;
    }

    /* 用 Groebner 基约化：余式为零则属于理想 */
    lvPolynomial *nf =
        poly_internal_reduce(poly, ideal->cached_basis->basis_polys, ideal->cached_basis->bases_count, ring);
    if (!nf) {
        return false;
    }

    result = poly_internal_is_zero(nf);
    poly_internal_destroy(nf);

    return result;
}

/**
 * @brief 将原环多项式嵌入辅助环（原 n 个变量 + 新变量 t）
 *
 * 指数数组维度从 n 变为 n+1：前 n 个分量原样拷贝，t（位于索引 n）指数置 0。
 * 返回新建的辅助环多项式，调用者负责释放。
 */
static lvPolynomial *poly_embed_aux(const lvPolynomialRing *aux_ring, const lvPolynomialRing *orig_ring,
                                    const lvPolynomial *p) {
    if (!aux_ring || !orig_ring || !p) {
        return NULL;
    }
    int vc_orig = orig_ring->var_count;
    int vc_aux = aux_ring->var_count;
    if (vc_aux != vc_orig + 1) {
        return NULL;
    }

    int cap = p->term_count > 0 ? p->term_count : 1;
    lvPolynomial *ep = poly_internal_create(aux_ring, cap, NULL);
    if (!ep) {
        return NULL;
    }
    if (!poly_ensure_capacity_ex(ep, cap, vc_aux)) {
        poly_internal_destroy(ep);
        return NULL;
    }

    ep->term_count = p->term_count;
    for (int i = 0; i < p->term_count; i++) {
        for (int k = 0; k < vc_orig; k++) {
            ep->powers[i * vc_aux + k] = p->powers[i * vc_orig + k];
        }
        ep->powers[i * vc_aux + vc_orig] = 0; /* 新变量 t 的指数为 0 */
        ((double *) ep->coeffs)[i] = ((double *) p->coeffs)[i];
    }
    ep->total_degree = poly_internal_total_degree(ep, vc_aux);
    ep->is_homogeneous = p->is_homogeneous;
    return ep;
}

/**
 * @brief 构造单项式多项式：coeff * (x_0^{p[0]} ... x_{vc-1}^{p[vc-1]})
 */
static lvPolynomial *poly_make_monomial(const lvPolynomialRing *ring, const int *powers, double coeff) {
    if (!ring || !powers) {
        return NULL;
    }
    int vc = ring->var_count;
    lvPolynomial *m = poly_internal_create(ring, 1, NULL);
    if (!m) {
        return NULL;
    }
    lv_free((void **) &m->powers);
    lv_free((void **) &m->coeffs);
    m->powers = (int *) lv_calloc((size_t) vc, sizeof(int));
    m->coeffs = (double *) lv_calloc(1, sizeof(double));
    if (!m->powers || !m->coeffs) {
        poly_internal_destroy(m);
        return NULL;
    }
    m->term_capacity = 1;
    m->term_count = 1;
    mono_copy(m->powers, powers, vc);
    ((double *) m->coeffs)[0] = coeff;
    m->total_degree = poly_internal_total_degree(m, vc);
    m->is_homogeneous = true;
    return m;
}

/**
 * @brief 多项式除法：返回商 q，满足 poly = q * divisor
 *
 * 反复用首项做单项式除法：取 LT(divisor) 除 LT(remainder) 得到商项，
 * 累加进商并从余式中减去商项与 divisor 的乘积，直至余式为零。
 * 仅当 divisor 能整除 poly 时保证精确（除数为零或不可整除时返回 NULL）。
 * 商多项式所有权转移给调用者。
 */
static lvPolynomial *poly_divide_by(const lvPolynomial *poly, const lvPolynomial *divisor,
                                    const lvPolynomialRing *ring) {
    if (!poly || !divisor || !ring) {
        return NULL;
    }
    if (poly_internal_is_zero(divisor)) {
        return NULL; /* 除数为零：除法无定义 */
    }
    if (poly_internal_is_zero(poly)) {
        return poly_internal_create(ring, 1, NULL); /* 零多项式除以非零除数得零 */
    }

    int vc = ring->var_count;
    lvPolynomial *remainder = poly_internal_copy(poly, ring);
    if (!remainder) {
        return NULL;
    }
    lvPolynomial *quotient = poly_internal_create(ring, poly->term_count, NULL);
    if (!quotient) {
        poly_internal_destroy(remainder);
        return NULL;
    }

    /* 迭代上限：商的项数可能超过约化步数配置，放宽到 20 万 */
    int max_steps = lv_config_get_int(LV_CFG_GROEBNER_REDUCE_MAX_STEPS, GROEBNER_REDUCE_MAX_STEPS_DEFAULT);
    if (max_steps < 200000) {
        max_steps = 200000;
    }
    int step = 0;

    while (!poly_internal_is_zero(remainder)) {
        if (step >= max_steps) {
            /* 超出迭代上限：视为失败，避免返回不完整的商 */
            poly_internal_destroy(remainder);
            poly_internal_destroy(quotient);
            return NULL;
        }
        step++;

        int *lt_r = (int *) lv_calloc((size_t) vc, sizeof(int));
        int *lt_g = (int *) lv_calloc((size_t) vc, sizeof(int));
        if (!lt_r || !lt_g) {
            lv_free((void **) &lt_r);
            lv_free((void **) &lt_g);
            poly_internal_destroy(remainder);
            poly_internal_destroy(quotient);
            return NULL;
        }
        double lc_r = 0.0, lc_g = 0.0;
        if (poly_leading_term(remainder, ring, lt_r, &lc_r) != 0 ||
            poly_leading_term(divisor, ring, lt_g, &lc_g) != 0) {
            lv_free((void **) &lt_r);
            lv_free((void **) &lt_g);
            poly_internal_destroy(remainder);
            poly_internal_destroy(quotient);
            return NULL;
        }
        if (!mono_divides(ring, lt_r, lt_g)) {
            /* 理论保证 I∩⟨g⟩ ⊆ ⟨g⟩，此处不可整除说明数值异常，按失败处理 */
            lv_free((void **) &lt_r);
            lv_free((void **) &lt_g);
            poly_internal_destroy(remainder);
            poly_internal_destroy(quotient);
            return NULL;
        }

        int *q_mono = (int *) lv_calloc((size_t) vc, sizeof(int));
        if (!q_mono) {
            lv_free((void **) &lt_r);
            lv_free((void **) &lt_g);
            poly_internal_destroy(remainder);
            poly_internal_destroy(quotient);
            return NULL;
        }
        mono_divide(ring, lt_r, lt_g, q_mono);
        lv_free((void **) &lt_r);
        lv_free((void **) &lt_g);

        double factor = lc_r / lc_g;
        lvPolynomial *term = poly_make_monomial(ring, q_mono, factor);
        lv_free((void **) &q_mono);
        if (!term) {
            poly_internal_destroy(remainder);
            poly_internal_destroy(quotient);
            return NULL;
        }

        /* 商累加：quotient += term */
        lvPolynomial *new_q = poly_internal_add(quotient, term, ring);
        poly_internal_destroy(quotient);
        if (!new_q) {
            poly_internal_destroy(term);
            poly_internal_destroy(remainder);
            return NULL;
        }
        quotient = new_q;

        /* 余式 -= term * divisor */
        lvPolynomial *sub = poly_internal_multiply(term, divisor, ring);
        poly_internal_destroy(term);
        if (!sub) {
            poly_internal_destroy(remainder);
            poly_internal_destroy(quotient);
            return NULL;
        }
        poly_internal_scale(sub, -1.0);
        lvPolynomial *new_r = poly_internal_add(remainder, sub, ring);
        poly_internal_destroy(remainder);
        poly_internal_destroy(sub);
        if (!new_r) {
            poly_internal_destroy(quotient);
            return NULL;
        }
        remainder = new_r;
    }

    poly_internal_destroy(remainder);
    return quotient;
}

/**
 * @brief 用消去算法计算两个理想交集 I ∩ J 的生成元
 *
 * 标准做法：构造辅助环 K[x_0,...,x_{n-1},t]（t 位于索引 n），
 * 生成元取 { t·f : f ∈ I } ∪ { (1-t)·g : g ∈ J }，在该环上计算
 * Gröbner 基，再取出所有不含 t 的多项式，即得 I ∩ J 的生成元。
 *
 * 辅助环为局部分配、不注册进注册表，计算完成后就地释放。
 *
 * @param out_count 输出生成元个数；等于 0 表示交集为零理想（返回 NULL）；
 *                  小于 0 表示计算失败。
 * @return 原环多项式的数组（所有权转移给调用者），失败或零理想时返回 NULL。
 */
static lvPolynomial **ideal_intersection_extract(const lvPolynomialRing *orig_ring, lvPolynomial **gens_a, int count_a,
                                                 lvPolynomial **gens_b, int count_b, int *out_count) {
    if (!orig_ring || !out_count) {
        return NULL;
    }
    *out_count = -1; /* 先标记为失败，成功路径再覆盖 */

    int vc_orig = orig_ring->var_count;
    if (vc_orig < 1) {
        return NULL;
    }
    int vc_aux = vc_orig + 1;

    /* 统计有效生成元个数（跳过空指针与零多项式） */
    int gen_cap = 0;
    for (int i = 0; i < count_a; i++) {
        if (gens_a && gens_a[i] && !poly_internal_is_zero(gens_a[i])) {
            gen_cap++;
        }
    }
    for (int i = 0; i < count_b; i++) {
        if (gens_b && gens_b[i] && !poly_internal_is_zero(gens_b[i])) {
            gen_cap++;
        }
    }

    if (gen_cap == 0) {
        /* 两理想均为零理想，交集为零理想 */
        *out_count = 0;
        return NULL;
    }

    /* 本地辅助环：变量 [x_0,...,x_{n-1}, t]，t 位于索引 n。
     * 采用 MONOMIAL_ELIM 消去序，消去变量组为 {t}：含 t 的单项式恒大于
     * 不含 t 的单项式，满足消去定理对单项式序的要求。 */
    lvPolynomialRing *aux_ring = (lvPolynomialRing *) lv_calloc(1, sizeof(lvPolynomialRing));
    lvPolynomial **aux_gens = NULL;
    lvPolynomial *t_poly = NULL;
    lvGroebnerBasis *basis = NULL;
    lvPolynomial **result = NULL;
    int aux_idx = 0;
    int res_cap = 0;
    int res_count = 0;

    if (!aux_ring) {
        return NULL;
    }
    aux_ring->ring_id = orig_ring->ring_id;
    aux_ring->var_count = vc_aux;
    aux_ring->field = orig_ring->field;
    aux_ring->order = MONOMIAL_ELIM;
    aux_ring->elim_vars = (int *) lv_calloc(1, sizeof(int));
    if (!aux_ring->elim_vars) {
        lv_free((void **) &aux_ring);
        return NULL;
    }
    aux_ring->elim_vars[0] = vc_orig; /* 消去变量：t */
    aux_ring->elim_var_count = 1;
    aux_ring->is_commutative = true;
    /* var_names / weights / label 留空即可，计算路径不使用 */

    /* 注册作用域守卫：任一失败分支直接 return NULL，辅助环资源在函数出口自动释放；
     * 成功时 result 移交调用方（置 NULL 解除守卫） */
    GroebnerRingGuard ring_guard = {aux_ring};
    lv_DEFER(groebner_ring_guard_cleanup, &ring_guard);
    GroebnerPolyArrayGuard aux_gens_guard = {&aux_gens, &aux_idx};
    lv_DEFER(groebner_poly_array_guard_cleanup, &aux_gens_guard);
    lv_DEFER(groebner_poly_defer_destroy, &t_poly);
    lv_DEFER(groebner_basis_defer_destroy, &basis);
    GroebnerPolyArrayGuard result_guard = {&result, &res_count};
    lv_DEFER(groebner_poly_array_guard_cleanup, &result_guard);

    aux_gens = (lvPolynomial **) lv_calloc((size_t) gen_cap, sizeof(lvPolynomial *));
    if (!aux_gens) {
        return NULL;
    }

    /* 单项式 t = x_{vc_orig}^1 */
    {
        int *t_powers = (int *) lv_calloc((size_t) vc_aux, sizeof(int));
        if (!t_powers) {
            return NULL;
        }
        t_powers[vc_orig] = 1;
        t_poly = poly_make_monomial(aux_ring, t_powers, 1.0);
        lv_free((void **) &t_powers);
        if (!t_poly) {
            return NULL;
        }
    }

    /* t·f, f ∈ I */
    for (int i = 0; i < count_a; i++) {
        if (!gens_a || !gens_a[i] || poly_internal_is_zero(gens_a[i])) {
            continue;
        }
        lvPolynomial *ef = poly_embed_aux(aux_ring, orig_ring, gens_a[i]);
        if (!ef) {
            return NULL;
        }
        lvPolynomial *tf = poly_internal_multiply(t_poly, ef, aux_ring);
        poly_internal_destroy(ef);
        if (!tf) {
            return NULL;
        }
        aux_gens[aux_idx++] = tf;
    }

    /* (1-t)·g = g - t·g, g ∈ J */
    for (int i = 0; i < count_b; i++) {
        if (!gens_b || !gens_b[i] || poly_internal_is_zero(gens_b[i])) {
            continue;
        }
        lvPolynomial *eg = poly_embed_aux(aux_ring, orig_ring, gens_b[i]);
        if (!eg) {
            return NULL;
        }
        lvPolynomial *tg = poly_internal_multiply(t_poly, eg, aux_ring);
        if (!tg) {
            poly_internal_destroy(eg);
            return NULL;
        }
        poly_internal_scale(tg, -1.0);
        lvPolynomial *one_minus_t_g = poly_internal_add(eg, tg, aux_ring);
        poly_internal_destroy(eg);
        poly_internal_destroy(tg);
        if (!one_minus_t_g) {
            return NULL;
        }
        aux_gens[aux_idx++] = one_minus_t_g;
    }

    /* 在辅助环上计算 tI + (1-t)J 的 Gröbner 基 */
    basis = groebner_internal_compute(aux_ring, aux_gens, aux_idx, GROEBNER_BUCHBERGER);
    if (!basis) {
        return NULL;
    }

    /* 从基中取出不含 t 的多项式并转换回原环 */
    for (int i = 0; i < basis->bases_count; i++) {
        lvPolynomial *p = basis->basis_polys[i];
        if (!p || poly_internal_is_zero(p)) {
            continue;
        }

        /* 检查所有项的 t 指数（位置 vc_orig）是否均为 0 */
        bool t_free = true;
        for (int j = 0; j < p->term_count; j++) {
            if (p->powers[j * vc_aux + vc_orig] != 0) {
                t_free = false;
                break;
            }
        }
        if (!t_free) {
            continue;
        }

        /* 转换：指数截取前 vc_orig 维，系数不变 */
        lvPolynomial *ep = poly_internal_create(orig_ring, p->term_count, NULL);
        if (!ep) {
            return NULL;
        }
        if (!poly_ensure_capacity_ex(ep, p->term_count, vc_orig)) {
            poly_internal_destroy(ep);
            return NULL;
        }
        ep->term_count = p->term_count;
        for (int j = 0; j < p->term_count; j++) {
            for (int k = 0; k < vc_orig; k++) {
                ep->powers[j * vc_orig + k] = p->powers[j * vc_aux + k];
            }
            ((double *) ep->coeffs)[j] = ((double *) p->coeffs)[j];
        }
        ep->total_degree = poly_internal_total_degree(ep, vc_orig);
        ep->is_homogeneous = p->is_homogeneous;

        if (!lv_ensure_capacity((void **) &result, res_count, &res_cap, sizeof(lvPolynomial *), 0)) {
            poly_internal_destroy(ep);
            return NULL;
        }
        result[res_count++] = ep;
    }

    /* 成功：辅助环资源已由守卫在函数出口释放，结果所有权转移给调用者 */
    *out_count = res_count;
    lvPolynomial **out_result = result;
    result = NULL; /* 守卫解除：结果移交调用方 */
    return out_result;
}

/**
 * @brief 理想交 I ∩ J
 *
 * 标准消去算法：I ∩ J = (t·I + (1-t)·J) ∩ K[x]，t 为新变量。
 */
int ideal_intersection(lvRingRegistry *registry, int ideal_id_a, int ideal_id_b) {
    /* exempt: 单指针 NULL 守卫（registry 非空），与 id 范围守卫不同构，保留 */
    if (!registry)
        return -1;

    lvLockGuard _lg;
    groebner_lock_guard_init(&_lg);
    lv_DEFER(groebner_lock_guard_defer_cleanup, &_lg);
    /* exempt: 单指针 NULL 守卫（g_data 非空），与 id 范围守卫不同构，保留 */
    if (!g_data) {
        return -1;
    }
    if (!lv_index_in_range(ideal_id_a, g_data->ideal_count) ||
        !lv_index_in_range(ideal_id_b, g_data->ideal_count)) {
        return -1;
    }

    lvIdeal *ia = g_data->ideals[ideal_id_a];
    lvIdeal *ib = g_data->ideals[ideal_id_b];
    if (!ia || !ib) {
        return -1;
    }
    if (ia->ring_id != ib->ring_id) {
        return -1;
    }

    lvPolynomialRing *ring = registry->rings[ia->ring_id];
    if (!ring) {
        return -1;
    }

    /* 消去算法提取交集生成元（inter_count < 0 失败；== 0 表示零理想） */
    int inter_count = 0;
    lvPolynomial **inter_gens = NULL;
    /* 守卫：inter_gens 数组在函数出口自动释放（元素按池归属情况手动处理） */
    lv_DEFER(lv_defer_free_ptr, &inter_gens);
    inter_gens =
        ideal_intersection_extract(ring, ia->generators, ia->generator_count, ib->generators, ib->generator_count,
                                   &inter_count);
    if (inter_count < 0) {
        return -1;
    }

    /* 创建结果理想（已持有锁，直接分配） */
    lvIdeal *result_ideal =
        ideal_alloc_locked(ia->ring_id, inter_count > 0 ? inter_count : GROEBNER_IDEAL_INIT_GEN_CAPACITY, NULL);
    GroebnerIdealGuard ideal_guard = {result_ideal};
    lv_DEFER(groebner_ideal_guard_cleanup, &ideal_guard);
    if (!result_ideal) {
        /* 未入池的生成元元素释放后由守卫释放数组 */
        for (int k = 0; k < inter_count; k++) {
            poly_internal_destroy(inter_gens[k]);
        }
        return -1;
    }

    /* 生成元注册进全局多项式池（遵循理想持有池内指针的约定） */
    for (int k = 0; k < inter_count; k++) {
        if (poly_internal_store(g_data, inter_gens[k]) < 0) {
            /* 前 k 个已入池（归池所有），剩余元素在此释放，数组与理想结构由守卫释放 */
            for (int j = k; j < inter_count; j++) {
                poly_internal_destroy(inter_gens[j]);
            }
            return -1;
        }
        result_ideal->generators[result_ideal->generator_count++] = inter_gens[k];
    }

    int ret = ideal_internal_store(g_data, result_ideal);
    if (ret >= 0) {
        ideal_guard.ideal = NULL; /* 注册成功，理想归注册表所有 */
    }
    /* 注册失败时理想结构由守卫释放（已入池的生成元由池持有并负责释放） */
    return ret;
}

/**
 * @brief 理想商 I : J
 *
 * 逐生成元计算商理想后两两求交：
 *   I : J = ∩_{g ∈ gens(J)} (I : ⟨g⟩)，其中 I : ⟨g⟩ = (I ∩ ⟨g⟩)/g
 */
int ideal_quotient(lvRingRegistry *registry, int ideal_id_a, int ideal_id_b, const char *result_label) {
    /* exempt: 单指针 NULL 守卫（registry 非空），与 id 范围守卫不同构，保留 */
    if (!registry)
        return -1;

    lvLockGuard _lg;
    groebner_lock_guard_init(&_lg);
    lv_DEFER(groebner_lock_guard_defer_cleanup, &_lg);
    /* exempt: 单指针 NULL 守卫（g_data 非空），与 id 范围守卫不同构，保留 */
    if (!g_data) {
        return -1;
    }
    if (!lv_index_in_range(ideal_id_a, g_data->ideal_count) ||
        !lv_index_in_range(ideal_id_b, g_data->ideal_count)) {
        return -1;
    }

    lvIdeal *ia = g_data->ideals[ideal_id_a];
    lvIdeal *ib = g_data->ideals[ideal_id_b];
    if (!ia || !ib) {
        return -1;
    }
    if (ia->ring_id != ib->ring_id) {
        return -1;
    }

    lvPolynomialRing *ring = registry->rings[ia->ring_id];
    if (!ring) {
        return -1;
    }

    /* 当前求交结果的生成元（尚未入池，由本函数持有，成功时转移给结果理想） */
    lvPolynomial **cur_gens = NULL;
    int cur_count = 0;
    bool first = true; /* 尚未处理过任何非零生成元 */

    /* 守卫：失败路径在函数出口统一释放 cur_gens（元素 [0..cur_count) + 数组）；
     * 元素入池后按池归属手动处理并置空解除守卫 */
    GroebnerPolyArrayGuard cur_guard = {&cur_gens, &cur_count};
    lv_DEFER(groebner_poly_array_guard_cleanup, &cur_guard);

    for (int gi = 0; gi < ib->generator_count; gi++) {
        lvPolynomial *g = ib->generators[gi];
        if (!g || poly_internal_is_zero(g)) {
            /* I : ⟨0⟩ = R（整环），与其它商理想求交时不改变结果 */
            continue;
        }

        /* 计算 I ∩ ⟨g⟩ */
        int inter_count = 0;
        lvPolynomial **inter_gens =
            ideal_intersection_extract(ring, ia->generators, ia->generator_count, &g, 1, &inter_count);
        if (inter_count < 0) {
            return -1;
        }

        /* 每个 h ∈ I∩⟨g⟩ 都满足 h = q·g（I∩⟨g⟩ ⊆ ⟨g⟩），作除法得 q ∈ I : ⟨g⟩ */
        lvPolynomial **q_gens =
            inter_count > 0 ? (lvPolynomial **) lv_calloc((size_t) inter_count, sizeof(lvPolynomial *)) : NULL;
        int q_count = 0;
        if (inter_count > 0 && !q_gens) {
            for (int k = 0; k < inter_count; k++) {
                poly_internal_destroy(inter_gens[k]);
            }
            lv_free((void **) &inter_gens);
            return -1;
        }

        for (int hi = 0; hi < inter_count; hi++) {
            lvPolynomial *q = poly_divide_by(inter_gens[hi], g, ring);
            if (!q) {
                for (int k = 0; k < q_count; k++) {
                    poly_internal_destroy(q_gens[k]);
                }
                lv_free((void **) &q_gens);
                for (int k = 0; k < inter_count; k++) {
                    poly_internal_destroy(inter_gens[k]);
                }
                lv_free((void **) &inter_gens);
                return -1;
            }
            if (poly_internal_is_zero(q)) {
                poly_internal_destroy(q);
            } else {
                q_gens[q_count++] = q;
            }
        }
        for (int k = 0; k < inter_count; k++) {
            poly_internal_destroy(inter_gens[k]);
        }
        lv_free((void **) &inter_gens);

        /* I : J = ∩_g (I : ⟨g⟩)：将本次商理想与累计结果求交 */
        lvPolynomial **new_gens = NULL;
        int new_count = 0;
        if (first) {
            new_gens = q_gens; /* 第一个非零生成元：商理想即当前结果 */
            new_count = q_count;
            q_gens = NULL;
        } else {
            new_gens = ideal_intersection_extract(ring, cur_gens, cur_count, q_gens, q_count, &new_count);
            for (int k = 0; k < q_count; k++) {
                poly_internal_destroy(q_gens[k]);
            }
            lv_free((void **) &q_gens);
            if (new_count < 0) {
                return -1;
            }
        }

        if (cur_gens) {
            for (int k = 0; k < cur_count; k++) {
                poly_internal_destroy(cur_gens[k]);
            }
            lv_free((void **) &cur_gens);
        }
        cur_gens = new_gens;
        cur_count = new_count;
        first = false;
    }

    /* J 的生成元全为零（或 J 为零理想）时，I : J = R = ⟨1⟩ */
    if (first) {
        lvPolynomial *one = poly_internal_create(ring, 1, NULL);
        if (!one) {
            return -1;
        }
        one->term_count = 1;
        ((double *) one->coeffs)[0] = 1.0;
        one->total_degree = 0;
        cur_gens = (lvPolynomial **) lv_calloc(1, sizeof(lvPolynomial *));
        if (!cur_gens) {
            poly_internal_destroy(one);
            return -1;
        }
        cur_gens[0] = one;
        cur_count = 1;
    }

    /* 创建结果理想（已持有锁，直接分配） */
    lvIdeal *result_ideal =
        ideal_alloc_locked(ia->ring_id, cur_count > 0 ? cur_count : GROEBNER_IDEAL_INIT_GEN_CAPACITY, result_label);
    GroebnerIdealGuard ideal_guard = {result_ideal};
    lv_DEFER(groebner_ideal_guard_cleanup, &ideal_guard);
    if (!result_ideal) {
        return -1; /* cur_gens 由守卫释放 */
    }

    /* 生成元注册进全局多项式池（遵循理想持有池内指针的约定） */
    for (int k = 0; k < cur_count; k++) {
        if (poly_internal_store(g_data, cur_gens[k]) < 0) {
            /* 前 k 个已入池（归池所有），剩余元素在此释放，数组与理想结构由守卫释放 */
            for (int j = k; j < cur_count; j++) {
                poly_internal_destroy(cur_gens[j]);
            }
            lv_free((void **) &cur_gens); /* 置空以解除守卫（数组由池外元素处理完成） */
            return -1;
        }
        result_ideal->generators[result_ideal->generator_count++] = cur_gens[k];
    }
    lv_free((void **) &cur_gens); /* 元素已全部入池，仅释放数组并解除守卫 */

    int ret = ideal_internal_store(g_data, result_ideal);
    if (ret >= 0) {
        ideal_guard.ideal = NULL; /* 注册成功，理想归注册表所有 */
    }
    /* 注册失败时理想结构由守卫释放（已入池的生成元由池持有并负责释放） */
    return ret;
}

