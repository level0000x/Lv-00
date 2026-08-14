/**
 * @file simd_v4d.c
 * @brief 4x double SIMD 向量操作（由 simd_ops.c 拆分子模块）
 *
 * @details SSE2/AVX/NEON 下的 lvVec4d 向量原语集。
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "simd_ops_internal.h"

#include "lv/lv_utils.h"

#include <math.h>
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

