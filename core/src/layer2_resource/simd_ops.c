/**
 * @file simd_ops.c
 * @brief SIMD向量运算库实现
 *
 * @details 跨平台SIMD实现，支持SSE/AVX/NEON，并提供标量回退。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "simd_ops.h"

#include "lv_utils.h"
#include "lv/lv_xmacro.h"

#include <math.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

/* ============== SIMD 内联头文件 ============== */
#if defined(__AVX__)
#include <immintrin.h>
#elif defined(__SSE2__)
#include <emmintrin.h>
#if defined(__SSE3__)
#include <pmmintrin.h>
#endif
#elif defined(__ARM_NEON__) || defined(__ARM_NEON)
#include <arm_neon.h>
#endif

/* ============== SIMD 能力检测 ============== */

static atomic_uint g_simd_capabilities = 0;
static atomic_bool g_simd_initialized = false;

/* 检测SIMD能力（线程安全：使用原子操作保证只初始化一次） */
static void detect_simd_capabilities(void) {
    /* 原子交换：多线程并发调用时，仅第一个线程返回 false 并执行初始化 */
    bool expected = false;
    if (!atomic_compare_exchange_strong(&g_simd_initialized, &expected, true)) {
        return; /* 已被其他线程初始化，直接返回 */
    }

    uint32_t caps = 0;

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
    /* x86/x64 平台 */

#if defined(__GNUC__) || defined(__clang__)
/* GCC/Clang: 使用 __builtin_cpu_supports */
#if defined(__SSE2__)
    caps |= lv_SIMD_SSE2;
#endif
#if defined(__SSE4_1__)
    caps |= lv_SIMD_SSE41;
#endif
#if defined(__AVX__)
    caps |= lv_SIMD_AVX;
#endif
#if defined(__AVX2__)
    caps |= lv_SIMD_AVX2;
#endif
#if defined(__AVX512F__)
    caps |= lv_SIMD_AVX512F;
#endif

#elif defined(_MSC_VER)
/* MSVC: 使用 __cpuid */
#include <intrin.h>
    int cpuinfo[4];
    __cpuid(cpuinfo, 0);

    if (cpuinfo[0] >= 1) {
        __cpuid(cpuinfo, 1);
        if (cpuinfo[3] & (1 << 26))
            caps |= lv_SIMD_SSE2;
        if (cpuinfo[2] & (1 << 19))
            caps |= lv_SIMD_SSE41;
        if (cpuinfo[2] & (1 << 28))
            caps |= lv_SIMD_AVX;
    }

    if (cpuinfo[0] >= 7) {
        __cpuidex(cpuinfo, 7, 0);
        if (cpuinfo[1] & (1 << 5))
            caps |= lv_SIMD_AVX2;
    }
#endif

#elif defined(__arm__) || defined(__aarch64__) || defined(_M_ARM) || defined(_M_ARM64)
/* ARM 平台 */
#if defined(__ARM_NEON) || defined(__ARM_NEON__) || defined(_M_ARM64)
    caps |= lv_SIMD_NEON;
#endif
#endif

    /* 如果没有检测到任何SIMD能力，使用标量实现 */
    if (caps == 0) {
        caps = lv_SIMD_NONE;
    }

    /* 原子写入：确保其他线程看到完整的初始化结果 */
    atomic_store(&g_simd_capabilities, caps);
}

uint32_t lv_simd_detect_capabilities(void) {
    detect_simd_capabilities();
    return atomic_load(&g_simd_capabilities);
}

bool lv_simd_has_capability(lvSimdCapability cap) {
    detect_simd_capabilities();
    return (atomic_load(&g_simd_capabilities) & cap) != 0;
}

/* ================================================================
 * 枚举 -> 名称 映射表（数据表化，替代 switch）
 * ================================================================ */

/** @brief lv_simd_capability_name 名称表（按枚举值升序） */
static const lvStrToEnumEntry s_lv_simd_capability_name_entries[] = {
    {"None (Scalar)", lv_SIMD_NONE},
    {"SSE2", lv_SIMD_SSE2},
    {"SSE4.1", lv_SIMD_SSE41},
    {"AVX", lv_SIMD_AVX},
    {"AVX2", lv_SIMD_AVX2},
    {"AVX-512F", lv_SIMD_AVX512F},
    {"NEON", lv_SIMD_NEON},
};

const char *lv_simd_capability_name(lvSimdCapability cap) {
    return lv_enum_to_str(s_lv_simd_capability_name_entries, lv_ARRAY_SIZE(s_lv_simd_capability_name_entries), (int) cap, "Unknown");
}

/* ============== 统计 ============== */

/*
 * 内部原子统计结构：字段与公共 lvSimdStats 一一对应（保持 simd_ops.h 公开布局不变），
 * 仅将字段类型原子化，消除热路径统计计数器的非原子自增竞态。
 */
typedef struct {
    _Atomic uint64_t vec4_ops;
    _Atomic uint64_t vec8_ops;
    _Atomic uint64_t array_ops;
    _Atomic uint64_t elements_processed;
    _Atomic uint64_t simd_time_us;
    _Atomic uint64_t scalar_fallbacks;
} lvSimdStatsAtomic;

static lvSimdStatsAtomic g_simd_stats = {0};

/* 热路径统计累加宏（relaxed 序：计数器允许乱序累加，只需保证读改写不撕裂） */
#define lv_SIMD_STATS_INC(field) \
    atomic_fetch_add_explicit(&g_simd_stats.field, 1, memory_order_relaxed)
#define lv_SIMD_STATS_ADD(field, n) \
    atomic_fetch_add_explicit(&g_simd_stats.field, (uint64_t)(n), memory_order_relaxed)

/* 原子计数器汇总为普通快照（get_stats / print_diag 共用，各字段独立 relaxed 读取） */
static void lv_simd_stats_snapshot(lvSimdStats *out) {
    out->vec4_ops = atomic_load_explicit(&g_simd_stats.vec4_ops, memory_order_relaxed);
    out->vec8_ops = atomic_load_explicit(&g_simd_stats.vec8_ops, memory_order_relaxed);
    out->array_ops = atomic_load_explicit(&g_simd_stats.array_ops, memory_order_relaxed);
    out->elements_processed = atomic_load_explicit(&g_simd_stats.elements_processed, memory_order_relaxed);
    out->simd_time_us = atomic_load_explicit(&g_simd_stats.simd_time_us, memory_order_relaxed);
    out->scalar_fallbacks = atomic_load_explicit(&g_simd_stats.scalar_fallbacks, memory_order_relaxed);
}

void lv_simd_get_stats(lvSimdStats *stats) {
    if (!stats)
        return;
    lv_simd_stats_snapshot(stats);
}

void lv_simd_reset_stats(void) {
    /* 原子类型不可用 memset，逐字段 store 0 清零（与并发累加竞争时语义等价于清零） */
    atomic_store_explicit(&g_simd_stats.vec4_ops, 0, memory_order_relaxed);
    atomic_store_explicit(&g_simd_stats.vec8_ops, 0, memory_order_relaxed);
    atomic_store_explicit(&g_simd_stats.array_ops, 0, memory_order_relaxed);
    atomic_store_explicit(&g_simd_stats.elements_processed, 0, memory_order_relaxed);
    atomic_store_explicit(&g_simd_stats.simd_time_us, 0, memory_order_relaxed);
    atomic_store_explicit(&g_simd_stats.scalar_fallbacks, 0, memory_order_relaxed);
}

void lv_simd_print_diag(void *stream) {
    FILE *f = stream ? (FILE *) stream : stdout;

    fprintf(f, "\n========== Lv-00 SIMD 诊断 ==========\n");
    fprintf(f, "检测到的SIMD能力:\n");

    detect_simd_capabilities();

    const char *caps[] = {"SSE2", "SSE4.1", "AVX", "AVX2", "AVX-512F", "NEON"};
    lvSimdCapability flags[] = {lv_SIMD_SSE2, lv_SIMD_SSE41, lv_SIMD_AVX, lv_SIMD_AVX2, lv_SIMD_AVX512F, lv_SIMD_NEON};

    bool has_any = false;
    for (int i = 0; i < 6; i++) {
        if (atomic_load(&g_simd_capabilities) & flags[i]) {
            fprintf(f, "  - %s\n", caps[i]);
            has_any = true;
        }
    }

    if (!has_any) {
        fprintf(f, "  - 无（使用标量实现）\n");
    }

    fprintf(f, "\n--- 性能统计 ---\n");
    lvSimdStats snapshot;
    lv_simd_stats_snapshot(&snapshot);
    fprintf(f, "4元素向量操作: %llu\n", (unsigned long long) snapshot.vec4_ops);
    fprintf(f, "8元素向量操作: %llu\n", (unsigned long long) snapshot.vec8_ops);
    fprintf(f, "数组操作: %llu\n", (unsigned long long) snapshot.array_ops);
    fprintf(f, "处理元素总数: %llu\n", (unsigned long long) snapshot.elements_processed);
    fprintf(f, "SIMD总耗时: %llu us\n", (unsigned long long) snapshot.simd_time_us);
    fprintf(f, "标量回退次数: %llu\n", (unsigned long long) snapshot.scalar_fallbacks);
    fprintf(f, "=====================================\n\n");
}

/* ============== 标量实现（回退） ============== */

/* ============== 4x double 向量操作 ============== */

lvVec4d lv_vec4d_zero(void) {
    lvVec4d v = {{0.0, 0.0, 0.0, 0.0}};
    lv_SIMD_STATS_INC(vec4_ops);
    return v;
}

lvVec4d lv_vec4d_one(void) {
    lvVec4d v = {{1.0, 1.0, 1.0, 1.0}};
    lv_SIMD_STATS_INC(vec4_ops);
    return v;
}

lvVec4d lv_vec4d_set1(double val) {
    lvVec4d v = {{val, val, val, val}};
    lv_SIMD_STATS_INC(vec4_ops);
    return v;
}

lvVec4d lv_vec4d_set(double x, double y, double z, double w) {
    lvVec4d v = {{x, y, z, w}};
    lv_SIMD_STATS_INC(vec4_ops);
    return v;
}

lvVec4d lv_vec4d_load(const double *ptr) {
    lvVec4d v;
    v.v[0] = ptr[0];
    v.v[1] = ptr[1];
    v.v[2] = ptr[2];
    v.v[3] = ptr[3];
    lv_SIMD_STATS_INC(vec4_ops);
    return v;
}

lvVec4d lv_vec4d_loadu(const double *ptr) {
    return lv_vec4d_load(ptr);
}

void lv_vec4d_store(double *ptr, lvVec4d vec) {
    ptr[0] = vec.v[0];
    ptr[1] = vec.v[1];
    ptr[2] = vec.v[2];
    ptr[3] = vec.v[3];
}

void lv_vec4d_storeu(double *ptr, lvVec4d vec) {
    lv_vec4d_store(ptr, vec);
}

lvVec4d lv_vec4d_add(lvVec4d a, lvVec4d b) {
    lvVec4d r;
#if defined(__AVX__)
    __m256d va = _mm256_loadu_pd(a.v);
    __m256d vb = _mm256_loadu_pd(b.v);
    __m256d vr = _mm256_add_pd(va, vb);
    _mm256_storeu_pd(r.v, vr);
#elif defined(__SSE2__)
    __m128d va_lo = _mm_loadu_pd(a.v);
    __m128d va_hi = _mm_loadu_pd(a.v + 2);
    __m128d vb_lo = _mm_loadu_pd(b.v);
    __m128d vb_hi = _mm_loadu_pd(b.v + 2);
    __m128d vr_lo = _mm_add_pd(va_lo, vb_lo);
    __m128d vr_hi = _mm_add_pd(va_hi, vb_hi);
    _mm_storeu_pd(r.v, vr_lo);
    _mm_storeu_pd(r.v + 2, vr_hi);
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
    float64x2_t va_lo = vld1q_f64(a.v);
    float64x2_t va_hi = vld1q_f64(a.v + 2);
    float64x2_t vb_lo = vld1q_f64(b.v);
    float64x2_t vb_hi = vld1q_f64(b.v + 2);
    float64x2_t vr_lo = vaddq_f64(va_lo, vb_lo);
    float64x2_t vr_hi = vaddq_f64(va_hi, vb_hi);
    vst1q_f64(r.v, vr_lo);
    vst1q_f64(r.v + 2, vr_hi);
#else
    r.v[0] = a.v[0] + b.v[0];
    r.v[1] = a.v[1] + b.v[1];
    r.v[2] = a.v[2] + b.v[2];
    r.v[3] = a.v[3] + b.v[3];
#endif
    lv_SIMD_STATS_INC(vec4_ops);
    return r;
}

lvVec4d lv_vec4d_sub(lvVec4d a, lvVec4d b) {
    lvVec4d r;
#if defined(__AVX__)
    __m256d va = _mm256_loadu_pd(a.v);
    __m256d vb = _mm256_loadu_pd(b.v);
    __m256d vr = _mm256_sub_pd(va, vb);
    _mm256_storeu_pd(r.v, vr);
#elif defined(__SSE2__)
    __m128d va_lo = _mm_loadu_pd(a.v);
    __m128d va_hi = _mm_loadu_pd(a.v + 2);
    __m128d vb_lo = _mm_loadu_pd(b.v);
    __m128d vb_hi = _mm_loadu_pd(b.v + 2);
    __m128d vr_lo = _mm_sub_pd(va_lo, vb_lo);
    __m128d vr_hi = _mm_sub_pd(va_hi, vb_hi);
    _mm_storeu_pd(r.v, vr_lo);
    _mm_storeu_pd(r.v + 2, vr_hi);
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
    float64x2_t va_lo = vld1q_f64(a.v);
    float64x2_t va_hi = vld1q_f64(a.v + 2);
    float64x2_t vb_lo = vld1q_f64(b.v);
    float64x2_t vb_hi = vld1q_f64(b.v + 2);
    float64x2_t vr_lo = vsubq_f64(va_lo, vb_lo);
    float64x2_t vr_hi = vsubq_f64(va_hi, vb_hi);
    vst1q_f64(r.v, vr_lo);
    vst1q_f64(r.v + 2, vr_hi);
#else
    r.v[0] = a.v[0] - b.v[0];
    r.v[1] = a.v[1] - b.v[1];
    r.v[2] = a.v[2] - b.v[2];
    r.v[3] = a.v[3] - b.v[3];
#endif
    lv_SIMD_STATS_INC(vec4_ops);
    return r;
}

lvVec4d lv_vec4d_mul(lvVec4d a, lvVec4d b) {
    lvVec4d r;
#if defined(__AVX__)
    __m256d va = _mm256_loadu_pd(a.v);
    __m256d vb = _mm256_loadu_pd(b.v);
    __m256d vr = _mm256_mul_pd(va, vb);
    _mm256_storeu_pd(r.v, vr);
#elif defined(__SSE2__)
    __m128d va_lo = _mm_loadu_pd(a.v);
    __m128d va_hi = _mm_loadu_pd(a.v + 2);
    __m128d vb_lo = _mm_loadu_pd(b.v);
    __m128d vb_hi = _mm_loadu_pd(b.v + 2);
    __m128d vr_lo = _mm_mul_pd(va_lo, vb_lo);
    __m128d vr_hi = _mm_mul_pd(va_hi, vb_hi);
    _mm_storeu_pd(r.v, vr_lo);
    _mm_storeu_pd(r.v + 2, vr_hi);
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
    float64x2_t va_lo = vld1q_f64(a.v);
    float64x2_t va_hi = vld1q_f64(a.v + 2);
    float64x2_t vb_lo = vld1q_f64(b.v);
    float64x2_t vb_hi = vld1q_f64(b.v + 2);
    float64x2_t vr_lo = vmulq_f64(va_lo, vb_lo);
    float64x2_t vr_hi = vmulq_f64(va_hi, vb_hi);
    vst1q_f64(r.v, vr_lo);
    vst1q_f64(r.v + 2, vr_hi);
#else
    r.v[0] = a.v[0] * b.v[0];
    r.v[1] = a.v[1] * b.v[1];
    r.v[2] = a.v[2] * b.v[2];
    r.v[3] = a.v[3] * b.v[3];
#endif
    lv_SIMD_STATS_INC(vec4_ops);
    return r;
}

lvVec4d lv_vec4d_div(lvVec4d a, lvVec4d b) {
    lvVec4d r;
#if defined(__AVX__)
    __m256d va = _mm256_loadu_pd(a.v);
    __m256d vb = _mm256_loadu_pd(b.v);
    __m256d vr = _mm256_div_pd(va, vb);
    _mm256_storeu_pd(r.v, vr);
#elif defined(__SSE2__)
    __m128d va_lo = _mm_loadu_pd(a.v);
    __m128d va_hi = _mm_loadu_pd(a.v + 2);
    __m128d vb_lo = _mm_loadu_pd(b.v);
    __m128d vb_hi = _mm_loadu_pd(b.v + 2);
    __m128d vr_lo = _mm_div_pd(va_lo, vb_lo);
    __m128d vr_hi = _mm_div_pd(va_hi, vb_hi);
    _mm_storeu_pd(r.v, vr_lo);
    _mm_storeu_pd(r.v + 2, vr_hi);
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
    float64x2_t va_lo = vld1q_f64(a.v);
    float64x2_t va_hi = vld1q_f64(a.v + 2);
    float64x2_t vb_lo = vld1q_f64(b.v);
    float64x2_t vb_hi = vld1q_f64(b.v + 2);
    float64x2_t vr_lo = vdivq_f64(va_lo, vb_lo);
    float64x2_t vr_hi = vdivq_f64(va_hi, vb_hi);
    vst1q_f64(r.v, vr_lo);
    vst1q_f64(r.v + 2, vr_hi);
#else
    r.v[0] = a.v[0] / b.v[0];
    r.v[1] = a.v[1] / b.v[1];
    r.v[2] = a.v[2] / b.v[2];
    r.v[3] = a.v[3] / b.v[3];
#endif
    lv_SIMD_STATS_INC(vec4_ops);
    return r;
}

lvVec4d lv_vec4d_neg(lvVec4d a) {
    lvVec4d r;
#if defined(__AVX__)
    __m256d va = _mm256_loadu_pd(a.v);
    __m256d sign_mask = _mm256_set1_pd(-0.0);
    __m256d vr = _mm256_xor_pd(va, sign_mask);
    _mm256_storeu_pd(r.v, vr);
#elif defined(__SSE2__)
    __m128d sign_mask = _mm_set1_pd(-0.0);
    __m128d va_lo = _mm_loadu_pd(a.v);
    __m128d va_hi = _mm_loadu_pd(a.v + 2);
    __m128d vr_lo = _mm_xor_pd(va_lo, sign_mask);
    __m128d vr_hi = _mm_xor_pd(va_hi, sign_mask);
    _mm_storeu_pd(r.v, vr_lo);
    _mm_storeu_pd(r.v + 2, vr_hi);
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
    float64x2_t va_lo = vld1q_f64(a.v);
    float64x2_t va_hi = vld1q_f64(a.v + 2);
    float64x2_t vr_lo = vnegq_f64(va_lo);
    float64x2_t vr_hi = vnegq_f64(va_hi);
    vst1q_f64(r.v, vr_lo);
    vst1q_f64(r.v + 2, vr_hi);
#else
    r.v[0] = -a.v[0];
    r.v[1] = -a.v[1];
    r.v[2] = -a.v[2];
    r.v[3] = -a.v[3];
#endif
    lv_SIMD_STATS_INC(vec4_ops);
    return r;
}

lvVec4d lv_vec4d_sqrt(lvVec4d a) {
    lvVec4d r;
#if defined(__AVX__)
    __m256d va = _mm256_loadu_pd(a.v);
    __m256d vr = _mm256_sqrt_pd(va);
    _mm256_storeu_pd(r.v, vr);
#elif defined(__SSE2__)
    __m128d va_lo = _mm_loadu_pd(a.v);
    __m128d va_hi = _mm_loadu_pd(a.v + 2);
    __m128d vr_lo = _mm_sqrt_pd(va_lo);
    __m128d vr_hi = _mm_sqrt_pd(va_hi);
    _mm_storeu_pd(r.v, vr_lo);
    _mm_storeu_pd(r.v + 2, vr_hi);
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
    float64x2_t va_lo = vld1q_f64(a.v);
    float64x2_t va_hi = vld1q_f64(a.v + 2);
    float64x2_t vr_lo = vsqrtq_f64(va_lo);
    float64x2_t vr_hi = vsqrtq_f64(va_hi);
    vst1q_f64(r.v, vr_lo);
    vst1q_f64(r.v + 2, vr_hi);
#else
    r.v[0] = sqrt(a.v[0]);
    r.v[1] = sqrt(a.v[1]);
    r.v[2] = sqrt(a.v[2]);
    r.v[3] = sqrt(a.v[3]);
#endif
    lv_SIMD_STATS_INC(vec4_ops);
    return r;
}

lvVec4d lv_vec4d_abs(lvVec4d a) {
    lvVec4d r;
#if defined(__AVX__)
    __m256d va = _mm256_loadu_pd(a.v);
    __m256d sign_mask = _mm256_set1_pd(-0.0);
    __m256d vr = _mm256_andnot_pd(sign_mask, va);
    _mm256_storeu_pd(r.v, vr);
#elif defined(__SSE2__)
    __m128d sign_mask = _mm_set1_pd(-0.0);
    __m128d va_lo = _mm_loadu_pd(a.v);
    __m128d va_hi = _mm_loadu_pd(a.v + 2);
    __m128d vr_lo = _mm_andnot_pd(sign_mask, va_lo);
    __m128d vr_hi = _mm_andnot_pd(sign_mask, va_hi);
    _mm_storeu_pd(r.v, vr_lo);
    _mm_storeu_pd(r.v + 2, vr_hi);
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
    float64x2_t va_lo = vld1q_f64(a.v);
    float64x2_t va_hi = vld1q_f64(a.v + 2);
    float64x2_t vr_lo = vabsq_f64(va_lo);
    float64x2_t vr_hi = vabsq_f64(va_hi);
    vst1q_f64(r.v, vr_lo);
    vst1q_f64(r.v + 2, vr_hi);
#else
    r.v[0] = fabs(a.v[0]);
    r.v[1] = fabs(a.v[1]);
    r.v[2] = fabs(a.v[2]);
    r.v[3] = fabs(a.v[3]);
#endif
    lv_SIMD_STATS_INC(vec4_ops);
    return r;
}

lvVec4d lv_vec4d_max(lvVec4d a, lvVec4d b) {
    lvVec4d r;
#if defined(__AVX__)
    __m256d va = _mm256_loadu_pd(a.v);
    __m256d vb = _mm256_loadu_pd(b.v);
    __m256d vr = _mm256_max_pd(va, vb);
    _mm256_storeu_pd(r.v, vr);
#elif defined(__SSE2__)
    __m128d va_lo = _mm_loadu_pd(a.v);
    __m128d va_hi = _mm_loadu_pd(a.v + 2);
    __m128d vb_lo = _mm_loadu_pd(b.v);
    __m128d vb_hi = _mm_loadu_pd(b.v + 2);
    __m128d vr_lo = _mm_max_pd(va_lo, vb_lo);
    __m128d vr_hi = _mm_max_pd(va_hi, vb_hi);
    _mm_storeu_pd(r.v, vr_lo);
    _mm_storeu_pd(r.v + 2, vr_hi);
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
    float64x2_t va_lo = vld1q_f64(a.v);
    float64x2_t va_hi = vld1q_f64(a.v + 2);
    float64x2_t vb_lo = vld1q_f64(b.v);
    float64x2_t vb_hi = vld1q_f64(b.v + 2);
    float64x2_t vr_lo = vmaxq_f64(va_lo, vb_lo);
    float64x2_t vr_hi = vmaxq_f64(va_hi, vb_hi);
    vst1q_f64(r.v, vr_lo);
    vst1q_f64(r.v + 2, vr_hi);
#else
    r.v[0] = (a.v[0] > b.v[0]) ? a.v[0] : b.v[0];
    r.v[1] = (a.v[1] > b.v[1]) ? a.v[1] : b.v[1];
    r.v[2] = (a.v[2] > b.v[2]) ? a.v[2] : b.v[2];
    r.v[3] = (a.v[3] > b.v[3]) ? a.v[3] : b.v[3];
#endif
    lv_SIMD_STATS_INC(vec4_ops);
    return r;
}

lvVec4d lv_vec4d_min(lvVec4d a, lvVec4d b) {
    lvVec4d r;
#if defined(__AVX__)
    __m256d va = _mm256_loadu_pd(a.v);
    __m256d vb = _mm256_loadu_pd(b.v);
    __m256d vr = _mm256_min_pd(va, vb);
    _mm256_storeu_pd(r.v, vr);
#elif defined(__SSE2__)
    __m128d va_lo = _mm_loadu_pd(a.v);
    __m128d va_hi = _mm_loadu_pd(a.v + 2);
    __m128d vb_lo = _mm_loadu_pd(b.v);
    __m128d vb_hi = _mm_loadu_pd(b.v + 2);
    __m128d vr_lo = _mm_min_pd(va_lo, vb_lo);
    __m128d vr_hi = _mm_min_pd(va_hi, vb_hi);
    _mm_storeu_pd(r.v, vr_lo);
    _mm_storeu_pd(r.v + 2, vr_hi);
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
    float64x2_t va_lo = vld1q_f64(a.v);
    float64x2_t va_hi = vld1q_f64(a.v + 2);
    float64x2_t vb_lo = vld1q_f64(b.v);
    float64x2_t vb_hi = vld1q_f64(b.v + 2);
    float64x2_t vr_lo = vminq_f64(va_lo, vb_lo);
    float64x2_t vr_hi = vminq_f64(va_hi, vb_hi);
    vst1q_f64(r.v, vr_lo);
    vst1q_f64(r.v + 2, vr_hi);
#else
    r.v[0] = (a.v[0] < b.v[0]) ? a.v[0] : b.v[0];
    r.v[1] = (a.v[1] < b.v[1]) ? a.v[1] : b.v[1];
    r.v[2] = (a.v[2] < b.v[2]) ? a.v[2] : b.v[2];
    r.v[3] = (a.v[3] < b.v[3]) ? a.v[3] : b.v[3];
#endif
    lv_SIMD_STATS_INC(vec4_ops);
    return r;
}

lvVec4d lv_vec4d_fmadd(lvVec4d a, lvVec4d x, lvVec4d y) {
    lvVec4d r;
#if defined(__AVX__)
    __m256d va = _mm256_loadu_pd(a.v);
    __m256d vx = _mm256_loadu_pd(x.v);
    __m256d vy = _mm256_loadu_pd(y.v);
    __m256d vr = _mm256_fmadd_pd(va, vx, vy);
    _mm256_storeu_pd(r.v, vr);
#elif defined(__SSE2__)
    __m128d a_lo = _mm_loadu_pd(a.v);
    __m128d a_hi = _mm_loadu_pd(a.v + 2);
    __m128d x_lo = _mm_loadu_pd(x.v);
    __m128d x_hi = _mm_loadu_pd(x.v + 2);
    __m128d y_lo = _mm_loadu_pd(y.v);
    __m128d y_hi = _mm_loadu_pd(y.v + 2);
    __m128d vr_lo = _mm_add_pd(_mm_mul_pd(a_lo, x_lo), y_lo);
    __m128d vr_hi = _mm_add_pd(_mm_mul_pd(a_hi, x_hi), y_hi);
    _mm_storeu_pd(r.v, vr_lo);
    _mm_storeu_pd(r.v + 2, vr_hi);
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
    float64x2_t a_lo = vld1q_f64(a.v);
    float64x2_t a_hi = vld1q_f64(a.v + 2);
    float64x2_t x_lo = vld1q_f64(x.v);
    float64x2_t x_hi = vld1q_f64(x.v + 2);
    float64x2_t y_lo = vld1q_f64(y.v);
    float64x2_t y_hi = vld1q_f64(y.v + 2);
    vst1q_f64(r.v, vmlaq_f64(y_lo, a_lo, x_lo));
    vst1q_f64(r.v + 2, vmlaq_f64(y_hi, a_hi, x_hi));
#else
    r.v[0] = a.v[0] * x.v[0] + y.v[0];
    r.v[1] = a.v[1] * x.v[1] + y.v[1];
    r.v[2] = a.v[2] * x.v[2] + y.v[2];
    r.v[3] = a.v[3] * x.v[3] + y.v[3];
#endif
    lv_SIMD_STATS_INC(vec4_ops);
    return r;
}

/* 比较操作 */
lvVec4d lv_vec4d_cmpeq(lvVec4d a, lvVec4d b) {
    lvVec4d r;
#if defined(__AVX__)
    __m256d va = _mm256_loadu_pd(a.v);
    __m256d vb = _mm256_loadu_pd(b.v);
    __m256d vm = _mm256_cmp_pd(va, vb, _CMP_EQ_OQ);
    _mm256_storeu_pd(r.v, vm);
#elif defined(__SSE2__)
    __m128d a_lo = _mm_loadu_pd(a.v), a_hi = _mm_loadu_pd(a.v + 2);
    __m128d b_lo = _mm_loadu_pd(b.v), b_hi = _mm_loadu_pd(b.v + 2);
    _mm_storeu_pd(r.v, _mm_cmpeq_pd(a_lo, b_lo));
    _mm_storeu_pd(r.v + 2, _mm_cmpeq_pd(a_hi, b_hi));
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
    float64x2_t a_lo = vld1q_f64(a.v), a_hi = vld1q_f64(a.v + 2);
    float64x2_t b_lo = vld1q_f64(b.v), b_hi = vld1q_f64(b.v + 2);
    uint64x2_t r_lo = vceqq_f64(a_lo, b_lo);
    uint64x2_t r_hi = vceqq_f64(a_hi, b_hi);
    vst1q_f64(r.v, (float64x2_t)r_lo);
    vst1q_f64(r.v + 2, (float64x2_t)r_hi);
#else
    r.v[0] = (a.v[0] == b.v[0]) ? -1.0 : 0.0;
    r.v[1] = (a.v[1] == b.v[1]) ? -1.0 : 0.0;
    r.v[2] = (a.v[2] == b.v[2]) ? -1.0 : 0.0;
    r.v[3] = (a.v[3] == b.v[3]) ? -1.0 : 0.0;
#endif
    return r;
}

lvVec4d lv_vec4d_cmplt(lvVec4d a, lvVec4d b) {
    lvVec4d r;
#if defined(__AVX__)
    __m256d va = _mm256_loadu_pd(a.v);
    __m256d vb = _mm256_loadu_pd(b.v);
    __m256d vm = _mm256_cmp_pd(va, vb, _CMP_LT_OQ);
    _mm256_storeu_pd(r.v, vm);
#elif defined(__SSE2__)
    __m128d a_lo = _mm_loadu_pd(a.v), a_hi = _mm_loadu_pd(a.v + 2);
    __m128d b_lo = _mm_loadu_pd(b.v), b_hi = _mm_loadu_pd(b.v + 2);
    _mm_storeu_pd(r.v, _mm_cmplt_pd(a_lo, b_lo));
    _mm_storeu_pd(r.v + 2, _mm_cmplt_pd(a_hi, b_hi));
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
    float64x2_t a_lo = vld1q_f64(a.v), a_hi = vld1q_f64(a.v + 2);
    float64x2_t b_lo = vld1q_f64(b.v), b_hi = vld1q_f64(b.v + 2);
    uint64x2_t r_lo = vcltq_f64(a_lo, b_lo);
    uint64x2_t r_hi = vcltq_f64(a_hi, b_hi);
    vst1q_f64(r.v, (float64x2_t)r_lo);
    vst1q_f64(r.v + 2, (float64x2_t)r_hi);
#else
    r.v[0] = (a.v[0] < b.v[0]) ? -1.0 : 0.0;
    r.v[1] = (a.v[1] < b.v[1]) ? -1.0 : 0.0;
    r.v[2] = (a.v[2] < b.v[2]) ? -1.0 : 0.0;
    r.v[3] = (a.v[3] < b.v[3]) ? -1.0 : 0.0;
#endif
    return r;
}

lvVec4d lv_vec4d_cmple(lvVec4d a, lvVec4d b) {
    lvVec4d r;
#if defined(__AVX__)
    __m256d va = _mm256_loadu_pd(a.v);
    __m256d vb = _mm256_loadu_pd(b.v);
    __m256d vm = _mm256_cmp_pd(va, vb, _CMP_LE_OQ);
    _mm256_storeu_pd(r.v, vm);
#elif defined(__SSE2__)
    __m128d a_lo = _mm_loadu_pd(a.v), a_hi = _mm_loadu_pd(a.v + 2);
    __m128d b_lo = _mm_loadu_pd(b.v), b_hi = _mm_loadu_pd(b.v + 2);
    _mm_storeu_pd(r.v, _mm_cmple_pd(a_lo, b_lo));
    _mm_storeu_pd(r.v + 2, _mm_cmple_pd(a_hi, b_hi));
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
    float64x2_t a_lo = vld1q_f64(a.v), a_hi = vld1q_f64(a.v + 2);
    float64x2_t b_lo = vld1q_f64(b.v), b_hi = vld1q_f64(b.v + 2);
    uint64x2_t r_lo = vcleq_f64(a_lo, b_lo);
    uint64x2_t r_hi = vcleq_f64(a_hi, b_hi);
    vst1q_f64(r.v, (float64x2_t)r_lo);
    vst1q_f64(r.v + 2, (float64x2_t)r_hi);
#else
    r.v[0] = (a.v[0] <= b.v[0]) ? -1.0 : 0.0;
    r.v[1] = (a.v[1] <= b.v[1]) ? -1.0 : 0.0;
    r.v[2] = (a.v[2] <= b.v[2]) ? -1.0 : 0.0;
    r.v[3] = (a.v[3] <= b.v[3]) ? -1.0 : 0.0;
#endif
    return r;
}

lvVec4d lv_vec4d_cmpgt(lvVec4d a, lvVec4d b) {
    lvVec4d r;
#if defined(__AVX__)
    __m256d va = _mm256_loadu_pd(a.v);
    __m256d vb = _mm256_loadu_pd(b.v);
    __m256d vm = _mm256_cmp_pd(va, vb, _CMP_GT_OQ);
    _mm256_storeu_pd(r.v, vm);
#elif defined(__SSE2__)
    __m128d a_lo = _mm_loadu_pd(a.v), a_hi = _mm_loadu_pd(a.v + 2);
    __m128d b_lo = _mm_loadu_pd(b.v), b_hi = _mm_loadu_pd(b.v + 2);
    _mm_storeu_pd(r.v, _mm_cmpgt_pd(a_lo, b_lo));
    _mm_storeu_pd(r.v + 2, _mm_cmpgt_pd(a_hi, b_hi));
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
    float64x2_t a_lo = vld1q_f64(a.v), a_hi = vld1q_f64(a.v + 2);
    float64x2_t b_lo = vld1q_f64(b.v), b_hi = vld1q_f64(b.v + 2);
    uint64x2_t r_lo = vcgtq_f64(a_lo, b_lo);
    uint64x2_t r_hi = vcgtq_f64(a_hi, b_hi);
    vst1q_f64(r.v, (float64x2_t)r_lo);
    vst1q_f64(r.v + 2, (float64x2_t)r_hi);
#else
    r.v[0] = (a.v[0] > b.v[0]) ? -1.0 : 0.0;
    r.v[1] = (a.v[1] > b.v[1]) ? -1.0 : 0.0;
    r.v[2] = (a.v[2] > b.v[2]) ? -1.0 : 0.0;
    r.v[3] = (a.v[3] > b.v[3]) ? -1.0 : 0.0;
#endif
    return r;
}

lvVec4d lv_vec4d_cmpge(lvVec4d a, lvVec4d b) {
    lvVec4d r;
#if defined(__AVX__)
    __m256d va = _mm256_loadu_pd(a.v);
    __m256d vb = _mm256_loadu_pd(b.v);
    __m256d vm = _mm256_cmp_pd(va, vb, _CMP_GE_OQ);
    _mm256_storeu_pd(r.v, vm);
#elif defined(__SSE2__)
    __m128d a_lo = _mm_loadu_pd(a.v), a_hi = _mm_loadu_pd(a.v + 2);
    __m128d b_lo = _mm_loadu_pd(b.v), b_hi = _mm_loadu_pd(b.v + 2);
    _mm_storeu_pd(r.v, _mm_cmpge_pd(a_lo, b_lo));
    _mm_storeu_pd(r.v + 2, _mm_cmpge_pd(a_hi, b_hi));
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
    float64x2_t a_lo = vld1q_f64(a.v), a_hi = vld1q_f64(a.v + 2);
    float64x2_t b_lo = vld1q_f64(b.v), b_hi = vld1q_f64(b.v + 2);
    uint64x2_t r_lo = vcgeq_f64(a_lo, b_lo);
    uint64x2_t r_hi = vcgeq_f64(a_hi, b_hi);
    vst1q_f64(r.v, (float64x2_t)r_lo);
    vst1q_f64(r.v + 2, (float64x2_t)r_hi);
#else
    r.v[0] = (a.v[0] >= b.v[0]) ? -1.0 : 0.0;
    r.v[1] = (a.v[1] >= b.v[1]) ? -1.0 : 0.0;
    r.v[2] = (a.v[2] >= b.v[2]) ? -1.0 : 0.0;
    r.v[3] = (a.v[3] >= b.v[3]) ? -1.0 : 0.0;
#endif
    return r;
}

lvVec4d lv_vec4d_select(lvVec4d mask, lvVec4d a, lvVec4d b) {
    lvVec4d r;
#if defined(__AVX__)
    __m256d vm = _mm256_loadu_pd(mask.v);
    __m256d va = _mm256_loadu_pd(a.v);
    __m256d vb = _mm256_loadu_pd(b.v);
    __m256d vr = _mm256_blendv_pd(vb, va, vm);
    _mm256_storeu_pd(r.v, vr);
#elif defined(__SSE4_1__)
    __m128d m_lo = _mm_loadu_pd(mask.v), m_hi = _mm_loadu_pd(mask.v + 2);
    __m128d a_lo = _mm_loadu_pd(a.v), a_hi = _mm_loadu_pd(a.v + 2);
    __m128d b_lo = _mm_loadu_pd(b.v), b_hi = _mm_loadu_pd(b.v + 2);
    _mm_storeu_pd(r.v, _mm_blendv_pd(b_lo, a_lo, m_lo));
    _mm_storeu_pd(r.v + 2, _mm_blendv_pd(b_hi, a_hi, m_hi));
#elif defined(__SSE2__)
    /* 使用 AND/ANDNOT/OR 模拟 blendv */
    __m128d m_lo = _mm_loadu_pd(mask.v), m_hi = _mm_loadu_pd(mask.v + 2);
    __m128d a_lo = _mm_loadu_pd(a.v), a_hi = _mm_loadu_pd(a.v + 2);
    __m128d b_lo = _mm_loadu_pd(b.v), b_hi = _mm_loadu_pd(b.v + 2);
    _mm_storeu_pd(r.v, _mm_or_pd(_mm_and_pd(m_lo, a_lo), _mm_andnot_pd(m_lo, b_lo)));
    _mm_storeu_pd(r.v + 2, _mm_or_pd(_mm_and_pd(m_hi, a_hi), _mm_andnot_pd(m_hi, b_hi)));
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
    /* NEON: vbslq_f64 selects based on the first argument's bits */
    uint64x2_t m_lo = vld1q_u64((const uint64_t*)mask.v);
    uint64x2_t m_hi = vld1q_u64((const uint64_t*)(mask.v + 2));
    float64x2_t a_lo = vld1q_f64(a.v), a_hi = vld1q_f64(a.v + 2);
    float64x2_t b_lo = vld1q_f64(b.v), b_hi = vld1q_f64(b.v + 2);
    vst1q_f64(r.v, vbslq_f64(m_lo, a_lo, b_lo));
    vst1q_f64(r.v + 2, vbslq_f64(m_hi, a_hi, b_hi));
#else
    union { double d; uint64_t u; } m, va, vb, vr;
    for (int i = 0; i < 4; i++) {
        m.d = mask.v[i]; va.d = a.v[i]; vb.d = b.v[i];
        vr.u = (va.u & m.u) | (vb.u & ~m.u);
        r.v[i] = vr.d;
    }
#endif
    return r;
}

/* 归约操作 */
double lv_vec4d_hsum(lvVec4d a) {
#if defined(__AVX__)
    __m256d va = _mm256_loadu_pd(a.v);
    __m256d vsum = _mm256_hadd_pd(va, _mm256_setzero_pd());
    return ((double*)&vsum)[0] + ((double*)&vsum)[2];
#elif defined(__SSE3__)
    __m128d vlo = _mm_loadu_pd(a.v);
    __m128d vhi = _mm_loadu_pd(a.v + 2);
    __m128d vsum = _mm_add_pd(vlo, vhi);
    vsum = _mm_hadd_pd(vsum, vsum);
    return _mm_cvtsd_f64(vsum);
#elif defined(__SSE2__)
    __m128d vlo = _mm_loadu_pd(a.v);
    __m128d vhi = _mm_loadu_pd(a.v + 2);
    __m128d vsum = _mm_add_pd(vlo, vhi);
    vsum = _mm_add_pd(vsum, _mm_shuffle_pd(vsum, vsum, 1));
    return _mm_cvtsd_f64(vsum);
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
    float64x2_t vlo = vld1q_f64(a.v);
    float64x2_t vhi = vld1q_f64(a.v + 2);
    float64x2_t vsum = vaddq_f64(vlo, vhi);
    return vgetq_lane_f64(vsum, 0) + vgetq_lane_f64(vsum, 1);
#else
    return a.v[0] + a.v[1] + a.v[2] + a.v[3];
#endif
}

double lv_vec4d_hmax(lvVec4d a) {
#if defined(__AVX__)
    __m256d va = _mm256_loadu_pd(a.v);
    __m256d vswap = _mm256_permute_pd(va, 0x05);
    __m256d vmax = _mm256_max_pd(va, vswap);
    double result = ((double*)&vmax)[0];
    if (((double*)&vmax)[2] > result) result = ((double*)&vmax)[2];
    return result;
#elif defined(__SSE2__)
    __m128d vlo = _mm_loadu_pd(a.v);
    __m128d vhi = _mm_loadu_pd(a.v + 2);
    __m128d vmax = _mm_max_pd(vlo, vhi);
    double m = _mm_cvtsd_f64(vmax);
    double m2 = _mm_cvtsd_f64(_mm_shuffle_pd(vmax, vmax, 1));
    return (m > m2) ? m : m2;
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
    float64x2_t vlo = vld1q_f64(a.v);
    float64x2_t vhi = vld1q_f64(a.v + 2);
    float64x2_t vmax = vmaxq_f64(vlo, vhi);
    double m0 = vgetq_lane_f64(vmax, 0);
    double m1 = vgetq_lane_f64(vmax, 1);
    return (m0 > m1) ? m0 : m1;
#else
    double m = a.v[0];
    if (a.v[1] > m) m = a.v[1];
    if (a.v[2] > m) m = a.v[2];
    if (a.v[3] > m) m = a.v[3];
    return m;
#endif
}

double lv_vec4d_hmin(lvVec4d a) {
#if defined(__AVX__)
    __m256d va = _mm256_loadu_pd(a.v);
    __m256d vswap = _mm256_permute_pd(va, 0x05);
    __m256d vmin = _mm256_min_pd(va, vswap);
    double result = ((double*)&vmin)[0];
    if (((double*)&vmin)[2] < result) result = ((double*)&vmin)[2];
    return result;
#elif defined(__SSE2__)
    __m128d vlo = _mm_loadu_pd(a.v);
    __m128d vhi = _mm_loadu_pd(a.v + 2);
    __m128d vmin = _mm_min_pd(vlo, vhi);
    double m = _mm_cvtsd_f64(vmin);
    double m2 = _mm_cvtsd_f64(_mm_shuffle_pd(vmin, vmin, 1));
    return (m < m2) ? m : m2;
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
    float64x2_t vlo = vld1q_f64(a.v);
    float64x2_t vhi = vld1q_f64(a.v + 2);
    float64x2_t vmin = vminq_f64(vlo, vhi);
    double m0 = vgetq_lane_f64(vmin, 0);
    double m1 = vgetq_lane_f64(vmin, 1);
    return (m0 < m1) ? m0 : m1;
#else
    double m = a.v[0];
    if (a.v[1] < m) m = a.v[1];
    if (a.v[2] < m) m = a.v[2];
    if (a.v[3] < m) m = a.v[3];
    return m;
#endif
}

double lv_vec4d_dot(lvVec4d a, lvVec4d b) {
#if defined(__AVX__)
    __m256d va = _mm256_loadu_pd(a.v);
    __m256d vb = _mm256_loadu_pd(b.v);
    __m256d vmul = _mm256_mul_pd(va, vb);
    __m256d vsum = _mm256_hadd_pd(vmul, _mm256_setzero_pd());
    /* vsum = [a0*b0+a1*b1, 0, a2*b2+a3*b3, 0] */
    double sum = ((double*)&vsum)[0] + ((double*)&vsum)[2];
    return sum;
#elif defined(__SSE2__)
    __m128d va_lo = _mm_loadu_pd(a.v);
    __m128d va_hi = _mm_loadu_pd(a.v + 2);
    __m128d vb_lo = _mm_loadu_pd(b.v);
    __m128d vb_hi = _mm_loadu_pd(b.v + 2);
    __m128d vmul_lo = _mm_mul_pd(va_lo, vb_lo);
    __m128d vmul_hi = _mm_mul_pd(va_hi, vb_hi);
    __m128d vsum = _mm_add_pd(vmul_lo, vmul_hi);
    /* vsum = [a0*b0 + a2*b2, a1*b1 + a3*b3] */
    vsum = _mm_add_pd(vsum, _mm_shuffle_pd(vsum, vsum, 1));
    return _mm_cvtsd_f64(vsum);
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
    float64x2_t va_lo = vld1q_f64(a.v);
    float64x2_t va_hi = vld1q_f64(a.v + 2);
    float64x2_t vb_lo = vld1q_f64(b.v);
    float64x2_t vb_hi = vld1q_f64(b.v + 2);
    float64x2_t vm_lo = vmulq_f64(va_lo, vb_lo);
    float64x2_t vm_hi = vmulq_f64(va_hi, vb_hi);
    float64x2_t vsum = vaddq_f64(vm_lo, vm_hi);
    return vgetq_lane_f64(vsum, 0) + vgetq_lane_f64(vsum, 1);
#else
    return a.v[0] * b.v[0] + a.v[1] * b.v[1] + a.v[2] * b.v[2] + a.v[3] * b.v[3];
#endif
}

double lv_vec4d_norm(lvVec4d a) {
#if defined(__AVX__)
    __m256d va = _mm256_loadu_pd(a.v);
    __m256d vsq = _mm256_mul_pd(va, va);
    __m256d vsum = _mm256_hadd_pd(vsq, _mm256_setzero_pd());
    double sum = ((double*)&vsum)[0] + ((double*)&vsum)[2];
    return sqrt(sum);
#elif defined(__SSE2__)
    __m128d va_lo = _mm_loadu_pd(a.v);
    __m128d va_hi = _mm_loadu_pd(a.v + 2);
    __m128d vsq_lo = _mm_mul_pd(va_lo, va_lo);
    __m128d vsq_hi = _mm_mul_pd(va_hi, va_hi);
    __m128d vsum = _mm_add_pd(vsq_lo, vsq_hi);
    vsum = _mm_add_pd(vsum, _mm_shuffle_pd(vsum, vsum, 1));
    return sqrt(_mm_cvtsd_f64(vsum));
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
    float64x2_t va_lo = vld1q_f64(a.v);
    float64x2_t va_hi = vld1q_f64(a.v + 2);
    float64x2_t vsq_lo = vmulq_f64(va_lo, va_lo);
    float64x2_t vsq_hi = vmulq_f64(va_hi, va_hi);
    float64x2_t vsum = vaddq_f64(vsq_lo, vsq_hi);
    double sum = vgetq_lane_f64(vsum, 0) + vgetq_lane_f64(vsum, 1);
    return sqrt(sum);
#else
    return sqrt(a.v[0] * a.v[0] + a.v[1] * a.v[1] + a.v[2] * a.v[2] + a.v[3] * a.v[3]);
#endif
}

lvVec4d lv_vec4d_normalize(lvVec4d a) {
    lvVec4d r;
#if defined(__AVX__)
    __m256d va = _mm256_loadu_pd(a.v);
    __m256d vsq = _mm256_mul_pd(va, va);
    __m256d vsum = _mm256_hadd_pd(vsq, _mm256_setzero_pd());
    double sum = ((double*)&vsum)[0] + ((double*)&vsum)[2];
    double norm = sqrt(sum);
    if (norm > 1e-15) {
        __m256d vnorm = _mm256_set1_pd(1.0 / norm);
        __m256d vr = _mm256_mul_pd(va, vnorm);
        _mm256_storeu_pd(r.v, vr);
    } else {
        _mm256_storeu_pd(r.v, va);
    }
#elif defined(__SSE2__)
    __m128d va_lo = _mm_loadu_pd(a.v);
    __m128d va_hi = _mm_loadu_pd(a.v + 2);
    __m128d vsq_lo = _mm_mul_pd(va_lo, va_lo);
    __m128d vsq_hi = _mm_mul_pd(va_hi, va_hi);
    __m128d vsum = _mm_add_pd(vsq_lo, vsq_hi);
    vsum = _mm_add_pd(vsum, _mm_shuffle_pd(vsum, vsum, 1));
    double norm = sqrt(_mm_cvtsd_f64(vsum));
    if (norm > 1e-15) {
        double inv_norm = 1.0 / norm;
        __m128d vinv = _mm_set1_pd(inv_norm);
        __m128d vr_lo = _mm_mul_pd(va_lo, vinv);
        __m128d vr_hi = _mm_mul_pd(va_hi, vinv);
        _mm_storeu_pd(r.v, vr_lo);
        _mm_storeu_pd(r.v + 2, vr_hi);
    } else {
        _mm_storeu_pd(r.v, va_lo);
        _mm_storeu_pd(r.v + 2, va_hi);
    }
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
    float64x2_t va_lo = vld1q_f64(a.v);
    float64x2_t va_hi = vld1q_f64(a.v + 2);
    float64x2_t vsq_lo = vmulq_f64(va_lo, va_lo);
    float64x2_t vsq_hi = vmulq_f64(va_hi, va_hi);
    float64x2_t vsum = vaddq_f64(vsq_lo, vsq_hi);
    double norm = sqrt(vgetq_lane_f64(vsum, 0) + vgetq_lane_f64(vsum, 1));
    if (norm > 1e-15) {
        double inv_norm = 1.0 / norm;
        float64x2_t vinv = vdupq_n_f64(inv_norm);
        vst1q_f64(r.v, vmulq_f64(va_lo, vinv));
        vst1q_f64(r.v + 2, vmulq_f64(va_hi, vinv));
    } else {
        vst1q_f64(r.v, va_lo);
        vst1q_f64(r.v + 2, va_hi);
    }
#else
    double norm = sqrt(a.v[0] * a.v[0] + a.v[1] * a.v[1] + a.v[2] * a.v[2] + a.v[3] * a.v[3]);
    if (norm > 1e-15) {
        r.v[0] = a.v[0] / norm;
        r.v[1] = a.v[1] / norm;
        r.v[2] = a.v[2] / norm;
        r.v[3] = a.v[3] / norm;
    } else {
        r.v[0] = a.v[0];
        r.v[1] = a.v[1];
        r.v[2] = a.v[2];
        r.v[3] = a.v[3];
    }
#endif
    lv_SIMD_STATS_INC(vec4_ops);
    return r;
}

lvVec4d lv_vec4d_cross(lvVec4d a, lvVec4d b) {
    lvVec4d r;
    /* 4D 叉积，最后一位为 0 */
    /* cross_x = a.y*b.z - a.z*b.y */
    /* cross_y = a.z*b.x - a.x*b.z */
    /* cross_z = a.x*b.y - a.y*b.x */
    /* cross_w = 0 */
#if defined(__AVX__) || defined(__SSE2__)
    double ax = a.v[0], ay = a.v[1], az = a.v[2];
    double bx = b.v[0], by = b.v[1], bz = b.v[2];
    r.v[0] = ay * bz - az * by;
    r.v[1] = az * bx - ax * bz;
    r.v[2] = ax * by - ay * bx;
    r.v[3] = 0.0;
#else
    r.v[0] = a.v[1] * b.v[2] - a.v[2] * b.v[1];
    r.v[1] = a.v[2] * b.v[0] - a.v[0] * b.v[2];
    r.v[2] = a.v[0] * b.v[1] - a.v[1] * b.v[0];
    r.v[3] = 0.0;
#endif
    lv_SIMD_STATS_INC(vec4_ops);
    return r;
}

/* ============== 4x float 向量操作 ============== */

lvVec4f lv_vec4f_zero(void) {
    lvVec4f v = {{0.0f, 0.0f, 0.0f, 0.0f}};
    lv_SIMD_STATS_INC(vec4_ops);
    return v;
}

lvVec4f lv_vec4f_set1(float val) {
    lvVec4f v = {{val, val, val, val}};
    lv_SIMD_STATS_INC(vec4_ops);
    return v;
}

lvVec4f lv_vec4f_load(const float *ptr) {
    lvVec4f v;
    v.v[0] = ptr[0];
    v.v[1] = ptr[1];
    v.v[2] = ptr[2];
    v.v[3] = ptr[3];
    lv_SIMD_STATS_INC(vec4_ops);
    return v;
}

void lv_vec4f_store(float *ptr, lvVec4f vec) {
    ptr[0] = vec.v[0];
    ptr[1] = vec.v[1];
    ptr[2] = vec.v[2];
    ptr[3] = vec.v[3];
}

lvVec4f lv_vec4f_add(lvVec4f a, lvVec4f b) {
    lvVec4f r;
#if defined(__AVX__)
    __m256 va = _mm256_loadu_ps(a.v);
    __m256 vb = _mm256_loadu_ps(b.v);
    __m256 vr = _mm256_add_ps(va, vb);
    _mm256_storeu_ps(r.v, vr);
#elif defined(__SSE2__)
    __m128 va = _mm_loadu_ps(a.v);
    __m128 vb = _mm_loadu_ps(b.v);
    __m128 vr = _mm_add_ps(va, vb);
    _mm_storeu_ps(r.v, vr);
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
    float32x4_t va = vld1q_f32(a.v);
    float32x4_t vb = vld1q_f32(b.v);
    float32x4_t vr = vaddq_f32(va, vb);
    vst1q_f32(r.v, vr);
#else
    r.v[0] = a.v[0] + b.v[0];
    r.v[1] = a.v[1] + b.v[1];
    r.v[2] = a.v[2] + b.v[2];
    r.v[3] = a.v[3] + b.v[3];
#endif
    lv_SIMD_STATS_INC(vec4_ops);
    return r;
}

lvVec4f lv_vec4f_sub(lvVec4f a, lvVec4f b) {
    lvVec4f r;
#if defined(__AVX__)
    __m256 va = _mm256_loadu_ps(a.v);
    __m256 vb = _mm256_loadu_ps(b.v);
    __m256 vr = _mm256_sub_ps(va, vb);
    _mm256_storeu_ps(r.v, vr);
#elif defined(__SSE2__)
    __m128 va = _mm_loadu_ps(a.v);
    __m128 vb = _mm_loadu_ps(b.v);
    __m128 vr = _mm_sub_ps(va, vb);
    _mm_storeu_ps(r.v, vr);
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
    float32x4_t va = vld1q_f32(a.v);
    float32x4_t vb = vld1q_f32(b.v);
    float32x4_t vr = vsubq_f32(va, vb);
    vst1q_f32(r.v, vr);
#else
    r.v[0] = a.v[0] - b.v[0];
    r.v[1] = a.v[1] - b.v[1];
    r.v[2] = a.v[2] - b.v[2];
    r.v[3] = a.v[3] - b.v[3];
#endif
    lv_SIMD_STATS_INC(vec4_ops);
    return r;
}

lvVec4f lv_vec4f_mul(lvVec4f a, lvVec4f b) {
    lvVec4f r;
#if defined(__AVX__)
    __m256 va = _mm256_loadu_ps(a.v);
    __m256 vb = _mm256_loadu_ps(b.v);
    __m256 vr = _mm256_mul_ps(va, vb);
    _mm256_storeu_ps(r.v, vr);
#elif defined(__SSE2__)
    __m128 va = _mm_loadu_ps(a.v);
    __m128 vb = _mm_loadu_ps(b.v);
    __m128 vr = _mm_mul_ps(va, vb);
    _mm_storeu_ps(r.v, vr);
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
    float32x4_t va = vld1q_f32(a.v);
    float32x4_t vb = vld1q_f32(b.v);
    float32x4_t vr = vmulq_f32(va, vb);
    vst1q_f32(r.v, vr);
#else
    r.v[0] = a.v[0] * b.v[0];
    r.v[1] = a.v[1] * b.v[1];
    r.v[2] = a.v[2] * b.v[2];
    r.v[3] = a.v[3] * b.v[3];
#endif
    lv_SIMD_STATS_INC(vec4_ops);
    return r;
}

lvVec4f lv_vec4f_div(lvVec4f a, lvVec4f b) {
    lvVec4f r;
#if defined(__AVX__)
    __m256 va = _mm256_loadu_ps(a.v);
    __m256 vb = _mm256_loadu_ps(b.v);
    __m256 vr = _mm256_div_ps(va, vb);
    _mm256_storeu_ps(r.v, vr);
#elif defined(__SSE2__)
    __m128 va = _mm_loadu_ps(a.v);
    __m128 vb = _mm_loadu_ps(b.v);
    __m128 vr = _mm_div_ps(va, vb);
    _mm_storeu_ps(r.v, vr);
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
    float32x4_t va = vld1q_f32(a.v);
    float32x4_t vb = vld1q_f32(b.v);
    float32x4_t vr = vdivq_f32(va, vb);
    vst1q_f32(r.v, vr);
#else
    r.v[0] = a.v[0] / b.v[0];
    r.v[1] = a.v[1] / b.v[1];
    r.v[2] = a.v[2] / b.v[2];
    r.v[3] = a.v[3] / b.v[3];
#endif
    lv_SIMD_STATS_INC(vec4_ops);
    return r;
}

lvVec4f lv_vec4f_sqrt(lvVec4f a) {
    lvVec4f r;
#if defined(__AVX__)
    __m256 va = _mm256_loadu_ps(a.v);
    __m256 vr = _mm256_sqrt_ps(va);
    _mm256_storeu_ps(r.v, vr);
#elif defined(__SSE2__)
    __m128 va = _mm_loadu_ps(a.v);
    __m128 vr = _mm_sqrt_ps(va);
    _mm_storeu_ps(r.v, vr);
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
    float32x4_t va = vld1q_f32(a.v);
    float32x4_t vr = vsqrtq_f32(va);
    vst1q_f32(r.v, vr);
#else
    r.v[0] = sqrtf(a.v[0]);
    r.v[1] = sqrtf(a.v[1]);
    r.v[2] = sqrtf(a.v[2]);
    r.v[3] = sqrtf(a.v[3]);
#endif
    lv_SIMD_STATS_INC(vec4_ops);
    return r;
}

float lv_vec4f_hsum(lvVec4f a) {
    return a.v[0] + a.v[1] + a.v[2] + a.v[3];
}

float lv_vec4f_dot(lvVec4f a, lvVec4f b) {
    return a.v[0] * b.v[0] + a.v[1] * b.v[1] + a.v[2] * b.v[2] + a.v[3] * b.v[3];
}

/* ============== 8x float 向量操作 ============== */

lvVec8f lv_vec8f_zero(void) {
    lvVec8f v = {{0}};
    lv_SIMD_STATS_INC(vec8_ops);
    return v;
}

lvVec8f lv_vec8f_set1(float val) {
    lvVec8f v;
#if defined(__AVX__)
    __m256 vv = _mm256_set1_ps(val);
    _mm256_storeu_ps(v.v, vv);
#elif defined(__SSE2__)
    __m128 vv = _mm_set1_ps(val);
    _mm_storeu_ps(v.v, vv);
    _mm_storeu_ps(v.v + 4, vv);
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
    float32x4_t vv = vdupq_n_f32(val);
    vst1q_f32(v.v, vv);
    vst1q_f32(v.v + 4, vv);
#else
    for (int i = 0; i < 8; i++)
        v.v[i] = val;
#endif
    lv_SIMD_STATS_INC(vec8_ops);
    return v;
}

lvVec8f lv_vec8f_load(const float *ptr) {
    lvVec8f v;
#if defined(__AVX__)
    __m256 vv = _mm256_loadu_ps(ptr);
    _mm256_storeu_ps(v.v, vv);
#elif defined(__SSE2__)
    _mm_storeu_ps(v.v, _mm_loadu_ps(ptr));
    _mm_storeu_ps(v.v + 4, _mm_loadu_ps(ptr + 4));
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
    vst1q_f32(v.v, vld1q_f32(ptr));
    vst1q_f32(v.v + 4, vld1q_f32(ptr + 4));
#else
    for (int i = 0; i < 8; i++)
        v.v[i] = ptr[i];
#endif
    lv_SIMD_STATS_INC(vec8_ops);
    return v;
}

void lv_vec8f_store(float *ptr, lvVec8f vec) {
#if defined(__AVX__)
    __m256 vv = _mm256_loadu_ps(vec.v);
    _mm256_storeu_ps(ptr, vv);
#elif defined(__SSE2__)
    _mm_storeu_ps(ptr, _mm_loadu_ps(vec.v));
    _mm_storeu_ps(ptr + 4, _mm_loadu_ps(vec.v + 4));
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
    vst1q_f32(ptr, vld1q_f32(vec.v));
    vst1q_f32(ptr + 4, vld1q_f32(vec.v + 4));
#else
    for (int i = 0; i < 8; i++)
        ptr[i] = vec.v[i];
#endif
}

lvVec8f lv_vec8f_add(lvVec8f a, lvVec8f b) {
    lvVec8f r;
#if defined(__AVX__)
    __m256 va = _mm256_loadu_ps(a.v);
    __m256 vb = _mm256_loadu_ps(b.v);
    _mm256_storeu_ps(r.v, _mm256_add_ps(va, vb));
#elif defined(__SSE2__)
    _mm_storeu_ps(r.v, _mm_add_ps(_mm_loadu_ps(a.v), _mm_loadu_ps(b.v)));
    _mm_storeu_ps(r.v + 4, _mm_add_ps(_mm_loadu_ps(a.v + 4), _mm_loadu_ps(b.v + 4)));
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
    vst1q_f32(r.v, vaddq_f32(vld1q_f32(a.v), vld1q_f32(b.v)));
    vst1q_f32(r.v + 4, vaddq_f32(vld1q_f32(a.v + 4), vld1q_f32(b.v + 4)));
#else
    for (int i = 0; i < 8; i++)
        r.v[i] = a.v[i] + b.v[i];
#endif
    lv_SIMD_STATS_INC(vec8_ops);
    return r;
}

lvVec8f lv_vec8f_sub(lvVec8f a, lvVec8f b) {
    lvVec8f r;
#if defined(__AVX__)
    __m256 va = _mm256_loadu_ps(a.v);
    __m256 vb = _mm256_loadu_ps(b.v);
    _mm256_storeu_ps(r.v, _mm256_sub_ps(va, vb));
#elif defined(__SSE2__)
    _mm_storeu_ps(r.v, _mm_sub_ps(_mm_loadu_ps(a.v), _mm_loadu_ps(b.v)));
    _mm_storeu_ps(r.v + 4, _mm_sub_ps(_mm_loadu_ps(a.v + 4), _mm_loadu_ps(b.v + 4)));
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
    vst1q_f32(r.v, vsubq_f32(vld1q_f32(a.v), vld1q_f32(b.v)));
    vst1q_f32(r.v + 4, vsubq_f32(vld1q_f32(a.v + 4), vld1q_f32(b.v + 4)));
#else
    for (int i = 0; i < 8; i++)
        r.v[i] = a.v[i] - b.v[i];
#endif
    lv_SIMD_STATS_INC(vec8_ops);
    return r;
}

lvVec8f lv_vec8f_mul(lvVec8f a, lvVec8f b) {
    lvVec8f r;
#if defined(__AVX__)
    __m256 va = _mm256_loadu_ps(a.v);
    __m256 vb = _mm256_loadu_ps(b.v);
    _mm256_storeu_ps(r.v, _mm256_mul_ps(va, vb));
#elif defined(__SSE2__)
    _mm_storeu_ps(r.v, _mm_mul_ps(_mm_loadu_ps(a.v), _mm_loadu_ps(b.v)));
    _mm_storeu_ps(r.v + 4, _mm_mul_ps(_mm_loadu_ps(a.v + 4), _mm_loadu_ps(b.v + 4)));
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
    vst1q_f32(r.v, vmulq_f32(vld1q_f32(a.v), vld1q_f32(b.v)));
    vst1q_f32(r.v + 4, vmulq_f32(vld1q_f32(a.v + 4), vld1q_f32(b.v + 4)));
#else
    for (int i = 0; i < 8; i++)
        r.v[i] = a.v[i] * b.v[i];
#endif
    lv_SIMD_STATS_INC(vec8_ops);
    return r;
}

lvVec8f lv_vec8f_div(lvVec8f a, lvVec8f b) {
    lvVec8f r;
#if defined(__AVX__)
    __m256 va = _mm256_loadu_ps(a.v);
    __m256 vb = _mm256_loadu_ps(b.v);
    _mm256_storeu_ps(r.v, _mm256_div_ps(va, vb));
#elif defined(__SSE2__)
    _mm_storeu_ps(r.v, _mm_div_ps(_mm_loadu_ps(a.v), _mm_loadu_ps(b.v)));
    _mm_storeu_ps(r.v + 4, _mm_div_ps(_mm_loadu_ps(a.v + 4), _mm_loadu_ps(b.v + 4)));
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
    vst1q_f32(r.v, vdivq_f32(vld1q_f32(a.v), vld1q_f32(b.v)));
    vst1q_f32(r.v + 4, vdivq_f32(vld1q_f32(a.v + 4), vld1q_f32(b.v + 4)));
#else
    for (int i = 0; i < 8; i++)
        r.v[i] = a.v[i] / b.v[i];
#endif
    lv_SIMD_STATS_INC(vec8_ops);
    return r;
}

float lv_vec8f_hsum(lvVec8f a) {
#if defined(__AVX__)
    __m256 va = _mm256_loadu_ps(a.v);
    __m128 vsum = _mm_add_ps(_mm256_castps256_ps128(va), _mm256_extractf128_ps(va, 1));
    vsum = _mm_hadd_ps(vsum, vsum);
    vsum = _mm_hadd_ps(vsum, vsum);
    return _mm_cvtss_f32(vsum);
#elif defined(__SSE2__)
    __m128 vlo = _mm_loadu_ps(a.v);
    __m128 vhi = _mm_loadu_ps(a.v + 4);
    __m128 vsum = _mm_add_ps(vlo, vhi);
    vsum = _mm_hadd_ps(vsum, vsum);
    vsum = _mm_hadd_ps(vsum, vsum);
    return _mm_cvtss_f32(vsum);
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
    float32x4_t vlo = vld1q_f32(a.v);
    float32x4_t vhi = vld1q_f32(a.v + 4);
    float32x4_t vsum = vaddq_f32(vlo, vhi);
    return vgetq_lane_f32(vsum, 0) + vgetq_lane_f32(vsum, 1)
         + vgetq_lane_f32(vsum, 2) + vgetq_lane_f32(vsum, 3);
#else
    float sum = 0.0f;
    for (int i = 0; i < 8; i++)
        sum += a.v[i];
    return sum;
#endif
}

/* ============== 批量运算 ============== */

void lv_simd_add_array_d(const double *a, const double *b, double *out, size_t count) {
    lv_SIMD_STATS_INC(array_ops);
    lv_SIMD_STATS_ADD(elements_processed, count);

    size_t i = 0;

    /* 4元素向量处理 */
    for (; i + 4 <= count; i += 4) {
        lvVec4d va = lv_vec4d_load(a + i);
        lvVec4d vb = lv_vec4d_load(b + i);
        lvVec4d vr = lv_vec4d_add(va, vb);
        lv_vec4d_store(out + i, vr);
    }

    /* 剩余元素 */
    for (; i < count; i++) {
        out[i] = a[i] + b[i];
    }
}

void lv_simd_mul_array_d(const double *a, const double *b, double *out, size_t count) {
    lv_SIMD_STATS_INC(array_ops);
    lv_SIMD_STATS_ADD(elements_processed, count);

    size_t i = 0;
    for (; i + 4 <= count; i += 4) {
        lvVec4d va = lv_vec4d_load(a + i);
        lvVec4d vb = lv_vec4d_load(b + i);
        lvVec4d vr = lv_vec4d_mul(va, vb);
        lv_vec4d_store(out + i, vr);
    }

    for (; i < count; i++) {
        out[i] = a[i] * b[i];
    }
}

void lv_simd_fmadd_array_d(const double *a, const double *b, const double *c, double *out, size_t count) {
    lv_SIMD_STATS_INC(array_ops);
    lv_SIMD_STATS_ADD(elements_processed, count);

    size_t i = 0;
    for (; i + 4 <= count; i += 4) {
        lvVec4d va = lv_vec4d_load(a + i);
        lvVec4d vb = lv_vec4d_load(b + i);
        lvVec4d vc = lv_vec4d_load(c + i);
        lvVec4d vr = lv_vec4d_fmadd(va, vb, vc);
        lv_vec4d_store(out + i, vr);
    }

    for (; i < count; i++) {
        out[i] = a[i] * b[i] + c[i];
    }
}

double lv_simd_sum_array_d(const double *arr, size_t count) {
    lv_SIMD_STATS_INC(array_ops);
    lv_SIMD_STATS_ADD(elements_processed, count);

    double sum = 0.0;
    size_t i = 0;

    /* 4元素向量累加 */
    lvVec4d vsum = lv_vec4d_zero();
    for (; i + 4 <= count; i += 4) {
        lvVec4d v = lv_vec4d_load(arr + i);
        vsum = lv_vec4d_add(vsum, v);
    }
    sum = lv_vec4d_hsum(vsum);

    /* 剩余元素 */
    for (; i < count; i++) {
        sum += arr[i];
    }

    return sum;
}

double lv_simd_dot_array_d(const double *a, const double *b, size_t count) {
    lv_SIMD_STATS_INC(array_ops);
    lv_SIMD_STATS_ADD(elements_processed, count);

    double dot = 0.0;
    size_t i = 0;

    lvVec4d vdot = lv_vec4d_zero();
    for (; i + 4 <= count; i += 4) {
        lvVec4d va = lv_vec4d_load(a + i);
        lvVec4d vb = lv_vec4d_load(b + i);
        vdot = lv_vec4d_fmadd(va, vb, vdot);
    }
    dot = lv_vec4d_hsum(vdot);

    for (; i < count; i++) {
        dot += a[i] * b[i];
    }

    return dot;
}

void lv_simd_scale_array_d(const double *in, double scale, double *out, size_t count) {
    lv_SIMD_STATS_INC(array_ops);
    lv_SIMD_STATS_ADD(elements_processed, count);

    lvVec4d vscale = lv_vec4d_set1(scale);
    size_t i = 0;

    for (; i + 4 <= count; i += 4) {
        lvVec4d v = lv_vec4d_load(in + i);
        lvVec4d vr = lv_vec4d_mul(v, vscale);
        lv_vec4d_store(out + i, vr);
    }

    for (; i < count; i++) {
        out[i] = in[i] * scale;
    }
}

double lv_simd_dot_product_array(const double *a, const double *b, size_t count) {
    lv_SIMD_STATS_INC(array_ops);
    lv_SIMD_STATS_ADD(elements_processed, count);

    double dot = 0.0;
    size_t i = 0;

#if defined(__AVX__)
    __m256d vdot = _mm256_setzero_pd();
    for (; i + 4 <= count; i += 4) {
        __m256d va = _mm256_loadu_pd(a + i);
        __m256d vb = _mm256_loadu_pd(b + i);
        vdot = _mm256_add_pd(vdot, _mm256_mul_pd(va, vb));
    }
    __m256d vsum = _mm256_hadd_pd(vdot, _mm256_setzero_pd());
    dot = ((double*)&vsum)[0] + ((double*)&vsum)[2];
#elif defined(__SSE2__)
    __m128d vdot_lo = _mm_setzero_pd();
    __m128d vdot_hi = _mm_setzero_pd();
    for (; i + 4 <= count; i += 4) {
        __m128d va_lo = _mm_loadu_pd(a + i);
        __m128d va_hi = _mm_loadu_pd(a + i + 2);
        __m128d vb_lo = _mm_loadu_pd(b + i);
        __m128d vb_hi = _mm_loadu_pd(b + i + 2);
        vdot_lo = _mm_add_pd(vdot_lo, _mm_mul_pd(va_lo, vb_lo));
        vdot_hi = _mm_add_pd(vdot_hi, _mm_mul_pd(va_hi, vb_hi));
    }
    __m128d vsum = _mm_add_pd(vdot_lo, vdot_hi);
    vsum = _mm_add_pd(vsum, _mm_shuffle_pd(vsum, vsum, 1));
    dot = _mm_cvtsd_f64(vsum);
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
    float64x2_t vdot_lo = vdupq_n_f64(0.0);
    float64x2_t vdot_hi = vdupq_n_f64(0.0);
    for (; i + 4 <= count; i += 4) {
        float64x2_t va_lo = vld1q_f64(a + i);
        float64x2_t va_hi = vld1q_f64(a + i + 2);
        float64x2_t vb_lo = vld1q_f64(b + i);
        float64x2_t vb_hi = vld1q_f64(b + i + 2);
        vdot_lo = vaddq_f64(vdot_lo, vmulq_f64(va_lo, vb_lo));
        vdot_hi = vaddq_f64(vdot_hi, vmulq_f64(va_hi, vb_hi));
    }
    float64x2_t vsum = vaddq_f64(vdot_lo, vdot_hi);
    dot = vgetq_lane_f64(vsum, 0) + vgetq_lane_f64(vsum, 1);
#else
    lvVec4d vdot = lv_vec4d_zero();
    for (; i + 4 <= count; i += 4) {
        lvVec4d va = lv_vec4d_load(a + i);
        lvVec4d vb = lv_vec4d_load(b + i);
        vdot = lv_vec4d_fmadd(va, vb, vdot);
    }
    dot = lv_vec4d_hsum(vdot);
#endif

    for (; i < count; i++) {
        dot += a[i] * b[i];
    }

    return dot;
}

void lv_simd_norm_array(const double *in, double *out, size_t count) {
    lv_SIMD_STATS_INC(array_ops);
    lv_SIMD_STATS_ADD(elements_processed, count);

    size_t i = 0;

    /* 计算每个 4D 向量的范数：sqrt(x0^2 + x1^2 + x2^2 + x3^2) */
#if defined(__AVX__)
    for (; i < count; i++) {
        __m256d v = _mm256_loadu_pd(in + i * 4);
        __m256d vsq = _mm256_mul_pd(v, v);
        __m256d vsum = _mm256_hadd_pd(vsq, _mm256_setzero_pd());
        double s = ((double*)&vsum)[0] + ((double*)&vsum)[2];
        out[i] = sqrt(s);
    }
#elif defined(__SSE2__)
    for (; i < count; i++) {
        __m128d v_lo = _mm_loadu_pd(in + i * 4);
        __m128d v_hi = _mm_loadu_pd(in + i * 4 + 2);
        __m128d vsq_lo = _mm_mul_pd(v_lo, v_lo);
        __m128d vsq_hi = _mm_mul_pd(v_hi, v_hi);
        __m128d vsum = _mm_add_pd(vsq_lo, vsq_hi);
        vsum = _mm_add_pd(vsum, _mm_shuffle_pd(vsum, vsum, 1));
        out[i] = sqrt(_mm_cvtsd_f64(vsum));
    }
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
    for (; i < count; i++) {
        float64x2_t v_lo = vld1q_f64(in + i * 4);
        float64x2_t v_hi = vld1q_f64(in + i * 4 + 2);
        float64x2_t vsq_lo = vmulq_f64(v_lo, v_lo);
        float64x2_t vsq_hi = vmulq_f64(v_hi, v_hi);
        float64x2_t vsum = vaddq_f64(vsq_lo, vsq_hi);
        out[i] = sqrt(vgetq_lane_f64(vsum, 0) + vgetq_lane_f64(vsum, 1));
    }
#else
    for (; i < count; i++) {
        double s = in[i * 4] * in[i * 4] + in[i * 4 + 1] * in[i * 4 + 1]
                 + in[i * 4 + 2] * in[i * 4 + 2] + in[i * 4 + 3] * in[i * 4 + 3];
        out[i] = sqrt(s);
    }
#endif
}

void lv_simd_scale_array(const double *in, double scale, double *out, size_t count) {
    lv_SIMD_STATS_INC(array_ops);
    lv_SIMD_STATS_ADD(elements_processed, count);

    size_t i = 0;

#if defined(__AVX__)
    __m256d vscale = _mm256_set1_pd(scale);
    for (; i + 4 <= count; i += 4) {
        __m256d v = _mm256_loadu_pd(in + i);
        _mm256_storeu_pd(out + i, _mm256_mul_pd(v, vscale));
    }
#elif defined(__SSE2__)
    __m128d vscale = _mm_set1_pd(scale);
    for (; i + 4 <= count; i += 4) {
        __m128d v_lo = _mm_loadu_pd(in + i);
        __m128d v_hi = _mm_loadu_pd(in + i + 2);
        _mm_storeu_pd(out + i, _mm_mul_pd(v_lo, vscale));
        _mm_storeu_pd(out + i + 2, _mm_mul_pd(v_hi, vscale));
    }
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
    float64x2_t vscale = vdupq_n_f64(scale);
    for (; i + 4 <= count; i += 4) {
        float64x2_t v_lo = vld1q_f64(in + i);
        float64x2_t v_hi = vld1q_f64(in + i + 2);
        vst1q_f64(out + i, vmulq_f64(v_lo, vscale));
        vst1q_f64(out + i + 2, vmulq_f64(v_hi, vscale));
    }
#else
    lvVec4d vscale = lv_vec4d_set1(scale);
    for (; i + 4 <= count; i += 4) {
        lvVec4d v = lv_vec4d_load(in + i);
        lvVec4d vr = lv_vec4d_mul(v, vscale);
        lv_vec4d_store(out + i, vr);
    }
#endif

    for (; i < count; i++) {
        out[i] = in[i] * scale;
    }
}

double lv_simd_max_array_d(const double *arr, size_t count) {
    if (count == 0)
        return 0.0;

    lv_SIMD_STATS_INC(array_ops);
    lv_SIMD_STATS_ADD(elements_processed, count);

    double max_val = arr[0];
    size_t i = 1;

    for (; i + 4 <= count; i += 4) {
        lvVec4d v = lv_vec4d_load(arr + i);
        max_val = (lv_vec4d_hmax(v) > max_val) ? lv_vec4d_hmax(v) : max_val;
    }

    for (; i < count; i++) {
        if (arr[i] > max_val)
            max_val = arr[i];
    }

    return max_val;
}

double lv_simd_min_array_d(const double *arr, size_t count) {
    if (count == 0)
        return 0.0;

    lv_SIMD_STATS_INC(array_ops);
    lv_SIMD_STATS_ADD(elements_processed, count);

    double min_val = arr[0];
    size_t i = 1;

    for (; i + 4 <= count; i += 4) {
        lvVec4d v = lv_vec4d_load(arr + i);
        min_val = (lv_vec4d_hmin(v) < min_val) ? lv_vec4d_hmin(v) : min_val;
    }

    for (; i < count; i++) {
        if (arr[i] < min_val)
            min_val = arr[i];
    }

    return min_val;
}

/* ============== 几何运算加速 ============== */

void lv_simd_distance_array(const double *x1, const double *y1, const double *x2, const double *y2, double *out,
                            size_t count) {
    lv_SIMD_STATS_INC(array_ops);
    lv_SIMD_STATS_ADD(elements_processed, count);

    for (size_t i = 0; i < count; i++) {
        double dx = x2[i] - x1[i];
        double dy = y2[i] - y1[i];
        out[i] = sqrt(dx * dx + dy * dy);
    }
}

void lv_simd_point_line_distance_array(const double *px, const double *py, double x1, double y1, double x2, double y2,
                                       double *out, size_t count) {
    lv_SIMD_STATS_INC(array_ops);
    lv_SIMD_STATS_ADD(elements_processed, count);

    double dx = x2 - x1;
    double dy = y2 - y1;
    double len_sq = dx * dx + dy * dy;

    if (len_sq < 1e-12) {
        /* 线段退化为点 */
        for (size_t i = 0; i < count; i++) {
            double ddx = px[i] - x1;
            double ddy = py[i] - y1;
            out[i] = sqrt(ddx * ddx + ddy * ddy);
        }
        return;
    }

    double len = sqrt(len_sq);
    double nx = -dy / len; /* 法向量 */
    double ny = dx / len;

    for (size_t i = 0; i < count; i++) {
        double ddx = px[i] - x1;
        double ddy = py[i] - y1;
        out[i] = fabs(ddx * nx + ddy * ny);
    }
}

void lv_simd_cross2d_array(const double *ax, const double *ay, const double *bx, const double *by, double *out,
                           size_t count) {
    lv_SIMD_STATS_INC(array_ops);
    lv_SIMD_STATS_ADD(elements_processed, count);

    size_t i = 0;
    for (; i + 4 <= count; i += 4) {
        lvVec4d vax = lv_vec4d_load(ax + i);
        lvVec4d vay = lv_vec4d_load(ay + i);
        lvVec4d vbx = lv_vec4d_load(bx + i);
        lvVec4d vby = lv_vec4d_load(by + i);

        /* cross = ax * by - ay * bx */
        lvVec4d v1 = lv_vec4d_mul(vax, vby);
        lvVec4d v2 = lv_vec4d_mul(vay, vbx);
        lvVec4d vr = lv_vec4d_sub(v1, v2);

        lv_vec4d_store(out + i, vr);
    }

    for (; i < count; i++) {
        out[i] = ax[i] * by[i] - ay[i] * bx[i];
    }
}

void lv_simd_point_in_circle_array(const double *px, const double *py, double cx, double cy, double r, int *out,
                                   size_t count) {
    lv_SIMD_STATS_INC(array_ops);
    lv_SIMD_STATS_ADD(elements_processed, count);

    double r_sq = r * r;

    for (size_t i = 0; i < count; i++) {
        double dx = px[i] - cx;
        double dy = py[i] - cy;
        double dist_sq = dx * dx + dy * dy;
        out[i] = (dist_sq <= r_sq) ? 1 : 0;
    }
}

/* ============== 矩阵运算 ============== */

lvVec4d lv_simd_mat4x4_vec4_mul(const double mat[16], lvVec4d vec) {
    lvVec4d result;

    result.v[0] = mat[0] * vec.v[0] + mat[1] * vec.v[1] + mat[2] * vec.v[2] + mat[3] * vec.v[3];
    result.v[1] = mat[4] * vec.v[0] + mat[5] * vec.v[1] + mat[6] * vec.v[2] + mat[7] * vec.v[3];
    result.v[2] = mat[8] * vec.v[0] + mat[9] * vec.v[1] + mat[10] * vec.v[2] + mat[11] * vec.v[3];
    result.v[3] = mat[12] * vec.v[0] + mat[13] * vec.v[1] + mat[14] * vec.v[2] + mat[15] * vec.v[3];

    lv_SIMD_STATS_INC(vec4_ops);
    return result;
}

void lv_simd_mat4x4_vec4_array_mul(const double mat[16], const double *vecs, double *out, size_t count) {
    lv_SIMD_STATS_INC(array_ops);
    lv_SIMD_STATS_ADD(elements_processed, count);

    for (size_t i = 0; i < count; i++) {
        lvVec4d v = lv_vec4d_load(vecs + i * 4);
        lvVec4d r = lv_simd_mat4x4_vec4_mul(mat, v);
        lv_vec4d_store(out + i * 4, r);
    }
}

void lv_simd_mat3x3_vec2_mul(const double mat[9], double x, double y, double *out_x, double *out_y) {
    /* 齐次坐标变换 */
    double w = mat[6] * x + mat[7] * y + mat[8];
    *out_x = (mat[0] * x + mat[1] * y + mat[2]) / w;
    *out_y = (mat[3] * x + mat[4] * y + mat[5]) / w;
}
