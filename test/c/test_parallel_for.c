/**
 * @file test_parallel_for.c
 * @brief 线程池并行 for（lv_parallel_for）单元测试
 *
 * 验证 n 次迭代全部执行且结果正确：
 *   - 无共享状态：每个迭代写入独立槽位
 *   - 共享聚合：互斥锁保护求和
 *   - chunk_size 分片、NULL 池顺序回退、非法参数空操作
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 必须先定义 lv_THREAD_POOL_IMPL 再包含 thread_pool.h，
 * 以获得 thread_pool.c 真实现的声明（而非顺序执行的占位版本） */
#define lv_THREAD_POOL_IMPL
#include "lv/thread_pool.h"

#include "test_unified.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ── 迭代回调：写入独立槽位（无共享竞争）── */
typedef struct {
    int *results;
    int n;
} ParForFillCtx;

static void fill_worker(int idx, void *ctx) {
    ParForFillCtx *c = (ParForFillCtx *) ctx;
    if (idx >= 0 && idx < c->n) {
        c->results[idx] = idx * 2 + 1;
    }
}

/* ── 迭代回调：共享累加（互斥锁保护聚合）── */
typedef struct {
    long long sum;
    lv_mutex_t mutex;
} ParForSumCtx;

static void sum_worker(int idx, void *ctx) {
    ParForSumCtx *c = (ParForSumCtx *) ctx;
    lv_mutex_lock(&c->mutex);
    c->sum += idx;
    lv_mutex_unlock(&c->mutex);
}

/* 1. 基本并行：全部迭代执行且结果正确（chunk=1） */
static void test_parallel_for_basic(void) {
    const int N = 1000;
    int *results = (int *) lv_calloc((size_t) N, sizeof(int));
    lv_ASSERT_NOT_NULL(results);
    ParForFillCtx ctx = {results, N};

    lvThreadPool *pool = lv_thread_pool_create(4);
    lv_ASSERT_NOT_NULL(pool);

    lv_parallel_for(pool, N, 1, fill_worker, &ctx);

    /* 验证全部迭代执行且结果正确 */
    for (int i = 0; i < N; i++) {
        lv_ASSERT(results[i] == i * 2 + 1);
    }

    lv_thread_pool_destroy(pool);
    lv_free((void **) &results);
    printf("basic 并行 for 测试通过!\n\n");
}

/* 2. 分片并行：chunk_size > 1 时全部迭代仍恰好执行一次 */
static void test_parallel_for_chunked(void) {
    const int N = 1000;
    const int CHUNK = 7;
    int *results = (int *) lv_calloc((size_t) N, sizeof(int));
    lv_ASSERT_NOT_NULL(results);
    ParForFillCtx ctx = {results, N};

    lvThreadPool *pool = lv_thread_pool_create(4);
    lv_ASSERT_NOT_NULL(pool);

    lv_parallel_for(pool, N, CHUNK, fill_worker, &ctx);

    for (int i = 0; i < N; i++) {
        lv_ASSERT(results[i] == i * 2 + 1);
    }

    lv_thread_pool_destroy(pool);
    lv_free((void **) &results);
    printf("chunked 分片并行 for 测试通过!\n\n");
}

/* 3. 共享聚合：多线程累加结果正确（与串行求和一致） */
static void test_parallel_for_sum(void) {
    const int N = 10000;
    ParForSumCtx ctx = {0};
    lv_mutex_init(&ctx.mutex);

    lvThreadPool *pool = lv_thread_pool_create(4);
    lv_ASSERT_NOT_NULL(pool);

    lv_parallel_for(pool, N, 64, sum_worker, &ctx);

    long long expect = (long long) N * (N - 1) / 2;
    lv_ASSERT(ctx.sum == expect);

    lv_mutex_destroy(&ctx.mutex);
    lv_thread_pool_destroy(pool);
    printf("共享聚合求和并行 for 测试通过!\n\n");
}

/* 4. NULL 池回退：顺序执行，结果一致 */
static void test_parallel_for_null_pool(void) {
    const int N = 100;
    int *results = (int *) lv_calloc((size_t) N, sizeof(int));
    lv_ASSERT_NOT_NULL(results);
    ParForFillCtx ctx = {results, N};

    lv_parallel_for(NULL, N, 1, fill_worker, &ctx);

    for (int i = 0; i < N; i++) {
        lv_ASSERT(results[i] == i * 2 + 1);
    }

    lv_free((void **) &results);
    printf("NULL 池顺序回退测试通过!\n\n");
}

/* 5. 非法参数：n_iters <= 0 / fn == NULL 均为空操作 */
static void test_parallel_for_invalid(void) {
    lvThreadPool *pool = lv_thread_pool_create(2);
    lv_ASSERT_NOT_NULL(pool);

    int touched = 0;
    lv_parallel_for(pool, 0, 1, fill_worker, &touched);  /* n=0：不执行 */
    lv_ASSERT(touched == 0);
    lv_parallel_for(pool, -5, 1, fill_worker, &touched); /* n<0：不执行 */
    lv_ASSERT(touched == 0);
    lv_parallel_for(pool, 10, 1, NULL, &touched);        /* fn=NULL：不执行 */
    lv_ASSERT(touched == 0);

    lv_thread_pool_destroy(pool);
    printf("非法参数空操作测试通过!\n\n");
}

/* 6. 全局线程池：获取全局单例并执行并行 for */
static void test_parallel_for_global_pool(void) {
    const int N = 500;
    int *results = (int *) lv_calloc((size_t) N, sizeof(int));
    lv_ASSERT_NOT_NULL(results);
    ParForFillCtx ctx = {results, N};

    lvThreadPool *pool = lv_get_global_thread_pool();
    lv_ASSERT_NOT_NULL(pool);

    lv_parallel_for(pool, N, 1, fill_worker, &ctx);

    for (int i = 0; i < N; i++) {
        lv_ASSERT(results[i] == i * 2 + 1);
    }

    lv_free((void **) &results);
    printf("全局线程池并行 for 测试通过!\n\n");
}

TEST_MAIN_BEGIN("并行 for 测试")
    printf("========================================\n");
    printf("lv_parallel_for 并行测试\n");
    printf("========================================\n\n");
    TEST_MAIN_RUN(test_parallel_for_basic);
    TEST_MAIN_RUN(test_parallel_for_chunked);
    TEST_MAIN_RUN(test_parallel_for_sum);
    TEST_MAIN_RUN(test_parallel_for_null_pool);
    TEST_MAIN_RUN(test_parallel_for_invalid);
    TEST_MAIN_RUN(test_parallel_for_global_pool);
    printf("========================================\n");
    printf("所有并行 for 测试通过!\n");
    printf("========================================\n");
TEST_MAIN_END()
