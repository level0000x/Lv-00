/**
 * @file thread_pool.c
 * @brief 线程池实现
 *
 * 提供工作线程的创建/销毁、任务队列管理和任务提交功能。
 * 支持等待组 (WaitGroup) 机制，允许提交一组任务后统一等待完成。
 *
 * @version 1.0.0
 *
 * @author Lv-00 Project
 */

#include "lv/lv_platform.h"

#define lv_THREAD_POOL_IMPL
#include "lv/thread_pool.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ========================================================================
 * 平台相关线程抽象
 * ======================================================================== */
#ifdef _WIN32
#include <windows.h>
typedef HANDLE lvThread;
typedef CRITICAL_SECTION lvMutex;
typedef CONDITION_VARIABLE lvCondVar;
#define MUTEX_INIT(m) InitializeCriticalSection(&(m))
#define MUTEX_LOCK(m) EnterCriticalSection(&(m))
#define MUTEX_UNLOCK(m) LeaveCriticalSection(&(m))
#define MUTEX_DESTROY(m) DeleteCriticalSection(&(m))
#define COND_INIT(c) InitializeConditionVariable(&(c))
#define COND_WAIT(c, m) SleepConditionVariableCS(&(c), &(m), INFINITE)
#define COND_SIGNAL(c) WakeConditionVariable(&(c))
#define COND_BROADCAST(c) WakeAllConditionVariable(&(c))
#else
#include <pthread.h>
typedef pthread_t lvThread;
typedef pthread_mutex_t lvMutex;
typedef pthread_cond_t lvCondVar;
#define MUTEX_INIT(m) pthread_mutex_init(&(m), NULL)
#define MUTEX_LOCK(m) pthread_mutex_lock(&(m))
#define MUTEX_UNLOCK(m) pthread_mutex_unlock(&(m))
#define MUTEX_DESTROY(m) pthread_mutex_destroy(&(m))
#define COND_INIT(c) pthread_cond_init(&(c), NULL)
#define COND_WAIT(c, m) pthread_cond_wait(&(c), &(m))
#define COND_SIGNAL(c) pthread_cond_signal(&(c))
#define COND_BROADCAST(c) pthread_cond_broadcast(&(c))
#endif

/* ========================================================================
 * 内部常量与数据结构
 * ======================================================================== */
#define MAX_TASK_QUEUE 4096 /**< 最大任务队列长度 */
#define DEFAULT_THREADS 4   /**< 默认工作线程数 */

/** 任务节点（链表） */
struct lvThreadTask {
    void (*func)(void *arg);   /**< 任务函数 */
    void *arg;                 /**< 任务参数 */
    lvWaitGroup *group;        /**< 所属等待组（可为 NULL） */
    struct lvThreadTask *next; /**< 下一个任务 */
};

/** 等待组 */
struct lvWaitGroup {
    int pending;         /**< 待完成任务数 */
    int completed_count; /**< 已完成任务数（支持超时查询） */
    lvMutex mutex;       /**< 保护互斥锁 */
    lvCondVar cond;      /**< 等待条件变量 */
};

/** 线程池 */
struct lvThreadPool {
    lvThread *threads; /**< 工作线程句柄数组 */
    int thread_count;  /**< 工作线程数 */

    /* 任务队列（链表） */
    lvThreadTask *queue_head; /**< 队列头 */
    lvThreadTask *queue_tail; /**< 队列尾 */
    int queue_size;           /**< 当前队列长度 */

    lvMutex mutex;       /**< 队列保护互斥锁 */
    lvCondVar not_empty; /**< 队列非空条件 */
    int shutdown;        /**< 关闭标志 */
};

/* ========================================================================
 * 全局线程池实例
 * ======================================================================== */
static lvThreadPool *g_global_pool = NULL;

/* ========================================================================
 * 工作线程主函数
 * ======================================================================== */
#ifdef _WIN32
static DWORD WINAPI worker_func(LPVOID arg)
#else
static void *worker_func(void *arg)
#endif
{
    lvThreadPool *pool = (lvThreadPool *) arg;

    for (;;) {
        MUTEX_LOCK(pool->mutex);

        /* 等待队列非空或关闭信号 */
        while (pool->queue_size == 0 && !pool->shutdown) {
            COND_WAIT(pool->not_empty, pool->mutex);
        }

        if (pool->shutdown && pool->queue_size == 0) {
            MUTEX_UNLOCK(pool->mutex);
            break;
        }

        /* 取出队列头任务 */
        lvThreadTask *task = pool->queue_head;
        if (task != NULL) {
            pool->queue_head = task->next;
            if (pool->queue_head == NULL) {
                pool->queue_tail = NULL;
            }
            pool->queue_size--;
        }

        MUTEX_UNLOCK(pool->mutex);

        /* 执行任务 */
        if (task != NULL) {
            if (task->func != NULL) {
                task->func(task->arg);
            }
            /* 通知等待组 */
            if (task->group != NULL) {
                MUTEX_LOCK(task->group->mutex);
                task->group->pending--;
                task->group->completed_count++;
                if (task->group->pending <= 0) {
                    COND_SIGNAL(task->group->cond);
                }
                MUTEX_UNLOCK(task->group->mutex);
            }
            free(task);
        }
    }

#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

/* ---- 前向声明 ---- */
void lv_thread_pool_destroy(lvThreadPool *pool);

/* ========================================================================
 * 线程池 API 实现
 * ======================================================================== */

/**
 * @brief 创建线程池
 * @param num_threads 工作线程数（<=0 时使用默认值 4）
 * @return 线程池（调用者通过 lv_thread_pool_destroy 释放），失败返回 NULL
 */
lvThreadPool *lv_thread_pool_create(int num_threads) {
    if (num_threads <= 0) {
        num_threads = DEFAULT_THREADS;
    }

    lvThreadPool *pool = (lvThreadPool *) calloc(1, sizeof(lvThreadPool));
    if (pool == NULL)
        return NULL;

    pool->thread_count = num_threads;
    pool->threads = (lvThread *) calloc((size_t) num_threads, sizeof(lvThread));
    if (pool->threads == NULL) {
        free(pool);
        return NULL;
    }

    MUTEX_INIT(pool->mutex);
    COND_INIT(pool->not_empty);
    pool->shutdown = 0;
    pool->queue_head = NULL;
    pool->queue_tail = NULL;
    pool->queue_size = 0;

    /* 创建工作线程 */
    for (int i = 0; i < num_threads; i++) {
#ifdef _WIN32
        pool->threads[i] = CreateThread(NULL, 0, worker_func, pool, 0, NULL);
        if (!pool->threads[i]) {
            /* [安全] 线程创建失败，清理已创建线程后返回 NULL */
            pool->thread_count = i; /* 只清理已成功创建的线程 */
            lv_thread_pool_destroy(pool);
            return NULL;
        }
#else
        if (pthread_create(&pool->threads[i], NULL, worker_func, pool) != 0) {
            pool->thread_count = i;
            lv_thread_pool_destroy(pool);
            return NULL;
        }
#endif
    }

    return pool;
}

/**
 * @brief 销毁线程池，等待所有工作线程结束并释放资源
 * @param pool 线程池指针（可为 NULL）
 */
void lv_thread_pool_destroy(lvThreadPool *pool) {
    if (pool == NULL)
        return;

    /* 通知所有工作线程退出 */
    MUTEX_LOCK(pool->mutex);
    pool->shutdown = 1;
    COND_BROADCAST(pool->not_empty);
    MUTEX_UNLOCK(pool->mutex);

    /* 等待所有线程结束 */
    for (int i = 0; i < pool->thread_count; i++) {
#ifdef _WIN32
        WaitForSingleObject(pool->threads[i], INFINITE);
        CloseHandle(pool->threads[i]);
#else
        pthread_join(pool->threads[i], NULL);
#endif
    }

    /* 释放残留任务 */
    lvThreadTask *task = pool->queue_head;
    while (task != NULL) {
        lvThreadTask *next = task->next;
        free(task);
        task = next;
    }

    MUTEX_DESTROY(pool->mutex);
    free(pool->threads);
    free(pool);
}

/**
 * @brief 提交任务到线程池
 * @details 创建等待组并将任务加入队列。调用者需通过 lv_thread_pool_wait_group 等待完成。
 * @param pool 线程池指针
 * @param task 任务节点（由调用者分配，线程池会在执行后 free）
 * @return 等待组指针（调用者传入 lv_thread_pool_wait_group），失败返回 NULL
 */
lvWaitGroup *lv_thread_pool_submit(lvThreadPool *pool, lvThreadTask *task) {
    if (pool == NULL || task == NULL)
        return NULL;

    /* 创建等待组 */
    lvWaitGroup *group = (lvWaitGroup *) calloc(1, sizeof(lvWaitGroup));
    if (group == NULL)
        return NULL;
    MUTEX_INIT(group->mutex);
    COND_INIT(group->cond);
    group->pending = 1;
    group->completed_count = 0;

    task->group = group;
    task->next = NULL;

    /* 入队 */
    MUTEX_LOCK(pool->mutex);

    if (pool->queue_size >= MAX_TASK_QUEUE) {
        MUTEX_UNLOCK(pool->mutex);
        free(group);
        return NULL;
    }

    if (pool->queue_tail != NULL) {
        pool->queue_tail->next = task;
    } else {
        pool->queue_head = task;
    }
    pool->queue_tail = task;
    pool->queue_size++;

    COND_SIGNAL(pool->not_empty);
    MUTEX_UNLOCK(pool->mutex);

    return group;
}

/**
 * @brief 等待一组任务完成并释放等待组
 * @param pool      线程池指针（当前未使用，保留为将来扩展）
 * @param group     等待组指针（全部完成后内部自动释放；超时后不释放，调用者可检查 group->pending）
 * @param timeout_ms 超时毫秒（<0 无限等待，==0 非阻塞检查立即返回，>0 等待指定毫秒）
 */
void lv_thread_pool_wait_group(lvThreadPool *pool, lvWaitGroup *group, int timeout_ms) {
    (void) pool;
    if (group == NULL)
        return;

    MUTEX_LOCK(group->mutex);

    if (timeout_ms < 0) {
        /* 无限等待，直到所有任务完成 */
        while (group->pending > 0) {
            COND_WAIT(group->cond, group->mutex);
        }
        MUTEX_UNLOCK(group->mutex);
        MUTEX_DESTROY(group->mutex);
        free(group);
    } else if (timeout_ms == 0) {
        /* 非阻塞检查：立即返回，不等待 */
        /* pending > 0 表示任务未完成，调用者可自行检查 group->pending */
        MUTEX_UNLOCK(group->mutex);
        /* 不销毁、不释放，调用者可重试 */
    } else {
        /* 带超时等待 */
#ifdef _WIN32
        while (group->pending > 0) {
            if (SleepConditionVariableCS(&group->cond, &group->mutex, (DWORD) timeout_ms) == 0) {
                /* 超时：不再等待，保留 group 供调用者检查 */
                break;
            }
        }
#else
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += timeout_ms / 1000;
        ts.tv_nsec += (long) (timeout_ms % 1000) * 1000000L;
        if (ts.tv_nsec >= 1000000000L) {
            ts.tv_sec++;
            ts.tv_nsec -= 1000000000L;
        }
        while (group->pending > 0) {
            if (pthread_cond_timedwait(&group->cond, &group->mutex, &ts) != 0) {
                /* 超时：不再等待，保留 group 供调用者检查 */
                break;
            }
        }
#endif

        if (group->pending <= 0) {
            /* 所有任务已完成 */
            MUTEX_UNLOCK(group->mutex);
            MUTEX_DESTROY(group->mutex);
            free(group);
        } else {
            /* 超时，保留 group，调用者可检查 pending/completed_count */
            MUTEX_UNLOCK(group->mutex);
        }
    }
}

/* ========================================================================
 * 全局线程池
 * ======================================================================== */

/**
 * @brief 获取全局单例线程池
 *
 * 使用静态初始化锁确保线程安全：
 * - Windows: SRWLOCK（静态初始化，轻量级读写锁）
 * - POSIX: pthread_mutex_t（静态初始化 PTHREAD_MUTEX_INITIALIZER）
 *
 * @return 全局线程池指针
 */
lvThreadPool *lv_get_global_thread_pool(void) {
#ifdef _WIN32
    static SRWLOCK g_pool_lock = SRWLOCK_INIT;
    AcquireSRWLockExclusive(&g_pool_lock);
    if (g_global_pool == NULL) {
        g_global_pool = lv_thread_pool_create(DEFAULT_THREADS);
    }
    ReleaseSRWLockExclusive(&g_pool_lock);
#else
    static pthread_mutex_t g_pool_lock = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_lock(&g_pool_lock);
    if (g_global_pool == NULL) {
        g_global_pool = lv_thread_pool_create(DEFAULT_THREADS);
    }
    pthread_mutex_unlock(&g_pool_lock);
#endif
    return g_global_pool;
}
