# -*- coding: utf-8 -*-
"""Split geometry_csg.c (2244 lines) into 9 files + internal header."""
import io
import os

SRC = r"c:\Users\xingg\Desktop\知识体系化Wiki\Lv-00\core\src\layer3_geometry\geometry_csg.c"
DIR = os.path.dirname(SRC)

with io.open(SRC, "rb") as f:
    raw = f.read()
lines = raw.decode("utf-8-sig").splitlines(keepends=True)
assert len(lines) == 2244, "line count mismatch: %d" % len(lines)

def seg(a, b):
    return lines[a - 1:b]

main_parts = [seg(1, 248)]
nod_parts = [seg(249, 359)]
bbx_parts = [seg(360, 452)]
prm_parts = [seg(453, 601)]
msh_parts = [seg(602, 814)]
bsp_parts = [seg(815, 1360)]
hul_parts = [seg(1361, 1554)]
evl_parts = [seg(1555, 2020)]
exp_parts = [seg(2021, 2244)]

all_parts = [main_parts, nod_parts, bbx_parts, prm_parts, msh_parts,
             bsp_parts, hul_parts, evl_parts, exp_parts]
covered = sum(len(p[0]) for p in all_parts)
assert covered == 2244, "coverage: %d" % covered

main_text = "".join(main_parts[0])
main_text = main_text.replace(
    '#include "lv_utils.h"',
    '#include "lv_utils.h"\n#include "geometry_csg_internal.h"', 1)

internal_h = '''/**
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

#ifdef __cplusplus
}
#endif

#endif /* lv_GEOMETRY_CSG_INTERNAL_H */
'''

def file_header(name, desc):
    return '/*\n' \
           ' * @file %(name)s\n' \
           ' * @brief CSG geometry module - %(desc)s\n' \
           ' * @details Split from geometry_csg.c\n' \
           ' */\n\n' \
           '#include "lv/lv_platform.h"\n' \
           '#include <math.h>\n' \
           '#include <stdio.h>\n' \
           '#include <stdlib.h>\n' \
           '#include <string.h>\n\n' \
           '#include "geometry_types.h"\n' \
           '#include "geometry_csg_internal.h"\n' \
           '#include "lv_internal.h"\n' \
           '#include "lv_utils.h"\n\n' % {"name": name, "desc": desc}


def join_parts(parts):
    out = []
    for i, p in enumerate(parts):
        out.append("".join(p))
        if i < len(parts) - 1:
            out.append("\n")
    return "".join(out)


files = {
    "geometry_csg_node.c": (file_header("geometry_csg_node.c", "node lifecycle"), nod_parts),
    "geometry_csg_bbox.c": (file_header("geometry_csg_bbox.c", "bounding box"), bbx_parts),
    "geometry_csg_primitive.c": (file_header("geometry_csg_primitive.c", "primitive creation and boolean ops"), prm_parts),
    "geometry_csg_mesh.c": (file_header("geometry_csg_mesh.c", "triangle mesh generation"), msh_parts),
    "geometry_csg_bsp.c": (file_header("geometry_csg_bsp.c", "BSP tree boolean operations"), bsp_parts),
    "geometry_csg_hull.c": (file_header("geometry_csg_hull.c", "convex hull and vertex extraction"), hul_parts),
    "geometry_csg_eval.c": (file_header("geometry_csg_eval.c", "CSG tree evaluation"), evl_parts),
    "geometry_csg_export.c": (file_header("geometry_csg_export.c", "openscad export and examples"), exp_parts),
}

for fname, (header, parts) in files.items():
    body = header + join_parts(parts)
    with io.open(os.path.join(DIR, fname), "w", encoding="utf-8", newline="\n") as f:
        f.write(body)
    print("written %s (%d lines)" % (fname, body.count("\n") + 1))

with io.open(SRC, "w", encoding="utf-8", newline="\n") as f:
    f.write(main_text)
print("rewritten geometry_csg.c (%d lines)" % (main_text.count("\n") + 1))

with io.open(os.path.join(DIR, "geometry_csg_internal.h"), "w", encoding="utf-8", newline="\n") as f:
    f.write(internal_h)
print("written geometry_csg_internal.h (%d lines)" % (internal_h.count("\n") + 1))

print("DONE")
