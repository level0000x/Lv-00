/**
 * @file geometry_transform_apply.c
 * @brief 变换应用与复合（由 geometry_transform.c 拆分子模块）
 *
 * @details lvTransform 对点/双精度坐标的仿射应用、变换序列与复合。
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "lv/lv_platform.h"
#include "lv/lv_internal.h"

#include "lv/geometry_transform.h"

#include "lv/constraint_graph.h"
#include "lv/lv_numeric.h"
#include "lv/lv_strbuf.h"
#include "lv/lv_str_utils.h"
#include "lv/symbolic_coord.h"
#include "lv/lv_utils.h"
#include "lv/lv_xmacro.h" /* LV_DISPATCH_VOID */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** 初始变换序列容量（原 geometry_transform.c 内部常量移入） */
#define TRANSFORM_SEQ_INIT_CAPACITY 8
/* ============== 变换应用实现 ============== */

bool lv_transform_apply_point(const lvTransform *t, mpq_t x, mpq_t y) {
    if (!t || !t->matrix_valid) {
        return false;
    }

    mpq_t new_x, new_y;
    mpq_init(new_x);
    mpq_init(new_y);

    /* 应用矩阵变换 */
    /* new_x = a*x + b*y + tx */
    mpq_mul(new_x, t->matrix.a, x);
    mpq_add(new_x, new_x, t->matrix.tx);
    mpq_t temp;
    mpq_init(temp);
    mpq_mul(temp, t->matrix.b, y);
    mpq_add(new_x, new_x, temp);

    /* new_y = c*x + d*y + ty */
    mpq_mul(new_y, t->matrix.c, x);
    mpq_add(new_y, new_y, t->matrix.ty);
    mpq_mul(temp, t->matrix.d, y);
    mpq_add(new_y, new_y, temp);

    mpq_set(x, new_x);
    mpq_set(y, new_y);

    mpq_clear(new_x);
    mpq_clear(new_y);
    mpq_clear(temp);

    return true;
}

void lv_transform_apply_double(const lvTransform *t, double src_x, double src_y, double *dst_x, double *dst_y) {
    if (!t || !t->matrix_valid || !dst_x || !dst_y) {
        if (dst_x) *dst_x = src_x;
        if (dst_y) *dst_y = src_y;
        return;
    }

    double a = mpq_get_d(t->matrix.a);
    double b = mpq_get_d(t->matrix.b);
    double tx = mpq_get_d(t->matrix.tx);
    double c = mpq_get_d(t->matrix.c);
    double d = mpq_get_d(t->matrix.d);
    double ty = mpq_get_d(t->matrix.ty);

    *dst_x = a * src_x + b * src_y + tx;
    *dst_y = c * src_x + d * src_y + ty;
}

void lv_transform_apply_double4x4(const double t[16], const double *in, double *out, size_t count) {
    if (!t || !in || !out) return;

    for (size_t i = 0; i < count; i++) {
        double x = in[i * 3];
        double y = in[i * 3 + 1];
        double z = in[i * 3 + 2];
        double w = 1.0;
        out[i * 3]     = t[0] * x + t[1] * y + t[2] * z + t[3] * w;
        out[i * 3 + 1] = t[4] * x + t[5] * y + t[6] * z + t[7] * w;
        out[i * 3 + 2] = t[8] * x + t[9] * y + t[10] * z + t[11] * w;
    }
}

bool lv_transform_get_matrix(lvTransform *t, lvTransformMatrix *matrix) {
    if (!t || !matrix) {
        return false;
    }

    if (!t->matrix_valid) {
        return false;
    }

    mpq_init(matrix->a);
    mpq_init(matrix->b);
    mpq_init(matrix->tx);
    mpq_init(matrix->c);
    mpq_init(matrix->d);
    mpq_init(matrix->ty);

    mpq_set(matrix->a, t->matrix.a);
    mpq_set(matrix->b, t->matrix.b);
    mpq_set(matrix->tx, t->matrix.tx);
    mpq_set(matrix->c, t->matrix.c);
    mpq_set(matrix->d, t->matrix.d);
    mpq_set(matrix->ty, t->matrix.ty);

    return true;
}

/* ============== 变换复合实现 ============== */

lvTransformSequence *lv_transform_sequence_create(void) {
    lvTransformSequence *seq = (lvTransformSequence *) lv_calloc(1, sizeof(lvTransformSequence));
    if (!seq) {
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "lv_transform_sequence_create: calloc failed");
    }

    lv_darray_init(&seq->transforms_da, sizeof(lvTransform *));
    if (!lv_darray_reserve(&seq->transforms_da, TRANSFORM_SEQ_INIT_CAPACITY)) {
        lv_free((void **) &seq);
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "lv_transform_sequence_create: darray_reserve failed");
    }

    return seq;
}

void lv_transform_sequence_destroy(lvTransformSequence *seq) {
    if (!seq) {
        return;
    }

    for (int i = 0; i < seq->transforms_da.count; i++) {
        lvTransform **pp = (lvTransform **)lv_darray_get(&seq->transforms_da, i);
        lv_transform_unref(*pp);
    }
    lv_darray_free(&seq->transforms_da);
    lv_free((void **) &seq);
}

bool lv_transform_sequence_add(lvTransformSequence *seq, lvTransform *t) {
    if (!seq || !t) {
        return false;
    }

    int idx = lv_darray_push(&seq->transforms_da, &t);
    if (idx < 0) {
        return false;
    }

    lv_transform_ref(t);
    seq->composite_valid = false;

    return true;
}

lvTransform *lv_transform_compose(const lvTransform *t1, const lvTransform *t2) {
    if (!t1 || !t2) {
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "lv_transform_compose: NULL input transform");
    }

    lvTransform *result = (lvTransform *) lv_calloc(1, sizeof(lvTransform));
    if (!result) {
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "lv_transform_compose: calloc failed");
    }

    result->type = TRANSFORM_GLUING;
    result->ref_count = 1;

    /* 初始化矩阵 */
    mpq_init(result->matrix.a);
    mpq_init(result->matrix.b);
    mpq_init(result->matrix.tx);
    mpq_init(result->matrix.c);
    mpq_init(result->matrix.d);
    mpq_init(result->matrix.ty);

    /* 矩阵乘法：M = M2 * M1 */
    mpq_t temp;
    mpq_init(temp);

    /* a = a2*a1 + b2*c1 */
    mpq_mul(result->matrix.a, t2->matrix.a, t1->matrix.a);
    mpq_mul(temp, t2->matrix.b, t1->matrix.c);
    mpq_add(result->matrix.a, result->matrix.a, temp);

    /* b = a2*b1 + b2*d1 */
    mpq_mul(result->matrix.b, t2->matrix.a, t1->matrix.b);
    mpq_mul(temp, t2->matrix.b, t1->matrix.d);
    mpq_add(result->matrix.b, result->matrix.b, temp);

    /* tx = a2*tx1 + b2*ty1 + tx2 */
    mpq_mul(result->matrix.tx, t2->matrix.a, t1->matrix.tx);
    mpq_mul(temp, t2->matrix.b, t1->matrix.ty);
    mpq_add(result->matrix.tx, result->matrix.tx, temp);
    mpq_add(result->matrix.tx, result->matrix.tx, t2->matrix.tx);

    /* c = c2*a1 + d2*c1 */
    mpq_mul(result->matrix.c, t2->matrix.c, t1->matrix.a);
    mpq_mul(temp, t2->matrix.d, t1->matrix.c);
    mpq_add(result->matrix.c, result->matrix.c, temp);

    /* d = c2*b1 + d2*d1 */
    mpq_mul(result->matrix.d, t2->matrix.c, t1->matrix.b);
    mpq_mul(temp, t2->matrix.d, t1->matrix.d);
    mpq_add(result->matrix.d, result->matrix.d, temp);

    /* ty = c2*tx1 + d2*ty1 + ty2 */
    mpq_mul(result->matrix.ty, t2->matrix.c, t1->matrix.tx);
    mpq_mul(temp, t2->matrix.d, t1->matrix.ty);
    mpq_add(result->matrix.ty, result->matrix.ty, temp);
    mpq_add(result->matrix.ty, result->matrix.ty, t2->matrix.ty);

    mpq_clear(temp);

    result->matrix_valid = true;
    result->is_isometry = t1->is_isometry && t2->is_isometry;
    result->is_orientation_preserving = t1->is_orientation_preserving == t2->is_orientation_preserving;

    return result;
}

lvTransform *lv_transform_sequence_composite(const lvTransformSequence *seq) {
    if (!seq || seq->transforms_da.count == 0) {
        lv_RETURN_ERROR_NULL(lv_ERROR_INVALID_PARAM, "lv_transform_sequence_composite: NULL or empty sequence");
    }

    lvTransform *result = lv_transform_identity();
    if (!result) {
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "lv_transform_sequence_composite: identity creation failed");
    }

    for (int i = 0; i < seq->transforms_da.count; i++) {
        lvTransform **pp = (lvTransform **)lv_darray_get(&seq->transforms_da, i);
        lvTransform *temp = lv_transform_compose(result, *pp);
        lv_transform_destroy(result);
        if (!temp) {
            lv_RETURN_ERROR_NULL(lv_ERROR_INTERNAL, "lv_transform_sequence_composite: compose failed");
        }
        result = temp;
    }

    return result;
}

bool lv_transform_sequence_apply(const lvTransformSequence *seq, mpq_t x, mpq_t y) {
    if (!seq) {
        return false;
    }

    for (int i = 0; i < seq->transforms_da.count; i++) {
        lvTransform **pp = (lvTransform **)lv_darray_get(&seq->transforms_da, i);
        if (!lv_transform_apply_point(*pp, x, y)) {
            return false;
        }
    }

    return true;
}

/* ============================================================
 * 补齐实现（批次 C-㊺续9）：以下 5 个 API 头文件声明但原实现缺失
 * （M5，零消费者故链接未暴露）
 * ============================================================ */

void lv_transform_apply_mpq(const lvTransform *t, const mpq_t src_x, const mpq_t src_y, mpq_t dst_x, mpq_t dst_y) {
    if (!t || !t->matrix_valid || !dst_x || !dst_y)
        return;

    /* dst_x = a*src_x + b*src_y + tx；dst_y = c*src_x + d*src_y + ty */
    mpq_t temp;
    mpq_init(temp);

    mpq_mul(dst_x, t->matrix.a, src_x);
    mpq_mul(temp, t->matrix.b, src_y);
    mpq_add(dst_x, dst_x, temp);
    mpq_add(dst_x, dst_x, t->matrix.tx);

    mpq_mul(dst_y, t->matrix.c, src_x);
    mpq_mul(temp, t->matrix.d, src_y);
    mpq_add(dst_y, dst_y, temp);
    mpq_add(dst_y, dst_y, t->matrix.ty);

    mpq_clear(temp);
}

void lv_transform_identity_double(double out[16]) {
    if (!out)
        return;
    memset(out, 0, 16 * sizeof(double));
    out[0] = 1.0;
    out[5] = 1.0;
    out[10] = 1.0;
    out[15] = 1.0;
}

void lv_transform_translate_double(double out[16], double x, double y, double z) {
    lv_transform_identity_double(out);
    if (!out)
        return;
    out[12] = x;
    out[13] = y;
    out[14] = z;
}

void lv_transform_rotate_double(double out[16], double angle_rad, double x, double y, double z) {
    lv_transform_identity_double(out);
    if (!out)
        return;

    /* Rodrigues 公式：绕单位轴 (x,y,z) 旋转 angle_rad */
    double len = sqrt(x * x + y * y + z * z);
    if (len < 1e-15) {
        return; /* 零轴：保持恒等 */
    }
    double nx = x / len, ny = y / len, nz = z / len;
    double c = cos(angle_rad), s = sin(angle_rad), t = 1.0 - c;

    out[0] = t * nx * nx + c;
    out[1] = t * nx * ny - s * nz;
    out[2] = t * nx * nz + s * ny;
    out[4] = t * nx * ny + s * nz;
    out[5] = t * ny * ny + c;
    out[6] = t * ny * nz - s * nx;
    out[8] = t * nx * nz - s * ny;
    out[9] = t * ny * nz + s * nx;
    out[10] = t * nz * nz + c;
}

void lv_transform_scale_double(double out[16], double sx, double sy, double sz) {
    lv_transform_identity_double(out);
    if (!out)
        return;
    out[0] = sx;
    out[5] = sy;
    out[10] = sz;
}

/* 补齐（C-㊺续9）：头文件声明 lv_transform_sequence_compose_all，
 * 原实现缺失（M5）——委托既有 sequence_composite（语义等价） */
lvTransform *lv_transform_sequence_compose_all(const lvTransformSequence *seq) {
    return lv_transform_sequence_composite(seq);
}

