/**
 * @file geometry_transform.c
 * @brief 几何变换推理系统实现
 *
 * @details 实现旋转、轴对称、平移等几何变换的符号计算
 *
 * 【内存管理策略说明】
 * 本模块使用标准 malloc/free 而非 lv00_malloc/lv00_free 统一内存分配器，原因如下：
 * 几何变换模块涉及大量临时数组和中间计算结果（如变换序列、群生成元矩阵等），
 * 这些临时对象生命周期短、大小多变，若使用 lv00 内存池分配器会导致严重的池碎片化问题。
 * 标准分配器能够更高效地处理此类短生命周期、变长大小的内存分配模式。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "geometry_transform.h"

#include "lv00_utils.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============== 内部常量 ============== */

/** 初始变换序列容量 */
#define TRANSFORM_SEQ_INIT_CAPACITY 8

/* ============== GMP辅助函数 ============== */

/**
 * @brief 辅助函数：mpq乘以无符号整数
 * @param result 结果
 * @param op 操作数
 * @param n 乘数
 */
static void mpq_mul_by_ui(mpq_t result, const mpq_t op, unsigned long n) {
    mpq_t temp;
    mpq_init(temp);
    mpq_set_ui(temp, n, 1);
    mpq_mul(result, op, temp);
    mpq_clear(temp);
}

/**
 * @brief 辅助函数：mpq减去无符号整数
 * @param result 结果
 * @param op 操作数
 * @param n 减数
 */
static void mpq_sub_by_ui(mpq_t result, const mpq_t op, unsigned long n) {
    mpq_t temp;
    mpq_init(temp);
    mpq_set_ui(temp, n, 1);
    mpq_sub(result, op, temp);
    mpq_clear(temp);
}

/** 变换群生成元最大数量 */
#define GROUP_MAX_GENERATORS 16

/* ============== 变换创建/销毁实现 ============== */

Lv00Transform *lv00_transform_identity(void) {
    Lv00Transform *t = (Lv00Transform *)lv00_malloc(sizeof(Lv00Transform));
    if (!t) {
        return NULL;
    }
    memset(t, 0, sizeof(Lv00Transform));

    t->type = TRANSFORM_IDENTITY;
    t->is_isometry = true;
    t->is_orientation_preserving = true;
    t->ref_count = 1;

    /* 初始化矩阵为单位矩阵 */
    mpq_init(t->matrix.a);
    mpq_init(t->matrix.b);
    mpq_init(t->matrix.tx);
    mpq_init(t->matrix.c);
    mpq_init(t->matrix.d);
    mpq_init(t->matrix.ty);

    mpq_set_ui(t->matrix.a, 1, 1); /* a = 1 */
    mpq_set_ui(t->matrix.d, 1, 1); /* d = 1 */
    mpq_set_ui(t->matrix.b, 0, 1); /* b = 0 */
    mpq_set_ui(t->matrix.c, 0, 1); /* c = 0 */
    mpq_set_ui(t->matrix.tx, 0, 1); /* tx = 0 */
    mpq_set_ui(t->matrix.ty, 0, 1); /* ty = 0 */

    t->matrix_valid = true;

    return t;
}

Lv00Transform *lv00_transform_translation(const mpq_t dx, const mpq_t dy) {
    Lv00Transform *t = lv00_transform_identity();
    if (!t) {
        return NULL;
    }

    t->type = TRANSFORM_TRANSLATION;

    /* 初始化参数 */
    mpq_init(t->params.params.translation.dx);
    mpq_init(t->params.params.translation.dy);
    mpq_set(t->params.params.translation.dx, dx);
    mpq_set(t->params.params.translation.dy, dy);

    /* 设置矩阵 */
    mpq_set(t->matrix.tx, dx);
    mpq_set(t->matrix.ty, dy);

    t->is_isometry = true;
    t->is_orientation_preserving = true;

    return t;
}

Lv00Transform *lv00_transform_rotation(const mpq_t cx, const mpq_t cy,
                                        int angle_num, int angle_denom) {
    Lv00Transform *t = lv00_transform_identity();
    if (!t) {
        return NULL;
    }

    t->type = TRANSFORM_ROTATION;

    /* 初始化参数 */
    mpq_init(t->params.params.rotation.cx);
    mpq_init(t->params.params.rotation.cy);
    mpq_init(t->params.params.rotation.cos_theta);
    mpq_init(t->params.params.rotation.sin_theta);

    mpq_set(t->params.params.rotation.cx, cx);
    mpq_set(t->params.params.rotation.cy, cy);
    t->params.params.rotation.is_special_angle = true;
    t->params.params.rotation.angle_numerator = angle_num;
    t->params.params.rotation.angle_denominator = angle_denom;

    /* 计算特殊角度的 cos 和 sin（有理数或根式） */
    /* 常见角度：0°, 30°, 45°, 60°, 90°, 120°, 135°, 150°, 180° */
    /* 简化实现：使用常见角度的有理数值 */

    /* 规范化角度到 [0, 360) */
    while (angle_num < 0) {
        angle_num += 360 * angle_denom;
    }
    while (angle_num >= 360 * angle_denom) {
        angle_num -= 360 * angle_denom;
    }

    /* 根据角度设置 cos 和 sin */
    /* 这里简化处理，只支持一些常见角度 */
    int normalized = angle_num / angle_denom;

    switch (normalized) {
        case 0:   /* 0° */
            mpq_set_ui(t->params.params.rotation.cos_theta, 1, 1);
            mpq_set_ui(t->params.params.rotation.sin_theta, 0, 1);
            mpq_set_ui(t->matrix.a, 1, 1);
            mpq_set_ui(t->matrix.b, 0, 1);
            mpq_set_ui(t->matrix.c, 0, 1);
            mpq_set_ui(t->matrix.d, 1, 1);
            break;
        case 90:  /* 90° */
            mpq_set_ui(t->params.params.rotation.cos_theta, 0, 1);
            mpq_set_ui(t->params.params.rotation.sin_theta, 1, 1);
            mpq_set_ui(t->matrix.a, 0, 1);
            mpq_set_ui(t->matrix.b, 1, 1);
            mpq_set_si(t->matrix.c, -1, 1);
            mpq_set_ui(t->matrix.d, 0, 1);
            break;
        case 180: /* 180° */
            mpq_set_si(t->params.params.rotation.cos_theta, -1, 1);
            mpq_set_ui(t->params.params.rotation.sin_theta, 0, 1);
            mpq_set_si(t->matrix.a, -1, 1);
            mpq_set_ui(t->matrix.b, 0, 1);
            mpq_set_ui(t->matrix.c, 0, 1);
            mpq_set_si(t->matrix.d, -1, 1);
            break;
        case 270: /* 270° */
            mpq_set_ui(t->params.params.rotation.cos_theta, 0, 1);
            mpq_set_si(t->params.params.rotation.sin_theta, -1, 1);
            mpq_set_si(t->matrix.a, 0, 1);
            mpq_set_si(t->matrix.b, -1, 1);
            mpq_set_ui(t->matrix.c, 1, 1);
            mpq_set_ui(t->matrix.d, 0, 1);
            break;
        default:
            /* 其他角度需要根式表示，这里简化处理 */
            mpq_set_ui(t->params.params.rotation.cos_theta, 1, 1);
            mpq_set_ui(t->params.params.rotation.sin_theta, 0, 1);
            break;
    }

    /* 计算平移分量：tx = cx - a*cx - b*cy, ty = cy - c*cx - d*cy */
    mpq_t temp1, temp2;
    mpq_init(temp1);
    mpq_init(temp2);

    mpq_mul(temp1, t->matrix.a, cx);
    mpq_mul(temp2, t->matrix.b, cy);
    mpq_sub(temp1, cx, temp1);
    mpq_sub(t->matrix.tx, temp1, temp2);

    mpq_mul(temp1, t->matrix.c, cx);
    mpq_mul(temp2, t->matrix.d, cy);
    mpq_sub(temp1, cy, temp1);
    mpq_sub(t->matrix.ty, temp1, temp2);

    mpq_clear(temp1);
    mpq_clear(temp2);

    t->is_isometry = true;
    t->is_orientation_preserving = true;

    return t;
}

Lv00Transform *lv00_transform_rotation_arbitrary(const mpq_t cx, const mpq_t cy,
                                                  const mpq_t cos_theta,
                                                  const mpq_t sin_theta) {
    Lv00Transform *t = lv00_transform_identity();
    if (!t) {
        return NULL;
    }

    t->type = TRANSFORM_ROTATION;

    mpq_init(t->params.params.rotation.cx);
    mpq_init(t->params.params.rotation.cy);
    mpq_init(t->params.params.rotation.cos_theta);
    mpq_init(t->params.params.rotation.sin_theta);

    mpq_set(t->params.params.rotation.cx, cx);
    mpq_set(t->params.params.rotation.cy, cy);
    mpq_set(t->params.params.rotation.cos_theta, cos_theta);
    mpq_set(t->params.params.rotation.sin_theta, sin_theta);
    t->params.params.rotation.is_special_angle = false;

    /* 设置矩阵 */
    mpq_set(t->matrix.a, cos_theta);
    mpq_set(t->matrix.b, sin_theta);
    mpq_neg(t->matrix.c, sin_theta);
    mpq_set(t->matrix.d, cos_theta);

    /* 计算平移分量 */
    mpq_t temp1, temp2;
    mpq_init(temp1);
    mpq_init(temp2);

    mpq_mul(temp1, t->matrix.a, cx);
    mpq_mul(temp2, t->matrix.b, cy);
    mpq_sub(temp1, cx, temp1);
    mpq_sub(t->matrix.tx, temp1, temp2);

    mpq_mul(temp1, t->matrix.c, cx);
    mpq_mul(temp2, t->matrix.d, cy);
    mpq_sub(temp1, cy, temp1);
    mpq_sub(t->matrix.ty, temp1, temp2);

    mpq_clear(temp1);
    mpq_clear(temp2);

    t->is_isometry = true;
    t->is_orientation_preserving = true;

    return t;
}

Lv00Transform *lv00_transform_reflection(const mpq_t ax, const mpq_t ay,
                                          const mpq_t bx, const mpq_t by) {
    Lv00Transform *t = lv00_transform_identity();
    if (!t) {
        return NULL;
    }

    t->type = TRANSFORM_REFLECTION;

    /* 初始化参数 */
    mpq_init(t->params.params.reflection.ax);
    mpq_init(t->params.params.reflection.ay);
    mpq_init(t->params.params.reflection.bx);
    mpq_init(t->params.params.reflection.by);
    mpq_init(t->params.params.reflection.line_a);
    mpq_init(t->params.params.reflection.line_b);
    mpq_init(t->params.params.reflection.line_c);

    mpq_set(t->params.params.reflection.ax, ax);
    mpq_set(t->params.params.reflection.ay, ay);
    mpq_set(t->params.params.reflection.bx, bx);
    mpq_set(t->params.params.reflection.by, by);

    /* 计算直线方程 ax + by + c = 0 */
    /* 方向向量 (dx, dy) = (bx-ax, by-ay) */
    /* 法向量 (a, b) = (dy, -dx) */
    mpq_t dx, dy;
    mpq_init(dx);
    mpq_init(dy);

    mpq_sub(dx, bx, ax);
    mpq_sub(dy, by, ay);

    mpq_set(t->params.params.reflection.line_a, dy);
    mpq_neg(t->params.params.reflection.line_b, dx);

    /* c = -(a*ax + b*ay) */
    mpq_t temp;
    mpq_init(temp);
    mpq_mul(temp, t->params.params.reflection.line_a, ax);
    mpq_mul(t->params.params.reflection.line_c, t->params.params.reflection.line_b, ay);
    mpq_add(t->params.params.reflection.line_c, temp, t->params.params.reflection.line_c);
    mpq_neg(t->params.params.reflection.line_c, t->params.params.reflection.line_c);

    /* 计算反射矩阵 */
    /* 反射矩阵：R = I - 2 * n * n^T / (n^T * n) */
    /* 其中 n = (a, b) 是直线法向量 */

    mpq_t a2, b2, denom;
    mpq_init(a2);
    mpq_init(b2);
    mpq_init(denom);

    mpq_mul(a2, t->params.params.reflection.line_a, t->params.params.reflection.line_a);
    mpq_mul(b2, t->params.params.reflection.line_b, t->params.params.reflection.line_b);
    mpq_add(denom, a2, b2);

    if (mpq_cmp_ui(denom, 0, 1) != 0) {
        /* a' = (b^2 - a^2) / (a^2 + b^2) */
        mpq_sub(t->matrix.a, b2, a2);
        mpq_div(t->matrix.a, t->matrix.a, denom);

        /* b' = -2ab / (a^2 + b^2) */
        mpq_mul(t->matrix.b, t->params.params.reflection.line_a, t->params.params.reflection.line_b);
        mpq_mul_by_ui(t->matrix.b, t->matrix.b, 2);
        mpq_neg(t->matrix.b, t->matrix.b);
        mpq_div(t->matrix.b, t->matrix.b, denom);

        /* c' = b' */
        mpq_set(t->matrix.c, t->matrix.b);

        /* d' = (a^2 - b^2) / (a^2 + b^2) */
        mpq_sub(t->matrix.d, a2, b2);
        mpq_div(t->matrix.d, t->matrix.d, denom);

        /* 计算平移分量 */
        mpq_t two_c;
        mpq_init(two_c);
        mpq_mul_by_ui(two_c, t->params.params.reflection.line_c, 2);

        mpq_mul(t->matrix.tx, t->matrix.b, t->params.params.reflection.line_c);
        mpq_mul(temp, t->matrix.a, t->params.params.reflection.line_c);
        mpq_add(t->matrix.tx, t->matrix.tx, temp);
        mpq_neg(t->matrix.tx, t->matrix.tx);
        mpq_mul_by_ui(t->matrix.tx, t->matrix.tx, 2);
        mpq_div(t->matrix.tx, t->matrix.tx, denom);

        mpq_mul(t->matrix.ty, t->matrix.d, t->params.params.reflection.line_c);
        mpq_mul(temp, t->matrix.c, t->params.params.reflection.line_c);
        mpq_add(t->matrix.ty, t->matrix.ty, temp);
        mpq_neg(t->matrix.ty, t->matrix.ty);
        mpq_mul_by_ui(t->matrix.ty, t->matrix.ty, 2);
        mpq_div(t->matrix.ty, t->matrix.ty, denom);

        mpq_clear(two_c);
    }

    mpq_clear(dx);
    mpq_clear(dy);
    mpq_clear(temp);
    mpq_clear(a2);
    mpq_clear(b2);
    mpq_clear(denom);

    t->is_isometry = true;
    t->is_orientation_preserving = false;

    return t;
}

Lv00Transform *lv00_transform_reflection_line(const mpq_t a, const mpq_t b, const mpq_t c) {
    /* 从直线方程创建反射变换 */
    mpq_t ax, ay, bx, by;
    mpq_init(ax);
    mpq_init(ay);
    mpq_init(bx);
    mpq_init(by);

    /* 找直线上两点 */
    if (mpq_cmp_ui(b, 0, 1) != 0) {
        /* 点1: (0, -c/b) */
        mpq_set_ui(ax, 0, 1);
        mpq_neg(ay, c);
        mpq_div(ay, ay, b);

        /* 点2: (1, -(a+c)/b) */
        mpq_set_ui(bx, 1, 1);
        mpq_add(by, a, c);
        mpq_neg(by, by);
        mpq_div(by, by, b);
    } else {
        /* 垂直线 ax + c = 0 */
        mpq_neg(ax, c);
        mpq_div(ax, ax, a);
        mpq_set_ui(ay, 0, 1);

        mpq_set(bx, ax);
        mpq_set_ui(by, 1, 1);
    }

    Lv00Transform *t = lv00_transform_reflection(ax, ay, bx, by);

    mpq_clear(ax);
    mpq_clear(ay);
    mpq_clear(bx);
    mpq_clear(by);

    return t;
}

Lv00Transform *lv00_transform_scaling(const mpq_t cx, const mpq_t cy, const mpq_t scale) {
    Lv00Transform *t = lv00_transform_identity();
    if (!t) {
        return NULL;
    }

    t->type = TRANSFORM_SCALING;

    mpq_init(t->params.params.scaling.cx);
    mpq_init(t->params.params.scaling.cy);
    mpq_init(t->params.params.scaling.scale);

    mpq_set(t->params.params.scaling.cx, cx);
    mpq_set(t->params.params.scaling.cy, cy);
    mpq_set(t->params.params.scaling.scale, scale);

    /* 设置矩阵 */
    mpq_set(t->matrix.a, scale);
    mpq_set(t->matrix.d, scale);

    /* 计算平移分量 */
    mpq_t temp;
    mpq_init(temp);

    mpq_sub_by_ui(temp, scale, 1);  /* 使用我们定义的辅助函数 */
    mpq_mul(t->matrix.tx, temp, cx);
    mpq_neg(t->matrix.tx, t->matrix.tx);

    mpq_mul(t->matrix.ty, temp, cy);
    mpq_neg(t->matrix.ty, t->matrix.ty);

    mpq_clear(temp);

    t->is_isometry = (mpq_cmp_ui(scale, 1, 1) == 0);
    t->is_orientation_preserving = (mpq_cmp_ui(scale, 0, 1) > 0);

    return t;
}

void lv00_transform_destroy(Lv00Transform *t) {
    if (!t) {
        return;
    }

    if (t->ref_count > 1) {
        t->ref_count--;
        return;
    }

    /* 清理参数 */
    switch (t->type) {
        case TRANSFORM_TRANSLATION:
            mpq_clear(t->params.params.translation.dx);
            mpq_clear(t->params.params.translation.dy);
            break;
        case TRANSFORM_ROTATION:
            mpq_clear(t->params.params.rotation.cx);
            mpq_clear(t->params.params.rotation.cy);
            mpq_clear(t->params.params.rotation.cos_theta);
            mpq_clear(t->params.params.rotation.sin_theta);
            break;
        case TRANSFORM_REFLECTION:
            mpq_clear(t->params.params.reflection.ax);
            mpq_clear(t->params.params.reflection.ay);
            mpq_clear(t->params.params.reflection.bx);
            mpq_clear(t->params.params.reflection.by);
            mpq_clear(t->params.params.reflection.line_a);
            mpq_clear(t->params.params.reflection.line_b);
            mpq_clear(t->params.params.reflection.line_c);
            break;
        case TRANSFORM_SCALING:
            mpq_clear(t->params.params.scaling.cx);
            mpq_clear(t->params.params.scaling.cy);
            mpq_clear(t->params.params.scaling.scale);
            break;
        default:
            break;
    }

    /* 清理矩阵 */
    mpq_clear(t->matrix.a);
    mpq_clear(t->matrix.b);
    mpq_clear(t->matrix.tx);
    mpq_clear(t->matrix.c);
    mpq_clear(t->matrix.d);
    mpq_clear(t->matrix.ty);

    lv00_free((void **) &t);
}

void lv00_transform_ref(Lv00Transform *t) {
    if (t) {
        t->ref_count++;
    }
}

void lv00_transform_unref(Lv00Transform *t) {
    if (t) {
        t->ref_count--;
        if (t->ref_count <= 0) {
            lv00_transform_destroy(t);
        }
    }
}

/* ============== 变换应用实现 ============== */

bool lv00_transform_apply_point(const Lv00Transform *t, mpq_t x, mpq_t y) {
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

bool lv00_transform_get_matrix(Lv00Transform *t, Lv00TransformMatrix *matrix) {
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

Lv00TransformSequence *lv00_transform_sequence_create(void) {
    Lv00TransformSequence *seq = (Lv00TransformSequence *)lv00_malloc(sizeof(Lv00TransformSequence));
    if (!seq) {
        return NULL;
    }
    memset(seq, 0, sizeof(Lv00TransformSequence));

    seq->transforms = (Lv00Transform **)lv00_malloc(TRANSFORM_SEQ_INIT_CAPACITY * sizeof(Lv00Transform *));
    if (!seq->transforms) {
        lv00_free((void **) &seq);
        return NULL;
    }
    seq->capacity = TRANSFORM_SEQ_INIT_CAPACITY;
    seq->count = 0;

    return seq;
}

void lv00_transform_sequence_destroy(Lv00TransformSequence *seq) {
    if (!seq) {
        return;
    }

    for (uint32_t i = 0; i < seq->count; i++) {
        lv00_transform_unref(seq->transforms[i]);
    }
    lv00_free((void **) &seq->transforms);
    lv00_free((void **) &seq);
}

bool lv00_transform_sequence_add(Lv00TransformSequence *seq, Lv00Transform *t) {
    if (!seq || !t) {
        return false;
    }

    if (seq->count >= seq->capacity) {
        uint32_t new_cap = seq->capacity * 2;
        Lv00Transform **new_arr = (Lv00Transform **)lv00_realloc(seq->transforms,
                                                             new_cap * sizeof(Lv00Transform *));
        if (!new_arr) {
            return false;
        }
        seq->transforms = new_arr;
        seq->capacity = new_cap;
    }

    lv00_transform_ref(t);
    seq->transforms[seq->count++] = t;
    seq->composite_valid = false;

    return true;
}

Lv00Transform *lv00_transform_compose(const Lv00Transform *t1, const Lv00Transform *t2) {
    if (!t1 || !t2) {
        return NULL;
    }

    Lv00Transform *result = (Lv00Transform *)lv00_malloc(sizeof(Lv00Transform));
    if (!result) {
        return NULL;
    }
    memset(result, 0, sizeof(Lv00Transform));

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

Lv00Transform *lv00_transform_sequence_composite(const Lv00TransformSequence *seq) {
    if (!seq || seq->count == 0) {
        return NULL;
    }

    Lv00Transform *result = lv00_transform_identity();
    if (!result) {
        return NULL;
    }

    for (uint32_t i = 0; i < seq->count; i++) {
        Lv00Transform *temp = lv00_transform_compose(result, seq->transforms[i]);
        lv00_transform_destroy(result);
        if (!temp) {
            return NULL;
        }
        result = temp;
    }

    return result;
}

bool lv00_transform_sequence_apply(const Lv00TransformSequence *seq, mpq_t x, mpq_t y) {
    if (!seq) {
        return false;
    }

    for (uint32_t i = 0; i < seq->count; i++) {
        if (!lv00_transform_apply_point(seq->transforms[i], x, y)) {
            return false;
        }
    }

    return true;
}

/* ============== 变换性质分析实现 ============== */

bool lv00_transform_is_isometry(const Lv00Transform *t) {
    return t ? t->is_isometry : false;
}

bool lv00_transform_is_orientation_preserving(const Lv00Transform *t) {
    return t ? t->is_orientation_preserving : false;
}

bool lv00_transform_find_fixed_point(const Lv00Transform *t, mpq_t out_x, mpq_t out_y) {
    if (!t || !t->matrix_valid) {
        return false;
    }

    /* 求解 (x, y) = M * (x, y) */
    /* x = a*x + b*y + tx => (1-a)*x - b*y = tx */
    /* y = c*x + d*y + ty => -c*x + (1-d)*y = ty */

    mpq_t det;
    mpq_init(det);

    /* det = (1-a)*(1-d) - b*c */
    mpq_t one_minus_a, one_minus_d;
    mpq_init(one_minus_a);
    mpq_init(one_minus_d);

    mpq_set_ui(one_minus_a, 1, 1);
    mpq_sub(one_minus_a, one_minus_a, t->matrix.a);

    mpq_set_ui(one_minus_d, 1, 1);
    mpq_sub(one_minus_d, one_minus_d, t->matrix.d);

    mpq_mul(det, one_minus_a, one_minus_d);
    mpq_t temp;
    mpq_init(temp);
    mpq_mul(temp, t->matrix.b, t->matrix.c);
    mpq_sub(det, det, temp);

    if (mpq_cmp_ui(det, 0, 1) == 0) {
        /* 无唯一不动点 */
        mpq_clear(det);
        mpq_clear(one_minus_a);
        mpq_clear(one_minus_d);
        mpq_clear(temp);
        return false;
    }

    /* x = (tx*(1-d) + b*ty) / det */
    mpq_mul(out_x, t->matrix.tx, one_minus_d);
    mpq_mul(temp, t->matrix.b, t->matrix.ty);
    mpq_add(out_x, out_x, temp);
    mpq_div(out_x, out_x, det);

    /* y = ((1-a)*ty + c*tx) / det */
    mpq_mul(out_y, one_minus_a, t->matrix.ty);
    mpq_mul(temp, t->matrix.c, t->matrix.tx);
    mpq_add(out_y, out_y, temp);
    mpq_div(out_y, out_y, det);

    mpq_clear(det);
    mpq_clear(one_minus_a);
    mpq_clear(one_minus_d);
    mpq_clear(temp);

    return true;
}

Lv00Transform *lv00_transform_inverse(const Lv00Transform *t) {
    if (!t || !t->matrix_valid) {
        return NULL;
    }

    Lv00Transform *inv = (Lv00Transform *)lv00_malloc(sizeof(Lv00Transform));
    if (!inv) {
        return NULL;
    }
    memset(inv, 0, sizeof(Lv00Transform));

    inv->type = t->type;
    inv->ref_count = 1;

    mpq_init(inv->matrix.a);
    mpq_init(inv->matrix.b);
    mpq_init(inv->matrix.tx);
    mpq_init(inv->matrix.c);
    mpq_init(inv->matrix.d);
    mpq_init(inv->matrix.ty);

    /* 计算行列式 */
    mpq_t det;
    mpq_init(det);
    mpq_mul(det, t->matrix.a, t->matrix.d);
    mpq_t temp;
    mpq_init(temp);
    mpq_mul(temp, t->matrix.b, t->matrix.c);
    mpq_sub(det, det, temp);

    if (mpq_cmp_ui(det, 0, 1) == 0) {
        /* 不可逆 */
        mpq_clear(det);
        mpq_clear(temp);
        lv00_transform_destroy(inv);
        return NULL;
    }

    /* 逆矩阵 */
    mpq_div(inv->matrix.a, t->matrix.d, det);
    mpq_neg(inv->matrix.b, t->matrix.b);
    mpq_div(inv->matrix.b, inv->matrix.b, det);
    mpq_neg(inv->matrix.c, t->matrix.c);
    mpq_div(inv->matrix.c, inv->matrix.c, det);
    mpq_div(inv->matrix.d, t->matrix.a, det);

    /* tx' = -(a'*tx + b'*ty) */
    mpq_mul(inv->matrix.tx, inv->matrix.a, t->matrix.tx);
    mpq_mul(temp, inv->matrix.b, t->matrix.ty);
    mpq_add(inv->matrix.tx, inv->matrix.tx, temp);
    mpq_neg(inv->matrix.tx, inv->matrix.tx);

    /* ty' = -(c'*tx + d'*ty) */
    mpq_mul(inv->matrix.ty, inv->matrix.c, t->matrix.tx);
    mpq_mul(temp, inv->matrix.d, t->matrix.ty);
    mpq_add(inv->matrix.ty, inv->matrix.ty, temp);
    mpq_neg(inv->matrix.ty, inv->matrix.ty);

    mpq_clear(det);
    mpq_clear(temp);

    inv->matrix_valid = true;
    inv->is_isometry = t->is_isometry;
    inv->is_orientation_preserving = t->is_orientation_preserving;

    return inv;
}

bool lv00_transform_equal(const Lv00Transform *t1, const Lv00Transform *t2) {
    if (!t1 || !t2) {
        return false;
    }

    if (t1->type != t2->type) {
        return false;
    }

    if (!t1->matrix_valid || !t2->matrix_valid) {
        return false;
    }

    return (mpq_equal(t1->matrix.a, t2->matrix.a) &&
            mpq_equal(t1->matrix.b, t2->matrix.b) &&
            mpq_equal(t1->matrix.c, t2->matrix.c) &&
            mpq_equal(t1->matrix.d, t2->matrix.d) &&
            mpq_equal(t1->matrix.tx, t2->matrix.tx) &&
            mpq_equal(t1->matrix.ty, t2->matrix.ty));
}

/* ============== 特殊变换识别实现 ============== */

bool lv00_points_are_symmetric(const mpq_t px, const mpq_t py,
                                const mpq_t qx, const mpq_t qy,
                                const mpq_t ax, const mpq_t ay,
                                const mpq_t bx, const mpq_t by) {
    mpq_t rx, ry;
    mpq_init(rx);
    mpq_init(ry);

    lv00_reflect_point(px, py, ax, ay, bx, by, rx, ry);

    bool result = (mpq_equal(rx, qx) && mpq_equal(ry, qy));

    mpq_clear(rx);
    mpq_clear(ry);

    return result;
}

bool lv00_reflect_point(const mpq_t px, const mpq_t py,
                         const mpq_t ax, const mpq_t ay,
                         const mpq_t bx, const mpq_t by,
                         mpq_t out_x, mpq_t out_y) {
    /* 计算点关于直线的对称点 */
    /* 使用向量投影公式 */

    mpq_t dx, dy;
    mpq_init(dx);
    mpq_init(dy);

    mpq_sub(dx, bx, ax);
    mpq_sub(dy, by, ay);

    /* 向量 v = P - A */
    mpq_t vx, vy;
    mpq_init(vx);
    mpq_init(vy);

    mpq_sub(vx, px, ax);
    mpq_sub(vy, py, ay);

    /* 投影系数 t = (v · d) / (d · d) */
    mpq_t dot_vd, dot_dd, t;
    mpq_init(dot_vd);
    mpq_init(dot_dd);
    mpq_init(t);

    mpq_mul(dot_vd, vx, dx);
    mpq_mul(t, vy, dy);
    mpq_add(dot_vd, dot_vd, t);

    mpq_mul(dot_dd, dx, dx);
    mpq_mul(t, dy, dy);
    mpq_add(dot_dd, dot_dd, t);

    mpq_div(t, dot_vd, dot_dd);

    /* 投影点 H = A + t * d */
    mpq_t hx, hy;
    mpq_init(hx);
    mpq_init(hy);

    mpq_mul(hx, t, dx);
    mpq_add(hx, hx, ax);
    mpq_mul(hy, t, dy);
    mpq_add(hy, hy, ay);

    /* 对称点 R = 2*H - P */
    mpq_t two;
    mpq_init(two);
    mpq_set_ui(two, 2, 1);  /* two = 2/1 = 2 */
    
    mpq_mul(out_x, hx, two);
    mpq_sub(out_x, out_x, px);

    mpq_mul(out_y, hy, two);
    mpq_sub(out_y, out_y, py);
    
    mpq_clear(two);

    mpq_clear(dx);
    mpq_clear(dy);
    mpq_clear(vx);
    mpq_clear(vy);
    mpq_clear(dot_vd);
    mpq_clear(dot_dd);
    mpq_clear(t);
    mpq_clear(hx);
    mpq_clear(hy);

    return true;
}

bool lv00_rotate_point(const mpq_t px, const mpq_t py,
                        const mpq_t cx, const mpq_t cy,
                        int angle_num, int angle_denom,
                        mpq_t out_x, mpq_t out_y) {
    Lv00Transform *rot = lv00_transform_rotation(cx, cy, angle_num, angle_denom);
    if (!rot) {
        return false;
    }

    mpq_set(out_x, px);
    mpq_set(out_y, py);

    bool result = lv00_transform_apply_point(rot, out_x, out_y);

    lv00_transform_destroy(rot);

    return result;
}

/* ============== 变换序列化实现 ============== */

char *lv00_transform_to_string(const Lv00Transform *t) {
    if (!t) {
        return NULL;
    }

    const char *type_str;
    switch (t->type) {
        case TRANSFORM_IDENTITY: type_str = "Identity"; break;
        case TRANSFORM_TRANSLATION: type_str = "Translation"; break;
        case TRANSFORM_ROTATION: type_str = "Rotation"; break;
        case TRANSFORM_REFLECTION: type_str = "Reflection"; break;
        case TRANSFORM_SCALING: type_str = "Scaling"; break;
        case TRANSFORM_GLUING: type_str = "Composite"; break;
        default: type_str = "Unknown"; break;
    }

    /* 动态计算字符串长度并分配缓冲区，避免固定缓冲区溢出 */
    int needed = snprintf(NULL, 0, "%s: matrix=[%Qd %Qd %Qd; %Qd %Qd %Qd]",
             type_str,
             t->matrix.a, t->matrix.b, t->matrix.tx,
             t->matrix.c, t->matrix.d, t->matrix.ty);
    if (needed < 0) return NULL;
    size_t size = (size_t)needed + 1;

    char *result = (char *)lv00_malloc(size);
    if (!result) {
        return NULL;
    }

    snprintf(result, size, "%s: matrix=[%Qd %Qd %Qd; %Qd %Qd %Qd]",
             type_str,
             t->matrix.a, t->matrix.b, t->matrix.tx,
             t->matrix.c, t->matrix.d, t->matrix.ty);

    return result;
}

char *lv00_transform_to_json(const Lv00Transform *t) {
    if (!t) {
        return NULL;
    }

    const char *type_str;
    switch (t->type) {
        case TRANSFORM_IDENTITY: type_str = "identity"; break;
        case TRANSFORM_TRANSLATION: type_str = "translation"; break;
        case TRANSFORM_ROTATION: type_str = "rotation"; break;
        case TRANSFORM_REFLECTION: type_str = "reflection"; break;
        case TRANSFORM_SCALING: type_str = "scaling"; break;
        case TRANSFORM_GLUING: type_str = "composite"; break;
        default: type_str = "unknown"; break;
    }

    /* 动态计算字符串长度并分配缓冲区，避免固定缓冲区溢出 */
    int needed = snprintf(NULL, 0,
             "{\"type\":\"%s\",\"matrix\":{\"a\":\"%Qd\",\"b\":\"%Qd\",\"tx\":\"%Qd\",\"c\":\"%Qd\",\"d\":\"%Qd\",\"ty\":\"%Qd\"},"
             "\"is_isometry\":%s,\"is_orientation_preserving\":%s}",
             type_str,
             t->matrix.a, t->matrix.b, t->matrix.tx,
             t->matrix.c, t->matrix.d, t->matrix.ty,
             t->is_isometry ? "true" : "false",
             t->is_orientation_preserving ? "true" : "false");
    if (needed < 0) return NULL;
    size_t size = (size_t)needed + 1;

    char *result = (char *)lv00_malloc(size);
    if (!result) {
        return NULL;
    }

    snprintf(result, size,
             "{\"type\":\"%s\",\"matrix\":{\"a\":\"%Qd\",\"b\":\"%Qd\",\"tx\":\"%Qd\",\"c\":\"%Qd\",\"d\":\"%Qd\",\"ty\":\"%Qd\"},"
             "\"is_isometry\":%s,\"is_orientation_preserving\":%s}",
             type_str,
             t->matrix.a, t->matrix.b, t->matrix.tx,
             t->matrix.c, t->matrix.d, t->matrix.ty,
             t->is_isometry ? "true" : "false",
             t->is_orientation_preserving ? "true" : "false");

    return result;
}

/* ============== 变换群实现 ============== */

Lv00TransformGroup *lv00_transform_group_create(const char *name) {
    Lv00TransformGroup *group = (Lv00TransformGroup *)lv00_malloc(sizeof(Lv00TransformGroup));
    if (!group) {
        return NULL;
    }
    memset(group, 0, sizeof(Lv00TransformGroup));

    if (name) {
        /* 使用 lv00_malloc + strcpy 替代标准 strdup，确保与 lv00_free 配对 */
        size_t name_len = strlen(name);
        group->group_name = (char *)lv00_malloc(name_len + 1);
        if (group->group_name) {
            memcpy(group->group_name, name, name_len + 1);
        }
    }

    group->generators = (Lv00Transform **)lv00_malloc(GROUP_MAX_GENERATORS * sizeof(Lv00Transform *));
    if (!group->generators) {
        lv00_free((void **) &group);
        return NULL;
    }

    return group;
}

void lv00_transform_group_destroy(Lv00TransformGroup *group) {
    if (!group) {
        return;
    }

    for (uint32_t i = 0; i < group->generator_count; i++) {
        lv00_transform_unref(group->generators[i]);
    }
    lv00_free((void **) &group->generators);
    lv00_free((void **) &group->group_name);
    lv00_free((void **) &group);
}

bool lv00_transform_group_add_generator(Lv00TransformGroup *group, Lv00Transform *generator) {
    if (!group || !generator || group->generator_count >= GROUP_MAX_GENERATORS) {
        return false;
    }

    lv00_transform_ref(generator);
    group->generators[group->generator_count++] = generator;

    return true;
}

Lv00TransformGroup *lv00_transform_group_create_preset(const char *type) {
    if (!type) {
        return NULL;
    }

    Lv00TransformGroup *group = lv00_transform_group_create(type);
    if (!group) {
        return NULL;
    }

    mpq_t zero, one, neg_one;
    mpq_init(zero);
    mpq_init(one);
    mpq_init(neg_one);
    mpq_set_ui(zero, 0, 1);
    mpq_set_ui(one, 1, 1);
    mpq_set_si(neg_one, -1, 1);

    if (strcmp(type, "C2") == 0) {
        /* C2: 180度旋转 */
        Lv00Transform *rot = lv00_transform_rotation(zero, zero, 180, 1);
        lv00_transform_group_add_generator(group, rot);
        lv00_transform_unref(rot);
        group->order = 2;
        group->is_abelian = true;
    } else if (strcmp(type, "C4") == 0) {
        /* C4: 90度旋转 */
        Lv00Transform *rot = lv00_transform_rotation(zero, zero, 90, 1);
        lv00_transform_group_add_generator(group, rot);
        lv00_transform_unref(rot);
        group->order = 4;
        group->is_abelian = true;
    } else if (strcmp(type, "D2") == 0 || strcmp(type, "Klein") == 0) {
        /* Klein 四元群：两个正交反射 */
        mpq_t ax, ay, bx, by;
        mpq_init(ax); mpq_init(ay); mpq_init(bx); mpq_init(by);
        mpq_set_ui(ax, 0, 1); mpq_set_ui(ay, 0, 1);
        mpq_set_ui(bx, 1, 1); mpq_set_ui(by, 0, 1);
        Lv00Transform *r1 = lv00_transform_reflection(ax, ay, bx, by);

        mpq_set_ui(bx, 0, 1); mpq_set_ui(by, 1, 1);
        Lv00Transform *r2 = lv00_transform_reflection(ax, ay, bx, by);

        lv00_transform_group_add_generator(group, r1);
        lv00_transform_group_add_generator(group, r2);
        lv00_transform_unref(r1);
        lv00_transform_unref(r2);
        mpq_clear(ax); mpq_clear(ay); mpq_clear(bx); mpq_clear(by);

        group->order = 4;
        group->is_abelian = true;
    }

    mpq_clear(zero);
    mpq_clear(one);
    mpq_clear(neg_one);

    return group;
}
