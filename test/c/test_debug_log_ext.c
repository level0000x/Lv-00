/**
 * @file test_debug_log_ext.c
 * @brief 调试子系统契约测试（批次 C-㊺续15：debug.h 环形日志缓冲区族 + 上下文日志）
 *
 * 覆盖零覆盖 API：
 *   lv_log_ring_buffer_create / destroy / write / export / clear / resize
 *   lv_log_with_context（7 个）
 *
 * 契约要点（与头注释核对）：
 *   - create：capacity < 1 时回退默认 256；失败返回 NULL。
 *   - write：线程安全写入，满时覆盖最旧；NULL 安全。
 *   - export：按插入顺序（最旧在前），count==0 返回 NULL；调用者 lv_free。
 *   - clear：清空所有条目；NULL 安全。
 *   - resize：保留现有条目（最多新容量条）；capacity < 1 失败。
 *   - lv_log_with_context：ctx 可为 NULL，正常写入日志流。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_unified.h"
#include "lv/debug.h"
#include "lv/lv_utils.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============== 测试：创建/销毁 ============== */

static void test_ringbuf_create_api(void) {
    /* create 正常 */
    lvLogRingBuffer *rb = lv_log_ring_buffer_create(8);
    TEST_ASSERT_NOT_NULL(rb);
    lv_log_ring_buffer_destroy(rb);

    /* capacity < 1 回退默认 */
    rb = lv_log_ring_buffer_create(0);
    TEST_ASSERT_NOT_NULL(rb);
    lv_log_ring_buffer_destroy(rb);
    rb = lv_log_ring_buffer_create(-5);
    TEST_ASSERT_NOT_NULL(rb);
    lv_log_ring_buffer_destroy(rb);

    /* destroy NULL 安全 */
    lv_log_ring_buffer_destroy(NULL);

    printf("  test_ringbuf_create_api: PASSED\n");
}

/* ============== 测试：写入/导出 ============== */

static void test_ringbuf_write_export_api(void) {
    lvLogRingBuffer *rb = lv_log_ring_buffer_create(4);
    TEST_ASSERT_NOT_NULL(rb);

    /* 写入 3 条 */
    lv_log_ring_buffer_write(rb, LOG_LEVEL_INFO, "test", "fn1", "file.c", 10, "msg %d", 1);
    lv_log_ring_buffer_write(rb, LOG_LEVEL_WARN, "test", "fn2", "file.c", 20, "msg %d", 2);
    lv_log_ring_buffer_write(rb, LOG_LEVEL_ERROR, "test", "fn3", "file.c", 30, "msg %d", 3);

    /* 导出：按插入顺序 3 条 */
    int count = 0;
    lvLogEntry *entries = lv_log_ring_buffer_export(rb, &count);
    TEST_ASSERT_NOT_NULL(entries);
    TEST_ASSERT_EQ(count, 3);
    TEST_ASSERT(strcmp(entries[0].message, "msg 1") == 0, "第 1 条消息");
    TEST_ASSERT(strcmp(entries[1].message, "msg 2") == 0, "第 2 条消息");
    TEST_ASSERT(strcmp(entries[2].message, "msg 3") == 0, "第 3 条消息");
    TEST_ASSERT_EQ(entries[0].level, LOG_LEVEL_INFO);
    TEST_ASSERT_EQ(entries[2].level, LOG_LEVEL_ERROR);
    TEST_ASSERT_EQ(entries[0].line_number, 10);
    TEST_ASSERT_EQ(entries[2].line_number, 30);
    lv_free((void **) &entries);

    /* 空缓冲区导出返回 NULL + count 0 */
    lvLogRingBuffer *empty = lv_log_ring_buffer_create(2);
    count = -1;
    entries = lv_log_ring_buffer_export(empty, &count);
    TEST_ASSERT_NULL(entries);
    TEST_ASSERT_EQ(count, 0);
    lv_log_ring_buffer_destroy(empty);

    /* export NULL 契约 */
    count = -1;
    entries = lv_log_ring_buffer_export(NULL, &count);
    TEST_ASSERT_NULL(entries);
    TEST_ASSERT_EQ(count, 0);
    entries = lv_log_ring_buffer_export(rb, NULL);
    TEST_ASSERT_NULL(entries);

    /* write NULL 安全 */
    lv_log_ring_buffer_write(NULL, LOG_LEVEL_INFO, "m", "f", "c", 1, "x");

    /* 溢出覆盖：容量 4 写 6 条，导出 4 条且为最新 4 条 */
    lv_log_ring_buffer_clear(rb);
    for (int i = 0; i < 6; i++) {
        lv_log_ring_buffer_write(rb, LOG_LEVEL_INFO, "t", "f", "c", i, "msg %d", i);
    }
    entries = lv_log_ring_buffer_export(rb, &count);
    TEST_ASSERT_NOT_NULL(entries);
    TEST_ASSERT_EQ(count, 4);
    TEST_ASSERT(strcmp(entries[0].message, "msg 2") == 0, "溢出后最旧被覆盖");
    TEST_ASSERT(strcmp(entries[3].message, "msg 5") == 0, "最新保留");
    lv_free((void **) &entries);

    lv_log_ring_buffer_destroy(rb);
    printf("  test_ringbuf_write_export_api: PASSED\n");
}

/* ============== 测试：清空/调整容量 ============== */

static void test_ringbuf_clear_resize_api(void) {
    lvLogRingBuffer *rb = lv_log_ring_buffer_create(4);
    TEST_ASSERT_NOT_NULL(rb);

    /* 写入 3 条后清空 */
    for (int i = 0; i < 3; i++) {
        lv_log_ring_buffer_write(rb, LOG_LEVEL_INFO, "t", "f", "c", i, "m%d", i);
    }
    lv_log_ring_buffer_clear(rb);
    int count = -1;
    lvLogEntry *entries = lv_log_ring_buffer_export(rb, &count);
    TEST_ASSERT_NULL(entries);
    TEST_ASSERT_EQ(count, 0);

    /* clear NULL 安全 */
    lv_log_ring_buffer_clear(NULL);

    /* resize 扩大容量：保留现有条目 */
    lv_log_ring_buffer_write(rb, LOG_LEVEL_INFO, "t", "f", "c", 1, "keep1");
    lv_log_ring_buffer_write(rb, LOG_LEVEL_INFO, "t", "f", "c", 2, "keep2");
    TEST_ASSERT(lv_log_ring_buffer_resize(rb, 8), "扩大容量成功");
    entries = lv_log_ring_buffer_export(rb, &count);
    TEST_ASSERT_NOT_NULL(entries);
    TEST_ASSERT_EQ(count, 2);
    TEST_ASSERT(strcmp(entries[0].message, "keep1") == 0, "扩大保留现有");
    lv_free((void **) &entries);

    /* resize 缩小容量：保留最新 3 条（n3, n4, n5），丢弃最旧 */
    for (int i = 0; i < 6; i++) {
        lv_log_ring_buffer_write(rb, LOG_LEVEL_INFO, "t", "f", "c", i, "n%d", i);
    }
    TEST_ASSERT(lv_log_ring_buffer_resize(rb, 3), "缩小容量成功");
    entries = lv_log_ring_buffer_export(rb, &count);
    TEST_ASSERT_NOT_NULL(entries);
    TEST_ASSERT_EQ(count, 3);
    TEST_ASSERT(strcmp(entries[0].message, "n3") == 0, "缩小保留最新");
    TEST_ASSERT(strcmp(entries[2].message, "n5") == 0, "最新在末尾");
    lv_free((void **) &entries);

    /* resize 非法参数 */
    TEST_ASSERT(!lv_log_ring_buffer_resize(rb, 0), "capacity<1 失败");
    TEST_ASSERT(!lv_log_ring_buffer_resize(NULL, 4), "NULL 失败");

    lv_log_ring_buffer_destroy(rb);
    printf("  test_ringbuf_clear_resize_api: PASSED\n");
}

/* ============== 测试：上下文日志 ============== */

static void test_log_with_context_api(void) {
    /* ctx NULL：仅写全局日志，不崩溃 */
    lv_log_with_context(NULL, LOG_LEVEL_INFO, "test_module", "test_fn", "test.c", 42, "ctx msg %d", 7);

    /* lv_LOG_CTX 宏展开路径 */
    lv_LOG_CTX(NULL, LOG_LEVEL_WARN, "test_module", "macro msg %s", "ok");

    /* 验证不崩溃且级别过滤后仍可写入（INFO 级别默认可见） */
    printf("  test_log_with_context_api: PASSED\n");
}

/* ============== 测试入口 ============== */

TEST_MAIN_BEGIN("Lv-00 Debug Log Ext Test Suite")
    printf("=== Lv-00 Debug Log Ext Test Suite (batch C-㊺续15) ===\n\n");
    lv_init();

    TEST_MAIN_RUN(test_ringbuf_create_api);
    TEST_MAIN_RUN(test_ringbuf_write_export_api);
    TEST_MAIN_RUN(test_ringbuf_clear_resize_api);
    TEST_MAIN_RUN(test_log_with_context_api);

    lv_cleanup();
TEST_MAIN_END()
