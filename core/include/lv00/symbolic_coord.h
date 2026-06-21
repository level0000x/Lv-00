#ifndef LV00_SYMBOLIC_COORD_H
#define LV00_SYMBOLIC_COORD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <gmp.h>

/* Forward decls */
typedef struct Rational Rational;

/* === Coord type enum === */
typedef enum {
    RATIONAL      = 0,
    ALGEBRAIC     = 1,
    QUADRATIC     = 2,
    TRANSCENDENTAL = 3
} SymbolicCoordType;

/* === Algebraic plan === */
typedef enum {
    PLAN_A_FULL_ALGEBRAIC = 0,
    PLAN_B_QUADRATIC_ONLY = 1,
    PLAN_C_RATIONAL_ONLY  = 2
} AlgebraicPlan;
#define algebraic_set_plan(p) ((void)0)
#define algebraic_get_plan()  PLAN_A_FULL_ALGEBRAIC

/* === Circuit stubs === */
#define circuit_set_context(r, op, t1, t2) ((void)0)
typedef enum { CIRCUIT_OK, CIRCUIT_OK_STATUS = 0, CIRCUIT_FAIL, CIRCUIT_STATUS_OK = 0 } CircuitStatus;
#define check_digit_circuit(c) CIRCUIT_OK
#define circuit_reset_context() ((void)0)
#define circuit_get_overflow_count() 0
#define circuit_handle_overflow() ((void)0)
#define circuit_set_frozen_point(p) ((void)(p))
#define circuit_has_frozen_point() true
#define circuit_get_frozen_point() ((void*)0)

/* === Trust color === */
typedef enum {
    TRUST_GREEN = 0, TRUST_BLUE = 0,
    TRUST_YELLOW = 1,
    TRUST_ORANGE = 2, TRUST_AMBER = 2,
    TRUST_RED = 3
} TrustColor;

typedef enum {
    LO_NONE = 0, LO_MEMORY = 1, LO_PERFORMANCE = 2, LO_NUMERIC = 3
} LightOrangeSubtype;

/* === Algebraic expression (e.g. sqrt(2), root of minimal polynomial) === */
typedef struct {
    Rational *minimal_poly; /* representative minimal polynomial */
    double left_bound;
    double right_bound;
} AlgebraicExpr;

AlgebraicExpr *algebraic_create(const Rational *p, double lb, double rb);
void algebraic_destroy(AlgebraicExpr *a);

/* === Quadratic expression (a*sqrt(b) + c) === */
typedef struct {
    Rational *a;
    Rational *b;
    int n;
} QuadraticExpr;

QuadraticExpr *quadratic_create(Rational *a, Rational *b, int n);
void quadratic_destroy(QuadraticExpr *q);

/* === Transcendental expression === */
typedef enum {
    TRAN_TYPE_PI, TRAN_TYPE_E, TRAN_TYPE_LOG, TRAN_TYPE_SIN,
    TRAN_TYPE_COS, TRAN_TYPE_EXP, TRAN_TYPE_CUSTOM
} TranscendentalType;

typedef struct TranscendentalExpr {
    TranscendentalType expr_type;
    char *base_name;
    Rational *rational_operand;
    bool out_of_scope;
    /* union for extra data */
    union {
        double numeric_approx;
        char *custom_id;
    };
} TranscendentalExpr;

typedef struct {
    char *name;
    TranscendentalExpr *expr;
} TranscendentalData;

TranscendentalData *transcendental_create(const char *name);
void transcendental_destroy(TranscendentalData *t);

/* === SymbolicCoord — main type === */
typedef struct Lv00SymbolicCoord {
    SymbolicCoordType type;
    TrustColor trust;
    union {
        Rational *rational;
        AlgebraicExpr *algebraic;
        QuadraticExpr *quadratic;
        TranscendentalData *transcendental;
    } data;
} Lv00SymbolicCoord;

typedef Lv00SymbolicCoord SymbolicCoord;

/* === Lifecycle === */
SymbolicCoord *symbolic_coord_create_rational(int num, int den);
SymbolicCoord *symbolic_coord_create_quadratic(const Rational *a, const Rational *b, int c);
SymbolicCoord *symbolic_coord_copy(const SymbolicCoord *c);
void symbolic_coord_destroy(SymbolicCoord *c);

/* === Arithmetic === */
SymbolicCoord *symbolic_coord_add(const SymbolicCoord *a, const SymbolicCoord *b);
SymbolicCoord *symbolic_coord_subtract(const SymbolicCoord *a, const SymbolicCoord *b);
SymbolicCoord *symbolic_coord_multiply(const SymbolicCoord *a, const SymbolicCoord *b);
SymbolicCoord *symbolic_coord_divide(const SymbolicCoord *a, const SymbolicCoord *b);
SymbolicCoord *symbolic_coord_negate(const SymbolicCoord *c);
SymbolicCoord *symbolic_coord_sqrt(const SymbolicCoord *c);
SymbolicCoord *symbolic_coord_pow(const SymbolicCoord *c, int exp);

/* === Comparison & queries === */
int symbolic_coord_compare(const SymbolicCoord *a, const SymbolicCoord *b);
bool symbolic_coord_is_zero(const SymbolicCoord *c);
bool symbolic_coord_is_positive(const SymbolicCoord *c);
bool symbolic_coord_is_negative(const SymbolicCoord *c);
bool symbolic_coord_is_amber(const SymbolicCoord *c);

/* === Conversion === */
double symbolic_coord_to_double(const SymbolicCoord *c);

/* === Serialization === */
char *symbolic_coord_serialize(const SymbolicCoord *c);

/* === Trust color === */
TrustColor symbolic_coord_get_trust(const SymbolicCoord *c);
void symbolic_coord_set_trust(SymbolicCoord *c, TrustColor t);
SymbolicCoord *symbolic_coord_downgrade_to_amber(SymbolicCoord *c, double factor, const char *reason);

/* === Hashing === */
uint64_t symbolic_coord_hash(const SymbolicCoord *c);

#ifdef __cplusplus
}
#endif
#endif
