/**
 * @file test_json_buf.c
 * @brief lvJsonBuf 对象级写入 API 测试
 *
 * 覆盖：
 *   (a) int/double/bool/null 标量值写入；
 *   (b) 字符串与键名含引号/反斜杠/换行/tab 的 JSON 转义；
 *   (c) 对象/数组嵌套混合 + 空对象/空数组；
 *   (d) 同一对象内 key 写入顺序保持；
 *   (e) pretty 模式缩进输出与紧凑输出、写入中开关切换；
 *   (f) 深度超过 64 层时缩进按 64 层钳制。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "test_unified.h"

int g_pass_count = 0;
int g_fail_count = 0;

#include "lv.h"
#include "lv/lv_json.h"

/* ---------- (a) 标量值：int/double/bool/null ---------- */

static void test_scalar_values(void) {
    printf("Testing scalar values...\n");

    lvJsonBuf w;
    lv_ASSERT(lv_json_buf_init(&w, 64));
    lv_ASSERT(lv_json_buf_begin_object(&w));
    lv_ASSERT(lv_json_buf_append_key(&w, "int_pos"));
    lv_ASSERT(lv_json_buf_append_int(&w, 42));
    lv_ASSERT(lv_json_buf_append_key(&w, "int_neg"));
    lv_ASSERT(lv_json_buf_append_int(&w, -7));
    lv_ASSERT(lv_json_buf_append_key(&w, "int_max"));
    lv_ASSERT(lv_json_buf_append_int(&w, 9223372036854775807LL));
    lv_ASSERT(lv_json_buf_append_key(&w, "double_a"));
    lv_ASSERT(lv_json_buf_append_double(&w, 3.14));
    lv_ASSERT(lv_json_buf_append_key(&w, "double_neg"));
    lv_ASSERT(lv_json_buf_append_double(&w, -0.5));
    lv_ASSERT(lv_json_buf_append_key(&w, "bool_t"));
    lv_ASSERT(lv_json_buf_append_bool(&w, true));
    lv_ASSERT(lv_json_buf_append_key(&w, "bool_f"));
    lv_ASSERT(lv_json_buf_append_bool(&w, false));
    lv_ASSERT(lv_json_buf_append_key(&w, "nil"));
    lv_ASSERT(lv_json_buf_append_null(&w));
    lv_ASSERT(lv_json_buf_end_object(&w));

    lv_ASSERT(strcmp(w.buffer,
                  "{\"int_pos\":42,\"int_neg\":-7,\"int_max\":9223372036854775807,"
                  "\"double_a\":3.14,\"double_neg\":-0.5,\"bool_t\":true,"
                  "\"bool_f\":false,\"nil\":null}") == 0);
    lv_json_buf_free(&w);

    /* 数组形式：逗号自动管理 */
    lv_ASSERT(lv_json_buf_init(&w, 64));
    lv_ASSERT(lv_json_buf_begin_array(&w));
    lv_ASSERT(lv_json_buf_append_int(&w, 1));
    lv_ASSERT(lv_json_buf_append_int(&w, 2));
    lv_ASSERT(lv_json_buf_append_int(&w, 3));
    lv_ASSERT(lv_json_buf_end_array(&w));
    lv_ASSERT_STR_EQ(w.buffer, "[1,2,3]");
    lv_json_buf_free(&w);

    printf("  PASSED\n");
}

/* ---------- (b) 字符串/键名转义 ---------- */

static void test_string_escape(void) {
    printf("Testing string escaping...\n");

    lvJsonBuf w;
    lv_ASSERT(lv_json_buf_init(&w, 64));
    lv_ASSERT(lv_json_buf_begin_object(&w));

    /* 值含引号、反斜杠、换行、tab：he said "hi"␊⇥\ done */
    lv_ASSERT(lv_json_buf_append_key(&w, "msg"));
    lv_json_buf_append_string(&w, "he said \"hi\"\n\t\\ done");

    /* 键名同样转义：k"ey\ + 换行 */
    lv_ASSERT(lv_json_buf_append_key(&w, "k\"ey\\\n"));
    lv_ASSERT(lv_json_buf_append_int(&w, 0));

    lv_ASSERT(lv_json_buf_end_object(&w));
    lv_ASSERT_STR_EQ(w.buffer, "{\"msg\":\"he said \\\"hi\\\"\\n\\t\\\\ done\",\"k\\\"ey\\\\\\n\":0}");
    lv_json_buf_free(&w);

    /* 空字符串与带控制字符（\x01 转成 \u0001） */
    lv_ASSERT(lv_json_buf_init(&w, 64));
    lv_ASSERT(lv_json_buf_begin_object(&w));
    lv_ASSERT(lv_json_buf_append_key(&w, "empty"));
    lv_json_buf_append_string(&w, "");
    lv_ASSERT(lv_json_buf_append_key(&w, "ctrl"));
    lv_json_buf_append_string(&w, "a\x01" "b");
    lv_ASSERT(lv_json_buf_end_object(&w));
    lv_ASSERT_STR_EQ(w.buffer, "{\"empty\":\"\",\"ctrl\":\"a\\u0001b\"}");
    lv_json_buf_free(&w);

    printf("  PASSED\n");
}

/* ---------- (c) 对象/数组嵌套混合 + 空容器 ---------- */

static void test_nested_mixed(void) {
    printf("Testing nested object/array mix...\n");

    lvJsonBuf w;
    lv_ASSERT(lv_json_buf_init(&w, 64));
    lv_ASSERT(lv_json_buf_begin_object(&w));
    lv_ASSERT(lv_json_buf_append_key(&w, "users"));
    lv_ASSERT(lv_json_buf_begin_array(&w));

    lv_ASSERT(lv_json_buf_begin_object(&w));
    lv_ASSERT(lv_json_buf_append_key(&w, "id"));
    lv_ASSERT(lv_json_buf_append_int(&w, 1));
    lv_ASSERT(lv_json_buf_append_key(&w, "tags"));
    lv_ASSERT(lv_json_buf_begin_array(&w));
    lv_ASSERT(lv_json_buf_append_int(&w, 1));
    lv_ASSERT(lv_json_buf_append_int(&w, 2));
    lv_ASSERT(lv_json_buf_end_array(&w));
    lv_ASSERT(lv_json_buf_end_object(&w));

    lv_ASSERT(lv_json_buf_begin_object(&w));
    lv_ASSERT(lv_json_buf_append_key(&w, "id"));
    lv_ASSERT(lv_json_buf_append_int(&w, 2));
    lv_ASSERT(lv_json_buf_append_key(&w, "tags"));
    lv_ASSERT(lv_json_buf_begin_array(&w)); /* 空数组 */
    lv_ASSERT(lv_json_buf_end_array(&w));
    lv_ASSERT(lv_json_buf_end_object(&w));

    lv_ASSERT(lv_json_buf_end_array(&w));
    lv_ASSERT(lv_json_buf_append_key(&w, "empty_obj"));
    lv_ASSERT(lv_json_buf_begin_object(&w)); /* 空对象 */
    lv_ASSERT(lv_json_buf_end_object(&w));
    lv_ASSERT(lv_json_buf_end_object(&w));

    lv_ASSERT(strcmp(w.buffer,
                     "{\"users\":[{\"id\":1,\"tags\":[1,2]},"
                     "{\"id\":2,\"tags\":[]}],\"empty_obj\":{}}") == 0);
    lv_json_buf_free(&w);

    /* 独立空数组/空对象 */
    lv_ASSERT(lv_json_buf_init(&w, 64));
    lv_ASSERT(lv_json_buf_begin_array(&w));
    lv_ASSERT(lv_json_buf_end_array(&w));
    lv_ASSERT_STR_EQ(w.buffer, "[]");
    lv_json_buf_free(&w);

    lv_ASSERT(lv_json_buf_init(&w, 64));
    lv_ASSERT(lv_json_buf_begin_object(&w));
    lv_ASSERT(lv_json_buf_end_object(&w));
    lv_ASSERT_STR_EQ(w.buffer, "{}");
    lv_json_buf_free(&w);

    printf("  PASSED\n");
}

/* ---------- (d) 同一对象内 key 顺序 ---------- */

static void test_key_order(void) {
    printf("Testing key order preservation...\n");

    lvJsonBuf w;
    lv_ASSERT(lv_json_buf_init(&w, 64));
    lv_ASSERT(lv_json_buf_begin_object(&w));
    lv_ASSERT(lv_json_buf_append_key(&w, "z"));
    lv_ASSERT(lv_json_buf_append_int(&w, 1));
    lv_ASSERT(lv_json_buf_append_key(&w, "a"));
    lv_ASSERT(lv_json_buf_append_int(&w, 2));
    lv_ASSERT(lv_json_buf_append_key(&w, "m"));
    lv_ASSERT(lv_json_buf_append_int(&w, 3));
    lv_ASSERT(lv_json_buf_append_key(&w, "b"));
    lv_ASSERT(lv_json_buf_append_bool(&w, true));
    lv_ASSERT(lv_json_buf_end_object(&w));

    lv_ASSERT_STR_EQ(w.buffer, "{\"z\":1,\"a\":2,\"m\":3,\"b\":true}");
    lv_json_buf_free(&w);

    printf("  PASSED\n");
}

/* ---------- (e) pretty 模式 ---------- */

static void test_pretty_mode(void) {
    printf("Testing pretty mode...\n");

    lvJsonBuf w;
    lv_ASSERT(lv_json_buf_init(&w, 64));
    lv_json_buf_set_pretty(&w, true);
    lv_ASSERT(lv_json_buf_begin_object(&w));
    lv_ASSERT(lv_json_buf_append_key(&w, "a"));
    lv_ASSERT(lv_json_buf_append_int(&w, 1));
    lv_ASSERT(lv_json_buf_append_key(&w, "b"));
    lv_ASSERT(lv_json_buf_begin_array(&w));
    lv_ASSERT(lv_json_buf_append_int(&w, 2));
    lv_ASSERT(lv_json_buf_append_int(&w, 3));
    lv_ASSERT(lv_json_buf_end_array(&w));
    lv_ASSERT(lv_json_buf_end_object(&w));

    lv_ASSERT_STR_EQ(w.buffer, "{\n  \"a\":1,\n  \"b\":[\n    2,\n    3\n  ]\n}");

    /* 紧凑模式同一结构 */
    lvJsonBuf c;
    lv_ASSERT(lv_json_buf_init(&c, 64));
    lv_ASSERT(lv_json_buf_begin_object(&c));
    lv_ASSERT(lv_json_buf_append_key(&c, "a"));
    lv_ASSERT(lv_json_buf_append_int(&c, 1));
    lv_ASSERT(lv_json_buf_append_key(&c, "b"));
    lv_ASSERT(lv_json_buf_begin_array(&c));
    lv_ASSERT(lv_json_buf_append_int(&c, 2));
    lv_ASSERT(lv_json_buf_append_int(&c, 3));
    lv_ASSERT(lv_json_buf_end_array(&c));
    lv_ASSERT(lv_json_buf_end_object(&c));
    lv_ASSERT_STR_EQ(c.buffer, "{\"a\":1,\"b\":[2,3]}");

    /* pretty 输出去掉空白后应与紧凑输出一致 */
    char compact[256];
    size_t k = 0;
    for (const char *p = w.buffer; *p; p++) {
        if (*p != '\n' && *p != ' ')
            compact[k++] = *p;
    }
    compact[k] = '\0';
    lv_ASSERT_STR_EQ(compact, c.buffer);

    lv_json_buf_free(&w);
    lv_json_buf_free(&c);

    /* pretty 空容器仍为紧凑 {} / []（无多余换行缩进） */
    lv_ASSERT(lv_json_buf_init(&w, 64));
    lv_json_buf_set_pretty(&w, true);
    lv_ASSERT(lv_json_buf_begin_object(&w));
    lv_ASSERT(lv_json_buf_end_object(&w));
    lv_ASSERT_STR_EQ(w.buffer, "{}");
    lv_json_buf_free(&w);

    /* 写入中切换 pretty 开关 */
    lv_ASSERT(lv_json_buf_init(&w, 64));
    lv_json_buf_set_pretty(&w, true);
    lv_ASSERT(lv_json_buf_begin_object(&w));
    lv_ASSERT(lv_json_buf_append_key(&w, "a"));
    lv_ASSERT(lv_json_buf_append_int(&w, 1));
    lv_json_buf_set_pretty(&w, false);
    lv_ASSERT(lv_json_buf_append_key(&w, "b"));
    lv_ASSERT(lv_json_buf_append_int(&w, 2));
    lv_json_buf_set_pretty(&w, true);
    lv_ASSERT(lv_json_buf_append_key(&w, "c"));
    lv_ASSERT(lv_json_buf_append_int(&w, 3));
    lv_ASSERT(lv_json_buf_end_object(&w));
    lv_ASSERT_STR_EQ(w.buffer, "{\n  \"a\":1,\"b\":2,\n  \"c\":3\n}");
    lv_json_buf_free(&w);

    printf("  PASSED\n");
}

/* ---------- (f) 深度超 64 层缩进钳制 ---------- */

static void test_depth_clamp(void) {
    printf("Testing depth clamp beyond 64 levels...\n");

    lvJsonBuf w;
    lv_ASSERT(lv_json_buf_init(&w, 512));
    lv_json_buf_set_pretty(&w, true);
    for (int i = 0; i < 70; i++)
        lv_ASSERT(lv_json_buf_begin_array(&w));
    lv_ASSERT(lv_json_buf_append_int(&w, 1));
    for (int i = 0; i < 70; i++)
        lv_ASSERT(lv_json_buf_end_array(&w));

    /* 最内层元素前应只有 64 级缩进（128 空格），且 70 个 ']' 收尾 */
    char expect[160];
    int e = 0;
    expect[e++] = '\n';
    for (int i = 0; i < 128; i++)
        expect[e++] = ' ';
    expect[e++] = '1';
    expect[e] = '\0';
    lv_ASSERT(strstr(w.buffer, expect) != NULL);

    size_t len = strlen(w.buffer);
    lv_ASSERT(len >= 2);
    lv_ASSERT(w.buffer[len - 1] == ']');   /* 最外层回缩到 0 级，末尾为 \n] */
    lv_ASSERT(w.buffer[len - 2] == '\n');
    size_t close_count = 0;
    for (size_t i = 0; i < len; i++) {
        if (w.buffer[i] == ']')
            close_count++;
    }
    lv_ASSERT(close_count == 70);          /* 70 层 begin 全部配对闭合 */
    lv_json_buf_free(&w);

    printf("  PASSED\n");
}

/* ---------- (g) key_space：append_key 冒号后 1 空格 ---------- */

static void test_key_space(void) {
    printf("Testing key_space mode...\n");

    lvJsonBuf w;
    lv_ASSERT(lv_json_buf_init(&w, 64));
    lv_json_buf_set_key_space(&w, true);
    lv_ASSERT(lv_json_buf_begin_object(&w));
    lv_ASSERT(lv_json_buf_append_key(&w, "a"));
    lv_ASSERT(lv_json_buf_append_int(&w, 1));
    lv_ASSERT(lv_json_buf_append_key(&w, "b"));
    lv_json_buf_append_string(&w, "x");
    lv_ASSERT(lv_json_buf_end_object(&w));
    /* 紧凑 + key_space：仅冒号后多 1 空格 */
    lv_ASSERT_STR_EQ(w.buffer, "{\"a\": 1,\"b\": \"x\"}");
    lv_json_buf_free(&w);

    /* pretty + key_space：展示型 `"key": ` 风格（2 空格/级缩进） */
    lv_ASSERT(lv_json_buf_init(&w, 64));
    lv_json_buf_set_pretty(&w, true);
    lv_json_buf_set_key_space(&w, true);
    lv_ASSERT(lv_json_buf_begin_object(&w));
    lv_ASSERT(lv_json_buf_append_key(&w, "a"));
    lv_ASSERT(lv_json_buf_append_int(&w, 1));
    lv_ASSERT(lv_json_buf_append_key(&w, "b"));
    lv_ASSERT(lv_json_buf_begin_array(&w));
    lv_ASSERT(lv_json_buf_append_int(&w, 2));
    lv_ASSERT(lv_json_buf_append_int(&w, 3));
    lv_ASSERT(lv_json_buf_end_array(&w));
    lv_ASSERT(lv_json_buf_end_object(&w));
    lv_ASSERT_STR_EQ(w.buffer, "{\n  \"a\": 1,\n  \"b\": [\n    2,\n    3\n  ]\n}");

    /* 默认关闭：行为与旧版一致（紧凑模式无冒号空格） */
    lv_json_buf_free(&w);
    lv_ASSERT(lv_json_buf_init(&w, 64));
    lv_ASSERT(lv_json_buf_begin_object(&w));
    lv_ASSERT(lv_json_buf_append_key(&w, "a"));
    lv_ASSERT(lv_json_buf_append_int(&w, 1));
    lv_ASSERT(lv_json_buf_end_object(&w));
    lv_ASSERT_STR_EQ(w.buffer, "{\"a\":1}");

    lv_json_buf_free(&w);

    printf("  PASSED\n");
}

static void test_unicode_surrogate(void) {
    printf("Test: unicode surrogate pairs in JSON escapes...\n");

    char out[32];

    /* lv_str_json_unescape: surrogate pair \uD83D\uDE00 (U+1F600) -> F0 9F 98 80 */
    const char *pair = "\\uD83D\\uDE00";
    size_t n = lv_str_json_unescape(pair, strlen(pair), out, sizeof(out));
    lv_ASSERT(n == 4);
    lv_ASSERT((unsigned char) out[0] == 0xF0);
    lv_ASSERT((unsigned char) out[1] == 0x9F);
    lv_ASSERT((unsigned char) out[2] == 0x98);
    lv_ASSERT((unsigned char) out[3] == 0x80);

    /* isolated high surrogate -> U+FFFD (EF BF BD) */
    const char *iso = "\\uD83D";
    n = lv_str_json_unescape(iso, strlen(iso), out, sizeof(out));
    lv_ASSERT(n == 3);
    lv_ASSERT((unsigned char) out[0] == 0xEF);
    lv_ASSERT((unsigned char) out[1] == 0xBF);
    lv_ASSERT((unsigned char) out[2] == 0xBD);

    /* BMP \u4E2D (zhong) -> E4 B8 AD */
    const char *bmp = "\\u4E2D";
    n = lv_str_json_unescape(bmp, strlen(bmp), out, sizeof(out));
    lv_ASSERT(n == 3);
    lv_ASSERT((unsigned char) out[0] == 0xE4);
    lv_ASSERT((unsigned char) out[1] == 0xB8);
    lv_ASSERT((unsigned char) out[2] == 0xAD);

    /* lv_json_parse_string: "\uD83D\uDE00" -> F0 9F 98 80 */
    lvJsonParser p;
    const char *src = "\"\\uD83D\\uDE00\"";
    lv_json_parser_init(&p, src, strlen(src));
    char *s = lv_json_parse_string(&p);
    lv_ASSERT_NOT_NULL(s);
    lv_ASSERT((unsigned char) s[0] == 0xF0);
    lv_ASSERT((unsigned char) s[1] == 0x9F);
    lv_ASSERT((unsigned char) s[2] == 0x98);
    lv_ASSERT((unsigned char) s[3] == 0x80);
    lv_ASSERT(s[4] == '\0');
    lv_free((void **) &s);

    /* lv_json_parse_string: isolated high surrogate -> U+FFFD */
    const char *src2 = "\"\\uD83D\"";
    lv_json_parser_init(&p, src2, strlen(src2));
    s = lv_json_parse_string(&p);
    lv_ASSERT_NOT_NULL(s);
    lv_ASSERT((unsigned char) s[0] == 0xEF);
    lv_ASSERT((unsigned char) s[1] == 0xBF);
    lv_ASSERT((unsigned char) s[2] == 0xBD);
    lv_ASSERT(s[3] == '\0');
    lv_free((void **) &s);

    printf("  PASSED\n");
}

TEST_MAIN_BEGIN("Lv-00 JSON Buf Object-level API Test Suite")
    printf("=== Lv-00 JSON Buf Object-level API Test Suite ===\n\n");
    if (!lv_init()) {
        fprintf(stderr, "Failed to initialize Lv-00 system\n");
        return 1;
    }
    TEST_MAIN_RUN(test_scalar_values);
    TEST_MAIN_RUN(test_string_escape);
    TEST_MAIN_RUN(test_nested_mixed);
    TEST_MAIN_RUN(test_key_order);
    TEST_MAIN_RUN(test_pretty_mode);
    TEST_MAIN_RUN(test_depth_clamp);
    TEST_MAIN_RUN(test_key_space);
    /* unicode surrogate pair handling */
    TEST_MAIN_RUN(test_unicode_surrogate);
    printf("\nAll json buf tests passed.\n");
TEST_MAIN_END()
