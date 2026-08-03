#ifndef lv_SIMD_OPS_H
#define lv_SIMD_OPS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ── SIMD capability flags ── */
typedef enum {
    lv_SIMD_NONE = 0,
    lv_SIMD_SSE2 = 1 << 0,
    lv_SIMD_SSE41 = 1 << 1,
    lv_SIMD_AVX = 1 << 2,
    lv_SIMD_AVX2 = 1 << 3,
    lv_SIMD_AVX512F = 1 << 4,
    lv_SIMD_NEON = 1 << 5
} lvSimdCapability;

/* ── SIMD statistics ── */
typedef struct {
    uint64_t vec4_ops;
    uint64_t vec8_ops;
    uint64_t array_ops;
    uint64_t elements_processed;
    uint64_t simd_time_us;
    uint64_t scalar_fallbacks;
} lvSimdStats;

/* ── 4-element double vector ── */
typedef struct {
    double v[4];
} lvVec4d;

/* ── 4-element float vector ── */
typedef struct {
    float v[4];
} lvVec4f;

/* ── 8-element float vector ── */
typedef struct {
    float v[8];
} lvVec8f;

/* ── Capability queries ── */
lvSimdCapability lv_simd_detect(void);
const char *lv_simd_capability_name(lvSimdCapability cap);
void lv_simd_get_stats(lvSimdStats *stats);
void lv_simd_reset_stats(void);

/* ── Vec4d ── */
lvVec4d lv_vec4d_zero(void);
lvVec4d lv_vec4d_one(void);
lvVec4d lv_vec4d_set1(double val);
lvVec4d lv_vec4d_set(double x, double y, double z, double w);
lvVec4d lv_vec4d_load(const double *ptr);
lvVec4d lv_vec4d_loadu(const double *ptr);
lvVec4d lv_vec4d_add(lvVec4d a, lvVec4d b);
lvVec4d lv_vec4d_sub(lvVec4d a, lvVec4d b);
lvVec4d lv_vec4d_mul(lvVec4d a, lvVec4d b);
lvVec4d lv_vec4d_div(lvVec4d a, lvVec4d b);
lvVec4d lv_vec4d_neg(lvVec4d a);
lvVec4d lv_vec4d_sqrt(lvVec4d a);
lvVec4d lv_vec4d_abs(lvVec4d a);
lvVec4d lv_vec4d_max(lvVec4d a, lvVec4d b);
lvVec4d lv_vec4d_min(lvVec4d a, lvVec4d b);
lvVec4d lv_vec4d_fmadd(lvVec4d a, lvVec4d x, lvVec4d y);
lvVec4d lv_vec4d_cmpeq(lvVec4d a, lvVec4d b);
lvVec4d lv_vec4d_cmplt(lvVec4d a, lvVec4d b);
lvVec4d lv_vec4d_cmple(lvVec4d a, lvVec4d b);
lvVec4d lv_vec4d_cmpgt(lvVec4d a, lvVec4d b);
lvVec4d lv_vec4d_cmpge(lvVec4d a, lvVec4d b);
lvVec4d lv_vec4d_select(lvVec4d mask, lvVec4d a, lvVec4d b);

/* ── Vec4d 归约/几何 ── */
double lv_vec4d_hsum(lvVec4d a);
double lv_vec4d_hmax(lvVec4d a);
double lv_vec4d_hmin(lvVec4d a);
double lv_vec4d_dot(lvVec4d a, lvVec4d b);
double lv_vec4d_norm(lvVec4d a);
lvVec4d lv_vec4d_normalize(lvVec4d a);
lvVec4d lv_vec4d_cross(lvVec4d a, lvVec4d b);

/* ── Vec4f ── */
lvVec4f lv_vec4f_zero(void);
lvVec4f lv_vec4f_set1(float val);
lvVec4f lv_vec4f_load(const float *ptr);
lvVec4f lv_vec4f_add(lvVec4f a, lvVec4f b);
lvVec4f lv_vec4f_sub(lvVec4f a, lvVec4f b);
lvVec4f lv_vec4f_mul(lvVec4f a, lvVec4f b);
lvVec4f lv_vec4f_div(lvVec4f a, lvVec4f b);
lvVec4f lv_vec4f_sqrt(lvVec4f a);

/* ── Vec4f 归约 ── */
float lv_vec4f_hsum(lvVec4f a);
float lv_vec4f_dot(lvVec4f a, lvVec4f b);

/* ── Vec8f ── */
lvVec8f lv_vec8f_zero(void);
lvVec8f lv_vec8f_set1(float val);
lvVec8f lv_vec8f_load(const float *ptr);
lvVec8f lv_vec8f_add(lvVec8f a, lvVec8f b);
lvVec8f lv_vec8f_sub(lvVec8f a, lvVec8f b);
lvVec8f lv_vec8f_mul(lvVec8f a, lvVec8f b);
lvVec8f lv_vec8f_div(lvVec8f a, lvVec8f b);

/* ── Matrix SIMD helper ── */
lvVec4d lv_simd_mat4x4_vec4_mul(const double mat[16], lvVec4d vec);

/* ── 批量数组运算 ── */
double lv_simd_dot_product_array(const double *a, const double *b, size_t count);
void lv_simd_norm_array(const double *in, double *out, size_t count);
void lv_simd_scale_array(const double *in, double scale, double *out, size_t count);

#ifdef __cplusplus
}
#endif
#endif
