/**
 * @file lv_arena.c
 * @brief 竞技场分配器实现 —— 批量分配一次性释放
 *
 * @details 实现基于块的竞技场分配器，支持：
 *          - 自动增长的块大小（翻倍策略）
 *          - 对齐分配（默认 8 字节，支持自定义 alignment）
 *          - Checkpoint/Rollback 机制
 *          - 可选线程安全
 *          - 线程局部临时竞技场
 *
 * @author Lv-00 Project
 * @version 1.0.0
 */

#include "lv/lv_arena.h"
#include "lv/lv_internal.h"
#include "lv/lv_thread.h"  /* lv_MUTEX_* 兼容宏依赖 lv_mutex_* 实现 */
#include "lv/cross_platform.h"  /* lv_THREAD_LOCAL */
#include <string.h>

/* ============================================================
 * 内部常量
 * ============================================================ */

/** @brief 默认块大小（64KB） */
#define LV_ARENA_DEFAULT_BLOCK_SIZE (64UL * 1024UL)

/** @brief 页大小（用于块大小对齐） */
#define LV_ARENA_PAGE_SIZE (4UL * 1024UL)

/** @brief 分配对齐（8 字节） */
#define LV_ARENA_ALIGNMENT 8

/* ============================================================
 * 内部辅助函数
 * ============================================================ */

/** @brief 将 size 向上对齐到页大小 */
static inline size_t align_to_page(size_t size) {
    return align_up(size, LV_ARENA_PAGE_SIZE);
}

/**
 * @brief 创建新的竞技场块
 * @param capacity  块的数据区域容量
 * @return 新块指针，失败返回 NULL
 */
static lvArenaBlock *arena_block_create(size_t capacity) {
    lvArenaBlock *block = (lvArenaBlock *)lv_malloc(sizeof(lvArenaBlock) + capacity);
    if (!block) return NULL;
    block->next = NULL;
    block->capacity = capacity;
    block->used = 0;
    return block;
}

/**
 * @brief 释放单个竞技场块
 */
static void arena_block_destroy(lvArenaBlock *block) {
    lv_free((void **)&block);
}

/**
 * @brief 释放整个竞技场块链表
 */
static void arena_block_list_destroy(lvArenaBlock *head) {
    while (head) {
        lvArenaBlock *next = head->next;
        arena_block_destroy(head);
        head = next;
    }
}

/**
 * @brief 从竞技场分配内存的内部实现（不进行锁操作）
 * @param alignment 对齐要求（2 的幂，0 表示默认 8 字节）
 */
static void *arena_alloc_impl(lvArena *arena, size_t size, size_t alignment) {
    if (!arena || size == 0) return NULL;

    size_t align = alignment > 0 ? alignment : LV_ARENA_ALIGNMENT;
    if (align < sizeof(void *)) {
        align = sizeof(void *);
    }
    /* 溢出检查：防止 size + align 溢出导致 align_up 回绕 */
    if (size > SIZE_MAX - align) {
        return NULL;
    }
    size_t aligned = align_up(size, align);

    /* 检查当前块是否有足够空间（保守预留 align-1 字节的对齐 padding） */
    if (!arena->head || arena->head->used + aligned + (align - 1) > arena->head->capacity) {
        /* 计算新块大小：翻倍策略 */
        size_t new_capacity = arena->block_size;
        if (arena->head) {
            size_t next_size = arena->head->capacity * 2;
            if (next_size > new_capacity) {
                new_capacity = next_size;
            }
        }

        /* 确保新块能容纳本次分配 */
        if (aligned > new_capacity) {
            new_capacity = align_up(aligned, arena->block_size);
        }

        /* 创建新块 */
        lvArenaBlock *block = arena_block_create(new_capacity);
        if (!block) return NULL;

        /* 追加到链表末尾 */
        if (arena->head) {
            arena->head->next = block;
        } else {
            arena->blocks = block;
        }
        arena->head = block;
        arena->total_allocated += sizeof(lvArenaBlock) + new_capacity;
    }

    /* 从当前块分配（补齐对齐 padding 后返回对齐地址） */
    uintptr_t start = (uintptr_t)((char *)(arena->head + 1) + arena->head->used);
    size_t padding = (align - (start % align)) % align;
    void *ptr = (char *)start + padding;
    size_t consumed = padding + aligned;
    arena->head->used += consumed;
    arena->total_used += consumed;
    return ptr;
}

/* ============================================================
 * 线程局部临时竞技场
 * ============================================================ */

static lv_THREAD_LOCAL lvArena *g_arena_tmp = NULL;

/* ============================================================
 * 公共 API 实现
 * ============================================================ */

lvArena *lv_arena_create(size_t block_size, bool thread_safe) {
    /* 确定块大小 */
    if (block_size == 0) {
        block_size = LV_ARENA_DEFAULT_BLOCK_SIZE;
    }
    block_size = align_to_page(block_size);

    lvArena *arena = (lvArena *)lv_malloc(sizeof(lvArena));
    if (!arena) return NULL;

    arena->head = NULL;
    arena->blocks = NULL;
    arena->block_size = block_size;
    arena->total_allocated = 0;
    arena->total_used = 0;
    arena->thread_safe = thread_safe;

    if (thread_safe) {
        lv_MUTEX_INIT(&arena->mutex);
    }

    return arena;
}

void lv_arena_destroy(lvArena *arena) {
    if (!arena) return;

    lv_arena_lock(arena);
    arena_block_list_destroy(arena->blocks);
    arena->head = NULL;
    arena->blocks = NULL;
    if (arena->thread_safe) {
        lv_MUTEX_DESTROY(&arena->mutex);
    }
    lv_free((void **)&arena);
}

void *lv_arena_alloc(lvArena *arena, size_t size) {
    lv_CHECK_NULL(arena, NULL);
    lv_arena_lock(arena);
    void *ptr = arena_alloc_impl(arena, size, LV_ARENA_ALIGNMENT);
    lv_arena_unlock(arena);
    return ptr;
}

void *lv_arena_alloc_aligned(lvArena *arena, size_t size, size_t alignment) {
    lv_CHECK_NULL(arena, NULL);
    lv_arena_lock(arena);
    void *ptr = arena_alloc_impl(arena, size, alignment);
    lv_arena_unlock(arena);
    return ptr;
}

void *lv_arena_calloc(lvArena *arena, size_t size) {
    lv_CHECK_NULL(arena, NULL);
    lv_arena_lock(arena);
    void *ptr = arena_alloc_impl(arena, size, LV_ARENA_ALIGNMENT);
    if (ptr) {
        memset(ptr, 0, size);
    }
    lv_arena_unlock(arena);
    return ptr;
}

char *lv_arena_strdup(lvArena *arena, const char *str) {
    lv_CHECK_NULL(arena, NULL);
    lv_CHECK_NULL(str, NULL);

    size_t len = strlen(str);
    lv_arena_lock(arena);
    char *ptr = (char *)arena_alloc_impl(arena, len + 1, LV_ARENA_ALIGNMENT);
    if (ptr) {
        memcpy(ptr, str, len + 1);
    }
    lv_arena_unlock(arena);
    return ptr;
}

void lv_arena_reset(lvArena *arena) {
    if (!arena) return;

    lv_arena_lock(arena);
    arena_block_list_destroy(arena->blocks);
    arena->head = NULL;
    arena->blocks = NULL;
    arena->total_allocated = 0;
    arena->total_used = 0;
    lv_arena_unlock(arena);
}

lvArenaMark lv_arena_mark(lvArena *arena) {
    lvArenaMark mark = { NULL, 0 };
    if (!arena) return mark;

    lv_arena_lock(arena);
    mark.block = arena->head;
    mark.offset = arena->head ? arena->head->used : 0;
    lv_arena_unlock(arena);
    return mark;
}

void lv_arena_reset_to_mark(lvArena *arena, lvArenaMark mark) {
    if (!arena || !mark.block) return;

    lv_arena_lock(arena);

    /* 遍历链表找到标记块及其前驱 */
    lvArenaBlock *prev = NULL;
    lvArenaBlock *block = arena->blocks;
    while (block) {
        if (block == mark.block) {
            /* 释放标记块之后的所有块 */
            lvArenaBlock *to_free = block->next;
            while (to_free) {
                lvArenaBlock *next = to_free->next;
                arena->total_allocated -= (sizeof(lvArenaBlock) + to_free->capacity);
                arena->total_used -= to_free->used;
                arena_block_destroy(to_free);
                to_free = next;
            }

            /* 恢复标记块的状态 */
            block->next = NULL;
            arena->total_used -= (block->used - mark.offset);
            block->used = mark.offset;
            arena->head = block;

            lv_arena_unlock(arena);
            return;
        }
        prev = block;
        block = block->next;
    }

    /* 未找到标记块（已被释放或无效），不做任何操作 */
    lv_arena_unlock(arena);
}

size_t lv_arena_total_allocated(const lvArena *arena) {
    return arena ? arena->total_allocated : 0;
}

size_t lv_arena_total_used(const lvArena *arena) {
    return arena ? arena->total_used : 0;
}

int lv_arena_block_count(const lvArena *arena) {
    if (!arena) return 0;
    int count = 0;
    lvArenaBlock *block = arena->blocks;
    while (block) {
        count++;
        block = block->next;
    }
    return count;
}

void lv_arena_lock(lvArena *arena) {
    if (arena && arena->thread_safe) {
        lv_MUTEX_LOCK(&arena->mutex);
    }
}

void lv_arena_unlock(lvArena *arena) {
    if (arena && arena->thread_safe) {
        lv_MUTEX_UNLOCK(&arena->mutex);
    }
}

lvArena *lv_arena_tmp(void) {
    if (!g_arena_tmp) {
        g_arena_tmp = lv_arena_create(0, false);
    }
    return g_arena_tmp;
}