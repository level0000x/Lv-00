/**
 * @file geo_aabb_tree.h
 * @brief AABB 树空间索引 —— 借鉴 CGAL AABB_tree + Boost.Geometry R-tree
 *
 * 借鉴来源：
 *   - CGAL AABB_tree (github.com/CGAL/cgal, AABB_tree/)
 *     AABB 层次包围体树，支持射线查询、最近邻查询
 *   - Boost.Geometry index (boost.org/libs/geometry/index)
 *     R-tree 空间索引，支持 within/nearest/intersects 查询
 *
 * 设计目标：
 *   - 纯 C 实现，与 Lv-00 几何层无缝集成
 *   - 支持 2D/3D 几何体的空间查询
 *   - 射线查询、最近邻查询、范围查询
 *   - 动态插入/删除
 *
 * 版本：v3.6.0（第十三梯队 CGAL + Boost.Geometry 落地）
 */

#ifndef LV00_GEO_AABB_TREE_H
#define LV00_GEO_AABB_TREE_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifndef LV00_PUBLIC_API
#define LV00_PUBLIC_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 第一部分：基础数据结构
 * ======================================================================== */

/**
 * @brief 2D 轴对齐包围盒
 */
typedef struct {
    double xmin, ymin;   /**< 最小角 */
    double xmax, ymax;   /**< 最大角 */
} Lv00AABB2D;

/**
 * @brief 3D 轴对齐包围盒
 */
typedef struct {
    double xmin, ymin, zmin;  /**< 最小角 */
    double xmax, ymax, zmax;  /**< 最大角 */
} Lv00AABB3D;

/**
 * @brief 2D 点
 */
typedef struct {
    double x, y;
} Lv00AABBPoint2D;

/**
 * @brief 3D 点
 */
typedef struct {
    double x, y, z;
} Lv00AABBPoint3D;

/**
 * @brief 2D 射线
 */
typedef struct {
    double ox, oy;     /**< 原点 */
    double dx, dy;     /**< 方向（不需要归一化） */
} Lv00AABBRay2D;

/**
 * @brief 3D 射线
 */
typedef struct {
    double ox, oy, oz;  /**< 原点 */
    double dx, dy, dz;  /**< 方向（不需要归一化） */
} Lv00AABBRay3D;

/**
 * @brief 射线-几何体相交结果
 */
typedef struct {
    bool hit;           /**< 是否相交 */
    double t;           /**< 射线参数 t（交点 = origin + t * direction） */
    int primitive_id;   /**< 命中的几何体 ID */
} Lv00AABBRayHit;

/**
 * @brief 最近邻查询结果
 */
typedef struct {
    int primitive_id;       /**< 最近邻几何体 ID */
    double distance;        /**< 距离 */
    double closest_x;       /**< 最近点 X 坐标 */
    double closest_y;       /**< 最近点 Y 坐标 */
    double closest_z;       /**< 最近点 Z 坐标 */
} Lv00AABBNearestResult;

/**
 * @brief 范围查询结果集合
 */
typedef struct {
    int *ids;              /**< 命中的几何体 ID 数组 */
    int count;             /**< 命中数量 */
    int capacity;          /**< 数组容量 */
} Lv00AABBQueryResult;

/* ========================================================================
 * 第二部分：AABB 树核心结构
 * ======================================================================== */

/**
 * @brief AABB 树节点（内部使用）
 */
typedef struct Lv00AABBNode {
    Lv00AABB3D bbox;        /**< 节点包围盒 */
    int left;               /**< 左子节点索引（-1 表示叶子） */
    int right;              /**< 右子节点索引（-1 表示叶子） */
    int primitive_id;       /**< 叶子节点存储的几何体 ID（-1 表示内部节点） */
    int height;             /**< 节点高度（叶子为 0） */
    int leaf_start;         /**< 叶子节点在 leaf_prim_ids 中的起始索引 */
    int leaf_count;         /**< 叶子节点包含的几何体数量 */
} Lv00AABBNode;

/**
 * @brief AABB 树配置
 */
typedef struct {
    int max_leaf_size;      /**< 叶子节点最大几何体数（默认 4） */
    int max_depth;          /**< 最大树深度（默认 64） */
    bool use_sah;           /**< 是否使用表面积启发式分裂（默认 true） */
} Lv00AABBTreeConfig;

/**
 * @brief AABB 树（2D 版本）
 */
typedef struct Lv00AABBTree2D {
    Lv00AABBNode *nodes;        /**< 节点数组 */
    int node_count;             /**< 节点数量 */
    int node_capacity;          /**< 节点容量 */
    int root;                   /**< 根节点索引 */
    Lv00AABB2D *primitives;     /**< 几何体包围盒数组 */
    int primitive_count;        /**< 几何体数量 */
    int *leaf_prim_ids;         /**< 叶子节点几何体 ID 数组 */
    int leaf_prim_capacity;     /**< leaf_prim_ids 已使用数量 */
    Lv00AABBTreeConfig config;  /**< 配置 */
} Lv00AABBTree2D;

/**
 * @brief AABB 树（3D 版本）
 */
typedef struct Lv00AABBTree3D {
    Lv00AABBNode *nodes;        /**< 节点数组 */
    int node_count;             /**< 节点数量 */
    int node_capacity;          /**< 节点容量 */
    int root;                   /**< 根节点索引 */
    Lv00AABB3D *primitives;     /**< 几何体包围盒数组 */
    int primitive_count;        /**< 几何体数量 */
    Lv00AABBTreeConfig config;  /**< 配置 */
} Lv00AABBTree3D;

/* ========================================================================
 * 第三部分：包围盒操作 API
 * ======================================================================== */

/**
 * @brief 创建空的 2D 包围盒
 */
LV00_PUBLIC_API Lv00AABB2D lv00_aabb2d_empty(void);

/**
 * @brief 创建包含单个点的 2D 包围盒
 */
LV00_PUBLIC_API Lv00AABB2D lv00_aabb2d_point(double x, double y);

/**
 * @brief 合并两个 2D 包围盒
 */
LV00_PUBLIC_API Lv00AABB2D lv00_aabb2d_merge(Lv00AABB2D a, Lv00AABB2D b);

/**
 * @brief 判定 2D 包围盒是否有效
 */
LV00_PUBLIC_API bool lv00_aabb2d_is_valid(Lv00AABB2D bb);

/**
 * @brief 判定点是否在 2D 包围盒内
 */
LV00_PUBLIC_API bool lv00_aabb2d_contains(Lv00AABB2D bb, double x, double y);

/**
 * @brief 判定两个 2D 包围盒是否相交
 */
LV00_PUBLIC_API bool lv00_aabb2d_intersects(Lv00AABB2D a, Lv00AABB2D b);

/**
 * @brief 计算 2D 包围盒的表面积
 */
LV00_PUBLIC_API double lv00_aabb2d_area(Lv00AABB2D bb);

/**
 * @brief 计算 2D 包围盒的中心点
 */
LV00_PUBLIC_API Lv00AABBPoint2D lv00_aabb2d_center(Lv00AABB2D bb);

/**
 * @brief 创建空的 3D 包围盒
 */
LV00_PUBLIC_API Lv00AABB3D lv00_aabb3d_empty(void);

/**
 * @brief 创建包含单个点的 3D 包围盒
 */
LV00_PUBLIC_API Lv00AABB3D lv00_aabb3d_point(double x, double y, double z);

/**
 * @brief 合并两个 3D 包围盒
 */
LV00_PUBLIC_API Lv00AABB3D lv00_aabb3d_merge(Lv00AABB3D a, Lv00AABB3D b);

/**
 * @brief 判定 3D 包围盒是否有效
 */
LV00_PUBLIC_API bool lv00_aabb3d_is_valid(Lv00AABB3D bb);

/**
 * @brief 判定点是否在 3D 包围盒内
 */
LV00_PUBLIC_API bool lv00_aabb3d_contains(Lv00AABB3D bb, double x, double y, double z);

/**
 * @brief 判定两个 3D 包围盒是否相交
 */
LV00_PUBLIC_API bool lv00_aabb3d_intersects(Lv00AABB3D a, Lv00AABB3D b);

/**
 * @brief 计算 3D 包围盒的表面积
 */
LV00_PUBLIC_API double lv00_aabb3d_surface_area(Lv00AABB3D bb);

/**
 * @brief 计算 3D 包围盒的体积
 */
LV00_PUBLIC_API double lv00_aabb3d_volume(Lv00AABB3D bb);

/**
 * @brief 计算 3D 包围盒的中心点
 */
LV00_PUBLIC_API Lv00AABBPoint3D lv00_aabb3d_center(Lv00AABB3D bb);

/* ========================================================================
 * 第四部分：AABB 树构建与查询 API（2D）
 * ======================================================================== */

/**
 * @brief 构建 2D AABB 树
 *
 * @param bboxes    几何体包围盒数组
 * @param count     几何体数量
 * @param config    配置（NULL 使用默认配置）
 * @return AABB 树指针（需用 lv00_aabb2d_free 释放）
 */
LV00_PUBLIC_API Lv00AABBTree2D *lv00_aabb2d_build(
    const Lv00AABB2D *bboxes, int count,
    const Lv00AABBTreeConfig *config);

/**
 * @brief 释放 2D AABB 树
 */
LV00_PUBLIC_API void lv00_aabb2d_free(Lv00AABBTree2D *tree);

/**
 * @brief 2D 射线查询 —— 找到第一个相交的几何体
 *
 * @param tree  AABB 树
 * @param ray   射线
 * @return 射线命中结果
 */
LV00_PUBLIC_API Lv00AABBRayHit lv00_aabb2d_ray_query(
    const Lv00AABBTree2D *tree, Lv00AABBRay2D ray);

/**
 * @brief 2D 最近邻查询
 *
 * @param tree AABB 树
 * @param px, py 查询点
 * @return 最近邻结果
 */
LV00_PUBLIC_API Lv00AABBNearestResult lv00_aabb2d_nearest(
    const Lv00AABBTree2D *tree, double px, double py);

/**
 * @brief 2D 范围查询 —— 找到所有与包围盒相交的几何体
 *
 * @param tree    AABB 树
 * @param query   查询包围盒
 * @param result  输出结果（需用 lv00_aabb_query_result_free 释放）
 */
LV00_PUBLIC_API void lv00_aabb2d_range_query(
    const Lv00AABBTree2D *tree, Lv00AABB2D query,
    Lv00AABBQueryResult *result);

/**
 * @brief 2D 点查询 —— 找到包含指定点的所有几何体
 *
 * @param tree AABB 树
 * @param px, py 查询点
 * @param result 输出结果
 */
LV00_PUBLIC_API void lv00_aabb2d_point_query(
    const Lv00AABBTree2D *tree, double px, double py,
    Lv00AABBQueryResult *result);

/**
 * @brief 获取 2D AABB 树的根包围盒
 */
LV00_PUBLIC_API Lv00AABB2D lv00_aabb2d_root_bbox(const Lv00AABBTree2D *tree);

/**
 * @brief 获取 2D AABB 树统计信息
 */
LV00_PUBLIC_API void lv00_aabb2d_stats(const Lv00AABBTree2D *tree,
    int *out_node_count, int *out_depth, int *out_leaf_count);

/* ========================================================================
 * 第五部分：AABB 树构建与查询 API（3D）
 * ======================================================================== */

/**
 * @brief 构建 3D AABB 树
 */
LV00_PUBLIC_API Lv00AABBTree3D *lv00_aabb3d_build(
    const Lv00AABB3D *bboxes, int count,
    const Lv00AABBTreeConfig *config);

/**
 * @brief 释放 3D AABB 树
 */
LV00_PUBLIC_API void lv00_aabb3d_free(Lv00AABBTree3D *tree);

/**
 * @brief 3D 射线查询
 */
LV00_PUBLIC_API Lv00AABBRayHit lv00_aabb3d_ray_query(
    const Lv00AABBTree3D *tree, Lv00AABBRay3D ray);

/**
 * @brief 3D 最近邻查询
 */
LV00_PUBLIC_API Lv00AABBNearestResult lv00_aabb3d_nearest(
    const Lv00AABBTree3D *tree, double px, double py, double pz);

/**
 * @brief 3D 范围查询
 */
LV00_PUBLIC_API void lv00_aabb3d_range_query(
    const Lv00AABBTree3D *tree, Lv00AABB3D query,
    Lv00AABBQueryResult *result);

/**
 * @brief 3D 点查询
 */
LV00_PUBLIC_API void lv00_aabb3d_point_query(
    const Lv00AABBTree3D *tree, double px, double py, double pz,
    Lv00AABBQueryResult *result);

/**
 * @brief 获取 3D AABB 树的根包围盒
 */
LV00_PUBLIC_API Lv00AABB3D lv00_aabb3d_root_bbox(const Lv00AABBTree3D *tree);

/* ========================================================================
 * 第六部分：查询结果管理
 * ======================================================================== */

/**
 * @brief 初始化查询结果
 */
LV00_PUBLIC_API void lv00_aabb_query_result_init(Lv00AABBQueryResult *result);

/**
 * @brief 释放查询结果
 */
LV00_PUBLIC_API void lv00_aabb_query_result_free(Lv00AABBQueryResult *result);

/**
 * @brief 获取默认 AABB 树配置
 */
LV00_PUBLIC_API Lv00AABBTreeConfig lv00_aabb_tree_default_config(void);

#ifdef __cplusplus
}
#endif

#endif /* LV00_GEO_AABB_TREE_H */
