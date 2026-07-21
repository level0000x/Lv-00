#ifndef LV00_GEO_AABB_TREE_H
#define LV00_GEO_AABB_TREE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdbool.h>

#ifndef LV00_PUBLIC_API
#define LV00_PUBLIC_API
#endif

/* ========================================================================
 * 基础类型定义
 * ======================================================================== */

/** 2D 包围盒 */
typedef struct {
    double xmin, ymin;
    double xmax, ymax;
} Lv00AABB2D;

/** 3D 包围盒 */
typedef struct {
    double xmin, ymin, zmin;
    double xmax, ymax, zmax;
} Lv00AABB3D;

/** 2D 点 */
typedef struct {
    double x, y;
} Lv00AABBPoint2D;

/** 3D 点 */
typedef struct {
    double x, y, z;
} Lv00AABBPoint3D;

/** 2D 射线 */
typedef struct {
    double ox, oy;
    double dx, dy;
} Lv00AABBRay2D;

/** 3D 射线 */
typedef struct {
    double ox, oy, oz;
    double dx, dy, dz;
} Lv00AABBRay3D;

/** 射线命中结果 */
typedef struct {
    bool   hit;
    double t;
    int    primitive_id;
} Lv00AABBRayHit;

/** 最近邻查询结果 */
typedef struct {
    int    primitive_id;
    double distance;
    double closest_x;
    double closest_y;
    double closest_z;
} Lv00AABBNearestResult;

/** 范围/点查询结果 */
typedef struct {
    int  *ids;
    int   count;
    int   capacity;
} Lv00AABBQueryResult;

/** AABB 树配置 */
typedef struct {
    int  max_leaf_size;
    int  max_depth;
    bool use_sah;
} Lv00AABBTreeConfig;

/** AABB 树节点（节点包围盒统一使用 3D 表示） */
typedef struct Lv00AABBNode {
    Lv00AABB3D bbox;
    int left;
    int right;
    int primitive_id;
    int height;
    int leaf_start;
    int leaf_count;
} Lv00AABBNode;

/** 2D AABB 树 */
typedef struct Lv00AABBTree2D {
    Lv00AABBNode *nodes;
    int   node_count;
    int   node_capacity;
    int   root;
    int   primitive_count;
    Lv00AABB2D *primitives;
    Lv00AABBTreeConfig config;
    int  *leaf_prim_ids;
    int   leaf_prim_capacity;
} Lv00AABBTree2D;

/** 3D AABB 树 */
typedef struct Lv00AABBTree3D {
    Lv00AABBNode *nodes;
    int   node_count;
    int   node_capacity;
    int   root;
    int   primitive_count;
    Lv00AABB3D *primitives;
    Lv00AABBTreeConfig config;
} Lv00AABBTree3D;

/* ========================================================================
 * 向后兼容类型定义
 * ======================================================================== */

/** 旧版 AABB 类型（兼容旧代码） */
typedef Lv00AABB2D Lv00AABB;
typedef Lv00AABBTree2D Lv00AABBTree;

/* ========================================================================
 * 2D 包围盒基础操作
 * ======================================================================== */

LV00_PUBLIC_API Lv00AABB2D lv00_aabb2d_empty(void);
LV00_PUBLIC_API Lv00AABB2D lv00_aabb2d_point(double x, double y);
LV00_PUBLIC_API Lv00AABB2D lv00_aabb2d_merge(Lv00AABB2D a, Lv00AABB2D b);
LV00_PUBLIC_API bool lv00_aabb2d_is_valid(Lv00AABB2D bb);
LV00_PUBLIC_API bool lv00_aabb2d_contains(Lv00AABB2D bb, double x, double y);
LV00_PUBLIC_API bool lv00_aabb2d_intersects(Lv00AABB2D a, Lv00AABB2D b);
LV00_PUBLIC_API double lv00_aabb2d_area(Lv00AABB2D bb);
LV00_PUBLIC_API Lv00AABBPoint2D lv00_aabb2d_center(Lv00AABB2D bb);

/* ========================================================================
 * 3D 包围盒基础操作
 * ======================================================================== */

LV00_PUBLIC_API Lv00AABB3D lv00_aabb3d_empty(void);
LV00_PUBLIC_API Lv00AABB3D lv00_aabb3d_point(double x, double y, double z);
LV00_PUBLIC_API Lv00AABB3D lv00_aabb3d_merge(Lv00AABB3D a, Lv00AABB3D b);
LV00_PUBLIC_API bool lv00_aabb3d_is_valid(Lv00AABB3D bb);
LV00_PUBLIC_API bool lv00_aabb3d_contains(Lv00AABB3D bb, double x, double y, double z);
LV00_PUBLIC_API bool lv00_aabb3d_intersects(Lv00AABB3D a, Lv00AABB3D b);
LV00_PUBLIC_API double lv00_aabb3d_surface_area(Lv00AABB3D bb);
LV00_PUBLIC_API double lv00_aabb3d_volume(Lv00AABB3D bb);
LV00_PUBLIC_API Lv00AABBPoint3D lv00_aabb3d_center(Lv00AABB3D bb);

/* ========================================================================
 * 查询结果管理
 * ======================================================================== */

LV00_PUBLIC_API void lv00_aabb_query_result_init(Lv00AABBQueryResult *result);
LV00_PUBLIC_API void lv00_aabb_query_result_free(Lv00AABBQueryResult *result);

/* ========================================================================
 * 树配置
 * ======================================================================== */

LV00_PUBLIC_API Lv00AABBTreeConfig lv00_aabb_tree_default_config(void);

/* ========================================================================
 * 2D AABB 树构建与查询 API
 * ======================================================================== */

LV00_PUBLIC_API Lv00AABBTree2D *lv00_aabb2d_build(
    const Lv00AABB2D *bboxes, int count,
    const Lv00AABBTreeConfig *config);

LV00_PUBLIC_API void lv00_aabb2d_destroy(Lv00AABBTree2D *tree);

LV00_PUBLIC_API Lv00AABBRayHit lv00_aabb2d_ray_query(
    const Lv00AABBTree2D *tree, Lv00AABBRay2D ray);

LV00_PUBLIC_API Lv00AABBNearestResult lv00_aabb2d_nearest(
    const Lv00AABBTree2D *tree, double px, double py);

LV00_PUBLIC_API void lv00_aabb2d_range_query(
    const Lv00AABBTree2D *tree, Lv00AABB2D query,
    Lv00AABBQueryResult *result);

LV00_PUBLIC_API void lv00_aabb2d_point_query(
    const Lv00AABBTree2D *tree, double px, double py,
    Lv00AABBQueryResult *result);

LV00_PUBLIC_API Lv00AABB2D lv00_aabb2d_root_bbox(const Lv00AABBTree2D *tree);

LV00_PUBLIC_API void lv00_aabb2d_stats(const Lv00AABBTree2D *tree,
    int *out_node_count, int *out_depth, int *out_leaf_count);

/* ========================================================================
 * 3D AABB 树构建与查询 API
 * ======================================================================== */

LV00_PUBLIC_API Lv00AABBTree3D *lv00_aabb3d_build(
    const Lv00AABB3D *bboxes, int count,
    const Lv00AABBTreeConfig *config);

LV00_PUBLIC_API void lv00_aabb3d_destroy(Lv00AABBTree3D *tree);

LV00_PUBLIC_API Lv00AABBRayHit lv00_aabb3d_ray_query(
    const Lv00AABBTree3D *tree, Lv00AABBRay3D ray);

LV00_PUBLIC_API Lv00AABBNearestResult lv00_aabb3d_nearest(
    const Lv00AABBTree3D *tree, double px, double py, double pz);

LV00_PUBLIC_API void lv00_aabb3d_range_query(
    const Lv00AABBTree3D *tree, Lv00AABB3D query,
    Lv00AABBQueryResult *result);

LV00_PUBLIC_API void lv00_aabb3d_point_query(
    const Lv00AABBTree3D *tree, double px, double py, double pz,
    Lv00AABBQueryResult *result);

LV00_PUBLIC_API Lv00AABB3D lv00_aabb3d_root_bbox(const Lv00AABBTree3D *tree);

/* ========================================================================
 * 旧版兼容 API
 * ======================================================================== */

/** Build AABB tree from points. */
Lv00AABBTree *lv00_aabb_tree_build(const double *points, size_t count, int dim);
/** Query AABB tree for points in box. */
size_t lv00_aabb_tree_query(const Lv00AABBTree *tree, const Lv00AABB *box, double *out, size_t max_out);

#ifdef __cplusplus
}
#endif

#endif
