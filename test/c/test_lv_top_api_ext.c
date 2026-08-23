/**
 * @file test_lv_top_api_ext.c
 * @brief 顶层 API 契约测试（批次 C-㊺续36：lv.h 零覆盖 API）
 *
 * 覆盖零覆盖 API（8 个）：
 *   lv_are_assertions_enabled / lv_set_assertions_enabled
 *   lv_check_version_compat（本批修复硬编码 3）
 *   lv_config_set_bool / lv_config_set_string
 *   lv_normalize / lv_set_numeric_assumption
 *   lv_get_last_error（本批补齐声明与实现）
 *
 * 契约要点（与 lv.c / error_codes.c 核对）：
 *   - assertions：set/get 往返（s_lv_state 字段）。
 *   - check_version_compat：单库静态构建恒 true（修复后）。
 *   - config_set_bool/string：NULL key → false；未初始化 config → false。
 *   - normalize：engine 或 main_graph NULL → NULL。
 *   - set_numeric_assumption：engine/main_graph NULL → 错误码。
 *   - get_last_error：返回线程局部错误消息（非 NULL）。
 *
 * @author Lv-00 Project
 */

#include <stdio.h>
#include <string.h>

#include "lv.h"
#include "lv/engine.h"
#include "lv/error_codes.h"

#include "test_unified.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============== 测试：断言开关 ============== */

static void test_assertions(void) {
    bool orig = lv_are_assertions_enabled();

    lv_set_assertions_enabled(true);
    TEST_ASSERT(lv_are_assertions_enabled(), "enabled");
    lv_set_assertions_enabled(false);
    TEST_ASSERT(!lv_are_assertions_enabled(), "disabled");

    lv_set_assertions_enabled(orig);
    TEST_ASSERT(lv_are_assertions_enabled() == orig, "restored");
}

/* ============== 测试：版本兼容 ============== */

static void test_version_compat(void) {
    /* 单库静态构建：运行时与编译头同源，恒兼容（修复后） */
    TEST_ASSERT(lv_check_version_compat(), "version compatible");
}

/* ============== 测试：配置 ============== */

static void test_config(void) {
    /* NULL key 拒绝 */
    TEST_ASSERT(!lv_config_set_bool(NULL, true), "NULL key");
    TEST_ASSERT(!lv_config_set_string(NULL, "x"), "NULL key");

    /* 未初始化 config：返回 false（或 A 键命中）。不崩即可 */
    lv_config_set_bool("some.key.bool", true);
    lv_config_set_string("some.key.str", "v");
}

/* ============== 测试：normalize / numeric assumption ============== */

static void test_normalize_and_assumption(void) {
    /* NULL 契约 */
    TEST_ASSERT_NULL(lv_normalize(NULL, false));
    TEST_ASSERT(lv_set_numeric_assumption(NULL, 0, 0.1, "d") != 0, "NULL engine errors");

    /* 空引擎（main_graph NULL） */
    lvEngine engine;
    memset(&engine, 0, sizeof(engine));
    TEST_ASSERT_NULL(lv_normalize(&engine, false));
    TEST_ASSERT(lv_set_numeric_assumption(&engine, 0, 0.1, "d") != 0, "no graph errors");
}

/* ============== 测试：get_last_error ============== */

static void test_get_last_error(void) {
    /* 未设置错误时返回默认描述或空消息（非 NULL） */
    const char *msg = lv_get_last_error();
    TEST_ASSERT_NOT_NULL(msg);

    /* 与 message 别名一致 */
    TEST_ASSERT(msg == lv_get_last_error_message() || strcmp(msg, lv_get_last_error_message()) == 0,
                "alias consistent");
}

/* ============== Main ============== */

TEST_MAIN_BEGIN("TopApiExt")

    printf("\n--- lv.h top API (zero-coverage) ---\n");
    TEST_MAIN_RUN(test_assertions);
    TEST_MAIN_RUN(test_version_compat);
    TEST_MAIN_RUN(test_config);
    TEST_MAIN_RUN(test_normalize_and_assumption);
    TEST_MAIN_RUN(test_get_last_error);

TEST_MAIN_END()
