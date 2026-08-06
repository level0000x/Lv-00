/**
 * @file allocator.c
 * @brief 可替换的内存分配器策略接口实现
 *
 * 提供：
 *   1. 原始分配器（标准 malloc/free）—— 无额外开销
 *   2. 调试分配器（魔数 + 追踪 + 毒模式）—— 当前 lv_malloc 的默认行为
 *   3. 线程安全的分配器切换机制
 *
 * @author Lv-00 Project
 * @version 1.0.0
 */

#include "lv/allocator.h"
#include "lv_utils_internal.h" /* AllocHeader, get_header, track_allocation, etc. */
#include "lv/cross_platform.h" /* lv_THREAD_LOCAL */
#include "lv/lv_thread.h"      /* lv_mutex_t, lv_once_t, lv_ONCE_INIT */

#include <stdlib.h>
#include <string.h>

/* 平台相关的内存大小查询头文件 */
#ifdef _WIN32
#include <malloc.h> /* _msize */
#elif defined(__linux__)
#include <malloc.h> /* malloc_usable_size */
#elif defined(__APPLE__)
#include <malloc/malloc.h> /* malloc_size */
#endif

/* ============================================================
 * 平台相关的内存大小查询函数
 * ============================================================ */

#ifdef _WIN32
static size_t win32_size_query(void *ptr) {
    return (size_t) _msize(ptr);
}
#elif defined(__APPLE__)
static size_t apple_size_query(void *ptr) {
    return (size_t) malloc_size(ptr);
}
#elif defined(__linux__)
static size_t linux_size_query(void *ptr) {
    return (size_t) malloc_usable_size(ptr);
}
#endif

/* ============================================================
 * 原始分配器（标准 malloc/free）
 * ============================================================ */

static void *raw_alloc(size_t size) {
    return malloc(size);
}

static void *raw_calloc(size_t count, size_t size) {
    return calloc(count, size);
}

static void *raw_realloc(void *ptr, size_t new_size) {
    return realloc(ptr, new_size);
}

static void raw_free(void *ptr) {
    free(ptr);
}

static const AllocatorOps g_raw_allocator = {
    .alloc   = raw_alloc,
    .calloc  = raw_calloc,
    .realloc = raw_realloc,
    .free    = raw_free,
    .name    = "raw",
#ifdef _WIN32
    .size_query = win32_size_query
#elif defined(__APPLE__)
    .size_query = apple_size_query
#elif defined(__linux__)
    .size_query = linux_size_query
#endif
};

/* ============================================================
 * 调试分配器（魔数 + 追踪 + 毒模式）
 *
 * 保持与当前 lv_malloc/lv_free 完全一致的行为。
 * 使用 lv_utils_internal.h 中声明的内部辅助函数和状态。
 * ============================================================ */

/** 默认内存限制 —— 0 表示无限制 */
#define DEBUG_DEFAULT_MEMORY_LIMIT 0

/**
 * @brief 调试分配器：分配内存
 *
 * 附加 AllocHeader 头部和尾部魔数，加入追踪链表，更新统计。
 */
static void *debug_alloc(size_t size) {
    /* 零大小请求：分配最小块（1 字节数据 + 尾魔数），保持 lv_malloc(0) 语义 */
    size_t alloc_size = size ? size : 1;

    if (s_utils_state.memory_limit > 0 &&
        s_utils_state.memory_stats.current_used > s_utils_state.memory_limit - alloc_size) {
        return NULL;
    }

    /* 检查溢出：头部大小 + 用户请求大小 + 尾部魔数大小 */
    AllocHeader *hdr = NULL;
    size_t total = ALLOC_HEADER_SIZE;
    if (total > SIZE_MAX - alloc_size || total + alloc_size > SIZE_MAX - ALLOC_TAIL_MAGIC_SIZE) {
        return NULL;
    }
    total += alloc_size;
    total += ALLOC_TAIL_MAGIC_SIZE;

    hdr = (AllocHeader *) malloc(total);
    if (!hdr)
        return NULL;

    hdr->head_magic = ALLOC_HEAD_MAGIC;
    hdr->tail_offset = (uint32_t) alloc_size;
    hdr->size = alloc_size;
    hdr->file = NULL;
    hdr->line = 0;

    /* 设置尾部魔数（使用 memcpy 避免未对齐访问） */
    uint32_t tail_magic = ALLOC_TAIL_MAGIC;
    memcpy(hdr->data + alloc_size, &tail_magic, sizeof(uint32_t));

    /* 加入全局追踪链表 */
    track_allocation(hdr);

    s_utils_state.memory_stats.total_allocated += alloc_size;
    s_utils_state.memory_stats.current_used += alloc_size;
    s_utils_state.memory_stats.allocation_count++;
    if (s_utils_state.memory_stats.current_used > s_utils_state.memory_stats.peak_used)
        s_utils_state.memory_stats.peak_used = s_utils_state.memory_stats.current_used;

    return hdr->data;
}

/**
 * @brief 调试分配器：分配并清零
 */
static void *debug_calloc(size_t nmemb, size_t size) {
    /* 零大小请求：分配最小块（1 字节数据），保持与 lv_malloc(0) 一致的语义 */
    if (nmemb == 0 || size == 0) {
        nmemb = 1;
        size = 1;
    }

    /* 检查溢出 */
    if (nmemb > SIZE_MAX / size) {
        return NULL;
    }

    size_t total = nmemb * size;

    /* 检查头部和尾部大小溢出 */
    {
        size_t full = ALLOC_HEADER_SIZE;
        if (full > SIZE_MAX - total)
            return NULL;
        full += total;
        if (full > SIZE_MAX - ALLOC_TAIL_MAGIC_SIZE)
            return NULL;
        full += ALLOC_TAIL_MAGIC_SIZE;

        AllocHeader *hdr = (AllocHeader *) malloc(full);
        if (!hdr)
            return NULL;
        memset(hdr, 0, full);

        hdr->head_magic = ALLOC_HEAD_MAGIC;
        hdr->tail_offset = (uint32_t) total;
        hdr->size = total;
        hdr->file = NULL;
        hdr->line = 0;

        /* 设置尾部魔数（使用 memcpy 避免未对齐访问） */
        uint32_t ctail_magic = ALLOC_TAIL_MAGIC;
        memcpy(hdr->data + total, &ctail_magic, sizeof(uint32_t));

        track_allocation(hdr);

        s_utils_state.memory_stats.total_allocated += total;
        s_utils_state.memory_stats.current_used += total;
        s_utils_state.memory_stats.allocation_count++;
        if (s_utils_state.memory_stats.current_used > s_utils_state.memory_stats.peak_used)
            s_utils_state.memory_stats.peak_used = s_utils_state.memory_stats.current_used;

        return hdr->data;
    }
}

/**
 * @brief 调试分配器：重新分配内存
 *
 * 行为与当前 lv_realloc 一致：
 * - 当 size 为 0 时，返回 NULL 但不释放原内存。
 * - 原指针由本分配器分配则走追踪路径，否则走 fallback 路径。
 */
static void *debug_realloc(void *ptr, size_t size) {
    if (!ptr)
        return debug_alloc(size);
    if (size == 0)
        return NULL;

    size_t alloc_size = size;
    AllocHeader *old_hdr = get_header(ptr);

    if (old_hdr) {
        /* 正规路径：由本分配器分配的指针，可以精确追踪 */
        size_t old_size = old_hdr->size;

        /* 计算新总大小 */
        size_t new_total = ALLOC_HEADER_SIZE;
        if (new_total > SIZE_MAX - alloc_size)
            return NULL;
        new_total += alloc_size;
        if (new_total > SIZE_MAX - ALLOC_TAIL_MAGIC_SIZE)
            return NULL;
        new_total += ALLOC_TAIL_MAGIC_SIZE;

        /* 从追踪链表中移除旧节点 */
        untrack_allocation(old_hdr);

        AllocHeader *new_hdr = (AllocHeader *) realloc(old_hdr, new_total);
        if (!new_hdr) {
            /* realloc 失败：旧分配仍然有效，重新加入追踪链表 */
            track_allocation(old_hdr);
            return NULL;
        }

        new_hdr->head_magic = ALLOC_HEAD_MAGIC;
        new_hdr->tail_offset = (uint32_t) alloc_size;
        new_hdr->size = alloc_size;

        /* 设置尾部魔数 */
        uint32_t tail_magic = ALLOC_TAIL_MAGIC;
        memcpy(new_hdr->data + alloc_size, &tail_magic, sizeof(uint32_t));

        /* 重新加入追踪链表 */
        track_allocation(new_hdr);

        /* 更新统计：减去旧大小，加上新大小 */
        s_utils_state.memory_stats.total_allocated += alloc_size;
        if (old_size <= s_utils_state.memory_stats.current_used) {
            s_utils_state.memory_stats.current_used = s_utils_state.memory_stats.current_used - old_size + alloc_size;
        } else {
            s_utils_state.memory_stats.current_used += alloc_size;
        }
        if (s_utils_state.memory_stats.current_used > s_utils_state.memory_stats.peak_used)
            s_utils_state.memory_stats.peak_used = s_utils_state.memory_stats.current_used;

        return new_hdr->data;
    } else {
        /* 非本分配器分配的指针（魔数不匹配）：
         * 尝试获取旧分配大小并复制数据，以避免数据丢失。 */
        void *new_ptr = debug_alloc(alloc_size);
        if (!new_ptr)
            return NULL;

        /* 尝试获取旧分配的实际可用大小 */
        size_t old_usable_size = 0;
        const AllocatorOps *ops = lv_allocator_get();
        if (ops && ops->size_query) {
            old_usable_size = ops->size_query(ptr);
        }
        if (old_usable_size > 0) {
            size_t copy_size = (alloc_size < old_usable_size) ? alloc_size : old_usable_size;
            memcpy(new_ptr, ptr, copy_size);
        }

        /* 释放旧指针（由 raw malloc/calloc 分配，用 raw free 释放） */
        free(ptr);

        return new_ptr;
    }
}

/**
 * @brief 调试分配器：释放内存
 *
 * 执行魔数验证、毒模式填充、追踪链表移除、统计更新，
 * 然后释放底层内存。与当前 lv_free_ptr 行为一致。
 */
static void debug_free(void *ptr) {
    if (!ptr)
        return;

    AllocHeader *hdr = get_header(ptr);
    if (hdr) {
        /* 正规路径：由本分配器分配的指针 */
        size_t freed_size = hdr->size;

        /* 从追踪链表中移除 */
        untrack_allocation(hdr);

        /* 用毒模式填充用户数据区（检测 use-after-free） */
        fill_poison(hdr->data, hdr->tail_offset);

        /* 标记头部魔数为已释放（防止 double-free） */
        hdr->head_magic = ALLOC_MAGIC_FREED;

        /* 更新统计 */
        if (freed_size <= s_utils_state.memory_stats.current_used) {
            s_utils_state.memory_stats.current_used -= freed_size;
        } else {
            /* 防御：统计不一致时将 current_used 归零 */
            s_utils_state.memory_stats.current_used = 0;
        }
        s_utils_state.memory_stats.total_freed += freed_size;
        s_utils_state.memory_stats.free_count++;

        free(hdr);
    } else {
        /* 非本分配器指针或已释放（魔数不匹配）：
         * 仍然释放内存以防泄漏，但不更新统计。 */
        free(ptr);
    }
}

static const AllocatorOps g_debug_allocator = {
    .alloc   = debug_alloc,
    .calloc  = debug_calloc,
    .realloc = debug_realloc,
    .free    = debug_free,
    .name    = "debug",
#ifdef _WIN32
    .size_query = win32_size_query
#elif defined(__APPLE__)
    .size_query = apple_size_query
#elif defined(__linux__)
    .size_query = linux_size_query
#endif
};

/* ============================================================
 * 分配器切换机制
 * ============================================================

 * 使用全局互斥锁保护写操作（lv_allocator_set / lv_allocator_reset）。
 * 读操作（lv_allocator_get）不使用锁，因为：
 *   - 指针写入仅在切换时发生（罕见操作）
 *   - 对齐的指针读取在目标平台上天然是原子的
 *   - 初始化时静态赋值，无竞态窗口
 */

/** 当前分配器指针，初始为调试分配器（向后兼容） */
static const AllocatorOps *s_current_allocator = &g_debug_allocator;

/** 写操作互斥锁（惰性初始化，首次加锁时自动完成） */
lv_LAZY_LOCK_DEFINE(s_allocator_lock);

const AllocatorOps *lv_allocator_set(const AllocatorOps *ops) {
    const AllocatorOps *prev = NULL;

    lv_lazy_lock_lock(&s_allocator_lock, s_allocator_lock_init_once);
    prev = s_current_allocator;
    if (ops && ops->alloc && ops->free) {
        s_current_allocator = ops;
    }
    lv_lazy_lock_unlock(&s_allocator_lock);

    return prev;
}

const AllocatorOps *lv_allocator_get(void) {
    return s_current_allocator;
}

void lv_allocator_reset(void) {
    lv_allocator_set(&g_debug_allocator);
}

const AllocatorOps *lv_allocator_raw(void) {
    return &g_raw_allocator;
}

const AllocatorOps *lv_allocator_debug(void) {
    return &g_debug_allocator;
}