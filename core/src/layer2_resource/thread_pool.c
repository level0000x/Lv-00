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
#include "lv/lv_internal.h"

/* ========================================================================
 * 内部常量与数据结构
 * ======================================================================== */
#define MAX_TASK_QUEUE 4096 /**< 最大任务队列长度 */
#define DEFAULT_THREADS 4   /**< 默认工作线程数 */

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
            /* 任务节点可能由提交线程（lv_calloc）或调用方（标准 calloc）分配：
             * 跨线程 lv_free 会破坏 lv 分配器 TLS 追踪链表，标准分配节点用 free 释放 */
            if (task->uses_std_free) {
                free(task);
            } else {
                lv_free((void **) &task);
            }
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

    lvThreadPool *pool = (lvThreadPool *) lv_calloc(1, sizeof(lvThreadPool));
    if (pool == NULL)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "lv_thread_pool_create: calloc pool failed");

    pool->thread_count = num_threads;
    pool->threads = (lv_thread_t *) lv_calloc((size_t) num_threads, sizeof(lv_thread_t));
    if (pool->threads == NULL) {
        lv_free((void **) &pool);
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
        lv_free((void **) &task);
        task = next;
    }

    lv_mutex_destroy(&pool->mutex);
    lv_free((void **) &pool->threads);
    lv_free((void **) &pool);
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
    lvWaitGroup *group = (lvWaitGroup *) lv_calloc(1, sizeof(lvWaitGroup));
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
        lv_free((void **) &group);
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
        lv_free((void **) &group);
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
            lv_free((void **) &group);
        } else {
            /* 超时，保留 group，调用者可检查 pending/completed_count */
            lv_mutex_unlock(&group->mutex);
        }
    }
}

/* ========================================================================
 * 全局线程池
 * ======================================================================== */

/** @brief 全局线程池 once 守卫（文件级，供 destroy 重置使用；g_global_pool 见 L49） */
static lv_once_t g_pool_once = lv_ONCE_INIT;

/** @brief 初始化全局单例线程池（仅执行一次，由 lv_once 保证线程安全） */
static void global_pool_init(void) {
    g_global_pool = lv_thread_pool_create(DEFAULT_THREADS);
}

/**
 * @brief 获取全局单例线程池
 *
 * 通过 lv_once 保证线程安全的一次性初始化，消除手动
 * g_pool_lock / g_pool_lock_inited 标志的数据竞争。
 * 【J1/F28】lv_global_thread_pool_destroy 后 lv_once_reset，
 * init/cleanup 循环可再次创建（原 static 局部 once 无法重置）。
 *
 * @return 全局线程池指针
 */
lvThreadPool *lv_get_global_thread_pool(void) {
    lv_once(&g_pool_once, global_pool_init);
    return g_global_pool;
}

/**
 * @brief 销毁全局单例线程池（供系统清理流程调用）
 *
 * 先将全局指针置空再销毁，保证 NULL 安全与重复销毁安全
 * （从未创建线程池时为空操作，重复调用亦为空操作）。
 * 【J1/F28】销毁后重置 once 守卫，允许后续 lv_init 循环重建。
 */
void lv_global_thread_pool_destroy(void) {
    lvThreadPool *pool = g_global_pool;
    g_global_pool = NULL;
    lv_thread_pool_destroy(pool);
    lv_once_reset(&g_pool_once);
}

/* ========================================================================
 * lv_parallel_for —— 并行 for 抽象
 * ======================================================================== */

/** @brief 单个分片的任务参数（args 数组由 lv_parallel_for 栈/堆持有，等待全部完成前有效） */
typedef struct lvParForArg {
    lvParallelForFn fn;   /**< 迭代回调 */
    void *ctx;            /**< 透传上下文 */
    int start;            /**< 本片起始下标（含） */
    int end;              /**< 本片结束下标（不含） */
} lvParForArg;

/** @brief 线程池 worker 入口：顺序执行本片 [start, end) 的所有迭代 */
static void par_for_worker(void *arg) {
    lvParForArg *a = (lvParForArg *) arg;
    for (int i = a->start; i < a->end; i++) {
        a->fn(i, a->ctx);
    }
}

void lv_parallel_for(lvThreadPool *pool, int n_iters, int chunk_size, lvParallelForFn fn, void *ctx) {
    /* 参数校验：pool 为 NULL 时顺序执行（与占位实现语义一致） */
    if (n_iters <= 0 || !fn)
        return;
    if (!pool) {
        for (int i = 0; i < n_iters; i++)
            fn(i, ctx);
        return;
    }
    if (chunk_size <= 0)
        chunk_size = 1;

    int n_tasks = (n_iters + chunk_size - 1) / chunk_size;
    if (n_tasks <= 0)
        return;

    /* 任务参数数组 + 等待组数组（全部任务阻塞等待完成后释放） */
    lvParForArg *args = (lvParForArg *) lv_calloc((size_t) n_tasks, sizeof(lvParForArg));
    lvWaitGroup **groups = (lvWaitGroup **) lv_calloc((size_t) n_tasks, sizeof(lvWaitGroup *));
    if (!args || !groups) {
        lv_free((void **) &args);
        lv_free((void **) &groups);
        return;
    }

    /* 提交阶段：全部提交后再统一等待，保证并行性 */
    int submitted = 0;
    for (; submitted < n_tasks; submitted++) {
        /* 任务节点由 worker 线程释放（跨线程），必须用标准 calloc 分配，
         * 否则 worker 的 lv_free 会破坏提交线程 lv 分配器的 TLS 追踪链表 */
        lvThreadTask *task = (lvThreadTask *) calloc(1, sizeof(lvThreadTask));
        if (!task)
            break;
        task->uses_std_free = 1;
        args[submitted].fn = fn;
        args[submitted].ctx = ctx;
        args[submitted].start = submitted * chunk_size;
        args[submitted].end = (submitted + 1) * chunk_size;
        if (args[submitted].end > n_iters)
            args[submitted].end = n_iters;

        task->func = par_for_worker;
        task->arg = &args[submitted];
        groups[submitted] = lv_thread_pool_submit(pool, task);
        if (!groups[submitted]) {
            /* submit 失败时不释放 task 节点（其内部仅释放 group），此处回收节点 */
            free(task);
            break;
        }
    }

    /* 等待阶段：逐个阻塞等待（timeout_ms = -1 无限等待，完成时 wait_group 内部自动销毁释放） */
    for (int t = 0; t < submitted; t++) {
        lv_thread_pool_wait_group(pool, groups[t], -1);
    }

    lv_free((void **) &args);
    lv_free((void **) &groups);
}
