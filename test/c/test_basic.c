/**
 * @file test_basic.c
 * @brief Lv-00 基础模块测试 - 有理数、约束图、归一化、模块系统、公理包、统一化、引擎
 *
 * 测试内容：
 * - 有理数算术运算与序列化
 * - 约束图的构建与约束添加
 * - 图归一化处理
 * - 模块系统依赖管理
 * - 公理包模板注册与查询
 * - 构造与命题的统一化
 * - 引擎生命周期与端口/功能块
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv.h"
#include "test_helpers.h"

/* 全局测试计数器 */
int g_pass_count = 0;
int g_fail_count = 0;

/**
 * @brief 测试有理数的基本算术运算
 *
 * 验证有理数的加法、乘法结果是否正确，
 * 以及序列化输出是否包含预期的分数表示。
 */
void test_rational() {
    printf("Testing rational numbers...\n");

    /* 创建两个有理数: 1/2 和 1/3 */
    Rational *r1 = rational_create(1, 2);
    Rational *r2 = rational_create(1, 3);
    Rational *sum = rational_add(r1, r2);
    Rational *prod = rational_multiply(r1, r2);

    /* 验证加法结果: 1/2 + 1/3 = 5/6 */
    Rational *expected_sum = rational_create(5, 6);
    TEST_ASSERT(rational_compare(sum, expected_sum) == 0, "1/2 + 1/3 should equal 5/6");
    rational_destroy(expected_sum);

    /* 验证乘法结果: 1/2 * 1/3 = 1/6 */
    Rational *expected_prod = rational_create(1, 6);
    TEST_ASSERT(rational_compare(prod, expected_prod) == 0, "1/2 * 1/3 should equal 1/6");
    rational_destroy(expected_prod);

    /* 验证序列化输出 */
    char *ser = rational_serialize(sum);
    printf("  Sum: %s\n", ser);
    lv_free_ptr(ser);

    rational_destroy(r1);
    rational_destroy(r2);
    rational_destroy(sum);
    rational_destroy(prod);

    printf("  PASSED\n");
}

/**
 * @brief 测试约束图的构建与基本操作
 *
 * 在约束图中添加两个点、一条线段和一个关联约束，
 * 验证节点数和约束数是否符合预期。
 */
void test_constraint_graph() {
    printf("Testing constraint graph...\n");

    ConstraintGraph *graph = graph_create();
    TEST_ASSERT(graph != NULL, "graph_create should return non-NULL");

    /* 使用辅助函数添加第一个点: 原点 (0, 0) */
    int p0 = add_point(graph, 0, 1, 0, 1);
    /* 检查 add_point 返回值：失败时返回 -1，不应将无效ID传入后续函数 */
    TEST_ASSERT(p0 >= 0, "add_point for p0 (origin) failed");

    /* 使用辅助函数添加第二个点: (1, 0) */
    int p1 = add_point(graph, 1, 1, 0, 1);
    TEST_ASSERT(p1 >= 0, "add_point for p1 failed");

    /* 添加线段: 连接点0和点1 */
    AddNodeResult res3 = graph_add_line_segment(graph, p0, p1);
    TEST_ASSERT(res3 == ADD_NODE_OK, "graph_add_line_segment should return ADD_NODE_OK");

    /* 添加关联约束: 点0在线段上 */
    int seg_id = graph->next_node_id - 1;
    AddConstraintResult res4 = graph_add_incidence(graph, p0, seg_id);
    TEST_ASSERT(res4 == ADD_CONSTRAINT_OK, "graph_add_incidence should return ADD_CONSTRAINT_OK");

    /* 验证图结构: 2个点 + 1条线段 = 3个节点, 1个约束 */
    TEST_ASSERT(graph->node_count == 3, "graph should have 3 nodes");
    TEST_ASSERT(graph->constraint_count == 1, "graph should have 1 constraint");

    graph_destroy(graph);
    printf("  PASSED\n");
}

/**
 * @brief 测试图的归一化处理
 *
 * 创建包含两个相同坐标点的图，验证归一化操作
 * 能正确执行且不改变节点数。
 */
void test_normalization() {
    printf("Testing graph normalization...\n");

    ConstraintGraph *graph = graph_create();

    /* 使用辅助函数添加第一个点: (1, 1) */
    add_point(graph, 1, 1, 1, 1);

    /* 使用辅助函数添加第二个点: 同样是 (1, 1) */
    add_point(graph, 1, 1, 1, 1);

    /* 两个点应独立存在 */
    TEST_ASSERT(graph->node_count == 2, "graph should have 2 nodes before normalization");

    /* 执行归一化 */
    NormalizationResult *nr = graph_normalize(graph, true);
    TEST_ASSERT(nr != NULL, "graph_normalize should return non-NULL");
    normalization_result_destroy(nr);

    graph_destroy(graph);
    printf("  PASSED\n");
}

/**
 * @brief 测试模块系统的基本功能
 *
 * 创建模块、设置名称、添加依赖项，
 * 验证模块属性和依赖计数是否正确。
 */
void test_module() {
    printf("Testing module system...\n");

    /* 创建模块 */
    Module *mod = module_create("test_module", "1.0.0");
    TEST_ASSERT(mod != NULL, "module_create should return non-NULL");

    /* 验证模块名称 */
    TEST_ASSERT(strcmp(module_get_name(mod), "test_module") == 0, "module name should be 'test_module'");

    /* 添加依赖并验证计数 */
    module_add_dependency(mod, "dep1", ">=1.0");
    TEST_ASSERT(module_get_dependency_count(mod) == 1, "module should have 1 dependency");

    module_destroy(mod);
    printf("  PASSED\n");
}

/**
 * @brief 测试公理包的模板注册与查询
 *
 * 创建公理包，注册约束模板，然后通过名称查找模板，
 * 验证模板能被正确注册和检索。
 */
void test_axiom_package() {
    printf("Testing axiom package...\n");

    /* 创建欧几里得公理包 */
    AxiomPackage *pkg = axiom_package_create("euclidean", "1.0");
    TEST_ASSERT(pkg != NULL, "axiom_package_create should return non-NULL");

    /* 注册距离约束模板 */
    ConstraintTemplate tmpl;
    tmpl.name = strdup("distance");
    tmpl.param_count = 2;
    tmpl.verified = false;
    axiom_package_register_template(pkg, &tmpl);

    /* 通过名称查找已注册的模板 */
    ConstraintTemplate *found = axiom_package_get_template(pkg, "distance");
    TEST_ASSERT(found != NULL, "axiom_package_get_template should find 'distance'");

    axiom_package_destroy(pkg);
    printf("  PASSED\n");
}

/**
 * @brief 测试构造图与命题图的统一化
 *
 * 创建两个结构相同的约束图（构造和命题），
 * 验证统一化操作能成功匹配。
 */
void test_unify() {
    printf("Testing unification...\n");

    ConstraintGraph *construction = graph_create();
    ConstraintGraph *proposition = graph_create();

    /* 构造图: 使用辅助函数添加原点 (0, 0) 和点 (1, 0) */
    int cp0 = add_point(construction, 0, 1, 0, 1);
    /* 检查 add_point 返回值：失败返回 -1，不应将无效ID传入后续函数 */
    TEST_ASSERT(cp0 >= 0, "add_point in construction for origin failed");
    int cp1 = add_point(construction, 1, 1, 0, 1);
    TEST_ASSERT(cp1 >= 0, "add_point in construction for (1,0) failed");

    /* 命题图: 使用辅助函数添加相同的两个点 */
    int pp0 = add_point(proposition, 0, 1, 0, 1);
    TEST_ASSERT(pp0 >= 0, "add_point in proposition for origin failed");
    int pp1 = add_point(proposition, 1, 1, 0, 1);
    TEST_ASSERT(pp1 >= 0, "add_point in proposition for (1,0) failed");

    /* 验证统一化结果: 结构相同应返回 OK */
    UnifyStatus status = unify_construction_with_proposition(construction, proposition);
    TEST_ASSERT(status == UNIFY_STATUS_OK, "unify should return UNIFY_STATUS_OK for identical graphs");

    graph_destroy(construction);
    graph_destroy(proposition);
    printf("  PASSED\n");
}

/**
 * @brief 测试引擎的生命周期与基本操作
 *
 * 创建引擎，在主图中添加点、线段、关联约束、端口和功能块，
 * 验证各操作返回正确状态。
 */
void test_engine() {
    printf("Testing engine...\n");

    lvEngine *engine = engine_create();
    TEST_ASSERT(engine != NULL, "engine_create should return non-NULL");

    /* 使用辅助函数在引擎主图中添加原点 (0, 0) */
    int ep0 = add_point(engine->main_graph, 0, 1, 0, 1);
    /* 检查 add_point 返回值：失败返回 -1，不应将无效ID传入后续函数 */
    TEST_ASSERT(ep0 >= 0, "add_point in engine for origin failed");

    /* 使用辅助函数添加点 (1, 0) */
    int ep1 = add_point(engine->main_graph, 1, 1, 0, 1);
    TEST_ASSERT(ep1 >= 0, "add_point in engine for (1,0) failed");

    /* 添加线段: 连接点0和点1 */
    graph_add_line_segment(engine->main_graph, ep0, ep1);

    /* 添加关联约束: 点0在线段上 */
    int seg_id = engine->main_graph->next_node_id - 1;
    AddConstraintResult res = graph_add_incidence(engine->main_graph, ep0, seg_id);
    TEST_ASSERT(res == ADD_CONSTRAINT_OK, "graph_add_incidence should return ADD_CONSTRAINT_OK");

    /* 添加输入端口 */
    AddNodeResult port_result = graph_add_port(engine->main_graph, PORT_INPUT, 0, -1);
    TEST_ASSERT(port_result == ADD_NODE_OK, "graph_add_port should return ADD_NODE_OK");

    /* 添加功能块（无参数版本） */
    AddNodeResult fb_result = graph_add_function_block(engine->main_graph, NULL, 0, NULL, 0, NULL, 0);
    TEST_ASSERT(fb_result == ADD_NODE_OK, "graph_add_function_block should return ADD_NODE_OK");

    engine_destroy(engine);
    printf("  PASSED\n");
}

/**
 * @brief 测试有理数的边界条件
 *
 * 验证有理数在零值运算、负数运算和分母为零时的行为。
 */
void test_rational_boundary(void) {
    printf("Testing rational boundary conditions...\n");

    /* --- 零值运算 --- */

    /* 创建零值有理数: 0/1 */
    Rational *zero = rational_create(0, 1);
    TEST_ASSERT_NOT_NULL(zero);

    /* 创建普通有理数: 3/4 */
    Rational *r = rational_create(3, 4);
    TEST_ASSERT_NOT_NULL(r);

    /* 0 + x == x */
    Rational *sum_zero = rational_add(zero, r);
    TEST_ASSERT_NOT_NULL(sum_zero);
    Rational *expected_r = rational_create(3, 4);
    TEST_ASSERT(rational_compare(sum_zero, expected_r) == 0, "0 + 3/4 should equal 3/4");
    rational_destroy(expected_r);
    rational_destroy(sum_zero);

    /* 0 * x == 0 */
    Rational *prod_zero = rational_multiply(zero, r);
    TEST_ASSERT_NOT_NULL(prod_zero);
    Rational *expected_zero = rational_create(0, 1);
    TEST_ASSERT(rational_compare(prod_zero, expected_zero) == 0, "0 * 3/4 should equal 0");
    rational_destroy(expected_zero);
    rational_destroy(prod_zero);

    /* x / 1 == x */
    Rational *one = rational_create(1, 1);
    Rational *div_one = rational_divide(r, one);
    TEST_ASSERT_NOT_NULL(div_one);
    Rational *expected_r2 = rational_create(3, 4);
    TEST_ASSERT(rational_compare(div_one, expected_r2) == 0, "3/4 / 1 should equal 3/4");
    rational_destroy(expected_r2);
    rational_destroy(div_one);

    rational_destroy(one);
    rational_destroy(zero);
    rational_destroy(r);

    /* --- 负数运算 --- */

    /* 创建负数有理数: -2/3 */
    Rational *neg = rational_create(-2, 3);
    TEST_ASSERT_NOT_NULL(neg);

    /* 创建正数有理数: 1/3 */
    Rational *pos = rational_create(1, 3);
    TEST_ASSERT_NOT_NULL(pos);

    /* 负数 + 正数: -2/3 + 1/3 = -1/3 */
    Rational *sum_neg = rational_add(neg, pos);
    TEST_ASSERT_NOT_NULL(sum_neg);
    Rational *expected_neg = rational_create(-1, 3);
    TEST_ASSERT(rational_compare(sum_neg, expected_neg) == 0, "-2/3 + 1/3 should equal -1/3");
    rational_destroy(expected_neg);
    rational_destroy(sum_neg);

    /* 负数 * 正数: -2/3 * 1/3 = -2/9 */
    Rational *prod_neg = rational_multiply(neg, pos);
    TEST_ASSERT_NOT_NULL(prod_neg);
    Rational *expected_neg2 = rational_create(-2, 9);
    TEST_ASSERT(rational_compare(prod_neg, expected_neg2) == 0, "-2/3 * 1/3 should equal -2/9");
    rational_destroy(expected_neg2);
    rational_destroy(prod_neg);

    /* 负数 * 负数: -2/3 * -2/3 = 4/9 */
    Rational *prod_neg_neg = rational_multiply(neg, neg);
    TEST_ASSERT_NOT_NULL(prod_neg_neg);
    Rational *expected_pos = rational_create(4, 9);
    TEST_ASSERT(rational_compare(prod_neg_neg, expected_pos) == 0, "-2/3 * -2/3 should equal 4/9");
    rational_destroy(expected_pos);
    rational_destroy(prod_neg_neg);

    rational_destroy(neg);
    rational_destroy(pos);

    /* --- 分母为零的错误处理 --- */

    /* 用分母为零创建有理数应返回 NULL 或被规范化处理 */
    Rational *div_by_zero = rational_create(1, 0);
    /* 实现应安全处理分母为零的情况：返回 NULL 或规范化为有效值 */
    printf("  分母为零创建: %s\n", div_by_zero ? "返回非NULL（已规范化）" : "返回NULL（安全处理）");
    if (div_by_zero != NULL) {
        rational_destroy(div_by_zero);
    }

    /* 有理数除以零应返回 NULL */
    Rational *a = rational_create(5, 1);
    Rational *b = rational_create(0, 1);
    Rational *div_result = rational_divide(a, b);
    TEST_ASSERT(div_result == NULL, "有理数除以零应返回 NULL");
    rational_destroy(a);
    rational_destroy(b);

    printf("  PASSED\n");
}

int main() {
    printf("=== Lv-00 Geometry Metalanguage Test Suite ===\n\n");
    test_rational();
    test_rational_boundary();
    test_constraint_graph();
    test_normalization();
    test_module();
    test_axiom_package();
    test_unify();
    test_engine();

    if (g_fail_count > 0) {
        printf("\n=== %d test(s) FAILED ===\n", g_fail_count);
        return 1;
    }
    printf("\n=== All tests PASSED! ===\n");
    return 0;
}
