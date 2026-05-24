/**
 * @file geometry_types.h
 * @brief 几何实体类型层次 —— 借鉴 SymPy Geometry + clifford flat array 存储
 *
 * 借鉴来源：
 *   - SymPy Geometry（github.com/sympy/sympy）
 *     GeometryEntity 继承层次、直观的 API 命名约定
 *   - clifford（github.com/pygae/clifford）
 *     flat array 多向量存储策略、Numba JIT 加速理念
 *
 * 设计目标：
 *   - 清晰的几何实体继承层次（SymPy 风格）
 *   - 紧凑的扁平数组存储（clifford 风格，便于 SIMD）
 *   - 与现有 ConstraintGraph 系统无缝衔接
 *
 * 版本：v3.2.0
 */

#ifndef LV00_GEOMETRY_TYPES_H
#define LV00_GEOMETRY_TYPES_H

#include "symbolic_coord.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 第一部分：几何实体类型层次（借鉴 SymPy GeometryEntity）
 *
 * SymPy 层次：
 *   GeometryEntity → GeometrySet → LinearEntity → Line / Ray / Segment
 *                               → Ellipse → Circle / Ellipse
 *                               → Polygon → Triangle
 *
 * Lv-00 适配层次：
 *   GeometryEntity → PointEntity
 *                  → LinearEntity → LineEntity / SegmentEntity / RayEntity
 *                  → CircularEntity → CircleEntity
 *                  → RegionEntity → PolygonEntity → TriangleEntity
 * ======================================================================== */

/**
 * @brief 几何实体基类型
 *
 * 所有几何实体的公共属性：
 * - 维度（0 = 点, 1 = 线, 2 = 面）
 * - 包围盒（用于碰撞检测和空间索引）
 * - 信任颜色
 */
typedef enum {
    GEOM_ENTITY_POINT,       /**< 点：零维，无长度 */
    GEOM_ENTITY_LINE,        /**< 直线：一维，无限延伸 */
    GEOM_ENTITY_RAY,         /**< 射线：一维，单向无限 */
    GEOM_ENTITY_SEGMENT,     /**< 线段：一维，有限长度 */
    GEOM_ENTITY_CIRCLE,      /**< 圆：一维曲线 */
    GEOM_ENTITY_POLYGON,     /**< 多边形：二维区域 */
    GEOM_ENTITY_TRIANGLE,    /**< 三角形：特殊多边形 */
    GEOM_ENTITY_MESH         /**< 网格：复合二维区域 */
} GeomEntityKind;

/**
 * @brief 几何实体基结构
 *
 * 借鉴 SymPy 的 GeometryEntity 设计——所有几何实体共享此基础。
 * 通过 kind 字段区分具体类型，提供统一的操作接口。
 */
typedef struct GeomEntity {
    GeomEntityKind kind;         /**< 实体类型 */
    int dimension;               /**< 维度（0/1/2） */
    TrustColor trust;            /**< 信任颜色 */

    /* 包围盒（轴对齐，用于快速碰撞检测和空间索引） */
    struct {
        double x_min, y_min;
        double x_max, y_max;
    } bounding_box;

    /* 关联的约束图节点ID（-1 = 未关联） */
    int graph_node_id;

    /* 名称（可选） */
    char *name;
} GeomEntity;

/**
 * @brief 点实体（0 维）
 */
typedef struct PointEntity {
    GeomEntity base;             /**< 基类 */
    SymbolicCoord *x;            /**< X 坐标 */
    SymbolicCoord *y;            /**< Y 坐标 */
    /* 在更高维度中可有 z, w 等 */
    int dimension;               /**< 坐标维度 */
    SymbolicCoord **coords;      /**< 坐标数组（含 x, y, z, ...） */
} PointEntity;

/**
 * @brief 线性实体（1 维）
 *
 * 借鉴 SymPy LinearEntity 设计——Line/Ray/Segment 的公共基类。
 */
typedef struct LinearEntity {
    GeomEntity base;             /**< 基类 */
    PointEntity *p1;             /**< 起点 */
    PointEntity *p2;             /**< 终点/方向点 */
    /* 预计算的派生量（用于加速计算） */
    struct {
        double dx, dy;           /**< 方向向量（浮点近似） */
        double length;           /**< 长度（浮点近似） */
        bool valid;              /**< 是否有效 */
    } _cache;
} LinearEntity;

/**
 * @brief 圆形实体（1 维曲线）
 */
typedef struct CircularEntity {
    GeomEntity base;             /**< 基类 */
    PointEntity *center;         /**< 圆心 */
    SymbolicCoord *radius;       /**< 半径 */
} CircularEntity;

/**
 * @brief 多边形实体（2 维区域）
 */
typedef struct PolygonEntity {
    GeomEntity base;             /**< 基类 */
    PointEntity **vertices;      /**< 顶点数组 */
    int vertex_count;            /**< 顶点数量 */
    bool is_convex;              /**< 是否为凸多边形 */
} PolygonEntity;

/**
 * @brief 三角形实体（特殊多边形，提供丰富的便捷方法）
 */
typedef struct TriangleEntity {
    PolygonEntity base;          /**< 继承多边形（顶点数为3） */
    /* 预计算的特殊点 */
    PointEntity *_centroid;      /**< 重心缓存 */
    PointEntity *_circumcenter;  /**< 外心缓存 */
    PointEntity *_incenter;      /**< 内心缓存 */
    PointEntity *_orthocenter;   /**< 垂心缓存 */
} TriangleEntity;

/* ========================================================================
 * 第二部分：扁平数组存储（借鉴 clifford flat array）
 *
 * clifford 的设计理念：
 *   - 多向量的所有分量用 flat array 存储
 *   - 避免 Python 对象开销
 *   - Numba JIT 可直接优化 flat array 操作
 *
 * Lv-00 适配：
 *   - 几何实体的数值分量用紧凑的双精度数组存储
 *   - 所有分量连续排列，便于 SIMD 批处理
 *   - 支持"符号标记"——标记哪些分量是符号的、需要精确计算
 * ======================================================================== */

/**
 * @brief 扁平分量存储
 *
 * 将几何实体的所有数值分量存储在连续的双精度数组中。
 *
 * 布局示例（三角形）：
 *   [A.x, A.y, B.x, B.y, C.x, C.y]  — 6 个 double 连续排列
 *
 * 布局示例（圆）：
 *   [center.x, center.y, radius]  — 3 个 double 连续排列
 *
 * 借鉴 clifford 的多向量分量存储：
 *   multivector.flat = [scalar, e1, e2, e3, e12, e13, e23, I]
 */
typedef struct FlatStorage {
    double *components;          /**< 扁平分量数组 */
    int component_count;         /**< 分量总数 */
    bool *is_symbolic;           /**< 符号标记（true = 需要 SymbolicCoord 精确值） */

    /* SIMD 友好的元数据 */
    int alignment;               /**< 内存对齐（通常 32 或 64 字节） */
    bool owns_memory;            /**< 是否拥有内存（需释放） */

    /* 分量→几何实体反向映射 */
    struct {
        int entity_index;        /**< 实体索引 */
        int offset;              /**< 在该实体内的偏移 */
    } *mapping;                  /**< 映射数组 */
    int mapping_count;           /**< 映射数量 */
} FlatStorage;

/* ========================================================================
 * 第三部分：几何实体 API（借鉴 SymPy Geometry 命名约定）
 *
 * 命名约定（对标 SymPy）：
 *   distance    — 距离计算
 *   contains    — 包含性判断
 *   intersect   — 相交计算
 *   reflect     — 反射变换
 *   rotate      — 旋转变换
 *   translate   — 平移变换
 *   scale       — 缩放变换
 *   midpoint    — 中点（线段专用）
 *   area        — 面积（区域专用）
 *   perimeter   — 周长（区域专用）
 * ======================================================================== */

/* --- 通用 API --- */

/**
 * @brief 计算两个几何实体的距离
 * @return 浮点距离，失败返回 NaN
 */
double geom_entity_distance(const GeomEntity *a, const GeomEntity *b);

/**
 * @brief 判断实体 a 是否包含实体 b
 * @return true 如果 b 完全在 a 内部（含边界）
 */
bool geom_entity_contains(const GeomEntity *a, const GeomEntity *b);

/**
 * @brief 计算两个几何实体的交点
 * @param a, b      两个几何实体
 * @param out_points 输出：交点数组（调用者释放）
 * @param out_count  输出：交点数量
 * @return true 成功
 */
bool geom_entity_intersect(
    const GeomEntity *a,
    const GeomEntity *b,
    PointEntity ***out_points,
    int *out_count);

/* --- 点实体 API --- */

PointEntity *point_entity_create(SymbolicCoord *x, SymbolicCoord *y);
PointEntity *point_entity_create_nd(SymbolicCoord **coords, int dim);
void point_entity_destroy(PointEntity *pt);

double point_entity_distance(const PointEntity *a, const PointEntity *b);
PointEntity *point_entity_midpoint(const PointEntity *a, const PointEntity *b);
bool point_entity_is_collinear(const PointEntity *a, const PointEntity *b, const PointEntity *c);

PointEntity *point_entity_translate(const PointEntity *pt, double dx, double dy);
PointEntity *point_entity_rotate(const PointEntity *pt, double angle_deg, double cx, double cy);
PointEntity *point_entity_reflect(const PointEntity *pt, const PointEntity *center);

/* --- 线性实体 API --- */

LinearEntity *linear_entity_create_line(PointEntity *p1, PointEntity *p2);
LinearEntity *linear_entity_create_segment(PointEntity *p1, PointEntity *p2);
LinearEntity *linear_entity_create_ray(PointEntity *origin, PointEntity *through);
void linear_entity_destroy(LinearEntity *le);

double linear_entity_length(const LinearEntity *le);
PointEntity *linear_entity_midpoint(const LinearEntity *le);
bool linear_entity_is_parallel(const LinearEntity *a, const LinearEntity *b);
bool linear_entity_is_perpendicular(const LinearEntity *a, const LinearEntity *b);

/* --- 圆形实体 API --- */

CircularEntity *circular_entity_create(PointEntity *center, SymbolicCoord *radius);
void circular_entity_destroy(CircularEntity *ce);

double circular_entity_area(const CircularEntity *ce);
double circular_entity_circumference(const CircularEntity *ce);
bool circular_entity_contains_point(const CircularEntity *ce, const PointEntity *pt);

/* --- 多边形实体 API --- */

PolygonEntity *polygon_entity_create(PointEntity **vertices, int count);
void polygon_entity_destroy(PolygonEntity *pe);

double polygon_entity_area(const PolygonEntity *pe);
double polygon_entity_perimeter(const PolygonEntity *pe);
bool polygon_entity_is_convex(const PolygonEntity *pe);
bool polygon_entity_is_regular(const PolygonEntity *pe, double tolerance);
PointEntity *polygon_entity_centroid(const PolygonEntity *pe);

/* --- 三角形实体 API（继承多边形，提供特殊点便捷方法）--- */

TriangleEntity *triangle_entity_create(PointEntity *a, PointEntity *b, PointEntity *c);
void triangle_entity_destroy(TriangleEntity *te);

PointEntity *triangle_entity_centroid(const TriangleEntity *te);
PointEntity *triangle_entity_circumcenter(const TriangleEntity *te);
PointEntity *triangle_entity_incenter(const TriangleEntity *te);
PointEntity *triangle_entity_orthocenter(const TriangleEntity *te);
bool triangle_entity_is_right(const TriangleEntity *te, double tolerance);
bool triangle_entity_is_equilateral(const TriangleEntity *te, double tolerance);

/* ========================================================================
 * 第四部分：扁平存储 API（借鉴 clifford）
 * ======================================================================== */

/**
 * @brief 创建扁平存储
 * @param component_count 分量总数
 * @param alignment      内存对齐字节数（0 = 默认 32）
 * @return 新分配的 FlatStorage
 */
FlatStorage *flat_storage_create(int component_count, int alignment);

/**
 * @brief 销毁扁平存储
 */
void flat_storage_destroy(FlatStorage *fs);

/**
 * @brief 从点实体写入分量到扁平存储
 * @param fs     扁平存储
 * @param offset 写入起始偏移
 * @param pt     点实体
 * @return 写入的分量数量
 */
int flat_storage_write_point(FlatStorage *fs, int offset, const PointEntity *pt);

/**
 * @brief 从线性实体写入分量到扁平存储
 * @return 写入的分量数量（4 = p1.x, p1.y, p2.x, p2.y）
 */
int flat_storage_write_linear(FlatStorage *fs, int offset, const LinearEntity *le);

/**
 * @brief 从多边形实体写入全部顶点分量到扁平存储
 * @return 写入的分量数量（2 * vertex_count）
 */
int flat_storage_write_polygon(FlatStorage *fs, int offset, const PolygonEntity *pe);

/**
 * @brief 批量变换：对扁平存储中的所有点分量批量应用变换
 *
 * 这是 clifford-style 的核心优化——用一个函数批量处理所有分量，
 * 避免逐个 Python 对象的开销。在 C 层面可利用 SIMD 自动向量化。
 *
 * @param fs       扁平存储
 * @param tx, ty   平移量
 * @param angle_deg  旋转角度（度）
 * @param sx, sy   缩放因子
 */
void flat_storage_batch_transform(
    FlatStorage *fs,
    double tx, double ty,
    double angle_deg,
    double sx, double sy);

/**
 * @brief 批量计算距离：计算扁平存储中所有点对的距离
 *
 * 利用连续内存布局，一次遍历计算所有距离，SIMD 友好。
 *
 * @param fs          扁平存储（每 2 个分量一个点）
 * @param distances   输出：距离数组（调用者分配，长度 = point_count * (point_count-1) / 2）
 * @param point_count 点的数量
 */
void flat_storage_batch_distances(
    const FlatStorage *fs,
    double *distances,
    int point_count);

/**
 * @brief 将扁平存储中的分量加载到几何实体
 * @param fs       扁平存储
 * @param offset   读取起始偏移
 * @param out_pt   输出：点实体（预先分配）
 */
void flat_storage_read_point(const FlatStorage *fs, int offset, PointEntity *out_pt);

/* ================================================================
 * === 第六梯队参考项目落地 (P1) — OpenSCAD CSG 操作符 =============
 * === 2026-05-24 ==================================================
 *
 * 借鉴 OpenSCAD (github.com/openscad/openscad) 的 CSG 操作符链
 * 和"脚本即 3D 模型"编译器范式。
 * ================================================================ */

/** @brief CSG 节点类型 */
typedef enum {
    CSG_NODE_PRIMITIVE,        /* 图元：cube/sphere/cylinder */
    CSG_NODE_UNION,            /* 布尔并集 */
    CSG_NODE_DIFFERENCE,       /* 布尔差集（第一个子节点减其余） */
    CSG_NODE_INTERSECTION,     /* 布尔交集 */
    CSG_NODE_TRANSFORM,        /* 变换：translate/rotate/scale */
    CSG_NODE_EXTRUDE_LINEAR,   /* 线性拉伸（2D→3D） */
    CSG_NODE_EXTRUDE_ROTATE,   /* 旋转拉伸 */
    CSG_NODE_HULL,             /* 凸包 */
    CSG_NODE_MINKOWSKI         /* Minkowski 和 */
} CSGNodeKind;

/** @brief CSG 构造树节点 — 对应 OpenSCAD CSG 操作树 */
typedef struct CSGNode {
    CSGNodeKind  kind;
    /* 图元数据（PRIMITIVE） */
    union { struct { int type; double params[6]; } prim; } data;
    /* 变换数据（TRANSFORM） */
    double transform[4][4];    /* 4x4 齐次变换矩阵 */
    /* 子树（UNION/DIFFERENCE/INTERSECTION 的内部子节点） */
    struct CSGNode **children;
    int              child_count;
    int              child_capacity;
    /* 包围盒 */
    double bbox_min[3];
    double bbox_max[3];
    /* 关联的 FuncBlock ID（用于 DSL 代码溯源） */
    int func_block_id;
} CSGNode;

/* ---- CSG 节点生命周期 ---- */
CSGNode *csg_node_create(CSGNodeKind kind);
void csg_node_add_child(CSGNode *parent, CSGNode *child);
void csg_node_destroy(CSGNode *node);

/* ---- CSG 布尔运算（BSP 树实现）---- */

/**
 * @brief CSG 布尔并集 — 合并两个 CSG 子树
 * @return 新 CSGNode（UNION 类型，a 和 b 作为子节点）
 */
CSGNode *geometry_csg_union(CSGNode *a, CSGNode *b);

/**
 * @brief CSG 布尔差集 — a 减去 b
 * @return 新 CSGNode（DIFFERENCE 类型）
 */
CSGNode *geometry_csg_difference(CSGNode *a, CSGNode *b);

/**
 * @brief CSG 布尔交集 — a 与 b 的交集
 * @return 新 CSGNode（INTERSECTION 类型）
 */
CSGNode *geometry_csg_intersection(CSGNode *a, CSGNode *b);

/**
 * @brief 将 CSG 树导出为 OpenSCAD .scad 格式文本
 * @param root  CSG 树根节点
 * @return OpenSCAD 脚本字符串（调用者释放）
 */
char *csg_export_to_openscad(const CSGNode *root);

#ifdef __cplusplus
}
#endif

#endif /* LV00_GEOMETRY_TYPES_H */
