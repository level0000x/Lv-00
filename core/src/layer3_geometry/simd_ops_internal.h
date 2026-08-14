#ifndef LV_SIMD_OPS_INTERNAL_H
#define LV_SIMD_OPS_INTERNAL_H

#include "lv/simd_ops.h"

#include <stdatomic.h>

/* ============== SIMD 内联头文件（从 simd_ops.c 移入，供各宽度族子模块复用） ============== */
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

/* ============== 统计（跨文件共享） ============== */

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

/* 定义在 simd_ops.c（核心文件保有唯一定义） */
extern lvSimdStatsAtomic g_simd_stats;

/* 热路径统计累加宏（relaxed 序：计数器允许乱序累加，只需保证读改写不撕裂） */
#define lv_SIMD_STATS_INC(field) \
    atomic_fetch_add_explicit(&g_simd_stats.field, 1, memory_order_relaxed)
#define lv_SIMD_STATS_ADD(field, n) \
    atomic_fetch_add_explicit(&g_simd_stats.field, (uint64_t)(n), memory_order_relaxed)

/* 定义在 simd_ops.c（核心文件） */
uint64_t lv_simd_now_us(void);

/* 轻量计时宏（各子模块热路径使用） */
#define lv_SIMD_TIME_BEGIN() uint64_t lv_t0 = lv_simd_now_us()
#define lv_SIMD_TIME_END() lv_SIMD_STATS_ADD(simd_time_us, lv_simd_now_us() - lv_t0)

#endif /* LV_SIMD_OPS_INTERNAL_H */
