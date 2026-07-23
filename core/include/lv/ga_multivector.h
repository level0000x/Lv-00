#ifndef lv_GA_MULTIVECTOR_H
#define lv_GA_MULTIVECTOR_H
/**
 * @file ga_multivector.h
 * @brief Projective Geometric Algebra (PGA) multivector public API
 *
 * @details Cl(3,0,1) algebra with 16 basis elements.
 */

#include "lv/lv.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Forward declaration for PGA multivector type. */
typedef struct lvMultiVector lvMultiVector;

/** Number of basis elements in Cl(3,0,1). */
#define GA_MV_DIM 16

/* ============================================================
 * Lifecycle
 * ============================================================ */

/**
 * @brief Create a zero multivector (all coefficients = 0).
 * @return 成功返回 multivector 指针，失败返回 NULL
 */
lv_PUBLIC_API lvMultiVector *ga_mv_create(void);

/**
 * @brief Free a multivector and set pointer to NULL.
 * @param mv 要释放的 multivector 指针
 */
lv_PUBLIC_API void ga_mv_destroy(lvMultiVector *mv);

/**
 * @brief Deep copy a multivector.
 * @param src 源 multivector 指针
 * @return 成功返回拷贝的指针，失败返回 NULL
 */
lv_PUBLIC_API lvMultiVector *ga_mv_copy(const lvMultiVector *src);

/**
 * @brief Create a zero multivector (alias for ga_mv_create).
 * @return 成功返回 multivector 指针，失败返回 NULL
 */
lv_PUBLIC_API lvMultiVector *ga_mv_zero(void);

/* ============================================================
 * Coefficient access
 * ============================================================ */

/**
 * @brief Get coefficient at basis index (0..15).
 * @param mv multivector 指针
 * @param index 基索引（0 到 15）
 * @return 返回对应基的系数值
 */
lv_PUBLIC_API double ga_mv_get(const lvMultiVector *mv, int index);

/**
 * @brief Set coefficient at basis index (0..15).
 * @param mv multivector 指针
 * @param index 基索引（0 到 15）
 * @param value 要设置的系数值
 */
lv_PUBLIC_API void ga_mv_set(lvMultiVector *mv, int index, double value);

/* ============================================================
 * Grade operations
 * ============================================================ */

/**
 * @brief Return the maximum grade with non-zero coefficient, or -1.
 * @param mv multivector 指针
 * @return 返回最大非零阶数，全零时返回 -1
 */
lv_PUBLIC_API int ga_mv_grade(const lvMultiVector *mv);

/**
 * @brief Project onto a specific grade. Caller owns result.
 * @param mv multivector 指针
 * @param grade 目标阶数
 * @return 成功返回投影结果的指针，失败返回 NULL
 */
lv_PUBLIC_API lvMultiVector *ga_mv_grade_project(const lvMultiVector *mv, int grade);

/* ============================================================
 * Arithmetic operations
 * ============================================================ */

/**
 * @brief Add two multivectors. Caller owns result.
 * @param a 第一个 multivector 指针
 * @param b 第二个 multivector 指针
 * @return 成功返回和值的指针，失败返回 NULL
 */
lv_PUBLIC_API lvMultiVector *ga_mv_add(const lvMultiVector *a, const lvMultiVector *b);

/**
 * @brief Subtract b from a. Caller owns result.
 * @param a 被减 multivector 指针
 * @param b 减数 multivector 指针
 * @return 成功返回差值的指针，失败返回 NULL
 */
lv_PUBLIC_API lvMultiVector *ga_mv_sub(const lvMultiVector *a, const lvMultiVector *b);

/**
 * @brief Scale multivector by a scalar. Caller owns result.
 * @param mv multivector 指针
 * @param scalar 标量因子
 * @return 成功返回缩放结果的指针，失败返回 NULL
 */
lv_PUBLIC_API lvMultiVector *ga_mv_scale(const lvMultiVector *mv, double scalar);

/**
 * @brief Negate a multivector. Caller owns result.
 * @param mv multivector 指针
 * @return 成功返回取反结果的指针，失败返回 NULL
 */
lv_PUBLIC_API lvMultiVector *ga_mv_negate(const lvMultiVector *mv);

/* ============================================================
 * Products
 * ============================================================ */

/**
 * @brief Geometric product (simplified). Caller owns result.
 * @param a 第一个 multivector 指针
 * @param b 第二个 multivector 指针
 * @return 成功返回几何积结果的指针，失败返回 NULL
 */
lv_PUBLIC_API lvMultiVector *ga_mv_geometric_product(const lvMultiVector *a, const lvMultiVector *b);

/**
 * @brief Inner (dot) product for vectors. Returns scalar.
 * @param a 第一个 multivector 指针
 * @param b 第二个 multivector 指针
 * @return 返回内积标量值
 */
lv_PUBLIC_API double ga_mv_inner_product(const lvMultiVector *a, const lvMultiVector *b);

/**
 * @brief Outer (wedge) product. Caller owns result.
 * @param a 第一个 multivector 指针
 * @param b 第二个 multivector 指针
 * @return 成功返回外积结果的指针，失败返回 NULL
 */
lv_PUBLIC_API lvMultiVector *ga_mv_outer_product(const lvMultiVector *a, const lvMultiVector *b);

/* ============================================================
 * Norm and reverse
 * ============================================================ */

/**
 * @brief Euclidean norm (Frobenius norm over all coefficients).
 * @param mv multivector 指针
 * @return 返回欧几里得范数值
 */
lv_PUBLIC_API double ga_mv_norm(const lvMultiVector *mv);

/**
 * @brief Squared norm (avoids sqrt).
 * @param mv multivector 指针
 * @return 返回范数的平方值
 */
lv_PUBLIC_API double ga_mv_norm_squared(const lvMultiVector *mv);

/**
 * @brief Reverse (grade-dependent sign flip). Caller owns result.
 * @param mv multivector 指针
 * @return 成功返回反转结果的指针，失败返回 NULL
 */
lv_PUBLIC_API lvMultiVector *ga_mv_reverse(const lvMultiVector *mv);

/**
 * @brief Normalize to unit norm. Returns NULL if norm < eps. Caller owns result.
 * @param mv multivector 指针
 * @return 成功返回归一化结果的指针，范数过小返回 NULL
 */
lv_PUBLIC_API lvMultiVector *ga_mv_normalize(const lvMultiVector *mv);

/* ============================================================
 * Dual and sandwich
 * ============================================================ */

/**
 * @brief Hodge dual (multiply by pseudoscalar inverse). Caller owns result.
 * @param mv multivector 指针
 * @return 成功返回对偶结果的指针，失败返回 NULL
 */
lv_PUBLIC_API lvMultiVector *ga_mv_dual(const lvMultiVector *mv);

/**
 * @brief Sandwich product: R * mv * reverse(R). Caller owns result.
 * @param rotor 转子 multivector 指针
 * @param mv 被变换的 multivector 指针
 * @return 成功返回夹心积结果的指针，失败返回 NULL
 */
lv_PUBLIC_API lvMultiVector *ga_mv_sandwich(const lvMultiVector *rotor, const lvMultiVector *mv);

/* ============================================================
 * Comparison
 * ============================================================ */

/**
 * @brief Element-wise equality within tolerance eps.
 * @param a 第一个 multivector 指针
 * @param b 第二个 multivector 指针
 * @param eps 容差
 * @return 在容差内相等返回 true，否则返回 false
 */
lv_PUBLIC_API bool ga_mv_equal(const lvMultiVector *a, const lvMultiVector *b, double eps);

/**
 * @brief Check if all coefficients are within eps of zero.
 * @param mv multivector 指针
 * @param eps 容差
 * @return 所有系数在容差内返回 true，否则返回 false
 */
lv_PUBLIC_API bool ga_mv_is_zero(const lvMultiVector *mv, double eps);

/**
 * @brief Create scalar multivector with given value at grade-0.
 * @param value 标量值
 * @return 成功返回标量 multivector 的指针，失败返回 NULL
 */
lv_PUBLIC_API lvMultiVector *ga_mv_scalar(double value);

/* ============================================================
 * Convenience aliases (short names)
 * ============================================================ */
#define ga_geometric_product(a, b) ga_mv_geometric_product(a, b)
#define ga_outer_product(a, b) ga_mv_outer_product(a, b)
#define ga_inner_product(a, b) ga_mv_inner_product(a, b)
#define ga_reverse(mv) ga_mv_reverse(mv)
#define ga_norm(mv) ga_mv_norm(mv)
#define ga_norm_squared(mv) ga_mv_norm_squared(mv)
#define ga_equal(a, b, eps) ga_mv_equal(a, b, eps)

#ifdef __cplusplus
}
#endif

#endif /* lv_GA_MULTIVECTOR_H */
