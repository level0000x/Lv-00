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
/**
 * @brief 为 3D 树分配一个新节点，返回节点索引
 *
 * 内部复用 lvAABBNode 结构，扩容策略与 2D 版本一致（2 倍扩容）。
 */
static int aabb3d_node_alloc(lvAABBTree3D *tree) {
    if (tree->node_count >= tree->node_capacity) {
        int new_cap = (tree->node_capacity > 0) ? tree->node_capacity * 2 : AABB_INITIAL_CAPACITY;
        lvAABBNode *new_nodes = (lvAABBNode *) lv_realloc(tree->nodes, (size_t) new_cap * sizeof(lvAABBNode));
        if (!new_nodes)
            return AABB_INVALID_NODE;
        tree->nodes = new_nodes;
        tree->node_capacity = new_cap;
    }
    int idx = tree->node_count++;
    memset(&tree->nodes[idx], 0, sizeof(lvAABBNode));
    tree->nodes[idx].left = AABB_INVALID_NODE;
    tree->nodes[idx].right = AABB_INVALID_NODE;
    tree->nodes[idx].primitive_id = AABB_INVALID_NODE;
    tree->nodes[idx].height = 0;
    return idx;
}

/**
 * @brief 将 3D 几何体索引数组按指定轴的中心坐标排序
 */
static void sort_primitives_3d(const lvAABBTree3D *tree, int *indices, int count, int axis) {
    for (int i = 0; i < count - 1; i++) {
        int min_idx = i;
        double min_val = 0.0;
        {
            const lvAABB3D *bb = &tree->primitives[indices[i]];
            switch (axis) {
                case 0:
                    min_val = bb->xmin + bb->xmax;
                    break;
                case 1:
                    min_val = bb->ymin + bb->ymax;
                    break;
                default:
                    min_val = bb->zmin + bb->zmax;
                    break;
            }
        }
        for (int j = i + 1; j < count; j++) {
            const lvAABB3D *bb = &tree->primitives[indices[j]];
            double val;
            switch (axis) {
                case 0:
                    val = bb->xmin + bb->xmax;
                    break;
                case 1:
                    val = bb->ymin + bb->ymax;
                    break;
                default:
                    val = bb->zmin + bb->zmax;
                    break;
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
 * 3D 递归构建
 * ----------------------------------------------------------------------- */

/**
 * @brief 递归构建 3D AABB 树（自顶向下中位数分裂）
 *
 * 沿跨度最大的轴将几何体按中心坐标排序后对半分割，
 * 递归构建左右子树，最终形成平衡二叉树。
 *
 * @param tree        AABB 树
 * @param prim_indices 几何体索引数组（会被重排）
 * @param count       当前子集的几何体数量
 * @param depth       当前递归深度
 * @return 新创建的节点索引
 */
static int aabb3d_build_recursive(lvAABBTree3D *tree, int *prim_indices, int count, int depth) {
    int node_idx = aabb3d_node_alloc(tree);
    if (node_idx == AABB_INVALID_NODE)
        return AABB_INVALID_NODE;

    /* 计算当前子集所有几何体的联合包围盒，存为该节点的 bbox */
    lvAABB3D node_bbox = lv_aabb3d_empty();
    for (int i = 0; i < count; i++) {
        node_bbox = lv_aabb3d_merge(node_bbox, tree->primitives[prim_indices[i]]);
    }
    tree->nodes[node_idx].bbox = node_bbox;

    /* 终止条件：几何体足够少或已达最大深度，停止分裂成为叶子节点 */
    if (count <= tree->config.max_leaf_size || depth >= tree->config.max_depth) {
        tree->nodes[node_idx].primitive_id = prim_indices[0];
        tree->nodes[node_idx].left = AABB_INVALID_NODE;
        tree->nodes[node_idx].right = AABB_INVALID_NODE;
        tree->nodes[node_idx].height = 0;
        return node_idx;
    }

    /* 选择分裂轴：跨度最大的轴有助于减少子树包围盒之间的重叠 */
    double span_x = node_bbox.xmax - node_bbox.xmin;
    double span_y = node_bbox.ymax - node_bbox.ymin;
    double span_z = node_bbox.zmax - node_bbox.zmin;
    int split_axis = 0;
    if (span_y >= span_x && span_y >= span_z)
        split_axis = 1;
    else if (span_z >= span_x && span_z >= span_y)
        split_axis = 2;

    /* 沿分裂轴按中心坐标排序，使相近的几何体在数组中相邻 */
    sort_primitives_3d(tree, prim_indices, count, split_axis);

    /* 中位数分裂：将排序后的数组从中间切分，构建平衡树 */
    int mid = count / 2;

    /* 递归构建左右子树 */
    int left_idx = aabb3d_build_recursive(tree, prim_indices, mid, depth + 1);
    int right_idx = aabb3d_build_recursive(tree, prim_indices + mid, count - mid, depth + 1);

    tree->nodes[node_idx].left = left_idx;
    tree->nodes[node_idx].right = right_idx;
    tree->nodes[node_idx].primitive_id = AABB_INVALID_NODE;

    /* 高度 = max(左子树高度, 右子树高度) + 1，用于树深度统计 */
    int lh = (left_idx != AABB_INVALID_NODE) ? tree->nodes[left_idx].height : 0;
    int rh = (right_idx != AABB_INVALID_NODE) ? tree->nodes[right_idx].height : 0;
    tree->nodes[node_idx].height = ((lh > rh) ? lh : rh) + 1;

    return node_idx;
}

/**
 * @brief 3D 射线与 AABB 相交检测（Slab Method）
 *
 * Slab method 将 AABB 视为三组平行平面围成的区域，对每组平面计算
 * 射线的进入参数 t1 和退出参数 t2，通过逐轴取交集（tmin = max, tmax = min）
 * 得到射线在 AABB 内的整体区间。若最终 tmin > tmax 则不相交。
 *
 * @param bb    包围盒
 * @param ray   射线
 * @param tmin  射线参数下界（会被收窄）
 * @param tmax  射线参数上界（会被收窄）
 * @return 射线在 [tmin, tmax] 范围内是否与 bb 相交
 */
static bool aabb3d_ray_intersect(lvAABB3D bb, lvAABBRay3D ray, double tmin, double tmax) {
    /* X 轴 slab：计算射线与 x=xmin 和 x=xmax 两平面的交点参数 */
    if (fabs(ray.dx) < DBL_EPSILON) {
        /* 射线平行于 X 轴平面：仅当射线起点在 slab 内才可能相交 */
        if (ray.ox < bb.xmin || ray.ox > bb.xmax)
            return false;
    } else {
        double inv_d = 1.0 / ray.dx;
        double t1 = (bb.xmin - ray.ox) * inv_d;
        double t2 = (bb.xmax - ray.ox) * inv_d;
        if (t1 > t2) {
            double tmp = t1;
            t1 = t2;
            t2 = tmp;
        }
        if (t1 > tmin)
            tmin = t1; /* 取各轴进入参数的最大值 */
        if (t2 < tmax)
            tmax = t2; /* 取各轴退出参数的最小值 */
        if (tmin > tmax)
            return false; /* 区间已空，提前退出 */
    }

    /* Y 轴 slab */
    if (fabs(ray.dy) < DBL_EPSILON) {
        if (ray.oy < bb.ymin || ray.oy > bb.ymax)
            return false;
    } else {
        double inv_d = 1.0 / ray.dy;
        double t1 = (bb.ymin - ray.oy) * inv_d;
        double t2 = (bb.ymax - ray.oy) * inv_d;
        if (t1 > t2) {
            double tmp = t1;
            t1 = t2;
            t2 = tmp;
        }
        if (t1 > tmin)
            tmin = t1;
        if (t2 < tmax)
            tmax = t2;
        if (tmin > tmax)
            return false;
    }

    /* Z 轴 slab */
    if (fabs(ray.dz) < DBL_EPSILON) {
        if (ray.oz < bb.zmin || ray.oz > bb.zmax)
            return false;
    } else {
        double inv_d = 1.0 / ray.dz;
        double t1 = (bb.zmin - ray.oz) * inv_d;
        double t2 = (bb.zmax - ray.oz) * inv_d;
        if (t1 > t2) {
            double tmp = t1;
            t1 = t2;
            t2 = tmp;
        }
        if (t1 > tmin)
            tmin = t1;
        if (t2 < tmax)
            tmax = t2;
        if (tmin > tmax)
            return false;
    }

    return true;
}

/**
 * @brief 计算 3D 点到 AABB 的最近距离平方
 *
 * 对每条轴独立计算点到边界的距离，原理与 2D 版本相同。
 */
static double aabb3d_point_distance_sq(lvAABB3D bb, double px, double py, double pz) {
    double dx = 0.0, dy = 0.0, dz = 0.0;

    if (px < bb.xmin)
        dx = bb.xmin - px;
    else if (px > bb.xmax)
        dx = px - bb.xmax;

    if (py < bb.ymin)
        dy = bb.ymin - py;
    else if (py > bb.ymax)
        dy = py - bb.ymax;

    if (pz < bb.zmin)
        dz = bb.zmin - pz;
    else if (pz > bb.zmax)
        dz = pz - bb.zmax;

    return dx * dx + dy * dy + dz * dz;
}

/**
 * @brief 计算 3D 点到 AABB 的最近距离
 */
static double aabb3d_point_distance(lvAABB3D bb, double px, double py, double pz) {
    return sqrt(aabb3d_point_distance_sq(bb, px, py, pz));
}

/**
 * @brief 计算 3D 点到 3D AABB 的最近点坐标
 *
 * 原理与 2D 版本相同：点在各轴上被夹持到最近的 AABB 边界。
 */
static lvAABBPoint3D aabb3d_closest_point(lvAABB3D bb, double px, double py, double pz) {
    lvAABBPoint3D cp;
    cp.x = (px < bb.xmin) ? bb.xmin : (px > bb.xmax) ? bb.xmax : px;
    cp.y = (py < bb.ymin) ? bb.ymin : (py > bb.ymax) ? bb.ymax : py;
    cp.z = (pz < bb.zmin) ? bb.zmin : (pz > bb.zmax) ? bb.zmax : pz;
    return cp;
}

/**
 * @brief 3D 射线递归查询
 *
 * 先检测节点 AABB 与射线的相交性进行剪枝，叶子节点则进一步
 * 计算精确的相交参数 t，保存最近（t 最小）的命中。
 */
static void aabb3d_ray_recursive(const lvAABBTree3D *tree, int node_idx, lvAABBRay3D ray, double tmin, double tmax,
                                 lvAABBRayHit *best) {
    if (node_idx == AABB_INVALID_NODE)
        return;
    /* 剪枝：当前 tmin 已超过已知最佳命中，后续 slab 只会使 tmin 更大 */
    if (tmin > best->t)
        return;

    const lvAABBNode *node = &tree->nodes[node_idx];

    /* 叶子节点：进行精确的射线-几何体相交检测 */
    if (node->left == AABB_INVALID_NODE && node->right == AABB_INVALID_NODE) {
        if (node->primitive_id == AABB_INVALID_NODE)
            return;

        const lvAABB3D *prim_bb = &tree->primitives[node->primitive_id];
        if (aabb3d_ray_intersect(*prim_bb, ray, tmin, tmax)) {
            /* 用 slab method 重新计算精确的相交参数 t0 */
            double t0 = 0.0, t1 = DBL_MAX;

            /* X slab */
            if (fabs(ray.dx) > DBL_EPSILON) {
                double inv_d = 1.0 / ray.dx;
                double tx1 = (prim_bb->xmin - ray.ox) * inv_d;
                double tx2 = (prim_bb->xmax - ray.ox) * inv_d;
                if (tx1 > tx2) {
                    double tmp = tx1;
                    tx1 = tx2;
                    tx2 = tmp;
                }
                if (tx1 > t0)
                    t0 = tx1; /* 各轴进入参数取最大 */
                if (tx2 < t1)
                    t1 = tx2; /* 各轴退出参数取最小 */
            } else {
                if (ray.ox < prim_bb->xmin || ray.ox > prim_bb->xmax)
                    return;
            }

            /* Y slab */
            if (fabs(ray.dy) > DBL_EPSILON) {
                double inv_d = 1.0 / ray.dy;
                double ty1 = (prim_bb->ymin - ray.oy) * inv_d;
                double ty2 = (prim_bb->ymax - ray.oy) * inv_d;
                if (ty1 > ty2) {
                    double tmp = ty1;
                    ty1 = ty2;
                    ty2 = tmp;
                }
                if (ty1 > t0)
                    t0 = ty1;
                if (ty2 < t1)
                    t1 = ty2;
            } else {
                if (ray.oy < prim_bb->ymin || ray.oy > prim_bb->ymax)
                    return;
            }

            /* Z slab */
            if (fabs(ray.dz) > DBL_EPSILON) {
                double inv_d = 1.0 / ray.dz;
                double tz1 = (prim_bb->zmin - ray.oz) * inv_d;
                double tz2 = (prim_bb->zmax - ray.oz) * inv_d;
                if (tz1 > tz2) {
                    double tmp = tz1;
                    tz1 = tz2;
                    tz2 = tmp;
                }
                if (tz1 > t0)
                    t0 = tz1;
                if (tz2 < t1)
                    t1 = tz2;
            } else {
                if (ray.oz < prim_bb->zmin || ray.oz > prim_bb->zmax)
                    return;
            }

            /* 有效区间且 t0 < best->t 时更新 */
            if (t0 <= t1 && t0 >= 0.0 && t0 < best->t) {
                best->hit = true;
                best->t = t0;
                best->primitive_id = node->primitive_id;
            }
        }
        return;
    }

    /* 内部节点：检测节点 AABB 与射线的相交性，不相交则跳过子树 */
    if (!aabb3d_ray_intersect(node->bbox, ray, tmin, tmax))
        return;

    aabb3d_ray_recursive(tree, node->left, ray, tmin, tmax, best);
    aabb3d_ray_recursive(tree, node->right, ray, tmin, tmax, best);
}

/**
 * @brief 3D 最近邻递归查询（线性搜索 + AABB 距离剪枝）
 *
 * 计算查询点到节点 AABB 的最近距离进行剪枝，
 * 内部节点优先遍历距离更近的子树以提高剪枝效率。
 */
static void aabb3d_nearest_recursive(const lvAABBTree3D *tree, int node_idx, double px, double py, double pz,
                                     lvAABBNearestResult *best) {
    if (node_idx == AABB_INVALID_NODE)
        return;

    const lvAABBNode *node = &tree->nodes[node_idx];

    /* 剪枝：如果点到节点 AABB 的距离已经 ≥ 已知最近距离，无需进入该子树 */
    double dist_to_node = aabb3d_point_distance(node->bbox, px, py, pz);
    if (dist_to_node > best->distance)
        return;

    /* 叶子节点：计算点到所有包含的几何体的精确距离 */
    if (node->left == AABB_INVALID_NODE && node->right == AABB_INVALID_NODE) {
        if (node->primitive_id == AABB_INVALID_NODE)
            return;

        const lvAABB3D *prim_bb = &tree->primitives[node->primitive_id];
        lvAABBPoint3D cp = aabb3d_closest_point(*prim_bb, px, py, pz);
        double dx = px - cp.x;
        double dy = py - cp.y;
        double dz = pz - cp.z;
        double dist = sqrt(dx * dx + dy * dy + dz * dz);

        if (dist < best->distance) {
            best->distance = dist;
            best->primitive_id = node->primitive_id;
            best->closest_x = cp.x;
            best->closest_y = cp.y;
            best->closest_z = cp.z;
        }
        return;
    }

    /* 内部节点：优先遍历距离更近的子树，使 best->distance 尽快减小以增强剪枝 */
    int first = node->left;
    int second = node->right;

    if (first != AABB_INVALID_NODE && second != AABB_INVALID_NODE) {
        double d_left = aabb3d_point_distance(tree->nodes[first].bbox, px, py, pz);
        double d_right = aabb3d_point_distance(tree->nodes[second].bbox, px, py, pz);
        if (d_right < d_left) {
            int tmp = first;
            first = second;
            second = tmp;
        }
    }

    aabb3d_nearest_recursive(tree, first, px, py, pz, best);
    aabb3d_nearest_recursive(tree, second, px, py, pz, best);
}

/**
 * @brief 3D 范围查询递归
 *
 * 原理与 2D 版本相同，使用 3D AABB 相交检测进行剪枝。
 */
static void aabb3d_range_recursive(const lvAABBTree3D *tree, int node_idx, lvAABB3D query, lvAABBQueryResult *result) {
    if (node_idx == AABB_INVALID_NODE)
        return;

    const lvAABBNode *node = &tree->nodes[node_idx];

    /* 剪枝：节点 AABB 与查询框不相交则跳过 */
    if (!lv_aabb3d_intersects(node->bbox, query))
        return;

    /* 叶子节点：精确检测几何体与查询框的相交性 */
    if (node->left == AABB_INVALID_NODE && node->right == AABB_INVALID_NODE) {
        if (node->primitive_id == AABB_INVALID_NODE)
            return;
        if (lv_aabb3d_intersects(tree->primitives[node->primitive_id], query)) {
            result_push_back(result, node->primitive_id);
        }
        return;
    }

    /* 递归遍历子树 */
    aabb3d_range_recursive(tree, node->left, query, result);
    aabb3d_range_recursive(tree, node->right, query, result);
}

/**
 * @brief 3D 点查询递归
 *
 * 原理与 2D 版本相同，使用 3D 点包含检测进行剪枝。
 */
static void aabb3d_point_recursive(const lvAABBTree3D *tree, int node_idx, double px, double py, double pz,
                                   lvAABBQueryResult *result) {
    if (node_idx == AABB_INVALID_NODE)
        return;

    const lvAABBNode *node = &tree->nodes[node_idx];

    /* 剪枝：点不在节点 AABB 内则跳过 */
    if (!lv_aabb3d_contains(node->bbox, px, py, pz))
        return;

    /* 叶子节点：精确检测几何体是否包含该点 */
    if (node->left == AABB_INVALID_NODE && node->right == AABB_INVALID_NODE) {
        if (node->primitive_id == AABB_INVALID_NODE)
            return;
        if (lv_aabb3d_contains(tree->primitives[node->primitive_id], px, py, pz)) {
            result_push_back(result, node->primitive_id);
        }
        return;
    }

    /* 递归遍历子树 */
    aabb3d_point_recursive(tree, node->left, px, py, pz, result);
    aabb3d_point_recursive(tree, node->right, px, py, pz, result);
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
 * @return AABB 树指针（需用 lv_aabb3d_destroy 释放）
 */
lv_PUBLIC_API lvAABBTree3D *lv_aabb3d_build(const lvAABB3D *bboxes, int count, const lvAABBTreeConfig *config) {
    if (!bboxes || count <= 0)
        lv_RETURN_ERROR_NULL(lv_ERROR_INVALID_PARAM, "lv_aabb3d_build: invalid bboxes or count");

    /* 分配树结构 */
    lvAABBTree3D *tree = (lvAABBTree3D *) lv_malloc(sizeof(lvAABBTree3D));
    if (!tree)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "lv_aabb3d_build: malloc failed");

    /* 初始化 */
    tree->nodes = NULL;
    tree->node_count = 0;
    tree->node_capacity = 0;
    tree->root = AABB_INVALID_NODE;
    tree->primitive_count = count;

    /* 设置配置 */
    if (config) {
        tree->config = *config;
    } else {
        tree->config = lv_aabb_tree_default_config();
    }

    /* 拷贝几何体包围盒 */
    tree->primitives = (lvAABB3D *) lv_calloc((size_t) count, sizeof(lvAABB3D));
    if (!tree->primitives) {
        lv_free((void **) &(tree));
        return NULL;
    }
    memcpy(tree->primitives, bboxes, (size_t) count * sizeof(lvAABB3D));

    /* 创建几何体索引数组 */
    int *prim_indices = (int *) lv_malloc((size_t) count * sizeof(int));
    if (!prim_indices) {
        lv_free((void **) &(tree->primitives));
        lv_free((void **) &(tree));
        lv_ERROR_SET(lv_ERROR_ALLOCATION_FAILED, "lv_aabb3d_build: malloc prim_indices failed");
        return NULL;
    }
    for (int i = 0; i < count; i++) {
        prim_indices[i] = i;
    }

    /* 递归构建 */
    tree->root = aabb3d_build_recursive(tree, prim_indices, count, 0);

    lv_free((void **) &(prim_indices));

    if (tree->root == AABB_INVALID_NODE) {
        lv_free((void **) &(tree->primitives));
        lv_free((void **) &(tree->nodes));
        lv_free((void **) &(tree));
        lv_ERROR_SET(lv_ERROR_INTERNAL, "lv_aabb3d_build: recursive build failed");
        return NULL;
    }

    return tree;
}

/**
 * @brief 释放 3D AABB 树
 */
lv_PUBLIC_API void lv_aabb3d_destroy(lvAABBTree3D *tree) {
    if (!tree)
        return;
    lv_free((void **) &(tree->nodes));
    lv_free((void **) &(tree->primitives));
    lv_free((void **) &(tree));
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
lv_PUBLIC_API lvAABBRayHit lv_aabb3d_ray_query(const lvAABBTree3D *tree, lvAABBRay3D ray) {
    lvAABBRayHit result;
    result.hit = false;
    result.t = DBL_MAX;
    result.primitive_id = AABB_INVALID_NODE;

    if (!tree || tree->root == AABB_INVALID_NODE)
        return result;

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
lv_PUBLIC_API lvAABBNearestResult lv_aabb3d_nearest(const lvAABBTree3D *tree, double px, double py, double pz) {
    lvAABBNearestResult result;
    result.primitive_id = AABB_INVALID_NODE;
    result.distance = DBL_MAX;
    result.closest_x = 0.0;
    result.closest_y = 0.0;
    result.closest_z = 0.0;

    if (!tree || tree->root == AABB_INVALID_NODE)
        return result;

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
lv_PUBLIC_API void lv_aabb3d_range_query(const lvAABBTree3D *tree, lvAABB3D query, lvAABBQueryResult *result) {
    if (!tree || !result || tree->root == AABB_INVALID_NODE)
        return;

    aabb3d_range_recursive(tree, tree->root, query, result);
}

/**
 * @brief 3D 点查询
 *
 * @param tree AABB 树
 * @param px, py, pz 查询点
 * @param result 输出结果
 */
lv_PUBLIC_API void lv_aabb3d_point_query(const lvAABBTree3D *tree, double px, double py, double pz,
                                         lvAABBQueryResult *result) {
    if (!tree || !result || tree->root == AABB_INVALID_NODE)
        return;

    aabb3d_point_recursive(tree, tree->root, px, py, pz, result);
}

/**
 * @brief 获取 3D AABB 树的根包围盒
 */
lv_PUBLIC_API lvAABB3D lv_aabb3d_root_bbox(const lvAABBTree3D *tree) {
    lvAABB3D bb = lv_aabb3d_empty();
    if (!tree || tree->root == AABB_INVALID_NODE)
        return bb;

    return tree->nodes[tree->root].bbox;
}
