/**
 * @file runtime_guard.c
 * @brief 运行时数据保护实现 —— 守卫上下文的初始化、销毁、统计与完整性校验
 *
 * @details 实现 runtime_guard.h 中声明的运行时守卫 API。
 *          仅在 LV00_ENABLE_RUNTIME_GUARDS 编译开关启用时编译。
 *          当开关关闭时，所有函数在头文件中以 static inline 空操作内联。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 * @date   2026-05-24
 */

#include "runtime_guard.h"

#ifdef LV00_ENABLE_RUNTIME_GUARDS

#include "context.h"

#include <string.h>

/* ========================================================================
 * 内部辅助：平台相关的锁初始化/销毁
 * ======================================================================== */

/**
 * @brief 初始化读写锁和互斥锁
 * @param guard 守卫上下文指针（已验证非 NULL）
 * @return true 成功，false 失败
 */
static bool guard_init_locks(Lv00GuardContext *guard) {
#ifdef _WIN32
    LV00_RWLOCK_INIT(&guard->ctx_rwlock);
    InitializeCriticalSection(&guard->stat_mutex);
#else
    if (pthread_rwlock_init(&guard->ctx_rwlock.rwlock, NULL) != 0) {
        return false;
    }
    guard->ctx_rwlock.write_flag = 0;
    guard->ctx_rwlock.reader_count = 0;
    if (pthread_mutex_init(&guard->stat_mutex, NULL) != 0) {
        pthread_rwlock_destroy(&guard->ctx_rwlock.rwlock);
        return false;
    }
#endif
    return true;
}

/**
 * @brief 销毁读写锁和互斥锁
 * @param guard 守卫上下文指针（已验证非 NULL）
 */
static void guard_destroy_locks(Lv00GuardContext *guard) {
#ifdef _WIN32
    LV00_RWLOCK_DESTROY(&guard->ctx_rwlock);
    DeleteCriticalSection(&guard->stat_mutex);
#else
    LV00_RWLOCK_DESTROY(&guard->ctx_rwlock);
    pthread_mutex_destroy(&guard->stat_mutex);
#endif
}

/**
 * @brief 锁定统计互斥锁
 */
static void guard_lock_stats(const Lv00GuardContext *guard) {
#ifdef _WIN32
    EnterCriticalSection((CRITICAL_SECTION *)&guard->stat_mutex);
#else
    pthread_mutex_lock((pthread_mutex_t *)&guard->stat_mutex);
#endif
}

/**
 * @brief 解锁统计互斥锁
 */
static void guard_unlock_stats(const Lv00GuardContext *guard) {
#ifdef _WIN32
    LeaveCriticalSection((CRITICAL_SECTION *)&guard->stat_mutex);
#else
    pthread_mutex_unlock((pthread_mutex_t *)&guard->stat_mutex);
#endif
}

/* ========================================================================
 * 公开 API 实现
 * ======================================================================== */

bool lv00_guard_init(Lv00GuardContext *guard) {
    if (!guard) {
        return false;
    }

    /* 初始化读写锁和互斥锁 */
    if (!guard_init_locks(guard)) {
        return false;
    }

    /* 清零统计信息 */
    memset(&guard->stats, 0, sizeof(Lv00GuardStats));

    /* 标记为已初始化 */
    guard->initialized = true;

    return true;
}

void lv00_guard_destroy(Lv00GuardContext *guard) {
    if (!guard) {
        return;
    }

    if (!guard->initialized) {
        return;
    }

    /* 销毁平台锁资源 */
    guard_destroy_locks(guard);

    /* 标记为未初始化 */
    guard->initialized = false;

    /* 清零统计信息，防止残留数据被误读 */
    memset(&guard->stats, 0, sizeof(Lv00GuardStats));
}

void lv00_guard_get_stats(const Lv00GuardContext *guard, Lv00GuardStats *stats) {
    if (!guard || !stats) {
        return;
    }

    if (!guard->initialized) {
        memset(stats, 0, sizeof(Lv00GuardStats));
        return;
    }

    /* 加锁拷贝统计信息，确保快照一致性 */
    guard_lock_stats(guard);
    memcpy(stats, &guard->stats, sizeof(Lv00GuardStats));
    guard_unlock_stats(guard);
}

void lv00_guard_reset_stats(Lv00GuardContext *guard) {
    if (!guard) {
        return;
    }

    if (!guard->initialized) {
        return;
    }

    /* 加锁清零统计信息 */
    guard_lock_stats(guard);
    memset(&guard->stats, 0, sizeof(Lv00GuardStats));
    guard_unlock_stats(guard);
}

/* ========================================================================
 * 数据完整性校验
 *
 * 检查 Lv00Context 中关键数据结构的一致性：
 *   1. 上下文指针非空
 *   2. 上下文状态有效性（不超过 LV00_CONTEXT_COMPLETE）
 *   3. 约束图主指针非空（若 state >= PARSING）
 *   4. 推理栈深度不超过上限
 *   5. 递归深度不超过上限
 *   6. 熔断器状态与上下文状态一致
 * ======================================================================== */

bool lv00_verify_data_integrity(struct Lv00Context *ctx) {
    if (!ctx) {
        return false;
    }

    /* 检查 1：上下文状态有效性 */
    if (ctx->state < LV00_CONTEXT_IDLE || ctx->state > LV00_CONTEXT_COMPLETE) {
        return false;
    }

    /* 检查 2：若处于 PARSING 或更高状态，主图指针不应为 NULL */
    if (ctx->state >= LV00_CONTEXT_PARSING && ctx->main_graph == NULL) {
        return false;
    }

    /* 检查 3：推理栈深度不超过上限 */
    if (ctx->reasoning_stack.top >= ctx->reasoning_stack.max_depth) {
        return false;
    }
    if (ctx->reasoning_stack.top >= 0 &&
        ctx->reasoning_stack.top >= ctx->reasoning_stack.capacity) {
        return false;
    }

    /* 检查 4：递归深度不超过上限 */
    if (ctx->recursion_depth > ctx->max_recursion_depth) {
        return false;
    }
    if (ctx->max_recursion_depth > LV00_CONTEXT_MAX_RECURSION_DEPTH) {
        return false;
    }

    /* 检查 5：熔断器状态一致性 */
    if (ctx->circuit_breaker.state < CIRCUIT_BREAKER_CLOSED ||
        ctx->circuit_breaker.state > CIRCUIT_BREAKER_OPEN) {
        return false;
    }

    /* 若熔断器处于 OPEN 状态且上下文不在 ERROR 状态，则不一致 */
    if (ctx->circuit_breaker.state == CIRCUIT_BREAKER_OPEN &&
        ctx->state != LV00_CONTEXT_ERROR) {
        return false;
    }

    /* 检查 6：快照引用计数非负 */
    if (ctx->snapshot_refcount < 0) {
        return false;
    }

    /* 所有检查通过 */
    return true;
}

#endif /* LV00_ENABLE_RUNTIME_GUARDS */
