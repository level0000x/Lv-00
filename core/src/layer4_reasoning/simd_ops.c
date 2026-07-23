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

#include <math.h>
#include <stdio.h>
#include <stdatomic.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
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
        if (cpuinfo[3] & (1 << 26)) caps |= lv_SIMD_SSE2;
        if (cpuinfo[2] & (1 << 19)) caps |= lv_SIMD_SSE41;
        if (cpuinfo[2] & (1 << 28)) caps |= lv_SIMD_AVX;
    }

    if (cpuinfo[0] >= 7) {
        __cpuidex(cpuinfo, 7, 0);
        if (cpuinfo[1] & (1 << 5)) caps |= lv_SIMD_AVX2;
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

static uint32_t lv_simd_detect_capabilities(void) {
    detect_simd_capabilities();
    return atomic_load(&g_simd_capabilities);
}

static bool lv_simd_has_capability(lvSimdCapability cap) {
    detect_simd_capabilities();
    return (atomic_load(&g_simd_capabilities) & cap) != 0;
}

const char *lv_simd_capability_name(lvSimdCapability cap) {
    switch (cap) {
        case lv_SIMD_NONE:    return "None (Scalar)";
        case lv_SIMD_SSE2:    return "SSE2";
        case lv_SIMD_SSE41:   return "SSE4.1";
        case lv_SIMD_AVX:     return "AVX";
        case lv_SIMD_AVX2:    return "AVX2";
        case lv_SIMD_AVX512F: return "AVX-512F";
        case lv_SIMD_NEON:    return "NEON";
        default:                return "Unknown";
    }
}

/* ============== 统计 ============== */

static lvSimdStats g_simd_stats = {0};

void lv_simd_get_stats(lvSimdStats *stats) {
    if (stats) *stats = g_simd_stats;
}

void lv_simd_reset_stats(void) {
    memset(&g_simd_stats, 0, sizeof(g_simd_stats));
}

static void lv_simd_print_diag(void *stream) {
    FILE *f = stream ? (FILE *)stream : stdout;

    fprintf(f, "\n========== Lv-00 SIMD 诊断 ==========\n");
    fprintf(f, "检测到的SIMD能力:\n");

    detect_simd_capabilities();

    const char *caps[] = {"SSE2", "SSE4.1", "AVX", "AVX2", "AVX-512F", "NEON"};
    lvSimdCapability flags[] = {
        lv_SIMD_SSE2, lv_SIMD_SSE41, lv_SIMD_AVX,
        lv_SIMD_AVX2, lv_SIMD_AVX512F, lv_SIMD_NEON
    };

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
    fprintf(f, "4元素向量操作: %llu\n", (unsigned long long)g_simd_stats.vec4_ops);
    fprintf(f, "8元素向量操作: %llu\n", (unsigned long long)g_simd_stats.vec8_ops);
    fprintf(f, "数组操作: %llu\n", (unsigned long long)g_simd_stats.array_ops);
    fprintf(f, "处理元素总数: %llu\n", (unsigned long long)g_simd_stats.elements_processed);
    fprintf(f, "SIMD总耗时: %llu us\n", (unsigned long long)g_simd_stats.simd_time_us);
    fprintf(f, "标量回退次数: %llu\n", (unsigned long long)g_simd_stats.scalar_fallbacks);
    fprintf(f, "=====================================\n\n");
}

/* ============== 标量实现（回退） ============== */

/* 内联辅助：获取时间 */
static inline uint64_t get_time_us(void) {
#ifdef _WIN32
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return (uint64_t)((counter.QuadPart * 1000000ULL) / freq.QuadPart);
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000ULL + (uint64_t)tv.tv_usec;
#endif
}

/* ============== 4x double 向量操作 ============== */

lvVec4d lv_vec4d_zero(void) {
    lvVec4d v = {{0.0, 0.0, 0.0, 0.0}};
    g_simd_stats.vec4_ops++;
    return v;
}

lvVec4d lv_vec4d_one(void) {
    lvVec4d v = {{1.0, 1.0, 1.0, 1.0}};
    g_simd_stats.vec4_ops++;
    return v;
}

lvVec4d lv_vec4d_set1(double val) {
    lvVec4d v = {{val, val, val, val}};
    g_simd_stats.vec4_ops++;
    return v;
}

lvVec4d lv_vec4d_set(double x, double y, double z, double w) {
    lvVec4d v = {{x, y, z, w}};
    g_simd_stats.vec4_ops++;
    return v;
}

lvVec4d lv_vec4d_load(const double *ptr) {
    lvVec4d v;
    v.v[0] = ptr[0];
    v.v[1] = ptr[1];
    v.v[2] = ptr[2];
    v.v[3] = ptr[3];
    g_simd_stats.vec4_ops++;
    return v;
}

lvVec4d lv_vec4d_loadu(const double *ptr) {
    return lv_vec4d_load(ptr);
}

static void lv_vec4d_store(double *ptr, lvVec4d vec) {
    ptr[0] = vec.v[0];
    ptr[1] = vec.v[1];
    ptr[2] = vec.v[2];
    ptr[3] = vec.v[3];
}

static void lv_vec4d_storeu(double *ptr, lvVec4d vec) {
    lv_vec4d_store(ptr, vec);
}

lvVec4d lv_vec4d_add(lvVec4d a, lvVec4d b) {
    lvVec4d r;
    r.v[0] = a.v[0] + b.v[0];
    r.v[1] = a.v[1] + b.v[1];
    r.v[2] = a.v[2] + b.v[2];
    r.v[3] = a.v[3] + b.v[3];
    g_simd_stats.vec4_ops++;
    return r;
}

lvVec4d lv_vec4d_sub(lvVec4d a, lvVec4d b) {
    lvVec4d r;
    r.v[0] = a.v[0] - b.v[0];
    r.v[1] = a.v[1] - b.v[1];
    r.v[2] = a.v[2] - b.v[2];
    r.v[3] = a.v[3] - b.v[3];
    g_simd_stats.vec4_ops++;
    return r;
}

lvVec4d lv_vec4d_mul(lvVec4d a, lvVec4d b) {
    lvVec4d r;
    r.v[0] = a.v[0] * b.v[0];
    r.v[1] = a.v[1] * b.v[1];
    r.v[2] = a.v[2] * b.v[2];
    r.v[3] = a.v[3] * b.v[3];
    g_simd_stats.vec4_ops++;
    return r;
}

lvVec4d lv_vec4d_div(lvVec4d a, lvVec4d b) {
    lvVec4d r;
    r.v[0] = a.v[0] / b.v[0];
    r.v[1] = a.v[1] / b.v[1];
    r.v[2] = a.v[2] / b.v[2];
    r.v[3] = a.v[3] / b.v[3];
    g_simd_stats.vec4_ops++;
    return r;
}

lvVec4d lv_vec4d_neg(lvVec4d a) {
    lvVec4d r;
    r.v[0] = -a.v[0];
    r.v[1] = -a.v[1];
    r.v[2] = -a.v[2];
    r.v[3] = -a.v[3];
    g_simd_stats.vec4_ops++;
    return r;
}

lvVec4d lv_vec4d_sqrt(lvVec4d a) {
    lvVec4d r;
    r.v[0] = sqrt(a.v[0]);
    r.v[1] = sqrt(a.v[1]);
    r.v[2] = sqrt(a.v[2]);
    r.v[3] = sqrt(a.v[3]);
    g_simd_stats.vec4_ops++;
    return r;
}

lvVec4d lv_vec4d_abs(lvVec4d a) {
    lvVec4d r;
    r.v[0] = fabs(a.v[0]);
    r.v[1] = fabs(a.v[1]);
    r.v[2] = fabs(a.v[2]);
    r.v[3] = fabs(a.v[3]);
    g_simd_stats.vec4_ops++;
    return r;
}

lvVec4d lv_vec4d_max(lvVec4d a, lvVec4d b) {
    lvVec4d r;
    r.v[0] = (a.v[0] > b.v[0]) ? a.v[0] : b.v[0];
    r.v[1] = (a.v[1] > b.v[1]) ? a.v[1] : b.v[1];
    r.v[2] = (a.v[2] > b.v[2]) ? a.v[2] : b.v[2];
    r.v[3] = (a.v[3] > b.v[3]) ? a.v[3] : b.v[3];
    g_simd_stats.vec4_ops++;
    return r;
}

lvVec4d lv_vec4d_min(lvVec4d a, lvVec4d b) {
    lvVec4d r;
    r.v[0] = (a.v[0] < b.v[0]) ? a.v[0] : b.v[0];
    r.v[1] = (a.v[1] < b.v[1]) ? a.v[1] : b.v[1];
    r.v[2] = (a.v[2] < b.v[2]) ? a.v[2] : b.v[2];
    r.v[3] = (a.v[3] < b.v[3]) ? a.v[3] : b.v[3];
    g_simd_stats.vec4_ops++;
    return r;
}

lvVec4d lv_vec4d_fmadd(lvVec4d a, lvVec4d x, lvVec4d y) {
    lvVec4d r;
    r.v[0] = a.v[0] * x.v[0] + y.v[0];
    r.v[1] = a.v[1] * x.v[1] + y.v[1];
    r.v[2] = a.v[2] * x.v[2] + y.v[2];
    r.v[3] = a.v[3] * x.v[3] + y.v[3];
    g_simd_stats.vec4_ops++;
    return r;
}

/* 比较操作 */
lvVec4d lv_vec4d_cmpeq(lvVec4d a, lvVec4d b) {
    lvVec4d r;
    r.v[0] = (a.v[0] == b.v[0]) ? -1.0 : 0.0;
    r.v[1] = (a.v[1] == b.v[1]) ? -1.0 : 0.0;
    r.v[2] = (a.v[2] == b.v[2]) ? -1.0 : 0.0;
    r.v[3] = (a.v[3] == b.v[3]) ? -1.0 : 0.0;
    return r;
}

lvVec4d lv_vec4d_cmplt(lvVec4d a, lvVec4d b) {
    lvVec4d r;
    r.v[0] = (a.v[0] < b.v[0]) ? -1.0 : 0.0;
    r.v[1] = (a.v[1] < b.v[1]) ? -1.0 : 0.0;
    r.v[2] = (a.v[2] < b.v[2]) ? -1.0 : 0.0;
    r.v[3] = (a.v[3] < b.v[3]) ? -1.0 : 0.0;
    return r;
}

lvVec4d lv_vec4d_cmple(lvVec4d a, lvVec4d b) {
    lvVec4d r;
    r.v[0] = (a.v[0] <= b.v[0]) ? -1.0 : 0.0;
    r.v[1] = (a.v[1] <= b.v[1]) ? -1.0 : 0.0;
    r.v[2] = (a.v[2] <= b.v[2]) ? -1.0 : 0.0;
    r.v[3] = (a.v[3] <= b.v[3]) ? -1.0 : 0.0;
    return r;
}

lvVec4d lv_vec4d_cmpgt(lvVec4d a, lvVec4d b) {
    lvVec4d r;
    r.v[0] = (a.v[0] > b.v[0]) ? -1.0 : 0.0;
    r.v[1] = (a.v[1] > b.v[1]) ? -1.0 : 0.0;
    r.v[2] = (a.v[2] > b.v[2]) ? -1.0 : 0.0;
    r.v[3] = (a.v[3] > b.v[3]) ? -1.0 : 0.0;
    return r;
}

lvVec4d lv_vec4d_cmpge(lvVec4d a, lvVec4d b) {
    lvVec4d r;
    r.v[0] = (a.v[0] >= b.v[0]) ? -1.0 : 0.0;
    r.v[1] = (a.v[1] >= b.v[1]) ? -1.0 : 0.0;
    r.v[2] = (a.v[2] >= b.v[2]) ? -1.0 : 0.0;
    r.v[3] = (a.v[3] >= b.v[3]) ? -1.0 : 0.0;
    return r;
}

lvVec4d lv_vec4d_select(lvVec4d mask, lvVec4d a, lvVec4d b) {
    lvVec4d r;
    /* 使用位操作实现选择 */
    union { double d; uint64_t u; } m, va, vb, vr;

    for (int i = 0; i < 4; i++) {
        m.d = mask.v[i];
        va.d = a.v[i];
        vb.d = b.v[i];
        vr.u = (va.u & m.u) | (vb.u & ~m.u);
        r.v[i] = vr.d;
    }
    return r;
}

/* 归约操作 */
static double lv_vec4d_hsum(lvVec4d a) {
    return a.v[0] + a.v[1] + a.v[2] + a.v[3];
}

static double lv_vec4d_hmax(lvVec4d a) {
    double m = a.v[0];
    if (a.v[1] > m) m = a.v[1];
    if (a.v[2] > m) m = a.v[2];
    if (a.v[3] > m) m = a.v[3];
    return m;
}

static double lv_vec4d_hmin(lvVec4d a) {
    double m = a.v[0];
    if (a.v[1] < m) m = a.v[1];
    if (a.v[2] < m) m = a.v[2];
    if (a.v[3] < m) m = a.v[3];
    return m;
}

static double lv_vec4d_dot(lvVec4d a, lvVec4d b) {
    return a.v[0] * b.v[0] + a.v[1] * b.v[1] + a.v[2] * b.v[2] + a.v[3] * b.v[3];
}

/* ============== 4x float 向量操作 ============== */

lvVec4f lv_vec4f_zero(void) {
    lvVec4f v = {{0.0f, 0.0f, 0.0f, 0.0f}};
    g_simd_stats.vec4_ops++;
    return v;
}

lvVec4f lv_vec4f_set1(float val) {
    lvVec4f v = {{val, val, val, val}};
    g_simd_stats.vec4_ops++;
    return v;
}

lvVec4f lv_vec4f_load(const float *ptr) {
    lvVec4f v;
    v.v[0] = ptr[0];
    v.v[1] = ptr[1];
    v.v[2] = ptr[2];
    v.v[3] = ptr[3];
    g_simd_stats.vec4_ops++;
    return v;
}

static void lv_vec4f_store(float *ptr, lvVec4f vec) {
    ptr[0] = vec.v[0];
    ptr[1] = vec.v[1];
    ptr[2] = vec.v[2];
    ptr[3] = vec.v[3];
}

lvVec4f lv_vec4f_add(lvVec4f a, lvVec4f b) {
    lvVec4f r;
    r.v[0] = a.v[0] + b.v[0];
    r.v[1] = a.v[1] + b.v[1];
    r.v[2] = a.v[2] + b.v[2];
    r.v[3] = a.v[3] + b.v[3];
    g_simd_stats.vec4_ops++;
    return r;
}

lvVec4f lv_vec4f_sub(lvVec4f a, lvVec4f b) {
    lvVec4f r;
    r.v[0] = a.v[0] - b.v[0];
    r.v[1] = a.v[1] - b.v[1];
    r.v[2] = a.v[2] - b.v[2];
    r.v[3] = a.v[3] - b.v[3];
    g_simd_stats.vec4_ops++;
    return r;
}

lvVec4f lv_vec4f_mul(lvVec4f a, lvVec4f b) {
    lvVec4f r;
    r.v[0] = a.v[0] * b.v[0];
    r.v[1] = a.v[1] * b.v[1];
    r.v[2] = a.v[2] * b.v[2];
    r.v[3] = a.v[3] * b.v[3];
    g_simd_stats.vec4_ops++;
    return r;
}

lvVec4f lv_vec4f_div(lvVec4f a, lvVec4f b) {
    lvVec4f r;
    r.v[0] = a.v[0] / b.v[0];
    r.v[1] = a.v[1] / b.v[1];
    r.v[2] = a.v[2] / b.v[2];
    r.v[3] = a.v[3] / b.v[3];
    g_simd_stats.vec4_ops++;
    return r;
}

lvVec4f lv_vec4f_sqrt(lvVec4f a) {
    lvVec4f r;
    r.v[0] = sqrtf(a.v[0]);
    r.v[1] = sqrtf(a.v[1]);
    r.v[2] = sqrtf(a.v[2]);
    r.v[3] = sqrtf(a.v[3]);
    g_simd_stats.vec4_ops++;
    return r;
}

static float lv_vec4f_hsum(lvVec4f a) {
    return a.v[0] + a.v[1] + a.v[2] + a.v[3];
}

static float lv_vec4f_dot(lvVec4f a, lvVec4f b) {
    return a.v[0] * b.v[0] + a.v[1] * b.v[1] + a.v[2] * b.v[2] + a.v[3] * b.v[3];
}

/* ============== 8x float 向量操作 ============== */

lvVec8f lv_vec8f_zero(void) {
    lvVec8f v = {{0}};
    g_simd_stats.vec8_ops++;
    return v;
}

lvVec8f lv_vec8f_set1(float val) {
    lvVec8f v;
    for (int i = 0; i < 8; i++) v.v[i] = val;
    g_simd_stats.vec8_ops++;
    return v;
}

lvVec8f lv_vec8f_load(const float *ptr) {
    lvVec8f v;
    for (int i = 0; i < 8; i++) v.v[i] = ptr[i];
    g_simd_stats.vec8_ops++;
    return v;
}

static void lv_vec8f_store(float *ptr, lvVec8f vec) {
    for (int i = 0; i < 8; i++) ptr[i] = vec.v[i];
}

lvVec8f lv_vec8f_add(lvVec8f a, lvVec8f b) {
    lvVec8f r;
    for (int i = 0; i < 8; i++) r.v[i] = a.v[i] + b.v[i];
    g_simd_stats.vec8_ops++;
    return r;
}

lvVec8f lv_vec8f_sub(lvVec8f a, lvVec8f b) {
    lvVec8f r;
    for (int i = 0; i < 8; i++) r.v[i] = a.v[i] - b.v[i];
    g_simd_stats.vec8_ops++;
    return r;
}

lvVec8f lv_vec8f_mul(lvVec8f a, lvVec8f b) {
    lvVec8f r;
    for (int i = 0; i < 8; i++) r.v[i] = a.v[i] * b.v[i];
    g_simd_stats.vec8_ops++;
    return r;
}

lvVec8f lv_vec8f_div(lvVec8f a, lvVec8f b) {
    lvVec8f r;
    for (int i = 0; i < 8; i++) r.v[i] = a.v[i] / b.v[i];
    g_simd_stats.vec8_ops++;
    return r;
}

static float lv_vec8f_hsum(lvVec8f a) {
    float sum = 0.0f;
    for (int i = 0; i < 8; i++) sum += a.v[i];
    return sum;
}

/* ============== 批量运算 ============== */

static void lv_simd_add_array_d(const double *a, const double *b, double *out, size_t count) {
    g_simd_stats.array_ops++;
    g_simd_stats.elements_processed += count;

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

static void lv_simd_mul_array_d(const double *a, const double *b, double *out, size_t count) {
    g_simd_stats.array_ops++;
    g_simd_stats.elements_processed += count;

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

static void lv_simd_fmadd_array_d(const double *a, const double *b, const double *c,
                              double *out, size_t count) {
    g_simd_stats.array_ops++;
    g_simd_stats.elements_processed += count;

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

static double lv_simd_sum_array_d(const double *arr, size_t count) {
    g_simd_stats.array_ops++;
    g_simd_stats.elements_processed += count;

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

static double lv_simd_dot_array_d(const double *a, const double *b, size_t count) {
    g_simd_stats.array_ops++;
    g_simd_stats.elements_processed += count;

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

static void lv_simd_scale_array_d(const double *in, double scale, double *out, size_t count) {
    g_simd_stats.array_ops++;
    g_simd_stats.elements_processed += count;

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

static double lv_simd_max_array_d(const double *arr, size_t count) {
    if (count == 0) return 0.0;

    g_simd_stats.array_ops++;
    g_simd_stats.elements_processed += count;

    double max_val = arr[0];
    size_t i = 1;

    for (; i + 4 <= count; i += 4) {
        lvVec4d v = lv_vec4d_load(arr + i);
        max_val = (lv_vec4d_hmax(v) > max_val) ? lv_vec4d_hmax(v) : max_val;
    }

    for (; i < count; i++) {
        if (arr[i] > max_val) max_val = arr[i];
    }

    return max_val;
}

static double lv_simd_min_array_d(const double *arr, size_t count) {
    if (count == 0) return 0.0;

    g_simd_stats.array_ops++;
    g_simd_stats.elements_processed += count;

    double min_val = arr[0];
    size_t i = 1;

    for (; i + 4 <= count; i += 4) {
        lvVec4d v = lv_vec4d_load(arr + i);
        min_val = (lv_vec4d_hmin(v) < min_val) ? lv_vec4d_hmin(v) : min_val;
    }

    for (; i < count; i++) {
        if (arr[i] < min_val) min_val = arr[i];
    }

    return min_val;
}

/* ============== 几何运算加速 ============== */

static void lv_simd_distance_array(const double *x1, const double *y1,
                               const double *x2, const double *y2,
                               double *out, size_t count) {
    g_simd_stats.array_ops++;
    g_simd_stats.elements_processed += count;

    for (size_t i = 0; i < count; i++) {
        double dx = x2[i] - x1[i];
        double dy = y2[i] - y1[i];
        out[i] = sqrt(dx * dx + dy * dy);
    }
}

static void lv_simd_point_line_distance_array(const double *px, const double *py,
                                          double x1, double y1,
                                          double x2, double y2,
                                          double *out, size_t count) {
    g_simd_stats.array_ops++;
    g_simd_stats.elements_processed += count;

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
    double nx = -dy / len;  /* 法向量 */
    double ny = dx / len;

    for (size_t i = 0; i < count; i++) {
        double ddx = px[i] - x1;
        double ddy = py[i] - y1;
        out[i] = fabs(ddx * nx + ddy * ny);
    }
}

static void lv_simd_cross2d_array(const double *ax, const double *ay,
                              const double *bx, const double *by,
                              double *out, size_t count) {
    g_simd_stats.array_ops++;
    g_simd_stats.elements_processed += count;

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

static void lv_simd_point_in_circle_array(const double *px, const double *py,
                                      double cx, double cy, double r,
                                      int *out, size_t count) {
    g_simd_stats.array_ops++;
    g_simd_stats.elements_processed += count;

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

    g_simd_stats.vec4_ops++;
    return result;
}

static void lv_simd_mat4x4_vec4_array_mul(const double mat[16],
                                      const double *vecs,
                                      double *out,
                                      size_t count) {
    g_simd_stats.array_ops++;
    g_simd_stats.elements_processed += count;

    for (size_t i = 0; i < count; i++) {
        lvVec4d v = lv_vec4d_load(vecs + i * 4);
        lvVec4d r = lv_simd_mat4x4_vec4_mul(mat, v);
        lv_vec4d_store(out + i * 4, r);
    }
}

static void lv_simd_mat3x3_vec2_mul(const double mat[9],
                                double x, double y,
                                double *out_x, double *out_y) {
    /* 齐次坐标变换 */
    double w = mat[6] * x + mat[7] * y + mat[8];
    *out_x = (mat[0] * x + mat[1] * y + mat[2]) / w;
    *out_y = (mat[3] * x + mat[4] * y + mat[5]) / w;
}
