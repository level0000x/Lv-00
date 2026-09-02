/**
 * @file lv_safety.h
 * @brief 蓝图安全宏族与泄漏检测（TEN_LAYER_OPTIMIZED_PLAN §16.2/16.3 落地）
 *
 * 提供：数值安全宏（CHECK_COORD/SAFE_DIV/validate_triangle）、深度守卫宏
 * （DEPTH_ENTER/LEAVE）、字符串安全宏（STRCPY/STRCAT，与 lv_strlcpy/
 * lv_strlcat 同族的不同形态）、引用计数宏（REFCOUNT_INIT/HEADER）、
 * 泄漏检测（snapshot/report/assert_clean，接线 lv_mem_* 全局统计）。
 */

#ifndef lv_LV_SAFETY_H
#define lv_LV_SAFETY_H

#include <inttypes.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "config.h" /* lv_PI 等常量权威源（不依赖，仅保持一致性 include） */
#include "lv_api_spec.h" /* lv_PUBLIC_API（K59） */

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 数值安全常量与宏（§16.2.2）
 * ============================================================ */

#ifndef lv_COORD_MAX
#define lv_COORD_MAX 1e15
#endif
#ifndef lv_COORD_MIN
#define lv_COORD_MIN 1e-10
#endif
#ifndef lv_DISTANCE_EPSILON
#define lv_DISTANCE_EPSILON 1e-12
#endif

/** @brief 安全坐标检查（蓝图 lv_CHECK_COORD）：NaN/Inf/超界 → return retval */
#define lv_CHECK_COORD(x, retval)         \
    do {                                  \
        double _v = (double) (x);         \
        if (isnan(_v) || isinf(_v) || fabs(_v) > lv_COORD_MAX) \
            return (retval);              \
    } while (0)

/** @brief 安全除法（蓝图 lv_SAFE_DIV）：分母过小返回 default */
#define lv_SAFE_DIV(num, den, dflt) \
    (fabs((double) (den)) < lv_DISTANCE_EPSILON ? (dflt) : ((num) / (den)))

/** @brief 三角形三边有效性验证（蓝图 lv_validate_triangle） */
static inline bool lv_validate_triangle(double a, double b, double c) {
    if (a <= 0.0 || b <= 0.0 || c <= 0.0)
        return false;
    if (a + b <= c + lv_DISTANCE_EPSILON)
        return false;
    if (a + c <= b + lv_DISTANCE_EPSILON)
        return false;
    if (b + c <= a + lv_DISTANCE_EPSILON)
        return false;
    return true;
}

/* ============================================================
 * 深度守卫（§16.2.3）
 * ============================================================ */

/** @brief 深度守卫结构（蓝图 lvDepthGuard；字段以 field##_depth 命名约定使用） */
typedef struct {
    int propagation_depth;
    int proof_search_depth;
    uint64_t total_steps;
} lvDepthGuard;

/** @brief 进入递归/搜索深度（蓝图 lv_DEPTH_ENTER）：超限 return retval */
#define lv_DEPTH_ENTER(guard, field, max_depth, retval) \
    do {                                                \
        (guard)->field##_depth++;                       \
        if ((guard)->field##_depth > (max_depth))       \
            return (retval);                            \
        (guard)->total_steps++;                         \
    } while (0)

/** @brief 离开递归/搜索深度（蓝图 lv_DEPTH_LEAVE） */
#define lv_DEPTH_LEAVE(guard, field) \
    do {                             \
        (guard)->field##_depth--;    \
    } while (0)

/* ============================================================
 * 安全字符串（§16.3.1；与 lv_strlcpy/lv_strlcat 同语义的宏形态）
 * ============================================================ */

/** @brief 安全 strcpy（蓝图 lv_STRCPY）：始终 NUL 终止 */
#define lv_STRCPY(dst, size, src)               \
    do {                                        \
        if ((size) > 0) {                       \
            strncpy((dst), (src), (size) - 1);  \
            (dst)[(size) - 1] = '\0';           \
        }                                       \
    } while (0)

/** @brief 安全 strcat（蓝图 lv_STRCAT）：不越界 */
#define lv_STRCAT(dst, size, src)                     \
    do {                                              \
        size_t _cl = strlen(dst);                     \
        size_t _rem = (size) > _cl ? (size) - _cl - 1 : 0; \
        if (_rem > 0)                                 \
            strncat((dst), (src), _rem);              \
    } while (0)

/* ============================================================
 * 引用计数（§16.3.2）
 * ============================================================ */

/** @brief 引用计数头字段（蓝图 lv_REFCOUNT_HEADER） */
#define lv_REFCOUNT_HEADER      \
    volatile int32_t _ref_count; \
    void (*_on_zero_ref)(void *self)

/** @brief 引用计数初始化（蓝图 lv_REFCOUNT_INIT） */
#define lv_REFCOUNT_INIT(obj, destructor) \
    do {                                  \
        (obj)->_ref_count = 1;            \
        (obj)->_on_zero_ref = (destructor); \
    } while (0)

/** @brief 引用计数递增 */
#define lv_REFCOUNT_RETAIN(obj) \
    do {                        \
        (obj)->_ref_count++;    \
    } while (0)

/** @brief 引用计数递减；归零调用 destructor */
#define lv_REFCOUNT_RELEASE(obj)                    \
    do {                                            \
        if (--(obj)->_ref_count <= 0) {             \
            if ((obj)->_on_zero_ref != NULL)        \
                (obj)->_on_zero_ref(obj);           \
        }                                           \
    } while (0)

/* ============================================================
 * 泄漏检测（§16.3.3；接线 lv_mem_* 全局统计）
 * ============================================================ */

/** @brief 泄漏快照（蓝图 lvLeakSnapshot：活跃分配块明细 + 汇总） */
typedef struct {
    uint64_t active_bytes;    /**< 当前活跃字节数 */
    uint64_t peak_bytes;      /**< 峰值字节数 */
    uint64_t active_count;    /**< 活跃分配块数 */
    /** 活跃块明细（capacity 块；遍历 active_count 个） */
    struct {
        void *ptr;            /**< 分配指针 */
        size_t size;          /**< 分配大小 */
        const char *file;     /**< 分配文件（NULL 表示未记录） */
        int line;             /**< 分配行号（0 表示未记录） */
    } records[256];
} lvLeakSnapshot;

/**
 * @brief 获取内存泄漏快照（蓝图 lv_leak_detector_snapshot）
 *
 * 接线 lv_mem_get_global_stats：活跃字节 > 0 表示存在未释放分配。
 */
lv_PUBLIC_API lvLeakSnapshot lv_leak_detector_snapshot(void);

/**
 * @brief 打印泄漏报告（蓝图 lv_leak_detector_report）
 *
 * @param snapshot 快照（NULL 时内部取当前）
 */
lv_PUBLIC_API void lv_leak_detector_report(const lvLeakSnapshot *snapshot);

/**
 * @brief 断言无泄漏（蓝图 lv_leak_detector_assert_clean）
 *
 * @return 0 无活跃泄漏；非 0 当前活跃字节数（>0 表示泄漏）
 */
lv_PUBLIC_API int lv_leak_detector_assert_clean(void);

#ifdef __cplusplus
}
#endif

#endif /* lv_LV_SAFETY_H */
