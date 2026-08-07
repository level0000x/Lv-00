/**
 * @file geom_evol.c
 * @brief 几何演化引擎 —— 自适应步长 ODE 求解器实现
 *
 * @details 实现借鉴 SUNDIALS CVODE 的自适应步长演化引擎。
 *          支持四种演化方法（Euler/RK4/Adams/BDF）和 PI 步长控制器，
 *          提供单步演化与主演化循环。
 *
 *          Adams-Bashforth-Moulton 实现为变阶（1~5阶，系数表支持范围）预测-校正法，
 *          BDF 实现为变阶（1~5阶）隐式多步法（Newton 迭代求解）。
 *          实际阶数由内置系数表限制为 1~5；GEOEVOL_ADAMS_MAX_ORDER（geom_evol.h，12）
 *          为多步法历史缓冲区容量。
 *
 * @author Lv-00 Project
 * @version v3.3.0
 * @date 2026-05-24
 *
 * @dependencies
 *   - geom_evol.h             : 演化引擎公共接口与类型定义
 *   - lv_numeric.h          : 轻量数值基础设施
 *   - lv_utils.h            : 统一内存分配器
 *   - lv_internal.h         : 内部常量与工具宏
 *   - error_codes.h           : 统一错误码系统
 */

/* ========================================================================
 * 包含头文件
 * ======================================================================== */

#include "geom_evol.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/config.h"
#include "lv/lv_lifecycle.h"
#include "lv/ode_integrator.h"

#include "error_codes.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "numerical_backend.h"

/* ========================================================================
 * 模块级常量定义
 * ======================================================================== */

/** @brief 默认相对误差容限 */
#define GEOEVOL_DEFAULT_REL_TOL lv_EPSILON_LOW

/** @brief 默认绝对误差容限 */
#define GEOEVOL_DEFAULT_ABS_TOL lv_EPSILON_ULTRA

/** @brief 默认初始步长 */
#define GEOEVOL_DEFAULT_STEP 0.01

/** @brief 误差测试阈值：error <= 1.0 则接受步 */
#define GEOEVOL_ERROR_THRESHOLD 1.0

/** @brief BDF Newton LU 分解的奇异阈值（绝对主元下界）。
 *  刻意取极小值 1e-30，仅在主元真正趋于零时判定奇异并回退对角近似；
 *  项目公共 epsilon（lv_EPSILON_*，最小 1e-15）均大于该值，替换会改变
 *  奇异判定行为，故保持原数值、仅命名化。 */
#define GEOEVOL_LU_SINGULAR_EPS 1e-30

/** @brief Adams 方法最大历史步数（由 geom_evol.h 统一定义，此处不再重复） */

/* ========================================================================
 * 静态辅助函数的前向声明
 * ======================================================================== */

/** @brief 计算误差加权向量：w_i = rel_tol * |y_i| + abs_tol */
static void geoevol_calc_error_weights(lvGeomEvol *evol);

/** @brief 计算局部截断误差估计（RK4 嵌入式 RK3 方法） */
static double geoevol_error_estimate_rk4(lvGeomEvol *evol, const double *y1, const double *y_trial);

/** @brief Euler 法单步积分 */
static int geoevol_step_euler(lvGeomEvol *evol, double h, const double *y, double *y_out);

/** @brief RK4 法单步积分 */
static int geoevol_step_rk4(lvGeomEvol *evol, double h, const double *y, double *y_out);

/** @brief Adams 法单步积分（变阶预测-校正） */
static int geoevol_step_adams(lvGeomEvol *evol, double h, const double *y, double *y_out);

/** @brief BDF 法单步积分（变阶 Newton 迭代） */
static int geoevol_step_bdf(lvGeomEvol *evol, double h, const double *y, double *y_out);

/** @brief 求 RHS 并递增求值计数 */
static int geoevol_rhs_eval(lvGeomEvol *evol, double t, const double *y, double *dy);

/** @brief 将当前状态存入多步法历史缓冲区 */
static void geoevol_ms_history_push(lvGeomEvol *evol, double t, const double *y, const double *f);

/** @brief 从循环缓冲区中按偏移量获取历史 y 指针（0=最近一步，1=上一步...） */
static const double *geoevol_ms_hist_y(const lvGeomEvol *evol, int offset);

/** @brief 从循环缓冲区中按偏移量获取历史 f 指针（0=最近一步，1=上一步...） */
static const double *geoevol_ms_hist_f(const lvGeomEvol *evol, int offset);

/** @brief 获取历史时间戳（0=最近一步，1=上一步...） */
static double geoevol_ms_hist_t(const lvGeomEvol *evol, int offset);

/** @brief 清空多步法历史缓冲区（重置启动阶段） */
static void geoevol_ms_history_reset(lvGeomEvol *evol);

/* ========================================================================
 * 辅助函数实现
 * ======================================================================== */

/**
 * @brief 计算误差加权向量
 *
 * w_i = 1.0 / (rel_tol * |y_i| + abs_tol)
 * 借鉴 CVODE 的误差控制，混合使用相对和绝对容限。
 */
static void geoevol_calc_error_weights(lvGeomEvol *evol) {
    if (!evol) {
        return;
    }
    int dim = evol->dim;
    double rt = evol->rel_tol;
    double at = evol->abs_tol;
    for (int i = 0; i < dim; ++i) {
        evol->error_weight[i] = 1.0 / (rt * fabs(evol->param[i]) + at);
    }
}

/**
 * @brief 计算 RK4 局部截断误差估计（Richardson 外推法）
 *
 * 使用 Richardson 外推估计误差：
 *   1. 用步长 h 执行一步 RK4 得到 y_h
 *   2. 用步长 h/2 执行两步 RK4 得到 y_{h/2}
 *   3. 误差估计 = (y_{h/2} - y_h) / (2^p - 1)，其中 p = 4（RK4 阶数）
 *
 * 对于经典 RK4（p=4），Richardson 因子为 1/15。
 * 误差 = max_i |(y_{h/2}[i] - y_h[i]) / 15.0 * weight[i]|
 *
 * 相比嵌入式 RK 方法，Richardson 外推不需要额外的低阶积分器，
 * 且能提供可靠的渐近误差估计。
 */
static double geoevol_error_estimate_rk4(lvGeomEvol *evol, const double *y_h, const double *y_half_h) {
    int dim = evol->dim;
    double error = 0.0;
    /* Richardson 因子：对于 p 阶方法，因子 = 1 / (2^p - 1)
     * RK4 的 p = 4，因此因子 = 1/15 */
    const double richardson_factor = 1.0 / 15.0;

    for (int i = 0; i < dim; ++i) {
        double diff = fabs(y_half_h[i] - y_h[i]) * richardson_factor * evol->error_weight[i];
        if (diff > error) {
            error = diff;
        }
    }
    return error;
}

/**
 * @brief 安全求 RHS 并递增统计计数
 */
static int geoevol_rhs_eval(lvGeomEvol *evol, double t, const double *y, double *dy) {
    if (!evol || !evol->rhs_func || !y || !dy) {
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "geoevol_rhs_eval: NULL parameter");
    }
    evol->stats.num_rhs_evals++;
    return evol->rhs_func(t, y, dy, evol);
}

/**
 * @brief 共享积分器回调适配器：将 (t, y, dydt, ctx) 映射到 geoevol_rhs_eval
 *
 * ctx 为 lvGeomEvol 指针；错误码与 RHS 求值计数由 geoevol_rhs_eval 维护。
 */
static int geoevol_rhs_adapter(double t, const double *y, double *dydt, void *ctx) {
    lvGeomEvol *evol = (lvGeomEvol *) ctx;
    return geoevol_rhs_eval(evol, t, y, dydt);
}

/* ========================================================================
 * 多步法历史缓冲区管理
 * ======================================================================== */

/**
 * @brief 清空多步法历史缓冲区
 *
 * 将历史计数归零、阶数归 1、写入索引归 0。
 */
static void geoevol_ms_history_reset(lvGeomEvol *evol) {
    evol->ms_hist_count = 0;
    evol->ms_order = 1;
    evol->ms_hist_idx = 0;
}

/**
 * @brief 将当前状态 (t, y, f) 存入多步法历史缓冲区
 *
 * 使用循环缓冲区：写入位置由 ms_hist_idx 决定，
 * 写入后索引递增并对 GEOEVOL_ADAMS_MAX_ORDER 取模。
 * 同时递增历史计数（不超过最大容量）。
 */
static void geoevol_ms_history_push(lvGeomEvol *evol, double t, const double *y, const double *f) {
    int idx = evol->ms_hist_idx;
    int dim = evol->dim;
    int max_ord = GEOEVOL_ADAMS_MAX_ORDER;

    evol->ms_hist_t[idx] = t;
    if (y) {
        memcpy(evol->ms_hist_y[idx], y, (size_t) dim * sizeof(double));
    }
    if (f) {
        memcpy(evol->ms_hist_f[idx], f, (size_t) dim * sizeof(double));
    }

    /* 推进写入索引（循环） */
    evol->ms_hist_idx = (idx + 1) % max_ord;

    /* 递增历史计数（不超过缓冲区容量） */
    if (evol->ms_hist_count < max_ord) {
        evol->ms_hist_count++;
    }

    /* 根据已有历史更新当前阶数 */
    evol->ms_order = evol->ms_hist_count;
}

/**
 * @brief 从循环缓冲区中按偏移量获取历史 y 指针
 *
 * offset=0 表示最近存入的一步（当前步），offset=1 表示上一步，以此类推。
 * 调用者需确保 offset < ms_hist_count。
 */
static const double *geoevol_ms_hist_y(const lvGeomEvol *evol, int offset) {
    int max_ord = GEOEVOL_ADAMS_MAX_ORDER;
    /* ms_hist_idx 指向下一个写入位置，所以最近一步在 (idx - 1 + max_ord) % max_ord */
    int idx = (evol->ms_hist_idx - 1 - offset + max_ord) % max_ord;
    return evol->ms_hist_y[idx];
}

/**
 * @brief 从循环缓冲区中按偏移量获取历史 f 指针
 *
 * offset=0 表示最近存入的 RHS 求值，offset=1 表示上一步，以此类推。
 */
static const double *geoevol_ms_hist_f(const lvGeomEvol *evol, int offset) {
    int max_ord = GEOEVOL_ADAMS_MAX_ORDER;
    int idx = (evol->ms_hist_idx - 1 - offset + max_ord) % max_ord;
    return evol->ms_hist_f[idx];
}

/**
 * @brief 获取历史时间戳
 *
 * offset=0 表示最近一步的时间，offset=1 表示上一步的时间。
 */
static double geoevol_ms_hist_t(const lvGeomEvol *evol, int offset) {
    int max_ord = GEOEVOL_ADAMS_MAX_ORDER;
    int idx = (evol->ms_hist_idx - 1 - offset + max_ord) % max_ord;
    return evol->ms_hist_t[idx];
}

/* ========================================================================
 * 演化方法：Euler 法（一阶）
 * ======================================================================== */

/**
 * @brief 显式 Euler 法单步积分
 *
 * y_{n+1} = y_n + h * f(t_n, y_n)
 */
static int geoevol_step_euler(lvGeomEvol *evol, double h, const double *y, double *y_out) {
    /* 复用共享积分器 lv_ode_euler_step（与 ode_solver.c 共用同一实现） */
    return lv_ode_euler_step(evol->t, y, (size_t) evol->dim, h, y_out, geoevol_rhs_adapter, evol);
}

/* ========================================================================
 * 演化方法：经典 RK4 法（四阶）
 * ======================================================================== */

/**
 * @brief 经典四阶 Runge-Kutta 单步积分
 *
 * k1 = f(t_n, y_n)
 * k2 = f(t_n + h/2, y_n + h/2 * k1)
 * k3 = f(t_n + h/2, y_n + h/2 * k2)
 * k4 = f(t_n + h, y_n + h * k3)
 * y_{n+1} = y_n + h/6 * (k1 + 2*k2 + 2*k3 + k4)
 */
static int geoevol_step_rk4(lvGeomEvol *evol, double h, const double *y, double *y_out) {
    /* 复用共享积分器 lv_ode_rk4_step（与 ode_solver.c 共用同一实现，
     * k1-k4 计算顺序逐位一致；evol->t 由调用方在必要时临时调整） */
    return lv_ode_rk4_step(evol->t, y, (size_t) evol->dim, h, y_out, geoevol_rhs_adapter, evol);
}

/* ========================================================================
 * 演化方法：Adams-Bashforth-Moulton 预测-校正法
 * ======================================================================== */

/**
 * @brief Adams-Bashforth-Moulton 预测-校正法单步积分
 *
 * 实现变阶（1~5阶）Adams-Bashforth-Moulton 预测-校正法：
 *
 * 1. 启动阶段：历史不足时使用 RK4 生成初始历史点
 * 2. Adams-Bashforth（显式预测）：
 *    y_predict = y_n + h * sum(beta_j * f_{n-j})
 * 3. Adams-Moulton（隐式校正）：
 *    y_correct = y_n + h * sum(alpha_j * f_{n+1-j})
 *    其中 f_{n+1} 使用预测的 y_predict 计算
 *
 * Adams-Bashforth 系数（阶 1~5）：
 *   AB1: [1]
 *   AB2: [3/2, -1/2]
 *   AB3: [23/12, -16/12, 5/12]
 *   AB4: [55/24, -59/24, 37/24, -9/24]
 *   AB5: [1901/720, -2774/720, 2616/720, -1274/720, 251/720]
 *
 * Adams-Moulton 系数（阶 1~5）：
 *   AM1: [1]
 *   AM2: [1/2, 1/2]
 *   AM3: [5/12, 8/12, -1/12]
 *   AM4: [9/24, 19/24, -5/24, 1/24]
 *   AM5: [251/720, 646/720, -264/720, 106/720, -19/720]
 */
static int geoevol_step_adams(lvGeomEvol *evol, double h, const double *y, double *y_out) {
    int dim = evol->dim;
    int max_ord = GEOEVOL_ADAMS_MAX_ORDER;

    /* Adams-Bashforth 系数表（阶 1~5）。
     * 阶 4 行（AB4）与共享系数表 lv_ode_ab4_coeffs 完全一致，
     * 使用处统一复用 lv_ode_ab4_coeffs，避免重复定义。 */
    static const double ab_coeffs[5][5] = {
        [0] = {1.0, 0.0, 0.0, 0.0, 0.0},                                                        /* AB1 */
        [1] = {1.5, -0.5, 0.0, 0.0, 0.0},                                                       /* AB2 */
        [2] = {23.0 / 12.0, -16.0 / 12.0, 5.0 / 12.0, 0.0, 0.0},                                /* AB3 */
        /* [3] 占位（AB4）：复用共享 lv_ode_ab4_coeffs */
        [4] = {1901.0 / 720.0, -2774.0 / 720.0, 2616.0 / 720.0, -1274.0 / 720.0, 251.0 / 720.0} /* AB5 */
    };

    /* Adams-Moulton 系数表（阶 1~5） */
    static const double am_coeffs[5][5] = {
        {1.0, 0.0, 0.0, 0.0, 0.0},                                                   /* AM1 */
        {0.5, 0.5, 0.0, 0.0, 0.0},                                                   /* AM2 */
        {5.0 / 12.0, 8.0 / 12.0, -1.0 / 12.0, 0.0, 0.0},                             /* AM3 */
        {9.0 / 24.0, 19.0 / 24.0, -5.0 / 24.0, 1.0 / 24.0, 0.0},                     /* AM4 */
        {251.0 / 720.0, 646.0 / 720.0, -264.0 / 720.0, 106.0 / 720.0, -19.0 / 720.0} /* AM5 */
    };

    /* ── 启动阶段：历史不足时使用 RK4 生成初始历史点 ── */
    if (evol->ms_hist_count < max_ord - 1) {
        lv_LOG_DEBUG("Adams启动阶段：历史=%d/%d，使用RK4", evol->ms_hist_count, max_ord - 1);

        /* 在执行 RK4 之前，先求当前点的 RHS 并存入历史 */
        int ret = geoevol_rhs_eval(evol, evol->t, y, evol->dparam);
        if (ret != 0) {
            return ret;
        }
        geoevol_ms_history_push(evol, evol->t, y, evol->dparam);

        /* 执行 RK4 步 */
        ret = geoevol_step_rk4(evol, h, y, y_out);
        if (ret != 0) {
            return ret;
        }

        /* RK4 完成后，将结果也存入历史 */
        ret = geoevol_rhs_eval(evol, evol->t + h, y_out, evol->dparam);
        if (ret != 0) {
            return ret;
        }
        geoevol_ms_history_push(evol, evol->t + h, y_out, evol->dparam);

        return 0;
    }

    /* ── 正式 Adams-Bashforth-Moulton 步 ── */
    int order = evol->ms_order; /* 当前阶数 = 已有历史步数 */

    /* 分配临时空间：预测值和校正值的 RHS */
    double *f_predict = lv_malloc((size_t) dim * sizeof(double));
    double *y_predict = lv_malloc((size_t) dim * sizeof(double));
    if (!f_predict || !y_predict) {
        if (f_predict)
            lv_free((void **) &f_predict);
        if (y_predict)
            lv_free((void **) &y_predict);
        lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "Adams temporary space allocation failed");
    }

    /* ── 第一步：Adams-Bashforth 显式预测 ── */
    /* y_predict = y_n + h * sum_{j=0}^{order-1} beta_j * f_{n-j} */
    /* 注意：y_n 就是 y（当前传入的参数），f_{n-j} 是历史中偏移 j 的 RHS */
    /* 阶 4（AB4）使用共享系数表，其余阶使用本地表 */
    const double *ab = (order == 4) ? lv_ode_ab4_coeffs : ab_coeffs[order - 1];
    for (int i = 0; i < dim; ++i) {
        double sum = 0.0;
        for (int j = 0; j < order; ++j) {
            const double *fj = geoevol_ms_hist_f(evol, j);
            sum += ab[j] * fj[i];
        }
        y_predict[i] = y[i] + h * sum;
    }

    /* 计算 f(t_{n+1}, y_predict) */
    int ret = geoevol_rhs_eval(evol, evol->t + h, y_predict, f_predict);
    if (ret != 0) {
        lv_free((void **) &f_predict);
        lv_free((void **) &y_predict);
        return ret;
    }

    /* ── 第二步：Adams-Moulton 隐式校正 ── */
    /* y_correct = y_n + h * (alpha_0 * f_{n+1} + sum_{j=1}^{order-1} alpha_j * f_{n+1-j}) */
    const double *am = am_coeffs[order - 1];
    for (int i = 0; i < dim; ++i) {
        double sum = am[0] * f_predict[i]; /* f_{n+1} 使用预测值 */
        for (int j = 1; j < order; ++j) {
            const double *fj = geoevol_ms_hist_f(evol, j - 1); /* f_{n+1-j} = f_{n-(j-1)} */
            sum += am[j] * fj[i];
        }
        y_out[i] = y[i] + h * sum;
    }

    /* ── 更新历史缓冲区 ── */
    /* 先将当前步 (y, f) 存入历史，再将校正后的结果也存入 */
    ret = geoevol_rhs_eval(evol, evol->t, y, evol->dparam);
    if (ret != 0) {
        lv_free((void **) &f_predict);
        lv_free((void **) &y_predict);
        return ret;
    }
    geoevol_ms_history_push(evol, evol->t, y, evol->dparam);

    /* 将校正后的 y_out 的 RHS 存入历史 */
    ret = geoevol_rhs_eval(evol, evol->t + h, y_out, evol->dparam);
    if (ret != 0) {
        lv_free((void **) &f_predict);
        lv_free((void **) &y_predict);
        return ret;
    }
    geoevol_ms_history_push(evol, evol->t + h, y_out, evol->dparam);

    lv_free((void **) &f_predict);
    lv_free((void **) &y_predict);
    return 0;
}

/* ========================================================================
 * 演化方法：BDF（后向差分公式）
 * ======================================================================== */

/**
 * @brief BDF (Backward Differentiation Formula) 单步积分
 *
 * 实现变阶（1~5阶）BDF 方法，适用于刚性 ODE：
 *
 * BDF 公式（隐式多步法）：
 *   BDF1: y_{n+1} - y_n = h * f(y_{n+1})
 *   BDF2: 3/2*y_{n+1} - 2*y_n + 1/2*y_{n-1} = h * f(y_{n+1})
 *   BDF3: 11/6*y_{n+1} - 3*y_n + 3/2*y_{n-1} - 1/3*y_{n-2} = h*f(y_{n+1})
 *   BDF4: 25/12*y_{n+1} - 4*y_n + 3*y_{n-1} - 4/3*y_{n-2} + 1/4*y_{n-3} = h*f(y_{n+1})
 *   BDF5: 137/60*y_{n+1} - 5*y_n + 5*y_{n-1} - 10/3*y_{n-2} + 5/4*y_{n-3} - 1/5*y_{n-4} = h*f(y_{n+1})
 *
 * 统一形式：gamma_0 * y_{n+1} + sum_{j=1}^{order} gamma_j * y_{n+1-j} = h * f(y_{n+1})
 * 其中 gamma_0 为 BDF 系数的主导项（y_{n+1} 的系数）。
 *
 * 使用 Newton 迭代求解隐式方程：
 *   G(y) = gamma_0 * y + sum(gamma_j * y_{n+1-j}) - h * f(t_{n+1}, y) = 0
 *   Newton: y_{k+1} = y_k - J^{-1} * G(y_k)
 *   使用有限差分 Jacobian 近似。
 *
 * Newton 迭代参数：最大 10 次，收敛容限 1e-10。
 * 若 Newton 不收敛，缩减步长并重试。
 */
static int geoevol_step_bdf(lvGeomEvol *evol, double h, const double *y, double *y_out) {
    int dim = evol->dim;
    int max_ord = GEOEVOL_ADAMS_MAX_ORDER;

    /* BDF 系数表（阶 1~5）
     * gamma[0] = y_{n+1} 的系数（主导项）
     * gamma[j] = y_{n+1-j} 的系数，j = 1..order */
    static const double bdf_gamma[5][6] = {
        {1.0, -1.0, 0.0, 0.0, 0.0, 0.0},                   /* BDF1 */
        {1.5, -2.0, 0.5, 0.0, 0.0, 0.0},                   /* BDF2 */
        {11.0 / 6.0, -3.0, 1.5, -1.0 / 3.0, 0.0, 0.0},     /* BDF3 */
        {25.0 / 12.0, -4.0, 3.0, -4.0 / 3.0, 0.25, 0.0},   /* BDF4 */
        {137.0 / 60.0, -5.0, 5.0, -10.0 / 3.0, 1.25, -0.2} /* BDF5 */
    };

    /* Newton 迭代参数 */
    const int newton_max_iter = 10;
    const double newton_tol = lv_EPSILON_HIGH;
    /* 有限差分 Jacobian 的扰动步长 */
    const double fd_eps = 1e-8;

    /* ── 启动阶段：历史不足时使用 RK4 生成初始历史点 ── */
    if (evol->ms_hist_count < max_ord - 1) {
        lv_LOG_DEBUG("BDF启动阶段：历史=%d/%d，使用RK4", evol->ms_hist_count, max_ord - 1);

        /* 先将当前点存入历史 */
        geoevol_ms_history_push(evol, evol->t, y, NULL);

        /* 执行 RK4 步 */
        int ret = geoevol_step_rk4(evol, h, y, y_out);
        if (ret != 0) {
            return ret;
        }

        /* 将结果存入历史 */
        geoevol_ms_history_push(evol, evol->t + h, y_out, NULL);

        return 0;
    }

    /* ── 正式 BDF 步 ── */
    int order = evol->ms_order;
    const double *gamma = bdf_gamma[order - 1];
    double gamma0 = gamma[0];

    /* 分配临时空间：NULL 初始化 + lv_DEFER 作用域守卫统一清理。
     * 任一分配失败 / 中途 goto cleanup_bdf / 正常返回，均在函数出口逆序自动释放，
     * 消除"先手动 free 再 goto 统一清理"的双重释放形态。 */
    double *y_new = NULL;   /* Newton 迭代当前值 */
    double *G = NULL;       /* 残差函数值 */
    double *delta = NULL;   /* Newton 修正量 */
    double *f_new = NULL;   /* f(t_{n+1}, y_new) */
    double *f_pert = NULL;  /* 有限差分扰动 RHS */
    double *rhs_sum = NULL; /* 历史项累加 */
    double *J = NULL;       /* 稠密 Jacobian 矩阵（行优先存储） */
    int *piv = NULL;        /* LU 分解主元数组 */
    double *J_diag = NULL;  /* LU 分解前对角副本 */
    lv_DEFER(lv_defer_free_ptr, &y_new);
    lv_DEFER(lv_defer_free_ptr, &G);
    lv_DEFER(lv_defer_free_ptr, &delta);
    lv_DEFER(lv_defer_free_ptr, &f_new);
    lv_DEFER(lv_defer_free_ptr, &f_pert);
    lv_DEFER(lv_defer_free_ptr, &rhs_sum);
    lv_DEFER(lv_defer_free_ptr, &J);
    lv_DEFER(lv_defer_free_ptr, &piv);
    lv_DEFER(lv_defer_free_ptr, &J_diag);

    y_new = lv_malloc((size_t) dim * sizeof(double));
    G = lv_malloc((size_t) dim * sizeof(double));
    delta = lv_malloc((size_t) dim * sizeof(double));
    f_new = lv_malloc((size_t) dim * sizeof(double));
    f_pert = lv_malloc((size_t) dim * sizeof(double));
    rhs_sum = lv_malloc((size_t) dim * sizeof(double));
    if (!y_new || !G || !delta || !f_new || !f_pert || !rhs_sum)
        lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "BDF temporary space allocation failed");

    /* 计算历史项累加：sum_{j=1}^{order} gamma_j * y_{n+1-j} */
    for (int i = 0; i < dim; ++i) {
        double s = 0.0;
        for (int j = 1; j <= order; ++j) {
            const double *yj = geoevol_ms_hist_y(evol, j - 1); /* y_{n+1-j} = y_{n-(j-1)} */
            s += gamma[j] * yj[i];
        }
        rhs_sum[i] = s;
    }

    /* 初始猜测：使用 Adams-Bashforth 预测（显式 Euler 或更高阶外推） */
    /* 简单起见，使用当前 y 作为初始猜测 */
    memcpy(y_new, y, (size_t) dim * sizeof(double));

    /* ── Newton 迭代 ── */
    int newton_converged = 0;
    for (int iter = 0; iter < newton_max_iter; ++iter) {
        /* 计算 f(t_{n+1}, y_new) */
        int ret = geoevol_rhs_eval(evol, evol->t + h, y_new, f_new);
        if (ret != 0) {
            goto cleanup_bdf;
        }

        /* 计算残差 G(y) = gamma_0 * y + rhs_sum - h * f(t_{n+1}, y) */
        double g_norm = 0.0;
        for (int i = 0; i < dim; ++i) {
            G[i] = gamma0 * y_new[i] + rhs_sum[i] - h * f_new[i];
            double abs_g = fabs(G[i]);
            if (abs_g > g_norm) {
                g_norm = abs_g;
            }
        }

        /* 检查收敛 */
        if (g_norm < newton_tol) {
            newton_converged = 1;
            break;
        }

        /* 计算有限差分 Jacobian 并求解线性系统 J * delta = -G
         *
         * J_{ij} = dG_i/dy_j = gamma_0 * delta_{ij} - h * df_i/dy_j
         *
         * 使用稠密有限差分 Jacobian + 部分选主元 LU 分解（Doolittle 方法）。
         * 对于几何约束系统，Jacobian 通常具有带状结构（每个约束仅涉及
         * 少数变量），但此处采用通用稠密 LU 分解以保证正确性。
         * 当 dim 较大时，可替换为带状 LU 或稀疏直接求解器以提升性能。 */
        {
            /* 分配稠密 Jacobian 矩阵（行优先存储） */
            J = lv_malloc((size_t) dim * dim * sizeof(double));
            if (!J) {
                goto cleanup_bdf;
            }

            /* 逐列构建 Jacobian：对每个变量 y_j 做有限差分扰动 */
            for (int j = 0; j < dim; ++j) {
                double y_save_j = y_new[j];
                double pert = fd_eps * (fabs(y_save_j) + 1.0);
                y_new[j] = y_save_j + pert;

                ret = geoevol_rhs_eval(evol, evol->t + h, y_new, f_pert);
                if (ret != 0) {
                    y_new[j] = y_save_j;
                    /* J 由 lv_DEFER 守卫在函数出口统一释放，此处不再手动 free */
                    goto cleanup_bdf;
                }
                y_new[j] = y_save_j;

                /* J_{ij} = gamma_0 * delta_{ij} - h * (f_pert[i] - f_new[i]) / pert */
                for (int i = 0; i < dim; ++i) {
                    double dfdy = (f_pert[i] - f_new[i]) / pert;
                    J[i * dim + j] = (i == j) ? (gamma0 - h * dfdy) : (-h * dfdy);
                }
            }

            /* 设置右端向量 delta = -G */
            for (int i = 0; i < dim; ++i) {
                delta[i] = -G[i];
            }

            /* 部分选主元 LU 分解（原地分解，结果存回 J） */
            /* 使用紧凑存储：J 的上三角存 U，下三角存 L（对角线为 1），
             * piv 记录行交换信息 */
            piv = lv_malloc((size_t) dim * sizeof(int));
            if (!piv) {
                goto cleanup_bdf;
            }
            for (int i = 0; i < dim; ++i)
                piv[i] = i;

            /* 在 LU 分解前保存原始对角线副本，用于分解失败时的对角回退 */
            J_diag = lv_malloc((size_t) dim * sizeof(double));
            if (!J_diag) {
                goto cleanup_bdf;
            }
            for (int i = 0; i < dim; ++i)
                J_diag[i] = J[i * dim + i];

            /* LU 分解：Doolittle 方法 + 部分选主元 */
            int lu_ok = 1;
            for (int k = 0; k < dim; ++k) {
                /* 选主元：找第 k 列中从第 k 行开始的最大元素 */
                int max_row = k;
                double max_val = fabs(J[k * dim + k]);
                for (int i = k + 1; i < dim; ++i) {
                    double val = fabs(J[i * dim + k]);
                    if (val > max_val) {
                        max_val = val;
                        max_row = i;
                    }
                }

                if (max_val < GEOEVOL_LU_SINGULAR_EPS) {
                    /* 矩阵奇异或接近奇异 */
                    lu_ok = 0;
                    break;
                }

                /* 交换行 */
                if (max_row != k) {
                    int tmp_piv = piv[k];
                    piv[k] = piv[max_row];
                    piv[max_row] = tmp_piv;
                    for (int j = 0; j < dim; ++j) {
                        double tmp = J[k * dim + j];
                        J[k * dim + j] = J[max_row * dim + j];
                        J[max_row * dim + j] = tmp;
                    }
                }

                /* 消元 */
                for (int i = k + 1; i < dim; ++i) {
                    J[i * dim + k] /= J[k * dim + k];
                    for (int j = k + 1; j < dim; ++j) {
                        J[i * dim + j] -= J[i * dim + k] * J[k * dim + j];
                    }
                }
            }

            if (lu_ok) {
                /* 前向替换（Ly = Pb） */
                for (int i = 0; i < dim; ++i) {
                    double sum = delta[piv[i]];
                    for (int j = 0; j < i; ++j) {
                        sum -= J[i * dim + j] * delta[piv[j]];
                    }
                    delta[piv[i]] = sum;
                }

                /* 回代（Ux = y） */
                for (int i = dim - 1; i >= 0; --i) {
                    double sum = delta[piv[i]];
                    for (int j = i + 1; j < dim; ++j) {
                        sum -= J[i * dim + j] * delta[piv[j]];
                    }
                    delta[piv[i]] = sum / J[i * dim + i];
                }

                /* 将结果从 piv 顺序还原到自然顺序 */
                double *delta_sorted = lv_malloc((size_t) dim * sizeof(double));
                if (delta_sorted) {
                    for (int i = 0; i < dim; ++i) {
                        delta_sorted[i] = delta[piv[i]];
                    }
                    for (int i = 0; i < dim; ++i) {
                        delta[i] = delta_sorted[i];
                    }
                    lv_free((void **) &delta_sorted);
                }
            } else {
                /* LU 分解失败（矩阵奇异），回退到对角近似 */
                /* 使用 LU 分解前保存的原始对角线副本，而非部分修改后的 J */
                for (int i = 0; i < dim; ++i) {
                    double J_ii = J_diag[i];
                    if (fabs(J_ii) < GEOEVOL_LU_SINGULAR_EPS) {
                        J_ii = (J_ii >= 0.0) ? GEOEVOL_LU_SINGULAR_EPS : -GEOEVOL_LU_SINGULAR_EPS;
                    }
                    delta[i] = -G[i] / J_ii;
                }
            }

            lv_free((void **) &J_diag);
            lv_free((void **) &piv);
            lv_free((void **) &J);
        }

        /* 应用修正 */
        for (int i = 0; i < dim; ++i) {
            y_new[i] += delta[i];
        }
    }

    if (!newton_converged) {
        evol->stats.num_convergence_fails++;
        lv_LOG_DEBUG("BDF Newton迭代未收敛（%d步，order=%d）", newton_max_iter, order);
        /* 将初始猜测作为输出，让外层步长控制器缩减步长重试 */
        memcpy(y_out, y, (size_t) dim * sizeof(double));
        /* 返回 -1 表示 Newton 未收敛，调用者可据此缩减步长重试 */
        goto cleanup_bdf;
    }

    /* 输出 Newton 收敛结果 */
    memcpy(y_out, y_new, (size_t) dim * sizeof(double));

    /* ── 更新历史缓冲区 ── */
    geoevol_ms_history_push(evol, evol->t, y, NULL);
    geoevol_ms_history_push(evol, evol->t + h, y_out, NULL);

cleanup_bdf:
    /* 所有临时数组已由 lv_DEFER 守卫在函数出口逆序自动释放（NULL 安全），
     * 此处仅保留统一的错误语义出口，行为与原实现完全一致 */
    if (!newton_converged)
        lv_RETURN_ERROR(lv_ERROR_SOLVER_NOT_CONVERGED, "BDF Newton iteration did not converge");
    return 0;
}

/* ================================================================
 * 演化方法 -> {步进函数, 阶数, 误差估计方式} 静态查找表（数据表化，替代 switch）
 * ================================================================ */

/** @brief 误差估计方式 */
typedef enum {
    GEOEVOL_ERR_EST_RICHARDSON, /**< Richardson 外推（RK4：半步长两步 vs 全步长） */
    GEOEVOL_ERR_EST_NONE,       /**< 无嵌入式估计（Euler：直接接受） */
    GEOEVOL_ERR_EST_EULER_REF,  /**< Euler 参考步估计（Adams/BDF：低阶参考） */
} GeoEvolErrEstKind;

/** @brief 演化方法表条目 */
typedef struct {
    int (*stepper)(lvGeomEvol *evol, double h, const double *y, double *y_out); /**< 单步积分函数 */
    int order;                 /**< 方法阶数；-1 表示变阶（取 evol->ms_order，1~5） */
    GeoEvolErrEstKind err_est; /**< 误差估计方式 */
} GeoEvolMethodEntry;

/** @brief 演化方法 -> 步进函数/阶数/误差估计 查找表（lvEvolMethod 枚举 0~3 连续） */
static const GeoEvolMethodEntry s_evol_method_table[] = {
    [lv_EVOL_EULER] = {geoevol_step_euler, 1, GEOEVOL_ERR_EST_NONE},
    [lv_EVOL_RK4] = {geoevol_step_rk4, 4, GEOEVOL_ERR_EST_RICHARDSON},
    [lv_EVOL_ADAMS] = {geoevol_step_adams, -1, GEOEVOL_ERR_EST_EULER_REF},
    [lv_EVOL_BDF] = {geoevol_step_bdf, -1, GEOEVOL_ERR_EST_EULER_REF},
};

/* ========================================================================
 * 核心单步演化
 * ======================================================================== */

/**
 * @brief 执行单步自适应演化 —— 借鉴 CVODE CVode() 逻辑
 *
 * 1. 计算 RHS 以求当前速度场
 * 2. 使用选定的积分方法计算试探步
 * 3. 使用低阶嵌入方法估计局部截断误差
 * 4. 进行误差测试：error <= 1.0 则接受步，否则缩减步长并重试
 * 5. 使用 PI 控制器预测下一步步长
 */
lvEvolStatus geoevol_step_once(lvGeomEvol *evol) {
    lv_CHECK_NULL(evol, lv_EVOL_STATUS_ERROR);

    if (evol->status != lv_EVOL_STATUS_RUNNING) {
        lv_ERROR_SET(lv_BACKEND_INVALID_ARGS, "演化引擎未处于运行状态，当前状态=%d", (int) evol->status);
        return lv_EVOL_STATUS_ERROR;
    }

    lv_CHECK_NULL(evol->rhs_func, lv_EVOL_STATUS_ERROR);

    int dim = evol->dim;
    if (dim <= 0 || dim > GEOEVOL_MAX_PARAM_DIM) {
        lv_ERROR_SET(lv_BACKEND_INVALID_ARGS, "无效的向量维度=%d", dim);
        return lv_EVOL_STATUS_ERROR;
    }

    geoevol_calc_error_weights(evol);

    /* 需要临时数组：y_trial（试探步结果）、y_half（半步两步结果）、y_save（当前状态备份） */
    double *y_trial = lv_malloc((size_t) dim * sizeof(double));
    double *y_half = lv_malloc((size_t) dim * sizeof(double));
    double *y_save = lv_malloc((size_t) dim * sizeof(double));
    if (!y_trial || !y_half || !y_save) {
        if (y_trial)
            lv_free((void **) &y_trial);
        if (y_half)
            lv_free((void **) &y_half);
        if (y_save)
            lv_free((void **) &y_save);
        lv_ERROR_SET(lv_ERROR_OUT_OF_MEMORY, "单步演化临时空间分配失败");
        return lv_EVOL_STATUS_ERROR;
    }

    /* 保存当前状态 */
    memcpy(y_save, evol->param, (size_t) dim * sizeof(double));
    double t_save = evol->t;

    double h = evol->step_size;
    lvEvolMethod method = evol->method;
    int method_order;

    /* 循环：如果误差过大则缩减步长重试 */
    int reject_count = 0;

    for (;;) {
        if (reject_count >= GEOEVOL_MAX_REJECTIONS) {
            lv_ERROR_SET(lv_ERROR_SOLVER_NOT_CONVERGED, "步长控制超过最大被拒次数 %d", GEOEVOL_MAX_REJECTIONS);
            evol->status = lv_EVOL_STATUS_ERROR;
            goto cleanup;
        }

        /* 选择积分方法和阶数（查表替代 switch） */
        int (*stepper)(lvGeomEvol *, double, const double *, double *) = NULL;
        const GeoEvolMethodEntry *me = NULL;
        if ((unsigned) method < lv_ARRAY_SIZE(s_evol_method_table) && s_evol_method_table[method].stepper) {
            me = &s_evol_method_table[method];
            stepper = me->stepper;
            method_order = (me->order >= 0) ? me->order : evol->ms_order; /* 变阶：1~5 */
        } else {
            lv_ERROR_SET(lv_BACKEND_UNSUPPORTED, "不支持的演化方法=%d", (int) method);
            evol->status = lv_EVOL_STATUS_ERROR;
            goto cleanup;
        }

        /* 执行试探步（全步长 h） */
        int ret = stepper(evol, h, y_save, y_trial);
        if (ret != 0) {
            evol->status = lv_EVOL_STATUS_ERROR;
            goto cleanup;
        }

        /* 误差估计（按方法查表选择方式）：
         * - Richardson 外推（RK4）：用步长 h/2 执行两步 RK4，与全步长结果比较。
         *   误差 = (y_{h/2} - y_h) / (2^p - 1)，p = 方法阶数 */
        if (me->err_est == GEOEVOL_ERR_EST_RICHARDSON) {
            /* 半步长两步法：先走 h/2，再走 h/2 */
            double half_h = 0.5 * h;
            double *y_mid = lv_malloc((size_t) dim * sizeof(double));
            if (!y_mid) {
                evol->status = lv_EVOL_STATUS_ERROR;
                lv_ERROR_SET(lv_ERROR_OUT_OF_MEMORY, "Richardson外推临时空间分配失败");
                goto cleanup;
            }

            /* 第一步：从 y_save 出发，步长 half_h */
            ret = geoevol_step_rk4(evol, half_h, y_save, y_mid);
            if (ret != 0) {
                lv_free((void **) &y_mid);
                evol->status = lv_EVOL_STATUS_ERROR;
                goto cleanup;
            }

            /* 恢复 evol->t 到中间状态，第二步：从 y_mid 出发，步长 half_h */
            double t_saved = evol->t;
            evol->t = t_saved + half_h;
            ret = geoevol_step_rk4(evol, half_h, y_mid, y_half);
            evol->t = t_saved; /* 恢复原时间 */
            lv_free((void **) &y_mid);
            if (ret != 0) {
                evol->status = lv_EVOL_STATUS_ERROR;
                goto cleanup;
            }
        } else if (me->err_est == GEOEVOL_ERR_EST_NONE) {
            /* Euler 没有嵌入式估计，直接接受 */
            memcpy(y_half, y_trial, (size_t) dim * sizeof(double));
        } else {
            /* Adams/BDF：使用 Euler 步作为低阶参考来估计局部截断误差。
             *
             * 注意：这是一个保守估计。Euler 方法是一阶方法，而 Adams/BDF 是高阶方法，
             * 因此差值 |y_trial - y_euler| 会高估真实的局部截断误差。这意味着：
             *   - 步可能会被不必要地拒绝（保守但安全）
             *   - 但绝不会接受不正确的步（保证正确性）
             *
             * FUTURE: 改用 Adams/BDF 的预测-校正残差或嵌入式低阶公式
             *       来获得更精确的误差估计，减少不必要的步拒绝。
             */
            ret = geoevol_step_euler(evol, h, y_save, y_half);
            if (ret != 0) {
                evol->status = lv_EVOL_STATUS_ERROR;
                goto cleanup;
            }
        }

        /* 计算误差 */
        double error = geoevol_error_estimate_rk4(evol, y_trial, y_half);
        evol->stats.last_error_est = error;

        /* 误差测试 */
        if (error <= GEOEVOL_ERROR_THRESHOLD) {
            /* 接受步 */
            break;
        }

        /* 拒绝步：缩减步长 */
        reject_count++;
        evol->stats.num_error_fails++;

        double h_factor = 0.5 * pow(GEOEVOL_ERROR_THRESHOLD / error, 1.0 / (double) (method_order + 1));
        if (h_factor < 0.1) {
            h_factor = 0.1;
        }
        h *= h_factor;
        if (h < evol->step_size_min) {
            h = evol->step_size_min;
        }
        /* 步长小于最小值也继续尝试 */
    }

    /* 接受该步 */
    memcpy(evol->param, y_trial, (size_t) dim * sizeof(double));
    evol->t = t_save + h;
    evol->stats.num_steps++;
    evol->stats.total_time = evol->t;
    evol->stats.last_step_size = h;
    evol->step_size = h;

    /* 计算最终 RHS 以获取当前速度场 */
    geoevol_rhs_eval(evol, evol->t, evol->param, evol->dparam);

    /* PI 控制器预测下一步长 */
    h = geoevol_pi_predict(&evol->pi, evol->stats.last_error_est, method_order, h);
    if (h > evol->step_size_max) {
        h = evol->step_size_max;
    }
    if (h < evol->step_size_min) {
        h = evol->step_size_min;
    }
    evol->step_size = h;

    /* 步后处理回调 */
    if (evol->post_step) {
        evol->post_step(evol, evol->t, evol->param);
    }

cleanup:
    if (y_trial)
        lv_free((void **) &y_trial);
    if (y_half)
        lv_free((void **) &y_half);
    if (y_save)
        lv_free((void **) &y_save);
    return evol->status;
}

/* ========================================================================
 * 主演化循环
 * ======================================================================== */

/**
 * @brief 将演化引擎运行至目标时间 tout
 *
 * 从当前时间 evol->t 出发，持续调用 geoevol_step_once() 直到 t >= tout。
 * 借鉴 CVODE 的 CVode() 主循环设计。
 */
lvEvolStatus geoevol_evolve(lvGeomEvol *evol, double tout) {
    lv_CHECK_NULL(evol, lv_EVOL_STATUS_ERROR);

    if (evol->status != lv_EVOL_STATUS_RUNNING && evol->status != lv_EVOL_STATUS_IDLE) {
        return evol->status;
    }

    /* 首次调用从 IDLE 切换到 RUNNING */
    if (evol->status == lv_EVOL_STATUS_IDLE) {
        evol->status = lv_EVOL_STATUS_RUNNING;
        /* 初始化 RHS */
        int ret = geoevol_rhs_eval(evol, evol->t, evol->param, evol->dparam);
        if (ret != 0) {
            evol->status = lv_EVOL_STATUS_ERROR;
            lv_ERROR_SET(lv_ERROR_SOLVER_NUMERIC, "初始RHS求值失败");
            return lv_EVOL_STATUS_ERROR;
        }
    }

    /* 检查 tout 是否有效 */
    if (tout <= evol->t) {
        lv_ERROR_SET(lv_BACKEND_INVALID_ARGS, "目标时间 tout=%g 必须大于当前时间 t=%g", tout, evol->t);
        return lv_EVOL_STATUS_ERROR;
    }

    /* 如果当前步长会越过 tout，缩减步长 */
    if (evol->t + evol->step_size > tout) {
        evol->step_size = tout - evol->t;
    }

    /* 主演化循环 */
    while (evol->t < tout) {
        /* 如果下一步会超过 tout，限制步长 */
        if (evol->t + evol->step_size > tout) {
            evol->step_size = tout - evol->t;
        }

        /* 若步长过小，停止 */
        if (evol->step_size < evol->step_size_min) {
            lv_ERROR_SET(lv_ERROR_SOLVER_NOT_CONVERGED, "步长过小: h=%g < h_min=%g", evol->step_size,
                         evol->step_size_min);
            evol->status = lv_EVOL_STATUS_ERROR;
            return lv_EVOL_STATUS_ERROR;
        }

        lvEvolStatus stat = geoevol_step_once(evol);
        if (stat != lv_EVOL_STATUS_RUNNING) {
            return stat;
        }
    }

    evol->status = lv_EVOL_STATUS_STOPPED;
    return lv_EVOL_STATUS_STOPPED;
}

/* ========================================================================
 * 生命周期管理
 * ======================================================================== */

/**
 * @brief 创建几何演化引擎
 */
lvGeomEvol *geoevol_create(int dim, lvEvolMethod method, lvGeomEvolRHSFunc rhs) {
    lv_CHECK_NULL(rhs, NULL);

    if (dim <= 0 || dim > GEOEVOL_MAX_PARAM_DIM) {
        lv_ERROR_SET(lv_BACKEND_INVALID_ARGS, "参数维度必须满足 1 <= dim <= %d，当前 dim=%d", GEOEVOL_MAX_PARAM_DIM,
                     dim);
        return NULL;
    }

    lvGeomEvol *evol = lv_malloc(sizeof(lvGeomEvol));
    lv_CHECK_ALLOC(evol, NULL);

    memset(evol, 0, sizeof(lvGeomEvol));

    evol->dim = dim;
    evol->method = method;
    evol->status = lv_EVOL_STATUS_IDLE;
    evol->t = 0.0;
    evol->step_size = GEOEVOL_DEFAULT_STEP;
    evol->step_size_min = GEOEVOL_MIN_STEP;
    evol->step_size_max = GEOEVOL_MAX_STEP;
    evol->rel_tol = GEOEVOL_DEFAULT_REL_TOL;
    evol->abs_tol = GEOEVOL_DEFAULT_ABS_TOL;

    evol->rhs_func = rhs;
    evol->post_step = NULL;
    evol->root_func = NULL;
    evol->root_ng = 0;
    evol->user_data = NULL;

    /* 初始化 PI 控制器 */
    geoevol_pi_init(&evol->pi);

    /* 初始化统计信息 */
    memset(&evol->stats, 0, sizeof(evol->stats));

    /* 初始化多步法历史缓冲区 */
    geoevol_ms_history_reset(evol);

    return evol;
}

/**
 * @brief 销毁演化引擎并释放所有关联资源
 */
void geoevol_destroy(lvGeomEvol *evol) {
    if (!evol) {
        return;
    }
    lv_free((void **) &evol);
}

/**
 * @brief 设置演化步长参数
 */
void geoevol_set_step(lvGeomEvol *evol, double step_size, double step_min, double step_max) {
    if (!evol) {
        return;
    }
    evol->step_size = step_size;
    evol->step_size_min = step_min;
    evol->step_size_max = step_max;
}
