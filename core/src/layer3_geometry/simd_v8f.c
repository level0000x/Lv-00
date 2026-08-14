/**
 * @file simd_v8f.c
 * @brief 8x float SIMD 向量操作
 *
 * @details SSE2/AVX/NEON 下的 lvVec8f 向量原语集。
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "simd_ops_internal.h"
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

