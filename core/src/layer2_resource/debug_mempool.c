/**
 * @file debug_mempool.c
 * @brief memory pool implementation
 * @details Split from debug.c
 */

#include "lv/lv_file.h"
#include "lv/lv_platform.h"
#include "lv/lv_thread.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "lv/engine.h"
#include "lv/lv_json.h"
#include "lv/lv_lifecycle.h"

#include "context.h"
#include "debug.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "stream.h"
#include "stream_context_util.h"
#include "type_system.h"
#include "lv/lv_xmacro.h"
#include "lv/lv_strbuf.h"
#include "debug_internal.h"

/* ================================================================== */
/*  内存池实现                                                         */
/* ================================================================== */

struct lvMemPool {
    uint8_t *blocks;   /* 连续内存块数组 */
    int *free_list;    /* 空闲块索引栈 */
    int free_count;    /* 空闲块数量 */
    int total_count;   /* 总块数量 */
    size_t block_size; /* 每个块的大小 */
    uint8_t *used;     /* 使用标志位数组，防止双重释放 */
};

/* 向后兼容别名 */
typedef struct lvMemPool lvMemPool;
#define MemPool lvMemPool

/* MemPool 部分构建守卫：任一成员分配失败时统一释放已分配成员与外壳，
 * 替代递增回滚样板 */
typedef struct {
    MemPool *pool;
} MemPoolGuard;

static void mem_pool_guard_cleanup(void *p) {
    MemPoolGuard *g = (MemPoolGuard *) p;
    if (g->pool) {
        lv_free((void **) &g->pool->blocks);
        lv_free((void **) &g->pool->free_list);
        lv_free((void **) &g->pool->used);
        lv_free((void **) &g->pool);
    }
}

/**
 * @brief 创建固定块大小的内存池
 * @param block_size      每个内存块的大小（字节），必须大于 0
 * @param initial_blocks  初始块数量，必须大于 0
 * @return 新创建的内存池指针，参数无效或内存分配失败时返回 NULL
 * @note 调用者在使用完毕后需调用 mem_pool_destroy() 释放资源
 */
MemPool *mem_pool_create(size_t block_size, int initial_blocks) {
    if (block_size == 0 || initial_blocks <= 0)
        lv_RETURN_ERROR_NULL(lv_ERROR_INVALID_PARAM, "内存池参数无效");

    MemPool *pool = (MemPool *) lv_calloc(1, sizeof(MemPool));
    if (!pool)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "分配 MemPool 失败");

    /* 部分构建守卫：后续任一分配失败自动释放已分配成员；成功路径 guard.pool = NULL 解除 */
    MemPoolGuard guard = {pool};
    lv_DEFER(mem_pool_guard_cleanup, &guard);

    pool->block_size = block_size;
    pool->total_count = initial_blocks;
    pool->free_count = initial_blocks;

    /* 分配连续内存块数组 */
    pool->blocks = (uint8_t *) lv_calloc((size_t) initial_blocks, block_size);
    if (!pool->blocks)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "分配内存池块失败");

    /* 分配空闲块索引栈 */
    pool->free_list = (int *) lv_calloc((size_t) initial_blocks, sizeof(int));
    if (!pool->free_list)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "分配空闲列表失败");

    /* 初始化空闲列表：所有块初始都是空闲的 */
    for (int i = 0; i < initial_blocks; i++) {
        pool->free_list[i] = i;
    }

    /* 分配使用标志位数组 */
    pool->used = (uint8_t *) lv_calloc((size_t) initial_blocks, sizeof(uint8_t));
    if (!pool->used)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "分配使用标志位失败");

    guard.pool = NULL; /* 守卫解除：结果移交调用方 */
    return pool;
}

/**
 * @brief 从内存池中分配一个内存块
 * @param pool 内存池指针
 * @return 分配到的内存块指针，池为空、空闲块耗尽或索引越界时返回 NULL
 * @note 返回的内存块大小由 mem_pool_create() 的 block_size 参数决定
 */
void *mem_pool_alloc(MemPool *pool) {
    if (!pool || pool->free_count <= 0) {
        if (debug_stream_ctx && pool && pool->free_count <= 0) {
            stream_emit_warning(debug_stream_ctx, "内存池分配失败：空闲块已耗尽", 0);
        }
        lv_RETURN_ERROR_NULL(lv_ERROR_INVALID_STATE, "内存池无效或已耗尽");
    }

    pool->free_count--;
    int idx = pool->free_list[pool->free_count];

    /* 溢出检查：确保从空闲列表中取出的索引在有效范围内。
     * 如果空闲列表被损坏（例如内存越界写入），idx 可能超出 total_count，
     * 此时拒绝分配以防止越界访问。 */
    if (idx < 0 || idx >= pool->total_count) {
        pool->free_count++; /* 恢复空闲计数 */
        if (debug_stream_ctx) {
            stream_emit_warning(debug_stream_ctx, "内存池分配失败：空闲列表索引越界", 0);
        }
        lv_RETURN_ERROR_NULL(lv_ERROR_INDEX_OUT_OF_RANGE, "空闲列表索引越界");
    }

    pool->used[idx] = 1; /* 标记为已使用 */
    return (void *) (pool->blocks + (size_t) idx * pool->block_size);
}

/**
 * @brief 将内存块释放回内存池
 * @param pool  内存池指针
 * @param block 要释放的内存块指针，必须由 mem_pool_alloc() 返回
 * @note 如果传入非本池分配的地址或已释放的块，函数将安全返回（不执行任何操作）
 */
void mem_pool_free(MemPool *pool, void *block) {
    if (!pool || !block)
        return;

    /* 计算块索引 */
    uint8_t *ptr = (uint8_t *) block;
    size_t offset = (size_t) (ptr - pool->blocks);
    if (offset % pool->block_size != 0)
        return; /* 不是有效的块地址 */

    int idx = (int) (offset / pool->block_size);
    if (idx < 0 || idx >= pool->total_count)
        return; /* 越界检查 */

    if (!pool->used[idx])
        return; /* 双重释放：块已在空闲列表中 */

    pool->used[idx] = 0; /* 标记为空闲 */

    /* 将块索引压入空闲列表 */
    if (pool->free_count >= pool->total_count)
        return;
    pool->free_list[pool->free_count] = idx;
    pool->free_count++;
}

/**
 * @brief 销毁内存池并释放所有关联资源
 * @param pool 内存池指针，传入 NULL 时安全返回
 * @note 销毁后所有通过 mem_pool_alloc() 分配的指针均失效，调用者需确保不再使用
 */
void mem_pool_destroy(MemPool *pool) {
    if (!pool)
        return;
    lv_free((void **) &pool->used);
    lv_free((void **) &pool->blocks);
    lv_free((void **) &pool->free_list);
    lv_free((void **) &pool);
}

/**
 * @brief 获取内存池的统计信息
 * @param pool         内存池指针，传入 NULL 时安全返回
 * @param total_blocks  输出参数，接收总块数量，可为 NULL
 * @param free_blocks   输出参数，接收空闲块数量，可为 NULL
 * @param total_bytes   输出参数，接收总字节数（使用 size_t 避免大内存池截断），可为 NULL
 */
void mem_pool_stats(const MemPool *pool, int *total_blocks, int *free_blocks, size_t *total_bytes) {
    if (!pool)
        return;
    if (total_blocks)
        *total_blocks = pool->total_count;
    if (free_blocks)
        *free_blocks = pool->free_count;
    if (total_bytes)
        *total_bytes = (size_t) pool->total_count * pool->block_size;
}
