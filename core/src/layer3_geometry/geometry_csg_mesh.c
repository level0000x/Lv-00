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

            /* 上半和下半：两个三角形 */
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

    for (int i = 0; i < strips; i++) {
        double z1 = -hh + height * (double) i / (double) strips;
        double z2 = -hh + height * (double) (i + 1) / (double) strips;
        for (int j = 0; j < slices; j++) {
            double t1 = 2.0 * M_PI * (double) j / (double) slices;
            double t2 = 2.0 * M_PI * (double) (j + 1) / (double) slices;
            double c1 = cos(t1), s1 = sin(t1);
            double c2 = cos(t2), s2 = sin(t2);

            CSGVec3 v00 = {radius * c1, radius * s1, z1};
            CSGVec3 v01 = {radius * c2, radius * s2, z1};
            CSGVec3 v10 = {radius * c1, radius * s1, z2};
            CSGVec3 v11 = {radius * c2, radius * s2, z2};

            CSGTriangle tri;
            tri.v[0] = v00;
            tri.v[1] = v10;
            tri.v[2] = v01;
            tri.normal = csg_tri_normal(&tri);
            tri.face_id = out->count;
            csg_trilist_append(out, &tri);

            tri.v[0] = v01;
            tri.v[1] = v10;
            tri.v[2] = v11;
            tri.normal = csg_tri_normal(&tri);
            tri.face_id = out->count;
            csg_trilist_append(out, &tri);
        }
    }

    /* 顶面和底面（辐射三角形） */
    for (int j = 0; j < slices; j++) {
        double t1 = 2.0 * M_PI * (double) j / (double) slices;
        double t2 = 2.0 * M_PI * (double) (j + 1) / (double) slices;
        double c1 = cos(t1), s1 = sin(t1);
        double c2 = cos(t2), s2 = sin(t2);

        CSGVec3 center_top = {0.0, 0.0, hh};
        CSGVec3 center_bottom = {0.0, 0.0, -hh};
        CSGVec3 p1_top = {radius * c1, radius * s1, hh};
        CSGVec3 p2_top = {radius * c2, radius * s2, hh};
        CSGVec3 p1_bot = {radius * c1, radius * s1, -hh};
        CSGVec3 p2_bot = {radius * c2, radius * s2, -hh};

        CSGTriangle tri;
        tri.v[0] = center_top;
        tri.v[1] = p1_top;
        tri.v[2] = p2_top;
        tri.normal = csg_tri_normal(&tri);
        tri.face_id = out->count;
        csg_trilist_append(out, &tri);

        tri.v[0] = center_bottom;
        tri.v[1] = p2_bot;
        tri.v[2] = p1_bot;
        tri.normal = csg_tri_normal(&tri);
        tri.face_id = out->count;
        csg_trilist_append(out, &tri);
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
