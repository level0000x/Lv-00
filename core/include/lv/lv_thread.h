/**
 * @file lv_thread.h
 * @brief 跨平台线程抽象层 —— 互斥锁、条件变量、线程创建、一次性初始化
 *
 * @details 统一封装 pthread (POSIX) 和 Win32 API (CRITICAL_SECTION / CONDITION_VARIABLE /
 *          CreateThread) 的差异，提供一致的 C 接口。所有实现为 static inline，
 *          零函数调用开销。
 *
 *          解决的问题：
 *          - thread_pool.c, test_framework.c, runtime_monitor.c 三个文件重复定义了
 *            几乎相同的 MUTEX_INIT/LOCK/UNLOCK/DESTROY 平台宏
 *          - stream.c 重复实现了 lv_mutex_create/destroy/lock/unlock 函数
 *          - interop_server.c, formula_renderer.c 各自封装 stdout/formula pool 锁
 *          - 缺乏统一的线程创建/等待抽象（pthread_create vs CreateThread）
 *
 * 使用方式：
 *   #include "lv/lv_thread.h"
 *
 *   lv_mutex_t lock;
 *   lv_mutex_init(&lock);
 *   lv_mutex_lock(&lock);
 *   // ... 临界区 ...
 *   lv_mutex_unlock(&lock);
 *   lv_mutex_destroy(&lock);
 *
 * @attention
 *   本文件不包含运行时保护（见 runtime_guard.h）或线程池（见 thread_pool.h）。
 *   只提供基础的跨平台线程原语。
 *
 * @version 1.0.0
 * @date   2026-07-30
 */

#ifndef lv_THREAD_H
#define lv_THREAD_H

#include "lv/lv_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════
 * 第 1 节：平台类型定义
 * ═══════════════════════════════════════════════════════════════════ */

#ifdef _WIN32
#include <windows.h>
#include <process.h> /* _beginthreadex */

/* lv_mutex_t 由 lv_platform.h 定义（本头文件已包含） */
typedef CONDITION_VARIABLE lv_cond_t;

/** Windows 下线程句柄类型（可用于 WaitForSingleObject / CloseHandle） */
typedef HANDLE lv_thread_t;

#else /* POSIX */
#include <pthread.h>

typedef pthread_cond_t  lv_cond_t;
typedef pthread_t       lv_thread_t;

#endif /* _WIN32 */

/* ═══════════════════════════════════════════════════════════════════
 * 第 2 节：互斥锁（Mutex）
 * ═══════════════════════════════════════════════════════════════════ */

/** @brief 初始化互斥锁 */
static inline void lv_mutex_init(lv_mutex_t *m) {
#ifdef _WIN32
    InitializeCriticalSection(m);
#else
    pthread_mutex_init(m, NULL);
#endif
}

/** @brief 销毁互斥锁 */
static inline void lv_mutex_destroy(lv_mutex_t *m) {
#ifdef _WIN32
    DeleteCriticalSection(m);
#else
    pthread_mutex_destroy(m);
#endif
}

/** @brief 获取互斥锁（阻塞） */
static inline void lv_mutex_lock(lv_mutex_t *m) {
#ifdef _WIN32
    EnterCriticalSection(m);
#else
    pthread_mutex_lock(m);
#endif
}

/** @brief 释放互斥锁 */
static inline void lv_mutex_unlock(lv_mutex_t *m) {
#ifdef _WIN32
    LeaveCriticalSection(m);
#else
    pthread_mutex_unlock(m);
#endif
}

/* ═══════════════════════════════════════════════════════════════════
 * 第 2b 节：锁守卫（Lock Guard）
 *
 * 提供 RAII 风格的锁管理，通过 goto cleanup 模式确保所有退出路径
 * 都会释放锁，避免遗漏 lv_mutex_unlock 调用。
 *
 * 使用方式：
 *   lvLockGuard _lg;
 *   lv_lock_guard_init(&_lg, &g_data_mutex);
 *   if (error)
 *       goto cleanup;
 *   // work
 * cleanup:
 *   lv_lock_guard_destroy(&_lg);
 *   return result;
 * ═══════════════════════════════════════════════════════════════════ */

/** @brief 锁守卫结构体 */
typedef struct { lv_mutex_t *mutex; } lvLockGuard;

/** @brief 初始化锁守卫（自动加锁） */
static inline void lv_lock_guard_init(lvLockGuard *g, lv_mutex_t *m) {
    g->mutex = m;
    lv_mutex_lock(m);
}

/** @brief 销毁锁守卫（自动解锁） */
static inline void lv_lock_guard_destroy(lvLockGuard *g) {
    if (g->mutex) {
        lv_mutex_unlock(g->mutex);
        g->mutex = NULL;
    }
}

/** @brief 锁守卫作用域清理回调（配合 LV_SCOPE_LOCK 的 cleanup 属性使用） */
static inline void lv_lock_guard_scope_cleanup(void *p) {
    lv_lock_guard_destroy((lvLockGuard *) p);
}

#if defined(__GNUC__) || defined(__clang__)
/**
 * @brief 作用域锁守卫：进入作用域加锁，离开作用域（含任意 return / goto）自动解锁
 *
 * 消除手写 lv_mutex_lock/unlock 配对（尤其多分支 return 函数中漏解锁的隐患）。
 * 用法：
 *   LV_SCOPE_LOCK(&g_mutex);
 *   if (error)
 *       return NULL;   // 自动解锁
 *
 * 条件加锁（可能不加锁）用法：
 *   lvLockGuard _guard __attribute__((cleanup(lv_lock_guard_scope_cleanup))) = {NULL};
 *   if (need_lock)
 *       lv_lock_guard_init(&_guard, &mutex);   // 离开作用域时若未加锁则 no-op
 */
#define LV_SCOPE_LOCK(mutex)                                                     \
    lvLockGuard _lv_scope_guard_ __attribute__((cleanup(lv_lock_guard_scope_cleanup))) = {(mutex)}; \
    lv_mutex_lock((mutex))
#else
/* MSVC 不支持 cleanup 属性：退化为手动配对，离开作用域不自动解锁，需配合 goto cleanup */
#define LV_SCOPE_LOCK(mutex) lvLockGuard _lv_scope_guard_; lv_lock_guard_init(&_lv_scope_guard_, (mutex))
#endif

/* ═══════════════════════════════════════════════════════════════════
 * 第 3 节：条件变量（Condition Variable）
 * ═══════════════════════════════════════════════════════════════════ */

/** @brief 初始化条件变量 */
static inline void lv_cond_init(lv_cond_t *cv) {
#ifdef _WIN32
    InitializeConditionVariable(cv);
#else
    pthread_cond_init(cv, NULL);
#endif
}

/** @brief 销毁条件变量 */
static inline void lv_cond_destroy(lv_cond_t *cv) {
#ifdef _WIN32
    /* Windows CONDITION_VARIABLE 无需显式销毁 */
    (void)cv;
#else
    pthread_cond_destroy(cv);
#endif
}

/** @brief 唤醒一个等待线程 */
static inline void lv_cond_signal(lv_cond_t *cv) {
#ifdef _WIN32
    WakeConditionVariable(cv);
#else
    pthread_cond_signal(cv);
#endif
}

/** @brief 唤醒所有等待线程 */
static inline void lv_cond_broadcast(lv_cond_t *cv) {
#ifdef _WIN32
    WakeAllConditionVariable(cv);
#else
    pthread_cond_broadcast(cv);
#endif
}

/** @brief 在条件变量上等待（调用前必须已持有 mutex） */
static inline void lv_cond_wait(lv_cond_t *cv, lv_mutex_t *m) {
#ifdef _WIN32
    SleepConditionVariableCS(cv, m, INFINITE);
#else
    pthread_cond_wait(cv, m);
#endif
}

/**
 * @brief 在条件变量上带超时等待（毫秒）
 * @param cv 条件变量
 * @param m  互斥锁（调用前必须已持有）
 * @param timeout_ms 超时毫秒
 * @return 0 表示被唤醒，非 0 表示超时
 */
static inline int lv_cond_timedwait(lv_cond_t *cv, lv_mutex_t *m, unsigned int timeout_ms) {
#ifdef _WIN32
    if (!SleepConditionVariableCS(cv, m, (DWORD)timeout_ms))
        return -1; /* 超时 */
    return 0;
#else
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += timeout_ms / 1000;
    ts.tv_nsec += (long)(timeout_ms % 1000) * 1000000L;
    if (ts.tv_nsec >= 1000000000L) {
        ts.tv_sec++;
        ts.tv_nsec -= 1000000000L;
    }
    return pthread_cond_timedwait(cv, m, &ts);
#endif
}

/* ═══════════════════════════════════════════════════════════════════
 * 第 4 节：线程创建与管理
 *
 * 线程函数签名为 POSIX 风格：void *func(void *arg)
 * Windows 版本内部通过包装函数适配 DWORD WINAPI 调用约定。
 * ═══════════════════════════════════════════════════════════════════ */

#ifdef _WIN32
/** Windows 线程包装结构 */
typedef struct {
    void *(*func)(void *);
    void *arg;
} lv_thread_pack_t;

/** Windows 线程入口包装（将 void* 函数签名适配为 DWORD WINAPI） */
static DWORD WINAPI lv_thread_entry(LPVOID param) {
    lv_thread_pack_t *pack = (lv_thread_pack_t *)param;
    void *(*f)(void *) = pack->func;
    void *a = pack->arg;
    free(pack);
    f(a);
    return 0;
}
#endif

/**
 * @brief 创建线程
 * @param thread 输出线程句柄
 * @param func   线程入口函数，签名 void*(*)(void*)
 * @param arg    传递给 func 的参数
 * @return 0 成功，非 0 失败
 */
static inline int lv_thread_create(lv_thread_t *thread, void *(*func)(void *), void *arg) {
#ifdef _WIN32
    lv_thread_pack_t *pack = (lv_thread_pack_t *)malloc(sizeof(lv_thread_pack_t));
    if (!pack) return -1;
    pack->func = func;
    pack->arg = arg;
    /* _beginthreadex 比 CreateThread 更安全：正确初始化 CRT */
    uintptr_t h = _beginthreadex(NULL, 0, lv_thread_entry, pack, 0, NULL);
    if (h == 0) {
        free(pack);
        return -1;
    }
    *thread = (HANDLE)h;
    return 0;
#else
    return pthread_create(thread, NULL, func, arg);
#endif
}

/**
 * @brief 等待线程结束
 * @param thread 线程句柄
 * @return 0 成功
 */
static inline int lv_thread_join(lv_thread_t thread) {
#ifdef _WIN32
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
    return 0;
#else
    return pthread_join(thread, NULL);
#endif
}

/**
 * @brief 分离线程（使其资源在线程退出时自动回收）
 * @param thread 线程句柄
 * @return 0 成功
 */
static inline int lv_thread_detach(lv_thread_t thread) {
#ifdef _WIN32
    CloseHandle(thread);
    return 0;
#else
    return pthread_detach(thread);
#endif
}

/**
 * @brief 获取当前线程 ID（用于日志和调试）
 * @return 线程 ID（平台相关类型转为 unsigned long）
 */
static inline unsigned long lv_thread_id(void) {
#ifdef _WIN32
    return (unsigned long)GetCurrentThreadId();
#else
    return (unsigned long)pthread_self();
#endif
}

/**
 * @brief 线程休眠（毫秒）
 * @param ms 休眠毫秒数
 */
static inline void lv_thread_sleep(unsigned int ms) {
#ifdef _WIN32
    Sleep(ms);
#else
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
#endif
}

/* ═══════════════════════════════════════════════════════════════════
 * 第 5 节：一次性初始化（Once）
 *
 * 线程安全的一次性初始化惯用法，替代手动 static int + mutex 模式。
 * runtime_monitor.c 和 test_framework.c 中有 7 处手动实现的
 * *_init_mutex + *_initialized 模式，均可简化为 lv_once_t。
 * ═══════════════════════════════════════════════════════════════════ */

#ifdef _WIN32
typedef INIT_ONCE lv_once_t;
#define lv_ONCE_INIT INIT_ONCE_STATIC_INIT

static BOOL CALLBACK lv_once_callback(PINIT_ONCE once, PVOID param, PVOID *context) {
    (void)once;
    (void)context;
    void (*init_func)(void) = (void (*)(void))param;
    init_func();
    return TRUE;
}

/** @brief 线程安全的一次性初始化（Windows InitOnceExecuteOnce） */
static inline void lv_once(lv_once_t *once, void (*init_func)(void)) {
    InitOnceExecuteOnce(once, lv_once_callback, (PVOID)init_func, NULL);
}

#else
typedef pthread_once_t lv_once_t;
#define lv_ONCE_INIT PTHREAD_ONCE_INIT

/** @brief 线程安全的一次性初始化（POSIX pthread_once） */
static inline void lv_once(lv_once_t *once, void (*init_func)(void)) {
    pthread_once(once, init_func);
}
#endif

/* ═══════════════════════════════════════════════════════════════════
 * 第 6 节：惰性互斥锁（Lazy Mutex）
 *
 * 消除「static lv_once_t g_x_once = lv_ONCE_INIT;
 *          static void x_init(void){ lv_mutex_init(&g_x_mutex); }
 *          lv_once(&g_x_once, x_init); lv_mutex_lock(&g_x_mutex);」
 * 的样板：将 once 初始化守卫与互斥锁捆绑为 lv_lazy_lock 对象，
 * 首次 lock（或显式 lv_lazy_lock_init）时自动完成互斥锁初始化，
 * 且仅初始化一次、初始化 happens-before 后续所有加锁。
 *
 * 使用方式（独立静态锁，声明宏自动生成 once 回调）：
 *   lv_LAZY_LOCK_DEFINE(g_log_lock);
 *   lv_lazy_lock_lock(&g_log_lock, g_log_lock_init_once);  // 首次调用自动初始化
 *   // ... 临界区 ...
 *   lv_lazy_lock_unlock(&g_log_lock);
 *
 * 使用方式（结构体内嵌锁 / 自定义 once 回调）：
 *   static struct { ...; lv_lazy_lock lock; } s_state = {0};
 *   static void state_lock_init_once(void){ lv_mutex_init(&s_state.lock.mutex); }
 *   lv_lazy_lock_lock(&s_state.lock, state_lock_init_once);
 *
 * 说明：
 *   - 初始化回调由调用方显式传入（无需闭包/上下文绑定），线程安全、无竞态。
 *   - lv_LAZY_LOCK_DEFINE 仅适用于「只初始化互斥锁」的纯惰性锁场景。
 *   - 销毁语义与 lv_mutex_destroy 一致；销毁后不可再使用（once 不可重置）。
 * ═══════════════════════════════════════════════════════════════════ */

/** @brief 惰性互斥锁：once 初始化守卫 + 实际互斥锁 */
typedef struct {
    lv_once_t  once;   /**< 一次性初始化守卫 */
    lv_mutex_t mutex;  /**< 实际互斥锁 */
} lv_lazy_lock;

/** @brief 惰性锁静态初始化器（once 零值与 lv_ONCE_INIT 等价，见 lv_once 注释惯例） */
#define lv_LAZY_LOCK_INIT { lv_ONCE_INIT, {0} }

/**
 * @brief 声明独立静态惰性锁（自动生成 once 初始化回调 name##_init_once）
 *
 * 等价于：
 *   static lv_lazy_lock name;
 *   static void name##_init_once(void){ lv_mutex_init(&(name).mutex); }
 *   static lv_lazy_lock name = lv_LAZY_LOCK_INIT;
 * 使用时配合 lv_lazy_lock_lock(&name, name##_init_once)。
 */
#define lv_LAZY_LOCK_DEFINE(name)                                         \
    static lv_lazy_lock name;                                             \
    static void name##_init_once(void) { lv_mutex_init(&(name).mutex); }  \
    static lv_lazy_lock name = lv_LAZY_LOCK_INIT

/** @brief 立即执行惰性锁的一次性初始化（等价于首次 lock 的效果；可安全多次调用） */
static inline void lv_lazy_lock_init(lv_lazy_lock *ll, void (*init_once)(void)) {
    lv_once(&ll->once, init_once);
}

/** @brief 获取惰性锁（阻塞；首次调用时自动完成互斥锁初始化） */
static inline void lv_lazy_lock_lock(lv_lazy_lock *ll, void (*init_once)(void)) {
    lv_once(&ll->once, init_once);
    lv_mutex_lock(&ll->mutex);
}

/** @brief 释放惰性锁 */
static inline void lv_lazy_lock_unlock(lv_lazy_lock *ll) {
    lv_mutex_unlock(&ll->mutex);
}

/** @brief 销毁惰性锁的互斥锁（与 lv_mutex_destroy 语义一致；销毁后不可再用） */
static inline void lv_lazy_lock_destroy(lv_lazy_lock *ll) {
    lv_mutex_destroy(&ll->mutex);
}

#ifdef __cplusplus
}
#endif

#endif /* lv_THREAD_H */
