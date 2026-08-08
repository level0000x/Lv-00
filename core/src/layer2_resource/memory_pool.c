/**
 * @file memory_pool.c
 * @brief 内存池系统实现
 *
 * @details 实现内存管理策略：
 *   1. 固定大小对象池
 *   （线性分配器已移除，由 lv_arena 承接；LRU 对象缓存已移除，缓存职责不再由本模块承担）
 *
 * 设计说明：本模块作为底层内存基础设施，池结构体本身使用 lv_malloc/lv_free
 * 分配，而内部数据块（性能关键路径）保留原生 malloc/free/realloc/calloc，
 * 原因：
 * 1. 避免循环依赖：lv_malloc 内部可能依赖内存池，而内存池不能依赖 lv_malloc
 *    进行内部块分配
 * 2. 对象池有独立的内存管理策略，内部块不走 lv 的统计系统
 * 3. 全局内存统计（lv_mem_*）是可选的附加功能，不影响核心分配路径
 * 4. 池结构体（lvObjectPool 等）
 *    的生命周期管理使用 lv_malloc/lv_free，便于统一追踪和调试
 *
 * 【为何内部数据块使用标准 malloc/free 而非 lv_malloc/lv_free】
 *
 * memory_pool 是整个项目的底层内存基础设施，位于依赖链的最底层。
 * 如果内部数据块也使用 lv_malloc/lv_free，会形成循环依赖：
 *
 *   lv_malloc() → 内存统计(lv_mem_record_alloc) → 可能触发内存池分配
 *   → lv_pool_alloc() → 内部 malloc → lv_malloc() → ...（无限递归）
 *
 * 因此，内部数据块（对象池中的内存块等）必须使用标准库的
 * malloc/free/realloc/calloc，绕过 lv 的统计和追踪系统。
 *
 * 而池结构体本身（lvObjectPool 等）的分配/释放
 * 使用 lv_malloc/lv_free，因为这些操作发生在池创建/销毁时，
 * 不在性能关键路径上，且便于统一追踪和调试。
 *
 * 使用约定：通过 lv_pool_alloc 分配的对象必须通过 lv_pool_free 释放，
 * 严禁混用 lv_free 或标准 free。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "memory_pool.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h" /* lv_FNV64_*, lv_CONFIG_POOL_* macros */

#include "lv_internal.h" /* lv_FNV64_* 哈希常量 */
#include "lv_utils.h"    /* lv_strdup, lv_malloc, lv_free */

/* ============== 内部常量 ============== */

/** 对象池增长因子 */
#define lv_POOL_GROWTH_FACTOR 2

/* [Bug修复] 溢出检查宏：检测 size_t 乘法是否溢出 */
#define lv_SIZE_MUL_OVERFLOW(a, b) (((a) != 0 && (b) > SIZE_MAX / (a)))

/* ============== 平台抽象层（线程安全） ============== */
#include "lv/lv_thread.h"

/* ============== 对象池实现 ============== */

/**
 * @brief 空闲链表节点
 */
typedef struct FreeNode {
    struct FreeNode *next;
} FreeNode;

/**
 * @brief 对象池结构
 */
struct lvObjectPool {
    /* 配置 */
    size_t object_size; /**< 单个对象大小 */
    size_t capacity;    /**< 当前容量 */
    bool thread_safe;   /**< 是否线程安全 */
    bool auto_grow;     /**< 是否自动扩展 */
    char name[32];      /**< 池名称 */

    /* 内存块 */
    void **blocks;            /**< 内存块数组 */
    size_t block_count;       /**< 内存块数量 */
    size_t block_capacity;    /**< 内存块数组容量 */
    size_t *block_capacities; /**< [Bug修复] 每块实际分配的对象数量，用于 pool_clear 正确重建空闲链表 */

    /* 空闲链表 */
    FreeNode *free_list; /**< 空闲对象链表 */

    /* 统计 */
    uint64_t total_allocs; /**< 总分配次数 */
    uint64_t total_frees;  /**< 总释放次数 */
    size_t current_used;   /**< 当前使用数量 */

    /* 线程安全 */
    lv_mutex_t mutex; /**< 互斥锁 */
};

/* 对齐辅助函数（共享定义见 lv_internal.h 的 align_up） */

lvObjectPool *lv_pool_create(const lvPoolConfig *config) {
    if (!config || config->object_size == 0) {
        return NULL;
    }

    lvObjectPool *pool = (lvObjectPool *) malloc(sizeof(lvObjectPool));
    if (!pool) {
        return NULL;
    }
    memset(pool, 0, sizeof(lvObjectPool));

    /* 对象大小至少能存放一个 FreeNode 指针 */
    pool->object_size = align_up(config->object_size, sizeof(void *));
    pool->capacity = config->capacity > 0 ? config->capacity : lv_POOL_DEFAULT_CAPACITY;
    pool->thread_safe = config->thread_safe;
    pool->auto_grow = config->auto_grow;

    if (config->name) {
        strncpy(pool->name, config->name, sizeof(pool->name) - 1);
        pool->name[sizeof(pool->name) - 1] = '\0';
    } else {
        pool->name[0] = '\0';
    }

    /* 初始化线程锁 */
    if (pool->thread_safe) {
        lv_mutex_init(&pool->mutex);
    }

    /* 分配初始内存块数组 */
    pool->block_capacity = 4;
    pool->blocks = (void **) malloc(pool->block_capacity * sizeof(void *));
    if (!pool->blocks) {
        lv_free((void **) &pool);
        return NULL;
    }

    /* [Bug修复] 分配块容量记录数组 */
    pool->block_capacities = (size_t *) malloc(pool->block_capacity * sizeof(size_t));
    if (!pool->block_capacities) {
        if (pool->thread_safe) {
            lv_mutex_destroy(&pool->mutex);
        }
        lv_free((void **) &pool->blocks);
        lv_free((void **) &pool);
        return NULL;
    }

    /* 分配第一个内存块（性能关键路径：保留原生 malloc，避免循环依赖开销） */
    void *block = malloc(pool->object_size * pool->capacity);
    if (!block) {
        if (pool->thread_safe) {
            lv_mutex_destroy(&pool->mutex);
        }
        lv_free((void **) &pool->block_capacities);
        lv_free((void **) &pool->blocks);
        lv_free((void **) &pool);
        return NULL;
    }
    pool->blocks[0] = block;
    pool->block_capacities[0] = pool->capacity; /* [Bug修复] 记录首块容量 */
    pool->block_count = 1;

    /* 初始化空闲链表 */
    pool->free_list = NULL;
    char *ptr = (char *) block;
    for (size_t i = 0; i < pool->capacity; i++) {
        FreeNode *node = (FreeNode *) (ptr + i * pool->object_size);
        node->next = pool->free_list;
        pool->free_list = node;
    }

    return pool;
}

void lv_pool_destroy(lvObjectPool *pool) {
    if (!pool) {
        return;
    }

    /* 释放所有内存块（原生 malloc 分配，用原生 free 释放，避免触发毒模式检测） */
    for (size_t i = 0; i < pool->block_count; i++) {
        free(pool->blocks[i]);
    }
    free(pool->blocks);
    free(pool->block_capacities);

    /* 销毁线程锁 */
    if (pool->thread_safe) {
        lv_mutex_destroy(&pool->mutex);
    }

    lv_free((void **) &pool);
}

void *lv_pool_alloc(lvObjectPool *pool) {
    if (!pool) {
        return NULL;
    }

    /* 作用域锁守卫：离开函数（含所有 return 分支）自动解锁，杜绝漏解锁 */
    lvLockGuard _pool_guard __attribute__((cleanup(lv_lock_guard_scope_cleanup))) = {NULL};
    if (pool->thread_safe) {
        lv_lock_guard_init(&_pool_guard, &pool->mutex);
    }

    /* 空闲链表为空，需要扩展 */
    if (!pool->free_list) {
        if (!pool->auto_grow) {
            return NULL;
        }

        /* 扩展内存块数组 */
        if (pool->block_count >= pool->block_capacity) {
            /* 溢出检查（双重检查由 lv_ensure_capacity 内部完成） */
            int cap = (int) pool->block_capacity;

            /* 第一次：扩容 blocks */
            if (!lv_ensure_capacity((void **) &pool->blocks, cap, &cap,
                                    sizeof(void *), 1)) {
                return NULL;
            }
            int blocks_cap = cap; /* blocks 的新容量 */

            /* 第二次：扩容 block_capacities 与 blocks 同步。
             * 临时回退容量指针使扩容真实执行；失败时回滚 blocks：
             * 缩回旧大小，若失败则保留新块（仍有效） */
            cap = (int) pool->block_capacity;
            if (!lv_ensure_capacity((void **) &pool->block_capacities, cap, &cap,
                                    sizeof(size_t), blocks_cap - cap)) {
                void **shrunk = (void **) lv_realloc(pool->blocks,
                                                     pool->block_capacity * sizeof(void *));
                if (shrunk) {
                    pool->blocks = shrunk;
                }
                /* pool->blocks 始终有效（要么是 shrunk，要么是新块） */
                return NULL;
            }
            pool->block_capacity = (size_t) blocks_cap;
        }

        /* 分配新内存块 */
        /* [Bug修复] 溢出检查：确保 capacity * GROWTH_FACTOR 不会溢出 */
        if (lv_SIZE_MUL_OVERFLOW(pool->capacity, lv_POOL_GROWTH_FACTOR)) {
            return NULL;
        }
        size_t new_capacity = pool->capacity * lv_POOL_GROWTH_FACTOR;

        /* [Bug修复] 溢出检查：确保 object_size * new_capacity 不会溢出 */
        if (lv_SIZE_MUL_OVERFLOW(pool->object_size, new_capacity)) {
            return NULL;
        }
        /* 分配新内存块（性能关键路径：保留原生 malloc，避免循环依赖开销） */
        void *block = malloc(pool->object_size * new_capacity);
        if (!block) {
            return NULL;
        }
        pool->blocks[pool->block_count] = block;
        pool->block_capacities[pool->block_count] = new_capacity; /* [Bug修复] 记录新块容量 */
        pool->block_count++;

        /* 添加到空闲链表 */
        char *ptr = (char *) block;
        for (size_t i = 0; i < new_capacity; i++) {
            FreeNode *node = (FreeNode *) (ptr + i * pool->object_size);
            node->next = pool->free_list;
            pool->free_list = node;
        }
        pool->capacity += new_capacity;
    }

    /* 从空闲链表取出 */
    FreeNode *node = pool->free_list;
    pool->free_list = node->next;
    pool->total_allocs++;
    pool->current_used++;

    /* 守卫在函数末尾自动解锁（原提前解锁点，语义等价：临界区无嵌套锁） */

    /* 清零对象 */
    memset(node, 0, pool->object_size);
    return node;
}

bool lv_pool_free(lvObjectPool *pool, void *obj) {
    if (!obj) {
        return false;
    }

    /* [归属校验] 池未初始化（lv_init 未调用或 preset pools 初始化失败）时，
     * 无池可归还：对象必然来自回退的普通分配，按 lv_free 语义释放 */
    if (!pool) {
        void *tmp = obj;
        lv_free(&tmp);
        return true;
    }

    /* 作用域锁守卫：离开函数（含所有 return 分支）自动解锁 */
    lvLockGuard _pool_guard __attribute__((cleanup(lv_lock_guard_scope_cleanup))) = {NULL};
    if (pool->thread_safe) {
        lv_lock_guard_init(&_pool_guard, &pool->mutex);
    }

    /* [归属校验] 校验 obj 是否落在池的内存块内且位于对象边界上。
     * 池外指针（如 lv_pool_alloc 失败后回退 lv_calloc 分配的对象）严禁
     * 写入空闲链表，否则会破坏链表结构；此类对象按普通分配释放。 */
    bool belongs = false;
    for (size_t b = 0; b < pool->block_count && !belongs; b++) {
        const char *base = (const char *) pool->blocks[b];
        const char *p = (const char *) obj;
        if (p >= base && (size_t) (p - base) < pool->block_capacities[b] * pool->object_size &&
            (size_t) (p - base) % pool->object_size == 0) {
            belongs = true;
        }
    }
    if (!belongs) {
        void *tmp = obj;
        lv_free(&tmp);
        return true;
    }

    /* 添加到空闲链表 */
    FreeNode *node = (FreeNode *) obj;
    node->next = pool->free_list;
    pool->free_list = node;
    pool->total_frees++;
    if (pool->current_used > 0)
        pool->current_used--;

    return true;
}

void lv_pool_get_stats(lvObjectPool *pool, uint64_t *out_total_allocs, uint64_t *out_total_frees,
                       size_t *out_current_used) {
    if (!pool) {
        return;
    }

    /* 作用域锁守卫：离开函数自动解锁 */
    lvLockGuard _pool_guard __attribute__((cleanup(lv_lock_guard_scope_cleanup))) = {NULL};
    if (pool->thread_safe) {
        lv_lock_guard_init(&_pool_guard, &pool->mutex);
    }

    if (out_total_allocs)
        *out_total_allocs = pool->total_allocs;
    if (out_total_frees)
        *out_total_frees = pool->total_frees;
    if (out_current_used)
        *out_current_used = pool->current_used;
}

void lv_pool_clear(lvObjectPool *pool) {
    if (!pool) {
        return;
    }

    /* 作用域锁守卫：离开函数自动解锁 */
    lvLockGuard _pool_guard __attribute__((cleanup(lv_lock_guard_scope_cleanup))) = {NULL};
    if (pool->thread_safe) {
        lv_lock_guard_init(&_pool_guard, &pool->mutex);
    }

    /* [Bug修复] 使用 block_capacities 记录的实际容量重建空闲链表，
     * 替代原来基于 lv_POOL_DEFAULT_CAPACITY 的错误计算 */
    pool->free_list = NULL;
    for (size_t b = 0; b < pool->block_count; b++) {
        char *ptr = (char *) pool->blocks[b];
        size_t block_size = pool->block_capacities[b];
        for (size_t i = 0; i < block_size; i++) {
            FreeNode *node = (FreeNode *) (ptr + i * pool->object_size);
            node->next = pool->free_list;
            pool->free_list = node;
        }
    }
    pool->current_used = 0;
}

/* ============== 全局内存统计 ============== */

/**
 * @brief 内存池模块全局状态
 *
 * 将所有模块级全局变量归并到单一上下文结构体中，
 * 降低模块耦合度，提高可维护性。
 */
typedef struct MemoryPoolState {
    /* 统计信息 */
    lvMemoryStats global_stats;
    lv_mutex_t stats_mutex;
    lv_once_t stats_once;

    /* 对象池 */
    lvObjectPool *node_pool;
    lvObjectPool *constraint_pool;
    lvObjectPool *symbolic_coord_pool;
    lvObjectPool *proof_step_pool;
} MemoryPoolState;

/** 模块级唯一状态实例 */
static MemoryPoolState s_mem_state = {0};

static void stats_mutex_init_func(void) {
    lv_mutex_init(&s_mem_state.stats_mutex);
}

int lv_mem_register_type(const char *name) {
    if (!name) {
        return -1;
    }

    lv_once(&s_mem_state.stats_once, stats_mutex_init_func);
    lv_mutex_lock(&s_mem_state.stats_mutex);

    if (s_mem_state.global_stats.type_count >= lv_MEM_STAT_MAX_TYPES) {
        lv_mutex_unlock(&s_mem_state.stats_mutex);
        return -1;
    }

    int id = s_mem_state.global_stats.type_count++;
    s_mem_state.global_stats.types[id].name = lv_strdup(name); /* 复制字符串，避免保存裸指针 */
    s_mem_state.global_stats.types[id].total_allocs = 0;
    s_mem_state.global_stats.types[id].total_frees = 0;
    s_mem_state.global_stats.types[id].current_bytes = 0;
    s_mem_state.global_stats.types[id].peak_bytes = 0;

    lv_mutex_unlock(&s_mem_state.stats_mutex);
    return id;
}

void lv_mem_record_alloc(int type_id, size_t size) {
    if (type_id < 0) {
        return;
    }

    lv_once(&s_mem_state.stats_once, stats_mutex_init_func);
    lv_mutex_lock(&s_mem_state.stats_mutex);

    if ((size_t) type_id < (size_t) s_mem_state.global_stats.type_count) {
        s_mem_state.global_stats.types[type_id].total_allocs++;
        s_mem_state.global_stats.types[type_id].current_bytes += size;
        if (s_mem_state.global_stats.types[type_id].current_bytes > s_mem_state.global_stats.types[type_id].peak_bytes) {
            s_mem_state.global_stats.types[type_id].peak_bytes = s_mem_state.global_stats.types[type_id].current_bytes;
        }
    }
    s_mem_state.global_stats.total_bytes += size;
    if (s_mem_state.global_stats.total_bytes > s_mem_state.global_stats.peak_bytes) {
        s_mem_state.global_stats.peak_bytes = s_mem_state.global_stats.total_bytes;
    }

    lv_mutex_unlock(&s_mem_state.stats_mutex);
}

void lv_mem_record_free(int type_id, size_t size) {
    if (type_id < 0) {
        return;
    }

    lv_once(&s_mem_state.stats_once, stats_mutex_init_func);
    lv_mutex_lock(&s_mem_state.stats_mutex);

    if ((size_t) type_id < (size_t) s_mem_state.global_stats.type_count) {
        s_mem_state.global_stats.types[type_id].total_frees++;
        if (s_mem_state.global_stats.types[type_id].current_bytes >= size) {
            s_mem_state.global_stats.types[type_id].current_bytes -= size;
        }
    }
    if (s_mem_state.global_stats.total_bytes >= size) {
        s_mem_state.global_stats.total_bytes -= size;
    }

    lv_mutex_unlock(&s_mem_state.stats_mutex);
}

void lv_mem_get_global_stats(lvMemoryStats *stats) {
    if (!stats) {
        return;
    }

    lv_once(&s_mem_state.stats_once, stats_mutex_init_func);
    lv_mutex_lock(&s_mem_state.stats_mutex);
    memcpy(stats, &s_mem_state.global_stats, sizeof(lvMemoryStats));
    lv_mutex_unlock(&s_mem_state.stats_mutex);
}

void lv_mem_reset_stats(void) {
    lv_once(&s_mem_state.stats_once, stats_mutex_init_func);
    lv_mutex_lock(&s_mem_state.stats_mutex);
    /* 释放已注册类型的名称字符串，防止内存泄漏 */
    for (int i = 0; i < s_mem_state.global_stats.type_count; i++) {
        if (s_mem_state.global_stats.types[i].name) {
            lv_free((void **) &s_mem_state.global_stats.types[i].name);
        }
    }
    memset(&s_mem_state.global_stats, 0, sizeof(lvMemoryStats));
    lv_mutex_unlock(&s_mem_state.stats_mutex);
}

void lv_mem_print_stats(void *stream) {
    if (!stream) {
        stream = stdout;
    }

    lv_once(&s_mem_state.stats_once, stats_mutex_init_func);
    lv_mutex_lock(&s_mem_state.stats_mutex);

    fprintf((FILE *) stream, "\n========== Lv-00 内存统计 ==========\n");
    fprintf((FILE *) stream, "总使用: %llu 字节, 峰值: %llu 字节\n", (unsigned long long) s_mem_state.global_stats.total_bytes,
            (unsigned long long) s_mem_state.global_stats.peak_bytes);
    fprintf((FILE *) stream, "\n各类型统计:\n");
    fprintf((FILE *) stream, "%-24s %12s %12s %12s %12s\n", "类型", "分配次数", "释放次数", "当前字节", "峰值字节");
    fprintf((FILE *) stream, "------------------------------------------------------------\n");

    for (int i = 0; i < s_mem_state.global_stats.type_count; i++) {
        lvMemTypeStat *s = &s_mem_state.global_stats.types[i];
        fprintf((FILE *) stream, "%-24s %12llu %12llu %12llu %12llu\n", s->name ? s->name : "(unnamed)",
                (unsigned long long) s->total_allocs, (unsigned long long) s->total_frees,
                (unsigned long long) s->current_bytes, (unsigned long long) s->peak_bytes);
    }

    fprintf((FILE *) stream, "====================================\n\n");

    lv_mutex_unlock(&s_mem_state.stats_mutex);
}

/* ============== 预定义对象池 ============== */

/* 对象大小定义 —— 集中管理于 config.h，此处引用 */
#define lv_CONSTRAINT_NODE_SIZE lv_CONFIG_POOL_CONSTRAINT_NODE_SIZE
#define lv_CONSTRAINT_SIZE lv_CONFIG_POOL_CONSTRAINT_SIZE
#define lv_SYMBOLIC_COORD_SIZE lv_CONFIG_POOL_SYMBOLIC_COORD_SIZE
#define lv_PROOF_STEP_SIZE lv_CONFIG_POOL_PROOF_STEP_SIZE

bool lv_init_preset_pools(void) {
    /* 防御性检查：防止二次初始化导致旧池泄漏 */
    if (s_mem_state.node_pool != NULL || s_mem_state.constraint_pool != NULL || s_mem_state.symbolic_coord_pool != NULL ||
        s_mem_state.proof_step_pool != NULL) {
        return true; /* 已经初始化 */
    }

    lvPoolConfig config = {
        .object_size = 0, .capacity = lv_POOL_DEFAULT_CAPACITY, .thread_safe = true, .auto_grow = true, .name = NULL};

    /* ConstraintNode 池 */
    config.object_size = lv_CONSTRAINT_NODE_SIZE;
    config.name = "ConstraintNode";
    s_mem_state.node_pool = lv_pool_create(&config);
    if (!s_mem_state.node_pool) {
        return false;
    }

    /* Constraint 池 */
    config.object_size = lv_CONSTRAINT_SIZE;
    config.name = "Constraint";
    s_mem_state.constraint_pool = lv_pool_create(&config);
    if (!s_mem_state.constraint_pool) {
        lv_pool_destroy(s_mem_state.node_pool);
        s_mem_state.node_pool = NULL;
        return false;
    }

    /* SymbolicCoord 池 */
    config.object_size = lv_SYMBOLIC_COORD_SIZE;
    config.name = "SymbolicCoord";
    s_mem_state.symbolic_coord_pool = lv_pool_create(&config);
    if (!s_mem_state.symbolic_coord_pool) {
        lv_pool_destroy(s_mem_state.node_pool);
        lv_pool_destroy(s_mem_state.constraint_pool);
        s_mem_state.node_pool = NULL;
        s_mem_state.constraint_pool = NULL;
        return false;
    }

    /* ProofStep 池 */
    config.object_size = lv_PROOF_STEP_SIZE;
    config.name = "ProofStep";
    s_mem_state.proof_step_pool = lv_pool_create(&config);
    if (!s_mem_state.proof_step_pool) {
        lv_pool_destroy(s_mem_state.node_pool);
        lv_pool_destroy(s_mem_state.constraint_pool);
        lv_pool_destroy(s_mem_state.symbolic_coord_pool);
        s_mem_state.node_pool = NULL;
        s_mem_state.constraint_pool = NULL;
        s_mem_state.symbolic_coord_pool = NULL;
        return false;
    }

    return true;
}

void lv_cleanup_preset_pools(void) {
    if (s_mem_state.proof_step_pool) {
        lv_pool_destroy(s_mem_state.proof_step_pool);
        s_mem_state.proof_step_pool = NULL;
    }
    if (s_mem_state.symbolic_coord_pool) {
        lv_pool_destroy(s_mem_state.symbolic_coord_pool);
        s_mem_state.symbolic_coord_pool = NULL;
    }
    if (s_mem_state.constraint_pool) {
        lv_pool_destroy(s_mem_state.constraint_pool);
        s_mem_state.constraint_pool = NULL;
    }
    if (s_mem_state.node_pool) {
        lv_pool_destroy(s_mem_state.node_pool);
        s_mem_state.node_pool = NULL;
    }
}

lvObjectPool *lv_get_node_pool(void) {
    return s_mem_state.node_pool;
}

lvObjectPool *lv_get_constraint_pool(void) {
    return s_mem_state.constraint_pool;
}

lvObjectPool *lv_get_symbolic_coord_pool(void) {
    return s_mem_state.symbolic_coord_pool;
}

lvObjectPool *lv_get_proof_step_pool(void) {
    return s_mem_state.proof_step_pool;
}
