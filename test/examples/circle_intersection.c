/**
 * @file circle_intersection.c
 * @brief 完整示例：圆与线段的相交构造
 *
 * 本示例演示：
 * 1. 构造一个圆（圆心和半径点，通过 graph_add_circle 建模）
 * 2. 构造一条线段
 * 3. 计算交点
 * 4. 验证交点在圆上且在线段上
 *
 * 几何语义：
 * - 圆由圆心O和半径点R定义（graph_add_circle 创建 GEOM_CIRCLE 节点，
 *   圆心到半径点的距离即半径）
 * - "交点在圆上"由 CONTAINMENT 约束表达（其 outer 参与者支持 GEOM_CIRCLE）
 * - "交点在线段上"由 INCIDENCE 约束表达
 * - 约束图 API 的 INTERSECTION 仅接受两条线段，无法直接表达圆-线段相交，
 *   因此用 CONTAINMENT(交点,圆) + INCIDENCE(交点,线段) 组合表达
 * - 本例中圆: x² + y² = 9, 线段: y = 2, 交点: x = ±√5
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv.h"
#include "examples_common.h"

/**
 * 构造圆与线段的相交
 *
 * 整体结构：
 *   圆心O(0,0) + 半径点R(3,0) --[graph_add_circle]--> 圆节点 (半径=3)
 *   线段AB: A(-4,2), B(4,2)
 *   交点: 线段AB 与 圆 相交于 (±√5, 2)
 *         "交点在圆上"由 CONTAINMENT 约束（outer=圆节点）表达，
 *         "交点在线段上"由 INCIDENCE 约束表达
 */
int main(void) {
    printf("========================================\n");
    printf("  Lv-00 圆与线段相交示例\n");
    printf("========================================\n\n");

    ConstraintGraph *g = graph_create();
    if (!g) {
        fprintf(stderr, "错误: 约束图创建失败\n");
        return 1;
    }

    /* 步骤1: 创建圆心 */
    /* 圆心O：圆的几何中心，位于(0, 0) */
    printf("[1/6] 创建圆心 O(0, 0)...\n");
    int center = add_point(g, 0, 1, 0, 1);
    if (center < 0) {
        fprintf(stderr, "错误: 创建圆心失败\n");
        graph_destroy(g);
        return 1;
    }

    /* 步骤2: 创建圆上的点（定义半径） */
    /* 半径点R(3, 0)：圆心到R的距离即圆的半径，半径=3 */
    printf("[2/6] 创建圆上的点 R(3, 0)（半径=3）...\n");
    int radius_point = add_point(g, 3, 1, 0, 1);
    if (radius_point < 0) {
        fprintf(stderr, "错误: 创建半径点失败\n");
        graph_destroy(g);
        return 1;
    }

    /*
     * 创建圆节点：以 center 为圆心、radius_point 为半径端点。
     * graph_add_circle 创建 GEOM_CIRCLE 节点，圆心到半径点的距离
     * 即为圆的半径（这里 |OR| = 3）。
     * 后续"交点在圆上"通过 CONTAINMENT 约束关联到该圆节点。
     */
    printf("[3/6] 创建圆（圆心 O, 半径点 R）...\n");
    AddNodeResult cires = graph_add_circle(g, center, radius_point);
    if (cires != ADD_NODE_OK) {
        fprintf(stderr, "错误: 创建圆失败 (错误码=%d)\n", cires);
        graph_destroy(g);
        return 1;
    }
    int circle = g->next_node_id - 1;

    /* 步骤3: 创建线段的端点 */
    /* 线段AB：A(-4, 2), B(4, 2) 是一条水平线段，位于y=2 */
    printf("[4/6] 创建线段端点 A(-4, 2), B(4, 2)...\n");
    int a = add_point(g, -4, 1, 2, 1);
    if (a < 0) {
        fprintf(stderr, "错误: 创建端点A失败\n");
        graph_destroy(g);
        return 1;
    }
    int b = add_point(g, 4, 1, 2, 1);
    if (b < 0) {
        fprintf(stderr, "错误: 创建端点B失败\n");
        graph_destroy(g);
        return 1;
    }

    /* 步骤4: 创建线段 */
    /* 线段AB由两个端点A和B定义 */
    printf("[5/6] 创建线段 AB...\n");
    AddNodeResult ares = graph_add_line_segment(g, a, b);
    if (ares != ADD_NODE_OK) {
        fprintf(stderr, "错误: 创建线段AB失败 (错误码=%d)\n", ares);
        graph_destroy(g);
        return 1;
    }
    int segment = g->next_node_id - 1;

    /* 步骤5: 创建交点 */
    /*
     * 理论交点: 圆 x² + y² = 9 与直线 y = 2 的交点。
     * 代入得 x² + 4 = 9 → x = ±√5。
     * √5 使用 exact quadratic 坐标 (0 + 1*√5) 表示，而非有理数近似值。
     */
    printf("[6/6] 计算交点...\n");

    /*
     * 交点1: (-√5, 2)
     * x坐标 = 0 + (-1)*√5 = -√5
     * 使用 quadratic 坐标精确表示无理数
     */
    Rational *ra1 = rational_create(0, 1);
    Rational *rb1 = rational_create(-1, 1);
    SymbolicCoord *ix1 = symbolic_coord_create_quadratic(ra1, rb1, 5);
    SymbolicCoord *iy1 = symbolic_coord_create_rational(2, 1);
    if (!ra1 || !rb1 || !ix1 || !iy1) {
        fprintf(stderr, "错误: 创建交点1坐标失败\n");
        if (ra1)
            rational_destroy(ra1);
        if (rb1)
            rational_destroy(rb1);
        if (ix1)
            symbolic_coord_destroy(ix1);
        if (iy1)
            symbolic_coord_destroy(iy1);
        graph_destroy(g);
        return 1;
    }
    SymbolicCoord *icoords1[] = {ix1, iy1};
    ares = graph_add_point(g, icoords1, 2);
    rational_destroy(ra1);
    rational_destroy(rb1);
    if (ares != ADD_NODE_OK) {
        fprintf(stderr, "错误: 创建交点1失败 (错误码=%d)\n", ares);
        symbolic_coord_destroy(ix1);
        symbolic_coord_destroy(iy1);
        graph_destroy(g);
        return 1;
    }
    /* graph_add_point 接管 SymbolicCoord 所有权，无需手动释放 ix1/iy1 */
    int intersection1 = g->next_node_id - 1;

    /*
     * 交点2: (+√5, 2)
     * x坐标 = 0 + 1*√5 = +√5
     */
    Rational *ra2 = rational_create(0, 1);
    Rational *rb2 = rational_create(1, 1);
    SymbolicCoord *ix2 = symbolic_coord_create_quadratic(ra2, rb2, 5);
    SymbolicCoord *iy2 = symbolic_coord_create_rational(2, 1);
    if (!ra2 || !rb2 || !ix2 || !iy2) {
        fprintf(stderr, "错误: 创建交点2坐标失败\n");
        if (ra2)
            rational_destroy(ra2);
        if (rb2)
            rational_destroy(rb2);
        if (ix2)
            symbolic_coord_destroy(ix2);
        if (iy2)
            symbolic_coord_destroy(iy2);
        graph_destroy(g);
        return 1;
    }
    SymbolicCoord *icoords2[] = {ix2, iy2};
    ares = graph_add_point(g, icoords2, 2);
    rational_destroy(ra2);
    rational_destroy(rb2);
    if (ares != ADD_NODE_OK) {
        fprintf(stderr, "错误: 创建交点2失败 (错误码=%d)\n", ares);
        symbolic_coord_destroy(ix2);
        symbolic_coord_destroy(iy2);
        graph_destroy(g);
        return 1;
    }
    /* graph_add_point 接管 SymbolicCoord 所有权，无需手动释放 ix2/iy2 */
    int intersection2 = g->next_node_id - 1;

    /*
     * 添加"交点在圆上"约束：交点位于圆节点 circle 上。
     * graph_add_intersection 仅接受两条线段，无法直接表达"圆与线段相交"；
     * 因此用 CONTAINMENT 约束（outer 支持 GEOM_CIRCLE）表达交点属于圆，
     * 结合下方 INCIDENCE 约束（交点在线段上），组合表达
     * "圆与线段相交、交点在圆上且在线段上"。
     */
    printf("  添加圆上约束...\n");
    AddConstraintResult cres = graph_add_containment(g, intersection1, circle);
    if (cres != ADD_CONSTRAINT_OK) {
        fprintf(stderr, "错误: 添加交点1圆上约束失败 (错误码=%d)\n", cres);
        graph_destroy(g);
        return 1;
    }
    cres = graph_add_containment(g, intersection2, circle);
    if (cres != ADD_CONSTRAINT_OK) {
        fprintf(stderr, "错误: 添加交点2圆上约束失败 (错误码=%d)\n", cres);
        graph_destroy(g);
        return 1;
    }

    /*
     * 添加关联约束：交点必须在线段AB上。
     * INCIDENCE 约束确保交点属于线段，这是线段与圆相交的必要条件。
     */
    printf("  添加关联约束...\n");
    cres = graph_add_incidence(g, intersection1, segment);
    if (cres != ADD_CONSTRAINT_OK) {
        fprintf(stderr, "错误: 添加交点1关联约束失败 (错误码=%d)\n", cres);
        graph_destroy(g);
        return 1;
    }
    cres = graph_add_incidence(g, intersection2, segment);
    if (cres != ADD_CONSTRAINT_OK) {
        fprintf(stderr, "错误: 添加交点2关联约束失败 (错误码=%d)\n", cres);
        graph_destroy(g);
        return 1;
    }

    printf("\n构造完成！\n");
    printf("  节点数: %d\n", g->node_count);
    printf("  约束数: %d\n", g->constraint_count);

    /* 归一化 */
    printf("\n[验证] 执行归一化...\n");
    NormalizationResult *norm = graph_normalize(g, false);
    if (norm) {
        printf("  合并了 %d 个节点\n", norm->merged_count);
        normalization_result_destroy(norm);
    } else {
        printf("  归一化失败或无需合并\n");
    }

    /* 确定性检查 */
    printf("\n[验证] 计算自由度...\n");
    int *free_vars = NULL;
    int dof = count_degrees_of_freedom(g, &free_vars);
    printf("  自由度: %d\n", dof);
    if (free_vars)
        lv_free(&free_vars); /* count_degrees_of_freedom 经 lv_calloc 分配，须用 lv_free 释放 */

    /* 清理 */
    printf("\n清理资源...\n");
    graph_destroy(g);

    printf("\n示例完成！\n");
    return 0;
}
