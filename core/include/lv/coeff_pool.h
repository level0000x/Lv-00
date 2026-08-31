/**
 * @file coeff_pool.h
 * @brief 多项式系数内存池共享实现（coeff_pool_alloc / coeff_pool_clear）
 *
 * @details 集中管理多项式系数数组的池化分配/释放逻辑，作为单一事实来源，
 *          消除 symbolic_coord_ops.c / symbolic_coord_transform.c /
 *          solver_coord_extract.c 中的三份重复实现（原错误信息与回退逻辑一致）。
 *
 *          池的拥有权集中在 symbolic_coord_ops.c（定义 g_coeff_pool），
 *          其他文件通过 extern 访问共享池；solver_coord_extract.c
 *          不再维护私有池。
 *
 * @author Lv-00 Project
 */

#ifndef LV_COEFF_POOL_H
#define LV_COEFF_POOL_H

#include "lv/lv_mempool_utils.h"
#include "lv/mpz_poly.h"
#include "lv/error_codes.h"
#include "lv/lv_utils.h"
#include "lv/lv_thread.h" /* lv_once / lv_once_t（F43/K15 g_coeff_pool TOCTOU 修复） */

#ifdef __cplusplus
extern "C" {
#endif

/* 多项式系数内存池：拥有权在 symbolic_coord_ops.c，其余文件共享 */
extern lvMemPool *g_coeff_pool;

/* 系数池参数 */
#define COEFF_POOL_BLOCK_SIZE (sizeof(mpz_t) * 8)
#define COEFF_POOL_INITIAL_COUNT 256

/* F43/K15：g_coeff_pool 惰性创建 TOCTOU 修复 —— 一次性初始化守卫
 * （原 lv_mempool_static_init 的 !*pool 检查 + create 非原子，
 * 并发首调双建泄漏 + 短暂撕裂；lv_once 保证恰一次 + happens-before） */
static lv_once_t s_coeff_pool_once = lv_ONCE_INIT;

/** @brief 创建系数池（lv_once 回调，恰执行一次） */
static inline void coeff_pool_create_once(void) {
    g_coeff_pool = lv_mempool_create(COEFF_POOL_BLOCK_SIZE, COEFF_POOL_INITIAL_COUNT);
}

/**
 * @brief 从内存池分配多项式系数数组（含回退）
 *
 * 【K43 修复】池块容量为 COEFF_POOL_BLOCK_SIZE（=8 个 mpz_t）；
 * 原实现忽略 count 直接取单块，count > 8 时越界写。
 * 现在：count 不超过块容量用池；否则直接 lv_malloc（池不适用）。
 *
 * @param count 需要的 mpz_t 元素个数（必须 > 0）
 * @return mpz_t 数组指针（含 count 个元素容量），失败返回 NULL
 */
static inline mpz_t *coeff_pool_alloc(int count) {
    if (count <= 0)
        lv_RETURN_ERROR_NULL(lv_ERROR_INVALID_PARAM, "coeff_pool_alloc: invalid count");

    /* F43/K15：线程安全惰性创建（lv_once 消除 TOCTOU） */
    lv_once(&s_coeff_pool_once, coeff_pool_create_once);
    if (!g_coeff_pool)
        lv_RETURN_ERROR_NULL(lv_ERROR_NOT_INITIALIZED, "coeff_pool_alloc: coefficient pool not initialized");

    /* 池块容量固定 8 个 mpz_t：超过则绕过池直接 lv_malloc（含溢出检查） */
    if ((size_t) count > COEFF_POOL_BLOCK_SIZE / sizeof(mpz_t)) {
        if ((size_t) count > SIZE_MAX / sizeof(mpz_t))
            lv_RETURN_ERROR_NULL(lv_ERROR_OVERFLOW, "coeff_pool_alloc: allocation size overflow");
        return (mpz_t *)lv_calloc((size_t) count, sizeof(mpz_t));
    }

    mpz_t *c = (mpz_t *)lv_mempool_alloc(g_coeff_pool);
    if (!c) {
        /* 池满回退到 lv_malloc */
        c = (mpz_t *)lv_calloc((size_t) count, sizeof(mpz_t));
    }
    return c;
}

/**
 * @brief 释放多项式系数数组（兼容池分配和 lv_malloc 回退）
 *
 * 对池内指针调用 lv_mempool_free 归还；对回退指针，
 * mem_pool_free 的地址越界检查会安全跳过。
 */
static inline void coeff_pool_clear(mpz_poly_t *p) {
    if (p->coeffs) {
        for (int i = 0; i <= p->degree; i++) {
            mpz_clear(p->coeffs[i]);
        }
        lv_mempool_free(g_coeff_pool, p->coeffs);
    }
    p->coeffs = NULL;
    p->degree = -1;
}

#ifdef __cplusplus
}
#endif

#endif /* LV_COEFF_POOL_H */
