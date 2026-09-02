/**
 * @file nt_number_theory.h
 * @brief Number theory algorithms -- modular arithmetic, primality testing, factorization
 *
 * Provides GMP-based modular arithmetic, Miller-Rabin primality testing,
 * trial division factorization, and GCD/LCM computation.
 *
 * Reference: NTL (Victor Shoup), GMP mpz documentation
 *
 * @author Lv-00 Project
 * @version 1.1.0
 */
#ifndef lv_NT_NUMBER_THEORY_H
#define lv_NT_NUMBER_THEORY_H
#include <gmp.h>
#include <stdbool.h>
#include <stddef.h>
#include "lv_api_spec.h" /* lv_PUBLIC_API（K59） */

#ifndef lv_PUBLIC_API
#define lv_PUBLIC_API
#endif
#ifdef __cplusplus
extern "C" {
#endif
/* ============================================================
 * Types
 * ============================================================ */
/**
 * @brief Modular arithmetic context
 *
 * Stores a modulus and caches whether it is prime for optimized
 * modular inverse computation.
 */
typedef struct lvModContext {
    mpz_t modulus; /**< The modulus (must be > 0) */
    int is_prime;  /**< Non-zero if modulus is known to be prime */
} lvModContext;
/* ============================================================
 * Lifecycle
 * ============================================================ */
/**
 * @brief Initialize a modular context
 *
 * Sets modulus to 1 and is_prime to 0. Caller must call
 * nt_mod_context_clear() when done.
 *
 * @param ctx  Pointer to context to initialize
 */
lv_PUBLIC_API void nt_mod_context_init(lvModContext *ctx);
/**
 * @brief Set the modulus of a modular context
 *
 * Clears any previous modulus value and sets the new one.
 * The is_prime flag is reset to 0; the caller may set it
 * manually if the primality is known.
 *
 * @param ctx      Pointer to context
 * @param modulus  The new modulus (must be > 0)
 */
lv_PUBLIC_API void nt_mod_context_set(lvModContext *ctx, const mpz_t modulus);
/**
 * @brief Release resources held by a modular context
 *
 * @param ctx  Pointer to context to clear
 */
lv_PUBLIC_API void nt_mod_context_clear(lvModContext *ctx);
/* ============================================================
 * Modular arithmetic
 * ============================================================ */
/**
 * @brief Modular addition: (a + b) mod n
 *
 * @param result  [out] Result (must be initialized)
 * @param ctx     Modular context
 * @param a       First operand
 * @param b       Second operand
 */
lv_PUBLIC_API void nt_mod_add(mpz_t result, const lvModContext *ctx, const mpz_t a, const mpz_t b);
/**
 * @brief Modular multiplication: (a * b) mod n
 *
 * @param result  [out] Result (must be initialized)
 * @param ctx     Modular context
 * @param a       First operand
 * @param b       Second operand
 */
lv_PUBLIC_API void nt_mod_mul(mpz_t result, const lvModContext *ctx, const mpz_t a, const mpz_t b);
/**
 * @brief Modular inverse: a^(-1) mod n
 *
 * Computes the modular inverse using mpz_invert (extended Euclidean).
 * Returns false if the inverse does not exist (i.e. gcd(a, n) != 1).
 *
 * @param result  [out] Result (must be initialized)
 * @param ctx     Modular context
 * @param a       Operand
 * @return true if inverse exists, false otherwise
 */
lv_PUBLIC_API int nt_mod_inv(mpz_t result, const lvModContext *ctx, const mpz_t a);
/**
 * @brief Modular exponentiation: base^exp mod n
 *
 * Uses GMP's mpz_powm for efficient computation.
 *
 * @param result  [out] Result (must be initialized)
 * @param ctx     Modular context
 * @param base    Base
 * @param exp     Exponent (must be >= 0)
 */
lv_PUBLIC_API void nt_mod_pow(mpz_t result, const lvModContext *ctx, const mpz_t base, const mpz_t exp);
/* ============================================================
 * GCD and LCM
 * ============================================================ */
/**
 * @brief Greatest common divisor: gcd(a, b)
 *
 * @param result  [out] Result (must be initialized)
 * @param a       First operand
 * @param b       Second operand
 */
lv_PUBLIC_API void nt_gcd(mpz_t result, const mpz_t a, const mpz_t b);
/**
 * @brief Least common multiple: lcm(a, b)
 *
 * Returns 0 if either a or b is 0.
 *
 * @param result  [out] Result (must be initialized)
 * @param a       First operand
 * @param b       Second operand
 */
lv_PUBLIC_API void nt_lcm(mpz_t result, const mpz_t a, const mpz_t b);
/* ============================================================
 * Primality testing
 * ============================================================ */
/**
 * @brief Miller-Rabin probabilistic primality test
 *
 * Performs k rounds of the Miller-Rabin test. Returns 1 if n is
 * probably prime, 0 if n is definitely composite.
 *
 * For n < 3, returns 0 (not prime). For n == 2 or n == 3, returns 1.
 *
 * @param n  Number to test (must be > 0)
 * @param k  Number of witness rounds (recommended: 20-40)
 * @return 1 if probably prime, 0 if composite
 */
lv_PUBLIC_API int nt_is_prime_miller_rabin(const mpz_t n, int k);
/**
 * @brief Find the next prime >= n
 *
 * Uses mpz_nextprime from GMP internally.
 *
 * @param result  [out] Next prime (must be initialized)
 * @param n       Starting point
 */
lv_PUBLIC_API void nt_next_prime(mpz_t result, const mpz_t n);
/* ============================================================
 * Factorization
 * ============================================================ */
/**
 * @brief Trial division factorization
 *
 * Finds all prime factors of n up to a given bound using trial division.
 * Factors are stored in the provided array in non-decreasing order.
 *
 * @param n           Number to factorize (must be > 1)
 * @param factors     [out] Array to store factors (each mpz_t must be initialized)
 * @param max_factors Maximum number of factors to store
 * @param bound       Upper bound for trial divisors (0 means no bound)
 * @return Number of factors found, or -1 on error
 */
lv_PUBLIC_API int nt_factorize_trial_div(const mpz_t n, mpz_t *factors, int max_factors, const mpz_t bound);
#ifdef __cplusplus
}
#endif
#endif /* lv_NT_NUMBER_THEORY_H */
