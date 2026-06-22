#ifndef LV00_SIMD_OPS_H
#define LV00_SIMD_OPS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/* ── SIMD capability flags ── */
typedef enum {
    LV00_SIMD_NONE    = 0,
    LV00_SIMD_SSE2    = 1 << 0,
    LV00_SIMD_SSE41   = 1 << 1,
    LV00_SIMD_AVX     = 1 << 2,
    LV00_SIMD_AVX2    = 1 << 3,
    LV00_SIMD_AVX512F = 1 << 4,
    LV00_SIMD_NEON    = 1 << 5
} Lv00SimdCapability;

/* ── SIMD statistics ── */
typedef struct {
    uint64_t vec4_ops;
    uint64_t vec8_ops;
    uint64_t array_ops;
    uint64_t elements_processed;
    uint64_t simd_time_us;
    uint64_t scalar_fallbacks;
} Lv00SimdStats;

/* ── 4-element double vector ── */
typedef struct {
    double v[4];
} Lv00Vec4d;

/* ── 4-element float vector ── */
typedef struct {
    float v[4];
} Lv00Vec4f;

/* ── 8-element float vector ── */
typedef struct {
    float v[8];
} Lv00Vec8f;

/* ── Capability queries ── */
Lv00SimdCapability lv00_simd_detect(void);
const char *lv00_simd_capability_name(Lv00SimdCapability cap);
void lv00_simd_get_stats(Lv00SimdStats *stats);
void lv00_simd_reset_stats(void);

/* ── Vec4d ── */
Lv00Vec4d lv00_vec4d_zero(void);
Lv00Vec4d lv00_vec4d_one(void);
Lv00Vec4d lv00_vec4d_set1(double val);
Lv00Vec4d lv00_vec4d_set(double x, double y, double z, double w);
Lv00Vec4d lv00_vec4d_load(const double *ptr);
Lv00Vec4d lv00_vec4d_loadu(const double *ptr);
Lv00Vec4d lv00_vec4d_add(Lv00Vec4d a, Lv00Vec4d b);
Lv00Vec4d lv00_vec4d_sub(Lv00Vec4d a, Lv00Vec4d b);
Lv00Vec4d lv00_vec4d_mul(Lv00Vec4d a, Lv00Vec4d b);
Lv00Vec4d lv00_vec4d_div(Lv00Vec4d a, Lv00Vec4d b);
Lv00Vec4d lv00_vec4d_neg(Lv00Vec4d a);
Lv00Vec4d lv00_vec4d_sqrt(Lv00Vec4d a);
Lv00Vec4d lv00_vec4d_abs(Lv00Vec4d a);
Lv00Vec4d lv00_vec4d_max(Lv00Vec4d a, Lv00Vec4d b);
Lv00Vec4d lv00_vec4d_min(Lv00Vec4d a, Lv00Vec4d b);
Lv00Vec4d lv00_vec4d_fmadd(Lv00Vec4d a, Lv00Vec4d x, Lv00Vec4d y);
Lv00Vec4d lv00_vec4d_cmpeq(Lv00Vec4d a, Lv00Vec4d b);
Lv00Vec4d lv00_vec4d_cmplt(Lv00Vec4d a, Lv00Vec4d b);
Lv00Vec4d lv00_vec4d_cmple(Lv00Vec4d a, Lv00Vec4d b);
Lv00Vec4d lv00_vec4d_cmpgt(Lv00Vec4d a, Lv00Vec4d b);
Lv00Vec4d lv00_vec4d_cmpge(Lv00Vec4d a, Lv00Vec4d b);
Lv00Vec4d lv00_vec4d_select(Lv00Vec4d mask, Lv00Vec4d a, Lv00Vec4d b);

/* ── Vec4f ── */
Lv00Vec4f lv00_vec4f_zero(void);
Lv00Vec4f lv00_vec4f_set1(float val);
Lv00Vec4f lv00_vec4f_load(const float *ptr);
Lv00Vec4f lv00_vec4f_add(Lv00Vec4f a, Lv00Vec4f b);
Lv00Vec4f lv00_vec4f_sub(Lv00Vec4f a, Lv00Vec4f b);
Lv00Vec4f lv00_vec4f_mul(Lv00Vec4f a, Lv00Vec4f b);
Lv00Vec4f lv00_vec4f_div(Lv00Vec4f a, Lv00Vec4f b);
Lv00Vec4f lv00_vec4f_sqrt(Lv00Vec4f a);

/* ── Vec8f ── */
Lv00Vec8f lv00_vec8f_zero(void);
Lv00Vec8f lv00_vec8f_set1(float val);
Lv00Vec8f lv00_vec8f_load(const float *ptr);
Lv00Vec8f lv00_vec8f_add(Lv00Vec8f a, Lv00Vec8f b);
Lv00Vec8f lv00_vec8f_sub(Lv00Vec8f a, Lv00Vec8f b);
Lv00Vec8f lv00_vec8f_mul(Lv00Vec8f a, Lv00Vec8f b);
Lv00Vec8f lv00_vec8f_div(Lv00Vec8f a, Lv00Vec8f b);

/* ── Matrix SIMD helper ── */
Lv00Vec4d lv00_simd_mat4x4_vec4_mul(const double mat[16], Lv00Vec4d vec);

#ifdef __cplusplus
}
#endif
#endif
