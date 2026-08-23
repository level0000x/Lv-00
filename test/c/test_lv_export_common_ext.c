/**
 * @file test_lv_export_common_ext.c
 * @brief 导出公共工具契约测试（批次 C-㊺续35：lv_export_common.h 零覆盖 API）
 *
 * 覆盖零覆盖 API（2 个）：
 *   lv_export_xml_escape / lv_export_write_file
 *
 * 契约要点（与 lv_export_common.c 核对）：
 *   - xml_escape：& < > " ' → &amp; &lt; &gt; &quot; &apos;；
 *     src/dst NULL 或 dst_size 0 直接返回。
 *   - write_file：fopen "w" 写入；打开失败返回 -1；否则返回写入字节数。
 *
 * @author Lv-00 Project
 */

#include <stdio.h>
#include <string.h>

#include "lv/lv_export_common.h"

#include "test_unified.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============== 测试：XML 转义 ============== */

static void test_xml_escape(void) {
    char buf[128];

    /* 五实体转义 */
    lv_export_xml_escape("a<b&c\"d'e", buf, sizeof(buf));
    TEST_ASSERT_STR_EQ(buf, "a&lt;b&amp;c&quot;d&apos;e");

    /* 无特殊字符：原样 */
    lv_export_xml_escape("plain text", buf, sizeof(buf));
    TEST_ASSERT_STR_EQ(buf, "plain text");

    /* 截断：小缓冲 */
    lv_export_xml_escape("abcdef", buf, 4);
    TEST_ASSERT_STR_EQ(buf, "abc");

    /* NULL 契约：直接返回（dst 内容不变） */
    strcpy(buf, "keep");
    lv_export_xml_escape(NULL, buf, sizeof(buf));
    TEST_ASSERT_STR_EQ(buf, "keep");
    lv_export_xml_escape("x", NULL, 10);
    lv_export_xml_escape("x", buf, 0);
}

/* ============== 测试：文件写出 ============== */

static void test_write_file(void) {
    const char *path = "export_common_tmp.txt";
    const char *data = "hello export";

    /* 成功写入 */
    int n = lv_export_write_file(path, data, strlen(data));
    TEST_ASSERT_EQ(n, (int) strlen(data));

    /* 读回验证 */
    FILE *f = fopen(path, "rb");
    TEST_ASSERT_NOT_NULL(f);
    if (f) {
        char rb[64];
        size_t got = fread(rb, 1, sizeof(rb) - 1, f);
        rb[got] = '\0';
        fclose(f);
        TEST_ASSERT_STR_EQ(rb, data);
    }
    remove(path);

    /* data NULL：写 0 字节 */
    n = lv_export_write_file(path, NULL, 10);
    TEST_ASSERT_EQ(n, 0);
    remove(path);
}

/* ============== Main ============== */

TEST_MAIN_BEGIN("ExportCommonExt")

    printf("\n--- lv_export_common (zero-coverage) ---\n");
    TEST_MAIN_RUN(test_xml_escape);
    TEST_MAIN_RUN(test_write_file);

TEST_MAIN_END()
