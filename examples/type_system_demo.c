/**
 * @file type_system_demo.c
 * @brief 类型系统演示 —— 展示类型创建、检查与推断
 *
 * 本示例演示 Lv-00 类型系统的核心功能：
 * 1. 创建类型检查上下文（TypeSystem）
 * 2. 定义基本类型（点、线段、实数/区域）
 * 3. 构建复合类型（函数类型、乘积类型、和类型）
 * 4. 进行类型等价检查
 * 5. 执行类型推断
 * 6. 演示宇宙层级与累积性
 * 7. 打印类型信息
 *
 * Lv-00 的类型系统借鉴 Martin-Lof 类型论，支持：
 * - 宇宙层级机制（无限层级体系）
 * - 类型等价检查（直接比较和规范化等价）
 * - 类型推断（规则表驱动）
 * - 多态类型变量
 * - 依赖类型
 *
 * 编译方式：
 *   gcc -o type_system_demo type_system_demo.c -llv00 -lgmp
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv00.h"

/* ============================================================
 * 辅助函数：打印分隔线
 * ============================================================ */
static void print_separator(const char *title) {
    printf("\n--- %s ", title);
    for (int i = (int)strlen(title); i < 50; i++) {
        printf("-");
    }
    printf("\n");
}

/* ============================================================
 * 辅助函数：打印类型种类的中文名称
 * ============================================================ */
static const char *type_kind_cn(TypeKind kind) {
    switch (kind) {
        case TYPE_KIND_POINT:        return "点类型";
        case TYPE_KIND_LINE_SEGMENT: return "线段类型";
        case TYPE_KIND_REGION:       return "区域类型";
        case TYPE_KIND_FUNCTION:     return "函数类型";
        case TYPE_KIND_PRODUCT:      return "乘积类型";
        case TYPE_KIND_SUM:          return "和类型";
        case TYPE_KIND_VARIABLE:     return "类型变量";
        case TYPE_KIND_DEPENDENT:    return "依赖类型";
        case TYPE_KIND_BOTTOM:       return "底部类型 (⊥)";
        default:                     return "未知类型";
    }
}

/* ============================================================
 * 演示1：创建基本类型
 *
 * 创建点类型、线段类型和区域类型，这是 Lv-00 系统中
 * 最基础的几何类型。每个类型都属于某个宇宙层级。
 * ============================================================ */
static void demo_basic_types(TypeSystem *ts) {
    print_separator("基本类型创建");

    /* 创建点类型 —— 第0层宇宙（基本几何体） */
    TypeRegion *point_type = type_create_point(ts);
    if (point_type) {
        type_add_alias(point_type, "Point");
        printf("  [OK] 创建点类型 (Point)\n");
        printf("       种类: %s\n", type_kind_cn(point_type->kind));
        printf("       宇宙层级: %d\n", type_get_level(point_type));
    }

    /* 创建线段类型 */
    TypeRegion *segment_type = type_create_line_segment(ts);
    if (segment_type) {
        type_add_alias(segment_type, "Segment");
        printf("  [OK] 创建线段类型 (Segment)\n");
        printf("       种类: %s\n", type_kind_cn(segment_type->kind));
        printf("       宇宙层级: %d\n", type_get_level(segment_type));
    }

    /* 创建区域类型 —— 用包含的节点 ID 列表定义 */
    int region_nodes[] = {0, 1, 2};
    TypeRegion *region_type = type_create_region(ts, region_nodes, 3);
    if (region_type) {
        type_add_alias(region_type, "Region");
        printf("  [OK] 创建区域类型 (Region)\n");
        printf("       种类: %s\n", type_kind_cn(region_type->kind));
        printf("       包含节点数: %d\n", region_type->contained_count);
    }

    /* 创建底部类型 —— 表示矛盾的空类型 */
    TypeRegion *bottom_type = type_create_bottom(ts);
    if (bottom_type) {
        type_add_alias(bottom_type, "Bottom");
        printf("  [OK] 创建底部类型 (Bottom/⊥)\n");
        printf("       种类: %s\n", type_kind_cn(bottom_type->kind));
    }
}

/* ============================================================
 * 演示2：构建复合类型
 *
 * 展示如何从基本类型构建更复杂的类型表达式：
 * - 函数类型：Point -> Segment
 * - 乘积类型：Point x Point（表示点对）
 * - 和类型：Point + Segment（表示几何对象联合）
 * ============================================================ */
static void demo_composite_types(TypeSystem *ts) {
    print_separator("复合类型构建");

    /* 获取基本类型 */
    TypeRegion *point_type = type_create_point(ts);
    TypeRegion *segment_type = type_create_line_segment(ts);

    if (!point_type || !segment_type) {
        fprintf(stderr, "  [错误] 基本类型创建失败\n");
        return;
    }

    /* 构建函数类型: Point -> Point
     * 表示从点到点的映射（如平移变换） */
    TypeRegion *point_to_point = type_create_function(ts, point_type, point_type);
    if (point_to_point) {
        type_add_alias(point_to_point, "PointToPoint");
        printf("  [OK] 函数类型 Point -> Point\n");
        printf("       输入类型层级: %d, 输出类型层级: %d\n",
               type_get_level(point_to_point->input_type),
               type_get_level(point_to_point->output_type));
    }

    /* 构建高阶函数类型: Point -> (Point -> Point)
     * 表示接受一个点，返回一个点变换的函数 */
    TypeRegion *higher_order = type_create_function(ts, point_type, point_to_point);
    if (higher_order) {
        type_add_alias(higher_order, "PointToTransform");
        printf("  [OK] 高阶函数类型 Point -> (Point -> Point)\n");
        printf("       宇宙层级: %d\n", type_get_level(higher_order));
    }

    /* 构建乘积类型: Point x Point
     * 表示一对点（如线段的两个端点） */
    TypeRegion *point_pair = type_create_product(ts, point_type, point_type);
    if (point_pair) {
        type_add_alias(point_pair, "PointPair");
        printf("  [OK] 乘积类型 Point x Point\n");
        printf("       左类型: %s, 右类型: %s\n",
               type_kind_cn(point_pair->left_type->kind),
               type_kind_cn(point_pair->right_type->kind));
    }

    /* 构建和类型: Point + Segment
     * 表示一个几何对象，它要么是点，要么是线段 */
    TypeRegion *geom_union = type_create_sum(ts, point_type, segment_type);
    if (geom_union) {
        type_add_alias(geom_union, "GeomObject");
        printf("  [OK] 和类型 Point + Segment\n");
        printf("       第一类型: %s, 第二类型: %s\n",
               type_kind_cn(geom_union->first_type->kind),
               type_kind_cn(geom_union->second_type->kind));
    }
}

/* ============================================================
 * 演示3：类型等价检查
 *
 * 验证类型之间的等价关系，这是类型系统的核心功能。
 * 支持直接比较和基于重写引擎的规范化等价检查。
 * ============================================================ */
static void demo_type_equivalence(TypeSystem *ts) {
    print_separator("类型等价检查");

    /* 创建两个独立的点类型 */
    TypeRegion *point_a = type_create_point(ts);
    TypeRegion *point_b = type_create_point(ts);
    TypeRegion *segment = type_create_line_segment(ts);

    if (!point_a || !point_b || !segment) {
        fprintf(stderr, "  [错误] 类型创建失败\n");
        return;
    }

    type_add_alias(point_a, "PointA");
    type_add_alias(point_b, "PointB");
    type_add_alias(segment, "Segment");

    /* 检查1：两个点类型应该等价 */
    TypeEquivResult r1 = type_check_equivalence(ts, point_a, point_b, false);
    printf("  PointA ≡ PointB (直接比较): %s\n",
           type_equiv_result_to_string(r1));

    /* 检查2：使用重写引擎的规范化等价检查 */
    TypeEquivResult r2 = type_check_equivalence(ts, point_a, point_b, true);
    printf("  PointA ≡ PointB (规范化比较): %s\n",
           type_equiv_result_to_string(r2));

    /* 检查3：点类型和线段类型不应该等价 */
    TypeEquivResult r3 = type_check_equivalence(ts, point_a, segment, false);
    printf("  PointA ≡ Segment (直接比较): %s\n",
           type_equiv_result_to_string(r3));

    /* 检查4：函数类型的等价 —— (A->A) ≡ (A->A) */
    TypeRegion *func1 = type_create_function(ts, point_a, point_a);
    TypeRegion *func2 = type_create_function(ts, point_b, point_b);
    if (func1 && func2) {
        TypeEquivResult r4 = type_check_equivalence(ts, func1, func2, true);
        printf("  (PointA->PointA) ≡ (PointB->PointB): %s\n",
               type_equiv_result_to_string(r4));
    }

    /* 检查5：不同函数类型不等价 —— (A->A) ≠ (A->B) */
    TypeRegion *func3 = type_create_function(ts, point_a, segment);
    if (func1 && func3) {
        TypeEquivResult r5 = type_check_equivalence(ts, func1, func3, true);
        printf("  (PointA->PointA) ≡ (PointA->Segment): %s\n",
               type_equiv_result_to_string(r5));
    }
}

/* ============================================================
 * 演示4：宇宙层级与累积性
 *
 * 宇宙层级是类型理论中的核心概念：
 * - 第0层：基本几何体（点、线段）
 * - 第1层：类型区域
 * - 更高层级：类型的类型（Type : Type_1, Type_1 : Type_2, ...）
 *
 * 累积性意味着：如果 A : Type_i，则 A : Type_{i+1}
 * ============================================================ */
static void demo_universe_levels(TypeSystem *ts) {
    print_separator("宇宙层级与累积性");

    /* 创建不同层级的类型 */
    TypeRegion *point = type_create_point(ts);
    TypeRegion *segment = type_create_line_segment(ts);
    TypeRegion *func_type = type_create_function(ts, point, segment);

    if (!point || !segment || !func_type) {
        fprintf(stderr, "  [错误] 类型创建失败\n");
        return;
    }

    /* 打印各类型的宇宙层级 */
    printf("  Point 类型层级:    %d (%s)\n",
           type_get_level(point),
           universe_level_to_string(type_get_level(point)));
    printf("  Segment 类型层级:  %d (%s)\n",
           type_get_level(segment),
           universe_level_to_string(type_get_level(segment)));
    printf("  (Point->Segment) 类型层级: %d (%s)\n",
           type_get_level(func_type),
           universe_level_to_string(type_get_level(func_type)));

    /* 检查层级有效性 */
    bool valid = type_check_level_validity(ts, func_type, point);
    printf("\n  层级有效性检查 (函数类型包含点类型): %s\n",
           valid ? "有效" : "无效");

    /* 演示累积性检查 */
    printf("\n  累积性检查:\n");
    type_system_set_cumulative(ts, true);

    bool cum1 = type_check_cumulative(ts, point, segment);
    printf("    Point 累积到 Segment: %s\n", cum1 ? "是" : "否");

    bool cum2 = type_check_cumulative(ts, point, func_type);
    printf("    Point 累积到 (Point->Segment): %s\n", cum2 ? "是" : "否");
}

/* ============================================================
 * 演示5：类型变量与多态
 *
 * 展示类型变量的创建和实例化，这是多态类型系统的基础。
 * 类型变量允许定义参数化的类型，如 "forall a. a -> a"。
 * ============================================================ */
static void demo_type_variables(TypeSystem *ts) {
    print_separator("类型变量与多态");

    /* 创建类型变量 */
    TypeRegion *alpha = type_create_variable(ts, "alpha");
    if (alpha) {
        printf("  [OK] 创建类型变量 alpha\n");
        printf("       种类: %s\n", type_kind_cn(alpha->kind));
        printf("       变量名: %s\n", alpha->variable_name);
    }

    /* 创建类型变量 beta */
    TypeRegion *beta = type_create_variable(ts, "beta");
    if (beta) {
        printf("  [OK] 创建类型变量 beta\n");
    }

    /* 构建多态函数类型: alpha -> alpha（恒等函数类型） */
    if (alpha) {
        TypeRegion *identity_type = type_create_function(ts, alpha, alpha);
        if (identity_type) {
            type_add_alias(identity_type, "Identity");
            printf("  [OK] 多态恒等函数类型: alpha -> alpha\n");
            printf("       宇宙层级: %d\n", type_get_level(identity_type));
        }
    }

    /* 实例化类型变量：将 alpha 替换为具体的 Point 类型 */
    if (alpha) {
        TypeRegion *point = type_create_point(ts);
        if (point) {
            bool ok = type_instantiate_variable(ts, alpha->variable_id, point);
            printf("  实例化 alpha := Point: %s\n", ok ? "成功" : "失败");
        }
    }
}

/* ============================================================
 * 演示6：类型推断
 *
 * 展示基于规则表的自动类型推断。
 * 通过注册推断规则，系统可以自动从几何节点推断其类型。
 * ============================================================ */
static void demo_type_inference(TypeSystem *ts) {
    print_separator("类型推断");

    /* 注册自定义推断规则 */
    printf("  注册推断规则...\n");

    /* 规则1：GEOM_POINT 节点 -> TYPE_KIND_POINT 类型 */
    int r1 = type_system_register_inference_rule(
        ts, GEOM_POINT, TYPE_KIND_POINT, 10,
        "几何点节点推断为点类型");
    printf("    规则1 (点推断): %s\n", r1 == 0 ? "注册成功" : "注册失败");

    /* 规则2：GEOM_LINE_SEGMENT 节点 -> TYPE_KIND_LINE_SEGMENT 类型 */
    int r2 = type_system_register_inference_rule(
        ts, GEOM_LINE_SEGMENT, TYPE_KIND_LINE_SEGMENT, 10,
        "线段节点推断为线段类型");
    printf("    规则2 (线段推断): %s\n", r2 == 0 ? "注册成功" : "注册失败");

    /* 创建约束图用于类型推断 */
    printf("\n  创建约束图用于推断...\n");
    ConstraintGraph *g = graph_create();

    /* 添加一个点 */
    SymbolicCoord *cx = symbolic_coord_create_rational(1, 1);
    SymbolicCoord *cy = symbolic_coord_create_rational(2, 1);
    SymbolicCoord *coords[] = {cx, cy};
    graph_add_point(g, coords, 2);
    int point_id = g->next_node_id - 1;
    printf("    添加点节点 (ID=%d)\n", point_id);

    /* 添加一条线段 */
    graph_add_line_segment(g, point_id, point_id);
    int seg_id = g->next_node_id - 1;
    printf("    添加线段节点 (ID=%d)\n", seg_id);

    /* 使用规则表进行类型推断 */
    printf("\n  执行类型推断...\n");
    TypeEquivResult infer1 = type_infer_by_rules(ts, g, point_id);
    printf("    点节点推断结果: %s\n", type_equiv_result_to_string(infer1));

    TypeEquivResult infer2 = type_infer_by_rules(ts, g, seg_id);
    printf("    线段节点推断结果: %s\n", type_equiv_result_to_string(infer2));

    /* 查看推断后附加到节点的类型 */
    TypeRegion *inferred_type = type_get_node_type(ts, point_id);
    if (inferred_type) {
        printf("    点节点的推断类型: %s (层级=%d)\n",
               type_kind_cn(inferred_type->kind),
               type_get_level(inferred_type));
    } else {
        printf("    点节点无推断类型\n");
    }

    /* 打印所有已注册的推断规则 */
    int rule_count = 0;
    const TypeInferenceRule *rules = type_system_get_inference_rules(ts, &rule_count);
    printf("\n  已注册推断规则数: %d\n", rule_count);
    for (int i = 0; i < rule_count; i++) {
        printf("    [%d] 优先级=%d, 描述: %s\n",
               i, rules[i].priority, rules[i].description);
    }

    /* 清理 */
    graph_destroy(g);
}

/* ============================================================
 * 主函数
 * ============================================================ */
int main(void) {
    printf("========================================\n");
    printf("  Lv-00 类型系统演示\n");
    printf("  类型创建、检查与推断\n");
    printf("========================================\n");

    /* 初始化系统 */
    printf("\n[初始化] Lv-00 系统版本: %s\n", lv00_get_version_string());

    /* 创建类型系统 */
    printf("[初始化] 创建类型系统上下文...\n");
    TypeSystem *ts = type_system_create();
    if (!ts) {
        fprintf(stderr, "错误: 类型系统创建失败\n");
        return 1;
    }

    /* 启用良基模式和累积性 */
    type_system_set_well_founded(ts, true);
    type_system_set_cumulative(ts, true);
    printf("  良基模式: 已启用\n");
    printf("  累积性:   已启用\n");

    /* 依次运行各演示 */
    demo_basic_types(ts);
    demo_composite_types(ts);
    demo_type_equivalence(ts);
    demo_universe_levels(ts);
    demo_type_variables(ts);
    demo_type_inference(ts);

    /* 清理 */
    printf("\n[清理] 释放类型系统资源...\n");
    type_system_destroy(ts);

    printf("\n示例完成！\n");
    return 0;
}
