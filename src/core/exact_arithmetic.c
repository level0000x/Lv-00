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

Lv00Timestamp lv00_timestamp_now(void) {
    Lv00Timestamp ts;
    ts.seconds = 0;
    ts.nanoseconds = 0;

#ifdef _WIN32
    /* Windows: 使用 QueryPerformanceCounter 实现高精度单调时钟 */
    static LARGE_INTEGER qpc_freq = { 0 };
    static volatile LONG qpc_initialized = 0;

    /* 使用 InterlockedCompareExchange 保证多线程下只初始化一次（线程安全） */
    if (InterlockedCompareExchange(&qpc_initialized, 1, 0) == 0) {
        QueryPerformanceFrequency(&qpc_freq);
    }

    LARGE_INTEGER counter;
    if (qpc_freq.QuadPart > 0 && QueryPerformanceCounter(&counter)) {
        ts.seconds = counter.QuadPart / qpc_freq.QuadPart;
        int64_t remainder = counter.QuadPart % qpc_freq.QuadPart;
        /* 将余数转换为纳秒 */
        ts.nanoseconds = (remainder * 1000000000LL) / qpc_freq.QuadPart;
    } else {
        /* 回退: 使用 GetSystemTimePreciseAsFileTime */
        FILETIME ft;
        GetSystemTimePreciseAsFileTime(&ft);
        ULARGE_INTEGER uli;
        uli.LowPart = ft.dwLowDateTime;
        uli.HighPart = ft.dwHighDateTime;
        /* FILETIME 表示自 1601-01-01 以来的 100ns 间隔 */
        ts.seconds = (int64_t)(uli.QuadPart / 10000000ULL);
        ts.nanoseconds = (int64_t)((uli.QuadPart % 10000000ULL) * 100ULL);
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
bool lv00_safe_pow(int64_t a, int64_t b, int64_t *result) {
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
            if (!lv00_safe_mul_impl(res, base, &res))
                return false;
        }
        exp >>= 1;
        if (exp > 0) {
            /* base *= base */
            if (!lv00_safe_mul_impl(base, base, &base))
                return false;
        }
    }

    *result = res;
    return true;
}
