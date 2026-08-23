/**
 * @file test_lv_event_bus_ext.c
 * @brief 事件总线契约测试（批次 C-㊺续28：lv_event_bus.h 7 个零覆盖 API）
 *
 * 覆盖：init / cleanup / subscribe / unsubscribe / emit / set_stream /
 *   get_stream
 * 契约：订阅回调按事件类型分发、-1 监听所有、unsubscribe 生效。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_unified.h"
#include "lv/lv_event_bus.h"

int g_pass_count = 0;
int g_fail_count = 0;

typedef struct {
    int last_type;
    int call_count;
    void *last_data;
} Recorder;

static void cb_recv(int event_type, void *event_data, void *user_data) {
    Recorder *r = (Recorder *) user_data;
    r->last_type = event_type;
    r->call_count++;
    r->last_data = event_data;
}

static void test_event_bus_subscribe_api(void) {
    lvEventBus bus;
    memset(&bus, 0, sizeof(bus)); /* 预置零初始化（同 runtime_monitor 静态全局） */
    lvEventBusConfig cfg = {.initial_capacity = 4, .max_callbacks = 0};
    lv_event_bus_init(&bus, &cfg);
    TEST_ASSERT_NULL(lv_event_bus_get_stream(&bus));

    Recorder r1 = {0, 0, NULL};
    Recorder r2 = {0, 0, NULL};

    /* 订阅：事件 100 */
    int sid1 = lv_event_subscribe(&bus, 100, cb_recv, &r1);
    TEST_ASSERT(sid1 > 0, "x");
    /* 订阅：事件 200 */
    int sid2 = lv_event_subscribe(&bus, 200, cb_recv, &r2);
    TEST_ASSERT(sid2 > 0, "x");

    /* emit 100 → 仅 r1 */
    int data = 42;
    lv_event_emit(&bus, 100, &data);
    TEST_ASSERT_EQ(r1.call_count, 1);
    TEST_ASSERT_EQ(r1.last_type, 100);
    TEST_ASSERT_EQ(r1.last_data, &data);
    TEST_ASSERT_EQ(r2.call_count, 0);

    /* emit 200 → 仅 r2 */
    lv_event_emit(&bus, 200, NULL);
    TEST_ASSERT_EQ(r2.call_count, 1);
    TEST_ASSERT_EQ(r2.last_type, 200);

    /* emit 300 → 无订阅者 */
    lv_event_emit(&bus, 300, NULL);
    TEST_ASSERT_EQ(r1.call_count, 1);
    TEST_ASSERT_EQ(r2.call_count, 1);

    /* unsubscribe */
    TEST_ASSERT(lv_event_unsubscribe(&bus, sid1), "退订成功");
    lv_event_emit(&bus, 100, NULL);
    TEST_ASSERT_EQ(r1.call_count, 1); /* 不再触发 */
    TEST_ASSERT(!lv_event_unsubscribe(&bus, sid1), "重复退订失败");
    TEST_ASSERT(!lv_event_unsubscribe(&bus, 0), "非法 ID 失败");

    /* NULL 契约：lv_RETURN_ERROR 宏统一返回 -1 */
    TEST_ASSERT_EQ(lv_event_subscribe(NULL, 100, cb_recv, &r1), -1);
    TEST_ASSERT_EQ(lv_event_subscribe(&bus, 100, NULL, &r1), -1);
    TEST_ASSERT(!lv_event_unsubscribe(NULL, sid2), "NULL bus 退订失败");
    lv_event_emit(NULL, 100, NULL);
    lv_event_bus_cleanup(NULL);

    lv_event_bus_cleanup(&bus);
    printf("  test_event_bus_subscribe_api: PASSED\n");
}

static void test_event_bus_wildcard_stream_api(void) {
    lvEventBus bus;
    memset(&bus, 0, sizeof(bus));
    lv_event_bus_init(&bus, NULL); /* 默认配置 */
    TEST_ASSERT_NULL(lv_event_bus_get_stream(&bus));

    Recorder all = {0, 0, NULL};
    /* -1 = 监听所有 */
    int sid = lv_event_subscribe(&bus, -1, cb_recv, &all);
    TEST_ASSERT(sid > 0, "x");

    lv_event_emit(&bus, 1, NULL);
    lv_event_emit(&bus, 2, NULL);
    lv_event_emit(&bus, 3, NULL);
    TEST_ASSERT_EQ(all.call_count, 3);

    /* set/get_stream */
    void *fake_ctx = (void *) 0x1234; /* 仅指针往返，不解引用 */
    lv_event_bus_set_stream(&bus, (struct StreamContext *) fake_ctx);
    TEST_ASSERT_EQ((void *) lv_event_bus_get_stream(&bus), fake_ctx);
    lv_event_bus_set_stream(&bus, NULL);
    TEST_ASSERT_NULL(lv_event_bus_get_stream(&bus));
    lv_event_bus_set_stream(NULL, (struct StreamContext *) fake_ctx);
    TEST_ASSERT_NULL(lv_event_bus_get_stream(NULL));

    lv_event_bus_cleanup(&bus);
    printf("  test_event_bus_wildcard_stream_api: PASSED\n");
}

TEST_MAIN_BEGIN("Lv-00 Event Bus Ext Test Suite")
    printf("=== Lv-00 Event Bus Ext Test Suite (batch C-㊺续28) ===\n\n");
    lv_init();
    TEST_MAIN_RUN(test_event_bus_subscribe_api);
    TEST_MAIN_RUN(test_event_bus_wildcard_stream_api);
    lv_cleanup();
TEST_MAIN_END()
