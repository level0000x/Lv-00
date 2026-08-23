/**
 * @file test_lv_process_ext.c
 * @brief 外部进程执行器契约测试（批次 C-㊺续35：lv_process.h 零覆盖 API）
 *
 * 覆盖零覆盖 API（2 个）：
 *   lv_external_process_available / lv_external_process_run
 *
 * 契约要点（与 lv_process.c 核对）：
 *   - available：NULL/空 → false；PATH 搜索。
 *   - run：任一输出参数 NULL → lv_ERROR_NULL_POINTER；argv[0] NULL →
 *     lv_ERROR_INVALID_PARAM；正常执行返回 lv_OK 且捕获 stdout。
 *
 * @author Lv-00 Project
 */

#include <stdio.h>
#include <string.h>

#include "lv/error_codes.h"
#include "lv/lv_process.h"
#include "lv/lv_utils.h"

#include "test_unified.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============== 测试：可用性检测 ============== */

static void test_available(void) {
    TEST_ASSERT(!lv_external_process_available(NULL), "NULL false");
    TEST_ASSERT(!lv_external_process_available(""), "empty false");

#ifdef _WIN32
    /* cmd 在 Windows PATH 中 */
    TEST_ASSERT(lv_external_process_available("cmd"), "cmd available");
#else
    /* sh 在 POSIX PATH 中 */
    TEST_ASSERT(lv_external_process_available("sh"), "sh available");
#endif

    /* 不存在的工具 */
    TEST_ASSERT(!lv_external_process_available("no-such-tool-xyz-123"), "missing false");
}

/* ============== 测试：执行外部进程 ============== */

static void test_run(void) {
    /* NULL 契约 */
    char *const argv0[] = {NULL};
    char *out = NULL;
    size_t out_len = 0;
    int exit_code = 0;

    TEST_ASSERT_EQ(lv_external_process_run(NULL, argv0, NULL, 0, 5000, &out, &out_len, &exit_code),
                   (int) lv_ERROR_NULL_POINTER);
    TEST_ASSERT_EQ(lv_external_process_run("cmd", NULL, NULL, 0, 5000, &out, &out_len, &exit_code),
                   (int) lv_ERROR_NULL_POINTER);
    TEST_ASSERT_EQ(lv_external_process_run("cmd", argv0, NULL, 0, 5000, NULL, &out_len, &exit_code),
                   (int) lv_ERROR_NULL_POINTER);

    /* argv[0] NULL → INVALID_PARAM */
    TEST_ASSERT_EQ(lv_external_process_run("cmd", argv0, NULL, 0, 5000, &out, &out_len, &exit_code),
                   (int) lv_ERROR_INVALID_PARAM);

    /* 正常执行：平台命令输出 hello（Windows cmd / POSIX sh） */
#ifdef _WIN32
    char *argv_echo[] = {"cmd", "/c", "echo", "hello", NULL};
    const char *echo_exe = "cmd";
#else
    char *argv_echo[] = {"sh", "-c", "echo hello", NULL};
    const char *echo_exe = "sh";
#endif
    int rc = lv_external_process_run(echo_exe, argv_echo, NULL, 0, 5000, &out, &out_len, &exit_code);
    TEST_ASSERT_EQ(rc, (int) lv_OK);
    TEST_ASSERT_EQ(exit_code, 0);
    TEST_ASSERT_NOT_NULL(out);
    if (out) {
        TEST_ASSERT(strstr(out, "hello") != NULL, "captured output contains hello");
        lv_free((void **) &out);
    }
}

/* ============== Main ============== */

TEST_MAIN_BEGIN("ProcessExt")

    printf("\n--- lv_process (zero-coverage) ---\n");
    TEST_MAIN_RUN(test_available);
    TEST_MAIN_RUN(test_run);

TEST_MAIN_END()
