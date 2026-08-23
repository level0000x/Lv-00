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

#include "lv/lv_internal.h"
#include "lv/lv_utils.h"

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

#define AABB_EMPTY() lv_aabb3d_empty()  /* 有意为之: aabb_tree_impl.h 模板内 node_bbox 为 lvAABB3D, 2D 树内部以 z=0 提升为 3D 表示 */
#define AABB_EMPTY_PRIM lv_aabb2d_empty()
#define AABB_MERGE(a, b) lv_aabb3d_merge(a, b)  /* 同上: 模板内 AABB_MERGE 接收 lvAABB3D */

#define AABB_PREFIX 2d

/* 内部节点射线检测：将 3D bbox 转为 2D 后检测 */
#define AABB_NODE_RAY_INT(node, ray, tmin, tmax) \
    aabb2d_ray_intersect( \
        (lvAABB2D){(node)->bbox.xmin, (node)->bbox.ymin, (node)->bbox.xmax, (node)->bbox.ymax}, \
        ray, tmin, tmax)

#define AABB_STATS 1
#define AABB_LEAF_MULTI 1

#include "aabb_tree_impl.h"

/* ============================================================
 * 旧版兼容 API（C-㊺续33 补齐：头声明无实现，调用即链接错误）
 *
 * lv_aabb_tree_build / lv_aabb_tree_query 为无维度后缀的旧版接口：
 *   - build：从点数组构建树（dim 2/3，点坐标按维度交错）；
 *     dim 2 走 lv_aabb2d_build，dim 3 走 lv_aabb3d_build。
 *   - query：按 2D 包围盒查询命中点，输出命中点的坐标（x,y 交错）；
 *     返回输出的坐标值数量（= 命中点数 * 2）。
 * ============================================================ */

#include <limits.h>

lvAABBTree *lv_aabb_tree_build(const double *points, size_t count, int dim) {
    if (!points || count == 0 || count > (size_t) INT_MAX)
        return NULL;

    if (dim == 2) {
        lvAABB2D *bboxes = (lvAABB2D *) lv_malloc(count * sizeof(lvAABB2D));
        if (!bboxes)
            return NULL;
        for (size_t i = 0; i < count; i++) {
            bboxes[i].xmin = points[2 * i];
            bboxes[i].xmax = points[2 * i];
            bboxes[i].ymin = points[2 * i + 1];
            bboxes[i].ymax = points[2 * i + 1];
        }
        lvAABBTree2D *tree = lv_aabb2d_build(bboxes, (int) count, NULL);
        lv_free((void **) &bboxes);
        return (lvAABBTree *) tree;
    }
    if (dim == 3) {
        lvAABB3D *bboxes = (lvAABB3D *) lv_malloc(count * sizeof(lvAABB3D));
        if (!bboxes)
            return NULL;
        for (size_t i = 0; i < count; i++) {
            bboxes[i].xmin = points[3 * i];
            bboxes[i].xmax = points[3 * i];
            bboxes[i].ymin = points[3 * i + 1];
            bboxes[i].ymax = points[3 * i + 1];
            bboxes[i].zmin = points[3 * i + 2];
            bboxes[i].zmax = points[3 * i + 2];
        }
        lvAABBTree3D *tree = lv_aabb3d_build(bboxes, (int) count, NULL);
        lv_free((void **) &bboxes);
        return (lvAABBTree *) tree;
    }
    return NULL;
}

size_t lv_aabb_tree_query(const lvAABBTree *tree, const lvAABB *box, double *out, size_t max_out) {
    if (!tree || !box || !out || max_out == 0)
        return 0;

    lvAABBQueryResult result;
    lv_aabb_query_result_init(&result);

    lvAABB2D q;
    q.xmin = box->xmin;
    q.ymin = box->ymin;
    q.xmax = box->xmax;
    q.ymax = box->ymax;
    lv_aabb2d_range_query((const lvAABBTree2D *) tree, q, &result);

    size_t n = 0;
    for (int i = 0; i < result.count && n + 1 < max_out; i++) {
        int id = result.ids[i];
        if (id >= 0 && id < tree->primitive_count) {
            out[n++] = tree->primitives[id].xmin;
            out[n++] = tree->primitives[id].ymin;
        }
    }
    lv_aabb_query_result_free(&result);
    return n;
}