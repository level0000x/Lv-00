/**
 * @file lv_vec3.h
 * @brief 公共 3D 向量（double 精度）类型与内联运算 —— 3D 向量库单一事实来源
 *
 * 收敛说明（几何层收敛任务）：
 *   - 原 lvPoint3D（geo_halfedge_mesh.h / parametric_curves.h 双 typedef）与
 *     CSGVec3（geometry_csg_internal.h）三套同构结构统一 typedef 到 lvVec3；
 *   - 原 geo_halfedge_mesh.c 的 vector_dot/cross/sub 静态函数与 geometry_csg.c
 *     的 csg_vec3_* 系列统一收敛到本头的内联实现（double 运算逐位一致）；
 *   - lv_vec3_normalize 判零阈值取 lv_EPSILON_MEDIUM（config.h，1e-9），
 *     与原 csg_vec3_normalize 使用的 CSG_BSP_EPSILON（1e-9）数值一致。
 */

#ifndef lv_VEC3_H
#define lv_VEC3_H

#include <math.h>

#include "config.h" /* lv_EPSILON_MEDIUM（1e-9，与 CSG_BSP_EPSILON 数值一致） */

#ifdef __cplusplus
extern "C" {
#endif

/* ── 3D 向量（double 精度） ── */
typedef struct {
    double x, y, z;
} lvVec3;

/** @brief 点积 */
static inline double lv_vec3_dot(lvVec3 a, lvVec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

/** @brief 叉积 */
static inline lvVec3 lv_vec3_cross(lvVec3 a, lvVec3 b) {
    lvVec3 r;
    r.x = a.y * b.z - a.z * b.y;
    r.y = a.z * b.x - a.x * b.z;
    r.z = a.x * b.y - a.y * b.x;
    return r;
}

/** @brief 向量减法 */
static inline lvVec3 lv_vec3_sub(lvVec3 a, lvVec3 b) {
    lvVec3 r;
    r.x = a.x - b.x;
    r.y = a.y - b.y;
    r.z = a.z - b.z;
    return r;
}

/** @brief 向量加法 */
static inline lvVec3 lv_vec3_add(lvVec3 a, lvVec3 b) {
    lvVec3 r;
    r.x = a.x + b.x;
    r.y = a.y + b.y;
    r.z = a.z + b.z;
    return r;
}

/** @brief 标量乘法 */
static inline lvVec3 lv_vec3_scale(lvVec3 v, double s) {
    lvVec3 r;
    r.x = v.x * s;
    r.y = v.y * s;
    r.z = v.z * s;
    return r;
}

/**
 * @brief 向量归一化（模长低于判零阈值时返回零向量）
 * @note 判零阈值 lv_EPSILON_MEDIUM = 1e-9，与原 CSG_BSP_EPSILON 一致，行为逐位不变
 */
static inline lvVec3 lv_vec3_normalize(lvVec3 v) {
    double len = sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    if (len < lv_EPSILON_MEDIUM) {
        lvVec3 zero = {0.0, 0.0, 0.0};
        return zero;
    }
    return lv_vec3_scale(v, 1.0 / len);
}

#ifdef __cplusplus
}
#endif

#endif /* lv_VEC3_H */
