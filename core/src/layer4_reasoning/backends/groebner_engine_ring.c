/**
 * @file groebner_engine_ring.c
 * @brief 环注册表 API
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
 *  第一部分：公共 API —— 环注册表管理
 * ================================================================ */

/**
 * @brief 创建环注册表
 */
lvRingRegistry *ring_registry_create(int capacity) {
    if (capacity < 1) {
        capacity = 8;
    }

    lvRingRegistry *registry = (lvRingRegistry *) lv_calloc(1, sizeof(lvRingRegistry));
    if (!registry) {
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "ring_registry_create: lv_calloc(%zu) failed", sizeof(lvRingRegistry));
    }

    registry->rings = (lvPolynomialRing **) lv_calloc((size_t) capacity, sizeof(lvPolynomialRing *));
    if (!registry->rings) {
        lv_free((void **) &registry);
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "ring_registry_create: lv_calloc for rings failed (cap=%d)", capacity);
    }
    registry->ring_capacity = capacity;
    registry->ring_count = 0;
    registry->active_ring_id = -1;
    registry->is_initialized = true;

    /* 初始化全局注册数据（加锁保护） */
    lvLockGuard _lg;
    groebner_lock_guard_init(&_lg);
    registry_data_ensure();
    lv_lock_guard_destroy(&_lg);

    return registry;
}

/**
 * @brief 销毁环注册表及所有关联对象
 */
void ring_registry_destroy(lvRingRegistry *registry) {
    if (!registry) {
        return;
    }

    /* 加锁保护全局池数据的释放 */
    {
    lvLockGuard _lg;
    groebner_lock_guard_init(&_lg);

    /* 释放全局池数据 */
    if (g_data) {
        if (g_data->polys) {
            for (int i = 0; i < g_data->poly_count; i++) {
                poly_internal_destroy(g_data->polys[i]);
            }
            lv_free((void **) &g_data->polys);
        }
        if (g_data->ideals) {
            for (int i = 0; i < g_data->ideal_count; i++) {
                if (g_data->ideals[i]) {
                    ideal_clear_cached_basis(g_data->ideals[i]);
                    lv_free((void **) &g_data->ideals[i]->label);
                    lv_free((void **) &g_data->ideals[i]);
                }
            }
            lv_free((void **) &g_data->ideals);
        }
        if (g_data->varieties) {
            for (int i = 0; i < g_data->variety_count; i++) {
                if (g_data->varieties[i]) {
                    if (g_data->varieties[i]->solution_points) {
                        for (int j = 0; j < g_data->varieties[i]->solution_count; j++) {
                            lv_free((void **) &g_data->varieties[i]->solution_points[j]);
                        }
                        lv_free((void **) &g_data->varieties[i]->solution_points);
                    }
                    lv_free((void **) &g_data->varieties[i]->label);
                    lv_free((void **) &g_data->varieties[i]);
                }
            }
            lv_free((void **) &g_data->varieties);
        }
        if (g_data->bases) {
            lv_free((void **) &g_data->bases);
        }
        lv_free((void **) &g_data);
        g_data = NULL;
    }

    lv_lock_guard_destroy(&_lg);
    }

    /* 注意：全局互斥锁 g_data_mutex 采用进程级生命周期，
     * 不在此处销毁。若销毁，后续 ring_registry_create 等调用
     * 再次加锁时将因 CRITICAL_SECTION 已被删除而崩溃。 */

    /* 释放环 */
    for (int i = 0; i < registry->ring_count; i++) {
        if (registry->rings[i]) {
            lv_free((void **) &registry->rings[i]->var_names);
            lv_free((void **) &registry->rings[i]->elim_vars);
            lv_free((void **) &registry->rings[i]->weights);
            lv_free((void **) &registry->rings[i]->label);
            lv_free((void **) &registry->rings[i]);
        }
    }
    lv_free((void **) &registry->rings);
    registry->rings = NULL;
    registry->ring_count = 0;
    registry->ring_capacity = 0;
    registry->is_initialized = false;
    lv_free((void **) &registry);
}

/**
 * @brief 创建一个多项式环
 */
int ring_create(lvRingRegistry *registry, const char *var_names[], int var_count, lvRingFieldType field,
                lvMonomialOrder order, const char *label) {
    if (!registry || !var_names || var_count < 1) {
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "ring_create: invalid params (registry=%p, var_names=%p, var_count=%d)",
                        (const void *)registry, (const void *)var_names, var_count);
    }

    if (registry->ring_count >= registry->ring_capacity) {
        if (!lv_ensure_capacity((void **) &registry->rings, registry->ring_count, &registry->ring_capacity,
                                sizeof(lvPolynomialRing *), 1)) {
            lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "ring_create: lv_realloc for rings failed (cap=%d)",
                            registry->ring_capacity);
        }
    }

    lvPolynomialRing *ring = (lvPolynomialRing *) lv_calloc(1, sizeof(lvPolynomialRing));
    if (!ring) {
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "ring_create: lv_calloc(%zu) failed", sizeof(lvPolynomialRing));
    }

    ring->var_names = (char **) lv_calloc((size_t) var_count, sizeof(char *));
    if (!ring->var_names) {
        lv_free((void **) &ring);
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "ring_create: lv_calloc for var_names failed (count=%d)", var_count);
    }
    for (int i = 0; i < var_count; i++) {
        ring->var_names[i] = groebner_strdup_safe(var_names[i]);
    }

    ring->var_count = var_count;
    ring->field = field;
    ring->order = order;
    ring->label = groebner_strdup_safe(label);
    ring->is_commutative = true;

    int ring_id = registry->ring_count;
    ring->ring_id = ring_id;
    registry->rings[registry->ring_count++] = ring;

    return ring_id;
}

/**
 * @brief 销毁一个多项式环
 */
void ring_destroy(lvRingRegistry *registry, int ring_id) {
    if (!registry || ring_id < 0 || ring_id >= registry->ring_count) {
        return;
    }

    lvPolynomialRing *ring = registry->rings[ring_id];
    if (ring) {
        if (ring->var_names) {
            for (int i = 0; i < ring->var_count; i++) {
                lv_free((void **) &ring->var_names[i]);
            }
            lv_free((void **) &ring->var_names);
        }
        lv_free((void **) &ring->elim_vars);
        lv_free((void **) &ring->weights);
        lv_free((void **) &ring->label);
        lv_free((void **) &ring);
    }

    /* 将后续环前移 */
    for (int i = ring_id; i < registry->ring_count - 1; i++) {
        registry->rings[i] = registry->rings[i + 1];
        if (registry->rings[i]) {
            registry->rings[i]->ring_id = i;
        }
    }
    registry->rings[registry->ring_count - 1] = NULL;
    registry->ring_count--;
}

/**
 * @brief 注册外部创建的环
 *
 * @note 已建未用/预留：当前全项目无业务调用者（仅头文件声明），
 *       保留供外部后端接入 groebner 引擎使用。
 */
int ring_register(lvRingRegistry *registry, lvPolynomialRing *ring) {
    if (!registry || !ring) {
        return -1;
    }

    if (registry->ring_count >= registry->ring_capacity) {
        if (!lv_ensure_capacity((void **) &registry->rings, registry->ring_count, &registry->ring_capacity,
                                sizeof(lvPolynomialRing *), 1)) {
            return -1;
        }
    }

    int ring_id = registry->ring_count;
    ring->ring_id = ring_id;
    registry->rings[registry->ring_count++] = ring;
    return ring_id;
}

/**
 * @brief 按 ID 查找环
 */
lvPolynomialRing *ring_find(const lvRingRegistry *registry, int ring_id) {
    if (!registry || ring_id < 0 || ring_id >= registry->ring_count) {
        return NULL;
    }
    return registry->rings[ring_id];
}

