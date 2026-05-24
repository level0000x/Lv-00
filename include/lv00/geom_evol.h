/**
 * @file geom_evol.h
 * @brief 几何演化引擎 —— 借鉴 SUNDIALS CVODE 自适应步长与误差控制
 *
 * @details 设计借鉴来源：
 *          - SUNDIALS CVODE (github.com/LLNL/sundials) — 刚性/非刚性 ODE 求解器
 *            · 自适应步长控制（基于局部截断误差估计）
 *            · PI 控制器（safety_factor / growth_factor / bias_factor）
 *            · 多步法（Adams-Bashforth-Moulton, BDF）
 *            · 误差控制（相对误差 rel_tol + 绝对误差 abs_tol 混合）
 *
 *          设计目标：
 *          - 将几何约束演化为 ODE 系统在约束流形上演化
 *          - PI 自适应步长控制以保证演化精度和效率
 *          - 支持多种演化方法（Euler/RK4/Adams/BDF）
 *          - 统计信息收集（步数/被拒步数/函数求值次数）
 *
 * @author Lv-00 Project
 * @version v3.3.0
 * @date 2026-05-24
 */

#ifndef LV00_GEOM_EVOL_H
#define LV00_GEOM_EVOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "lv00_numeric.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 常量定义 ==================== */

/** 几何参数向量的最大维度 */
#define GEOEVOL_MAX_PARAM_DIM 256

/** 最小允许步长（绝对值，防止下溢） */
#define GEOEVOL_MIN_STEP 1e-15

/** 最大允许步长 */
#define GEOEVOL_MAX_STEP 1e10

/** PI 步长控制器的 I 分量平滑因子 */
#define GEOEVOL_PI_SMOOTH_FACTOR 0.25

/** 最大被拒步数（超过后触发错误） */
#define GEOEVOL_MAX_REJECTIONS 20

/* ==================== 类型定义 ==================== */

/**
 * @brief 演化方法类型 —— 借鉴 CVODE 的积分器选择
 *
 * CVODE 提供 Adams（非刚性）和 BDF（刚性）两类方法，
 * 同时用户也可选择固定步长的显式方法。
 */
typedef enum {
    LV00_EVOL_EULER    = 0,  /**< 显式 Euler 法（一阶，用于调试/快速测试） */
    LV00_EVOL_RK4      = 1,  /**< 经典四阶 Runge-Kutta 法（非刚性） */
    LV00_EVOL_ADAMS    = 2,  /**< Adams-Bashforth-Moulton 预测-校正法（非刚性，
                                  借鉴 CVODE Adams 变阶变步长实现） */
    LV00_EVOL_BDF      = 3   /**< 后向差分公式 BDF（刚性，
                                  借鉴 CVODE BDF 变阶变步长实现） */
} Lv00EvolMethod;

/**
 * @brief 演化状态
 */
typedef enum {
    LV00_EVOL_STATUS_IDLE       = 0,  /**< 空闲（未开始） */
    LV00_EVOL_STATUS_RUNNING    = 1,  /**< 演化进行中 */
    LV00_EVOL_STATUS_CONVERGED  = 2,  /**< 已收敛 */
    LV00_EVOL_STATUS_STOPPED    = 3,  /**< 已停止（达到终止时间） */
    LV00_EVOL_STATUS_ERROR      = 4   /**< 发生错误 */
} Lv00EvolStatus;

/* ==================== 回调类型 ==================== */

/** @cond 前向声明 */
typedef struct Lv00GeomEvol Lv00GeomEvol;
/** @endcond */

/**
 * @brief 几何演化右端函数（RHS）—— 计算约束流形上的"速度场"
 *
 * 借鉴 CVODE 的 CVRhsFn 设计。给定当前参数向量 param 和时间 t，
 * 计算 d(param)/dt = f(t, param)，即几何参数随时间演化的速度。
 *
 * @param[in]  t       当前时间（演化参数）
 * @param[in]  param   当前参数向量（维度为 evol->dim）
 * @param[out] dparam  输出：参数向量的时间导数 d(param)/dt
 * @param[in]  evol    演化引擎指针（可访问附加上下文）
 * @return 成功返回 0，失败返回非零
 */
typedef int (*Lv00GeomEvolRHSFunc)(double t, const double *param,
                                    double *dparam, Lv00GeomEvol *evol);

/**
 * @brief 演化步后处理回调
 *
 * 每一步成功后被调用，可用于日志、可视化更新、事件检测触发等。
 *
 * @param[in] evol  演化引擎
 * @param[in] t     当前时间
 * @param[in] param 当前参数
 */
typedef void (*Lv00GeomEvolPostStepFunc)(const Lv00GeomEvol *evol,
                                          double t, const double *param);

/**
 * @brief 根函数（事件函数）类型声明
 *
 * 在演化过程中检测 g(t, param) 的符号变化。
 * 具体事件检测实现见 geo_event_detect.h。
 *
 * @param[in]  t      当前时间
 * @param[in]  param  当前参数向量
 * @param[out] gout   输出：事件函数值数组
 * @param[in]  evol   演化引擎
 * @return 成功返回 0
 */
typedef int (*Lv00GeomEvolRootFunc)(double t, const double *param,
                                     double *gout, Lv00GeomEvol *evol);

/* ==================== PI 步长控制器 ==================== */

/**
 * @brief PI 步长控制器参数 —— 借鉴 CVODE 自适应步长控制
 *
 * CVODE 使用 PI (Proportional-Integral) 控制器来动态调整步长：
 *   h_new = h * safety * (1/error)^(k_I + k_P)
 * 其中 k_I 使用前一次的误差估计比率，提供积分反馈。
 */
typedef struct Lv00GeomEvolPI {
    double safety_factor;    /**< 安全因子（典型值 0.9），保证步长不过于激进 */
    double growth_factor;    /**< 最大增长因子（典型值 2.0~5.0） */
    double bias_factor;      /**< PI 控制器偏置：P 分量权重（典型值 0.6） */
    double min_scale;        /**< 最小缩放因子（典型值 0.1~0.2） */
    double max_scale;        /**< 最大缩放因子（典型值 5.0~10.0） */
    double error_prev;       /**< 上一步误差估计（用于 I 分量） */
    double est_prev;         /**< 上一步步长估计（用于 I 分量） */
    bool   initialized;      /**< 是否已初始化（至少成功走了一步） */
} Lv00GeomEvolPI;

/* ==================== 步统计信息 ==================== */

/**
 * @brief 演化步统计信息 —— 借鉴 CVODE 统计结构
 */
typedef struct Lv00GeomEvolStats {
    int64_t num_steps;           /**< 累计已执行步数 */
    int64_t num_rhs_evals;       /**< 累计 RHS 函数求值次数 */
    int64_t num_error_fails;     /**< 局部误差测试失败次数（步被拒绝） */
    int64_t num_convergence_fails; /**< 求解器收敛失败次数 */
    int64_t num_root_events;     /**< 检测到的事件（求根触发）次数 */
    double  last_step_size;      /**< 当前步长 */
    double  last_error_est;      /**< 最近的误差估计 */
    double  total_time;          /**< 已模拟的总时间 */
} Lv00GeomEvolStats;

/* ==================== 演化引擎主结构 ==================== */

/**
 * @brief 几何演化引擎 —— 借鉴 CVODE 核心数据结构
 *
 * 将 CVODE 的 void *cvode_mem 设计转化为显式 C 结构体，
 * 包含 RHS 函数、当前参数状态、误差控制、步长控制器和统计信息。
 */
struct Lv00GeomEvol {
    /* ── 配置 ── */
    int dim;                          /**< 参数向量维度 */
    Lv00EvolMethod method;            /**< 演化方法 */
    Lv00EvolStatus status;            /**< 当前状态 */

    /* ── 当前状态 ── */
    double t;                         /**< 当前演化时间 */
    double param[GEOEVOL_MAX_PARAM_DIM]; /**< 当前参数向量 */
    double dparam[GEOEVOL_MAX_PARAM_DIM]; /**< 当前参数导数（速度场） */

    /* ── 步长控制 ── */
    double step_size;                 /**< 当前步长 */
    double step_size_min;             /**< 最小允许步长 */
    double step_size_max;             /**< 最大允许步长 */

    /* ── 误差控制 ── */
    double rel_tol;                   /**< 相对误差容限（典型值 1e-6） */
    double abs_tol;                   /**< 绝对误差容限（典型值 1e-12） */
    double error_weight[GEOEVOL_MAX_PARAM_DIM]; /**< 误差加权向量（rel*|y| + abs） */

    /* ── PI 控制器 ── */
    Lv00GeomEvolPI pi;                /**< PI 步长控制器 */

    /* ── 回调 ── */
    Lv00GeomEvolRHSFunc rhs_func;     /**< 右端函数（必须设置） */
    Lv00GeomEvolPostStepFunc post_step; /**< 步后处理回调（可选） */
    Lv00GeomEvolRootFunc root_func;   /**< 根检测函数（可选） */
    int root_ng;                      /**< 根函数数量 */
    void *user_data;                  /**< 用户自定义数据（透传给回调） */

    /* ── 统计 ── */
    Lv00GeomEvolStats stats;          /**< 步统计信息 */
};

/* ==================== API 函数 ==================== */

/**
 * @brief 创建几何演化引擎
 *
 * @param[in] dim     参数向量维度
 * @param[in] method  演化方法
 * @param[in] rhs     右端函数（必须非 NULL）
 * @return 新分配的演化引擎，失败返回 NULL
 */
Lv00GeomEvol* geoevol_create(int dim, Lv00EvolMethod method,
                              Lv00GeomEvolRHSFunc rhs);

/**
 * @brief 销毁演化引擎并释放所有关联资源
 *
 * @param[in,out] evol  要销毁的引擎（设为 NULL 是安全的）
 */
void geoevol_destroy(Lv00GeomEvol *evol);

/**
 * @brief 设置演化步长参数
 *
 * @param[in,out] evol      演化引擎
 * @param[in]     step_size 初始步长
 * @param[in]     step_min  最小步长
 * @param[in]     step_max  最大步长
 */
void geoevol_set_step(Lv00GeomEvol *evol, double step_size,
                       double step_min, double step_max);

/**
 * @brief 将演化引擎运行至目标时间 tout
 *
 * 从当前时间 evol->t 出发，持续调用 geoevol_step_once() 直到 t >= tout。
 *
 * @param[in,out] evol  演化引擎
 * @param[in]     tout  目标时间（必须 > evol->t）
 * @return 演化状态（IDLE 表示未初始化，CONVERGED/STOPPED 表示已完成）
 */
Lv00EvolStatus geoevol_evolve(Lv00GeomEvol *evol, double tout);

/**
 * @brief 执行单步演化（内部核心，自适应步长）
 *
 * 借鉴 CVODE 的 CVode() 单步逻辑：
 * 1. 计算 RHS 求值
 * 2. 使用选定的积分方法计算试探步
 * 3. 进行局部误差测试
 * 4. 若误差过大则缩减步长并重试，否则接受步
 * 5. 使用 PI 控制器预测下一步步长
 *
 * @param[in,out] evol  演化引擎
 * @return 演化状态
 */
Lv00EvolStatus geoevol_step_once(Lv00GeomEvol *evol);

/**
 * @brief 获取演化统计信息的只读副本
 *
 * @param[in] evol  演化引擎
 * @return 指向 stats 的常量指针
 */
static inline const Lv00GeomEvolStats* geoevol_get_step_stats(
        const Lv00GeomEvol *evol) {
    return (evol) ? &evol->stats : NULL;
}

/* ==================== PI 控制器辅助函数 ==================== */

/**
 * @brief 初始化 PI 步长控制器
 *
 * 设置默认参数：safety=0.9, growth=2.5, bias=0.6, min_scale=0.2, max_scale=5.0。
 *
 * @param[out] pi  PI 控制器
 */
static inline void geoevol_pi_init(Lv00GeomEvolPI *pi) {
    pi->safety_factor  = 0.9;
    pi->growth_factor  = 2.5;
    pi->bias_factor    = 0.6;
    pi->min_scale      = 0.2;
    pi->max_scale      = 5.0;
    pi->error_prev     = 1.0;
    pi->est_prev       = 1.0;
    pi->initialized    = false;
}

/**
 * @brief PI 步长预测：根据误差估计给出下一合适步长
 *
 * 借鉴 CVODE 的 CVAdjustParams 函数：
 *   factor = safety * (1.0 / error)^(bias/(order+1)) * (error_prev/error)^(1-bias)
 *
 * @param[in,out] pi      PI 控制器（更新内部状态）
 * @param[in]     error   当前步的误差估计
 * @param[in]     order   当前方法的阶数（Euler=1, RK4=4, Adams 可变）
 * @param[in]     h_curr  当前步长
 * @return 建议的下一步长（限制在 [min_scale*h_curr, max_scale*h_curr]）
 */
static inline double geoevol_pi_predict(Lv00GeomEvolPI *pi,
                                         double error, int order,
                                         double h_curr) {
    double exponent = pi->bias_factor / (double)(order + 1);
    double factor;
    if (pi->initialized && error > 0.0) {
        factor = pi->safety_factor
               * pow(1.0 / error, exponent)
               * pow(pi->error_prev / error, 1.0 - pi->bias_factor);
    } else {
        factor = pi->safety_factor * pow(1.0 / error, exponent);
    }
    /* 限制缩放范围 */
    if (factor > pi->max_scale) factor = pi->max_scale;
    if (factor < pi->min_scale) factor = pi->min_scale;

    pi->error_prev = error;
    pi->est_prev   = factor;
    pi->initialized = true;
    return h_curr * factor;
}

/* ═══════════════════════════════════════════════════════════════
 * 使用示例（参考）
 * ═══════════════════════════════════════════════════════════════
 *
 * @code
 * // 定义一个简单的 2D 几何演化 RHS：参数在圆上匀速旋转
 * static int circle_rhs(double t, const double *param, double *dparam,
 *                       Lv00GeomEvol *evol) {
 *     (void)t; (void)evol;
 *     dparam[0] = -param[1];   // dx/dt = -y
 *     dparam[1] =  param[0];   // dy/dt =  x
 *     return 0;
 * }
 *
 * // 创建并运行演化
 * Lv00GeomEvol *evol = geoevol_create(2, LV00_EVOL_RK4, circle_rhs);
 * evol->param[0] = 1.0; evol->param[1] = 0.0;
 * evol->rel_tol  = 1e-6;
 * evol->abs_tol  = 1e-12;
 * geoevol_set_step(evol, 0.01, 1e-8, 0.1);
 * geoevol_evolve(evol, 2.0 * LV00_PI);  // 演化一圈
 *
 * const Lv00GeomEvolStats *st = geoevol_get_step_stats(evol);
 * printf("Steps: %lld, RHS evals: %lld\n", st->num_steps, st->num_rhs_evals);
 * geoevol_destroy(evol);
 * @endcode
 */

#ifdef __cplusplus
}
#endif

#endif /* LV00_GEOM_EVOL_H */
