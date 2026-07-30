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

#include "lv/lv_thread.h"
#include "lv_internal.h"

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
    lv_mutex_t mutex;       /**< 保护互斥锁 */
    lv_cond_t cond;      /**< 等待条件变量 */
};

/** 线程池 */
struct lvThreadPool {
    lv_thread_t *threads; /**< 工作线程句柄数组 */
    int thread_count;  /**< 工作线程数 */

    /* 任务队列（链表） */
    lvThreadTask *queue_head; /**< 队列头 */
    lvThreadTask *queue_tail; /**< 队列尾 */
    int queue_size;           /**< 当前队列长度 */

    lv_mutex_t mutex;       /**< 队列保护互斥锁 */
    lv_cond_t not_empty; /**< 队列非空条件 */
    int shutdown;        /**< 关闭标志 */
};

/* ========================================================================
 * 全局线程池实例
 * ======================================================================== */
static lvThreadPool *g_global_pool = NULL;

/* ========================================================================
 * 工作线程主函数
 * ======================================================================== */
static void *worker_func(void *arg)
{
    lvThreadPool *pool = (lvThreadPool *) arg;

    for (;;) {
        lv_mutex_lock(&pool->mutex);

        /* 等待队列非空或关闭信号 */
        while (pool->queue_size == 0 && !pool->shutdown) {
            lv_cond_wait(&pool->not_empty, &pool->mutex);
        }

        if (pool->shutdown && pool->queue_size == 0) {
            lv_mutex_unlock(&pool->mutex);
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

        lv_mutex_unlock(&pool->mutex);

        /* 执行任务 */
        if (task != NULL) {
            if (task->func != NULL) {
                task->func(task->arg);
            }
            /* 通知等待组 */
            if (task->group != NULL) {
                lv_mutex_lock(&task->group->mutex);
                task->group->pending--;
                task->group->completed_count++;
                if (task->group->pending <= 0) {
                    lv_cond_signal(&task->group->cond);
                }
                lv_mutex_unlock(&task->group->mutex);
            }
            free(task);
        }
    }

    return NULL;
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
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "lv_thread_pool_create: calloc pool failed");

    pool->thread_count = num_threads;
    pool->threads = (lv_thread_t *) calloc((size_t) num_threads, sizeof(lv_thread_t));
    if (pool->threads == NULL) {
        free(pool);
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "lv_thread_pool_create: calloc threads failed");
    }

    lv_mutex_init(&pool->mutex);
    lv_cond_init(&pool->not_empty);
    pool->shutdown = 0;
    pool->queue_head = NULL;
    pool->queue_tail = NULL;
    pool->queue_size = 0;

    /* 创建工作线程 */
    for (int i = 0; i < num_threads; i++) {
        if (lv_thread_create(&pool->threads[i], worker_func, pool) != 0) {
            pool->thread_count = i;
            lv_thread_pool_destroy(pool);
            lv_RETURN_ERROR_NULL(lv_ERROR_INTERNAL, "lv_thread_pool_create: thread creation failed");
        }
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
    lv_mutex_lock(&pool->mutex);
    pool->shutdown = 1;
    lv_cond_broadcast(&pool->not_empty);
    lv_mutex_unlock(&pool->mutex);

    /* 等待所有线程结束 */
    for (int i = 0; i < pool->thread_count; i++) {
        lv_thread_join(pool->threads[i]);
    }

    /* 释放残留任务 */
    lvThreadTask *task = pool->queue_head;
    while (task != NULL) {
        lvThreadTask *next = task->next;
        free(task);
        task = next;
    }

    lv_mutex_destroy(&pool->mutex);
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
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "lv_thread_pool_submit: pool or task is NULL");

    /* 创建等待组 */
    lvWaitGroup *group = (lvWaitGroup *) calloc(1, sizeof(lvWaitGroup));
    if (group == NULL)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "lv_thread_pool_submit: calloc group failed");
    lv_mutex_init(&group->mutex);
    lv_cond_init(&group->cond);
    group->pending = 1;
    group->completed_count = 0;

    task->group = group;
    task->next = NULL;

    /* 入队 */
    lv_mutex_lock(&pool->mutex);

    if (pool->queue_size >= MAX_TASK_QUEUE) {
        lv_mutex_unlock(&pool->mutex);
        free(group);
        lv_RETURN_ERROR_NULL(lv_ERROR_OVERFLOW, "lv_thread_pool_submit: task queue full");
    }

    if (pool->queue_tail != NULL) {
        pool->queue_tail->next = task;
    } else {
        pool->queue_head = task;
    }
    pool->queue_tail = task;
    pool->queue_size++;

    lv_cond_signal(&pool->not_empty);
    lv_mutex_unlock(&pool->mutex);

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

    lv_mutex_lock(&group->mutex);

    if (timeout_ms < 0) {
        /* 无限等待，直到所有任务完成 */
        while (group->pending > 0) {
            lv_cond_wait(&group->cond, &group->mutex);
        }
        lv_mutex_unlock(&group->mutex);
        lv_mutex_destroy(&group->mutex);
        free(group);
    } else if (timeout_ms == 0) {
        /* 非阻塞检查：立即返回，不等待 */
        /* pending > 0 表示任务未完成，调用者可自行检查 group->pending */
        lv_mutex_unlock(&group->mutex);
        /* 不销毁、不释放，调用者可重试 */
    } else {
        /* 带超时等待 */
        while (group->pending > 0) {
            if (lv_cond_timedwait(&group->cond, &group->mutex, (unsigned int)timeout_ms) != 0) {
                /* 超时：不再等待，保留 group 供调用者检查 */
                break;
            }
        }

        if (group->pending <= 0) {
            /* 所有任务已完成 */
            lv_mutex_unlock(&group->mutex);
            lv_mutex_destroy(&group->mutex);
            free(group);
        } else {
            /* 超时，保留 group，调用者可检查 pending/completed_count */
            lv_mutex_unlock(&group->mutex);
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
    static lv_once_t g_pool_once = lv_ONCE_INIT;
    static lv_mutex_t g_pool_lock;
    static int g_pool_lock_inited = 0;
    
    if (!g_pool_lock_inited) {
        lv_mutex_init(&g_pool_lock);
        g_pool_lock_inited = 1;
    }
    
    lv_mutex_lock(&g_pool_lock);
    if (g_global_pool == NULL) {
        g_global_pool = lv_thread_pool_create(DEFAULT_THREADS);
    }
    lv_mutex_unlock(&g_pool_lock);
    return g_global_pool;
}
