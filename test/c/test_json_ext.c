/**
 * @file test_json_ext.c
 * @brief JSON 库契约测试（批次 C-㊺续：lv_json.h 23 个零覆盖 API）
 *
 * 覆盖 23 个 ctest 零覆盖 API：
 *   - 构建族：lv_json_buf_append_char / _append_fmt / _append_raw /
 *     _append_raw_value / _begin_value / _ensure
 *   - 解析族：lv_json_expect / _get_bool / _get_double / _get_int /
 *     _get_string / _next / _parse_bool / _parse_double /
 *     _parse_double_array / _parse_field / _parse_int / _parse_int64 /
 *     _parse_int_array / _parse_uint64 / _peek / _skip_value / _skip_ws
 *
 * 契约要点（与头注释核对）：
 *   - parse_int64 溢出返回 false；parse_uint64 拒绝负号 + 溢出 false。
 *   - parse_field 空对象/结束返回 false、尾部逗号容忍、键非字符串 false。
 *   - parse_int_array/double_array 越界 max_count 时截断跳过剩余。
 *   - lv_json_buf_begin_value 元素分隔（数组元素间逗号）。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_unified.h"
#include "lv/lv_json.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============== 测试：写入缓冲构建 ============== */

static void test_buf_build_api(void) {
    lvJsonBuf buf;
    TEST_ASSERT(lv_json_buf_init(&buf, 16), "buf init");

    /* append_char / append_raw / append_fmt */
    lv_json_buf_append_char(&buf, '{');
    lv_json_buf_append_raw(&buf, "\"a\":");
    lv_json_buf_append_fmt(&buf, "%d", 42);
    lv_json_buf_append_char(&buf, '}');

    /* ensure 扩容（写入超过初始容量） */
    lv_json_buf_ensure(&buf, 1000);
    for (int i = 0; i < 200; i++)
        lv_json_buf_append_char(&buf, 'x');

    char *s = lv_json_buf_finalize(&buf);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT(strstr(s, "\"a\":42") != NULL, "raw+fmt 拼接");
    TEST_ASSERT(strlen(s) >= 206, "扩容后内容完整");
    lv_free((void **)&s);

    /* begin_value 分隔符：数组元素间逗号 */
    lvJsonBuf buf2;
    TEST_ASSERT(lv_json_buf_init(&buf2, 16), "buf2 init");
    lv_json_buf_append_char(&buf2, '[');
    lv_json_buf_begin_value(&buf2);
    lv_json_buf_append_raw(&buf2, "1");
    lv_json_buf_begin_value(&buf2);
    lv_json_buf_append_raw(&buf2, "2");
    lv_json_buf_append_char(&buf2, ']');
    s = lv_json_buf_finalize(&buf2);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_STR_EQ(s, "[1,2]");
    lv_free((void **)&s);

    /* append_raw_value：分隔 + raw 一步 */
    lvJsonBuf buf3;
    TEST_ASSERT(lv_json_buf_init(&buf3, 16), "buf3 init");
    lv_json_buf_append_char(&buf3, '[');
    lv_json_buf_append_raw_value(&buf3, "\"x\"");
    lv_json_buf_append_raw_value(&buf3, "null");
    lv_json_buf_append_char(&buf3, ']');
    s = lv_json_buf_finalize(&buf3);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_STR_EQ(s, "[\"x\",null]");
    lv_free((void **)&s);

    printf("  test_buf_build_api: PASSED\n");
}

/* ============== 测试：标量解析 ============== */

static void test_parse_scalar_api(void) {
    /* parse_int：正/负/非法 */
    lvJsonParser p;
    lv_json_parser_init(&p, "42 -7 abc", 9);
    int v = 0;
    TEST_ASSERT(lv_json_parse_int(&p, &v), "解析 42");
    TEST_ASSERT_EQ(v, 42);
    TEST_ASSERT(lv_json_parse_int(&p, &v), "解析 -7");
    TEST_ASSERT_EQ(v, -7);
    TEST_ASSERT(!lv_json_parse_int(&p, &v), "非数字失败");

    /* parse_int64：溢出 false */
    lv_json_parser_init(&p, "99999999999999999999999", 23);
    int64_t v64 = 0;
    TEST_ASSERT(!lv_json_parse_int64(&p, &v64), "int64 溢出");
    lv_json_parser_init(&p, "1234567890123", 13);
    TEST_ASSERT(lv_json_parse_int64(&p, &v64), "解析 int64");
    TEST_ASSERT_EQ(v64, 1234567890123LL);

    /* parse_uint64：拒绝负号 */
    lv_json_parser_init(&p, "-5", 2);
    uint64_t u64 = 0;
    TEST_ASSERT(!lv_json_parse_uint64(&p, &u64), "uint64 拒绝负号");
    lv_json_parser_init(&p, "777", 3);
    TEST_ASSERT(lv_json_parse_uint64(&p, &u64), "解析 uint64");
    TEST_ASSERT_EQ(u64, 777);

    /* parse_double */
    lv_json_parser_init(&p, "3.25", 4);
    double d = 0;
    TEST_ASSERT(lv_json_parse_double(&p, &d), "解析 double");
    TEST_ASSERT_EQ(d, 3.25);

    /* parse_bool */
    lv_json_parser_init(&p, "true false x", 12);
    bool b = false;
    TEST_ASSERT(lv_json_parse_bool(&p, &b), "解析 true");
    TEST_ASSERT(b, "true 解析");
    TEST_ASSERT(lv_json_parse_bool(&p, &b), "解析 false");
    TEST_ASSERT(!b, "false 解析");
    TEST_ASSERT(!lv_json_parse_bool(&p, &b), "非法布尔");

    printf("  test_parse_scalar_api: PASSED\n");
}

/* ============== 测试：游标 + 字符串 + 跳过 ============== */

static void test_cursor_string_api(void) {
    /* peek/next/expect/skip_ws */
    lvJsonParser p;
    lv_json_parser_init(&p, "  {a}", 5);
    lv_json_skip_ws(&p);
    TEST_ASSERT_EQ(lv_json_peek(&p), '{');
    TEST_ASSERT(lv_json_expect(&p, '{'), "期望 {");
    TEST_ASSERT_EQ(lv_json_next(&p), 'a');
    TEST_ASSERT(!lv_json_expect(&p, 'z'), "期望不匹配");

    /* parse_string：转义 */
    lv_json_parser_init(&p, "\"he\\\"llo\\n\"", 11);
    char *str = lv_json_parse_string(&p);
    TEST_ASSERT_NOT_NULL(str);
    TEST_ASSERT_STR_EQ(str, "he\"llo\n");
    lv_free((void **)&str);

    /* skip_value：对象/数组/字符串 */
    lv_json_parser_init(&p, "{\"a\":[1,2]}\"tail\"", 17);
    lv_json_skip_ws(&p);
    lv_json_skip_value(&p); /* 跳过对象 */
    lv_json_skip_ws(&p);
    TEST_ASSERT_EQ(lv_json_peek(&p), '"');

    printf("  test_cursor_string_api: PASSED\n");
}

/* ============== 测试：字段与数组解析 ============== */

static void test_parse_field_array_api(void) {
    /* parse_field：对象字段遍历 */
    lvJsonParser p;
    const char *obj = "{\"id\":1,\"name\":\"x\"}";
    lv_json_parser_init(&p, obj, strlen(obj));
    lv_json_skip_ws(&p);
    TEST_ASSERT(lv_json_expect(&p, '{'), "对象开始");
    char *key = NULL;
    int seen_id = 0, seen_name = 0;
    while (lv_json_parse_field(&p, &key)) {
        if (strcmp(key, "id") == 0) {
            int v = 0;
            TEST_ASSERT(lv_json_parse_int(&p, &v), "解析 id");
            TEST_ASSERT_EQ(v, 1);
            seen_id = 1;
        } else if (strcmp(key, "name") == 0) {
            char *val = lv_json_parse_string(&p);
            TEST_ASSERT_NOT_NULL(val);
            TEST_ASSERT_STR_EQ(val, "x");
            lv_free((void **)&val);
            seen_name = 1;
        } else {
            lv_json_skip_value(&p);
        }
        lv_free((void **)&key);
    }
    TEST_ASSERT(seen_id && seen_name, "两个字段均解析");

    /* 空对象 → parse_field false */
    lv_json_parser_init(&p, "{}", 2);
    lv_json_skip_ws(&p);
    TEST_ASSERT(lv_json_expect(&p, '{'), "空对象开始");
    TEST_ASSERT(!lv_json_parse_field(&p, &key), "空对象无字段");

    /* parse_int_array：正路径 + 越界截断 */
    lv_json_parser_init(&p, "[1,2,3,4]", 9);
    int arr[2];
    size_t cnt = 0;
    TEST_ASSERT(lv_json_parse_int_array(&p, arr, 2, &cnt), "解析 int 数组");
    TEST_ASSERT_EQ(cnt, 2);
    TEST_ASSERT_EQ(arr[0], 1);
    TEST_ASSERT_EQ(arr[1], 2);

    /* parse_double_array */
    lv_json_parser_init(&p, "[1.5,2.5]", 9);
    double darr[2];
    cnt = 0;
    TEST_ASSERT(lv_json_parse_double_array(&p, darr, 2, &cnt), "解析 double 数组");
    TEST_ASSERT_EQ(cnt, 2);
    TEST_ASSERT_EQ(darr[0], 1.5);

    printf("  test_parse_field_array_api: PASSED\n");
}

/* ============== 测试：顶层键查询 ============== */

static void test_get_api(void) {
    const char *json = "{\"a\":42,\"b\":3.5,\"c\":true,\"s\":\"hi\"}";

    int iv = 0;
    TEST_ASSERT(lv_json_get_int(json, "a", &iv), "获取 int");
    TEST_ASSERT_EQ(iv, 42);
    TEST_ASSERT(!lv_json_get_int(json, "nope", &iv), "键不存在");

    double dv = 0;
    TEST_ASSERT(lv_json_get_double(json, "b", &dv), "获取 double");
    TEST_ASSERT_EQ(dv, 3.5);

    bool bv = false;
    TEST_ASSERT(lv_json_get_bool(json, "c", &bv), "获取 true");
    TEST_ASSERT(bv, "布尔值 true");

    char out[16];
    TEST_ASSERT(lv_json_get_string(json, "s", out, sizeof(out)), "获取 string");
    TEST_ASSERT_STR_EQ(out, "hi");

    /* NULL 契约：json/key NULL → false */
    TEST_ASSERT(!lv_json_get_int(NULL, "a", &iv), "NULL json");
    TEST_ASSERT(!lv_json_get_int(json, NULL, &iv), "NULL key");

    printf("  test_get_api: PASSED\n");
}

/* ============== 测试入口 ============== */

TEST_MAIN_BEGIN("Lv-00 JSON Ext Test Suite")
    printf("=== Lv-00 JSON Ext Test Suite (batch C-㊺续) ===\n\n");
    lv_init();

    TEST_MAIN_RUN(test_buf_build_api);
    TEST_MAIN_RUN(test_parse_scalar_api);
    TEST_MAIN_RUN(test_cursor_string_api);
    TEST_MAIN_RUN(test_parse_field_array_api);
    TEST_MAIN_RUN(test_get_api);

    lv_cleanup();
TEST_MAIN_END()
