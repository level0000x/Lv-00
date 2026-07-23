/**
 * @file ga_multivector.c
 * @brief 投影几何代数（PGA）多向量实现
 * @details 实现 Cl(3,0,1) 代数，包含 16 个基元素：
 *          1, e0, e1, e2, e3, e01, e02, e03, e12, e13, e23,
 *          e012, e013, e023, e123, e0123。
 *          提供多向量的创建/销毁/复制、四则运算、几何积、外积、
 *          内积、对偶、sandwich 积等基础操作。
 * @author Lv-00 Project
 * @version 1.0.0
 */

#include "lv/ga_multivector.h"
#include "lv/lv_internal.h"
#include "lv/lv_utils.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ============================================================
 * Basis element indices (Cl(3,0,1))
 * ============================================================ */
#define GA_S    0   /* 1 (scalar) */
#define GA_E0   1   /* e0 */
#define GA_E1   2   /* e1 */
#define GA_E2   3   /* e2 */
#define GA_E3   4   /* e3 */
#define GA_E01  5   /* e0^e1 */
#define GA_E02  6   /* e0^e2 */
#define GA_E03  7   /* e0^e3 */
#define GA_E12  8   /* e1^e2 */
#define GA_E13  9   /* e1^e3 */
#define GA_E23  10  /* e2^e3 */
#define GA_E012 11  /* e0^e1^e2 */
#define GA_E013 12  /* e0^e1^e3 */
#define GA_E023 13  /* e0^e2^e3 */
#define GA_E123 14  /* e1^e2^e3 */
#define GA_E0123 15 /* e0^e1^e2^e3 (pseudoscalar) */

/* ============================================================
 * Internal structure
 * ============================================================ */

struct lvMultiVector {
    double c[16];  /* Coefficients for each basis element */
};

/* ============================================================
 * Lifecycle
 * ============================================================ */

/**
 * @brief 创建零多向量
 * @return 新多向量（调用者通过 ga_mv_destroy 释放），失败返回 NULL
 */
lvMultiVector *ga_mv_create(void) {
    lvMultiVector *mv = lv_calloc(1, sizeof(lvMultiVector));
    return mv;
}

/**
 * @brief 销毁多向量并释放内存
 * @param mv 目标多向量（可为 NULL）
 */
void ga_mv_destroy(lvMultiVector *mv) {
    lv_free((void **)&mv);
}

/**
 * @brief 深度复制多向量
 * @param src 源多向量
 * @return 新副本（调用者负责释放），src 为 NULL 时返回 NULL
 */
lvMultiVector *ga_mv_copy(const lvMultiVector *src) {
    if (!src) return NULL;

    lvMultiVector *copy = ga_mv_create();
    if (!copy) return NULL;

    memcpy(copy->c, src->c, sizeof(copy->c));
    return copy;
}

/**
 * @brief 创建全零多向量（等价于 ga_mv_create）
 * @return 零多向量（调用者负责释放），失败返回 NULL
 */
lvMultiVector *ga_mv_zero(void) {
    return ga_mv_create();  /* calloc initializes to zero */
}

/* ============================================================
 * Coefficient access
 * ============================================================ */

/**
 * @brief 获取指定基元素的系数
 * @param mv    多向量
 * @param index 基索引（0~15）
 * @return 系数值；参数无效时返回 0.0
 */
double ga_mv_get(const lvMultiVector *mv, int index) {
    if (!mv || index < 0 || index >= 16) return 0.0;
    return mv->c[index];
}

/**
 * @brief 设置指定基元素的系数
 * @param mv    多向量
 * @param index 基索引（0~15）
 * @param value 系数值
 */
void ga_mv_set(lvMultiVector *mv, int index, double value) {
    if (!mv || index < 0 || index >= 16) return;
    mv->c[index] = value;
}

/* ============================================================
 * Grade operations
 * ============================================================ */

/**
 * @brief 计算多向量的最高阶数（grade）
 * @param mv 多向量
 * @return 最高阶数（0~4），mv 为 NULL 时返回 -1，零向量返回 -1
 */
int ga_mv_grade(const lvMultiVector *mv) {
    if (!mv) return -1;

    int max_grade = -1;
    double eps = 1e-10;

    /* Grade 0: scalar */
    if (fabs(mv->c[GA_S]) > eps) max_grade = 0;

    /* Grade 1: vectors */
    if (fabs(mv->c[GA_E0]) > eps || fabs(mv->c[GA_E1]) > eps ||
        fabs(mv->c[GA_E2]) > eps || fabs(mv->c[GA_E3]) > eps)
        max_grade = 1;

    /* Grade 2: bivectors */
    if (fabs(mv->c[GA_E01]) > eps || fabs(mv->c[GA_E02]) > eps ||
        fabs(mv->c[GA_E03]) > eps || fabs(mv->c[GA_E12]) > eps ||
        fabs(mv->c[GA_E13]) > eps || fabs(mv->c[GA_E23]) > eps)
        max_grade = 2;

    /* Grade 3: trivectors */
    if (fabs(mv->c[GA_E012]) > eps || fabs(mv->c[GA_E013]) > eps ||
        fabs(mv->c[GA_E023]) > eps || fabs(mv->c[GA_E123]) > eps)
        max_grade = 3;

    /* Grade 4: pseudoscalar */
    if (fabs(mv->c[GA_E0123]) > eps)
        max_grade = 4;

    return max_grade;
}

/**
 * @brief 提取指定阶数的分量
 * @param mv    多向量
 * @param grade 目标阶数（0~4）
 * @return 新多向量（仅包含指定阶数的分量），失败返回 NULL
 */
lvMultiVector *ga_mv_grade_project(const lvMultiVector *mv, int grade) {
    if (!mv) return NULL;

    lvMultiVector *result = ga_mv_zero();
    if (!result) return NULL;

    switch (grade) {
    case 0:
        result->c[GA_S] = mv->c[GA_S];
        break;
    case 1:
        result->c[GA_E0] = mv->c[GA_E0];
        result->c[GA_E1] = mv->c[GA_E1];
        result->c[GA_E2] = mv->c[GA_E2];
        result->c[GA_E3] = mv->c[GA_E3];
        break;
    case 2:
        result->c[GA_E01] = mv->c[GA_E01];
        result->c[GA_E02] = mv->c[GA_E02];
        result->c[GA_E03] = mv->c[GA_E03];
        result->c[GA_E12] = mv->c[GA_E12];
        result->c[GA_E13] = mv->c[GA_E13];
        result->c[GA_E23] = mv->c[GA_E23];
        break;
    case 3:
        result->c[GA_E012] = mv->c[GA_E012];
        result->c[GA_E013] = mv->c[GA_E013];
        result->c[GA_E023] = mv->c[GA_E023];
        result->c[GA_E123] = mv->c[GA_E123];
        break;
    case 4:
        result->c[GA_E0123] = mv->c[GA_E0123];
        break;
    }

    return result;
}

/* ============================================================
 * Arithmetic operations
 * ============================================================ */

/**
 * @brief 多向量加法
 * @param a, b  加数多向量
 * @return 新多向量（a + b），失败返回 NULL
 */
lvMultiVector *ga_mv_add(const lvMultiVector *a, const lvMultiVector *b) {
    if (!a || !b) return NULL;

    lvMultiVector *result = ga_mv_create();
    if (!result) return NULL;

    for (int i = 0; i < 16; i++) {
        result->c[i] = a->c[i] + b->c[i];
    }

    return result;
}

/**
 * @brief 多向量减法
 * @param a, b  多向量
 * @return 新多向量（a - b），失败返回 NULL
 */
lvMultiVector *ga_mv_sub(const lvMultiVector *a, const lvMultiVector *b) {
    if (!a || !b) return NULL;

    lvMultiVector *result = ga_mv_create();
    if (!result) return NULL;

    for (int i = 0; i < 16; i++) {
        result->c[i] = a->c[i] - b->c[i];
    }

    return result;
}

/**
 * @brief 多向量数乘
 * @param mv     多向量
 * @param scalar 标量因子
 * @return 新多向量（mv * scalar），失败返回 NULL
 */
lvMultiVector *ga_mv_scale(const lvMultiVector *mv, double scalar) {
    if (!mv) return NULL;

    lvMultiVector *result = ga_mv_create();
    if (!result) return NULL;

    for (int i = 0; i < 16; i++) {
        result->c[i] = mv->c[i] * scalar;
    }

    return result;
}

/**
 * @brief 多向量取负
 * @param mv 多向量
 * @return 新多向量（-mv），失败返回 NULL
 */
lvMultiVector *ga_mv_negate(const lvMultiVector *mv) {
    return ga_mv_scale(mv, -1.0);
}

/* ============================================================
 * Geometric product (simplified)
 * ============================================================ */

/**
 * @brief 多向量几何积（简化实现）
 * @details 当前仅正确处理：标量×任意、向量×向量（点积+外积）
 *          其余情况仅复制 a 的分量作为占位。
 * @param a, b  相乘的多向量
 * @return 新多向量（调用者负责释放），失败返回 NULL
 */
lvMultiVector *ga_mv_geometric_product(const lvMultiVector *a,
                                          const lvMultiVector *b) {
    if (!a || !b) return NULL;

    lvMultiVector *result = ga_mv_zero();
    if (!result) return NULL;

    /* Simplified: only handle common cases */
    /* Scalar * anything */
    if (fabs(a->c[GA_S]) > 1e-10 && ga_mv_grade(a) == 0) {
        return ga_mv_scale(b, a->c[GA_S]);
    }
    if (fabs(b->c[GA_S]) > 1e-10 && ga_mv_grade(b) == 0) {
        return ga_mv_scale(a, b->c[GA_S]);
    }

    /* Vector * Vector: a·b + a^b */
    if (ga_mv_grade(a) == 1 && ga_mv_grade(b) == 1) {
        /* Dot product (scalar part) */
        result->c[GA_S] = (a->c[GA_E1] * b->c[GA_E1] +
                           a->c[GA_E2] * b->c[GA_E2] +
                           a->c[GA_E3] * b->c[GA_E3]);

        /* Outer product (bivector part) */
        result->c[GA_E12] = a->c[GA_E1] * b->c[GA_E2] - a->c[GA_E2] * b->c[GA_E1];
        result->c[GA_E13] = a->c[GA_E1] * b->c[GA_E3] - a->c[GA_E3] * b->c[GA_E1];
        result->c[GA_E23] = a->c[GA_E2] * b->c[GA_E3] - a->c[GA_E3] * b->c[GA_E2];

        return result;
    }

    /* General case: copy a (placeholder) */
    for (int i = 0; i < 16; i++) {
        result->c[i] = a->c[i];
    }

    return result;
}

/* ============================================================
 * Inner product (dot product)
 * ============================================================ */

/**
 * @brief 多向量内积（点积）
 * @details 当前仅实现向量部分的标准点积（e1·e1 + e2·e2 + e3·e3）
 * @param a, b  多向量
 * @return 内积标量值
 */
double ga_mv_inner_product(const lvMultiVector *a, const lvMultiVector *b) {
    if (!a || !b) return 0.0;

    /* For vectors: standard dot product */
    return (a->c[GA_E1] * b->c[GA_E1] +
            a->c[GA_E2] * b->c[GA_E2] +
            a->c[GA_E3] * b->c[GA_E3]);
}

/* ============================================================
 * Outer product (wedge product)
 * ============================================================ */

/**
 * @brief 多向量外积（wedge product）
 * @details 实现 Cl(3,0,1) 中外积的完整计算，包括各阶数之间的外积。
 * @param a, b  多向量
 * @return 新多向量（调用者负责释放），失败返回 NULL
 */
lvMultiVector *ga_mv_outer_product(const lvMultiVector *a,
                                      const lvMultiVector *b) {
    if (!a || !b) return NULL;

    lvMultiVector *result = ga_mv_zero();
    if (!result) return NULL;

    /* scalar * anything */
    for (int i = 0; i < 16; i++) {
        result->c[i] += a->c[GA_S] * b->c[i] + a->c[i] * b->c[GA_S];
    }
    /* subtract double-counted scalar*scalar */
    result->c[GA_S] = a->c[GA_S] * b->c[GA_S];

    /* grade-1 ^ grade-1 → grade-2 */
    result->c[GA_E01] += a->c[GA_E0]*b->c[GA_E1] - a->c[GA_E1]*b->c[GA_E0];
    result->c[GA_E02] += a->c[GA_E0]*b->c[GA_E2] - a->c[GA_E2]*b->c[GA_E0];
    result->c[GA_E03] += a->c[GA_E0]*b->c[GA_E3] - a->c[GA_E3]*b->c[GA_E0];
    result->c[GA_E12] += a->c[GA_E1]*b->c[GA_E2] - a->c[GA_E2]*b->c[GA_E1];
    result->c[GA_E13] += a->c[GA_E1]*b->c[GA_E3] - a->c[GA_E3]*b->c[GA_E1];
    result->c[GA_E23] += a->c[GA_E2]*b->c[GA_E3] - a->c[GA_E3]*b->c[GA_E2];

    /* grade-1 ^ grade-2 → grade-3 */
    result->c[GA_E012] += a->c[GA_E0]*b->c[GA_E12] - a->c[GA_E1]*b->c[GA_E02] + a->c[GA_E2]*b->c[GA_E01];
    result->c[GA_E013] += a->c[GA_E0]*b->c[GA_E13] - a->c[GA_E1]*b->c[GA_E03] + a->c[GA_E3]*b->c[GA_E01];
    result->c[GA_E023] += a->c[GA_E0]*b->c[GA_E23] - a->c[GA_E2]*b->c[GA_E03] + a->c[GA_E3]*b->c[GA_E02];
    result->c[GA_E123] += a->c[GA_E1]*b->c[GA_E23] - a->c[GA_E2]*b->c[GA_E13] + a->c[GA_E3]*b->c[GA_E12];

    /* grade-2 ^ grade-1 → grade-3 */
    result->c[GA_E012] += a->c[GA_E01]*b->c[GA_E2] - a->c[GA_E02]*b->c[GA_E1] + a->c[GA_E12]*b->c[GA_E0];
    result->c[GA_E013] += a->c[GA_E01]*b->c[GA_E3] - a->c[GA_E03]*b->c[GA_E1] + a->c[GA_E13]*b->c[GA_E0];
    result->c[GA_E023] += a->c[GA_E02]*b->c[GA_E3] - a->c[GA_E03]*b->c[GA_E2] + a->c[GA_E23]*b->c[GA_E0];
    result->c[GA_E123] += a->c[GA_E12]*b->c[GA_E3] - a->c[GA_E13]*b->c[GA_E2] + a->c[GA_E23]*b->c[GA_E1];

    /* grade-2 ^ grade-2 → grade-4 */
    result->c[GA_E0123] += a->c[GA_E01]*b->c[GA_E23] - a->c[GA_E02]*b->c[GA_E13]
                         + a->c[GA_E03]*b->c[GA_E12] + a->c[GA_E12]*b->c[GA_E03]
                         - a->c[GA_E13]*b->c[GA_E02] + a->c[GA_E23]*b->c[GA_E01];

    /* grade-1 ^ grade-3 → grade-4 */
    result->c[GA_E0123] += a->c[GA_E0]*b->c[GA_E123] - a->c[GA_E1]*b->c[GA_E023]
                         + a->c[GA_E2]*b->c[GA_E013] - a->c[GA_E3]*b->c[GA_E012];

    /* grade-3 ^ grade-1 → grade-4 */
    result->c[GA_E0123] += a->c[GA_E012]*b->c[GA_E3] - a->c[GA_E013]*b->c[GA_E2]
                         + a->c[GA_E023]*b->c[GA_E1] - a->c[GA_E123]*b->c[GA_E0];

    return result;
}

/* ============================================================
 * Norm and reverse
 * ============================================================ */

/**
 * @brief 计算多向量的欧几里得范数
 * @param mv 多向量
 * @return 范数值；mv 为 NULL 时返回 0.0
 */
double ga_mv_norm(const lvMultiVector *mv) {
    if (!mv) return 0.0;

    double sum = 0.0;
    for (int i = 0; i < 16; i++) {
        sum += mv->c[i] * mv->c[i];
    }

    return sqrt(sum);
}

/**
 * @brief 计算多向量范数的平方
 * @param mv 多向量
 * @return 范数平方值；mv 为 NULL 时返回 0.0
 */
double ga_mv_norm_squared(const lvMultiVector *mv) {
    if (!mv) return 0.0;

    double sum = 0.0;
    for (int i = 0; i < 16; i++) {
        sum += mv->c[i] * mv->c[i];
    }

    return sum;
}

/**
 * @brief 多向量反转（reverse）
 * @details 偶数阶不变，奇数阶取反。用于 sandwich 积中的 R~ 计算。
 * @param mv 多向量
 * @return 反转后的多向量（调用者负责释放），失败返回 NULL
 */
lvMultiVector *ga_mv_reverse(const lvMultiVector *mv) {
    if (!mv) return NULL;

    lvMultiVector *result = ga_mv_create();
    if (!result) return NULL;

    /* Grade 0: unchanged */
    result->c[GA_S] = mv->c[GA_S];

    /* Grade 1: unchanged */
    result->c[GA_E0] = mv->c[GA_E0];
    result->c[GA_E1] = mv->c[GA_E1];
    result->c[GA_E2] = mv->c[GA_E2];
    result->c[GA_E3] = mv->c[GA_E3];

    /* Grade 2: negate */
    result->c[GA_E01] = -mv->c[GA_E01];
    result->c[GA_E02] = -mv->c[GA_E02];
    result->c[GA_E03] = -mv->c[GA_E03];
    result->c[GA_E12] = -mv->c[GA_E12];
    result->c[GA_E13] = -mv->c[GA_E13];
    result->c[GA_E23] = -mv->c[GA_E23];

    /* Grade 3: negate */
    result->c[GA_E012] = -mv->c[GA_E012];
    result->c[GA_E013] = -mv->c[GA_E013];
    result->c[GA_E023] = -mv->c[GA_E023];
    result->c[GA_E123] = -mv->c[GA_E123];

    /* Grade 4: unchanged */
    result->c[GA_E0123] = mv->c[GA_E0123];

    return result;
}

/**
 * @brief 多向量归一化
 * @param mv 多向量
 * @return 单位多向量（调用者负责释放）；范数为零时返回 NULL
 */
lvMultiVector *ga_mv_normalize(const lvMultiVector *mv) {
    if (!mv) return NULL;

    double norm = ga_mv_norm(mv);
    if (fabs(norm) < 1e-10) return NULL;

    return ga_mv_scale(mv, 1.0 / norm);
}

/* ============================================================
 * Dual and sandwich
 * ============================================================ */

/**
 * @brief Hodge 对偶变换
 * @details 在 Cl(3,0,1) 中通过乘以伪标量逆（-e0123）实现。
 * @param mv 多向量
 * @return 对偶多向量（调用者负责释放），失败返回 NULL
 */
lvMultiVector *ga_mv_dual(const lvMultiVector *mv) {
    if (!mv) return NULL;

    lvMultiVector *result = ga_mv_create();
    if (!result) return NULL;

    /* Hodge dual: multiply by pseudoscalar inverse */
    /* In Cl(3,0,1), I = e0123, I^{-1} = -e0123 */
    result->c[GA_S] = -mv->c[GA_E0123];
    result->c[GA_E0] = -mv->c[GA_E123];
    result->c[GA_E1] = mv->c[GA_E023];
    result->c[GA_E2] = -mv->c[GA_E013];
    result->c[GA_E3] = mv->c[GA_E012];
    result->c[GA_E01] = mv->c[GA_E23];
    result->c[GA_E02] = -mv->c[GA_E13];
    result->c[GA_E03] = mv->c[GA_E12];
    result->c[GA_E12] = mv->c[GA_E03];
    result->c[GA_E13] = -mv->c[GA_E02];
    result->c[GA_E23] = mv->c[GA_E01];
    result->c[GA_E012] = mv->c[GA_E3];
    result->c[GA_E013] = -mv->c[GA_E2];
    result->c[GA_E023] = mv->c[GA_E1];
    result->c[GA_E123] = -mv->c[GA_E0];
    result->c[GA_E0123] = mv->c[GA_S];

    return result;
}

/**
 * @brief Sandwich 积（R * mv * R~）
 * @details 用于 rotor 对多向量的共轭变换。
 * @param rotor 旋转多向量
 * @param mv    被变换的多向量
 * @return 变换后的多向量（调用者负责释放），失败返回 NULL
 */
lvMultiVector *ga_mv_sandwich(const lvMultiVector *rotor,
                                 const lvMultiVector *mv) {
    if (!rotor || !mv) return NULL;

    /* Sandwich product: R * mv * R~ */
    lvMultiVector *r_rev = ga_mv_reverse(rotor);
    if (!r_rev) return NULL;

    lvMultiVector *temp = ga_mv_geometric_product(rotor, mv);
    if (!temp) {
        ga_mv_destroy(r_rev);
        return NULL;
    }

    lvMultiVector *result = ga_mv_geometric_product(temp, r_rev);

    ga_mv_destroy(r_rev);
    ga_mv_destroy(temp);

    return result;
}

/* ============================================================
 * Comparison
 * ============================================================ */

/**
 * @brief 比较两个多向量是否在容差内相等
 * @param a, b  多向量
 * @param eps   容差阈值
 * @return 所有分量差值绝对值均 <= eps 时返回 true
 */
bool ga_mv_equal(const lvMultiVector *a, const lvMultiVector *b, double eps) {
    if (!a || !b) return false;

    for (int i = 0; i < 16; i++) {
        if (fabs(a->c[i] - b->c[i]) > eps) return false;
    }

    return true;
}

/* ── ga_mv_scalar: create scalar multivector ── */
/**
 * @brief 创建标量多向量
 * @param value 标量值
 * @return 新多向量（仅标量分量非零），失败返回 NULL
 */
lvMultiVector *ga_mv_scalar(double value) {
    lvMultiVector *mv = ga_mv_create();
    if (mv) mv->c[0] = value;
    return mv;
}

/**
 * @brief 判断多向量是否为零（所有分量在容差内为零）
 * @param mv  多向量
 * @param eps 容差阈值
 * @return 零向量返回 true；mv 为 NULL 时视为零向量返回 true
 */
bool ga_mv_is_zero(const lvMultiVector *mv, double eps) {
    if (!mv) return true;

    for (int i = 0; i < 16; i++) {
        if (fabs(mv->c[i]) > eps) return false;
    }

    return true;
}
