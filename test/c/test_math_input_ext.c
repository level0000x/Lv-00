/**
 * @file test_math_input_ext.c
 * @brief 数学输入契约测试（批次 C-㊺续35：math_input.h 零覆盖 API）
 *
 * 覆盖零覆盖 API（2 个）：
 *   lv_math_input_parse / lv_math_input_detect_format
 *
 * 契约要点（与 math_input.c 核对）：
 *   - detect_format：NULL/空 → -1；$ 开头 → 1（LaTeX）；
 *     point/line/circle 前缀 → 2（GCLC）；否则 0。
 *   - parse：NULL 参数 → -1；LaTeX 去 $；其他去首尾空白；返回长度。
 *
 * @author Lv-00 Project
 */

#include <stdio.h>
#include <string.h>

#include "lv/math_input.h"

#include "test_unified.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============== 测试：格式检测 ============== */

static void test_detect_format(void) {
    TEST_ASSERT_EQ(lv_math_input_detect_format(NULL), -1);
    TEST_ASSERT_EQ(lv_math_input_detect_format(""), -1);

    /* LaTeX */
    TEST_ASSERT_EQ(lv_math_input_detect_format("$x^2$"), 1);
    TEST_ASSERT_EQ(lv_math_input_detect_format("$$x + y$$"), 1);

    /* GCLC */
    TEST_ASSERT_EQ(lv_math_input_detect_format("point A 0 0"), 2);
    TEST_ASSERT_EQ(lv_math_input_detect_format("line l AB"), 2);
    TEST_ASSERT_EQ(lv_math_input_detect_format("circle c O r"), 2);

    /* 纯文本 */
    TEST_ASSERT_EQ(lv_math_input_detect_format("x + y = 1"), 0);
}

/* ============== 测试：解析规范化 ============== */

static void test_parse(void) {
    char buf[128];

    /* LaTeX：去 $ */
    int n = lv_math_input_parse("$x^2$", buf, sizeof(buf));
    TEST_ASSERT_EQ(n, 3);
    TEST_ASSERT_STR_EQ(buf, "x^2");

    /* 纯文本：去首尾空白 */
    n = lv_math_input_parse("  hello world  ", buf, sizeof(buf));
    TEST_ASSERT_EQ(n, 11);
    TEST_ASSERT_STR_EQ(buf, "hello world");

    /* GCLC：trim 保留内容 */
    n = lv_math_input_parse("point A 0 0", buf, sizeof(buf));
    TEST_ASSERT_EQ(n, 11);
    TEST_ASSERT_STR_EQ(buf, "point A 0 0");

    /* NULL 契约 */
    TEST_ASSERT_EQ(lv_math_input_parse(NULL, buf, sizeof(buf)), -1);
    TEST_ASSERT_EQ(lv_math_input_parse("x", NULL, sizeof(buf)), -1);
    TEST_ASSERT_EQ(lv_math_input_parse("x", buf, 0), -1);
}

/* ============== Main ============== */

TEST_MAIN_BEGIN("MathInputExt")

    printf("\n--- math_input (zero-coverage) ---\n");
    TEST_MAIN_RUN(test_detect_format);
    TEST_MAIN_RUN(test_parse);

TEST_MAIN_END()
