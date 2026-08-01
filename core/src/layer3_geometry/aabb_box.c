/**
 * @file aabb_box.c
 * @brief AABB 包围盒基础操作（2D / 3D）
 *
 * 从 geo_aabb_tree.c 拆分的模块之一：
 *   - aabb_box.c     包围盒基础操作
 *   - aabb_common.c  查询结果管理与内部公共工具
 *   - aabb_tree_2d.c 2D AABB 树构建与查询
 *   - aabb_tree_3d.c 3D AABB 树构建与查询
 *
 * @version v3.6.0
 */

#include "lv/geo_aabb_tree.h"

#include <float.h>
#include <stdbool.h>
/* ========================================================================
 * 第一部分：包围盒基础操作 —— 2D
 * ======================================================================== */

/**
 * @brief 创建空的 2D 包围盒
 *
 * 空包围盒用 +INF / -INF 标记，表示不包含任何点。
 */
lv_PUBLIC_API lvAABB2D lv_aabb2d_empty(void) {
    lvAABB2D bb;
    bb.xmin = DBL_MAX;
    bb.ymin = DBL_MAX;
    bb.xmax = -DBL_MAX;
    bb.ymax = -DBL_MAX;
    return bb;
}

/**
 * @brief 创建包含单个点的 2D 包围盒
 */
lv_PUBLIC_API lvAABB2D lv_aabb2d_point(double x, double y) {
    lvAABB2D bb;
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
lv_PUBLIC_API lvAABB2D lv_aabb2d_merge(lvAABB2D a, lvAABB2D b) {
    lvAABB2D result;
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
lv_PUBLIC_API bool lv_aabb2d_is_valid(lvAABB2D bb) {
    return bb.xmin <= bb.xmax && bb.ymin <= bb.ymax;
}

/**
 * @brief 判定点是否在 2D 包围盒内（含边界）
 */
lv_PUBLIC_API bool lv_aabb2d_contains(lvAABB2D bb, double x, double y) {
    return x >= bb.xmin && x <= bb.xmax && y >= bb.ymin && y <= bb.ymax;
}

/**
 * @brief 判定两个 2D 包围盒是否相交
 *
 * 两个 AABB 相交当且仅当它们在所有轴上都有重叠区间。
 */
lv_PUBLIC_API bool lv_aabb2d_intersects(lvAABB2D a, lvAABB2D b) {
    return a.xmin <= b.xmax && a.xmax >= b.xmin && a.ymin <= b.ymax && a.ymax >= b.ymin;
}

/**
 * @brief 计算 2D 包围盒的面积
 *
 * 面积 = (xmax - xmin) * (ymax - ymin)
 * 空包围盒返回 0.0。
 */
lv_PUBLIC_API double lv_aabb2d_area(lvAABB2D bb) {
    if (!lv_aabb2d_is_valid(bb))
        return 0.0;
    return (bb.xmax - bb.xmin) * (bb.ymax - bb.ymin);
}

/**
 * @brief 计算 2D 包围盒的中心点
 */
lv_PUBLIC_API lvAABBPoint2D lv_aabb2d_center(lvAABB2D bb) {
    lvAABBPoint2D pt;
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
lv_PUBLIC_API lvAABB3D lv_aabb3d_empty(void) {
    lvAABB3D bb;
    bb.xmin = DBL_MAX;
    bb.ymin = DBL_MAX;
    bb.zmin = DBL_MAX;
    bb.xmax = -DBL_MAX;
    bb.ymax = -DBL_MAX;
    bb.zmax = -DBL_MAX;
    return bb;
}

/**
 * @brief 创建包含单个点的 3D 包围盒
 */
lv_PUBLIC_API lvAABB3D lv_aabb3d_point(double x, double y, double z) {
    lvAABB3D bb;
    bb.xmin = bb.xmax = x;
    bb.ymin = bb.ymax = y;
    bb.zmin = bb.zmax = z;
    return bb;
}

/**
 * @brief 合并两个 3D 包围盒
 */
lv_PUBLIC_API lvAABB3D lv_aabb3d_merge(lvAABB3D a, lvAABB3D b) {
    lvAABB3D result;
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
lv_PUBLIC_API bool lv_aabb3d_is_valid(lvAABB3D bb) {
    return bb.xmin <= bb.xmax && bb.ymin <= bb.ymax && bb.zmin <= bb.zmax;
}

/**
 * @brief 判定点是否在 3D 包围盒内（含边界）
 */
lv_PUBLIC_API bool lv_aabb3d_contains(lvAABB3D bb, double x, double y, double z) {
    return x >= bb.xmin && x <= bb.xmax && y >= bb.ymin && y <= bb.ymax && z >= bb.zmin && z <= bb.zmax;
}

/**
 * @brief 判定两个 3D 包围盒是否相交
 */
lv_PUBLIC_API bool lv_aabb3d_intersects(lvAABB3D a, lvAABB3D b) {
    return a.xmin <= b.xmax && a.xmax >= b.xmin && a.ymin <= b.ymax && a.ymax >= b.ymin && a.zmin <= b.zmax &&
           a.zmax >= b.zmin;
}

/**
 * @brief 计算 3D 包围盒的表面积
 *
 * 表面积 = 2 * (xy面 + yz面 + zx面)
 */
lv_PUBLIC_API double lv_aabb3d_surface_area(lvAABB3D bb) {
    if (!lv_aabb3d_is_valid(bb))
        return 0.0;
    double dx = bb.xmax - bb.xmin;
    double dy = bb.ymax - bb.ymin;
    double dz = bb.zmax - bb.zmin;
    return 2.0 * (dx * dy + dy * dz + dz * dx);
}

/**
 * @brief 计算 3D 包围盒的体积
 */
lv_PUBLIC_API double lv_aabb3d_volume(lvAABB3D bb) {
    if (!lv_aabb3d_is_valid(bb))
        return 0.0;
    return (bb.xmax - bb.xmin) * (bb.ymax - bb.ymin) * (bb.zmax - bb.zmin);
}

/**
 * @brief 计算 3D 包围盒的中心点
 */
lv_PUBLIC_API lvAABBPoint3D lv_aabb3d_center(lvAABB3D bb) {
    lvAABBPoint3D pt;
    pt.x = (bb.xmin + bb.xmax) * 0.5;
    pt.y = (bb.ymin + bb.ymax) * 0.5;
    pt.z = (bb.zmin + bb.zmax) * 0.5;
    return pt;
}
