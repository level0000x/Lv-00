/**
 * @file parametric_curves.h
 * @brief 参数曲线曲面模块 -- 参数化几何对象的创建、求值与分析
 *
 * 提供参数曲线（一维参数域 -> R^2）和参数曲面（二维参数域 -> R^3）
 * 的创建、求值、切线/法线计算以及弧长/面积数值积分功能。
 */

#ifndef LV00_PARAMETRIC_CURVES_H
#define LV00_PARAMETRIC_CURVES_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 数据类型
 * ============================================================ */

/** 二维点（double 精度） */
typedef struct { double x, y; } Lv00Point2D;

/** 三维点（double 精度） */
typedef struct { double x, y, z; } Lv00Point3D;

/** 参数曲线求值函数: C(t) -> Point2D */
typedef void (*Lv00CurveEvalFunc)(double t, void *user_data, Lv00Point2D *out);

/** 参数曲线导数函数: C'(t) -> (dx/dt, dy/dt) */
typedef void (*Lv00CurveDerivFunc)(double t, void *user_data,
                                   double *out_dx, double *out_dy);

/** 参数曲面求值函数: S(u,v) -> Point3D */
typedef void (*Lv00SurfaceEvalFunc)(double u, double v, void *user_data,
                                    Lv00Point3D *out);

/** 参数曲面偏导数函数: dP/du, dP/dv */
typedef void (*Lv00SurfaceDerivFunc)(double u, double v, void *user_data,
                                     Lv00Point3D *out_du, Lv00Point3D *out_dv);

/** 一维参数域 */
typedef struct { double t_min, t_max; } Lv00ParametricDomain1D;

/** 二维参数域（矩形） */
typedef struct { double u_min, u_max, v_min, v_max; } Lv00ParametricDomain2D;

/* 前向声明 */
struct Lv00ParametricCurve;
struct Lv00ParametricSurface;

/* ============================================================
 * 参数曲线 API
 * ============================================================ */

struct Lv00ParametricCurve *lv00_curve_create(
    double t_min, double t_max,
    Lv00CurveEvalFunc eval_func, Lv00CurveDerivFunc deriv_func,
    void *user_data, bool is_closed);

void  lv00_curve_destroy(struct Lv00ParametricCurve *curve);
int  lv00_curve_evaluate(const struct Lv00ParametricCurve *curve,
                          double t, Lv00Point2D *out);
int  lv00_curve_tangent(const struct Lv00ParametricCurve *curve,
                         double t, double *out_dx, double *out_dy);
double lv00_curve_arc_length(const struct Lv00ParametricCurve *curve,
                             int n_steps);
int  lv00_curve_get_domain(const struct Lv00ParametricCurve *curve,
                            double *out_t_min, double *out_t_max);
bool  lv00_curve_is_closed(const struct Lv00ParametricCurve *curve);

/* ============================================================
 * 参数曲面 API
 * ============================================================ */

struct Lv00ParametricSurface *lv00_surface_create(
    double u_min, double u_max, double v_min, double v_max,
    Lv00SurfaceEvalFunc eval_func, Lv00SurfaceDerivFunc deriv_func,
    void *user_data);

void  lv00_surface_destroy(struct Lv00ParametricSurface *surf);
int  lv00_surface_evaluate(const struct Lv00ParametricSurface *surf,
                            double u, double v, Lv00Point3D *out);
int  lv00_surface_normal(const struct Lv00ParametricSurface *surf,
                          double u, double v,
                          double *out_nx, double *out_ny, double *out_nz);
double lv00_surface_area(const struct Lv00ParametricSurface *surf,
                         int n_u, int n_v);
int  lv00_surface_get_domain(const struct Lv00ParametricSurface *surf,
                              Lv00ParametricDomain2D *out);

#ifdef __cplusplus
}
#endif

#endif /* LV00_PARAMETRIC_CURVES_H */
