/**
 * @file geometry_transform.c
 * @brief 几何变换推理系统实现
 *
 * @details 实现旋转、轴对称、平移等几何变换的符号计算（创建/销毁核心；应用/复合、性质分析、群与对称性见 geometry_transform_apply/analysis/group.c）
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
    /* 覆盖 30° 间隔的全部常见角：0°,30°,45°,60°,90°,120°,135°,150°,180°,210°,225°,240°,270°,300°,315°,330° */

    /* 规范化角度到 [0, 360) */
    while (angle_num < 0) {
        angle_num += (int)lv_FULL_CIRCLE_DEG * angle_denom;
    }
    while (angle_num >= (int)lv_FULL_CIRCLE_DEG * angle_denom) {
        angle_num -= (int)lv_FULL_CIRCLE_DEG * angle_denom;
    }

    /* 根据角度设置 cos 和 sin（30° 间隔常见角全表；未命中走下方数值回退） */
    int normalized = angle_num / angle_denom;

    /* 特化角度查找表 */
    static const struct {
        int angle;
        double cos_val;
        double sin_val;
    } kAngleTable[] = {
        {  0,  1.0,                    0.0                   },
        { 30,  0.8660254037844386,     0.5                   },
        { 45,  0.7071067811865476,     0.7071067811865476    },
        { 60,  0.5,                    0.8660254037844386    },
        { 90,  0.0,                    1.0                   },
        {120, -0.5,                    0.8660254037844386    },
        {135, -0.7071067811865476,     0.7071067811865476    },
        {150, -0.8660254037844386,     0.5                   },
        {180, -1.0,                    0.0                   },
        {210, -0.8660254037844386,    -0.5                   },
        {225, -0.7071067811865476,    -0.7071067811865476    },
        {240, -0.5,                   -0.8660254037844386    },
        {270,  0.0,                   -1.0                   },
        {300,  0.5,                   -0.8660254037844386    },
        {315,  0.7071067811865476,    -0.7071067811865476    },
        {330,  0.8660254037844386,    -0.5                   },
    };
#define ANGLE_TABLE_SIZE (sizeof(kAngleTable)/sizeof(kAngleTable[0]))

    /* 在查找表中查找匹配的角度 */
    int i;
    for (i = 0; i < (int)ANGLE_TABLE_SIZE; i++) {
        if (kAngleTable[i].angle == normalized) {
            mpq_set_d(t->params.params.rotation.cos_theta, kAngleTable[i].cos_val);
            mpq_set_d(t->params.params.rotation.sin_theta, kAngleTable[i].sin_val);
            mpq_set_d(t->matrix.a, kAngleTable[i].cos_val);
            mpq_set_d(t->matrix.b, -kAngleTable[i].sin_val);
            mpq_set_d(t->matrix.c, kAngleTable[i].sin_val);
            mpq_set_d(t->matrix.d, kAngleTable[i].cos_val);
            break;
        }
    }
    if (i == (int)ANGLE_TABLE_SIZE) {
        /* 其他角度：使用有理数近似计算 cos/sin。
         * 先将角度转换为弧度，用 double 计算 cos/sin，
         * 再通过 mpq_set_d 转换为有理数近似。
         * 这保证了变换矩阵始终被正确设置，而非静默降级为单位矩阵。 */
        {
            double angle_rad = lv_deg_to_rad((double) angle_num / (double) angle_denom);
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

    /* 设置矩阵（逆时针，与 lv_transform_rotation 特殊角版一致）：
     * a=cos, b=-sin, c=sin, d=cos —— 修复（C-㊺续32 测试暴露）：
     * 原实现 b=sin, c=-sin 为顺时针矩阵，与同族 rotation 方向相反 */
    mpq_set(t->matrix.a, cos_theta);
    mpq_neg(t->matrix.b, sin_theta);
    mpq_set(t->matrix.c, sin_theta);
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

/* ── 变换参数清理处理器（VTable 重构） ── */

/** @brief 清理处理器函数指针类型 */
typedef void (*TransformCleanupHandler)(const lvTransform *t);

/** @brief 清理平移变换参数 */
static void cleanup_translation(const lvTransform *t) {
    mpq_clear(t->params.params.translation.dx);
    mpq_clear(t->params.params.translation.dy);
}

/** @brief 清理旋转变换参数 */
static void cleanup_rotation(const lvTransform *t) {
    mpq_clear(t->params.params.rotation.cx);
    mpq_clear(t->params.params.rotation.cy);
    mpq_clear(t->params.params.rotation.cos_theta);
    mpq_clear(t->params.params.rotation.sin_theta);
}

/** @brief 清理反射变换参数 */
static void cleanup_reflection(const lvTransform *t) {
    mpq_clear(t->params.params.reflection.ax);
    mpq_clear(t->params.params.reflection.ay);
    mpq_clear(t->params.params.reflection.bx);
    mpq_clear(t->params.params.reflection.by);
    mpq_clear(t->params.params.reflection.line_a);
    mpq_clear(t->params.params.reflection.line_b);
    mpq_clear(t->params.params.reflection.line_c);
}

/** @brief 清理缩放变换参数 */
static void cleanup_scaling(const lvTransform *t) {
    mpq_clear(t->params.params.scaling.cx);
    mpq_clear(t->params.params.scaling.cy);
    mpq_clear(t->params.params.scaling.scale);
}

/** @brief 清理 SCALE（沿坐标轴缩放）变换参数 */
static void cleanup_scale(const lvTransform *t) {
    mpq_clear(t->params.params.scale.sx);
    mpq_clear(t->params.params.scale.sy);
}

/** @brief 默认清理处理器（空操作） */
static void cleanup_default(const lvTransform *t) {
    (void)t;
}

/** @brief 清理处理器查找表（按 lvTransformType 枚举索引） */
static const TransformCleanupHandler kTransformCleanupHandlers[] = {
    [TRANSFORM_IDENTITY]    = cleanup_default,
    [TRANSFORM_TRANSLATION] = cleanup_translation,
    [TRANSFORM_ROTATION]    = cleanup_rotation,
    [TRANSFORM_SCALE]       = cleanup_scale,
    [TRANSFORM_SHEAR]       = cleanup_default,
    [TRANSFORM_REFLECTION]  = cleanup_reflection,
    [TRANSFORM_SCALING]     = cleanup_scaling,
    [TRANSFORM_AFFINE]      = cleanup_default,
    [TRANSFORM_PROJECTIVE]  = cleanup_default,
    [TRANSFORM_GLUING]      = cleanup_default,
    [TRANSFORM_COMPOSITE]   = cleanup_default,
};

void lv_transform_destroy(lvTransform *t) {
    if (!t) {
        return;
    }

    if (t->ref_count > 1) {
        t->ref_count--;
        return;
    }

    /* 清理参数（统一调度表分发：越界/NULL 槽自动跳过） */
    LV_DISPATCH_VOID(kTransformCleanupHandlers, t->type, t);

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

/* ================================================================
 * 补齐实现：声明无实现 API（C-㊺续32 测试暴露）
 *
 * lv_transform_rotation_double / lv_transform_scale / lv_transform_type_name
 * 此前仅头文件声明、全库无实现（调用即链接错误），本批补齐。
 * 矩阵/参数布局与同文件既有构造（identity/rotation_arbitrary/scaling）
 * 保持一致；destroy 清理经 kTransformCleanupHandlers 分派（SCALE 清理
 * 处理器已随本批补入）。
 * ================================================================ */

/**
 * @brief 创建缩放变换（沿坐标轴，无缩放中心）
 *
 * 矩阵 [[sx, 0, 0], [0, sy, 0], [0, 0, 1]]；is_isometry 由 sx/sy 是否均为
 * 1 判定，is_orientation_preserving 由 sx*sy 符号判定。
 */
lvTransform *lv_transform_scale(const mpq_t sx, const mpq_t sy) {
    lvTransform *t = lv_transform_identity();
    if (!t)
        return NULL;

    t->type = TRANSFORM_SCALE;

    mpq_init(t->params.params.scale.sx);
    mpq_init(t->params.params.scale.sy);
    mpq_set(t->params.params.scale.sx, sx);
    mpq_set(t->params.params.scale.sy, sy);

    mpq_set(t->matrix.a, sx);
    mpq_set(t->matrix.d, sy);

    t->is_isometry = (mpq_cmp_ui(sx, 1, 1) == 0 && mpq_cmp_ui(sy, 1, 1) == 0);

    mpq_t prod;
    mpq_init(prod);
    mpq_mul(prod, sx, sy);
    t->is_orientation_preserving = (mpq_sgn(prod) > 0);
    mpq_clear(prod);

    return t;
}

/**
 * @brief 创建旋转变换（浮点角度，绕中心 (cx, cy)）
 *
 * 角度经 cos/sin 计算后以 mpq_set_d 存入有理参数与矩阵，
 * 与 lv_transform_rotation_arbitrary 布局一致。
 */
lvTransform *lv_transform_rotation_double(double cx, double cy, double angle_rad) {
    lvTransform *t = lv_transform_identity();
    if (!t)
        return NULL;

    t->type = TRANSFORM_ROTATION;

    mpq_init(t->params.params.rotation.cx);
    mpq_init(t->params.params.rotation.cy);
    mpq_init(t->params.params.rotation.cos_theta);
    mpq_init(t->params.params.rotation.sin_theta);

    mpq_set_d(t->params.params.rotation.cx, cx);
    mpq_set_d(t->params.params.rotation.cy, cy);

    double cos_a = cos(angle_rad);
    double sin_a = sin(angle_rad);
    mpq_set_d(t->params.params.rotation.cos_theta, cos_a);
    mpq_set_d(t->params.params.rotation.sin_theta, sin_a);
    t->params.params.rotation.angle = angle_rad;
    t->params.params.rotation.angle_cos = cos_a;
    t->params.params.rotation.angle_sin = sin_a;
    t->params.params.rotation.is_special_angle = false;

    /* 旋转矩阵（绕原点，逆时针）：a=cos, b=-sin, c=sin, d=cos */
    mpq_set(t->matrix.a, t->params.params.rotation.cos_theta);
    mpq_neg(t->matrix.b, t->params.params.rotation.sin_theta);
    mpq_set(t->matrix.c, t->params.params.rotation.sin_theta);
    mpq_set(t->matrix.d, t->params.params.rotation.cos_theta);

    /* 平移分量：tx = cx - a*cx - b*cy, ty = cy - c*cx - d*cy */
    mpq_t tmp1, tmp2;
    mpq_init(tmp1);
    mpq_init(tmp2);
    mpq_mul(tmp1, t->matrix.a, t->params.params.rotation.cx);
    mpq_mul(tmp2, t->matrix.b, t->params.params.rotation.cy);
    mpq_sub(tmp1, t->params.params.rotation.cx, tmp1);
    mpq_sub(t->matrix.tx, tmp1, tmp2);
    mpq_mul(tmp1, t->matrix.c, t->params.params.rotation.cx);
    mpq_mul(tmp2, t->matrix.d, t->params.params.rotation.cy);
    mpq_sub(tmp1, t->params.params.rotation.cy, tmp1);
    mpq_sub(t->matrix.ty, tmp1, tmp2);
    mpq_clear(tmp1);
    mpq_clear(tmp2);

    t->is_isometry = true;
    t->is_orientation_preserving = true;

    return t;
}

/**
 * @brief 获取变换类型名称
 */
const char *lv_transform_type_name(lvTransformType type) {
    switch ((int) type) {
        case TRANSFORM_IDENTITY:
            return "identity";
        case TRANSFORM_TRANSLATION:
            return "translation";
        case TRANSFORM_ROTATION:
            return "rotation";
        case TRANSFORM_SCALE:
            return "scale";
        case TRANSFORM_SHEAR:
            return "shear";
        case TRANSFORM_REFLECTION:
            return "reflection";
        case TRANSFORM_SCALING:
            return "scaling";
        case TRANSFORM_AFFINE:
            return "affine";
        case TRANSFORM_PROJECTIVE:
            return "projective";
        case TRANSFORM_GLUING:
            return "gluing";
        case TRANSFORM_COMPOSITE:
            return "composite";
        default:
            return "unknown";
    }
}

