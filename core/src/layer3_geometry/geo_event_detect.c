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
 *   - lv_numeric.h          : 轻量数值基础设施
 *   - lv_utils.h            : 统一内存分配器
 *   - lv_internal.h         : 内部常量与工具宏
 *   - error_codes.h           : 统一错误码系统
 */

/* ========================================================================
 * 包含头文件
 * ======================================================================== */

#include "geo_event_detect.h"
#include "geo_utils.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "error_codes.h"
#include "lv/lv_registry.h"
#include "lv/lv_thread.h"
#include "lv_internal.h"
#include "lv_utils.h"

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
 * 事件注册表（泛型注册表设施）
 *
 * key = "<detector>:<event_id>"，value = lvEventEntry*（指向 detector->events[i]）。
 * 注册表承担 event_id 查重、尾部追加与动态扩容；
 * detector->events 数组仍为事件数据的主存储（geodet_eval_event_func 等按索引访问），
 * 两者追加顺序一致（注册表条目下标 == events 下标，无删除路径）。
 * 文件级单例（lv_once 惰性初始化，线程安全）。
 * ======================================================================== */

/** @brief 事件注册表 key 缓冲区大小 */
#define GEODET_REGKEY_MAX 96

/** @brief 事件注册表（文件级单例） */
lv_REGISTRY_STATIC(geo_event_registry, GEO_EVENT_MAX_EVENTS);

/** @brief 构造事件注册表 key（栈缓冲区，调用方提供） */
static void geodet_build_key(const lvEventDetector *detector, int event_id, char *buf, size_t bufsz) {
    snprintf(buf, bufsz, "%p:%d", (const void *) detector, event_id);
}

/* ========================================================================
 * 静态辅助函数的前向声明
 * ======================================================================== */

static int geodet_root_bisection(lvEventDetector *detector, int event_id, const double *param_a, const double *param_b,
                                 int dim, double a, double b, double ga, double gb, double *root);

static int geodet_root_illinois(lvEventDetector *detector, int event_id, const double *param_a, const double *param_b,
                                int dim, double a, double b, double ga, double gb, double *root);

static int geodet_eval_event_func(lvEventDetector *detector, int event_idx, double t, const double *param, int dim,
                                  double *g_out);

static int geodet_check_intersection(double t, const double *param, int dim, double *g, lvEventDetector *detector);

static int geodet_check_contact(double t, const double *param, int dim, double *g, lvEventDetector *detector);

static int geodet_check_crossing(double t, const double *param, int dim, double *g, lvEventDetector *detector);

static int geodet_check_threshold(double t, const double *param, int dim, double *g, lvEventDetector *detector);

static int geodet_check_periodic(double t, const double *param, int dim, double *g, lvEventDetector *detector);

static int geodet_find_event_index(const lvEventDetector *detector, int event_id);

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
static int geodet_eval_event_func(lvEventDetector *detector, int event_idx, double t, const double *param, int dim,
                                  double *g_out) {
    if (!detector || !param || !g_out) {
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "geodet_eval_event_func: NULL parameter");
    }
    if (event_idx < 0 || event_idx >= detector->num_events) {
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "geodet_eval_event_func: invalid event_idx %d", event_idx);
    }
    lvEventEntry *evt = &detector->events[event_idx];
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
static int geodet_find_event_index(const lvEventDetector *detector, int event_id) {
    if (!detector) {
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "geodet_find_event_index: NULL detector");
    }

    geo_event_registry_ensure();

    /* 委托注册表按 key（"<detector>:<event_id>"）查找；
     * 注册表条目下标与 events 数组下标一致（同步尾部追加、无删除路径） */
    char regkey[GEODET_REGKEY_MAX];
    geodet_build_key(detector, event_id, regkey, sizeof(regkey));
    int idx = lv_registry_find(&g_geo_event_registry, regkey);
    if (idx < 0) {
        lv_RETURN_ERROR(lv_ERROR_NOT_FOUND, "geodet_find_event_index: event_id %d not found", event_id);
    }
    return idx;
}

/* ========================================================================
 * 事件类型检查函数
 *
 * 每种事件类型提供默认的事件函数计算逻辑。
 * 当用户未指定 func 时，系统根据事件类型选择默认行为。
 * 交点/接触事件基于真实的线段几何计算（含共线重叠与端点接触）。
 * ======================================================================== */

/**
 * @brief 点到线段的最近距离平方
 *
 * 把点投影到线段所在直线上，投影参数 t 钳制到 [0,1]，
 * 得到线段上最近点，返回两点距离平方。
 */
static double geodet_point_segment_dist2(double px, double py, double x1, double y1, double x2, double y2) {
    double dx = x2 - x1;
    double dy = y2 - y1;
    double len2 = dx * dx + dy * dy;
    double t = 0.0;
    if (len2 > 0.0) {
        t = ((px - x1) * dx + (py - y1) * dy) / len2;
        if (t < 0.0)
            t = 0.0;
        else if (t > 1.0)
            t = 1.0;
    }
    double qx = x1 + t * dx;
    double qy = y1 + t * dy;
    double ex = px - qx;
    double ey = py - qy;
    return ex * ex + ey * ey;
}

/**
 * @brief 两条线段的最近距离平方
 *
 * 先做精确的相交判定（含共线重叠与端点接触，geo_segments_intersect）；
 * 相交则最短距离为 0。否则两线段间的最短距离必然出现在
 * 某条线段端点到另一条线段的垂足处，取 4 个端点-线段距离的最小值。
 */
static double geodet_segment_segment_dist2(double ax1, double ay1, double ax2, double ay2, double bx1, double by1,
                                           double bx2, double by2) {
    if (geo_segments_intersect(ax1, ay1, ax2, ay2, bx1, by1, bx2, by2))
        return 0.0;
    double d = geodet_point_segment_dist2(ax1, ay1, bx1, by1, bx2, by2);
    double d2 = geodet_point_segment_dist2(ax2, ay2, bx1, by1, bx2, by2);
    if (d2 < d)
        d = d2;
    d2 = geodet_point_segment_dist2(bx1, by1, ax1, ay1, ax2, ay2);
    if (d2 < d)
        d = d2;
    d2 = geodet_point_segment_dist2(bx2, by2, ax1, ay1, ax2, ay2);
    if (d2 < d)
        d = d2;
    return d;
}

/**
 * @brief 交点事件：检测两条线段的最近距离是否为零
 *
 * param 约定：线段 A = (param[0],param[1])->(param[2],param[3])，
 *             线段 B = (param[4],param[5])->(param[6],param[7])，需 dim>=8。
 * g = 两线段最近距离平方；相交（含共线重叠、端点接触）时 g=0。
 * dim 不足时回退为两点的距离平方（保留原调用约定）。
 */
static int geodet_check_intersection(double t, const double *param, int dim, double *g, lvEventDetector *detector) {
    lv_UNUSED(t);
    lv_UNUSED(detector);
    if (dim < 8) {
        if (dim < 4) {
            *g = 1.0;
            return 0;
        }
        double dx = param[0] - param[2];
        double dy = param[1] - param[3];
        *g = dx * dx + dy * dy;
        return 0;
    }
    *g = geodet_segment_segment_dist2(param[0], param[1], param[2], param[3], param[4], param[5], param[6], param[7]);
    return 0;
}

/**
 * @brief 接触事件：检测点到线段是否进入阈值范围
 *
 * param 约定：点 P = (param[0],param[1])，线段 = (param[2],param[3])->(param[4],param[5])，
 *             需 dim>=6。g = 点到线段距离平方 - threshold^2。
 * dim 不足时回退为两点的距离平方（保留原调用约定）。
 */
static int geodet_check_contact(double t, const double *param, int dim, double *g, lvEventDetector *detector) {
    lv_UNUSED(t);
    lv_UNUSED(detector);
    double threshold = 1e-3;
    if (dim < 6) {
        if (dim < 4) {
            *g = 1.0;
            return 0;
        }
        double dx = param[0] - param[2];
        double dy = param[1] - param[3];
        *g = dx * dx + dy * dy - threshold * threshold;
        return 0;
    }
    *g = geodet_point_segment_dist2(param[0], param[1], param[2], param[3], param[4], param[5]) - threshold * threshold;
    return 0;
}

/**
 * @brief 穿越事件：检测 y 坐标越过零线
 *
 * g = param[y_index]，使用 param[1] 为 y 坐标
 */
static int geodet_check_crossing(double t, const double *param, int dim, double *g, lvEventDetector *detector) {
    lv_UNUSED(t);
    lv_UNUSED(detector);
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
static int geodet_check_threshold(double t, const double *param, int dim, double *g, lvEventDetector *detector) {
    lv_UNUSED(t);
    lv_UNUSED(detector);
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
static int geodet_check_periodic(double t, const double *param, int dim, double *g, lvEventDetector *detector) {
    lv_UNUSED(param);
    lv_UNUSED(dim);
    lv_UNUSED(detector);
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
static int geodet_root_bisection(lvEventDetector *detector, int event_id, const double *param_a, const double *param_b,
                                 int dim, double a, double b, double ga, double gb, double *root) {
    lv_UNUSED(param_a);
    lv_UNUSED(param_b);

    int event_idx = geodet_find_event_index(detector, event_id);
    if (event_idx < 0) {
        lv_RETURN_ERROR(lv_ERROR_NOT_FOUND, "geodet_root_illinois: event_id %d not found", event_id);
    }

    double tol = detector->root_tol;
    int max_iter = detector->max_root_iters;

    double fa = ga;
    double fb = gb;
    double mid, fmid;
    double fa_initial = fabs(fa); /* 用于发散检测的参考值 */

    for (int iter = 0; iter < max_iter; ++iter) {
        mid = 0.5 * (a + b);
        if (fabs(b - a) < tol) {
            *root = mid;
            return 0;
        }

        int ret = geodet_eval_event_func(detector, event_idx, mid, param_b, dim, &fmid);
        if (ret != 0) {
            lv_RETURN_ERROR(lv_ERROR_INTERNAL, "geodet_root_bisection: eval_event_func failed at mid=%.17g", mid);
        }

        /* 发散检测：如果 |fmid| 比初始函数值大 1e6 倍，
         * 说明区间内可能存在极点而非根。终止搜索。 */
        if (fa_initial > 0.0 && fabs(fmid) > fa_initial * 1e6) {
            lv_RETURN_ERROR(lv_ERROR_INTERNAL, "geodet_root_bisection: divergence detected, mid=%.17g fmid=%.17g", mid, fmid);
        }

        if (fabs(fmid) < lv_EPSILON_NUMERIC_COMPARE) {
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
    lv_RETURN_ERROR(lv_ERROR_INTERNAL, "geodet_root_bisection: failed to converge in %d iterations", max_iter);
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
static int geodet_root_illinois(lvEventDetector *detector, int event_id, const double *param_a, const double *param_b,
                                int dim, double a, double b, double ga, double gb, double *root) {
    int event_idx = geodet_find_event_index(detector, event_id);
    if (event_idx < 0) {
        lv_RETURN_ERROR(lv_ERROR_NOT_FOUND, "geodet_root_illinois: event_id %d not found", event_id);
    }

    double tol = detector->root_tol;
    int max_iter = detector->max_root_iters;

    double x_l = a, f_l = ga;
    double x_r = b, f_r = gb;
    int stall_counter = 0;          /* 一端被卡住的计数器 */
    double f_l_initial = fabs(f_l); /* 用于发散检测的参考值 */

    for (int iter = 0; iter < max_iter; ++iter) {
        /* 线性插值 */
        double denom = f_r - f_l;
        if (fabs(denom) < lv_EPSILON_DOUBLE) {
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
        int ret = geodet_eval_event_func(detector, event_idx, x_new, param_b, dim, &f_new);
        if (ret != 0) {
            lv_RETURN_ERROR(lv_ERROR_INTERNAL, "geodet_root_illinois: eval_event_func failed at x_new=%.17g", x_new);
        }

        if (f_new == 0.0) {
            *root = x_new;
            return 0;
        }

        /* 发散检测：如果 |f_new| 比初始函数值大 1e6 倍，
         * 说明区间内可能存在极点而非根。终止搜索。 */
        if (f_l_initial > 0.0 && fabs(f_new) > f_l_initial * 1e6) {
            lv_RETURN_ERROR(lv_ERROR_INTERNAL, "geodet_root_illinois: divergence detected, x_new=%.17g f_new=%.17g", x_new, f_new);
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
            ret = geodet_eval_event_func(detector, event_idx, x_mid, param_b, dim, &f_new);
            if (ret != 0) {
                lv_RETURN_ERROR(lv_ERROR_INTERNAL, "geodet_root_illinois: eval_event_func failed at x_mid=%.17g", x_mid);
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
    lv_RETURN_ERROR(lv_ERROR_INTERNAL, "geodet_root_illinois: failed to converge in %d iterations", max_iter);
}

/* ========================================================================
 * Brent 法求根 + 符号变化检测
 *
 * geo_event_root_brent / geo_event_check_sign 由头文件声明为公共辅助函数，
 * 此前仅有声明与调用点而无定义（任何链接到本文件的程序在调用路径上
 * 会触发未定义引用）。此处补齐完整实现：
 *   - geo_event_check_sign：判定 [g_prev, g_curr] 是否发生满足方向条件的穿越；
 *   - geo_event_root_brent：标准 Brent 逆二次插值求根（Numerical Recipes zbrent
 *     风格），风格与同文件 Illinois/二分实现保持一致（event_idx 定位、
 *     经 geodet_eval_event_func 求值、发散检测、tol/max_iter 驱动）。
 * ======================================================================== */

/**
 * @brief 判定事件函数值在 [g_prev, g_curr] 上是否发生满足方向条件的符号变化
 *
 * direction 约定（与 SUNDIALS Rootfinding 一致）：
 *   - direction > 0：仅检测从负到正的上升穿越（g_prev <= 0 且 g_curr >= 0）；
 *   - direction < 0：仅检测从正到负的下降穿越（g_prev >= 0 且 g_curr <= 0）；
 *   - direction == 0：任意符号变化均触发。
 * 端点恰好为零（过零）也视为一次有效穿越。
 *
 * @return 满足方向条件的符号变化返回 1，否则返回 0
 */
int geo_event_check_sign(double g_prev, double g_curr, int direction) {
    bool rising = (g_prev < 0.0 && g_curr > 0.0) || (g_prev == 0.0 && g_curr > 0.0) ||
                  (g_prev < 0.0 && g_curr == 0.0);
    bool falling = (g_prev > 0.0 && g_curr < 0.0) || (g_prev == 0.0 && g_curr < 0.0) ||
                   (g_prev > 0.0 && g_curr == 0.0);
    bool any_change = rising || falling;

    if (direction > 0)
        return rising ? 1 : 0;
    if (direction < 0)
        return falling ? 1 : 0;
    return any_change ? 1 : 0;
}

/**
 * @brief Brent 逆二次插值求根
 *
 * 在区间 [a, b] 上求事件函数 g 的根（调用方已通过 geo_event_check_sign
 * 确认区间端点异号）。混合二分与逆二次插值，收敛速度优于二分法，
 * 且保证不脱离包含根的区间。
 *
 * @param[in]  detector   事件检测器
 * @param[in]  event_id   事件 ID
 * @param[in]  param_a    左端点参数向量（本实现未用，保留签名一致性）
 * @param[in]  param_b    右端点参数向量（求值时按 b 处参数计算，与同文件其他求根方法一致）
 * @param[in]  dim        参数维度
 * @param[in]  a,b        区间端点
 * @param[in]  ga,gb      端点处事件函数值
 * @param[in]  tol        区间宽度收敛容差
 * @param[in]  max_iter   最大迭代次数
 * @param[out] root       求得的根
 * @return 收敛返回 0，未收敛/发散返回错误码
 */
int geo_event_root_brent(lvEventDetector *detector, int event_id, const double *param_a, const double *param_b,
                         int dim, double a, double b, double ga, double gb, double tol, int max_iter,
                         double *root) {
    lv_UNUSED(param_a);
    if (!detector || !root)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "geo_event_root_brent: NULL parameter");

    int event_idx = geodet_find_event_index(detector, event_id);
    if (event_idx < 0)
        lv_RETURN_ERROR(lv_ERROR_NOT_FOUND, "geo_event_root_brent: event_id %d not found", event_id);

    double fa = ga;
    double fb = gb;
    /* 保证 |fa| >= |fb|：交换端点使 b 为更接近根的一端 */
    if (fabs(fa) < fabs(fb)) {
        double tmp = a;
        a = b;
        b = tmp;
        tmp = fa;
        fa = fb;
        fb = tmp;
    }
    double c = a;
    double fc = fa;
    double d = b - a;
    double e = b - a;
    double f_initial = fabs(fa); /* 发散检测参考值 */

    for (int iter = 0; iter < max_iter; ++iter) {
        /* 重新选取最优点：使 b 为当前最小函数值端点 */
        if (fabs(fc) < fabs(fb)) {
            a = b;
            b = c;
            c = a;
            fa = fb;
            fb = fc;
            fc = fa;
        }
        double tol1 = 2.0 * lv_EPSILON_DOUBLE * fabs(b) + 0.5 * tol;
        double xm = 0.5 * (c - b);
        if (fabs(xm) <= tol1 || fb == 0.0) {
            *root = b;
            return 0;
        }
        if (fabs(e) >= tol1 && fabs(fa) > fabs(fb)) {
            /* 逆二次插值 */
            double s = fb / fa;
            double p, q;
            if (a == c) {
                p = 2.0 * xm * s;
                q = 1.0 - s;
            } else {
                double r = fb / fc;
                q = fa / fc;
                p = s * (2.0 * xm * q * (q - r) - (b - a) * (r - 1.0));
                q = (q - 1.0) * (r - 1.0) * (s - 1.0);
            }
            if (p > 0.0)
                q = -q;
            p = fabs(p);
            double min1 = 3.0 * xm * q - fabs(tol1 * q);
            double min2 = fabs(e * q);
            if (2.0 * p < (min1 < min2 ? min1 : min2)) {
                e = d;
                d = p / q;
            } else {
                d = xm;
                e = d;
            }
        } else {
            /* 二分步 */
            d = xm;
            e = d;
        }
        a = b;
        fa = fb;
        if (fabs(d) > tol1)
            b += d;
        else
            b += (xm > 0.0 ? fabs(tol1) : -fabs(tol1));

        double fnew;
        int ret = geodet_eval_event_func(detector, event_idx, b, param_b, dim, &fnew);
        if (ret != 0)
            lv_RETURN_ERROR(lv_ERROR_INTERNAL, "geo_event_root_brent: eval_event_func failed at x=%.17g", b);

        /* 发散检测：|fnew| 超过初始参考值 1e6 倍视为极点而非根 */
        if (f_initial > 0.0 && fabs(fnew) > f_initial * 1e6)
            lv_RETURN_ERROR(lv_ERROR_INTERNAL, "geo_event_root_brent: divergence detected, x=%.17g f=%.17g", b, fnew);

        fb = fnew;
        if ((fb > 0.0 && fc > 0.0) || (fb < 0.0 && fc < 0.0)) {
            c = a;
            fc = fa;
            e = d = b - a;
        }
    }

    *root = b;
    lv_RETURN_ERROR(lv_ERROR_INTERNAL, "geo_event_root_brent: failed to converge in %d iterations", max_iter);
}

/* ========================================================================
 * 生命周期管理
 * ======================================================================== */

/**
 * @brief 创建几何事件检测器
 */
lvEventDetector *geo_event_detector_create(void) {
    lvEventDetector *detector = lv_malloc(sizeof(lvEventDetector));
    lv_CHECK_ALLOC(detector, NULL);

    memset(detector, 0, sizeof(lvEventDetector));

    detector->num_events = 0;
    detector->root_method = lv_ROOTFIND_BRENT;
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
void geo_event_detector_destroy(lvEventDetector *detector) {
    if (!detector) {
        return;
    }

    /* 从事件注册表移除该检测器的所有条目（防止残留悬垂 value） */
    geo_event_registry_ensure();
    char prefix[64];
    snprintf(prefix, sizeof(prefix), "%p:", (const void *) detector);
    lv_registry_remove_prefix(&g_geo_event_registry, prefix);

    lv_free((void **) &detector);
}

/* ========================================================================
 * 事件注册
 * ======================================================================== */

/**
 * @brief 注册一个几何事件
 *
 * @note 已建未用/预留：当前无业务调用者（仅 test/c/test_registry.c 测试使用），
 *       保留供事件检测系统接入。
 */
int geo_event_register(lvEventDetector *detector, int event_id, lvEventType type, lvEventFunc func, int direction,
                       bool terminal, lvEventCallback callback) {
    lv_CHECK_NULL(detector, -1);

    if (detector->num_events >= GEO_EVENT_MAX_EVENTS) {
        lv_RETURN_ERROR(lv_ERROR_OVERFLOW, "事件注册已满，最大%d个事件", GEO_EVENT_MAX_EVENTS);
    }

    geo_event_registry_ensure();

    /* 检查 ID 是否重复（委托注册表 strcmp 查重） */
    char regkey[GEODET_REGKEY_MAX];
    geodet_build_key(detector, event_id, regkey, sizeof(regkey));
    if (lv_registry_find(&g_geo_event_registry, regkey) >= 0) {
        lv_RETURN_ERROR(lv_ERROR_ALREADY_EXISTS, "事件ID=%d已存在", event_id);
    }

    /* 若未提供自定义事件函数，使用类型默认函数 */
    if (!func) {
        static const lvEventFunc s_default_event_funcs[] = {
            [lv_EVENT_INTERSECTION] = geodet_check_intersection,
            [lv_EVENT_CONTACT] = geodet_check_contact,
            [lv_EVENT_CROSSING] = geodet_check_crossing,
            [lv_EVENT_THRESHOLD] = geodet_check_threshold,
            [lv_EVENT_PERIODIC] = geodet_check_periodic,
        };
        if ((int) type >= 0 && (size_t) type < lv_ARRAY_SIZE(s_default_event_funcs) && s_default_event_funcs[(int) type]) {
            func = s_default_event_funcs[(int) type];
        } else {
            lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "自定义事件lv_EVENT_CUSTOM必须提供func参数");
        }
    }

    /* 尾部追加到注册表（value = 事件描述结构体指针，指向 events[idx]） */
    int idx = detector->num_events;
    if (!lv_registry_put(&g_geo_event_registry, regkey, &detector->events[idx])) {
        lv_RETURN_ERROR(lv_ERROR_RESOURCE_EXHAUSTED, "事件注册失败：注册表容量不足");
    }

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
lvEventResult geo_event_detect(lvEventDetector *detector, double t_prev, const double *param_prev, double t_curr,
                               const double *param_curr, int dim, int *event_id, double *t_event) {
    lv_CHECK_NULL(detector, lv_EVENT_RESULT_ERROR);
    lv_CHECK_NULL(param_prev, lv_EVENT_RESULT_ERROR);
    lv_CHECK_NULL(param_curr, lv_EVENT_RESULT_ERROR);
    lv_CHECK_NULL(event_id, lv_EVENT_RESULT_ERROR);
    lv_CHECK_NULL(t_event, lv_EVENT_RESULT_ERROR);

    *event_id = -1;
    *t_event = t_curr;

    if (detector->num_events == 0) {
        return lv_EVENT_RESULT_NONE;
    }

    geo_event_registry_ensure();

    /* 遍历所有注册事件（通过注册表条目，仅处理当前检测器） */
    char geodet_prefix[64];
    snprintf(geodet_prefix, sizeof(geodet_prefix), "%p:", (const void *) detector);
    size_t geodet_prefix_len = strlen(geodet_prefix);

    int geo_reg_count = lv_registry_count(&g_geo_event_registry);
    for (int i = 0; i < geo_reg_count; ++i) {
        const char *reg_name = NULL;
        void *reg_value = NULL;
        if (!lv_registry_get_at(&g_geo_event_registry, i, &reg_name, &reg_value)) {
            continue;
        }
        if (strncmp(reg_name, geodet_prefix, geodet_prefix_len) != 0) {
            continue;
        }
        lvEventEntry *evt = (lvEventEntry *) reg_value;
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
        ret = geo_event_root_locate(detector, evt->event_id, param_prev, param_curr, dim, t_prev, t_curr, g_prev_val,
                                    g_curr_val, &event_time);
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

        return (ret == 0) ? lv_EVENT_RESULT_DETECTED : lv_EVENT_RESULT_WARNING;
    }

    return lv_EVENT_RESULT_NONE;
}

/* ========================================================================
 * 求根定位调度 — 函数指针查找表（替代 switch-case 分派）
 * ======================================================================== */

/** @brief 求根处理器函数指针类型 */
typedef int (*RootFindHandler)(lvEventDetector *detector, int event_id, const double *param_a, const double *param_b,
                               int dim, double a, double b, double ga, double gb, double *root);

/** @brief Brent 法 wrapper（适配多出的 tol/max_iter 参数） */
static int rootfind_brent_wrapper(lvEventDetector *detector, int event_id, const double *param_a,
                                  const double *param_b, int dim, double a, double b, double ga, double gb,
                                  double *root) {
    return geo_event_root_brent(detector, event_id, param_a, param_b, dim, a, b, ga, gb, detector->root_tol,
                                detector->max_root_iters, root);
}

/** @brief Illinois 法 wrapper */
static int rootfind_illinois_wrapper(lvEventDetector *detector, int event_id, const double *param_a,
                                     const double *param_b, int dim, double a, double b, double ga, double gb,
                                     double *root) {
    return geodet_root_illinois(detector, event_id, param_a, param_b, dim, a, b, ga, gb, root);
}

/** @brief 二分法 wrapper */
static int rootfind_bisection_wrapper(lvEventDetector *detector, int event_id, const double *param_a,
                                       const double *param_b, int dim, double a, double b, double ga, double gb,
                                       double *root) {
    return geodet_root_bisection(detector, event_id, param_a, param_b, dim, a, b, ga, gb, root);
}

/** @brief 求根方法查找表 */
static const RootFindHandler kRootFindHandlers[] = {
    [lv_ROOTFIND_BRENT]     = rootfind_brent_wrapper,
    [lv_ROOTFIND_ILLINOIS]  = rootfind_illinois_wrapper,
    [lv_ROOTFIND_BISECTION] = rootfind_bisection_wrapper,
};

/**
 * @brief 在区间 [a, b] 内精确求根定位
 *
 * 根据检测器配置的 root_method 选择合适的求根算法。
 */
int geo_event_root_locate(lvEventDetector *detector, int event_id, const double *param_a, const double *param_b,
                          int dim, double a, double b, double ga, double gb, double *root) {
    lv_CHECK_NULL(detector, -1);
    lv_CHECK_NULL(root, -1);

    /* 通过函数指针查找表调度 */
    if ((unsigned)detector->root_method < sizeof(kRootFindHandlers)/sizeof(kRootFindHandlers[0]))
        return kRootFindHandlers[detector->root_method](detector, event_id, param_a, param_b, dim, a, b, ga, gb, root);

    /* 默认回退 Brent 法 */
    return geo_event_root_brent(detector, event_id, param_a, param_b, dim, a, b, ga, gb, detector->root_tol,
                                detector->max_root_iters, root);
}
