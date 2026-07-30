/**
 * @file lv_mempool_utils.c
 * @brief 内存池静态单例工具实现
 *
 * @author Lv-00 Project
 */

#include "lv/lv_mempool_utils.h"

lvMemPool *lv_mempool_static_init(lvMemPool **pool, size_t block_size, int initial_count) {
    if (!pool)
        return NULL;
    if (!*pool) {
        *pool = lv_mempool_create(block_size, initial_count);
    }
    return *pool;
}

void lv_mempool_static_destroy(lvMemPool **pool) {
    if (pool && *pool) {
        lv_mempool_destroy(*pool);
        *pool = NULL;
    }
}
