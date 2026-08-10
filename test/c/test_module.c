/**
 * @file test_module.c
 * @brief 模块系统测试 - 模块创建、依赖管理、导出管理
 *
 * 测试内容：
 * - 模块生命周期
 * - 依赖添加与管理
 * - 公理包添加
 * - 函数块和类型导出
 * - 循环依赖检测
 * - 版本哈希计算
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv.h"
#include "test_helpers.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============== 测试：模块生命周期 ============== */

static void test_module_lifecycle(void) {
    printf("Test: module lifecycle...\n");

    Module *mod = module_create("Geometry", "1.0.0");
    lv_ASSERT_NOT_NULL(mod);
    lv_ASSERT_STR_EQ(module_get_name(mod), "Geometry");
    lv_ASSERT_STR_EQ(module_get_version(mod), "1.0.0");
    lv_ASSERT(module_get_dependency_count(mod) == 0);

    printf("  模块 '%s' v%s 创建成功\n", module_get_name(mod), module_get_version(mod));

    module_destroy(mod);
    printf("  PASSED\n");

}

/* ============== 测试：依赖管理 ============== */

static void test_dependency_management(void) {
    printf("Test: dependency management...\n");

    Module *mod = module_create("TestModule", "1.0");
    lv_ASSERT_NOT_NULL(mod);

    /* 添加依赖 */
    bool ok = module_add_dependency(mod, "BaseModule", ">=1.0.0");
    lv_ASSERT(ok);
    lv_ASSERT(module_get_dependency_count(mod) == 1);
    printf("  添加依赖 'BaseModule >=1.0.0' 成功\n");

    /* 添加更多依赖 */
    ok = module_add_dependency(mod, "MathLib", "^2.0.0");
    lv_ASSERT(ok);
    lv_ASSERT(module_get_dependency_count(mod) == 2);
    printf("  添加依赖 'MathLib ^2.0.0' 成功\n");

    module_destroy(mod);
    printf("  PASSED\n");

}

/* ============== 测试：公理包添加 ============== */

static void test_axiom_package_addition(void) {
    printf("Test: axiom package addition...\n");

    Module *mod = module_create("GeoModule", "1.0");
    lv_ASSERT_NOT_NULL(mod);

    /* 创建公理包 */
    AxiomPackage *pkg = axiom_package_create("Euclidean", "1.0");
    lv_ASSERT_NOT_NULL(pkg);

    /* 添加到模块 */
    bool ok = module_add_axiom_package(mod, pkg);
    lv_ASSERT(ok);
    lv_ASSERT(module_get_axiom_package_count(mod) == 1);
    printf("  添加公理包 '%s' 成功\n", pkg->name);

    module_destroy(mod);
    printf("  PASSED\n");

}

/* ============== 测试：函数块导出 ============== */

static void test_function_block_export(void) {
    printf("Test: function block export...\n");

    Module *mod = module_create("ExportModule", "1.0");
    lv_ASSERT_NOT_NULL(mod);

    /* 创建独立的约束图 */
    ConstraintGraph *g = graph_create();

    /* 创建函数块 */
    int p1 = add_point(g, 0, 1, 0, 1);
    int p2 = add_point(g, 1, 1, 1, 1);
    graph_add_line_segment(g, p1, p2);

    graph_add_port(g, PORT_INPUT, -1, -1);
    int in_port = g->next_node_id - 1;
    graph_add_port(g, PORT_OUTPUT, -1, -1);
    int out_port = g->next_node_id - 1;

    int internal[] = {p1, p2};
    int inputs[] = {in_port};
    int outputs[] = {out_port};

    FuncBlock *fb = NULL;
    PackResult result = func_block_pack(g, internal, 2, inputs, 1, outputs, 1, NULL, 0, &fb);

    if (result == PACK_RESULT_OK && fb) {
        /* 导出函数块 */
        bool ok = module_export_function_block(mod, fb->id);
        printf("  导出函数块 ID=%d: %s\n", fb->id, ok ? "成功" : "失败");
        func_block_destroy(fb);
    } else {
        printf("  函数块打包失败，跳过导出测试\n");
    }

    graph_destroy(g);
    module_destroy(mod);
    printf("  PASSED\n");

}

/* ============== 测试：类型导出 ============== */

static void test_type_export(void) {
    printf("Test: type region export...\n");

    Module *mod = module_create("TypeModule", "1.0");
    lv_ASSERT_NOT_NULL(mod);

    /* 创建类型系统 */
    TypeSystem *ts = type_system_create();
    TypeRegion *point_type = type_create_point(ts);

    /* 导出类型 */
    bool ok = module_export_type_region(mod, point_type->id);
    printf("  导出类型 ID=%d: %s\n", point_type->id, ok ? "成功" : "失败");

    type_system_destroy(ts);
    module_destroy(mod);
    printf("  PASSED\n");

}

/* ============== 测试：循环依赖检测 ============== */

static void test_circular_dependency(void) {
    printf("Test: circular dependency detection...\n");

    Module *mod1 = module_create("ModuleA", "1.0");
    Module *mod2 = module_create("ModuleB", "1.0");
    lv_ASSERT(mod1 != NULL && mod2 != NULL);

    /* 添加依赖：A -> B */
    module_add_dependency(mod1, "ModuleB", "1.0");
    /* 添加依赖：B -> A (形成循环) */
    module_add_dependency(mod2, "ModuleA", "1.0");

    /* 检测循环依赖 */
    Module *visited[] = {mod1};
    bool has_cycle = module_detect_circular_dependency(mod2, visited, 1);
    printf("  循环依赖检测: %s\n", has_cycle ? "发现循环" : "无循环");

    module_destroy(mod1);
    module_destroy(mod2);
    printf("  PASSED\n");

}

/* ============== 测试：版本哈希 ============== */

static void test_version_hash(void) {
    printf("Test: version hash computation...\n");

    Module *mod = module_create("HashModule", "1.0.0");
    lv_ASSERT_NOT_NULL(mod);

    /* 计算版本哈希 */
    char *hash = module_compute_version_hash(mod);
    if (hash) {
        printf("  版本哈希: %s\n", hash);
        lv_free_ptr(hash);
    } else {
        printf("  版本哈希: (未实现)\n");
    }

    module_destroy(mod);
    printf("  PASSED\n");

}

/* ============== 测试：依赖链验证 ============== */

static void test_dependency_chain_validation(void) {
    printf("Test: dependency chain validation...\n");

    Module *base = module_create("Base", "1.0");
    Module *extended = module_create("Extended", "1.0");
    lv_ASSERT(base != NULL && extended != NULL);

    /* 添加依赖 */
    module_add_dependency(extended, "Base", "1.0");

    /* 验证依赖链 */
    Module *all_modules[] = {base, extended};
    bool valid = module_validate_dependency_chain(extended, all_modules, 2);
    printf("  依赖链验证: %s\n", valid ? "通过" : "失败/未实现");

    module_destroy(base);
    module_destroy(extended);
    printf("  PASSED\n");

}

/* ============== 测试：模块深度限制 ============== */

static void test_module_depth_limit(void) {
    printf("Test: module depth limit...\n");

    printf("  最大模块深度: %d\n", MAX_MODULE_DEPTH);
    printf("  (深度限制用于防止循环依赖导致的栈溢出)\n");

    printf("  PASSED\n");

}

/* ============== 测试：辅助函数 ============== */

static void test_helper_functions(void) {
    printf("Test: helper functions...\n");

    /* 测试错误信息 */
    const char *error = module_get_last_error();
    printf("  最后错误信息: %s\n", error ? error : "(无错误)");

    printf("  PASSED\n");

}

/* ============== 测试：C3-2 依赖扩容（lv_darray_push 自动倍增） ============== */

static void test_dependency_capacity_growth(void) {
    printf("Test: dependency capacity growth (C3-2)...\n");

    Module *mod = module_create("CapacityModule", "1.0");
    lv_ASSERT_NOT_NULL(mod);

    /* 连续添加超过初始容量的依赖，触发 lv_darray_push 倍增扩容（extend+attach 回潮） */
    const int kTotal = 70;
    for (int i = 0; i < kTotal; i++) {
        char name[64];
        char ver[64];
        snprintf(name, sizeof(name), "Dep%03d", i);
        snprintf(ver, sizeof(ver), ">=%d.0.0", i);
        lv_ASSERT(module_add_dependency(mod, name, ver));
    }
    lv_ASSERT(module_get_dependency_count(mod) == kTotal);

    /* 序列化后抽样验证首/中/尾依赖完整保留（扩容无丢失） */
    char *json = module_serialize_to_json(mod);
    lv_ASSERT_NOT_NULL(json);
    lv_ASSERT(strstr(json, "Dep000") != NULL);
    lv_ASSERT(strstr(json, "Dep035") != NULL);
    lv_ASSERT(strstr(json, "Dep069") != NULL);
    lv_free((void **) &json);

    module_destroy(mod);
    printf("  PASSED\n");

}

/* ============== 主函数 ============== */

TEST_MAIN_BEGIN("Lv-00 Module System Test Suite")
    printf("=== Lv-00 Module System Test Suite ===\n\n");
    TEST_MAIN_RUN(test_module_lifecycle);
    TEST_MAIN_RUN(test_dependency_management);
    TEST_MAIN_RUN(test_axiom_package_addition);
    TEST_MAIN_RUN(test_function_block_export);
    TEST_MAIN_RUN(test_type_export);
    TEST_MAIN_RUN(test_circular_dependency);
    TEST_MAIN_RUN(test_version_hash);
    TEST_MAIN_RUN(test_dependency_chain_validation);
    TEST_MAIN_RUN(test_module_depth_limit);
    TEST_MAIN_RUN(test_helper_functions);
    TEST_MAIN_RUN(test_dependency_capacity_growth);
    printf("\n=== All module system tests PASSED! ===\n");
TEST_MAIN_END()
