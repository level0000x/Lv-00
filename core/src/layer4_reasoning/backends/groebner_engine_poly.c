/**
 * @file groebner_engine_poly.c
 * @brief 多项式 API
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
 *  第二部分：公共 API —— 多项式操作
 * ================================================================ */

/**
 * @brief 创建多项式并存入池
 */
int poly_create(lvRingRegistry *registry, int ring_id, int capacity, const char *label) {
    if (!registry || ring_id < 0 || ring_id >= registry->ring_count) {
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "poly_create: invalid params (registry=%p, ring_id=%d)",
                        (const void *)registry, ring_id);
    }

    lvRingRegistry *r = registry;
    lv_UNUSED(r);

    lvPolynomialRing *ring = registry->rings[ring_id];
    if (!ring) {
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "poly_create: ring not found (ring_id=%d)", ring_id);
    }

    lvPolynomial *poly = poly_internal_create(ring, capacity, label);
    if (!poly) {
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "poly_create: poly_internal_create failed");
    }

    int ret = -1;
    lvLockGuard _lg;
    lv_lock_guard_init(&_lg, &g_data_mutex);
    lvRegistryData *data = registry_data_ensure();
    if (!data) {
        poly_internal_destroy(poly);
        lv_set_error_ctx(lv_ERROR_INTERNAL, __FILE__, __LINE__, __func__,
                         "poly_create: registry_data_ensure failed");
        goto cleanup;
    }

    ret = poly_internal_store(data, poly);
cleanup:
    lv_lock_guard_destroy(&_lg);
    return ret;
}

/**
 * @brief 销毁多项式
 */
void poly_destroy(lvRingRegistry *registry, int poly_id) {
    lv_UNUSED(registry);
    lvLockGuard _lg;
    lv_lock_guard_init(&_lg, &g_data_mutex);
    if (!g_data || poly_id < 0 || poly_id >= g_data->poly_count)
        goto cleanup;

    if (g_data->polys[poly_id]) {
        poly_internal_destroy(g_data->polys[poly_id]);
        g_data->polys[poly_id] = NULL;
    }
cleanup:
    lv_lock_guard_destroy(&_lg);
    return;
}

/**
 * @brief 多项式加法
 */
int poly_add(lvRingRegistry *registry, int poly_id_f, int poly_id_g, const char *result_label) {
    if (!registry)
        return -1;

    int ret = -1;
    lvLockGuard _lg;
    lv_lock_guard_init(&_lg, &g_data_mutex);
    if (!g_data)
        goto cleanup;
    if (poly_id_f < 0 || poly_id_g < 0)
        goto cleanup;
    if (poly_id_f >= g_data->poly_count || poly_id_g >= g_data->poly_count)
        goto cleanup;

    lvPolynomial *f = g_data->polys[poly_id_f];
    lvPolynomial *g = g_data->polys[poly_id_g];
    if (!f || !g)
        goto cleanup;

    if (f->ring_id != g->ring_id)
        goto cleanup;
    lvPolynomialRing *ring = registry->rings[f->ring_id];
    if (!ring)
        goto cleanup;

    lvPolynomial *result = poly_internal_add(f, g, ring);
    if (!result)
        goto cleanup;

    lv_free((void **) &result->label);
    result->label = groebner_strdup_safe(result_label);

    ret = poly_internal_store(g_data, result);
cleanup:
    lv_lock_guard_destroy(&_lg);
    return ret;
}

/**
 * @brief 多项式乘法
 */
int poly_multiply(lvRingRegistry *registry, int poly_id_f, int poly_id_g, const char *result_label) {
    if (!registry)
        return -1;

    int ret = -1;
    lvLockGuard _lg;
    lv_lock_guard_init(&_lg, &g_data_mutex);
    if (!g_data)
        goto cleanup;
    if (poly_id_f < 0 || poly_id_g < 0)
        goto cleanup;
    if (poly_id_f >= g_data->poly_count || poly_id_g >= g_data->poly_count)
        goto cleanup;

    lvPolynomial *f = g_data->polys[poly_id_f];
    lvPolynomial *g = g_data->polys[poly_id_g];
    if (!f || !g)
        goto cleanup;

    if (f->ring_id != g->ring_id)
        goto cleanup;
    lvPolynomialRing *ring = registry->rings[f->ring_id];
    if (!ring)
        goto cleanup;

    lvPolynomial *result = poly_internal_multiply(f, g, ring);
    if (!result)
        goto cleanup;

    lv_free((void **) &result->label);
    result->label = groebner_strdup_safe(result_label);

    ret = poly_internal_store(g_data, result);
cleanup:
    lv_lock_guard_destroy(&_lg);
    return ret;
}

/**
 * @brief 多项式代入
 */
int poly_substitute(lvRingRegistry *registry, int poly_id, int var_index, int subst_poly_id, const char *result_label) {
    if (!registry)
        return -1;

    int ret = -1;
    lvLockGuard _lg;
    lv_lock_guard_init(&_lg, &g_data_mutex);
    if (!g_data)
        goto cleanup;
    if (poly_id < 0 || subst_poly_id < 0)
        goto cleanup;
    if (poly_id >= g_data->poly_count || subst_poly_id >= g_data->poly_count)
        goto cleanup;

    lvPolynomial *f = g_data->polys[poly_id];
    lvPolynomial *subst = g_data->polys[subst_poly_id];
    if (!f || !subst)
        goto cleanup;

    if (f->ring_id != subst->ring_id)
        goto cleanup;
    lvPolynomialRing *ring = registry->rings[f->ring_id];
    if (!ring)
        goto cleanup;

    lvPolynomial *result = poly_internal_substitute(f, var_index, subst, ring);
    if (!result)
        goto cleanup;

    lv_free((void **) &result->label);
    result->label = groebner_strdup_safe(result_label);

    ret = poly_internal_store(g_data, result);
cleanup:
    lv_lock_guard_destroy(&_lg);
    return ret;
}

/**
 * @brief 获取多项式实例
 */
const lvPolynomial *poly_get(const lvRingRegistry *registry, int poly_id) {
    lv_UNUSED(registry);
    const lvPolynomial *p = NULL;
    lvLockGuard _lg;
    lv_lock_guard_init(&_lg, &g_data_mutex);
    if (!g_data || poly_id < 0 || poly_id >= g_data->poly_count)
        goto cleanup;
    p = g_data->polys[poly_id];
cleanup:
    lv_lock_guard_destroy(&_lg);
    return p;
}

