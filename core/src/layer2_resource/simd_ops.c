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
#include "lv/geo_utils.h"

#include <math.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

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
 *
 * 注（P1-3 标注）：保持 C11 atomic 而非迁移 lv_ATOMIC_* —— 统计字段为
 * _Atomic uint64_t，而 lv_ATOMIC_* 宏族仅有 32 位 LOAD/STORE（64 位仅
 * INC64/DEC64/ADD64），无 64 位 LOAD/STORE 原语可表达本处的 load/store；
 * 且一次性初始化依赖 compare_exchange_strong 的失败更新 expected 语义。
 * 此处 relaxed 累加/读取语义与宏族不完全等价，故保留 C11 原子类型。
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

/* 轻量计时：simd_time_us 真实计数（可简化：仅批量数组操作级计时，逐向量操作计时开销过大不作） */
static uint64_t lv_simd_now_us(void) {
    return lv_get_time_us();
}

#define lv_SIMD_TIME_BEGIN() uint64_t lv_t0 = lv_simd_now_us()
#define lv_SIMD_TIME_END() lv_SIMD_STATS_ADD(simd_time_us, lv_simd_now_us() - lv_t0)

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
#if defined(__AVX__)
    _mm256_storeu_pd(v.v, _mm256_load_pd(ptr));
#elif defined(__SSE2__)
    _mm_storeu_pd(v.v, _mm_load_pd(ptr));
    _mm_storeu_pd(v.v + 2, _mm_load_pd(ptr + 2));
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
    vst1q_f64(v.v, vld1q_f64(ptr));
    vst1q_f64(v.v + 2, vld1q_f64(ptr + 2));
#else
    v.v[0] = ptr[0];
    v.v[1] = ptr[1];
    v.v[2] = ptr[2];
    v.v[3] = ptr[3];
#endif
    lv_SIMD_STATS_INC(vec4_ops);
    return v;
}

lvVec4d lv_vec4d_loadu(const double *ptr) {
    lvVec4d v;
#if defined(__AVX__)
    _mm256_storeu_pd(v.v, _mm256_loadu_pd(ptr));
#elif defined(__SSE2__)
    _mm_storeu_pd(v.v, _mm_loadu_pd(ptr));
    _mm_storeu_pd(v.v + 2, _mm_loadu_pd(ptr + 2));
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
    vst1q_f64(v.v, vld1q_f64(ptr));
    vst1q_f64(v.v + 2, vld1q_f64(ptr + 2));
#else
    v.v[0] = ptr[0];
    v.v[1] = ptr[1];
    v.v[2] = ptr[2];
    v.v[3] = ptr[3];
#endif
    lv_SIMD_STATS_INC(vec4_ops);
    return v;
}

void lv_vec4d_store(double *ptr, lvVec4d vec) {
#if defined(__AVX__)
    _mm256_store_pd(ptr, _mm256_loadu_pd(vec.v));
#elif defined(__SSE2__)
    _mm_store_pd(ptr, _mm_loadu_pd(vec.v));
    _mm_store_pd(ptr + 2, _mm_loadu_pd(vec.v + 2));
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
    vst1q_f64(ptr, vld1q_f64(vec.v));
    vst1q_f64(ptr + 2, vld1q_f64(vec.v + 2));
#else
    ptr[0] = vec.v[0];
    ptr[1] = vec.v[1];
    ptr[2] = vec.v[2];
    ptr[3] = vec.v[3];
#endif
    lv_SIMD_STATS_INC(vec4_ops);
}

void lv_vec4d_storeu(double *ptr, lvVec4d vec) {
#if defined(__AVX__)
    _mm256_storeu_pd(ptr, _mm256_loadu_pd(vec.v));
#elif defined(__SSE2__)
    _mm_storeu_pd(ptr, _mm_loadu_pd(vec.v));
    _mm_storeu_pd(ptr + 2, _mm_loadu_pd(vec.v + 2));
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
    vst1q_f64(ptr, vld1q_f64(vec.v));
    vst1q_f64(ptr + 2, vld1q_f64(vec.v + 2));
#else
    ptr[0] = vec.v[0];
    ptr[1] = vec.v[1];
    ptr[2] = vec.v[2];
    ptr[3] = vec.v[3];
#endif
    lv_SIMD_STATS_INC(vec4_ops);
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
#if defined(__AVX__) && defined(__FMA__)
    __m256d va = _mm256_loadu_pd(a.v);
    __m256d vx = _mm256_loadu_pd(x.v);
    __m256d vy = _mm256_loadu_pd(y.v);
    __m256d vr = _mm256_fmadd_pd(va, vx, vy);
    _mm256_storeu_pd(r.v, vr);
#elif defined(__AVX__)
    __m256d va = _mm256_loadu_pd(a.v);
    __m256d vx = _mm256_loadu_pd(x.v);
    __m256d vy = _mm256_loadu_pd(y.v);
    __m256d vr = _mm256_add_pd(_mm256_mul_pd(va, vx), vy);
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
    lv_SIMD_STATS_INC(vec4_ops);
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
    lv_SIMD_STATS_INC(vec4_ops);
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
    lv_SIMD_STATS_INC(vec4_ops);
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
    lv_SIMD_STATS_INC(vec4_ops);
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
    lv_SIMD_STATS_INC(vec4_ops);
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
    lv_SIMD_STATS_INC(vec4_ops);
    return r;
}

/* 归约操作 */
double lv_vec4d_hsum(lvVec4d a) {
    lv_SIMD_STATS_INC(vec4_ops);
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
    lv_SIMD_STATS_INC(vec4_ops);
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
    lv_SIMD_STATS_INC(vec4_ops);
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
    lv_SIMD_STATS_INC(vec4_ops);
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
    lv_SIMD_STATS_INC(vec4_ops);
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
    if (norm > lv_NORMALIZATION_THRESHOLD) {
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
    if (norm > lv_NORMALIZATION_THRESHOLD) {
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
    if (norm > lv_NORMALIZATION_THRESHOLD) {
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
    if (norm > lv_NORMALIZATION_THRESHOLD) {
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
#if defined(__SSE2__)   /* 覆盖 AVX（__AVX__ 隐含 __SSE2__），4 个 double 用 2×128 位即可 */
    __m128d a_lo = _mm_loadu_pd(a.v), a_hi = _mm_loadu_pd(a.v + 2);
    __m128d b_lo = _mm_loadu_pd(b.v), b_hi = _mm_loadu_pd(b.v + 2);
    /* 车道重排：ayz=[a1,a2] azx=[a2,a0] bzy=[b2,b1] bxz=[b0,b2] */
    __m128d ayz = _mm_shuffle_pd(a_lo, a_hi, 0x01);
    __m128d azx = _mm_shuffle_pd(a_hi, a_lo, 0x00);
    __m128d bzy = _mm_shuffle_pd(b_hi, b_lo, 0x02);
    __m128d bxz = _mm_shuffle_pd(b_lo, b_hi, 0x00);
    __m128d txy = _mm_mul_pd(ayz, bzy); /* [a1*b2, a2*b1] */
    __m128d tyz = _mm_mul_pd(azx, bxz); /* [a2*b0, a0*b2] */
    r.v[0] = _mm_cvtsd_f64(_mm_sub_pd(txy, _mm_shuffle_pd(txy, txy, 0x01)));
    r.v[1] = _mm_cvtsd_f64(_mm_sub_pd(tyz, _mm_shuffle_pd(tyz, tyz, 0x01)));
    __m128d p = _mm_mul_pd(a_lo, _mm_shuffle_pd(b_lo, b_lo, 0x01)); /* [a0*b1, a1*b0] */
    r.v[2] = _mm_cvtsd_f64(_mm_sub_pd(p, _mm_shuffle_pd(p, p, 0x01)));
    r.v[3] = 0.0;
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
    float64x2_t a_lo = vld1q_f64(a.v), a_hi = vld1q_f64(a.v + 2);
    float64x2_t b_lo = vld1q_f64(b.v), b_hi = vld1q_f64(b.v + 2);
    float64x2_t ayz = vextq_f64(a_lo, a_hi, 1);   /* [a1, a2] */
    float64x2_t azx = vextq_f64(a_hi, a_lo, 1);   /* [a2, a0] */
    float64x2_t bzy = vdupq_n_f64(vgetq_lane_f64(b_hi, 0));
    bzy = vsetq_lane_f64(vgetq_lane_f64(b_lo, 1), bzy, 1); /* [b2, b1] */
    float64x2_t bxz = vdupq_n_f64(vgetq_lane_f64(b_lo, 0));
    bxz = vsetq_lane_f64(vgetq_lane_f64(b_hi, 0), bxz, 1); /* [b0, b2] */
    float64x2_t txy = vmulq_f64(ayz, bzy); /* [a1*b2, a2*b1] */
    float64x2_t tyz = vmulq_f64(azx, bxz); /* [a2*b0, a0*b2] */
    r.v[0] = vgetq_lane_f64(txy, 0) - vgetq_lane_f64(txy, 1);
    r.v[1] = vgetq_lane_f64(tyz, 0) - vgetq_lane_f64(tyz, 1);
    float64x2_t p = vmulq_f64(a_lo, vextq_f64(b_lo, b_lo, 1)); /* [a0*b1, a1*b0] */
    r.v[2] = vgetq_lane_f64(p, 0) - vgetq_lane_f64(p, 1);
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
    lvVec4f v;
#if defined(__SSE2__)   /* 覆盖 AVX */
    _mm_storeu_ps(v.v, _mm_setzero_ps());
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
    vst1q_f32(v.v, vdupq_n_f32(0.0f));
#else
    v.v[0] = 0.0f; v.v[1] = 0.0f; v.v[2] = 0.0f; v.v[3] = 0.0f;
#endif
    lv_SIMD_STATS_INC(vec4_ops);
    return v;
}

lvVec4f lv_vec4f_set1(float val) {
    lvVec4f v;
#if defined(__SSE2__)   /* 覆盖 AVX */
    _mm_storeu_ps(v.v, _mm_set1_ps(val));
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
    vst1q_f32(v.v, vdupq_n_f32(val));
#else
    v.v[0] = val; v.v[1] = val; v.v[2] = val; v.v[3] = val;
#endif
    lv_SIMD_STATS_INC(vec4_ops);
    return v;
}

lvVec4f lv_vec4f_load(const float *ptr) {
    lvVec4f v;
#if defined(__SSE2__)   /* 覆盖 AVX */
    _mm_storeu_ps(v.v, _mm_loadu_ps(ptr));
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
    vst1q_f32(v.v, vld1q_f32(ptr));
#else
    v.v[0] = ptr[0];
    v.v[1] = ptr[1];
    v.v[2] = ptr[2];
    v.v[3] = ptr[3];
#endif
    lv_SIMD_STATS_INC(vec4_ops);
    return v;
}

void lv_vec4f_store(float *ptr, lvVec4f vec) {
#if defined(__SSE2__)   /* 覆盖 AVX */
    _mm_storeu_ps(ptr, _mm_loadu_ps(vec.v));
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
    vst1q_f32(ptr, vld1q_f32(vec.v));
#else
    ptr[0] = vec.v[0];
    ptr[1] = vec.v[1];
    ptr[2] = vec.v[2];
    ptr[3] = vec.v[3];
#endif
    lv_SIMD_STATS_INC(vec4_ops);
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
    lv_SIMD_STATS_INC(vec4_ops);
#if defined(__SSE2__)   /* 覆盖 AVX */
    __m128 v = _mm_loadu_ps(a.v);
    __m128 t = _mm_add_ps(v, _mm_shuffle_ps(v, v, _MM_SHUFFLE(2, 3, 0, 1)));
    t = _mm_add_ps(t, _mm_shuffle_ps(t, t, _MM_SHUFFLE(1, 0, 3, 2)));
    return _mm_cvtss_f32(t);
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
    float32x4_t v = vld1q_f32(a.v);
    float32x2_t r = vpadd_f32(vget_low_f32(v), vget_high_f32(v));
    r = vpadd_f32(r, r);
    return vget_lane_f32(r, 0);
#else
    return a.v[0] + a.v[1] + a.v[2] + a.v[3];
#endif
}

float lv_vec4f_dot(lvVec4f a, lvVec4f b) {
    lv_SIMD_STATS_INC(vec4_ops);
#if defined(__SSE2__)   /* 覆盖 AVX */
    __m128 v = _mm_mul_ps(_mm_loadu_ps(a.v), _mm_loadu_ps(b.v));
    __m128 t = _mm_add_ps(v, _mm_shuffle_ps(v, v, _MM_SHUFFLE(2, 3, 0, 1)));
    t = _mm_add_ps(t, _mm_shuffle_ps(t, t, _MM_SHUFFLE(1, 0, 3, 2)));
    return _mm_cvtss_f32(t);
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
    float32x4_t v = vmulq_f32(vld1q_f32(a.v), vld1q_f32(b.v));
    float32x2_t r = vpadd_f32(vget_low_f32(v), vget_high_f32(v));
    r = vpadd_f32(r, r);
    return vget_lane_f32(r, 0);
#else
    return a.v[0] * b.v[0] + a.v[1] * b.v[1] + a.v[2] * b.v[2] + a.v[3] * b.v[3];
#endif
}

/* ============== 8x float 向量操作 ============== */

lvVec8f lv_vec8f_zero(void) {
    lvVec8f v;
#if defined(__AVX__)
    _mm256_storeu_ps(v.v, _mm256_setzero_ps());
#elif defined(__SSE2__)
    _mm_storeu_ps(v.v, _mm_setzero_ps());
    _mm_storeu_ps(v.v + 4, _mm_setzero_ps());
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
    float32x4_t z = vdupq_n_f32(0.0f);
    vst1q_f32(v.v, z);
    vst1q_f32(v.v + 4, z);
#else
    for (int i = 0; i < 8; i++)
        v.v[i] = 0.0f;
#endif
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
    lv_SIMD_STATS_INC(vec8_ops);
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
    lv_SIMD_STATS_INC(vec8_ops);
#if defined(__AVX__)
    __m256 va = _mm256_loadu_ps(a.v);
    __m128 vsum = _mm_add_ps(_mm256_castps256_ps128(va), _mm256_extractf128_ps(va, 1));
    vsum = _mm_hadd_ps(vsum, vsum);
    vsum = _mm_hadd_ps(vsum, vsum);
    return _mm_cvtss_f32(vsum);
#elif defined(__SSE2__)
    __m128 vlo = _mm_loadu_ps(a.v);
    __m128 vhi = _mm_loadu_ps(a.v + 4);
    __m128 s = _mm_add_ps(vlo, vhi);                                   /* [a0+a4, a1+a5, a2+a6, a3+a7] */
    s = _mm_add_ps(s, _mm_shuffle_ps(s, s, _MM_SHUFFLE(2, 3, 0, 1)));  /* + [a2+a6, a3+a7, a0+a4, a1+a5] */
    s = _mm_add_ps(s, _mm_shuffle_ps(s, s, _MM_SHUFFLE(1, 0, 3, 2)));  /* + 交换对后归约到 lane0 */
    return _mm_cvtss_f32(s);
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
    lv_SIMD_TIME_BEGIN();

    size_t i = 0;

#if defined(__AVX__)
    /* AVX 宽度 = 4 double/次 */
    for (; i + 4 <= count; i += 4)
        _mm256_storeu_pd(out + i, _mm256_add_pd(_mm256_loadu_pd(a + i), _mm256_loadu_pd(b + i)));
#elif defined(__SSE2__)
    /* SSE2 宽度 = 4 double/次（2×128 位） */
    for (; i + 4 <= count; i += 4) {
        _mm_storeu_pd(out + i, _mm_add_pd(_mm_loadu_pd(a + i), _mm_loadu_pd(b + i)));
        _mm_storeu_pd(out + i + 2, _mm_add_pd(_mm_loadu_pd(a + i + 2), _mm_loadu_pd(b + i + 2)));
    }
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
    for (; i + 4 <= count; i += 4) {
        vst1q_f64(out + i, vaddq_f64(vld1q_f64(a + i), vld1q_f64(b + i)));
        vst1q_f64(out + i + 2, vaddq_f64(vld1q_f64(a + i + 2), vld1q_f64(b + i + 2)));
    }
#else
    for (; i + 4 <= count; i += 4) {
        lvVec4d va = lv_vec4d_loadu(a + i);
        lvVec4d vb = lv_vec4d_loadu(b + i);
        lv_vec4d_storeu(out + i, lv_vec4d_add(va, vb));
    }
#endif

    /* 剩余元素 */
    for (; i < count; i++) {
        out[i] = a[i] + b[i];
    }

    lv_SIMD_TIME_END();
}

void lv_simd_mul_array_d(const double *a, const double *b, double *out, size_t count) {
    lv_SIMD_STATS_INC(array_ops);
    lv_SIMD_STATS_ADD(elements_processed, count);
    lv_SIMD_TIME_BEGIN();

    size_t i = 0;
#if defined(__AVX__)
    for (; i + 4 <= count; i += 4)
        _mm256_storeu_pd(out + i, _mm256_mul_pd(_mm256_loadu_pd(a + i), _mm256_loadu_pd(b + i)));
#elif defined(__SSE2__)
    for (; i + 4 <= count; i += 4) {
        _mm_storeu_pd(out + i, _mm_mul_pd(_mm_loadu_pd(a + i), _mm_loadu_pd(b + i)));
        _mm_storeu_pd(out + i + 2, _mm_mul_pd(_mm_loadu_pd(a + i + 2), _mm_loadu_pd(b + i + 2)));
    }
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
    for (; i + 4 <= count; i += 4) {
        vst1q_f64(out + i, vmulq_f64(vld1q_f64(a + i), vld1q_f64(b + i)));
        vst1q_f64(out + i + 2, vmulq_f64(vld1q_f64(a + i + 2), vld1q_f64(b + i + 2)));
    }
#else
    for (; i + 4 <= count; i += 4) {
        lvVec4d va = lv_vec4d_loadu(a + i);
        lvVec4d vb = lv_vec4d_loadu(b + i);
        lv_vec4d_storeu(out + i, lv_vec4d_mul(va, vb));
    }
#endif

    for (; i < count; i++) {
        out[i] = a[i] * b[i];
    }

    lv_SIMD_TIME_END();
}

void lv_simd_fmadd_array_d(const double *a, const double *b, const double *c, double *out, size_t count) {
    lv_SIMD_STATS_INC(array_ops);
    lv_SIMD_STATS_ADD(elements_processed, count);
    lv_SIMD_TIME_BEGIN();

    size_t i = 0;
#if defined(__AVX__)
    for (; i + 4 <= count; i += 4) {
#if defined(__FMA__)
        _mm256_storeu_pd(out + i,
                         _mm256_fmadd_pd(_mm256_loadu_pd(a + i), _mm256_loadu_pd(b + i), _mm256_loadu_pd(c + i)));
#else
        _mm256_storeu_pd(out + i,
                         _mm256_add_pd(_mm256_mul_pd(_mm256_loadu_pd(a + i), _mm256_loadu_pd(b + i)),
                                       _mm256_loadu_pd(c + i)));
#endif
    }
#elif defined(__SSE2__)
    for (; i + 4 <= count; i += 4) {
        _mm_storeu_pd(out + i, _mm_add_pd(_mm_mul_pd(_mm_loadu_pd(a + i), _mm_loadu_pd(b + i)), _mm_loadu_pd(c + i)));
        _mm_storeu_pd(out + i + 2,
                      _mm_add_pd(_mm_mul_pd(_mm_loadu_pd(a + i + 2), _mm_loadu_pd(b + i + 2)), _mm_loadu_pd(c + i + 2)));
    }
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
    for (; i + 4 <= count; i += 4) {
        vst1q_f64(out + i, vmlaq_f64(vld1q_f64(c + i), vld1q_f64(a + i), vld1q_f64(b + i)));
        vst1q_f64(out + i + 2, vmlaq_f64(vld1q_f64(c + i + 2), vld1q_f64(a + i + 2), vld1q_f64(b + i + 2)));
    }
#else
    for (; i + 4 <= count; i += 4) {
        lvVec4d va = lv_vec4d_loadu(a + i);
        lvVec4d vb = lv_vec4d_loadu(b + i);
        lvVec4d vc = lv_vec4d_loadu(c + i);
        lv_vec4d_storeu(out + i, lv_vec4d_fmadd(va, vb, vc));
    }
#endif

    for (; i < count; i++) {
        out[i] = a[i] * b[i] + c[i];
    }

    lv_SIMD_TIME_END();
}

double lv_simd_sum_array_d(const double *arr, size_t count) {
    lv_SIMD_STATS_INC(array_ops);
    lv_SIMD_STATS_ADD(elements_processed, count);
    lv_SIMD_TIME_BEGIN();

    double sum = 0.0;
    size_t i = 0;

#if defined(__AVX__)
    __m256d vsum = _mm256_setzero_pd();
    for (; i + 4 <= count; i += 4)
        vsum = _mm256_add_pd(vsum, _mm256_loadu_pd(arr + i));
    __m256d vh = _mm256_hadd_pd(vsum, _mm256_setzero_pd());
    sum = ((double*)&vh)[0] + ((double*)&vh)[2];
#elif defined(__SSE2__)
    __m128d vsum_lo = _mm_setzero_pd();
    __m128d vsum_hi = _mm_setzero_pd();
    for (; i + 4 <= count; i += 4) {
        vsum_lo = _mm_add_pd(vsum_lo, _mm_loadu_pd(arr + i));
        vsum_hi = _mm_add_pd(vsum_hi, _mm_loadu_pd(arr + i + 2));
    }
    __m128d vh = _mm_add_pd(vsum_lo, vsum_hi);
    vh = _mm_add_pd(vh, _mm_shuffle_pd(vh, vh, 1));
    sum = _mm_cvtsd_f64(vh);
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
    float64x2_t vsum_lo = vdupq_n_f64(0.0);
    float64x2_t vsum_hi = vdupq_n_f64(0.0);
    for (; i + 4 <= count; i += 4) {
        vsum_lo = vaddq_f64(vsum_lo, vld1q_f64(arr + i));
        vsum_hi = vaddq_f64(vsum_hi, vld1q_f64(arr + i + 2));
    }
    float64x2_t vh = vaddq_f64(vsum_lo, vsum_hi);
    sum = vgetq_lane_f64(vh, 0) + vgetq_lane_f64(vh, 1);
#else
    lvVec4d vsum = lv_vec4d_zero();
    for (; i + 4 <= count; i += 4)
        vsum = lv_vec4d_add(vsum, lv_vec4d_loadu(arr + i));
    sum = lv_vec4d_hsum(vsum);
#endif

    /* 剩余元素 */
    for (; i < count; i++) {
        sum += arr[i];
    }

    lv_SIMD_TIME_END();
    return sum;
}

double lv_simd_dot_array_d(const double *a, const double *b, size_t count) {
    lv_SIMD_STATS_INC(array_ops);
    lv_SIMD_STATS_ADD(elements_processed, count);
    lv_SIMD_TIME_BEGIN();

    double dot = 0.0;
    size_t i = 0;

#if defined(__AVX__)
    __m256d vdot = _mm256_setzero_pd();
    for (; i + 4 <= count; i += 4)
        vdot = _mm256_add_pd(vdot, _mm256_mul_pd(_mm256_loadu_pd(a + i), _mm256_loadu_pd(b + i)));
    __m256d vh = _mm256_hadd_pd(vdot, _mm256_setzero_pd());
    dot = ((double*)&vh)[0] + ((double*)&vh)[2];
#elif defined(__SSE2__)
    __m128d vdot_lo = _mm_setzero_pd();
    __m128d vdot_hi = _mm_setzero_pd();
    for (; i + 4 <= count; i += 4) {
        vdot_lo = _mm_add_pd(vdot_lo, _mm_mul_pd(_mm_loadu_pd(a + i), _mm_loadu_pd(b + i)));
        vdot_hi = _mm_add_pd(vdot_hi, _mm_mul_pd(_mm_loadu_pd(a + i + 2), _mm_loadu_pd(b + i + 2)));
    }
    __m128d vh = _mm_add_pd(vdot_lo, vdot_hi);
    vh = _mm_add_pd(vh, _mm_shuffle_pd(vh, vh, 1));
    dot = _mm_cvtsd_f64(vh);
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
    float64x2_t vdot_lo = vdupq_n_f64(0.0);
    float64x2_t vdot_hi = vdupq_n_f64(0.0);
    for (; i + 4 <= count; i += 4) {
        vdot_lo = vaddq_f64(vdot_lo, vmulq_f64(vld1q_f64(a + i), vld1q_f64(b + i)));
        vdot_hi = vaddq_f64(vdot_hi, vmulq_f64(vld1q_f64(a + i + 2), vld1q_f64(b + i + 2)));
    }
    float64x2_t vh = vaddq_f64(vdot_lo, vdot_hi);
    dot = vgetq_lane_f64(vh, 0) + vgetq_lane_f64(vh, 1);
#else
    lvVec4d vdot = lv_vec4d_zero();
    for (; i + 4 <= count; i += 4) {
        lvVec4d va = lv_vec4d_loadu(a + i);
        lvVec4d vb = lv_vec4d_loadu(b + i);
        vdot = lv_vec4d_fmadd(va, vb, vdot);
    }
    dot = lv_vec4d_hsum(vdot);
#endif

    for (; i < count; i++) {
        dot += a[i] * b[i];
    }

    lv_SIMD_TIME_END();
    return dot;
}

void lv_simd_scale_array_d(const double *in, double scale, double *out, size_t count) {
    lv_SIMD_STATS_INC(array_ops);
    lv_SIMD_STATS_ADD(elements_processed, count);
    lv_SIMD_TIME_BEGIN();

    size_t i = 0;

#if defined(__AVX__)
    __m256d vscale = _mm256_set1_pd(scale);
    for (; i + 4 <= count; i += 4)
        _mm256_storeu_pd(out + i, _mm256_mul_pd(_mm256_loadu_pd(in + i), vscale));
#elif defined(__SSE2__)
    __m128d vscale = _mm_set1_pd(scale);
    for (; i + 4 <= count; i += 4) {
        _mm_storeu_pd(out + i, _mm_mul_pd(_mm_loadu_pd(in + i), vscale));
        _mm_storeu_pd(out + i + 2, _mm_mul_pd(_mm_loadu_pd(in + i + 2), vscale));
    }
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
    float64x2_t vscale = vdupq_n_f64(scale);
    for (; i + 4 <= count; i += 4) {
        vst1q_f64(out + i, vmulq_f64(vld1q_f64(in + i), vscale));
        vst1q_f64(out + i + 2, vmulq_f64(vld1q_f64(in + i + 2), vscale));
    }
#else
    lvVec4d vscale = lv_vec4d_set1(scale);
    for (; i + 4 <= count; i += 4) {
        lvVec4d v = lv_vec4d_loadu(in + i);
        lvVec4d vr = lv_vec4d_mul(v, vscale);
        lv_vec4d_storeu(out + i, vr);
    }
#endif

    for (; i < count; i++) {
        out[i] = in[i] * scale;
    }

    lv_SIMD_TIME_END();
}

double lv_simd_dot_product_array(const double *a, const double *b, size_t count) {
    lv_SIMD_STATS_INC(array_ops);
    lv_SIMD_STATS_ADD(elements_processed, count);
    lv_SIMD_TIME_BEGIN();

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

    lv_SIMD_TIME_END();
    return dot;
}

void lv_simd_norm_array(const double *in, double *out, size_t count) {
    lv_SIMD_STATS_INC(array_ops);
    lv_SIMD_STATS_ADD(elements_processed, count);
    lv_SIMD_TIME_BEGIN();

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

    lv_SIMD_TIME_END();
}

void lv_simd_scale_array(const double *in, double scale, double *out, size_t count) {
    lv_SIMD_STATS_INC(array_ops);
    lv_SIMD_STATS_ADD(elements_processed, count);
    lv_SIMD_TIME_BEGIN();

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
        lvVec4d v = lv_vec4d_loadu(in + i);
        lvVec4d vr = lv_vec4d_mul(v, vscale);
        lv_vec4d_storeu(out + i, vr);
    }
#endif

    for (; i < count; i++) {
        out[i] = in[i] * scale;
    }

    lv_SIMD_TIME_END();
}

double lv_simd_max_array_d(const double *arr, size_t count) {
    if (count == 0)
        return 0.0;

    lv_SIMD_STATS_INC(array_ops);
    lv_SIMD_STATS_ADD(elements_processed, count);
    lv_SIMD_TIME_BEGIN();

    double max_val = arr[0];
    size_t i = 0;

#if defined(__AVX__)
    __m256d vmax = _mm256_set1_pd(arr[0]);
    for (; i + 4 <= count; i += 4)
        vmax = _mm256_max_pd(vmax, _mm256_loadu_pd(arr + i));
    double m = ((double*)&vmax)[0];
    if (((double*)&vmax)[1] > m) m = ((double*)&vmax)[1];
    if (((double*)&vmax)[2] > m) m = ((double*)&vmax)[2];
    if (((double*)&vmax)[3] > m) m = ((double*)&vmax)[3];
    max_val = m;
#elif defined(__SSE2__)
    __m128d vmax = _mm_set1_pd(arr[0]);
    for (; i + 4 <= count; i += 4) {
        vmax = _mm_max_pd(vmax, _mm_max_pd(_mm_loadu_pd(arr + i), _mm_loadu_pd(arr + i + 2)));
    }
    double m = _mm_cvtsd_f64(vmax);
    double m2 = _mm_cvtsd_f64(_mm_shuffle_pd(vmax, vmax, 1));
    max_val = (m > m2) ? m : m2;
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
    float64x2_t vmax = vdupq_n_f64(arr[0]);
    for (; i + 4 <= count; i += 4) {
        vmax = vmaxq_f64(vmax, vmaxq_f64(vld1q_f64(arr + i), vld1q_f64(arr + i + 2)));
    }
    double m0 = vgetq_lane_f64(vmax, 0);
    double m1 = vgetq_lane_f64(vmax, 1);
    max_val = (m0 > m1) ? m0 : m1;
#else
    for (; i + 4 <= count; i += 4) {
        lvVec4d v = lv_vec4d_loadu(arr + i);
        double hm = lv_vec4d_hmax(v);
        if (hm > max_val)
            max_val = hm;
    }
#endif

    for (; i < count; i++) {
        if (arr[i] > max_val)
            max_val = arr[i];
    }

    lv_SIMD_TIME_END();
    return max_val;
}

double lv_simd_min_array_d(const double *arr, size_t count) {
    if (count == 0)
        return 0.0;

    lv_SIMD_STATS_INC(array_ops);
    lv_SIMD_STATS_ADD(elements_processed, count);
    lv_SIMD_TIME_BEGIN();

    double min_val = arr[0];
    size_t i = 0;

#if defined(__AVX__)
    __m256d vmin = _mm256_set1_pd(arr[0]);
    for (; i + 4 <= count; i += 4)
        vmin = _mm256_min_pd(vmin, _mm256_loadu_pd(arr + i));
    double m = ((double*)&vmin)[0];
    if (((double*)&vmin)[1] < m) m = ((double*)&vmin)[1];
    if (((double*)&vmin)[2] < m) m = ((double*)&vmin)[2];
    if (((double*)&vmin)[3] < m) m = ((double*)&vmin)[3];
    min_val = m;
#elif defined(__SSE2__)
    __m128d vmin = _mm_set1_pd(arr[0]);
    for (; i + 4 <= count; i += 4) {
        vmin = _mm_min_pd(vmin, _mm_min_pd(_mm_loadu_pd(arr + i), _mm_loadu_pd(arr + i + 2)));
    }
    double m = _mm_cvtsd_f64(vmin);
    double m2 = _mm_cvtsd_f64(_mm_shuffle_pd(vmin, vmin, 1));
    min_val = (m < m2) ? m : m2;
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
    float64x2_t vmin = vdupq_n_f64(arr[0]);
    for (; i + 4 <= count; i += 4) {
        vmin = vminq_f64(vmin, vminq_f64(vld1q_f64(arr + i), vld1q_f64(arr + i + 2)));
    }
    double m0 = vgetq_lane_f64(vmin, 0);
    double m1 = vgetq_lane_f64(vmin, 1);
    min_val = (m0 < m1) ? m0 : m1;
#else
    for (; i + 4 <= count; i += 4) {
        lvVec4d v = lv_vec4d_loadu(arr + i);
        double hm = lv_vec4d_hmin(v);
        if (hm < min_val)
            min_val = hm;
    }
#endif

    for (; i < count; i++) {
        if (arr[i] < min_val)
            min_val = arr[i];
    }

    lv_SIMD_TIME_END();
    return min_val;
}

/* ============== 几何运算加速 ============== */

void lv_simd_distance_array(const double *x1, const double *y1, const double *x2, const double *y2, double *out,
                            size_t count) {
    lv_SIMD_STATS_INC(array_ops);
    lv_SIMD_STATS_ADD(elements_processed, count);
    lv_SIMD_TIME_BEGIN();

    /* geo_distance_2d 为外部标量函数（地理距离，含 atan2/sin 等超越函数），无法向量化，保持标量逐元素 */
    for (size_t i = 0; i < count; i++) {
        out[i] = geo_distance_2d(x1[i], y1[i], x2[i], y2[i]);
    }

    lv_SIMD_TIME_END();
}

void lv_simd_point_line_distance_array(const double *px, const double *py, double x1, double y1, double x2, double y2,
                                       double *out, size_t count) {
    lv_SIMD_STATS_INC(array_ops);
    lv_SIMD_STATS_ADD(elements_processed, count);
    lv_SIMD_TIME_BEGIN();

    double dx = x2 - x1;
    double dy = y2 - y1;
    double len_sq = dx * dx + dy * dy;

    if (len_sq < 1e-12) {
        /* 线段退化为点 */
        for (size_t i = 0; i < count; i++) {
            out[i] = geo_distance_2d(x1, y1, px[i], py[i]);
        }
        lv_SIMD_TIME_END();
        return;
    }

    double len = geo_distance_2d(x1, y1, x2, y2);
    double nx = -dy / len; /* 法向量 */
    double ny = dx / len;

    size_t i = 0;
#if defined(__AVX__)
    __m256d vx1 = _mm256_set1_pd(x1), vy1 = _mm256_set1_pd(y1);
    __m256d vnx = _mm256_set1_pd(nx), vny = _mm256_set1_pd(ny);
    __m256d vsign = _mm256_set1_pd(-0.0);
    for (; i + 4 <= count; i += 4) {
        __m256d ddx = _mm256_sub_pd(_mm256_loadu_pd(px + i), vx1);
        __m256d ddy = _mm256_sub_pd(_mm256_loadu_pd(py + i), vy1);
        __m256d d = _mm256_add_pd(_mm256_mul_pd(ddx, vnx), _mm256_mul_pd(ddy, vny));
        _mm256_storeu_pd(out + i, _mm256_andnot_pd(vsign, d)); /* fabs */
    }
#elif defined(__SSE2__)
    __m128d vx1 = _mm_set1_pd(x1), vy1 = _mm_set1_pd(y1);
    __m128d vnx = _mm_set1_pd(nx), vny = _mm_set1_pd(ny);
    __m128d vsign = _mm_set1_pd(-0.0);
    for (; i + 2 <= count; i += 2) {
        __m128d ddx = _mm_sub_pd(_mm_loadu_pd(px + i), vx1);
        __m128d ddy = _mm_sub_pd(_mm_loadu_pd(py + i), vy1);
        __m128d d = _mm_add_pd(_mm_mul_pd(ddx, vnx), _mm_mul_pd(ddy, vny));
        _mm_storeu_pd(out + i, _mm_andnot_pd(vsign, d)); /* fabs */
    }
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
    float64x2_t vx1 = vdupq_n_f64(x1), vy1 = vdupq_n_f64(y1);
    float64x2_t vnx = vdupq_n_f64(nx), vny = vdupq_n_f64(ny);
    for (; i + 2 <= count; i += 2) {
        float64x2_t ddx = vsubq_f64(vld1q_f64(px + i), vx1);
        float64x2_t ddy = vsubq_f64(vld1q_f64(py + i), vy1);
        float64x2_t d = vaddq_f64(vmulq_f64(ddx, vnx), vmulq_f64(ddy, vny));
        vst1q_f64(out + i, vabsq_f64(d));
    }
#endif

    for (; i < count; i++) {
        double ddx = px[i] - x1;
        double ddy = py[i] - y1;
        out[i] = fabs(ddx * nx + ddy * ny);
    }

    lv_SIMD_TIME_END();
}

void lv_simd_cross2d_array(const double *ax, const double *ay, const double *bx, const double *by, double *out,
                           size_t count) {
    lv_SIMD_STATS_INC(array_ops);
    lv_SIMD_STATS_ADD(elements_processed, count);
    lv_SIMD_TIME_BEGIN();

    size_t i = 0;
#if defined(__AVX__)
    for (; i + 4 <= count; i += 4) {
        __m256d vax = _mm256_loadu_pd(ax + i), vay = _mm256_loadu_pd(ay + i);
        __m256d vbx = _mm256_loadu_pd(bx + i), vby = _mm256_loadu_pd(by + i);
        /* cross = ax * by - ay * bx */
        _mm256_storeu_pd(out + i, _mm256_sub_pd(_mm256_mul_pd(vax, vby), _mm256_mul_pd(vay, vbx)));
    }
#elif defined(__SSE2__)
    for (; i + 4 <= count; i += 4) {
        _mm_storeu_pd(out + i, _mm_sub_pd(_mm_mul_pd(_mm_loadu_pd(ax + i), _mm_loadu_pd(by + i)),
                                          _mm_mul_pd(_mm_loadu_pd(ay + i), _mm_loadu_pd(bx + i))));
        _mm_storeu_pd(out + i + 2, _mm_sub_pd(_mm_mul_pd(_mm_loadu_pd(ax + i + 2), _mm_loadu_pd(by + i + 2)),
                                              _mm_mul_pd(_mm_loadu_pd(ay + i + 2), _mm_loadu_pd(bx + i + 2))));
    }
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
    for (; i + 4 <= count; i += 4) {
        vst1q_f64(out + i, vsubq_f64(vmulq_f64(vld1q_f64(ax + i), vld1q_f64(by + i)),
                                     vmulq_f64(vld1q_f64(ay + i), vld1q_f64(bx + i))));
        vst1q_f64(out + i + 2, vsubq_f64(vmulq_f64(vld1q_f64(ax + i + 2), vld1q_f64(by + i + 2)),
                                         vmulq_f64(vld1q_f64(ay + i + 2), vld1q_f64(bx + i + 2))));
    }
#else
    for (; i + 4 <= count; i += 4) {
        lvVec4d vax = lv_vec4d_loadu(ax + i);
        lvVec4d vay = lv_vec4d_loadu(ay + i);
        lvVec4d vbx = lv_vec4d_loadu(bx + i);
        lvVec4d vby = lv_vec4d_loadu(by + i);
        lvVec4d v1 = lv_vec4d_mul(vax, vby);
        lvVec4d v2 = lv_vec4d_mul(vay, vbx);
        lvVec4d vr = lv_vec4d_sub(v1, v2);
        lv_vec4d_storeu(out + i, vr);
    }
#endif

    for (; i < count; i++) {
        out[i] = ax[i] * by[i] - ay[i] * bx[i];
    }

    lv_SIMD_TIME_END();
}

void lv_simd_point_in_circle_array(const double *px, const double *py, double cx, double cy, double r, int *out,
                                   size_t count) {
    lv_SIMD_STATS_INC(array_ops);
    lv_SIMD_STATS_ADD(elements_processed, count);
    lv_SIMD_TIME_BEGIN();

    double r_sq = r * r;
    size_t i = 0;

#if defined(__AVX__)
    __m256d vcx = _mm256_set1_pd(cx), vcy = _mm256_set1_pd(cy);
    __m256d vrsq = _mm256_set1_pd(r_sq);
    for (; i + 4 <= count; i += 4) {
        __m256d ddx = _mm256_sub_pd(_mm256_loadu_pd(px + i), vcx);
        __m256d ddy = _mm256_sub_pd(_mm256_loadu_pd(py + i), vcy);
        __m256d d2 = _mm256_add_pd(_mm256_mul_pd(ddx, ddx), _mm256_mul_pd(ddy, ddy));
        int m = _mm256_movemask_pd(_mm256_cmp_pd(d2, vrsq, _CMP_LE_OQ));
        out[i + 0] = (m & 1) ? 1 : 0;
        out[i + 1] = (m & 2) ? 1 : 0;
        out[i + 2] = (m & 4) ? 1 : 0;
        out[i + 3] = (m & 8) ? 1 : 0;
    }
#elif defined(__SSE2__)
    __m128d vcx = _mm_set1_pd(cx), vcy = _mm_set1_pd(cy);
    __m128d vrsq = _mm_set1_pd(r_sq);
    for (; i + 2 <= count; i += 2) {
        __m128d ddx = _mm_sub_pd(_mm_loadu_pd(px + i), vcx);
        __m128d ddy = _mm_sub_pd(_mm_loadu_pd(py + i), vcy);
        __m128d d2 = _mm_add_pd(_mm_mul_pd(ddx, ddx), _mm_mul_pd(ddy, ddy));
        int m = _mm_movemask_pd(_mm_cmple_pd(d2, vrsq));
        out[i + 0] = (m & 1) ? 1 : 0;
        out[i + 1] = (m & 2) ? 1 : 0;
    }
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
    float64x2_t vcx = vdupq_n_f64(cx), vcy = vdupq_n_f64(cy);
    float64x2_t vrsq = vdupq_n_f64(r_sq);
    for (; i + 2 <= count; i += 2) {
        float64x2_t ddx = vsubq_f64(vld1q_f64(px + i), vcx);
        float64x2_t ddy = vsubq_f64(vld1q_f64(py + i), vcy);
        float64x2_t d2 = vaddq_f64(vmulq_f64(ddx, ddx), vmulq_f64(ddy, ddy));
        uint64x2_t m = vcleq_f64(d2, vrsq);
        out[i + 0] = vgetq_lane_u64(m, 0) ? 1 : 0;
        out[i + 1] = vgetq_lane_u64(m, 1) ? 1 : 0;
    }
#endif

    for (; i < count; i++) {
        double ddx = px[i] - cx;
        double ddy = py[i] - cy;
        double dist_sq = ddx * ddx + ddy * ddy;
        out[i] = (dist_sq <= r_sq) ? 1 : 0;
    }

    lv_SIMD_TIME_END();
}

/* ============== 矩阵运算 ============== */

lvVec4d lv_simd_mat4x4_vec4_mul(const double mat[16], lvVec4d vec) {
    lvVec4d result;
    /* result[i] = Σ_k mat[i*4+k] * vec[k]（按行点积，与标量实现语义一致） */
#if defined(__AVX__)
    __m256d vv = _mm256_loadu_pd(vec.v);
    for (int i = 0; i < 4; i++) {
        __m256d row = _mm256_loadu_pd(mat + i * 4);
        __m256d p = _mm256_mul_pd(row, vv);
        __m256d h = _mm256_hadd_pd(p, _mm256_setzero_pd());
        result.v[i] = ((double*)&h)[0] + ((double*)&h)[2];
    }
#elif defined(__SSE2__)
    __m128d v_lo = _mm_loadu_pd(vec.v), v_hi = _mm_loadu_pd(vec.v + 2);
    for (int i = 0; i < 4; i++) {
        __m128d r_lo = _mm_loadu_pd(mat + i * 4);
        __m128d r_hi = _mm_loadu_pd(mat + i * 4 + 2);
        __m128d s = _mm_add_pd(_mm_mul_pd(r_lo, v_lo), _mm_mul_pd(r_hi, v_hi));
        s = _mm_add_pd(s, _mm_shuffle_pd(s, s, 1));
        result.v[i] = _mm_cvtsd_f64(s);
    }
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
    float64x2_t v_lo = vld1q_f64(vec.v), v_hi = vld1q_f64(vec.v + 2);
    for (int i = 0; i < 4; i++) {
        float64x2_t r_lo = vld1q_f64(mat + i * 4), r_hi = vld1q_f64(mat + i * 4 + 2);
        float64x2_t s = vaddq_f64(vmulq_f64(r_lo, v_lo), vmulq_f64(r_hi, v_hi));
        result.v[i] = vgetq_lane_f64(s, 0) + vgetq_lane_f64(s, 1);
    }
#else
    result.v[0] = mat[0] * vec.v[0] + mat[1] * vec.v[1] + mat[2] * vec.v[2] + mat[3] * vec.v[3];
    result.v[1] = mat[4] * vec.v[0] + mat[5] * vec.v[1] + mat[6] * vec.v[2] + mat[7] * vec.v[3];
    result.v[2] = mat[8] * vec.v[0] + mat[9] * vec.v[1] + mat[10] * vec.v[2] + mat[11] * vec.v[3];
    result.v[3] = mat[12] * vec.v[0] + mat[13] * vec.v[1] + mat[14] * vec.v[2] + mat[15] * vec.v[3];
#endif

    lv_SIMD_STATS_INC(vec4_ops);
    return result;
}

void lv_simd_mat4x4_vec4_array_mul(const double mat[16], const double *vecs, double *out, size_t count) {
    lv_SIMD_STATS_INC(array_ops);
    lv_SIMD_STATS_ADD(elements_processed, count);
    lv_SIMD_TIME_BEGIN();

    for (size_t i = 0; i < count; i++) {
        /* 输入/输出数组仅保证 8 字节对齐，用 loadu/storeu */
        lvVec4d v = lv_vec4d_loadu(vecs + i * 4);
        lvVec4d r = lv_simd_mat4x4_vec4_mul(mat, v);
        lv_vec4d_storeu(out + i * 4, r);
    }

    lv_SIMD_TIME_END();
}

void lv_simd_mat3x3_vec2_mul(const double mat[9], double x, double y, double *out_x, double *out_y) {
    /* 齐次坐标变换 */
    double w = mat[6] * x + mat[7] * y + mat[8];
    *out_x = (mat[0] * x + mat[1] * y + mat[2]) / w;
    *out_y = (mat[3] * x + mat[4] * y + mat[5]) / w;
}
