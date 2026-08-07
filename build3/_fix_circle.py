# -*- coding: utf-8 -*-
"""Fix test/examples/circle_intersection.c: use graph_add_circle + containment/incidence."""
import io
import sys

path = r"c:\Users\xingg\Desktop\知识体系化Wiki\Lv-00\test\examples\circle_intersection.c"

with io.open(path, "r", encoding="utf-8") as f:
    src = f.read()

edits = [
    # E1: file header comment
    (
        """ * 本示例演示：
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
 */""",
        """ * 本示例演示：
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
 */""",
    ),
    # E2: structure comment
    (
        """ * 整体结构：
 *   圆心(0,0) --[包含约束]--> 半径点(3,0)   ← 定义圆
 *   线段AB: A(-4,2), B(4,2)
 *   交点: 线段AB 与 圆心O 相交于 (±√5, 2)
 *         其中交点也通过关联约束绑定到线段AB上
 */""",
        """ * 整体结构：
 *   圆心O(0,0) + 半径点R(3,0) --[graph_add_circle]--> 圆节点 (半径=3)
 *   线段AB: A(-4,2), B(4,2)
 *   交点: 线段AB 与 圆 相交于 (±√5, 2)
 *         "交点在圆上"由 CONTAINMENT 约束（outer=圆节点）表达，
 *         "交点在线段上"由 INCIDENCE 约束表达
 */""",
    ),
    # E3: replace containment(center,radius_point) with graph_add_circle
    (
        """    /*
     * 添加包含约束：半径点R在圆心O定义的圆上。
     * CONTAINMENT 约束在这里的语义是：O包含R，
     * 即R位于以O为圆心、|OR|为半径的圆周上。
     * 这使得后续的交点约束可以引用圆心O作为圆的代表。
     */
    printf("  添加圆的包含约束 O→R...\\n");
    AddConstraintResult cres = graph_add_containment(g, center, radius_point);
    if (cres != ADD_CONSTRAINT_OK) {
        fprintf(stderr, "错误: 添加包含约束失败 (错误码=%d)\\n", cres);
        graph_destroy(g);
        return 1;
    }""",
        """    /*
     * 创建圆节点：以 center 为圆心、radius_point 为半径端点。
     * graph_add_circle 创建 GEOM_CIRCLE 节点，圆心到半径点的距离
     * 即为圆的半径（这里 |OR| = 3）。
     * 后续"交点在圆上"通过 CONTAINMENT 约束关联到该圆节点。
     */
    printf("[3/6] 创建圆（圆心 O, 半径点 R）...\\n");
    AddNodeResult cires = graph_add_circle(g, center, radius_point);
    if (cires != ADD_NODE_OK) {
        fprintf(stderr, "错误: 创建圆失败 (错误码=%d)\\n", cires);
        graph_destroy(g);
        return 1;
    }
    int circle = g->next_node_id - 1;""",
    ),
    # E4: step numbering
    (
        '[1/5] 创建圆心 O(0, 0)',
        '[1/6] 创建圆心 O(0, 0)',
    ),
    (
        '[2/5] 创建圆上的点 R(3, 0)（半径=3）',
        '[2/6] 创建圆上的点 R(3, 0)（半径=3）',
    ),
    (
        '[3/5] 创建线段端点 A(-4, 2), B(4, 2)',
        '[4/6] 创建线段端点 A(-4, 2), B(4, 2)',
    ),
    (
        '[4/5] 创建线段 AB',
        '[5/6] 创建线段 AB',
    ),
    (
        '[5/5] 计算交点',
        '[6/6] 计算交点',
    ),
    # E5: replace intersection(segment,center,intersectionN) with containment(intersectionN,circle)
    (
        """    /*
     * 添加相交约束：线段AB与圆（以圆心O为代表）相交。
     * 使用两个不同的参与者（线段和圆心）代替原来的自相交模式，
     * 语义上表示"线段与圆相交"，圆心作为圆的代表参与相交判定。
     */
    printf("  添加相交约束...\\n");
    cres = graph_add_intersection(g, segment, center, intersection1);
    if (cres != ADD_CONSTRAINT_OK) {
        fprintf(stderr, "错误: 添加交点1相交约束失败 (错误码=%d)\\n", cres);
        graph_destroy(g);
        return 1;
    }
    cres = graph_add_intersection(g, segment, center, intersection2);
    if (cres != ADD_CONSTRAINT_OK) {
        fprintf(stderr, "错误: 添加交点2相交约束失败 (错误码=%d)\\n", cres);
        graph_destroy(g);
        return 1;
    }""",
        """    /*
     * 添加"交点在圆上"约束：交点位于圆节点 circle 上。
     * graph_add_intersection 仅接受两条线段，无法直接表达"圆与线段相交"；
     * 因此用 CONTAINMENT 约束（outer 支持 GEOM_CIRCLE）表达交点属于圆，
     * 结合下方 INCIDENCE 约束（交点在线段上），组合表达
     * "圆与线段相交、交点在圆上且在线段上"。
     */
    printf("  添加圆上约束...\\n");
    AddConstraintResult cres = graph_add_containment(g, intersection1, circle);
    if (cres != ADD_CONSTRAINT_OK) {
        fprintf(stderr, "错误: 添加交点1圆上约束失败 (错误码=%d)\\n", cres);
        graph_destroy(g);
        return 1;
    }
    cres = graph_add_containment(g, intersection2, circle);
    if (cres != ADD_CONSTRAINT_OK) {
        fprintf(stderr, "错误: 添加交点2圆上约束失败 (错误码=%d)\\n", cres);
        graph_destroy(g);
        return 1;
    }""",
    ),
]

for i, (old, new) in enumerate(edits, 1):
    cnt = src.count(old)
    if cnt != 1:
        print("EDIT %d FAILED: found %d occurrences" % (i, cnt))
        sys.exit(1)
    src = src.replace(old, new)

with io.open(path, "w", encoding="utf-8", newline="") as f:
    f.write(src)

print("circle_intersection.c: all %d edits applied OK" % len(edits))
