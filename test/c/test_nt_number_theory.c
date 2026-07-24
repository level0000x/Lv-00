/**
 * @file test_nt_number_theory.c
 * @brief Test suite for the number theory module
 *
 * Tests all public API functions of nt_number_theory.h:
 * - Lifecycle (init, set, clear)
 * - Modular arithmetic (add, mul, inv, pow)
 * - GCD and LCM
 * - Miller-Rabin primality testing
 * - Next prime
 * - Trial division factorization
 *
 * @version 3.3.0
 * @date 2026-05-25
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nt_number_theory.h"
#include "test_helpers.h"

/* ============================================================
 * Global test counters
 * ============================================================ */
int g_pass_count = 0;
int g_fail_count = 0;

/* ============================================================
 * Test: modular context lifecycle
 * ============================================================ */

static void test_mod_context_init(void) {
    lvModContext ctx;
    nt_mod_context_init(&ctx);
    TEST_ASSERT_MSG(mpz_cmp_ui(ctx.modulus, 1) == 0, "initial modulus should be 1");
    TEST_ASSERT_MSG(ctx.is_prime == 0, "initial is_prime should be 0");
    nt_mod_context_clear(&ctx);
}

static void test_mod_context_set(void) {
    lvModContext ctx;
    mpz_t m;
    mpz_init_set_ui(m, 13);
    nt_mod_context_init(&ctx);
    nt_mod_context_set(&ctx, m);
    TEST_ASSERT_MSG(mpz_cmp(ctx.modulus, m) == 0, "modulus should be 13 after set");
    TEST_ASSERT_MSG(ctx.is_prime == 0, "is_prime should be reset to 0 after set");
    mpz_clear(m);
    nt_mod_context_clear(&ctx);
}

/* ============================================================
 * Test: modular addition
 * ============================================================ */

static void test_mod_add(void) {
    lvModContext ctx;
    mpz_t mod, a, b, result;
    mpz_init_set_ui(mod, 7);
    mpz_init_set_ui(a, 5);
    mpz_init_set_ui(b, 4);
    mpz_init(result);

    nt_mod_context_init(&ctx);
    nt_mod_context_set(&ctx, mod);
    nt_mod_add(result, &ctx, a, b);

    /* (5 + 4) mod 7 = 2 */
    TEST_ASSERT_MSG(mpz_cmp_ui(result, 2) == 0, "(5 + 4) mod 7 should be 2");

    mpz_clear(mod);
    mpz_clear(a);
    mpz_clear(b);
    mpz_clear(result);
    nt_mod_context_clear(&ctx);
}

static void test_mod_add_negative(void) {
    lvModContext ctx;
    mpz_t mod, a, b, result;
    mpz_init_set_ui(mod, 11);
    mpz_init_set_si(a, -3);
    mpz_init_set_ui(b, 5);
    mpz_init(result);

    nt_mod_context_init(&ctx);
    nt_mod_context_set(&ctx, mod);
    nt_mod_add(result, &ctx, a, b);

    /* (-3 + 5) mod 11 = 2 */
    TEST_ASSERT_MSG(mpz_cmp_ui(result, 2) == 0, "(-3 + 5) mod 11 should be 2");

    mpz_clear(mod);
    mpz_clear(a);
    mpz_clear(b);
    mpz_clear(result);
    nt_mod_context_clear(&ctx);
}

/* ============================================================
 * Test: modular multiplication
 * ============================================================ */

static void test_mod_mul(void) {
    lvModContext ctx;
    mpz_t mod, a, b, result;
    mpz_init_set_ui(mod, 13);
    mpz_init_set_ui(a, 4);
    mpz_init_set_ui(b, 5);
    mpz_init(result);

    nt_mod_context_init(&ctx);
    nt_mod_context_set(&ctx, mod);
    nt_mod_mul(result, &ctx, a, b);

    /* (4 * 5) mod 13 = 7 */
    TEST_ASSERT_MSG(mpz_cmp_ui(result, 7) == 0, "(4 * 5) mod 13 should be 7");

    mpz_clear(mod);
    mpz_clear(a);
    mpz_clear(b);
    mpz_clear(result);
    nt_mod_context_clear(&ctx);
}

static void test_mod_mul_zero(void) {
    lvModContext ctx;
    mpz_t mod, a, b, result;
    mpz_init_set_ui(mod, 17);
    mpz_init_set_ui(a, 0);
    mpz_init_set_ui(b, 42);
    mpz_init(result);

    nt_mod_context_init(&ctx);
    nt_mod_context_set(&ctx, mod);
    nt_mod_mul(result, &ctx, a, b);

    /* (0 * 42) mod 17 = 0 */
    TEST_ASSERT_MSG(mpz_cmp_ui(result, 0) == 0, "(0 * 42) mod 17 should be 0");

    mpz_clear(mod);
    mpz_clear(a);
    mpz_clear(b);
    mpz_clear(result);
    nt_mod_context_clear(&ctx);
}

/* ============================================================
 * Test: modular inverse
 * ============================================================ */

static void test_mod_inv(void) {
    lvModContext ctx;
    mpz_t mod, a, result;
    int ok;
    mpz_init_set_ui(mod, 7);
    mpz_init_set_ui(a, 3);
    mpz_init(result);

    nt_mod_context_init(&ctx);
    nt_mod_context_set(&ctx, mod);
    ok = nt_mod_inv(result, &ctx, a);

    /* 3^(-1) mod 7 = 5 (because 3 * 5 = 15 = 2*7 + 1) */
    TEST_ASSERT_MSG(ok == 1, "inverse of 3 mod 7 should exist");
    TEST_ASSERT_MSG(mpz_cmp_ui(result, 5) == 0, "3^(-1) mod 7 should be 5");

    mpz_clear(mod);
    mpz_clear(a);
    mpz_clear(result);
    nt_mod_context_clear(&ctx);
}

static void test_mod_inv_no_inverse(void) {
    lvModContext ctx;
    mpz_t mod, a, result;
    int ok;
    mpz_init_set_ui(mod, 6);
    mpz_init_set_ui(a, 2);
    mpz_init(result);

    nt_mod_context_init(&ctx);
    nt_mod_context_set(&ctx, mod);
    ok = nt_mod_inv(result, &ctx, a);

    /* gcd(2, 6) = 2 != 1, so no inverse exists */
    TEST_ASSERT_MSG(ok == 0, "inverse of 2 mod 6 should not exist");

    mpz_clear(mod);
    mpz_clear(a);
    mpz_clear(result);
    nt_mod_context_clear(&ctx);
}

/* ============================================================
 * Test: modular exponentiation
 * ============================================================ */

static void test_mod_pow(void) {
    lvModContext ctx;
    mpz_t mod, base, exp, result;
    mpz_init_set_ui(mod, 13);
    mpz_init_set_ui(base, 2);
    mpz_init_set_ui(exp, 10);
    mpz_init(result);

    nt_mod_context_init(&ctx);
    nt_mod_context_set(&ctx, mod);
    nt_mod_pow(result, &ctx, base, exp);

    /* 2^10 mod 13 = 1024 mod 13 = 1024 - 78*13 = 1024 - 1014 = 10 */
    TEST_ASSERT_MSG(mpz_cmp_ui(result, 10) == 0, "2^10 mod 13 should be 10");

    mpz_clear(mod);
    mpz_clear(base);
    mpz_clear(exp);
    mpz_clear(result);
    nt_mod_context_clear(&ctx);
}

/* ============================================================
 * Test: GCD
 * ============================================================ */

static void test_gcd(void) {
    mpz_t a, b, result;
    mpz_init_set_ui(a, 48);
    mpz_init_set_ui(b, 18);
    mpz_init(result);

    nt_gcd(result, a, b);
    TEST_ASSERT_MSG(mpz_cmp_ui(result, 6) == 0, "gcd(48, 18) should be 6");

    mpz_clear(a);
    mpz_clear(b);
    mpz_clear(result);
}

static void test_gcd_coprime(void) {
    mpz_t a, b, result;
    mpz_init_set_ui(a, 17);
    mpz_init_set_ui(b, 13);
    mpz_init(result);

    nt_gcd(result, a, b);
    TEST_ASSERT_MSG(mpz_cmp_ui(result, 1) == 0, "gcd(17, 13) should be 1");

    mpz_clear(a);
    mpz_clear(b);
    mpz_clear(result);
}

static void test_gcd_zero(void) {
    mpz_t a, b, result;
    mpz_init_set_ui(a, 0);
    mpz_init_set_ui(b, 5);
    mpz_init(result);

    nt_gcd(result, a, b);
    TEST_ASSERT_MSG(mpz_cmp_ui(result, 5) == 0, "gcd(0, 5) should be 5");

    mpz_clear(a);
    mpz_clear(b);
    mpz_clear(result);
}

/* ============================================================
 * Test: LCM
 * ============================================================ */

static void test_lcm(void) {
    mpz_t a, b, result;
    mpz_init_set_ui(a, 4);
    mpz_init_set_ui(b, 6);
    mpz_init(result);

    nt_lcm(result, a, b);
    TEST_ASSERT_MSG(mpz_cmp_ui(result, 12) == 0, "lcm(4, 6) should be 12");

    mpz_clear(a);
    mpz_clear(b);
    mpz_clear(result);
}

static void test_lcm_zero(void) {
    mpz_t a, b, result;
    mpz_init_set_ui(a, 0);
    mpz_init_set_ui(b, 5);
    mpz_init(result);

    nt_lcm(result, a, b);
    TEST_ASSERT_MSG(mpz_cmp_ui(result, 0) == 0, "lcm(0, 5) should be 0");

    mpz_clear(a);
    mpz_clear(b);
    mpz_clear(result);
}

/* ============================================================
 * Test: Miller-Rabin primality
 * ============================================================ */

static void test_miller_rabin_prime(void) {
    mpz_t n;
    mpz_init_set_ui(n, 104729); /* known prime */
    TEST_ASSERT_MSG(nt_is_prime_miller_rabin(n, 25) == 1, "104729 should be prime");
    mpz_clear(n);
}

static void test_miller_rabin_composite(void) {
    mpz_t n;
    mpz_init_set_ui(n, 104730); /* 104729 + 1, composite */
    TEST_ASSERT_MSG(nt_is_prime_miller_rabin(n, 25) == 0, "104730 should be composite");
    mpz_clear(n);
}

static void test_miller_rabin_small(void) {
    mpz_t n;
    mpz_init(n);

    mpz_set_ui(n, 2);
    TEST_ASSERT_MSG(nt_is_prime_miller_rabin(n, 10) == 1, "2 should be prime");

    mpz_set_ui(n, 3);
    TEST_ASSERT_MSG(nt_is_prime_miller_rabin(n, 10) == 1, "3 should be prime");

    mpz_set_ui(n, 4);
    TEST_ASSERT_MSG(nt_is_prime_miller_rabin(n, 10) == 0, "4 should be composite");

    mpz_set_ui(n, 1);
    TEST_ASSERT_MSG(nt_is_prime_miller_rabin(n, 10) == 0, "1 should not be prime");

    mpz_set_ui(n, 0);
    TEST_ASSERT_MSG(nt_is_prime_miller_rabin(n, 10) == 0, "0 should not be prime");

    mpz_clear(n);
}

static void test_miller_rabin_carmichael(void) {
    mpz_t n;
    mpz_init_set_ui(n, 561); /* Carmichael number: 3 * 11 * 17 */
    TEST_ASSERT_MSG(nt_is_prime_miller_rabin(n, 25) == 0, "561 (Carmichael) should be composite");
    mpz_clear(n);
}

/* ============================================================
 * Test: next prime
 * ============================================================ */

static void test_next_prime(void) {
    mpz_t n, result;
    mpz_init_set_ui(n, 14);
    mpz_init(result);

    nt_next_prime(result, n);
    TEST_ASSERT_MSG(mpz_cmp_ui(result, 17) == 0, "next prime after 14 should be 17");

    mpz_clear(n);
    mpz_clear(result);
}

static void test_next_prime_from_prime(void) {
    mpz_t n, result;
    mpz_init_set_ui(n, 7);
    mpz_init(result);

    nt_next_prime(result, n);
    /* nextprime(7) returns the next prime >= 7, which is 7 itself */
    TEST_ASSERT_MSG(mpz_cmp_ui(result, 7) == 0, "next prime >= 7 should be 7");

    mpz_clear(n);
    mpz_clear(result);
}

/* ============================================================
 * Test: trial division factorization
 * ============================================================ */

static void test_factorize_trial_div(void) {
    mpz_t n, bound;
    mpz_t factors[10];
    int count, i;

    mpz_init_set_ui(n, 360);   /* 360 = 2^3 * 3^2 * 5 */
    mpz_init_set_ui(bound, 0); /* no bound */

    for (i = 0; i < 10; i++)
        mpz_init(factors[i]);

    count = nt_factorize_trial_div(n, factors, 10, bound);

    /* Expected: 2, 2, 2, 3, 3, 5 (6 factors) */
    TEST_ASSERT_MSG(count == 6, "360 should have 6 prime factors");
    TEST_ASSERT_MSG(mpz_cmp_ui(factors[0], 2) == 0, "first factor should be 2");
    TEST_ASSERT_MSG(mpz_cmp_ui(factors[1], 2) == 0, "second factor should be 2");
    TEST_ASSERT_MSG(mpz_cmp_ui(factors[2], 2) == 0, "third factor should be 2");
    TEST_ASSERT_MSG(mpz_cmp_ui(factors[3], 3) == 0, "fourth factor should be 3");
    TEST_ASSERT_MSG(mpz_cmp_ui(factors[4], 3) == 0, "fifth factor should be 3");
    TEST_ASSERT_MSG(mpz_cmp_ui(factors[5], 5) == 0, "sixth factor should be 5");

    mpz_clear(n);
    mpz_clear(bound);
    for (i = 0; i < 10; i++)
        mpz_clear(factors[i]);
}

static void test_factorize_prime(void) {
    mpz_t n, bound;
    mpz_t factors[1];
    int count;

    mpz_init_set_ui(n, 97); /* prime */
    mpz_init_set_ui(bound, 0);

    mpz_init(factors[0]);

    count = nt_factorize_trial_div(n, factors, 1, bound);

    TEST_ASSERT_MSG(count == 1, "97 should have 1 factor (itself)");
    TEST_ASSERT_MSG(mpz_cmp_ui(factors[0], 97) == 0, "factor should be 97");

    mpz_clear(n);
    mpz_clear(bound);
    mpz_clear(factors[0]);
}

/* ============================================================
 * Main
 * ============================================================ */
int main(void) {
    TEST_SUITE_BEGIN("NumberTheory");

    /* Lifecycle */
    TEST_RUN(test_mod_context_init);
    TEST_RUN(test_mod_context_set);

    /* Modular arithmetic */
    TEST_RUN(test_mod_add);
    TEST_RUN(test_mod_add_negative);
    TEST_RUN(test_mod_mul);
    TEST_RUN(test_mod_mul_zero);
    TEST_RUN(test_mod_inv);
    TEST_RUN(test_mod_inv_no_inverse);
    TEST_RUN(test_mod_pow);

    /* GCD and LCM */
    TEST_RUN(test_gcd);
    TEST_RUN(test_gcd_coprime);
    TEST_RUN(test_gcd_zero);
    TEST_RUN(test_lcm);
    TEST_RUN(test_lcm_zero);

    /* Primality */
    TEST_RUN(test_miller_rabin_prime);
    TEST_RUN(test_miller_rabin_composite);
    TEST_RUN(test_miller_rabin_small);
    TEST_RUN(test_miller_rabin_carmichael);

    /* Next prime */
    TEST_RUN(test_next_prime);
    TEST_RUN(test_next_prime_from_prime);

    /* Factorization */
    TEST_RUN(test_factorize_trial_div);
    TEST_RUN(test_factorize_prime);

    TEST_SUITE_END();
    return (g_fail_count > 0) ? 1 : 0;
}
