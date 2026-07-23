/**
 * @file exact_arithmetic.c
 * @brief 精确算术基础设施实现 —— 时间戳、安全幂运算
 *
 * @author Lv-00 Project
 * @version v1.0.0
 * @date 2026-05-24
 */

#include "exact_arithmetic.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif

/* ========================================================================
 * 时间戳实现
 * ======================================================================== */

#ifdef _WIN32
/* Windows: QPC 高精度单调时钟的静态状态和初始化回调 */
static LARGE_INTEGER s_qpc_freq = {0};
static INIT_ONCE s_qpc_init_once = INIT_ONCE_STATIC_INIT;

/**
 * @brief QPC 一次性初始化回调
 * @note 由 InitOnceExecuteOnce 保证仅执行一次，消除竞态条件
 */
static BOOL CALLBACK qpc_init_callback(PINIT_ONCE init_once, PVOID param, PVOID *context) {
    (void) init_once;
    (void) param;
    (void) context;
    return QueryPerformanceFrequency(&s_qpc_freq);
}
#endif

lvTimestamp lv_timestamp_now(void) {
    lvTimestamp ts;
    ts.seconds = 0;
    ts.nanoseconds = 0;

#ifdef _WIN32
    InitOnceExecuteOnce(&s_qpc_init_once, qpc_init_callback, NULL, NULL);

    LARGE_INTEGER counter;
    if (s_qpc_freq.QuadPart > 0 && QueryPerformanceCounter(&counter)) {
        ts.seconds = counter.QuadPart / s_qpc_freq.QuadPart;
        int64_t remainder = counter.QuadPart % s_qpc_freq.QuadPart;
        /* 将余数转换为纳秒 */
        ts.nanoseconds = (remainder * 1000000000LL) / s_qpc_freq.QuadPart;
    } else {
        /* 回退: 使用 GetSystemTimePreciseAsFileTime */
        FILETIME ft;
        GetSystemTimePreciseAsFileTime(&ft);
        ULARGE_INTEGER uli;
        uli.LowPart = ft.dwLowDateTime;
        uli.HighPart = ft.dwHighDateTime;
        /* FILETIME 表示自 1601-01-01 以来的 100ns 间隔 */
        ts.seconds = (int64_t) (uli.QuadPart / 10000000ULL);
        ts.nanoseconds = (int64_t) ((uli.QuadPart % 10000000ULL) * 100ULL);
    }
#else
    /* POSIX: 使用 clock_gettime(CLOCK_MONOTONIC) */
    struct timespec tp;
    if (clock_gettime(CLOCK_MONOTONIC, &tp) == 0) {
        ts.seconds = (int64_t) tp.tv_sec;
        ts.nanoseconds = (int64_t) tp.tv_nsec;
    }
#endif

    /* 规范化纳秒（使用除法/取模，避免极端值时循环过多） */
    if (ts.nanoseconds >= 1000000000 || ts.nanoseconds < 0) {
        int64_t extra_secs = ts.nanoseconds / 1000000000;
        ts.seconds += extra_secs;
        ts.nanoseconds -= extra_secs * 1000000000;
        /* 处理负数取模的余数修正 */
        if (ts.nanoseconds < 0) {
            ts.nanoseconds += 1000000000;
            ts.seconds -= 1;
        }
    }

    return ts;
}

/* ========================================================================
 * 安全乘法
 * ======================================================================== */

/**
 * @brief 安全乘法 —— a * b，检测溢出
 */
bool lv_safe_mul_impl(int64_t a, int64_t b, int64_t *out) {
    if (!out)
        return false;
    if (a == 0 || b == 0) {
        *out = 0;
        return true;
    }
    /* 检查是否会溢出: |a * b| > INT64_MAX */
    if (a > 0 && b > 0 && a > INT64_MAX / b)
        return false;
    if (a > 0 && b < 0 && b < INT64_MIN / a)
        return false;
    if (a < 0 && b > 0 && a < INT64_MIN / b)
        return false;
    if (a < 0 && b < 0 && a < INT64_MAX / b)
        return false;
    *out = a * b;
    return true;
}

/* ========================================================================
 * 安全幂运算
 * ======================================================================== */

/**
 * @brief 安全取幂 —— a^b
 *
 * 使用快速幂算法（exponentiation by squaring）。
 * 每次乘法前检查溢出。
 *
 * @param a      底数
 * @param b      指数（必须 >= 0）
 * @param result 输出 a^b
 * @return true 成功，false 溢出或 b < 0
 */
bool lv_safe_pow(int64_t a, int64_t b, int64_t *result) {
    if (!result)
        return false;
    if (b < 0)
        return false; /* 负指数不支持（结果不是整数） */

    if (b == 0) {
        *result = 1;
        return true;
    }

    int64_t base = a;
    int64_t exp = b;
    int64_t res = 1;

    while (exp > 0) {
        if (exp & 1) {
            /* res *= base */
            if (!lv_safe_mul_impl(res, base, &res))
                return false;
        }
        exp >>= 1;
        if (exp > 0) {
            /* base *= base */
            if (!lv_safe_mul_impl(base, base, &base))
                return false;
        }
    }

    *result = res;
    return true;
}

/* ========================================================================
 * 安全加法
 * ======================================================================== */

/**
 * @brief 安全加法 -- a + b，检测溢出
 *
 * @param a  加数 a
 * @param b  加数 b
 * @param out 输出：a + b 的结果
 * @return true 成功（无溢出），false 溢出或 out 为 NULL
 */
bool lv_safe_add_check_impl(int64_t a, int64_t b, int64_t *out) {
    if (!out)
        return false;

    /* 溢出条件检测：
     * 正溢出: a > 0 && b > 0 && a > INT64_MAX - b
     * 负溢出: a < 0 && b < 0 && a < INT64_MIN - b */
    if (b > 0 && a > INT64_MAX - b)
        return false;
    if (b < 0 && a < INT64_MIN - b)
        return false;

    *out = a + b;
    return true;
}

/* ========================================================================
 * 安全减法
 * ======================================================================== */

/**
 * @brief 安全减法 -- a - b，检测溢出
 *
 * @param a  被减数 a
 * @param b  减数 b
 * @param out 输出：a - b 的结果
 * @return true 成功（无溢出），false 溢出或 out 为 NULL
 */
bool lv_safe_sub_impl(int64_t a, int64_t b, int64_t *out) {
    if (!out)
        return false;

    /* 溢出条件检测：
     * 正溢出: b < 0 && a > INT64_MAX + b  (减负数可能溢出)
     * 负溢出: b > 0 && a < INT64_MIN + b  (减正数可能下溢) */
    if (b < 0 && a > INT64_MAX + b)
        return false;
    if (b > 0 && a < INT64_MIN + b)
        return false;

    *out = a - b;
    return true;
}
