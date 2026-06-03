/**
 * @file geo_event_detect.h
 * @brief 几何事件检测 —— 借鉴 SUNDIALS Rootfinding 事件检测机制
 *
 * @details 设计借鉴来源：
 *          - SUNDIALS CVODE/CVODES (github.com/LLNL/sundials)
 *            · RootsFinding 模块：在 ODE 演化中检测事件函数过零
 *            · 与演化引擎松耦合，通过注册回调函数工作
 *            · 支持多种求根方法（Brent / Illinois / Bisection）
 *
 *          设计目标：
 *          - 与 geom_evol.h 演化引擎松耦合（事件注册模式）
 *          - 支持多种几何事件类型（交点/接触/穿越/阈值/周期）
 *          - 精确事件定位（区间二分 + 高精度求根）
 *          - 事件检测回调链（多个检测器可同时注册）
 *
 * @author Lv-00 Project
 * @version v3.3.0
 * @date 2026-05-24
 */

#ifndef LV00_GEO_EVENT_DETECT_H
#define LV00_GEO_EVENT_DETECT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "lv00_numeric.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 常量定义 ==================== */

/** 最大同时注册的事件数量 */
#define GEO_EVENT_MAX_EVENTS 32

/** 最大检测器嵌套深度（事件触发新检测时使用） */
#define GEO_EVENT_MAX_DEPTH 8

/** 事件定位最大迭代次数 */
#define GEO_EVENT_MAX_ROOT_ITERS 100

/** 事件定位默认容差 */
#define GEO_EVENT_DEFAULT_TOL 1e-12

/** 事件方向过滤掩码 */
#define GEO_EVENT_DIR_NEGATIVE -1 /**< 仅检测负向穿越（从正到负） */
#define GEO_EVENT_DIR_POSITIVE 1  /**< 仅检测正向穿越（从负到正） */
#define GEO_EVENT_DIR_BOTH 2      /**< 检测双向穿越 */
#define GEO_EVENT_DIR_ANY 3       /**< 检测任意符号变化（含触碰） */

/* ==================== 类型定义 ==================== */

/**
 * @brief 几何事件类型 —— 借鉴 SUNDIALS 根检测类型
 */
typedef enum {
    LV00_EVENT_INTERSECTION = 0, /**< 交点事件：两条曲线相交 */
    LV00_EVENT_CONTACT = 1,      /**< 接触事件：几何体发生接触 */
    LV00_EVENT_CROSSING = 2,     /**< 穿越事件：点/线穿越某个边界 */
    LV00_EVENT_THRESHOLD = 3,    /**< 阈值事件：某个量超过阈值 */
    LV00_EVENT_PERIODIC = 4,     /**< 周期性事件：在固定时间点触发 */
    LV00_EVENT_CUSTOM = 99       /**< 自定义事件 */
} Lv00EventType;

/**
 * @brief 求根方法 —— 借鉴 SUNDIALS 求根算法
 *
 * SUNDIALS 默认使用 Illinois 算法（改进的试位法），
 * 也支持 Brent 和二分法作为备选。
 */
typedef enum {
    LV00_ROOTFIND_BRENT = 0,    /**< Brent 法（结合二分、割线和逆二次插值，默认推荐） */
    LV00_ROOTFIND_ILLINOIS = 1, /**< Illinois 法（改进试位法，SUNDIALS 默认） */
    LV00_ROOTFIND_BISECTION = 2 /**< 二分法（最稳健，但收敛较慢，作为备选） */
} Lv00RootfindMethod;

/**
 * @brief 事件检测结果
 */
typedef enum {
    LV00_EVENT_RESULT_NONE = 0,     /**< 未检测到事件 */
    LV00_EVENT_RESULT_DETECTED = 1, /**< 检测到事件，已精确定位 */
    LV00_EVENT_RESULT_WARNING = 2,  /**< 检测到事件但定位精度不足 */
    LV00_EVENT_RESULT_ERROR = 3     /**< 检测过程出错 */
} Lv00EventResult;

/* ==================== 回调类型 ==================== */

/** @cond 前向声明 */
typedef struct Lv00EventDetector Lv00EventDetector;
/** @endcond */

/**
 * @brief 事件函数 —— 计算几何事件检测量
 *
 * 借鉴 SUNDIALS 的 root function 设计。
 * 在演化参数 t 处计算事件函数值 g(t, param)。
 * 当 g 在相邻两步之间改变符号（或满足特定方向条件）时，
 * 表示事件可能发生，需要精确求根定位。
 *
 * @param[in]  t      当前参数（时间或弧长）
 * @param[in]  param  当前几何参数向量
 * @param[in]  dim    参数维度
 * @param[out] g      输出：事件函数值
 *                    例如：distance(curve1(t), curve2(t)) - 用于检测交点
 * @param[in]  detector 事件检测器（可访问用户上下文）
 * @return 成功返回 0，失败返回非零
 */
typedef int (*Lv00EventFunc)(double t, const double *param, int dim, double *g, Lv00EventDetector *detector);

/**
 * @brief 事件处理回调
 *
 * 当事件被检测到并精确定位后调用。
 *
 * @param[in] detector  事件检测器
 * @param[in] event_id  触发的事件 ID
 * @param[in] t_event   事件发生的精确参数值
 * @param[in] param     事件发生时的参数向量
 */
typedef void (*Lv00EventCallback)(Lv00EventDetector *detector, int event_id, double t_event, const double *param);

/* ==================== 事件条目 ==================== */

/**
 * @brief 单个事件的定义
 */
typedef struct Lv00EventEntry {
    int event_id;               /**< 事件唯一 ID（在检测器内唯一） */
    Lv00EventType type;         /**< 事件类型 */
    Lv00EventFunc func;         /**< 事件函数 g(t, param) */
    int direction;              /**< 方向过滤：
                                       GEO_EVENT_DIR_NEGATIVE / POSITIVE / BOTH / ANY */
    bool enabled;               /**< 是否活跃 */
    bool terminal;              /**< 是否为终止事件（触发后停止演化） */
    Lv00EventCallback callback; /**< 事件处理回调（可为 NULL） */
} Lv00EventEntry;

/* ==================== 事件检测器主结构 ==================== */

/**
 * @brief 几何事件检测器 —— 借鉴 SUNDIALS RootsFinding 模块
 *
 * 独立于演化引擎，通过事件注册机制工作。
 * 演化引擎在每一步后调用 geo_event_detect() 检查是否有事件发生。
 */
struct Lv00EventDetector {
    /* ── 注册的事件 ── */
    Lv00EventEntry events[GEO_EVENT_MAX_EVENTS]; /**< 事件数组 */
    int num_events;                              /**< 已注册事件数量 */

    /* ── 求根配置 ── */
    Lv00RootfindMethod root_method; /**< 使用的求根方法 */
    double root_tol;                /**< 求根容差 */
    int max_root_iters;             /**< 求根最大迭代次数 */

    /* ── 上一步状态（用于检测符号变化）── */
    double t_prev;                       /**< 上一步的参数值 */
    double g_prev[GEO_EVENT_MAX_EVENTS]; /**< 上一步的事件函数值 */

    /* ── 用户上下文 ── */
    void *user_data; /**< 用户自定义数据（透传给事件函数/回调） */
};

/* ==================== API 函数 ==================== */

/**
 * @brief 创建几何事件检测器
 *
 * @return 新分配的事件检测器，失败返回 NULL
 */
Lv00EventDetector *geo_event_detector_create(void);

/**
 * @brief 销毁事件检测器并释放所有关联资源
 *
 * @param[in,out] detector  要销毁的检测器（设为 NULL 是安全的）
 */
void geo_event_detector_destroy(Lv00EventDetector *detector);

/**
 * @brief 注册一个几何事件
 *
 * 在检测器中添加一个事件定义。事件 ID 在检测器内必须唯一。
 *
 * @param[in,out] detector   事件检测器
 * @param[in]     event_id   事件 ID（检测器内唯一）
 * @param[in]     type       事件类型
 * @param[in]     func       事件函数（必须非 NULL）
 * @param[in]     direction  方向过滤
 * @param[in]     terminal   是否为终止事件
 * @param[in]     callback   事件处理回调（可为 NULL）
 * @return 成功返回 0，失败（如事件已满或 ID 重复）返回非零
 */
int geo_event_register(Lv00EventDetector *detector, int event_id, Lv00EventType type, Lv00EventFunc func, int direction,
                       bool terminal, Lv00EventCallback callback);

/**
 * @brief 检测事件：在演化步 [t_prev, t_curr] 间检测事件
 *
 * 借鉴 SUNDIALS 的 CVodeRootInit + CVode 求根逻辑：
 * 1. 检查 t_prev 和 t_curr 处各事件函数值是否有符号变化
 * 2. 对满足方向条件的事件，通过求根方法精确定位
 * 3. 触发对应回调
 *
 * @param[in,out] detector  事件检测器
 * @param[in]     t_prev    上一步参数值
 * @param[in]     param_prev 上一步参数向量
 * @param[in]     t_curr    当前步参数值
 * @param[in]     param_curr 当前步参数向量
 * @param[in]     dim       参数向量维度
 * @param[out]    event_id  输出：触发的事件 ID（若检测到，-1 表示无事件）
 * @param[out]    t_event   输出：事件发生的精确参数值（若检测到）
 * @return 事件检测结果
 */
Lv00EventResult geo_event_detect(Lv00EventDetector *detector, double t_prev, const double *param_prev, double t_curr,
                                 const double *param_curr, int dim, int *event_id, double *t_event);

/**
 * @brief 在区间 [a, b] 内精确求根定位
 *
 * 使用配置的求根方法对事件函数 g(t) 在区间 [a, b] 内找根，
 * 即 g(root) ≈ 0。
 *
 * @param[in,out] detector  事件检测器
 * @param[in]     event_id  事件 ID
 * @param[in]     param_a   t=a 时的参数向量
 * @param[in]     param_b   t=b 时的参数向量
 * @param[in]     dim       参数维度
 * @param[in]     a         区间左端点（g(a) 已知）
 * @param[in]     b         区间右端点（g(b) 已知，与 g(a) 异号）
 * @param[in]     ga        g(a) 的值
 * @param[in]     gb        g(b) 的值
 * @param[out]    root      输出：根的精确位置
 * @return 成功返回 0，超过最大迭代返回非零
 */
int geo_event_root_locate(Lv00EventDetector *detector, int event_id, const double *param_a, const double *param_b,
                          int dim, double a, double b, double ga, double gb, double *root);

/* ==================== 辅助内联函数 ==================== */

/**
 * @brief 检查是否有符号变化并满足方向条件
 *
 * @param[in] g_prev     上一步事件函数值
 * @param[in] g_curr     当前步事件函数值
 * @param[in] direction  方向过滤条件
 * @return 满足方向条件且有符号变化返回 true
 */
static inline bool geo_event_check_sign(double g_prev, double g_curr, int direction) {
    if (g_prev == 0.0 && g_curr == 0.0)
        return false;
    /* 负向穿越（正到负） */
    if (direction == GEO_EVENT_DIR_NEGATIVE) {
        return g_prev >= 0.0 && g_curr < 0.0;
    }
    /* 正向穿越（负到正） */
    if (direction == GEO_EVENT_DIR_POSITIVE) {
        return g_prev <= 0.0 && g_curr > 0.0;
    }
    /* 双向穿越 */
    if (direction == GEO_EVENT_DIR_BOTH) {
        return (g_prev > 0.0 && g_curr < 0.0) || (g_prev < 0.0 && g_curr > 0.0);
    }
    /* 任意符号变化（含触碰零） */
    if (direction == GEO_EVENT_DIR_ANY) {
        return (g_prev > 0.0 && g_curr <= 0.0) || (g_prev < 0.0 && g_curr >= 0.0) || (g_prev != 0.0 && g_curr == 0.0);
    }
    return false;
}

/**
 * @brief 数值求根：Brent 法的简化实现
 *
 * 在区间 [a, b] 内找 g(t) 的根，假设 g(a) 和 g(b) 异号。
 *
 * @param[in]  detector  事件检测器
 * @param[in]  event_id  事件 ID
 * @param[in]  param_a   t=a 时的参数向量
 * @param[in]  param_b   t=b 时的参数向量
 * @param[in]  dim       参数维度
 * @param[in]  a         区间左端点
 * @param[in]  b         区间右端点
 * @param[in]  ga        g(a) 的值
 * @param[in]  gb        g(b) 的值
 * @param[in]  tol       容差
 * @param[in]  max_iter  最大迭代次数
 * @param[out] root      输出：根的精确位置
 * @return 实际迭代次数，超过 max_iter 返回 -1
 */
static inline int geo_event_root_brent(Lv00EventDetector *detector, int event_id, const double *param_a,
                                       const double *param_b, int dim, double a, double b, double ga, double gb,
                                       double tol, int max_iter, double *root) {
    double c = b, gc = gb;
    double d = b - a, e = d;
    double m, tol_act, p, q, r, s;

    for (int iter = 1; iter <= max_iter; ++iter) {
        if ((gb > 0.0 && gc > 0.0) || (gb < 0.0 && gc < 0.0)) {
            c = a;
            gc = ga;
            d = b - a;
            e = d;
        }
        if (fabs(gc) < fabs(gb)) {
            a = b;
            b = c;
            c = a;
            ga = gb;
            gb = gc;
            gc = ga;
        }
        tol_act = 2.0 * LV00_EPSILON * fabs(b) + 0.5 * tol;
        m = 0.5 * (c - b);

        if (fabs(m) <= tol_act || gb == 0.0) {
            *root = b;
            return iter;
        }

        if (fabs(e) < tol_act || fabs(ga) <= fabs(gb)) {
            d = m;
            e = m;
        } else {
            s = gb / ga;
            if (a == c) {
                p = 2.0 * m * s;
                q = 1.0 - s;
            } else {
                q = ga / gc;
                r = gb / gc;
                p = s * (2.0 * m * q * (q - r) - (b - a) * (r - 1.0));
                q = (q - 1.0) * (r - 1.0) * (s - 1.0);
            }
            if (p > 0.0)
                q = -q;
            else
                p = -p;
            s = e;
            e = d;
            if (2.0 * p < 3.0 * m * q - fabs(tol_act * q) && p < fabs(0.5 * s * q)) {
                d = p / q;
            } else {
                d = m;
                e = m;
            }
        }
        a = b;
        ga = gb;
        if (fabs(d) > tol_act) {
            b = b + d;
        } else {
            b = b + (m > 0.0 ? tol_act : -tol_act);
        }

        double g;
        int ret = detector->events[event_id].func(b, param_b, dim, &g, detector);
        if (ret != 0)
            return -1;
        gb = g;
    }
    *root = b;
    return -1; /* 超过最大迭代 */
}

/* ═══════════════════════════════════════════════════════════════
 * 使用示例（参考）
 * ═══════════════════════════════════════════════════════════════
 *
 * @code
 * // 定义事件函数：检测两条曲线是否相交
 * // 当 curve1(t) 和 curve2(t) 的距离为 0 时触发
 * static int intersection_event(double t, const double *param, int dim,
 *                               double *g, Lv00EventDetector *detector) {
 *     (void)dim; (void)detector;
 *     // param[0..1] = curve1.x, curve1.y, param[2..3] = curve2.x, curve2.y
 *     double dx = param[0] - param[2];
 *     double dy = param[1] - param[3];
 *     *g = sqrt(dx*dx + dy*dy);  // 距离 -> 为 0 时相交
 *     return 0;
 * }
 *
 * // 创建检测器并注册事件
 * Lv00EventDetector *det = geo_event_detector_create();
 * geo_event_register(det, 0, LV00_EVENT_INTERSECTION,
 *                    intersection_event, GEO_EVENT_DIR_NEGATIVE,
 *                    false, NULL);
 *
 * // 在演化步中检测事件
 * int evt_id;
 * double t_evt;
 * Lv00EventResult res = geo_event_detect(det,
 *     t_prev, param_prev, t_curr, param_curr, 4, &evt_id, &t_evt);
 * if (res == LV00_EVENT_RESULT_DETECTED) {
 *     printf("Event %d at t=%f\n", evt_id, t_evt);
 * }
 * geo_event_detector_destroy(det);
 * @endcode
 */

#ifdef __cplusplus
}
#endif

#endif /* LV00_GEO_EVENT_DETECT_H */
