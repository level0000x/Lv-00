/**
 * @file nt_number_theory.c
 * @brief Number theory algorithms -- GMP-based implementation
 *
 * Implements modular arithmetic, primality testing (Miller-Rabin),
 * trial division factorization, and GCD/LCM computation.
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "lv/nt_number_theory.h"

#include <gmp.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================
 * Lifecycle
 * ============================================================ */

lv_PUBLIC_API void nt_mod_context_init(lvModContext *ctx) {
    if (!ctx)
        return;
    mpz_init_set_ui(ctx->modulus, 1);
    ctx->is_prime = 0;
}

lv_PUBLIC_API void nt_mod_context_set(lvModContext *ctx, const mpz_t modulus) {
    if (!ctx)
        return;
    mpz_set(ctx->modulus, modulus);
    ctx->is_prime = 0;
}

lv_PUBLIC_API void nt_mod_context_clear(lvModContext *ctx) {
    if (!ctx)
        return;
    mpz_clear(ctx->modulus);
    ctx->modulus->_mp_d = NULL; /* safety: prevent dangling pointer */
    ctx->is_prime = 0;
}

/* ============================================================
 * Modular arithmetic
 * ============================================================ */

lv_PUBLIC_API void nt_mod_add(mpz_t result, const lvModContext *ctx, const mpz_t a, const mpz_t b) {
    if (!ctx)
        return;
    mpz_add(result, a, b);
    mpz_mod(result, result, ctx->modulus);
}

lv_PUBLIC_API void nt_mod_mul(mpz_t result, const lvModContext *ctx, const mpz_t a, const mpz_t b) {
    if (!ctx)
        return;
    mpz_mul(result, a, b);
    mpz_mod(result, result, ctx->modulus);
}

lv_PUBLIC_API int nt_mod_inv(mpz_t result, const lvModContext *ctx, const mpz_t a) {
    if (!ctx)
        return 0;
    return (mpz_invert(result, a, ctx->modulus) != 0) ? 1 : 0;
}

lv_PUBLIC_API void nt_mod_pow(mpz_t result, const lvModContext *ctx, const mpz_t base, const mpz_t exp) {
    if (!ctx)
        return;
    mpz_powm(result, base, exp, ctx->modulus);
}

/* ============================================================
 * GCD and LCM
 * ============================================================ */

lv_PUBLIC_API void nt_gcd(mpz_t result, const mpz_t a, const mpz_t b) {
    mpz_gcd(result, a, b);
}

lv_PUBLIC_API void nt_lcm(mpz_t result, const mpz_t a, const mpz_t b) {
    if (mpz_cmp_ui(a, 0) == 0 || mpz_cmp_ui(b, 0) == 0) {
        mpz_set_ui(result, 0);
        return;
    }
    mpz_lcm(result, a, b);
}

/* ============================================================
 * Primality testing
 * ============================================================ */

lv_PUBLIC_API int nt_is_prime_miller_rabin(const mpz_t n, int k) {
    /* Handle small cases */
    if (mpz_cmp_ui(n, 2) < 0)
        return 0;
    if (mpz_cmp_ui(n, 2) == 0)
        return 1;
    if (mpz_cmp_ui(n, 3) == 0)
        return 1;
    if (mpz_even_p(n))
        return 0;

    /* Write n - 1 = 2^r * d with d odd */
    mpz_t n_minus_1, d;
    mpz_t a, x, n_mod;
    gmp_randstate_t state;
    int r = 0;

    mpz_init(n_minus_1);
    mpz_init(d);
    mpz_init(a);
    mpz_init(x);
    mpz_init(n_mod);

    mpz_sub_ui(n_minus_1, n, 1);
    mpz_set(d, n_minus_1);

    while (mpz_even_p(d)) {
        mpz_fdiv_q_2exp(d, d, 1);
        r++;
    }

    gmp_randinit_default(state);
    gmp_randseed_ui(state, (unsigned long) 42); /* deterministic seed for reproducibility */

    for (int i = 0; i < k; i++) {
        /* Pick a random witness a in [2, n - 2] */
        mpz_urandomm(a, state, n_minus_1);
        if (mpz_cmp_ui(a, 2) < 0)
            mpz_set_ui(a, 2);

        /* x = a^d mod n */
        mpz_powm(x, a, d, n);

        if (mpz_cmp_ui(x, 1) == 0 || mpz_cmp(x, n_minus_1) == 0) {
            continue; /* probably prime for this witness */
        }

        int found = 0;
        for (int j = 0; j < r - 1; j++) {
            mpz_powm_ui(x, x, 2, n);
            if (mpz_cmp(x, n_minus_1) == 0) {
                found = 1;
                break;
            }
        }

        if (!found) {
            /* Composite */
            gmp_randclear(state);
            mpz_clear(n_minus_1);
            mpz_clear(d);
            mpz_clear(a);
            mpz_clear(x);
            mpz_clear(n_mod);
            return 0;
        }
    }

    gmp_randclear(state);
    mpz_clear(n_minus_1);
    mpz_clear(d);
    mpz_clear(a);
    mpz_clear(x);
    mpz_clear(n_mod);
    return 1;
}

lv_PUBLIC_API void nt_next_prime(mpz_t result, const mpz_t n) {
    /* mpz_nextprime returns the smallest prime > n.
     * For the "next prime >= n" semantics expected by the test,
     * check if n itself is prime first. */
    if (mpz_probab_prime_p(n, 25) > 0) {
        mpz_set(result, n);
        return;
    }
    mpz_nextprime(result, n);
}

/* ============================================================
 * Factorization
 * ============================================================ */

lv_PUBLIC_API int nt_factorize_trial_div(const mpz_t n, mpz_t *factors, int max_factors, const mpz_t bound) {
    if (!factors || max_factors <= 0)
        return 0;
    if (mpz_cmp_ui(n, 2) < 0)
        return 0;

    int count = 0;
    mpz_t remainder, divisor, limit;
    mpz_init(remainder);
    mpz_init(divisor);
    mpz_init(limit);

    mpz_set(remainder, n);

    /* Extract factor 2 */
    while (mpz_even_p(remainder) && count < max_factors) {
        mpz_set_ui(factors[count], 2);
        mpz_fdiv_q_2exp(remainder, remainder, 1);
        count++;
    }

    /* Trial division by odd numbers starting from 3 */
    mpz_set_ui(divisor, 3);

    if (mpz_cmp_ui(bound, 0) > 0) {
        mpz_set(limit, bound);
    } else {
        mpz_sqrt(limit, remainder);
        mpz_add_ui(limit, limit, 1);
    }

    while (mpz_cmp(divisor, limit) <= 0 && count < max_factors) {
        while (mpz_divisible_p(remainder, divisor) && count < max_factors) {
            mpz_set(factors[count], divisor);
            mpz_fdiv_q(remainder, remainder, divisor);
            count++;
        }
        if (mpz_cmp_ui(remainder, 1) == 0)
            break;

        mpz_add_ui(divisor, divisor, 2);

        /* Update limit if no explicit bound */
        if (mpz_cmp_ui(bound, 0) == 0) {
            mpz_sqrt(limit, remainder);
            mpz_add_ui(limit, limit, 1);
        }
    }

    /* If remainder > 1 and we still have space, it is a large prime factor */
    if (mpz_cmp_ui(remainder, 1) > 0 && count < max_factors) {
        mpz_set(factors[count], remainder);
        count++;
    }

    mpz_clear(remainder);
    mpz_clear(divisor);
    mpz_clear(limit);
    return count;
}
