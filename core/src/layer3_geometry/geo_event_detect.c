/**
 * @file geo_event_detect.c
 * @brief 几何事件检测器 —— 基于 SUNDIALS Rootfinding 的事件检测实现
 *
 * @details 实现事件检测器的完整生命周期、事件注册、符号变化检测和
 *          多求根方法（Brent/Illinois/Bisection）的精确定位。
 *
 *          Brent 法已在头文件中以内联形式提供完整实现，
 *          本文件在 geo_event_root_locate() 中根据配置选择求根方法并调度。
 *
 * @author Lv-00 Project
 * @version v3.3.0
 * @date 2026-05-24
 *
 * @dependencies
 *   - geo_event_detect.h      : 事件检测器公共接口与类型定义
 *   - lv00_numeric.h          : 轻量数值基础设施
 *   - lv00_utils.h            : 统一内存分配器
 *   - lv00_internal.h         : 内部常量与工具宏
 *   - error_codes.h           : 统一错误码系统
 */

/* ========================================================================
 * 包含头文件
 * ======================================================================== */

#include "geo_event_detect.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "error_codes.h"
#include "lv00_internal.h"
#include "lv00_utils.h"

/* ========================================================================
 * 模块级常量定义
 * ======================================================================== */

/** @brief 参数缓存最大维度 */
#define GEODET_CACHE_DIM_MAX 512

/** @brief Illinois 法参数：迭代中缩小低效端点的比例因子 */
#define GEODET_ILLINOIS_FACTOR 0.5

/** @brief 周期性事件检测的分辨率（每周期内检测的精细点数） */
#define GEODET_PERIODIC_PTS 16

/* ========================================================================
 * 静态辅助函数的前向声明
 * ======================================================================== */

static int geodet_root_bisection(Lv00EventDetector *detector, int event_id,
                                  const double *param_a, const double *param_b,
                                  int dim, double a, double b, double ga,
                                  double gb, double *root);

static int geodet_root_illinois(Lv00EventDetector *detector, int event_id,
                                 const double *param_a, const double *param_b,
                                 int dim, double a, double b, double ga,
                                 double gb, double *root);

static int geodet_eval_event_func(Lv00EventDetector *detector, int event_idx,
                                   double t, const double *param, int dim,
                                   double *g_out);

static int geodet_check_intersection(double t, const double *param, int dim,
                                      double *g, Lv00EventDetector *detector);

static int geodet_check_contact(double t, const double *param, int dim,
                                 double *g, Lv00EventDetector *detector);

static int geodet_check_crossing(double t, const double *param, int dim,
                                  double *g, Lv00EventDetector *detector);

static int geodet_check_threshold(double t, const double *param, int dim,
                                   double *g, Lv00EventDetector *detector);

static int geodet_check_periodic(double t, const double *param, int dim,
                                  double *g, Lv00EventDetector *detector);

static int geodet_find_event_index(const Lv00EventDetector *detector,
                                    int event_id);

/* ========================================================================
 * 事件函数求值辅助
 * ======================================================================== */

/**
 * @brief 在给定时间 t 和参数 param 处求事件函数 g(t, param)
 *
 * @param[in]  detector   事件检测器
 * @param[in]  event_idx  事件数组索引
 * @param[in]  t          当前参数值
 * @param[in]  param      当前参数向量
 * @param[in]  dim        参数维度
 * @param[out] g_out      事件函数值
 * @return 成功返回 0，失败返回非零
 */
static int geodet_eval_event_func(Lv00EventDetector *detector, int event_idx,
                                   double t, const double *param, int dim,
                                   double *g_out) {
    if (!detector || !param || !g_out) {
        return -1;
    }
    if (event_idx < 0 || event_idx >= detector->num_events) {
        return -1;
    }
    Lv00EventEntry *evt = &detector->events[event_idx];
    if (!evt->enabled || !evt->func) {
        *g_out = 1.0; /* 禁用事件不触发 */
        return 0;
    }

    return evt->func(t, param, dim, g_out, detector);
}

/**
 * @brief 根据事件 ID 查找事件在数组中的索引
 *
 * @return 索引位置，未找到返回 -1
 */
static int geodet_find_event_index(const Lv00EventDetector *detector,
                                    int event_id) {
    if (!detector) {
        return -1;
    }
    for (int i = 0; i < detector->num_events; ++i) {
        if (detector->events[i].event_id == event_id) {
            return i;
        }
    }
    return -1;
}

/* ========================================================================
 * 事件类型检查函数
 *
 * 每种事件类型提供默认的事件函数计算逻辑。
 * 当用户未指定 func 时，系统根据事件类型选择默认行为。
 * 当前实现提供基础的占位逻辑。
 * ======================================================================== */

/**
 * @brief 交点事件：检测两条曲线的距离是否为零
 *
 * 使用 param 的最后 4 个分量作为两条曲线在 2D 下的位置。
 * g = distance_squared - epsilon
 */
static int geodet_check_intersection(double t, const double *param, int dim,
                                      double *g, Lv00EventDetector *detector) {
    LV00_UNUSED(t);
    LV00_UNUSED(detector);
    if (dim < 4) {
        *g = 1.0;
        return 0;
    }
    double dx = param[0] - param[2];
    double dy = param[1] - param[3];
    *g = dx * dx + dy * dy;
    return 0;
}

/**
 * @brief 接触事件：检测距离是否小于某阈值
 *
 * g = distance_squared - threshold^2
 */
static int geodet_check_contact(double t, const double *param, int dim,
                                 double *g, Lv00EventDetector *detector) {
    LV00_UNUSED(t);
    LV00_UNUSED(detector);
    if (dim < 4) {
        *g = 1.0;
        return 0;
    }
    double dx = param[0] - param[2];
    double dy = param[1] - param[3];
    /* 接触阈值设为一个较小正数 */
    double threshold = 1e-3;
    *g = dx * dx + dy * dy - threshold * threshold;
    return 0;
}

/**
 * @brief 穿越事件：检测 y 坐标越过零线
 *
 * g = param[y_index]，使用 param[1] 为 y 坐标
 */
static int geodet_check_crossing(double t, const double *param, int dim,
                                  double *g, Lv00EventDetector *detector) {
    LV00_UNUSED(t);
    LV00_UNUSED(detector);
    if (dim < 2) {
        *g = 1.0;
        return 0;
    }
    *g = param[1];
    return 0;
}

/**
 * @brief 阈值事件：检测某分量是否超过指定阈值
 *
 * g = |param[0]| - threshold
 */
static int geodet_check_threshold(double t, const double *param, int dim,
                                   double *g, Lv00EventDetector *detector) {
    LV00_UNUSED(t);
    LV00_UNUSED(detector);
    if (dim < 1) {
        *g = 1.0;
        return 0;
    }
    double threshold = 1.0;
    *g = fabs(param[0]) - threshold;
    return 0;
}

/**
 * @brief 周期性事件：使用三角函数映射到等距时间段
 *
 * g = sin(2*pi*t / period)，周期设为 1.0
 */
static int geodet_check_periodic(double t, const double *param, int dim,
                                  double *g, Lv00EventDetector *detector) {
    LV00_UNUSED(param);
    LV00_UNUSED(dim);
    LV00_UNUSED(detector);
    double period = 1.0;
    *g = sin(2.0 * M_PI * t / period);
    return 0;
}

/* ========================================================================
 * 二分法求根
 * ======================================================================== */

/**
 * @brief 二分法求根实现
 *
 * 最稳健但收敛较慢的求根方法，作为其他方法的备选。
 */
static int geodet_root_bisection(Lv00EventDetector *detector, int event_id,
                                  const double *param_a, const double *param_b,
                                  int dim, double a, double b, double ga,
                                  double gb, double *root) {
    LV00_UNUSED(param_a);
    LV00_UNUSED(param_b);

    int event_idx = geodet_find_event_index(detector, event_id);
    if (event_idx < 0) {
        return -1;
    }

    double tol = detector->root_tol;
    int max_iter = detector->max_root_iters;

    double fa = ga;
    double fb = gb;
    double mid, fmid;

    for (int iter = 0; iter < max_iter; ++iter) {
        mid = 0.5 * (a + b);
        if (fabs(b - a) < tol) {
            *root = mid;
            return 0;
        }

        int ret = geodet_eval_event_func(detector, event_idx, mid, param_b,
                                          dim, &fmid);
        if (ret != 0) {
            return -1;
        }

        if (fmid == 0.0) {
            *root = mid;
            return 0;
        }

        if (fa * fmid < 0.0) {
            b = mid;
            fb = fmid;
        } else {
            a = mid;
            fa = fmid;
        }
    }

    *root = 0.5 * (a + b);
    return -1;
}

/* ========================================================================
 * Illinois 法求根
 *
 * Illinois 法是改进的试位法（False Position），借鉴 SUNDIALS 默认实现。
 * 当一端被"卡住"时，将该端的函数值缩小至一半以加速收敛。
 * ======================================================================== */

/**
 * @brief Illinois 改进试位法求根实现
 *
 * 算法：
 *   1. 使用线性插值得到猜测点 x = (a*fb - b*fa) / (fb - fa)
 *   2. 计算 f(x)
 *   3. 若 f(x) 与 f(a) 同号，则将 f(b) 减半（Illinois 技巧）
 *   4. 更新区间
 */
static int geodet_root_illinois(Lv00EventDetector *detector, int event_id,
                                 const double *param_a, const double *param_b,
                                 int dim, double a, double b, double ga,
                                 double gb, double *root) {
    int event_idx = geodet_find_event_index(detector, event_id);
    if (event_idx < 0) {
        return -1;
    }

    double tol = detector->root_tol;
    int max_iter = detector->max_root_iters;

    double x_l = a, f_l = ga;
    double x_r = b, f_r = gb;
    int stall_counter = 0; /* 一端被卡住的计数器 */

    for (int iter = 0; iter < max_iter; ++iter) {
        /* 线性插值 */
        double denom = f_r - f_l;
        if (fabs(denom) < LV00_EPSILON_DOUBLE) {
            /* 退化为二分法 */
            double x_mid = 0.5 * (x_l + x_r);
            *root = x_mid;
            return 0;
        }

        double x_new = (x_l * f_r - x_r * f_l) / denom;

        /* 检查区间宽度 */
        if (fabs(x_r - x_l) < tol) {
            *root = x_new;
            return 0;
        }

        double f_new;
        int ret = geodet_eval_event_func(detector, event_idx, x_new, param_b,
                                          dim, &f_new);
        if (ret != 0) {
            return -1;
        }

        if (f_new == 0.0) {
            *root = x_new;
            return 0;
        }

        /* 更新区间 */
        if (f_l * f_new < 0.0) {
            /* 根在 [x_l, x_new] */
            x_r = x_new;
            f_r = f_new;
            stall_counter = 0;
        } else {
            /* 根在 [x_new, x_r]，f_l 端被卡住 */
            x_l = x_new;
            f_r *= GEODET_ILLINOIS_FACTOR; /* Illinois 技巧 */
            f_l = f_new;
            stall_counter++;
        }

        /* 若一端被卡住过久，回退到二分法 */
        if (stall_counter > 3) {
            double x_mid = 0.5 * (x_l + x_r);
            ret = geodet_eval_event_func(detector, event_idx, x_mid, param_b,
                                          dim, &f_new);
            if (ret != 0) {
                return -1;
            }
            if (f_l * f_new < 0.0) {
                x_r = x_mid;
                f_r = f_new;
            } else {
                x_l = x_mid;
                f_l = f_new;
            }
            stall_counter = 0;
        }
    }

    *root = 0.5 * (x_l + x_r);
    return -1;
}

/* ========================================================================
 * 生命周期管理
 * ======================================================================== */

/**
 * @brief 创建几何事件检测器
 */
Lv00EventDetector *geo_event_detector_create(void) {
    Lv00EventDetector *detector =
        lv00_malloc(sizeof(Lv00EventDetector));
    LV00_CHECK_ALLOC(detector, NULL);

    memset(detector, 0, sizeof(Lv00EventDetector));

    detector->num_events = 0;
    detector->root_method = LV00_ROOTFIND_BRENT;
    detector->root_tol = GEO_EVENT_DEFAULT_TOL;
    detector->max_root_iters = GEO_EVENT_MAX_ROOT_ITERS;
    detector->t_prev = 0.0;
    detector->user_data = NULL;

    for (int i = 0; i < GEO_EVENT_MAX_EVENTS; ++i) {
        detector->events[i].event_id = -1;
        detector->events[i].enabled = false;
        detector->events[i].func = NULL;
        detector->events[i].callback = NULL;
        detector->g_prev[i] = 0.0;
    }

    return detector;
}

/**
 * @brief 销毁事件检测器并释放所有关联资源
 */
void geo_event_detector_destroy(Lv00EventDetector *detector) {
    if (!detector) {
        return;
    }
    lv00_free((void **) &detector);
}

/* ========================================================================
 * 事件注册
 * ======================================================================== */

/**
 * @brief 注册一个几何事件
 */
int geo_event_register(Lv00EventDetector *detector, int event_id,
                       Lv00EventType type, Lv00EventFunc func, int direction,
                       bool terminal, Lv00EventCallback callback) {
    LV00_CHECK_NULL(detector, -1);

    if (detector->num_events >= GEO_EVENT_MAX_EVENTS) {
        LV00_ERROR_SET(LV00_ERROR_OVERFLOW,
                       "事件注册已满，最大%d个事件", GEO_EVENT_MAX_EVENTS);
        return -1;
    }

    /* 检查 ID 是否重复 */
    for (int i = 0; i < detector->num_events; ++i) {
        if (detector->events[i].event_id == event_id) {
            LV00_ERROR_SET(LV00_ERROR_ALREADY_EXISTS,
                           "事件ID=%d已存在", event_id);
            return -1;
        }
    }

    /* 若未提供自定义事件函数，使用类型默认函数 */
    if (!func) {
        switch (type) {
            case LV00_EVENT_INTERSECTION:
                func = geodet_check_intersection;
                break;
            case LV00_EVENT_CONTACT:
                func = geodet_check_contact;
                break;
            case LV00_EVENT_CROSSING:
                func = geodet_check_crossing;
                break;
            case LV00_EVENT_THRESHOLD:
                func = geodet_check_threshold;
                break;
            case LV00_EVENT_PERIODIC:
                func = geodet_check_periodic;
                break;
            default:
                LV00_ERROR_SET(LV00_ERROR_INVALID_PARAM,
                               "自定义事件LV00_EVENT_CUSTOM必须提供func参数");
                return -1;
        }
    }

    int idx = detector->num_events;
    detector->events[idx].event_id = event_id;
    detector->events[idx].type = type;
    detector->events[idx].func = func;
    detector->events[idx].direction = direction;
    detector->events[idx].enabled = true;
    detector->events[idx].terminal = terminal;
    detector->events[idx].callback = callback;

    detector->num_events++;

    /* 初始化 g_prev 为一个大正值，避免误触发 */
    detector->g_prev[idx] = 1.0;

    return 0;
}

/* ========================================================================
 * 事件检测主循环
 * ======================================================================== */

/**
 * @brief 检测事件：在演化步 [t_prev, t_curr] 间检测事件
 *
 * 借鉴 SUNDIALS CVodeRootInit + CVode 求根逻辑：
 * 1. 对每个已注册的活跃事件，计算 t_prev 和 t_curr 处的事件函数值
 * 2. 检查是否有符号变化并满足方向条件
 * 3. 对满足条件的事件，通过求根方法精确定位事件时间
 * 4. 触发对应回调
 */
Lv00EventResult geo_event_detect(Lv00EventDetector *detector, double t_prev,
                                 const double *param_prev, double t_curr,
                                 const double *param_curr, int dim,
                                 int *event_id, double *t_event) {
    LV00_CHECK_NULL(detector, LV00_EVENT_RESULT_ERROR);
    LV00_CHECK_NULL(param_prev, LV00_EVENT_RESULT_ERROR);
    LV00_CHECK_NULL(param_curr, LV00_EVENT_RESULT_ERROR);
    LV00_CHECK_NULL(event_id, LV00_EVENT_RESULT_ERROR);
    LV00_CHECK_NULL(t_event, LV00_EVENT_RESULT_ERROR);

    *event_id = -1;
    *t_event = t_curr;

    if (detector->num_events == 0) {
        return LV00_EVENT_RESULT_NONE;
    }

    /* 遍历所有注册事件 */
    for (int i = 0; i < detector->num_events; ++i) {
        Lv00EventEntry *evt = &detector->events[i];
        if (!evt->enabled || !evt->func) {
            continue;
        }

        /* 计算 t_prev 处的事件函数值 */
        double g_prev_val;
        int ret = evt->func(t_prev, param_prev, dim, &g_prev_val, detector);
        if (ret != 0) {
            continue;
        }

        /* 计算 t_curr 处的事件函数值 */
        double g_curr_val;
        ret = evt->func(t_curr, param_curr, dim, &g_curr_val, detector);
        if (ret != 0) {
            continue;
        }

        /* 检查符号变化和方向条件 */
        if (!geo_event_check_sign(g_prev_val, g_curr_val, evt->direction)) {
            detector->g_prev[i] = g_curr_val;
            continue;
        }

        /* 符号已变化且满足方向条件：精确定位事件时间 */
        double event_time;
        ret = geo_event_root_locate(detector, evt->event_id, param_prev,
                                     param_curr, dim, t_prev, t_curr,
                                     g_prev_val, g_curr_val, &event_time);
        if (ret != 0) {
            /* 定位未收敛：使用警告结果 */
            event_time = 0.5 * (t_prev + t_curr);
        }

        *event_id = evt->event_id;
        *t_event = event_time;

        /* 触发回调 */
        if (evt->callback) {
            evt->callback(detector, evt->event_id, event_time, param_curr);
        }

        detector->g_prev[i] = g_curr_val;

        return (ret == 0) ? LV00_EVENT_RESULT_DETECTED
                          : LV00_EVENT_RESULT_WARNING;
    }

    return LV00_EVENT_RESULT_NONE;
}

/* ========================================================================
 * 求根定位调度
 * ======================================================================== */

/**
 * @brief 在区间 [a, b] 内精确求根定位
 *
 * 根据检测器配置的 root_method 选择合适的求根算法。
 */
int geo_event_root_locate(Lv00EventDetector *detector, int event_id,
                          const double *param_a, const double *param_b,
                          int dim, double a, double b, double ga, double gb,
                          double *root) {
    LV00_CHECK_NULL(detector, -1);
    LV00_CHECK_NULL(root, -1);

    /* 调用对应的方法 */
    switch (detector->root_method) {
        case LV00_ROOTFIND_BRENT:
            /* 使用头文件中的内联 Brent 法实现 */
            return geo_event_root_brent(detector, event_id, param_a, param_b,
                                         dim, a, b, ga, gb,
                                         detector->root_tol,
                                         detector->max_root_iters, root);

        case LV00_ROOTFIND_ILLINOIS:
            return geodet_root_illinois(detector, event_id, param_a, param_b,
                                         dim, a, b, ga, gb, root);

        case LV00_ROOTFIND_BISECTION:
            return geodet_root_bisection(detector, event_id, param_a, param_b,
                                          dim, a, b, ga, gb, root);

        default:
            /* 默认回退 Brent 法 */
            return geo_event_root_brent(detector, event_id, param_a, param_b,
                                         dim, a, b, ga, gb,
                                         detector->root_tol,
                                         detector->max_root_iters, root);
    }
}
