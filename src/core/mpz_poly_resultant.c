/**
 * @file mpz_poly_resultant.c
 * @brief 多精度多项式结式计算
 *
 * @details 计算多项式的结式（resultant），用于代数数加减乘运算中
 *          极小多项式的推导。限制多项式次数不超过 4 以防止组合爆炸。
 *
 * @author Lv-00 Project
 * @version 3.2.0
 */

#include "mpz_poly.h"
#include "lv00_internal.h"
#include "lv00_utils.h"
#include <stdlib.h>
#include <string.h>

/* ============================================================
 * 模块级常量定义
 * ============================================================ */

/* 多项式次数上限：输入多项式的最高允许次数，防止组合爆炸 */
#define MPZ_RES_INPUT_DEGREE_MAX         4

/* 结果多项式次数硬上限：插值结果多项式最多达到20次 */
#define MPZ_RES_RESULT_DEGREE_CAP        20

/* Bareiss 行列式算法的约定值和初始值 */
#define MPZ_RES_EMPTY_DETERMINANT        1   /* 空方阵的行列式约定值 */
#define MPZ_RES_INITIAL_PIVOT            1   /* Bareiss算法初始主元 */
#define MPZ_RES_SIGN_POSITIVE            1   /* 行列式符号跟踪 */

/* 多项式幂次初始化常量 */
#define MPZ_RES_POWER_INIT               1   /* x^0 / y^0 的初始值 */
#define MPZ_RES_NEWTON_CONST_TERM        1   /* 牛顿插值初始常数项 */

/* 内部数学常量 */
#define MPZ_RES_EVEN_PARITY_CHECK        2   /* 奇偶性检查模数 */
#define MPZ_RES_NEGATIVE_SIGN           (-1) /* 负号标记 */

/* ------------------------------------------------------------------ */
/*  内部：双变量多项式表示                                           */
/*  双变量多项式 f(x,y) 以"以 y 为变量、按 x 的幂次展开"的形式存储：  */
/*  f(x,y) = sum_{i=0}^{deg_x} coeffs[i](y) * x^i，                      */
/*  其中每个 coeffs[i] 是一个以 y 为变量的一元多项式。               */
/* ------------------------------------------------------------------ */

/*
 * 【双变量多项式内部表示】
 *
 * 双变量多项式 f(x,y) 以"以 y 为变量、按 x 的幂次展开"的形式存储：
 *
 *   f(x,y) = sum_{i=0}^{deg_x} coeffs[i](y) * x^i
 *
 * 其中每个 coeffs[i] 是一个以 y 为变量的一元多项式 (mpz_poly_t)。
 * 这种表示方式便于：
 *   - 在固定 x 取值后将多项式退化为 y 的一元多项式，
 *     从而构造西尔维斯特矩阵
 *   - 将 (x-y)^k 展开为关于 x 和 y 的双变量多项式
 */

typedef struct {
    mpz_poly_t *coeffs;  /* Array of polynomials in y, indexed by x-degree */
    int deg_x;           /* Degree in x */
} BivariatePoly;

/**
 * @brief 初始化双变量多项式
 *
 * 分配 double 变量多项式的系数数组（每个系数为 y 的一元多项式）。
 * 当 deg_x < 0 时，不分配系数数组（表示空多项式）。
 *
 * @param bp    双变量多项式指针
 * @param deg_x x 的次数
 * @return 0 成功，-1 内存分配失败
 */
static int bivariate_poly_init(BivariatePoly *bp, int deg_x) {
    bp->deg_x = deg_x;
    if (deg_x >= 0) {
        bp->coeffs = lv00_malloc((deg_x + 1) * sizeof(mpz_poly_t));
        if (!bp->coeffs) {
            bp->deg_x = -1;
            return -1;
        }
        for (int i = 0; i <= deg_x; i++) {
            mpz_poly_init(&bp->coeffs[i]);
        }
    } else {
        bp->coeffs = NULL;
    }
    return 0;
}

/**
 * @brief 清理双变量多项式，释放所有关联内存
 *
 * 依次清理每个系数（y 的一元多项式）、释放系数数组。
 * 可安全传入已清理或空的多项式。
 *
 * @param bp 双变量多项式指针
 */
static void bivariate_poly_clear(BivariatePoly *bp) {
    if (bp->coeffs) {
        for (int i = 0; i <= bp->deg_x; i++) {
            mpz_poly_clear(&bp->coeffs[i]);
        }
        lv00_free((void**)&bp->coeffs);
    }
    bp->coeffs = NULL;
    bp->deg_x = -1;
}

/**
 * @brief 设置双变量多项式中指定 x 幂次的系数
 *
 * 将 poly_in_y（一个关于 y 的一元多项式）深拷贝到 bp 中 x_pow 对应的位置。
 * 若 x_pow 越界则静默忽略。
 *
 * @param bp         双变量多项式指针
 * @param x_pow      目标 x 的幂次
 * @param poly_in_y  作为该幂次系数的 y 元多项式
 */
static void bivariate_poly_set_coeff(BivariatePoly *bp, int x_pow, const mpz_poly_t *poly_in_y) {
    if (x_pow < 0 || x_pow > bp->deg_x) return;
    mpz_poly_set(&bp->coeffs[x_pow], poly_in_y);
}

/* ------------------------------------------------------------------ */
/*  内部：矩阵运算（用于行列式计算）                                 */
/* ------------------------------------------------------------------ */

/*
 * 【多精度整数矩阵 —— 用于行列式计算】
 *
 * MPZMatrix 以行优先（row-major）方式存储 GMP 多精度整数矩阵。
 * 提供初始化、清理、按行列索引访问等基本操作。
 * 矩阵初始化时进行整数溢出检查（rows * cols 不超过 size_t 范围）。
 */

/* 简化的矩阵结构，用于行列式计算 */
typedef struct {
    mpz_t *data;    /* 行优先存储：data[row * cols + col] */
    int rows;
    int cols;
} MPZMatrix;

/**
 * @brief 初始化多精度整数矩阵
 *
 * 以行优先（row-major）方式分配存储空间。自动进行 rows*cols 整数溢出检查。
 * 若溢出或 rows/cols 非正，则创建空矩阵（data = NULL）。
 * 所有元素初始化为 0。
 *
 * @param m    矩阵指针
 * @param rows 行数
 * @param cols 列数
 */
static void mpz_matrix_init(MPZMatrix *m, int rows, int cols) {
    if (rows <= 0 || cols <= 0) {
        m->rows = m->cols = 0;
        m->data = NULL;
        return;
    }
    /* 防止 rows * cols 整数溢出 */
    size_t total = (size_t)rows * (size_t)cols;
    if (total / (size_t)rows != (size_t)cols) {
        /* 溢出 */
        m->rows = m->cols = 0;
        m->data = NULL;
        return;
    }
    m->rows = rows;
    m->cols = cols;
    m->data = lv00_malloc(total * sizeof(mpz_t));
    if (!m->data) {
        m->rows = m->cols = 0;
        return;
    }
    for (size_t i = 0; i < total; i++) {
        mpz_init(m->data[i]);
    }
}

/**
 * @brief 清理矩阵，释放所有 mpz_t 元素和存储空间
 *
 * @param m 矩阵指针
 */
static void mpz_matrix_clear(MPZMatrix *m) {
    int total = m->rows * m->cols;
    for (int i = 0; i < total; i++) {
        mpz_clear(m->data[i]);
    }
    lv00_free((void**)&m->data);
    m->data = NULL;
}

/**
 * @brief 获取矩阵中指定行列的可变元素指针
 *
 * @param m   矩阵指针
 * @param row 行索引（从0开始）
 * @param col 列索引（从0开始）
 * @return mpz_t 指针
 */
static inline mpz_t* mpz_matrix_at(MPZMatrix *m, int row, int col) {
    return &m->data[row * m->cols + col];
}

/**
 * @brief 获取矩阵中指定行列的只读元素指针
 *
 * @param m   矩阵指针
 * @param row 行索引（从0开始）
 * @param col 列索引（从0开始）
 * @return const mpz_t 指针
 */
static inline const mpz_t* mpz_matrix_at_const(const MPZMatrix *m, int row, int col) {
    return &m->data[row * m->cols + col];
}

/**
 * @brief Bareiss 算法计算确切整数行列式
 *
 * 时间复杂度 O(n^3)，无需分数运算。所有除法均为确切整除（无余数）。
 *
 * 【Bareiss 算法说明】
 * Bareiss 算法是高斯消元的分数无关（fraction-free）变体，
 * 通过对角元除以上一轮的主元（prev_pivot），确保所有中间
 * 结果保持整数运算，避免有理数算术的开销。
 *
 * 算法特点：
 *   - 时间复杂度 O(n^3)，空间复杂度 O(n^2)（需要工作副本）
 *   - 所有除法均为精确除法（无余数），由数学保证
 *   - 支持行交换以处理零主元，行交换会翻转行列式符号
 *   - n=0 时返回 1（空矩阵行列式约定）
 *   - n=1 时直接返回唯一元素
 */
static bool mpz_matrix_det_bareiss(MPZMatrix *m, mpz_t result) {
    if (m->rows != m->cols) {
        mpz_set_si(result, 0);
        return false;
    }

    int n = m->rows;
    if (n == 0) { mpz_set_ui(result, MPZ_RES_EMPTY_DETERMINANT); return true; }
    if (n == 1) { mpz_set(result, *mpz_matrix_at_const(m, 0, 0)); return true; }

    /* 创建工作副本 */
    mpz_t *a = lv00_malloc((size_t)n * (size_t)n * sizeof(mpz_t));
    if (!a) {
        mpz_set_si(result, 0);
        return false;
    }
    for (int i = 0; i < n * n; i++) mpz_init_set(a[i], m->data[i]);

    mpz_t pivot, temp, prev_pivot;
    mpz_inits(pivot, temp, prev_pivot, NULL);
    mpz_set_ui(prev_pivot, MPZ_RES_INITIAL_PIVOT);

    int sign = MPZ_RES_SIGN_POSITIVE;  /* 跟踪行交换的符号变化 */

    for (int k = 0; k < n - 1; k++) {
        mpz_set(pivot, a[k * n + k]);
        if (mpz_sgn(pivot) == 0) {
            /* 查找非零主元 */
            int swap_row = -1;
            for (int i = k + 1; i < n; i++) {
                if (mpz_sgn(a[i * n + k]) != 0) { swap_row = i; break; }
            }
            if (swap_row < 0) { mpz_set_ui(result, 0); goto cleanup; }
            /* 交换第 k 行和 swap_row 行 */
            for (int j = k; j < n; j++) {
                mpz_swap(a[k * n + j], a[swap_row * n + j]);
            }
            sign = -sign;  /* 行交换改变行列式符号 */
            mpz_set(pivot, a[k * n + k]);
        }

        for (int i = k + 1; i < n; i++) {
            for (int j = k + 1; j < n; j++) {
                /* a[i][j] = (a[i][j]*a[k][k] - a[i][k]*a[k][j]) / prev_pivot */
                mpz_mul(temp, a[i * n + j], pivot);
                mpz_mul(a[i * n + j], a[i * n + k], a[k * n + j]);
                mpz_sub(temp, temp, a[i * n + j]);
                mpz_fdiv_q(a[i * n + j], temp, prev_pivot); /* exact division */
            }
        }
        mpz_set(prev_pivot, pivot);
    }

    mpz_set(result, a[(n - 1) * n + (n - 1)]);
    if (sign < 0) mpz_neg(result, result);

cleanup:
    mpz_clears(pivot, temp, prev_pivot, NULL);
    for (int i = 0; i < n * n; i++) mpz_clear(a[i]);
    lv00_free((void**)&a);
    return true;
}

/**
 * @brief 使用 Bareiss 算法计算矩阵的行列式（O(n^3)）
 *
 * 调用 mpz_matrix_det_bareiss 实现确切整数行列式计算。
 *
 * @param m   方阵指针
 * @param det 输出：行列式值（mpz_t 类型）
 * @return true 计算成功
 */
static bool mpz_matrix_det(MPZMatrix *m, mpz_t det) {
    return mpz_matrix_det_bareiss(m, det);
}

/* ------------------------------------------------------------------ */
/*  内部：构造西尔维斯特矩阵，用于结式计算                         */
/*  给定 f(x,y)（关于 y 的次数为 m）和 g(x,y)（关于 y 的次数为 n），*/
/*  西尔维斯特矩阵大小为 (m+n) x (m+n)。                           */
/*  每个元素是关于 x 的多项式（y^k 的系数）。                     */
/* ------------------------------------------------------------------ */

/*
 * 【西尔维斯特矩阵与结式计算方法】
 *
 * 对于两个双变量多项式 f(x,y) 和 g(x,y)，计算关于 y 的结式，
 * 结果是一个关于 x 的多项式。
 *
 * 核心步骤：
 *   1. 构造西尔维斯特矩阵（size = m+n 阶方阵）：
 *      前 n 行填充 f 的系数（按 y 的幂次排列并移位），
 *      后 m 行填充 g 的系数。
 *
 *   2. 在若干个 x 取值处分别计算矩阵的行列式：
 *      对每个选定的 x = x_k，将 f 和 g 的系数（均为 x 的多项式）
 *      在 x_k 处估值，得到数值矩阵，再计算行列式 det(S(x_k))。
 *
 *   3. 多项式插值：
 *      使用牛顿均差法（Newton's divided differences），
 *      将点 (x_k, det(S(x_k))) 插值还原为关于 x 的结式多项式。
 *
 * 限制与防护：
 *   - x 取值以 0 为中心均匀分布（i - res_deg_bound/2）
 *   - 结果多项式的次数上限硬编码为 20，防止组合爆炸
 *   - 整数除法假设插值点上恰好整除，否则结果精度不保证
 */

/*
 * 计算两个双变量多项式关于 y 的结式。
 * 结果是一个关于 x 的多项式。
 *
 * 西尔维斯特矩阵构造方法：
 * 设 f = sum_{i=0}^{m} a_i(x) * y^i  (关于 y 的次数为 m)
 * 设 g = sum_{i=0}^{n} b_i(x) * y^i  (关于 y 的次数为 n)
 *
 * 西尔维斯特矩阵 S 大小为 (m+n) x (m+n)：
 * 第 0 行:     [a_m, a_{m-1}, ..., a_0, 0, ..., 0]
 * 第 1 行:     [0, a_m, ..., a_1, a_0, 0, ..., 0]
 * ... (n 行 f 系数，逐行移位)
 * 第 n 行:     [b_n, b_{n-1}, ..., b_0, 0, ..., 0]
 * 第 n+1 行:   [0, b_n, ..., b_1, b_0, 0, ..., 0]
 * ... (m 行 g 系数，逐行移位)
 */

/* 计算两个双变量多项式关于 y 的结式 */
/* 返回存储在 resultant 中的关于 x 的多项式 */

/*
 * 【双变量结式计算核心函数】
 *
 * 计算两个双变量多项式 f(x,y) 和 g(x,y) 关于 y 的结式 Res_y(f, g)，
 * 结果是一个仅关于 x 的一元多项式。
 *
 * 计算流程：
 *   1. 边界检查：若任一多项式在 y 上的次数为负，返回空多项式
 *   2. 大小为零（两个多项式均为常数）：结式 = 1
 *   3. 估计结式多项式的次数上限（取 f.deg_x * n + g.deg_x * m + 1，截断到 20）
 *   4. 在 res_deg_bound 个 x 取值处逐点计算西尔维斯特矩阵的行列式
 *   5. 使用牛顿均差法（Newton's divided differences）插值得到结式多项式
 *   6. 清理前导零系数（trim leading zeros）
 *
 * @param f         双变量多项式 f(x,y)
 * @param g         双变量多项式 g(x,y)
 * @param deg_f_y   f 在 y 上的次数
 * @param deg_g_y   g 在 y 上的次数
 * @param resultant 输出：Res_y(f, g) 作为 x 的一元多项式
 * @return true 表示计算成功，false 表示内存分配失败
 */
static bool compute_bivariate_resultant(
    const BivariatePoly *f,  /* f(x,y) - x 和 y 的二元多项式 */
    const BivariatePoly *g,  /* g(x,y) - x 和 y 的二元多项式 */
    int deg_f_y,             /* f 关于 y 的次数 */
    int deg_g_y,             /* g 关于 y 的次数 */
    mpz_poly_t *resultant)   /* 输出：关于 x 的一元多项式 */
{
    bool ret = false;

    if (deg_f_y < 0 || deg_g_y < 0) {
        mpz_poly_init(resultant);
        resultant->degree = -1;
        return false;
    }

    int m = deg_f_y;
    int n = deg_g_y;
    int size = m + n;

    if (size == 0) {
        /* 两者均为常数，结式 = 1 */
        mpz_poly_init(resultant);
        resultant->degree = 0;
        resultant->coeffs = lv00_malloc(sizeof(mpz_t));
        if (!resultant->coeffs) {
            resultant->degree = -1;
            return false;
        }
        mpz_init_set_si(resultant->coeffs[0], MPZ_RES_EMPTY_DETERMINANT);
        return true;
    }

    /* 结式关于 x 的次数最多为 deg_x(f)*deg_y(g) + deg_y(f)*deg_x(g) */
    /* 为简化计算，估计上限 */
    int res_deg_bound = (f->deg_x + 1) * n + (g->deg_x + 1) * m + 1;
    /* 安全上限：防止插值点过多导致计算量爆炸。
     * 超过上限时返回 false，而非静默产生错误结果。 */
    if (res_deg_bound > MPZ_RES_RESULT_DEGREE_CAP) {
        mpz_poly_init(resultant);
        resultant->degree = -1;
        return false;
    }

    /* 在多个 x 取值处计算结式，然后进行插值 */
    mpz_t *x_vals = lv00_malloc(res_deg_bound * sizeof(mpz_t));
    mpz_t *y_vals = lv00_malloc(res_deg_bound * sizeof(mpz_t));
    if (!x_vals || !y_vals) {
        lv00_free((void**)&x_vals);
        lv00_free((void**)&y_vals);
        mpz_poly_init(resultant);
        resultant->degree = -1;
        return false;
    }
    for (int i = 0; i < res_deg_bound; i++) {
        mpz_init(x_vals[i]);
        mpz_init(y_vals[i]);
        /* 使用不对称取值点（偏移 0.5），避免当 res_deg_bound 为偶数时
         * 以 0 为中心对称分布导致 x_vals[i] == x_vals[i-j] 重复 */
        mpz_set_si(x_vals[i], i - res_deg_bound / 2); /* 以 0 为中心 */
        if (i > 0 && mpz_cmp(x_vals[i], x_vals[i-1]) == 0) {
            mpz_add_ui(x_vals[i], x_vals[i], 1); /* 消除重复 */
        }
    }

    /* 对每个 x 取值，计算 f 和 g 在该 x 处的值（得到关于 y 的一元多项式），
     * 然后计算西尔维斯特行列式 */
    for (int xi = 0; xi < res_deg_bound; xi++) {
        /* 计算 f(x_val, y) 作为关于 y 的一元多项式 */
        mpz_t x_val;
        mpz_init_set(x_val, x_vals[xi]);

        /* 为该 x 取值构造西尔维斯特矩阵 */
        MPZMatrix S;
        mpz_matrix_init(&S, size, size);
        if (!S.data) {
            mpz_clear(x_val);
            goto cleanup_x_y_vals;
        }

        /* 填充来自 f 的行（n 行） */
        for (int row = 0; row < n; row++) {
            for (int col = 0; col < size; col++) {
                int y_idx = m - (col - row);
                if (y_idx < 0 || y_idx > m) {
                    mpz_set_si(*mpz_matrix_at(&S, row, col), 0);
                } else {
                    /* 计算 f 中 y^y_idx 项的系数在 x_val 处的值 */
                    mpz_t coeff_val;
                    mpz_init(coeff_val);
                    mpz_set_si(coeff_val, 0);

                    /* 对 x 的各幂次求和：sum_i f.coeffs[i](y^y_idx) * x_val^i */
                    mpz_t x_pow;
                    mpz_init_set_si(x_pow, MPZ_RES_POWER_INIT);
                    for (int xi_pow = 0; xi_pow <= f->deg_x; xi_pow++) {
                        /* 获取 f.coeffs[xi_pow] 中 y^y_idx 项的系数 */
                        mpz_t y_coeff;
                        mpz_init(y_coeff);
                        if (f->coeffs[xi_pow].degree >= y_idx && f->coeffs[xi_pow].coeffs) {
                            mpz_set(y_coeff, f->coeffs[xi_pow].coeffs[y_idx]);
                        } else {
                            mpz_set_si(y_coeff, 0);
                        }
                        mpz_t term;
                        mpz_init(term);
                        mpz_mul(term, y_coeff, x_pow);
                        mpz_add(coeff_val, coeff_val, term);
                        mpz_clear(term);
                        mpz_clear(y_coeff);

                        mpz_mul(x_pow, x_pow, x_val);
                    }
                    mpz_set(*mpz_matrix_at(&S, row, col), coeff_val);
                    mpz_clear(x_pow);
                    mpz_clear(coeff_val);
                }
            }
        }

        /* 填充来自 g 的行（m 行） */
        for (int row = 0; row < m; row++) {
            for (int col = 0; col < size; col++) {
                int y_idx = n - (col - row);
                if (y_idx < 0 || y_idx > n) {
                    mpz_set_si(*mpz_matrix_at(&S, n + row, col), 0);
                } else {
                    /* 计算 g 中 y^y_idx 项在 x_val 处的系数值 */
                    mpz_t coeff_val;
                    mpz_init(coeff_val);
                    mpz_set_si(coeff_val, 0);

                    mpz_t x_pow;
                    mpz_init_set_si(x_pow, MPZ_RES_POWER_INIT);
                    for (int xi_pow = 0; xi_pow <= g->deg_x; xi_pow++) {
                        mpz_t y_coeff;
                        mpz_init(y_coeff);
                        if (g->coeffs[xi_pow].degree >= y_idx && g->coeffs[xi_pow].coeffs) {
                            mpz_set(y_coeff, g->coeffs[xi_pow].coeffs[y_idx]);
                        } else {
                            mpz_set_si(y_coeff, 0);
                        }
                        mpz_t term;
                        mpz_init(term);
                        mpz_mul(term, y_coeff, x_pow);
                        mpz_add(coeff_val, coeff_val, term);
                        mpz_clear(term);
                        mpz_clear(y_coeff);

                        mpz_mul(x_pow, x_pow, x_val);
                    }
                    mpz_set(*mpz_matrix_at(&S, n + row, col), coeff_val);
                    mpz_clear(x_pow);
                    mpz_clear(coeff_val);
                }
            }
        }

        /* 计算行列式 */
        mpz_matrix_det(&S, y_vals[xi]);
        mpz_matrix_clear(&S);
        mpz_clear(x_val);
    }

    /* 插值得到结式多项式 */
    /* 使用牛顿均差法 */

    mpz_poly_init(resultant);

    /* 通过检查最高次非零系数确定实际次数 */
    int actual_deg = res_deg_bound - 1;

    /* 分配系数数组 */
    resultant->coeffs = lv00_malloc((actual_deg + 1) * sizeof(mpz_t));
    if (!resultant->coeffs) {
        resultant->degree = -1;
        goto cleanup_x_y_vals;
    }
    for (int i = 0; i <= actual_deg; i++) {
        mpz_init(resultant->coeffs[i]);
    }
    resultant->degree = actual_deg;

    /* 使用均差法进行插值 */
    mpz_t *div_diff = NULL;
    div_diff = lv00_malloc((actual_deg + 1) * sizeof(mpz_t));
    if (!div_diff) {
        goto cleanup_resultant;
    }
    for (int i = 0; i <= actual_deg; i++) {
        mpz_init(div_diff[i]);
        mpz_set(div_diff[i], y_vals[i]);
    }

    /* 计算均差 */
    for (int j = 1; j <= actual_deg; j++) {
        for (int i = actual_deg; i >= j; i--) {
            mpz_t num, den, diff;
            mpz_init(num);
            mpz_init(den);
            mpz_init(diff);

            mpz_sub(num, div_diff[i], div_diff[i-1]);
            mpz_sub(den, x_vals[i], x_vals[i-j]);

            /* 整数除法（对于多项式插值应该是精确的） */
            if (mpz_cmp_si(den, 0) == 0) {
                /* 除数为零说明 x_vals 中存在重复取值点，
                 * 这是插值失败的条件，应中止并返回错误 */
                mpz_clear(num);
                mpz_clear(den);
                mpz_clear(diff);
                goto cleanup_resultant;
            }
            mpz_tdiv_q(diff, num, den);
            mpz_set(div_diff[i], diff);

            mpz_clear(num);
            mpz_clear(den);
            mpz_clear(diff);
        }
    }

    /* 将牛顿形式转换为标准形式 */
    /* p(x) = d_0 + d_1*(x-x_0) + d_2*(x-x_0)*(x-x_1) + ... */

    /* 用常数项初始化 */
    mpz_set(resultant->coeffs[0], div_diff[0]);
    for (int i = 1; i <= actual_deg; i++) {
        mpz_set_si(resultant->coeffs[i], 0);
    }

    /* 逐项累加构造多项式 */
    mpz_poly_t newton_term;
    mpz_poly_init(&newton_term);

    /* newton_term 初始化为 1（对应 d_0） */
    newton_term.degree = 0;
    newton_term.coeffs = lv00_malloc(sizeof(mpz_t));
    if (!newton_term.coeffs) {
        goto cleanup_resultant;
    }
    mpz_init_set_si(newton_term.coeffs[0], MPZ_RES_NEWTON_CONST_TERM);

    for (int k = 1; k <= actual_deg; k++) {
        /* 将 newton_term 乘以 (x - x_{k-1}) */
        mpz_poly_t new_term;
        mpz_poly_init(&new_term);
        new_term.degree = newton_term.degree + 1;
        new_term.coeffs = lv00_malloc((new_term.degree + 1) * sizeof(mpz_t));
        if (!new_term.coeffs) {
            mpz_poly_clear(&new_term);
            mpz_poly_clear(&newton_term);
            goto cleanup_resultant;
        }

        /* 将所有系数初始化为 0 */
        for (int i = 0; i <= new_term.degree; i++) {
            mpz_init(new_term.coeffs[i]);
        }

        /* 乘法：(a_0 + a_1*x + ...)*(x - x_{k-1}) */
        /* = -x_{k-1}*a_0 + (a_0 - x_{k-1}*a_1)*x + ... + a_n*x^{n+1} */
        for (int i = 0; i <= newton_term.degree; i++) {
            /* 如果 i > 0，来自 a_{i-1}*x 的 x^i 项贡献 */
            if (i > 0) {
                mpz_add(new_term.coeffs[i], new_term.coeffs[i],
                        newton_term.coeffs[i-1]);
            }
            /* 来自 -x_{k-1}*a_i 的 x^i 项贡献 */
            mpz_t prod;
            mpz_init(prod);
            mpz_mul(prod, x_vals[k-1], newton_term.coeffs[i]);
            mpz_sub(new_term.coeffs[i], new_term.coeffs[i], prod);
            mpz_clear(prod);
        }

        mpz_poly_clear(&newton_term);
        newton_term = new_term;

        /* Add d_k * newton_term to resultant */
        for (int i = 0; i <= newton_term.degree && i <= actual_deg; i++) {
            mpz_t prod;
            mpz_init(prod);
            mpz_mul(prod, div_diff[k], newton_term.coeffs[i]);
            mpz_add(resultant->coeffs[i], resultant->coeffs[i], prod);
            mpz_clear(prod);
        }
    }

    mpz_poly_clear(&newton_term);

    /* Clean up leading zero coefficients */
    while (resultant->degree > 0 &&
           mpz_cmp_si(resultant->coeffs[resultant->degree], 0) == 0) {
        mpz_clear(resultant->coeffs[resultant->degree]);
        resultant->degree--;
    }

    /* 清理均差数组 */
    for (int i = 0; i <= actual_deg; i++) {
        mpz_clear(div_diff[i]);
    }
    lv00_free((void**)&div_diff);

    ret = true;

cleanup_resultant:
    if (!ret) {
        /* 错误路径：清理部分构建的结式和均差数组 */
        if (div_diff) {
            for (int i = 0; i <= actual_deg; i++) {
                mpz_clear(div_diff[i]);
            }
            lv00_free((void**)&div_diff);
        }
        if (resultant->coeffs) {
            for (int i = 0; i <= resultant->degree; i++) {
                mpz_clear(resultant->coeffs[i]);
            }
            lv00_free((void**)&resultant->coeffs);
            resultant->coeffs = NULL;
        }
        resultant->degree = -1;
    }

cleanup_x_y_vals:
    for (int i = 0; i < res_deg_bound; i++) {
        mpz_clear(x_vals[i]);
        mpz_clear(y_vals[i]);
    }
    lv00_free((void**)&x_vals);
    lv00_free((void**)&y_vals);

    return ret;
}

/* ------------------------------------------------------------------ */
/*  Public: Compute resultant for algebraic number operations          */
/* ------------------------------------------------------------------ */

/*
 * 【多项式结式公共API —— 代数数运算的极小多项式推导】
 *
 * 计算 alpha + beta 或 alpha * beta 的极小多项式（minimal polynomial），
 * 使用结式（resultant）方法。
 *
 * 数学原理：
 *   给定代数数 alpha 的极小多项式 p(y)，beta 的极小多项式 q(y)，
 *   则 alpha + beta 的极小多项式可通过计算：
 *     Res_y(p(y), q(x - y))
 *   得到。类似地，alpha * beta 的极小多项式可通过：
 *     Res_y(p(y), y^n * q(x/y))   （其中 n = deg(q)）
 *   得到。
 *
 * 实现限制：
 *   - 输入多项式的次数不超过 4，超出范围直接返回 false。
 *     此限制是为了防止西尔维斯特矩阵过大导致的组合爆炸。
 *   - 结果多项式次数最多为 m*n（两个多项式次数的乘积），
 *     但实际计算中被硬截断为 20。
 *
 * 算法流程：
 *   1. 将 p(y) 转换为双变量多项式 f(x,y) = p(y)（x 的 0 次项）
 *   2. 根据操作类型（加法/乘法）将 q(y) 转换为双变量多项式 g(x,y)：
 *      - 加法：g(x,y) = q(x - y)，展开为关于 x 和 y 的双变量形式
 *      - 乘法：g(x,y) = y^n * q(x/y)，即交换 x 和 y 的幂次
 *   3. 调用 compute_bivariate_resultant(f, g, deg_y_f, deg_y_g, result)
 *      计算关于 y 的结式，得到关于 x 的一元多项式
 *
 * @param p       alpha 的极小多项式（以 y 为变量的一元多项式）
 * @param q       beta 的极小多项式（以 y 为变量的一元多项式）
 * @param op      代数运算类型：ALG_OP_SUM（加法）或 ALG_OP_PRODUCT（乘法）
 * @param result  输出：alpha op beta 的极小多项式（调用者负责 mpz_poly_clear）
 * @return true   计算成功
 * @return false  参数无效、次数超限或内存分配失败
 */

/*
 * 使用结式计算 alpha + beta 或 alpha * beta 的最小多项式。
 *
 * 对于 alpha + beta：
 *   给定 p(y) = alpha 的最小多项式
 *   给定 q(y) = beta 的最小多项式
 *   计算 Res_y(p(y), q(x - y))
 *
 * 对于 alpha * beta：
 *   给定 p(y) = alpha 的最小多项式
 *   给定 q(y) = beta 的最小多项式
 *   计算 Res_y(p(y), y^n * q(x/y))，其中 n = deg(q)
 */

bool mpz_poly_resultant(
    const mpz_poly_t *p,       /* Minimal polynomial of alpha (in y) */
    const mpz_poly_t *q,       /* Minimal polynomial of beta (in y) */
    AlgebraicOp op,            /* SUM or PRODUCT */
    mpz_poly_t *result         /* Output: minimal polynomial of result */
) {
    if (!p || !q || !result) return false;
    if (p->degree < 0 || q->degree < 0) return false;
    if (p->degree > MPZ_RES_INPUT_DEGREE_MAX || q->degree > MPZ_RES_INPUT_DEGREE_MAX) {
        /* Out of scope for our simple implementation */
        mpz_poly_init(result);
        result->degree = -1;
        return false;
    }

    int n = q->degree;
    int m = p->degree;

    /*
     * For alpha + beta: Res_y(p(y), q(x - y))
     * We need to construct q(x - y) from q(y).
     * If q(y) = sum_{i=0}^n c_i * y^i, then
     * q(x - y) = sum_{i=0}^n c_i * (x - y)^i
     *          = sum_{i=0}^n c_i * sum_{j=0}^i binom(i,j) * x^j * (-y)^{i-j}
     *
     * For alpha * beta: Res_y(p(y), y^n * q(x/y))
     * y^n * q(x/y) = sum_{i=0}^n c_i * y^n * (x/y)^i
     *              = sum_{i=0}^n c_i * x^i * y^{n-i}
     */

    BivariatePoly f, g;

    /* f(y) = p(y) - interpreted as polynomial in y with constant coeffs in x */
    if (bivariate_poly_init(&f, 0) < 0) {
        mpz_poly_init(result);
        result->degree = -1;
        return false;
    }
    f.coeffs[0].degree = m;
    f.coeffs[0].coeffs = lv00_malloc((m + 1) * sizeof(mpz_t));
    if (!f.coeffs[0].coeffs) {
        bivariate_poly_clear(&f);
        mpz_poly_init(result);
        result->degree = -1;
        return false;
    }
    for (int i = 0; i <= m; i++) {
        mpz_init_set(f.coeffs[0].coeffs[i], p->coeffs[i]);
    }

    if (op == ALG_OP_SUM) {
        /* g(x,y) = q(x - y) */
        /* Degree in x: n, degree in y: n */
        if (bivariate_poly_init(&g, n) < 0) {
            bivariate_poly_clear(&f);
            mpz_poly_init(result);
            result->degree = -1;
            return false;
        }

        /* Initialize all coefficients to 0 */
        for (int xi = 0; xi <= n; xi++) {
            g.coeffs[xi].degree = n;
            g.coeffs[xi].coeffs = lv00_malloc((n + 1) * sizeof(mpz_t));
            if (!g.coeffs[xi].coeffs) {
                bivariate_poly_clear(&g);
                bivariate_poly_clear(&f);
                mpz_poly_init(result);
                result->degree = -1;
                return false;
            }
            for (int yi = 0; yi <= n; yi++) {
                mpz_init_set_si(g.coeffs[xi].coeffs[yi], 0);
            }
        }

        /* Expand q(x - y) = sum_{i=0}^n c_i * (x - y)^i */
        for (int i = 0; i <= n; i++) {
            /* (x - y)^i = sum_{j=0}^i binom(i,j) * x^j * (-y)^{i-j} */
            for (int j = 0; j <= i; j++) {
                int x_pow = j;
                int y_pow = i - j;

                /* binom(i, j) * (-1)^{y_pow} */
                mpz_t binom_val, sign;
                mpz_init(binom_val);
                mpz_init(sign);

                /* Compute binomial coefficient binom(i, j) */
                mpz_bin_uiui(binom_val, i, j);

                /* Sign: (-1)^{y_pow} */
                if (y_pow % MPZ_RES_EVEN_PARITY_CHECK == 0) {
                    mpz_set_si(sign, MPZ_RES_SIGN_POSITIVE);
                } else {
                    mpz_set_si(sign, MPZ_RES_NEGATIVE_SIGN);
                }

                mpz_t term;
                mpz_init(term);
                mpz_mul(term, q->coeffs[i], binom_val);
                mpz_mul(term, term, sign);

                /* Add to g.coeffs[x_pow].coeffs[y_pow] */
                mpz_add(g.coeffs[x_pow].coeffs[y_pow],
                        g.coeffs[x_pow].coeffs[y_pow], term);

                mpz_clear(term);
                mpz_clear(binom_val);
                mpz_clear(sign);
            }
        }

    } else { /* ALG_OP_PRODUCT */
        /* g(x,y) = y^n * q(x/y) = sum_{i=0}^n c_i * x^i * y^{n-i} */
        if (bivariate_poly_init(&g, n) < 0) {
            bivariate_poly_clear(&f);
            mpz_poly_init(result);
            result->degree = -1;
            return false;
        }

        for (int xi = 0; xi <= n; xi++) {
            g.coeffs[xi].degree = n;
            g.coeffs[xi].coeffs = lv00_malloc((n + 1) * sizeof(mpz_t));
            if (!g.coeffs[xi].coeffs) {
                bivariate_poly_clear(&g);
                bivariate_poly_clear(&f);
                mpz_poly_init(result);
                result->degree = -1;
                return false;
            }
            for (int yi = 0; yi <= n; yi++) {
                mpz_init_set_si(g.coeffs[xi].coeffs[yi], 0);
            }
        }

        for (int i = 0; i <= n; i++) {
            int x_pow = i;
            int y_pow = n - i;

            /* g.coeffs[x_pow].coeffs[y_pow] += q->coeffs[i] */
            mpz_add(g.coeffs[x_pow].coeffs[y_pow],
                    g.coeffs[x_pow].coeffs[y_pow],
                    q->coeffs[i]);
        }
    }

    /* 计算关于 y 的结式 */
    bool success = compute_bivariate_resultant(&f, &g, m, n, result);

    bivariate_poly_clear(&f);
    bivariate_poly_clear(&g);

    return success;
}
