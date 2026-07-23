#ifndef lv_THREAD_POOL_H
#define lv_THREAD_POOL_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 线程池模块 */

typedef struct lvThreadPool lvThreadPool;
typedef struct lvThreadTask lvThreadTask;
typedef struct lvWaitGroup lvWaitGroup;

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
