/**
 * @file nt_polynomial.h
 * @brief Polynomial arithmetic with arbitrary-precision integer coefficients
 *
 * Provides GMP-based polynomial operations: creation, destruction,
 * coefficient access, addition, multiplication, modular reduction,
 * GCD, evaluation, and degree query.
 *
 * Reference: NTL ZZ_pX, FLINT nmod_poly
 *
 * @author Lv-00 Project
 * @version 1.1.0
 */
#ifndef lv_NT_POLYNOMIAL_H
#define lv_NT_POLYNOMIAL_H
#include <gmp.h>
#include <stdbool.h>
#include <stddef.h>

#include "lv.h"
#ifdef __cplusplus
extern "C" {
#endif
/* ============================================================
 * Types
 * ============================================================ */
/**
 * @brief Polynomial with arbitrary-precision integer coefficients
 *
 * Stores coefficients in ascending order of degree:
 *   coeffs[0] is the constant term, coeffs[i] is the x^i coefficient.
 *
 * A polynomial with degree < 0 represents the zero polynomial.
 * The capacity field tracks the allocated size for amortized growth.
 */
typedef struct lvPoly {
    mpz_t *coeffs; /**< Array of coefficients (ascending degree order) */
    int degree;    /**< Current degree (-1 for zero polynomial) */
    int capacity;  /**< Allocated size of coeffs array */
} lvPoly;
/* ============================================================
 * Lifecycle
 * ============================================================ */
/**
 * @brief Create a zero polynomial
 *
 * @return Pointer to newly allocated polynomial, or NULL on allocation failure
 */
lv_PUBLIC_API lvPoly *nt_poly_create(void);
/**
 * @brief Destroy a polynomial and release all resources
 *
 * @param p  Pointer to polynomial (may be NULL, no-op in that case)
 */
lv_PUBLIC_API void nt_poly_destroy(lvPoly *p);
/* ============================================================
 * Coefficient access
 * ============================================================ */
/**
 * @brief Set a coefficient at a given degree
 *
 * If deg exceeds current capacity, the internal array is reallocated.
 * The degree is updated if the new coefficient is non-zero and deg > degree.
 *
 * @param p    Pointer to polynomial
 * @param deg  Degree index (must be >= 0)
 * @param val  Value to set
 * @return 0 on success, -1 on error
 */
lv_PUBLIC_API int nt_poly_set_coeff(lvPoly *p, int deg, const mpz_t val);
/**
 * @brief Get a coefficient at a given degree
 *
 * Returns 0 if deg is out of range (deg < 0 or deg > degree).
 *
 * @param p    Pointer to polynomial
 * @param deg  Degree index
 * @param out  [out] Output value (must be initialized)
 * @return 0 on success, -1 if deg is out of range
 */
lv_PUBLIC_API int nt_poly_get_coeff(const lvPoly *p, int deg, mpz_t out);
/* ============================================================
 * Arithmetic
 * ============================================================ */
/**
 * @brief Polynomial addition: result = a + b
 *
 * @param result  [out] Result polynomial (must be created via nt_poly_create)
 * @param a       First operand
 * @param b       Second operand
 * @return 0 on success, -1 on error
 */
lv_PUBLIC_API int nt_poly_add(lvPoly *result, const lvPoly *a, const lvPoly *b);
/**
 * @brief Polynomial multiplication: result = a * b
 *
 * @param result  [out] Result polynomial (must be created via nt_poly_create)
 * @param a       First operand
 * @param b       Second operand
 * @return 0 on success, -1 on error
 */
lv_PUBLIC_API int nt_poly_mul(lvPoly *result, const lvPoly *a, const lvPoly *b);
/**
 * @brief Polynomial modular reduction: result = f mod m
 *
 * Computes f reduced modulo polynomial m using polynomial long division.
 * The result has degree < deg(m).
 *
 * @param result  [out] Result polynomial (must be created via nt_poly_create)
 * @param f       Polynomial to reduce
 * @param m       Modulus polynomial (must be non-zero)
 * @return 0 on success, -1 on error
 */
lv_PUBLIC_API int nt_poly_mod(lvPoly *result, const lvPoly *f, const lvPoly *m);
/**
 * @brief Polynomial GCD: result = gcd(a, b)
 *
 * Uses the Euclidean algorithm over Z[x].
 *
 * @param result  [out] Result polynomial (must be created via nt_poly_create)
 * @param a       First operand
 * @param b       Second operand
 * @return 0 on success, -1 on error
 */
lv_PUBLIC_API int nt_poly_gcd(lvPoly *result, const lvPoly *a, const lvPoly *b);
/* ============================================================
 * Evaluation and properties
 * ============================================================ */
/**
 * @brief Evaluate polynomial at a given point: p(x)
 *
 * Uses Horner's method for efficient evaluation.
 *
 * @param p    Pointer to polynomial
 * @param x    Evaluation point
 * @param out  [out] Result value (must be initialized)
 * @return 0 on success, -1 on error
 */
lv_PUBLIC_API int nt_poly_eval(const lvPoly *p, const mpz_t x, mpz_t out);
/**
 * @brief Get the degree of a polynomial
 *
 * Returns -1 for the zero polynomial.
 *
 * @param p  Pointer to polynomial
 * @return Degree of the polynomial, or -1 if p is NULL or zero
 */
lv_PUBLIC_API int nt_poly_degree(const lvPoly *p);
#ifdef __cplusplus
}
#endif
#endif /* lv_NT_POLYNOMIAL_H */
