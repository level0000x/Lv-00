# -*- coding: utf-8 -*-
"""Unstatic cross-file helpers in geometry_csg split."""
import io, os
DIR = r"c:\Users\xingg\Desktop\知识体系化Wiki\Lv-00\core\src\layer3_geometry"
def read(p):
    with io.open(p, "rb") as f:
        return f.read().decode("utf-8-sig").replace("\r\n", "\n")
def write(p, s):
    with io.open(p, "w", encoding="utf-8", newline="\n") as f:
        f.write(s)
jobs = [
    ("geometry_csg.c", "static CSGVec3 csg_vec3_cross(CSGVec3 a, CSGVec3 b) {"),
    ("geometry_csg.c", "static double csg_vec3_dot(CSGVec3 a, CSGVec3 b) {"),
    ("geometry_csg.c", "static CSGVec3 csg_vec3_sub(CSGVec3 a, CSGVec3 b) {"),
    ("geometry_csg.c", "static CSGVec3 csg_vec3_add(CSGVec3 a, CSGVec3 b) {"),
    ("geometry_csg.c", "static CSGVec3 csg_vec3_scale(CSGVec3 v, double s) {"),
    ("geometry_csg.c", "static CSGVec3 csg_vec3_normalize(CSGVec3 v) {"),
    ("geometry_csg.c", "static CSGVec3 csg_tri_normal(const CSGTriangle *tri) {"),
    ("geometry_csg.c", "static double csg_signed_distance(CSGVec3 plane_point, CSGVec3 plane_normal, CSGVec3 point) {"),
    ("geometry_csg.c", "static void csg_trilist_init(CSGTriList *list, int init_cap) {"),
    ("geometry_csg.c", "static void csg_trilist_append(CSGTriList *list, const CSGTriangle *tri) {"),
    ("geometry_csg.c", "static void csg_trilist_free(CSGTriList *list) {"),
    ("geometry_csg_mesh.c", "static void csg_gen_sphere_tris(double radius, CSGTriList *out) {"),
    ("geometry_csg_mesh.c", "static void csg_gen_cube_tris(double w, double h, double d, CSGTriList *out) {"),
    ("geometry_csg_mesh.c", "static void csg_gen_cylinder_tris(double radius, double height, CSGTriList *out) {"),
    ("geometry_csg_bsp.c", "static CSGBSPNode *csg_bsp_node_create(void) {"),
    ("geometry_csg_bsp.c", "static void csg_bsp_node_add_tri(CSGBSPNode *node, const CSGTriangle *tri) {"),
    ("geometry_csg_bsp.c", "static void csg_bsp_node_destroy(CSGBSPNode *node) {"),
    ("geometry_csg_bsp.c", "static CSGBSPClass csg_bsp_classify_triangle(const CSGBSPNode *node, const CSGTriangle *tri, double eps) {"),
    ("geometry_csg_bsp.c", "static void csg_bsp_split_triangle(const CSGTriangle *tri, CSGVec3 plane_point, CSGVec3 plane_normal, double eps,"),
    ("geometry_csg_bsp.c", "static CSGBSPNode *csg_bsp_build(CSGTriList *tris, double eps) {"),
    ("geometry_csg_bsp.c", "static void csg_bsp_clip_triangle(const CSGTriangle *tri, const CSGBSPNode *node, CSGTriList *out, double eps, int keep_inside) {"),
    ("geometry_csg_bsp.c", "static void csg_bsp_union_tri(const CSGTriList *list_a, const CSGTriList *list_b, CSGTriList *out) {"),
    ("geometry_csg_bsp.c", "static void csg_bsp_difference_tri(const CSGTriList *list_a, const CSGTriList *list_b, CSGTriList *out) {"),
    ("geometry_csg_bsp.c", "static void csg_bsp_intersection_tri(const CSGTriList *list_a, const CSGTriList *list_b, CSGTriList *out) {"),
    ("geometry_csg_hull.c", "static void csg_primitive_to_tris(const CSGNode *node, CSGTriList *out) {"),
    ("geometry_csg_hull.c", "static void csg_compute_convex_hull(const CSGVec3 *vertices, int vertex_count, CSGTriList *out) {"),
    ("geometry_csg_hull.c", "static void csg_extract_vertices(const CSGTriList *tris, CSGVec3 **out_verts, int *out_count) {"),
    ("geometry_csg_eval.c", "static void eval_csg_primitive(const CSGNode *node, CSGTriList *out) {"),
    ("geometry_csg_eval.c", "static void eval_csg_bool(const CSGNode *node, CSGTriList *out) {"),
    ("geometry_csg_eval.c", "static void eval_csg_transform(const CSGNode *node, CSGTriList *out) {"),
    ("geometry_csg_eval.c", "static void eval_csg_hull(const CSGNode *node, CSGTriList *out) {"),
    ("geometry_csg_eval.c", "static void eval_csg_minkowski(const CSGNode *node, CSGTriList *out) {"),
    ("geometry_csg_eval.c", "static void eval_csg_extrude_linear(const CSGNode *node, CSGTriList *out) {"),
    ("geometry_csg_eval.c", "static void eval_csg_extrude_rotate(const CSGNode *node, CSGTriList *out) {"),
]
count = 0
for fname, sig in jobs:
    p = os.path.join(DIR, fname)
    t = read(p)
    if sig in t:
        t = t.replace(sig, sig.replace("static ", "", 1), 1)
        write(p, t)
        count += 1
    else:
        print("MISS:", fname, sig[:60])
print("unstatic done (%d/%d)" % (count, len(jobs)))
