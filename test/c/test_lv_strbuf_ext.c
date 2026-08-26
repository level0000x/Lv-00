/**
 * @file test_lv_strbuf_ext.c
 * @brief 字符串缓冲契约测试（批次 C-㊺续29：lv_strbuf.h 2 个零覆盖 API）
 *
 * 覆盖：reset / vprintf
 * 契约：vprintf 格式化追加、reset 清空但缓冲可复用。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "test_unified.h"
#include "lv/lv_strbuf.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* 辅助：通过 va_list 调用 vprintf */
static void append_fmt(lvStrBuf *sb, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    lv_strbuf_vprintf(sb, fmt, args);
    va_end(args);
}

static void test_strbuf_vprintf_api(void) {
    lvStrBuf sb = {0};

    /* vprintf 格式化追加 */
    append_fmt(&sb, "val=%d", 42);
    append_fmt(&sb, " str=%s", "hi");
    TEST_ASSERT(strcmp(lv_strbuf_cstr(&sb), "val=42 str=hi") == 0, "vprintf 追加内容");

    /* NULL 契约（glibc 中 va_list 是数组类型，不能 cast NULL，
       改用清零的 va_list 变量以兼容 GCC -Wpedantic 的数组 cast 检查） */
    va_list va_empty;
    memset(&va_empty, 0, sizeof(va_empty));
    lv_strbuf_vprintf(&sb, NULL, va_empty);
    lv_strbuf_vprintf(NULL, "x", va_empty);

    /* reset 清空 */
    lv_strbuf_reset(&sb);
    TEST_ASSERT(strlen(lv_strbuf_cstr(&sb)) == 0, "reset 后空");
    lv_strbuf_reset(NULL);

    /* reset 后可复用 */
    append_fmt(&sb, "after reset %d", 7);
    TEST_ASSERT(strcmp(lv_strbuf_cstr(&sb), "after reset 7") == 0, "reset 后复用");

    lv_strbuf_destroy(&sb);
    printf("  test_strbuf_vprintf_api: PASSED\n");
}

TEST_MAIN_BEGIN("Lv-00 StrBuf Ext Test Suite")
    printf("=== Lv-00 StrBuf Ext Test Suite (batch C-㊺续29) ===\n\n");
    lv_init();
    TEST_MAIN_RUN(test_strbuf_vprintf_api);
    lv_cleanup();
TEST_MAIN_END()
