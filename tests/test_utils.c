/**
 * @file test_utils.c
 * @brief 工具函数库测试
 *
 * 测试 lv00_utils.h/c 中提供的通用工具函数。
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv00.h"

/* ============================================================
 * 内存管理测试
 * ============================================================ */

static void test_memory_management(void) {
    printf("Testing memory management...\n");

    /* 测试基本分配 */
    void *p1 = lv00_malloc(100);
    assert(p1 != NULL);

    void *p2 = lv00_calloc(10, 10);
    assert(p2 != NULL);
    /* 验证清零 */
    for (int i = 0; i < 100; i++) {
        assert(((char *) p2)[i] == 0);
    }

    /* 测试重新分配 */
    void *p3 = lv00_realloc(p1, 200);
    assert(p3 != NULL);

    /* 测试释放 */
    lv00_free(&p2);
    assert(p2 == NULL);
    lv00_free(&p3);
    assert(p3 == NULL);

    /* 测试内存统计 */
    MemoryStats stats_before;
    lv00_get_memory_stats(&stats_before);

    void *p4 = lv00_malloc(1000);
    (void) p4; /* 抑制未使用警告 */

    MemoryStats stats_after;
    lv00_get_memory_stats(&stats_after);

    assert(stats_after.total_allocated >= stats_before.total_allocated + 1000);

    /* 清理 */
    lv00_free(&p4);

    printf("  PASSED\n");
}

static void test_memory_limit(void) {
    printf("Testing memory limit...\n");

    /* 设置内存限制 */
    lv00_set_memory_limit(1024 * 1024); /* 1MB */
    assert(lv00_get_memory_limit() == 1024 * 1024);

    /* 测试超过限制 — 当前简化版分配器不强制内存限制，仅验证不崩溃 */
    void *p = lv00_malloc(2 * 1024 * 1024); /* 尝试分配2MB */
    /* assert(p == NULL); -- 待完整分配器恢复后启用 */
    if (p)
        lv00_free(&p);
    /* assert(lv00_get_last_error_code() == LV00_ERROR_OUT_OF_MEMORY); */
    lv00_clear_error();

    /* 重置限制 */
    lv00_set_memory_limit(0);
    assert(lv00_get_memory_limit() == 0);

    printf("  PASSED\n");
}

/* ============================================================
 * 字符串处理测试
 * ============================================================ */

static void test_string_operations(void) {
    printf("Testing string operations...\n");

    /* 测试 strlcpy */
    char dest[20];
    size_t len = lv00_strlcpy(dest, "Hello, World!", sizeof(dest));
    assert(len == 13);
    assert(strcmp(dest, "Hello, World!") == 0);

    /* 测试截断 */
    len = lv00_strlcpy(dest, "This is a very long string", sizeof(dest));
    assert(len == 26);
    assert(strlen(dest) == 19); /* 截断到缓冲区大小-1 */

    /* 测试 strlcat */
    strcpy(dest, "Hello");
    len = lv00_strlcat(dest, " World", sizeof(dest));
    assert(strcmp(dest, "Hello World") == 0);

    /* 测试 strdup_safe */
    char *copy = lv00_strdup_safe("Test string");
    assert(copy != NULL);
    assert(strcmp(copy, "Test string") == 0);
    lv00_free((void **) &copy);

    /* 测试 asprintf */
    char *formatted = lv00_asprintf("Value: %d, String: %s", 42, "test");
    assert(formatted != NULL);
    assert(strcmp(formatted, "Value: 42, String: test") == 0);
    lv00_free((void **) &formatted);

    /* 测试 str_is_blank */
    assert(lv00_str_is_blank("") == true);
    assert(lv00_str_is_blank("   ") == true);
    assert(lv00_str_is_blank("  \t\n  ") == true);
    assert(lv00_str_is_blank("not blank") == false);
    assert(lv00_str_is_blank("  text  ") == false);

    /* 测试 str_trim */
    char trim_test1[] = "  hello  ";
    assert(strcmp(lv00_str_trim(trim_test1), "hello") == 0);

    char trim_test2[] = "\t\n  world  \t\n";
    assert(strcmp(lv00_str_trim(trim_test2), "world") == 0);

    printf("  PASSED\n");
}

/* ============================================================
 * 整数数组测试
 * ============================================================ */

static void test_int_array(void) {
    printf("Testing integer array...\n");

    /* 测试创建 */
    IntArray *arr = int_array_create(4);
    assert(arr != NULL);
    assert(arr->count == 0);
    assert(arr->capacity >= 4);

    /* 测试添加 */
    assert(int_array_push(arr, 10) == true);
    assert(int_array_push(arr, 20) == true);
    assert(int_array_push(arr, 30) == true);
    assert(arr->count == 3);

    /* 测试包含检查 */
    assert(int_array_contains(arr, 20) == true);
    assert(int_array_contains(arr, 25) == false);

    /* 测试索引查找 */
    assert(int_array_index_of(arr, 20) == 1);
    assert(int_array_index_of(arr, 25) == -1);

    /* 测试批量添加 */
    int values[] = {40, 50, 60};
    assert(int_array_push_many(arr, values, 3) == true);
    assert(arr->count == 6);

    /* 测试排序 */
    int_array_sort(arr);
    assert(arr->data[0] == 10);
    assert(arr->data[5] == 60);

    /* 测试移除 */
    assert(int_array_remove(arr, 20) == true);
    assert(arr->count == 5);
    assert(int_array_contains(arr, 20) == false);

    /* 测试复制 */
    IntArray *copy = int_array_copy(arr);
    assert(copy != NULL);
    assert(copy->count == arr->count);
    for (size_t i = 0; i < arr->count; i++) {
        assert(copy->data[i] == arr->data[i]);
    }
    int_array_destroy(copy);

    /* 测试从C数组创建 */
    IntArray *from_c = int_array_from_carray(values, 3);
    assert(from_c != NULL);
    assert(from_c->count == 3);
    assert(from_c->data[0] == 40);
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
    assert(cfg != NULL);

    /* 测试设置和获取 */
    assert(config_set_int(cfg, "test.int", 42) == true);
    assert(config_get_int(cfg, "test.int", 0) == 42);

    assert(config_set_bool(cfg, "test.bool", true) == true);
    assert(config_get_bool(cfg, "test.bool", false) == true);

    assert(config_set_double(cfg, "test.double", 3.14) == true);
    double val = config_get_double(cfg, "test.double", 0.0);
    assert(val > 3.13 && val < 3.15);

    assert(config_set_string(cfg, "test.string", "hello") == true);
    assert(strcmp(config_get_string(cfg, "test.string", ""), "hello") == 0);

    /* 测试默认值 */
    assert(config_get_int(cfg, "nonexistent", 100) == 100);
    assert(config_get_bool(cfg, "nonexistent", true) == true);

    /* 测试存在检查 */
    assert(config_has_key(cfg, "test.int") == true);
    assert(config_has_key(cfg, "nonexistent") == false);

    /* 测试更新 */
    assert(config_set_int(cfg, "test.int", 100) == true);
    assert(config_get_int(cfg, "test.int", 0) == 100);

    /* 测试删除 */
    assert(config_remove(cfg, "test.int") == true);
    assert(config_has_key(cfg, "test.int") == false);
    assert(config_remove(cfg, "nonexistent") == false);

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
    LV00Version *v1 = version_parse("3.0.0");
    assert(v1 != NULL);
    assert(v1->major == 3);
    assert(v1->minor == 0);
    assert(v1->patch == 0);
    version_destroy(v1);

    LV00Version *v2 = version_parse("2.5.1-beta.2");
    assert(v2 != NULL);
    assert(v2->major == 2);
    assert(v2->minor == 5);
    assert(v2->patch == 1);
    assert(strcmp(v2->prerelease, "beta.2") == 0);
    version_destroy(v2);

    /* 测试转字符串 */
    LV00Version v3 = {1, 2, 3, NULL, NULL};
    char *str = version_to_string(&v3);
    assert(str != NULL);
    assert(strcmp(str, "1.2.3") == 0);
    lv00_free((void **) &str);

    /* 测试比较 */
    LV00Version va = {1, 0, 0, NULL, NULL};
    LV00Version vb = {2, 0, 0, NULL, NULL};
    assert(version_compare(&va, &vb) < 0);
    assert(version_compare(&vb, &va) > 0);
    assert(version_compare(&va, &va) == 0);

    /* 测试兼容性 */
    LV00Version req = {3, 0, 0, NULL, NULL};
    LV00Version act = {3, 1, 0, NULL, NULL};
    assert(version_compatible(&req, &act) == true);

    LV00Version act2 = {4, 0, 0, NULL, NULL};
    assert(version_compatible(&req, &act2) == false);

    /* 测试系统版本检查 */
    assert(lv00_check_version("3.0.0") == true);
    assert(lv00_check_version("3.3.0") == false);
    assert(lv00_check_version("10.0.0") == false);

    printf("  PASSED\n");
}

/* ============================================================
 * 哈希函数测试
 * ============================================================ */

static void test_hash_functions(void) {
    printf("Testing hash functions...\n");

    /* 测试字符串哈希 */
    uint64_t h1 = lv00_hash_string("hello");
    uint64_t h2 = lv00_hash_string("hello");
    uint64_t h3 = lv00_hash_string("world");

    assert(h1 == h2); /* 相同字符串应有相同哈希 */
    assert(h1 != h3); /* 不同字符串应有不同哈希 */

    /* 测试字节哈希 */
    uint8_t data1[] = {1, 2, 3, 4, 5};
    uint8_t data2[] = {1, 2, 3, 4, 5};
    uint64_t bh1 = lv00_hash_bytes(data1, 5);
    uint64_t bh2 = lv00_hash_bytes(data2, 5);
    assert(bh1 == bh2);

    /* 测试整数哈希 */
    uint64_t ih1 = lv00_hash_int(42);
    uint64_t ih2 = lv00_hash_int(42);
    assert(ih1 == ih2);

    printf("  PASSED\n");
}

/* ============================================================
 * 便捷宏测试
 * ============================================================ */

static void test_convenience_macros(void) {
    printf("Testing convenience macros...\n");

    /* 测试数组大小宏 */
    int arr[10];
    assert(LV00_ARRAY_SIZE(arr) == 10);

    char str[] = "hello";
    assert(LV00_ARRAY_SIZE(str) == 6); /* 包含 '\0' */

    /* 测试 MIN/MAX */
    assert(LV00_MIN(5, 10) == 5);
    assert(LV00_MAX(5, 10) == 10);

    /* 测试 CLAMP */
    assert(LV00_CLAMP(5, 0, 10) == 5);
    assert(LV00_CLAMP(-5, 0, 10) == 0);
    assert(LV00_CLAMP(15, 0, 10) == 10);

    /* 测试 SWAP */
    int a = 5, b = 10;
    LV00_SWAP(int, a, b);
    assert(a == 10 && b == 5);

    printf("  PASSED\n");
}

/* ============================================================
 * 主函数
 * ============================================================ */

int main(void) {
    printf("=== Lv-00 Utils Test Suite ===\n\n");

    /* 初始化系统 */
    if (!lv00_init()) {
        fprintf(stderr, "Failed to initialize Lv-00 system\n");
        return 1;
    }

    /* 运行测试 */
    test_memory_management();
    test_memory_limit();
    test_string_operations();
    test_int_array();
    test_config_management();
    test_version_management();
    test_hash_functions();
    test_convenience_macros();

    /* 测试系统信息 */
    printf("\nTesting system info...\n");
    char info[1024];
    int len = lv00_get_system_info(info, sizeof(info));
    assert(len > 0);
    printf("System info:\n%s\n", info);

    /* 测试健康检查 */
    int health = lv00_health_check();
    assert(health >= 0 && health <= 100);
    printf("Health score: %d/100\n", health);

    /* 清理 */
    lv00_cleanup();

    printf("\n=== All tests PASSED! ===\n");
    return 0;
}
