#ifndef LV00_GA_MULTIVECTOR_H
#define LV00_GA_MULTIVECTOR_H
/**
 * @file ga_multivector.h
 * @brief Projective Geometric Algebra (PGA) multivector public API
 *
 * @details Cl(3,0,1) algebra with 16 basis elements.
 */

#include "lv00/lv00.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Forward declaration for PGA multivector type. */
typedef struct Lv00MultiVector Lv00MultiVector;

/** Number of basis elements in Cl(3,0,1). */
#define GA_MV_DIM 16

/* ============================================================
 * Lifecycle
 * ============================================================ */

/**
 * @brief Create a zero multivector (all coefficients = 0).
 * @return 成功返回 multivector 指针，失败返回 NULL
 */
LV00_PUBLIC_API Lv00MultiVector *ga_mv_create(void);

/**
 * @brief Free a multivector and set pointer to NULL.
 * @param mv 要释放的 multivector 指针
 */
LV00_PUBLIC_API void ga_mv_free(Lv00MultiVector *mv);

/**
 * @brief Deep copy a multivector.
 * @param src 源 multivector 指针
 * @return 成功返回拷贝的指针，失败返回 NULL
 */
LV00_PUBLIC_API Lv00MultiVector *ga_mv_copy(const Lv00MultiVector *src);

/**
 * @brief Create a zero multivector (alias for ga_mv_create).
 * @return 成功返回 multivector 指针，失败返回 NULL
 */
LV00_PUBLIC_API Lv00MultiVector *ga_mv_zero(void);

/* ============================================================
 * Coefficient access
 * ============================================================ */

/**
 * @brief Get coefficient at basis index (0..15).
 * @param mv multivector 指针
 * @param index 基索引（0 到 15）
 * @return 返回对应基的系数值
 */
LV00_PUBLIC_API double ga_mv_get(const Lv00MultiVector *mv, int index);

/**
 * @brief Set coefficient at basis index (0..15).
 * @param mv multivector 指针
 * @param index 基索引（0 到 15）
 * @param value 要设置的系数值
 */
LV00_PUBLIC_API void ga_mv_set(Lv00MultiVector *mv, int index, double value);

/* ============================================================
 * Grade operations
 * ============================================================ */

/**
 * @brief Return the maximum grade with non-zero coefficient, or -1.
 * @param mv multivector 指针
 * @return 返回最大非零阶数，全零时返回 -1
 */
LV00_PUBLIC_API int ga_mv_grade(const Lv00MultiVector *mv);

/**
 * @brief Project onto a specific grade. Caller owns result.
 * @param mv multivector 指针
 * @param grade 目标阶数
 * @return 成功返回投影结果的指针，失败返回 NULL
 */
LV00_PUBLIC_API Lv00MultiVector *ga_mv_grade_project(const Lv00MultiVector *mv, int grade);

/* ============================================================
 * Arithmetic operations
 * ============================================================ */

/**
 * @brief Add two multivectors. Caller owns result.
 * @param a 第一个 multivector 指针
 * @param b 第二个 multivector 指针
 * @return 成功返回和值的指针，失败返回 NULL
 */
LV00_PUBLIC_API Lv00MultiVector *ga_mv_add(const Lv00MultiVector *a, const Lv00MultiVector *b);

/**
 * @brief Subtract b from a. Caller owns result.
 * @param a 被减 multivector 指针
 * @param b 减数 multivector 指针
 * @return 成功返回差值的指针，失败返回 NULL
 */
LV00_PUBLIC_API Lv00MultiVector *ga_mv_sub(const Lv00MultiVector *a, const Lv00MultiVector *b);

/**
 * @brief Scale multivector by a scalar. Caller owns result.
 * @param mv multivector 指针
 * @param scalar 标量因子
 * @return 成功返回缩放结果的指针，失败返回 NULL
 */
LV00_PUBLIC_API Lv00MultiVector *ga_mv_scale(const Lv00MultiVector *mv, double scalar);

/**
 * @brief Negate a multivector. Caller owns result.
 * @param mv multivector 指针
 * @return 成功返回取反结果的指针，失败返回 NULL
 */
LV00_PUBLIC_API Lv00MultiVector *ga_mv_negate(const Lv00MultiVector *mv);

/* ============================================================
 * Products
 * ============================================================ */

/**
 * @brief Geometric product (simplified). Caller owns result.
 * @param a 第一个 multivector 指针
 * @param b 第二个 multivector 指针
 * @return 成功返回几何积结果的指针，失败返回 NULL
 */
LV00_PUBLIC_API Lv00MultiVector *ga_mv_geometric_product(const Lv00MultiVector *a,
                                                          const Lv00MultiVector *b);

/**
 * @brief Inner (dot) product for vectors. Returns scalar.
 * @param a 第一个 multivector 指针
 * @param b 第二个 multivector 指针
 * @return 返回内积标量值
 */
LV00_PUBLIC_API double ga_mv_inner_product(const Lv00MultiVector *a, const Lv00MultiVector *b);

/**
 * @brief Outer (wedge) product. Caller owns result.
 * @param a 第一个 multivector 指针
 * @param b 第二个 multivector 指针
 * @return 成功返回外积结果的指针，失败返回 NULL
 */
LV00_PUBLIC_API Lv00MultiVector *ga_mv_outer_product(const Lv00MultiVector *a,
                                                      const Lv00MultiVector *b);

/* ============================================================
 * Norm and reverse
 * ============================================================ */

/**
 * @brief Euclidean norm (Frobenius norm over all coefficients).
 * @param mv multivector 指针
 * @return 返回欧几里得范数值
 */
LV00_PUBLIC_API double ga_mv_norm(const Lv00MultiVector *mv);

/**
 * @brief Squared norm (avoids sqrt).
 * @param mv multivector 指针
 * @return 返回范数的平方值
 */
LV00_PUBLIC_API double ga_mv_norm_squared(const Lv00MultiVector *mv);

/**
 * @brief Reverse (grade-dependent sign flip). Caller owns result.
 * @param mv multivector 指针
 * @return 成功返回反转结果的指针，失败返回 NULL
 */
LV00_PUBLIC_API Lv00MultiVector *ga_mv_reverse(const Lv00MultiVector *mv);

/**
 * @brief Normalize to unit norm. Returns NULL if norm < eps. Caller owns result.
 * @param mv multivector 指针
 * @return 成功返回归一化结果的指针，范数过小返回 NULL
 */
LV00_PUBLIC_API Lv00MultiVector *ga_mv_normalize(const Lv00MultiVector *mv);

/* ============================================================
 * Dual and sandwich
 * ============================================================ */

/**
 * @brief Hodge dual (multiply by pseudoscalar inverse). Caller owns result.
 * @param mv multivector 指针
 * @return 成功返回对偶结果的指针，失败返回 NULL
 */
LV00_PUBLIC_API Lv00MultiVector *ga_mv_dual(const Lv00MultiVector *mv);

/**
 * @brief Sandwich product: R * mv * reverse(R). Caller owns result.
 * @param rotor 转子 multivector 指针
 * @param mv 被变换的 multivector 指针
 * @return 成功返回夹心积结果的指针，失败返回 NULL
 */
LV00_PUBLIC_API Lv00MultiVector *ga_mv_sandwich(const Lv00MultiVector *rotor,
                                                  const Lv00MultiVector *mv);

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
LV00_PUBLIC_API bool ga_mv_equal(const Lv00MultiVector *a, const Lv00MultiVector *b, double eps);

/**
 * @brief Check if all coefficients are within eps of zero.
 * @param mv multivector 指针
 * @param eps 容差
 * @return 所有系数在容差内返回 true，否则返回 false
 */
LV00_PUBLIC_API bool ga_mv_is_zero(const Lv00MultiVector *mv, double eps);

/**
 * @brief Create scalar multivector with given value at grade-0.
 * @param value 标量值
 * @return 成功返回标量 multivector 的指针，失败返回 NULL
 */
LV00_PUBLIC_API Lv00MultiVector *ga_mv_scalar(double value);

/* ============================================================
 * Convenience aliases (short names)
 * ============================================================ */
#define ga_geometric_product(a, b)  ga_mv_geometric_product(a, b)
#define ga_outer_product(a, b)      ga_mv_outer_product(a, b)
#define ga_inner_product(a, b)      ga_mv_inner_product(a, b)
#define ga_reverse(mv)              ga_mv_reverse(mv)
#define ga_norm(mv)                 ga_mv_norm(mv)
#define ga_norm_squared(mv)         ga_mv_norm_squared(mv)
#define ga_equal(a, b, eps)         ga_mv_equal(a, b, eps)

#ifdef __cplusplus
}
#endif

#endif /* LV00_GA_MULTIVECTOR_H */
