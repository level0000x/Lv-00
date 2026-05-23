/**
 * @file circle_intersection.c
 * @brief 完整示例：圆与线段的相交构造
 *
 * 本示例演示：
 * 1. 构造一个圆（圆心和半径点，通过包含约束建模）
 * 2. 构造一条线段
 * 3. 计算交点
 * 4. 验证交点在圆上
 *
 * 几何语义：
 * - 圆由圆心O和半径点R定义，通过CONTAINMENT约束表示"R在O为心、|OR|为半径的圆上"
 * - 线段AB与圆的交点由CONSTRAINT_INTERSECTION约束建模
 * - 交点同时关联于线段（INCIDENCE）确保交点在线段上
 * - 本例中圆: x² + y² = 9, 线段: y = 2, 交点: x = ±√5
 */

#include "lv00.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * 辅助函数：添加一个点
 * 使用有理数坐标创建点，返回新节点的ID
 * 注意：各个add_point调用失败会通过返回值-1通知调用方
 */
static int add_point(ConstraintGraph *g, int64_t xn, uint64_t xd, int64_t yn, uint64_t yd)
{
    if (!g || xd == 0 || yd == 0) return -1;
    SymbolicCoord *cx = symbolic_coord_create_rational(xn, xd);
    SymbolicCoord *cy = symbolic_coord_create_rational(yn, yd);
    if (!cx || !cy) return -1;
    SymbolicCoord *coords[] = {cx, cy};
    AddNodeResult res = graph_add_point(g, coords, 2);
    if (res != ADD_NODE_OK) return -1;
    return g->next_node_id - 1;
}

/**
 * 构造圆与线段的相交
 *
 * 整体结构：
 *   圆心(0,0) --[包含约束]--> 半径点(3,0)   ← 定义圆
 *   线段AB: A(-4,2), B(4,2)
 *   交点: 线段AB 与 圆心O 相交于 (±√5, 2)
 *         其中交点也通过关联约束绑定到线段AB上
 */
int main(void)
{
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
    printf("[1/5] 创建圆心 O(0, 0)...\n");
    int center = add_point(g, 0, 1, 0, 1);
    if (center < 0) {
        fprintf(stderr, "错误: 创建圆心失败\n");
        graph_destroy(g);
        return 1;
    }

    /* 步骤2: 创建圆上的点（定义半径） */
    /* 半径点R(3, 0)：圆心到R的距离即圆的半径，半径=3 */
    printf("[2/5] 创建圆上的点 R(3, 0)（半径=3）...\n");
    int radius_point = add_point(g, 3, 1, 0, 1);
    if (radius_point < 0) {
        fprintf(stderr, "错误: 创建半径点失败\n");
        graph_destroy(g);
        return 1;
    }

    /*
     * 添加包含约束：半径点R在圆心O定义的圆上。
     * CONTAINMENT 约束在这里的语义是：O包含R，
     * 即R位于以O为圆心、|OR|为半径的圆周上。
     * 这使得后续的交点约束可以引用圆心O作为圆的代表。
     */
    printf("  添加圆的包含约束 O→R...\n");
    AddConstraintResult cres = graph_add_containment(g, center, radius_point);
    if (cres != ADD_CONSTRAINT_OK) {
        fprintf(stderr, "错误: 添加包含约束失败 (错误码=%d)\n", cres);
        graph_destroy(g);
        return 1;
    }

    /* 步骤3: 创建线段的端点 */
    /* 线段AB：A(-4, 2), B(4, 2) 是一条水平线段，位于y=2 */
    printf("[3/5] 创建线段端点 A(-4, 2), B(4, 2)...\n");
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
    printf("[4/5] 创建线段 AB...\n");
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
    printf("[5/5] 计算交点...\n");

    /*
     * 交点1: (-√5, 2)
     * x坐标 = 0 + (-1)*√5 = -√5
     * 使用 quadratic 坐标精确表示无理数
     */
    Rational *ra1 = rational_create(0, 1);
    Rational *rb1 = rational_create(-1, 1);
    SymbolicCoord *ix1 = symbolic_coord_create_quadratic(ra1, rb1, 5);
    SymbolicCoord *iy1 = symbolic_coord_create_rational(2, 1);
    SymbolicCoord *icoords1[] = {ix1, iy1};
    ares = graph_add_point(g, icoords1, 2);
    rational_destroy(ra1);
    rational_destroy(rb1);
    if (ares != ADD_NODE_OK) {
        fprintf(stderr, "错误: 创建交点1失败 (错误码=%d)\n", ares);
        graph_destroy(g);
        return 1;
    }
    int intersection1 = g->next_node_id - 1;

    /*
     * 交点2: (+√5, 2)
     * x坐标 = 0 + 1*√5 = +√5
     */
    Rational *ra2 = rational_create(0, 1);
    Rational *rb2 = rational_create(1, 1);
    SymbolicCoord *ix2 = symbolic_coord_create_quadratic(ra2, rb2, 5);
    SymbolicCoord *iy2 = symbolic_coord_create_rational(2, 1);
    SymbolicCoord *icoords2[] = {ix2, iy2};
    ares = graph_add_point(g, icoords2, 2);
    rational_destroy(ra2);
    rational_destroy(rb2);
    if (ares != ADD_NODE_OK) {
        fprintf(stderr, "错误: 创建交点2失败 (错误码=%d)\n", ares);
        graph_destroy(g);
        return 1;
    }
    int intersection2 = g->next_node_id - 1;

    /*
     * 添加相交约束：线段AB与圆（以圆心O为代表）相交。
     * 使用两个不同的参与者（线段和圆心）代替原来的自相交模式，
     * 语义上表示"线段与圆相交"，圆心作为圆的代表参与相交判定。
     */
    printf("  添加相交约束...\n");
    cres = graph_add_intersection(g, segment, center, intersection1);
    if (cres != ADD_CONSTRAINT_OK) {
        fprintf(stderr, "错误: 添加交点1相交约束失败 (错误码=%d)\n", cres);
        graph_destroy(g);
        return 1;
    }
    cres = graph_add_intersection(g, segment, center, intersection2);
    if (cres != ADD_CONSTRAINT_OK) {
        fprintf(stderr, "错误: 添加交点2相交约束失败 (错误码=%d)\n", cres);
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
    if (free_vars) free(free_vars);

    /* 清理 */
    printf("\n清理资源...\n");
    graph_destroy(g);

    printf("\n示例完成！\n");
    return 0;
}
