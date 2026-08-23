/**
 * @file test_lv_version_ext.c
 * @brief 版本与日志级别契约测试（批次 C-㊺续35：lv.h 零覆盖 API）
 *
 * 覆盖零覆盖 API（5 个）：
 *   lv_version_major / minor / patch（宏）
 *   lv_get_log_level / lv_set_log_level
 *
 * 契约要点（与 lv.h / lv.c 核对）：
 *   - version_*：宏展开为 lv_VERSION_MAJOR/MINOR/PATCH 常量。
 *   - get_log_level：返回 s_lv_state.log_level（初始 0）。
 *   - set_log_level：设置并写回 config（若存在）。
 *
 * @author Lv-00 Project
 */

#include <stdio.h>

#include "lv.h"

#include "test_unified.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============== 测试：版本宏 ============== */

static void test_version_macros(void) {
    TEST_ASSERT_EQ(lv_version_major(), lv_VERSION_MAJOR);
    TEST_ASSERT_EQ(lv_version_minor(), lv_VERSION_MINOR);
    TEST_ASSERT_EQ(lv_version_patch(), lv_VERSION_PATCH);

    /* 与字符串版本一致 */
    TEST_ASSERT_STR_EQ(lv_VERSION_STRING, "1.1.0");
}

/* ============== 测试：日志级别 ============== */

static void test_log_level(void) {
    /* 初始值（静态零初始化） */
    int orig = lv_get_log_level();

    /* set/get 往返 */
    lv_set_log_level(3);
    TEST_ASSERT_EQ(lv_get_log_level(), 3);
    lv_set_log_level(1);
    TEST_ASSERT_EQ(lv_get_log_level(), 1);
    lv_set_log_level(0);
    TEST_ASSERT_EQ(lv_get_log_level(), 0);

    /* 恢复原值 */
    lv_set_log_level(orig);
    TEST_ASSERT_EQ(lv_get_log_level(), orig);
}

/* ============== Main ============== */

TEST_MAIN_BEGIN("VersionExt")

    printf("\n--- lv version/log-level (zero-coverage) ---\n");
    TEST_MAIN_RUN(test_version_macros);
    TEST_MAIN_RUN(test_log_level);

TEST_MAIN_END()
