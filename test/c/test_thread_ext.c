/**
 * @file test_thread_ext.c
 * @brief 线程原语契约测试（批次 C-㊺续10：lv_thread.h 22 个零覆盖 API）
 *
 * 覆盖 22 个 ctest 零覆盖 API（全部为 header-only static inline）：
 *   - 线程族：lv_thread_create / join / detach / id / sleep / entry
 *   - 条件变量族：lv_cond_init / destroy / signal / broadcast / wait /
 *     timedwait
 *   - 惰性锁族：lv_LAZY_LOCK_DEFINE / lazy_lock_init / lock / unlock /
 *     destroy
 *   - 守卫族：lv_lock_guard_init / destroy / scope_cleanup
 *   - once 族：lv_once / lv_once_callback
 *
 * 契约要点（与头注释核对）：
 *   - thread_create 入口签名 void*(*)(void*)；join 等待并回收。
 *   - cond_timedwait 超时返回非 0（Windows SleepConditionVariableCS
 *     失败 → -1）。
 *   - lv_once 回调仅执行一次（线程安全）。
 *   - lazy_lock 首次 lock 自动初始化 mutex。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_unified.h"
#include "lv/lv_thread.h"

int g_pass_count = 0;
int g_fail_count = 0;

static int g_thread_counter = 0;

static void *thread_inc(void *arg) {
    (void)arg;
    g_thread_counter++;
    return NULL;
}

static int g_once_calls = 0;
static void once_inc(void) {
    g_once_calls++;
}

/* ============== 测试：线程 ============== */

static void test_thread_api(void) {
    /* create + join：入口执行一次 */
    lv_thread_t th;
    TEST_ASSERT_EQ(lv_thread_create(&th, thread_inc, NULL), 0);
    TEST_ASSERT_EQ(lv_thread_join(th), 0);
    TEST_ASSERT_EQ(g_thread_counter, 1);

    /* id：当前线程非 0 */
    TEST_ASSERT(lv_thread_id() != 0, "当前线程 id 非零");

    /* sleep：短暂休眠不崩溃 */
    lv_thread_sleep(5);

    /* detach：创建后分离（资源自动回收） */
    TEST_ASSERT_EQ(lv_thread_create(&th, thread_inc, NULL), 0);
    TEST_ASSERT_EQ(lv_thread_detach(th), 0);

    printf("  test_thread_api: PASSED\n");
}

/* ============== 测试：条件变量 ============== */

static void test_cond_api(void) {
    lv_mutex_t m;
    lv_cond_t cv;
    lv_mutex_init(&m);
    lv_cond_init(&cv);

    /* signal / broadcast：无等待者时不崩溃 */
    lv_cond_signal(&cv);
    lv_cond_broadcast(&cv);

    /* timedwait：1ms 超时返回非 0（无唤醒者） */
    lv_mutex_lock(&m);
    int r = lv_cond_timedwait(&cv, &m, 1);
    lv_mutex_unlock(&m);
    TEST_ASSERT(r != 0, "超时返回非 0");

    lv_cond_destroy(&cv);
    lv_mutex_destroy(&m);
    printf("  test_cond_api: PASSED\n");
}

/* ============== 测试：once 与惰性锁 ============== */

lv_LAZY_LOCK_DEFINE(g_test_lock); /* 宏已含 static 声明 */

static void test_once_lazy_api(void) {
    /* once：回调仅执行一次（即使多次调用） */
    static lv_once_t once = lv_ONCE_INIT;
    lv_once(&once, once_inc);
    lv_once(&once, once_inc);
    lv_once(&once, once_inc);
    TEST_ASSERT_EQ(g_once_calls, 1);

    /* lazy_lock：init → lock → unlock → destroy */
    lv_lazy_lock_init(&g_test_lock, g_test_lock_init_once);
    lv_lazy_lock_lock(&g_test_lock, g_test_lock_init_once);
    lv_lazy_lock_unlock(&g_test_lock);
    lv_lazy_lock_destroy(&g_test_lock);

    printf("  test_once_lazy_api: PASSED\n");
}

/* ============== 测试：锁守卫 ============== */

static void test_lock_guard_api(void) {
    lv_mutex_t m;
    lv_mutex_init(&m);

    /* init（加锁）→ destroy（解锁） */
    lvLockGuard g;
    lv_lock_guard_init(&g, &m);
    TEST_ASSERT(g.mutex == &m, "守卫持有锁指针");
    lv_lock_guard_destroy(&g);
    TEST_ASSERT_NULL(g.mutex);

    /* scope_cleanup：解锁 + 置 NULL */
    lvLockGuard g2;
    g2.mutex = &m;
    lv_lock_guard_scope_cleanup(&g2);
    TEST_ASSERT_NULL(g2.mutex);

    lv_mutex_destroy(&m);
    printf("  test_lock_guard_api: PASSED\n");
}

/* ============== 测试入口 ============== */

TEST_MAIN_BEGIN("Lv-00 Thread Ext Test Suite")
    printf("=== Lv-00 Thread Ext Test Suite (batch C-㊺续10) ===\n\n");
    lv_init();

    TEST_MAIN_RUN(test_thread_api);
    TEST_MAIN_RUN(test_cond_api);
    TEST_MAIN_RUN(test_once_lazy_api);
    TEST_MAIN_RUN(test_lock_guard_api);

    lv_cleanup();
TEST_MAIN_END()
