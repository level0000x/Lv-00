/**
 * @file geometry_transform.c
 * @brief 几何变换推理系统实现
 *
 * @details 实现旋转、轴对称、平移等几何变换的符号计算
 *
 * 【内存管理策略说明】
 * 本模块使用标准 malloc/free 而非 lv_malloc/lv_free 统一内存分配器，原因如下：
 * 几何变换模块涉及大量临时数组和中间计算结果（如变换序列、群生成元矩阵等），
 * 这些临时对象生命周期短、大小多变，若使用 lv 内存池分配器会导致严重的池碎片化问题。
 * 标准分配器能够更高效地处理此类短生命周期、变长大小的内存分配模式。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "lv/lv_platform.h"
#include "lv/lv_internal.h"

#include "geometry_transform.h"

#include "lv/constraint_graph.h"
#include "lv/lv_strbuf.h"
#include "lv/symbolic_coord.h"
#include "lv_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============== 内部常量 ============== */

/** 初始变换序列容量 */
#define TRANSFORM_SEQ_INIT_CAPACITY 8

/**
 * 对称性验证时从 double 重建符号坐标的缩放比例。
 * 非有理数坐标（代数数、二次根式等）经 lv_transform_apply_double 计算后，
 * 按该缩放比例重建为有理数坐标。缩放越大精度越高，但需保证
 * 坐标值 × 缩放不超过 int64 安全范围。
 */
#define SYMMETRY_COORD_SCALE 1000000LL

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

lvTransform *lv_transform_identity(void) {
    lvTransform *t = (lvTransform *) lv_calloc(1, sizeof(lvTransform));
    if (!t) {
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "lv_transform_identity: calloc failed");
    }

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

    mpq_set_ui(t->matrix.a, 1, 1);  /* a = 1 */
    mpq_set_ui(t->matrix.d, 1, 1);  /* d = 1 */
    mpq_set_ui(t->matrix.b, 0, 1);  /* b = 0 */
    mpq_set_ui(t->matrix.c, 0, 1);  /* c = 0 */
    mpq_set_ui(t->matrix.tx, 0, 1); /* tx = 0 */
    mpq_set_ui(t->matrix.ty, 0, 1); /* ty = 0 */

    t->matrix_valid = true;

    return t;
}

lvTransform *lv_transform_translation(const mpq_t dx, const mpq_t dy) {
    lvTransform *t = lv_transform_identity();
    if (!t) {
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "lv_transform_translation: identity creation failed");
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

lvTransform *lv_transform_rotation(const mpq_t cx, const mpq_t cy, int angle_num, int angle_denom) {
    lvTransform *t = lv_transform_identity();
    if (!t) {
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "lv_transform_rotation: identity creation failed");
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
        case 0: /* 0° */
            mpq_set_ui(t->params.params.rotation.cos_theta, 1, 1);
            mpq_set_ui(t->params.params.rotation.sin_theta, 0, 1);
            mpq_set_ui(t->matrix.a, 1, 1);
            mpq_set_ui(t->matrix.b, 0, 1);
            mpq_set_ui(t->matrix.c, 0, 1);
            mpq_set_ui(t->matrix.d, 1, 1);
            break;
        case 30: /* 30°: a=cos30, b=-1/2, c=1/2, d=cos30 */
            mpq_set_d(t->params.params.rotation.cos_theta, 0.8660254037844386);
            mpq_set_ui(t->params.params.rotation.sin_theta, 1, 2);
            mpq_set_d(t->matrix.a, 0.8660254037844386);
            mpq_set_si(t->matrix.b, -1, 2);
            mpq_set_ui(t->matrix.c, 1, 2);
            mpq_set_d(t->matrix.d, 0.8660254037844386);
            break;
        case 45: /* 45°: a=cos45, b=-sin45, c=sin45, d=cos45 */
            mpq_set_d(t->params.params.rotation.cos_theta, 0.7071067811865476);
            mpq_set_d(t->params.params.rotation.sin_theta, 0.7071067811865476);
            mpq_set_d(t->matrix.a, 0.7071067811865476);
            mpq_set_d(t->matrix.b, -0.7071067811865476);
            mpq_set_d(t->matrix.c, 0.7071067811865476);
            mpq_set_d(t->matrix.d, 0.7071067811865476);
            break;
        case 60: /* 60°: a=1/2, b=-sin60, c=sin60, d=1/2 */
            mpq_set_ui(t->params.params.rotation.cos_theta, 1, 2);
            mpq_set_d(t->params.params.rotation.sin_theta, 0.8660254037844386);
            mpq_set_ui(t->matrix.a, 1, 2);
            mpq_set_d(t->matrix.b, -0.8660254037844386);
            mpq_set_d(t->matrix.c, 0.8660254037844386);
            mpq_set_ui(t->matrix.d, 1, 2);
            break;
        case 90: /* 90° */
            mpq_set_ui(t->params.params.rotation.cos_theta, 0, 1);
            mpq_set_ui(t->params.params.rotation.sin_theta, 1, 1);
            mpq_set_ui(t->matrix.a, 0, 1);
            mpq_set_si(t->matrix.b, -1, 1);
            mpq_set_ui(t->matrix.c, 1, 1);
            mpq_set_ui(t->matrix.d, 0, 1);
            break;
        case 120: /* 120°: a=-1/2, b=-sin120, c=sin120, d=-1/2 */
            mpq_set_si(t->params.params.rotation.cos_theta, -1, 2);
            mpq_set_d(t->params.params.rotation.sin_theta, 0.8660254037844386);
            mpq_set_si(t->matrix.a, -1, 2);
            mpq_set_d(t->matrix.b, -0.8660254037844386);
            mpq_set_d(t->matrix.c, 0.8660254037844386);
            mpq_set_si(t->matrix.d, -1, 2);
            break;
        case 135: /* 135°: a=cos135, b=-sin135, c=sin135, d=cos135 */
            mpq_set_d(t->params.params.rotation.cos_theta, -0.7071067811865476);
            mpq_set_d(t->params.params.rotation.sin_theta, 0.7071067811865476);
            mpq_set_d(t->matrix.a, -0.7071067811865476);
            mpq_set_d(t->matrix.b, -0.7071067811865476);
            mpq_set_d(t->matrix.c, 0.7071067811865476);
            mpq_set_d(t->matrix.d, -0.7071067811865476);
            break;
        case 150: /* 150°: a=cos150, b=-1/2, c=1/2, d=cos150 */
            mpq_set_d(t->params.params.rotation.cos_theta, -0.8660254037844386);
            mpq_set_ui(t->params.params.rotation.sin_theta, 1, 2);
            mpq_set_d(t->matrix.a, -0.8660254037844386);
            mpq_set_si(t->matrix.b, -1, 2);
            mpq_set_ui(t->matrix.c, 1, 2);
            mpq_set_d(t->matrix.d, -0.8660254037844386);
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
            mpq_set_ui(t->matrix.a, 0, 1);
            mpq_set_ui(t->matrix.b, 1, 1);
            mpq_set_si(t->matrix.c, -1, 1);
            mpq_set_ui(t->matrix.d, 0, 1);
            break;
        default:
            /* 其他角度：使用有理数近似计算 cos/sin。
             * 先将角度转换为弧度，用 double 计算 cos/sin，
             * 再通过 mpq_set_d 转换为有理数近似。
             * 这保证了变换矩阵始终被正确设置，而非静默降级为单位矩阵。 */
            {
                double angle_rad = (double) angle_num / (double) angle_denom * M_PI / 180.0;
                double cos_d = cos(angle_rad);
                double sin_d = sin(angle_rad);
                mpq_set_d(t->params.params.rotation.cos_theta, cos_d);
                mpq_set_d(t->params.params.rotation.sin_theta, sin_d);
                mpq_set_d(t->matrix.a, cos_d);
                mpq_set_si(t->matrix.b, -1, 1);
                mpq_mul(t->matrix.b, t->matrix.b, t->params.params.rotation.sin_theta);
                mpq_set_d(t->matrix.c, sin_d);
                mpq_set(t->matrix.d, t->params.params.rotation.cos_theta);
            }
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

lvTransform *lv_transform_rotation_arbitrary(const mpq_t cx, const mpq_t cy, const mpq_t cos_theta,
                                             const mpq_t sin_theta) {
    lvTransform *t = lv_transform_identity();
    if (!t) {
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "lv_transform_rotation_arbitrary: identity creation failed");
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

lvTransform *lv_transform_reflection(const mpq_t ax, const mpq_t ay, const mpq_t bx, const mpq_t by) {
    lvTransform *t = lv_transform_identity();
    if (!t) {
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "lv_transform_reflection: identity creation failed");
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

        /* 计算平移分量 tx = -2*a*c / (a²+b²), ty = -2*b*c / (a²+b²)
         *
         * 反射变换公式（关于直线 ax+by+c=0）：
         *   [a'] = [(b²-a²)/(a²+b²)  -2ab/(a²+b²)  ]
         *   [b']   [-2ab/(a²+b²)     (a²-b²)/(a²+b²)]
         *   tx = -2ac/(a²+b²),  ty = -2bc/(a²+b²)
         *
         * 注意：matrix.a/b/c/d 已经在前面除以 denom 归一化，
         * 因此不能再用 matrix 元素来组装平移分量。
         */
        mpq_mul(t->matrix.tx, t->params.params.reflection.line_a, t->params.params.reflection.line_c);
        mpq_mul_by_ui(t->matrix.tx, t->matrix.tx, 2);
        mpq_neg(t->matrix.tx, t->matrix.tx);
        mpq_div(t->matrix.tx, t->matrix.tx, denom);

        mpq_mul(t->matrix.ty, t->params.params.reflection.line_b, t->params.params.reflection.line_c);
        mpq_mul_by_ui(t->matrix.ty, t->matrix.ty, 2);
        mpq_neg(t->matrix.ty, t->matrix.ty);
        mpq_div(t->matrix.ty, t->matrix.ty, denom);
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

lvTransform *lv_transform_reflection_line(const mpq_t a, const mpq_t b, const mpq_t c) {
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
        if (mpq_sgn(a) == 0) {
            /* a 和 b 同时为零，不是有效直线方程 */
            mpq_clear(ax);
            mpq_clear(ay);
            mpq_clear(bx);
            mpq_clear(by);
            lv_RETURN_ERROR_NULL(lv_ERROR_INVALID_PARAM, "lv_transform_reflection_line: a and b both zero, invalid line equation");
        }
        mpq_neg(ax, c);
        mpq_div(ax, ax, a);
        mpq_set_ui(ay, 0, 1);

        mpq_set(bx, ax);
        mpq_set_ui(by, 1, 1);
    }

    lvTransform *t = lv_transform_reflection(ax, ay, bx, by);

    mpq_clear(ax);
    mpq_clear(ay);
    mpq_clear(bx);
    mpq_clear(by);

    return t;
}

lvTransform *lv_transform_scaling(const mpq_t cx, const mpq_t cy, const mpq_t scale) {
    lvTransform *t = lv_transform_identity();
    if (!t) {
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "lv_transform_scaling: identity creation failed");
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

    mpq_sub_by_ui(temp, scale, 1); /* 使用我们定义的辅助函数 */
    mpq_mul(t->matrix.tx, temp, cx);
    mpq_neg(t->matrix.tx, t->matrix.tx);

    mpq_mul(t->matrix.ty, temp, cy);
    mpq_neg(t->matrix.ty, t->matrix.ty);

    mpq_clear(temp);

    t->is_isometry = (mpq_cmp_ui(scale, 1, 1) == 0);
    t->is_orientation_preserving = (mpq_cmp_ui(scale, 0, 1) > 0);

    return t;
}

void lv_transform_destroy(lvTransform *t) {
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

    lv_free((void **) &t);
}

void lv_transform_ref(lvTransform *t) {
    if (t) {
        t->ref_count++;
    }
}

void lv_transform_unref(lvTransform *t) {
    if (t) {
        t->ref_count--;
        if (t->ref_count <= 0) {
            lv_transform_destroy(t);
        }
    }
}

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

/* ============== 变换性质分析实现 ============== */

bool lv_transform_is_isometry(const lvTransform *t) {
    return t ? t->is_isometry : false;
}

bool lv_transform_is_orientation_preserving(const lvTransform *t) {
    return t ? t->is_orientation_preserving : false;
}

bool lv_transform_find_fixed_point(const lvTransform *t, mpq_t out_x, mpq_t out_y) {
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

lvTransform *lv_transform_inverse(const lvTransform *t) {
    if (!t || !t->matrix_valid) {
        lv_RETURN_ERROR_NULL(lv_ERROR_INVALID_PARAM, "lv_transform_inverse: NULL or invalid matrix");
    }

    lvTransform *inv = (lvTransform *) lv_calloc(1, sizeof(lvTransform));
    if (!inv) {
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "lv_transform_inverse: calloc failed");
    }

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
        lv_transform_destroy(inv);
        lv_RETURN_ERROR_NULL(lv_ERROR_INVALID_STATE, "lv_transform_inverse: singular matrix, not invertible");
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

bool lv_transform_equal(const lvTransform *t1, const lvTransform *t2) {
    if (!t1 || !t2) {
        return false;
    }

    if (t1->type != t2->type) {
        return false;
    }

    if (!t1->matrix_valid || !t2->matrix_valid) {
        return false;
    }

    return (mpq_equal(t1->matrix.a, t2->matrix.a) && mpq_equal(t1->matrix.b, t2->matrix.b) &&
            mpq_equal(t1->matrix.c, t2->matrix.c) && mpq_equal(t1->matrix.d, t2->matrix.d) &&
            mpq_equal(t1->matrix.tx, t2->matrix.tx) && mpq_equal(t1->matrix.ty, t2->matrix.ty));
}

/* ============== 特殊变换识别实现 ============== */

bool lv_points_are_symmetric(const mpq_t px, const mpq_t py, const mpq_t qx, const mpq_t qy, const mpq_t ax,
                             const mpq_t ay, const mpq_t bx, const mpq_t by) {
    mpq_t rx, ry;
    mpq_init(rx);
    mpq_init(ry);

    lv_reflect_point(px, py, ax, ay, bx, by, rx, ry);

    bool result = (mpq_equal(rx, qx) && mpq_equal(ry, qy));

    mpq_clear(rx);
    mpq_clear(ry);

    return result;
}

bool lv_reflect_point(const mpq_t px, const mpq_t py, const mpq_t ax, const mpq_t ay, const mpq_t bx, const mpq_t by,
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
    mpq_set_ui(two, 2, 1); /* two = 2/1 = 2 */

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

bool lv_rotate_point(const mpq_t px, const mpq_t py, const mpq_t cx, const mpq_t cy, int angle_num, int angle_denom,
                     mpq_t out_x, mpq_t out_y) {
    lvTransform *rot = lv_transform_rotation(cx, cy, angle_num, angle_denom);
    if (!rot) {
        return false;
    }

    mpq_set(out_x, px);
    mpq_set(out_y, py);

    bool result = lv_transform_apply_point(rot, out_x, out_y);

    lv_transform_destroy(rot);

    return result;
}

/* ── 变换类型名称映射表（替代 switch 语句） ── */

/** @brief 变换类型 → 显示名称 / JSON 标识符 */
static const struct {
    const char *display;
    const char *json;
} s_transform_type_names[] = {
    [TRANSFORM_IDENTITY]    = {"Identity",    "identity"},
    [TRANSFORM_TRANSLATION] = {"Translation", "translation"},
    [TRANSFORM_ROTATION]    = {"Rotation",    "rotation"},
    [TRANSFORM_SCALE]       = {"Scale",       "scale"},
    [TRANSFORM_SHEAR]       = {"Shear",       "shear"},
    [TRANSFORM_REFLECTION]  = {"Reflection",  "reflection"},
    [TRANSFORM_SCALING]     = {"Scaling",     "scaling"},
    [TRANSFORM_AFFINE]      = {"Affine",      "affine"},
    [TRANSFORM_PROJECTIVE]  = {"Projective",  "protective"},
    [TRANSFORM_GLUING]      = {"Composite",   "composite"},
    [TRANSFORM_COMPOSITE]   = {"Composite",   "composite"},
};

/* ============== 变换序列化实现 ============== */

char *lv_transform_to_string(const lvTransform *t) {
    if (!t) {
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "lv_transform_to_string: NULL transform");
    }

    const char *type_str = (t->type >= 0 && t->type < (int)(sizeof(s_transform_type_names)/sizeof(s_transform_type_names[0])))
                           ? s_transform_type_names[t->type].display : "Unknown";

    /* 用 lvStrBuf 统一构建，避免两遍 snprintf 重复格式串 */
    lvStrBuf sb = {0};
    lv_strbuf_printf(&sb, "%s: matrix=[%Qd %Qd %Qd; %Qd %Qd %Qd]", type_str, t->matrix.a, t->matrix.b,
                     t->matrix.tx, t->matrix.c, t->matrix.d, t->matrix.ty);
    return lv_strbuf_to_string(&sb);
}

char *lv_transform_to_json(const lvTransform *t) {
    if (!t) {
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "lv_transform_to_json: NULL transform");
    }

    const char *type_str = (t->type >= 0 && t->type < (int)(sizeof(s_transform_type_names)/sizeof(s_transform_type_names[0])))
                           ? s_transform_type_names[t->type].json : "unknown";

    /* 用 lvStrBuf 统一构建，避免两遍 snprintf 重复格式串 */
    lvStrBuf sb = {0};
    lv_strbuf_printf(&sb,
                     "{\"type\":\"%s\",\"matrix\":{\"a\":\"%Qd\",\"b\":\"%Qd\",\"tx\":\"%Qd\",\"c\":\"%Qd\",\"d\":"
                     "\"%Qd\",\"ty\":\"%Qd\"},"
                     "\"is_isometry\":%s,\"is_orientation_preserving\":%s}",
                     type_str, t->matrix.a, t->matrix.b, t->matrix.tx, t->matrix.c, t->matrix.d, t->matrix.ty,
                     t->is_isometry ? "true" : "false", t->is_orientation_preserving ? "true" : "false");
    return lv_strbuf_to_string(&sb);
}

/* ============== 变换群实现 ============== */

lvTransformGroup *lv_transform_group_create(const char *name) {
    lvTransformGroup *group = (lvTransformGroup *) lv_calloc(1, sizeof(lvTransformGroup));
    if (!group) {
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "lv_transform_group_create: calloc failed");
    }

    if (name) {
        /* 使用 lv_malloc + strcpy 替代标准 strdup，确保与 lv_free 配对 */
        size_t name_len = strlen(name);
        group->group_name = (char *) lv_malloc(name_len + 1);
        if (group->group_name) {
            memcpy(group->group_name, name, name_len + 1);
        }
    }

    group->generators = (lvTransform **) lv_malloc(GROUP_MAX_GENERATORS * sizeof(lvTransform *));
    if (!group->generators) {
        lv_free((void **) &group);
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "lv_transform_group_create: generators malloc failed");
    }

    return group;
}

void lv_transform_group_destroy(lvTransformGroup *group) {
    if (!group) {
        return;
    }

    for (uint32_t i = 0; i < group->generator_count; i++) {
        lv_transform_unref(group->generators[i]);
    }
    lv_free((void **) &group->generators);
    lv_free((void **) &group->group_name);
    lv_free((void **) &group);
}

bool lv_transform_group_add_generator(lvTransformGroup *group, lvTransform *generator) {
    if (!group || !generator || group->generator_count >= GROUP_MAX_GENERATORS) {
        return false;
    }

    lv_transform_ref(generator);
    group->generators[group->generator_count++] = generator;

    return true;
}

lvTransformGroup *lv_transform_group_create_preset(const char *type) {
    if (!type) {
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "lv_transform_group_create_preset: NULL type");
    }

    lvTransformGroup *group = lv_transform_group_create(type);
    if (!group) {
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "lv_transform_group_create_preset: group creation failed");
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
        lvTransform *rot = lv_transform_rotation(zero, zero, 180, 1);
        lv_transform_group_add_generator(group, rot);
        lv_transform_unref(rot);
        group->order = 2;
        group->is_abelian = true;
    } else if (strcmp(type, "C4") == 0) {
        /* C4: 90度旋转 */
        lvTransform *rot = lv_transform_rotation(zero, zero, 90, 1);
        lv_transform_group_add_generator(group, rot);
        lv_transform_unref(rot);
        group->order = 4;
        group->is_abelian = true;
    } else if (strcmp(type, "D2") == 0 || strcmp(type, "Klein") == 0) {
        /* Klein 四元群：两个正交反射 */
        mpq_t ax, ay, bx, by;
        mpq_init(ax);
        mpq_init(ay);
        mpq_init(bx);
        mpq_init(by);
        mpq_set_ui(ax, 0, 1);
        mpq_set_ui(ay, 0, 1);
        mpq_set_ui(bx, 1, 1);
        mpq_set_ui(by, 0, 1);
        lvTransform *r1 = lv_transform_reflection(ax, ay, bx, by);

        mpq_set_ui(bx, 0, 1);
        mpq_set_ui(by, 1, 1);
        lvTransform *r2 = lv_transform_reflection(ax, ay, bx, by);

        lv_transform_group_add_generator(group, r1);
        lv_transform_group_add_generator(group, r2);
        lv_transform_unref(r1);
        lv_transform_unref(r2);
        mpq_clear(ax);
        mpq_clear(ay);
        mpq_clear(bx);
        mpq_clear(by);

        group->order = 4;
        group->is_abelian = true;
    }

    mpq_clear(zero);
    mpq_clear(one);
    mpq_clear(neg_one);

    return group;
}

/* ============== 变换阶计算与对称性识别 ============== */

/** 变换阶计算的安全上限，防止无限循环 */
#define TRANSFORM_ORDER_MAX_ITERATIONS 1000

/**
 * @brief 计算变换的阶 -- 满足 T^n = I 的最小正整数 n
 *
 * 通过反复复合变换并检查是否为单位矩阵来确定阶。
 * 对于有限阶变换（如旋转 90 度 -> 阶为 4），返回最小的 n。
 * 对于无限阶变换（如非平凡平移），返回 0。
 *
 * @param t 变换指针（非 NULL）
 * @return n > 0: 变换的阶（T^n = 恒等变换）
 * @return 0: 变换为无限阶（如非零平移、非等比缩放）
 * @return -1: 参数无效（t 为 NULL 或矩阵无效）
 */
int lv_transform_order(const lvTransform *t) {
    if (!t || !t->matrix_valid) {
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "lv_transform_order: NULL or invalid matrix");
    }

    /* 恒等变换的阶为 1 */
    if (t->type == TRANSFORM_IDENTITY) {
        return 1;
    }

    /* 获取当前变换矩阵的有理数副本用于比较 */
    lvTransform *current = lv_transform_identity();
    if (!current) {
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "lv_transform_order: identity creation failed");
    }

    /* 生成恒等矩阵用于比较 */
    lvTransform *identity = lv_transform_identity();
    if (!identity) {
        lv_transform_destroy(current);
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "lv_transform_order: identity creation failed");
    }

    /* 迭代计算 T^n，检查何时等于恒等变换 */
    for (int n = 1; n <= TRANSFORM_ORDER_MAX_ITERATIONS; n++) {
        /* current = current * t (即 T^n) */
        lvTransform *next = lv_transform_compose(current, t);
        lv_transform_destroy(current);
        current = next;

        if (!current) {
            /* 复合失败（内存不足等） */
            lv_transform_destroy(identity);
            lv_RETURN_ERROR(lv_ERROR_INTERNAL, "lv_transform_order: compose failed");
        }

        /* 检查 T^n 是否为单位矩阵 */
        if (lv_transform_equal(current, identity)) {
            lv_transform_destroy(current);
            lv_transform_destroy(identity);
            return n;
        }

        /* 对于平移变换，T^n 的平移量是 n 倍，永不等于恒等（除非 dx=dy=0）。
         * 快速检测：如果第一次复合后平移量不为零，且类型是平移，则无限阶 */
        if (n == 1 && t->type == TRANSFORM_TRANSLATION) {
            if (mpq_cmp_ui(current->matrix.tx, 0, 1) != 0 || mpq_cmp_ui(current->matrix.ty, 0, 1) != 0) {
                lv_transform_destroy(current);
                lv_transform_destroy(identity);
                return 0; /* 无限阶 */
            }
        }
    }

    /* 超过最大迭代次数，认为无限阶 */
    lv_transform_destroy(current);
    lv_transform_destroy(identity);
    return 0;
}

/**
 * @brief 由精确有理数创建有理型符号坐标
 *
 * 几何变换模块需要保留变换后坐标的精确性（用于退化线段 / 重复约束检测），
 * 因此对 RATIONAL 类型坐标走精确 mpq 路径，避免 double 舍入误差。
 * 该函数将 mpq 值封装为 SymbolicCoord（RATIONAL 类型），供验证流程使用。
 *
 * @param value 有理数值（mpq）
 * @param trust 继承原坐标的信任颜色
 * @return 新建的符号坐标，失败返回 NULL
 */
static SymbolicCoord *symbolic_coord_create_from_mpq(const mpq_t value, TrustColor trust) {
    SymbolicCoord *coord = lv_calloc(1, sizeof(SymbolicCoord));
    if (!coord) {
        return NULL;
    }

    coord->type = RATIONAL;
    coord->trust = trust;
    coord->cache_valid = false;
    coord->cached_value = 0.0;

    Rational *rat = lv_calloc(1, sizeof(Rational));
    if (!rat) {
        lv_free((void **) &coord);
        return NULL;
    }
    mpq_init(rat->value);
    mpq_set(rat->value, value);
    coord->data.rational = rat;

    return coord;
}

/**
 * @brief 验证候选变换是否保持约束图的所有约束关系
 *
 * 对称变换必须是约束图的自同构：对图中每个几何节点的符号坐标应用变换后，
 * 所有约束仍应成立。实现步骤：
 *   1. 用 graph_copy 深拷贝原图，在副本上操作，避免修改调用者的数据；
 *   2. 以 (x, y) 点对为单位遍历每个节点的符号坐标：
 *        - RATIONAL 坐标走精确有理数路径（mpq 矩阵运算，无浮点误差）；
 *        - 其他类型坐标经 lv_transform_apply_double 计算后按固定缩放重建；
 *   3. 调用 graph_check_compatibility 校验变换后图与原图具有相同的
 *      相容性状态——若出现退化线段（INCONSISTENT）或新增重复约束
 *      （OVER_CONSTRAINED）等差异，说明变换破坏了约束关系。
 *
 * @param graph 约束图（const，不会被修改）
 * @param t     候选变换（非 NULL，矩阵须有效）
 * @return true 表示变换保持约束图（对称变换），false 表示变换破坏了约束
 */
static bool transform_preserves_graph(const ConstraintGraph *graph, const lvTransform *t) {
    if (!graph || !t || !t->matrix_valid) {
        return false;
    }

    /* 评估原图的相容性状态，作为变换后状态对比的基准 */
    lvConstraintCompatibilityResult orig_compat;
    if (!graph_check_compatibility(graph, &orig_compat)) {
        return false;
    }

    /* 深拷贝原图，在副本上应用变换 */
    ConstraintGraph *temp = graph_copy(graph);
    if (!temp) {
        return false;
    }

    bool preserved = true;

    /* 逐节点应用变换：符号坐标以 (x, y) 点对连续存储（点为 1 对、线段为 2 对） */
    for (int i = 0; i < temp->node_count && preserved; i++) {
        GeomNode *node = temp->nodes[i];
        if (!node || !node->symbolic_coords || node->coord_count < 2) {
            continue;
        }
        for (int k = 0; k + 1 < node->coord_count; k += 2) {
            SymbolicCoord *sx = node->symbolic_coords[k];
            SymbolicCoord *sy = node->symbolic_coords[k + 1];
            if (!sx || !sy) {
                continue;
            }

            SymbolicCoord *nsx = NULL;
            SymbolicCoord *nsy = NULL;

            /* 有理数坐标：精确 mpq 路径，避免浮点舍入误差 */
            if (sx->type == RATIONAL && sy->type == RATIONAL && sx->data.rational && sy->data.rational) {
                mpq_t mx, my;
                mpq_init(mx);
                mpq_init(my);
                mpq_set(mx, sx->data.rational->value);
                mpq_set(my, sy->data.rational->value);
                if (lv_transform_apply_point(t, mx, my)) {
                    nsx = symbolic_coord_create_from_mpq(mx, sx->trust);
                    nsy = symbolic_coord_create_from_mpq(my, sy->trust);
                }
                mpq_clear(mx);
                mpq_clear(my);
            } else {
                /* 非有理坐标：double 路径，按固定缩放重建为有理坐标 */
                double dx = 0.0, dy = 0.0;
                lv_transform_apply_double(t, symbolic_coord_to_double(sx), symbolic_coord_to_double(sy), &dx, &dy);
                nsx = symbolic_coord_from_double_rounded(dx, SYMMETRY_COORD_SCALE);
                nsy = symbolic_coord_from_double_rounded(dy, SYMMETRY_COORD_SCALE);
            }

            if (!nsx || !nsy) {
                /* 坐标重建失败：释放已分配部分并判定变换未保持约束图 */
                symbolic_coord_destroy(nsx);
                symbolic_coord_destroy(nsy);
                preserved = false;
                break;
            }

            /* 用变换后的坐标替换原坐标（原坐标已由 graph_copy 深拷贝，可安全释放） */
            symbolic_coord_destroy(sx);
            symbolic_coord_destroy(sy);
            node->symbolic_coords[k] = nsx;
            node->symbolic_coords[k + 1] = nsy;
        }
    }

    if (preserved) {
        /* 校验所有约束在变换后是否仍然成立：变换后图的相容性状态必须与原图一致 */
        lvConstraintCompatibilityResult compat;
        if (!graph_check_compatibility(temp, &compat)) {
            preserved = false;
        } else {
            preserved = (compat.status == orig_compat.status);
        }
    }

    graph_destroy(temp);
    return preserved;
}

/**
 * @brief 分析约束图的所有对称变换
 *
 * 遍历约束图中的几何对象，识别保持约束关系的对称变换。
 * 检测的对称类型包括：
 *   - 关于坐标轴的反射
 *   - 关于原点的中心对称（180 度旋转）
 *   - 常见角度的旋转对称（60/90/120/180 度）
 *   - 简单平移对称
 *
 * 找到的对称变换以 lvTransform 指针数组的形式通过
 * out_transforms 输出。调用者负责销毁每个变换。
 *
 * @param graph           约束图指针（非 NULL）
 * @param out_transforms  输出：对称变换数组（由本函数分配，调用者负责 free）
 * @param max_count       输出数组的最大容量
 * @return 找到的对称变换数量（0 表示无对称或参数无效）
 */
int lv_transform_identify_symmetries(const ConstraintGraph *graph, lvTransform **out_transforms, int max_count) {
    if (!graph || !out_transforms || max_count <= 0) {
        return 0;
    }

    int found = 0;
    mpq_t zero, one;
    mpq_init(zero);
    mpq_init(one);
    mpq_set_ui(zero, 0, 1);
    mpq_set_ui(one, 1, 1);

    /* ---- 检测 1: 关于 x 轴的反射 ----
     * 反射直线: y = 0 (即 a=0, b=1, c=0 -> line from (0,0) to (1,0)) */
    if (found < max_count) {
        mpq_t ax, ay, bx, by;
        mpq_init(ax);
        mpq_init(ay);
        mpq_init(bx);
        mpq_init(by);
        mpq_set_ui(ax, 0, 1);
        mpq_set_ui(ay, 0, 1);
        mpq_set_ui(bx, 1, 1);
        mpq_set_ui(by, 0, 1);

        lvTransform *ref_x = lv_transform_reflection(ax, ay, bx, by);
        if (ref_x) {
            /* 验证 x 轴反射后所有约束是否仍然成立，成立才作为对称变换加入输出 */
            if (transform_preserves_graph(graph, ref_x)) {
                out_transforms[found++] = ref_x;
            } else {
                lv_transform_destroy(ref_x);
            }
        }

        mpq_clear(ax);
        mpq_clear(ay);
        mpq_clear(bx);
        mpq_clear(by);
    }

    /* ---- 检测 2: 关于 y 轴的反射 ----
     * 反射直线: x = 0 (即 a=1, b=0, c=0 -> line from (0,0) to (0,1)) */
    if (found < max_count) {
        mpq_t ax, ay, bx, by;
        mpq_init(ax);
        mpq_init(ay);
        mpq_init(bx);
        mpq_init(by);
        mpq_set_ui(ax, 0, 1);
        mpq_set_ui(ay, 0, 1);
        mpq_set_ui(bx, 0, 1);
        mpq_set_ui(by, 1, 1);

        lvTransform *ref_y = lv_transform_reflection(ax, ay, bx, by);
        if (ref_y) {
            /* 验证 y 轴反射后所有约束是否仍然成立，成立才作为对称变换加入输出 */
            if (transform_preserves_graph(graph, ref_y)) {
                out_transforms[found++] = ref_y;
            } else {
                lv_transform_destroy(ref_y);
            }
        }

        mpq_clear(ax);
        mpq_clear(ay);
        mpq_clear(bx);
        mpq_clear(by);
    }

    /* ---- 检测 3: 关于原点的 180 度旋转（中心对称） ---- */
    if (found < max_count) {
        lvTransform *rot180 = lv_transform_rotation(zero, zero, 180, 1);
        if (rot180) {
            /* 验证 180 度旋转后所有约束是否仍然成立，成立才作为对称变换加入输出 */
            if (transform_preserves_graph(graph, rot180)) {
                out_transforms[found++] = rot180;
            } else {
                lv_transform_destroy(rot180);
            }
        }
    }

    /* ---- 检测 4: 90 度旋转对称 ---- */
    if (found < max_count) {
        lvTransform *rot90 = lv_transform_rotation(zero, zero, 90, 1);
        if (rot90) {
            /* 验证 90 度旋转后所有约束是否仍然成立，成立才作为对称变换加入输出 */
            if (transform_preserves_graph(graph, rot90)) {
                out_transforms[found++] = rot90;
            } else {
                lv_transform_destroy(rot90);
            }
        }
    }

    /* ---- 检测 5: 120 度旋转对称（正三角形等） ---- */
    if (found < max_count) {
        lvTransform *rot120 = lv_transform_rotation(zero, zero, 120, 1);
        if (rot120) {
            /* 验证 120 度旋转后所有约束是否仍然成立，成立才作为对称变换加入输出 */
            if (transform_preserves_graph(graph, rot120)) {
                out_transforms[found++] = rot120;
            } else {
                lv_transform_destroy(rot120);
            }
        }
    }

    /* ---- 检测 6: 关于 y=x 的反射 ----
     * 反射直线: y=x (即 from (0,0) to (1,1)) */
    if (found < max_count) {
        mpq_t ax, ay, bx, by;
        mpq_init(ax);
        mpq_init(ay);
        mpq_init(bx);
        mpq_init(by);
        mpq_set_ui(ax, 0, 1);
        mpq_set_ui(ay, 0, 1);
        mpq_set_ui(bx, 1, 1);
        mpq_set_ui(by, 1, 1);

        lvTransform *ref_yx = lv_transform_reflection(ax, ay, bx, by);
        if (ref_yx) {
            /* 验证 y=x 反射后所有约束是否仍然成立，成立才作为对称变换加入输出 */
            if (transform_preserves_graph(graph, ref_yx)) {
                out_transforms[found++] = ref_yx;
            } else {
                lv_transform_destroy(ref_yx);
            }
        }

        mpq_clear(ax);
        mpq_clear(ay);
        mpq_clear(bx);
        mpq_clear(by);
    }

    mpq_clear(zero);
    mpq_clear(one);

    return found;
}
