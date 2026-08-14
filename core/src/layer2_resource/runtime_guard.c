/**
 * @file runtime_guard.c
 * @brief 运行时数据保护实现 —— 守卫上下文的初始化、销毁、统计与完整性校验
 *
 * @details 实现 runtime_guard.h 中声明的运行时守卫 API。
 *          仅在 lv_ENABLE_RUNTIME_GUARDS 编译开关启用时编译。
 *          当开关关闭时，所有函数在头文件中以 static inline 空操作内联。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 * @date   2026-05-24
 */

#include "lv/runtime_guard.h"
#include "lv/lv_thread.h"  /* lv_MUTEX_* 兼容宏依赖 lv_mutex_* 实现 */

#ifdef lv_ENABLE_RUNTIME_GUARDS

#include <string.h>

#include "lv/context.h"

/* ========================================================================
 * 内部辅助：平台相关的锁初始化/销毁
 * ======================================================================== */

/**
 * @brief 初始化读写锁和互斥锁
 * @param guard 守卫上下文指针（已验证非 NULL）
 * @return true 成功，false 失败
 */
static bool guard_init_locks(lvGuardContext *guard) {
    lv_RWLOCK_INIT(&guard->ctx_rwlock);
    lv_MUTEX_INIT(&guard->stat_mutex);
    return true;
}

/**
 * @brief 销毁读写锁和互斥锁
 * @param guard 守卫上下文指针（已验证非 NULL）
 */
static void guard_destroy_locks(lvGuardContext *guard) {
    lv_RWLOCK_DESTROY(&guard->ctx_rwlock);
    lv_MUTEX_DESTROY(&guard->stat_mutex);
}

/**
 * @brief 锁定统计互斥锁
 */
static void guard_lock_stats(const lvGuardContext *guard) {
    lv_MUTEX_LOCK((lvMutex *) &guard->stat_mutex);
}

static void guard_unlock_stats(const lvGuardContext *guard) {
    lv_MUTEX_UNLOCK((lvMutex *) &guard->stat_mutex);
}

/* ========================================================================
 * 公开 API 实现
 * ======================================================================== */

/* exempt: 惰性守卫豁免 —— guard->initialized 为 lvGuardContext 对象实例字段：
 * 由 lv_guard_init/lv_guard_destroy 显式生命周期配对管理（destroy 置 false 后可
 * 再次 init，reinit 语义）；L86/L105/L121 的检查均为实例活性防御检查，
 * 非"进程级单例惰性初始化"，lv_once 不适用（每实例一个守卫），
 * 故保留手写标志检查，不迁移。 */
bool lv_guard_init(lvGuardContext *guard) {
    if (!guard) {
        return false;
    }

    /* 初始化读写锁和互斥锁 */
    if (!guard_init_locks(guard)) {
        return false;
    }

    /* 清零统计信息 */
    memset(&guard->stats, 0, sizeof(lvGuardStats));

    /* 标记为已初始化 */
    guard->initialized = true;

    return true;
}

void lv_guard_destroy(lvGuardContext *guard) {
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
    memset(&guard->stats, 0, sizeof(lvGuardStats));
}

void lv_guard_get_stats(const lvGuardContext *guard, lvGuardStats *stats) {
    if (!guard || !stats) {
        return;
    }

    if (!guard->initialized) {
        memset(stats, 0, sizeof(lvGuardStats));
        return;
    }

    /* 加锁拷贝统计信息，确保快照一致性 */
    guard_lock_stats(guard);
    memcpy(stats, &guard->stats, sizeof(lvGuardStats));
    guard_unlock_stats(guard);
}

void lv_guard_reset_stats(lvGuardContext *guard) {
    if (!guard) {
        return;
    }

    if (!guard->initialized) {
        return;
    }

    /* 加锁清零统计信息 */
    guard_lock_stats(guard);
    memset(&guard->stats, 0, sizeof(lvGuardStats));
    guard_unlock_stats(guard);
}

/* ========================================================================
 * 数据完整性校验
 *
 * 检查 lvContext 中关键数据结构的一致性：
 *   1. 上下文指针非空
 *   2. 上下文状态有效性（不超过 lv_CONTEXT_COMPLETE）
 *   3. 约束图主指针非空（若 state >= PARSING）
 *   4. 推理栈深度不超过上限
 *   5. 递归深度不超过上限
 *   6. 熔断器状态与上下文状态一致
 * ======================================================================== */

bool lv_verify_data_integrity(struct lvContext *ctx) {
    if (!ctx) {
        return false;
    }

    /* 检查 1：上下文状态有效性 */
    if (ctx->state < lv_CONTEXT_IDLE || ctx->state > lv_CONTEXT_COMPLETE) {
        return false;
    }

    /* 检查 2：若处于 PARSING 或更高状态，主图指针不应为 NULL */
    if (ctx->state >= lv_CONTEXT_PARSING && ctx->main_graph == NULL) {
        return false;
    }

    /* 检查 3：推理栈深度不超过上限 */
    if (ctx->reasoning_stack.top >= ctx->reasoning_stack.max_depth) {
        return false;
    }
    if (ctx->reasoning_stack.top >= 0 && ctx->reasoning_stack.top >= ctx->reasoning_stack.capacity) {
        return false;
    }

    /* 检查 4：递归深度不超过上限 */
    if (ctx->recursion_depth > ctx->max_recursion_depth) {
        return false;
    }
    if (ctx->max_recursion_depth > lv_CONTEXT_MAX_RECURSION_DEPTH) {
        return false;
    }

    /* 检查 5：熔断器状态一致性 */
    if (ctx->circuit_breaker.state < CIRCUIT_BREAKER_CLOSED || ctx->circuit_breaker.state > CIRCUIT_BREAKER_OPEN) {
        return false;
    }

    /* 若熔断器处于 OPEN 状态且上下文不在 ERROR 状态，则不一致 */
    if (ctx->circuit_breaker.state == CIRCUIT_BREAKER_OPEN && ctx->state != lv_CONTEXT_ERROR) {
        return false;
    }

    /* 检查 6：快照引用计数非负 */
    if (ctx->snapshot_refcount < 0) {
        return false;
    }

    /* 所有检查通过 */
    return true;
}

#endif /* lv_ENABLE_RUNTIME_GUARDS */
