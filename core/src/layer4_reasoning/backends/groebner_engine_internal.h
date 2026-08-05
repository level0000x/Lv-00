/**
 * @file groebner_engine_internal.h
 * @brief Groebner 引擎内部共享声明（groebner_mono.c / groebner_poly.c 与 groebner_engine.c 共用）
 *
 * @details 从 groebner_engine.c 拆分单项式操作与多项式运算段后，
 *          将共享宏与跨文件函数声明集中于此，避免重复定义。
 */

#ifndef lv_GROEBNER_ENGINE_INTERNAL_H
#define lv_GROEBNER_ENGINE_INTERNAL_H

#include <stdbool.h>

#include "groebner_engine.h"
#include "lv/lv_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 数值零阈值 */
#define GROEBNER_ZERO_THRESHOLD 1e-15

/** @brief 多项式初始项容量 */
#define GROEBNER_POLY_INIT_CAPACITY 8

/** @brief 多项式扩容因子 */
#define GROEBNER_POLY_GROW_FACTOR 2

/* ================================================================
 *  单项式操作（groebner_mono.c 与 groebner_engine.c 共享）
 * ================================================================ */
int mono_compare(const lvPolynomialRing *ring, const int *powers_a, const int *powers_b);
int mono_total_degree(const int *powers, int var_count);
void mono_lcm(const lvPolynomialRing *ring, const int *powers_a, const int *powers_b, int *lcm_out);
bool mono_divides(const lvPolynomialRing *ring, const int *powers_d, const int *powers_e);
void mono_divide(const lvPolynomialRing *ring, const int *powers_dividend, const int *powers_divisor,
                 int *quotient_out);
bool mono_is_coprime(const lvPolynomialRing *ring, const int *powers_a, const int *powers_b);
void mono_copy(int *dest, const int *src, int var_count);

/* ================================================================
 *  多项式内部运算（groebner_poly.c 与 groebner_engine.c 共享）
 * ================================================================ */
int poly_sort_terms(lvPolynomial *poly, const lvPolynomialRing *ring);
lvPolynomial *poly_internal_create(const lvPolynomialRing *ring, int capacity, const char *label);
void poly_internal_destroy(lvPolynomial *poly);
bool poly_ensure_capacity(lvPolynomial *poly, int needed);
bool poly_ensure_capacity_ex(lvPolynomial *poly, int needed, int var_count);
lvPolynomial *poly_internal_copy(const lvPolynomial *src, const lvPolynomialRing *ring);
lvPolynomial *poly_internal_add(const lvPolynomial *f, const lvPolynomial *g, const lvPolynomialRing *ring);
lvPolynomial *poly_internal_multiply(const lvPolynomial *f, const lvPolynomial *g, const lvPolynomialRing *ring);
lvPolynomial *poly_internal_substitute(const lvPolynomial *f, int var_index, const lvPolynomial *subst,
                                       const lvPolynomialRing *ring);
lvPolynomial *poly_internal_s_polynomial(const lvPolynomial *f, const lvPolynomial *g,
                                         const lvPolynomialRing *ring);
lvPolynomial *poly_internal_reduce(const lvPolynomial *p, lvPolynomial **basis, int basis_count,
                                   const lvPolynomialRing *ring);
bool poly_internal_is_zero(const lvPolynomial *poly);
int poly_internal_total_degree(const lvPolynomial *poly, int var_count);
void poly_internal_scale(lvPolynomial *poly, double scalar);
int poly_leading_term(const lvPolynomial *poly, const lvPolynomialRing *ring, int *lt_out, double *lc_out);

/* ================================================================
 *  通用辅助
 * ================================================================ */
char *groebner_strdup_safe(const char *src);

/* ================================================================
 *  引擎内部常量（从 groebner_engine.c 段1 迁移，供拆分文件共享）
 * ================================================================ */
#define GROEBNER_IDEAL_INIT_GEN_CAPACITY 8
#define GROEBNER_BASIS_INIT_CAPACITY 16
#define GROEBNER_VARIETY_INIT_SOL_CAPACITY 32
#define GROEBNER_SOLVE_MAX_ITER 200
#define GROEBNER_NEWTON_TOL 1e-12
#define GROEBNER_NEWTON_MAX_ITER 50
#define GROEBNER_ROOT_SEARCH_SEGMENTS 1000
#define GROEBNER_STR_MAX 256

/* ================================================================
 *  注册表内部扩展数据（从 groebner_engine.c 迁移）
 * ================================================================ */
typedef struct {
    /** 多项式池 —— 按 poly_id 索引 */
    lvPolynomial **polys;
    int poly_count;    /* 当前多项式数量 */
    int poly_capacity; /* 多项式池容量 */
    int next_poly_id;  /* 下一个多项式 ID */

    /* 理想池 —— 按 ideal_id 索引 */
    lvIdeal **ideals;
    int ideal_count;
    int ideal_capacity;
    int next_ideal_id;

    /* Groebner 基池 —— 按基索引（不直接暴露 ID） */
    lvGroebnerBasis **bases;
    int bases_count;
    int bases_capacity;

    /* 代数簇池 —— 按 variety_id 索引 */
    lvVariety **varieties;
    int variety_count;
    int variety_capacity;
    int next_variety_id;
} lvRegistryData;

/* ================================================================
 *  注册中心全局状态（groebner_engine.c 定义，拆分文件共享）
 * ================================================================ */
extern lvRegistryData *g_data;
extern lv_mutex_t g_data_mutex;
extern int g_data_mutex_initialized;

/* ================================================================
 *  全局互斥锁生命周期（groebner_engine.c 实现）
 * ================================================================ */
void groebner_mutex_ensure(void);
void groebner_lock_guard_init(lvLockGuard *g);

/* ================================================================
 *  锁守卫样板宏（groebner_engine_*.c 共享，收敛 goto cleanup 样板）
 *
 * 用法：
 *   int ret = -1;
 *   GROEBNER_LOCK_GUARD_BEGIN();   // 声明 _lg 并加锁 + g_data 空检查
 *   ...业务代码（失败路径 goto _gcleanup）...
 * GROEBNER_LOCK_GUARD_END();       // _gcleanup 标签 + 解锁
 *   return <表达式>;                // 返回由调用函数自行书写
 *
 * 注意：
 *   - BEGIN 宏内定义局部变量 _lg，函数内不得再声明同名变量；
 *   - 仅适用于清理段只含解锁 + 返回（或函数尾 return）的函数，
 *     清理段含额外 free/错误上报的保持原样；
 *   - 宏为语句序列（非 do-while），调用处务必以分号结尾，
 *     不得放在 if/else 等无大括号语句之后（避免悬空 else）。
 * ================================================================ */
#define GROEBNER_LOCK_GUARD_BEGIN() lvLockGuard _lg; groebner_lock_guard_init(&_lg); if (!g_data) goto _gcleanup;
#define GROEBNER_LOCK_GUARD_END() _gcleanup: lv_lock_guard_destroy(&_lg);

/* ================================================================
 *  注册存储与查询（groebner_engine.c 实现）
 * ================================================================ */
lvRegistryData *registry_data_ensure(void);
int poly_internal_store(lvRegistryData *data, lvPolynomial *poly);
int ideal_internal_store(lvRegistryData *data, lvIdeal *ideal);
int variety_internal_store(lvRegistryData *data, lvVariety *variety);

/* ================================================================
 *  理想 / Gröbner 基内部辅助（groebner_engine_ideal.c 实现）
 * ================================================================ */
lvGroebnerBasis *basis_alloc(int capacity);
void basis_destroy(lvGroebnerBasis *basis);
void ideal_clear_cached_basis(lvIdeal *ideal);

/* ================================================================
 *  Buchberger 核心（groebner_engine_core.c 实现）
 * ================================================================ */
lvGroebnerBasis *groebner_internal_compute(const lvPolynomialRing *ring, lvPolynomial **generators,
                                           int gen_count, lvGroebnerAlgorithm algorithm);
lvGroebnerBasis *groebner_internal_reduce_basis(lvGroebnerBasis *basis, const lvPolynomialRing *ring);

/* ================================================================
 *  数值求解（groebner_engine_variety.c 实现）
 * ================================================================ */
lvPolynomial **groebner_solve_zero_dim(const lvGroebnerBasis *basis, const lvPolynomialRing *ring,
                                       int *solution_count);

#ifdef __cplusplus
}
#endif

#endif /* lv_GROEBNER_ENGINE_INTERNAL_H */
