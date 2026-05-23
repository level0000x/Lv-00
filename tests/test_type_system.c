/**
 * @file test_type_system.c
 * @brief 类型系统测试 - 宇宙层级、类型等价检查、类型推断
 *
 * 测试内容：
 * - 类型系统创建与管理
 * - 类型区域创建（点、线段、区域、函数、乘积、和）
 * - 宇宙层级检查
 * - 类型等价检查
 * - 类型推断
 * - 类型变量实例化
 * - 非良基模式
 */

#include "lv00.h"
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

/* ============== 测试：类型系统生命周期 ============== */

static int test_type_system_lifecycle(void)
{
    printf("Test: type system lifecycle...\n");

    TypeSystem *ts = type_system_create();
    assert(ts != NULL);
    assert(ts->well_founded == true);
    assert(ts->cumulative == true);
    assert(ts->type_region_count == 0);

    printf("  类型系统创建成功\n");

    type_system_destroy(ts);
    printf("  PASSED\n");
    return 0;
}

/* ============== 测试：类型系统设置 ============== */

static int test_type_system_settings(void)
{
    printf("Test: type system settings...\n");

    TypeSystem *ts = type_system_create();
    assert(ts != NULL);

    /* 测试良基模式设置 */
    type_system_set_well_founded(ts, false);
    assert(ts->well_founded == false);
    printf("  良基模式: 关闭\n");

    type_system_set_well_founded(ts, true);
    assert(ts->well_founded == true);
    printf("  良基模式: 开启\n");

    /* 测试累积性设置 */
    type_system_set_cumulative(ts, false);
    assert(ts->cumulative == false);
    printf("  累积性: 关闭\n");

    type_system_set_cumulative(ts, true);
    assert(ts->cumulative == true);
    printf("  累积性: 开启\n");

    type_system_destroy(ts);
    printf("  PASSED\n");
    return 0;
}

/* ============== 测试：基本类型创建 ============== */

static int test_basic_type_creation(void)
{
    printf("Test: basic type creation...\n");

    TypeSystem *ts = type_system_create();
    assert(ts != NULL);

    /* 创建点类型 */
    TypeRegion *point_type = type_create_point(ts);
    assert(point_type != NULL);
    assert(point_type->kind == TYPE_KIND_POINT);
    assert(point_type->level == UNIVERSE_BASE);
    printf("  点类型创建成功, ID=%d\n", point_type->id);

    /* 创建线段类型 */
    TypeRegion *segment_type = type_create_line_segment(ts);
    assert(segment_type != NULL);
    assert(segment_type->kind == TYPE_KIND_LINE_SEGMENT);
    printf("  线段类型创建成功, ID=%d\n", segment_type->id);

    /* 创建区域类型 */
    int contained_ids[] = {1, 2, 3};
    TypeRegion *region_type = type_create_region(ts, contained_ids, 3);
    assert(region_type != NULL);
    assert(region_type->kind == TYPE_KIND_REGION);
    assert(region_type->contained_count == 3);
    printf("  区域类型创建成功, 包含 %d 个节点\n", region_type->contained_count);

    assert(ts->type_region_count == 3);

    type_system_destroy(ts);
    printf("  PASSED\n");
    return 0;
}

/* ============== 测试：复合类型创建 ============== */

static int test_composite_type_creation(void)
{
    printf("Test: composite type creation...\n");

    TypeSystem *ts = type_system_create();
    assert(ts != NULL);

    /* 创建基本类型 */
    TypeRegion *point_type = type_create_point(ts);
    TypeRegion *segment_type = type_create_line_segment(ts);

    /* 创建函数类型: Point -> Segment */
    TypeRegion *func_type = type_create_function(ts, point_type, segment_type);
    assert(func_type != NULL);
    assert(func_type->kind == TYPE_KIND_FUNCTION);
    assert(func_type->input_type == point_type);
    assert(func_type->output_type == segment_type);
    printf("  函数类型创建成功\n");

    /* 创建乘积类型: Point * Segment */
    TypeRegion *prod_type = type_create_product(ts, point_type, segment_type);
    assert(prod_type != NULL);
    assert(prod_type->kind == TYPE_KIND_PRODUCT);
    assert(prod_type->left_type == point_type);
    assert(prod_type->right_type == segment_type);
    printf("  乘积类型创建成功\n");

    /* 创建和类型: Point + Segment */
    TypeRegion *sum_type = type_create_sum(ts, point_type, segment_type);
    assert(sum_type != NULL);
    assert(sum_type->kind == TYPE_KIND_SUM);
    printf("  和类型创建成功\n");

    type_system_destroy(ts);
    printf("  PASSED\n");
    return 0;
}

/* ============== 测试：类型变量 ============== */

static int test_type_variables(void)
{
    printf("Test: type variables...\n");

    TypeSystem *ts = type_system_create();
    assert(ts != NULL);

    /* 创建类型变量 */
    TypeRegion *var_a = type_create_variable(ts, "A");
    assert(var_a != NULL);
    assert(var_a->kind == TYPE_KIND_VARIABLE);
    assert(strcmp(var_a->variable_name, "A") == 0);
    printf("  类型变量 'A' 创建成功\n");

    TypeRegion *var_b = type_create_variable(ts, "B");
    assert(var_b != NULL);
    assert(strcmp(var_b->variable_name, "B") == 0);
    printf("  类型变量 'B' 创建成功\n");

    /* 创建依赖类型变量的函数类型 */
    TypeRegion *poly_func = type_create_function(ts, var_a, var_b);
    assert(poly_func != NULL);
    printf("  多态函数类型 A -> B 创建成功\n");

    type_system_destroy(ts);
    printf("  PASSED\n");
    return 0;
}

/* ============== 测试：依赖类型 ============== */

static int test_dependent_types(void)
{
    printf("Test: dependent types...\n");

    TypeSystem *ts = type_system_create();
    assert(ts != NULL);

    /* 创建依赖类型 */
    int param_id = 42;
    TypeRegion *body_type = type_create_point(ts);
    TypeRegion *dep_type = type_create_dependent(ts, param_id, body_type);

    assert(dep_type != NULL);
    assert(dep_type->kind == TYPE_KIND_DEPENDENT);
    assert(dep_type->param_node_id == param_id);
    assert(dep_type->body_type == body_type);
    printf("  依赖类型创建成功, 参数ID=%d\n", param_id);

    type_system_destroy(ts);
    printf("  PASSED\n");
    return 0;
}

/* ============== 测试：底部类型 ============== */

static int test_bottom_type(void)
{
    printf("Test: bottom type...\n");

    TypeSystem *ts = type_system_create();
    assert(ts != NULL);

    TypeRegion *bottom = type_create_bottom(ts);
    assert(bottom != NULL);
    assert(bottom->kind == TYPE_KIND_BOTTOM);
    printf("  底部类型创建成功\n");

    type_system_destroy(ts);
    printf("  PASSED\n");
    return 0;
}

/* ============== 测试：宇宙层级检查 ============== */

static int test_universe_level(void)
{
    printf("Test: universe level checking...\n");

    TypeSystem *ts = type_system_create();
    assert(ts != NULL);

    TypeRegion *point = type_create_point(ts);
    TypeRegion *segment = type_create_line_segment(ts);
    TypeRegion *region = type_create_region(ts, NULL, 0);

    /* 检查层级 */
    UniverseLevel point_level = type_get_level(point);
    UniverseLevel segment_level = type_get_level(segment);
    UniverseLevel region_level = type_get_level(region);

    printf("  点层级: %d\n", point_level);
    printf("  线段层级: %d\n", segment_level);
    printf("  区域层级: %d\n", region_level);

    assert(point_level == UNIVERSE_BASE);
    assert(segment_level == UNIVERSE_BASE);
    assert(region_level == UNIVERSE_TYPE_1);

    /* 检查层级有效性 */
    bool valid = type_check_level_validity(ts, region, point);
    printf("  区域包含点: %s\n", valid ? "有效" : "无效");

    type_system_destroy(ts);
    printf("  PASSED\n");
    return 0;
}

/* ============== 测试：累积性检查 ============== */

static int test_cumulative_checking(void)
{
    printf("Test: cumulative checking...\n");

    TypeSystem *ts = type_system_create();
    assert(ts != NULL);

    TypeRegion *point = type_create_point(ts);
    TypeRegion *region = type_create_region(ts, NULL, 0);

    /* 检查累积性 */
    bool cumulative = type_check_cumulative(ts, point, region);
    printf("  点累积到区域: %s\n", cumulative ? "是" : "否");

    type_system_destroy(ts);
    printf("  PASSED\n");
    return 0;
}

/* ============== 测试：类型等价检查 ============== */

static int test_type_equivalence(void)
{
    printf("Test: type equivalence...\n");

    TypeSystem *ts = type_system_create();
    assert(ts != NULL);

    /* 创建相同类型 */
    TypeRegion *point1 = type_create_point(ts);
    TypeRegion *point2 = type_create_point(ts);

    /* 检查等价 */
    TypeEquivResult equiv = type_check_equivalence(ts, point1, point2, false);
    printf("  两个点类型等价: %s\n", 
           equiv == TYPE_EQUIV_OK ? "是" : 
           equiv == TYPE_EQUIV_NOT_EQUIV ? "否" : "未知");

    /* 创建不同类型 */
    TypeRegion *segment = type_create_line_segment(ts);
    TypeEquivResult equiv2 = type_check_equivalence(ts, point1, segment, false);
    printf("  点与线段等价: %s\n",
           equiv2 == TYPE_EQUIV_OK ? "是" :
           equiv2 == TYPE_EQUIV_NOT_EQUIV ? "否" : "未知");

    type_system_destroy(ts);
    printf("  PASSED\n");
    return 0;
}

/* ============== 测试：类型推断 ============== */

static int test_type_inference(void)
{
    printf("Test: type inference...\n");

    TypeSystem *ts = type_system_create();
    ConstraintGraph *g = graph_create();
    assert(ts != NULL && g != NULL);

    /* 创建点 */
    SymbolicCoord *cx = symbolic_coord_create_rational(0, 1);
    SymbolicCoord *cy = symbolic_coord_create_rational(0, 1);
    SymbolicCoord *coords[] = {cx, cy};
    graph_add_point(g, coords, 2);
    int point_id = g->next_node_id - 1;

    /* 推断节点类型 */
    TypeRegion *inferred_type = NULL;
    bool success = type_infer_node(ts, g, point_id, &inferred_type);
    printf("  类型推断: %s\n", success ? "成功" : "失败");

    if (inferred_type) {
        printf("  推断类型: %s\n", type_kind_to_string(inferred_type->kind));
    }

    graph_destroy(g);
    type_system_destroy(ts);
    printf("  PASSED\n");
    return 0;
}

/* ============== 测试：类型别名 ============== */

static int test_type_alias(void)
{
    printf("Test: type alias...\n");

    TypeSystem *ts = type_system_create();
    assert(ts != NULL);

    TypeRegion *point = type_create_point(ts);

    /* 添加别名 */
    bool success = type_add_alias(point, "Point");
    printf("  添加别名: %s\n", success ? "成功" : "失败");

    if (point->alias_name) {
        printf("  别名: %s\n", point->alias_name);
    }

    type_system_destroy(ts);
    printf("  PASSED\n");
    return 0;
}

/* ============== 测试：类型变量实例化 ============== */

static int test_type_instantiation(void)
{
    printf("Test: type variable instantiation...\n");

    TypeSystem *ts = type_system_create();
    assert(ts != NULL);

    /* 创建类型变量 */
    TypeRegion *var = type_create_variable(ts, "T");
    assert(var != NULL);

    /* 创建具体类型 */
    TypeRegion *point = type_create_point(ts);

    /* 实例化变量 */
    bool success = type_instantiate_variable(ts, var->variable_id, point);
    printf("  实例化: %s\n", success ? "成功" : "失败");

    type_system_destroy(ts);
    printf("  PASSED\n");
    return 0;
}

/* ============== 测试：非良基模式 ============== */

static int test_non_well_founded(void)
{
    printf("Test: non-well-founded mode...\n");

    TypeSystem *ts = type_system_create();
    assert(ts != NULL);

    /* 关闭良基模式 */
    type_system_set_well_founded(ts, false);

    TypeRegion *point = type_create_point(ts);

    /* 检测循环 */
    bool has_cycle = type_detect_cycle(ts, point);
    printf("  检测循环: %s\n", has_cycle ? "是" : "否");

    /* 检查非良基相容性 */
    bool compatible = type_check_non_well_founded_compatibility(ts, point);
    printf("  非良基相容: %s\n", compatible ? "是" : "否");

    type_system_destroy(ts);
    printf("  PASSED\n");
    return 0;
}

/* ============== 测试：辅助函数 ============== */

static int test_helper_functions(void)
{
    printf("Test: helper functions...\n");

    /* 测试类型种类转字符串 */
    const char *str = type_kind_to_string(TYPE_KIND_POINT);
    printf("  POINT -> %s\n", str);

    str = type_kind_to_string(TYPE_KIND_FUNCTION);
    printf("  FUNCTION -> %s\n", str);

    str = type_kind_to_string(TYPE_KIND_VARIABLE);
    printf("  VARIABLE -> %s\n", str);

    /* 测试宇宙层级转字符串 */
    str = universe_level_to_string(UNIVERSE_BASE);
    printf("  BASE -> %s\n", str);

    str = universe_level_to_string(UNIVERSE_TYPE_1);
    printf("  TYPE_1 -> %s\n", str);

    /* 测试类型等价结果转字符串 */
    str = type_equiv_result_to_string(TYPE_EQUIV_OK);
    printf("  EQUIV_OK -> %s\n", str);

    str = type_equiv_result_to_string(TYPE_EQUIV_NOT_EQUIV);
    printf("  NOT_EQUIV -> %s\n", str);

    /* 测试类型检查结果转字符串 */
    str = type_check_result_to_string(TYPE_CHECK_OK);
    printf("  CHECK_OK -> %s\n", str);

    str = type_check_result_to_string(TYPE_CHECK_MISMATCH);
    printf("  MISMATCH -> %s\n", str);

    printf("  PASSED\n");
    return 0;
}

/* ============== 测试：类型附加到节点 ============== */

static int test_type_attach_to_node(void)
{
    printf("Test: type attach to node...\n");

    TypeSystem *ts = type_system_create();
    assert(ts != NULL);

    /* 创建类型 */
    TypeRegion *point_type = type_create_point(ts);
    TypeRegion *segment_type = type_create_line_segment(ts);
    assert(point_type != NULL);
    assert(segment_type != NULL);

    /* 附加类型到节点 */
    bool result = type_attach_to_node(ts, 1, point_type);
    printf("  附加点类型到节点1: %s\n", result ? "成功" : "失败");
    assert(result);

    /* 获取节点类型 */
    TypeRegion *retrieved = type_get_node_type(ts, 1);
    assert(retrieved == point_type);
    printf("  获取节点1类型: %s\n", type_kind_to_string(retrieved->kind));

    /* 附加另一个类型到另一个节点 */
    result = type_attach_to_node(ts, 2, segment_type);
    assert(result);
    retrieved = type_get_node_type(ts, 2);
    assert(retrieved == segment_type);
    printf("  附加线段类型到节点2: 成功\n");

    /* 更新已有节点的类型 */
    TypeRegion *region_type = type_create_region(ts, NULL, 0);
    result = type_attach_to_node(ts, 1, region_type);
    assert(result);
    retrieved = type_get_node_type(ts, 1);
    assert(retrieved == region_type);
    printf("  更新节点1类型为区域: 成功\n");

    /* 获取不存在的节点类型 */
    retrieved = type_get_node_type(ts, 999);
    assert(retrieved == NULL);
    printf("  获取不存在节点类型: NULL (正确)\n");

    /* 分离节点类型 */
    result = type_detach_node_type(ts, 1);
    assert(result);
    retrieved = type_get_node_type(ts, 1);
    assert(retrieved == NULL);
    printf("  分离节点1类型: 成功\n");

    /* 分离不存在的节点类型 */
    result = type_detach_node_type(ts, 999);
    assert(!result);
    printf("  分离不存在节点类型: false (正确)\n");

    /* 边界情况：无效参数 */
    result = type_attach_to_node(NULL, 1, point_type);
    assert(!result);
    result = type_attach_to_node(ts, 0, point_type);
    assert(!result);
    result = type_attach_to_node(ts, 1, NULL);
    assert(!result);
    printf("  无效参数处理: 正确\n");

    type_system_destroy(ts);
    printf("  PASSED\n");
    return 0;
}

/* ============== 测试：累积性层级检查 ============== */

static int test_cumulative_level_check(void)
{
    printf("Test: cumulative universe level check...\n");

    TypeSystem *ts = type_system_create();
    assert(ts != NULL);

    /* 第0层类型 */
    TypeRegion *point = type_create_point(ts);
    assert(point->level == UNIVERSE_BASE);

    /* 第1层类型 */
    TypeRegion *region = type_create_region(ts, NULL, 0);
    assert(region->level == UNIVERSE_TYPE_1);

    /* 累积性：第0层类型可以出现在第1层区域 */
    bool cumulative = type_check_cumulative(ts, point, region);
    assert(cumulative == true);
    printf("  第0层累积到第1层: %s\n", cumulative ? "是" : "否");

    /* 同层级也兼容 */
    cumulative = type_check_cumulative(ts, point, point);
    assert(cumulative == true);
    printf("  同层级(0->0): %s\n", cumulative ? "是" : "否");

    /* 关闭累积性 */
    type_system_set_cumulative(ts, false);
    cumulative = type_check_cumulative(ts, point, region);
    assert(cumulative == false);
    printf("  关闭累积性后(0->1): %s\n", cumulative ? "是" : "否");

    /* 重新开启 */
    type_system_set_cumulative(ts, true);

    /* 创建第2层类型（函数类型 Point -> Point = max(0,0)+1 = 1） */
    TypeRegion *func_type = type_create_function(ts, point, point);
    printf("  函数类型层级: %d\n", func_type->level);

    /* 第0层累积到第2层 */
    cumulative = type_check_cumulative(ts, point, func_type);
    assert(cumulative == true);
    printf("  第0层累积到第%d层: %s\n", func_type->level,
           cumulative ? "是" : "否");

    type_system_destroy(ts);
    printf("  PASSED\n");
    return 0;
}

/* ============== 测试：依赖类型检查 ============== */

static int test_dependent_type_check(void)
{
    printf("Test: dependent type check...\n");

    TypeSystem *ts = type_system_create();
    assert(ts != NULL);

    /* 测试1：相同基本类型兼容 */
    TypeRegion *point1 = type_create_point(ts);
    TypeRegion *point2 = type_create_point(ts);
    bool result = type_check_dependent(ts, point1, point2, NULL);
    assert(result);
    printf("  点与点兼容: %s\n", result ? "是" : "否");

    /* 测试2：不同基本类型不兼容 */
    TypeRegion *segment = type_create_line_segment(ts);
    result = type_check_dependent(ts, point1, segment, NULL);
    assert(!result);
    printf("  点与线段不兼容: %s\n", result ? "是" : "否");

    /* 测试3：类型变量与任何类型兼容 */
    TypeRegion *var = type_create_variable(ts, "T");
    result = type_check_dependent(ts, point1, var, NULL);
    assert(result);
    printf("  点与类型变量兼容: %s\n", result ? "是" : "否");

    result = type_check_dependent(ts, var, segment, NULL);
    assert(result);
    printf("  类型变量与线段兼容: %s\n", result ? "是" : "否");

    /* 测试4：底部类型与任何类型兼容 */
    TypeRegion *bottom = type_create_bottom(ts);
    result = type_check_dependent(ts, point1, bottom, NULL);
    assert(result);
    printf("  点与底部类型兼容: %s\n", result ? "是" : "否");

    /* 测试5：函数类型递归检查 */
    TypeRegion *func1 = type_create_function(ts, point1, segment);
    TypeRegion *func2 = type_create_function(ts, point2, segment);
    result = type_check_dependent(ts, func1, func2, NULL);
    assert(result);
    printf("  相同签名的函数类型兼容: %s\n", result ? "是" : "否");

    /* 测试6：函数类型签名不同不兼容 */
    TypeRegion *point3 = type_create_point(ts);
    TypeRegion *func3 = type_create_function(ts, point3, point3);
    result = type_check_dependent(ts, func1, func3, NULL);
    assert(!result);
    printf("  不同签名的函数类型不兼容: %s\n", result ? "是" : "否");

    /* 测试7：依赖类型的体类型检查 */
    TypeRegion *body = type_create_point(ts);
    TypeRegion *dep_type = type_create_dependent(ts, 42, body);
    result = type_check_dependent(ts, point1, dep_type, NULL);
    assert(result);
    printf("  点与依赖类型(体为点)兼容: %s\n", result ? "是" : "否");

    /* 测试8：依赖类型体不兼容 */
    TypeRegion *body_segment = type_create_line_segment(ts);
    TypeRegion *dep_type2 = type_create_dependent(ts, 43, body_segment);
    result = type_check_dependent(ts, point1, dep_type2, NULL);
    assert(!result);
    printf("  点与依赖类型(体为线段)不兼容: %s\n", result ? "是" : "否");

    /* 测试9：无效参数 */
    result = type_check_dependent(NULL, point1, point2, NULL);
    assert(!result);
    result = type_check_dependent(ts, NULL, point2, NULL);
    assert(!result);
    result = type_check_dependent(ts, point1, NULL, NULL);
    assert(!result);
    printf("  无效参数处理: 正确\n");

    /* 测试10：非累积模式下的层级严格相等 */
    type_system_set_cumulative(ts, false);
    TypeRegion *region = type_create_region(ts, NULL, 0);
    result = type_check_dependent(ts, region, point1, NULL);
    assert(!result);
    printf("  非累积模式下层级不等的类型不兼容: %s\n", result ? "是" : "否");

    type_system_destroy(ts);
    printf("  PASSED\n");
    return 0;
}

/* ============== 主函数 ============== */

int main(void)
{
    printf("=== Lv-00 Type System Test Suite ===\n\n");

    test_type_system_lifecycle();
    test_type_system_settings();
    test_basic_type_creation();
    test_composite_type_creation();
    test_type_variables();
    test_dependent_types();
    test_bottom_type();
    test_universe_level();
    test_cumulative_checking();
    test_type_equivalence();
    test_type_inference();
    test_type_alias();
    test_type_instantiation();
    test_non_well_founded();
    test_helper_functions();
    test_type_attach_to_node();
    test_cumulative_level_check();
    test_dependent_type_check();

    printf("\n=== All type system tests PASSED! ===\n");
    return 0;
}
