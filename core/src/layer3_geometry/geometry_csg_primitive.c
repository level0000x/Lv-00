/*
 * @file geometry_csg_primitive.c
 * @brief CSG geometry module - primitive creation and boolean ops
 * @details Split from geometry_csg.c
 */

#include "lv/lv_platform.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "geometry_types.h"
#include "geometry_csg_internal.h"
#include "lv_internal.h"
#include "lv_utils.h"

CSGNode *csg_sphere_create(double radius) {
    CSGNode *node = csg_node_create(CSG_NODE_PRIMITIVE);
    if (!node)
        return NULL;
    node->data.prim.type = 0;
    node->data.prim.params[0] = fabs(radius);
    csg_node_init_bbox(node);
    return node;
}

/**
 * @brief 创建立方体图元
 * @param w  宽度（X 轴方向）
 * @param h  高度（Y 轴方向）
 * @param d  深度（Z 轴方向）
 * @return 新 CSGNode（PRIMITIVE 类型，type=1）
 */
CSGNode *csg_cube_create(double w, double h, double d) {
    CSGNode *node = csg_node_create(CSG_NODE_PRIMITIVE);
    if (!node)
        return NULL;
    node->data.prim.type = 1;
    node->data.prim.params[0] = fabs(w);
    node->data.prim.params[1] = fabs(h);
    node->data.prim.params[2] = fabs(d);
    csg_node_init_bbox(node);
    return node;
}

/**
 * @brief 创建圆柱体图元
 * @param radius  圆柱底面半径
 * @param height  圆柱高度（Z 轴方向）
 * @return 新 CSGNode（PRIMITIVE 类型，type=2）
 */
CSGNode *csg_cylinder_create(double radius, double height) {
    CSGNode *node = csg_node_create(CSG_NODE_PRIMITIVE);
    if (!node)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "csg_cylinder_create: node allocation failed");
    node->data.prim.type = 2;
    node->data.prim.params[0] = fabs(radius);
    node->data.prim.params[1] = fabs(height);
    csg_node_init_bbox(node);
    return node;
}

/**
 * @brief 创建长方体/盒子图元
 * @param width   宽度（X 轴方向）
 * @param height  高度（Y 轴方向）
 * @param depth   深度（Z 轴方向）
 * @return 新 CSGNode（PRIMITIVE 类型，type=1）
 */
CSGNode *csg_box_create(double width, double height, double depth) {
    return csg_cube_create(width, height, depth);
}

/**
 * @brief 创建圆锥/圆台图元
 * @param radius1  下底面半径
 * @param radius2  上底面半径
 * @param height   高度（Z 轴方向）
 * @return 新 CSGNode（PRIMITIVE 类型，type=3）
 */
CSGNode *csg_cone_create(double radius1, double radius2, double height) {
    CSGNode *node = csg_node_create(CSG_NODE_PRIMITIVE);
    if (!node)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "csg_cone_create: node allocation failed");
    node->data.prim.type = 3;
    node->data.prim.params[0] = fabs(radius1);
    node->data.prim.params[1] = fabs(radius2);
    node->data.prim.params[2] = fabs(height);
    csg_node_init_bbox(node);
    return node;
}

/* ================================================================
 * CSG 布尔运算（返回新组合节点）
 * ================================================================ */

/**
 * @brief CSG 布尔并集 — a 与 b 的并集
 *
 * 创建一个类型为 CSG_NODE_UNION 的新节点，将 a 和 b 作为子节点。
 * 此阶段不进行实际 BSP 求值；调用 csg_evaluate() 进行几何评估。
 *
 * @param a  第一个操作数
 * @param b  第二个操作数
 * @return 新 CSGNode（UNION 类型）
 */
CSGNode *geometry_csg_union(CSGNode *a, CSGNode *b) {
    CSGNode *node = csg_node_create(CSG_NODE_UNION);
    if (!node)
        return NULL;
    csg_node_add_child(node, a);
    csg_node_add_child(node, b);
    csg_node_init_bbox(node);
    return node;
}

/**
 * @brief CSG 布尔差集 — a 减去 b
 *
 * 创建一个类型为 CSG_NODE_DIFFERENCE 的新节点，
 * a 是第一个子节点（被减数），b 是第二个子节点（减数）。
 *
 * @param a  第一个操作数（被减体）
 * @param b  第二个操作数（减体）
 * @return 新 CSGNode（DIFFERENCE 类型）
 */
CSGNode *geometry_csg_difference(CSGNode *a, CSGNode *b) {
    CSGNode *node = csg_node_create(CSG_NODE_DIFFERENCE);
    if (!node)
        return NULL;
    csg_node_add_child(node, a);
    csg_node_add_child(node, b);
    csg_node_init_bbox(node);
    return node;
}

/**
 * @brief CSG 布尔交集 — a 与 b 的交集
 *
 * 创建一个类型为 CSG_NODE_INTERSECTION 的新节点。
 *
 * @param a  第一个操作数
 * @param b  第二个操作数
 * @return 新 CSGNode（INTERSECTION 类型）
 */
CSGNode *geometry_csg_intersection(CSGNode *a, CSGNode *b) {
    CSGNode *node = csg_node_create(CSG_NODE_INTERSECTION);
    if (!node)
        return NULL;
    csg_node_add_child(node, a);
    csg_node_add_child(node, b);
    csg_node_init_bbox(node);
    return node;
}

/* ================================================================
 * 图元 → 三角形面生成
 * ================================================================ */

/**
 * @brief 对球体进行 UV 球面参数化，生成三角形面
 *
 * 使用 longitude × latitude 切分（stacks × slices）。
 * 默认 stacks=16, slices=32。
 */
