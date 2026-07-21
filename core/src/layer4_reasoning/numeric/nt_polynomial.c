/**
 * @file nt_polynomial.c
 * @brief Polynomial arithmetic with arbitrary-precision integer coefficients
 *
 * Implements polynomial operations using GMP mpz_t for coefficients.
 * Coefficients are stored in ascending degree order.
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "nt_polynomial.h"

#include <gmp.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================
 * Internal helpers
 * ============================================================ */

/** Default initial capacity for coefficient array */
#define NT_POLY_DEFAULT_CAPACITY 8

/**
 * @brief Ensure the polynomial has enough capacity for a given degree
 *
 * Reallocates the coefficient array if necessary.
 *
 * @param p    Polynomial to grow
 * @param deg  Required minimum degree
 * @return 0 on success, -1 on allocation failure
 */
static int nt_poly_ensure_capacity(Lv00Poly *p, int deg) {
    if (deg < 0) return 0;
    if (deg < p->capacity) return 0;

    int new_cap = p->capacity;
    if (new_cap <= 0) new_cap = NT_POLY_DEFAULT_CAPACITY;
    while (new_cap <= deg) {
        if (new_cap > INT_MAX / 2) return -1; /* overflow guard */
        new_cap *= 2;
    }

    mpz_t *new_coeffs = (mpz_t *)lv00_realloc(p->coeffs, (size_t)new_cap * sizeof(mpz_t));
    if (!new_coeffs) return -1;

    p->coeffs = new_coeffs;

    /* Initialize newly allocated slots */
    for (int i = p->capacity; i < new_cap; i++) {
        mpz_init(p->coeffs[i]);
    }

    p->capacity = new_cap;
    return 0;
}

/**
 * @brief Normalize polynomial: trim trailing zero coefficients
 *
 * Updates degree to reflect the highest non-zero coefficient.
 * A polynomial with all zero coefficients has degree -1.
 */
static void nt_poly_normalize(Lv00Poly *p) {
    if (!p || !p->coeffs) return;
    while (p->degree >= 0 && mpz_cmp_ui(p->coeffs[p->degree], 0) == 0) {
        p->degree--;
    }
}

/* ============================================================
 * Lifecycle
 * ============================================================ */

LV00_PUBLIC_API Lv00Poly *nt_poly_create(void) {
    Lv00Poly *p = (Lv00Poly *)calloc(1, sizeof(Lv00Poly));
    if (!p) return NULL;
    p->coeffs = NULL;
    p->degree = -1;
    p->capacity = 0;
    return p;
}

LV00_PUBLIC_API void nt_poly_destroy(Lv00Poly *p) {
    if (!p) return;
    if (p->coeffs) {
        for (int i = 0; i < p->capacity; i++) {
            mpz_clear(p->coeffs[i]);
        }
        free(p->coeffs);
    }
    free(p);
}

/* ============================================================
 * Coefficient access
 * ============================================================ */

LV00_PUBLIC_API int nt_poly_set_coeff(Lv00Poly *p, int deg, const mpz_t val) {
    if (!p || deg < 0) return -1;

    if (nt_poly_ensure_capacity(p, deg) != 0) return -1;

    mpz_set(p->coeffs[deg], val);

    /* Update degree if necessary */
    if (mpz_cmp_ui(val, 0) != 0 && deg > p->degree) {
        p->degree = deg;
    } else if (deg == p->degree && mpz_cmp_ui(val, 0) == 0) {
        nt_poly_normalize(p);
    }

    return 0;
}

LV00_PUBLIC_API int nt_poly_get_coeff(const Lv00Poly *p, int deg, mpz_t out) {
    if (!p || deg < 0 || deg > p->degree) {
        mpz_set_ui(out, 0);
        return -1;
    }
    mpz_set(out, p->coeffs[deg]);
    return 0;
}

/* ============================================================
 * Arithmetic
 * ============================================================ */

LV00_PUBLIC_API int nt_poly_add(Lv00Poly *result, const Lv00Poly *a,
                                const Lv00Poly *b) {
    if (!result || !a || !b) return -1;

    int max_deg = (a->degree > b->degree) ? a->degree : b->degree;
    if (max_deg < 0) {
        /* Both are zero polynomials */
        result->degree = -1;
        return 0;
    }

    if (nt_poly_ensure_capacity(result, max_deg) != 0) return -1;

    for (int i = 0; i <= max_deg; i++) {
        mpz_t tmp;
        mpz_init(tmp);
        if (i <= a->degree && i <= b->degree) {
            mpz_add(tmp, a->coeffs[i], b->coeffs[i]);
        } else if (i <= a->degree) {
            mpz_set(tmp, a->coeffs[i]);
        } else {
            mpz_set(tmp, b->coeffs[i]);
        }
        mpz_set(result->coeffs[i], tmp);
        mpz_clear(tmp);
    }

    result->degree = max_deg;
    nt_poly_normalize(result);
    return 0;
}

LV00_PUBLIC_API int nt_poly_mul(Lv00Poly *result, const Lv00Poly *a,
                                const Lv00Poly *b) {
    if (!result || !a || !b) return -1;

    /* Zero polynomial cases */
    if (a->degree < 0 || b->degree < 0) {
        result->degree = -1;
        return 0;
    }

    /* Overflow guard */
    if (a->degree > INT_MAX - b->degree) return -1;

    int new_deg = a->degree + b->degree;

    if (nt_poly_ensure_capacity(result, new_deg) != 0) return -1;

    /* Zero out result coefficients up to new_deg */
    for (int i = 0; i <= new_deg; i++) {
        mpz_set_ui(result->coeffs[i], 0);
    }

    /* Convolution */
    for (int i = 0; i <= a->degree; i++) {
        for (int j = 0; j <= b->degree; j++) {
            mpz_t tmp;
            mpz_init(tmp);
            mpz_mul(tmp, a->coeffs[i], b->coeffs[j]);
            mpz_add(result->coeffs[i + j], result->coeffs[i + j], tmp);
            mpz_clear(tmp);
        }
    }

    result->degree = new_deg;
    nt_poly_normalize(result);
    return 0;
}

LV00_PUBLIC_API int nt_poly_mod(Lv00Poly *result, const Lv00Poly *f,
                                const Lv00Poly *m) {
    if (!result || !f || !m) return -1;
    if (m->degree < 0) return -1; /* cannot mod by zero polynomial */

    /* If deg(f) < deg(m), remainder is f */
    if (f->degree < m->degree) {
        /* Copy f into result */
        if (f->degree >= 0) {
            if (nt_poly_ensure_capacity(result, f->degree) != 0) return -1;
            for (int i = 0; i <= f->degree; i++) {
                mpz_set(result->coeffs[i], f->coeffs[i]);
            }
        }
        result->degree = f->degree;
        return 0;
    }

    /* Polynomial long division: compute f mod m
     * We work on a copy of f's coefficients */
    int rem_deg = f->degree;
    mpz_t *rem = (mpz_t *)malloc((size_t)(rem_deg + 1) * sizeof(mpz_t));
    if (!rem) return -1;

    for (int i = 0; i <= rem_deg; i++) {
        mpz_init_set(rem[i], f->coeffs[i]);
    }

    mpz_t lead_m, factor, tmp;
    mpz_init(lead_m);
    mpz_init(factor);
    mpz_init(tmp);
    mpz_set(lead_m, m->coeffs[m->degree]);

    while (rem_deg >= m->degree) {
        /* Compute factor = rem[rem_deg] / lead_m */
        mpz_fdiv_q(factor, rem[rem_deg], lead_m);

        /* Subtract factor * m * x^(rem_deg - m->degree) from rem */
        int shift = rem_deg - m->degree;
        for (int j = 0; j <= m->degree; j++) {
            mpz_mul(tmp, factor, m->coeffs[j]);
            mpz_sub(rem[shift + j], rem[shift + j], tmp);
        }

        /* Update remainder degree */
        rem_deg--;
        while (rem_deg >= 0 && mpz_cmp_ui(rem[rem_deg], 0) == 0) {
            rem_deg--;
        }
    }

    /* Copy remainder into result */
    if (rem_deg >= 0) {
        if (nt_poly_ensure_capacity(result, rem_deg) != 0) {
            /* cleanup on failure */
            for (int i = 0; i <= f->degree; i++) mpz_clear(rem[i]);
            free(rem);
            mpz_clear(lead_m);
            mpz_clear(factor);
            mpz_clear(tmp);
            return -1;
        }
        for (int i = 0; i <= rem_deg; i++) {
            mpz_set(result->coeffs[i], rem[i]);
        }
    }
    result->degree = rem_deg;

    /* Cleanup */
    for (int i = 0; i <= f->degree; i++) {
        mpz_clear(rem[i]);
    }
    free(rem);
    mpz_clear(lead_m);
    mpz_clear(factor);
    mpz_clear(tmp);

    return 0;
}

LV00_PUBLIC_API int nt_poly_gcd(Lv00Poly *result, const Lv00Poly *a,
                                const Lv00Poly *b) {
    if (!result || !a || !b) return -1;

    /* Handle zero polynomial cases */
    if (a->degree < 0 && b->degree < 0) {
        result->degree = -1;
        return 0;
    }
    if (a->degree < 0) {
        /* gcd(0, b) = b (normalized) */
        if (nt_poly_ensure_capacity(result, b->degree) != 0) return -1;
        for (int i = 0; i <= b->degree; i++) {
            mpz_set(result->coeffs[i], b->coeffs[i]);
        }
        result->degree = b->degree;
        nt_poly_normalize(result);
        return 0;
    }
    if (b->degree < 0) {
        /* gcd(a, 0) = a (normalized) */
        if (nt_poly_ensure_capacity(result, a->degree) != 0) return -1;
        for (int i = 0; i <= a->degree; i++) {
            mpz_set(result->coeffs[i], a->coeffs[i]);
        }
        result->degree = a->degree;
        nt_poly_normalize(result);
        return 0;
    }

    /* Euclidean algorithm using polynomial remainder sequence */
    /* We use temporary Lv00Poly objects to avoid manual mpz array management */

    /* Make u = a, v = b */
    Lv00Poly *u = nt_poly_create();
    Lv00Poly *v = nt_poly_create();
    Lv00Poly *temp = nt_poly_create();
    if (!u || !v || !temp) {
        nt_poly_destroy(u);
        nt_poly_destroy(v);
        nt_poly_destroy(temp);
        return -1;
    }

    /* Copy a into u */
    if (nt_poly_ensure_capacity(u, a->degree) != 0) goto fail;
    for (int i = 0; i <= a->degree; i++) mpz_set(u->coeffs[i], a->coeffs[i]);
    u->degree = a->degree;

    /* Copy b into v */
    if (nt_poly_ensure_capacity(v, b->degree) != 0) goto fail;
    for (int i = 0; i <= b->degree; i++) mpz_set(v->coeffs[i], b->coeffs[i]);
    v->degree = b->degree;

    while (v->degree >= 0) {
        nt_poly_mod(temp, u, v);
        /* Swap u and v */
        Lv00Poly *swap_tmp = u;
        u = v;
        v = temp;
        temp = swap_tmp;
    }

    /* u now holds the GCD; copy into result */
    if (u->degree >= 0) {
        if (nt_poly_ensure_capacity(result, u->degree) != 0) goto fail;
        for (int i = 0; i <= u->degree; i++) {
            mpz_set(result->coeffs[i], u->coeffs[i]);
        }
    }
    result->degree = u->degree;

    nt_poly_destroy(u);
    nt_poly_destroy(v);
    nt_poly_destroy(temp);
    return 0;

fail:
    nt_poly_destroy(u);
    nt_poly_destroy(v);
    nt_poly_destroy(temp);
    return -1;
}

/* ============================================================
 * Evaluation and properties
 * ============================================================ */

LV00_PUBLIC_API int nt_poly_eval(const Lv00Poly *p, const mpz_t x, mpz_t out) {
    if (!p) {
        mpz_set_ui(out, 0);
        return -1;
    }

    if (p->degree < 0) {
        mpz_set_ui(out, 0);
        return 0;
    }

    /* Horner's method: p(x) = (...((a_n * x + a_{n-1}) * x + a_{n-2}) * x + ...) + a_0 */
    mpz_set(out, p->coeffs[p->degree]);
    for (int i = p->degree - 1; i >= 0; i--) {
        mpz_mul(out, out, x);
        mpz_add(out, out, p->coeffs[i]);
    }

    return 0;
}

LV00_PUBLIC_API int nt_poly_degree(const Lv00Poly *p) {
    if (!p) return -1;
    return p->degree;
}
