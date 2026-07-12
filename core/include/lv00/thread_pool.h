#ifndef LV00_THREAD_POOL_H
#define LV00_THREAD_POOL_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 线程池模块 */

typedef struct Lv00ThreadPool Lv00ThreadPool;
typedef struct Lv00ThreadTask Lv00ThreadTask;
typedef struct Lv00WaitGroup Lv00WaitGroup;

/* 仅当未提供真正实现时使用内联占位函数 */
#ifndef LV00_THREAD_POOL_IMPL
static inline Lv00ThreadPool *lv00_get_global_thread_pool(void) { return NULL; }
static inline Lv00WaitGroup *lv00_thread_pool_submit(Lv00ThreadPool *pool, Lv00ThreadTask *task) { (void)pool; (void)task; return NULL; }
static inline void lv00_thread_pool_wait_group(Lv00ThreadPool *pool, Lv00WaitGroup *group, int timeout_ms) { (void)pool; (void)group; (void)timeout_ms; }
#endif

#ifdef __cplusplus
}
#endif

#endif /* LV00_THREAD_POOL_H */
