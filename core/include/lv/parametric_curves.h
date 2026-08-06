/**
 * @file parametric_curves.h
 * @brief 参数曲线曲面模块 -- 参数化几何对象的创建、求值与分析
 *
 * 提供参数曲线（一维参数域 -> R^2）和参数曲面（二维参数域 -> R^3）
 * 的创建、求值、切线/法线计算以及弧长/面积数值积分功能。
 */

#ifndef lv_PARAMETRIC_CURVES_H
#define lv_PARAMETRIC_CURVES_H

#include <stdbool.h>

#include "lv_vec3.h" /* lvVec3：3D 向量单一事实来源，lvPoint3D 为其 typedef */

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 数据类型
 * ============================================================ */

/** 二维点（double 精度） */
typedef struct {
    double x, y;
} lvPoint2D;

/** 三维点（double 精度，收敛：typedef 到公共 lvVec3，结构逐位一致） */
typedef lvVec3 lvPoint3D;

/** 参数曲线求值函数: C(t) -> Point2D */
typedef void (*lvCurveEvalFunc)(double t, void *user_data, lvPoint2D *out);

/** 参数曲线导数函数: C'(t) -> (dx/dt, dy/dt) */
typedef void (*lvCurveDerivFunc)(double t, void *user_data, double *out_dx, double *out_dy);

/** 参数曲面求值函数: S(u,v) -> Point3D */
typedef void (*lvSurfaceEvalFunc)(double u, double v, void *user_data, lvPoint3D *out);

/** 参数曲面偏导数函数: dP/du, dP/dv */
typedef void (*lvSurfaceDerivFunc)(double u, double v, void *user_data, lvPoint3D *out_du, lvPoint3D *out_dv);

/** 一维参数域 */
typedef struct {
    double t_min, t_max;
} lvParametricDomain1D;

/** 二维参数域（矩形） */
typedef struct {
    double u_min, u_max, v_min, v_max;
} lvParametricDomain2D;

/* 前向声明 */
struct lvParametricCurve;
struct lvParametricSurface;

/* ============================================================
 * 参数曲线 API
 * ============================================================ */

struct lvParametricCurve *lv_curve_create(double t_min, double t_max, lvCurveEvalFunc eval_func,
                                          lvCurveDerivFunc deriv_func, void *user_data, bool is_closed);

void lv_curve_destroy(struct lvParametricCurve *curve);
bool lv_curve_evaluate(const struct lvParametricCurve *curve, double t, lvPoint2D *out);
bool lv_curve_tangent(const struct lvParametricCurve *curve, double t, double *out_dx, double *out_dy);
double lv_curve_arc_length(const struct lvParametricCurve *curve, int n_steps);
bool lv_curve_get_domain(const struct lvParametricCurve *curve, double *out_t_min, double *out_t_max);
bool lv_curve_is_closed(const struct lvParametricCurve *curve);

/* ============================================================
 * 参数曲面 API
 * ============================================================ */

struct lvParametricSurface *lv_surface_create(double u_min, double u_max, double v_min, double v_max,
                                              lvSurfaceEvalFunc eval_func, lvSurfaceDerivFunc deriv_func,
                                              void *user_data);

void lv_surface_destroy(struct lvParametricSurface *surf);
bool lv_surface_evaluate(const struct lvParametricSurface *surf, double u, double v, lvPoint3D *out);
bool lv_surface_normal(const struct lvParametricSurface *surf, double u, double v, double *out_nx, double *out_ny,
                       double *out_nz);
double lv_surface_area(const struct lvParametricSurface *surf, int n_u, int n_v);
bool lv_surface_get_domain(const struct lvParametricSurface *surf, lvParametricDomain2D *out);

#ifdef __cplusplus
}
#endif

#endif /* lv_PARAMETRIC_CURVES_H */
