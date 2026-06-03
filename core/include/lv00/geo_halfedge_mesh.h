/**
 * @file geo_halfedge_mesh.h
 * @brief Halfedge 网格拓扑数据结构 —— 借鉴 geometry-central
 *
 * 借鉴来源：
 *   - geometry-central (github.com/nmwsharp/geometry-central)
 *     Halfedge 数据结构、表面网格抽象、几何量计算
 *
 * 设计目标：
 *   - 提供统一的表面网格拓扑表示
 *   - 支持高效的顶点/边/面遍历
 *   - 与现有 Layer 3 几何系统无缝集成
 *
 * 版本：v3.6.0（第十三梯队 geometry-central Halfedge 拓扑落地）
 */

#ifndef LV00_GEO_HALFEDGE_MESH_H
#define LV00_GEO_HALFEDGE_MESH_H

#include <stdbool.h>
#include <stdint.h>

#include "lv00.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 第一部分：Halfedge 基础类型
 * ======================================================================== */

/**
 * @brief 半边标识符
 */
typedef int Lv00Halfedge;

/**
 * @brief 顶点标识符
 */
typedef int Lv00Vertex;

/**
 * @brief 边标识符
 */
typedef int Lv00Edge;

/**
 * @brief 面标识符
 */
typedef int Lv00Face;

/**
 * @brief 无效标识符常量
 */
#define LV00_HE_INVALID (-1)

/* ========================================================================
 * 第二部分：几何量存储
 * ======================================================================== */

/**
 * @brief 2D/3D 点坐标联合体
 */
typedef struct Lv00Point2D {
    double x;
    double y;
} Lv00Point2D;

/**
 * @brief 3D 点坐标
 */
typedef struct Lv00Point3D {
    double x;
    double y;
    double z;
} Lv00Point3D;

/**
 * @brief 顶点数据
 */
typedef struct Lv00VertexData {
    Lv00Point3D position;     /**< 位置 */
    Lv00Point3D normal;       /**< 法向量 */
    double curvature;        /**< 曲率 */
    double weight;          /**< 权重（用于热方法等算法） */
} Lv00VertexData;

/**
 * @brief 半边数据（额外属性）
 */
typedef struct Lv00HalfedgeData {
    double length;           /**< 边长 */
    double angle;            /**< 拐角角度 */
} Lv00HalfedgeData;

/**
 * @brief 面数据
 */
typedef struct Lv00FaceData {
    Lv00Point3D normal;      /**< 面法向量 */
    double area;             /**< 面积 */
    int valence;             /**< 价（顶点度数） */
} Lv00FaceData;

/* ========================================================================
 * 第三部分：Halfedge 网格结构
 * ======================================================================== */

/**
 * @brief Halfedge 网格配置
 */
typedef struct Lv00HeMeshConfig {
    int initial_capacity;    /**< 初始容量 */
    int max_faces_per_edge;  /**< 每条边最大面数（2 表示流形） */
    bool maintain_normals;   /**< 是否维护法向量 */
    bool maintain_curvature; /**< 是否维护曲率 */
} Lv00HeMeshConfig;

/**
 * @brief Halfedge 网格数据结构
 *
 * 存储格式（CSR 风格压缩存储）：
 *   - vertices: 顶点数组，每个顶点存储一个 outgoing halfedge
 *   - halfedges: 半边数组，每个半边存储 (twin, next, face, vertex)
 *   - edges: 边数组，每条边存储第一个半边
 *   - faces: 面数组，每个面存储第一个半边
 */
typedef struct Lv00HeMesh {
    /* 顶点数据 */
    Lv00VertexData *vertex_data;       /**< 顶点数据数组 */
    Lv00Halfedge *vertex_out_he;        /**< 顶点 -> outgoing halfedge */
    int vertex_count;                   /**< 顶点数量 */
    int vertex_capacity;                /**< 顶点容量 */

    /* 半边数据 */
    Lv00Halfedge *he_twin;              /**< 半边 -> 对向半边 */
    Lv00Halfedge *he_next;              /**< 半边 -> 面内下一半边 */
    Lv00Halfedge *he_prev;              /**< 半边 -> 面内前一半边 */
    Lv00Face *he_face;                  /**< 半边 -> 所属面 */
    Lv00Vertex *he_vertex;               /**< 半边 -> 起始顶点 */
    Lv00HalfedgeData *he_data;          /**< 半边额外数据 */
    int halfedge_count;                  /**< 半边数量 */
    int halfedge_capacity;               /**< 半边容量 */

    /* 边数据 */
    Lv00Halfedge *edge_he;              /**< 边 -> 第一个半边 */
    int edge_count;                      /**< 边数量 */
    int edge_capacity;                   /**< 边容量 */

    /* 面数据 */
    Lv00Halfedge *face_he;              /**< 面 -> 第一个半边 */
    Lv00FaceData *face_data;            /**< 面数据数组 */
    int face_count;                      /**< 面数量 */
    int face_capacity;                   /**< 面容量 */

    /* 配置 */
    Lv00HeMeshConfig config;             /**< 网格配置 */

    /* 统计 */
    int64_t operation_count;             /**< 操作计数 */
} Lv00HeMesh;

/* ========================================================================
 * 第四部分：Halfedge 网格操作 API
 * ======================================================================== */

/**
 * @brief 获取默认网格配置
 */
LV00_PUBLIC_API Lv00HeMeshConfig lv00_he_mesh_default_config(void);

/**
 * @brief 创建 Halfedge 网格
 * @param config 配置（NULL 使用默认配置）
 * @return 网格指针（失败返回 NULL）
 */
LV00_PUBLIC_API Lv00HeMesh *lv00_he_mesh_create(const Lv00HeMeshConfig *config);

/**
 * @brief 释放 Halfedge 网格
 * @param mesh 网格指针
 */
LV00_PUBLIC_API void lv00_he_mesh_free(Lv00HeMesh *mesh);

/**
 * @brief 清空网格内容
 * @param mesh 网格指针
 */
LV00_PUBLIC_API void lv00_he_mesh_clear(Lv00HeMesh *mesh);

/* ========================================================================
 * 第五部分：顶点和半边操作
 * ======================================================================== */

/**
 * @brief 添加顶点
 * @param mesh 网格
 * @param x X 坐标
 * @param y Y 坐标
 * @param z Z 坐标
 * @return 顶点 ID（LV00_HE_INVALID 表示失败）
 */
LV00_PUBLIC_API Lv00Vertex lv00_he_mesh_add_vertex(
    Lv00HeMesh *mesh,
    double x, double y, double z);

/**
 * @brief 获取顶点位置
 * @param mesh 网格
 * @param v 顶点 ID
 * @return 顶点位置
 */
LV00_PUBLIC_API Lv00Point3D lv00_he_mesh_get_vertex_position(
    const Lv00HeMesh *mesh,
    Lv00Vertex v);

/**
 * @brief 设置顶点位置
 * @param mesh 网格
 * @param v 顶点 ID
 * @param pos 新位置
 */
LV00_PUBLIC_API void lv00_he_mesh_set_vertex_position(
    Lv00HeMesh *mesh,
    Lv00Vertex v,
    Lv00Point3D pos);

/**
 * @brief 获取顶点的 outgoing halfedge
 * @param mesh 网格
 * @param v 顶点 ID
 * @return 半边 ID（LV00_HE_INVALID 表示无半边）
 */
LV00_PUBLIC_API Lv00Halfedge lv00_he_mesh_vertex_out_halfedge(
    const Lv00HeMesh *mesh,
    Lv00Vertex v);

/**
 * @brief 获取半边的起始顶点
 * @param mesh 网格
 * @param he 半边 ID
 * @return 顶点 ID
 */
LV00_PUBLIC_API Lv00Vertex lv00_he_mesh_halfedge_vertex(
    const Lv00HeMesh *mesh,
    Lv00Halfedge he);

/**
 * @brief 获取半边的 twin（对向半边）
 * @param mesh 网格
 * @param he 半边 ID
 * @return twin 半边 ID
 */
LV00_PUBLIC_API Lv00Halfedge lv00_he_mesh_halfedge_twin(
    const Lv00HeMesh *mesh,
    Lv00Halfedge he);

/**
 * @brief 获取半边的 next（面内下一半边）
 * @param mesh 网格
 * @param he 半边 ID
 * @return 下一半边 ID
 */
LV00_PUBLIC_API Lv00Halfedge lv00_he_mesh_halfedge_next(
    const Lv00HeMesh *mesh,
    Lv00Halfedge he);

/**
 * @brief 获取半边的 face（所属面）
 * @param mesh 网格
 * @param he 半边 ID
 * @return 面 ID
 */
LV00_PUBLIC_API Lv00Face lv00_he_mesh_halfedge_face(
    const Lv00HeMesh *mesh,
    Lv00Halfedge he);

/* ========================================================================
 * 第六部分：边操作
 * ======================================================================== */

/**
 * @brief 根据两个顶点获取边
 * @param mesh 网格
 * @param v1 第一个顶点
 * @param v2 第二个顶点
 * @return 边 ID（不存在返回 LV00_HE_INVALID）
 */
LV00_PUBLIC_API Lv00Edge lv00_he_mesh_find_edge(
    const Lv00HeMesh *mesh,
    Lv00Vertex v1,
    Lv00Vertex v2);

/**
 * @brief 获取边对应的半边
 * @param mesh 网格
 * @param e 边 ID
 * @return 半边 ID
 */
LV00_PUBLIC_API Lv00Halfedge lv00_he_mesh_edge_halfedge(
    const Lv00HeMesh *mesh,
    Lv00Edge e);

/**
 * @brief 获取边长度
 * @param mesh 网格
 * @param e 边 ID
 * @return 边长度
 */
LV00_PUBLIC_API double lv00_he_mesh_edge_length(
    const Lv00HeMesh *mesh,
    Lv00Edge e);

/**
 * @brief 获取边的两个顶点
 * @param mesh 网格
 * @param e 边 ID
 * @param out_v1 输出第一个顶点
 * @param out_v2 输出第二个顶点
 */
LV00_PUBLIC_API void lv00_he_mesh_edge_vertices(
    const Lv00HeMesh *mesh,
    Lv00Edge e,
    Lv00Vertex *out_v1,
    Lv00Vertex *out_v2);

/* ========================================================================
 * 第七部分：面操作
 * ======================================================================== */

/**
 * @brief 添加三角形面
 * @param mesh 网格
 * @param v1 第一个顶点
 * @param v2 第二个顶点
 * @param v3 第三个顶点
 * @return 面 ID（LV00_HE_INVALID 表示失败）
 */
LV00_PUBLIC_API Lv00Face lv00_he_mesh_add_face_triangle(
    Lv00HeMesh *mesh,
    Lv00Vertex v1,
    Lv00Vertex v2,
    Lv00Vertex v3);

/**
 * @brief 添加四边形面
 * @param mesh 网格
 * @param v1 第一个顶点
 * @param v2 第二个顶点
 * @param v3 第三个顶点
 * @param v4 第四个顶点
 * @return 面 ID（LV00_HE_INVALID 表示失败）
 */
LV00_PUBLIC_API Lv00Face lv00_he_mesh_add_face_quad(
    Lv00HeMesh *mesh,
    Lv00Vertex v1,
    Lv00Vertex v2,
    Lv00Vertex v3,
    Lv00Vertex v4);

/**
 * @brief 获取面的第一个半边
 * @param mesh 网格
 * @param f 面 ID
 * @return 半边 ID
 */
LV00_PUBLIC_API Lv00Halfedge lv00_he_mesh_face_halfedge(
    const Lv00HeMesh *mesh,
    Lv00Face f);

/**
 * @brief 获取面的法向量
 * @param mesh 网格
 * @param f 面 ID
 * @return 法向量
 */
LV00_PUBLIC_API Lv00Point3D lv00_he_mesh_face_normal(
    const Lv00HeMesh *mesh,
    Lv00Face f);

/**
 * @brief 获取面的面积
 * @param mesh 网格
 * @param f 面 ID
 * @return 面积
 */
LV00_PUBLIC_API double lv00_he_mesh_face_area(
    const Lv00HeMesh *mesh,
    Lv00Face f);

/**
 * @brief 获取面的顶点数
 * @param mesh 网格
 * @param f 面 ID
 * @return 顶点数（三角形为 3）
 */
LV00_PUBLIC_API int lv00_he_mesh_face_valence(
    const Lv00HeMesh *mesh,
    Lv00Face f);

/**
 * @brief 获取面的所有顶点
 * @param mesh 网格
 * @param f 面 ID
 * @param out_vertices 输出顶点数组（需 pre-allocated，大小 >= 4）
 * @return 顶点数
 */
LV00_PUBLIC_API int lv00_he_mesh_face_vertices(
    const Lv00HeMesh *mesh,
    Lv00Face f,
    Lv00Vertex *out_vertices);

/* ========================================================================
 * 第八部分：遍历工具
 * ======================================================================== */

/**
 * @brief 顶点邻接半边迭代器上下文
 */
typedef struct Lv00HeVertexIterator {
    const Lv00HeMesh *mesh;
    Lv00Halfedge current;
    int count;
    int index;
} Lv00HeVertexIterator;

/**
 * @brief 初始化顶点邻接半边迭代器
 * @param mesh 网格
 * @param v 顶点
 * @return 迭代器
 */
LV00_PUBLIC_API Lv00HeVertexIterator lv00_he_vertex_iter_begin(
    const Lv00HeMesh *mesh,
    Lv00Vertex v);

/**
 * @brief 获取迭代器当前半边
 * @param iter 迭代器
 * @return 半边 ID
 */
LV00_PUBLIC_API Lv00Halfedge lv00_he_vertex_iter_get(
    const Lv00HeVertexIterator *iter);

/**
 * @brief 迭代器是否有效
 * @param iter 迭代器
 * @return 是否有效
 */
LV00_PUBLIC_API bool lv00_he_vertex_iter_valid(
    const Lv00HeVertexIterator *iter);

/**
 * @brief 移动迭代器到下一条邻接半边
 * @param iter 迭代器
 */
LV00_PUBLIC_API void lv00_he_vertex_iter_next(
    Lv00HeVertexIterator *iter);

/**
 * @brief 宏：遍历顶点的所有邻接半边
 *
 * 用法示例：
 *   Lv00HeVertexIter it;
 *   lv00_he_iter_vertex_out_halfedges(mesh, v, it) {
 *       Lv00Halfedge he = lv00_he_iter_get(it);
 *       // 处理半边
 *   }
 */
#define LV00_HE_ITER_VERTEX_OUT_HALFEDGES(mesh, v, iter) \
    for ((iter) = lv00_he_vertex_iter_begin((mesh), (v)); \
         lv00_he_vertex_iter_valid(&(iter)); \
         lv00_he_vertex_iter_next(&(iter)))

/**
 * @brief 面邻接半边迭代器上下文
 */
typedef struct Lv00HeFaceIterator {
    const Lv00HeMesh *mesh;
    Lv00Halfedge current;
    int count;
    int index;
} Lv00HeFaceIterator;

/**
 * @brief 初始化面邻接半边迭代器
 * @param mesh 网格
 * @param f 面
 * @return 迭代器
 */
LV00_PUBLIC_API Lv00HeFaceIterator lv00_he_face_iter_begin(
    const Lv00HeMesh *mesh,
    Lv00Face f);

/**
 * @brief 获取迭代器当前半边
 * @param iter 迭代器
 * @return 半边 ID
 */
LV00_PUBLIC_API Lv00Halfedge lv00_he_face_iter_get(
    const Lv00HeFaceIterator *iter);

/**
 * @brief 迭代器是否有效
 * @param iter 迭代器
 * @return 是否有效
 */
LV00_PUBLIC_API bool lv00_he_face_iter_valid(
    const Lv00HeFaceIterator *iter);

/**
 * @brief 移动迭代器到下一条半边
 * @param iter 迭代器
 */
LV00_PUBLIC_API void lv00_he_face_iter_next(
    Lv00HeFaceIterator *iter);

/* ========================================================================
 * 第九部分：几何量计算
 * ======================================================================== */

/**
 * @brief 计算顶点处的角度（以该顶点为一角的所有角的总和）
 * @param mesh 网格
 * @param v 顶点
 * @return 角度（弧度）
 */
LV00_PUBLIC_API double lv00_he_mesh_vertex_angle(
    const Lv00HeMesh *mesh,
    Lv00Vertex v);

/**
 * @brief 计算顶点处的离散曲率
 * @param mesh 网格
 * @param v 顶点
 * @return 曲率值
 */
LV00_PUBLIC_API double lv00_he_mesh_vertex_curvature(
    const Lv00HeMesh *mesh,
    Lv00Vertex v);

/**
 * @brief 计算顶点法向量（面积加权）
 * @param mesh 网格
 * @param v 顶点
 * @return 法向量
 */
LV00_PUBLIC_API Lv00Point3D lv00_he_mesh_vertex_normal(
    const Lv00HeMesh *mesh,
    Lv00Vertex v);

/**
 * @brief 计算两条半边（边）的夹角
 * @param mesh 网格
 * @param he1 第一条半边
 * @param he2 第二条半边（应共享终点）
 * @return 夹角（弧度）
 */
LV00_PUBLIC_API double lv00_he_mesh_halfedge_angle(
    const Lv00HeMesh *mesh,
    Lv00Halfedge he1,
    Lv00Halfedge he2);

/**
 * @brief 计算半边处的拐角角度
 * @param mesh 网格
 * @param he 半边
 * @return 拐角角度（弧度）
 */
LV00_PUBLIC_API double lv00_he_mesh_halfedge_corner_angle(
    const Lv00HeMesh *mesh,
    Lv00Halfedge he);

/**
 * @brief 更新所有几何量（法向量、曲率等）
 * @param mesh 网格
 */
LV00_PUBLIC_API void lv00_he_mesh_update_geometry(Lv00HeMesh *mesh);

/* ========================================================================
 * 第十部分：网格查询
 * ======================================================================== */

/**
 * @brief 查询点最近的顶点
 * @param mesh 网格
 * @param point 查询点
 * @param out_distance 输出最近距离（可为 NULL）
 * @return 最近顶点 ID
 */
LV00_PUBLIC_API Lv00Vertex lv00_he_mesh_nearest_vertex(
    const Lv00HeMesh *mesh,
    Lv00Point3D point,
    double *out_distance);

/**
 * @brief 查询点在哪个面内（射线投影法）
 * @param mesh 网格
 * @param point 查询点
 * @param out_barycentric 输出重心坐标（可为 NULL）
 * @return 面 ID（不在任何面内返回 LV00_HE_INVALID）
 */
LV00_PUBLIC_API Lv00Face lv00_he_mesh_point_in_face(
    const Lv00HeMesh *mesh,
    Lv00Point3D point,
    double *out_barycentric);

/**
 * @brief 计算网格的总表面积
 * @param mesh 网格
 * @return 总面积
 */
LV00_PUBLIC_API double lv00_he_mesh_total_area(const Lv00HeMesh *mesh);

/**
 * @brief 计算网格的 Euler 特征数 (V - E + F)
 * @param mesh 网格
 * @return Euler 特征数
 */
LV00_PUBLIC_API int lv00_he_mesh_euler_characteristic(const Lv00HeMesh *mesh);

/* ========================================================================
 * 第十一部分：统计与调试
 * ======================================================================== */

/**
 * @brief 网格统计信息
 */
typedef struct Lv00HeMeshStats {
    int vertex_count;
    int edge_count;
    int halfedge_count;
    int face_count;
    double total_area;
    int euler_characteristic;
    int max_vertex_valence;
    double average_edge_length;
} Lv00HeMeshStats;

/**
 * @brief 获取网格统计信息
 * @param mesh 网格
 * @param out_stats 输出统计
 */
LV00_PUBLIC_API void lv00_he_mesh_get_stats(
    const Lv00HeMesh *mesh,
    Lv00HeMeshStats *out_stats);

/**
 * @brief 验证网格拓扑一致性
 * @param mesh 网格
 * @return 一致返回 true
 */
LV00_PUBLIC_API bool lv00_he_mesh_validate(const Lv00HeMesh *mesh);

#ifdef __cplusplus
}
#endif

#endif /* LV00_GEO_HALFEDGE_MESH_H */
