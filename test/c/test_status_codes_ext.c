/**
 * @file test_status_codes_ext.c
 * @brief 状态码查询契约测试（批次 C-㊺续31：status_codes.h 零覆盖 API）
 *
 * 覆盖零覆盖 API（3 个）：
 *   lv_status_is_success / is_error / message
 *
 * 契约要点（与 status_codes.c 核对）：
 *   - is_success：code == 0 返回 1，其余 0。
 *   - is_error：code != 0 返回 1，其余 0。
 *   - message：委托统一错误表（lv_error_string）；未收录码返回 "未知状态码"。
 *
 * @author Lv-00 Project
 */

#include <stdio.h>
#include <string.h>

#include "lv/error_codes.h"
#include "lv/status_codes.h"

#include "test_unified.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============== 测试：成功/错误判定 ============== */

static void test_is_success_error(void) {
    /* 0 = 成功 */
    TEST_ASSERT_EQ(lv_status_is_success(0), 1);
    TEST_ASSERT_EQ(lv_status_is_error(0), 0);

    /* 正错误码 */
    TEST_ASSERT_EQ(lv_status_is_success(5), 0);
    TEST_ASSERT_EQ(lv_status_is_error(5), 1);

    /* 负错误码 */
    TEST_ASSERT_EQ(lv_status_is_success(-1), 0);
    TEST_ASSERT_EQ(lv_status_is_error(-1), 1);
}

/* ============== 测试：状态码消息 ============== */

static void test_status_message(void) {
    /* 已知状态码（0 = lv_OK）：非 NULL 且非未知回退文本 */
    const char *m0 = lv_status_message(0);
    TEST_ASSERT_NOT_NULL(m0);
    TEST_ASSERT(strcmp(m0, "未知状态码") != 0, "known code message");

    /* 已知错误码：非未知回退文本 */
    const char *mn = lv_status_message(lv_ERROR_NULL_POINTER);
    TEST_ASSERT_NOT_NULL(mn);
    TEST_ASSERT(strcmp(mn, "未知状态码") != 0, "known error message");

    /* 未知状态码：返回 "未知状态码" */
    const char *mu = lv_status_message(99999);
    TEST_ASSERT_NOT_NULL(mu);
    TEST_ASSERT_STR_EQ(mu, "未知状态码");
}

/* ============== Main ============== */

TEST_MAIN_BEGIN("StatusCodesExt")

    printf("\n--- status_codes (zero-coverage) ---\n");
    TEST_MAIN_RUN(test_is_success_error);
    TEST_MAIN_RUN(test_status_message);

TEST_MAIN_END()
