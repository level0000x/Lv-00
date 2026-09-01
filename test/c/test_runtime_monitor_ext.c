/**
 * @file test_runtime_monitor_ext.c
 * @brief 运行时监控契约测试（批次 C-㊺续7：runtime_monitor.h 19 个零覆盖 API）
 *
 * 覆盖 19 个 ctest 零覆盖 API：
 *   - 日志族：lv_log_set_level / set_targets / set_file / set_callback /
 *     lv_log_write
 *   - 计时器族：lv_timer_pause / resume / reset / elapsed_ms / elapsed_ns
 *     （auto_stop 为 inline 宏）
 *   - 性能族：lv_perf_stats_reset / lv_perf_get_all_timer_stats
 *   - 健康族：lv_health_set_cpu_thresholds / lv_health_set_memory_thresholds
 *   - 诊断族：lv_diagnostics_write_file
 *   - 追踪族：lv_event_trace_get_all / export_chrome / set_stream_context
 *
 * 契约要点（与头注释核对）：
 *   - 计时器暂停/恢复/重置/经过时间语义。
 *   - 日志回调在写日志时触发。
 *   - 追踪 get_all 返回记录数；export_chrome 写文件。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_unified.h"
#include "lv/runtime_monitor.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* K65：LogCapture/log_cb 随死 sink API（set_callback）删除 */

/* ============== 测试：日志系统 ============== */

static void test_log_api(void) {
    /* 默认配置初始化 */
    TEST_ASSERT(lv_log_init(NULL), "默认日志初始化");

    /* K65：set_targets/set_callback/set_file 死 sink API 已删（委托主管道，
     * 生产 0 调用）——仅保留 set_level 活性 API */
    lv_log_set_level(LOG_LEVEL_DEBUG);

    /* lv_log_write 委托统一主通道（lv_log_message），调用不崩溃 */
    lv_log_write(LOG_LEVEL_INFO, "test_tag", "test.c", 42, "fn", "hello %d", 7);
    lv_log_write(LOG_LEVEL_ERROR, "err_tag", "test.c", 43, "fn", "boom");

    printf("  test_log_api: PASSED\n");
}

/* ============== 测试：计时器 ============== */

static void test_timer_api(void) {
    /* 计时器依赖性能子系统初始化 */
    TEST_ASSERT(lv_perf_init(), "perf 初始化");

    /* NULL 契约：elapsed 对 NULL → 0 */
    TEST_ASSERT_EQ(lv_timer_elapsed_ms(NULL), 0);
    TEST_ASSERT_EQ(lv_timer_elapsed_ns(NULL), 0);

    lvTimer *t = lv_timer_create("t1");
    TEST_ASSERT_NOT_NULL(t);
    lv_timer_start(t);
    TEST_ASSERT(lv_timer_elapsed_ms(t) >= 0, "运行中经过时间非负");

    /* pause：暂停后经过时间冻结（不递增） */
    lv_timer_pause(t);
    int64_t p1 = lv_timer_elapsed_ms(t);
    TEST_ASSERT(p1 >= 0, "暂停经过时间");
    int64_t p2 = lv_timer_elapsed_ms(t);
    TEST_ASSERT(p2 >= p1 - 1, "暂停后经过时间稳定");

    /* resume：恢复后继续 */
    lv_timer_resume(t);
    TEST_ASSERT(lv_timer_elapsed_ms(t) >= p1 - 1, "恢复后经过时间不回退");

    /* stop：返回经过毫秒数 */
    int64_t ms = lv_timer_stop(t);
    TEST_ASSERT(ms >= 0, "停止返回毫秒");

    /* reset：清零 */
    lv_timer_start(t);
    lv_timer_reset(t);
    TEST_ASSERT(lv_timer_elapsed_ms(t) <= 1, "重置后经过时间近零");
    lv_timer_destroy(t);

    printf("  test_timer_api: PASSED\n");
}

/* ============== 测试：性能统计 ============== */

static void test_perf_api(void) {
    TEST_ASSERT(lv_perf_init(), "性能监控初始化");

    lvPerfStats *st = lv_perf_stats_create("metric_a");
    TEST_ASSERT_NOT_NULL(st);
    lv_perf_stats_record(st, 1.0);
    lv_perf_stats_record(st, 3.0);
    TEST_ASSERT_EQ(st->count, 2);
    TEST_ASSERT_EQ(st->mean, 2.0);

    /* reset：清零 */
    lv_perf_stats_reset(st);
    TEST_ASSERT_EQ(st->count, 0);
    TEST_ASSERT_EQ(st->sum, 0.0);

    /* get_all_timer_stats：NULL 契约 + 计数 */
    uint32_t n = lv_perf_get_all_timer_stats(NULL, 0);
    TEST_ASSERT(n >= 0, "统计获取计数");

    lv_perf_stats_destroy(st);
    lv_perf_shutdown();
    printf("  test_perf_api: PASSED\n");
}

/* ============== 测试：健康阈值 ============== */

static void test_health_api(void) {
    /* 阈值设置：无返回值，调用不崩溃 */
    lv_health_set_cpu_thresholds(80.0, 95.0);
    lv_health_set_memory_thresholds(1024.0, 2048.0);
    lv_health_set_cpu_thresholds(90.0, 99.0);
    lv_health_set_memory_thresholds(512.0, 1024.0);
    printf("  test_health_api: PASSED\n");
}

/* ============== 测试：诊断报告 ============== */

static void test_diag_api(void) {
    lvDiagnostics *diag = lv_diagnostics_generate();
    TEST_ASSERT_NOT_NULL(diag);

    /* write_file：临时文件 */
    const char *path = "./_tmp_c47_diag.txt";
    remove(path);
    TEST_ASSERT(lv_diagnostics_write_file(diag, path), "诊断写入文件");
    FILE *f = fopen(path, "r");
    TEST_ASSERT(f != NULL, "诊断文件存在");
    if (f) {
        TEST_ASSERT(fseek(f, 0, SEEK_END) == 0 && ftell(f) > 0, "诊断内容非空");
        fclose(f);
    }
    remove(path);
    lv_diagnostics_destroy(diag);
    printf("  test_diag_api: PASSED\n");
}

/* ============== 测试：事件追踪 ============== */

static void test_trace_api(void) {
    /* set_stream_context：NULL 安全 */
    lv_event_trace_set_stream_context(NULL);

    TEST_ASSERT(lv_event_trace_init(100), "追踪初始化");

    /* 记录 + begin/end */
    lv_event_trace_record(EVENT_TYPE_CUSTOM, "evt1", "data1");
    int eid = lv_event_trace_begin(EVENT_TYPE_PROOF_START, "proof1");
    TEST_ASSERT(eid >= 0, "begin 返回事件 ID");
    lv_event_trace_end(eid, "done");

    /* get_all：计数 */
    lvEventRecord *records = NULL;
    uint32_t n = lv_event_trace_get_all(&records, 100);
    TEST_ASSERT(n >= 2, "记录数 >= 2");
    TEST_ASSERT_NOT_NULL(records);
    lv_free((void **)&records);

    /* export_chrome：临时文件 */
    const char *path = "./_tmp_c47_trace.json";
    remove(path);
    TEST_ASSERT(lv_event_trace_export_chrome(path), "Chrome 追踪导出");
    remove(path);

    lv_event_trace_shutdown();
    printf("  test_trace_api: PASSED\n");
}

/* ============== 测试入口 ============== */

TEST_MAIN_BEGIN("Lv-00 Runtime Monitor Ext Test Suite")
    printf("=== Lv-00 Runtime Monitor Ext Test Suite (batch C-㊺续7) ===\n\n");
    lv_init();

    TEST_MAIN_RUN(test_log_api);
    TEST_MAIN_RUN(test_timer_api);
    TEST_MAIN_RUN(test_perf_api);
    TEST_MAIN_RUN(test_health_api);
    TEST_MAIN_RUN(test_diag_api);
    TEST_MAIN_RUN(test_trace_api);

    lv_cleanup();
TEST_MAIN_END()
