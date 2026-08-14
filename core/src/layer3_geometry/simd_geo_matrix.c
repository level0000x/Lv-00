/**
 * @file simd_geo_matrix.c
 * @brief 几何运算加速与矩阵运算
 *
 * @details SSE2/AVX/NEON 下的几何运算加速与矩阵运算原语集。
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "simd_ops_internal.h"

#include "lv/geo_utils.h"

#include <math.h>
/* ============== 几何运算加速 ============== */

void lv_simd_distance_array(const double *x1, const double *y1, const double *x2, const double *y2, double *out,
                            size_t count) {
    lv_SIMD_STATS_INC(array_ops);
    lv_SIMD_STATS_ADD(elements_processed, count);
    lv_SIMD_TIME_BEGIN();

    /* geo_distance_2d 为外部标量函数（地理距离，含 atan2/sin 等超越函数），无法向量化，保持标量逐元素 */
    for (size_t i = 0; i < count; i++) {
        out[i] = geo_distance_2d(x1[i], y1[i], x2[i], y2[i]);
    }

    lv_SIMD_TIME_END();
}

void lv_simd_point_line_distance_array(const double *px, const double *py, double x1, double y1, double x2, double y2,
                                       double *out, size_t count) {
    lv_SIMD_STATS_INC(array_ops);
    lv_SIMD_STATS_ADD(elements_processed, count);
    lv_SIMD_TIME_BEGIN();

    double dx = x2 - x1;
    double dy = y2 - y1;
    double len_sq = geo_norm_sq_2d(dx, dy);

    if (len_sq < lv_EPSILON_ULTRA) {
        /* 线段退化为点 */
        for (size_t i = 0; i < count; i++) {
            out[i] = geo_distance_2d(x1, y1, px[i], py[i]);
        }
        lv_SIMD_TIME_END();
        return;
    }

    double len = geo_distance_2d(x1, y1, x2, y2);
    double nx = -dy / len; /* 法向量 */
    double ny = dx / len;

    size_t i = 0;
#if defined(__AVX__)
    __m256d vx1 = _mm256_set1_pd(x1), vy1 = _mm256_set1_pd(y1);
    __m256d vnx = _mm256_set1_pd(nx), vny = _mm256_set1_pd(ny);
    __m256d vsign = _mm256_set1_pd(-0.0);
    for (; i + 4 <= count; i += 4) {
        __m256d ddx = _mm256_sub_pd(_mm256_loadu_pd(px + i), vx1);
        __m256d ddy = _mm256_sub_pd(_mm256_loadu_pd(py + i), vy1);
        __m256d d = _mm256_add_pd(_mm256_mul_pd(ddx, vnx), _mm256_mul_pd(ddy, vny));
        _mm256_storeu_pd(out + i, _mm256_andnot_pd(vsign, d)); /* fabs */
    }
#elif defined(__SSE2__)
    __m128d vx1 = _mm_set1_pd(x1), vy1 = _mm_set1_pd(y1);
    __m128d vnx = _mm_set1_pd(nx), vny = _mm_set1_pd(ny);
    __m128d vsign = _mm_set1_pd(-0.0);
    for (; i + 2 <= count; i += 2) {
        __m128d ddx = _mm_sub_pd(_mm_loadu_pd(px + i), vx1);
        __m128d ddy = _mm_sub_pd(_mm_loadu_pd(py + i), vy1);
        __m128d d = _mm_add_pd(_mm_mul_pd(ddx, vnx), _mm_mul_pd(ddy, vny));
        _mm_storeu_pd(out + i, _mm_andnot_pd(vsign, d)); /* fabs */
    }
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
    float64x2_t vx1 = vdupq_n_f64(x1), vy1 = vdupq_n_f64(y1);
    float64x2_t vnx = vdupq_n_f64(nx), vny = vdupq_n_f64(ny);
    for (; i + 2 <= count; i += 2) {
        float64x2_t ddx = vsubq_f64(vld1q_f64(px + i), vx1);
        float64x2_t ddy = vsubq_f64(vld1q_f64(py + i), vy1);
        float64x2_t d = vaddq_f64(vmulq_f64(ddx, vnx), vmulq_f64(ddy, vny));
        vst1q_f64(out + i, vabsq_f64(d));
    }
#endif

    for (; i < count; i++) {
        double ddx = px[i] - x1;
        double ddy = py[i] - y1;
        out[i] = fabs(ddx * nx + ddy * ny);
    }

    lv_SIMD_TIME_END();
}

void lv_simd_cross2d_array(const double *ax, const double *ay, const double *bx, const double *by, double *out,
                           size_t count) {
    lv_SIMD_STATS_INC(array_ops);
    lv_SIMD_STATS_ADD(elements_processed, count);
    lv_SIMD_TIME_BEGIN();

    size_t i = 0;
#if defined(__AVX__)
    for (; i + 4 <= count; i += 4) {
        __m256d vax = _mm256_loadu_pd(ax + i), vay = _mm256_loadu_pd(ay + i);
        __m256d vbx = _mm256_loadu_pd(bx + i), vby = _mm256_loadu_pd(by + i);
        /* cross = ax * by - ay * bx */
        _mm256_storeu_pd(out + i, _mm256_sub_pd(_mm256_mul_pd(vax, vby), _mm256_mul_pd(vay, vbx)));
    }
#elif defined(__SSE2__)
    for (; i + 4 <= count; i += 4) {
        _mm_storeu_pd(out + i, _mm_sub_pd(_mm_mul_pd(_mm_loadu_pd(ax + i), _mm_loadu_pd(by + i)),
                                          _mm_mul_pd(_mm_loadu_pd(ay + i), _mm_loadu_pd(bx + i))));
        _mm_storeu_pd(out + i + 2, _mm_sub_pd(_mm_mul_pd(_mm_loadu_pd(ax + i + 2), _mm_loadu_pd(by + i + 2)),
                                              _mm_mul_pd(_mm_loadu_pd(ay + i + 2), _mm_loadu_pd(bx + i + 2))));
    }
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
    for (; i + 4 <= count; i += 4) {
        vst1q_f64(out + i, vsubq_f64(vmulq_f64(vld1q_f64(ax + i), vld1q_f64(by + i)),
                                     vmulq_f64(vld1q_f64(ay + i), vld1q_f64(bx + i))));
        vst1q_f64(out + i + 2, vsubq_f64(vmulq_f64(vld1q_f64(ax + i + 2), vld1q_f64(by + i + 2)),
                                         vmulq_f64(vld1q_f64(ay + i + 2), vld1q_f64(bx + i + 2))));
    }
#else
    for (; i + 4 <= count; i += 4) {
        lvVec4d vax = lv_vec4d_loadu(ax + i);
        lvVec4d vay = lv_vec4d_loadu(ay + i);
        lvVec4d vbx = lv_vec4d_loadu(bx + i);
        lvVec4d vby = lv_vec4d_loadu(by + i);
        lvVec4d v1 = lv_vec4d_mul(vax, vby);
        lvVec4d v2 = lv_vec4d_mul(vay, vbx);
        lvVec4d vr = lv_vec4d_sub(v1, v2);
        lv_vec4d_storeu(out + i, vr);
    }
#endif

    for (; i < count; i++) {
        out[i] = ax[i] * by[i] - ay[i] * bx[i];
    }

    lv_SIMD_TIME_END();
}

void lv_simd_point_in_circle_array(const double *px, const double *py, double cx, double cy, double r, int *out,
                                   size_t count) {
    lv_SIMD_STATS_INC(array_ops);
    lv_SIMD_STATS_ADD(elements_processed, count);
    lv_SIMD_TIME_BEGIN();

    double r_sq = r * r;
    size_t i = 0;

#if defined(__AVX__)
    __m256d vcx = _mm256_set1_pd(cx), vcy = _mm256_set1_pd(cy);
    __m256d vrsq = _mm256_set1_pd(r_sq);
    for (; i + 4 <= count; i += 4) {
        __m256d ddx = _mm256_sub_pd(_mm256_loadu_pd(px + i), vcx);
        __m256d ddy = _mm256_sub_pd(_mm256_loadu_pd(py + i), vcy);
        __m256d d2 = _mm256_add_pd(_mm256_mul_pd(ddx, ddx), _mm256_mul_pd(ddy, ddy));
        int m = _mm256_movemask_pd(_mm256_cmp_pd(d2, vrsq, _CMP_LE_OQ));
        out[i + 0] = (m & 1) ? 1 : 0;
        out[i + 1] = (m & 2) ? 1 : 0;
        out[i + 2] = (m & 4) ? 1 : 0;
        out[i + 3] = (m & 8) ? 1 : 0;
    }
#elif defined(__SSE2__)
    __m128d vcx = _mm_set1_pd(cx), vcy = _mm_set1_pd(cy);
    __m128d vrsq = _mm_set1_pd(r_sq);
    for (; i + 2 <= count; i += 2) {
        __m128d ddx = _mm_sub_pd(_mm_loadu_pd(px + i), vcx);
        __m128d ddy = _mm_sub_pd(_mm_loadu_pd(py + i), vcy);
        __m128d d2 = _mm_add_pd(_mm_mul_pd(ddx, ddx), _mm_mul_pd(ddy, ddy));
        int m = _mm_movemask_pd(_mm_cmple_pd(d2, vrsq));
        out[i + 0] = (m & 1) ? 1 : 0;
        out[i + 1] = (m & 2) ? 1 : 0;
    }
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
    float64x2_t vcx = vdupq_n_f64(cx), vcy = vdupq_n_f64(cy);
    float64x2_t vrsq = vdupq_n_f64(r_sq);
    for (; i + 2 <= count; i += 2) {
        float64x2_t ddx = vsubq_f64(vld1q_f64(px + i), vcx);
        float64x2_t ddy = vsubq_f64(vld1q_f64(py + i), vcy);
        float64x2_t d2 = vaddq_f64(vmulq_f64(ddx, ddx), vmulq_f64(ddy, ddy));
        uint64x2_t m = vcleq_f64(d2, vrsq);
        out[i + 0] = vgetq_lane_u64(m, 0) ? 1 : 0;
        out[i + 1] = vgetq_lane_u64(m, 1) ? 1 : 0;
    }
#endif

    for (; i < count; i++) {
        double ddx = px[i] - cx;
        double ddy = py[i] - cy;
        double dist_sq = geo_norm_sq_2d(ddx, ddy);
        out[i] = (dist_sq <= r_sq) ? 1 : 0;
    }

    lv_SIMD_TIME_END();
}

/* ============== 矩阵运算 ============== */

lvVec4d lv_simd_mat4x4_vec4_mul(const double mat[16], lvVec4d vec) {
    lvVec4d result;
    /* result[i] = Σ_k mat[i*4+k] * vec[k]（按行点积，与标量实现语义一致） */
#if defined(__AVX__)
    __m256d vv = _mm256_loadu_pd(vec.v);
    for (int i = 0; i < 4; i++) {
        __m256d row = _mm256_loadu_pd(mat + i * 4);
        __m256d p = _mm256_mul_pd(row, vv);
        __m256d h = _mm256_hadd_pd(p, _mm256_setzero_pd());
        result.v[i] = ((double*)&h)[0] + ((double*)&h)[2];
    }
#elif defined(__SSE2__)
    __m128d v_lo = _mm_loadu_pd(vec.v), v_hi = _mm_loadu_pd(vec.v + 2);
    for (int i = 0; i < 4; i++) {
        __m128d r_lo = _mm_loadu_pd(mat + i * 4);
        __m128d r_hi = _mm_loadu_pd(mat + i * 4 + 2);
        __m128d s = _mm_add_pd(_mm_mul_pd(r_lo, v_lo), _mm_mul_pd(r_hi, v_hi));
        s = _mm_add_pd(s, _mm_shuffle_pd(s, s, 1));
        result.v[i] = _mm_cvtsd_f64(s);
    }
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
    float64x2_t v_lo = vld1q_f64(vec.v), v_hi = vld1q_f64(vec.v + 2);
    for (int i = 0; i < 4; i++) {
        float64x2_t r_lo = vld1q_f64(mat + i * 4), r_hi = vld1q_f64(mat + i * 4 + 2);
        float64x2_t s = vaddq_f64(vmulq_f64(r_lo, v_lo), vmulq_f64(r_hi, v_hi));
        result.v[i] = vgetq_lane_f64(s, 0) + vgetq_lane_f64(s, 1);
    }
#else
    result.v[0] = mat[0] * vec.v[0] + mat[1] * vec.v[1] + mat[2] * vec.v[2] + mat[3] * vec.v[3];
    result.v[1] = mat[4] * vec.v[0] + mat[5] * vec.v[1] + mat[6] * vec.v[2] + mat[7] * vec.v[3];
    result.v[2] = mat[8] * vec.v[0] + mat[9] * vec.v[1] + mat[10] * vec.v[2] + mat[11] * vec.v[3];
    result.v[3] = mat[12] * vec.v[0] + mat[13] * vec.v[1] + mat[14] * vec.v[2] + mat[15] * vec.v[3];
#endif

    lv_SIMD_STATS_INC(vec4_ops);
    return result;
}

void lv_simd_mat4x4_vec4_array_mul(const double mat[16], const double *vecs, double *out, size_t count) {
    lv_SIMD_STATS_INC(array_ops);
    lv_SIMD_STATS_ADD(elements_processed, count);
    lv_SIMD_TIME_BEGIN();

    for (size_t i = 0; i < count; i++) {
        /* 输入/输出数组仅保证 8 字节对齐，用 loadu/storeu */
        lvVec4d v = lv_vec4d_loadu(vecs + i * 4);
        lvVec4d r = lv_simd_mat4x4_vec4_mul(mat, v);
        lv_vec4d_storeu(out + i * 4, r);
    }

    lv_SIMD_TIME_END();
}

void lv_simd_mat3x3_vec2_mul(const double mat[9], double x, double y, double *out_x, double *out_y) {
    /* 齐次坐标变换 */
    double w = mat[6] * x + mat[7] * y + mat[8];
    *out_x = (mat[0] * x + mat[1] * y + mat[2]) / w;
    *out_y = (mat[3] * x + mat[4] * y + mat[5]) / w;
}
