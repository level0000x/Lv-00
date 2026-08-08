/**
 * @file geometry_csg_internal.h
 * @brief Internal shared definitions for CSG geometry module.
 */

#ifndef lv_GEOMETRY_CSG_INTERNAL_H
#define lv_GEOMETRY_CSG_INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>

#include "geometry_types.h"
#include "lv_internal.h"
#include "lv_vec3.h" /* 收敛：CSGVec3 统一 typedef 到公共 lvVec3 */
#include "lv/config.h" /* CSG_BSP_EPSILON 语义别名 = lv_EPSILON_MEDIUM */

/* ---- constants ---- */
#define CSG_BSP_EPSILON lv_EPSILON_MEDIUM /* BSP 分割容差（语义别名 = config.h lv_EPSILON_MEDIUM，1e-9） */
#define CSG_EXPORT_BUF_INIT 4096
#define CSG_TRI_VERT_COUNT 3
#define CSG_MAX_TRI_BUFFER 256

/* ---- internal data structures ----
 * CSGVec3 收敛为 lvVec3 的别名（结构 { double x,y,z } 逐位一致，
 * 全部 CSG 调用点无需改动，csg_vec3_* 函数仍保留为薄转发入口） */
typedef lvVec3 CSGVec3;

typedef struct {
    CSGVec3 v[CSG_TRI_VERT_COUNT];
    CSGVec3 normal;
    int face_id;
} CSGTriangle;

typedef struct {
    CSGTriangle *tris;
    int count;
    int capacity;
} CSGTriList;

typedef struct {
    CSGVec3 *vertices;
    int vertex_count;
    int vertex_capacity;
    int *faces;
    int face_count;
    int face_capacity;
} CSGMesh;

/* ---- BSP types (moved from geometry_csg_mesh.c) ---- */
typedef enum {
    CSG_BSP_FRONT = 0,
    CSG_BSP_BACK  = 1,
    CSG_BSP_ON    = 2,
    CSG_BSP_SPLIT = 3
} CSGBSPClass;

typedef struct CSGBSPNode {
    CSGVec3 plane_point;
    CSGVec3 plane_normal;
    struct CSGBSPNode *front;
    struct CSGBSPNode *back;
    CSGTriangle *tris;
    int tri_count;
    int tri_capacity;
} CSGBSPNode;

/* eval function pointer table entry (geometry_csg_eval.c) */
typedef void (*CSGEvalFunc)(const CSGNode *node, CSGTriList *out);

/* ---- 图元类型枚举（与 node->data.prim.type 取值一致） ---- */
typedef enum {
    CSG_PRIM_SPHERE = 0,
    CSG_PRIM_CUBE = 1,
    CSG_PRIM_CYLINDER = 2,
    CSG_PRIM_CONE = 3
} CSGPrimKind;

/* ---- 图元统一分派 vtable（表定义于 geometry_csg_primitive.c） ----
 * gen_tris / bbox 分别与 geometry_csg_mesh.c 的 csg_gen_* 系列、
 * geometry_csg_bbox.c 的 csg_bbox_* 系列签名对齐（均返回 void）；
 * 由于各图元散参个数不同，gen_tris 统一为 (node, out) 签名，
 * 由实现层解包 node->data.prim.params。
 */
typedef struct CSGPrimOps {
    CSGPrimKind kind;                     /* 图元类型（与 node->data.prim.type 一致） */
    const char *name;                     /* 内部名称 */
    void (*gen_tris)(const CSGNode *node, CSGTriList *out); /* 三角面生成 */
    void (*bbox)(CSGNode *node);          /* 包围盒计算 */
    const char *scad_name;                /* OpenSCAD 导出基元名 */
    int param_count;                      /* params 有效参数个数 */
} CSGPrimOps;

extern const CSGPrimOps s_prim_ops[];
extern const int s_prim_ops_count;

/* ---- shared helpers (defined in geometry_csg.c) ---- */
CSGVec3 csg_vec3_cross(CSGVec3 a, CSGVec3 b);
double csg_vec3_dot(CSGVec3 a, CSGVec3 b);
CSGVec3 csg_vec3_sub(CSGVec3 a, CSGVec3 b);
CSGVec3 csg_vec3_add(CSGVec3 a, CSGVec3 b);
CSGVec3 csg_vec3_scale(CSGVec3 v, double s);
CSGVec3 csg_vec3_normalize(CSGVec3 v);
CSGVec3 csg_tri_normal(const CSGTriangle *tri);
double csg_signed_distance(CSGVec3 plane_point, CSGVec3 plane_normal, CSGVec3 point);
void csg_trilist_init(CSGTriList *list, int init_cap);
void csg_trilist_append(CSGTriList *list, const CSGTriangle *tri);
void csg_trilist_free(CSGTriList *list);

/* mesh generation (geometry_csg_mesh.c；圆锥实现于 geometry_csg_primitive.c) */
void csg_gen_sphere_tris(double radius, CSGTriList *out);
void csg_gen_cube_tris(double w, double h, double d, CSGTriList *out);
void csg_gen_cylinder_tris(double radius, double height, CSGTriList *out);
void csg_gen_cone_tris(double radius1, double radius2, double height, CSGTriList *out);

/* ---- 网格骨架共享（geometry_csg_mesh.c；圆柱/圆锥/球体共用） ----
 * csg_emit_quad / csg_gen_lathe_side_tris / csg_gen_disk_tris 为圆柱、圆锥、
 * 球体的网格生成提取的公共骨架：四角网格扇区、车削体侧带（常数/线性半径）、
 * 圆盘扇形（CCW/CW 绕序）。行为与收敛前逐位一致
 * （三角顶点顺序、winding、face_id 分配均不变）。 */
#define CSG_DISK_CCW 1 /**< 圆盘扇形 CCW：顶点序 (center, p1, p2)，法线朝 +Z（顶面） */
#define CSG_DISK_CW -1 /**< 圆盘扇形 CW ：顶点序 (center, p2, p1)，法线朝 -Z（底面） */

void csg_emit_quad(CSGTriList *out, CSGVec3 v00, CSGVec3 v01, CSGVec3 v10, CSGVec3 v11);
void csg_gen_lathe_side_tris(double r1, double r2, double z1, double z2, int slices, CSGTriList *out);
void csg_gen_disk_tris(CSGVec3 center, double radius, double z, int slices, int winding, CSGTriList *out);

/* bounding box (geometry_csg_bbox.c，供图元 vtable 引用) */
void csg_bbox_sphere(CSGNode *node);
void csg_bbox_cube(CSGNode *node);
void csg_bbox_cylinder(CSGNode *node);
void csg_bbox_cone(CSGNode *node);

/* BSP tree (geometry_csg_bsp.c) */
CSGBSPNode *csg_bsp_node_create(void);
void csg_bsp_node_add_tri(CSGBSPNode *node, const CSGTriangle *tri);
void csg_bsp_node_destroy(CSGBSPNode *node);
CSGBSPClass csg_bsp_classify_triangle(const CSGBSPNode *node, const CSGTriangle *tri, double eps);
void csg_bsp_split_triangle(const CSGTriangle *tri, CSGVec3 plane_point, CSGVec3 plane_normal, double eps,
                            CSGTriList *front_list, CSGTriList *back_list);
CSGBSPNode *csg_bsp_build(CSGTriList *tris, double eps);
void csg_bsp_clip_triangle(const CSGTriangle *tri, const CSGBSPNode *node, CSGTriList *out, double eps, int keep_inside);
void csg_bsp_union_tri(const CSGTriList *list_a, const CSGTriList *list_b, CSGTriList *out);
void csg_bsp_difference_tri(const CSGTriList *list_a, const CSGTriList *list_b, CSGTriList *out);
void csg_bsp_intersection_tri(const CSGTriList *list_a, const CSGTriList *list_b, CSGTriList *out);

/* hull / vertex extraction (geometry_csg_hull.c) */
void csg_primitive_to_tris(const CSGNode *node, CSGTriList *out);
void csg_compute_convex_hull(const CSGVec3 *vertices, int vertex_count, CSGTriList *out);
void csg_extract_vertices(const CSGTriList *tris, CSGVec3 **out_verts, int *out_count);

/* tree evaluation (geometry_csg_eval.c) */
void eval_csg_primitive(const CSGNode *node, CSGTriList *out);
void eval_csg_bool(const CSGNode *node, CSGTriList *out);
void eval_csg_transform(const CSGNode *node, CSGTriList *out);
void eval_csg_hull(const CSGNode *node, CSGTriList *out);
void eval_csg_minkowski(const CSGNode *node, CSGTriList *out);
void eval_csg_extrude_linear(const CSGNode *node, CSGTriList *out);
void eval_csg_extrude_rotate(const CSGNode *node, CSGTriList *out);
extern CSGEvalFunc s_eval_funcs[];
extern const int s_eval_func_count;

/* entry point (geometry_csg_export.c) */
void csg_evaluate(const CSGNode *node, CSGTriList *out);

#ifdef __cplusplus
}
#endif

#endif /* lv_GEOMETRY_CSG_INTERNAL_H */
