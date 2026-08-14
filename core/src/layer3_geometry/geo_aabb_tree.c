/**
 * @file geo_aabb_tree.c
 * @brief AABB 树空间索引 —— 已拆分为独立模块
 *
 * 原始单体实现已按职责拆分为以下模块：
 *   - aabb_box.c     包围盒基础操作（2D / 3D）
 *   - aabb_common.c  查询结果管理与内部公共工具
 *   - aabb_tree_2d.c 2D AABB 树构建与查询
 *   - aabb_tree_3d.c 3D AABB 树构建与查询
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

#include "lv/geo_aabb_tree.h"
#include "aabb_internal.h"

#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "lv/geometry_config.h"
#include "lv/lv_internal.h"
#include "lv/lv_utils.h"

/* 如果 geometry_config.h 中没有定义 lv_PUBLIC_API，则定义空宏 */
#ifndef lv_PUBLIC_API
#define lv_PUBLIC_API
#endif
