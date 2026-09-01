#ifndef lv_SIMD_OPS_H
#define lv_SIMD_OPS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "lv_api_spec.h" /* lv_PUBLIC_API（K59 导出装饰） */

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
lv_PUBLIC_API uint32_t lv_simd_detect_capabilities(void);
lv_PUBLIC_API bool lv_simd_has_capability(lvSimdCapability cap);
lv_PUBLIC_API const char *lv_simd_capability_name(lvSimdCapability cap);
lv_PUBLIC_API void lv_simd_get_stats(lvSimdStats *stats);
lv_PUBLIC_API void lv_simd_reset_stats(void);

/* ── Vec4d ── */
lvVec4d lv_vec4d_zero(void);
lvVec4d lv_vec4d_one(void);
lvVec4d lv_vec4d_set1(double val);
lvVec4d lv_vec4d_set(double x, double y, double z, double w);
lvVec4d lv_vec4d_load(const double *ptr);
lvVec4d lv_vec4d_loadu(const double *ptr);
lv_PUBLIC_API void lv_vec4d_store(double *ptr, lvVec4d vec);
lv_PUBLIC_API void lv_vec4d_storeu(double *ptr, lvVec4d vec);
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
lv_PUBLIC_API double lv_vec4d_hsum(lvVec4d a);
lv_PUBLIC_API double lv_vec4d_hmax(lvVec4d a);
lv_PUBLIC_API double lv_vec4d_hmin(lvVec4d a);
lv_PUBLIC_API double lv_vec4d_dot(lvVec4d a, lvVec4d b);
lv_PUBLIC_API double lv_vec4d_norm(lvVec4d a);
lvVec4d lv_vec4d_normalize(lvVec4d a);
lvVec4d lv_vec4d_cross(lvVec4d a, lvVec4d b);

/* ── Vec4f ── */
lvVec4f lv_vec4f_zero(void);
lvVec4f lv_vec4f_set1(float val);
lvVec4f lv_vec4f_load(const float *ptr);
lv_PUBLIC_API void lv_vec4f_store(float *ptr, lvVec4f vec);
lvVec4f lv_vec4f_add(lvVec4f a, lvVec4f b);
lvVec4f lv_vec4f_sub(lvVec4f a, lvVec4f b);
lvVec4f lv_vec4f_mul(lvVec4f a, lvVec4f b);
lvVec4f lv_vec4f_div(lvVec4f a, lvVec4f b);
lvVec4f lv_vec4f_sqrt(lvVec4f a);

/* ── Vec4f 归约 ── */
lv_PUBLIC_API float lv_vec4f_hsum(lvVec4f a);
lv_PUBLIC_API float lv_vec4f_dot(lvVec4f a, lvVec4f b);

/* ── Vec8f ── */
lvVec8f lv_vec8f_zero(void);
lvVec8f lv_vec8f_set1(float val);
lvVec8f lv_vec8f_load(const float *ptr);
lv_PUBLIC_API void lv_vec8f_store(float *ptr, lvVec8f vec);
lvVec8f lv_vec8f_add(lvVec8f a, lvVec8f b);
lvVec8f lv_vec8f_sub(lvVec8f a, lvVec8f b);
lvVec8f lv_vec8f_mul(lvVec8f a, lvVec8f b);
lvVec8f lv_vec8f_div(lvVec8f a, lvVec8f b);
lv_PUBLIC_API float lv_vec8f_hsum(lvVec8f a);

/* ── Matrix SIMD helper ── */
lvVec4d lv_simd_mat4x4_vec4_mul(const double mat[16], lvVec4d vec);

/* ── 4x4 列主序矩阵（收敛：algebra_mode.c / geo_visual_complete.c 原静态实现上移共享） ──
 * 语义与收敛前逐位一致：
 *   - identity：清零后对角置 1（+0.0 / +1.0，与各调用方原 memset 置 0 位型一致）；
 *   - mul：result 允许与 a / b 别名（内部先写 tmp 再复制），double 版按 k 序累加、
 *     float 版按展开式自左向右累加，与各调用方原实现表达式结构完全相同。 */
static inline void lv_mat4_identity(double m[16]) {
    for (int i = 0; i < 16; i++)
        m[i] = 0.0;
    m[0] = m[5] = m[10] = m[15] = 1.0;
}

static inline void lv_mat4_mul(double result[16], const double a[16], const double b[16]) {
    double tmp[16];
    for (int j = 0; j < 4; j++) {
        for (int i = 0; i < 4; i++) {
            double sum = 0.0;
            for (int k = 0; k < 4; k++) {
                sum += a[i + k * 4] * b[k + j * 4];
            }
            tmp[i + j * 4] = sum;
        }
    }
    for (int i = 0; i < 16; i++)
        result[i] = tmp[i];
}

static inline void lv_mat4_identity_f(float m[16]) {
    for (int i = 0; i < 16; i++)
        m[i] = 0.0f;
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

static inline void lv_mat4_mul_f(float result[16], const float a[16], const float b[16]) {
    float tmp[16];
    for (int col = 0; col < 4; col++) {
        for (int row = 0; row < 4; row++) {
            tmp[col * 4 + row] = a[0 * 4 + row] * b[col * 4 + 0] + a[1 * 4 + row] * b[col * 4 + 1] +
                                 a[2 * 4 + row] * b[col * 4 + 2] + a[3 * 4 + row] * b[col * 4 + 3];
        }
    }
    for (int i = 0; i < 16; i++)
        result[i] = tmp[i];
}

/* ── 批量数组运算 ── */
lv_PUBLIC_API double lv_simd_dot_product_array(const double *a, const double *b, size_t count);
lv_PUBLIC_API void lv_simd_norm_array(const double *in, double *out, size_t count);
lv_PUBLIC_API void lv_simd_scale_array(const double *in, double scale, double *out, size_t count);

#ifdef __cplusplus
}
#endif
#endif
