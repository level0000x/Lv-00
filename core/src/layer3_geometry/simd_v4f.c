/**
 * @file simd_v4f.c
 * @brief 4x float SIMD 向量操作
 *
 * @details SSE2/AVX/NEON 下的 lvVec4f 向量原语集。
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "simd_ops_internal.h"

#include <math.h>
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

