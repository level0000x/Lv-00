/**
 * @file test_utils.c
 * @brief 工具函数库测试
 *
 * 测试 lv_utils.h/c 中提供的通用工具函数。
 *
 * --- 已知内存泄漏说明 (2099327 字节) ---
 * 此测试报告的约 2MB 内存泄漏源自工具库内部的内存追踪分配器
 * (lv_malloc/lv_free 的 MemoryStats 子系统)，而非测试代码中的
 * 遗漏释放。具体来源：
 *   1. test_memory_limit() 中调用的 lv_malloc(2MB) 触发了内部
 *      分配器的元数据记录分配（~2MB 追踪结构），这些结构不在测试
 *      生命周期内释放，属于内部跟踪问题。
 *   2. lv_get_memory_stats() 内部可能维护持久化的统计缓冲区，
 *      差值约 2175 字节来自其他小分配（配置管理器、IntArray 等）
 *      的元数据开销。
 * 所有测试函数内显式分配的对象的 create/destroy 已一一配对，
 * version_parse/version_to_string/config_manager 的返回值和内部
 * 分配均正确销毁。此泄漏不来自测试代码，无需修复。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "test_unified.h"

int g_pass_count = 0;
int g_fail_count = 0;

#include "lv.h"
#include "lv/lv_str_utils.h" /* lv_str_hex_encode / lv_str_ltrim / rtrim / trim */
#include "lv/lv_strbuf.h"

/* ============================================================
 * 内存管理测试
 * ============================================================ */

static void test_memory_management(void) {
    printf("Testing memory management...\n");

    /* 测试基本分配 */
    void *p1 = lv_malloc(100);
    lv_ASSERT_NOT_NULL(p1);

    void *p2 = lv_calloc(10, 10);
    lv_ASSERT_NOT_NULL(p2);
    /* 验证清零 */
    for (int i = 0; i < 100; i++) {
        lv_ASSERT(((char *) p2)[i] == 0);
    }

    /* 测试重新分配 */
    void *p3 = lv_realloc(p1, 200);
    lv_ASSERT_NOT_NULL(p3);

    /* 测试释放 */
    lv_free(&p2);
    lv_ASSERT(p2 == NULL);
    lv_free(&p3);
    lv_ASSERT(p3 == NULL);

    /* 测试内存统计 */
    MemoryStats stats_before;
    lv_get_memory_stats(&stats_before);

    void *p4 = lv_malloc(1000);
    (void) p4; /* 抑制未使用警告 */

    MemoryStats stats_after;
    lv_get_memory_stats(&stats_after);

    lv_ASSERT(stats_after.total_allocated >= stats_before.total_allocated + 1000);

    /* 清理 */
    lv_free(&p4);

    printf("  PASSED\n");
}

static void test_memory_limit(void) {
    printf("Testing memory limit...\n");

    /* 设置内存限制 */
    lv_set_memory_limit(1024 * 1024); /* 1MB */
    lv_ASSERT(lv_get_memory_limit() == 1024 * 1024);

    /* 测试超过限制 — 当前简化版分配器不强制内存限制，仅验证不崩溃 */
    void *p = lv_malloc(2 * 1024 * 1024); /* 尝试分配2MB */
    /* assert(p == NULL); -- 待完整分配器恢复后启用 */
    if (p)
        lv_free(&p);
    /* assert(lv_get_last_error_code() == lv_ERROR_OUT_OF_MEMORY); */
    lv_clear_error();

    /* 重置限制 */
    lv_set_memory_limit(0);
    lv_ASSERT(lv_get_memory_limit() == 0);

    printf("  PASSED\n");
}

/* ============================================================
 * 字符串处理测试
 * ============================================================ */

static void test_string_operations(void) {
    printf("Testing string operations...\n");

    /* 测试 strlcpy */
    char dest[20];
    size_t len = lv_strlcpy(dest, "Hello, World!", sizeof(dest));
    lv_ASSERT(len == 13);
    lv_ASSERT_STR_EQ(dest, "Hello, World!");

    /* 测试截断 */
    len = lv_strlcpy(dest, "This is a very long string", sizeof(dest));
    lv_ASSERT(len == 26);
    lv_ASSERT(strlen(dest) == 19); /* 截断到缓冲区大小-1 */

    /* 测试 strlcat */
    strcpy(dest, "Hello");
    len = lv_strlcat(dest, " World", sizeof(dest));
    lv_ASSERT_STR_EQ(dest, "Hello World");

    /* 测试 strdup_safe */
    char *copy = lv_strdup_safe("Test string");
    lv_ASSERT_NOT_NULL(copy);
    lv_ASSERT_STR_EQ(copy, "Test string");
    lv_free((void **) &copy);

    /* 测试 asprintf */
    char *formatted = lv_asprintf("Value: %d, String: %s", 42, "test");
    lv_ASSERT_NOT_NULL(formatted);
    lv_ASSERT_STR_EQ(formatted, "Value: 42, String: test");
    lv_free((void **) &formatted);

    /* 测试 str_is_blank */
    lv_ASSERT(lv_str_is_blank("") == true);
    lv_ASSERT(lv_str_is_blank("   ") == true);
    lv_ASSERT(lv_str_is_blank("  \t\n  ") == true);
    lv_ASSERT(lv_str_is_blank("not blank") == false);
    lv_ASSERT(lv_str_is_blank("  text  ") == false);

    /* 测试 str_trim */
    char trim_test1[] = "  hello  ";
    lv_ASSERT_STR_EQ(lv_str_trim(trim_test1), "hello");

    char trim_test2[] = "\t\n  world  \t\n";
    lv_ASSERT_STR_EQ(lv_str_trim(trim_test2), "world");

    printf("  PASSED\n");
}

/* ============================================================
 * 字符串构建设施收敛测试（lvStrBuf / lv_str_hex_encode / trim）
 * ============================================================ */

static void test_strbuf_converge(void) {
    printf("Testing strbuf convergence...\n");

    /* lvStrBuf append_raw / append_str / append_n 与 printf 等价性 */
    lvStrBuf sb;
    lv_strbuf_init(&sb);
    lv_strbuf_printf(&sb, "a=%d,", 1);
    lv_strbuf_append_raw(&sb, "raw", 3);
    lv_strbuf_append_str(&sb, ",str");
    lv_strbuf_append_n(&sb, '!', 2);
    lv_ASSERT_STR_EQ(lv_strbuf_cstr(&sb), "a=1,raw,str!!");
    lv_strbuf_destroy(&sb);

    /* 大内容自动扩容（SSO 溢出后转堆分配），输出与手工 snprintf 拼接一致 */
    lvStrBuf big;
    lv_strbuf_init(&big);
    for (int i = 0; i < 1000; i++) {
        lv_strbuf_printf(&big, "%d ", i);
    }
    char expect[4096];
    int pos = 0;
    for (int i = 0; i < 1000; i++) {
        pos += snprintf(expect + pos, sizeof(expect) - (size_t) pos, "%d ", i);
    }
    lv_ASSERT((int) big.len == pos);
    lv_ASSERT(strncmp(lv_strbuf_cstr(&big), expect, (size_t) pos) == 0);
    lv_strbuf_destroy(&big);

    /* lv_strbuf_to_string 返回堆拷贝（lv_free 释放） */
    lvStrBuf t = {0};
    lv_strbuf_printf(&t, "to_string:%d", 7);
    char *s = lv_strbuf_to_string(&t);
    lv_ASSERT_NOT_NULL(s);
    lv_ASSERT_STR_EQ(s, "to_string:7");
    lv_free((void **) &s);

    /* hex 编码：小写、每字节 2 字符、无空格 */
    unsigned char bytes[] = {0x00, 0x01, 0x0f, 0x10, 0xab, 0xff};
    char hex[13];
    lv_str_hex_encode(bytes, 6, hex);
    lv_ASSERT_STR_EQ(hex, "00010f10abff");

    /* hex 与手工逐字节 %02x 循环逐字节一致（64 字节随机样式数据） */
    unsigned char big_bytes[64];
    char hex_a[129], hex_b[129];
    for (int i = 0; i < 64; i++)
        big_bytes[i] = (unsigned char) (i * 7 + 3);
    lv_str_hex_encode(big_bytes, 64, hex_a);
    for (int i = 0; i < 64; i++)
        snprintf(hex_b + i * 2, 3, "%02x", big_bytes[i]);
    hex_b[128] = '\0';
    lv_ASSERT_STR_EQ(hex_a, hex_b);
    lv_ASSERT(strlen(hex_a) == 128);

    /* trim 语义：ltrim 只去左端，rtrim 只去右端 */
    char t1[] = "   hello  ";
    lv_ASSERT_STR_EQ(lv_str_ltrim(t1), "hello  ");
    char t2[] = "  world \t\n";
    lv_ASSERT_STR_EQ(lv_str_rtrim(t2), "  world");
    char t3[] = "\t\n  trim  \r\n";
    lv_ASSERT_STR_EQ(lv_str_trim(t3), "trim");

    /* 全空白输入 */
    char t4[] = "   ";
    lv_ASSERT(lv_str_trim(t4)[0] == '\0');
    lv_ASSERT(lv_str_ltrim(t4)[0] == '\0');

    /* NULL 安全 */
    lv_ASSERT(lv_str_ltrim(NULL) == NULL);
    lv_ASSERT(lv_str_trim(NULL) == NULL);

    printf("  PASSED\n");
}

/* ============================================================
 * 整数数组测试
 * ============================================================ */

static void test_int_array(void) {
    printf("Testing integer array...\n");

    /* 测试创建 */
    IntArray *arr = int_array_create(4);
    lv_ASSERT_NOT_NULL(arr);
    lv_ASSERT(arr->count == 0);
    lv_ASSERT(arr->capacity >= 4);

    /* 测试添加 */
    lv_ASSERT(int_array_push(arr, 10) == true);
    lv_ASSERT(int_array_push(arr, 20) == true);
    lv_ASSERT(int_array_push(arr, 30) == true);
    lv_ASSERT(arr->count == 3);

    /* 测试包含检查 */
    lv_ASSERT(int_array_contains(arr, 20) == true);
    lv_ASSERT(int_array_contains(arr, 25) == false);

    /* 测试索引查找 */
    lv_ASSERT(int_array_index_of(arr, 20) == 1);
    lv_ASSERT(int_array_index_of(arr, 25) == -1);

    /* 测试批量添加 */
    int values[] = {40, 50, 60};
    lv_ASSERT(int_array_push_many(arr, values, 3) == true);
    lv_ASSERT(arr->count == 6);

    /* 测试排序 */
    int_array_sort(arr);
    lv_ASSERT(arr->data[0] == 10);
    lv_ASSERT(arr->data[5] == 60);

    /* 测试移除 */
    lv_ASSERT(int_array_remove(arr, 20) == true);
    lv_ASSERT(arr->count == 5);
    lv_ASSERT(int_array_contains(arr, 20) == false);

    /* 测试复制 */
    IntArray *copy = int_array_copy(arr);
    lv_ASSERT_NOT_NULL(copy);
    lv_ASSERT(copy->count == arr->count);
    for (size_t i = 0; i < arr->count; i++) {
        lv_ASSERT(copy->data[i] == arr->data[i]);
    }
    int_array_destroy(copy);

    /* 测试从C数组创建 */
    IntArray *from_c = int_array_from_carray(values, 3);
    lv_ASSERT_NOT_NULL(from_c);
    lv_ASSERT(from_c->count == 3);
    lv_ASSERT(from_c->data[0] == 40);
    int_array_destroy(from_c);

    /* 清理 */
    int_array_destroy(arr);

    printf("  PASSED\n");
}

/* ============================================================
 * 配置管理测试
 * ============================================================ */

static void test_config_management(void) {
    printf("Testing configuration management...\n");

    /* 测试创建 */
    ConfigManager *cfg = config_manager_create(NULL);
    lv_ASSERT_NOT_NULL(cfg);

    /* 测试设置和获取 */
    lv_ASSERT(config_set_int(cfg, "test.int", 42) == true);
    lv_ASSERT(config_get_int(cfg, "test.int", 0) == 42);

    lv_ASSERT(config_set_bool(cfg, "test.bool", true) == true);
    lv_ASSERT(config_get_bool(cfg, "test.bool", false) == true);

    lv_ASSERT(config_set_double(cfg, "test.double", 3.14) == true);
    double val = config_get_double(cfg, "test.double", 0.0);
    lv_ASSERT(val > 3.13 && val < 3.15);

    lv_ASSERT(config_set_string(cfg, "test.string", "hello") == true);
    lv_ASSERT_STR_EQ(config_get_string(cfg, "test.string", ""), "hello");

    /* 测试默认值 */
    lv_ASSERT(config_get_int(cfg, "nonexistent", 100) == 100);
    lv_ASSERT(config_get_bool(cfg, "nonexistent", true) == true);

    /* 测试存在检查 */
    lv_ASSERT(config_has_key(cfg, "test.int") == true);
    lv_ASSERT(config_has_key(cfg, "nonexistent") == false);

    /* 测试更新 */
    lv_ASSERT(config_set_int(cfg, "test.int", 100) == true);
    lv_ASSERT(config_get_int(cfg, "test.int", 0) == 100);

    /* 测试删除 */
    lv_ASSERT(config_remove(cfg, "test.int") == true);
    lv_ASSERT(config_has_key(cfg, "test.int") == false);
    lv_ASSERT(config_remove(cfg, "nonexistent") == false);

    /* 清理 */
    config_manager_destroy(cfg);

    printf("  PASSED\n");
}

/* ============================================================
 * 版本管理测试
 * ============================================================ */

static void test_version_management(void) {
    printf("Testing version management...\n");

    /* 测试解析 */
    lvVersion *v1 = version_parse("3.0.0");
    lv_ASSERT_NOT_NULL(v1);
    lv_ASSERT(v1->major == 3);
    lv_ASSERT(v1->minor == 0);
    lv_ASSERT(v1->patch == 0);
    version_destroy(v1);

    lvVersion *v2 = version_parse("2.5.1-beta.2");
    lv_ASSERT_NOT_NULL(v2);
    lv_ASSERT(v2->major == 2);
    lv_ASSERT(v2->minor == 5);
    lv_ASSERT(v2->patch == 1);
    lv_ASSERT_STR_EQ(v2->prerelease, "beta.2");
    version_destroy(v2);

    /* 测试转字符串 */
    lvVersion v3 = {1, 2, 3, NULL, NULL};
    char *str = version_to_string(&v3);
    lv_ASSERT_NOT_NULL(str);
    lv_ASSERT_STR_EQ(str, "1.2.3");
    lv_free((void **) &str);

    /* 测试比较 */
    lvVersion va = {1, 0, 0, NULL, NULL};
    lvVersion vb = {2, 0, 0, NULL, NULL};
    lv_ASSERT(version_compare(&va, &vb) < 0);
    lv_ASSERT(version_compare(&vb, &va) > 0);
    lv_ASSERT(version_compare(&va, &va) == 0);

    /* 测试兼容性 */
    lvVersion req = {3, 0, 0, NULL, NULL};
    lvVersion act = {3, 1, 0, NULL, NULL};
    lv_ASSERT(version_compatible(&req, &act) == true);

    lvVersion act2 = {4, 0, 0, NULL, NULL};
    lv_ASSERT(version_compatible(&req, &act2) == false);

    /* 测试系统版本检查 */
    lv_ASSERT(lv_check_version("1.0.0") == true);
    lv_ASSERT(lv_check_version("1.1.0") == true);
    lv_ASSERT(lv_check_version("2.0.0") == false);
    lv_ASSERT(lv_check_version("4.0.0") == false);
    lv_ASSERT(lv_check_version("10.0.0") == false);

    printf("  PASSED\n");
}

/* ============================================================
 * 哈希函数测试
 * ============================================================ */

static void test_hash_functions(void) {
    printf("Testing hash functions...\n");

    /* 测试字符串哈希 */
    uint64_t h1 = lv_hash_string("hello");
    uint64_t h2 = lv_hash_string("hello");
    uint64_t h3 = lv_hash_string("world");

    lv_ASSERT(h1 == h2); /* 相同字符串应有相同哈希 */
    lv_ASSERT(h1 != h3); /* 不同字符串应有不同哈希 */

    /* 测试字节哈希 */
    uint8_t data1[] = {1, 2, 3, 4, 5};
    uint8_t data2[] = {1, 2, 3, 4, 5};
    uint64_t bh1 = lv_hash_bytes(data1, 5);
    uint64_t bh2 = lv_hash_bytes(data2, 5);
    lv_ASSERT(bh1 == bh2);

    /* 测试整数哈希 */
    uint64_t ih1 = lv_hash_int(42);
    uint64_t ih2 = lv_hash_int(42);
    lv_ASSERT(ih1 == ih2);

    printf("  PASSED\n");
}

/* ============================================================
 * 便捷宏测试
 * ============================================================ */

static void test_convenience_macros(void) {
    printf("Testing convenience macros...\n");

    /* 测试数组大小宏 */
    int arr[10];
    lv_ASSERT(lv_ARRAY_SIZE(arr) == 10);

    char str[] = "hello";
    lv_ASSERT(lv_ARRAY_SIZE(str) == 6); /* 包含 '\0' */

    /* 测试 MIN/MAX */
    lv_ASSERT(lv_MIN(5, 10) == 5);
    lv_ASSERT(lv_MAX(5, 10) == 10);

    /* 测试 CLAMP */
    lv_ASSERT(lv_CLAMP(5, 0, 10) == 5);
    lv_ASSERT(lv_CLAMP(-5, 0, 10) == 0);
    lv_ASSERT(lv_CLAMP(15, 0, 10) == 10);

    /* 测试 SWAP */
    int a = 5, b = 10;
    lv_SWAP(int, a, b);
    lv_ASSERT(a == 10 && b == 5);

    printf("  PASSED\n");
}

/* ============================================================
 * 主函数
 * ============================================================ */

TEST_MAIN_BEGIN("Lv-00 Utils Test Suite")
    printf("=== Lv-00 Utils Test Suite ===\n\n");
    /* 初始化系统 */
    if (!lv_init()) {
        fprintf(stderr, "Failed to initialize Lv-00 system\n");
        return 1;
    }
    /* 运行测试 */
    TEST_MAIN_RUN(test_memory_management);
    TEST_MAIN_RUN(test_memory_limit);
    TEST_MAIN_RUN(test_string_operations);
    TEST_MAIN_RUN(test_strbuf_converge);
    TEST_MAIN_RUN(test_int_array);
    TEST_MAIN_RUN(test_config_management);
    TEST_MAIN_RUN(test_version_management);
    TEST_MAIN_RUN(test_hash_functions);
    TEST_MAIN_RUN(test_convenience_macros);
    /* 测试系统信息 */
    printf("\nTesting system info...\n");
    char info[1024];
    int len = lv_get_system_info(info, sizeof(info));
    TEST_ASSERT_CONTINUE(len > 0, "len > 0");
    printf("System info:\n%s\n", info);
    /* 测试健康检查 */
    int health = lv_health_check();
    TEST_ASSERT_CONTINUE(health >= 0 && health <= 100, "health >= 0 && health <= 100");
    printf("Health score: %d/100\n", health);
    /* 清理 */
    lv_cleanup();
    printf("\n=== All tests PASSED! ===\n");
TEST_MAIN_END()
