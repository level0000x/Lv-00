/**
 * @file aabb_tree_impl.h
 * @brief AABB 树模板实现（2D/3D 通用）
 *
 * exempt: 宏泛型（2D/3D 双簿记）。本文件通过宏参数被 aabb_tree_2d.c /
 *    aabb_tree_3d.c 各包含一次，同时产出两套 typed 实现；迁移为
 *    aabb2d/aabb3d 两套独立 typed 代码需同时改动两调用方 .c 与
 *    geo_aabb_tree.h（均超出 K5 白名单），风险与成本过高，列为遗留
 *    （迁移建议见 K5 报告第③条）。轴展开的 ray slab 段（ray_intersect
 *    与 ray_recursive）存在 `fabs(d)==DBL_EPSILON` 边界平行判定差异
 *    （< 与 >），不满足逐位等价前提，同样登记为遗留，不做本组内收敛。
 *
 * 通过宏参数控制维度差异，被 aabb_tree_2d.c 和 aabb_tree_3d.c 包含。
 *
 * 需要调用方在 include 之前定义以下宏：
 *   AABB_DIMS           - 轴数（2 或 3）
 *   AABB_PRIM_TYPE      - 原始体类型（lvAABB2D 或 lvAABB3D）
 *   AABB_TREE_TYPE      - 树类型（lvAABBTree2D 或 lvAABBTree3D）
 *   AABB_RAY_TYPE       - 射线类型（lvAABBRay2D 或 lvAABBRay3D）
 *   AABB_AXIS_CENTER(bb, axis)  - 轴中心值表达式
 *   AABB_AXIS_SWITCH(axis, body_x, body_y, body_z) - 按轴分派
 *   AABB_EMPTY()        - 空盒函数（节点 bbox 始终为 lvAABB3D，此宏用于节点 bbox）
 *   AABB_EMPTY_PRIM     - 原始体类型空盒表达式（lv_aabb2d_empty() / lv_aabb3d_empty()）
 *   AABB_MERGE(a, b)    - 合并盒函数
 *   AABB_PREFIX         - 函数名前缀（2d / 3d）
 *   AABB_NODE_RAY_INT(node, ray, tmin, tmax) - 内部节点射线检测
 *   AABB_STATS          - 是否包含 stats 函数（1 或 0）
 *   AABB_LEAF_MULTI     - 是否支持多面片叶子（1 或 0）
 */

#include "lv/lv_utils.h"
#include "lv/geo_utils.h"
#include "lv/lv_numeric.h"

/* ========================================================================
 * 宏连接辅助
 * ======================================================================== */
#define AABB__CONCAT2(a, b) a ## b
#define AABB__CONCAT(a, b) AABB__CONCAT2(a, b)
#define AABB__STR2(s) #s
#define AABB_STR(s) AABB__STR2(s)

/** 静态函数名 */
#define AABB_FUNC(name) AABB__CONCAT(aabb, AABB__CONCAT(AABB_PREFIX, _ ## name))

/** 公共 API 函数名（name 已包含前导下划线，如 _build） */
#define AABB_API(name) AABB__CONCAT(lv_aabb, AABB__CONCAT(AABB_PREFIX, name))

/* ========================================================================
 * node_alloc — 完全一致
 * ======================================================================== */
static int AABB_FUNC(node_alloc)(AABB_TREE_TYPE *tree) {
    /* Unified growth via lv_ensure_capacity (overflow-checked doubling; 0 -> unified initial capacity) */
    if (!lv_ensure_capacity((void **) &tree->nodes, tree->node_count, &tree->node_capacity, sizeof(lvAABBNode), 0))
        return AABB_INVALID_NODE;
    int idx = tree->node_count++;
    memset(&tree->nodes[idx], 0, sizeof(lvAABBNode));
    tree->nodes[idx].left = AABB_INVALID_NODE;
    tree->nodes[idx].right = AABB_INVALID_NODE;
    tree->nodes[idx].primitive_id = AABB_INVALID_NODE;
    tree->nodes[idx].height = 0;
    return idx;
}

/* ========================================================================
 * sort_primitives — 差异在轴数
 * ======================================================================== */
static void AABB_FUNC(sort_primitives)(const AABB_TREE_TYPE *tree, int *indices, int count, int axis) {
    for (int i = 0; i < count - 1; i++) {
        int min_idx = i;
        double min_val = 0.0;
        {
            const AABB_PRIM_TYPE *bb = &tree->primitives[indices[i]];
            AABB_AXIS_SWITCH(axis,
                min_val = bb->xmin + bb->xmax;,
                min_val = bb->ymin + bb->ymax;,
                min_val = bb->zmin + bb->zmax;
            )
        }
        for (int j = i + 1; j < count; j++) {
            const AABB_PRIM_TYPE *bb = &tree->primitives[indices[j]];
            double val;
            AABB_AXIS_SWITCH(axis,
                val = bb->xmin + bb->xmax;,
                val = bb->ymin + bb->ymax;,
                val = bb->zmin + bb->zmax;
            )
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

/* ========================================================================
 * build_recursive — 差异在 bbox 聚合方式、叶子存储
 * ======================================================================== */
static int AABB_FUNC(build_recursive)(AABB_TREE_TYPE *tree, int *prim_indices, int count, int depth) {
    int node_idx = AABB_FUNC(node_alloc)(tree);
    if (node_idx == AABB_INVALID_NODE)
        return AABB_INVALID_NODE;

    /* 计算当前子集的包围盒 */
    lvAABB3D node_bbox = AABB_EMPTY();
#if AABB_DIMS == 2
    for (int i = 0; i < count; i++) {
        const lvAABB2D *bb2d = &tree->primitives[prim_indices[i]];
        lvAABB3D bb3d;
        bb3d.xmin = bb2d->xmin;
        bb3d.ymin = bb2d->ymin;
        bb3d.zmin = 0.0;
        bb3d.xmax = bb2d->xmax;
        bb3d.ymax = bb2d->ymax;
        bb3d.zmax = 0.0;
        node_bbox = AABB_MERGE(node_bbox, bb3d);
    }
#elif AABB_DIMS == 3
    for (int i = 0; i < count; i++) {
        node_bbox = AABB_MERGE(node_bbox, tree->primitives[prim_indices[i]]);
    }
#endif
    tree->nodes[node_idx].bbox = node_bbox;

    /* 终止条件 */
    if (count <= tree->config.max_leaf_size || depth >= tree->config.max_depth) {
        tree->nodes[node_idx].left = AABB_INVALID_NODE;
        tree->nodes[node_idx].right = AABB_INVALID_NODE;
        tree->nodes[node_idx].height = 0;
        tree->nodes[node_idx].primitive_id = prim_indices[0];

#if AABB_LEAF_MULTI
        tree->nodes[node_idx].leaf_start = tree->leaf_prim_capacity;
        tree->nodes[node_idx].leaf_count = count;

        /* 扩展 leaf_prim_ids 容量 */
        int old_size = tree->leaf_prim_capacity;
        int needed = old_size + count;
        if (needed > tree->leaf_prim_capacity) {
            /* Unified growth via lv_ensure_capacity (overflow-checked doubling to >= needed) */
            if (!lv_ensure_capacity((void **) &tree->leaf_prim_ids, needed, &tree->leaf_prim_capacity,
                                    sizeof(int), 0))
                return AABB_INVALID_NODE;
        }

        for (int k = 0; k < count; k++) {
            tree->leaf_prim_ids[old_size + k] = prim_indices[k];
        }
#endif

        return node_idx;
    }

    /* 选择分裂轴：跨度最大的轴 */
#if AABB_DIMS == 2
    double span_x = node_bbox.xmax - node_bbox.xmin;
    double span_y = node_bbox.ymax - node_bbox.ymin;
    int split_axis = (span_x >= span_y) ? 0 : 1;
#elif AABB_DIMS == 3
    double span_x = node_bbox.xmax - node_bbox.xmin;
    double span_y = node_bbox.ymax - node_bbox.ymin;
    double span_z = node_bbox.zmax - node_bbox.zmin;
    int split_axis = 0;
    if (span_y >= span_x && span_y >= span_z)
        split_axis = 1;
    else if (span_z >= span_x && span_z >= span_y)
        split_axis = 2;
#endif

    AABB_FUNC(sort_primitives)(tree, prim_indices, count, split_axis);

    int mid = count / 2;

    int left_idx = AABB_FUNC(build_recursive)(tree, prim_indices, mid, depth + 1);
    int right_idx = AABB_FUNC(build_recursive)(tree, prim_indices + mid, count - mid, depth + 1);

    tree->nodes[node_idx].left = left_idx;
    tree->nodes[node_idx].right = right_idx;
    tree->nodes[node_idx].primitive_id = AABB_INVALID_NODE;

    int lh = (left_idx != AABB_INVALID_NODE) ? tree->nodes[left_idx].height : 0;
    int rh = (right_idx != AABB_INVALID_NODE) ? tree->nodes[right_idx].height : 0;
    tree->nodes[node_idx].height = ((lh > rh) ? lh : rh) + 1;

    return node_idx;
}

/* ========================================================================
 * ray_intersect — Slab Method
 * ======================================================================== */
static bool AABB_FUNC(ray_intersect)(AABB_PRIM_TYPE bb, AABB_RAY_TYPE ray, double tmin, double tmax) {
    /* X 轴 slab */
    if (fabs(ray.dx) < DBL_EPSILON) {
        if (ray.ox < bb.xmin || ray.ox > bb.xmax)
            return false;
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
        if (ray.oy < bb.ymin || ray.oy > bb.ymax)
            return false;
    } else {
        double inv_d = 1.0 / ray.dy;
        double t1 = (bb.ymin - ray.oy) * inv_d;
        double t2 = (bb.ymax - ray.oy) * inv_d;
        if (t1 > t2) { double tmp = t1; t1 = t2; t2 = tmp; }
        if (t1 > tmin) tmin = t1;
        if (t2 < tmax) tmax = t2;
        if (tmin > tmax) return false;
    }

#if AABB_DIMS == 3
    /* Z 轴 slab */
    if (fabs(ray.dz) < DBL_EPSILON) {
        if (ray.oz < bb.zmin || ray.oz > bb.zmax)
            return false;
    } else {
        double inv_d = 1.0 / ray.dz;
        double t1 = (bb.zmin - ray.oz) * inv_d;
        double t2 = (bb.zmax - ray.oz) * inv_d;
        if (t1 > t2) { double tmp = t1; t1 = t2; t2 = tmp; }
        if (t1 > tmin) tmin = t1;
        if (t2 < tmax) tmax = t2;
        if (tmin > tmax) return false;
    }
#endif

    return true;
}

/* ========================================================================
 * point_distance_sq / point_distance
 * ======================================================================== */
static double AABB_FUNC(point_distance_sq)(AABB_PRIM_TYPE bb, double px, double py
#if AABB_DIMS == 3
    , double pz
#endif
) {
    double dx = 0.0, dy = 0.0
#if AABB_DIMS == 3
        , dz = 0.0
#endif
        ;

    if (px < bb.xmin)
        dx = bb.xmin - px;
    else if (px > bb.xmax)
        dx = px - bb.xmax;

    if (py < bb.ymin)
        dy = bb.ymin - py;
    else if (py > bb.ymax)
        dy = py - bb.ymax;

#if AABB_DIMS == 3
    if (pz < bb.zmin)
        dz = bb.zmin - pz;
    else if (pz > bb.zmax)
        dz = pz - bb.zmax;
#endif

#if AABB_DIMS == 3
    return geo_norm_sq_3d(dx, dy, dz);
#else
    return geo_norm_sq_2d(dx, dy);
#endif
}

static double AABB_FUNC(point_distance)(AABB_PRIM_TYPE bb, double px, double py
#if AABB_DIMS == 3
    , double pz
#endif
) {
    return sqrt(AABB_FUNC(point_distance_sq)(bb, px, py
#if AABB_DIMS == 3
        , pz
#endif
    ));
}

/* ========================================================================
 * closest_point
 * ======================================================================== */
#if AABB_DIMS == 2
static lvAABBPoint2D AABB_FUNC(closest_point)(lvAABB2D bb, double px, double py) {
    lvAABBPoint2D cp;
    cp.x = lv_clamp(px, bb.xmin, bb.xmax);
    cp.y = lv_clamp(py, bb.ymin, bb.ymax);
    return cp;
}
#elif AABB_DIMS == 3
static lvAABBPoint3D AABB_FUNC(closest_point)(lvAABB3D bb, double px, double py, double pz) {
    lvAABBPoint3D cp;
    cp.x = lv_clamp(px, bb.xmin, bb.xmax);
    cp.y = lv_clamp(py, bb.ymin, bb.ymax);
    cp.z = lv_clamp(pz, bb.zmin, bb.zmax);
    return cp;
}
#endif

/* ========================================================================
 * ray_recursive
 * ======================================================================== */
static void AABB_FUNC(ray_recursive)(const AABB_TREE_TYPE *tree, int node_idx, AABB_RAY_TYPE ray, double tmin, double tmax,
                                     lvAABBRayHit *best) {
    if (node_idx == AABB_INVALID_NODE)
        return;
    if (tmin > best->t)
        return;

    const lvAABBNode *node = &tree->nodes[node_idx];

    /* 叶子节点 */
    if (node->left == AABB_INVALID_NODE && node->right == AABB_INVALID_NODE) {
        if (node->primitive_id == AABB_INVALID_NODE)
            return;

        const AABB_PRIM_TYPE *prim_bb = &tree->primitives[node->primitive_id];
        if (AABB_FUNC(ray_intersect)(*prim_bb, ray, tmin, tmax)) {
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
                if (ray.ox < prim_bb->xmin || ray.ox > prim_bb->xmax)
                    return;
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
                if (ray.oy < prim_bb->ymin || ray.oy > prim_bb->ymax)
                    return;
            }

#if AABB_DIMS == 3
            /* Z slab */
            if (fabs(ray.dz) > DBL_EPSILON) {
                double inv_d = 1.0 / ray.dz;
                double tz1 = (prim_bb->zmin - ray.oz) * inv_d;
                double tz2 = (prim_bb->zmax - ray.oz) * inv_d;
                if (tz1 > tz2) { double tmp = tz1; tz1 = tz2; tz2 = tmp; }
                if (tz1 > t0) t0 = tz1;
                if (tz2 < t1) t1 = tz2;
            } else {
                if (ray.oz < prim_bb->zmin || ray.oz > prim_bb->zmax)
                    return;
            }
#endif

            if (t0 <= t1 && t0 >= 0.0 && t0 < best->t) {
                best->hit = true;
                best->t = t0;
                best->primitive_id = node->primitive_id;
            }
        }
        return;
    }

    /* 内部节点：通过宏检测射线与节点 AABB 的相交 */
    if (!AABB_NODE_RAY_INT(node, ray, tmin, tmax))
        return;

    AABB_FUNC(ray_recursive)(tree, node->left, ray, tmin, tmax, best);
    AABB_FUNC(ray_recursive)(tree, node->right, ray, tmin, tmax, best);
}

/* ========================================================================
 * nearest_recursive
 * ======================================================================== */
static void AABB_FUNC(nearest_recursive)(const AABB_TREE_TYPE *tree, int node_idx, double px, double py
#if AABB_DIMS == 3
    , double pz
#endif
    , lvAABBNearestResult *best) {
    if (node_idx == AABB_INVALID_NODE)
        return;

    const lvAABBNode *node = &tree->nodes[node_idx];

    /* 剪枝 */
#if AABB_DIMS == 2
    lvAABB2D node_bb2d;
    node_bb2d.xmin = node->bbox.xmin;
    node_bb2d.ymin = node->bbox.ymin;
    node_bb2d.xmax = node->bbox.xmax;
    node_bb2d.ymax = node->bbox.ymax;
    double dist_to_node = aabb2d_point_distance(node_bb2d, px, py);
#elif AABB_DIMS == 3
    double dist_to_node = aabb3d_point_distance(node->bbox, px, py, pz);
#endif
    if (dist_to_node > best->distance)
        return;

    /* 叶子节点 */
    if (node->left == AABB_INVALID_NODE && node->right == AABB_INVALID_NODE) {
#if AABB_LEAF_MULTI
        int count = node->leaf_count;
        if (count <= 0)
            count = 1;
        for (int k = 0; k < count; k++) {
            int pid = (node->leaf_count > 0 && tree->leaf_prim_ids) ? tree->leaf_prim_ids[node->leaf_start + k]
                                                                    : node->primitive_id;
            if (!lv_index_in_range(pid, tree->primitive_count))
                continue;

            const lvAABB2D *prim_bb = &tree->primitives[pid];
            lvAABBPoint2D cp = aabb2d_closest_point(*prim_bb, px, py);
            double dist = geo_distance_2d(px, py, cp.x, cp.y);

            if (dist < best->distance) {
                best->distance = dist;
                best->primitive_id = pid;
                best->closest_x = cp.x;
                best->closest_y = cp.y;
                best->closest_z = 0.0;
            }
        }
#else
        if (node->primitive_id == AABB_INVALID_NODE)
            return;

        const AABB_PRIM_TYPE *prim_bb = &tree->primitives[node->primitive_id];
#if AABB_DIMS == 3
        lvAABBPoint3D cp = AABB_FUNC(closest_point)(*prim_bb, px, py, pz);
        double dist = geo_distance_3d(px, py, pz, cp.x, cp.y, cp.z);
#else
        lvAABBPoint2D cp = AABB_FUNC(closest_point)(*prim_bb, px, py);
        double dist = geo_distance_2d(px, py, cp.x, cp.y);
#endif

        if (dist < best->distance) {
            best->distance = dist;
            best->primitive_id = node->primitive_id;
            best->closest_x = cp.x;
            best->closest_y = cp.y;
            best->closest_z = cp.z;
        }
#endif
        return;
    }

    /* 内部节点：优先遍历距离更近的子树 */
    int first = node->left;
    int second = node->right;

    if (first != AABB_INVALID_NODE && second != AABB_INVALID_NODE) {
#if AABB_DIMS == 2
        lvAABB2D left_bb2d, right_bb2d;
        left_bb2d.xmin = tree->nodes[first].bbox.xmin;
        left_bb2d.ymin = tree->nodes[first].bbox.ymin;
        left_bb2d.xmax = tree->nodes[first].bbox.xmax;
        left_bb2d.ymax = tree->nodes[first].bbox.ymax;
        right_bb2d.xmin = tree->nodes[second].bbox.xmin;
        right_bb2d.ymin = tree->nodes[second].bbox.ymin;
        right_bb2d.xmax = tree->nodes[second].bbox.xmax;
        right_bb2d.ymax = tree->nodes[second].bbox.ymax;
        double d_left = aabb2d_point_distance(left_bb2d, px, py);
        double d_right = aabb2d_point_distance(right_bb2d, px, py);
#elif AABB_DIMS == 3
        double d_left = aabb3d_point_distance(tree->nodes[first].bbox, px, py, pz);
        double d_right = aabb3d_point_distance(tree->nodes[second].bbox, px, py, pz);
#endif
        if (d_right < d_left) {
            int tmp = first;
            first = second;
            second = tmp;
        }
    }

    AABB_FUNC(nearest_recursive)(tree, first, px, py
#if AABB_DIMS == 3
        , pz
#endif
        , best);
    AABB_FUNC(nearest_recursive)(tree, second, px, py
#if AABB_DIMS == 3
        , pz
#endif
        , best);
}

/* ========================================================================
 * range_recursive
 * ======================================================================== */
static void AABB_FUNC(range_recursive)(const AABB_TREE_TYPE *tree, int node_idx, AABB_PRIM_TYPE query, lvAABBQueryResult *result) {
    if (node_idx == AABB_INVALID_NODE)
        return;

    const lvAABBNode *node = &tree->nodes[node_idx];

    /* 剪枝 */
#if AABB_DIMS == 2
    lvAABB2D node_bb2d;
    node_bb2d.xmin = node->bbox.xmin;
    node_bb2d.ymin = node->bbox.ymin;
    node_bb2d.xmax = node->bbox.xmax;
    node_bb2d.ymax = node->bbox.ymax;
    if (!lv_aabb2d_intersects(node_bb2d, query))
        return;
#elif AABB_DIMS == 3
    if (!lv_aabb3d_intersects(node->bbox, query))
        return;
#endif

    /* 叶子节点 */
    if (node->left == AABB_INVALID_NODE && node->right == AABB_INVALID_NODE) {
        if (node->primitive_id == AABB_INVALID_NODE)
            return;
        if (AABB_API(_intersects)(tree->primitives[node->primitive_id], query)) {
            result_push_back(result, node->primitive_id);
        }
        return;
    }

    AABB_FUNC(range_recursive)(tree, node->left, query, result);
    AABB_FUNC(range_recursive)(tree, node->right, query, result);
}

/* ========================================================================
 * point_recursive
 * ======================================================================== */
static void AABB_FUNC(point_recursive)(const AABB_TREE_TYPE *tree, int node_idx, double px, double py
#if AABB_DIMS == 3
    , double pz
#endif
    , lvAABBQueryResult *result) {
    if (node_idx == AABB_INVALID_NODE)
        return;

    const lvAABBNode *node = &tree->nodes[node_idx];

    /* 剪枝 */
#if AABB_DIMS == 2
    lvAABB2D node_bb2d;
    node_bb2d.xmin = node->bbox.xmin;
    node_bb2d.ymin = node->bbox.ymin;
    node_bb2d.xmax = node->bbox.xmax;
    node_bb2d.ymax = node->bbox.ymax;
    if (!lv_aabb2d_contains(node_bb2d, px, py))
        return;
#elif AABB_DIMS == 3
    if (!lv_aabb3d_contains(node->bbox, px, py, pz))
        return;
#endif

    /* 叶子节点 */
    if (node->left == AABB_INVALID_NODE && node->right == AABB_INVALID_NODE) {
        if (node->primitive_id == AABB_INVALID_NODE)
            return;
        if (AABB_API(_contains)(tree->primitives[node->primitive_id], px, py
#if AABB_DIMS == 3
            , pz
#endif
        )) {
            result_push_back(result, node->primitive_id);
        }
        return;
    }

    AABB_FUNC(point_recursive)(tree, node->left, px, py
#if AABB_DIMS == 3
        , pz
#endif
        , result);
    AABB_FUNC(point_recursive)(tree, node->right, px, py
#if AABB_DIMS == 3
        , pz
#endif
        , result);
}

/* ========================================================================
 * aabb_tree_depth / aabb_tree_leaf_count — 完全一致
 * ======================================================================== */
static int AABB_FUNC(tree_depth)(const lvAABBNode *nodes, int root) {
    if (root == AABB_INVALID_NODE)
        return 0;
    return nodes[root].height + 1;
}

static int AABB_FUNC(tree_leaf_count)(const lvAABBNode *nodes, int root) {
    if (root == AABB_INVALID_NODE)
        return 0;
    const lvAABBNode *node = &nodes[root];
    if (node->left == AABB_INVALID_NODE && node->right == AABB_INVALID_NODE) {
        return 1;
    }
    return AABB_FUNC(tree_leaf_count)(nodes, node->left) + AABB_FUNC(tree_leaf_count)(nodes, node->right);
}

/* ========================================================================
 * 公共 API：lv_aabb##PREFIX##_build
 * ======================================================================== */
lv_PUBLIC_API AABB_TREE_TYPE *AABB_API(_build)(const AABB_PRIM_TYPE *bboxes, int count, const lvAABBTreeConfig *config) {
    if (!bboxes || count <= 0)
        lv_RETURN_ERROR_NULL(lv_ERROR_INVALID_PARAM, "lv_aabb" AABB_STR(PREFIX) "_build: invalid bboxes or count");

#if AABB_LEAF_MULTI
    /* 2D: 使用 calloc（原文如此） */
    AABB_TREE_TYPE *tree = (AABB_TREE_TYPE *) lv_calloc(1, sizeof(AABB_TREE_TYPE));
    if (!tree)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "lv_aabb2d_build: calloc failed");

    tree->nodes = NULL;
    tree->node_count = 0;
    tree->node_capacity = 0;
    tree->root = AABB_INVALID_NODE;
    tree->primitive_count = count;
    tree->leaf_prim_ids = NULL;
    tree->leaf_prim_capacity = 0;
#else
    /* 3D: 使用 malloc（原文如此） */
    AABB_TREE_TYPE *tree = (AABB_TREE_TYPE *) lv_malloc(sizeof(AABB_TREE_TYPE));
    if (!tree)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "lv_aabb3d_build: malloc failed");

    tree->nodes = NULL;
    tree->node_count = 0;
    tree->node_capacity = 0;
    tree->root = AABB_INVALID_NODE;
    tree->primitive_count = count;
#endif

    if (config) {
        tree->config = *config;
    } else {
        tree->config = lv_aabb_tree_default_config();
    }

#if AABB_LEAF_MULTI
    /* 2D: 使用 malloc */
    tree->primitives = (AABB_PRIM_TYPE *) lv_malloc((size_t) count * sizeof(AABB_PRIM_TYPE));
    if (!tree->primitives) {
        lv_free((void **) &(tree));
        lv_ERROR_SET(lv_ERROR_ALLOCATION_FAILED, "lv_aabb2d_build: malloc primitives failed");
        return NULL;
    }
#else
    /* 3D: 使用 calloc */
    tree->primitives = (AABB_PRIM_TYPE *) lv_calloc((size_t) count, sizeof(AABB_PRIM_TYPE));
    if (!tree->primitives) {
        lv_free((void **) &(tree));
        return NULL;
    }
#endif
    memcpy(tree->primitives, bboxes, (size_t) count * sizeof(AABB_PRIM_TYPE));

    /* 创建几何体索引数组 */
    int *prim_indices = (int *) lv_malloc((size_t) count * sizeof(int));
    if (!prim_indices) {
        lv_free((void **) &(tree->primitives));
        lv_free((void **) &(tree));
#if AABB_LEAF_MULTI
        /* 2D 原文的 bug：错误消息说 lv_aabb3d_build 而非 lv_aabb2d_build，保留不变 */
        lv_ERROR_SET(lv_ERROR_ALLOCATION_FAILED, "lv_aabb3d_build: malloc prim_indices failed");
#else
        lv_ERROR_SET(lv_ERROR_ALLOCATION_FAILED, "lv_aabb3d_build: malloc prim_indices failed");
#endif
        return NULL;
    }
    for (int i = 0; i < count; i++) {
        prim_indices[i] = i;
    }

    /* 递归构建 */
    tree->root = AABB_FUNC(build_recursive)(tree, prim_indices, count, 0);

    lv_free((void **) &(prim_indices));

    if (tree->root == AABB_INVALID_NODE) {
        lv_free((void **) &(tree->primitives));
        lv_free((void **) &(tree->nodes));
        lv_free((void **) &(tree));
#if AABB_DIMS == 3
        lv_ERROR_SET(lv_ERROR_INTERNAL, "lv_aabb3d_build: recursive build failed");
#endif
        return NULL;
    }

    return tree;
}

/* ========================================================================
 * 公共 API：lv_aabb##PREFIX##_destroy
 * ======================================================================== */
lv_PUBLIC_API void AABB_API(_destroy)(AABB_TREE_TYPE *tree) {
    if (!tree)
        return;
#if AABB_LEAF_MULTI
    lv_free((void **) &(tree->leaf_prim_ids));
#endif
    lv_free((void **) &(tree->nodes));
    lv_free((void **) &(tree->primitives));
    lv_free((void **) &(tree));
}

/* ========================================================================
 * 公共 API：lv_aabb##PREFIX##_ray_query
 * ======================================================================== */
lv_PUBLIC_API lvAABBRayHit AABB_API(_ray_query)(const AABB_TREE_TYPE *tree, AABB_RAY_TYPE ray) {
    lvAABBRayHit result;
    result.hit = false;
    result.t = DBL_MAX;
    result.primitive_id = AABB_INVALID_NODE;

    if (!tree || tree->root == AABB_INVALID_NODE)
        return result;

    AABB_FUNC(ray_recursive)(tree, tree->root, ray, 0.0, DBL_MAX, &result);
    return result;
}

/* ========================================================================
 * 公共 API：lv_aabb##PREFIX##_nearest
 * ======================================================================== */
lv_PUBLIC_API lvAABBNearestResult AABB_API(_nearest)(const AABB_TREE_TYPE *tree, double px, double py
#if AABB_DIMS == 3
    , double pz
#endif
) {
    lvAABBNearestResult result;
    result.primitive_id = AABB_INVALID_NODE;
    result.distance = DBL_MAX;
    result.closest_x = 0.0;
    result.closest_y = 0.0;
    result.closest_z = 0.0;

    if (!tree || tree->root == AABB_INVALID_NODE)
        return result;

    AABB_FUNC(nearest_recursive)(tree, tree->root, px, py
#if AABB_DIMS == 3
        , pz
#endif
        , &result);
    return result;
}

/* ========================================================================
 * 公共 API：lv_aabb##PREFIX##_range_query
 * ======================================================================== */
lv_PUBLIC_API void AABB_API(_range_query)(const AABB_TREE_TYPE *tree, AABB_PRIM_TYPE query, lvAABBQueryResult *result) {
    if (!tree || !result || tree->root == AABB_INVALID_NODE)
        return;

    AABB_FUNC(range_recursive)(tree, tree->root, query, result);
}

/* ========================================================================
 * 公共 API：lv_aabb##PREFIX##_point_query
 * ======================================================================== */
lv_PUBLIC_API void AABB_API(_point_query)(const AABB_TREE_TYPE *tree, double px, double py
#if AABB_DIMS == 3
    , double pz
#endif
    , lvAABBQueryResult *result) {
    if (!tree || !result || tree->root == AABB_INVALID_NODE)
        return;

    AABB_FUNC(point_recursive)(tree, tree->root, px, py
#if AABB_DIMS == 3
        , pz
#endif
        , result);
}

/* ========================================================================
 * 公共 API：lv_aabb##PREFIX##_root_bbox
 * ======================================================================== */
lv_PUBLIC_API AABB_PRIM_TYPE AABB_API(_root_bbox)(const AABB_TREE_TYPE *tree) {
    AABB_PRIM_TYPE bb = AABB_EMPTY_PRIM;
    if (!tree || tree->root == AABB_INVALID_NODE)
        return bb;

#if AABB_DIMS == 2
    const lvAABBNode *root = &tree->nodes[tree->root];
    bb.xmin = root->bbox.xmin;
    bb.ymin = root->bbox.ymin;
    bb.xmax = root->bbox.xmax;
    bb.ymax = root->bbox.ymax;
#elif AABB_DIMS == 3
    bb = tree->nodes[tree->root].bbox;
#endif
    return bb;
}

#if AABB_STATS
/* ========================================================================
 * 公共 API：lv_aabb##PREFIX##_stats
 * ======================================================================== */
lv_PUBLIC_API void AABB_API(_stats)(const AABB_TREE_TYPE *tree, int *out_node_count, int *out_depth, int *out_leaf_count) {
    if (!tree) {
        if (out_node_count) *out_node_count = 0;
        if (out_depth) *out_depth = 0;
        if (out_leaf_count) *out_leaf_count = 0;
        return;
    }

    if (out_node_count)
        *out_node_count = tree->node_count;
    if (out_depth)
        *out_depth = AABB_FUNC(tree_depth)(tree->nodes, tree->root);
    if (out_leaf_count)
        *out_leaf_count = AABB_FUNC(tree_leaf_count)(tree->nodes, tree->root);
}
#endif /* AABB_STATS */

/* ========================================================================
 * 清理辅助宏
 * ======================================================================== */
#undef AABB__CONCAT2
#undef AABB__CONCAT
#undef AABB__STR2
#undef AABB_STR
#undef AABB_FUNC
#undef AABB_API