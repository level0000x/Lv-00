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

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "lv/lv_internal.h"
#include "lv/lv_utils.h"

/* ============================================================
 * Basis element indices (Cl(3,0,1))
 * ============================================================ */
#define GA_S 0      /* 1 (scalar) */
#define GA_E0 1     /* e0 */
#define GA_E1 2     /* e1 */
#define GA_E2 3     /* e2 */
#define GA_E3 4     /* e3 */
#define GA_E01 5    /* e0^e1 */
#define GA_E02 6    /* e0^e2 */
#define GA_E03 7    /* e0^e3 */
#define GA_E12 8    /* e1^e2 */
#define GA_E13 9    /* e1^e3 */
#define GA_E23 10   /* e2^e3 */
#define GA_E012 11  /* e0^e1^e2 */
#define GA_E013 12  /* e0^e1^e3 */
#define GA_E023 13  /* e0^e2^e3 */
#define GA_E123 14  /* e1^e2^e3 */
#define GA_E0123 15 /* e0^e1^e2^e3 (pseudoscalar) */

/* ============================================================
 * Internal structure
 * ============================================================ */

struct lvMultiVector {
    double c[16]; /* Coefficients for each basis element */
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
    lv_free((void **) &mv);
}

/**
 * @brief 深度复制多向量
 * @param src 源多向量
 * @return 新副本（调用者负责释放），src 为 NULL 时返回 NULL
 */
lvMultiVector *ga_mv_copy(const lvMultiVector *src) {
    if (!src)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "ga_mv_copy: src is NULL");

    lvMultiVector *copy = ga_mv_create();
    if (!copy)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "ga_mv_copy: copy allocation failed");

    memcpy(copy->c, src->c, sizeof(copy->c));
    return copy;
}

/**
 * @brief 创建全零多向量（等价于 ga_mv_create）
 * @return 零多向量（调用者负责释放），失败返回 NULL
 */
lvMultiVector *ga_mv_zero(void) {
    return ga_mv_create(); /* calloc initializes to zero */
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
    if (!mv || index < 0 || index >= 16)
        return 0.0;
    return mv->c[index];
}

/**
 * @brief 设置指定基元素的系数
 * @param mv    多向量
 * @param index 基索引（0~15）
 * @param value 系数值
 */
void ga_mv_set(lvMultiVector *mv, int index, double value) {
    if (!mv || index < 0 || index >= 16)
        return;
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
    if (!mv)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "ga_mv_grade: mv is NULL");

    int max_grade = -1;
    double eps = 1e-10;

    /* Grade 0: scalar */
    if (fabs(mv->c[GA_S]) > eps)
        max_grade = 0;

    /* Grade 1: vectors */
    if (fabs(mv->c[GA_E0]) > eps || fabs(mv->c[GA_E1]) > eps || fabs(mv->c[GA_E2]) > eps || fabs(mv->c[GA_E3]) > eps)
        max_grade = 1;

    /* Grade 2: bivectors */
    if (fabs(mv->c[GA_E01]) > eps || fabs(mv->c[GA_E02]) > eps || fabs(mv->c[GA_E03]) > eps ||
        fabs(mv->c[GA_E12]) > eps || fabs(mv->c[GA_E13]) > eps || fabs(mv->c[GA_E23]) > eps)
        max_grade = 2;

    /* Grade 3: trivectors */
    if (fabs(mv->c[GA_E012]) > eps || fabs(mv->c[GA_E013]) > eps || fabs(mv->c[GA_E023]) > eps ||
        fabs(mv->c[GA_E123]) > eps)
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
    if (!mv)
        return NULL;

    lvMultiVector *result = ga_mv_zero();
    if (!result)
        return NULL;

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
    if (!a || !b)
        return NULL;

    lvMultiVector *result = ga_mv_create();
    if (!result)
        return NULL;

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
    if (!a || !b)
        return NULL;

    lvMultiVector *result = ga_mv_create();
    if (!result)
        return NULL;

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
    if (!mv)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "ga_mv_scale: mv is NULL");

    lvMultiVector *result = ga_mv_create();
    if (!result)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "ga_mv_scale: result allocation failed");

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
 * Geometric product (complete)
 * ============================================================ */

/*
 * Bitmask representation for basis vectors:
 *   bit 0 = e0, bit 1 = e1, bit 2 = e2, bit 3 = e3
 *
 * Geometric product rules for Cl(3,0,1):
 *   e0 * e0 = 0
 *   e1 * e1 = e2 * e2 = e3 * e3 = 1
 *   ei * ej = -ej * ei  (i ≠ j)
 */

/* Basis vector bitmask for each blade index (0-15) */
static const unsigned char s_blade_bits[16] = {
    0,  /* GA_S     (scalar)       */
    1,  /* GA_E0    (e0)           */
    2,  /* GA_E1    (e1)           */
    4,  /* GA_E2    (e2)           */
    8,  /* GA_E3    (e3)           */
    3,  /* GA_E01   (e0∧e1)        */
    5,  /* GA_E02   (e0∧e2)        */
    9,  /* GA_E03   (e0∧e3)        */
    6,  /* GA_E12   (e1∧e2)        */
    10, /* GA_E13   (e1∧e3)        */
    12, /* GA_E23   (e2∧e3)        */
    7,  /* GA_E012  (e0∧e1∧e2)     */
    11, /* GA_E013  (e0∧e1∧e3)     */
    13, /* GA_E023  (e0∧e2∧e3)     */
    14, /* GA_E123  (e1∧e2∧e3)     */
    15  /* GA_E0123 (e0∧e1∧e2∧e3)  */
};

/* Reverse: bitmask (0-15) → blade index */
static const unsigned char s_blade_from_mask[16] = {
    0,  /* mask 0  → GA_S     */
    1,  /* mask 1  → GA_E0    */
    2,  /* mask 2  → GA_E1    */
    5,  /* mask 3  → GA_E01   */
    3,  /* mask 4  → GA_E2    */
    6,  /* mask 5  → GA_E02   */
    8,  /* mask 6  → GA_E12   */
    11, /* mask 7  → GA_E012  */
    4,  /* mask 8  → GA_E3    */
    7,  /* mask 9  → GA_E03   */
    9,  /* mask 10 → GA_E13   */
    12, /* mask 11 → GA_E013  */
    10, /* mask 12 → GA_E23   */
    13, /* mask 13 → GA_E023  */
    14, /* mask 14 → GA_E123  */
    15  /* mask 15 → GA_E0123 */
};

/**
 * @brief 计算两个基元素（basis blade）的几何积
 * @param a_idx, b_idx  基元素索引 (0-15)
 * @param out_blade     输出：结果 blade 索引 (0-15)，乘积为零时输出 -1
 * @param out_sign      输出：+1.0 或 -1.0（out_blade == -1 时未定义）
 *
 * @details 算法：将两个 blade 的基向量展开成有序列表，连接后使用冒泡排序
 *          重排至规范次序。排序过程中处理：
 *          - 相等相邻向量：ei*ei → 1（e0*e0 → 0 返回零）
 *          - 逆序相邻向量：交换并翻转符号
 */
static void ga_basis_geometric_product(int a_idx, int b_idx, int *out_blade, double *out_sign) {
    int mask_a = s_blade_bits[a_idx];
    int mask_b = s_blade_bits[b_idx];

    /* e0 同时在两个 blade 中出现 → e0² = 0，乘积为零 */
    if ((mask_a & mask_b) & 1) {
        *out_blade = -1;
        *out_sign = 0.0;
        return;
    }

    /* 收集两个 blade 的基向量（已各自有序） */
    unsigned char vecs[8];
    int n = 0;
    for (int i = 0; i < 4; i++) {
        if (mask_a & (1 << i))
            vecs[n++] = (unsigned char) i;
    }
    for (int i = 0; i < 4; i++) {
        if (mask_b & (1 << i))
            vecs[n++] = (unsigned char) i;
    }

    double sign = 1.0;

    /* 冒泡排序 + 同时检查相等收缩 */
    for (;;) {
        int changed = 0;
        for (int i = 0; i < n - 1; i++) {
            if (vecs[i] == vecs[i + 1]) {
                /* 相等相邻 → 收缩 */
                if (vecs[i] == 0) {
                    /* e0 * e0 = 0 */
                    *out_blade = -1;
                    *out_sign = 0.0;
                    return;
                }
                /* ei * ei = 1 (i=1,2,3) → 删除两个元素 */
                for (int j = i; j < n - 2; j++)
                    vecs[j] = vecs[j + 2];
                n -= 2;
                changed = 1;
                break;
            }
            if (vecs[i] > vecs[i + 1]) {
                /* 逆序 → 交换 */
                unsigned char tmp = vecs[i];
                vecs[i] = vecs[i + 1];
                vecs[i + 1] = tmp;
                sign = -sign;
                changed = 1;
                break;
            }
        }
        if (!changed)
            break;
    }

    /* 将结果向量列表映射回 blade 索引 */
    if (n == 0) {
        *out_blade = GA_S;
        *out_sign = sign;
        return;
    }
    int result_mask = 0;
    for (int i = 0; i < n; i++)
        result_mask |= (1 << vecs[i]);
    *out_blade = (int) s_blade_from_mask[result_mask];
    *out_sign = sign;
}

/**
 * @brief 多向量几何积（完整实现）
 * @details 遍历两个多向量的非零系数分量，使用基元素几何积查找表
 *          计算所有 blade 组合的结果并累加。覆盖 Cl(3,0,1) 中所有
 *          16×16 种基元素组合。
 * @param a, b  相乘的多向量
 * @return 新多向量（调用者负责释放），失败返回 NULL
 */
lvMultiVector *ga_mv_geometric_product(const lvMultiVector *a, const lvMultiVector *b) {
    if (!a || !b)
        return NULL;

    lvMultiVector *result = ga_mv_zero();
    if (!result)
        return NULL;

    for (int i = 0; i < 16; i++) {
        double ai = a->c[i];
        if (fabs(ai) < 1e-14)
            continue;
        for (int j = 0; j < 16; j++) {
            double bj = b->c[j];
            if (fabs(bj) < 1e-14)
                continue;

            int blade;
            double sign;
            ga_basis_geometric_product(i, j, &blade, &sign);
            if (blade >= 0) {
                result->c[blade] += ai * bj * sign;
            }
        }
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
    if (!a || !b)
        return 0.0;

    /* For vectors: standard dot product */
    return (a->c[GA_E1] * b->c[GA_E1] + a->c[GA_E2] * b->c[GA_E2] + a->c[GA_E3] * b->c[GA_E3]);
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
lvMultiVector *ga_mv_outer_product(const lvMultiVector *a, const lvMultiVector *b) {
    if (!a || !b)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "ga_mv_outer_product: a or b is NULL");

    lvMultiVector *result = ga_mv_zero();
    if (!result)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "ga_mv_outer_product: result allocation failed");

    /* scalar * anything */
    for (int i = 0; i < 16; i++) {
        result->c[i] += a->c[GA_S] * b->c[i] + a->c[i] * b->c[GA_S];
    }
    /* subtract double-counted scalar*scalar */
    result->c[GA_S] = a->c[GA_S] * b->c[GA_S];

    /* grade-1 ^ grade-1 → grade-2 */
    result->c[GA_E01] += a->c[GA_E0] * b->c[GA_E1] - a->c[GA_E1] * b->c[GA_E0];
    result->c[GA_E02] += a->c[GA_E0] * b->c[GA_E2] - a->c[GA_E2] * b->c[GA_E0];
    result->c[GA_E03] += a->c[GA_E0] * b->c[GA_E3] - a->c[GA_E3] * b->c[GA_E0];
    result->c[GA_E12] += a->c[GA_E1] * b->c[GA_E2] - a->c[GA_E2] * b->c[GA_E1];
    result->c[GA_E13] += a->c[GA_E1] * b->c[GA_E3] - a->c[GA_E3] * b->c[GA_E1];
    result->c[GA_E23] += a->c[GA_E2] * b->c[GA_E3] - a->c[GA_E3] * b->c[GA_E2];

    /* grade-1 ^ grade-2 → grade-3 */
    result->c[GA_E012] += a->c[GA_E0] * b->c[GA_E12] - a->c[GA_E1] * b->c[GA_E02] + a->c[GA_E2] * b->c[GA_E01];
    result->c[GA_E013] += a->c[GA_E0] * b->c[GA_E13] - a->c[GA_E1] * b->c[GA_E03] + a->c[GA_E3] * b->c[GA_E01];
    result->c[GA_E023] += a->c[GA_E0] * b->c[GA_E23] - a->c[GA_E2] * b->c[GA_E03] + a->c[GA_E3] * b->c[GA_E02];
    result->c[GA_E123] += a->c[GA_E1] * b->c[GA_E23] - a->c[GA_E2] * b->c[GA_E13] + a->c[GA_E3] * b->c[GA_E12];

    /* grade-2 ^ grade-1 → grade-3 */
    result->c[GA_E012] += a->c[GA_E01] * b->c[GA_E2] - a->c[GA_E02] * b->c[GA_E1] + a->c[GA_E12] * b->c[GA_E0];
    result->c[GA_E013] += a->c[GA_E01] * b->c[GA_E3] - a->c[GA_E03] * b->c[GA_E1] + a->c[GA_E13] * b->c[GA_E0];
    result->c[GA_E023] += a->c[GA_E02] * b->c[GA_E3] - a->c[GA_E03] * b->c[GA_E2] + a->c[GA_E23] * b->c[GA_E0];
    result->c[GA_E123] += a->c[GA_E12] * b->c[GA_E3] - a->c[GA_E13] * b->c[GA_E2] + a->c[GA_E23] * b->c[GA_E1];

    /* grade-2 ^ grade-2 → grade-4 */
    result->c[GA_E0123] += a->c[GA_E01] * b->c[GA_E23] - a->c[GA_E02] * b->c[GA_E13] + a->c[GA_E03] * b->c[GA_E12] +
                           a->c[GA_E12] * b->c[GA_E03] - a->c[GA_E13] * b->c[GA_E02] + a->c[GA_E23] * b->c[GA_E01];

    /* grade-1 ^ grade-3 → grade-4 */
    result->c[GA_E0123] += a->c[GA_E0] * b->c[GA_E123] - a->c[GA_E1] * b->c[GA_E023] + a->c[GA_E2] * b->c[GA_E013] -
                           a->c[GA_E3] * b->c[GA_E012];

    /* grade-3 ^ grade-1 → grade-4 */
    result->c[GA_E0123] += a->c[GA_E012] * b->c[GA_E3] - a->c[GA_E013] * b->c[GA_E2] + a->c[GA_E023] * b->c[GA_E1] -
                           a->c[GA_E123] * b->c[GA_E0];

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
    if (!mv)
        return 0.0;

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
    if (!mv)
        return 0.0;

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
    if (!mv)
        return NULL;

    lvMultiVector *result = ga_mv_create();
    if (!result)
        return NULL;

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
    if (!mv)
        return NULL;

    double norm = ga_mv_norm(mv);
    if (fabs(norm) < 1e-10)
        return NULL;

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
    if (!mv)
        return NULL;

    lvMultiVector *result = ga_mv_create();
    if (!result)
        return NULL;

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
lvMultiVector *ga_mv_sandwich(const lvMultiVector *rotor, const lvMultiVector *mv) {
    if (!rotor || !mv)
        return NULL;

    /* Sandwich product: R * mv * R~ */
    lvMultiVector *r_rev = ga_mv_reverse(rotor);
    if (!r_rev)
        return NULL;

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
    if (!a || !b)
        return false;

    for (int i = 0; i < 16; i++) {
        if (fabs(a->c[i] - b->c[i]) > eps)
            return false;
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
    if (mv)
        mv->c[0] = value;
    return mv;
}

/**
 * @brief 判断多向量是否为零（所有分量在容差内为零）
 * @param mv  多向量
 * @param eps 容差阈值
 * @return 零向量返回 true；mv 为 NULL 时视为零向量返回 true
 */
bool ga_mv_is_zero(const lvMultiVector *mv, double eps) {
    if (!mv)
        return true;

    for (int i = 0; i < 16; i++) {
        if (fabs(mv->c[i]) > eps)
            return false;
    }

    return true;
}
