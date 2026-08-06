/*
 * @file geometry_csg_mesh.c
 * @brief CSG geometry module - triangle mesh generation
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

/* ================================================================
 * 网格骨架共享（圆柱/圆锥/球体共用）
 * ================================================================ */

/**
 * @brief 由四角网格的四个顶点生成两个三角形（网格扇区）
 *
 * 顶点命名沿用网格约定：v00=(z1,t1)、v01=(z1,t2)、v10=(z2,t1)、v11=(z2,t2)。
 * 三角 1 = (v00, v10, v01)，三角 2 = (v01, v10, v11)，
 * 与球体/圆柱/圆锥侧带原有三角带顺序逐位一致（winding 与 face_id 分配不变）。
 */
void csg_emit_quad(CSGTriList *out, CSGVec3 v00, CSGVec3 v01, CSGVec3 v10, CSGVec3 v11) {
    CSGTriangle tri;

    /* 三角形 1 */
    tri.v[0] = v00;
    tri.v[1] = v10;
    tri.v[2] = v01;
    tri.normal = csg_tri_normal(&tri);
    tri.face_id = out->count;
    csg_trilist_append(out, &tri);

    /* 三角形 2 */
    tri.v[0] = v01;
    tri.v[1] = v10;
    tri.v[2] = v11;
    tri.normal = csg_tri_normal(&tri);
    tri.face_id = out->count;
    csg_trilist_append(out, &tri);
}

/**
 * @brief 生成车削体（lathe）单条侧带三角片（相邻两个 z 环之间）
 *
 * 顶点由两端 z 环处的半径生成：
 *   v00 = {r1*c1, r1*s1, z1}、v01 = {r1*c2, r1*s2, z1}
 *   v10 = {r2*c1, r2*s1, z2}、v11 = {r2*c2, r2*s2, z2}
 * 圆柱 = 常数半径（r1 == r2 == radius），圆锥 = 线性半径（r1/r2 由调用方插值）。
 */
void csg_gen_lathe_side_tris(double r1, double r2, double z1, double z2, int slices, CSGTriList *out) {
    for (int j = 0; j < slices; j++) {
        double t1 = 2.0 * M_PI * (double) j / (double) slices;
        double t2 = 2.0 * M_PI * (double) (j + 1) / (double) slices;
        double c1 = cos(t1), s1 = sin(t1);
        double c2 = cos(t2), s2 = sin(t2);

        CSGVec3 v00 = {r1 * c1, r1 * s1, z1};
        CSGVec3 v01 = {r1 * c2, r1 * s2, z1};
        CSGVec3 v10 = {r2 * c1, r2 * s1, z2};
        CSGVec3 v11 = {r2 * c2, r2 * s2, z2};

        csg_emit_quad(out, v00, v01, v10, v11);
    }
}

/**
 * @brief 生成单个圆盘扇形三角形（第 j 号扇区）
 *
 * 绕序由 winding 决定（CSG_DISK_CCW：顶点序 (center, p1, p2)，法线朝 +Z；
 * CSG_DISK_CW：顶点序 (center, p2, p1)，法线朝 -Z），
 * 与圆柱/圆锥原有圆盘顶点序逐位一致。
 */
static void csg_emit_disk_sector(CSGVec3 center, double radius, double z, int slices, int j, int winding,
                                 CSGTriList *out) {
    double t1 = 2.0 * M_PI * (double) j / (double) slices;
    double t2 = 2.0 * M_PI * (double) (j + 1) / (double) slices;
    double c1 = cos(t1), s1 = sin(t1);
    double c2 = cos(t2), s2 = sin(t2);
    CSGVec3 p1 = {radius * c1, radius * s1, z};
    CSGVec3 p2 = {radius * c2, radius * s2, z};

    CSGTriangle tri;
    if (winding > 0) {
        /* CCW：法线朝 +Z（顶面） */
        tri.v[0] = center;
        tri.v[1] = p1;
        tri.v[2] = p2;
    } else {
        /* CW：法线朝 -Z（底面） */
        tri.v[0] = center;
        tri.v[1] = p2;
        tri.v[2] = p1;
    }
    tri.normal = csg_tri_normal(&tri);
    tri.face_id = out->count;
    csg_trilist_append(out, &tri);
}

/**
 * @brief 生成整圆盘扇形（slices 个辐射三角形）
 *
 * @param center   圆盘中心（通常位于 z 轴上，z 坐标等于圆盘平面高度）
 * @param radius   圆盘半径
 * @param z        圆盘平面高度
 * @param slices   扇区数（与侧带 slices 一致）
 * @param winding  绕序（CSG_DISK_CCW 法线 +Z / CSG_DISK_CW 法线 -Z）
 */
void csg_gen_disk_tris(CSGVec3 center, double radius, double z, int slices, int winding, CSGTriList *out) {
    for (int j = 0; j < slices; j++) {
        csg_emit_disk_sector(center, radius, z, slices, j, winding, out);
    }
}

void csg_gen_sphere_tris(double radius, CSGTriList *out) {
    int stacks = 16;
    int slices = 32;

    for (int i = 0; i < stacks; i++) {
        double phi1 = M_PI * (double) i / (double) stacks;
        double phi2 = M_PI * (double) (i + 1) / (double) stacks;
        double sin_p1 = sin(phi1), cos_p1 = cos(phi1);
        double sin_p2 = sin(phi2), cos_p2 = cos(phi2);

        for (int j = 0; j < slices; j++) {
            double theta1 = 2.0 * M_PI * (double) j / (double) slices;
            double theta2 = 2.0 * M_PI * (double) (j + 1) / (double) slices;
            double sin_t1 = sin(theta1), cos_t1 = cos(theta1);
            double sin_t2 = sin(theta2), cos_t2 = cos(theta2);

            CSGVec3 v00 = {radius * sin_p1 * cos_t1, radius * sin_p1 * sin_t1, radius * cos_p1};
            CSGVec3 v01 = {radius * sin_p1 * cos_t2, radius * sin_p1 * sin_t2, radius * cos_p1};
            CSGVec3 v10 = {radius * sin_p2 * cos_t1, radius * sin_p2 * sin_t1, radius * cos_p2};
            CSGVec3 v11 = {radius * sin_p2 * cos_t2, radius * sin_p2 * sin_t2, radius * cos_p2};

            /* 上半和下半：两个三角形（公共四角扇区骨架） */
            csg_emit_quad(out, v00, v01, v10, v11);
        }
    }
}

/**
 * @brief 生成立方体的 12 个三角形面（6 个面各 2 个三角形）
 */
void csg_gen_cube_tris(double w, double h, double d, CSGTriList *out) {
    double hw = w * 0.5, hh = h * 0.5, hd = d * 0.5;

    /* 每个面的 8 个角点（CCW 序） */
    struct {
        CSGVec3 v[4];
    } faces[6];

    /* +X */
    faces[0].v[0] = (CSGVec3) {hw, -hh, -hd};
    faces[0].v[1] = (CSGVec3) {hw, hh, -hd};
    faces[0].v[2] = (CSGVec3) {hw, hh, hd};
    faces[0].v[3] = (CSGVec3) {hw, -hh, hd};
    /* -X */
    faces[1].v[0] = (CSGVec3) {-hw, -hh, hd};
    faces[1].v[1] = (CSGVec3) {-hw, hh, hd};
    faces[1].v[2] = (CSGVec3) {-hw, hh, -hd};
    faces[1].v[3] = (CSGVec3) {-hw, -hh, -hd};
    /* +Y */
    faces[2].v[0] = (CSGVec3) {-hw, hh, -hd};
    faces[2].v[1] = (CSGVec3) {hw, hh, -hd};
    faces[2].v[2] = (CSGVec3) {hw, hh, hd};
    faces[2].v[3] = (CSGVec3) {-hw, hh, hd};
    /* -Y */
    faces[3].v[0] = (CSGVec3) {-hw, -hh, hd};
    faces[3].v[1] = (CSGVec3) {hw, -hh, hd};
    faces[3].v[2] = (CSGVec3) {hw, -hh, -hd};
    faces[3].v[3] = (CSGVec3) {-hw, -hh, -hd};
    /* +Z */
    faces[4].v[0] = (CSGVec3) {-hw, -hh, hd};
    faces[4].v[1] = (CSGVec3) {-hw, hh, hd};
    faces[4].v[2] = (CSGVec3) {hw, hh, hd};
    faces[4].v[3] = (CSGVec3) {hw, -hh, hd};
    /* -Z */
    faces[5].v[0] = (CSGVec3) {hw, -hh, -hd};
    faces[5].v[1] = (CSGVec3) {hw, hh, -hd};
    faces[5].v[2] = (CSGVec3) {-hw, hh, -hd};
    faces[5].v[3] = (CSGVec3) {-hw, -hh, -hd};

    for (int f = 0; f < 6; f++) {
        CSGTriangle tri;
        tri.v[0] = faces[f].v[0];
        tri.v[1] = faces[f].v[1];
        tri.v[2] = faces[f].v[2];
        tri.normal = csg_tri_normal(&tri);
        tri.face_id = out->count;
        csg_trilist_append(out, &tri);

        tri.v[0] = faces[f].v[0];
        tri.v[1] = faces[f].v[2];
        tri.v[2] = faces[f].v[3];
        tri.normal = csg_tri_normal(&tri);
        tri.face_id = out->count;
        csg_trilist_append(out, &tri);
    }
}

/**
 * @brief 生成圆柱体的三角形面
 *
 * 将圆柱体侧面划分为 strips × slices 网格，加上顶面和底面。
 */
void csg_gen_cylinder_tris(double radius, double height, CSGTriList *out) {
    int slices = 32;
    int strips = 8;
    double hh = height * 0.5;

    /* 侧面三角带：常数半径（共用车削体侧带骨架） */
    for (int i = 0; i < strips; i++) {
        double z1 = -hh + height * (double) i / (double) strips;
        double z2 = -hh + height * (double) (i + 1) / (double) strips;
        csg_gen_lathe_side_tris(radius, radius, z1, z2, slices, out);
    }

    /* 顶面和底面（辐射三角形）：
     * 保持原有交错顺序（每 j 先顶面后底面），与圆锥圆盘共用扇形骨架 */
    CSGVec3 center_top = {0.0, 0.0, hh};
    CSGVec3 center_bottom = {0.0, 0.0, -hh};
    for (int j = 0; j < slices; j++) {
        csg_emit_disk_sector(center_top, radius, hh, slices, j, CSG_DISK_CCW, out);
        csg_emit_disk_sector(center_bottom, radius, -hh, slices, j, CSG_DISK_CW, out);
    }
}

/* ================================================================
 * BSP 树数据结构
 * ================================================================ */

/**
 * @brief 三角形相对于平面的分类
 */


/* ================================================================
 * BSP 树构建与操作
 * ================================================================ */

/**
 * @brief 创建一个 BSP 树节点
 */
