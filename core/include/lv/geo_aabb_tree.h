#ifndef lv_GEO_AABB_TREE_H
#define lv_GEO_AABB_TREE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include "lv_api_spec.h" /* lv_PUBLIC_API（K59） */

#ifndef lv_PUBLIC_API
#define lv_PUBLIC_API
#endif

/* ========================================================================
 * 基础类型定义
 * ======================================================================== */

/** 2D 包围盒 */
typedef struct {
    double xmin, ymin;
    double xmax, ymax;
} lvAABB2D;

/** 3D 包围盒 */
typedef struct {
    double xmin, ymin, zmin;
    double xmax, ymax, zmax;
} lvAABB3D;

/** 2D 点 */
typedef struct {
    double x, y;
} lvAABBPoint2D;

/** 3D 点 */
typedef struct {
    double x, y, z;
} lvAABBPoint3D;

/** 2D 射线 */
typedef struct {
    double ox, oy;
    double dx, dy;
} lvAABBRay2D;

/** 3D 射线 */
typedef struct {
    double ox, oy, oz;
    double dx, dy, dz;
} lvAABBRay3D;

/** 射线命中结果 */
typedef struct {
    bool hit;
    double t;
    int primitive_id;
} lvAABBRayHit;

/** 最近邻查询结果 */
typedef struct {
    int primitive_id;
    double distance;
    double closest_x;
    double closest_y;
    double closest_z;
} lvAABBNearestResult;

/** 范围/点查询结果 */
typedef struct {
    int *ids;
    int count;
    int capacity;
} lvAABBQueryResult;

/** AABB 树配置 */
typedef struct {
    int max_leaf_size;
    int max_depth;
    bool use_sah;
} lvAABBTreeConfig;

/** AABB 树节点（节点包围盒统一使用 3D 表示） */
typedef struct lvAABBNode {
    lvAABB3D bbox;
    int left;
    int right;
    int primitive_id;
    int height;
    int leaf_start;
    int leaf_count;
} lvAABBNode;

/** 2D AABB 树 */
typedef struct lvAABBTree2D {
    lvAABBNode *nodes;
    int node_count;
    int node_capacity;
    int root;
    int primitive_count;
    lvAABB2D *primitives;
    lvAABBTreeConfig config;
    int *leaf_prim_ids;
    int leaf_prim_capacity;
} lvAABBTree2D;

/** 3D AABB 树 */
typedef struct lvAABBTree3D {
    lvAABBNode *nodes;
    int node_count;
    int node_capacity;
    int root;
    int primitive_count;
    lvAABB3D *primitives;
    lvAABBTreeConfig config;
} lvAABBTree3D;

/* ========================================================================
 * 向后兼容类型定义
 * ======================================================================== */

/** 旧版 AABB 类型（兼容旧代码） */
typedef lvAABB2D lvAABB;
typedef lvAABBTree2D lvAABBTree;

/* ========================================================================
 * 2D 包围盒基础操作
 * ======================================================================== */

lv_PUBLIC_API lvAABB2D lv_aabb2d_empty(void);
lv_PUBLIC_API lvAABB2D lv_aabb2d_point(double x, double y);
lv_PUBLIC_API lvAABB2D lv_aabb2d_merge(lvAABB2D a, lvAABB2D b);
lv_PUBLIC_API bool lv_aabb2d_is_valid(lvAABB2D bb);
lv_PUBLIC_API bool lv_aabb2d_contains(lvAABB2D bb, double x, double y);
lv_PUBLIC_API bool lv_aabb2d_intersects(lvAABB2D a, lvAABB2D b);
lv_PUBLIC_API double lv_aabb2d_area(lvAABB2D bb);
lv_PUBLIC_API lvAABBPoint2D lv_aabb2d_center(lvAABB2D bb);

/* ========================================================================
 * 3D 包围盒基础操作
 * ======================================================================== */

lv_PUBLIC_API lvAABB3D lv_aabb3d_empty(void);
lv_PUBLIC_API lvAABB3D lv_aabb3d_point(double x, double y, double z);
lv_PUBLIC_API lvAABB3D lv_aabb3d_merge(lvAABB3D a, lvAABB3D b);
lv_PUBLIC_API bool lv_aabb3d_is_valid(lvAABB3D bb);
lv_PUBLIC_API bool lv_aabb3d_contains(lvAABB3D bb, double x, double y, double z);
lv_PUBLIC_API bool lv_aabb3d_intersects(lvAABB3D a, lvAABB3D b);
lv_PUBLIC_API double lv_aabb3d_surface_area(lvAABB3D bb);
lv_PUBLIC_API double lv_aabb3d_volume(lvAABB3D bb);
lv_PUBLIC_API lvAABBPoint3D lv_aabb3d_center(lvAABB3D bb);

/* ========================================================================
 * 查询结果管理
 * ======================================================================== */

lv_PUBLIC_API void lv_aabb_query_result_init(lvAABBQueryResult *result);
lv_PUBLIC_API void lv_aabb_query_result_free(lvAABBQueryResult *result);

/* ========================================================================
 * 树配置
 * ======================================================================== */

lv_PUBLIC_API lvAABBTreeConfig lv_aabb_tree_default_config(void);

/* ========================================================================
 * 2D AABB 树构建与查询 API
 * ======================================================================== */

lv_PUBLIC_API lvAABBTree2D *lv_aabb2d_build(const lvAABB2D *bboxes, int count, const lvAABBTreeConfig *config);

lv_PUBLIC_API void lv_aabb2d_destroy(lvAABBTree2D *tree);

lv_PUBLIC_API lvAABBRayHit lv_aabb2d_ray_query(const lvAABBTree2D *tree, lvAABBRay2D ray);

lv_PUBLIC_API lvAABBNearestResult lv_aabb2d_nearest(const lvAABBTree2D *tree, double px, double py);

lv_PUBLIC_API void lv_aabb2d_range_query(const lvAABBTree2D *tree, lvAABB2D query, lvAABBQueryResult *result);

lv_PUBLIC_API void lv_aabb2d_point_query(const lvAABBTree2D *tree, double px, double py, lvAABBQueryResult *result);

lv_PUBLIC_API lvAABB2D lv_aabb2d_root_bbox(const lvAABBTree2D *tree);

lv_PUBLIC_API void lv_aabb2d_stats(const lvAABBTree2D *tree, int *out_node_count, int *out_depth, int *out_leaf_count);

/* ========================================================================
 * 3D AABB 树构建与查询 API
 * ======================================================================== */

lv_PUBLIC_API lvAABBTree3D *lv_aabb3d_build(const lvAABB3D *bboxes, int count, const lvAABBTreeConfig *config);

lv_PUBLIC_API void lv_aabb3d_destroy(lvAABBTree3D *tree);

lv_PUBLIC_API lvAABBRayHit lv_aabb3d_ray_query(const lvAABBTree3D *tree, lvAABBRay3D ray);

lv_PUBLIC_API lvAABBNearestResult lv_aabb3d_nearest(const lvAABBTree3D *tree, double px, double py, double pz);

lv_PUBLIC_API void lv_aabb3d_range_query(const lvAABBTree3D *tree, lvAABB3D query, lvAABBQueryResult *result);

lv_PUBLIC_API void lv_aabb3d_point_query(const lvAABBTree3D *tree, double px, double py, double pz,
                                         lvAABBQueryResult *result);

lv_PUBLIC_API lvAABB3D lv_aabb3d_root_bbox(const lvAABBTree3D *tree);

/* ========================================================================
 * 旧版兼容 API
 * ======================================================================== */

/** Build AABB tree from points. */
lvAABBTree *lv_aabb_tree_build(const double *points, size_t count, int dim);
/** Query AABB tree for points in box. */
lv_PUBLIC_API size_t lv_aabb_tree_query(const lvAABBTree *tree, const lvAABB *box, double *out, size_t max_out);

#ifdef __cplusplus
}
#endif

#endif
