/**
 * @file ga_interface.c
 * @brief PGA 几何量的嵌入与提取接口
 * @details 实现 Cl(3,0,1) 投影几何代数中几何对象（点、向量、平面、射线、
 *          旋转、平移）与多向量之间的相互转换。提供几何构造工具函数
 *          （三点共线、四点共面、平面构造等）。
 * @author Lv-00 Project
 * @version 1.0.0
 */

#include "lv/ga_interface.h"

#include <math.h>
#include <stdbool.h>
#include <stdlib.h>

#include "lv/ga_multivector.h"
#include "lv/lv_internal.h"
#include "lv/lv_utils.h"

/* ============================================================
 * Basis element indices (Cl(3,0,1))
 * ============================================================ */
#define GA_S 0
#define GA_E0 1
#define GA_E1 2
#define GA_E2 3
#define GA_E3 4
#define GA_E01 5
#define GA_E02 6
#define GA_E03 7
#define GA_E12 8
#define GA_E13 9
#define GA_E23 10
#define GA_E012 11
#define GA_E013 12
#define GA_E023 13
#define GA_E123 14
#define GA_E0123 15

/* ============================================================
 * Point Operations
 * ============================================================ */

/**
 * @brief 将三维点嵌入为 PGA 多向量
 * @param x, y, z  点的世界坐标
 * @return 嵌入后的多向量（调用者负责释放），失败返回 NULL
 */
lvMultiVector *ga_embed_point(double x, double y, double z) {
    lvMultiVector *mv = ga_mv_create();
    if (!mv)
        return NULL;

    /* Point P = x*e023 + y*e013 + z*e012 + e123 */
    ga_mv_set(mv, GA_E023, x);
    ga_mv_set(mv, GA_E013, y);
    ga_mv_set(mv, GA_E012, z);
    ga_mv_set(mv, GA_E123, 1.0);

    return mv;
}

/**
 * @brief 从 PGA 多向量中提取三维点坐标
 * @param mv    源多向量
 * @param out_x, out_y, out_z  输出坐标（不能为 NULL）
 * @return 成功返回 0；参数无效或非有效点（w 分量为零）返回 -1
 */
int ga_extract_point(const lvMultiVector *mv, double *out_x, double *out_y, double *out_z) {
    if (!mv || !out_x || !out_y || !out_z)
        return -1;

    double w = ga_mv_get(mv, GA_E123);
    if (fabs(w) < 1e-10)
        return -1; /* Not a valid point */

    *out_x = ga_mv_get(mv, GA_E023) / w;
    *out_y = ga_mv_get(mv, GA_E013) / w;
    *out_z = ga_mv_get(mv, GA_E012) / w;

    return 0;
}

/* ============================================================
 * Vector Operations
 * ============================================================ */

/**
 * @brief 将方向向量嵌入为 PGA 多向量
 * @param vx, vy, vz  向量分量
 * @return 嵌入后的多向量（调用者负责释放），失败返回 NULL
 */
lvMultiVector *ga_embed_vector(double vx, double vy, double vz) {
    lvMultiVector *mv = ga_mv_create();
    if (!mv)
        return NULL;

    /* Vector v = vx*e1 + vy*e2 + vz*e3 */
    ga_mv_set(mv, GA_E1, vx);
    ga_mv_set(mv, GA_E2, vy);
    ga_mv_set(mv, GA_E3, vz);

    return mv;
}

/**
 * @brief 从 PGA 多向量中提取方向向量
 * @param mv    源多向量
 * @param out_vx, out_vy, out_vz  输出向量分量（不能为 NULL）
 * @return 成功返回 0，失败返回 -1
 */
int ga_extract_vector(const lvMultiVector *mv, double *out_vx, double *out_vy, double *out_vz) {
    if (!mv || !out_vx || !out_vy || !out_vz)
        return -1;

    *out_vx = ga_mv_get(mv, GA_E1);
    *out_vy = ga_mv_get(mv, GA_E2);
    *out_vz = ga_mv_get(mv, GA_E3);

    return 0;
}

/* ============================================================
 * Plane Operations
 * ============================================================ */

/**
 * @brief 将平面嵌入为 PGA 多向量（法线-距离形式）
 * @param nx, ny, nz  平面法向量分量
 * @param d           原点到平面的距离（带符号）
 * @return 嵌入后的多向量（调用者负责释放），失败返回 NULL
 */
lvMultiVector *ga_embed_plane(double nx, double ny, double nz, double d) {
    lvMultiVector *mv = ga_mv_create();
    if (!mv)
        return NULL;

    /* Plane pi = nx*e1 + ny*e2 + nz*e3 + d*e0 */
    ga_mv_set(mv, GA_E1, nx);
    ga_mv_set(mv, GA_E2, ny);
    ga_mv_set(mv, GA_E3, nz);
    ga_mv_set(mv, GA_E0, d);

    return mv;
}

/**
 * @brief 从 PGA 多向量中提取平面参数（法线-距离形式）
 * @param mv                源多向量
 * @param out_nx, out_ny, out_nz  输出法向量分量（不能为 NULL）
 * @param out_d             输出距离（不能为 NULL）
 * @return 成功返回 0，失败返回 -1
 */
int ga_extract_plane(const lvMultiVector *mv, double *out_nx, double *out_ny, double *out_nz, double *out_d) {
    if (!mv || !out_nx || !out_ny || !out_nz || !out_d)
        return -1;

    *out_nx = ga_mv_get(mv, GA_E1);
    *out_ny = ga_mv_get(mv, GA_E2);
    *out_nz = ga_mv_get(mv, GA_E3);
    *out_d = ga_mv_get(mv, GA_E0);

    return 0;
}

/* ============================================================
 * Ray Operations
 * ============================================================ */

/**
 * @brief 将射线嵌入为 PGA 多向量（起点 ^ 方向的外积）
 * @param origin 起点多向量
 * @param dir    方向多向量
 * @return 表示射线的多向量（调用者负责释放），失败返回 NULL
 */
lvMultiVector *ga_embed_ray(const lvMultiVector *origin, const lvMultiVector *dir) {
    if (!origin || !dir)
        return NULL;

    /* Ray = origin ^ direction (outer product) */
    return ga_mv_outer_product(origin, dir);
}

/**
 * @brief 从 PGA 多向量中提取射线的起点和方向
 * @param mv         源多向量
 * @param out_origin 输出起点多向量（调用者需通过 ga_mv_destroy 释放）
 * @param out_dir    输出方向多向量（调用者需通过 ga_mv_destroy 释放）
 * @return 成功返回 0，失败返回 -1
 * @warning 输出参数在失败时可能部分分配，调用者仍须对非 NULL 输出调用 ga_mv_destroy
 */
int ga_extract_ray(const lvMultiVector *mv, lvMultiVector **out_origin, lvMultiVector **out_dir) {
    if (!mv || !out_origin || !out_dir)
        return -1;

    /* Simplified: extract bivector components */
    *out_origin = ga_mv_create();
    *out_dir = ga_mv_create();

    if (!*out_origin || !*out_dir) {
        ga_mv_destroy(*out_origin);
        ga_mv_destroy(*out_dir);
        return -1;
    }

    /* Extract origin from trivector components */
    ga_mv_set(*out_origin, GA_E023, ga_mv_get(mv, GA_E012));
    ga_mv_set(*out_origin, GA_E013, ga_mv_get(mv, GA_E013));
    ga_mv_set(*out_origin, GA_E012, ga_mv_get(mv, GA_E023));
    ga_mv_set(*out_origin, GA_E123, 1.0);

    /* Extract direction from bivector components */
    ga_mv_set(*out_dir, GA_E1, ga_mv_get(mv, GA_E23));
    ga_mv_set(*out_dir, GA_E2, ga_mv_get(mv, GA_E13));
    ga_mv_set(*out_dir, GA_E3, ga_mv_get(mv, GA_E12));

    return 0;
}

/* ============================================================
 * Rotor Operations (Rotations)
 * ============================================================ */

/**
 * @brief 将旋转嵌入为 PGA Rotor 多向量
 * @param ax, ay, az  旋转轴向量（不必归一化）
 * @param angle       旋转角度（弧度）
 * @return Rotor 多向量（调用者负责释放），轴为零向量时返回 NULL
 */
lvMultiVector *ga_embed_rotation(double ax, double ay, double az, double angle) {
    /* Normalize axis */
    double len = sqrt(ax * ax + ay * ay + az * az);
    if (len < 1e-10)
        return NULL;

    ax /= len;
    ay /= len;
    az /= len;

    lvMultiVector *mv = ga_mv_create();
    if (!mv)
        return NULL;

    /* Rotor R = cos(angle/2) + sin(angle/2) * (ax*e23 + ay*e13 + az*e12) */
    double half_angle = angle / 2.0;
    double c = cos(half_angle);
    double s = sin(half_angle);

    ga_mv_set(mv, GA_S, c);
    ga_mv_set(mv, GA_E23, s * ax);
    ga_mv_set(mv, GA_E13, s * ay);
    ga_mv_set(mv, GA_E12, s * az);

    return mv;
}

/**
 * @brief 从 Rotor 多向量中提取旋转轴和角度
 * @param rotor          Rotor 多向量
 * @param out_ax, out_ay, out_az  输出旋转轴单位向量（不能为 NULL）
 * @param out_angle      输出旋转角度（弧度，不能为 NULL）
 * @return 成功返回 0，失败返回 -1
 */
int ga_extract_rotation(const lvMultiVector *rotor, double *out_ax, double *out_ay, double *out_az, double *out_angle) {
    if (!rotor || !out_ax || !out_ay || !out_az || !out_angle)
        return -1;

    double c = ga_mv_get(rotor, GA_S);
    double bx = ga_mv_get(rotor, GA_E23);
    double by = ga_mv_get(rotor, GA_E13);
    double bz = ga_mv_get(rotor, GA_E12);

    /* Extract angle */
    *out_angle = 2.0 * atan2(sqrt(bx * bx + by * by + bz * bz), c);

    /* Extract axis */
    double s = sin(*out_angle / 2.0);
    if (fabs(s) < 1e-10) {
        *out_ax = 1.0;
        *out_ay = 0.0;
        *out_az = 0.0;
    } else {
        *out_ax = bx / s;
        *out_ay = by / s;
        *out_az = bz / s;
    }

    return 0;
}

/* ============================================================
 * Motor Operations (Translations)
 * ============================================================ */

/**
 * @brief 将平移嵌入为 PGA Motor 多向量
 * @param tx, ty, tz  平移向量分量
 * @return Motor 多向量（调用者负责释放），失败返回 NULL
 */
lvMultiVector *ga_embed_translation(double tx, double ty, double tz) {
    lvMultiVector *mv = ga_mv_create();
    if (!mv)
        return NULL;

    /* Translation motor T = 1 + 0.5*(tx*e01 + ty*e02 + tz*e03) */
    ga_mv_set(mv, GA_S, 1.0);
    ga_mv_set(mv, GA_E01, 0.5 * tx);
    ga_mv_set(mv, GA_E02, 0.5 * ty);
    ga_mv_set(mv, GA_E03, 0.5 * tz);

    return mv;
}

/* ============================================================
 * Geometric Construction Functions
 * ============================================================ */

/**
 * @brief 构造经过两点的直线
 * @param p1, p2  点多向量
 * @return 表示直线的多向量（调用者负责释放），失败返回 NULL
 */
lvMultiVector *ga_line_from_two_points(const lvMultiVector *p1, const lvMultiVector *p2) {
    if (!p1 || !p2)
        return NULL;

    /* Line L = P1 ^ P2 (outer product) */
    return ga_mv_outer_product(p1, p2);
}

/**
 * @brief 判断三点是否共线
 * @param p1, p2, p3  点多向量
 * @return 共线返回 true，否则返回 false（参数无效时也返回 false）
 */
bool ga_three_points_collinear(const lvMultiVector *p1, const lvMultiVector *p2, const lvMultiVector *p3) {
    if (!p1 || !p2 || !p3)
        return false;

    /* PGA points are grade-3 trivectors; outer product of grade-3 in 4D = 0.
     * Use determinant of coordinate matrix instead. */
    double x1, y1, z1, x2, y2, z2, x3, y3, z3;
    if (ga_extract_point(p1, &x1, &y1, &z1) < 0)
        return false;
    if (ga_extract_point(p2, &x2, &y2, &z2) < 0)
        return false;
    if (ga_extract_point(p3, &x3, &y3, &z3) < 0)
        return false;

    /* 3x3 determinant (last column is all 1s for homogeneous) */
    double det = x1 * (y2 - y3) - y1 * (x2 - x3) + (x2 * y3 - x3 * y2);
    return fabs(det) < 1e-6;
}

/**
 * @brief 判断四点是否共面
 * @param p1, p2, p3, p4  点多向量
 * @return 共面返回 true，否则返回 false（参数无效时也返回 false）
 */
bool ga_four_points_coplanar(const lvMultiVector *p1, const lvMultiVector *p2, const lvMultiVector *p3,
                             const lvMultiVector *p4) {
    if (!p1 || !p2 || !p3 || !p4)
        return false;

    double x1, y1, z1, x2, y2, z2, x3, y3, z3, x4, y4, z4;
    if (ga_extract_point(p1, &x1, &y1, &z1) < 0)
        return false;
    if (ga_extract_point(p2, &x2, &y2, &z2) < 0)
        return false;
    if (ga_extract_point(p3, &x3, &y3, &z3) < 0)
        return false;
    if (ga_extract_point(p4, &x4, &y4, &z4) < 0)
        return false;

    /* 4x4 determinant (last column all 1s) */
    double det = x1 * (y2 * (z3 - z4) - y3 * (z2 - z4) + y4 * (z2 - z3)) -
                 y1 * (x2 * (z3 - z4) - x3 * (z2 - z4) + x4 * (z2 - z3)) +
                 z1 * (x2 * (y3 - y4) - x3 * (y2 - y4) + x4 * (y2 - y3)) -
                 (x2 * (y3 * z4 - y4 * z3) - x3 * (y2 * z4 - y4 * z2) + x4 * (y2 * z3 - y3 * z2));
    return fabs(det) < 1e-6;
}

/**
 * @brief 过三点构造平面（外积 P1 ^ P2 ^ P3）
 * @param p1, p2, p3  点多向量
 * @return 表示平面的多向量（调用者负责释放），失败返回 NULL
 */
lvMultiVector *ga_plane_from_three_points(const lvMultiVector *p1, const lvMultiVector *p2, const lvMultiVector *p3) {
    if (!p1 || !p2 || !p3)
        return NULL;

    /* Plane pi = P1 ^ P2 ^ P3 (outer product) */
    lvMultiVector *temp = ga_mv_outer_product(p1, p2);
    if (!temp)
        return NULL;

    lvMultiVector *result = ga_mv_outer_product(temp, p3);
    ga_mv_destroy(temp);

    return result;
}
