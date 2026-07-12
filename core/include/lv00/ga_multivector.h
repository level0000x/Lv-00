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

/** @brief Create a zero multivector (all coefficients = 0). */
LV00_PUBLIC_API Lv00MultiVector *ga_mv_create(void);

/** @brief Free a multivector and set pointer to NULL. */
LV00_PUBLIC_API void ga_mv_free(Lv00MultiVector *mv);

/** @brief Deep copy a multivector. */
LV00_PUBLIC_API Lv00MultiVector *ga_mv_copy(const Lv00MultiVector *src);

/** @brief Create a zero multivector (alias for ga_mv_create). */
LV00_PUBLIC_API Lv00MultiVector *ga_mv_zero(void);

/* ============================================================
 * Coefficient access
 * ============================================================ */

/** @brief Get coefficient at basis index (0..15). */
LV00_PUBLIC_API double ga_mv_get(const Lv00MultiVector *mv, int index);

/** @brief Set coefficient at basis index (0..15). */
LV00_PUBLIC_API void ga_mv_set(Lv00MultiVector *mv, int index, double value);

/* ============================================================
 * Grade operations
 * ============================================================ */

/** @brief Return the maximum grade with non-zero coefficient, or -1. */
LV00_PUBLIC_API int ga_mv_grade(const Lv00MultiVector *mv);

/** @brief Project onto a specific grade. Caller owns result. */
LV00_PUBLIC_API Lv00MultiVector *ga_mv_grade_project(const Lv00MultiVector *mv, int grade);

/* ============================================================
 * Arithmetic operations
 * ============================================================ */

/** @brief Add two multivectors. Caller owns result. */
LV00_PUBLIC_API Lv00MultiVector *ga_mv_add(const Lv00MultiVector *a, const Lv00MultiVector *b);

/** @brief Subtract b from a. Caller owns result. */
LV00_PUBLIC_API Lv00MultiVector *ga_mv_sub(const Lv00MultiVector *a, const Lv00MultiVector *b);

/** @brief Scale multivector by a scalar. Caller owns result. */
LV00_PUBLIC_API Lv00MultiVector *ga_mv_scale(const Lv00MultiVector *mv, double scalar);

/** @brief Negate a multivector. Caller owns result. */
LV00_PUBLIC_API Lv00MultiVector *ga_mv_negate(const Lv00MultiVector *mv);

/* ============================================================
 * Products
 * ============================================================ */

/** @brief Geometric product (simplified). Caller owns result. */
LV00_PUBLIC_API Lv00MultiVector *ga_mv_geometric_product(const Lv00MultiVector *a,
                                                          const Lv00MultiVector *b);

/** @brief Inner (dot) product for vectors. Returns scalar. */
LV00_PUBLIC_API double ga_mv_inner_product(const Lv00MultiVector *a, const Lv00MultiVector *b);

/** @brief Outer (wedge) product. Caller owns result. */
LV00_PUBLIC_API Lv00MultiVector *ga_mv_outer_product(const Lv00MultiVector *a,
                                                      const Lv00MultiVector *b);

/* ============================================================
 * Norm and reverse
 * ============================================================ */

/** @brief Euclidean norm (Frobenius norm over all coefficients). */
LV00_PUBLIC_API double ga_mv_norm(const Lv00MultiVector *mv);

/** @brief Squared norm (avoids sqrt). */
LV00_PUBLIC_API double ga_mv_norm_squared(const Lv00MultiVector *mv);

/** @brief Reverse (grade-dependent sign flip). Caller owns result. */
LV00_PUBLIC_API Lv00MultiVector *ga_mv_reverse(const Lv00MultiVector *mv);

/** @brief Normalize to unit norm. Returns NULL if norm < eps. Caller owns result. */
LV00_PUBLIC_API Lv00MultiVector *ga_mv_normalize(const Lv00MultiVector *mv);

/* ============================================================
 * Dual and sandwich
 * ============================================================ */

/** @brief Hodge dual (multiply by pseudoscalar inverse). Caller owns result. */
LV00_PUBLIC_API Lv00MultiVector *ga_mv_dual(const Lv00MultiVector *mv);

/** @brief Sandwich product: R * mv * reverse(R). Caller owns result. */
LV00_PUBLIC_API Lv00MultiVector *ga_mv_sandwich(const Lv00MultiVector *rotor,
                                                  const Lv00MultiVector *mv);

/* ============================================================
 * Comparison
 * ============================================================ */

/** @brief Element-wise equality within tolerance eps. */
LV00_PUBLIC_API bool ga_mv_equal(const Lv00MultiVector *a, const Lv00MultiVector *b, double eps);

/** @brief Check if all coefficients are within eps of zero. */
LV00_PUBLIC_API bool ga_mv_is_zero(const Lv00MultiVector *mv, double eps);

/** @brief Create scalar multivector with given value at grade-0. */
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
