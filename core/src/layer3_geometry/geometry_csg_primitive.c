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
    node->data.prim.type = CSG_PRIM_SPHERE;
    node->data.prim.params[0] = fabs(radius);
    csg_node_init_bbox(node);
    return node;
}

/**
 * @brief 创建立方体图元
 * @param w  宽度（X 轴方向）
 * @param h  高度（Y 轴方向）
 * @param d  深度（Z 轴方向）
 * @return 新 CSGNode（PRIMITIVE 类型，CSG_PRIM_CUBE）
 */
CSGNode *csg_cube_create(double w, double h, double d) {
    CSGNode *node = csg_node_create(CSG_NODE_PRIMITIVE);
    if (!node)
        return NULL;
    node->data.prim.type = CSG_PRIM_CUBE;
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
 * @return 新 CSGNode（PRIMITIVE 类型，CSG_PRIM_CYLINDER）
 */
CSGNode *csg_cylinder_create(double radius, double height) {
    CSGNode *node = csg_node_create(CSG_NODE_PRIMITIVE);
    if (!node)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "csg_cylinder_create: node allocation failed");
    node->data.prim.type = CSG_PRIM_CYLINDER;
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
 * @return 新 CSGNode（PRIMITIVE 类型，CSG_PRIM_CUBE）
 */
CSGNode *csg_box_create(double width, double height, double depth) {
    return csg_cube_create(width, height, depth);
}

/**
 * @brief 创建圆锥/圆台图元
 * @param radius1  下底面半径
 * @param radius2  上底面半径
 * @param height   高度（Z 轴方向）
 * @return 新 CSGNode（PRIMITIVE 类型，CSG_PRIM_CONE）
 */
CSGNode *csg_cone_create(double radius1, double radius2, double height) {
    CSGNode *node = csg_node_create(CSG_NODE_PRIMITIVE);
    if (!node)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "csg_cone_create: node allocation failed");
    node->data.prim.type = CSG_PRIM_CONE;
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
 * @brief 生成圆锥/圆台的三角形面
 *
 * 与圆柱体网格生成共用同一骨架（geometry_csg_mesh.c 的
 * csg_gen_lathe_side_tris / csg_gen_disk_tris，声明于 geometry_csg_internal.h）：
 * 侧面按 strips × slices 分段三角带，半径沿高度从 radius1（z=-h/2）
 * 线性过渡到 radius2（z=+h/2），另加底面/顶面圆盘（对应半径大于零时
 * 生成，避免退化三角形）。
 */
void csg_gen_cone_tris(double radius1, double radius2, double height, CSGTriList *out) {
    if (height <= 0.0)
        return;

    int slices = 32;
    int strips = 8;
    double hh = height * 0.5;

    /* 侧面三角带：半径随 z 线性插值（与圆柱共用车削体侧带骨架） */
    for (int i = 0; i < strips; i++) {
        double z1 = -hh + height * (double) i / (double) strips;
        double z2 = -hh + height * (double) (i + 1) / (double) strips;
        double r1 = radius1 + (radius2 - radius1) * (z1 + hh) / height;
        double r2 = radius1 + (radius2 - radius1) * (z2 + hh) / height;
        csg_gen_lathe_side_tris(r1, r2, z1, z2, slices, out);
    }

    /* 底面圆盘（z=-hh，半径 radius1），绕序与圆柱底面一致（法线朝 -Z） */
    if (radius1 > CSG_BSP_EPSILON) {
        CSGVec3 center_bottom = {0.0, 0.0, -hh};
        csg_gen_disk_tris(center_bottom, radius1, -hh, slices, CSG_DISK_CW, out);
    }

    /* 顶面圆盘（z=+hh，半径 radius2），绕序与圆柱顶面一致（法线朝 +Z） */
    if (radius2 > CSG_BSP_EPSILON) {
        CSGVec3 center_top = {0.0, 0.0, hh};
        csg_gen_disk_tris(center_top, radius2, hh, slices, CSG_DISK_CCW, out);
    }
}

/* ================================================================
 * 图元统一分派表（hull / bbox / export 共用）
 * ================================================================ */

/* 图元 vtable 适配层：统一签名 (node, out)，解包 params 后调用 csg_gen_* 系列 */
static void prim_gen_sphere(const CSGNode *node, CSGTriList *out) {
    csg_gen_sphere_tris(node->data.prim.params[0], out);
}
static void prim_gen_cube(const CSGNode *node, CSGTriList *out) {
    csg_gen_cube_tris(node->data.prim.params[0], node->data.prim.params[1], node->data.prim.params[2], out);
}
static void prim_gen_cylinder(const CSGNode *node, CSGTriList *out) {
    csg_gen_cylinder_tris(node->data.prim.params[0], node->data.prim.params[1], out);
}
static void prim_gen_cone(const CSGNode *node, CSGTriList *out) {
    csg_gen_cone_tris(node->data.prim.params[0], node->data.prim.params[1], node->data.prim.params[2], out);
}

/* 图元 → 三角面生成 / 包围盒 / 导出名 统一分派表 */
const CSGPrimOps s_prim_ops[] = {
    { CSG_PRIM_SPHERE,   "sphere",   prim_gen_sphere,   csg_bbox_sphere,   "sphere",   1 },
    { CSG_PRIM_CUBE,     "cube",     prim_gen_cube,     csg_bbox_cube,     "cube",     3 },
    { CSG_PRIM_CYLINDER, "cylinder", prim_gen_cylinder, csg_bbox_cylinder, "cylinder", 2 },
    { CSG_PRIM_CONE,     "cone",     prim_gen_cone,     csg_bbox_cone,     "cylinder", 3 },
};
const int s_prim_ops_count = (int)(sizeof(s_prim_ops) / sizeof(s_prim_ops[0]));