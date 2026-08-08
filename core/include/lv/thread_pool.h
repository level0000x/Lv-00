#ifndef lv_THREAD_POOL_H
#define lv_THREAD_POOL_H

#include <stddef.h>
#include "lv/lv_thread.h"

#ifndef lv_PUBLIC_API
#define lv_PUBLIC_API
#endif

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
    int uses_std_free;         /**< 1 = 节点由标准 calloc/malloc 分配，worker 用 free 释放
                                    （避免跨线程 lv_free 破坏 lv 分配器 TLS 追踪链表） */
};

/** @brief 等待组（任务同步） */
struct lvWaitGroup {
    int pending;         /**< 待完成任务数 */
    int completed_count; /**< 已完成任务数（支持超时查询） */
    lv_mutex_t mutex;    /**< 保护互斥锁 */
    lv_cond_t cond;      /**< 等待条件变量 */
};

/** @brief 并行 for 迭代回调：idx 为迭代下标，ctx 为透传上下文 */
typedef void (*lvParallelForFn)(int idx, void *ctx);

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

/**
 * @brief 占位实现（无 lv_THREAD_POOL_IMPL 链接时）：顺序执行全部迭代，保证正确性
 */
static inline void lv_parallel_for(lvThreadPool *pool, int n_iters, int chunk_size, lvParallelForFn fn, void *ctx) {
    (void) pool;
    (void) chunk_size;
    if (n_iters > 0 && fn) {
        for (int i = 0; i < n_iters; i++)
            fn(i, ctx);
    }
}
#else
/* 真实现模式（定义 lv_THREAD_POOL_IMPL，如 thread_pool.c 与单元测试）：
 * 全部 API 声明为外部符号，链接 thread_pool.c 的真实现。 */
lv_PUBLIC_API lvThreadPool *lv_thread_pool_create(int num_threads);
lv_PUBLIC_API void lv_thread_pool_destroy(lvThreadPool *pool);
lv_PUBLIC_API lvThreadPool *lv_get_global_thread_pool(void);
lv_PUBLIC_API lvWaitGroup *lv_thread_pool_submit(lvThreadPool *pool, lvThreadTask *task);
lv_PUBLIC_API void lv_thread_pool_wait_group(lvThreadPool *pool, lvWaitGroup *group, int timeout_ms);

/**
 * @brief 并行 for：将 [0, n_iters) 按 chunk_size 分片提交到线程池并行执行，阻塞等待全部完成
 * @param pool       线程池（NULL 时顺序执行）
 * @param n_iters    迭代总数（<=0 时为空操作）
 * @param chunk_size 每片迭代数（<=0 时视为 1）；迭代回调内必须无共享状态竞争
 * @param fn         迭代回调 fn(idx, ctx)
 * @param ctx        透传给 fn 的上下文（可为 NULL）
 * @note 迭代回调在多个工作线程上并发调用；如需聚合结果，回调内须自行加锁或原子操作。
 */
lv_PUBLIC_API void lv_parallel_for(lvThreadPool *pool, int n_iters, int chunk_size, lvParallelForFn fn, void *ctx);
#endif

#ifdef __cplusplus
}
#endif

#endif /* lv_THREAD_POOL_H */
