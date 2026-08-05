/**
 * @file geometry_csg.c
 * @brief Lv-00 CSG 构造实体几何 — BSP 树布尔运算与 OpenSCAD 互操作
 *
 * 借鉴 OpenSCAD (github.com/openscad/openscad) 的 CSG 操作符链范式，
 * 实现基于 BSP（Binary Space Partitioning）树的构造实体几何操作。
 *
 * 核心功能：
 *   - CSG 构造树生命周期管理（创建/添加子树/销毁）
 *   - 包围盒递归计算
 *   - 基本图元生成（球体、立方体、圆柱体）
 *   - 布尔运算（并集、差集、交集）——返回新的 CSG 组合节点
 *   - 递归 CSG 树评估（BSP 面分类 + 三角形裁剪）
 *   - OpenSCAD .scad 格式导出
 *   - 内建示例演示（泰姬陵圆顶 CSG 组合）
 *
 * 参考项目：
 *   - OpenSCAD（openscad.org） — CSG 操作符链 + 脚本即 3D 模型
 *   - CGAL Nef polyhedra — 精确 CSG 布尔运算参考
 *
 * 注：本实现为概念演示级，生产环境建议集成 CGAL 或 Carve 库
 * 以获得精确的 BSP 布尔运算支持。
 *
 * @category 第六梯队参考项目落地 (P1)
 * @date 2026-05-24
 * @author Lv-00 Project
 * @version 1.0.0
 */

#include "lv/lv_platform.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "geometry_types.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "geometry_csg_internal.h"

/* ================================================================
 * 内部数据结构
 * ================================================================ */



/* ================================================================
 * 内部辅助函数：向量 / 三角形几何运算
 * ================================================================ */

/**
 * @brief 计算两个向量的叉积
 */
CSGVec3 csg_vec3_cross(CSGVec3 a, CSGVec3 b) {
    CSGVec3 r;
    r.x = a.y * b.z - a.z * b.y;
    r.y = a.z * b.x - a.x * b.z;
    r.z = a.x * b.y - a.y * b.x;
    return r;
}

/**
 * @brief 计算两个向量的点积
 */
double csg_vec3_dot(CSGVec3 a, CSGVec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

/**
 * @brief 向量减法
 */
CSGVec3 csg_vec3_sub(CSGVec3 a, CSGVec3 b) {
    CSGVec3 r;
    r.x = a.x - b.x;
    r.y = a.y - b.y;
    r.z = a.z - b.z;
    return r;
}

/**
 * @brief 向量加法
 */
CSGVec3 csg_vec3_add(CSGVec3 a, CSGVec3 b) {
    CSGVec3 r;
    r.x = a.x + b.x;
    r.y = a.y + b.y;
    r.z = a.z + b.z;
    return r;
}

/**
 * @brief 标量乘法
 */
CSGVec3 csg_vec3_scale(CSGVec3 v, double s) {
    CSGVec3 r;
    r.x = v.x * s;
    r.y = v.y * s;
    r.z = v.z * s;
    return r;
}

/**
 * @brief 向量归一化
 */
CSGVec3 csg_vec3_normalize(CSGVec3 v) {
    double len = sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    if (len < CSG_BSP_EPSILON) {
        CSGVec3 zero = {0.0, 0.0, 0.0};
        return zero;
    }
    return csg_vec3_scale(v, 1.0 / len);
}

/**
 * @brief 计算三角形面的法向量
 */
CSGVec3 csg_tri_normal(const CSGTriangle *tri) {
    CSGVec3 e1 = csg_vec3_sub(tri->v[1], tri->v[0]);
    CSGVec3 e2 = csg_vec3_sub(tri->v[2], tri->v[0]);
    return csg_vec3_normalize(csg_vec3_cross(e1, e2));
}

/**
 * @brief 判断点到平面的有符号距离
 * @param plane_point  平面上一点
 * @param plane_normal 平面法向
 * @param point        待测点
 * @return 有符号距离（正 = 法向一侧，负 = 反向一侧）
 */
double csg_signed_distance(CSGVec3 plane_point, CSGVec3 plane_normal, CSGVec3 point) {
    CSGVec3 d = csg_vec3_sub(point, plane_point);
    return csg_vec3_dot(d, plane_normal);
}

/**
 * @brief 初始化三角形面列表
 */
void csg_trilist_init(CSGTriList *list, int init_cap) {
    list->count = 0;
    list->capacity = init_cap > 0 ? init_cap : CSG_MAX_TRI_BUFFER;
    list->tris = (CSGTriangle *) lv_calloc((size_t) list->capacity, sizeof(CSGTriangle));
    if (!list->tris) {
        list->capacity = 0;
    }
}

/**
 * @brief 向三角形面列表追加一个三角形
 */
void csg_trilist_append(CSGTriList *list, const CSGTriangle *tri) {
    if (list->count >= list->capacity) {
        if (list->capacity > INT_MAX / 2)
            return;
        int new_cap = list->capacity * 2;
        CSGTriangle *new_tris = (CSGTriangle *) lv_realloc(list->tris, (size_t) new_cap * sizeof(CSGTriangle));
        if (!new_tris)
            return;
        list->tris = new_tris;
        list->capacity = new_cap;
    }
    list->tris[list->count] = *tri;
    list->count++;
}

/**
 * @brief 释放三角形面列表
 */
void csg_trilist_free(CSGTriList *list) {
    if (list->tris) {
        lv_free((void **) &list->tris);
    }
    list->count = 0;
    list->capacity = 0;
}

/* ================================================================
 * CSG 节点生命周期管理
 * ================================================================ */

/**
 * @brief 创建新的 CSG 构造树节点
 *
 * 分配内存并初始化默认值。变换矩阵初始化为单位矩阵，
 * 包围盒初始化为无效范围。
 *
 * @param kind  CSG 节点类型
 * @return 新分配的 CSGNode 指针，失败返回 NULL
 */
