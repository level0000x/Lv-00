/*
 * @file geometry_csg_bbox.c
 * @brief CSG geometry module - bounding box
 * @details Split from geometry_csg.c
 */

#include "lv/lv_platform.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/geometry_types.h"
#include "geometry_csg_internal.h"
#include "lv/lv_internal.h"
#include "lv/lv_utils.h"

/* --- 包围盒计算：按图元类型拆分的独立实现（供图元 vtable s_prim_ops 引用） --- */

void csg_bbox_sphere(CSGNode *node) {
    /* 球体：params = [radius] */
    double *p = node->data.prim.params;
    node->bbox_min[0] = node->bbox_min[1] = node->bbox_min[2] = -p[0];
    node->bbox_max[0] = node->bbox_max[1] = node->bbox_max[2] = p[0];
}

void csg_bbox_cube(CSGNode *node) {
    /* 立方体：params = [w, h, d] */
    double *p = node->data.prim.params;
    node->bbox_min[0] = -p[0] * 0.5;
    node->bbox_min[1] = -p[1] * 0.5;
    node->bbox_min[2] = -p[2] * 0.5;
    node->bbox_max[0] = p[0] * 0.5;
    node->bbox_max[1] = p[1] * 0.5;
    node->bbox_max[2] = p[2] * 0.5;
}

void csg_bbox_cylinder(CSGNode *node) {
    /* 圆柱体：params = [radius, height] */
    double *p = node->data.prim.params;
    node->bbox_min[0] = -p[0];
    node->bbox_min[1] = -p[0];
    node->bbox_min[2] = -p[1] * 0.5;
    node->bbox_max[0] = p[0];
    node->bbox_max[1] = p[0];
    node->bbox_max[2] = p[1] * 0.5;
}

void csg_bbox_cone(CSGNode *node) {
    /* 圆锥/圆台：params = [radius1, radius2, height]，取两底最大半径 */
    double *p = node->data.prim.params;
    double r = fmax(p[0], p[1]);
    node->bbox_min[0] = -r;
    node->bbox_min[1] = -r;
    node->bbox_min[2] = -p[2] * 0.5;
    node->bbox_max[0] = r;
    node->bbox_max[1] = r;
    node->bbox_max[2] = p[2] * 0.5;
}

static void bbox_primitive(CSGNode *node) {
    int ptype = node->data.prim.type;

    /* 图元类型 → 包围盒计算 统一走 vtable（见 s_prim_ops，定义于 geometry_csg_primitive.c） */
    if (ptype >= 0 && ptype < s_prim_ops_count && s_prim_ops[ptype].bbox) {
        s_prim_ops[ptype].bbox(node);
    }
}

/* --- 包围盒计算：函数指针表 --- */
typedef void (*CSGBBoxFunc)(CSGNode *node);

static void bbox_merge_children(CSGNode *node) {
    for (int i = 0; i < node->child_count; i++) {
        csg_node_init_bbox(node->children[i]);
    }
    if (node->child_count > 0) {
        CSGNode *first = node->children[0];
        node->bbox_min[0] = first->bbox_min[0];
        node->bbox_min[1] = first->bbox_min[1];
        node->bbox_min[2] = first->bbox_min[2];
        node->bbox_max[0] = first->bbox_max[0];
        node->bbox_max[1] = first->bbox_max[1];
        node->bbox_max[2] = first->bbox_max[2];
    }
    for (int i = 1; i < node->child_count; i++) {
        CSGNode *ch = node->children[i];
        if (ch->bbox_min[0] < node->bbox_min[0])
            node->bbox_min[0] = ch->bbox_min[0];
        if (ch->bbox_min[1] < node->bbox_min[1])
            node->bbox_min[1] = ch->bbox_min[1];
        if (ch->bbox_min[2] < node->bbox_min[2])
            node->bbox_min[2] = ch->bbox_min[2];
        if (ch->bbox_max[0] > node->bbox_max[0])
            node->bbox_max[0] = ch->bbox_max[0];
        if (ch->bbox_max[1] > node->bbox_max[1])
            node->bbox_max[1] = ch->bbox_max[1];
        if (ch->bbox_max[2] > node->bbox_max[2])
            node->bbox_max[2] = ch->bbox_max[2];
    }
}

static CSGBBoxFunc s_bbox_funcs[] = {
    [CSG_NODE_PRIMITIVE] = bbox_primitive,
    [CSG_NODE_UNION] = bbox_merge_children,
    [CSG_NODE_DIFFERENCE] = bbox_merge_children,
    [CSG_NODE_INTERSECTION] = bbox_merge_children,
    [CSG_NODE_HULL] = bbox_merge_children,
    [CSG_NODE_MINKOWSKI] = bbox_merge_children,
    [CSG_NODE_TRANSFORM] = bbox_merge_children,
    [CSG_NODE_EXTRUDE_LINEAR] = bbox_merge_children,
    [CSG_NODE_EXTRUDE_ROTATE] = bbox_merge_children,
};
static const int s_bbox_func_count = (int)(sizeof(s_bbox_funcs) / sizeof(s_bbox_funcs[0]));

void csg_node_init_bbox(CSGNode *node) {
    if (!node)
        return;
    if (node->kind >= 0 && node->kind < s_bbox_func_count && s_bbox_funcs[node->kind]) {
        s_bbox_funcs[node->kind](node);
    }
}

/* ================================================================
 * 基本图元创建
 * ================================================================ */

/**
 * @brief 创建球体图元
 * @param radius  球体半径
 * @return 新 CSGNode（PRIMITIVE 类型，type=0）
 */