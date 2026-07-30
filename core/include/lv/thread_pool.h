#ifndef lv_THREAD_POOL_H
#define lv_THREAD_POOL_H

#include <stddef.h>
#include "lv/lv_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 线程池模块 */

typedef struct lvThreadPool lvThreadPool;
typedef struct lvThreadTask lvThreadTask;
typedef struct lvWaitGroup lvWaitGroup;

/** @brief 线程任务节点 */
struct lvThreadTask {
    void (*func)(void *arg);   /**< 任务函数 */
    void *arg;                 /**< 任务参数 */
    struct lvWaitGroup *group; /**< 所属等待组（可为 NULL） */
    struct lvThreadTask *next; /**< 下一个任务 */
};

/** @brief 等待组（任务同步） */
struct lvWaitGroup {
    int pending;         /**< 待完成任务数 */
    int completed_count; /**< 已完成任务数（支持超时查询） */
    lv_mutex_t mutex;    /**< 保护互斥锁 */
    lv_cond_t cond;      /**< 等待条件变量 */
};

/* 仅当未提供真正实现时使用内联占位函数 */
#ifndef lv_THREAD_POOL_IMPL
static inline lvThreadPool *lv_get_global_thread_pool(void) {
    return NULL;
}
static inline lvWaitGroup *lv_thread_pool_submit(lvThreadPool *pool, lvThreadTask *task) {
    (void) pool;
    (void) task;
    return NULL;
}
static inline void lv_thread_pool_wait_group(lvThreadPool *pool, lvWaitGroup *group, int timeout_ms) {
    (void) pool;
    (void) group;
    (void) timeout_ms;
}
#endif

#ifdef __cplusplus
}
#endif

#endif /* lv_THREAD_POOL_H */
