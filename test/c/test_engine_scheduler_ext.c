/**
 * @file test_engine_scheduler_ext.c
 * @brief 调度器向后兼容 API 契约测试（批次 C-㊺续33：engine_scheduler.h 零覆盖 API）
 *
 * 覆盖零覆盖 API（5 个）：
 *   lv_engine_scheduler_init / shutdown / schedule / execute_pending / pending_count
 *
 * 契约要点（与 engine_scheduler.c 核对）：
 *   - init：NULL 安全；记录 TLS 引擎；为 engine->scheduler 惰性创建调度器。
 *   - shutdown：NULL 安全；TLS 匹配时清空。
 *   - schedule：task_name NULL → -1；无 TLS 引擎 → 错误码；main_graph NULL → 0；
 *     未知任务名 → 0。
 *   - execute_pending：无引擎/无调度器/无 main_graph → false。
 *   - pending_count：恒 0（新版调度器无任务队列）。
 *
 * @author Lv-00 Project
 */

#include <stdio.h>
#include <string.h>

#include "lv/engine.h"
#include "lv/engine_scheduler.h"

#include "test_unified.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============== 测试：未初始化行为 ============== */

static void test_uninitialized(void) {
    /* 未调用 init：TLS 引擎为 NULL */
    TEST_ASSERT_EQ(lv_engine_pending_count(), 0);
    TEST_ASSERT(!lv_engine_execute_pending(), "execute_pending false without engine");
    TEST_ASSERT(lv_engine_schedule("solve", 0) != 0, "schedule errors without engine");
    TEST_ASSERT_EQ(lv_engine_schedule(NULL, 0), -1);

    /* NULL 安全 */
    lv_engine_scheduler_init(NULL);
    lv_engine_scheduler_shutdown(NULL);
}

/* ============== 测试：初始化后行为 ============== */

static void test_initialized(void) {
    lvEngine engine;
    memset(&engine, 0, sizeof(engine));

    /* init：创建调度器 */
    lv_engine_scheduler_init(&engine);
    TEST_ASSERT_NOT_NULL(engine.scheduler);

    /* main_graph 为 NULL：schedule 返回 0（无操作），execute_pending false */
    TEST_ASSERT_EQ(lv_engine_schedule("solve", 0), 0);
    TEST_ASSERT(!lv_engine_execute_pending(), "execute_pending false with NULL main_graph");
    TEST_ASSERT_EQ(lv_engine_pending_count(), 0);

    /* 未知任务名：0 */
    TEST_ASSERT_EQ(lv_engine_schedule("no-such-task", 5), 0);

    /* shutdown：TLS 清空 */
    lv_engine_scheduler_shutdown(&engine);
    TEST_ASSERT(lv_engine_schedule("solve", 0) != 0, "schedule errors after shutdown");
}

/* ============== Main ============== */

TEST_MAIN_BEGIN("EngineSchedulerExt")

    printf("\n--- engine_scheduler (zero-coverage) ---\n");
    TEST_MAIN_RUN(test_uninitialized);
    TEST_MAIN_RUN(test_initialized);

TEST_MAIN_END()
