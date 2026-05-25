/**
 * @file ga_interface.c
 * @brief 几何代数接口实现 —— 投影几何代数 (PGA) 嵌入与提取
 *
 * @details 实现标准三维几何表示与投影几何代数 Cl(3,0,1) 编码之间的相互转换。
 *          支持点、向量、平面、射线、旋子（旋转）和电机（平移）的嵌入与提取操作。
 *
 *          PGA 约定 (Cl(3,0,1))：
 *          - 点 (x,y,z):  P = x*e023 + y*e013 + z*e012 + e123
 *          - 向量 (vx,vy,vz): v = vx*e1 + vy*e2 + vz*e3
 *          - 平面 (nx,ny,nz,d): pi = nx*e1 + ny*e2 + nz*e3 + d*e0
 *          - 经过 P,Q 的直线: L = P ^ Q (二重向量)
 *          - 旋子 (axis, angle): R = cos(a/2) + sin(a/2)*(ax*e23 + ay*e13 + az*e12)
 *          - 电机 (tx,ty,tz): T = 1 + 0.5*(tx*e01 + ty*e02 + tz*e03)
 *
 *          本模块为 Lv-00 几何元语言提供几何代数层面的计算能力，
 *          支持与约束图、符号坐标等模块的集成。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 *
 * @dependencies
 *   - ga_interface.h    : 几何代数接口公共定义
 *   - ga_multivector.h  : 多向量数据结构
 *   - lv00_utils.h      : 统一内存分配器
 *
 * @note 内存管理：所有动态分配使用 ga_mv_malloc/ga_mv_free，
 *        这与 lv00_malloc/lv00_free 不同，因为 Lv00MultiVector
 *        内部使用特殊的内存布局。
 */

#include "ga_interface.h"
#include "ga_multivector.h"
#include "lv00_utils.h"

#include <math.h>
#include <string.h>

/* ========================================================================
 * 点运算 (Point Operations)
 * ======================================================================== */

/**
 * @brief 将三维坐标嵌入为 PGA 点表示
 *
 * 使用齐次坐标表示法，将普通三维点 (x,y,z) 转换为 PGA 多向量形式。
 * 齐次坐标的 e123 分量固定为 1.0，使得仿射变换可以直接用外积表达。
 *
 * @param x 点的 x 坐标
 * @param y 点的 y 坐标
 * @param z 点的 z 坐标
 * @return 嵌入后的多向量指针，失败返回 NULL
 *
 * @note 调用方负责通过 ga_mv_free() 释放返回的指针
 */
Lv00MultiVector *ga_embed_point(double x, double y, double z) {
    Lv00MultiVector *mv = ga_mv_zero();
    if (!mv) return NULL;

    /* P = x*e023 + y*e013 + z*e012 + e123 */
    mv->components[GA_BLADE_E023]  = x;  /* e023 分量存储 x 坐标 */
    mv->components[GA_BLADE_E013]  = y;  /* e013 分量存储 y 坐标 */
    mv->components[GA_BLADE_E0123] = z;  /* e0123 分量存储 z 坐标 */
    mv->components[GA_BLADE_E123]  = 1.0; /* e123 齐次坐标（固定为 1.0）*/

    return mv;
}

/**
 * @brief 从 PGA 多向量提取三维点坐标
 *
 * 通过归一化齐次坐标分量来恢复原始三维坐标。
 *
 * @param mv 输入的多向量指针
 * @param out_x 输出参数，接收 x 坐标
 * @param out_y 输出参数，接收 y 坐标
 * @param out_z 输出参数，接收 z 坐标
 * @return 0 成功，-1 失败（参数无效或齐次分量为零）
 */
int ga_extract_point(const Lv00MultiVector *mv,
                      double *out_x, double *out_y, double *out_z) {
    if (!mv || !out_x || !out_y || !out_z) return -1;

    /* 通过 e123（齐次）分量进行归一化 */
    double w = mv->components[GA_BLADE_E123];
    if (fabs(w) < 1e-15) return -1; /* 齐次分量为零，无法归一化 */

    *out_x = mv->components[GA_BLADE_E023] / w;
    *out_y = mv->components[GA_BLADE_E013] / w;
    *out_z = mv->components[GA_BLADE_E0123] / w;

    return 0;
}

/* ========================================================================
 * 向量运算 (Vector Operations)
 * ======================================================================== */

/**
 * @brief 将三维向量嵌入为 PGA 纯向量表示
 *
 * 纯向量用于表示方向和位移，不包含位置信息。
 * 相比点表示，纯向量缺少 e123 齐次分量。
 *
 * @param vx 向量的 x 分量
 * @param vy 向量的 y 分量
 * @param vz 向量的 z 分量
 * @return 嵌入后的多向量指针，失败返回 NULL
 */
Lv00MultiVector *ga_embed_vector(double vx, double vy, double vz) {
    Lv00MultiVector *mv = ga_mv_zero();
    if (!mv) return NULL;

    /* v = vx*e1 + vy*e2 + vz*e3 */
    mv->components[GA_BLADE_E1] = vx;
    mv->components[GA_BLADE_E2] = vy;
    mv->components[GA_BLADE_E3] = vz;

    return mv;
}

/**
 * @brief 从 PGA 多向量提取三维向量分量
 *
 * @param mv 输入的多向量指针
 * @param out_vx 输出参数，接收 x 分量
 * @param out_vy 输出参数，接收 y 分量
 * @param out_vz 输出参数，接收 z 分量
 * @return 0 成功，-1 失败（参数无效）
 */
int ga_extract_vector(const Lv00MultiVector *mv,
                       double *out_vx, double *out_vy, double *out_vz) {
    if (!mv || !out_vx || !out_vy || !out_vz) return -1;

    *out_vx = mv->components[GA_BLADE_E1];
    *out_vy = mv->components[GA_BLADE_E2];
    *out_vz = mv->components[GA_BLADE_E3];

    return 0;
}

/* ========================================================================
 * 平面运算 (Plane Operations)
 * ======================================================================== */

/**
 * @brief 将平面参数嵌入为 PGA 平面表示
 *
 * 平面由法向量 (nx, ny, nz) 和距离 d 定义。
 *
 * @param nx 平面法向量的 x 分量
 * @param ny 平面法向量的 y 分量
 * @param nz 平面法向量的 z 分量
 * @param d  平面到原点的有符号距离
 * @return 嵌入后的多向量指针，失败返回 NULL
 */
Lv00MultiVector *ga_embed_plane(double nx, double ny, double nz, double d) {
    Lv00MultiVector *mv = ga_mv_zero();
    if (!mv) return NULL;

    /* pi = nx*e1 + ny*e2 + nz*e3 + d*e0 */
    mv->components[GA_BLADE_E1] = nx;
    mv->components[GA_BLADE_E2] = ny;
    mv->components[GA_BLADE_E3] = nz;
    mv->components[GA_BLADE_E0] = d;

    return mv;
}

/**
 * @brief 从 PGA 多向量提取平面参数
 *
 * @param mv 输入的多向量指针
 * @param out_nx 输出参数，接收法向量的 x 分量
 * @param out_ny 输出参数，接收法向量的 y 分量
 * @param out_nz 输出参数，接收法向量的 z 分量
 * @param out_d  输出参数，接收平面到原点的距离
 * @return 0 成功，-1 失败（参数无效）
 */
int ga_extract_plane(const Lv00MultiVector *mv,
                      double *out_nx, double *out_ny, double *out_nz,
                      double *out_d) {
    if (!mv || !out_nx || !out_ny || !out_nz || !out_d) return -1;

    *out_nx = mv->components[GA_BLADE_E1];
    *out_ny = mv->components[GA_BLADE_E2];
    *out_nz = mv->components[GA_BLADE_E3];
    *out_d  = mv->components[GA_BLADE_E0];

    return 0;
}

/* ========================================================================
 * 射线运算 (Ray Operations)
 * ======================================================================== */

/**
 * @brief 通过起点和方向向量构造射线
 *
 * 使用外积运算将起点和方向向量组合为射线表示。
 *
 * @param origin 射线的起点
 * @param dir    射线的方向向量
 * @return 嵌入后的多向量指针，失败返回 NULL
 */
Lv00MultiVector *ga_embed_ray(const Lv00MultiVector *origin,
                               const Lv00MultiVector *dir) {
    if (!origin || !dir) return NULL;

    /* 射线 = 起点 ^ 方向向量（外积） */
    return ga_outer_product(origin, dir);
}

/**
 * @brief 从射线多向量提取起点和方向
 *
 * 注意：完整的射线提取需要额外的几何上下文，
 * 当前实现将起点默认为原点，方向从二重向量分量提取。
 *
 * @param mv 输入的射线多向量
 * @param out_origin 输出参数，接收起点指针
 * @param out_dir    输出参数，接收方向向量指针
 * @return 0 成功，-1 失败（参数无效或内存分配失败）
 *
 * @note 调用方负责通过 ga_mv_free() 释放返回的指针
 */
int ga_extract_ray(const Lv00MultiVector *mv,
                    Lv00MultiVector **out_origin,
                    Lv00MultiVector **out_dir) {
    if (!mv || !out_origin || !out_dir) return -1;

    /* PGA 中射线是二重向量，简单提取时将起点设为原点，
     * 方向从二重向量分量获取。完整提取需要额外的几何上下文。 */
    *out_origin = ga_embed_point(0.0, 0.0, 0.0);
    *out_dir = ga_embed_vector(
        mv->components[GA_BLADE_E12],
        mv->components[GA_BLADE_E13],
        mv->components[GA_BLADE_E23]
    );

    /* 错误处理：任一分配失败时清理并返回错误 */
    if (!*out_origin || !*out_dir) {
        ga_mv_free(*out_origin);
        ga_mv_free(*out_dir);
        *out_origin = NULL;
        *out_dir = NULL;
        return -1;
    }

    return 0;
}

/* ========================================================================
 * 旋子运算 (Rotor Operations) - 旋转
 * ======================================================================== */

/**
 * @brief 构造绕指定轴旋转指定角度的旋子
 *
 * 旋子是 PGA 中表示旋转的基本元素，可直接用于点的旋转变换。
 *
 * @param ax     旋转轴向量的 x 分量（应为单位向量）
 * @param ay     旋转轴向量的 y 分量
 * @param az     旋转轴向量的 z 分量
 * @param angle  旋转角度（弧度制）
 * @return 旋子多向量指针，失败返回 NULL
 */
Lv00MultiVector *ga_embed_rotation(double ax, double ay, double az,
                                    double angle) {
    Lv00MultiVector *mv = ga_mv_zero();
    if (!mv) return NULL;

    double half_angle = angle * 0.5;
    double cos_half = cos(half_angle);
    double sin_half = sin(half_angle);

    /* R = cos(theta/2) + sin(theta/2)*(ax*e23 + ay*e13 + az*e12) */
    mv->components[GA_BLADE_1]   = cos_half;
    mv->components[GA_BLADE_E23]  = sin_half * ax;
    mv->components[GA_BLADE_E13]  = sin_half * ay;
    mv->components[GA_BLADE_E12]  = sin_half * az;

    return mv;
}

/**
 * @brief 从旋子提取旋转轴和角度
 *
 * @param rotor    输入的旋子多向量
 * @param out_ax   输出参数，接收旋转轴的 x 分量
 * @param out_ay   输出参数，接收旋转轴的 y 分量
 * @param out_az   输出参数，接收旋转轴的 z 分量
 * @param out_angle 输出参数，接收旋转角度（弧度制）
 * @return 0 成功，-1 失败（参数无效）
 */
int ga_extract_rotation(const Lv00MultiVector *rotor,
                         double *out_ax, double *out_ay, double *out_az,
                         double *out_angle) {
    if (!rotor || !out_ax || !out_ay || !out_az || !out_angle) return -1;

    /* 提取标量（余弦）部分和二重向量（正弦）部分 */
    double cos_half = rotor->components[GA_BLADE_1];
    double sin_x = rotor->components[GA_BLADE_E23];
    double sin_y = rotor->components[GA_BLADE_E13];
    double sin_z = rotor->components[GA_BLADE_E12];

    double sin_half = sqrt(sin_x * sin_x + sin_y * sin_y + sin_z * sin_z);

    if (sin_half < 1e-15) {
        /* 零旋转情况：返回默认轴和零角度 */
        *out_ax = 0.0;
        *out_ay = 0.0;
        *out_az = 1.0;
        *out_angle = 0.0;
        return 0;
    }

    /* 归一化轴向量并计算旋转角度 */
    *out_ax = sin_x / sin_half;
    *out_ay = sin_y / sin_half;
    *out_az = sin_z / sin_half;
    *out_angle = 2.0 * atan2(sin_half, cos_half);

    return 0;
}

/* ========================================================================
 * 电机运算 (Motor Operations) - 平移
 * ======================================================================== */

/**
 * @brief 构造沿指定方向的平移电机
 *
 * 电机是 PGA 中表示平移的基本元素，可直接用于点的平移变换。
 *
 * @param tx 沿 x 轴的平移距离
 * @param ty 沿 y 轴的平移距离
 * @param tz 沿 z 轴的平移距离
 * @return 电机多向量指针，失败返回 NULL
 */
Lv00MultiVector *ga_embed_translation(double tx, double ty, double tz) {
    Lv00MultiVector *mv = ga_mv_zero();
    if (!mv) return NULL;

    /* T = 1 + 0.5*(tx*e01 + ty*e02 + tz*e03)
     * 刀片排序映射：e01 = e03 (索引7), e02 = e023 (索引9)
     * 映射关系：tx -> e03, ty -> e023, tz -> e013 */
    mv->components[GA_BLADE_1]    = 1.0;
    mv->components[GA_BLADE_E03]  = 0.5 * tx;
    mv->components[GA_BLADE_E023] = 0.5 * ty;
    mv->components[GA_BLADE_E013] = 0.5 * tz;

    return mv;
}

/* ========================================================================
 * 几何构造函数 (Geometric Construction Functions)
 * ======================================================================== */

/**
 * @brief 通过两点构造直线
 *
 * 使用外积运算，两点的外积即为它们确定的直线。
 *
 * @param p1 第一个点
 * @param p2 第二个点
 * @return 表示直线的多向量指针，失败返回 NULL
 */
Lv00MultiVector *ga_line_from_two_points(const Lv00MultiVector *p1,
                                          const Lv00MultiVector *p2) {
    if (!p1 || !p2) return NULL;

    /* 直线 = P1 ^ P2（两点的外积） */
    return ga_outer_product(p1, p2);
}

/**
 * @brief 判断三点是否共线
 *
 * 使用外积判零：三线段 P1^P2^P3 = 0 当且仅当三点共线。
 *
 * @param p1 第一个点
 * @param p2 第二个点
 * @param p3 第三个点
 * @return true 共线，false 不共线或参数无效
 */
bool ga_three_points_collinear(const Lv00MultiVector *p1,
                                const Lv00MultiVector *p2,
                                const Lv00MultiVector *p3) {
    if (!p1 || !p2 || !p3) return false;

    /* 三点共线当且仅当 P1 ^ P2 ^ P3 = 0
     * 分步计算外积并检查结果是否为零 */
    Lv00MultiVector *l12 = ga_outer_product(p1, p2);
    if (!l12) return false;

    Lv00MultiVector *l123 = ga_outer_product(l12, p3);
    ga_mv_free(l12); /* 及时释放中间结果，避免内存泄漏 */
    if (!l123) return false;

    /* 检查所有分量是否接近零 */
    bool collinear = true;
    for (int i = 0; i < GA_MV_DIM; i++) {
        if (fabs(l123->components[i]) > 1e-10) {
            collinear = false;
            break;
        }
    }

    ga_mv_free(l123);
    return collinear;
}

/**
 * @brief 判断四点是否共面
 *
 * 使用外积判零：四线段 P1^P2^P3^P4 = 0 当且仅当四点共面。
 *
 * @param p1 第一个点
 * @param p2 第二个点
 * @param p3 第三个点
 * @param p4 第四个点
 * @return true 共面，false 不共面或参数无效
 */
bool ga_four_points_coplanar(const Lv00MultiVector *p1,
                              const Lv00MultiVector *p2,
                              const Lv00MultiVector *p3,
                              const Lv00MultiVector *p4) {
    if (!p1 || !p2 || !p3 || !p4) return false;

    /* 四点共面当且仅当 P1 ^ P2 ^ P3 ^ P4 = 0 */
    Lv00MultiVector *l12 = ga_outer_product(p1, p2);
    if (!l12) return false;

    Lv00MultiVector *l123 = ga_outer_product(l12, p3);
    ga_mv_free(l12); /* 及时释放中间结果 */
    if (!l123) return false;

    Lv00MultiVector *l1234 = ga_outer_product(l123, p4);
    ga_mv_free(l123); /* 及时释放中间结果 */
    if (!l1234) return false;

    /* 检查所有分量是否接近零 */
    bool coplanar = true;
    for (int i = 0; i < GA_MV_DIM; i++) {
        if (fabs(l1234->components[i]) > 1e-10) {
            coplanar = false;
            break;
        }
    }

    ga_mv_free(l1234);
    return coplanar;
}

/**
 * @brief 通过三点构造平面
 *
 * 使用外积运算，三点的外积即为它们确定的平面。
 *
 * @param p1 第一个点（应在平面上）
 * @param p2 第二个点（应在平面上）
 * @param p3 第三个点（应在平面上）
 * @return 表示平面的多向量指针，失败返回 NULL
 */
Lv00MultiVector *ga_plane_from_three_points(const Lv00MultiVector *p1,
                                             const Lv00MultiVector *p2,
                                             const Lv00MultiVector *p3) {
    if (!p1 || !p2 || !p3) return NULL;

    /* 平面 = P1 ^ P2 ^ P3（三点的外积） */
    Lv00MultiVector *l12 = ga_outer_product(p1, p2);
    if (!l12) return NULL;

    Lv00MultiVector *plane = ga_outer_product(l12, p3);
    ga_mv_free(l12); /* 及时释放中间结果 */
    return plane;
}
