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
#include "lv/lv_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 多项式系数内存池：拥有权在 symbolic_coord_ops.c，其余文件共享 */
extern lvMemPool *g_coeff_pool;

/* 系数池参数 */
#define COEFF_POOL_BLOCK_SIZE (sizeof(mpz_t) * 8)
#define COEFF_POOL_INITIAL_COUNT 256

/**
 * @brief 从内存池分配多项式系数数组（含回退）
 * @param count 需要的 mpz_t 元素个数
 * @return mpz_t 数组指针，失败返回 NULL
 */
static inline mpz_t *coeff_pool_alloc(int count) {
    if (!lv_mempool_static_init(&g_coeff_pool, COEFF_POOL_BLOCK_SIZE, COEFF_POOL_INITIAL_COUNT))
        lv_RETURN_ERROR_NULL(lv_ERROR_NOT_INITIALIZED, "coeff_pool_alloc: coefficient pool not initialized");
    mpz_t *c = (mpz_t *)lv_mempool_alloc(g_coeff_pool);
    if (!c) {
        /* 池满回退到 lv_malloc */
        c = (mpz_t *)lv_malloc((size_t)count * sizeof(mpz_t));
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
