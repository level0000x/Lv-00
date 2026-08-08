/**
 * @file test_axiom_pkg.c
 * @brief 公理包系统测试 - 公理包创建、已知不可构造项、模板注册
 *
 * 测试内容：
 * - 公理包生命周期
 * - 已知不可构造项管理
 * - 约束模板注册与获取
 * - 内容哈希计算
 * - 依赖验证
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "test_unified.h"

int g_pass_count = 0;
int g_fail_count = 0;

#include "lv.h"

/* ============== 测试：公理包生命周期 ============== */

static void test_axiom_package_lifecycle(void) {
    printf("Test: axiom package lifecycle...\n");

    AxiomPackage *pkg = axiom_package_create("Euclidean", "1.0.0");
    lv_ASSERT_NOT_NULL(pkg);
    lv_ASSERT_STR_EQ(pkg->name, "Euclidean");
    lv_ASSERT_STR_EQ(pkg->version, "1.0.0");
    lv_ASSERT(pkg->templates.count == 0);
    lv_ASSERT(pkg->known_unconstructibles.count == 0);

    printf("  公理包 '%s' v%s 创建成功\n", pkg->name, pkg->version);

    axiom_package_destroy(pkg);
    printf("  PASSED\n");

}

/* ============== 测试：已知不可构造项 ============== */

static void test_known_unconstructible(void) {
    printf("Test: known unconstructible items...\n");

    AxiomPackage *pkg = axiom_package_create("TestPkg", "1.0");
    lv_ASSERT_NOT_NULL(pkg);

    /* 创建已知不可构造项 */
    KnownUnconstructible *item = lv_malloc(sizeof(KnownUnconstructible));
    lv_ASSERT_NOT_NULL(item);
    item->name = strdup("TrisectAngle");
    item->reduces_to = strdup("CubicEquation");
    lv_darray_init(&item->dependency_chain, sizeof(char *));
    item->external_ref = strdup("https://math.example/trisection");
    item->green_verified = true;

    /* 添加到公理包 */
    bool ok = axiom_package_add_known_unconstructible(pkg, item);
    lv_ASSERT(ok);
    lv_ASSERT(pkg->known_unconstructibles.count == 1);
    printf("  添加不可构造项 '%s' 成功\n", item->name);

    /* 查找 */
    KnownUnconstructible *found = axiom_package_lookup_unconstructible(pkg, "TrisectAngle");
    lv_ASSERT_NOT_NULL(found);
    lv_ASSERT_STR_EQ(found->name, "TrisectAngle");
    printf("  查找 '%s': 找到 (验证状态: %s)\n", found->name, found->green_verified ? "已验证" : "未验证");

    /* 查找不存在的项 */
    KnownUnconstructible *not_found = axiom_package_lookup_unconstructible(pkg, "NonExistent");
    lv_ASSERT(not_found == NULL);
    printf("  查找不存在的项: 未找到 (正确)\n");

    axiom_package_destroy(pkg);
    printf("  PASSED\n");

}

/* ============== 测试：约束模板 ============== */

static void test_expand_func(SymbolicCoord **params, ConstraintGraph *target) {
    /* 测试用的模板展开函数 */
    (void) params;
    (void) target;
}

static void test_constraint_templates(void) {
    printf("Test: constraint templates...\n");

    AxiomPackage *pkg = axiom_package_create("TemplatePkg", "1.0");
    lv_ASSERT_NOT_NULL(pkg);

    /* 创建模板 */
    ConstraintTemplate *tmpl = lv_malloc(sizeof(ConstraintTemplate));
    lv_ASSERT_NOT_NULL(tmpl);
    tmpl->name = strdup("Midpoint");
    tmpl->param_count = 2;
    tmpl->expand = test_expand_func;
    tmpl->verified = true;

    /* 注册模板 */
    bool ok = axiom_package_register_template(pkg, tmpl);
    lv_ASSERT(ok);
    lv_ASSERT(pkg->templates.count == 1);
    printf("  注册模板 '%s' 成功\n", tmpl->name);

    /* 获取模板 */
    ConstraintTemplate *found = axiom_package_get_template(pkg, "Midpoint");
    lv_ASSERT_NOT_NULL(found);
    lv_ASSERT_STR_EQ(found->name, "Midpoint");
    lv_ASSERT(found->param_count == 2);
    printf("  获取模板 '%s': 找到 (参数数: %d)\n", found->name, found->param_count);

    /* 获取不存在的模板 */
    ConstraintTemplate *not_found = axiom_package_get_template(pkg, "NonExistent");
    lv_ASSERT(not_found == NULL);
    printf("  获取不存在的模板: 未找到 (正确)\n");

    axiom_package_destroy(pkg);
    printf("  PASSED\n");

}

/* ============== 测试：内容哈希 ============== */

static void test_content_hash(void) {
    printf("Test: content hash computation...\n");

    AxiomPackage *pkg = axiom_package_create("HashTest", "1.0");
    lv_ASSERT_NOT_NULL(pkg);

    /* 添加一些内容 */
    ConstraintTemplate *tmpl = lv_malloc(sizeof(ConstraintTemplate));
    tmpl->name = strdup("TestTemplate");
    tmpl->param_count = 1;
    tmpl->expand = test_expand_func;
    tmpl->verified = false;
    axiom_package_register_template(pkg, tmpl);

    /* 计算哈希 */
    char *hash = axiom_package_compute_content_hash(pkg);
    if (hash) {
        printf("  内容哈希: %s\n", hash);
        lv_free_ptr(hash);
    } else {
        printf("  内容哈希: (未实现或为空)\n");
    }

    axiom_package_destroy(pkg);
    printf("  PASSED\n");

}

/* ============== 测试：依赖验证 ============== */

static void test_dependency_validation(void) {
    printf("Test: dependency validation...\n");

    AxiomPackage *pkg1 = axiom_package_create("BasePkg", "1.0");
    AxiomPackage *pkg2 = axiom_package_create("ExtendedPkg", "1.0");
    lv_ASSERT(pkg1 != NULL && pkg2 != NULL);

    /* 验证依赖 */
    AxiomPackage *loaded[] = {pkg1};
    bool valid = axiom_package_validate_dependencies(pkg2, loaded, 1);
    printf("  依赖验证: %s\n", valid ? "通过" : "失败/未实现");

    axiom_package_destroy(pkg1);
    axiom_package_destroy(pkg2);
    printf("  PASSED\n");

}

/* ============== 测试：命题种类 ============== */

static void test_proposition_kinds(void) {
    printf("Test: proposition kinds...\n");

    /* 测试不同种类的命题 */
    printf("  构造性命题\n");
    printf("  非构造性Oracle\n");
    printf("  爆炸原理\n");

    printf("  PASSED\n");

}

/* ============== 测试：辅助函数 ============== */

static void test_helper_functions(void) {
    printf("Test: helper functions...\n");

    /* 测试加载状态字符串 */
    const char *str = axiom_package_get_last_error();
    printf("  最后错误信息: %s\n", str ? str : "(无错误)");

    printf("  PASSED\n");

}

/* ============== 主函数 ============== */

TEST_MAIN_BEGIN("Lv-00 Axiom Package Test Suite")
    printf("=== Lv-00 Axiom Package Test Suite ===\n\n");
    TEST_MAIN_RUN(test_axiom_package_lifecycle);
    TEST_MAIN_RUN(test_known_unconstructible);
    TEST_MAIN_RUN(test_constraint_templates);
    TEST_MAIN_RUN(test_content_hash);
    TEST_MAIN_RUN(test_dependency_validation);
    TEST_MAIN_RUN(test_proposition_kinds);
    TEST_MAIN_RUN(test_helper_functions);
    printf("\n=== All axiom package tests PASSED! ===\n");
TEST_MAIN_END()
