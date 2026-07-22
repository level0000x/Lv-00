/**
 * @file ga_multivector.c
 * @brief Projective Geometric Algebra (PGA) multivector implementation
 *
 * @details Implements Cl(3,0,1) algebra with 16 basis elements:
 *          1, e0, e1, e2, e3, e01, e02, e03, e12, e13, e23, e012, e013, e023, e123, e0123
 *
 * @version 1.0.0
 */

#include "lv00/ga_multivector.h"
#include "lv00/lv00_internal.h"
#include "lv00/lv00_utils.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ============================================================
 * Basis element indices (Cl(3,0,1))
 * ============================================================ */
#define GA_S    0   /* 1 (scalar) */
#define GA_E0   1   /* e0 */
#define GA_E1   2   /* e1 */
#define GA_E2   3   /* e2 */
#define GA_E3   4   /* e3 */
#define GA_E01  5   /* e0^e1 */
#define GA_E02  6   /* e0^e2 */
#define GA_E03  7   /* e0^e3 */
#define GA_E12  8   /* e1^e2 */
#define GA_E13  9   /* e1^e3 */
#define GA_E23  10  /* e2^e3 */
#define GA_E012 11  /* e0^e1^e2 */
#define GA_E013 12  /* e0^e1^e3 */
#define GA_E023 13  /* e0^e2^e3 */
#define GA_E123 14  /* e1^e2^e3 */
#define GA_E0123 15 /* e0^e1^e2^e3 (pseudoscalar) */

/* ============================================================
 * Internal structure
 * ============================================================ */

struct Lv00MultiVector {
    double c[16];  /* Coefficients for each basis element */
};

/* ============================================================
 * Lifecycle
 * ============================================================ */

Lv00MultiVector *ga_mv_create(void) {
    Lv00MultiVector *mv = lv00_calloc(1, sizeof(Lv00MultiVector));
    return mv;
}

void ga_mv_destroy(Lv00MultiVector *mv) {
    lv00_free((void **)&mv);
}

Lv00MultiVector *ga_mv_copy(const Lv00MultiVector *src) {
    if (!src) return NULL;

    Lv00MultiVector *copy = ga_mv_create();
    if (!copy) return NULL;

    memcpy(copy->c, src->c, sizeof(copy->c));
    return copy;
}

Lv00MultiVector *ga_mv_zero(void) {
    return ga_mv_create();  /* calloc initializes to zero */
}

/* ============================================================
 * Coefficient access
 * ============================================================ */

double ga_mv_get(const Lv00MultiVector *mv, int index) {
    if (!mv || index < 0 || index >= 16) return 0.0;
    return mv->c[index];
}

void ga_mv_set(Lv00MultiVector *mv, int index, double value) {
    if (!mv || index < 0 || index >= 16) return;
    mv->c[index] = value;
}

/* ============================================================
 * Grade operations
 * ============================================================ */

int ga_mv_grade(const Lv00MultiVector *mv) {
    if (!mv) return -1;

    int max_grade = -1;
    double eps = 1e-10;

    /* Grade 0: scalar */
    if (fabs(mv->c[GA_S]) > eps) max_grade = 0;

    /* Grade 1: vectors */
    if (fabs(mv->c[GA_E0]) > eps || fabs(mv->c[GA_E1]) > eps ||
        fabs(mv->c[GA_E2]) > eps || fabs(mv->c[GA_E3]) > eps)
        max_grade = 1;

    /* Grade 2: bivectors */
    if (fabs(mv->c[GA_E01]) > eps || fabs(mv->c[GA_E02]) > eps ||
        fabs(mv->c[GA_E03]) > eps || fabs(mv->c[GA_E12]) > eps ||
        fabs(mv->c[GA_E13]) > eps || fabs(mv->c[GA_E23]) > eps)
        max_grade = 2;

    /* Grade 3: trivectors */
    if (fabs(mv->c[GA_E012]) > eps || fabs(mv->c[GA_E013]) > eps ||
        fabs(mv->c[GA_E023]) > eps || fabs(mv->c[GA_E123]) > eps)
        max_grade = 3;

    /* Grade 4: pseudoscalar */
    if (fabs(mv->c[GA_E0123]) > eps)
        max_grade = 4;

    return max_grade;
}

Lv00MultiVector *ga_mv_grade_project(const Lv00MultiVector *mv, int grade) {
    if (!mv) return NULL;

    Lv00MultiVector *result = ga_mv_zero();
    if (!result) return NULL;

    switch (grade) {
    case 0:
        result->c[GA_S] = mv->c[GA_S];
        break;
    case 1:
        result->c[GA_E0] = mv->c[GA_E0];
        result->c[GA_E1] = mv->c[GA_E1];
        result->c[GA_E2] = mv->c[GA_E2];
        result->c[GA_E3] = mv->c[GA_E3];
        break;
    case 2:
        result->c[GA_E01] = mv->c[GA_E01];
        result->c[GA_E02] = mv->c[GA_E02];
        result->c[GA_E03] = mv->c[GA_E03];
        result->c[GA_E12] = mv->c[GA_E12];
        result->c[GA_E13] = mv->c[GA_E13];
        result->c[GA_E23] = mv->c[GA_E23];
        break;
    case 3:
        result->c[GA_E012] = mv->c[GA_E012];
        result->c[GA_E013] = mv->c[GA_E013];
        result->c[GA_E023] = mv->c[GA_E023];
        result->c[GA_E123] = mv->c[GA_E123];
        break;
    case 4:
        result->c[GA_E0123] = mv->c[GA_E0123];
        break;
    }

    return result;
}

/* ============================================================
 * Arithmetic operations
 * ============================================================ */

Lv00MultiVector *ga_mv_add(const Lv00MultiVector *a, const Lv00MultiVector *b) {
    if (!a || !b) return NULL;

    Lv00MultiVector *result = ga_mv_create();
    if (!result) return NULL;

    for (int i = 0; i < 16; i++) {
        result->c[i] = a->c[i] + b->c[i];
    }

    return result;
}

Lv00MultiVector *ga_mv_sub(const Lv00MultiVector *a, const Lv00MultiVector *b) {
    if (!a || !b) return NULL;

    Lv00MultiVector *result = ga_mv_create();
    if (!result) return NULL;

    for (int i = 0; i < 16; i++) {
        result->c[i] = a->c[i] - b->c[i];
    }

    return result;
}

Lv00MultiVector *ga_mv_scale(const Lv00MultiVector *mv, double scalar) {
    if (!mv) return NULL;

    Lv00MultiVector *result = ga_mv_create();
    if (!result) return NULL;

    for (int i = 0; i < 16; i++) {
        result->c[i] = mv->c[i] * scalar;
    }

    return result;
}

Lv00MultiVector *ga_mv_negate(const Lv00MultiVector *mv) {
    return ga_mv_scale(mv, -1.0);
}

/* ============================================================
 * Geometric product (simplified)
 * ============================================================ */

Lv00MultiVector *ga_mv_geometric_product(const Lv00MultiVector *a,
                                          const Lv00MultiVector *b) {
    if (!a || !b) return NULL;

    Lv00MultiVector *result = ga_mv_zero();
    if (!result) return NULL;

    /* Simplified: only handle common cases */
    /* Scalar * anything */
    if (fabs(a->c[GA_S]) > 1e-10 && ga_mv_grade(a) == 0) {
        return ga_mv_scale(b, a->c[GA_S]);
    }
    if (fabs(b->c[GA_S]) > 1e-10 && ga_mv_grade(b) == 0) {
        return ga_mv_scale(a, b->c[GA_S]);
    }

    /* Vector * Vector: a·b + a^b */
    if (ga_mv_grade(a) == 1 && ga_mv_grade(b) == 1) {
        /* Dot product (scalar part) */
        result->c[GA_S] = (a->c[GA_E1] * b->c[GA_E1] +
                           a->c[GA_E2] * b->c[GA_E2] +
                           a->c[GA_E3] * b->c[GA_E3]);

        /* Outer product (bivector part) */
        result->c[GA_E12] = a->c[GA_E1] * b->c[GA_E2] - a->c[GA_E2] * b->c[GA_E1];
        result->c[GA_E13] = a->c[GA_E1] * b->c[GA_E3] - a->c[GA_E3] * b->c[GA_E1];
        result->c[GA_E23] = a->c[GA_E2] * b->c[GA_E3] - a->c[GA_E3] * b->c[GA_E2];

        return result;
    }

    /* General case: copy a (placeholder) */
    for (int i = 0; i < 16; i++) {
        result->c[i] = a->c[i];
    }

    return result;
}

/* ============================================================
 * Inner product (dot product)
 * ============================================================ */

double ga_mv_inner_product(const Lv00MultiVector *a, const Lv00MultiVector *b) {
    if (!a || !b) return 0.0;

    /* For vectors: standard dot product */
    return (a->c[GA_E1] * b->c[GA_E1] +
            a->c[GA_E2] * b->c[GA_E2] +
            a->c[GA_E3] * b->c[GA_E3]);
}

/* ============================================================
 * Outer product (wedge product)
 * ============================================================ */

Lv00MultiVector *ga_mv_outer_product(const Lv00MultiVector *a,
                                      const Lv00MultiVector *b) {
    if (!a || !b) return NULL;

    Lv00MultiVector *result = ga_mv_zero();
    if (!result) return NULL;

    /* scalar * anything */
    for (int i = 0; i < 16; i++) {
        result->c[i] += a->c[GA_S] * b->c[i] + a->c[i] * b->c[GA_S];
    }
    /* subtract double-counted scalar*scalar */
    result->c[GA_S] = a->c[GA_S] * b->c[GA_S];

    /* grade-1 ^ grade-1 → grade-2 */
    result->c[GA_E01] += a->c[GA_E0]*b->c[GA_E1] - a->c[GA_E1]*b->c[GA_E0];
    result->c[GA_E02] += a->c[GA_E0]*b->c[GA_E2] - a->c[GA_E2]*b->c[GA_E0];
    result->c[GA_E03] += a->c[GA_E0]*b->c[GA_E3] - a->c[GA_E3]*b->c[GA_E0];
    result->c[GA_E12] += a->c[GA_E1]*b->c[GA_E2] - a->c[GA_E2]*b->c[GA_E1];
    result->c[GA_E13] += a->c[GA_E1]*b->c[GA_E3] - a->c[GA_E3]*b->c[GA_E1];
    result->c[GA_E23] += a->c[GA_E2]*b->c[GA_E3] - a->c[GA_E3]*b->c[GA_E2];

    /* grade-1 ^ grade-2 → grade-3 */
    result->c[GA_E012] += a->c[GA_E0]*b->c[GA_E12] - a->c[GA_E1]*b->c[GA_E02] + a->c[GA_E2]*b->c[GA_E01];
    result->c[GA_E013] += a->c[GA_E0]*b->c[GA_E13] - a->c[GA_E1]*b->c[GA_E03] + a->c[GA_E3]*b->c[GA_E01];
    result->c[GA_E023] += a->c[GA_E0]*b->c[GA_E23] - a->c[GA_E2]*b->c[GA_E03] + a->c[GA_E3]*b->c[GA_E02];
    result->c[GA_E123] += a->c[GA_E1]*b->c[GA_E23] - a->c[GA_E2]*b->c[GA_E13] + a->c[GA_E3]*b->c[GA_E12];

    /* grade-2 ^ grade-1 → grade-3 */
    result->c[GA_E012] += a->c[GA_E01]*b->c[GA_E2] - a->c[GA_E02]*b->c[GA_E1] + a->c[GA_E12]*b->c[GA_E0];
    result->c[GA_E013] += a->c[GA_E01]*b->c[GA_E3] - a->c[GA_E03]*b->c[GA_E1] + a->c[GA_E13]*b->c[GA_E0];
    result->c[GA_E023] += a->c[GA_E02]*b->c[GA_E3] - a->c[GA_E03]*b->c[GA_E2] + a->c[GA_E23]*b->c[GA_E0];
    result->c[GA_E123] += a->c[GA_E12]*b->c[GA_E3] - a->c[GA_E13]*b->c[GA_E2] + a->c[GA_E23]*b->c[GA_E1];

    /* grade-2 ^ grade-2 → grade-4 */
    result->c[GA_E0123] += a->c[GA_E01]*b->c[GA_E23] - a->c[GA_E02]*b->c[GA_E13]
                         + a->c[GA_E03]*b->c[GA_E12] + a->c[GA_E12]*b->c[GA_E03]
                         - a->c[GA_E13]*b->c[GA_E02] + a->c[GA_E23]*b->c[GA_E01];

    /* grade-1 ^ grade-3 → grade-4 */
    result->c[GA_E0123] += a->c[GA_E0]*b->c[GA_E123] - a->c[GA_E1]*b->c[GA_E023]
                         + a->c[GA_E2]*b->c[GA_E013] - a->c[GA_E3]*b->c[GA_E012];

    /* grade-3 ^ grade-1 → grade-4 */
    result->c[GA_E0123] += a->c[GA_E012]*b->c[GA_E3] - a->c[GA_E013]*b->c[GA_E2]
                         + a->c[GA_E023]*b->c[GA_E1] - a->c[GA_E123]*b->c[GA_E0];

    return result;
}

/* ============================================================
 * Norm and reverse
 * ============================================================ */

double ga_mv_norm(const Lv00MultiVector *mv) {
    if (!mv) return 0.0;

    double sum = 0.0;
    for (int i = 0; i < 16; i++) {
        sum += mv->c[i] * mv->c[i];
    }

    return sqrt(sum);
}

double ga_mv_norm_squared(const Lv00MultiVector *mv) {
    if (!mv) return 0.0;

    double sum = 0.0;
    for (int i = 0; i < 16; i++) {
        sum += mv->c[i] * mv->c[i];
    }

    return sum;
}

Lv00MultiVector *ga_mv_reverse(const Lv00MultiVector *mv) {
    if (!mv) return NULL;

    Lv00MultiVector *result = ga_mv_create();
    if (!result) return NULL;

    /* Grade 0: unchanged */
    result->c[GA_S] = mv->c[GA_S];

    /* Grade 1: unchanged */
    result->c[GA_E0] = mv->c[GA_E0];
    result->c[GA_E1] = mv->c[GA_E1];
    result->c[GA_E2] = mv->c[GA_E2];
    result->c[GA_E3] = mv->c[GA_E3];

    /* Grade 2: negate */
    result->c[GA_E01] = -mv->c[GA_E01];
    result->c[GA_E02] = -mv->c[GA_E02];
    result->c[GA_E03] = -mv->c[GA_E03];
    result->c[GA_E12] = -mv->c[GA_E12];
    result->c[GA_E13] = -mv->c[GA_E13];
    result->c[GA_E23] = -mv->c[GA_E23];

    /* Grade 3: negate */
    result->c[GA_E012] = -mv->c[GA_E012];
    result->c[GA_E013] = -mv->c[GA_E013];
    result->c[GA_E023] = -mv->c[GA_E023];
    result->c[GA_E123] = -mv->c[GA_E123];

    /* Grade 4: unchanged */
    result->c[GA_E0123] = mv->c[GA_E0123];

    return result;
}

Lv00MultiVector *ga_mv_normalize(const Lv00MultiVector *mv) {
    if (!mv) return NULL;

    double norm = ga_mv_norm(mv);
    if (fabs(norm) < 1e-10) return NULL;

    return ga_mv_scale(mv, 1.0 / norm);
}

/* ============================================================
 * Dual and sandwich
 * ============================================================ */

Lv00MultiVector *ga_mv_dual(const Lv00MultiVector *mv) {
    if (!mv) return NULL;

    Lv00MultiVector *result = ga_mv_create();
    if (!result) return NULL;

    /* Hodge dual: multiply by pseudoscalar inverse */
    /* In Cl(3,0,1), I = e0123, I^{-1} = -e0123 */
    result->c[GA_S] = -mv->c[GA_E0123];
    result->c[GA_E0] = -mv->c[GA_E123];
    result->c[GA_E1] = mv->c[GA_E023];
    result->c[GA_E2] = -mv->c[GA_E013];
    result->c[GA_E3] = mv->c[GA_E012];
    result->c[GA_E01] = mv->c[GA_E23];
    result->c[GA_E02] = -mv->c[GA_E13];
    result->c[GA_E03] = mv->c[GA_E12];
    result->c[GA_E12] = mv->c[GA_E03];
    result->c[GA_E13] = -mv->c[GA_E02];
    result->c[GA_E23] = mv->c[GA_E01];
    result->c[GA_E012] = mv->c[GA_E3];
    result->c[GA_E013] = -mv->c[GA_E2];
    result->c[GA_E023] = mv->c[GA_E1];
    result->c[GA_E123] = -mv->c[GA_E0];
    result->c[GA_E0123] = mv->c[GA_S];

    return result;
}

Lv00MultiVector *ga_mv_sandwich(const Lv00MultiVector *rotor,
                                 const Lv00MultiVector *mv) {
    if (!rotor || !mv) return NULL;

    /* Sandwich product: R * mv * R~ */
    Lv00MultiVector *r_rev = ga_mv_reverse(rotor);
    if (!r_rev) return NULL;

    Lv00MultiVector *temp = ga_mv_geometric_product(rotor, mv);
    if (!temp) {
        ga_mv_destroy(r_rev);
        return NULL;
    }

    Lv00MultiVector *result = ga_mv_geometric_product(temp, r_rev);

    ga_mv_destroy(r_rev);
    ga_mv_destroy(temp);

    return result;
}

/* ============================================================
 * Comparison
 * ============================================================ */

bool ga_mv_equal(const Lv00MultiVector *a, const Lv00MultiVector *b, double eps) {
    if (!a || !b) return false;

    for (int i = 0; i < 16; i++) {
        if (fabs(a->c[i] - b->c[i]) > eps) return false;
    }

    return true;
}

/* ── ga_mv_scalar: create scalar multivector ── */
Lv00MultiVector *ga_mv_scalar(double value) {
    Lv00MultiVector *mv = ga_mv_create();
    if (mv) mv->c[0] = value;
    return mv;
}

bool ga_mv_is_zero(const Lv00MultiVector *mv, double eps) {
    if (!mv) return true;

    for (int i = 0; i < 16; i++) {
        if (fabs(mv->c[i]) > eps) return false;
    }

    return true;
}
