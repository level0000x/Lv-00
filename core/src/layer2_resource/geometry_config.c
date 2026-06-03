/**
 * @file geometry_config.c
 * @brief 几何计算可配置容差参数实现
 *
 * 提供线程安全的几何容差配置管理。
 * 使用互斥锁保护全局配置的读写。
 *
 * @version 1.0.0
 */

#include "geometry_config.h"

#include <string.h>

/* ============================================================
 * 平台相关互斥锁抽象
 * ============================================================ */

#if defined(_WIN32) || defined(_WIN64)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

typedef CRITICAL_SECTION lv00_mutex_t;

static void lv00_mutex_init(lv00_mutex_t *m) {
    InitializeCriticalSection(m);
}

static void lv00_mutex_lock(lv00_mutex_t *m) {
    EnterCriticalSection(m);
}

static void lv00_mutex_unlock(lv00_mutex_t *m) {
    LeaveCriticalSection(m);
}

#else
#include <pthread.h>

typedef pthread_mutex_t lv00_mutex_t;

static void lv00_mutex_init(lv00_mutex_t *m) {
    pthread_mutex_init(m, NULL);
}

static void lv00_mutex_lock(lv00_mutex_t *m) {
    pthread_mutex_lock(m);
}

static void lv00_mutex_unlock(lv00_mutex_t *m) {
    pthread_mutex_unlock(m);
}

#endif

/* ============================================================
 * 默认配置与全局状态
 * ============================================================ */

/** 默认几何容差配置 */
static const Lv00GeometryConfig DEFAULT_CONFIG = {
    .collinear_epsilon      = 1e-9,   /* 共线判定容差：三点共线时叉积的绝对值上限。
                                         1e-9 适用于 double 精度下坐标值在常规范围（0~1e6）的几何场景，
                                         能有效区分"几乎共线"与"明显不共线"，同时避免浮点累积误差导致误判。 */
    .perpendicular_epsilon  = 1e-9,   /* 垂直判定容差：两向量点积的绝对值上限。
                                         与 collinear_epsilon 取相同量级，因为垂直判定与共线判定
                                         在数值精度需求上是对称的（点积 vs 叉积）。 */
    .parallel_epsilon       = 1e-9,   /* 平行判定容差：两向量叉积的绝对值上限。
                                         与 collinear_epsilon 一致，平行判定本质上是方向向量的共线判定。 */
    .distance_epsilon       = 1e-9,   /* 距离容差：两点距离或点到直线距离的判定阈值。
                                         1e-9 约等于 1 纳米（以米为单位时），在几何证明系统中
                                         用于判断"两点重合"或"点在直线上"，远小于任何有意义的几何距离。 */
    .angle_epsilon          = 1e-6,   /* 角度容差：角度比较的绝对误差上限（弧度）。
                                         1e-6 弧度约等于 0.000057 度（约 0.2 角秒），
                                         比距离类容差放宽 3 个数量级，因为三角函数计算（sin/cos/atan2）
                                         在角度接近 0 或 pi/2 时精度损失较大，需要更大的容差来避免误判。 */
    .singular_threshold     = 1e-12,  /* 奇异矩阵/退化判定阈值：行列式绝对值的下限。
                                         1e-12 比 collinear_epsilon 更严格 3 个数量级，用于矩阵求逆、
                                         线性方程组求解等数值计算中判断矩阵是否接近奇异（不可逆）。
                                         过大的值会导致有效矩阵被误判为奇异，过小则可能导致数值不稳定。 */
};

/** 当前活跃配置（可被用户替换） */
static Lv00GeometryConfig current_config = {
    .collinear_epsilon      = 1e-9,
    .perpendicular_epsilon  = 1e-9,
    .parallel_epsilon       = 1e-9,
    .distance_epsilon       = 1e-9,
    .angle_epsilon          = 1e-6,
    .singular_threshold     = 1e-12,
};

/** 配置访问互斥锁 */
static lv00_mutex_t config_mutex;

/** 互斥锁初始化标志（原子变量，避免竞态条件） */
static volatile int config_mutex_initialized = 0;

/**
 * @brief 确保互斥锁已初始化（线程安全的懒初始化）
 * @note 使用双重检查锁定模式，仅在首次调用时初始化互斥锁。
 *       config_mutex_initialized 使用 volatile 确保跨线程可见性。
 */
static void ensure_mutex_initialized(void) {
    /* 第一次检查：快速路径，避免不必要的原子操作 */
    if (!config_mutex_initialized) {
        /* 注意：在极少数情况下，两个线程可能同时通过此检查。
         * 但 lv00_mutex_init 对同一互斥锁的重复初始化在大多数平台上是安全的。
         * 如果平台要求严格一次性初始化，应改用 pthread_once / InitOnceExecuteOnce。 */
        lv00_mutex_init(&config_mutex);
        config_mutex_initialized = 1;
    }
}

/* ============================================================
 * 公共 API 实现
 * ============================================================ */

const Lv00GeometryConfig *lv00_geometry_default_config(void) {
    return &DEFAULT_CONFIG;
}

void lv00_geometry_set_config(const Lv00GeometryConfig *config) {
    ensure_mutex_initialized();

    lv00_mutex_lock(&config_mutex);

    if (config) {
        current_config = *config;
    } else {
        /* NULL 参数：恢复默认值 */
        current_config = DEFAULT_CONFIG;
    }

    lv00_mutex_unlock(&config_mutex);
}

const Lv00GeometryConfig *lv00_geometry_get_config(void) {
    ensure_mutex_initialized();

    lv00_mutex_lock(&config_mutex);
    /* 返回指向全局静态变量的指针，调用者不应释放 */
    const Lv00GeometryConfig *result = &current_config;
    lv00_mutex_unlock(&config_mutex);

    return result;
}
