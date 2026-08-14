/**
 * @file lv_mempool.c
 * @brief 内存池公共 API 实现 —— 委托到 debug.h 的 mem_pool_* 函数
 *
 * @details 轻量包装层，将 lv_mempool_* API 映射到 debug.h 中已实现的
 *          mem_pool_* 函数。避免上层模块直接包含 debug.h。
 *
 * @author Lv-00 Project
 */

#include "lv/lv_mempool.h"
#include "lv/debug.h"  /* 提供 MemPool / lvMemPool 实现 */
#include <stdlib.h>

lvMemPool *lv_mempool_create(size_t block_size, int initial_blocks) {
    return (lvMemPool *)mem_pool_create(block_size, initial_blocks);
}

void lv_mempool_destroy(lvMemPool *pool) {
    mem_pool_destroy(pool);
}

void *lv_mempool_alloc(lvMemPool *pool) {
    return mem_pool_alloc(pool);
}

void lv_mempool_free(lvMemPool *pool, void *block) {
    mem_pool_free(pool, block);
}
