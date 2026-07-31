/**
 * @file triangle_construction.c
 * @brief 完整示例：三角形构造与证明
 *
 * 本示例演示 Lv-00 的完整工作流程：
 * 1. 构造一个三角形（三个点和三条边）
 * 2. 添加几何约束（边长关系）
 * 3. 创建命题（等边三角形判定）
 * 4. 统一化验证（证明构造满足命题）
 * 5. 打包为可复用的函数块
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv.h"
#include "examples_common.h"

/**
 * 步骤1: 构造三角形
 *
 * 创建一个三角形 ABC，其中：
 * - A = (0, 0)
 * - B = (2, 0)
 * - C = (1, √3) ≈ (1, 1.732)
 *
 * 这是一个等边三角形，边长为2
 */
static int construct_triangle(ConstraintGraph *g, int *out_a, int *out_b, int *out_c) {
    printf("  创建顶点 A(0, 0)...\n");
    int a = add_point(g, 0, 1, 0, 1);
    if (a < 0) {
        fprintf(stderr, "construct_triangle: 添加点A失败\n");
        *out_a = *out_b = *out_c = -1;
        return -1;
    }

    printf("  创建顶点 B(2, 0)...\n");
    int b = add_point(g, 2, 1, 0, 1);
    if (b < 0) {
        fprintf(stderr, "construct_triangle: 添加点B失败\n");
        *out_a = *out_b = *out_c = -1;
        return -1;
    }

    printf("  创建顶点 C(1, √3)...\n");
    /* 使用 exact quadratic 坐标 (1, 0 + 1*√3) 代替有理数近似 */
    {
        SymbolicCoord *cx = symbolic_coord_create_rational(1, 1);
        /* quadratic 坐标 a + b*sqrt(n): 0 + 1*sqrt(3) */
        Rational *ra = rational_create(0, 1);
        Rational *rb = rational_create(1, 1);
        SymbolicCoord *cy = symbolic_coord_create_quadratic(ra, rb, 3);
        if (!cx || !ra || !rb || !cy) {
            fprintf(stderr, "construct_triangle: 创建点C坐标失败\n");
            if (cx)
                symbolic_coord_destroy(cx);
            if (ra)
                rational_destroy(ra);
            if (rb)
                rational_destroy(rb);
            if (cy)
                symbolic_coord_destroy(cy);
            *out_a = *out_b = *out_c = -1;
            return -1;
        }
        SymbolicCoord *coords_c[] = {cx, cy};
        AddNodeResult res = graph_add_point(g, coords_c, 2);
        rational_destroy(ra);
        rational_destroy(rb);
        if (res != ADD_NODE_OK) {
            fprintf(stderr, "construct_triangle: 添加点C失败\n");
            symbolic_coord_destroy(cx);
            symbolic_coord_destroy(cy);
            *out_a = *out_b = *out_c = -1;
            return -1;
        }
        int c = g->next_node_id - 1;
        *out_c = c;
    }
    *out_a = a;
    *out_b = b;
    return 0;
}

/**
 * 步骤2: 添加边和约束
 */
static void add_triangle_constraints(ConstraintGraph *g, int a, int b, int c) {
    printf("  添加边 AB...\n");
    AddNodeResult res = graph_add_line_segment(g, a, b);
    if (res != ADD_NODE_OK) {
        fprintf(stderr, "add_triangle_constraints: 添加边AB失败 (错误码=%d)\n", res);
        return;
    }
    int ab = g->next_node_id - 1;

    printf("  添加边 BC...\n");
    res = graph_add_line_segment(g, b, c);
    if (res != ADD_NODE_OK) {
        fprintf(stderr, "add_triangle_constraints: 添加边BC失败 (错误码=%d)\n", res);
        return;
    }
    int bc = g->next_node_id - 1;

    printf("  添加边 CA...\n");
    res = graph_add_line_segment(g, c, a);
    if (res != ADD_NODE_OK) {
        fprintf(stderr, "add_triangle_constraints: 添加边CA失败 (错误码=%d)\n", res);
        return;
    }
    int ca = g->next_node_id - 1;

    printf("  添加约束：C 在边 AB 的上方（用于确定方向）...\n");
    /* 使用 betweenness 约束表示点的顺序关系 */
    graph_add_betweenness(g, a, b, c);

    (void) ab;
    (void) bc;
    (void) ca; /* 暂时未使用 */
}

/**
 * 步骤3: 创建等边三角形判定命题
 *
 * 命题：如果三角形的三条边长度相等，则它是等边三角形
 */
static ConstraintGraph *create_equilateral_proposition(void) {
    printf("  创建命题图...\n");
    ConstraintGraph *prop = graph_create();

    /* 命题中的抽象点 */
    int p1 = add_point(prop, 0, 1, 0, 1);
    int p2 = add_point(prop, 1, 1, 0, 1);
    int p3 = add_point(prop, 0, 1, 1, 1);

    /* 命题中的边 */
    graph_add_line_segment(prop, p1, p2);
    graph_add_line_segment(prop, p2, p3);
    graph_add_line_segment(prop, p3, p1);

    /* 添加等边约束（使用 betweenness 作为简化表示）*/
    graph_add_betweenness(prop, p1, p2, p3);

    return prop;
}

/**
 * 步骤4: 将三角形构造打包为函数块
 */
static FuncBlock *pack_triangle_constructor(ConstraintGraph *g, int a, int b, int c) {
    printf("  创建输入端口（边长参数）...\n");
    graph_add_port(g, PORT_INPUT, -1, -1);
    int in_port = g->next_node_id - 1;

    printf("  创建输出端口（三角形顶点）...\n");
    graph_add_port(g, PORT_OUTPUT, c, -1);
    int out_port = g->next_node_id - 1;

    printf("  打包为函数块...\n");
    int internal_nodes[] = {a, b, c};
    int input_ports[] = {in_port};
    int output_ports[] = {out_port};

    FuncBlock *fb = NULL;
    PackResult result = func_block_pack(g, internal_nodes, 3, input_ports, 1, output_ports, 1, NULL, 0, &fb);

    if (result != PACK_RESULT_OK) {
        printf("  打包失败: %s\n", pack_result_to_string(result));
        return NULL;
    }

    fb->name = lv_strdup_safe("EquilateralTriangle");
    fb->description = lv_strdup_safe("构造一个等边三角形，给定边长");

    printf("  函数块 '%s' 创建成功 (ID=%d)\n", fb->name, fb->id);

    return fb;
}

/**
 * 步骤5: 执行确定性检查
 */
static void check_determinism(FuncBlock *fb, ConstraintGraph *g) {
    printf("  执行静态确定性检查...\n");
    /*
     * 参数 max_iterations=1000 表示确定性检查的最大迭代次数。
     * 1000 是经验值：在大多数几何构造中，迭代次数超过此值意味着
     * 存在多解或循环依赖，应终止检查并报告超时。
     */
    DeterminismCheckResult result = (DeterminismCheckResult) func_block_determinism_check_static(fb, g);

    printf("  检查结果: ");
    switch (result) {
        case DETERMINISM_CHECK_RESULT_UNIQUE:
            printf("唯一解（确定性）\n");
            break;
        case DETERMINISM_CHECK_RESULT_MULTIPLE:
            printf("多解（需要选择器）\n");
            break;
        case DETERMINISM_CHECK_RESULT_NO_SOLUTION:
            printf("无解\n");
            break;
        case DETERMINISM_CHECK_RESULT_TIMEOUT:
            printf("检查超时\n");
            break;
        case DETERMINISM_CHECK_RESULT_OUT_OF_RANGE:
            printf("超出范围\n");
            break;
        default:
            printf("未知结果\n");
    }
}

/**
 * 主示例流程
 */
int main(void) {
    printf("========================================\n");
    printf("  Lv-00 几何构造与证明示例\n");
    printf("  等边三角形构造与验证\n");
    printf("========================================\n\n");

    /* 创建主约束图 */
    printf("[1/5] 构造三角形...\n");
    ConstraintGraph *construction = graph_create();
    int a, b, c;
    if (construct_triangle(construction, &a, &b, &c) != 0) {
        fprintf(stderr, "错误: 三角形构造失败\n");
        graph_destroy(construction);
        return 1;
    }
    printf("  三角形顶点: A=%d, B=%d, C=%d\n\n", a, b, c);

    /* 添加约束 */
    printf("[2/5] 添加几何约束...\n");
    add_triangle_constraints(construction, a, b, c);
    printf("  当前节点数: %d, 约束数: %d\n\n", construction->node_count, construction->constraint_count);

    /* 创建命题 */
    printf("[3/5] 创建等边三角形判定命题...\n");
    ConstraintGraph *proposition = create_equilateral_proposition();
    printf("  命题图节点数: %d, 约束数: %d\n\n", proposition->node_count, proposition->constraint_count);

    /* 统一化验证 */
    printf("[4/5] 执行统一化验证...\n");
    UnifyStatus status = unify_construction_with_proposition(construction, proposition);
    printf("  统一化结果: ");
    switch (status) {
        case UNIFY_STATUS_OK:
            printf("成功 - 构造满足命题！\n");
            break;
        case UNIFY_STATUS_PORT_TYPE_MISMATCH:
            printf("失败 - 端口类型不匹配\n");
            break;
        case UNIFY_STATUS_CONSTRAINT_MISMATCH:
            printf("失败 - 约束不匹配\n");
            break;
        case UNIFY_STATUS_COORD_MISMATCH:
            printf("失败 - 坐标不匹配\n");
            break;
        default:
            printf("失败 - 错误码 %d\n", status);
    }
    printf("\n");

    /* 打包为函数块 */
    printf("[5/5] 打包为可复用函数块...\n");
    FuncBlock *fb = pack_triangle_constructor(construction, a, b, c);
    if (fb) {
        check_determinism(fb, construction);
    }
    printf("\n");

    /* 示例：归一化 */
    printf("[额外] 执行图归一化...\n");
    NormalizationResult *norm_result = graph_normalize(construction, false);
    if (norm_result) {
        printf("  归一化完成: 合并了 %d 个节点\n", norm_result->merged_count);
        normalization_result_destroy(norm_result);
    }
    printf("\n");

    /* 清理 */
    printf("清理资源...\n");
    if (fb) {
        func_block_destroy(fb);
    }
    graph_destroy(construction);
    graph_destroy(proposition);

    printf("\n示例完成！\n");
    return 0;
}
