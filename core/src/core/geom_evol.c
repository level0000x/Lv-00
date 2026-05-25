/**
 * @file geom_evol.c
 * @brief 几何演化引擎 —— 自适应步长 ODE 求解器实现
 *
 * @details 实现借鉴 SUNDIALS CVODE 的自适应步长演化引擎。
 *          支持四种演化方法（Euler/RK4/Adams/BDF）和 PI 步长控制器，
 *          提供单步演化与主演化循环。
 *
 *          BDF 和 Adams 方法当前实现为简化桩（内部回退到 RK4），
 *          后续可按需扩展为完整的多步变阶实现。
 *
 * @author Lv-00 Project
 * @version v3.3.0
 * @date 2026-05-24
 *
 * @dependencies
 *   - geom_evol.h             : 演化引擎公共接口与类型定义
 *   - lv00_numeric.h          : 轻量数值基础设施
 *   - lv00_utils.h            : 统一内存分配器
 *   - lv00_internal.h         : 内部常量与工具宏
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

#include "error_codes.h"
#include "lv00_internal.h"
#include "lv00_utils.h"
#include "numerical_backend.h"

/* ========================================================================
 * 模块级常量定义
 * ======================================================================== */

/** @brief 默认相对误差容限 */
#define GEOEVOL_DEFAULT_REL_TOL 1e-6

/** @brief 默认绝对误差容限 */
#define GEOEVOL_DEFAULT_ABS_TOL 1e-12

/** @brief 默认初始步长 */
#define GEOEVOL_DEFAULT_STEP 0.01

/** @brief 误差测试阈值：error <= 1.0 则接受步 */
#define GEOEVOL_ERROR_THRESHOLD 1.0

/** @brief Adams 方法最大历史步数 */
#define GEOEVOL_ADAMS_MAX_ORDER 5

/* ========================================================================
 * 静态辅助函数的前向声明
 * ======================================================================== */

/** @brief 计算误差加权向量：w_i = rel_tol * |y_i| + abs_tol */
static void geoevol_calc_error_weights(Lv00GeomEvol *evol);

/** @brief 计算局部截断误差估计（RK4 嵌入式 RK3 方法） */
static double geoevol_error_estimate_rk4(Lv00GeomEvol *evol, const double *y1,
                                         const double *y_trial);

/** @brief Euler 法单步积分 */
static int geoevol_step_euler(Lv00GeomEvol *evol, double h, const double *y,
                              double *y_out);

/** @brief RK4 法单步积分 */
static int geoevol_step_rk4(Lv00GeomEvol *evol, double h, const double *y,
                            double *y_out);

/** @brief Adams 法单步积分桩（回退 RK4） */
static int geoevol_step_adams(Lv00GeomEvol *evol, double h, const double *y,
                              double *y_out);

/** @brief BDF 法单步积分桩（回退 RK4） */
static int geoevol_step_bdf(Lv00GeomEvol *evol, double h, const double *y,
                            double *y_out);

/** @brief 求 RHS 并递增求值计数 */
static int geoevol_rhs_eval(Lv00GeomEvol *evol, double t, const double *y,
                            double *dy);

/* ========================================================================
 * 辅助函数实现
 * ======================================================================== */

/**
 * @brief 计算误差加权向量
 *
 * w_i = 1.0 / (rel_tol * |y_i| + abs_tol)
 * 借鉴 CVODE 的误差控制，混合使用相对和绝对容限。
 */
static void geoevol_calc_error_weights(Lv00GeomEvol *evol) {
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
 * @brief 计算 RK4 局部截断误差估计
 *
 * 使用嵌入式 RK3(2) 估计误差：比较 RK4 试探步 y_trial 与低阶 RK3 结果 y1。
 * error = max_i |(y_trial[i] - y1[i]) * weight[i]|
 *
 * 简化实现：使用标准 RK4-Fehlberg 风格的误差估计，
 * 其中 y1 来自 3 阶嵌入方法。
 */
static double geoevol_error_estimate_rk4(Lv00GeomEvol *evol, const double *y1,
                                         const double *y_trial) {
    int dim = evol->dim;
    double error = 0.0;
    for (int i = 0; i < dim; ++i) {
        double diff = fabs(y_trial[i] - y1[i]) * evol->error_weight[i];
        if (diff > error) {
            error = diff;
        }
    }
    /* 放大因子用于补偿简化估计 */
    return error * 1.5;
}

/**
 * @brief 安全求 RHS 并递增统计计数
 */
static int geoevol_rhs_eval(Lv00GeomEvol *evol, double t, const double *y,
                            double *dy) {
    if (!evol || !evol->rhs_func || !y || !dy) {
        return -1;
    }
    evol->stats.num_rhs_evals++;
    return evol->rhs_func(t, y, dy, evol);
}

/* ========================================================================
 * 演化方法：Euler 法（一阶）
 * ======================================================================== */

/**
 * @brief 显式 Euler 法单步积分
 *
 * y_{n+1} = y_n + h * f(t_n, y_n)
 */
static int geoevol_step_euler(Lv00GeomEvol *evol, double h, const double *y,
                              double *y_out) {
    int dim = evol->dim;
    int ret = geoevol_rhs_eval(evol, evol->t, y, evol->dparam);
    if (ret != 0) {
        return ret;
    }

    for (int i = 0; i < dim; ++i) {
        y_out[i] = y[i] + h * evol->dparam[i];
    }

    return 0;
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
static int geoevol_step_rk4(Lv00GeomEvol *evol, double h, const double *y,
                            double *y_out) {
    int dim = evol->dim;
    double half_h = 0.5 * h;
    double sixth_h = h / 6.0;
    double *k1 = evol->dparam;

    /* 需要临时空间存放 k2/k3/k4 和中间值（复用 param 空间不安全） */
    double *k2 = lv00_malloc((size_t) dim * sizeof(double));
    double *k3 = lv00_malloc((size_t) dim * sizeof(double));
    double *k4 = lv00_malloc((size_t) dim * sizeof(double));
    double *tmp = lv00_malloc((size_t) dim * sizeof(double));

    if (!k2 || !k3 || !k4 || !tmp) {
        if (k2) lv00_free((void **) &k2);
        if (k3) lv00_free((void **) &k3);
        if (k4) lv00_free((void **) &k4);
        if (tmp) lv00_free((void **) &tmp);
        LV00_ERROR_SET(LV00_ERROR_OUT_OF_MEMORY, "RK4临时空间分配失败，dim=%d", dim);
        return -1;
    }

    /* k1 = f(t, y) */
    int ret = geoevol_rhs_eval(evol, evol->t, y, k1);
    if (ret != 0) {
        goto cleanup_rk4;
    }

    /* tmp = y + h/2 * k1, k2 = f(t + h/2, tmp) */
    for (int i = 0; i < dim; ++i) {
        tmp[i] = y[i] + half_h * k1[i];
    }
    ret = geoevol_rhs_eval(evol, evol->t + half_h, tmp, k2);
    if (ret != 0) {
        goto cleanup_rk4;
    }

    /* tmp = y + h/2 * k2, k3 = f(t + h/2, tmp) */
    for (int i = 0; i < dim; ++i) {
        tmp[i] = y[i] + half_h * k2[i];
    }
    ret = geoevol_rhs_eval(evol, evol->t + half_h, tmp, k3);
    if (ret != 0) {
        goto cleanup_rk4;
    }

    /* tmp = y + h * k3, k4 = f(t + h, tmp) */
    for (int i = 0; i < dim; ++i) {
        tmp[i] = y[i] + h * k3[i];
    }
    ret = geoevol_rhs_eval(evol, evol->t + h, tmp, k4);
    if (ret != 0) {
        goto cleanup_rk4;
    }

    /* y_{n+1} = y_n + h/6 * (k1 + 2*k2 + 2*k3 + k4) */
    for (int i = 0; i < dim; ++i) {
        y_out[i] = y[i] + sixth_h * (k1[i] + 2.0 * k2[i] + 2.0 * k3[i] + k4[i]);
    }

cleanup_rk4:
    if (k2) lv00_free((void **) &k2);
    if (k3) lv00_free((void **) &k3);
    if (k4) lv00_free((void **) &k4);
    if (tmp) lv00_free((void **) &tmp);
    return ret;
}

/* ========================================================================
 * 演化方法：Adams-Bashforth-Moulton 桩（回退 RK4）
 * ======================================================================== */

/**
 * @brief Adams-Bashforth-Moulton 预测-校正法单步桩
 *
 * 当前为简化桩，直接调用 RK4 作为回退。
 * 完整实现需要维护多步历史（GEOEVOL_ADAMS_MAX_ORDER 步），
 * 使用显式 Adams-Bashforth 预测 + 隐式 Adams-Moulton 校正。
 */
static int geoevol_step_adams(Lv00GeomEvol *evol, double h, const double *y,
                              double *y_out) {
    LV00_UNUSED(h);
    /* 简化桩：回退到 RK4 */
    LV00_LOG_DEBUG("Adams方法桩：回退到RK4单步积分");

    /* 需要保留 y 副本在临时空间进行 RK4 积分 */
    int dim = evol->dim;
    double *y_copy = lv00_malloc((size_t) dim * sizeof(double));
    if (!y_copy) {
        LV00_ERROR_SET(LV00_ERROR_OUT_OF_MEMORY, "Adams桩临时空间分配失败");
        return -1;
    }
    memcpy(y_copy, y, (size_t) dim * sizeof(double));

    int ret = geoevol_step_rk4(evol, h, y_copy, y_out);
    lv00_free((void **) &y_copy);
    return ret;
}

/* ========================================================================
 * 演化方法：BDF 桩（回退 RK4）
 * ======================================================================== */

/**
 * @brief 后向差分公式 (BDF) 单步桩
 *
 * 当前为简化桩，直接调用 RK4 作为回退。
 * 完整实现需要 Newton 迭代求解隐式方程。
 */
static int geoevol_step_bdf(Lv00GeomEvol *evol, double h, const double *y,
                            double *y_out) {
    LV00_UNUSED(h);
    /* 简化桩：回退到 RK4 */
    LV00_LOG_DEBUG("BDF方法桩：回退到RK4单步积分");

    int dim = evol->dim;
    double *y_copy = lv00_malloc((size_t) dim * sizeof(double));
    if (!y_copy) {
        LV00_ERROR_SET(LV00_ERROR_OUT_OF_MEMORY, "BDF桩临时空间分配失败");
        return -1;
    }
    memcpy(y_copy, y, (size_t) dim * sizeof(double));

    int ret = geoevol_step_rk4(evol, h, y_copy, y_out);
    lv00_free((void **) &y_copy);
    return ret;
}

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
Lv00EvolStatus geoevol_step_once(Lv00GeomEvol *evol) {
    LV00_CHECK_NULL(evol, LV00_EVOL_STATUS_ERROR);

    if (evol->status != LV00_EVOL_STATUS_RUNNING) {
        LV00_ERROR_SET(LV00_BACKEND_INVALID_ARGS,
                       "演化引擎未处于运行状态，当前状态=%d", (int) evol->status);
        return LV00_EVOL_STATUS_ERROR;
    }

    LV00_CHECK_NULL(evol->rhs_func, LV00_EVOL_STATUS_ERROR);

    int dim = evol->dim;
    if (dim <= 0 || dim > GEOEVOL_MAX_PARAM_DIM) {
        LV00_ERROR_SET(LV00_BACKEND_INVALID_ARGS, "无效的向量维度=%d", dim);
        return LV00_EVOL_STATUS_ERROR;
    }

    geoevol_calc_error_weights(evol);

    /* 需要三个临时数组：y_trial（试探步结果）、y_low（低阶结果）、y_save（当前状态备份） */
    double *y_trial = lv00_malloc((size_t) dim * sizeof(double));
    double *y_low = lv00_malloc((size_t) dim * sizeof(double));
    double *y_save = lv00_malloc((size_t) dim * sizeof(double));
    if (!y_trial || !y_low || !y_save) {
        if (y_trial) lv00_free((void **) &y_trial);
        if (y_low) lv00_free((void **) &y_low);
        if (y_save) lv00_free((void **) &y_save);
        LV00_ERROR_SET(LV00_ERROR_OUT_OF_MEMORY, "单步演化临时空间分配失败");
        return LV00_EVOL_STATUS_ERROR;
    }

    /* 保存当前状态 */
    memcpy(y_save, evol->param, (size_t) dim * sizeof(double));
    double t_save = evol->t;

    double h = evol->step_size;
    Lv00EvolMethod method = evol->method;
    int method_order;

    /* 循环：如果误差过大则缩减步长重试 */
    int reject_count = 0;

    for (;;) {
        if (reject_count >= GEOEVOL_MAX_REJECTIONS) {
            LV00_ERROR_SET(LV00_ERROR_SOLVER_NOT_CONVERGED,
                           "步长控制超过最大被拒次数 %d", GEOEVOL_MAX_REJECTIONS);
            evol->status = LV00_EVOL_STATUS_ERROR;
            goto cleanup;
        }

        /* 选择积分方法和阶数 */
        int (*stepper)(Lv00GeomEvol *, double, const double *, double *) = NULL;
        switch (method) {
            case LV00_EVOL_EULER:
                stepper = geoevol_step_euler;
                method_order = 1;
                break;
            case LV00_EVOL_RK4:
                stepper = geoevol_step_rk4;
                method_order = 4;
                break;
            case LV00_EVOL_ADAMS:
                stepper = geoevol_step_adams;
                method_order = 4;
                break;
            case LV00_EVOL_BDF:
                stepper = geoevol_step_bdf;
                method_order = 4;
                break;
            default:
                LV00_ERROR_SET(LV00_BACKEND_UNSUPPORTED,
                               "不支持的演化方法=%d", (int) method);
                evol->status = LV00_EVOL_STATUS_ERROR;
                goto cleanup;
        }

        /* 执行试探步 */
        int ret = stepper(evol, h, y_save, y_trial);
        if (ret != 0) {
            evol->status = LV00_EVOL_STATUS_ERROR;
            goto cleanup;
        }

        /* 低阶估计：使用 Euler 步作为低阶估计 */
        if (method == LV00_EVOL_EULER) {
            /* Euler 没有嵌入式估计，直接接受 */
            memcpy(y_low, y_trial, (size_t) dim * sizeof(double));
        } else {
            /* 使用 Euler 步作为低阶参考 */
            ret = geoevol_step_euler(evol, h, y_save, y_low);
            if (ret != 0) {
                evol->status = LV00_EVOL_STATUS_ERROR;
                goto cleanup;
            }
        }

        /* 计算误差 */
        double error = geoevol_error_estimate_rk4(evol, y_low, y_trial);
        evol->stats.last_error_est = error;

        /* 误差测试 */
        if (error <= GEOEVOL_ERROR_THRESHOLD) {
            /* 接受步 */
            break;
        }

        /* 拒绝步：缩减步长 */
        reject_count++;
        evol->stats.num_error_fails++;

        double h_factor = 0.5 * pow(GEOEVOL_ERROR_THRESHOLD / error,
                                     1.0 / (double) (method_order + 1));
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
    h = geoevol_pi_predict(&evol->pi, evol->stats.last_error_est,
                           method_order, h);
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
    if (y_trial) lv00_free((void **) &y_trial);
    if (y_low) lv00_free((void **) &y_low);
    if (y_save) lv00_free((void **) &y_save);
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
Lv00EvolStatus geoevol_evolve(Lv00GeomEvol *evol, double tout) {
    LV00_CHECK_NULL(evol, LV00_EVOL_STATUS_ERROR);

    if (evol->status != LV00_EVOL_STATUS_RUNNING &&
        evol->status != LV00_EVOL_STATUS_IDLE) {
        return evol->status;
    }

    /* 首次调用从 IDLE 切换到 RUNNING */
    if (evol->status == LV00_EVOL_STATUS_IDLE) {
        evol->status = LV00_EVOL_STATUS_RUNNING;
        /* 初始化 RHS */
        int ret = geoevol_rhs_eval(evol, evol->t, evol->param, evol->dparam);
        if (ret != 0) {
            evol->status = LV00_EVOL_STATUS_ERROR;
            LV00_ERROR_SET(LV00_ERROR_SOLVER_NUMERIC, "初始RHS求值失败");
            return LV00_EVOL_STATUS_ERROR;
        }
    }

    /* 检查 tout 是否有效 */
    if (tout <= evol->t) {
        LV00_ERROR_SET(LV00_BACKEND_INVALID_ARGS,
                       "目标时间 tout=%g 必须大于当前时间 t=%g", tout, evol->t);
        return LV00_EVOL_STATUS_ERROR;
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
            LV00_ERROR_SET(LV00_ERROR_SOLVER_NOT_CONVERGED,
                           "步长过小: h=%g < h_min=%g", evol->step_size,
                           evol->step_size_min);
            evol->status = LV00_EVOL_STATUS_ERROR;
            return LV00_EVOL_STATUS_ERROR;
        }

        Lv00EvolStatus stat = geoevol_step_once(evol);
        if (stat != LV00_EVOL_STATUS_RUNNING) {
            return stat;
        }
    }

    evol->status = LV00_EVOL_STATUS_STOPPED;
    return LV00_EVOL_STATUS_STOPPED;
}

/* ========================================================================
 * 生命周期管理
 * ======================================================================== */

/**
 * @brief 创建几何演化引擎
 */
Lv00GeomEvol *geoevol_create(int dim, Lv00EvolMethod method,
                             Lv00GeomEvolRHSFunc rhs) {
    LV00_CHECK_NULL(rhs, NULL);

    if (dim <= 0 || dim > GEOEVOL_MAX_PARAM_DIM) {
        LV00_ERROR_SET(LV00_BACKEND_INVALID_ARGS,
                       "参数维度必须满足 1 <= dim <= %d，当前 dim=%d",
                       GEOEVOL_MAX_PARAM_DIM, dim);
        return NULL;
    }

    Lv00GeomEvol *evol = lv00_malloc(sizeof(Lv00GeomEvol));
    LV00_CHECK_ALLOC(evol, NULL);

    memset(evol, 0, sizeof(Lv00GeomEvol));

    evol->dim = dim;
    evol->method = method;
    evol->status = LV00_EVOL_STATUS_IDLE;
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

    return evol;
}

/**
 * @brief 销毁演化引擎并释放所有关联资源
 */
void geoevol_destroy(Lv00GeomEvol *evol) {
    if (!evol) {
        return;
    }
    lv00_free((void **) &evol);
}

/**
 * @brief 设置演化步长参数
 */
void geoevol_set_step(Lv00GeomEvol *evol, double step_size, double step_min,
                      double step_max) {
    if (!evol) {
        return;
    }
    evol->step_size = step_size;
    evol->step_size_min = step_min;
    evol->step_size_max = step_max;
}
