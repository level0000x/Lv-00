/*
 * @file geometry_csg_eval.c
 * @brief CSG geometry module - CSG tree evaluation
 * @details Split from geometry_csg.c
 */

#include "lv/lv_platform.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "geometry_types.h"
#include "geometry_csg_internal.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "lv/geo_utils.h" /* geo_norm_3d / geo_norm_sq_3d（向量模长统一工具） */
#include "lv/lv_numeric.h"

void csg_evaluate(const CSGNode *node, CSGTriList *out);

void eval_csg_primitive(const CSGNode *node, CSGTriList *out) {
    csg_primitive_to_tris(node, out);
}

/* ── CSG 布尔运算：kind → BSP 求值函数 查找表（按 CSGNodeKind 值索引，同 s_eval_funcs 风格） ── */
typedef void (*CSGBoolOpFunc)(const CSGTriList *list_a, const CSGTriList *list_b, CSGTriList *out);

static CSGBoolOpFunc s_bool_op_funcs[] = {
    [CSG_NODE_UNION] = csg_bsp_union_tri,
    [CSG_NODE_DIFFERENCE] = csg_bsp_difference_tri,
    [CSG_NODE_INTERSECTION] = csg_bsp_intersection_tri,
};
static const int s_bool_op_count = (int)(sizeof(s_bool_op_funcs) / sizeof(s_bool_op_funcs[0]));
void eval_csg_bool(const CSGNode *node, CSGTriList *out) {
    if (node->child_count < 2) {
        if (node->child_count == 1) {
            csg_evaluate(node->children[0], out);
        }
        return;
    }

    CSGTriList tris_a;
    csg_trilist_init(&tris_a, CSG_MAX_TRI_BUFFER);
    csg_evaluate(node->children[0], &tris_a);

    CSGTriList tris_b;
    csg_trilist_init(&tris_b, CSG_MAX_TRI_BUFFER);
    csg_evaluate(node->children[1], &tris_b);

    /* 布尔运算 kind → BSP 求值函数，统一走查找表 */
    if (lv_index_in_range(node->kind, s_bool_op_count) && s_bool_op_funcs[node->kind]) {
        s_bool_op_funcs[node->kind](&tris_a, &tris_b, out);
    }

    csg_trilist_free(&tris_a);
    csg_trilist_free(&tris_b);
}

void eval_csg_transform(const CSGNode *node, CSGTriList *out) {
    if (node->child_count < 1)
        return;

    CSGTriList child_tris;
    csg_trilist_init(&child_tris, CSG_MAX_TRI_BUFFER);
    csg_evaluate(node->children[0], &child_tris);

    const double (*M)[4] = node->transform;

    double m00 = M[0][0], m01 = M[0][1], m02 = M[0][2];
    double m10 = M[1][0], m11 = M[1][1], m12 = M[1][2];
    double m20 = M[2][0], m21 = M[2][1], m22 = M[2][2];

    double det = m00 * (m11 * m22 - m12 * m21) - m01 * (m10 * m22 - m12 * m20) + m02 * (m10 * m21 - m11 * m20);

    /* 计算矩阵元素绝对值的最大值（用于行列式容差缩放） */
    const double m_els[9] = {m00, m01, m02,
                             m10, m11, m12,
                             m20, m21, m22};
    double max_el = lv_max_abs(m_els, 9);
    double det_tol = lv_rel_tol_scale(CSG_BSP_EPSILON, max_el * max_el);

    double inv_det;
    double invT[3][3];
    if (fabs(det) < det_tol) {
        invT[0][0] = 1.0; invT[0][1] = 0.0; invT[0][2] = 0.0;
        invT[1][0] = 0.0; invT[1][1] = 1.0; invT[1][2] = 0.0;
        invT[2][0] = 0.0; invT[2][1] = 0.0; invT[2][2] = 1.0;
    } else {
        inv_det = 1.0 / det;
        invT[0][0] = (m11 * m22 - m12 * m21) * inv_det;
        invT[0][1] = (m02 * m21 - m01 * m22) * inv_det;
        invT[0][2] = (m01 * m12 - m02 * m11) * inv_det;
        invT[1][0] = (m12 * m20 - m10 * m22) * inv_det;
        invT[1][1] = (m00 * m22 - m02 * m20) * inv_det;
        invT[1][2] = (m02 * m10 - m00 * m12) * inv_det;
        invT[2][0] = (m10 * m21 - m11 * m20) * inv_det;
        invT[2][1] = (m01 * m20 - m00 * m21) * inv_det;
        invT[2][2] = (m00 * m11 - m01 * m10) * inv_det;
    }

    for (int i = 0; i < child_tris.count; i++) {
        CSGTriangle tri = child_tris.tris[i];
        for (int v = 0; v < 3; v++) {
            double x = tri.v[v].x;
            double y = tri.v[v].y;
            double z = tri.v[v].z;
            double w = M[3][0] * x + M[3][1] * y + M[3][2] * z + M[3][3];
            double inv_w = (fabs(w - 1.0) > CSG_BSP_EPSILON) ? 1.0 / w : 1.0;
            tri.v[v].x = (M[0][0] * x + M[0][1] * y + M[0][2] * z + M[0][3]) * inv_w;
            tri.v[v].y = (M[1][0] * x + M[1][1] * y + M[1][2] * z + M[1][3]) * inv_w;
            tri.v[v].z = (M[2][0] * x + M[2][1] * y + M[2][2] * z + M[2][3]) * inv_w;
        }
        double nx = tri.normal.x, ny = tri.normal.y, nz = tri.normal.z;
        tri.normal.x = invT[0][0] * nx + invT[0][1] * ny + invT[0][2] * nz;
        tri.normal.y = invT[1][0] * nx + invT[1][1] * ny + invT[1][2] * nz;
        tri.normal.z = invT[2][0] * nx + invT[2][1] * ny + invT[2][2] * nz;
        tri.normal = csg_vec3_normalize(tri.normal);
        tri.face_id = out->count;
        csg_trilist_append(out, &tri);
    }
    csg_trilist_free(&child_tris);
}
void eval_csg_hull(const CSGNode *node, CSGTriList *out) {
    if (node->child_count < 1)
        return;

    CSGTriList all_tris;
    csg_trilist_init(&all_tris, CSG_MAX_TRI_BUFFER * node->child_count);
    for (int i = 0; i < node->child_count; i++) {
        csg_evaluate(node->children[i], &all_tris);
    }

    CSGVec3 *hull_verts = NULL;
    int hull_vert_count = 0;
    csg_extract_vertices(&all_tris, &hull_verts, &hull_vert_count);

    if (hull_verts && hull_vert_count >= 4) {
        csg_compute_convex_hull(hull_verts, hull_vert_count, out);
    }

    if (hull_verts) {
        lv_free((void **) &hull_verts);
    }
    csg_trilist_free(&all_tris);
}

void eval_csg_minkowski(const CSGNode *node, CSGTriList *out) {
    if (node->child_count < 2)
        return;

    CSGTriList tris_a, tris_b;
    csg_trilist_init(&tris_a, CSG_MAX_TRI_BUFFER);
    csg_trilist_init(&tris_b, CSG_MAX_TRI_BUFFER);
    csg_evaluate(node->children[0], &tris_a);
    csg_evaluate(node->children[1], &tris_b);

    CSGVec3 *verts_a = NULL, *verts_b = NULL;
    int count_a = 0, count_b = 0;
    csg_extract_vertices(&tris_a, &verts_a, &count_a);
    csg_extract_vertices(&tris_b, &verts_b, &count_b);

    if (verts_a && verts_b && count_a > 0 && count_b > 0) {
        if (count_a <= INT_MAX / count_b) {
            int sum_count = count_a * count_b;
            CSGVec3 *sum_verts = (CSGVec3 *) lv_calloc((size_t) sum_count, sizeof(CSGVec3));
            if (sum_verts) {
                int idx = 0;
                for (int i = 0; i < count_a; i++) {
                    for (int j = 0; j < count_b; j++) {
                        sum_verts[idx] = csg_vec3_add(verts_a[i], verts_b[j]);
                        idx++;
                    }
                }
                csg_compute_convex_hull(sum_verts, sum_count, out);
                lv_free((void **) &sum_verts);
            }
        }
    }

    if (verts_a)
        lv_free((void **) &verts_a);
    if (verts_b)
        lv_free((void **) &verts_b);
    csg_trilist_free(&tris_a);
    csg_trilist_free(&tris_b);
}

void eval_csg_extrude_linear(const CSGNode *node, CSGTriList *out) {
    if (node->child_count < 1)
        return;

    CSGTriList section_tris;
    csg_trilist_init(&section_tris, CSG_MAX_TRI_BUFFER);
    csg_evaluate(node->children[0], &section_tris);

    double height = node->data.prim.params[0];
    if (height <= 0.0)
        height = 1.0;

    double dir_x = node->data.prim.params[1];
    double dir_y = node->data.prim.params[2];
    double dir_z = node->data.prim.params[3];

    CSGVec3 dir = {dir_x, dir_y, dir_z};
    double dir_len = geo_norm_3d(dir.x, dir.y, dir.z);
    if (dir_len < CSG_BSP_EPSILON) {
        dir.x = 0.0; dir.y = 0.0; dir.z = 1.0;
    } else {
        dir.x /= dir_len; dir.y /= dir_len; dir.z /= dir_len;
    }

    CSGVec3 offset = csg_vec3_scale(dir, height);

    CSGVec3 *section_verts = NULL;
    int section_vert_count = 0;
    csg_extract_vertices(&section_tris, &section_verts, &section_vert_count);

    for (int i = 0; i < section_tris.count; i++) {
        CSGTriangle tri = section_tris.tris[i];
        CSGVec3 flip_n = csg_vec3_scale(tri.normal, -1.0);
        if (csg_vec3_dot(flip_n, dir) < 0.0)
            flip_n = csg_vec3_scale(flip_n, -1.0);
        tri.normal = csg_vec3_normalize(flip_n);
        tri.face_id = out->count;
        csg_trilist_append(out, &tri);
    }

    for (int i = 0; i < section_tris.count; i++) {
        CSGTriangle tri = section_tris.tris[i];
        tri.v[0] = csg_vec3_add(tri.v[0], offset);
        tri.v[1] = csg_vec3_add(tri.v[1], offset);
        tri.v[2] = csg_vec3_add(tri.v[2], offset);
        if (csg_vec3_dot(tri.normal, dir) < 0.0)
            tri.normal = csg_vec3_scale(tri.normal, -1.0);
        tri.normal = csg_vec3_normalize(tri.normal);
        tri.face_id = out->count;
        csg_trilist_append(out, &tri);
    }

    if (section_verts && section_vert_count >= 2) {
        int edge_cap = section_tris.count * 3;
        int *edge_a = (int *) lv_calloc((size_t) edge_cap, sizeof(int));
        int *edge_b = (int *) lv_calloc((size_t) edge_cap, sizeof(int));
        int edge_count = 0;

                if (edge_a && edge_b) {
                    for (int i = 0; i < section_tris.count; i++) {
                        for (int e = 0; e < 3; e++) {
                            int ia = e;
                            int ib = (e + 1) % 3;
                            CSGVec3 va = section_tris.tris[i].v[ia];
                            CSGVec3 vb = section_tris.tris[i].v[ib];

                            /* 找到顶点在 section_verts 中的索引 */
                            int idx_a = -1, idx_b = -1;
                            for (int s = 0; s < section_vert_count; s++) {
                                CSGVec3 da = csg_vec3_sub(va, section_verts[s]);
                                if (geo_norm_sq_3d(da.x, da.y, da.z) < CSG_BSP_EPSILON * CSG_BSP_EPSILON) {
                                    idx_a = s;
                                }
                                CSGVec3 db = csg_vec3_sub(vb, section_verts[s]);
                                if (geo_norm_sq_3d(db.x, db.y, db.z) < CSG_BSP_EPSILON * CSG_BSP_EPSILON) {
                                    idx_b = s;
                                }
                            }
                            if (idx_a < 0 || idx_b < 0 || idx_a == idx_b)
                                continue;

                            /* 确保边的方向一致（较小索引在前） */
                            if (idx_a > idx_b) {
                                lv_SWAP(int, idx_a, idx_b);
                            }

                            /* 检查边是否已存在 */
                            int found = 0;
                            for (int ee = 0; ee < edge_count; ee++) {
                                if (edge_a[ee] == idx_a && edge_b[ee] == idx_b) {
                                    found = 1;
                                    break;
                                }
                            }
                            if (!found && edge_count < edge_cap) {
                                edge_a[edge_count] = idx_a;
                                edge_b[edge_count] = idx_b;
                                edge_count++;
                            }
                        }
                    }

                    /* 为每条边生成侧面四边形 */
                    for (int e = 0; e < edge_count; e++) {
                        CSGVec3 v0 = section_verts[edge_a[e]];
                        CSGVec3 v1 = section_verts[edge_b[e]];
                        CSGVec3 v2 = csg_vec3_add(v1, offset);
                        CSGVec3 v3 = csg_vec3_add(v0, offset);

                        /* 两个三角形组成四边形，法线朝外 */
                        CSGTriList side_tris;
                        csg_trilist_init(&side_tris, 2);

                        CSGTriangle tri;
                        tri.v[0] = v0;
                        tri.v[1] = v1;
                        tri.v[2] = v2;
                        tri.normal = csg_tri_normal(&tri);
                        /* 确保法线朝外（远离拉伸轴） */
                        CSGVec3 edge_mid = csg_vec3_scale(csg_vec3_add(v0, v1), 0.5);
                        CSGVec3 edge_out = csg_vec3_sub(edge_mid, csg_vec3_scale(dir, csg_vec3_dot(edge_mid, dir)));
                        if (csg_vec3_dot(tri.normal, edge_out) < 0.0) {
                            /* 翻转三角形绕序 */
                            lv_SWAP(CSGVec3, tri.v[1], tri.v[2]);
                            tri.normal = csg_vec3_scale(tri.normal, -1.0);
                        }
                        tri.face_id = out->count;
                        csg_trilist_append(out, &tri);

                        tri.v[0] = v0;
                        tri.v[1] = v2;
                        tri.v[2] = v3;
                        tri.normal = csg_tri_normal(&tri);
                        if (csg_vec3_dot(tri.normal, edge_out) < 0.0) {
                            lv_SWAP(CSGVec3, tri.v[1], tri.v[2]);
                            tri.normal = csg_vec3_scale(tri.normal, -1.0);
                        }
                        tri.face_id = out->count;
                        csg_trilist_append(out, &tri);

                        csg_trilist_free(&side_tris);
                    }
                }

                if (edge_a)
                    lv_free((void **) &edge_a);
                if (edge_b)
                    lv_free((void **) &edge_b);
        }
}

void eval_csg_extrude_rotate(const CSGNode *node, CSGTriList *out) {
    if (node->child_count < 1)
        return;

    CSGTriList section_tris;
    csg_trilist_init(&section_tris, CSG_MAX_TRI_BUFFER);
    csg_evaluate(node->children[0], &section_tris);

    double angle_deg = node->data.prim.params[0];
    if (angle_deg <= 0.0) angle_deg = 360.0;
    int segments = (int)node->data.prim.params[1];
    if (segments <= 0) segments = 32;
    if (segments > 128) segments = 128;

    CSGVec3 axis = {node->data.prim.params[2], node->data.prim.params[3], node->data.prim.params[4]};
    double axis_len = geo_norm_3d(axis.x, axis.y, axis.z);
    if (axis_len < CSG_BSP_EPSILON) {
        axis.x = 0.0; axis.y = 1.0; axis.z = 0.0;
    } else {
        axis.x /= axis_len; axis.y /= axis_len; axis.z /= axis_len;
    }

    double angle_rad = lv_deg_to_rad(angle_deg);
    double angle_step = angle_rad / (double)segments;

    CSGVec3 *sec_verts = NULL;
    int sec_count = 0;
    csg_extract_vertices(&section_tris, &sec_verts, &sec_count);

    if (!sec_verts || sec_count < 2) {
        if (sec_verts) lv_free((void **)&sec_verts);
        csg_trilist_free(&section_tris);
        return;
    }

    for (int seg = 0; seg < segments; seg++) {
        double theta1 = angle_step * (double)seg;
        double theta2 = angle_step * (double)(seg + 1);
        double cos1 = cos(theta1), sin1 = sin(theta1);
        double cos2 = cos(theta2), sin2 = sin(theta2);

        for (int i = 0; i < section_tris.count; i++) {
            for (int e = 0; e < 3; e++) {
                CSGVec3 va = section_tris.tris[i].v[e];
                CSGVec3 vb = section_tris.tris[i].v[(e + 1) % 3];

                CSGVec3 ediff = csg_vec3_sub(vb, va);
                if (geo_norm_sq_3d(ediff.x, ediff.y, ediff.z) < CSG_BSP_EPSILON * CSG_BSP_EPSILON)
                    continue;

                CSGVec3 va1, va2, vb1, vb2;
                CSGVec3 kxva = csg_vec3_cross(axis, va);
                double kdva = csg_vec3_dot(axis, va);
                CSGVec3 kxvb = csg_vec3_cross(axis, vb);
                double kdvb = csg_vec3_dot(axis, vb);

                double c1_complement = 1.0 - cos1;
                va1.x = va.x * cos1 + kxva.x * sin1 + axis.x * kdva * c1_complement;
                va1.y = va.y * cos1 + kxva.y * sin1 + axis.y * kdva * c1_complement;
                va1.z = va.z * cos1 + kxva.z * sin1 + axis.z * kdva * c1_complement;

                double c2_complement = 1.0 - cos2;
                va2.x = va.x * cos2 + kxva.x * sin2 + axis.x * kdva * c2_complement;
                va2.y = va.y * cos2 + kxva.y * sin2 + axis.y * kdva * c2_complement;
                va2.z = va.z * cos2 + kxva.z * sin2 + axis.z * kdva * c2_complement;

                vb1.x = vb.x * cos1 + kxvb.x * sin1 + axis.x * kdvb * c1_complement;
                vb1.y = vb.y * cos1 + kxvb.y * sin1 + axis.y * kdvb * c1_complement;
                vb1.z = vb.z * cos1 + kxvb.z * sin1 + axis.z * kdvb * c1_complement;

                vb2.x = vb.x * cos2 + kxvb.x * sin2 + axis.x * kdvb * c2_complement;
                vb2.y = vb.y * cos2 + kxvb.y * sin2 + axis.y * kdvb * c2_complement;
                vb2.z = vb.z * cos2 + kxvb.z * sin2 + axis.z * kdvb * c2_complement;

                CSGTriangle tri;
                tri.v[0] = va1; tri.v[1] = vb1; tri.v[2] = va2;
                tri.normal = csg_tri_normal(&tri);
                tri.face_id = out->count;
                csg_trilist_append(out, &tri);

                tri.v[0] = vb1; tri.v[1] = vb2; tri.v[2] = va2;
                tri.normal = csg_tri_normal(&tri);
                tri.face_id = out->count;
                csg_trilist_append(out, &tri);
            }
        }
    }

    if (angle_deg < lv_FULL_CIRCLE_DEG - CSG_BSP_EPSILON) {
        for (int i = 0; i < section_tris.count; i++) {
            CSGTriangle tri = section_tris.tris[i];
            CSGVec3 face_normal = csg_tri_normal(&tri);
            if (csg_vec3_dot(face_normal, axis) < 0.0) {
                face_normal = csg_vec3_scale(face_normal, -1.0);
                lv_SWAP(CSGVec3, tri.v[1], tri.v[2]);
            }
            tri.normal = csg_vec3_normalize(face_normal);
            tri.face_id = out->count;
            csg_trilist_append(out, &tri);
        }

        double cos_end = cos(angle_rad), sin_end = sin(angle_rad);
        for (int i = 0; i < section_tris.count; i++) {
            CSGTriangle tri = section_tris.tris[i];
            for (int v = 0; v < 3; v++) {
                CSGVec3 p = tri.v[v];
                CSGVec3 kxp = csg_vec3_cross(axis, p);
                double kdp = csg_vec3_dot(axis, p);
                tri.v[v].x = p.x * cos_end + kxp.x * sin_end + axis.x * kdp * (1.0 - cos_end);
                tri.v[v].y = p.y * cos_end + kxp.y * sin_end + axis.y * kdp * (1.0 - cos_end);
                tri.v[v].z = p.z * cos_end + kxp.z * sin_end + axis.z * kdp * (1.0 - cos_end);
            }
            CSGVec3 face_normal = csg_tri_normal(&tri);
            if (csg_vec3_dot(face_normal, axis) > 0.0) {
                face_normal = csg_vec3_scale(face_normal, -1.0);
                lv_SWAP(CSGVec3, tri.v[1], tri.v[2]);
            }
            tri.normal = csg_vec3_normalize(face_normal);
            tri.face_id = out->count;
            csg_trilist_append(out, &tri);
        }
    }

    if (sec_verts) lv_free((void **)&sec_verts);
    csg_trilist_free(&section_tris);
}

CSGEvalFunc s_eval_funcs[] = {
    [CSG_NODE_PRIMITIVE] = eval_csg_primitive,
    [CSG_NODE_UNION] = eval_csg_bool,
    [CSG_NODE_DIFFERENCE] = eval_csg_bool,
    [CSG_NODE_INTERSECTION] = eval_csg_bool,
    [CSG_NODE_TRANSFORM] = eval_csg_transform,
    [CSG_NODE_HULL] = eval_csg_hull,
    [CSG_NODE_MINKOWSKI] = eval_csg_minkowski,
    [CSG_NODE_EXTRUDE_LINEAR] = eval_csg_extrude_linear,
    [CSG_NODE_EXTRUDE_ROTATE] = eval_csg_extrude_rotate,
};
const int s_eval_func_count = (int)(sizeof(s_eval_funcs) / sizeof(s_eval_funcs[0]));

