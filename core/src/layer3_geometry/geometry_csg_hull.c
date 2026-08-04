/*
 * @file geometry_csg_hull.c
 * @brief CSG geometry module - convex hull and vertex extraction
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

void csg_primitive_to_tris(const CSGNode *node, CSGTriList *out) {
    int ptype = node->data.prim.type;

    /* 图元类型 → 三角面生成 统一走 vtable（见 s_prim_ops，定义于 geometry_csg_primitive.c） */
    if (ptype >= 0 && ptype < s_prim_ops_count && s_prim_ops[ptype].gen_tris) {
        s_prim_ops[ptype].gen_tris(node, out);
    }
}

/* ================================================================
 * 凸包计算（HULL / MINKOWSKI 共用）
 * ================================================================ */

/**
 * @brief 从顶点集合计算 3D 凸包（暴力枚举法，适用于顶点数 < 200 的场景）
 *
 * 算法：
 *   1. 枚举所有顶点三元组 (i, j, k) 作为候选面
 *   2. 对每个候选面，检查所有其他顶点是否都在同一侧（或平面上）
 *   3. 满足条件的面为凸包面，法线朝外
 *   4. 将凸包面输出为三角形列表
 *
 * @param vertices      顶点数组
 * @param vertex_count  顶点数量
 * @param out           输出三角形面列表
 */
void csg_compute_convex_hull(const CSGVec3 *vertices, int vertex_count, CSGTriList *out) {
    if (vertex_count < 4) {
        /* 不足 4 个顶点，无法构成 3D 凸包 */
        return;
    }

    /* 计算所有顶点的质心，用于判断法线朝向 */
    CSGVec3 centroid = {0.0, 0.0, 0.0};
    for (int i = 0; i < vertex_count; i++) {
        centroid.x += vertices[i].x;
        centroid.y += vertices[i].y;
        centroid.z += vertices[i].z;
    }
    centroid.x /= (double) vertex_count;
    centroid.y /= (double) vertex_count;
    centroid.z /= (double) vertex_count;

    /* 计算顶点坐标最大绝对值，用于相对 epsilon 缩放 */
    double max_vertex_abs = 0.0;
    for (int i = 0; i < vertex_count; i++) {
        max_vertex_abs = fmax(max_vertex_abs, fabs(vertices[i].x));
        max_vertex_abs = fmax(max_vertex_abs, fabs(vertices[i].y));
        max_vertex_abs = fmax(max_vertex_abs, fabs(vertices[i].z));
    }
    double csg_hull_eps = CSG_BSP_EPSILON * fmax(1.0, max_vertex_abs);

    /* 暴力枚举所有三元组 */
    for (int i = 0; i < vertex_count - 2; i++) {
        for (int j = i + 1; j < vertex_count - 1; j++) {
            for (int k = j + 1; k < vertex_count; k++) {
                CSGVec3 e1 = csg_vec3_sub(vertices[j], vertices[i]);
                CSGVec3 e2 = csg_vec3_sub(vertices[k], vertices[i]);
                CSGVec3 normal = csg_vec3_normalize(csg_vec3_cross(e1, e2));

                /* 退化三角形（面积为零），跳过 */
                double nlen = sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
                if (nlen < csg_hull_eps)
                    continue;

                /* 检查所有其他顶点是否在面的同一侧 */
                int pos_count = 0;
                int neg_count = 0;
                int valid = 1;

                for (int m = 0; m < vertex_count; m++) {
                    if (m == i || m == j || m == k)
                        continue;

                    double d = csg_signed_distance(vertices[i], normal, vertices[m]);
                    if (d > csg_hull_eps)
                        pos_count++;
                    else if (d < -csg_hull_eps)
                        neg_count++;

                    /* 两侧都有顶点 → 不是凸包面 */
                    if (pos_count > 0 && neg_count > 0) {
                        valid = 0;
                        break;
                    }
                }

                if (!valid)
                    continue;

                /* 确保法线朝外（远离质心方向） */
                double centroid_dist = csg_signed_distance(vertices[i], normal, centroid);
                if (centroid_dist > 0.0) {
                    /* 法线指向质心方向 → 翻转 */
                    normal = csg_vec3_scale(normal, -1.0);
                    /* 翻转顶点顺序以保持 CCW */
                    CSGTriangle tri;
                    tri.v[0] = vertices[i];
                    tri.v[1] = vertices[k];
                    tri.v[2] = vertices[j];
                    tri.normal = normal;
                    tri.face_id = out->count;
                    csg_trilist_append(out, &tri);
                } else {
                    CSGTriangle tri;
                    tri.v[0] = vertices[i];
                    tri.v[1] = vertices[j];
                    tri.v[2] = vertices[k];
                    tri.normal = normal;
                    tri.face_id = out->count;
                    csg_trilist_append(out, &tri);
                }
            }
        }
    }
}

/**
 * @brief 从三角形面列表中提取所有唯一顶点
 *
 * @param tris       三角形面列表
 * @param out_verts  输出：顶点数组（调用者释放）
 * @param out_count  输出：顶点数量
 */
void csg_extract_vertices(const CSGTriList *tris, CSGVec3 **out_verts, int *out_count) {
    /* 最坏情况：每个三角形 3 个顶点 */
    int max_verts = tris->count * 3;
    CSGVec3 *verts = (CSGVec3 *) lv_calloc((size_t) max_verts, sizeof(CSGVec3));
    if (!verts) {
        *out_verts = NULL;
        *out_count = 0;
        return;
    }

    int count = 0;
    for (int i = 0; i < tris->count; i++) {
        for (int v = 0; v < 3; v++) {
            CSGVec3 pt = tris->tris[i].v[v];
            /* 检查是否已存在（简单 O(n^2) 去重） */
            int found = 0;
            /* 使用相对容差：计算已存顶点的最大量级 */
            double max_coord = 0.0;
            for (int j = 0; j < count; j++) {
                double cx = fabs(verts[j].x), cy = fabs(verts[j].y), cz = fabs(verts[j].z);
                if (cx > max_coord)
                    max_coord = cx;
                if (cy > max_coord)
                    max_coord = cy;
                if (cz > max_coord)
                    max_coord = cz;
            }
            double eps_sq = CSG_BSP_EPSILON * CSG_BSP_EPSILON * (1.0 + max_coord * max_coord);
            for (int j = 0; j < count; j++) {
                CSGVec3 d = csg_vec3_sub(pt, verts[j]);
                double dist2 = d.x * d.x + d.y * d.y + d.z * d.z;
                if (dist2 < eps_sq) {
                    found = 1;
                    break;
                }
            }
            if (!found) {
                verts[count] = pt;
                count++;
            }
        }
    }

    *out_verts = verts;
    *out_count = count;
}