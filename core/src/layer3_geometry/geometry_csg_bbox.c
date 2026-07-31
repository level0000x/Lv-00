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

#include "geometry_types.h"
#include "geometry_csg_internal.h"
#include "lv_internal.h"
#include "lv_utils.h"

/* --- 包围盒计算：函数指针表 --- */
typedef void (*CSGBBoxFunc)(CSGNode *node);

static void bbox_primitive(CSGNode *node) {
    int ptype = node->data.prim.type;
    double *p = node->data.prim.params;

    switch (ptype) {
        case 0: /* 球体：params = [radius] */
            node->bbox_min[0] = node->bbox_min[1] = node->bbox_min[2] = -p[0];
            node->bbox_max[0] = node->bbox_max[1] = node->bbox_max[2] = p[0];
            break;
        case 1: /* 立方体：params = [w, h, d] */
            node->bbox_min[0] = -p[0] * 0.5;
            node->bbox_min[1] = -p[1] * 0.5;
            node->bbox_min[2] = -p[2] * 0.5;
            node->bbox_max[0] = p[0] * 0.5;
            node->bbox_max[1] = p[1] * 0.5;
            node->bbox_max[2] = p[2] * 0.5;
            break;
        case 2: /* 圆柱体：params = [radius, height] */
            node->bbox_min[0] = -p[0];
            node->bbox_min[1] = -p[0];
            node->bbox_min[2] = -p[1] * 0.5;
            node->bbox_max[0] = p[0];
            node->bbox_max[1] = p[0];
            node->bbox_max[2] = p[1] * 0.5;
            break;
        default:
            break;
    }
}

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
