/**
 * @file parametric_curves.c
 * @brief 参数曲线曲面模块 -- 参数化几何对象的创建、求值与分析
 *
 * @details 实现参数曲线和参数曲面的基本操作：
 *          - 参数曲线：一维参数域 [t_min, t_max] -> R^2/R^3
 *          - 参数曲面：二维参数域 [u_min,u_max] x [v_min,v_max] -> R^3
 *
 *          支持用户自定义求值函数和导数/切线函数，
 *          提供弧长、面积等积分量的数值计算。
 *
 * @author Lv-00 Project
 * @version 1.0.0
 */

#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "lv00_utils.h"

/* ============================================================
 * 第一部分：数据结构定义
 * ============================================================ */

/** 默认弧长/面积积分数值积分的子区间数 */
#define PARAMETRIC_DEFAULT_INTEGRATION_STEPS 64

/** 最大允许的积分子区间数（防止资源耗尽） */
#define PARAMETRIC_MAX_INTEGRATION_STEPS 10000

/**
 * @brief 二维点（double 精度，用于数值求值）
 */
typedef struct {
    double x;
    double y;
} Lv00Point2D;

/**
 * @brief 三维点（double 精度，用于数值求值）
 */
typedef struct {
    double x;
    double y;
    double z;
} Lv00Point3D;

/**
 * @brief 参数曲线的求值函数类型
 *
 * @param t       参数值
 * @param user_data 用户自定义数据指针
 * @param out     输出点坐标
 */
typedef void (*Lv00CurveEvalFunc)(double t, void *user_data, Lv00Point2D *out);

/**
 * @brief 参数曲线的导数（切线向量）函数类型
 *
 * @param t       参数值
 * @param user_data 用户自定义数据指针
 * @param out_dx  输出 dx/dt
 * @param out_dy  输出 dy/dt
 */
typedef void (*Lv00CurveDerivFunc)(double t, void *user_data,
                                    double *out_dx, double *out_dy);

/**
 * @brief 参数曲面的求值函数类型
 *
 * @param u, v    参数值
 * @param user_data 用户自定义数据指针
 * @param out     输出点坐标
 */
typedef void (*Lv00SurfaceEvalFunc)(double u, double v, void *user_data,
                                     Lv00Point3D *out);

/**
 * @brief 参数曲面的偏导数函数类型
 *
 * @param u, v      参数值
 * @param user_data 用户自定义数据指针
 * @param out_du    输出偏导 dP/du (三维向量)
 * @param out_dv    输出偏导 dP/dv (三维向量)
 */
typedef void (*Lv00SurfaceDerivFunc)(double u, double v, void *user_data,
                                      Lv00Point3D *out_du, Lv00Point3D *out_dv);

/**
 * @brief 参数域（一维区间）
 */
typedef struct {
    double t_min;  /**< 参数下界 */
    double t_max;  /**< 参数上界 */
} Lv00ParametricDomain1D;

/**
 * @brief 参数域（二维矩形区域）
 */
typedef struct {
    double u_min;  /**< u 参数下界 */
    double u_max;  /**< u 参数上界 */
    double v_min;  /**< v 参数下界 */
    double v_max;  /**< v 参数上界 */
} Lv00ParametricDomain2D;

/**
 * @brief 参数曲线结构体
 *
 * 表示一条参数曲线 C(t) = (x(t), y(t))，t 属于 [t_min, t_max]。
 * 用户通过函数指针提供求值和导数计算逻辑。
 */
typedef struct Lv00ParametricCurve {
    Lv00ParametricDomain1D domain;      /**< 参数域 */
    Lv00CurveEvalFunc      eval_func;   /**< 求值函数 C(t) */
    Lv00CurveDerivFunc     deriv_func;  /**< 导数函数 C'(t) */
    void                  *user_data;   /**< 用户自定义数据 */
    bool                   is_closed;   /**< 是否闭合曲线 */
} Lv00ParametricCurve;

/**
 * @brief 参数曲面结构体
 *
 * 表示一张参数曲面 S(u,v) = (x,y,z)，(u,v) 属于矩形参数域。
 */
typedef struct Lv00ParametricSurface {
    Lv00ParametricDomain2D domain;      /**< 参数域 */
    Lv00SurfaceEvalFunc    eval_func;   /**< 求值函数 S(u,v) */
    Lv00SurfaceDerivFunc   deriv_func;  /**< 偏导数函数 */
    void                  *user_data;   /**< 用户自定义数据 */
} Lv00ParametricSurface;

/* ============================================================
 * 第二部分：参数曲线 API
 * ============================================================ */

/**
 * @brief 创建参数曲线
 *
 * @param t_min     参数下界
 * @param t_max     参数上界（必须 > t_min）
 * @param eval_func 求值函数（非 NULL）
 * @param deriv_func 导数函数（可为 NULL，此时 tangent 和 arc_length 不可用）
 * @param user_data 用户数据指针
 * @param is_closed 是否闭合曲线
 * @return 新曲线指针，失败返回 NULL
 */
Lv00ParametricCurve *lv00_curve_create(double t_min, double t_max,
                                        Lv00CurveEvalFunc eval_func,
                                        Lv00CurveDerivFunc deriv_func,
                                        void *user_data,
                                        bool is_closed) {
    if (!eval_func || t_min >= t_max) {
        return NULL;
    }

    Lv00ParametricCurve *curve = (Lv00ParametricCurve *)lv00_malloc(
        sizeof(Lv00ParametricCurve));
    if (!curve) {
        return NULL;
    }

    curve->domain.t_min = t_min;
    curve->domain.t_max = t_max;
    curve->eval_func = eval_func;
    curve->deriv_func = deriv_func;
    curve->user_data = user_data;
    curve->is_closed = is_closed;

    return curve;
}

/**
 * @brief 销毁参数曲线，释放资源
 *
 * @param curve 曲线指针（可为 NULL，此时为空操作）
 */
void lv00_curve_destroy(Lv00ParametricCurve *curve) {
    if (!curve) return;
    lv00_free((void **)&curve);
}

/**
 * @brief 在指定参数值处求曲线上的点
 *
 * @param curve  曲线指针（非 NULL）
 * @param t      参数值
 * @param out    输出：曲线上对应点
 * @return true  成功
 * @return false 失败（参数无效）
 */
bool lv00_curve_evaluate(const Lv00ParametricCurve *curve, double t,
                          Lv00Point2D *out) {
    if (!curve || !curve->eval_func || !out) {
        return false;
    }
    curve->eval_func(t, curve->user_data, out);
    return true;
}

/**
 * @brief 在指定参数值处求曲线的切线向量
 *
 * @param curve  曲线指针（非 NULL，deriv_func 非 NULL）
 * @param t      参数值
 * @param out_dx 输出：dx/dt
 * @param out_dy 输出：dy/dt
 * @return true  成功
 * @return false 失败（无导数函数）
 */
bool lv00_curve_tangent(const Lv00ParametricCurve *curve, double t,
                         double *out_dx, double *out_dy) {
    if (!curve || !curve->deriv_func || !out_dx || !out_dy) {
        return false;
    }
    curve->deriv_func(t, curve->user_data, out_dx, out_dy);
    return true;
}

/**
 * @brief 计算曲线的近似弧长（复合梯形公式）
 *
 * 在参数域 [t_min, t_max] 上均匀采样 n 个区间，
 * 使用复合梯形公式近似计算弧长积分 L = integral(|C'(t)| dt)。
 *
 * @param curve  曲线指针（非 NULL，deriv_func 非 NULL）
 * @param n_steps 积分子区间数（0 使用默认值 64）
 * @return 弧长值，失败返回 -1.0
 */
double lv00_curve_arc_length(const Lv00ParametricCurve *curve, int n_steps) {
    if (!curve || !curve->deriv_func) {
        return -1.0;
    }

    if (n_steps <= 0) {
        n_steps = PARAMETRIC_DEFAULT_INTEGRATION_STEPS;
    }
    if (n_steps > PARAMETRIC_MAX_INTEGRATION_STEPS) {
        n_steps = PARAMETRIC_MAX_INTEGRATION_STEPS;
    }

    double a = curve->domain.t_min;
    double b = curve->domain.t_max;
    double h = (b - a) / (double)n_steps;

    /* 计算端点的 |C'(t)| */
    double dx, dy;
    curve->deriv_func(a, curve->user_data, &dx, &dy);
    double sum = 0.5 * sqrt(dx * dx + dy * dy);

    /* 内部点 */
    for (int i = 1; i < n_steps; i++) {
        double t = a + i * h;
        curve->deriv_func(t, curve->user_data, &dx, &dy);
        sum += sqrt(dx * dx + dy * dy);
    }

    /* 另一端点 */
    curve->deriv_func(b, curve->user_data, &dx, &dy);
    sum += 0.5 * sqrt(dx * dx + dy * dy);

    return sum * h;
}

/**
 * @brief 获取曲线参数域
 *
 * @param curve 曲线指针（非 NULL）
 * @param out_t_min 输出参数下界
 * @param out_t_max 输出参数上界
 * @return true 成功，false 参数无效
 */
bool lv00_curve_get_domain(const Lv00ParametricCurve *curve,
                            double *out_t_min, double *out_t_max) {
    if (!curve || !out_t_min || !out_t_max) return false;
    *out_t_min = curve->domain.t_min;
    *out_t_max = curve->domain.t_max;
    return true;
}

/**
 * @brief 判断曲线是否闭合
 *
 * @param curve 曲线指针（非 NULL）
 * @return true 闭合，false 非闭合或参数无效
 */
bool lv00_curve_is_closed(const Lv00ParametricCurve *curve) {
    return curve ? curve->is_closed : false;
}

/* ============================================================
 * 第三部分：参数曲面 API
 * ============================================================ */

/**
 * @brief 创建参数曲面
 *
 * @param u_min     u 参数下界
 * @param u_max     u 参数上界（必须 > u_min）
 * @param v_min     v 参数下界
 * @param v_max     v 参数上界（必须 > v_min）
 * @param eval_func 求值函数（非 NULL）
 * @param deriv_func 偏导数函数（可为 NULL）
 * @param user_data 用户数据指针
 * @return 新曲面指针，失败返回 NULL
 */
Lv00ParametricSurface *lv00_surface_create(double u_min, double u_max,
                                            double v_min, double v_max,
                                            Lv00SurfaceEvalFunc eval_func,
                                            Lv00SurfaceDerivFunc deriv_func,
                                            void *user_data) {
    if (!eval_func || u_min >= u_max || v_min >= v_max) {
        return NULL;
    }

    Lv00ParametricSurface *surf = (Lv00ParametricSurface *)lv00_malloc(
        sizeof(Lv00ParametricSurface));
    if (!surf) {
        return NULL;
    }

    surf->domain.u_min = u_min;
    surf->domain.u_max = u_max;
    surf->domain.v_min = v_min;
    surf->domain.v_max = v_max;
    surf->eval_func = eval_func;
    surf->deriv_func = deriv_func;
    surf->user_data = user_data;

    return surf;
}

/**
 * @brief 销毁参数曲面，释放资源
 *
 * @param surf 曲面指针（可为 NULL，此时为空操作）
 */
void lv00_surface_destroy(Lv00ParametricSurface *surf) {
    if (!surf) return;
    lv00_free((void **)&surf);
}

/**
 * @brief 在指定参数值处求曲面上的点
 *
 * @param surf  曲面指针（非 NULL）
 * @param u, v  参数值
 * @param out   输出：曲面上对应点
 * @return true 成功，false 参数无效
 */
bool lv00_surface_evaluate(const Lv00ParametricSurface *surf,
                            double u, double v, Lv00Point3D *out) {
    if (!surf || !surf->eval_func || !out) {
        return false;
    }
    surf->eval_func(u, v, surf->user_data, out);
    return true;
}

/**
 * @brief 计算曲面在指定参数值处的法向量
 *
 * 通过偏导数叉积 n = dP/du x dP/dv 计算法向量。
 * 结果未归一化（保持原始长度以保留面积微元信息）。
 *
 * @param surf    曲面指针（非 NULL，deriv_func 非 NULL）
 * @param u, v    参数值
 * @param out_nx  输出法向量 x 分量
 * @param out_ny  输出法向量 y 分量
 * @param out_nz  输出法向量 z 分量
 * @return true   成功
 * @return false  失败（无偏导数函数）
 */
bool lv00_surface_normal(const Lv00ParametricSurface *surf,
                          double u, double v,
                          double *out_nx, double *out_ny, double *out_nz) {
    if (!surf || !surf->deriv_func || !out_nx || !out_ny || !out_nz) {
        return false;
    }

    Lv00Point3D du, dv;
    surf->deriv_func(u, v, surf->user_data, &du, &dv);

    /* 叉积: n = du x dv */
    *out_nx = du.y * dv.z - du.z * dv.y;
    *out_ny = du.z * dv.x - du.x * dv.z;
    *out_nz = du.x * dv.y - du.y * dv.x;

    return true;
}

/**
 * @brief 计算曲面的近似表面积（复合梯形公式）
 *
 * 在参数域上均匀网格采样，使用复合梯形公式近似计算
 * 表面积分 A = integral(|dP/du x dP/dv| du dv)。
 *
 * @param surf     曲面指针（非 NULL，deriv_func 非 NULL）
 * @param n_u      u 方向的子区间数（0 使用默认值）
 * @param n_v      v 方向的子区间数（0 使用默认值）
 * @return 表面积值，失败返回 -1.0
 */
double lv00_surface_area(const Lv00ParametricSurface *surf,
                          int n_u, int n_v) {
    if (!surf || !surf->deriv_func) {
        return -1.0;
    }

    if (n_u <= 0) n_u = PARAMETRIC_DEFAULT_INTEGRATION_STEPS;
    if (n_v <= 0) n_v = PARAMETRIC_DEFAULT_INTEGRATION_STEPS;
    if (n_u > PARAMETRIC_MAX_INTEGRATION_STEPS) n_u = PARAMETRIC_MAX_INTEGRATION_STEPS;
    if (n_v > PARAMETRIC_MAX_INTEGRATION_STEPS) n_v = PARAMETRIC_MAX_INTEGRATION_STEPS;

    double a = surf->domain.u_min;
    double b = surf->domain.u_max;
    double c = surf->domain.v_min;
    double d = surf->domain.v_max;
    double hu = (b - a) / (double)n_u;
    double hv = (d - c) / (double)n_v;

    double total = 0.0;

    for (int i = 0; i <= n_u; i++) {
        double u = a + i * hu;
        /* u 方向权重（梯形公式） */
        double w_u = (i == 0 || i == n_u) ? 0.5 : 1.0;

        for (int j = 0; j <= n_v; j++) {
            double v = c + j * hv;
            /* v 方向权重（梯形公式） */
            double w_v = (j == 0 || j == n_v) ? 0.5 : 1.0;

            /* 计算偏导数叉积的模 */
            Lv00Point3D du, dv;
            surf->deriv_func(u, v, surf->user_data, &du, &dv);

            double nx = du.y * dv.z - du.z * dv.y;
            double ny = du.z * dv.x - du.x * dv.z;
            double nz = du.x * dv.y - du.y * dv.x;
            double mag = sqrt(nx * nx + ny * ny + nz * nz);

            total += w_u * w_v * mag;
        }
    }

    return total * hu * hv;
}

/**
 * @brief 获取曲面参数域
 *
 * @param surf 曲面指针（非 NULL）
 * @param out 输出参数域
 * @return true 成功，false 参数无效
 */
bool lv00_surface_get_domain(const Lv00ParametricSurface *surf,
                              Lv00ParametricDomain2D *out) {
    if (!surf || !out) return false;
    *out = surf->domain;
    return true;
}
