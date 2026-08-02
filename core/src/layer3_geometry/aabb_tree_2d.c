/**
 * @file aabb_tree_2d.c
 * @brief 2D AABB 树构建与查询
 *
 * 从 geo_aabb_tree.c 拆分的模块之一：
 *   - aabb_box.c     包围盒基础操作
 *   - aabb_common.c  查询结果管理与内部公共工具
 *   - aabb_tree_2d.c 2D AABB 树构建与查询
 *   - aabb_tree_3d.c 3D AABB 树构建与查询
 *
 * 实现由 aabb_tree_impl.h 模板生成。
 *
 * @version v3.6.0
 */

#include "lv/geo_aabb_tree.h"
#include "aabb_internal.h"

#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "lv_internal.h"
#include "lv_utils.h"

/* ========================================================================
 * 模板实例化：2D AABB 树
 * ======================================================================== */

#define AABB_DIMS 2
#define AABB_PRIM_TYPE lvAABB2D
#define AABB_TREE_TYPE lvAABBTree2D
#define AABB_RAY_TYPE lvAABBRay2D

#define AABB_AXIS_CENTER(bb, axis) ((axis) == 0 ? (bb).xmin + (bb).xmax : (bb).ymin + (bb).ymax)

#define AABB_AXIS_SWITCH(axis, body_x, body_y, body_z) \
    if ((axis) == 0) { body_x } else { body_y }

#define AABB_EMPTY() lv_aabb3d_empty()
#define AABB_EMPTY_PRIM lv_aabb2d_empty()
#define AABB_MERGE(a, b) lv_aabb3d_merge(a, b)

#define AABB_PREFIX 2d

/* 内部节点射线检测：将 3D bbox 转为 2D 后检测 */
#define AABB_NODE_RAY_INT(node, ray, tmin, tmax) \
    aabb2d_ray_intersect( \
        (lvAABB2D){(node)->bbox.xmin, (node)->bbox.ymin, (node)->bbox.xmax, (node)->bbox.ymax}, \
        ray, tmin, tmax)

#define AABB_STATS 1
#define AABB_LEAF_MULTI 1

#include "aabb_tree_impl.h"