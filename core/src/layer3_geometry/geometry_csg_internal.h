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

/* ---- constants ---- */
#define CSG_CHILD_CAPACITY_INIT 4
#define CSG_CHILD_CAPACITY_GROW_FACTOR 2
#define CSG_BSP_EPSILON 1e-9
#define CSG_EXPORT_BUF_INIT 4096
#define CSG_TRI_VERT_COUNT 3
#define CSG_MAX_TRI_BUFFER 256

/* ---- internal data structures ---- */
typedef struct {
    double x, y, z;
} CSGVec3;

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

/* mesh generation (geometry_csg_mesh.c) */
void csg_gen_sphere_tris(double radius, CSGTriList *out);
void csg_gen_cube_tris(double w, double h, double d, CSGTriList *out);
void csg_gen_cylinder_tris(double radius, double height, CSGTriList *out);

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
