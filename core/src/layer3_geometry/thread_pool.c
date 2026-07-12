/**
 * @file thread_pool.c
 * @brief 线程池实现
 *
 * 提供工作线程的创建/销毁、任务队列管理和任务提交功能。
 * 支持等待组 (WaitGroup) 机制，允许提交一组任务后统一等待完成。
 *
 * @version 1.0.0
 */

#define LV00_THREAD_POOL_IMPL
#include "lv00/thread_pool.h"
#include <stdlib.h>
#include <string.h>

/* ========================================================================
 * 平台相关线程抽象
 * ======================================================================== */
#ifdef _WIN32
  #include <windows.h>
  typedef HANDLE Lv00Thread;
  typedef CRITICAL_SECTION Lv00Mutex;
  typedef CONDITION_VARIABLE Lv00CondVar;
  #define MUTEX_INIT(m)    InitializeCriticalSection(&(m))
  #define MUTEX_LOCK(m)    EnterCriticalSection(&(m))
  #define MUTEX_UNLOCK(m)  LeaveCriticalSection(&(m))
  #define MUTEX_DESTROY(m) DeleteCriticalSection(&(m))
  #define COND_INIT(c)     InitializeConditionVariable(&(c))
  #define COND_WAIT(c, m)  SleepConditionVariableCS(&(c), &(m), INFINITE)
  #define COND_SIGNAL(c)   WakeConditionVariable(&(c))
  #define COND_BROADCAST(c) WakeAllConditionVariable(&(c))
#else
  #include <pthread.h>
  typedef pthread_t Lv00Thread;
  typedef pthread_mutex_t Lv00Mutex;
  typedef pthread_cond_t Lv00CondVar;
  #define MUTEX_INIT(m)    pthread_mutex_init(&(m), NULL)
  #define MUTEX_LOCK(m)    pthread_mutex_lock(&(m))
  #define MUTEX_UNLOCK(m)  pthread_mutex_unlock(&(m))
  #define MUTEX_DESTROY(m) pthread_mutex_destroy(&(m))
  #define COND_INIT(c)     pthread_cond_init(&(c), NULL)
  #define COND_WAIT(c, m)  pthread_cond_wait(&(c), &(m))
  #define COND_SIGNAL(c)   pthread_cond_signal(&(c))
  #define COND_BROADCAST(c) pthread_cond_broadcast(&(c))
#endif

/* ========================================================================
 * 内部常量与数据结构
 * ======================================================================== */
#define MAX_TASK_QUEUE 4096    /**< 最大任务队列长度 */
#define DEFAULT_THREADS 4      /**< 默认工作线程数 */

/** 任务节点（链表） */
struct Lv00ThreadTask {
    void (*func)(void *arg);   /**< 任务函数 */
    void *arg;                 /**< 任务参数 */
    Lv00WaitGroup *group;      /**< 所属等待组（可为 NULL） */
    struct Lv00ThreadTask *next; /**< 下一个任务 */
};

/** 等待组 */
struct Lv00WaitGroup {
    int pending;               /**< 待完成任务数 */
    Lv00Mutex mutex;           /**< 保护互斥锁 */
    Lv00CondVar cond;          /**< 等待条件变量 */
};

/** 线程池 */
struct Lv00ThreadPool {
    Lv00Thread *threads;       /**< 工作线程句柄数组 */
    int thread_count;          /**< 工作线程数 */

    /* 任务队列（链表） */
    Lv00ThreadTask *queue_head; /**< 队列头 */
    Lv00ThreadTask *queue_tail; /**< 队列尾 */
    int queue_size;            /**< 当前队列长度 */

    Lv00Mutex mutex;           /**< 队列保护互斥锁 */
    Lv00CondVar not_empty;     /**< 队列非空条件 */
    int shutdown;              /**< 关闭标志 */
};

/* ========================================================================
 * 全局线程池实例
 * ======================================================================== */
static Lv00ThreadPool *g_global_pool = NULL;

/* ========================================================================
 * 工作线程主函数
 * ======================================================================== */
#ifdef _WIN32
static DWORD WINAPI worker_func(LPVOID arg)
#else
static void *worker_func(void *arg)
#endif
{
    Lv00ThreadPool *pool = (Lv00ThreadPool *)arg;

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
        Lv00ThreadTask *task = pool->queue_head;
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

/* ========================================================================
 * 线程池 API 实现
 * ======================================================================== */

Lv00ThreadPool *lv00_thread_pool_create(int num_threads)
{
    if (num_threads <= 0) {
        num_threads = DEFAULT_THREADS;
    }

    Lv00ThreadPool *pool = (Lv00ThreadPool *)calloc(1, sizeof(Lv00ThreadPool));
    if (pool == NULL) return NULL;

    pool->thread_count = num_threads;
    pool->threads = (Lv00Thread *)calloc((size_t)num_threads, sizeof(Lv00Thread));
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
#else
        pthread_create(&pool->threads[i], NULL, worker_func, pool);
#endif
    }

    return pool;
}

void lv00_thread_pool_destroy(Lv00ThreadPool *pool)
{
    if (pool == NULL) return;

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
    Lv00ThreadTask *task = pool->queue_head;
    while (task != NULL) {
        Lv00ThreadTask *next = task->next;
        free(task);
        task = next;
    }

    MUTEX_DESTROY(pool->mutex);
    free(pool->threads);
    free(pool);
}

Lv00WaitGroup *lv00_thread_pool_submit(Lv00ThreadPool *pool, Lv00ThreadTask *task)
{
    if (pool == NULL || task == NULL) return NULL;

    /* 创建等待组 */
    Lv00WaitGroup *group = (Lv00WaitGroup *)calloc(1, sizeof(Lv00WaitGroup));
    if (group == NULL) return NULL;
    MUTEX_INIT(group->mutex);
    COND_INIT(group->cond);
    group->pending = 1;

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

void lv00_thread_pool_wait_group(Lv00ThreadPool *pool, Lv00WaitGroup *group,
                                  int timeout_ms)
{
    (void)pool;
    if (group == NULL) return;

    MUTEX_LOCK(group->mutex);
    /* 简化实现：不支持精确超时，一直等到所有任务完成 */
    (void)timeout_ms;
    while (group->pending > 0) {
        COND_WAIT(group->cond, group->mutex);
    }
    MUTEX_UNLOCK(group->mutex);

    MUTEX_DESTROY(group->mutex);
    free(group);
}

/* ========================================================================
 * 全局线程池
 * ======================================================================== */

Lv00ThreadPool *lv00_get_global_thread_pool(void)
{
    /* 注意：此简化实现非线程安全地初始化全局池 */
    if (g_global_pool == NULL) {
        g_global_pool = lv00_thread_pool_create(DEFAULT_THREADS);
    }
    return g_global_pool;
}
