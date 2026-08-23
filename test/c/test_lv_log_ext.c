/**
 * @file test_lv_log_ext.c
 * @brief 日志系统配置契约测试（批次 C-㊺续30：lv_log.h 零覆盖 API）
 *
 * 覆盖零覆盖 API（4 个）：
 *   lv_log_get_level / set_output / enable_timestamp / enable_source
 *
 * 契约要点（与 lv_log.h / lv_log.c 核对）：
 *   - get_level：默认 lv_LOG_INFO。
 *   - set_output(fp)：非 NULL 输出重定向到 fp；NULL 恢复默认 stderr。
 *   - enable_timestamp(true)：输出前缀追加 [HH:MM:SS]。
 *   - enable_source(true)：输出前缀追加 源文件:行号（记录点 __FILE__，含 lv_log.c）。
 *   - lv_log：level > lv_LOG_FATAL 直接忽略（不输出）。
 *
 * 说明：本测试全部走"显式定制输出路径"（set_output + 开关），不依赖
 * lv_init/lv_log_message 主管道，进程内自包含；测试结束恢复全局状态，
 * 避免污染同进程后续调用。
 *
 * @author Lv-00 Project
 */

#include <stdio.h>
#include <string.h>

#include "lv/lv_log.h"

#include "test_unified.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============== 测试：默认日志级别 ============== */

static void test_get_level_default(void) {
    TEST_ASSERT_EQ((int) lv_log_get_level(), (int) lv_LOG_INFO);
}

/* ============== 测试：输出重定向 + 前缀开关 ============== */

static void test_output_customization(void) {
    FILE *f = tmpfile();
    TEST_ASSERT_NOT_NULL(f);

    /* 重定向到文件，开启时间戳与源位置 */
    lv_log_set_output(f);
    lv_log_enable_timestamp(true);
    lv_log_enable_source(true);

    lv_log(lv_LOG_ERROR, "marker-%d", 42);

    fflush(f);
    rewind(f);

    char buf[512];
    char *got = fgets(buf, sizeof(buf), f);
    TEST_ASSERT_NOT_NULL(got);

    /* 消息体 */
    TEST_ASSERT(strstr(buf, "marker-42") != NULL, "message body emitted");
    /* 时间戳前缀 [HH:MM:SS] */
    TEST_ASSERT(buf[0] == '[', "timestamp prefix present");
    /* 源位置前缀含 lv_log.c（记录点 __FILE__） */
    TEST_ASSERT(strstr(buf, "lv_log.c") != NULL, "source prefix present");

    /* 恢复默认：关闭前缀并还原输出 */
    lv_log_enable_timestamp(false);
    lv_log_enable_source(false);
    lv_log_set_output(NULL);
    fclose(f);

    /* 恢复后再记录不崩溃（定制路径输出到 stderr） */
    lv_log(lv_LOG_WARN, "restored-%d", 7);
}

/* ============== 测试：level > FATAL 忽略 ============== */

static void test_level_above_fatal_ignored(void) {
    FILE *f = tmpfile();
    TEST_ASSERT_NOT_NULL(f);

    lv_log_set_output(f);
    lv_log_enable_timestamp(false);
    lv_log_enable_source(false);

    /* level 超出枚举范围（> lv_LOG_FATAL）：直接忽略，不输出 */
    lv_log((lvLogLevel) 9, "ghost-message");

    fflush(f);
    rewind(f);
    char buf[64];
    TEST_ASSERT(fgets(buf, sizeof(buf), f) == NULL, "no output for level > FATAL");

    lv_log_set_output(NULL);
    fclose(f);
}

/* ============== Main ============== */

TEST_MAIN_BEGIN("LogExt")

    printf("\n--- lv_log (zero-coverage) ---\n");
    TEST_MAIN_RUN(test_get_level_default);
    TEST_MAIN_RUN(test_output_customization);
    TEST_MAIN_RUN(test_level_above_fatal_ignored);

TEST_MAIN_END()
