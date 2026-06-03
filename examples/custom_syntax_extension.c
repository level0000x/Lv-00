/**
 * @file custom_syntax_extension.c
 * @brief 自定义语法扩展示例
 *
 * @details 本示例展示如何在 Lv-00 中扩展自定义函数和语法。
 * 包含以下内容：
 *   1. 注册自定义几何函数
 *   2. 使用函数块模板
 *   3. 创建自定义 DSL 构造
 *   4. 批量操作示例
 *   5. 链式调用示例
 *
 * @version v3.5.0
 * @date 2026-05-29
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "lv00.h"
#include "dsl_compiler.h"
#include "func_block.h"
#include "error_messages_cn.h"

/* ============================================================
 *  示例 1：自定义几何函数 - 计算三角形的重心
 * ============================================================ */

/**
 * @brief 自定义函数：计算三角形重心
 *
 * 重心是三角形三条中线的交点，坐标为三个顶点坐标的平均值。
 */
static bool custom_centroid(ConstraintGraph *graph,
                             const int *input_nodes, int input_count,
                             int **output_nodes, int *output_count,
                             void *user_data) {
    (void)user_data; /* 未使用 */
    
    /* 验证输入 */
    if (input_count != 3) {
        printf("错误: centroid 函数需要 3 个输入点\n");
        return false;
    }
    
    /* 获取三个顶点的坐标 */
    /* 实际实现中需要从约束图中获取节点坐标 */
    printf("计算三角形重心...\n");
    printf("  顶点 A: 节点 %d\n", input_nodes[0]);
    printf("  顶点 B: 节点 %d\n", input_nodes[1]);
    printf("  顶点 C: 节点 %d\n", input_nodes[2]);
    
    /* 创建重心节点 */
    /* 实际实现中需要在约束图中创建新节点 */
    *output_count = 1;
    *output_nodes = (int *)malloc(sizeof(int));
    if (!*output_nodes) return false;
    
    /* 模拟创建新节点，实际 ID 应由约束图分配 */
    (*output_nodes)[0] = 1000; /* 示例 ID */
    
    printf("  重心节点 ID: %d\n", (*output_nodes)[0]);
    
    return true;
}

/**
 * @brief 注册重心计算函数
 */
static void register_centroid_function(void) {
    CustomFunctionRegistration reg = {
        .meta = {
            .name = "centroid",
            .description = "计算三角形的重心",
            .min_inputs = 3,
            .max_inputs = 3,
            .output_count = 1,
            .input_types = (const char *[]){"Point", "Point", "Point"},
            .output_types = (const char *[]){"Point"}
        },
        .callback = custom_centroid,
        .user_data = NULL,
        .free_user_data = NULL
    };
    
    if (func_block_register_custom(&reg)) {
        printf("成功注册自定义函数: centroid\n");
    } else {
        printf("注册自定义函数失败\n");
    }
}

/* ============================================================
 *  示例 2：自定义几何函数 - 计算外心
 * ============================================================ */

/**
 * @brief 自定义函数：计算三角形外心
 *
 * 外心是三角形三边垂直平分线的交点，也是外接圆的圆心。
 */
static bool custom_circumcenter(ConstraintGraph *graph,
                                 const int *input_nodes, int input_count,
                                 int **output_nodes, int *output_count,
                                 void *user_data) {
    (void)graph;
    (void)user_data;
    
    if (input_count != 3) {
        printf("错误: circumcenter 函数需要 3 个输入点\n");
        return false;
    }
    
    printf("计算三角形外心...\n");
    printf("  顶点 A: 节点 %d\n", input_nodes[0]);
    printf("  顶点 B: 节点 %d\n", input_nodes[1]);
    printf("  顶点 C: 节点 %d\n", input_nodes[2]);
    
    *output_count = 1;
    *output_nodes = (int *)malloc(sizeof(int));
    if (!*output_nodes) return false;
    
    (*output_nodes)[0] = 1001; /* 示例 ID */
    
    printf("  外心节点 ID: %d\n", (*output_nodes)[0]);
    
    return true;
}

/**
 * @brief 注册外心计算函数
 */
static void register_circumcenter_function(void) {
    CustomFunctionRegistration reg = {
        .meta = {
            .name = "circumcenter",
            .description = "计算三角形的外心",
            .min_inputs = 3,
            .max_inputs = 3,
            .output_count = 1,
            .input_types = (const char *[]){"Point", "Point", "Point"},
            .output_types = (const char *[]){"Point"}
        },
        .callback = custom_circumcenter,
        .user_data = NULL,
        .free_user_data = NULL
    };
    
    if (func_block_register_custom(&reg)) {
        printf("成功注册自定义函数: circumcenter\n");
    } else {
        printf("注册自定义函数失败\n");
    }
}

/* ============================================================
 *  示例 3：创建函数块模板
 * ============================================================ */

/**
 * @brief 创建等边三角形模板
 */
static void create_equilateral_triangle_template(void) {
    FuncBlockTemplate *tmpl = func_block_template_create(
        "equilateral_triangle",
        "创建给定边长的等边三角形"
    );
    
    if (!tmpl) {
        printf("创建模板失败\n");
        return;
    }
    
    /* 添加参数 */
    func_block_template_add_param(tmpl, "center", "Point", NULL);
    func_block_template_add_param(tmpl, "side_length", "Scalar", "1.0");
    func_block_template_add_param(tmpl, "rotation", "Scalar", "0.0");
    
    /* 设置 DSL 脚本 */
    const char *script = 
        "#version 3.5.0\n"
        "Point A, B, C;\n"
        "Let center : Point = $center;\n"
        "Let s : Scalar = $side_length;\n"
        "Let r : Scalar = $rotation;\n"
        "Constraint distance(A, B) = s;\n"
        "Constraint distance(B, C) = s;\n"
        "Constraint distance(C, A) = s;\n"
        "Constraint centroid(A, B, C) = center;\n";
    
    func_block_template_set_script(tmpl, script);
    
    /* 注册模板 */
    if (func_block_template_register(tmpl)) {
        printf("成功注册模板: equilateral_triangle\n");
    } else {
        printf("注册模板失败\n");
        func_block_template_destroy(tmpl);
    }
}

/* ============================================================
 *  示例 4：使用 DSL 编译器版本控制
 * ============================================================ */

/**
 * @brief 演示版本检测和转换
 */
static void demo_version_control(void) {
    printf("\n=== 版本控制示例 ===\n");
    
    /* 示例 1: 新版本语法 */
    const char *new_syntax = 
        "#version 3.5.0\n"
        "Point A, B;\n"
        "Let l : Line = line(A, B);\n";
    
    DslVersion version;
    if (dsl_version_extract(new_syntax, &version)) {
        printf("检测到版本声明: %d.%d.%d\n", 
               version.major, version.minor, version.patch);
    }
    
    /* 示例 2: 旧版本语法（需要转换） */
    const char *old_syntax = 
        "point A 0 0\n"
        "point B 1 0\n"
        "line l A B\n";
    
    DslVersion old_version = {3, 0, 0};
    DslVersion current = {3, 5, 0};
    
    char *transformed = NULL;
    if (dsl_syntax_transform(old_syntax, &old_version, &current, &transformed)) {
        printf("\n原始语法（v3.0.0）:\n%s\n", old_syntax);
        printf("转换后语法（v3.5.0）:\n%s\n", transformed);
        free(transformed);
    }
}

/* ============================================================
 *  示例 5：使用增强的错误报告
 * ============================================================ */

/**
 * @brief 演示错误报告
 */
static void demo_error_reporting(void) {
    printf("\n=== 错误报告示例 ===\n");
    
    /* 模拟有错误的输入 */
    const char *error_input = 
        "#version 3.5.0\n"
        "Point A B C\n"  /* 缺少分号 */
        "Constraint length(A, B) = length(A, C)\n";
    
    printf("解析输入:\n%s\n", error_input);
    
    /* 获取错误信息 */
    const Lv00ErrorMessage *err = lv00_get_error_message(2006); /* 缺少分号 */
    if (err) {
        printf("\n错误类型: %s\n", lv00_error_category_name_cn(err->category));
        printf("错误信息: %s\n", err->message);
        printf("修复建议: %s\n", err->suggestion);
    }
}

/* ============================================================
 *  示例 6：批量操作
 * ============================================================ */

/**
 * @brief 演示批量操作
 */
static void demo_batch_operations(void) {
    printf("\n=== 批量操作示例 ===\n");
    
    /* 模拟批量创建点 */
    printf("批量创建点:\n");
    double coords[][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
    const char *names[] = {"A", "B", "C", "D"};
    
    for (int i = 0; i < 4; i++) {
        printf("  创建点 %s: (%.1f, %.1f)\n", names[i], coords[i][0], coords[i][1]);
    }
    
    /* 模拟批量连接线段 */
    printf("\n批量连接线段（闭合）:\n");
    for (int i = 0; i < 4; i++) {
        int next = (i + 1) % 4;
        printf("  连接 %s -> %s\n", names[i], names[next]);
    }
}

/* ============================================================
 *  示例 7：链式调用
 * ============================================================ */

/**
 * @brief 演示链式调用
 */
static void demo_chaining(void) {
    printf("\n=== 链式调用示例 ===\n");
    
    printf("链式几何构造:\n");
    printf("  1. 创建点 A(0, 0)\n");
    printf("  2. 创建点 B(4, 0)\n");
    printf("  3. 创建线段 AB\n");
    printf("  4. 计算中点 M\n");
    printf("  5. 创建垂直线\n");
    printf("  6. 创建圆（圆心 M，半径 2）\n");
    
    printf("\n对应的 DSL 代码:\n");
    printf("  Point A, B;\n");
    printf("  Let AB : Segment = segment(A, B);\n");
    printf("  Let M : Point = midpoint(A, B);\n");
    printf("  Let perp : Line = perpendicular(AB, M);\n");
    printf("  Let c : Circle = circle(M, 2);\n");
}

/* ============================================================
 *  示例 8：完整的 DSL 代码生成
 * ============================================================ */

/**
 * @brief 演示完整的 DSL 代码生成
 */
static void demo_dsl_generation(void) {
    printf("\n=== DSL 代码生成示例 ===\n");
    
    printf("生成的 DSL 代码:\n\n");
    
    printf("#version 3.5.0\n\n");
    
    printf("/* 定义自定义函数 */\n");
    printf("Define midpoint(A: Point, B: Point) -> Point {\n");
    printf("    Let M : Point = point((A.x + B.x)/2, (A.y + B.y)/2);\n");
    printf("    return M;\n");
    printf("}\n\n");
    
    printf("/* 几何构造 */\n");
    printf("Point A, B, C;\n");
    printf("Constraint length(A, B) = length(A, C);\n");
    printf("Prove angle(A, B, C) = angle(B, C, A);\n\n");
    
    printf("/* 使用自定义函数 */\n");
    printf("Let M : Point = midpoint(B, C);\n");
    printf("Let H : Point = orthocenter(A, B, C);\n");
}

/* ============================================================
 *  主函数
 * ============================================================ */

int main(void) {
    printf("========================================\n");
    printf("Lv-00 自定义语法扩展示例 (v3.5.0)\n");
    printf("========================================\n\n");
    
    /* 初始化 */
    printf("初始化 Lv-00 系统...\n\n");
    
    /* 示例 1: 注册自定义函数 */
    printf("=== 注册自定义函数 ===\n");
    register_centroid_function();
    register_circumcenter_function();
    
    /* 示例 2: 创建模板 */
    printf("\n=== 创建函数块模板 ===\n");
    create_equilateral_triangle_template();
    
    /* 示例 3: 版本控制 */
    demo_version_control();
    
    /* 示例 4: 错误报告 */
    demo_error_reporting();
    
    /* 示例 5: 批量操作 */
    demo_batch_operations();
    
    /* 示例 6: 链式调用 */
    demo_chaining();
    
    /* 示例 7: DSL 生成 */
    demo_dsl_generation();
    
    /* 清理 */
    printf("\n========================================\n");
    printf("清理资源...\n");
    
    /* 注销自定义函数 */
    func_block_unregister_custom("centroid");
    func_block_unregister_custom("circumcenter");
    
    printf("示例完成!\n");
    
    return 0;
}
