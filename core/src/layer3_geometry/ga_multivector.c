/**
 * @file ga_multivector.c
 * @brief Implementation of PGA multivector operations
 *
 * Implements the geometric algebra operations for Cl(3,0,1) using
 * a precomputed 16x16 multiplication table.
 *
 * The multiplication table is initialized at first use and encodes
 * the geometric product of all pairs of basis blades:
 *   e_i * e_j = table[i*16 + j].sign * e_{table[i*16 + j].result_index}
 *
 * Blade ordering for Cl(3,0,1) (16 blades):
 *   Index 0:  1      (grade 0)
 *   Index 1:  e1     (grade 1)
 *   Index 2:  e2     (grade 1)
 *   Index 3:  e3     (grade 1)
 *   Index 4:  e0     (grade 1, null basis, e0^2 = 0)
 *   Index 5:  e12    (grade 2)
 *   Index 6:  e13    (grade 2)
 *   Index 7:  e03    (grade 2)
 *   Index 8:  e23    (grade 2)
 *   Index 9:  e023   (grade 2)
 *   Index 10: e123   (grade 3)
 *   Index 11: e0123  (grade 3)
 *   Index 12: e013   (grade 3)
 *   Index 13: e0234  (grade 3)
 *   Index 14: e01234 (grade 4)
 *   Index 15: e1234  (grade 4)
 *
 * Signature: e1^2 = e2^2 = e3^2 = +1, e0^2 = 0
 *
 * @version 1.0.0
 */

#include "ga_multivector.h"
#include "lv00_utils.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/* ========================================================================
 * Internal helpers
 * ======================================================================== */

/** @brief Cl(3,0,1) 中每个基 blade 索引对应的阶数（grade） */
static const int g_blade_grade[GA_MV_DIM] = {
    0,  /* 0:  1      */
    1,  /* 1:  e1     */
    1,  /* 2:  e2     */
    1,  /* 3:  e3     */
    1,  /* 4:  e0     */
    2,  /* 5:  e12    */
    2,  /* 6:  e13    */
    2,  /* 7:  e03    */
    2,  /* 8:  e23    */
    2,  /* 9:  e023   */
    3,  /* 10: e123   */
    3,  /* 11: e0123  */
    3,  /* 12: e013   */
    3,  /* 13: e0234  */
    4,  /* 14: e01234 */
    4   /* 15: e1234  */
};

/** @brief Cl(3,0,1) 中每个基 blade 索引对应的名称字符串 */
static const char *g_blade_names[GA_MV_DIM] = {
    "1",       "e1",    "e2",    "e3",    "e0",
    "e12",     "e13",   "e03",   "e23",   "e023",
    "e123",    "e0123", "e013",  "e0234", "e01234",
    "e1234"
};

/**
 * @brief 计算基向量子集的规范 blade 索引
 *
 * 使用位掩码表示：bit 0 = e1, bit 1 = e2, bit 2 = e3, bit 3 = e0。
 * 通过输出参数返回规范索引和符号。
 */
static void blade_canonical(int e1, int e2, int e3, int e0,
                             int *out_index, int *out_sign) {
    int bits[4] = {e1, e2, e3, e0};
    int index = 0;
    int sign = 1;
    int swaps = 0;

    /* Count set bits to determine the canonical index */
    for (int i = 0; i < 4; i++) {
        if (bits[i]) {
            /* Count how many set bits are at positions < i */
            for (int j = 0; j < i; j++) {
                if (bits[j]) swaps++;
            }
        }
    }

    /* Each swap introduces a sign flip */
    if (swaps % 2 != 0) sign = -1;

    /* Compute canonical index from bitmask */
    /* Bit layout: e1=bit0, e2=bit1, e3=bit2, e0=bit3 */
    int mask = (e1 ? 1 : 0) | (e2 ? 2 : 0) | (e3 ? 4 : 0) | (e0 ? 8 : 0);

    /* Map bitmask to blade index */
    /* 0b0000 = 1      -> 0
     * 0b0001 = e1     -> 1
     * 0b0010 = e2     -> 2
     * 0b0011 = e12    -> 5
     * 0b0100 = e3     -> 3
     * 0b0101 = e13    -> 6
     * 0b0110 = e23    -> 8
     * 0b0111 = e123   -> 10
     * 0b1000 = e0     -> 4
     * 0b1001 = e01=e03 -> 7
     * 0b1010 = e02=e023 -> 9
     * 0b1011 = e012=e0123 -> 11
     * 0b1100 = e03    -> 7 (already handled above, but e03 = e30 reversed)
     * 0b1101 = e013   -> 12
     * 0b1110 = e0234  -> 13
     * 0b1111 = e01234 -> 14
     */
    static const int mask_to_index[16] = {
        0,   /* 0b0000: 1      */
        1,   /* 0b0001: e1     */
        2,   /* 0b0010: e2     */
        5,   /* 0b0011: e12    */
        3,   /* 0b0100: e3     */
        6,   /* 0b0101: e13    */
        8,   /* 0b0110: e23    */
        10,  /* 0b0111: e123   */
        4,   /* 0b1000: e0     */
        7,   /* 0b1001: e01=e03 */
        9,   /* 0b1010: e02=e023 */
        11,  /* 0b1011: e012=e0123 */
        7,   /* 0b1100: e03 (same as e01) -- but sign differs */
        12,  /* 0b1101: e013   */
        13,  /* 0b1110: e0234  */
        14   /* 0b1111: e01234 */
    };

    /* Handle the e03 case (mask=0b1100=12): e30 = -e03 */
    if (mask == 12) {
        /* e3*e0 = -e0*e3 = -e03, so index 7 with sign flip */
        *out_index = 7;
        *out_sign = sign * (-1);
        return;
    }

    *out_index = mask_to_index[mask];
    *out_sign = sign;
}

/**
 * @brief 获取 blade 索引对应的位掩码
 *
 * 返回该 blade 中包含哪些基向量。
 */
static int blade_to_mask(int index) {
    /* Inverse of mask_to_index */
    static const int index_to_mask[GA_MV_DIM] = {
        0,   /* 0:  1      */
        1,   /* 1:  e1     */
        2,   /* 2:  e2     */
        4,   /* 3:  e3     */
        8,   /* 4:  e0     */
        3,   /* 5:  e12    */
        5,   /* 6:  e13    */
        9,   /* 7:  e03    */
        6,   /* 8:  e23    */
        10,  /* 9:  e023   */
        7,   /* 10: e123   */
        11,  /* 11: e0123  */
        13,  /* 12: e013   */
        14,  /* 13: e0234  */
        15,  /* 14: e01234 */
        -1   /* 15: reserved (e1234 not representable in 4-bit mask) */
    };
    if (index >= 0 && index < GA_MV_DIM) return index_to_mask[index];
    return 0;
}

/**
 * @brief 计算两个位掩码合并时的符号
 *
 * 当计算 e_A * e_B 时，符号取决于将结果规范化的基向量交换次数。
 */
static int merge_sign(int mask_a, int mask_b) {
    int swaps = 0;
    int temp = mask_b;
    while (temp) {
        int bit = temp & (-temp); /* lowest set bit of B */
        int bits_below = mask_a & (bit - 1);
        /* Count set bits in bits_below */
        while (bits_below) {
            swaps++;
            bits_below &= bits_below - 1;
        }
        temp &= temp - 1;
    }
    return (swaps % 2 == 0) ? 1 : -1;
}

/**
 * @brief 初始化 Cl(3,0,1) 的 16x16 乘法表
 *
 * 对每对 (i, j)，计算 e_i * e_j：
 *   1. 获取 blade i 和 j 的位掩码
 *   2. 对位掩码取异或得到结果掩码
 *   3. 根据交换次数计算符号
 *   4. 查找结果掩码的规范索引
 *   5. 应用度量：e0^2 = 0, e1^2 = e2^2 = e3^2 = +1
 */
static void init_cl301_table(GAMultTable *tbl) {
    tbl->dim = GA_MV_DIM;
    tbl->table = (GAMultEntry *)lv00_malloc(sizeof(GAMultEntry) * (size_t)(GA_MV_DIM * GA_MV_DIM));
    tbl->blade_names = (char **)lv00_malloc(sizeof(char *) * (size_t)GA_MV_DIM);

    if (!tbl->table || !tbl->blade_names) return;

    /* Copy blade names */
    for (int i = 0; i < GA_MV_DIM; i++) {
        tbl->blade_names[i] = (char *)lv00_malloc(strlen(g_blade_names[i]) + 1);
        if (tbl->blade_names[i]) {
            /* 使用安全的字符串复制函数，自动保证零终止 */
            lv00_strlcpy(tbl->blade_names[i], g_blade_names[i], strlen(g_blade_names[i]) + 1);
        }
    }

    /* Build the multiplication table */
    for (int i = 0; i < GA_MV_DIM; i++) {
        for (int j = 0; j < GA_MV_DIM; j++) {
            GAMultEntry entry;
            entry.result_index = 0;
            entry.sign = 0;

            int mask_i = blade_to_mask(i);
            int mask_j = blade_to_mask(j);

            /* XOR gives the result basis vectors */
            int mask_result = mask_i ^ mask_j;

            /* Compute sign from swap count */
            int sign = merge_sign(mask_i, mask_j);

            /* Apply metric: if a basis vector appears in both i and j,
             * it gets squared. e0^2 = 0, e1^2 = e2^2 = e3^2 = +1 */
            int common = mask_i & mask_j;
            int metric_sign = 1;
            int is_zero = 0;

            while (common) {
                int bit = common & (-common);
                if (bit == 8) {
                    /* e0^2 = 0: the entire product is zero */
                    is_zero = 1;
                }
                /* e1^2 = e2^2 = e3^2 = +1, so metric_sign stays 1 */
                common &= common - 1;
            }

            if (is_zero) {
                entry.result_index = 0;
                entry.sign = 0;
            } else {
                /* Find canonical index for result mask */
                int result_idx = 0;
                int canon_sign = 1;

                /* Convert mask to canonical blade index */
                /* Use the same mapping as blade_canonical */
                int e1 = (mask_result & 1) ? 1 : 0;
                int e2 = (mask_result & 2) ? 1 : 0;
                int e3 = (mask_result & 4) ? 1 : 0;
                int e0 = (mask_result & 8) ? 1 : 0;
                blade_canonical(e1, e2, e3, e0, &result_idx, &canon_sign);

                entry.result_index = result_idx;
                entry.sign = sign * canon_sign * metric_sign;
            }

            tbl->table[i * GA_MV_DIM + j] = entry;
        }
    }
}

/* ========================================================================
 * 基本多重向量操作
 * ======================================================================== */

/**
 * @brief 创建零多重向量
 * @return 新分配的零多重向量，失败返回 NULL
 */
Lv00MultiVector *ga_mv_zero(void) {
    Lv00MultiVector *mv = (Lv00MultiVector *)lv00_malloc(sizeof(Lv00MultiVector));
    if (!mv) return NULL;
    memset(mv, 0, sizeof(Lv00MultiVector));
    mv->trust = 1.0;
    return mv;
}

/**
 * @brief 创建标量多重向量
 * @param value 标量值
 * @return 新分配的标量多重向量，失败返回 NULL
 */
Lv00MultiVector *ga_mv_scalar(double value) {
    Lv00MultiVector *mv = ga_mv_zero();
    if (!mv) return NULL;
    mv->components[GA_BLADE_1] = value;
    return mv;
}

Lv00MultiVector *ga_mv_copy(const Lv00MultiVector *mv) {
    if (!mv) return NULL;
    Lv00MultiVector *copy = (Lv00MultiVector *)lv00_malloc(sizeof(Lv00MultiVector));
    if (!copy) return NULL;
    memcpy(copy, mv, sizeof(Lv00MultiVector));

    /* Deep copy symbolic components */
    if (mv->is_symbolic) {
        for (int i = 0; i < GA_MV_DIM; i++) {
            if (mv->symbolic_components[i]) {
                size_t len = strlen(mv->symbolic_components[i]) + 1;
                copy->symbolic_components[i] = (char *)lv00_malloc(len);
                if (copy->symbolic_components[i]) {
                    memcpy(copy->symbolic_components[i], mv->symbolic_components[i], len);
                }
            }
        }
    }
    return copy;
}

/**
 * @brief 释放多重向量及其符号分量
 * @param mv 要释放的多重向量指针
 */
void ga_mv_free(Lv00MultiVector *mv) {
    if (!mv) return;
    if (mv->is_symbolic) {
        for (int i = 0; i < GA_MV_DIM; i++) {
            if (mv->symbolic_components[i]) {
                lv00_free((void **)&mv->symbolic_components[i]);
            }
        }
    }
    lv00_free((void **)&mv);
}

/**
 * @brief 多重向量的阶投影（提取指定阶数的分量）
 * @param mv 源多重向量
 * @param grade 目标阶数
 * @return 新分配的投影结果，失败返回 NULL
 */
Lv00MultiVector *ga_mv_grade_projection(const Lv00MultiVector *mv, int grade) {
    if (!mv) return NULL;
    Lv00MultiVector *result = ga_mv_zero();
    if (!result) return NULL;
    result->trust = mv->trust;

    for (int i = 0; i < GA_MV_DIM; i++) {
        if (g_blade_grade[i] == grade) {
            result->components[i] = mv->components[i];
        }
    }
    return result;
}

/* ========================================================================
 * 乘法表操作
 * ======================================================================== */

/**
 * @brief 创建几何代数乘法表
 * @param sig 度量签名（目前仅支持 Cl(3,0,1)）
 * @return 新分配的乘法表，失败返回 NULL
 */
GAMultTable *ga_mult_table_create(GASignature sig) {
    GAMultTable *tbl = (GAMultTable *)lv00_malloc(sizeof(GAMultTable));
    if (!tbl) return NULL;
    memset(tbl, 0, sizeof(GAMultTable));
    tbl->sig = sig;

    /* Only Cl(3,0,1) is fully supported */
    if (sig.p == 3 && sig.q == 0 && sig.r == 1) {
        init_cl301_table(tbl);
    } else {
        /* For other signatures, allocate a zero-initialized table */
        tbl->dim = GA_MV_DIM;
        tbl->table = (GAMultEntry *)lv00_malloc(sizeof(GAMultEntry) * (size_t)(GA_MV_DIM * GA_MV_DIM));
        tbl->blade_names = (char **)lv00_malloc(sizeof(char *) * (size_t)GA_MV_DIM);
        if (tbl->table) memset(tbl->table, 0, sizeof(GAMultEntry) * (size_t)(GA_MV_DIM * GA_MV_DIM));
        if (tbl->blade_names) memset(tbl->blade_names, 0, sizeof(char *) * (size_t)GA_MV_DIM);
    }

    return tbl;
}

/**
 * @brief 销毁几何代数乘法表并释放所有资源
 * @param tbl 要销毁的乘法表指针
 */
void ga_mult_table_destroy(GAMultTable *tbl) {
    if (!tbl) return;
    if (tbl->blade_names) {
        for (int i = 0; i < tbl->dim; i++) {
            if (tbl->blade_names[i]) {
                lv00_free((void **)&tbl->blade_names[i]);
            }
        }
        lv00_free((void **)&tbl->blade_names);
    }
    if (tbl->table) {
        lv00_free((void **)&tbl->table);
    }
    lv00_free((void **)&tbl);
}

/**
 * @brief 获取乘法表中指定位置的条目
 * @param tbl 乘法表
 * @param i 行索引（第一个 blade 索引）
 * @param j 列索引（第二个 blade 索引）
 * @return 乘法条目（包含结果索引和符号），越界返回零条目
 */
GAMultEntry ga_mult_table_get_entry(const GAMultTable *tbl, int i, int j) {
    GAMultEntry zero_entry = {0, 0};
    if (!tbl || !tbl->table) return zero_entry;
    if (i < 0 || i >= tbl->dim || j < 0 || j >= tbl->dim) return zero_entry;
    return tbl->table[i * tbl->dim + j];
}

/* ========================================================================
 * 几何代数运算
 * ======================================================================== */

/* Cl(3,0,1) 全局乘法表，惰性初始化 */
static GAMultTable *g_cl301_table = NULL;

static GAMultTable *get_cl301_table(void) {
    if (!g_cl301_table) {
        g_cl301_table = ga_mult_table_create(GA_CL_3_0_1);
    }
    return g_cl301_table;
}

/**
 * @brief 几何积（Geometric Product）
 * @param a 第一个多重向量
 * @param b 第二个多重向量
 * @return 新分配的几何积结果，失败返回 NULL
 */
Lv00MultiVector *ga_geometric_product(const Lv00MultiVector *a,
                                       const Lv00MultiVector *b) {
    if (!a || !b) return NULL;

    GAMultTable *tbl = get_cl301_table();
    if (!tbl) return NULL;

    Lv00MultiVector *result = ga_mv_zero();
    if (!result) return NULL;
    result->trust = (a->trust < b->trust) ? a->trust : b->trust;

    for (int i = 0; i < GA_MV_DIM; i++) {
        if (a->components[i] == 0.0) continue;
        for (int j = 0; j < GA_MV_DIM; j++) {
            if (b->components[j] == 0.0) continue;
            GAMultEntry entry = ga_mult_table_get_entry(tbl, i, j);
            if (entry.sign != 0) {
                result->components[entry.result_index] +=
                    (double)entry.sign * a->components[i] * b->components[j];
            }
        }
    }

    return result;
}

/**
 * @brief 外积（Outer Product / Wedge Product）
 * @param a 第一个多重向量
 * @param b 第二个多重向量
 * @return 新分配的外积结果，失败返回 NULL
 */
Lv00MultiVector *ga_outer_product(const Lv00MultiVector *a,
                                   const Lv00MultiVector *b) {
    if (!a || !b) return NULL;

    GAMultTable *tbl = get_cl301_table();
    if (!tbl) return NULL;

    Lv00MultiVector *result = ga_mv_zero();
    if (!result) return NULL;
    result->trust = (a->trust < b->trust) ? a->trust : b->trust;

    /* Outer product: keep only components where grade(a) + grade(b) == grade(result) */
    for (int i = 0; i < GA_MV_DIM; i++) {
        if (a->components[i] == 0.0) continue;
        int grade_a = g_blade_grade[i];
        for (int j = 0; j < GA_MV_DIM; j++) {
            if (b->components[j] == 0.0) continue;
            int grade_b = g_blade_grade[j];
            int grade_ab = grade_a + grade_b;

            GAMultEntry entry = ga_mult_table_get_entry(tbl, i, j);
            if (entry.sign != 0 && g_blade_grade[entry.result_index] == grade_ab) {
                result->components[entry.result_index] +=
                    (double)entry.sign * a->components[i] * b->components[j];
            }
        }
    }

    return result;
}

/**
 * @brief 内积（Inner Product）
 * @param a 第一个多重向量
 * @param b 第二个多重向量
 * @return 新分配的内积结果，失败返回 NULL
 */
Lv00MultiVector *ga_inner_product(const Lv00MultiVector *a,
                                   const Lv00MultiVector *b) {
    if (!a || !b) return NULL;

    GAMultTable *tbl = get_cl301_table();
    if (!tbl) return NULL;

    Lv00MultiVector *result = ga_mv_zero();
    if (!result) return NULL;
    result->trust = (a->trust < b->trust) ? a->trust : b->trust;

    /* Inner product: keep only components where |grade(a) - grade(b)| == grade(result) */
    for (int i = 0; i < GA_MV_DIM; i++) {
        if (a->components[i] == 0.0) continue;
        int grade_a = g_blade_grade[i];
        for (int j = 0; j < GA_MV_DIM; j++) {
            if (b->components[j] == 0.0) continue;
            int grade_b = g_blade_grade[j];
            int grade_diff = grade_a - grade_b;
            if (grade_diff < 0) grade_diff = -grade_diff;

            GAMultEntry entry = ga_mult_table_get_entry(tbl, i, j);
            if (entry.sign != 0 && g_blade_grade[entry.result_index] == grade_diff) {
                result->components[entry.result_index] +=
                    (double)entry.sign * a->components[i] * b->components[j];
            }
        }
    }

    return result;
}

/**
 * @brief 多重向量反转（Reverse）：对每个 k-vector 乘以 (-1)^(k*(k-1)/2)
 * @param mv 源多重向量
 * @return 新分配的反转结果，失败返回 NULL
 */
Lv00MultiVector *ga_reverse(const Lv00MultiVector *mv) {
    if (!mv) return NULL;
    Lv00MultiVector *result = ga_mv_copy(mv);
    if (!result) return NULL;

    for (int i = 0; i < GA_MV_DIM; i++) {
        int grade = g_blade_grade[i];
        /* Reverse sign: (-1)^(grade*(grade-1)/2) */
        int flip = (grade * (grade - 1)) / 2;
        if (flip % 2 != 0) {
            result->components[i] = -result->components[i];
        }
    }

    return result;
}

/**
 * @brief 多重向量阶对合（Grade Involute）：对每个 k-vector 乘以 (-1)^k
 * @param mv 源多重向量
 * @return 新分配的阶对合结果，失败返回 NULL
 */
Lv00MultiVector *ga_grade_involute(const Lv00MultiVector *mv) {
    if (!mv) return NULL;
    Lv00MultiVector *result = ga_mv_copy(mv);
    if (!result) return NULL;

    for (int i = 0; i < GA_MV_DIM; i++) {
        int grade = g_blade_grade[i];
        /* Grade involute: (-1)^grade */
        if (grade % 2 != 0) {
            result->components[i] = -result->components[i];
        }
    }

    return result;
}

/**
 * @brief 计算多重向量的范数平方 ||mv||^2
 * @param mv 多重向量
 * @return 范数平方（标量部分的值），失败返回 0.0
 */
double ga_norm_squared(const Lv00MultiVector *mv) {
    if (!mv) return 0.0;

    /* ||mv||^2 = <mv * ~mv>_0 (scalar part of mv * reverse(mv)) */
    Lv00MultiVector *rev = ga_reverse(mv);
    if (!rev) return 0.0;

    Lv00MultiVector *prod = ga_geometric_product(mv, rev);
    ga_mv_free(rev);
    if (!prod) return 0.0;

    double norm_sq = prod->components[GA_BLADE_1];
    ga_mv_free(prod);
    return norm_sq;
}

/* ========================================================================
 * 算术运算
 * ======================================================================== */

/**
 * @brief 多重向量加法
 * @param a 第一个多重向量
 * @param b 第二个多重向量
 * @return 新分配的加法结果，失败返回 NULL
 */
Lv00MultiVector *ga_mv_add(const Lv00MultiVector *a,
                            const Lv00MultiVector *b) {
    if (!a || !b) return NULL;
    Lv00MultiVector *result = ga_mv_zero();
    if (!result) return NULL;
    result->trust = (a->trust < b->trust) ? a->trust : b->trust;

    for (int i = 0; i < GA_MV_DIM; i++) {
        result->components[i] = a->components[i] + b->components[i];
    }
    return result;
}

/**
 * @brief 多重向量减法
 * @param a 第一个多重向量
 * @param b 第二个多重向量
 * @return 新分配的减法结果，失败返回 NULL
 */
Lv00MultiVector *ga_mv_sub(const Lv00MultiVector *a,
                            const Lv00MultiVector *b) {
    if (!a || !b) return NULL;
    Lv00MultiVector *result = ga_mv_zero();
    if (!result) return NULL;
    result->trust = (a->trust < b->trust) ? a->trust : b->trust;

    for (int i = 0; i < GA_MV_DIM; i++) {
        result->components[i] = a->components[i] - b->components[i];
    }
    return result;
}

/**
 * @brief 多重向量标量乘法
 * @param mv 多重向量
 * @param scale 标量系数
 * @return 新分配的缩放结果，失败返回 NULL
 */
Lv00MultiVector *ga_mv_scale(const Lv00MultiVector *mv, double scale) {
    if (!mv) return NULL;
    Lv00MultiVector *result = ga_mv_copy(mv);
    if (!result) return NULL;

    for (int i = 0; i < GA_MV_DIM; i++) {
        result->components[i] *= scale;
    }
    return result;
}
