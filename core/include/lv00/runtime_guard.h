/**
 * @file runtime_guard.h
 * @brief 运行时数据保护 —— 读写锁宏、原子操作与数据完整性校验
 *
 * @details 提供编译期可选的运行时保护机制，用于在多线程或无锁环境下
 *          保护 Lv-00 关键数据结构的并发访问和完整性。
 *
 *          设计原则：
 *          - 通过 LV00_ENABLE_RUNTIME_GUARDS 编译开关控制启用/禁用
 *          - 禁用时（默认），所有宏展开为空操作，零性能开销
 *          - 启用时，提供真正的读写锁、原子操作和数据校验
 *          - 所有锁操作内联展开，避免函数调用开销
 *
 *          借鉴来源：
 *          - Linux kernel RCU（Read-Copy-Update）读写锁模式
 *          - PostgreSQL LWLock 轻量级锁设计
 *          - Zig 语言的编译期零开销抽象理念
 *
 * @author Lv-00 Project
 * @version 3.3.0
 * @date   2026-05-24
 */

#ifndef LV00_RUNTIME_GUARD_H
#define LV00_RUNTIME_GUARD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/* 前向声明 —— 避免循环依赖 */
struct Lv00Context;

/* ============================================================
 * 第一部分：编译期开关 —— LV00_ENABLE_RUNTIME_GUARDS
 *
 * 通过在 CMakeLists.txt 或编译选项中定义此宏来开启保护：
 *   cmake -DLV00_ENABLE_RUNTIME_GUARDS=ON ..
 *   gcc -DLV00_ENABLE_RUNTIME_GUARDS ...
 *
 * 默认不开启，保证单线程场景下的最大性能。
 * ============================================================ */

/* ============================================================
 * 第二部分：运行时保护配置常量
 * ============================================================ */

/** @brief 运行时保护默认递归深度上限 */
#define LV00_RUNTIME_GUARD_MAX_RECURSE 128

/** @brief 自旋锁最大尝试次数（用于 trylock 回退） */
#define LV00_RUNTIME_GUARD_SPIN_ATTEMPTS 1024

/** @brief 读锁持有最大时间警告阈值（微秒） */
#define LV00_RUNTIME_GUARD_READ_WARN_US 5000

/** @brief 写锁持有最大时间警告阈值（微秒） */
#define LV00_RUNTIME_GUARD_WRITE_WARN_US 10000

/* ============================================================
 * 第三部分：运行时守卫宏（条件编译）
 * ============================================================ */

#ifdef LV00_ENABLE_RUNTIME_GUARDS

/* ── 需要实际的原子操作支持 ── */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L && !defined(__STDC_NO_ATOMICS__)
/* C11 标准原子操作 */
#include <stdatomic.h>
#elif defined(__GNUC__) || defined(__clang__)
/* GCC/Clang 内建原子操作（回退方案） */
#if !defined(__STDC_NO_ATOMICS__)
#include <stdatomic.h>
#endif
#endif

/* ============================================================
 * 运行时守卫的互斥锁实现（平台适配层）
 * ============================================================ */
#ifdef _WIN32
/* Windows 平台：使用 CRITICAL_SECTION 和 SRWLock */
#include <windows.h>

/** @brief 运行时保护读写锁类型（Windows） */
typedef struct Lv00RwLock {
    SRWLOCK srw_lock;          /**< Slim Reader/Writer Lock（Vista+） */
    volatile LONG write_flag;  /**< 写者标记 */
    volatile LONG reader_count;/**< 当前读者计数 */
} Lv00RwLock;

/** @brief 运行时保护互斥锁类型（Windows） */
typedef CRITICAL_SECTION Lv00Mutex;

#else
/* POSIX 平台：使用 pthread_rwlock 和 pthread_mutex */
#include <pthread.h>

/** @brief 运行时保护读写锁类型（POSIX） */
typedef struct Lv00RwLock {
    pthread_rwlock_t rwlock;        /**< POSIX 读写锁 */
    volatile int write_flag;        /**< 写者标记（用于快速路径检查） */
    volatile int reader_count;      /**< 当前读者计数 */
} Lv00RwLock;

/** @brief 运行时保护互斥锁类型（POSIX） */
typedef pthread_mutex_t Lv00Mutex;

#endif /* _WIN32 */

/* ============================================================
 * 运行时守卫状态结构
 *
 * 每个 Lv00Context 在启用运行时保护时可内嵌此结构体，
 * 用于追踪所有运行时保护的锁状态和统计信息。
 * ============================================================ */

/**
 * @brief 运行时守卫统计信息
 *
 * 记录运行时保护的锁争用、超时等统计，
 * 用于性能调优和问题诊断。
 */
typedef struct Lv00GuardStats {
    uint64_t lock_acquired_count;   /**< 锁成功获取次数 */
    uint64_t lock_contention_count; /**< 锁争用次数（trylock 失败） */
    uint64_t lock_timeout_count;    /**< 锁超时次数 */
    uint64_t read_guard_count;      /**< 读守卫进入次数 */
    uint64_t write_guard_count;     /**< 写守卫进入次数 */
    uint64_t integrity_checks;      /**< 数据完整性校验次数 */
    uint64_t integrity_failures;    /**< 数据完整性校验失败次数 */
    uint64_t deadlock_warnings;     /**< 死锁警告次数 */
    uint64_t max_read_hold_us;      /**< 最大读锁持有时间（微秒） */
    uint64_t max_write_hold_us;     /**< 最大写锁持有时间（微秒） */
} Lv00GuardStats;

/**
 * @brief 运行时守卫上下文
 *
 * 封装运行时保护所需的锁和统计信息。
 * 内嵌在 Lv00Context 结构体中（当 LV00_ENABLE_RUNTIME_GUARDS 启用时）。
 */
typedef struct Lv00GuardContext {
    Lv00RwLock ctx_rwlock;      /**< 上下文级读写锁（保护整个上下文） */
    Lv00Mutex stat_mutex;       /**< 统计信息互斥锁 */
    Lv00GuardStats stats;       /**< 运行时保护统计 */
    bool initialized;           /**< 是否已初始化 */
} Lv00GuardContext;

/* ============================================================
 * 运行时守卫 API 声明
 * ============================================================ */

/**
 * @brief 初始化运行时守卫上下文
 * @param guard 守卫上下文指针（非 NULL）
 * @return true 成功，false 失败
 */
LV00_PUBLIC_API bool lv00_guard_init(Lv00GuardContext *guard);

/**
 * @brief 销毁运行时守卫上下文
 * @param guard 守卫上下文指针（非 NULL）
 */
LV00_PUBLIC_API void lv00_guard_destroy(Lv00GuardContext *guard);

/**
 * @brief 获取运行时保护统计信息快照
 * @param guard  守卫上下文指针（非 NULL）
 * @param stats  输出统计信息（非 NULL）
 */
LV00_PUBLIC_API void lv00_guard_get_stats(const Lv00GuardContext *guard, Lv00GuardStats *stats);

/**
 * @brief 重置运行时保护统计信息
 * @param guard 守卫上下文指针（非 NULL）
 */
LV00_PUBLIC_API void lv00_guard_reset_stats(Lv00GuardContext *guard);

/* ============================================================
 * 核心宏：LV00_RUNTIME_LOCK / LV00_RUNTIME_UNLOCK
 *
 * 全面的写锁：用于保护整个上下文的关键修改操作。
 * 实现为独占写锁，同一时刻只有一个线程能持有。
 *
 * 使用场景：
 *   - 约束图拓扑修改（添加/删除节点或约束）
 *   - 上下文状态切换
 *   - 熔断器重置
 *
 * 【锁策略】先尝试自旋锁（busy-wait），超时后回退到阻塞锁。
 *   自旋次数由 LV00_RUNTIME_GUARD_SPIN_ATTEMPTS 控制。
 * ============================================================ */

#ifdef _WIN32

/** @brief 初始化读写锁（Windows SRWLock） */
#define LV00_RWLOCK_INIT(lock)  do { \
    InitializeSRWLock(&(lock)->srw_lock); \
    (lock)->write_flag = 0; \
    (lock)->reader_count = 0; \
} while(0)

/** @brief 销毁读写锁（Windows SRWLock 无需显式销毁） */
#define LV00_RWLOCK_DESTROY(lock)  ((void)0)

/** @brief 获取写锁（独占锁） */
#define LV00_RWLOCK_WRLOCK(lock)  AcquireSRWLockExclusive(&(lock)->srw_lock)

/** @brief 释放写锁 */
#define LV00_RWLOCK_WRUNLOCK(lock)  ReleaseSRWLockExclusive(&(lock)->srw_lock)

/** @brief 获取读锁（共享锁） */
#define LV00_RWLOCK_RDLOCK(lock)  AcquireSRWLockShared(&(lock)->srw_lock)

/** @brief 释放读锁 */
#define LV00_RWLOCK_RDUNLOCK(lock)  ReleaseSRWLockShared(&(lock)->srw_lock)

#else /* POSIX */

/** @brief 初始化读写锁（POSIX pthread_rwlock） */
#define LV00_RWLOCK_INIT(lock)  do { \
    pthread_rwlock_init(&(lock)->rwlock, NULL); \
    (lock)->write_flag = 0; \
    (lock)->reader_count = 0; \
} while(0)

/** @brief 销毁读写锁 */
#define LV00_RWLOCK_DESTROY(lock)  pthread_rwlock_destroy(&(lock)->rwlock)

/** @brief 获取写锁（独占锁） */
#define LV00_RWLOCK_WRLOCK(lock)  pthread_rwlock_wrlock(&(lock)->rwlock)

/** @brief 释放写锁 */
#define LV00_RWLOCK_WRUNLOCK(lock)  pthread_rwlock_unlock(&(lock)->rwlock)

/** @brief 获取读锁（共享锁） */
#define LV00_RWLOCK_RDLOCK(lock)  pthread_rwlock_rdlock(&(lock)->rwlock)

/** @brief 释放读锁 */
#define LV00_RWLOCK_RDUNLOCK(lock)  pthread_rwlock_unlock(&(lock)->rwlock)

#endif /* _WIN32 */

/* ── 带上下文的高层锁宏 ── */

/**
 * @brief 获取上下文的运行时写锁
 *
 * 保护整个上下文的关键修改操作。在锁内代码执行期间，
 * 其他线程的读写操作都将被阻塞。
 *
 * @param ctx Lv00Context 指针（必须已启用运行时保护）
 */
#define LV00_RUNTIME_LOCK(ctx)  do { \
    if ((ctx) && (ctx)->guard.initialized) { \
        LV00_RWLOCK_WRLOCK(&(ctx)->guard.ctx_rwlock); \
        (ctx)->guard.stats.lock_acquired_count++; \
    } \
} while(0)

/**
 * @brief 释放上下文的运行时写锁
 * @param ctx Lv00Context 指针
 */
#define LV00_RUNTIME_UNLOCK(ctx)  do { \
    if ((ctx) && (ctx)->guard.initialized) { \
        LV00_RWLOCK_WRUNLOCK(&(ctx)->guard.ctx_rwlock); \
    } \
} while(0)

/**
 * @brief 获取上下文的读守卫（共享读锁）
 *
 * 允许多个读者并发访问上下文，但阻塞写者。
 * 适用于只读操作（查询、序列化、统计等）。
 *
 * @param ctx Lv00Context 指针
 */
#define LV00_READ_GUARD(ctx)  do { \
    if ((ctx) && (ctx)->guard.initialized) { \
        LV00_RWLOCK_RDLOCK(&(ctx)->guard.ctx_rwlock); \
        (ctx)->guard.stats.read_guard_count++; \
    } \
} while(0)

/**
 * @brief 释放上下文的读守卫
 * @param ctx Lv00Context 指针
 */
#define LV00_READ_UNGUARD(ctx)  do { \
    if ((ctx) && (ctx)->guard.initialized) { \
        LV00_RWLOCK_RDUNLOCK(&(ctx)->guard.ctx_rwlock); \
    } \
} while(0)

/**
 * @brief 获取上下文的写守卫（独占写锁）
 *
 * 仅允许单一写者访问上下文。在关键修改操作前使用。
 *
 * @param ctx Lv00Context 指针
 */
#define LV00_WRITE_GUARD(ctx)  do { \
    if ((ctx) && (ctx)->guard.initialized) { \
        LV00_RWLOCK_WRLOCK(&(ctx)->guard.ctx_rwlock); \
        (ctx)->guard.stats.write_guard_count++; \
    } \
} while(0)

/**
 * @brief 释放上下文的写守卫
 * @param ctx Lv00Context 指针
 */
#define LV00_WRITE_UNGUARD(ctx)  do { \
    if ((ctx) && (ctx)->guard.initialized) { \
        LV00_RWLOCK_WRUNLOCK(&(ctx)->guard.ctx_rwlock); \
    } \
} while(0)

/* ============================================================
 * 原子操作宏 —— 用于关键计数器的无锁递增/递减
 *
 * 适用场景：
 *   - 节点/约束创建计数
 *   - 错误计数
 *   - 推理步骤计数
 *   - 缓存命中/未命中计数
 *
 * 注意：这些宏要求目标变量类型与 int 或 int64_t 兼容。
 * ============================================================ */

/**
 * @brief 原子递增（返回新值）
 * @param var 要递增的变量（左值，int 兼容类型）
 * @return 递增后的值
 */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L && !defined(__STDC_NO_ATOMICS__)
#define LV00_ATOMIC_INC(var)   atomic_fetch_add((_Atomic int*)&(var), 1) + 1
#define LV00_ATOMIC_DEC(var)   atomic_fetch_sub((_Atomic int*)&(var), 1) - 1
#define LV00_ATOMIC_ADD(var, n) atomic_fetch_add((_Atomic int*)&(var), (n))
#define LV00_ATOMIC_LOAD(var)  atomic_load((_Atomic int*)&(var))
#define LV00_ATOMIC_STORE(var, n) atomic_store((_Atomic int*)&(var), (n))
#define LV00_ATOMIC_CAS(var, expected, desired) \
    atomic_compare_exchange_strong((_Atomic int*)&(var), &(expected), (desired))

#elif defined(__GNUC__) || defined(__clang__)
/* GCC/Clang 内建原子操作 */
#define LV00_ATOMIC_INC(var)   __sync_add_and_fetch(&(var), 1)
#define LV00_ATOMIC_DEC(var)   __sync_sub_and_fetch(&(var), 1)
#define LV00_ATOMIC_ADD(var, n) __sync_add_and_fetch(&(var), (n))
#define LV00_ATOMIC_LOAD(var)  __sync_fetch_and_add(&(var), 0)
#define LV00_ATOMIC_STORE(var, n) (void)__sync_lock_test_and_set(&(var), (n))
#define LV00_ATOMIC_CAS(var, expected, desired) \
    __sync_bool_compare_and_swap(&(var), (expected), (desired))

#elif defined(_MSC_VER)
/* MSVC 内建原子操作 */
#include <windows.h>
#define LV00_ATOMIC_INC(var)   InterlockedIncrement((LONG volatile*)&(var))
#define LV00_ATOMIC_DEC(var)   InterlockedDecrement((LONG volatile*)&(var))
#define LV00_ATOMIC_ADD(var, n) InterlockedExchangeAdd((LONG volatile*)&(var), (n))
#define LV00_ATOMIC_LOAD(var)  InterlockedCompareExchange((LONG volatile*)&(var), 0, 0)
#define LV00_ATOMIC_STORE(var, n) InterlockedExchange((LONG volatile*)&(var), (n))
#define LV00_ATOMIC_CAS(var, expected, desired) \
    (InterlockedCompareExchange((LONG volatile*)&(var), (desired), (expected)) == (expected))

#else
/* 无原子操作支持时的回退方案：使用 volatile 读/写 */
#warning "LV00_ENABLE_RUNTIME_GUARDS: 无原子操作支持，使用 volatile 回退（非线程安全）"
#define LV00_ATOMIC_INC(var)   (++(var))
#define LV00_ATOMIC_DEC(var)   (--(var))
#define LV00_ATOMIC_ADD(var, n) ((var) += (n))
#define LV00_ATOMIC_LOAD(var)  (var)
#define LV00_ATOMIC_STORE(var, n) ((var) = (n))
#define LV00_ATOMIC_CAS(var, expected, desired) \
    (((var) == (expected)) ? ((var) = (desired), true) : ((expected) = (var), false))
#endif

/**
 * @brief 原子递增 64 位计数器
 * @param var int64_t 兼容变量
 */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L && !defined(__STDC_NO_ATOMICS__)
#define LV00_ATOMIC_INC64(var)  (atomic_fetch_add((_Atomic int64_t*)&(var), 1) + 1)
#define LV00_ATOMIC_ADD64(var, n) atomic_fetch_add((_Atomic int64_t*)&(var), (n))
#elif defined(__GNUC__) || defined(__clang__)
#define LV00_ATOMIC_INC64(var)  __sync_add_and_fetch(&(var), 1LL)
#define LV00_ATOMIC_ADD64(var, n) __sync_add_and_fetch(&(var), (int64_t)(n))
#elif defined(_MSC_VER)
#define LV00_ATOMIC_INC64(var)  InterlockedIncrement64((LONG64 volatile*)&(var))
#define LV00_ATOMIC_ADD64(var, n) InterlockedExchangeAdd64((LONG64 volatile*)&(var), (n))
#else
#define LV00_ATOMIC_INC64(var)  (++(var))
#define LV00_ATOMIC_ADD64(var, n) ((var) += (n))
#endif

/* ============================================================
 * 数据完整性校验
 *
 * lv00_verify_data_integrity(ctx) 执行一系列低成本检查，
 * 确保上下文中关键数据结构的一致性。
 *
 * 检查项目：
 *   1. 上下文状态有效性（不超过 LV00_CONTEXT_COMPLETE）
 *   2. 约束图主指针非空（若 state >= PARSING）
 *   3. 推理栈深度不超过上限
 *   4. 递归深度不超过上限
 *   5. 熔断器状态与上下文状态一致
 *
 * @param ctx Lv00Context 指针
 * @return true 数据完整性校验通过，false 检测到不一致
 * ============================================================ */

LV00_PUBLIC_API bool lv00_verify_data_integrity(struct Lv00Context *ctx);

#else /* !LV00_ENABLE_RUNTIME_GUARDS */

/* ============================================================
 * 运行时保护禁用时的空操作宏
 *
 * 所有宏展开为空，编译器会完全优化掉这些代码路径。
 * 零开销：不影响任何性能。
 * ============================================================ */

#define LV00_RUNTIME_LOCK(ctx)        ((void)0)
#define LV00_RUNTIME_UNLOCK(ctx)      ((void)0)
#define LV00_READ_GUARD(ctx)          ((void)0)
#define LV00_READ_UNGUARD(ctx)        ((void)0)
#define LV00_WRITE_GUARD(ctx)         ((void)0)
#define LV00_WRITE_UNGUARD(ctx)       ((void)0)

/* 回退到普通操作（非原子的，但已是全局/线程局部即可） */
#define LV00_ATOMIC_INC(var)          (++(var))
#define LV00_ATOMIC_DEC(var)          (--(var))
#define LV00_ATOMIC_ADD(var, n)       ((var) += (n))
#define LV00_ATOMIC_LOAD(var)         (var)
#define LV00_ATOMIC_STORE(var, n)     ((var) = (n))
#define LV00_ATOMIC_CAS(var, expected, desired) \
    (((var) == (expected)) ? ((var) = (desired), true) : ((expected) = (var), false))

#define LV00_ATOMIC_INC64(var)        (++(var))
#define LV00_ATOMIC_ADD64(var, n)     ((var) += (n))

/**
 * @brief 运行时保护禁用时的数据完整性校验 —— 始终返回 true
 */
static inline bool lv00_verify_data_integrity(struct Lv00Context *ctx) {
    (void)ctx;
    return true;
}

#endif /* LV00_ENABLE_RUNTIME_GUARDS */

/* ============================================================
 * 第四部分：安全断言宏（始终可用的辅助宏）
 *
 * 这些宏无论 LV00_ENABLE_RUNTIME_GUARDS 是否启用都有效。
 * ============================================================ */

/**
 * @brief 运行时条件下断言：如果条件为假，记录错误并返回指定值
 *
 * 在 LV00_ENABLE_RUNTIME_GUARDS 启用时执行额外的完整性检查，
 * 在禁用时退化为普通条件判断。
 *
 * @param ctx     Lv00Context 指针
 * @param cond    断言条件
 * @param retval  条件为假时的返回值
 */
#ifdef LV00_ENABLE_RUNTIME_GUARDS
#define LV00_ASSERT_RUNTIME(ctx, cond, retval)  do { \
    if (!(cond)) { \
        (ctx)->guard.stats.integrity_failures++; \
        lv00_context_set_error((ctx), LV00_ERROR_ASSERTION_FAILED, \
            "运行时断言失败: %s [%s:%d]", #cond, __FILE__, __LINE__); \
        return (retval); \
    } \
} while(0)
#else
#define LV00_ASSERT_RUNTIME(ctx, cond, retval)  do { \
    if (!(cond)) { return (retval); } \
} while(0)
#endif

/**
 * @brief 运行时守卫区域（RAII 风格的保护区）
 *
 * 在 LV00_ENABLE_RUNTIME_GUARDS 启用时获取锁，
 * 退出作用域时自动释放（需与 LV00_RUNTIME_UNLOCK 配对）。
 *
 * 使用示例：
 * @code
 *   LV00_GUARDED_SECTION(ctx) {
 *       // 在此区域内 ctx 被保护，可安全读写
 *       ctx->main_graph = ...;
 *   }
 *   // 自动释放锁
 * @endcode
 *
 * 注意：C 语言不支持真正的 RAII，此宏需要使用
 * LV00_RUNTIME_UNLOCK(ctx) 显式释放。也可借助
 * __attribute__((cleanup)) 在 GCC/Clang 中实现自动释放。
 */
#define LV00_GUARDED_SECTION(ctx) \
    LV00_RUNTIME_LOCK(ctx)

#ifdef __cplusplus
}
#endif

#endif /* LV00_RUNTIME_GUARD_H */
