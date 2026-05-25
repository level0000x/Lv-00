/**
 * @file test_func_block_preset.c
 * @brief 预设函数块库测试
 *
 * @details 测试预设函数块库的初始化、查找、实例化和文档生成功能。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "func_block_preset.h"
#include "lv00.h"
#include "test_helpers.h"

/* 全局测试计数器 */
int g_pass_count = 0;
int g_fail_count = 0;

/* ============================================================
 * 测试辅助宏（仅保留项目特有的）
 * ============================================================ */

#define TEST_PASS(name)              \
    do {                             \
        printf("[PASS] %s\n", name); \
    } while (0)

/* ============================================================
 * 测试用例
 * ============================================================ */

/**
 * @brief 测试库初始化和清理
 */
static void test_library_lifecycle(void) {
    /* 测试重复初始化（幂等性） */
    TEST_ASSERT(func_block_preset_library_init(), "第一次初始化失败");
    TEST_ASSERT(func_block_preset_library_init(), "第二次初始化应成功（幂等）");

    /* 测试清理 */
    func_block_preset_library_cleanup();

    /* 重新初始化 */
    TEST_ASSERT(func_block_preset_library_init(), "清理后重新初始化失败");

    TEST_PASS("test_library_lifecycle");
}

/**
 * @brief 测试预设查找和元数据获取
 */
static void test_preset_lookup(void) {
    func_block_preset_library_init();

    /* 测试存在的预设 */
    TEST_ASSERT(func_block_preset_exists("midpoint"), "midpoint应存在");
    TEST_ASSERT(func_block_preset_exists("centroid"), "centroid应存在");
    TEST_ASSERT(func_block_preset_exists("distance"), "distance应存在");
    TEST_ASSERT(func_block_preset_exists("translation"), "translation应存在");
    TEST_ASSERT(func_block_preset_exists("vector_add"), "vector_add应存在");
    TEST_ASSERT(func_block_preset_exists("collinearity_test"), "collinearity_test应存在");

    /* 测试不存在的预设 */
    TEST_ASSERT(!func_block_preset_exists("nonexistent"), "不存在的预设应返回false");
    TEST_ASSERT(!func_block_preset_exists(NULL), "NULL应返回false");

    /* 测试元数据获取 */
    const PresetMetadata *m = func_block_preset_get_metadata("midpoint");
    TEST_ASSERT(m != NULL, "应能获取midpoint元数据");
    TEST_ASSERT_STR_EQ(m->name, "midpoint");
    TEST_ASSERT_EQ_MSG(m->input_count, 2, "midpoint应有2个输入");
    TEST_ASSERT_EQ_MSG(m->output_count, 1, "midpoint应有1个输出");
    TEST_ASSERT(m->category == PRESET_CATEGORY_CONSTRUCTION, "类别应为几何构造");

    /* 测试不存在的元数据 */
    TEST_ASSERT(func_block_preset_get_metadata("nonexistent") == NULL, "不存在的预设应返回NULL");
    TEST_ASSERT(func_block_preset_get_metadata(NULL) == NULL, "NULL应返回NULL");

    TEST_PASS("test_preset_lookup");
}

/**
 * @brief 测试参数数量查询
 */
static void test_param_counts(void) {
    func_block_preset_library_init();

    /* 测试输入参数数量 */
    TEST_ASSERT_EQ_MSG(func_block_preset_get_input_count("midpoint"), 2, "midpoint输入数应为2");
    TEST_ASSERT_EQ_MSG(func_block_preset_get_input_count("centroid"), 3, "centroid输入数应为3");
    TEST_ASSERT_EQ_MSG(func_block_preset_get_input_count("distance"), 2, "distance输入数应为2");
    TEST_ASSERT_EQ_MSG(func_block_preset_get_input_count("translation"), 3, "translation输入数应为3");
    TEST_ASSERT_EQ_MSG(func_block_preset_get_input_count("rotation"), 4, "rotation输入数应为4");

    /* 测试输出参数数量 */
    TEST_ASSERT_EQ_MSG(func_block_preset_get_output_count("midpoint"), 1, "midpoint输出数应为1");
    TEST_ASSERT_EQ_MSG(func_block_preset_get_output_count("centroid"), 1, "centroid输出数应为1");
    TEST_ASSERT_EQ_MSG(func_block_preset_get_output_count("tangent_lines"), 2, "tangent_lines输出数应为2");

    /* 测试不存在的预设 */
    TEST_ASSERT_EQ_MSG(func_block_preset_get_input_count("nonexistent"), -1, "不存在的预设应返回-1");
    TEST_ASSERT_EQ_MSG(func_block_preset_get_output_count("nonexistent"), -1, "不存在的预设应返回-1");

    TEST_PASS("test_param_counts");
}

/**
 * @brief 测试预设列表获取
 */
static void test_preset_list(void) {
    func_block_preset_library_init();

    const char *names[100];
    int count;

    /* 测试获取所有预设 */
    count = func_block_preset_list(names, 100, -1);
    TEST_ASSERT(count > 0, "应返回预设数量");
    printf("  [INFO] 总预设数量: %d\n", count);

    /* 测试按类别筛选 */
    count = func_block_preset_list(names, 100, PRESET_CATEGORY_CONSTRUCTION);
    TEST_ASSERT(count > 0, "几何构造类应有预设");
    printf("  [INFO] 几何构造类预设数量: %d\n", count);

    count = func_block_preset_list(names, 100, PRESET_CATEGORY_MEASUREMENT);
    TEST_ASSERT(count > 0, "度量计算类应有预设");
    printf("  [INFO] 度量计算类预设数量: %d\n", count);

    count = func_block_preset_list(names, 100, PRESET_CATEGORY_TRANSFORMATION);
    TEST_ASSERT(count > 0, "几何变换类应有预设");
    printf("  [INFO] 几何变换类预设数量: %d\n", count);

    count = func_block_preset_list(names, 100, PRESET_CATEGORY_ALGEBRAIC);
    TEST_ASSERT(count > 0, "代数运算类应有预设");
    printf("  [INFO] 代数运算类预设数量: %d\n", count);

    count = func_block_preset_list(names, 100, PRESET_CATEGORY_LOGIC);
    TEST_ASSERT(count > 0, "逻辑推导类应有预设");
    printf("  [INFO] 逻辑推导类预设数量: %d\n", count);

    /* 测试缓冲区限制 */
    count = func_block_preset_list(names, 5, -1);
    TEST_ASSERT(count >= 0, "即使缓冲区小也应返回数量");

    TEST_PASS("test_preset_list");
}

/**
 * @brief 测试字符串转换函数
 */
static void test_string_conversions(void) {
    func_block_preset_library_init();

    /* 测试类别字符串 */
    TEST_ASSERT_STR_EQ(func_block_preset_category_string(PRESET_CATEGORY_CONSTRUCTION), "几何构造");
    TEST_ASSERT_STR_EQ(func_block_preset_category_string(PRESET_CATEGORY_MEASUREMENT), "度量计算");
    TEST_ASSERT_STR_EQ(func_block_preset_category_string(PRESET_CATEGORY_TRANSFORMATION), "几何变换");
    TEST_ASSERT_STR_EQ(func_block_preset_category_string(PRESET_CATEGORY_ALGEBRAIC), "代数运算");
    TEST_ASSERT_STR_EQ(func_block_preset_category_string(PRESET_CATEGORY_LOGIC), "逻辑推导");

    /* 测试参数类型字符串 */
    TEST_ASSERT_STR_EQ(func_block_preset_param_type_string(PARAM_TYPE_POINT), "点");
    TEST_ASSERT_STR_EQ(func_block_preset_param_type_string(PARAM_TYPE_LINE), "直线");
    TEST_ASSERT_STR_EQ(func_block_preset_param_type_string(PARAM_TYPE_CIRCLE), "圆");
    TEST_ASSERT_STR_EQ(func_block_preset_param_type_string(PARAM_TYPE_SCALAR), "标量");

    /* 测试复杂度字符串 */
    TEST_ASSERT_STR_EQ(func_block_preset_complexity_string(COMPLEXITY_O1), "O(1) - 常数时间");
    TEST_ASSERT_STR_EQ(func_block_preset_complexity_string(COMPLEXITY_ON), "O(n) - 线性时间");

    /* 测试性质字符串 */
    char buffer[256];
    int len = func_block_preset_properties_string(
        PRESET_PROPERTY_DETERMINISTIC | PRESET_PROPERTY_CONTINUOUS | PRESET_PROPERTY_CONSTRUCTIVE, buffer,
        sizeof(buffer));
    TEST_ASSERT(len > 0, "性质字符串应有内容");
    printf("  [INFO] 性质字符串: %s\n", buffer);

    TEST_PASS("test_string_conversions");
}

/**
 * @brief 测试文档生成
 */
static void test_documentation(void) {
    func_block_preset_library_init();

    char buffer[4096];
    size_t len;

    /* 测试单个预设文档生成 */
    len = func_block_preset_generate_doc("midpoint", buffer, sizeof(buffer));
    TEST_ASSERT(len > 0 && len <= sizeof(buffer), "应成功生成文档");
    TEST_ASSERT(strstr(buffer, "midpoint") != NULL, "文档应包含预设名称");
    TEST_ASSERT(strstr(buffer, "描述") != NULL, "文档应包含描述");
    printf("  [INFO] midpoint文档长度: %zu\n", len);

    /* 测试不存在的预设 */
    len = func_block_preset_generate_doc("nonexistent", buffer, sizeof(buffer));
    TEST_ASSERT_EQ_MSG((int) len, 0, "不存在的预设应返回0");

    /* 测试索引生成 */
    len = func_block_preset_generate_index(buffer, sizeof(buffer));
    TEST_ASSERT(len > 0 && len <= sizeof(buffer), "应成功生成索引");
    TEST_ASSERT(strstr(buffer, "Lv-00 预设函数块库") != NULL, "索引应包含标题");
    printf("  [INFO] 索引文档长度: %zu\n", len);

    TEST_PASS("test_documentation");
}

/**
 * @brief 测试逆操作查询
 */
static void test_inverse_operations(void) {
    func_block_preset_library_init();

    /* 测试可逆操作 */
    const char *inv;

    inv = func_block_preset_get_inverse("translation");
    TEST_ASSERT(inv != NULL, "translation应有逆操作");

    inv = func_block_preset_get_inverse("rotation");
    TEST_ASSERT(inv != NULL, "rotation应有逆操作");

    inv = func_block_preset_get_inverse("scaling");
    TEST_ASSERT(inv != NULL, "scaling应有逆操作");

    inv = func_block_preset_get_inverse("inversion");
    TEST_ASSERT(inv != NULL, "inversion应有逆操作");

    inv = func_block_preset_get_inverse("reflection_point");
    TEST_ASSERT(inv != NULL, "reflection_point应有逆操作");

    /* 测试不存在的预设 */
    inv = func_block_preset_get_inverse("nonexistent");
    TEST_ASSERT(inv == NULL, "不存在的预设应返回NULL");

    TEST_PASS("test_inverse_operations");
}

/* ============================================================
 * 主函数
 * ============================================================ */

int main(void) {
    printf("========================================\n");
    printf("预设函数块库测试\n");
    printf("========================================\n\n");

    g_pass_count = 0;
    g_fail_count = 0;

    test_library_lifecycle();
    test_preset_lookup();
    test_param_counts();
    test_preset_list();
    test_string_conversions();
    test_documentation();
    test_inverse_operations();

    printf("\n========================================\n");
    if (g_fail_count == 0) {
        printf("所有测试通过! (%d 项)\n", g_pass_count);
    } else {
        printf("测试结果: %d 通过, %d 失败, %d 总计\n", g_pass_count, g_fail_count, g_pass_count + g_fail_count);
    }
    printf("========================================\n");

    func_block_preset_library_cleanup();

    return g_fail_count > 0 ? 1 : 0;
}
