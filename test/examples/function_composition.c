/**
 * @file function_composition.c
 * @brief 完整示例：函数块组合与类型系统
 *
 * 本示例演示：
 * 1. 创建基本几何构造函数块
 * 2. 使用组合子组合函数块
 * 3. 类型推断与检查
 * 4. 部分应用（柯里化）
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv00.h"

/**
 * 辅助函数：添加一个点
 */
static int add_point(ConstraintGraph *g, int64_t xn, uint64_t xd, int64_t yn, uint64_t yd) {
    SymbolicCoord *cx = symbolic_coord_create_rational(xn, xd);
    SymbolicCoord *cy = symbolic_coord_create_rational(yn, yd);
    SymbolicCoord *coords[] = {cx, cy};
    graph_add_point(g, coords, 2);
    return g->next_node_id - 1;
}

/**
 * 创建一个简单的"中点"函数块
 * 功能：给定两个输入点P1和P2，构造其中点M
 * 内部构造：P1--Seg--P2，且M在P1和P2之间
 * 端口：输入端口in1映射P1, in2映射P2; 输出端口out映射M
 */
static FuncBlock *create_midpoint_function(ConstraintGraph *g) {
    printf("  创建中点函数块...\n");

    /* 创建两个输入点 */
    int p1 = add_point(g, 0, 1, 0, 1);
    int p2 = add_point(g, 2, 1, 0, 1);

    /* 创建中点 */
    int mid = add_point(g, 1, 1, 0, 1);

    /* 创建线段 */
    graph_add_line_segment(g, p1, p2);
    int seg = g->next_node_id - 1;

    /* 添加之间约束：中点在p1和p2之间 */
    graph_add_betweenness(g, p1, p2, mid);

    /* 创建端口 */
    graph_add_port(g, PORT_INPUT, -1, -1);
    int in1 = g->next_node_id - 1;
    graph_add_port(g, PORT_INPUT, -1, -1);
    int in2 = g->next_node_id - 1;
    graph_add_port(g, PORT_OUTPUT, mid, -1);
    int out = g->next_node_id - 1;

    /* 打包 */
    int internal[] = {p1, p2, mid, seg};
    int inputs[] = {in1, in2};
    int outputs[] = {out};

    FuncBlock *fb = NULL;
    PackResult result = func_block_pack(g, internal, 4, inputs, 2, outputs, 1, NULL, 0, &fb);

    if (result == PACK_RESULT_OK && fb) {
        /*
         * 注意：lv00_strdup_safe 分配的内存在 func_block_destroy 中释放。
         * func_block_destroy 负责清理 fb->name 和 fb->description。
         */
        fb->name = lv00_strdup_safe("Midpoint");
        printf("  中点函数块创建成功 (ID=%d)\n", fb->id);
    } else {
        printf("  中点函数块创建失败\n");
    }

    return fb;
}

/**
 * 创建一个"距离"函数块
 * 功能：给定两个输入点P1和P2，构造连接两点的线段
 * 内部构造：P1和P2通过线段连接
 * 端口：输入端口in1映射P1, in2映射P2; 输出端口out代表线段
 */
static FuncBlock *create_distance_function(ConstraintGraph *g) {
    printf("  创建距离函数块...\n");

    /* 创建两个输入点 */
    int p1 = add_point(g, 0, 1, 0, 1);
    int p2 = add_point(g, 1, 1, 1, 1);

    /* 创建线段 */
    graph_add_line_segment(g, p1, p2);

    /* 创建端口 */
    graph_add_port(g, PORT_INPUT, -1, -1);
    int in1 = g->next_node_id - 1;
    graph_add_port(g, PORT_INPUT, -1, -1);
    int in2 = g->next_node_id - 1;
    graph_add_port(g, PORT_OUTPUT, -1, -1);
    int out = g->next_node_id - 1;

    /* 打包 */
    int internal[] = {p1, p2};
    int inputs[] = {in1, in2};
    int outputs[] = {out};

    FuncBlock *fb = NULL;
    PackResult result = func_block_pack(g, internal, 2, inputs, 2, outputs, 1, NULL, 0, &fb);

    if (result == PACK_RESULT_OK && fb) {
        /*
         * 注意：lv00_strdup_safe 分配的内存在 func_block_destroy 中释放。
         * func_block_destroy 负责清理 fb->name 和 fb->description。
         */
        fb->name = lv00_strdup_safe("Distance");
        printf("  距离函数块创建成功 (ID=%d)\n", fb->id);
    } else {
        printf("  距离函数块创建失败\n");
    }

    return fb;
}

/**
 * 演示类型系统
 * 包含：基本类型创建、函数类型构建、乘积类型、
 *       类型等价检查、宇宙层级、累积性检查
 */
static void demonstrate_type_system(void) {
    printf("\n[类型系统演示]\n");

    TypeSystem *ts = type_system_create();
    if (!ts) {
        fprintf(stderr, "错误: 类型系统创建失败\n");
        return;
    }

    /* 创建基本类型 */
    printf("  创建基本类型...\n");
    TypeRegion *point_type = type_create_point(ts);
    if (!point_type) {
        fprintf(stderr, "错误: Point类型创建失败\n");
        type_system_destroy(ts);
        return;
    }
    type_add_alias(point_type, "Point");

    TypeRegion *segment_type = type_create_line_segment(ts);
    if (!segment_type) {
        fprintf(stderr, "错误: Segment类型创建失败\n");
        type_system_destroy(ts);
        return;
    }
    type_add_alias(segment_type, "Segment");

    /* 创建函数类型: Point -> Point -> Point */
    printf("  创建函数类型: Point -> Point -> Point\n");
    TypeRegion *point_to_point = type_create_function(ts, point_type, point_type);
    if (!point_to_point) {
        fprintf(stderr, "错误: Point->Point函数类型创建失败\n");
        type_system_destroy(ts);
        return;
    }
    TypeRegion *point_to_point_to_point = type_create_function(ts, point_type, point_to_point);
    if (!point_to_point_to_point) {
        fprintf(stderr, "错误: Point->Point->Point函数类型创建失败\n");
        type_system_destroy(ts);
        return;
    }

    /* 创建乘积类型: Point x Point */
    printf("  创建乘积类型: Point × Point\n");
    TypeRegion *point_pair = type_create_product(ts, point_type, point_type);
    if (!point_pair) {
        fprintf(stderr, "错误: Point×Point乘积类型创建失败\n");
        type_system_destroy(ts);
        return;
    }

    /* 类型等价检查 */
    printf("\n  类型等价检查:\n");
    TypeRegion *point_type2 = type_create_point(ts);
    if (point_type2) {
        TypeEquivResult equiv = type_check_equivalence(ts, point_type, point_type2, false);
        printf("    Point ≡ Point: %s\n", equiv == TYPE_EQUIV_OK ? "是" : "否");

        equiv = type_check_equivalence(ts, point_type, segment_type, false);
        printf("    Point ≡ Segment: %s\n", equiv == TYPE_EQUIV_OK ? "是" : "否");
    } else {
        printf("    类型等价检查跳过（Point类型创建失败）\n");
    }

    /* 宇宙层级 */
    printf("\n  宇宙层级:\n");
    printf("    Point 层级: %d\n", type_get_level(point_type));
    printf("    Segment 层级: %d\n", type_get_level(segment_type));

    /* 累积性检查 */
    printf("\n  累积性检查:\n");
    bool cumulative = type_check_cumulative(ts, point_type, segment_type);
    printf("    Point 累积到 Segment: %s\n", cumulative ? "是" : "否");

    type_system_destroy(ts);
}

/**
 * 主函数
 *
 * 演示完整工作流程：
 * [1/4] 创建基本几何函数块（中点、距离）
 * [2/4] 函数块组合（函数复合、乘积）
 * [3/4] 部分应用（柯里化，固定第一个参数）
 * [4/4] 类型系统演示（类型创建、等价检查、宇宙层级）
 */
int main(void) {
    printf("========================================\n");
    printf("  Lv-00 函数块组合与类型系统示例\n");
    printf("========================================\n\n");

    ConstraintGraph *g = graph_create();

    /* 步骤1: 创建基本函数块
     * 中点块：给定两点，返回中点
     * 距离块：给定两点，返回连接线段
     */
    printf("[1/4] 创建基本几何函数块...\n");
    FuncBlock *midpoint_fb = create_midpoint_function(g);
    FuncBlock *distance_fb = create_distance_function(g);

    /* 步骤2: 演示函数块组合
     * 组合 = 函数复合：把中点块的输出作为距离块的输入
     * 乘积 = 并行的两个独立块
     */
    printf("\n[2/4] 函数块组合...\n");
    if (midpoint_fb && distance_fb) {
        /* 组合: Distance ∘ Midpoint
         * 先计算中点，再计算距离 */
        FuncBlock *composed = NULL;
        bool success = func_block_compose(distance_fb, midpoint_fb, g, &composed);
        printf("  组合 Distance ∘ Midpoint: %s\n", success ? "成功" : "失败");

        if (composed) {
            printf("  组合函数块输入数: %d\n", composed->input_count);
            printf("  组合函数块输出数: %d\n", composed->output_count);
            func_block_destroy(composed);
        }

        /* 乘积: Midpoint × Distance
         * 两个块并行独立运行 */
        FuncBlock *product = NULL;
        success = func_block_product(midpoint_fb, distance_fb, g, &product);
        printf("  乘积 Midpoint × Distance: %s\n", success ? "成功" : "失败");

        if (product) {
            printf("  乘积函数块输入数: %d\n", product->input_count);
            printf("  乘积函数块输出数: %d\n", product->output_count);
            func_block_destroy(product);
        }
    }

    /* 步骤3: 演示部分应用（柯里化）
     * 固定中点函数块的第一个输入参数，得到一个一元函数
     */
    printf("\n[3/4] 部分应用（柯里化）...\n");
    if (midpoint_fb) {
        /* 固定第一个参数为原点(0,0) */
        int fixed_arg = add_point(g, 0, 1, 0, 1);
        int fixed_mappings[] = {fixed_arg};

        FuncBlock *curried = NULL;
        bool success = func_block_partial_apply(midpoint_fb, g, fixed_mappings, 1, &curried);
        printf("  部分应用 Midpoint: %s\n", success ? "成功" : "失败");

        if (curried) {
            printf("  柯里化后输入数: %d (原: %d)\n", curried->input_count, midpoint_fb->input_count);
            func_block_destroy(curried);
        }
    }

    /* 步骤4: 类型系统演示 */
    printf("\n[4/4] 类型系统演示...\n");
    demonstrate_type_system();

    /* 清理 */
    printf("\n清理资源...\n");
    if (midpoint_fb)
        func_block_destroy(midpoint_fb);
    if (distance_fb)
        func_block_destroy(distance_fb);
    graph_destroy(g);

    printf("\n示例完成！\n");
    return 0;
}
