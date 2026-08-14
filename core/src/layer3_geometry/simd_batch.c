/**
 * @file simd_batch.c
 * @brief 批量数组 SIMD 运算
 *
 * @details SSE2/AVX/NEON 下的数组级批量运算原语集。
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "simd_ops_internal.h"

#include <math.h>
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

