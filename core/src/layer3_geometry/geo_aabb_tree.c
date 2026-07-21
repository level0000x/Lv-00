/**
 * @file geo_aabb_tree.c
 * @brief AABB 树空间索引的完整 C 实现
 *
 * 实现策略：
 *   - 自顶向下的中位数分裂构建（top-down median split）
 *   - 射线查询使用 slab method（AABB 射线相交检测）
 *   - 最近邻查询使用线性搜索 + 剪枝（简化优先队列）
 *   - 范围查询使用递归遍历 + AABB 相交检测
 *
 * 借鉴来源：
 *   - CGAL AABB_tree (github.com/CGAL/cgal)
 *   - Boost.Geometry R-tree (boost.org/libs/geometry/index)
 *
 * @version v3.6.0
 */

#include "lv00/geo_aabb_tree.h"
#include "lv00/geometry_config.h"


#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <stdbool.h>
#include "lv00/lv00_utils.h"

#include "lv00/lv00_utils.h"

/* 如果 geometry_config.h 中没有定义 LV00_PUBLIC_API，则定义空宏 */
#ifndef LV00_PUBLIC_API
#define LV00_PUBLIC_API
#endif

/* ========================================================================
 * 内部辅助宏与常量
 * ======================================================================== */

/** 初始节点容量 */
#define AABB_INITIAL_CAPACITY 64

/** 默认叶子节点最大几何体数 */
#define AABB_DEFAULT_MAX_LEAF_SIZE 4

/** 默认最大树深度 */
#define AABB_DEFAULT_MAX_DEPTH 64

/** 无效节点索引 */
#define AABB_INVALID_NODE (-1)

/** 默认使用 SAH */
#define AABB_DEFAULT_USE_SAH true

/* ========================================================================
 * 内部辅助函数声明
 * ======================================================================== */

static int aabb_node_alloc(Lv00AABBTree2D *tree);
static int aabb3d_node_alloc(Lv00AABBTree3D *tree);

/* 2D 内部构建 */
static int aabb2d_build_recursive(Lv00AABBTree2D *tree,
                                   int *prim_indices, int count, int depth);

/* 3D 内部构建 */
static int aabb3d_build_recursive(Lv00AABBTree3D *tree,
                                   int *prim_indices, int count, int depth);

/* 2D 射线-AABB 相交（slab method） */
static bool aabb2d_ray_intersect(Lv00AABB2D bb, Lv00AABBRay2D ray,
                                  double tmin, double tmax);

/* 3D 射线-AABB 相交（slab method） */
static bool aabb3d_ray_intersect(Lv00AABB3D bb, Lv00AABBRay3D ray,
                                  double tmin, double tmax);

/* 2D 点到 AABB 最近距离 */
static double aabb2d_point_distance(Lv00AABB2D bb, double px, double py);

/* 3D 点到 AABB 最近距离 */
static double aabb3d_point_distance(Lv00AABB3D bb, double px, double py,
                                     double pz);

/* 2D 射线递归查询 */
static void aabb2d_ray_recursive(const Lv00AABBTree2D *tree, int node_idx,
                                  Lv00AABBRay2D ray,
                                  double tmin, double tmax,
                                  Lv00AABBRayHit *best);

/* 3D 射线递归查询 */
static void aabb3d_ray_recursive(const Lv00AABBTree3D *tree, int node_idx,
                                  Lv00AABBRay3D ray,
                                  double tmin, double tmax,
                                  Lv00AABBRayHit *best);

/* 2D 最近邻递归查询 */
static void aabb2d_nearest_recursive(const Lv00AABBTree2D *tree, int node_idx,
                                      double px, double py,
                                      Lv00AABBNearestResult *best);

/* 3D 最近邻递归查询 */
static void aabb3d_nearest_recursive(const Lv00AABBTree3D *tree, int node_idx,
                                      double px, double py, double pz,
                                      Lv00AABBNearestResult *best);

/* 2D 范围查询递归 */
static void aabb2d_range_recursive(const Lv00AABBTree2D *tree, int node_idx,
                                    Lv00AABB2D query,
                                    Lv00AABBQueryResult *result);

/* 3D 范围查询递归 */
static void aabb3d_range_recursive(const Lv00AABBTree3D *tree, int node_idx,
                                    Lv00AABB3D query,
                                    Lv00AABBQueryResult *result);

/* 2D 点查询递归 */
static void aabb2d_point_recursive(const Lv00AABBTree2D *tree, int node_idx,
                                    double px, double py,
                                    Lv00AABBQueryResult *result);

/* 3D 点查询递归 */
static void aabb3d_point_recursive(const Lv00AABBTree3D *tree, int node_idx,
                                    double px, double py, double pz,
                                    Lv00AABBQueryResult *result);

/* 计算树深度 */
static int aabb_tree_depth(const Lv00AABBNode *nodes, int root);

/* 计算叶子节点数量 */
static int aabb_tree_leaf_count(const Lv00AABBNode *nodes, int root);

/* ========================================================================
 * 第一部分：包围盒基础操作 —— 2D
 * ======================================================================== */

/**
 * @brief 创建空的 2D 包围盒
 *
 * 空包围盒用 +INF / -INF 标记，表示不包含任何点。
 */
LV00_PUBLIC_API Lv00AABB2D lv00_aabb2d_empty(void)
{
    Lv00AABB2D bb;
    bb.xmin =  DBL_MAX;
    bb.ymin =  DBL_MAX;
    bb.xmax = -DBL_MAX;
    bb.ymax = -DBL_MAX;
    return bb;
}

/**
 * @brief 创建包含单个点的 2D 包围盒
 */
LV00_PUBLIC_API Lv00AABB2D lv00_aabb2d_point(double x, double y)
{
    Lv00AABB2D bb;
    bb.xmin = bb.xmax = x;
    bb.ymin = bb.ymax = y;
    return bb;
}

/**
 * @brief 合并两个 2D 包围盒
 *
 * 对每条轴分别取 min/max，返回能同时包含 a 和 b 的最小包围盒。
 * 如果 a 或 b 为空包围盒，返回非空的那个。
 */
LV00_PUBLIC_API Lv00AABB2D lv00_aabb2d_merge(Lv00AABB2D a, Lv00AABB2D b)
{
    Lv00AABB2D result;
    result.xmin = (a.xmin < b.xmin) ? a.xmin : b.xmin;
    result.ymin = (a.ymin < b.ymin) ? a.ymin : b.ymin;
    result.xmax = (a.xmax > b.xmax) ? a.xmax : b.xmax;
    result.ymax = (a.ymax > b.ymax) ? a.ymax : b.ymax;
    return result;
}

/**
 * @brief 判定 2D 包围盒是否有效
 *
 * 有效包围盒满足 xmin <= xmax 且 ymin <= ymax。
 */
LV00_PUBLIC_API bool lv00_aabb2d_is_valid(Lv00AABB2D bb)
{
    return bb.xmin <= bb.xmax && bb.ymin <= bb.ymax;
}

/**
 * @brief 判定点是否在 2D 包围盒内（含边界）
 */
LV00_PUBLIC_API bool lv00_aabb2d_contains(Lv00AABB2D bb, double x, double y)
{
    return x >= bb.xmin && x <= bb.xmax &&
           y >= bb.ymin && y <= bb.ymax;
}

/**
 * @brief 判定两个 2D 包围盒是否相交
 *
 * 两个 AABB 相交当且仅当它们在所有轴上都有重叠区间。
 */
LV00_PUBLIC_API bool lv00_aabb2d_intersects(Lv00AABB2D a, Lv00AABB2D b)
{
    return a.xmin <= b.xmax && a.xmax >= b.xmin &&
           a.ymin <= b.ymax && a.ymax >= b.ymin;
}

/**
 * @brief 计算 2D 包围盒的面积
 *
 * 面积 = (xmax - xmin) * (ymax - ymin)
 * 空包围盒返回 0.0。
 */
LV00_PUBLIC_API double lv00_aabb2d_area(Lv00AABB2D bb)
{
    if (!lv00_aabb2d_is_valid(bb)) return 0.0;
    return (bb.xmax - bb.xmin) * (bb.ymax - bb.ymin);
}

/**
 * @brief 计算 2D 包围盒的中心点
 */
LV00_PUBLIC_API Lv00AABBPoint2D lv00_aabb2d_center(Lv00AABB2D bb)
{
    Lv00AABBPoint2D pt;
    pt.x = (bb.xmin + bb.xmax) * 0.5;
    pt.y = (bb.ymin + bb.ymax) * 0.5;
    return pt;
}

/* ========================================================================
 * 第一部分：包围盒基础操作 —— 3D
 * ======================================================================== */

/**
 * @brief 创建空的 3D 包围盒
 */
LV00_PUBLIC_API Lv00AABB3D lv00_aabb3d_empty(void)
{
    Lv00AABB3D bb;
    bb.xmin =  DBL_MAX;
    bb.ymin =  DBL_MAX;
    bb.zmin =  DBL_MAX;
    bb.xmax = -DBL_MAX;
    bb.ymax = -DBL_MAX;
    bb.zmax = -DBL_MAX;
    return bb;
}

/**
 * @brief 创建包含单个点的 3D 包围盒
 */
LV00_PUBLIC_API Lv00AABB3D lv00_aabb3d_point(double x, double y, double z)
{
    Lv00AABB3D bb;
    bb.xmin = bb.xmax = x;
    bb.ymin = bb.ymax = y;
    bb.zmin = bb.zmax = z;
    return bb;
}

/**
 * @brief 合并两个 3D 包围盒
 */
LV00_PUBLIC_API Lv00AABB3D lv00_aabb3d_merge(Lv00AABB3D a, Lv00AABB3D b)
{
    Lv00AABB3D result;
    result.xmin = (a.xmin < b.xmin) ? a.xmin : b.xmin;
    result.ymin = (a.ymin < b.ymin) ? a.ymin : b.ymin;
    result.zmin = (a.zmin < b.zmin) ? a.zmin : b.zmin;
    result.xmax = (a.xmax > b.xmax) ? a.xmax : b.xmax;
    result.ymax = (a.ymax > b.ymax) ? a.ymax : b.ymax;
    result.zmax = (a.zmax > b.zmax) ? a.zmax : b.zmax;
    return result;
}

/**
 * @brief 判定 3D 包围盒是否有效
 */
LV00_PUBLIC_API bool lv00_aabb3d_is_valid(Lv00AABB3D bb)
{
    return bb.xmin <= bb.xmax &&
           bb.ymin <= bb.ymax &&
           bb.zmin <= bb.zmax;
}

/**
 * @brief 判定点是否在 3D 包围盒内（含边界）
 */
LV00_PUBLIC_API bool lv00_aabb3d_contains(Lv00AABB3D bb, double x, double y,
                                            double z)
{
    return x >= bb.xmin && x <= bb.xmax &&
           y >= bb.ymin && y <= bb.ymax &&
           z >= bb.zmin && z <= bb.zmax;
}

/**
 * @brief 判定两个 3D 包围盒是否相交
 */
LV00_PUBLIC_API bool lv00_aabb3d_intersects(Lv00AABB3D a, Lv00AABB3D b)
{
    return a.xmin <= b.xmax && a.xmax >= b.xmin &&
           a.ymin <= b.ymax && a.ymax >= b.ymin &&
           a.zmin <= b.zmax && a.zmax >= b.zmin;
}

/**
 * @brief 计算 3D 包围盒的表面积
 *
 * 表面积 = 2 * (xy面 + yz面 + zx面)
 */
LV00_PUBLIC_API double lv00_aabb3d_surface_area(Lv00AABB3D bb)
{
    if (!lv00_aabb3d_is_valid(bb)) return 0.0;
    double dx = bb.xmax - bb.xmin;
    double dy = bb.ymax - bb.ymin;
    double dz = bb.zmax - bb.zmin;
    return 2.0 * (dx * dy + dy * dz + dz * dx);
}

/**
 * @brief 计算 3D 包围盒的体积
 */
LV00_PUBLIC_API double lv00_aabb3d_volume(Lv00AABB3D bb)
{
    if (!lv00_aabb3d_is_valid(bb)) return 0.0;
    return (bb.xmax - bb.xmin) *
           (bb.ymax - bb.ymin) *
           (bb.zmax - bb.zmin);
}

/**
 * @brief 计算 3D 包围盒的中心点
 */
LV00_PUBLIC_API Lv00AABBPoint3D lv00_aabb3d_center(Lv00AABB3D bb)
{
    Lv00AABBPoint3D pt;
    pt.x = (bb.xmin + bb.xmax) * 0.5;
    pt.y = (bb.ymin + bb.ymax) * 0.5;
    pt.z = (bb.zmin + bb.zmax) * 0.5;
    return pt;
}

/* ========================================================================
 * 第二部分：查询结果管理
 * ======================================================================== */

/**
 * @brief 初始化查询结果
 *
 * 将 ids 置 NULL，count 和 capacity 置 0。
 */
LV00_PUBLIC_API void lv00_aabb_query_result_init(Lv00AABBQueryResult *result)
{
    if (!result) return;
    result->ids      = NULL;
    result->count    = 0;
    result->capacity = 0;
}

/**
 * @brief 释放查询结果
 *
 * 释放 ids 数组并将结构体重置为初始状态。
 */
LV00_PUBLIC_API void lv00_aabb_query_result_free(Lv00AABBQueryResult *result)
{
    if (!result) return;
    lv00_free((void **)&result->ids);
    result->ids      = NULL;
    result->count    = 0;
    result->capacity = 0;
}

/**
 * @brief 获取默认 AABB 树配置
 */
LV00_PUBLIC_API Lv00AABBTreeConfig lv00_aabb_tree_default_config(void)
{
    Lv00AABBTreeConfig cfg;
    cfg.max_leaf_size = AABB_DEFAULT_MAX_LEAF_SIZE;
    cfg.max_depth     = AABB_DEFAULT_MAX_DEPTH;
    cfg.use_sah       = AABB_DEFAULT_USE_SAH;
    return cfg;
}

/* ========================================================================
 * 第三部分：内部辅助函数实现
 * ======================================================================== */

/**
 * @brief 为 2D 树分配一个新节点，返回节点索引
 *
 * 如果容量不足，自动扩容为 2 倍。
 */
static int aabb_node_alloc(Lv00AABBTree2D *tree)
{
    if (tree->node_count >= tree->node_capacity) {
        int new_cap = (tree->node_capacity > 0)
                          ? tree->node_capacity * 2
                          : AABB_INITIAL_CAPACITY;
        Lv00AABBNode *new_nodes = (Lv00AABBNode *)lv00_realloc(
            tree->nodes, (size_t)new_cap * sizeof(Lv00AABBNode));
        if (!new_nodes) return AABB_INVALID_NODE;
        tree->nodes = new_nodes;
        tree->node_capacity = new_cap;
    }
    int idx = tree->node_count++;
    memset(&tree->nodes[idx], 0, sizeof(Lv00AABBNode));
    tree->nodes[idx].left  = AABB_INVALID_NODE;
    tree->nodes[idx].right = AABB_INVALID_NODE;
    tree->nodes[idx].primitive_id = AABB_INVALID_NODE;
    tree->nodes[idx].height = 0;
    return idx;
}

/**
 * @brief 为 3D 树分配一个新节点，返回节点索引
 */
static int aabb3d_node_alloc(Lv00AABBTree3D *tree)
{
    if (tree->node_count >= tree->node_capacity) {
        int new_cap = (tree->node_capacity > 0)
                          ? tree->node_capacity * 2
                          : AABB_INITIAL_CAPACITY;
        Lv00AABBNode *new_nodes = (Lv00AABBNode *)lv00_realloc(
            tree->nodes, (size_t)new_cap * sizeof(Lv00AABBNode));
        if (!new_nodes) return AABB_INVALID_NODE;
        tree->nodes = new_nodes;
        tree->node_capacity = new_cap;
    }
    int idx = tree->node_count++;
    memset(&tree->nodes[idx], 0, sizeof(Lv00AABBNode));
    tree->nodes[idx].left  = AABB_INVALID_NODE;
    tree->nodes[idx].right = AABB_INVALID_NODE;
    tree->nodes[idx].primitive_id = AABB_INVALID_NODE;
    tree->nodes[idx].height = 0;
    return idx;
}

/* -----------------------------------------------------------------------
 * 2D 整数比较函数（用于 qsort）
 * ----------------------------------------------------------------------- */

/** 比较两个 int 值（升序） */
static int int_compare_asc(const void *a, const void *b)
{
    int ia = *(const int *)a;
    int ib = *(const int *)b;
    return (ia > ib) - (ia < ib);
}

/**
 * @brief 将 2D 几何体索引数组按指定轴的中心坐标排序
 */
static void sort_primitives_2d(const Lv00AABBTree2D *tree, int *indices,
                                int count, int axis)
{
    /* 使用简单的选择排序（对小数组足够高效，避免 qsort 的函数指针开销） */
    for (int i = 0; i < count - 1; i++) {
        int min_idx = i;
        double min_val = 0.0;
        {
            const Lv00AABB2D *bb = &tree->primitives[indices[i]];
            if (axis == 0) min_val = bb->xmin + bb->xmax;
            else           min_val = bb->ymin + bb->ymax;
        }
        for (int j = i + 1; j < count; j++) {
            const Lv00AABB2D *bb = &tree->primitives[indices[j]];
            double val = (axis == 0)
                             ? bb->xmin + bb->xmax
                             : bb->ymin + bb->ymax;
            if (val < min_val) {
                min_val = val;
                min_idx = j;
            }
        }
        if (min_idx != i) {
            int tmp = indices[i];
            indices[i] = indices[min_idx];
            indices[min_idx] = tmp;
        }
    }
}

/**
 * @brief 将 3D 几何体索引数组按指定轴的中心坐标排序
 */
static void sort_primitives_3d(const Lv00AABBTree3D *tree, int *indices,
                                int count, int axis)
{
    for (int i = 0; i < count - 1; i++) {
        int min_idx = i;
        double min_val = 0.0;
        {
            const Lv00AABB3D *bb = &tree->primitives[indices[i]];
            switch (axis) {
                case 0: min_val = bb->xmin + bb->xmax; break;
                case 1: min_val = bb->ymin + bb->ymax; break;
                default: min_val = bb->zmin + bb->zmax; break;
            }
        }
        for (int j = i + 1; j < count; j++) {
            const Lv00AABB3D *bb = &tree->primitives[indices[j]];
            double val;
            switch (axis) {
                case 0:  val = bb->xmin + bb->xmax; break;
                case 1:  val = bb->ymin + bb->ymax; break;
                default: val = bb->zmin + bb->zmax; break;
            }
            if (val < min_val) {
                min_val = val;
                min_idx = j;
            }
        }
        if (min_idx != i) {
            int tmp = indices[i];
            indices[i] = indices[min_idx];
            indices[min_idx] = tmp;
        }
    }
}

/* -----------------------------------------------------------------------
 * 2D 递归构建
 * ----------------------------------------------------------------------- */

/**
 * @brief 递归构建 2D AABB 树（自顶向下中位数分裂）
 *
 * @param tree        AABB 树
 * @param prim_indices 几何体索引数组（会被修改/重排）
 * @param count       当前子集的几何体数量
 * @param depth       当前递归深度
 * @return 新创建的节点索引
 */
static int aabb2d_build_recursive(Lv00AABBTree2D *tree,
                                   int *prim_indices, int count, int depth)
{
    /* 分配当前节点 */
    int node_idx = aabb_node_alloc(tree);
    if (node_idx == AABB_INVALID_NODE) return AABB_INVALID_NODE;

    /* 计算当前子集的包围盒 */
    Lv00AABB3D node_bbox = lv00_aabb3d_empty();
    for (int i = 0; i < count; i++) {
        const Lv00AABB2D *bb2d = &tree->primitives[prim_indices[i]];
        Lv00AABB3D bb3d;
        bb3d.xmin = bb2d->xmin;
        bb3d.ymin = bb2d->ymin;
        bb3d.zmin = 0.0;
        bb3d.xmax = bb2d->xmax;
        bb3d.ymax = bb2d->ymax;
        bb3d.zmax = 0.0;
        node_bbox = lv00_aabb3d_merge(node_bbox, bb3d);
    }
    tree->nodes[node_idx].bbox = node_bbox;

    /* 终止条件：几何体数量 <= max_leaf_size 或达到最大深度 */
    if (count <= tree->config.max_leaf_size || depth >= tree->config.max_depth) {
        /* 叶子节点：保存所有几何体 ID 到 leaf_prim_ids */
        tree->nodes[node_idx].left  = AABB_INVALID_NODE;
        tree->nodes[node_idx].right = AABB_INVALID_NODE;
        tree->nodes[node_idx].height = 0;
        tree->nodes[node_idx].primitive_id = prim_indices[0];
        tree->nodes[node_idx].leaf_start = tree->leaf_prim_capacity;
        tree->nodes[node_idx].leaf_count = count;

        /* 扩展 leaf_prim_ids 容量（如需要） */
        int old_size = tree->leaf_prim_capacity;
        int needed = old_size + count;
        if (needed > tree->leaf_prim_capacity) {
            int new_cap = (tree->leaf_prim_capacity > 0)
                              ? tree->leaf_prim_capacity * 2
                              : AABB_INITIAL_CAPACITY;
            while (new_cap < needed) new_cap *= 2;
            int *new_ids = (int *)lv00_realloc(tree->leaf_prim_ids,
                                       (size_t)new_cap * sizeof(int));
            if (!new_ids) return AABB_INVALID_NODE;
            tree->leaf_prim_ids = new_ids;
            tree->leaf_prim_capacity = new_cap;
        }

        /* 写入所有几何体 ID（使用 old_size 作为偏移） */
        for (int k = 0; k < count; k++) {
            tree->leaf_prim_ids[old_size + k] = prim_indices[k];
        }
        /* 注意：leaf_prim_capacity 保持为容量值，不要修改为已使用大小 */
        return node_idx;
    }

    /* 选择分裂轴：选择跨度最大的轴 */
    double span_x = node_bbox.xmax - node_bbox.xmin;
    double span_y = node_bbox.ymax - node_bbox.ymin;
    int split_axis = (span_x >= span_y) ? 0 : 1;

    /* 按分裂轴排序 */
    sort_primitives_2d(tree, prim_indices, count, split_axis);

    /* 中位数分裂 */
    int mid = count / 2;

    /* 递归构建左右子树 */
    int left_idx  = aabb2d_build_recursive(tree, prim_indices, mid, depth + 1);
    int right_idx = aabb2d_build_recursive(tree, prim_indices + mid,
                                            count - mid, depth + 1);

    tree->nodes[node_idx].left  = left_idx;
    tree->nodes[node_idx].right = right_idx;
    tree->nodes[node_idx].primitive_id = AABB_INVALID_NODE;

    /* 高度 = max(左子树高度, 右子树高度) + 1 */
    int lh = (left_idx != AABB_INVALID_NODE)  ? tree->nodes[left_idx].height  : 0;
    int rh = (right_idx != AABB_INVALID_NODE) ? tree->nodes[right_idx].height : 0;
    tree->nodes[node_idx].height = ((lh > rh) ? lh : rh) + 1;

    return node_idx;
}

/* -----------------------------------------------------------------------
 * 3D 递归构建
 * ----------------------------------------------------------------------- */

/**
 * @brief 递归构建 3D AABB 树（自顶向下中位数分裂）
 */
static int aabb3d_build_recursive(Lv00AABBTree3D *tree,
                                   int *prim_indices, int count, int depth)
{
    int node_idx = aabb3d_node_alloc(tree);
    if (node_idx == AABB_INVALID_NODE) return AABB_INVALID_NODE;

    /* 计算当前子集的包围盒 */
    Lv00AABB3D node_bbox = lv00_aabb3d_empty();
    for (int i = 0; i < count; i++) {
        node_bbox = lv00_aabb3d_merge(node_bbox,
                                       tree->primitives[prim_indices[i]]);
    }
    tree->nodes[node_idx].bbox = node_bbox;

    /* 终止条件 */
    if (count <= tree->config.max_leaf_size || depth >= tree->config.max_depth) {
        tree->nodes[node_idx].primitive_id = prim_indices[0];
        tree->nodes[node_idx].left  = AABB_INVALID_NODE;
        tree->nodes[node_idx].right = AABB_INVALID_NODE;
        tree->nodes[node_idx].height = 0;
        return node_idx;
    }

    /* 选择分裂轴：选择跨度最大的轴 */
    double span_x = node_bbox.xmax - node_bbox.xmin;
    double span_y = node_bbox.ymax - node_bbox.ymin;
    double span_z = node_bbox.zmax - node_bbox.zmin;
    int split_axis = 0;
    if (span_y >= span_x && span_y >= span_z) split_axis = 1;
    else if (span_z >= span_x && span_z >= span_y) split_axis = 2;

    /* 按分裂轴排序 */
    sort_primitives_3d(tree, prim_indices, count, split_axis);

    /* 中位数分裂 */
    int mid = count / 2;

    /* 递归构建左右子树 */
    int left_idx  = aabb3d_build_recursive(tree, prim_indices, mid, depth + 1);
    int right_idx = aabb3d_build_recursive(tree, prim_indices + mid,
                                            count - mid, depth + 1);

    tree->nodes[node_idx].left  = left_idx;
    tree->nodes[node_idx].right = right_idx;
    tree->nodes[node_idx].primitive_id = AABB_INVALID_NODE;

    int lh = (left_idx != AABB_INVALID_NODE)  ? tree->nodes[left_idx].height  : 0;
    int rh = (right_idx != AABB_INVALID_NODE) ? tree->nodes[right_idx].height : 0;
    tree->nodes[node_idx].height = ((lh > rh) ? lh : rh) + 1;

    return node_idx;
}

/* -----------------------------------------------------------------------
 * 射线-AABB 相交检测（Slab Method）
 * ----------------------------------------------------------------------- */

/**
 * @brief 2D 射线与 AABB 相交检测（Slab Method）
 *
 * Slab method 将 AABB 视为三个（2D 为两个）slab 的交集，
 * 通过计算射线在每个 slab 上的进入/退出区间来确定整体相交区间。
 *
 * @param bb    包围盒
 * @param ray   射线
 * @param tmin  射线参数下界
 * @param tmax  射线参数上界
 * @return 射线在 [tmin, tmax] 范围内是否与 bb 相交
 */
static bool aabb2d_ray_intersect(Lv00AABB2D bb, Lv00AABBRay2D ray,
                                  double tmin, double tmax)
{
    /* X 轴 slab */
    if (fabs(ray.dx) < DBL_EPSILON) {
        /* 射线平行于 X 轴平面 */
        if (ray.ox < bb.xmin || ray.ox > bb.xmax) return false;
    } else {
        double inv_d = 1.0 / ray.dx;
        double t1 = (bb.xmin - ray.ox) * inv_d;
        double t2 = (bb.xmax - ray.ox) * inv_d;
        if (t1 > t2) { double tmp = t1; t1 = t2; t2 = tmp; }
        if (t1 > tmin) tmin = t1;
        if (t2 < tmax) tmax = t2;
        if (tmin > tmax) return false;
    }

    /* Y 轴 slab */
    if (fabs(ray.dy) < DBL_EPSILON) {
        if (ray.oy < bb.ymin || ray.oy > bb.ymax) return false;
    } else {
        double inv_d = 1.0 / ray.dy;
        double t1 = (bb.ymin - ray.oy) * inv_d;
        double t2 = (bb.ymax - ray.oy) * inv_d;
        if (t1 > t2) { double tmp = t1; t1 = t2; t2 = tmp; }
        if (t1 > tmin) tmin = t1;
        if (t2 < tmax) tmax = t2;
        if (tmin > tmax) return false;
    }

    return true;
}

/**
 * @brief 3D 射线与 AABB 相交检测（Slab Method）
 */
static bool aabb3d_ray_intersect(Lv00AABB3D bb, Lv00AABBRay3D ray,
                                  double tmin, double tmax)
{
    /* X 轴 slab */
    if (fabs(ray.dx) < DBL_EPSILON) {
        if (ray.ox < bb.xmin || ray.ox > bb.xmax) return false;
    } else {
        double inv_d = 1.0 / ray.dx;
        double t1 = (bb.xmin - ray.ox) * inv_d;
        double t2 = (bb.xmax - ray.ox) * inv_d;
        if (t1 > t2) { double tmp = t1; t1 = t2; t2 = tmp; }
        if (t1 > tmin) tmin = t1;
        if (t2 < tmax) tmax = t2;
        if (tmin > tmax) return false;
    }

    /* Y 轴 slab */
    if (fabs(ray.dy) < DBL_EPSILON) {
        if (ray.oy < bb.ymin || ray.oy > bb.ymax) return false;
    } else {
        double inv_d = 1.0 / ray.dy;
        double t1 = (bb.ymin - ray.oy) * inv_d;
        double t2 = (bb.ymax - ray.oy) * inv_d;
        if (t1 > t2) { double tmp = t1; t1 = t2; t2 = tmp; }
        if (t1 > tmin) tmin = t1;
        if (t2 < tmax) tmax = t2;
        if (tmin > tmax) return false;
    }

    /* Z 轴 slab */
    if (fabs(ray.dz) < DBL_EPSILON) {
        if (ray.oz < bb.zmin || ray.oz > bb.zmax) return false;
    } else {
        double inv_d = 1.0 / ray.dz;
        double t1 = (bb.zmin - ray.oz) * inv_d;
        double t2 = (bb.zmax - ray.oz) * inv_d;
        if (t1 > t2) { double tmp = t1; t1 = t2; t2 = tmp; }
        if (t1 > tmin) tmin = t1;
        if (t2 < tmax) tmax = t2;
        if (tmin > tmax) return false;
    }

    return true;
}

/* -----------------------------------------------------------------------
 * 点到 AABB 最近距离
 * ----------------------------------------------------------------------- */

/**
 * @brief 计算 2D 点到 AABB 的最近距离平方
 *
 * 如果点在 AABB 内部，返回 0。
 * 否则返回各轴上最近距离的平方和。
 */
static double aabb2d_point_distance_sq(Lv00AABB2D bb, double px, double py)
{
    double dx = 0.0, dy = 0.0;

    if (px < bb.xmin)      dx = bb.xmin - px;
    else if (px > bb.xmax) dx = px - bb.xmax;

    if (py < bb.ymin)      dy = bb.ymin - py;
    else if (py > bb.ymax) dy = py - bb.ymax;

    return dx * dx + dy * dy;
}

/**
 * @brief 计算 3D 点到 AABB 的最近距离平方
 */
static double aabb3d_point_distance_sq(Lv00AABB3D bb, double px, double py,
                                        double pz)
{
    double dx = 0.0, dy = 0.0, dz = 0.0;

    if (px < bb.xmin)      dx = bb.xmin - px;
    else if (px > bb.xmax) dx = px - bb.xmax;

    if (py < bb.ymin)      dy = bb.ymin - py;
    else if (py > bb.ymax) dy = py - bb.ymax;

    if (pz < bb.zmin)      dz = bb.zmin - pz;
    else if (pz > bb.zmax) dz = pz - bb.zmax;

    return dx * dx + dy * dy + dz * dz;
}

/**
 * @brief 计算 2D 点到 AABB 的最近距离
 */
static double aabb2d_point_distance(Lv00AABB2D bb, double px, double py)
{
    return sqrt(aabb2d_point_distance_sq(bb, px, py));
}

/**
 * @brief 计算 3D 点到 AABB 的最近距离
 */
static double aabb3d_point_distance(Lv00AABB3D bb, double px, double py,
                                     double pz)
{
    return sqrt(aabb3d_point_distance_sq(bb, px, py, pz));
}

/* -----------------------------------------------------------------------
 * 2D 点到几何体（AABB）最近点
 * ----------------------------------------------------------------------- */

/**
 * @brief 计算 2D 点到 2D AABB 的最近点坐标
 */
static Lv00AABBPoint2D aabb2d_closest_point(Lv00AABB2D bb, double px, double py)
{
    Lv00AABBPoint2D cp;
    cp.x = (px < bb.xmin) ? bb.xmin : (px > bb.xmax) ? bb.xmax : px;
    cp.y = (py < bb.ymin) ? bb.ymin : (py > bb.ymax) ? bb.ymax : py;
    return cp;
}

/**
 * @brief 计算 3D 点到 3D AABB 的最近点坐标
 */
static Lv00AABBPoint3D aabb3d_closest_point(Lv00AABB3D bb, double px,
                                              double py, double pz)
{
    Lv00AABBPoint3D cp;
    cp.x = (px < bb.xmin) ? bb.xmin : (px > bb.xmax) ? bb.xmax : px;
    cp.y = (py < bb.ymin) ? bb.ymin : (py > bb.ymax) ? bb.ymax : py;
    cp.z = (pz < bb.zmin) ? bb.zmin : (pz > bb.zmax) ? bb.zmax : pz;
    return cp;
}

/* -----------------------------------------------------------------------
 * 射线递归查询
 * ----------------------------------------------------------------------- */

/**
 * @brief 2D 射线递归查询
 *
 * 使用 slab method 检测射线与节点 AABB 的相交性，
 * 优先遍历更近的子树以实现提前终止。
 */
static void aabb2d_ray_recursive(const Lv00AABBTree2D *tree, int node_idx,
                                  Lv00AABBRay2D ray,
                                  double tmin, double tmax,
                                  Lv00AABBRayHit *best)
{
    if (node_idx == AABB_INVALID_NODE) return;
    if (tmin > best->t) return; /* 剪枝：已有更近的命中 */

    const Lv00AABBNode *node = &tree->nodes[node_idx];

    /* 叶子节点：检测射线与几何体 AABB 的精确相交 */
    if (node->left == AABB_INVALID_NODE && node->right == AABB_INVALID_NODE) {
        if (node->primitive_id == AABB_INVALID_NODE) return;

        const Lv00AABB2D *prim_bb = &tree->primitives[node->primitive_id];
        if (aabb2d_ray_intersect(*prim_bb, ray, tmin, tmax)) {
            /* 计算精确的 t 值 */
            double t_enter = tmin;
            /* 重新计算精确 t 值 */
            double t0 = 0.0, t1 = DBL_MAX;

            /* X slab */
            if (fabs(ray.dx) > DBL_EPSILON) {
                double inv_d = 1.0 / ray.dx;
                double tx1 = (prim_bb->xmin - ray.ox) * inv_d;
                double tx2 = (prim_bb->xmax - ray.ox) * inv_d;
                if (tx1 > tx2) { double tmp = tx1; tx1 = tx2; tx2 = tmp; }
                if (tx1 > t0) t0 = tx1;
                if (tx2 < t1) t1 = tx2;
            } else {
                if (ray.ox < prim_bb->xmin || ray.ox > prim_bb->xmax) return;
            }

            /* Y slab */
            if (fabs(ray.dy) > DBL_EPSILON) {
                double inv_d = 1.0 / ray.dy;
                double ty1 = (prim_bb->ymin - ray.oy) * inv_d;
                double ty2 = (prim_bb->ymax - ray.oy) * inv_d;
                if (ty1 > ty2) { double tmp = ty1; ty1 = ty2; ty2 = tmp; }
                if (ty1 > t0) t0 = ty1;
                if (ty2 < t1) t1 = ty2;
            } else {
                if (ray.oy < prim_bb->ymin || ray.oy > prim_bb->ymax) return;
            }

            if (t0 <= t1 && t0 >= 0.0 && t0 < best->t) {
                best->hit = true;
                best->t = t0;
                best->primitive_id = node->primitive_id;
            }
        }
        return;
    }

    /* 内部节点：检测射线与节点 AABB 的相交 */
    /* 将 3D bbox 转换为 2D 用于检测 */
    Lv00AABB2D node_bb2d;
    node_bb2d.xmin = node->bbox.xmin;
    node_bb2d.ymin = node->bbox.ymin;
    node_bb2d.xmax = node->bbox.xmax;
    node_bb2d.ymax = node->bbox.ymax;

    if (!aabb2d_ray_intersect(node_bb2d, ray, tmin, tmax)) return;

    /* 递归遍历子树 */
    aabb2d_ray_recursive(tree, node->left,  ray, tmin, tmax, best);
    aabb2d_ray_recursive(tree, node->right, ray, tmin, tmax, best);
}

/**
 * @brief 3D 射线递归查询
 */
static void aabb3d_ray_recursive(const Lv00AABBTree3D *tree, int node_idx,
                                  Lv00AABBRay3D ray,
                                  double tmin, double tmax,
                                  Lv00AABBRayHit *best)
{
    if (node_idx == AABB_INVALID_NODE) return;
    if (tmin > best->t) return;

    const Lv00AABBNode *node = &tree->nodes[node_idx];

    /* 叶子节点 */
    if (node->left == AABB_INVALID_NODE && node->right == AABB_INVALID_NODE) {
        if (node->primitive_id == AABB_INVALID_NODE) return;

        const Lv00AABB3D *prim_bb = &tree->primitives[node->primitive_id];
        if (aabb3d_ray_intersect(*prim_bb, ray, tmin, tmax)) {
            double t0 = 0.0, t1 = DBL_MAX;

            /* X slab */
            if (fabs(ray.dx) > DBL_EPSILON) {
                double inv_d = 1.0 / ray.dx;
                double tx1 = (prim_bb->xmin - ray.ox) * inv_d;
                double tx2 = (prim_bb->xmax - ray.ox) * inv_d;
                if (tx1 > tx2) { double tmp = tx1; tx1 = tx2; tx2 = tmp; }
                if (tx1 > t0) t0 = tx1;
                if (tx2 < t1) t1 = tx2;
            } else {
                if (ray.ox < prim_bb->xmin || ray.ox > prim_bb->xmax) return;
            }

            /* Y slab */
            if (fabs(ray.dy) > DBL_EPSILON) {
                double inv_d = 1.0 / ray.dy;
                double ty1 = (prim_bb->ymin - ray.oy) * inv_d;
                double ty2 = (prim_bb->ymax - ray.oy) * inv_d;
                if (ty1 > ty2) { double tmp = ty1; ty1 = ty2; ty2 = tmp; }
                if (ty1 > t0) t0 = ty1;
                if (ty2 < t1) t1 = ty2;
            } else {
                if (ray.oy < prim_bb->ymin || ray.oy > prim_bb->ymax) return;
            }

            /* Z slab */
            if (fabs(ray.dz) > DBL_EPSILON) {
                double inv_d = 1.0 / ray.dz;
                double tz1 = (prim_bb->zmin - ray.oz) * inv_d;
                double tz2 = (prim_bb->zmax - ray.oz) * inv_d;
                if (tz1 > tz2) { double tmp = tz1; tz1 = tz2; tz2 = tmp; }
                if (tz1 > t0) t0 = tz1;
                if (tz2 < t1) t1 = tz2;
            } else {
                if (ray.oz < prim_bb->zmin || ray.oz > prim_bb->zmax) return;
            }

            if (t0 <= t1 && t0 >= 0.0 && t0 < best->t) {
                best->hit = true;
                best->t = t0;
                best->primitive_id = node->primitive_id;
            }
        }
        return;
    }

    /* 内部节点 */
    if (!aabb3d_ray_intersect(node->bbox, ray, tmin, tmax)) return;

    aabb3d_ray_recursive(tree, node->left,  ray, tmin, tmax, best);
    aabb3d_ray_recursive(tree, node->right, ray, tmin, tmax, best);
}

/* -----------------------------------------------------------------------
 * 最近邻递归查询
 * ----------------------------------------------------------------------- */

/**
 * @brief 2D 最近邻递归查询（线性搜索 + 剪枝）
 *
 * 遍历所有叶子节点，计算查询点到几何体 AABB 的距离，
 * 利用查询点到内部节点 AABB 的距离进行剪枝。
 */
static void aabb2d_nearest_recursive(const Lv00AABBTree2D *tree, int node_idx,
                                      double px, double py,
                                      Lv00AABBNearestResult *best)
{
    if (node_idx == AABB_INVALID_NODE) return;

    const Lv00AABBNode *node = &tree->nodes[node_idx];

    /* 剪枝：如果查询点到当前节点 AABB 的距离已经大于已知最近距离 */
    Lv00AABB2D node_bb2d;
    node_bb2d.xmin = node->bbox.xmin;
    node_bb2d.ymin = node->bbox.ymin;
    node_bb2d.xmax = node->bbox.xmax;
    node_bb2d.ymax = node->bbox.ymax;

    double dist_to_node = aabb2d_point_distance(node_bb2d, px, py);
    if (dist_to_node > best->distance) return;

    /* 叶子节点 */
    if (node->left == AABB_INVALID_NODE && node->right == AABB_INVALID_NODE) {
        /* 遍历叶子节点包含的所有几何体 */
        int count = node->leaf_count;
        if (count <= 0) count = 1;  /* 向后兼容：使用 primitive_id */
        for (int k = 0; k < count; k++) {
            int pid = (node->leaf_count > 0 && tree->leaf_prim_ids)
                          ? tree->leaf_prim_ids[node->leaf_start + k]
                          : node->primitive_id;
            if (pid < 0 || pid >= tree->primitive_count) continue;

            const Lv00AABB2D *prim_bb = &tree->primitives[pid];
            Lv00AABBPoint2D cp = aabb2d_closest_point(*prim_bb, px, py);
            double dx = px - cp.x;
            double dy = py - cp.y;
            double dist = sqrt(dx * dx + dy * dy);

            if (dist < best->distance) {
                best->distance    = dist;
                best->primitive_id = pid;
                best->closest_x   = cp.x;
                best->closest_y   = cp.y;
                best->closest_z   = 0.0;
            }
        }
        return;
    }

    /* 内部节点：优先遍历更近的子树 */
    int first  = node->left;
    int second = node->right;

    if (first != AABB_INVALID_NODE && second != AABB_INVALID_NODE) {
        Lv00AABB2D left_bb2d, right_bb2d;
        left_bb2d.xmin  = tree->nodes[first].bbox.xmin;
        left_bb2d.ymin  = tree->nodes[first].bbox.ymin;
        left_bb2d.xmax  = tree->nodes[first].bbox.xmax;
        left_bb2d.ymax  = tree->nodes[first].bbox.ymax;
        right_bb2d.xmin = tree->nodes[second].bbox.xmin;
        right_bb2d.ymin = tree->nodes[second].bbox.ymin;
        right_bb2d.xmax = tree->nodes[second].bbox.xmax;
        right_bb2d.ymax = tree->nodes[second].bbox.ymax;

        double d_left  = aabb2d_point_distance(left_bb2d, px, py);
        double d_right = aabb2d_point_distance(right_bb2d, px, py);
        if (d_right < d_left) {
            int tmp = first; first = second; second = tmp;
        }
    }

    aabb2d_nearest_recursive(tree, first,  px, py, best);
    aabb2d_nearest_recursive(tree, second, px, py, best);
}

/**
 * @brief 3D 最近邻递归查询
 */
static void aabb3d_nearest_recursive(const Lv00AABBTree3D *tree, int node_idx,
                                      double px, double py, double pz,
                                      Lv00AABBNearestResult *best)
{
    if (node_idx == AABB_INVALID_NODE) return;

    const Lv00AABBNode *node = &tree->nodes[node_idx];

    /* 剪枝 */
    double dist_to_node = aabb3d_point_distance(node->bbox, px, py, pz);
    if (dist_to_node > best->distance) return;

    /* 叶子节点 */
    if (node->left == AABB_INVALID_NODE && node->right == AABB_INVALID_NODE) {
        if (node->primitive_id == AABB_INVALID_NODE) return;

        const Lv00AABB3D *prim_bb = &tree->primitives[node->primitive_id];
        Lv00AABBPoint3D cp = aabb3d_closest_point(*prim_bb, px, py, pz);
        double dx = px - cp.x;
        double dy = py - cp.y;
        double dz = pz - cp.z;
        double dist = sqrt(dx * dx + dy * dy + dz * dz);

        if (dist < best->distance) {
            best->distance     = dist;
            best->primitive_id = node->primitive_id;
            best->closest_x    = cp.x;
            best->closest_y    = cp.y;
            best->closest_z    = cp.z;
        }
        return;
    }

    /* 内部节点：优先遍历更近的子树 */
    int first  = node->left;
    int second = node->right;

    if (first != AABB_INVALID_NODE && second != AABB_INVALID_NODE) {
        double d_left  = aabb3d_point_distance(tree->nodes[first].bbox,
                                                px, py, pz);
        double d_right = aabb3d_point_distance(tree->nodes[second].bbox,
                                                px, py, pz);
        if (d_right < d_left) {
            int tmp = first; first = second; second = tmp;
        }
    }

    aabb3d_nearest_recursive(tree, first,  px, py, pz, best);
    aabb3d_nearest_recursive(tree, second, px, py, pz, best);
}

/* -----------------------------------------------------------------------
 * 范围查询递归
 * ----------------------------------------------------------------------- */

/**
 * @brief 向查询结果中添加一个 ID
 */
static void result_push_back(Lv00AABBQueryResult *result, int id)
{
    if (result->count >= result->capacity) {
        int new_cap = (result->capacity > 0)
                          ? result->capacity * 2
                          : 16;
        int *new_ids = (int *)lv00_realloc(result->ids,
                                       (size_t)new_cap * sizeof(int));
        if (!new_ids) return;
        result->ids = new_ids;
        result->capacity = new_cap;
    }
    result->ids[result->count++] = id;
}

/**
 * @brief 2D 范围查询递归
 */
static void aabb2d_range_recursive(const Lv00AABBTree2D *tree, int node_idx,
                                    Lv00AABB2D query,
                                    Lv00AABBQueryResult *result)
{
    if (node_idx == AABB_INVALID_NODE) return;

    const Lv00AABBNode *node = &tree->nodes[node_idx];

    /* 将节点 3D bbox 转换为 2D */
    Lv00AABB2D node_bb2d;
    node_bb2d.xmin = node->bbox.xmin;
    node_bb2d.ymin = node->bbox.ymin;
    node_bb2d.xmax = node->bbox.xmax;
    node_bb2d.ymax = node->bbox.ymax;

    /* 剪枝：节点 AABB 与查询 AABB 不相交则跳过 */
    if (!lv00_aabb2d_intersects(node_bb2d, query)) return;

    /* 叶子节点 */
    if (node->left == AABB_INVALID_NODE && node->right == AABB_INVALID_NODE) {
        if (node->primitive_id == AABB_INVALID_NODE) return;
        if (lv00_aabb2d_intersects(tree->primitives[node->primitive_id],
                                    query)) {
            result_push_back(result, node->primitive_id);
        }
        return;
    }

    /* 递归遍历子树 */
    aabb2d_range_recursive(tree, node->left,  query, result);
    aabb2d_range_recursive(tree, node->right, query, result);
}

/**
 * @brief 3D 范围查询递归
 */
static void aabb3d_range_recursive(const Lv00AABBTree3D *tree, int node_idx,
                                    Lv00AABB3D query,
                                    Lv00AABBQueryResult *result)
{
    if (node_idx == AABB_INVALID_NODE) return;

    const Lv00AABBNode *node = &tree->nodes[node_idx];

    /* 剪枝 */
    if (!lv00_aabb3d_intersects(node->bbox, query)) return;

    /* 叶子节点 */
    if (node->left == AABB_INVALID_NODE && node->right == AABB_INVALID_NODE) {
        if (node->primitive_id == AABB_INVALID_NODE) return;
        if (lv00_aabb3d_intersects(tree->primitives[node->primitive_id],
                                    query)) {
            result_push_back(result, node->primitive_id);
        }
        return;
    }

    /* 递归遍历子树 */
    aabb3d_range_recursive(tree, node->left,  query, result);
    aabb3d_range_recursive(tree, node->right, query, result);
}

/* -----------------------------------------------------------------------
 * 点查询递归
 * ----------------------------------------------------------------------- */

/**
 * @brief 2D 点查询递归
 */
static void aabb2d_point_recursive(const Lv00AABBTree2D *tree, int node_idx,
                                    double px, double py,
                                    Lv00AABBQueryResult *result)
{
    if (node_idx == AABB_INVALID_NODE) return;

    const Lv00AABBNode *node = &tree->nodes[node_idx];

    /* 将节点 3D bbox 转换为 2D */
    Lv00AABB2D node_bb2d;
    node_bb2d.xmin = node->bbox.xmin;
    node_bb2d.ymin = node->bbox.ymin;
    node_bb2d.xmax = node->bbox.xmax;
    node_bb2d.ymax = node->bbox.ymax;

    /* 剪枝：点不在节点 AABB 内 */
    if (!lv00_aabb2d_contains(node_bb2d, px, py)) return;

    /* 叶子节点 */
    if (node->left == AABB_INVALID_NODE && node->right == AABB_INVALID_NODE) {
        if (node->primitive_id == AABB_INVALID_NODE) return;
        if (lv00_aabb2d_contains(tree->primitives[node->primitive_id],
                                  px, py)) {
            result_push_back(result, node->primitive_id);
        }
        return;
    }

    /* 递归遍历子树 */
    aabb2d_point_recursive(tree, node->left,  px, py, result);
    aabb2d_point_recursive(tree, node->right, px, py, result);
}

/**
 * @brief 3D 点查询递归
 */
static void aabb3d_point_recursive(const Lv00AABBTree3D *tree, int node_idx,
                                    double px, double py, double pz,
                                    Lv00AABBQueryResult *result)
{
    if (node_idx == AABB_INVALID_NODE) return;

    const Lv00AABBNode *node = &tree->nodes[node_idx];

    /* 剪枝 */
    if (!lv00_aabb3d_contains(node->bbox, px, py, pz)) return;

    /* 叶子节点 */
    if (node->left == AABB_INVALID_NODE && node->right == AABB_INVALID_NODE) {
        if (node->primitive_id == AABB_INVALID_NODE) return;
        if (lv00_aabb3d_contains(tree->primitives[node->primitive_id],
                                  px, py, pz)) {
            result_push_back(result, node->primitive_id);
        }
        return;
    }

    /* 递归遍历子树 */
    aabb3d_point_recursive(tree, node->left,  px, py, pz, result);
    aabb3d_point_recursive(tree, node->right, px, py, pz, result);
}

/* -----------------------------------------------------------------------
 * 树统计辅助函数
 * ----------------------------------------------------------------------- */

/**
 * @brief 递归计算树深度
 */
static int aabb_tree_depth(const Lv00AABBNode *nodes, int root)
{
    if (root == AABB_INVALID_NODE) return 0;
    return nodes[root].height + 1;
}

/**
 * @brief 递归计算叶子节点数量
 */
static int aabb_tree_leaf_count(const Lv00AABBNode *nodes, int root)
{
    if (root == AABB_INVALID_NODE) return 0;

    const Lv00AABBNode *node = &nodes[root];
    if (node->left == AABB_INVALID_NODE && node->right == AABB_INVALID_NODE) {
        return 1;
    }
    return aabb_tree_leaf_count(nodes, node->left) +
           aabb_tree_leaf_count(nodes, node->right);
}

/* ========================================================================
 * 第四部分：AABB 树构建与查询 API —— 2D
 * ======================================================================== */

/**
 * @brief 构建 2D AABB 树
 *
 * 使用自顶向下的中位数分裂策略构建平衡的 AABB 树。
 * 内部节点存储子节点的合并包围盒，叶子节点存储几何体 ID。
 *
 * @param bboxes    几何体包围盒数组
 * @param count     几何体数量
 * @param config    配置（NULL 使用默认配置）
 * @return AABB 树指针（需用 lv00_aabb2d_destroy 释放）
 */
LV00_PUBLIC_API Lv00AABBTree2D *lv00_aabb2d_build(
    const Lv00AABB2D *bboxes, int count,
    const Lv00AABBTreeConfig *config)
{
    if (!bboxes || count <= 0) return NULL;

    /* 分配树结构 */
    Lv00AABBTree2D *tree = (Lv00AABBTree2D *)lv00_malloc(sizeof(Lv00AABBTree2D));
    if (!tree) return NULL;

    /* 初始化 */
    tree->nodes          = NULL;
    tree->node_count     = 0;
    tree->node_capacity  = 0;
    tree->root           = AABB_INVALID_NODE;
    tree->primitive_count = count;
    tree->leaf_prim_ids  = NULL;
    tree->leaf_prim_capacity = 0;

    /* 设置配置 */
    if (config) {
        tree->config = *config;
    } else {
        tree->config = lv00_aabb_tree_default_config();
    }

    /* 拷贝几何体包围盒 */
    tree->primitives = (Lv00AABB2D *)lv00_malloc((size_t)count * sizeof(Lv00AABB2D));
    if (!tree->primitives) {
        lv00_free((void **)&tree);
        return NULL;
    }
    memcpy(tree->primitives, bboxes, (size_t)count * sizeof(Lv00AABB2D));

    /* 创建几何体索引数组 */
    int *prim_indices = (int *)lv00_malloc((size_t)count * sizeof(int));
    if (!prim_indices) {
        lv00_free((void **)&tree->primitives);
        lv00_free((void **)&tree);
        return NULL;
    }
    for (int i = 0; i < count; i++) {
        prim_indices[i] = i;
    }

    /* 递归构建 */
    tree->root = aabb2d_build_recursive(tree, prim_indices, count, 0);

    lv00_free((void **)&prim_indices);

    if (tree->root == AABB_INVALID_NODE) {
        lv00_free((void **)&tree->primitives);
        lv00_free((void **)&tree->nodes);
        lv00_free((void **)&tree);
        return NULL;
    }

    return tree;
}

/**
 * @brief 释放 2D AABB 树
 *
 * 释放树结构、节点数组和几何体包围盒数组。
 */
LV00_PUBLIC_API void lv00_aabb2d_destroy(Lv00AABBTree2D *tree)
{
    if (!tree) return;
    lv00_free((void **)&tree->leaf_prim_ids);
    lv00_free((void **)&tree->nodes);
    lv00_free((void **)&tree->primitives);
    lv00_free((void **)&tree);
}

/**
 * @brief 2D 射线查询 —— 找到第一个相交的几何体
 *
 * 使用 slab method 检测射线与 AABB 的相交性，
 * 递归遍历树结构，返回 t 值最小的命中结果。
 *
 * @param tree  AABB 树
 * @param ray   射线
 * @return 射线命中结果（hit=false 表示未命中）
 */
LV00_PUBLIC_API Lv00AABBRayHit lv00_aabb2d_ray_query(
    const Lv00AABBTree2D *tree, Lv00AABBRay2D ray)
{
    Lv00AABBRayHit result;
    result.hit = false;
    result.t = DBL_MAX;
    result.primitive_id = AABB_INVALID_NODE;

    if (!tree || tree->root == AABB_INVALID_NODE) return result;

    aabb2d_ray_recursive(tree, tree->root, ray, 0.0, DBL_MAX, &result);
    return result;
}

/**
 * @brief 2D 最近邻查询
 *
 * 查找距离查询点 (px, py) 最近的几何体。
 * 使用递归遍历 + AABB 距离剪枝。
 *
 * @param tree AABB 树
 * @param px, py 查询点坐标
 * @return 最近邻结果（distance=DBL_MAX 表示树为空）
 */
LV00_PUBLIC_API Lv00AABBNearestResult lv00_aabb2d_nearest(
    const Lv00AABBTree2D *tree, double px, double py)
{
    Lv00AABBNearestResult result;
    result.primitive_id = AABB_INVALID_NODE;
    result.distance     = DBL_MAX;
    result.closest_x    = 0.0;
    result.closest_y    = 0.0;
    result.closest_z    = 0.0;

    if (!tree || tree->root == AABB_INVALID_NODE) return result;

    aabb2d_nearest_recursive(tree, tree->root, px, py, &result);
    return result;
}

/**
 * @brief 2D 范围查询 —— 找到所有与包围盒相交的几何体
 *
 * @param tree    AABB 树
 * @param query   查询包围盒
 * @param result  输出结果（需用 lv00_aabb_query_result_free 释放）
 */
LV00_PUBLIC_API void lv00_aabb2d_range_query(
    const Lv00AABBTree2D *tree, Lv00AABB2D query,
    Lv00AABBQueryResult *result)
{
    if (!tree || !result || tree->root == AABB_INVALID_NODE) return;

    aabb2d_range_recursive(tree, tree->root, query, result);
}

/**
 * @brief 2D 点查询 —— 找到包含指定点的所有几何体
 *
 * @param tree AABB 树
 * @param px, py 查询点
 * @param result 输出结果
 */
LV00_PUBLIC_API void lv00_aabb2d_point_query(
    const Lv00AABBTree2D *tree, double px, double py,
    Lv00AABBQueryResult *result)
{
    if (!tree || !result || tree->root == AABB_INVALID_NODE) return;

    aabb2d_point_recursive(tree, tree->root, px, py, result);
}

/**
 * @brief 获取 2D AABB 树的根包围盒
 */
LV00_PUBLIC_API Lv00AABB2D lv00_aabb2d_root_bbox(const Lv00AABBTree2D *tree)
{
    Lv00AABB2D bb = lv00_aabb2d_empty();
    if (!tree || tree->root == AABB_INVALID_NODE) return bb;

    const Lv00AABBNode *root = &tree->nodes[tree->root];
    bb.xmin = root->bbox.xmin;
    bb.ymin = root->bbox.ymin;
    bb.xmax = root->bbox.xmax;
    bb.ymax = root->bbox.ymax;
    return bb;
}

/**
 * @brief 获取 2D AABB 树统计信息
 *
 * @param tree            AABB 树
 * @param out_node_count  输出节点总数
 * @param out_depth       输出树深度
 * @param out_leaf_count  输出叶子节点数
 */
LV00_PUBLIC_API void lv00_aabb2d_stats(const Lv00AABBTree2D *tree,
    int *out_node_count, int *out_depth, int *out_leaf_count)
{
    if (!tree) {
        if (out_node_count) *out_node_count = 0;
        if (out_depth)      *out_depth      = 0;
        if (out_leaf_count) *out_leaf_count = 0;
        return;
    }

    if (out_node_count) *out_node_count = tree->node_count;
    if (out_depth)      *out_depth      = aabb_tree_depth(tree->nodes,
                                                            tree->root);
    if (out_leaf_count) *out_leaf_count = aabb_tree_leaf_count(tree->nodes,
                                                                tree->root);
}

/* ========================================================================
 * 第五部分：AABB 树构建与查询 API —— 3D
 * ======================================================================== */

/**
 * @brief 构建 3D AABB 树
 *
 * 使用自顶向下的中位数分裂策略构建平衡的 AABB 树。
 *
 * @param bboxes    几何体包围盒数组
 * @param count     几何体数量
 * @param config    配置（NULL 使用默认配置）
 * @return AABB 树指针（需用 lv00_aabb3d_destroy 释放）
 */
LV00_PUBLIC_API Lv00AABBTree3D *lv00_aabb3d_build(
    const Lv00AABB3D *bboxes, int count,
    const Lv00AABBTreeConfig *config)
{
    if (!bboxes || count <= 0) return NULL;

    /* 分配树结构 */
    Lv00AABBTree3D *tree = (Lv00AABBTree3D *)lv00_malloc(sizeof(Lv00AABBTree3D));
    if (!tree) return NULL;

    /* 初始化 */
    tree->nodes          = NULL;
    tree->node_count     = 0;
    tree->node_capacity  = 0;
    tree->root           = AABB_INVALID_NODE;
    tree->primitive_count = count;

    /* 设置配置 */
    if (config) {
        tree->config = *config;
    } else {
        tree->config = lv00_aabb_tree_default_config();
    }

    /* 拷贝几何体包围盒 */
    tree->primitives = (Lv00AABB3D *)lv00_malloc((size_t)count * sizeof(Lv00AABB3D));
    if (!tree->primitives) {
        lv00_free((void **)&tree);
        return NULL;
    }
    memcpy(tree->primitives, bboxes, (size_t)count * sizeof(Lv00AABB3D));

    /* 创建几何体索引数组 */
    int *prim_indices = (int *)lv00_malloc((size_t)count * sizeof(int));
    if (!prim_indices) {
        lv00_free((void **)&tree->primitives);
        lv00_free((void **)&tree);
        return NULL;
    }
    for (int i = 0; i < count; i++) {
        prim_indices[i] = i;
    }

    /* 递归构建 */
    tree->root = aabb3d_build_recursive(tree, prim_indices, count, 0);

    lv00_free((void **)&prim_indices);

    if (tree->root == AABB_INVALID_NODE) {
        lv00_free((void **)&tree->primitives);
        lv00_free((void **)&tree->nodes);
        lv00_free((void **)&tree);
        return NULL;
    }

    return tree;
}

/**
 * @brief 释放 3D AABB 树
 */
LV00_PUBLIC_API void lv00_aabb3d_destroy(Lv00AABBTree3D *tree)
{
    if (!tree) return;
    lv00_free((void **)&tree->nodes);
    lv00_free((void **)&tree->primitives);
    lv00_free((void **)&tree);
}

/**
 * @brief 3D 射线查询
 *
 * 使用 slab method 检测射线与 AABB 的相交性，
 * 递归遍历树结构，返回 t 值最小的命中结果。
 *
 * @param tree  AABB 树
 * @param ray   射线
 * @return 射线命中结果
 */
LV00_PUBLIC_API Lv00AABBRayHit lv00_aabb3d_ray_query(
    const Lv00AABBTree3D *tree, Lv00AABBRay3D ray)
{
    Lv00AABBRayHit result;
    result.hit = false;
    result.t = DBL_MAX;
    result.primitive_id = AABB_INVALID_NODE;

    if (!tree || tree->root == AABB_INVALID_NODE) return result;

    aabb3d_ray_recursive(tree, tree->root, ray, 0.0, DBL_MAX, &result);
    return result;
}

/**
 * @brief 3D 最近邻查询
 *
 * @param tree AABB 树
 * @param px, py, pz 查询点坐标
 * @return 最近邻结果
 */
LV00_PUBLIC_API Lv00AABBNearestResult lv00_aabb3d_nearest(
    const Lv00AABBTree3D *tree, double px, double py, double pz)
{
    Lv00AABBNearestResult result;
    result.primitive_id = AABB_INVALID_NODE;
    result.distance     = DBL_MAX;
    result.closest_x    = 0.0;
    result.closest_y    = 0.0;
    result.closest_z    = 0.0;

    if (!tree || tree->root == AABB_INVALID_NODE) return result;

    aabb3d_nearest_recursive(tree, tree->root, px, py, pz, &result);
    return result;
}

/**
 * @brief 3D 范围查询
 *
 * @param tree    AABB 树
 * @param query   查询包围盒
 * @param result  输出结果
 */
LV00_PUBLIC_API void lv00_aabb3d_range_query(
    const Lv00AABBTree3D *tree, Lv00AABB3D query,
    Lv00AABBQueryResult *result)
{
    if (!tree || !result || tree->root == AABB_INVALID_NODE) return;

    aabb3d_range_recursive(tree, tree->root, query, result);
}

/**
 * @brief 3D 点查询
 *
 * @param tree AABB 树
 * @param px, py, pz 查询点
 * @param result 输出结果
 */
LV00_PUBLIC_API void lv00_aabb3d_point_query(
    const Lv00AABBTree3D *tree, double px, double py, double pz,
    Lv00AABBQueryResult *result)
{
    if (!tree || !result || tree->root == AABB_INVALID_NODE) return;

    aabb3d_point_recursive(tree, tree->root, px, py, pz, result);
}

/**
 * @brief 获取 3D AABB 树的根包围盒
 */
LV00_PUBLIC_API Lv00AABB3D lv00_aabb3d_root_bbox(const Lv00AABBTree3D *tree)
{
    Lv00AABB3D bb = lv00_aabb3d_empty();
    if (!tree || tree->root == AABB_INVALID_NODE) return bb;

    return tree->nodes[tree->root].bbox;
}
