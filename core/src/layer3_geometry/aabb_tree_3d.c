/**
 * @file aabb_tree_3d.c
 * @brief 3D AABB 树构建与查询
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

#include "lv/lv_internal.h"
#include "lv/lv_utils.h"

/* ========================================================================
 * 模板实例化：3D AABB 树
 * ======================================================================== */

#define AABB_DIMS 3
#define AABB_PRIM_TYPE lvAABB3D
#define AABB_TREE_TYPE lvAABBTree3D
#define AABB_RAY_TYPE lvAABBRay3D

#define AABB_AXIS_CENTER(bb, axis) \
    ((axis) == 0 ? (bb).xmin + (bb).xmax : (axis) == 1 ? (bb).ymin + (bb).ymax : (bb).zmin + (bb).zmax)

#define AABB_AXIS_SWITCH(axis, body_x, body_y, body_z) \
    switch (axis) { case 0: { body_x } break; case 1: { body_y } break; default: { body_z } break; }

#define AABB_EMPTY() lv_aabb3d_empty()
#define AABB_EMPTY_PRIM lv_aabb3d_empty()
#define AABB_MERGE(a, b) lv_aabb3d_merge(a, b)

#define AABB_PREFIX 3d

/* 内部节点射线检测：直接使用 3D bbox */
#define AABB_NODE_RAY_INT(node, ray, tmin, tmax) \
    aabb3d_ray_intersect((node)->bbox, ray, tmin, tmax)

#define AABB_STATS 0
#define AABB_LEAF_MULTI 0

#include "aabb_tree_impl.h"