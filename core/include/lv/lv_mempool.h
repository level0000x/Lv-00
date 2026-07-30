/**
 * @file lv_mempool.h
 * @brief 内存池公共 API —— 基于 debug.h 中 lvMemPool 的轻量包装
 *
 * @details 提供独立于 debug.h 的内存池接口，用于高频小对象分配场景。
 *          实际实现在 debug.h / debug.c 的 mem_pool_* 函数中。
 *
 * @author Lv-00 Project
 */

#ifndef lv_MEMPOOL_H
#define lv_MEMPOOL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

/* 前向声明，实际实现在 debug.h 中 */
struct lvMemPool;
typedef struct lvMemPool lvMemPool;

/**
 * @brief 创建内存池
 * @param block_size  每个块的大小（字节）
 * @param initial_blocks 初始块数量
 * @return 内存池指针，失败返回 NULL
 */
lvMemPool *lv_mempool_create(size_t block_size, int initial_blocks);

/**
 * @brief 销毁内存池
 * @param pool 内存池指针（NULL 安全）
 */
void lv_mempool_destroy(lvMemPool *pool);

/**
 * @brief 从内存池分配一个块
 * @param pool 内存池指针
 * @return 块指针，池满时返回 NULL
 */
void *lv_mempool_alloc(lvMemPool *pool);

/**
 * @brief 将块归还内存池
 * @param pool  内存池指针
 * @param block 要释放的块指针（必须由 lv_mempool_alloc 返回）
 */
void lv_mempool_free(lvMemPool *pool, void *block);

#ifdef __cplusplus
}
#endif

#endif /* lv_MEMPOOL_H */
