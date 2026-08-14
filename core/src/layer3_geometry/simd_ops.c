/**
 * @file simd_ops.c
 * @brief SIMD向量运算库实现
 *
 * @details 跨平台SIMD实现，支持SSE/AVX/NEON，并提供标量回退。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "lv/simd_ops.h"

#include "lv/lv_utils.h"
#include "lv/lv_xmacro.h"
#include "lv/geo_utils.h"

#include <math.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "simd_ops_internal.h"

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

/* 定义见 simd_ops_internal.h（核心文件保有唯一定义） */
lvSimdStatsAtomic g_simd_stats = {0};

/* 热路径统计累加宏（relaxed 序：计数器允许乱序累加，只需保证读改写不撕裂） */
#define lv_SIMD_STATS_INC(field) \
    atomic_fetch_add_explicit(&g_simd_stats.field, 1, memory_order_relaxed)
#define lv_SIMD_STATS_ADD(field, n) \
    atomic_fetch_add_explicit(&g_simd_stats.field, (uint64_t)(n), memory_order_relaxed)

/* 轻量计时：simd_time_us 真实计数（可简化：仅批量数组操作级计时，逐向量操作计时开销过大不作） */
uint64_t lv_simd_now_us(void) {
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

