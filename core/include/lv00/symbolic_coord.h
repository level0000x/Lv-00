#ifndef LV00_SYMBOLIC_COORD_H
#define LV00_SYMBOLIC_COORD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <gmp.h>
#include "rational.h"   /* Rational full definition */
#include "mpz_poly.h"   /* mpz_poly_t */

/* ── Forward decls ── */
typedef struct Algebraic        Algebraic;
typedef struct Quadratic        Quadratic;
typedef struct TranscendentalExpr TranscendentalExpr;
typedef struct Transcendental   Transcendental;

/* Aliases for node_deep_copy.c compatibility */
typedef Algebraic         AlgebraExpr;
typedef Algebraic         AlgebraicExpr;
typedef Quadratic         QuadraticExpr;

/* ── Coord type ── */
typedef enum {
    RATIONAL       = 0,
    ALGEBRAIC      = 1,
    QUADRATIC      = 2,
    TRANSCENDENTAL = 3
} SymbolicCoordType;

/* ── Algebraic plan ── */
typedef enum {
    PLAN_A_FULL_ALGEBRAIC = 0,
    PLAN_B_QUADRATIC_ONLY = 1,
    PLAN_C_RATIONAL_ONLY  = 2
} AlgebraicPlan;
#define algebraic_set_plan(p) ((void)0)
#define algebraic_get_plan()  PLAN_A_FULL_ALGEBRAIC

/* ── Circuit helpers ── */
#define circuit_set_context(r, op, t1, t2) ((void)0)
typedef enum { CIRCUIT_OK, CIRCUIT_OK_STATUS = 0, CIRCUIT_FAIL, CIRCUIT_STATUS_OK = 0 } CircuitStatus;
#define check_digit_circuit(c) CIRCUIT_OK
#define circuit_reset_context() ((void)0)
#define circuit_get_overflow_count() 0
#define circuit_handle_overflow() ((void)0)
#define circuit_set_frozen_point(p) ((void)(p))
#define circuit_has_frozen_point() true
#define circuit_get_frozen_point() ((void*)0)

/* ── Trust color ── */
typedef enum {
    TRUST_GREEN        = 0,
    TRUST_BLUE         = 1,
    TRUST_YELLOW       = 2,
    TRUST_LIGHT_ORANGE = 3,
    TRUST_ORANGE       = 4,
    TRUST_AMBER        = 5,
    TRUST_RED          = 6
} TrustColor;

typedef enum {
    LO_NONE = 0, LO_MEMORY = 1, LO_PERFORMANCE = 2, LO_NUMERIC = 3
} LightOrangeSubtype;

/* ── Algebraic (symbolic_coord.c) ── */
struct Algebraic {
    mpz_poly_t minimal_poly;
    double     left_bound;
    double     right_bound;
};

/* ── Quadratic (symbolic_coord.c) ── */
struct Quadratic {
    Rational     *a;
    Rational     *b;
    unsigned int  n;
};

/* ── Transcendental (symbolic_coord.c + node_deep_copy.c) ── */
struct TranscendentalExpr {
    int         expr_type;
    char        base_name[64];
    Rational   *rational_operand;
    bool        out_of_scope;
};

struct Transcendental {
    char                *name;
    TranscendentalExpr  *expr;
};

/* ── Symbolic coordinate ── */
typedef struct Lv00SymbolicCoord {
    SymbolicCoordType type;
    TrustColor trust;
    union {
        Rational        *rational;
        Algebraic       *algebraic;
        Quadratic       *quadratic;
        Transcendental  *transcendental;
    } data;
} Lv00SymbolicCoord;

typedef Lv00SymbolicCoord SymbolicCoord;

/* ── Lifecycle ── */
SymbolicCoord *symbolic_coord_create_rational(int num, int den);
SymbolicCoord *symbolic_coord_create_quadratic(const Rational *a, const Rational *b, int c);
SymbolicCoord *symbolic_coord_copy(const SymbolicCoord *c);
void symbolic_coord_destroy(SymbolicCoord *c);

Algebraic *algebraic_create(mpz_poly_t *poly, double left_bound, double right_bound);
void algebraic_destroy(Algebraic *a);

Quadratic *quadratic_create(Rational *a, Rational *b, unsigned int n);
void quadratic_destroy(Quadratic *q);

Transcendental *transcendental_create(const char *name);
void transcendental_destroy(Transcendental *t);

/* ── Arithmetic ── */
SymbolicCoord *symbolic_coord_add(const SymbolicCoord *a, const SymbolicCoord *b);
SymbolicCoord *symbolic_coord_subtract(const SymbolicCoord *a, const SymbolicCoord *b);
SymbolicCoord *symbolic_coord_multiply(const SymbolicCoord *a, const SymbolicCoord *b);
SymbolicCoord *symbolic_coord_divide(const SymbolicCoord *a, const SymbolicCoord *b);
SymbolicCoord *symbolic_coord_negate(const SymbolicCoord *c);
SymbolicCoord *symbolic_coord_sqrt(const SymbolicCoord *c);
SymbolicCoord *symbolic_coord_pow(const SymbolicCoord *c, int exp);

/* ── Comparison ── */
int symbolic_coord_compare(const SymbolicCoord *a, const SymbolicCoord *b);
bool symbolic_coord_is_zero(const SymbolicCoord *c);
bool symbolic_coord_is_positive(const SymbolicCoord *c);
bool symbolic_coord_is_negative(const SymbolicCoord *c);
bool symbolic_coord_is_amber(const SymbolicCoord *c);

/* ── Conversion ── */
double symbolic_coord_to_double(const SymbolicCoord *c);

/* ── Serialization ── */
char *symbolic_coord_serialize(const SymbolicCoord *c);

/* ── Trust ── */
TrustColor symbolic_coord_get_trust(const SymbolicCoord *c);
void symbolic_coord_set_trust(SymbolicCoord *c, TrustColor t);
SymbolicCoord *symbolic_coord_downgrade_to_amber(SymbolicCoord *c, double factor, const char *reason);

/* ── Hashing ── */
uint64_t symbolic_coord_hash(const SymbolicCoord *c);

#ifdef __cplusplus
}
#endif
#endif
